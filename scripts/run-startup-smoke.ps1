[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game',
    [ValidateRange(10, 60)][int]$StartupTimeoutSeconds = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$traceVerifier = Join-Path $PSScriptRoot 'verify-startup-smoke.ps1'
$resultVerifier = Join-Path $PSScriptRoot 'verify-startup-smoke-result.ps1'

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
$xexPath = Join-Path $gameRootPath 'default.xex'
foreach ($path in @($executablePath, (Join-Path $buildRootPath 'rexgpu-xenosrd.dll'),
        $xexPath, $traceVerifier, $resultVerifier)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required startup-smoke input was not found: '$path'."
    }
}
$xexItem = Get-Item -LiteralPath $xexPath
$xexHash = (Get-FileHash -LiteralPath $xexPath -Algorithm SHA256).Hash
if ($xexItem.Length -ne 9252864 -or
    $xexHash -ne 'C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432') {
    throw 'The startup smoke requires the verified Complete Edition default.xex (size/hash mismatch).'
}
if (@(Get-Process -Name mcla -ErrorAction SilentlyContinue).Count -ne 0) {
    throw 'An mcla process is already running; close it before the serialized startup smoke.'
}

$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $repoRoot "private/evidence/M3-014/$runId"
$userRoot = Join-Path $runRoot 'user'
$cacheRoot = Join-Path $runRoot 'cache'
[System.IO.Directory]::CreateDirectory($userRoot) | Out-Null
[System.IO.Directory]::CreateDirectory($cacheRoot) | Out-Null
$logPath = Join-Path $runRoot 'mcla.log'
$resultPath = Join-Path $runRoot 'result.json'
$process = $null
$trace = $null
$cleanupConfirmed = $false
$cleanupTimeoutMilliseconds = 5000
$harnessStopIssued = $false
$processSignalConfirmed = $false
$startupElapsedMilliseconds = 0
$cleanupElapsedMilliseconds = 0
$failureRecord = $null
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

try {
    $process = Start-Process -FilePath $executablePath -ArgumentList @(
        "--game_data_root=$gameRootPath",
        "--user_data_root=$userRoot",
        "--cache_root=$cacheRoot",
        "--log_file=$logPath",
        '--log_level=trace',
        '--fullscreen=false'
    ) -WorkingDirectory $buildRootPath -PassThru

    while ($stopwatch.Elapsed.TotalSeconds -lt $StartupTimeoutSeconds) {
        if ($process.HasExited) {
            throw "Startup smoke exited early with code $($process.ExitCode). Private run: '$runRoot'."
        }
        if (Test-Path -LiteralPath $logPath -PathType Leaf) {
            $text = Get-Content -LiteralPath $logPath -Raw
            if ($text.IndexOf('AudioWorker: dispatching callback ',
                    [System.StringComparison]::Ordinal) -ge 0) {
                $trace = & $traceVerifier -RuntimeLogPath $logPath
                $startupElapsedMilliseconds = $stopwatch.ElapsedMilliseconds
                break
            }
        }
        Start-Sleep -Milliseconds 250
    }
    if (-not $trace) {
        throw "Startup smoke did not reach all expected markers within $StartupTimeoutSeconds seconds. Private run: '$runRoot'."
    }
    if ($process.HasExited) {
        throw "Startup smoke exited after its final marker but before controlled termination. Private run: '$runRoot'."
    }
    $cleanupStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        Stop-Process -Id $process.Id -Force -ErrorAction Stop
        $harnessStopIssued = $true
    } catch {
        if ($process.HasExited) {
            throw "Startup smoke exited before the harness could terminate it. Private run: '$runRoot'."
        }
        throw
    }
    $processSignalConfirmed = $process.WaitForExit($cleanupTimeoutMilliseconds)
    if (-not $processSignalConfirmed) {
        throw "Startup-smoke process did not signal exit within the cleanup deadline. Private run: '$runRoot'."
    }
    $cleanupConfirmed = -not [bool](Get-Process -Id $process.Id -ErrorAction SilentlyContinue)
    if (-not $cleanupConfirmed) {
        throw "Startup-smoke process cleanup was not confirmed. Private run: '$runRoot'."
    }
    $cleanupStopwatch.Stop()
    $cleanupElapsedMilliseconds = $cleanupStopwatch.ElapsedMilliseconds
    $trace = & $traceVerifier -RuntimeLogPath $logPath
} catch {
    $failureRecord = $_
} finally {
    $stopwatch.Stop()
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        [void]$process.WaitForExit($cleanupTimeoutMilliseconds)
    }
    if ($process) {
        $cleanupConfirmed = -not [bool](Get-Process -Id $process.Id -ErrorAction SilentlyContinue)
    }
}
if ($failureRecord) {
    if ($process -and -not $cleanupConfirmed) {
        throw "Startup smoke failed and its owned process could not be cleaned up. Original failure: $($failureRecord.Exception.Message)"
    }
    throw $failureRecord
}
if (-not $harnessStopIssued -or -not $processSignalConfirmed -or -not $cleanupConfirmed) {
    throw "Startup-smoke controlled-termination contract was not satisfied. Private run: '$runRoot'."
}

$result = [ordered]@{
    schema = 1
    task = 'M3-014'
    startup_timeout_seconds = $StartupTimeoutSeconds
    cleanup_timeout_seconds = [int]($cleanupTimeoutMilliseconds / 1000)
    startup_elapsed_milliseconds = $startupElapsedMilliseconds
    cleanup_elapsed_milliseconds = $cleanupElapsedMilliseconds
    total_elapsed_milliseconds = $stopwatch.ElapsedMilliseconds
    termination_reason = 'expected_markers_reached'
    harness_stop_issued = $true
    process_signal_confirmed = $true
    process_exited_early = $false
    process_cleanup_confirmed = $true
    expected_marker_count = $trace.MarkerCount
    title_id = $trace.TitleId
    media_id = $trace.MediaId
    image_range = $trace.ImageRange
    entry_point = $trace.EntryPoint
    gpu_plugin_loaded = $trace.GpuLoaded
    vfs_verified = $trace.VfsVerified
    vfs_read_only_verified = $trace.VfsReadOnlyVerified
    module_launch_reached = $trace.ModuleLaunchReached
    graphics_pipeline_reached = $trace.GraphicsPipelineReached
    audio_callback_reached = $trace.AudioCallbackReached
    fatal_markers = $trace.FatalMarkers
    post_launch_bink_evidence = $trace.PostLaunchBinkEvidence
    executable_sha256 = (Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash
    default_xex_size = $xexItem.Length
    default_xex_sha256 = $xexHash
    runtime_log_sha256 = (Get-FileHash -LiteralPath $logPath -Algorithm SHA256).Hash
    runtime_log_bytes = $trace.LogBytes
}
[System.IO.File]::WriteAllText(
    $resultPath,
    (($result | ConvertTo-Json -Depth 4) + [Environment]::NewLine),
    [System.Text.UTF8Encoding]::new($false)
)
$verifiedResult = & $resultVerifier -ResultPath $resultPath -RuntimeLogPath $logPath

[pscustomobject]@{
    Passed = $verifiedResult.Passed
    StartupElapsedMilliseconds = $startupElapsedMilliseconds
    TotalElapsedMilliseconds = $stopwatch.ElapsedMilliseconds
    ExpectedMarkerCount = $trace.MarkerCount
    TitleId = $trace.TitleId
    MediaId = $trace.MediaId
    ProcessCleanupConfirmed = $true
    PrivateRunRoot = $runRoot
    ResultPath = $resultPath
}
