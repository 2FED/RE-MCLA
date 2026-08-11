[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-crash-report.ps1'
$fixtureRoot = Join-Path $repoRoot 'private/evidence/M3-009/verifier-fixtures'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$positiveReport = @'
REX_GUEST_CRASH schema=1
reason=MCLA synthetic crash probe
guest_pc=0x821322BC
ppc_function=sub_821322B8
thread_id=0x4D434C41
last_import=__imp__XGetAVPack
host_stack_count=2
host_stack[0]=mcla.exe+0x1234 (0x7FF612341234)
host_stack[1]=rexruntimerd.dll+0x5678 (0x7FF856785678)
guest_memory_included=false
'@
$positiveLog = @'
MCLA module config: loaded XEX base 82000000, entry 821322B8
MCLA crash probe: privacy-safe report written
MCLA crash probe: complete; guest launch skipped
MCLA lifecycle: shutdown
'@

function Write-Fixture {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Report,
        [string]$Log = $positiveLog
    )
    $root = Join-Path $fixtureRoot $Name
    [System.IO.Directory]::CreateDirectory($root) | Out-Null
    $reportPath = Join-Path $root 'mcla-crash-report.txt'
    $logPath = Join-Path $root 'mcla.log'
    [System.IO.File]::WriteAllText($reportPath, $Report, $utf8)
    [System.IO.File]::WriteAllText($logPath, $Log, $utf8)
    return [pscustomobject]@{ Report = $reportPath; Log = $logPath }
}

$positive = Write-Fixture -Name 'positive' -Report $positiveReport
$verified = & $verifier -ReportPath $positive.Report -RuntimeLogPath $positive.Log
if (-not $verified.Passed -or $verified.HostStackFrames -ne 2 -or
    $verified.GuestMemoryIncluded) {
    throw 'Positive crash-report fixture did not return the reviewed result.'
}

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Report)
    $fixture = Write-Fixture -Name $Name -Report $Report
    try {
        & $verifier -ReportPath $fixture.Report -RuntimeLogPath $fixture.Log | Out-Null
    } catch {
        return
    }
    throw "Negative crash-report fixture '$Name' was accepted."
}

Assert-Rejected -Name 'missing-guest-pc' -Report $positiveReport.Replace(
    "guest_pc=0x821322BC`n", '')
Assert-Rejected -Name 'guest-memory-enabled' -Report $positiveReport.Replace(
    'guest_memory_included=false', 'guest_memory_included=true')
Assert-Rejected -Name 'private-path' -Report $positiveReport.Replace(
    'reason=MCLA synthetic crash probe', 'reason=C:\Users\tester\private\dump.bin')
Assert-Rejected -Name 'wrong-stack-count' -Report $positiveReport.Replace(
    'host_stack_count=2', 'host_stack_count=3')

[pscustomobject]@{ Passed = $true; Cases = 5 }
