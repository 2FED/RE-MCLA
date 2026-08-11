[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game',
    [ValidateRange(10, 60)][int]$ObservationSeconds = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$traceVerifier = Join-Path $PSScriptRoot 'verify-startup-trap-trace.ps1'

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
foreach ($path in @($executablePath, (Join-Path $buildRootPath 'rexgpu-xenosrd.dll'),
        (Join-Path $gameRootPath 'default.xex'), $traceVerifier)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required startup-trap input was not found: '$path'."
    }
}

$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $repoRoot "private/evidence/M3-013/$runId"
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
    '--log_level=trace',
    '--fullscreen=false'
) -WorkingDirectory $buildRootPath -PassThru

$exitedEarly = $process.WaitForExit($ObservationSeconds * 1000)
if ($exitedEarly) {
    throw "Xenos startup trace exited early with code $($process.ExitCode). Private run: '$runRoot'."
}
Stop-Process -Id $process.Id -Force
$process.WaitForExit()
if (Get-Process -Id $process.Id -ErrorAction SilentlyContinue) {
    throw "Startup trace process $($process.Id) survived forced cleanup. Private run: '$runRoot'."
}

$trace = & $traceVerifier -RuntimeLogPath $logPath
$result = [ordered]@{
    schema = 1
    task = 'M3-013'
    observation_seconds = $ObservationSeconds
    process_exited_early = $false
    process_cleanup_confirmed = $true
    gpu_selected_by_project_default = $trace.GpuSelected
    gpu_plugin_loaded = $trace.GpuLoaded
    module_launch_reached = $trace.ModuleLaunchReached
    graphics_pipeline_reached = $trace.GraphicsPipelineReached
    audio_callback_reached = $trace.AudioCallbackReached
    fatal_markers = $trace.FatalMarkers
    post_launch_bink_evidence = $trace.PostLaunchBinkEvidence
    runtime_log_sha256 = (Get-FileHash -LiteralPath $logPath -Algorithm SHA256).Hash
}
[System.IO.File]::WriteAllText(
    $resultPath,
    (($result | ConvertTo-Json -Depth 4) + [Environment]::NewLine),
    [System.Text.UTF8Encoding]::new($false)
)

[pscustomobject]@{
    Passed = $true
    GpuLoaded = $trace.GpuLoaded
    GraphicsPipelineReached = $trace.GraphicsPipelineReached
    AudioCallbackReached = $trace.AudioCallbackReached
    ProcessCleanupConfirmed = $true
    PrivateRunRoot = $runRoot
    ResultPath = $resultPath
}
