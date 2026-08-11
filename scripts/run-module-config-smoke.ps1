[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-release',
    [string]$GameRoot = 'private/game',
    [ValidateRange(1, 120)][int]$TimeoutSeconds = 30
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
function Resolve-ContainedPath {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Description)
    $resolved = if ([System.IO.Path]::IsPathRooted($Path)) {
        (Resolve-Path -LiteralPath $Path).Path
    } else {
        (Resolve-Path -LiteralPath (Join-Path $repoRoot $Path)).Path
    }
    $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay inside the repository: '$resolved'."
    }
    return $resolved
}

$buildRootPath = Resolve-ContainedPath -Path $BuildRoot -Description 'Build root'
$gameRootPath = Resolve-ContainedPath -Path $GameRoot -Description 'Game root'
$executablePath = Join-Path $buildRootPath 'mcla.exe'
$xexPath = Join-Path $gameRootPath 'default.xex'
foreach ($path in @($executablePath, $xexPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required module-config probe input was not found: '$path'."
    }
}

$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $repoRoot "private/evidence/M3-003/$runId"
$userRoot = Join-Path $runRoot 'user'
$cacheRoot = Join-Path $runRoot 'cache'
[System.IO.Directory]::CreateDirectory($userRoot) | Out-Null
[System.IO.Directory]::CreateDirectory($cacheRoot) | Out-Null
$logPath = Join-Path $runRoot 'mcla-module-config.log'

$process = Start-Process -FilePath $executablePath -ArgumentList @(
    '--mcla_module_config_probe',
    "--game_data_root=$gameRootPath",
    "--user_data_root=$userRoot",
    "--cache_root=$cacheRoot",
    "--log_file=$logPath",
    '--log_level=trace'
) -WorkingDirectory $buildRootPath -PassThru

if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $process.Id -Force
    throw "Module-config probe exceeded $TimeoutSeconds seconds. Private run: '$runRoot'."
}
if ($process.ExitCode -ne 0) {
    throw "Module-config probe exited with code $($process.ExitCode). Private run: '$runRoot'."
}
if (-not (Test-Path -LiteralPath $logPath -PathType Leaf)) {
    throw "Module-config log was not created: '$logPath'."
}

$markers = @(
    'MCLA module config: static image 82000000-829E0000, code 82130000-827CD054',
    'MCLA module config: loaded XEX base 82000000, entry 821322B8',
    'MCLA module config: entry 821322B8 registered in dispatch range 82130000-827CD054',
    'MCLA module config: probe complete; guest launch skipped',
    'MCLA lifecycle: shutdown'
)
$logText = Get-Content -LiteralPath $logPath -Raw
$offset = -1
foreach ($marker in $markers) {
    $next = $logText.IndexOf($marker, $offset + 1, [System.StringComparison]::Ordinal)
    if ($next -lt 0) {
        throw "Missing or out-of-order module-config marker: '$marker'. Private run: '$runRoot'."
    }
    $offset = $next
}
if ($logText.IndexOf('Execution complete', [System.StringComparison]::Ordinal) -ge 0) {
    throw "Module-config probe unexpectedly entered guest execution. Private run: '$runRoot'."
}

[pscustomobject]@{
    Passed = $true
    ExitCode = $process.ExitCode
    OrderedMarkers = $markers.Count
    GuestLaunchSkipped = $true
    PrivateRunRoot = $runRoot
    LogSha256 = (Get-FileHash -LiteralPath $logPath -Algorithm SHA256).Hash
}
