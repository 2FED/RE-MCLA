[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-startup-trap-trace.ps1'
$fixtureRoot = Join-Path $repoRoot ('private/test-startup-trap-trace-' + [guid]::NewGuid().ToString('N'))
$fixturePath = Join-Path $fixtureRoot 'mcla.log'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$positive = @'
[info] [gpu] MCLA graphics: selected GPU plugin 'xenos'
[info] [sys] GPU plugin 'xenos' loaded (rexgpu-xenosrd.dll)
[info] [sys] KernelState: Preparing module launch...
[info] [gpu] SetInterruptCallback(82411478, 40002080)
[debug] [gpu] Creating graphics pipeline with VS A, PS B
[debug] [apu] AudioWorker: dispatching callback 823F56D0 with arg 1 for client 0
'@

function Write-Fixture {
    param([Parameter(Mandatory)][string]$Text)
    [System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
    [System.IO.File]::WriteAllText($fixturePath, $Text, $utf8)
}

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Text)
    Write-Fixture $Text
    try {
        & $verifier -RuntimeLogPath $fixturePath | Out-Null
    } catch {
        return
    }
    throw "Negative startup-trap fixture '$Name' was accepted."
}

try {
    Write-Fixture $positive
    $verified = & $verifier -RuntimeLogPath $fixturePath
    if (-not $verified.Passed -or -not $verified.GpuLoaded -or
        -not $verified.AudioCallbackReached) {
        throw 'Positive startup-trap fixture returned an unexpected result.'
    }

    Assert-Rejected missing-selection $positive.Replace(
        "[info] [gpu] MCLA graphics: selected GPU plugin 'xenos'`n", '')
    Assert-Rejected no-gpu ($positive + '[warning] gpu_plugin not set')
    Assert-Rejected invalid-function ($positive + '[FATAL] Call to invalid or unregistered function')
    Assert-Rejected ppc-unimplemented ($positive + 'PPC_UNIMPLEMENTED')
    Assert-Rejected guest-crash ($positive + 'REX_GUEST_CRASH schema=1')
    Assert-Rejected missing-pipeline ($positive -replace '(?m)^.*Creating graphics pipeline.*\r?\n?', '')
    Assert-Rejected post-launch-bink ($positive + 'game:\intro720.bik')

    [pscustomobject]@{ Passed = $true; PositiveCases = 1; NegativeCases = 7 }
} finally {
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        [System.IO.Directory]::Delete($fixtureRoot, $true)
    }
}
