[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game',
    [ValidateRange(5, 60)][int]$TimeoutSeconds = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifierPath = Join-Path $PSScriptRoot 'verify-crash-report.ps1'

function Resolve-ContainedPath {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Description)
    $candidate = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repoRoot $Path }
    $resolved = (Resolve-Path -LiteralPath $candidate).Path
    $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay inside the repository: '$resolved'."
    }
    return $resolved
}

$buildRootPath = Resolve-ContainedPath -Path $BuildRoot -Description 'Build root'
$gameRootPath = Resolve-ContainedPath -Path $GameRoot -Description 'Game root'
$executablePath = Join-Path $buildRootPath 'mcla.exe'
foreach ($path in @($executablePath, (Join-Path $gameRootPath 'default.xex'), $verifierPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required crash-probe input was not found: '$path'."
    }
}

$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $repoRoot "private/evidence/M3-009/$runId"
$userRoot = Join-Path $runRoot 'user'
$cacheRoot = Join-Path $runRoot 'cache'
[System.IO.Directory]::CreateDirectory($userRoot) | Out-Null
[System.IO.Directory]::CreateDirectory($cacheRoot) | Out-Null
$logPath = Join-Path $runRoot 'mcla.log'
$reportPath = Join-Path $userRoot 'mcla-crash-report.txt'
$resultPath = Join-Path $runRoot 'result.json'

$process = Start-Process -FilePath $executablePath -ArgumentList @(
    '--mcla_crash_probe',
    "--game_data_root=$gameRootPath",
    "--user_data_root=$userRoot",
    "--cache_root=$cacheRoot",
    "--log_file=$logPath",
    '--log_level=trace'
) -WorkingDirectory $buildRootPath -PassThru

if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $process.Id -Force
    throw "Crash-report probe exceeded $TimeoutSeconds seconds. Private run: '$runRoot'."
}
if ($process.ExitCode -ne 0) {
    throw "Crash-report probe exited with code $($process.ExitCode). Private run: '$runRoot'."
}

$verified = & $verifierPath -ReportPath $reportPath -RuntimeLogPath $logPath
$result = [ordered]@{
    schema = 1
    task = 'M3-009'
    exit_code = $process.ExitCode
    report_schema = $verified.Schema
    required_fields = $verified.RequiredFields
    host_stack_frames = $verified.HostStackFrames
    guest_memory_included = $verified.GuestMemoryIncluded
    report_sha256 = (Get-FileHash -LiteralPath $reportPath -Algorithm SHA256).Hash
    runtime_log_sha256 = (Get-FileHash -LiteralPath $logPath -Algorithm SHA256).Hash
}
[System.IO.File]::WriteAllText(
    $resultPath,
    (($result | ConvertTo-Json -Depth 4) + [Environment]::NewLine),
    [System.Text.UTF8Encoding]::new($false)
)

[pscustomobject]@{
    Passed = $true
    ExitCode = $process.ExitCode
    HostStackFrames = $verified.HostStackFrames
    GuestMemoryIncluded = $verified.GuestMemoryIncluded
    PrivateRunRoot = $runRoot
    ResultPath = $resultPath
}
