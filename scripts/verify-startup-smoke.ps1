[CmdletBinding()]
param([Parameter(Mandatory)][string]$RuntimeLogPath)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $RuntimeLogPath -PathType Leaf)) {
    throw "Startup-smoke runtime log was not found: '$RuntimeLogPath'."
}
$item = Get-Item -LiteralPath $RuntimeLogPath
if ($item.Length -gt 1048576) {
    throw 'Startup-smoke runtime log exceeded the reviewed 1-MiB bound.'
}
$log = Get-Content -LiteralPath $RuntimeLogPath -Raw
$markers = [ordered]@{
    lifecycle = 'MCLA lifecycle: logging ready'
    gpu_selection = "MCLA graphics: selected GPU plugin 'xenos'"
    gpu_loaded = "GPU plugin 'xenos' loaded (rexgpu-xenosrd.dll)"
    static_image = 'MCLA module config: static image 82000000-829E0000, code 82130000-827CD054, 30034 mappings'
    runtime = 'Runtime initialized successfully'
    xex_load = 'Loading XEX image: game:\default.xex'
    identity = 'MCLA module identity: title 545407F8, media 5940C9DB, image 82000000-829E0000, entry 821322B8'
    dispatch = 'MCLA module config: entry 821322B8 registered in dispatch range 82130000-827CD054'
    vfs = 'MCLA VFS: game: and d: resolve 3/3 expected disc files'
    vfs_read_only = 'MCLA VFS: write, create, delete, and writable-map requests denied'
    launch = 'KernelState: Preparing module launch...'
    title = 'Initializing shader storage for title 545407F8...'
    interrupt = 'SetInterruptCallback('
    pipeline = 'Creating graphics pipeline with VS '
    audio = 'AudioWorker: dispatching callback '
}
$positions = [ordered]@{}
foreach ($entry in $markers.GetEnumerator()) {
    $position = $log.IndexOf($entry.Value, [System.StringComparison]::Ordinal)
    if ($position -lt 0) {
        throw "Required startup-smoke marker '$($entry.Key)' is missing."
    }
    $positions[$entry.Key] = $position
}
$orderedPositions = @($positions.Values)
for ($index = 1; $index -lt $orderedPositions.Count; $index++) {
    if ($orderedPositions[$index] -le $orderedPositions[$index - 1]) {
        throw 'Startup-smoke markers are out of order.'
    }
}

$bannedPatterns = @(
    '(?i)gpu_plugin not set',
    '(?i)\[(?:fatal|critical)\]',
    '(?i)invalid or unregistered function',
    '(?i)PPC_UNIMPLEMENTED',
    '(?i)REX_GUEST_CRASH',
    '(?i)GPU plugin .* failed to load',
    '(?i)Failed to load GPU plugin',
    '(?i)loaded image contract rejected',
    '(?i)disc-root contract rejected'
)
foreach ($pattern in $bannedPatterns) {
    if ($log -match $pattern) {
        throw "Startup smoke contains banned failure pattern '$pattern'."
    }
}
$postLaunch = $log.Substring([int]$positions.launch)
if ($postLaunch -match '(?i)Bink|[.]bik') {
    throw 'Bink/intro evidence appeared after module launch; M3-011 must be re-evaluated.'
}

[pscustomobject]@{
    Passed = $true
    MarkerCount = $markers.Count
    TitleId = '545407F8'
    MediaId = '5940C9DB'
    ImageRange = '82000000-829E0000'
    EntryPoint = '821322B8'
    GpuLoaded = $true
    VfsVerified = $true
    VfsReadOnlyVerified = $true
    ModuleLaunchReached = $true
    GraphicsPipelineReached = $true
    AudioCallbackReached = $true
    FatalMarkers = 0
    PostLaunchBinkEvidence = 0
    LogBytes = $item.Length
}
