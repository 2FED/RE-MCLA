[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BundleRoot,
    [switch]$Fixture,
    [switch]$RunLauncherVerify
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$expectedIsoHash = 'AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB'
$saveRelative = 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
$headerRelative = 'B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'

function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') }
        finally { $sha.Dispose() }
    } finally { $stream.Dispose() }
}

function Assert-ExactProperties($Value, [string[]]$Names, [string]$Label) {
    $actual = @($Value.PSObject.Properties.Name | Sort-Object)
    $expected = @($Names | Sort-Object)
    if (($actual -join '|') -cne ($expected -join '|')) { throw "$Label property topology drifted." }
}

function Resolve-Contained([string]$Root, [string]$Relative, [string]$Label, [switch]$Leaf, [switch]$Container) {
    if ([string]::IsNullOrWhiteSpace($Relative) -or [IO.Path]::IsPathRooted($Relative) -or
        @($Relative.Replace('\','/').Split('/') | Where-Object { $_ -eq '..' -or $_ -eq '.' -or $_ -eq '' }).Count) {
        throw "$Label has an unsafe relative path '$Relative'."
    }
    $candidate = [IO.Path]::GetFullPath((Join-Path $Root $Relative.Replace('/','\')))
    $prefix = $Root.TrimEnd('\') + [IO.Path]::DirectorySeparatorChar
    if (-not $candidate.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Label escapes the bundle root." }
    if ($Leaf -and -not (Test-Path -LiteralPath $candidate -PathType Leaf)) { throw "$Label is missing." }
    if ($Container -and -not (Test-Path -LiteralPath $candidate -PathType Container)) { throw "$Label is missing." }
    if ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "$Label is a reparse point." }
    return $candidate
}

$root = (Resolve-Path -LiteralPath $BundleRoot).Path.TrimEnd('\')
if ((Get-Item -LiteralPath $root -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Bundle root must not be a reparse point.' }
$all = @(Get-ChildItem -LiteralPath $root -Recurse -Force)
foreach ($item in $all) {
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Bundle contains a reparse point: '$($item.Name)'." }
    $lower = $item.Name.ToLowerInvariant()
    if ($lower.Contains('.sync-conflict-') -or $lower.StartsWith('.syncthing.')) { throw "Bundle contains a Syncthing conflict/incomplete transfer: '$($item.Name)'." }
    if ($lower.EndsWith('.partial')) { throw "Bundle contains an incomplete publication: '$($item.Name)'." }
    if ($item.Extension -match '^(?i)\.(iso|dvd)$') { throw 'Bundle must never contain the source disc image.' }
}

$requiredTop = @('Launch-MCLA.exe','bin','bundle-files.sha256','bundle-id.txt','bundle-manifest.json','cache','diagnostics','game','game-manifest.json','logs','metadata','results','update','user')
$actualTop = @((Get-ChildItem -LiteralPath $root -Force).Name | Sort-Object)
if (($actualTop -join '|') -cne (($requiredTop | Sort-Object) -join '|')) { throw 'Portable bundle root topology is not exact.' }
foreach ($directory in @('bin','cache','diagnostics','game','logs','metadata','results','update','user')) {
    $null = Resolve-Contained $root $directory "Writable/content root '$directory'" -Container
}

$bundleIdPath = Resolve-Contained $root 'bundle-id.txt' 'Bundle ID' -Leaf
$bundleId = ([IO.File]::ReadAllText($bundleIdPath)).Trim()
if ($bundleId -cnotmatch '^mcla-[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+-win64-[0-9A-F]{16}-[0-9A-F]{16}-[0-9A-F]{16}$') { throw 'Bundle ID is malformed.' }
$manifestPath = Resolve-Contained $root 'bundle-manifest.json' 'Bundle manifest' -Leaf
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
Assert-ExactProperties $manifest @('schema','task','bundle_id','project_version','sdk_version','sdk_commit','created_utc','platform','architecture','launcher','window_mode','immutable_file_count','game','selected_save','writable_roots','retention','private_content') 'Bundle manifest'
if ($manifest.schema -cne 'mcla-portable-campaign-bundle-v1' -or $manifest.task -cne 'M7-016' -or
    $manifest.bundle_id -cne $bundleId -or $manifest.project_version -cnotmatch '^\d+\.\d+\.\d+\.\d+$' -or
    $bundleId -cnotmatch ('^mcla-' + [regex]::Escape([string]$manifest.project_version) + '-win64-') -or
    $manifest.sdk_version -cne '0.10.0.2' -or $manifest.sdk_commit -cne '492614eec92c31f11d75dd8fa0f09785cbae4a66' -or
    $manifest.platform -cne 'windows-proton' -or
    $manifest.architecture -cne 'x86_64' -or $manifest.launcher -cne 'Launch-MCLA.exe') { throw 'Bundle manifest identity is invalid.' }
$createdValue = $manifest.created_utc
if ($createdValue -is [DateTime]) {
    if ($createdValue.Kind -ne [DateTimeKind]::Utc) { throw 'Bundle manifest timestamp must be UTC.' }
} else {
    try { $created = [DateTimeOffset]::Parse([string]$createdValue, [Globalization.CultureInfo]::InvariantCulture) } catch { throw 'Bundle manifest timestamp is invalid.' }
    if ($created.Offset -ne ([TimeSpan]::Zero)) { throw 'Bundle manifest timestamp must be UTC.' }
}
if ((@($manifest.writable_roots) -join '|') -cne 'user|cache|logs|diagnostics|results|update|metadata') { throw 'Bundle writable-root contract drifted.' }
Assert-ExactProperties $manifest.window_mode @('startup_fullscreen','alt_enter_toggle','left_double_click_toggle') 'Bundle window-mode contract'
if ($manifest.window_mode.startup_fullscreen -ne $true -or $manifest.window_mode.alt_enter_toggle -ne $true -or
    $manifest.window_mode.left_double_click_toggle -ne $true) { throw 'Bundle window-mode contract is invalid.' }
Assert-ExactProperties $manifest.retention @('completed_sessions','save_snapshots_per_session') 'Bundle retention'
if ([int]$manifest.retention.completed_sessions -ne 32 -or [int]$manifest.retention.save_snapshots_per_session -ne 32) { throw 'Bundle retention bounds drifted.' }
Assert-ExactProperties $manifest.private_content @('game_and_save_private','source_iso_included') 'Bundle private-content policy'
if ($manifest.private_content.game_and_save_private -ne $true -or $manifest.private_content.source_iso_included -ne $false) { throw 'Bundle private-content policy is invalid.' }

$lockPath = Resolve-Contained $root 'bundle-files.sha256' 'Bundle hash lock' -Leaf
$entries = [ordered]@{}
$lineNumber = 0
foreach ($line in [IO.File]::ReadAllLines($lockPath)) {
    $lineNumber++
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    if ($line -cnotmatch '^([0-9A-F]{64}) \*(.+)$') { throw "Malformed bundle hash-lock row $lineNumber." }
    $relative = $Matches[2].Replace('\','/')
    if ($entries.Contains($relative)) { throw "Duplicate bundle hash-lock path '$relative'." }
    $path = Resolve-Contained $root $relative "Immutable file '$relative'" -Leaf
    $entries[$relative] = [ordered]@{ path=$path; sha256=$Matches[1] }
}
if ([int]$manifest.immutable_file_count -ne $entries.Count) { throw 'Bundle immutable-file count drifted.' }

$gameManifestPath = Resolve-Contained $root 'game-manifest.json' 'Private game manifest' -Leaf
$gameManifest = Get-Content -LiteralPath $gameManifestPath -Raw | ConvertFrom-Json
Assert-ExactProperties $manifest.game @('manifest_sha256','file_count','payload_bytes') 'Bundle game identity'
if ($manifest.game.manifest_sha256 -cne (Get-Sha256 $gameManifestPath) -or
    [int]$manifest.game.file_count -ne [int]$gameManifest.FileCount -or
    [long]$manifest.game.payload_bytes -ne [long]$gameManifest.PayloadBytes) { throw 'Bundle game identity drifted.' }
if (-not $Fixture -and ([int]$gameManifest.SchemaVersion -ne 1 -or [string]$gameManifest.SourceIsoSha256 -cne $expectedIsoHash -or
    [int]$gameManifest.FileCount -ne 15 -or [long]$gameManifest.PayloadBytes -ne 6569586392)) { throw 'Bundle contains an unsupported prepared game identity.' }

$expectedImmutable = @('Launch-MCLA.exe','bundle-id.txt','bundle-manifest.json','game-manifest.json')
$expectedBin = @('mcla.exe','mcla_crash_handler.exe','mcla.toml','rexgpu-xenos.dll','rexruntime.dll','TracyClient.dll')
$actualBin = @((Get-ChildItem -LiteralPath (Join-Path $root 'bin') -File -Force).Name | Sort-Object)
if (($actualBin -join '|') -cne (($expectedBin | Sort-Object) -join '|')) { throw 'Portable runtime binary topology is not exact.' }
$expectedImmutable += @($expectedBin | ForEach-Object { 'bin/' + $_ })
$expectedGamePaths = @()
$payloadBytes = [long]0
foreach ($file in @($gameManifest.Files)) {
    $relative = ([string]$file.Path).Replace('\','/')
    $gamePath = Resolve-Contained (Join-Path $root 'game') $relative "Prepared game file '$relative'" -Leaf
    if ([long](Get-Item -LiteralPath $gamePath).Length -ne [long]$file.Size) { throw "Prepared game size drifted for '$relative'." }
    $payloadBytes += [long]$file.Size
    $expectedGamePaths += 'game/' + $relative
}
if ($expectedGamePaths.Count -ne [int]$gameManifest.FileCount -or $payloadBytes -ne [long]$gameManifest.PayloadBytes) { throw 'Prepared game manifest cardinality/bytes drifted.' }
$actualGameFiles = @(Get-ChildItem -LiteralPath (Join-Path $root 'game') -Recurse -File -Force)
if ($actualGameFiles.Count -ne $expectedGamePaths.Count) { throw 'Prepared game physical file count drifted.' }
$expectedImmutable += $expectedGamePaths
if ((@($entries.Keys | Sort-Object) -join '|') -cne (($expectedImmutable | Sort-Object) -join '|')) { throw 'Bundle hash-lock topology is not exact.' }
foreach ($entry in $entries.GetEnumerator()) {
    if ((Get-Sha256 $entry.Value.path) -cne $entry.Value.sha256) { throw "Bundle immutable SHA-256 drifted for '$($entry.Key)'." }
}

$configPath = Resolve-Contained $root 'bin/mcla.toml' 'Portable host config' -Leaf
$config = [IO.File]::ReadAllText($configPath)
foreach ($required in @('mcla_diagnostics_root = "diagnostics"','log_file = "logs/mcla.log"','fullscreen = true')) {
    if (-not $config.Contains($required)) { throw "Portable config is missing '$required'." }
}
if ($config -match '(?im)^\s*(?:game_data_root|user_data_root|update_data_root|cache_root|metadata_root)\s*=\s*"[^\"]+"' -or
    $config -match '(?i)[A-Z]:\\|C:/BDU/MCLA-Recomp') { throw 'Portable config contains an absolute or repository-bound root.' }

Assert-ExactProperties $manifest.selected_save @('source_snapshot','save_relative','header_relative','save_sha256','header_sha256') 'Selected save'
if ($manifest.selected_save.save_relative -cne $saveRelative -or $manifest.selected_save.header_relative -cne $headerRelative -or
    $manifest.selected_save.source_snapshot -cnotmatch '^private/save-archive/M6-014/.+' -or
    $manifest.selected_save.save_sha256 -cnotmatch '^[0-9A-F]{64}$' -or $manifest.selected_save.header_sha256 -cnotmatch '^[0-9A-F]{64}$') { throw 'Selected save identity is invalid.' }
$savePath = Resolve-Contained $root ('user/' + $saveRelative) 'Selected save payload' -Leaf
$headerPath = Resolve-Contained $root ('user/' + $headerRelative) 'Selected save header' -Leaf
if ((Get-Sha256 $savePath) -cne $manifest.selected_save.save_sha256 -or (Get-Sha256 $headerPath) -cne $manifest.selected_save.header_sha256) { throw 'Selected save physical identity drifted.' }

$textSurface = [IO.File]::ReadAllText($manifestPath) + [IO.File]::ReadAllText($lockPath) + [IO.File]::ReadAllText($configPath)
if ($textSurface -match '(?i)C:\\BDU\\MCLA-Recomp|C:/BDU/MCLA-Recomp') { throw 'Bundle text metadata leaks the repository path.' }

$launcherResult = $null
if ($RunLauncherVerify) {
    $before = @(Get-ChildItem -LiteralPath (Join-Path $root 'results') -File -Filter 'portable-verification-*.json')
    $beforeNames = @($before | ForEach-Object { $_.FullName })
    $process = Start-Process -FilePath (Join-Path $root 'Launch-MCLA.exe') -ArgumentList '--verify-only' -WorkingDirectory $root -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Native portable launcher verification exited $($process.ExitCode)." }
    $after = @(Get-ChildItem -LiteralPath (Join-Path $root 'results') -File -Filter 'portable-verification-*.json')
    $new = @($after | Where-Object { $beforeNames -cnotcontains $_.FullName } | Sort-Object LastWriteTimeUtc -Descending)
    if ($new.Count -ne 1) { throw 'Native launcher did not publish exactly one verification result.' }
    $launcherResult = Get-Content -LiteralPath $new[0].FullName -Raw | ConvertFrom-Json
    Assert-ExactProperties $launcherResult @('schema','bundle_id','verified_utc','verified_file_count','relocatable_root_verified','decision') 'Native launcher verification'
    if ($launcherResult.schema -cne 'mcla-portable-verification-v1' -or $launcherResult.bundle_id -cne $bundleId -or
        [int]$launcherResult.verified_file_count -ne $entries.Count -or $launcherResult.relocatable_root_verified -ne $true -or
        $launcherResult.decision -cne 'portable-bundle-integrity-pass') { throw 'Native launcher verification result is invalid.' }
}

[pscustomobject]@{
    Passed = $true
    Decision = 'portable-campaign-bundle-pass'
    BundleId = $bundleId
    ImmutableFiles = $entries.Count
    GameFiles = $expectedGamePaths.Count
    GameBytes = $payloadBytes
    SelectedSaveSha256 = [string]$manifest.selected_save.save_sha256
    LauncherVerified = [bool]$RunLauncherVerify
}
