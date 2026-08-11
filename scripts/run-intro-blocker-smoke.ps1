[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game',
    [ValidateRange(5, 60)][int]$ObservationSeconds = 15
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$traceVerifier = Join-Path $PSScriptRoot 'verify-intro-blocker-trace.ps1'
$decisionVerifier = Join-Path $PSScriptRoot 'verify-skip-intro-decision.ps1'

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
foreach ($path in @($executablePath, (Join-Path $gameRootPath 'default.xex'),
        $traceVerifier, $decisionVerifier)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required intro-blocker input was not found: '$path'."
    }
}

$decision = & $decisionVerifier
$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $repoRoot "private/evidence/M3-011/$runId"
$userRoot = Join-Path $runRoot 'user'
$cacheRoot = Join-Path $runRoot 'cache'
[System.IO.Directory]::CreateDirectory($userRoot) | Out-Null
[System.IO.Directory]::CreateDirectory($cacheRoot) | Out-Null
$logPath = Join-Path $runRoot 'mcla.log'
$resultPath = Join-Path $runRoot 'result.json'

$process = Start-Process -FilePath $executablePath -ArgumentList @(
    "--game_data_root=$gameRootPath",
    "--user_data_root=$userRoot",
    "--cache_root=$cacheRoot",
    "--log_file=$logPath",
    '--log_level=trace'
) -WorkingDirectory $buildRootPath -PassThru

$exitedEarly = $process.WaitForExit($ObservationSeconds * 1000)
if ($exitedEarly) {
    throw "Unpatched native observation exited early with code $($process.ExitCode). Private run: '$runRoot'."
}
Stop-Process -Id $process.Id -Force
$process.WaitForExit()
if (Get-Process -Id $process.Id -ErrorAction SilentlyContinue) {
    throw "Native observation process $($process.Id) survived forced cleanup. Private run: '$runRoot'."
}

$trace = & $traceVerifier -RuntimeLogPath $logPath
$result = [ordered]@{
    schema = 1
    task = 'M3-011'
    observation_seconds = $ObservationSeconds
    process_exited_early = $false
    process_cleanup_confirmed = $true
    classification = $trace.Classification
    module_launch_reached = $trace.ModuleLaunchReached
    gpu_prerequisite_markers = $trace.GpuPrerequisiteMarkers
    post_launch_bink_evidence = $trace.PostLaunchBinkEvidence
    patch_implemented = $decision.PatchImplemented
    patch_enabled = $decision.PatchEnabled
    runtime_log_sha256 = (Get-FileHash -LiteralPath $logPath -Algorithm SHA256).Hash
}
[System.IO.File]::WriteAllText(
    $resultPath,
    (($result | ConvertTo-Json -Depth 4) + [Environment]::NewLine),
    [System.Text.UTF8Encoding]::new($false)
)

[pscustomobject]@{
    Passed = $true
    Classification = $trace.Classification
    PostLaunchBinkEvidence = $trace.PostLaunchBinkEvidence
    PatchImplemented = $decision.PatchImplemented
    ProcessCleanupConfirmed = $true
    PrivateRunRoot = $runRoot
    ResultPath = $resultPath
}
