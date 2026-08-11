[CmdletBinding()]
param([Parameter(Mandatory)][string]$RuntimeLogPath)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $RuntimeLogPath -PathType Leaf)) {
    throw "Intro-blocker runtime log was not found: '$RuntimeLogPath'."
}
$logItem = Get-Item -LiteralPath $RuntimeLogPath
if ($logItem.Length -gt 1048576) {
    throw 'Intro-blocker runtime log exceeds the reviewed 1-MiB bound.'
}
$log = Get-Content -LiteralPath $RuntimeLogPath -Raw
$launchMarker = 'KernelState: Preparing module launch...'
$launchOffset = $log.IndexOf($launchMarker, [System.StringComparison]::Ordinal)
if ($launchOffset -lt 0 -or
    $log.IndexOf('MCLA module config: loaded XEX base 82000000, entry 821322B8',
        [System.StringComparison]::Ordinal) -lt 0) {
    throw 'Intro-blocker trace did not reach the verified native module-launch boundary.'
}
$postLaunch = $log.Substring($launchOffset + $launchMarker.Length)
$gpuMarkers = @(
    'VdSetGraphicsInterruptCallback: no GPU emulation loaded (gpu_plugin not set); call ignored',
    'VdInitializeRingBuffer: no GPU emulation loaded (gpu_plugin not set); call ignored'
)
foreach ($marker in $gpuMarkers) {
    if ($postLaunch.IndexOf($marker, [System.StringComparison]::Ordinal) -lt 0) {
        throw "Intro-blocker trace is missing the current GPU prerequisite marker '$marker'."
    }
}
if ($postLaunch -match '(?i)\bBink\b|intro(?:576_16x9|576_4x3|720)[.]bik') {
    throw 'Post-launch Bink/intro evidence exists; the skip-intro decision must be re-evaluated.'
}
if ($log -match '(?i)\[FATAL\]|PPC_UNIMPLEMENTED|REX_GUEST_CRASH') {
    throw 'Intro-blocker trace contains a fatal, unimplemented, or guest-crash marker.'
}

[pscustomobject]@{
    Passed = $true
    Classification = 'gpu-plugin-unconfigured-before-bink'
    ModuleLaunchReached = $true
    GpuPrerequisiteMarkers = $gpuMarkers.Count
    PostLaunchBinkEvidence = $false
    LogBytes = $logItem.Length
}
