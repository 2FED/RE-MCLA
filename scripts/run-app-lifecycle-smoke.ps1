[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-release',
    [ValidateRange(1, 60)][int]$TimeoutSeconds = 15
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$buildRootPath = if ([System.IO.Path]::IsPathRooted($BuildRoot)) {
    (Resolve-Path -LiteralPath $BuildRoot).Path
} else {
    (Resolve-Path -LiteralPath (Join-Path $repoRoot $BuildRoot)).Path
}
$repoPrefix = $repoRoot.TrimEnd('\') + '\'
if (-not $buildRootPath.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Build root must stay inside the repository: '$buildRootPath'."
}

$executablePath = Join-Path $buildRootPath 'mcla.exe'
if (-not (Test-Path -LiteralPath $executablePath -PathType Leaf)) {
    throw "Native executable was not found: '$executablePath'."
}

$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $repoRoot "private/evidence/M3-002/$runId"
[System.IO.Directory]::CreateDirectory($runRoot) | Out-Null
$logPath = Join-Path $runRoot 'mcla-lifecycle.log'

$process = Start-Process -FilePath $executablePath -ArgumentList @(
    '--mcla_lifecycle_probe',
    "--log_file=$logPath",
    '--log_level=trace'
) -WorkingDirectory $buildRootPath -PassThru

if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $process.Id -Force
    throw "Lifecycle probe exceeded $TimeoutSeconds seconds and was terminated. Private run: '$runRoot'."
}
if ($process.ExitCode -ne 0) {
    throw "Lifecycle probe exited with code $($process.ExitCode). Private run: '$runRoot'."
}
if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
    throw "Lifecycle log was not created: '$logPath'."
}

$markers = @(
    'MCLA lifecycle: logging ready',
    'MCLA lifecycle: probe requested; guest runtime skipped',
    'MCLA lifecycle: probe complete',
    'MCLA lifecycle: shutdown'
)
$logText = Get-Content -LiteralPath $logPath -Raw
$offset = -1
foreach ($marker in $markers) {
    $next = $logText.IndexOf($marker, $offset + 1, [System.StringComparison]::Ordinal)
    if ($next -lt 0) {
        throw "Missing or out-of-order lifecycle marker: '$marker'. Private run: '$runRoot'."
    }
    $offset = $next
}

[pscustomobject]@{
    Passed = $true
    ExitCode = $process.ExitCode
    OrderedMarkers = $markers.Count
    GuestRuntimeSkipped = $true
    PrivateRunRoot = $runRoot
    LogSha256 = (Get-FileHash -LiteralPath $logPath -Algorithm SHA256).Hash
}
