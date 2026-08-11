[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ReportPath,
    [Parameter(Mandatory)][string]$RuntimeLogPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

foreach ($path in @($ReportPath, $RuntimeLogPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required crash-report input was not found: '$path'."
    }
}
if ((Get-Item -LiteralPath $ReportPath).Length -gt 16384) {
    throw 'Crash report exceeds the reviewed 16-KiB metadata bound.'
}

$report = Get-Content -LiteralPath $ReportPath -Raw
$log = Get-Content -LiteralPath $RuntimeLogPath -Raw
$exactFields = @(
    'REX_GUEST_CRASH schema=1',
    'reason=MCLA synthetic crash probe',
    'guest_pc=0x821322BC',
    'ppc_function=sub_821322B8',
    'thread_id=0x4D434C41',
    'last_import=__imp__XGetAVPack',
    'guest_memory_included=false'
)
foreach ($field in $exactFields) {
    if ([regex]::Matches($report, '(?m)^' + [regex]::Escape($field) + '\r?$').Count -ne 1) {
        throw "Crash report does not contain exactly one reviewed field '$field'."
    }
}

$countMatch = [regex]::Match($report, '(?m)^host_stack_count=([0-9]+)\r?$')
if (-not $countMatch.Success) {
    throw 'Crash report does not contain a host_stack_count field.'
}
$hostStackCount = [int]$countMatch.Groups[1].Value
if ($hostStackCount -lt 1 -or $hostStackCount -gt 16) {
    throw "Host stack count is outside the reviewed 1..16 bound: $hostStackCount."
}
$frames = [regex]::Matches(
    $report,
    '(?m)^host_stack\[([0-9]+)\]=([A-Za-z0-9_.-]+)\+0x[0-9A-Fa-f]+ \(0x[0-9A-Fa-f]+\)\r?$'
)
if ($frames.Count -ne $hostStackCount) {
    throw "Host stack declares $hostStackCount frames but contains $($frames.Count)."
}
for ($i = 0; $i -lt $frames.Count; $i++) {
    if ([int]$frames[$i].Groups[1].Value -ne $i) {
        throw 'Host stack frame indices are not contiguous and zero-based.'
    }
}

$bannedPatterns = @(
    '(?i)guest_stack',
    '(?i)memory_dump',
    '(?i)registers\s*=',
    '(?i)(?:^|\s)r1\s*=',
    '(?i)[A-Z]:\\',
    '(?i)\\Users\\',
    '(?i)\\private\\'
)
foreach ($pattern in $bannedPatterns) {
    if ($report -match $pattern) {
        throw "Crash report contains banned private-data pattern '$pattern'."
    }
}

$markers = @(
    'MCLA module config: loaded XEX base 82000000, entry 821322B8',
    'MCLA crash probe: privacy-safe report written',
    'MCLA crash probe: complete; guest launch skipped',
    'MCLA lifecycle: shutdown'
)
$offset = -1
foreach ($marker in $markers) {
    $next = $log.IndexOf($marker, $offset + 1, [System.StringComparison]::Ordinal)
    if ($next -lt 0) {
        throw "Missing or out-of-order crash-probe marker '$marker'."
    }
    $offset = $next
}
if ($log -match '(?i)\[FATAL\]|PPC_UNIMPLEMENTED|Execution complete') {
    throw 'Crash probe unexpectedly entered guest execution or logged a fatal failure.'
}

[pscustomobject]@{
    Passed = $true
    Schema = 1
    RequiredFields = $exactFields.Count
    HostStackFrames = $hostStackCount
    GuestMemoryIncluded = $false
    OrderedMarkers = $markers.Count
}
