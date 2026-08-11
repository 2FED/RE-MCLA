[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-logging-schema.ps1'
$fixtureRoot = Join-Path $repoRoot ('private/test-logging-schema-' + [guid]::NewGuid().ToString('N'))
$utf8 = [System.Text.UTF8Encoding]::new($false)
$positive = '[2026-08-11 14:30:00.000] [info] [app] [t1234] MCLA_LOG_SCHEMA schema=1 category=app event=probe'

function Write-Fixture {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Text)
    [System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
    $path = Join-Path $fixtureRoot "$Name.log"
    [System.IO.File]::WriteAllText($path, $Text + [Environment]::NewLine, $utf8)
    return $path
}

function Assert-Rejected {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Text,
        [string]$ExpectedCategory = 'app'
    )
    $path = Write-Fixture -Name $Name -Text $Text
    try {
        & $verifier -LogPath $path -ExpectedCategory $ExpectedCategory | Out-Null
    } catch {
        return
    }
    throw "Negative logging-schema fixture '$Name' was accepted."
}

try {
    $positivePath = Write-Fixture -Name 'positive' -Text $positive
    $verified = & $verifier -LogPath $positivePath -ExpectedCategory app
    if (-not $verified.Passed -or $verified.Category -ne 'app' -or
        $verified.SchemaMarkers -ne 1) {
        throw 'Positive logging-schema fixture returned an unexpected result.'
    }

    Assert-Rejected -Name 'wrong-category' -Text $positive -ExpectedCategory ppc
    Assert-Rejected -Name 'multiple-markers' -Text ($positive + "`n" + $positive)
    Assert-Rejected -Name 'private-path' -Text ($positive + ' C:\Users\tester\private\log')
    Assert-Rejected -Name 'error-level' -Text $positive.Replace('[info]', '[error]')

    [pscustomobject]@{ Passed = $true; PositiveCases = 1; NegativeCases = 4 }
} finally {
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        [System.IO.Directory]::Delete($fixtureRoot, $true)
    }
}
