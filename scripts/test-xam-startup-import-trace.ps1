[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifierPath = Join-Path $PSScriptRoot 'verify-xam-startup-import-trace.ps1'
$fixtureRoot = Join-Path $repoRoot 'private/evidence/M3-006/verifier-fixtures'
[System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
$positiveTranscript = Join-Path $fixtureRoot 'positive-cdb.txt'
$positiveRuntime = Join-Path $fixtureRoot 'positive-runtime.log'

[System.IO.File]::WriteAllLines(
    $positiveTranscript,
    @(
        'MCLA_XAM_IMPORT XGetAVPack',
        'MCLA_XAM_RESULT XGetAVPack 0x6',
        'MCLA_BOUNDARY title-main'
    ),
    [System.Text.UTF8Encoding]::new($false)
)
[System.IO.File]::WriteAllText(
    $positiveRuntime,
    "[INFO] clean startup`n",
    [System.Text.UTF8Encoding]::new($false)
)

$positive = & $verifierPath -TranscriptPath $positiveTranscript -RuntimeLogPath $positiveRuntime
if (-not $positive.Passed -or $positive.FunctionImportsReached -ne 1 -or
    $positive.FunctionImportsReviewed -ne 4) {
    throw 'Positive XAM startup verifier fixture did not pass with the expected counts.'
}

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Mutation)
    $transcript = Join-Path $fixtureRoot "$Name-cdb.txt"
    $runtime = Join-Path $fixtureRoot "$Name-runtime.log"
    [System.IO.File]::Copy($positiveTranscript, $transcript, $true)
    [System.IO.File]::Copy($positiveRuntime, $runtime, $true)
    & $Mutation $transcript $runtime
    try {
        & $verifierPath -TranscriptPath $transcript -RuntimeLogPath $runtime | Out-Null
    } catch {
        return
    }
    throw "Negative verifier fixture '$Name' was accepted."
}

Assert-Rejected -Name 'missing-boundary' -Mutation {
    param($transcript, $runtime)
    (Get-Content -LiteralPath $transcript) |
        Where-Object { $_ -ne 'MCLA_BOUNDARY title-main' } |
        Set-Content -LiteralPath $transcript -Encoding utf8
}
Assert-Rejected -Name 'missing-function' -Mutation {
    param($transcript, $runtime)
    (Get-Content -LiteralPath $transcript) |
        Where-Object { $_ -ne 'MCLA_XAM_IMPORT XGetAVPack' } |
        Set-Content -LiteralPath $transcript -Encoding utf8
}
Assert-Rejected -Name 'wrong-av-pack' -Mutation {
    param($transcript, $runtime)
    (Get-Content -LiteralPath $transcript -Raw).Replace(
        'MCLA_XAM_RESULT XGetAVPack 0x6',
        'MCLA_XAM_RESULT XGetAVPack 0x5'
    ) | Set-Content -LiteralPath $transcript -Encoding utf8
}
Assert-Rejected -Name 'unexpected-language' -Mutation {
    param($transcript, $runtime)
    Add-Content -LiteralPath $transcript -Value 'MCLA_XAM_IMPORT XGetLanguage'
}
Assert-Rejected -Name 'message-box-stub' -Mutation {
    param($transcript, $runtime)
    Add-Content -LiteralPath $runtime -Value '[STUB] XamShowMessageBoxUIEx - not implemented'
}
Assert-Rejected -Name 'fatal-runtime' -Mutation {
    param($transcript, $runtime)
    Add-Content -LiteralPath $runtime -Value '[FATAL] invalid or unregistered function'
}

[pscustomobject]@{ Passed = $true; Cases = 7 }
