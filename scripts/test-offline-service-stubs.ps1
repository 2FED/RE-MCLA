[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifierPath = Join-Path $PSScriptRoot 'verify-offline-service-stubs.ps1'
$sdkRoot = Join-Path $repoRoot 'third_party/rexglue-sdk'
$coveragePath = Join-Path $repoRoot 'docs/evidence/M2-013-import-coverage.md'
$fixtureRoot = Join-Path $repoRoot 'private/evidence/M3-007/verifier-fixtures'

$positive = & $verifierPath -SdkRoot $sdkRoot -ImportCoveragePath $coveragePath
if (-not $positive.Passed -or $positive.ImportsReviewed -ne 10 -or
    $positive.GenericStubs -ne 0 -or $positive.Assertions -ne 10) {
    throw 'Positive offline-service verifier run did not return the expected counts.'
}

function New-FixtureSdk {
    param([Parameter(Mandatory)][string]$Name)
    $root = Join-Path $fixtureRoot $Name
    foreach ($relative in @(
        'src/kernel/xam/xam_misc.cpp',
        'src/kernel/xam/xam_net.cpp',
        'src/kernel/xam/xam_ui.cpp',
        'src/kernel/xam/xam_voice.cpp',
        'tests/unit/kernel/offline_service_test.cpp',
        'tests/unit/CMakeLists.txt'
    )) {
        $destination = Join-Path $root $relative
        [System.IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
        [System.IO.File]::Copy((Join-Path $sdkRoot $relative), $destination, $true)
    }
    return $root
}

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Mutation)
    $fixtureSdk = New-FixtureSdk -Name $Name
    & $Mutation $fixtureSdk
    try {
        & $verifierPath -SdkRoot $fixtureSdk -ImportCoveragePath $coveragePath | Out-Null
    } catch {
        return
    }
    throw "Negative offline-service fixture '$Name' was accepted."
}

Assert-Rejected -Name 'generic-stub' -Mutation {
    param($fixtureSdk)
    $path = Join-Path $fixtureSdk 'src/kernel/xam/xam_ui.cpp'
    $source = Get-Content -LiteralPath $path -Raw
    $source = $source.Replace(
        'REX_EXPORT(__imp__XamShowFriendsUI, rex::kernel::xam::XamShowFriendsUIOffline_entry)',
        'REX_EXPORT_STUB(__imp__XamShowFriendsUI);'
    )
    [System.IO.File]::WriteAllText($path, $source, [System.Text.UTF8Encoding]::new($false))
}
Assert-Rejected -Name 'missing-offline-log' -Mutation {
    param($fixtureSdk)
    $path = Join-Path $fixtureSdk 'src/kernel/xam/xam_voice.cpp'
    $source = Get-Content -LiteralPath $path -Raw
    $source = $source.Replace('[OFFLINE] XamVoiceHeadsetPresent', 'XamVoiceHeadsetPresent')
    [System.IO.File]::WriteAllText($path, $source, [System.Text.UTF8Encoding]::new($false))
}
Assert-Rejected -Name 'wrong-return' -Mutation {
    param($fixtureSdk)
    $path = Join-Path $fixtureSdk 'src/kernel/xam/xam_net.cpp'
    $source = Get-Content -LiteralPath $path -Raw
    $source = $source.Replace(
        '  return kXNetConnectStatusLost;',
        '  return 0;'
    )
    [System.IO.File]::WriteAllText($path, $source, [System.Text.UTF8Encoding]::new($false))
}

[pscustomobject]@{ Passed = $true; Cases = 4 }
