[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-intro-blocker-trace.ps1'
$fixtureRoot = Join-Path $repoRoot ('private/test-intro-blocker-' + [guid]::NewGuid().ToString('N'))
$utf8 = [System.Text.UTF8Encoding]::new($false)
$positive = @'
MCLA module config: loaded XEX base 82000000, entry 821322B8
KernelState: Preparing module launch...
VdSetGraphicsInterruptCallback: no GPU emulation loaded (gpu_plugin not set); call ignored
VdInitializeRingBuffer: no GPU emulation loaded (gpu_plugin not set); call ignored
'@

function Write-Fixture {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Text)
    [System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
    $path = Join-Path $fixtureRoot "$Name.log"
    [System.IO.File]::WriteAllText($path, $Text, $utf8)
    return $path
}

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Text)
    $path = Write-Fixture -Name $Name -Text $Text
    try {
        & $verifier -RuntimeLogPath $path | Out-Null
    } catch {
        return
    }
    throw "Negative intro-blocker fixture '$Name' was accepted."
}

try {
    $verified = & $verifier -RuntimeLogPath (Write-Fixture -Name 'positive' -Text $positive)
    if (-not $verified.Passed -or $verified.PostLaunchBinkEvidence -or
        $verified.Classification -ne 'gpu-plugin-unconfigured-before-bink') {
        throw 'Positive intro-blocker fixture returned an unexpected classification.'
    }

    Assert-Rejected -Name 'missing-launch' -Text $positive.Replace(
        'KernelState: Preparing module launch...', 'KernelState: not launched')
    Assert-Rejected -Name 'missing-gpu-marker' -Text $positive.Replace(
        'VdInitializeRingBuffer: no GPU emulation loaded (gpu_plugin not set); call ignored', '')
    Assert-Rejected -Name 'post-launch-bink' -Text ($positive + "`nBink failed to open intro720.bik")
    Assert-Rejected -Name 'guest-crash' -Text ($positive + "`nREX_GUEST_CRASH schema=1")

    [pscustomobject]@{ Passed = $true; PositiveCases = 1; NegativeCases = 4 }
} finally {
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        [System.IO.Directory]::Delete($fixtureRoot, $true)
    }
}
