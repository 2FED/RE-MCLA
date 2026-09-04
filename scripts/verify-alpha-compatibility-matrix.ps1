[CmdletBinding()]
param(
    [string]$MatrixPath,
    [switch]$Fixture
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $MatrixPath) { $MatrixPath = Join-Path $repo 'config/alpha-compatibility-matrix.json' }

function Exact-Properties($Object, [string[]]$Expected, [string]$Label) {
    $actual = @($Object.PSObject.Properties.Name | Sort-Object)
    $wanted = @($Expected | Sort-Object)
    if (($actual -join "`n") -cne ($wanted -join "`n")) { throw "$Label properties drifted." }
}

function Required-Text($Value, [string]$Label) {
    if ($Value -isnot [string] -or [string]::IsNullOrWhiteSpace($Value)) { throw "$Label must be nonempty text." }
}

function Resolve-PublicEvidence([string]$RelativePath) {
    Required-Text $RelativePath 'Evidence path'
    if ([IO.Path]::IsPathRooted($RelativePath) -or $RelativePath.Contains('..')) { throw 'Evidence path is not repository-relative.' }
    $full = [IO.Path]::GetFullPath((Join-Path $repo $RelativePath))
    $root = [IO.Path]::GetFullPath($repo).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) { throw 'Evidence path escapes the repository.' }
    $item = Get-Item -LiteralPath $full -Force
    if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Evidence path is not a regular public file.' }
    return $full
}

$matrixFull = (Resolve-Path -LiteralPath $MatrixPath).Path
$matrix = Get-Content -LiteralPath $matrixFull -Raw | ConvertFrom-Json
Exact-Properties $matrix @('schema','task','decision','tested_build','reference_host','rows','known_issue_inventory','claims') 'Matrix'
if ($matrix.schema -cne 'mcla-alpha-compatibility-matrix-v1' -or $matrix.task -cne 'M6-016' -or
    $matrix.decision -cne 'bounded-single-host-alpha-compatibility-matrix') { throw 'Matrix identity is invalid.' }

$build = $matrix.tested_build
Exact-Properties $build @('project_version','project_commit','configuration','sdk_version','sdk_commit','title_id','media_id','module_hash','source_iso_sha256','stock_frame_rate') 'Tested build'
if ($build.project_version -cne '0.7.0.0' -or $build.project_commit -cne '6c167fae0805d8f1417e8e0484c8614fef27f77a' -or $build.configuration -cne 'Release' -or
    $build.sdk_version -cne '0.10.0.0' -or $build.sdk_commit -cne '5d3e98c064c38e0769b4f59d11729c8f6270eb83' -or
    $build.title_id -cne '545407F8' -or $build.media_id -cne '5940C9DB' -or $build.module_hash -cne '1984A3354B78CE19' -or
    $build.source_iso_sha256 -cne 'AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB' -or
    [int]$build.stock_frame_rate -ne 30) { throw 'Tested build identity drifted.' }

$hostInfo = $matrix.reference_host
Exact-Properties $hostInfo @('operating_system','os_build','cpu','gpu','gpu_driver','graphics_api','physically_verified_resolutions','audio_output','physically_verified_gamepads','physically_verified_wheels','remote_display_paths_observed') 'Reference host'
if ($hostInfo.operating_system -cne 'Windows 11 Pro x86-64' -or $hostInfo.os_build -cne '26200' -or
    $hostInfo.cpu -cne 'AMD Ryzen 9 5900X 12-Core Processor' -or $hostInfo.gpu -cne 'NVIDIA GeForce RTX 3090' -or
    $hostInfo.gpu_driver -cne '32.0.16.1088' -or $hostInfo.graphics_api -cne 'Direct3D 12' -or
    $hostInfo.audio_output -cne 'Windows default SDL playback endpoint; device identity withheld') { throw 'Reference host identity drifted.' }
if ((@($hostInfo.physically_verified_resolutions) -join ',') -cne '1280x720,2560x1440') { throw 'Verified resolution inventory drifted.' }
if (@($hostInfo.physically_verified_gamepads).Count -ne 2 -or @($hostInfo.physically_verified_wheels).Count -ne 1 -or
    @($hostInfo.remote_display_paths_observed).Count -ne 2) { throw 'Reference input/display path inventory drifted.' }

$expectedRows = [ordered]@{
    'boot-frontend'='verified'
    'saved-single-player'='verified-with-limitations'
    'gamepad-input'='verified'
    'racing-wheel'='verified-with-limitations'
    'graphics'='usable-with-open-s2-defects'
    'audio'='verified-with-limitations'
    'city-streaming'='verified'
    'race-systems'='verified-representative'
    'garage-customization'='verified-representative'
    'window-device-lifecycle'='verified-with-limitations'
    'offline-services'='bounded-offline'
    'long-session-soak'='verified-with-limitations'
    'campaign-complete-edition'='not-verified'
}
$rows = @($matrix.rows)
if ($rows.Count -ne $expectedRows.Count) { throw 'Compatibility row cardinality drifted.' }
$seen = @{}
for ($i = 0; $i -lt $rows.Count; $i++) {
    $row = $rows[$i]
    Exact-Properties $row @('id','status','scope','evidence','limitations') "Compatibility row $i"
    $expectedId = @($expectedRows.Keys)[$i]
    if ($row.id -cne $expectedId -or $row.status -cne $expectedRows[$expectedId]) { throw 'Compatibility row identity, order, or status drifted.' }
    if ($seen.ContainsKey($row.id)) { throw 'Duplicate compatibility row ID.' }; $seen[$row.id] = $true
    Required-Text $row.scope "Compatibility row $($row.id) scope"
    if (@($row.evidence).Count -lt 1 -or @($row.limitations).Count -lt 1) { throw 'Every compatibility row needs evidence and an explicit limitation.' }
    foreach ($path in @($row.evidence)) { Resolve-PublicEvidence $path | Out-Null }
    foreach ($limit in @($row.limitations)) { Required-Text $limit "Compatibility row $($row.id) limitation" }
}

$knownText = Get-Content -LiteralPath (Join-Path $repo 'docs/known-issues.md') -Raw
$matches = [regex]::Matches($knownText, '(?m)^\|\s*(KI-\d{3})\s*\|\s*(S\d)\s*\|\s*(Open|Closed|Contained|Policy)\s*\|')
if ($matches.Count -ne 26) { throw 'Canonical known-issue register must contain exactly twenty-six rows.' }
$inventory = @($matrix.known_issue_inventory)
if ($inventory.Count -ne $matches.Count) { throw 'Known-issue inventory cardinality drifted.' }
for ($i = 0; $i -lt $matches.Count; $i++) {
    Exact-Properties $inventory[$i] @('id','severity','status') "Known issue $i"
    $expectedId = 'KI-{0:D3}' -f ($i + 1)
    if ($matches[$i].Groups[1].Value -cne $expectedId -or $inventory[$i].id -cne $expectedId -or
        $inventory[$i].severity -cne $matches[$i].Groups[2].Value -or $inventory[$i].status -cne $matches[$i].Groups[3].Value) {
        throw 'Known-issue ID, order, severity, or status drifted.'
    }
}

$claims = $matrix.claims
Exact-Properties $claims @('single_reference_host_only','alternate_gpu_verified','alternate_os_verified','cross_model_wheel_physical_claim','full_campaign_claim','online_service_claim','rendering_parity_claim','exact_audio_mix_claim','current_five_stage_soak_complete') 'Claims'
if ($claims.single_reference_host_only -ne $true) { throw 'The alpha matrix must remain scoped to one reference host.' }
foreach ($name in @('alternate_gpu_verified','alternate_os_verified','cross_model_wheel_physical_claim','full_campaign_claim','online_service_claim','rendering_parity_claim','exact_audio_mix_claim','current_five_stage_soak_complete')) {
    if ($claims.$name -ne $false) { throw "Unsupported alpha claim enabled: $name" }
}

if (-not $Fixture) {
    $doc = Get-Content -LiteralPath (Join-Path $repo 'docs/alpha-compatibility.md') -Raw
    foreach ($needle in @('0.7.0.0','5d3e98c064c38e0769b4f59d11729c8f6270eb83','Windows 11 Pro x86-64 build 26200','AMD Ryzen 9 5900X','NVIDIA GeForce RTX 3090','Thrustmaster T300RS','## Route matrix','## Known issues','known-issues.md','## Reading the status labels','Full campaign / all Complete Edition content','3,600-second mixed gameplay passed')) {
        if (-not $doc.Contains($needle, [StringComparison]::Ordinal)) { throw "Alpha compatibility document is missing: $needle" }
    }
    $cmake = Get-Content -LiteralPath (Join-Path $repo 'CMakeLists.txt') -Raw
    $bootstrap = Get-Content -LiteralPath (Join-Path $repo 'scripts/bootstrap.ps1') -Raw
    if ($cmake -notmatch 'MCLA_REXGLUE_VERSION\s+"0\.10\.0\.2"' -or
        -not $bootstrap.Contains('492614eec92c31f11d75dd8fa0f09785cbae4a66', [StringComparison]::Ordinal) -or
        $build.sdk_version -cne '0.10.0.0' -or
        $build.sdk_commit -cne '5d3e98c064c38e0769b4f59d11729c8f6270eb83') {
        throw 'Tested alpha identity or its reviewed SDK hotfix successor drifted.'
    }
}

[pscustomobject]@{
    Decision = $matrix.decision
    CompatibilityRows = $rows.Count
    KnownIssues = $inventory.Count
    ReferenceHosts = 1
    BroadClaimsRejected = 8
    Passed = $true
}
