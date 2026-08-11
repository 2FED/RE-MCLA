[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifierPath = Join-Path $PSScriptRoot 'verify-early-init-trace.ps1'
$fixtureRoot = Join-Path $repoRoot 'private/evidence/M3-008/trace-verifier-fixtures'
[System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
$utf8 = [System.Text.UTF8Encoding]::new($false)

function Write-Fixture {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string[]]$Events,
        [string]$Log = '[info] early initialization'
    )
    $root = Join-Path $fixtureRoot $Name
    [System.IO.Directory]::CreateDirectory($root) | Out-Null
    $transcriptPath = Join-Path $root 'cdb.txt'
    $logPath = Join-Path $root 'mcla.log'
    [System.IO.File]::WriteAllLines(
        $transcriptPath,
        @($Events | ForEach-Object { "MCLA_EARLY_INIT $_" }),
        $utf8
    )
    [System.IO.File]::WriteAllText($logPath, $Log + [Environment]::NewLine, $utf8)
    return [pscustomobject]@{ Transcript = $transcriptPath; Log = $logPath }
}

$positive = Write-Fixture -Name 'positive' -Events @(
    'create-thread', 'create-thread', 'system-time', 'time-frequency', 'start-plan'
)
$verified = & $verifierPath -TranscriptPath $positive.Transcript -RuntimeLogPath $positive.Log
if (-not $verified.Passed -or $verified.CreateThreadHits -ne 2 -or
    $verified.FirstOccurrenceSignature -ne
        'create-thread -> system-time -> time-frequency -> start-plan') {
    throw 'Positive early-init trace fixture did not return the expected result.'
}

function Assert-Rejected {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string[]]$Events,
        [string]$Log = '[info] early initialization'
    )
    $fixture = Write-Fixture -Name $Name -Events $Events -Log $Log
    try {
        & $verifierPath -TranscriptPath $fixture.Transcript -RuntimeLogPath $fixture.Log | Out-Null
    } catch {
        return
    }
    throw "Negative early-init trace fixture '$Name' was accepted."
}

Assert-Rejected -Name 'missing-frequency' -Events @(
    'create-thread', 'system-time', 'start-plan'
)
Assert-Rejected -Name 'plan-before-create' -Events @(
    'start-plan', 'create-thread', 'system-time', 'time-frequency'
)
Assert-Rejected -Name 'fatal-runtime' -Events @(
    'create-thread', 'system-time', 'time-frequency', 'start-plan'
) -Log '[FATAL] synthetic failure'

[pscustomobject]@{ Passed = $true; Cases = 4 }
