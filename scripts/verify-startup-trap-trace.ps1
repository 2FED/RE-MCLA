[CmdletBinding()]
param([Parameter(Mandatory)][string]$RuntimeLogPath)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $RuntimeLogPath -PathType Leaf)) {
    throw "Startup-trap runtime log was not found: '$RuntimeLogPath'."
}
$item = Get-Item -LiteralPath $RuntimeLogPath
if ($item.Length -gt 1048576) {
    throw 'Startup-trap runtime log exceeded the reviewed 1-MiB bound.'
}
$log = Get-Content -LiteralPath $RuntimeLogPath -Raw
$markers = [ordered]@{
    selection = "MCLA graphics: selected GPU plugin 'xenos'"
    loaded = "GPU plugin 'xenos' loaded (rexgpu-xenosrd.dll)"
    launch = 'KernelState: Preparing module launch...'
    interrupt = 'SetInterruptCallback('
    pipeline = 'Creating graphics pipeline with VS '
    audio = 'AudioWorker: dispatching callback '
}
$positions = [ordered]@{}
foreach ($entry in $markers.GetEnumerator()) {
    $position = $log.IndexOf($entry.Value, [System.StringComparison]::Ordinal)
    if ($position -lt 0) {
        throw "Required startup-trap marker '$($entry.Key)' is missing."
    }
    $positions[$entry.Key] = $position
}
$orderedPositions = @($positions.Values)
for ($index = 1; $index -lt $orderedPositions.Count; $index++) {
    if ($orderedPositions[$index] -le $orderedPositions[$index - 1]) {
        throw 'Startup-trap markers are out of order.'
    }
}

$bannedPatterns = @(
    '(?i)gpu_plugin not set',
    '(?i)\[FATAL\]',
    '(?i)invalid or unregistered function',
    '(?i)PPC_UNIMPLEMENTED',
    '(?i)REX_GUEST_CRASH'
)
foreach ($pattern in $bannedPatterns) {
    if ($log -match $pattern) {
        throw "Startup-trap trace contains banned failure pattern '$pattern'."
    }
}
$postLaunch = $log.Substring([int]$positions.launch)
if ($postLaunch -match '(?i)Bink|[.]bik') {
    throw 'Bink/intro evidence appeared after module launch; M3-011 must be re-evaluated.'
}

[pscustomobject]@{
    Passed = $true
    GpuSelected = $true
    GpuLoaded = $true
    ModuleLaunchReached = $true
    GraphicsPipelineReached = $true
    AudioCallbackReached = $true
    FatalMarkers = 0
    PostLaunchBinkEvidence = 0
    LogBytes = $item.Length
}
