[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$TranscriptPath,
    [Parameter(Mandatory)][string]$RuntimeLogPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

foreach ($path in @($TranscriptPath, $RuntimeLogPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required early-init trace input was not found: '$path'."
    }
}

$transcript = Get-Content -LiteralPath $TranscriptPath -Raw
$runtimeLog = Get-Content -LiteralPath $RuntimeLogPath -Raw
if ($transcript -match "(?m)^Couldn't resolve error at" -or
    $transcript -match '(?m)^Syntax error at' -or
    $transcript -match '(?m)^Ambiguous symbol error at' -or
    $transcript -match '(?m)^No matching code symbols found') {
    throw 'CDB could not resolve every requested early-init symbol or command.'
}

$matches = [regex]::Matches(
    $transcript,
    '(?m)^MCLA_EARLY_INIT (time-frequency|system-time|create-thread|start-plan)\r?$'
)
$events = @($matches | ForEach-Object { $_.Groups[1].Value })
if ($events.Count -gt 64) {
    throw "Early-init trace exceeded the reviewed 64-event bound: $($events.Count)."
}
$required = @('time-frequency', 'system-time', 'create-thread', 'start-plan')
foreach ($event in $required) {
    $count = @($events | Where-Object { $_ -eq $event }).Count
    if ($count -lt 1) {
        throw "Required early-init event '$event' was not observed."
    }
}
if (@($events | Where-Object { $_ -eq 'start-plan' }).Count -ne 1) {
    throw 'Expected exactly one terminal guest-thread start-plan event.'
}

$createIndex = [Array]::IndexOf($events, 'create-thread')
$planIndex = [Array]::IndexOf($events, 'start-plan')
if ($createIndex -lt 0 -or $planIndex -le $createIndex) {
    throw 'Guest thread start-plan did not occur after ExCreateThread.'
}
if ($events[-1] -ne 'start-plan') {
    throw 'The debugger trace did not terminate at the first guest-thread start plan.'
}
if ($runtimeLog -match '(?i)\[FATAL\]|invalid or unregistered function|PPC_UNIMPLEMENTED') {
    throw 'Runtime log contains a fatal startup or dispatch failure before the first thread plan.'
}

$firstOccurrence = @($required | Sort-Object {
    [Array]::IndexOf($events, $_)
})
[pscustomobject]@{
    Passed = $true
    Bounded = $true
    EventCount = $events.Count
    FirstOccurrenceSignature = $firstOccurrence -join ' -> '
    CreateThreadHits = @($events | Where-Object { $_ -eq 'create-thread' }).Count
    SystemTimeHits = @($events | Where-Object { $_ -eq 'system-time' }).Count
    PerformanceFrequencyHits = @($events | Where-Object { $_ -eq 'time-frequency' }).Count
    StartPlanHits = 1
}
