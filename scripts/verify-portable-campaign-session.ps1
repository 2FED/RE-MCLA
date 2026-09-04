[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BundleRoot,
    [Parameter(Mandatory=$true)][string]$SessionPath,
    [switch]$RequireDiagnosticsProbe
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
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

function Exact($Value,[string[]]$Names,[string]$Label) {
    if ((@($Value.PSObject.Properties.Name|Sort-Object)-join'|') -cne (@($Names|Sort-Object)-join'|')) { throw "$Label property topology drifted." }
}

$root = (Resolve-Path -LiteralPath $BundleRoot).Path.TrimEnd('\')
$session = (Resolve-Path -LiteralPath $SessionPath).Path.TrimEnd('\')
if (-not $session.StartsWith((Join-Path $root 'results').TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or
    (Split-Path -Leaf $session) -cnotmatch '^session-\d{8}T\d{6}Z-\d+$') { throw 'Session path is outside the bundle or malformed.' }
foreach ($item in @(Get-Item -LiteralPath $root -Force) + @(Get-Item -LiteralPath $session -Force) + @(Get-ChildItem -LiteralPath $session -Recurse -Force)) {
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Portable session contains a reparse point.' }
    if ($item.Name.ToLowerInvariant().EndsWith('.partial')) { throw 'Portable session contains an incomplete publication.' }
}
if (Test-Path -LiteralPath (Join-Path $root 'bundle.lock')) { throw 'Portable bundle still has an active/stale writer lock.' }

$bundleId = ([IO.File]::ReadAllText((Join-Path $root 'bundle-id.txt'))).Trim()
$immutableCount = @([IO.File]::ReadAllLines((Join-Path $root 'bundle-files.sha256')) | Where-Object { $_ }).Count
$started = Get-Content -LiteralPath (Join-Path $session 'session.json') -Raw | ConvertFrom-Json
$result = Get-Content -LiteralPath (Join-Path $session 'result.json') -Raw | ConvertFrom-Json
Exact $started @('schema','bundle_id','session_id','started_utc','mode','state') 'Portable running-session manifest'
Exact $result @('schema','bundle_id','session_id','started_utc','completed_utc','mode','exit_code','verified_file_count','save_snapshot_count','save_watcher_error_count','final_save_sha256','final_header_sha256','state') 'Portable session result'
$sessionId = Split-Path -Leaf $session
$startedText = [string]$started.started_utc
$completedText = [string]$result.completed_utc
if ($startedText -cnotmatch '^\d{8}T\d{6}Z$' -or $completedText -cnotmatch '^\d{8}T\d{6}Z$' -or
    $sessionId -cnotmatch ('^session-' + [regex]::Escape($startedText) + '-\d+$')) { throw 'Portable session timestamp identity is malformed.' }
$startedAt = [DateTime]::ParseExact($startedText,'yyyyMMddTHHmmssZ',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::AssumeUniversal -bor [Globalization.DateTimeStyles]::AdjustToUniversal)
$completedAt = [DateTime]::ParseExact($completedText,'yyyyMMddTHHmmssZ',[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::AssumeUniversal -bor [Globalization.DateTimeStyles]::AdjustToUniversal)
if ($completedAt -lt $startedAt) { throw 'Portable session completion predates its start.' }
if ($started.schema -cne 'mcla-portable-session-v1' -or $result.schema -cne 'mcla-portable-session-result-v1' -or
    $started.bundle_id -cne $bundleId -or $result.bundle_id -cne $bundleId -or
    $started.session_id -cne $sessionId -or $result.session_id -cne $sessionId -or
    $started.started_utc -cne $result.started_utc -or $started.state -cne 'running' -or $result.state -cne 'complete' -or
    $started.mode -cne $result.mode -or [int]$result.exit_code -ne 0 -or [int]$result.verified_file_count -ne $immutableCount -or
    [int]$result.save_watcher_error_count -ne 0) { throw 'Portable session identity/lifecycle contract failed.' }

$snapshotsRoot = Join-Path $session 'saves'
$snapshots = @(if (Test-Path -LiteralPath $snapshotsRoot -PathType Container) { Get-ChildItem -LiteralPath $snapshotsRoot -Directory -Force })
if ($snapshots.Count -ne [int]$result.save_snapshot_count -or $snapshots.Count -lt 1 -or $snapshots.Count -gt 32) { throw 'Portable save-snapshot count drifted.' }
foreach ($snapshot in $snapshots) {
    if ($snapshot.Name -cnotmatch '^[0-9A-F]{16}$') { throw 'Portable save-snapshot name is malformed.' }
    $manifest = Get-Content -LiteralPath (Join-Path $snapshot.FullName 'snapshot.json') -Raw | ConvertFrom-Json
    Exact $manifest @('schema','captured_utc','reason','save_sha256','header_sha256','complete_profile_tree') 'Portable save snapshot'
    $save = Join-Path $snapshot.FullName $saveRelative.Replace('/','\')
    $header = Join-Path $snapshot.FullName $headerRelative.Replace('/','\')
    if ($manifest.schema -cne 'mcla-portable-save-snapshot-v1' -or [string]$manifest.captured_utc -cnotmatch '^\d{8}T\d{6}Z$' -or $manifest.complete_profile_tree -ne $true -or
        $manifest.save_sha256 -cnotmatch '^[0-9A-F]{64}$' -or $manifest.header_sha256 -cnotmatch '^[0-9A-F]{64}$' -or
        -not (Test-Path -LiteralPath $save -PathType Leaf) -or -not (Test-Path -LiteralPath $header -PathType Leaf) -or
        (Get-Sha256 $save) -cne $manifest.save_sha256 -or (Get-Sha256 $header) -cne $manifest.header_sha256) { throw 'Portable save snapshot physical identity failed.' }
}

$finalSave = Join-Path (Join-Path $root 'user') $saveRelative.Replace('/','\')
$finalHeader = Join-Path (Join-Path $root 'user') $headerRelative.Replace('/','\')
if (-not (Test-Path -LiteralPath $finalSave -PathType Leaf) -or -not (Test-Path -LiteralPath $finalHeader -PathType Leaf) -or
    (Get-Sha256 $finalSave) -cne [string]$result.final_save_sha256 -or (Get-Sha256 $finalHeader) -cne [string]$result.final_header_sha256) { throw 'Portable final save identity drifted.' }

$diagnosticPackage = $null
if ($RequireDiagnosticsProbe) {
    if ($result.mode -cne 'diagnostics-probe') { throw 'Portable session is not a diagnostics probe.' }
    $pointer = Join-Path $root 'diagnostics\latest-live.txt'
    if (-not (Test-Path -LiteralPath $pointer -PathType Leaf)) { throw 'Portable diagnostics latest pointer is missing.' }
    $name = ([IO.File]::ReadAllText($pointer)).Trim()
    if ($name -cnotmatch '^live-\d{8}T\d{6}Z-\d+-\d+$') { throw 'Portable diagnostics latest pointer is malformed.' }
    $diagnosticPackage = Join-Path $root ('diagnostics\live\' + $name)
    $diagnosticRoot = [IO.Path]::GetFullPath((Join-Path $root 'diagnostics\live'))
    $diagnosticPackage = [IO.Path]::GetFullPath($diagnosticPackage)
    if (-not $diagnosticPackage.StartsWith($diagnosticRoot.TrimEnd('\') + '\',[StringComparison]::OrdinalIgnoreCase) -or
        -not (Test-Path -LiteralPath $diagnosticPackage -PathType Container)) { throw 'Portable diagnostics package path escaped.' }
    foreach ($item in @(Get-Item -LiteralPath $diagnosticPackage -Force) + @(Get-ChildItem -LiteralPath $diagnosticPackage -Recurse -Force)) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Portable diagnostics package contains a reparse point.' }
    }
    $manifest = Get-Content -LiteralPath (Join-Path $diagnosticPackage 'manifest.json') -Raw | ConvertFrom-Json
    if ($manifest.schema -cne 'mcla-diagnostic-package-v1' -or $manifest.kind -cne 'live' -or
        $manifest.privacy.automatic_upload -ne $false -or $manifest.privacy.package_safe_to_share -ne $false) { throw 'Portable diagnostics privacy/identity contract failed.' }
    foreach ($artifact in @($manifest.artifacts)) {
        $artifactPath = [IO.Path]::GetFullPath((Join-Path $diagnosticPackage ([string]$artifact.name)))
        if (-not $artifactPath.StartsWith($diagnosticPackage.TrimEnd('\') + '\',[StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-Path -LiteralPath $artifactPath -PathType Leaf) -or
            ((Get-Item -LiteralPath $artifactPath -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -or
            (Get-Sha256 $artifactPath) -cne [string]$artifact.sha256) { throw 'Portable diagnostics artifact identity drifted.' }
    }
    $logs = @(Get-ChildItem -LiteralPath (Join-Path $root 'logs') -File -Filter 'mcla*.log')
    if (-not $logs.Count) { throw 'Portable sibling logs root is empty.' }
    $logText = ($logs | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join "`n"
    if (-not $logText.Contains('MCLA_DIAGNOSTIC_SNAPSHOT_PROBE v=1 queued=1 completed=1 status=PASS') -or
        $logText -match '(?i)\[FATAL\]|PPC_UNIMPLEMENTED|invalid or unregistered function|device removed|DXGI_ERROR_DEVICE_REMOVED') { throw 'Portable diagnostics-probe log contract failed.' }
}

[pscustomobject]@{Passed=$true;Decision='portable-campaign-session-pass';BundleId=$bundleId;SessionId=$sessionId;Mode=[string]$result.mode;SaveSnapshots=$snapshots.Count;DiagnosticsVerified=[bool]$RequireDiagnosticsProbe;DiagnosticPackage=$diagnosticPackage}
