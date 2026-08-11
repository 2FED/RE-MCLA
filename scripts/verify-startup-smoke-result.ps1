[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ResultPath,
    [Parameter(Mandatory)][string]$RuntimeLogPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

foreach ($path in @($ResultPath, $RuntimeLogPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Startup-smoke result input was not found: '$path'."
    }
}
$resultItem = Get-Item -LiteralPath $ResultPath
if ($resultItem.Length -gt 65536) {
    throw 'Startup-smoke result exceeded the reviewed 64-KiB bound.'
}
$result = Get-Content -LiteralPath $ResultPath -Raw | ConvertFrom-Json
$trace = & (Join-Path $PSScriptRoot 'verify-startup-smoke.ps1') -RuntimeLogPath $RuntimeLogPath
$logItem = Get-Item -LiteralPath $RuntimeLogPath
$logHash = (Get-FileHash -LiteralPath $RuntimeLogPath -Algorithm SHA256).Hash

if ($result.schema -ne 1 -or $result.task -ne 'M3-014') {
    throw 'Startup-smoke result schema or task is invalid.'
}
if ($result.startup_timeout_seconds -lt 10 -or $result.startup_timeout_seconds -gt 60 -or
    $result.cleanup_timeout_seconds -ne 5 -or
    $result.startup_elapsed_milliseconds -lt 0 -or
    $result.startup_elapsed_milliseconds -gt ($result.startup_timeout_seconds * 1000) -or
    $result.cleanup_elapsed_milliseconds -lt 0 -or
    $result.cleanup_elapsed_milliseconds -gt ($result.cleanup_timeout_seconds * 1000) -or
    $result.total_elapsed_milliseconds -lt $result.startup_elapsed_milliseconds) {
    throw 'Startup-smoke result contains invalid deadline or elapsed-time values.'
}
if ($result.termination_reason -ne 'expected_markers_reached' -or
    $result.process_exited_early -ne $false -or $result.harness_stop_issued -ne $true -or
    $result.process_signal_confirmed -ne $true -or
    $result.process_cleanup_confirmed -ne $true) {
    throw 'Startup-smoke result does not prove harness-controlled bounded termination.'
}
if ($result.expected_marker_count -ne $trace.MarkerCount -or
    $result.title_id -ne $trace.TitleId -or $result.media_id -ne $trace.MediaId -or
    $result.image_range -ne $trace.ImageRange -or $result.entry_point -ne $trace.EntryPoint -or
    $result.gpu_plugin_loaded -ne $true -or $result.vfs_verified -ne $true -or
    $result.vfs_read_only_verified -ne $true -or $result.module_launch_reached -ne $true -or
    $result.graphics_pipeline_reached -ne $true -or $result.audio_callback_reached -ne $true -or
    $result.fatal_markers -ne 0 -or $result.post_launch_bink_evidence -ne 0) {
    throw 'Startup-smoke result does not match the verified final trace contract.'
}
if ($result.default_xex_size -ne 9252864 -or
    $result.default_xex_sha256 -ne 'C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432' -or
    $result.executable_sha256 -notmatch '^[0-9A-F]{64}$') {
    throw 'Startup-smoke result contains invalid executable or XEX identity.'
}
if ($result.runtime_log_bytes -ne $logItem.Length -or
    $result.runtime_log_sha256 -ne $logHash) {
    throw 'Startup-smoke result does not match the final runtime log bytes/hash.'
}

[pscustomobject]@{
    Passed = $true
    MarkerCount = $trace.MarkerCount
    RuntimeLogBytes = $logItem.Length
    RuntimeLogSha256 = $logHash
    ControlledTerminationVerified = $true
}
