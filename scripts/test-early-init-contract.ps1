[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifierPath = Join-Path $PSScriptRoot 'verify-early-init-contract.ps1'
$sdkRoot = Join-Path $repoRoot 'third_party/rexglue-sdk'
$coveragePath = Join-Path $repoRoot 'docs/evidence/M2-013-import-coverage.md'
$fixtureRoot = Join-Path $repoRoot 'private/evidence/M3-008/verifier-fixtures'
$fixtureFiles = @(
    'src/system/runtime.cpp',
    'src/codegen/builders/system.cpp',
    'resources/templates/codegen/init_h.inja',
    'src/kernel/xboxkrnl/xboxkrnl_threading.cpp',
    'include/rex/system/xthread.h',
    'src/system/xthread.cpp',
    'tests/unit/CMakeLists.txt',
    'tests/unit/core/chrono_test.cpp',
    'tests/unit/system/thread_start_test.cpp'
)

$positive = & $verifierPath -SdkRoot $sdkRoot -ImportCoveragePath $coveragePath
if (-not $positive.Passed -or $positive.ImportsReviewed -ne 3 -or
    $positive.TimebaseChecks -ne 7 -or $positive.ThreadStartChecks -ne 10 -or
    $positive.RegressionCases -ne 4) {
    throw 'Positive early-init verifier run did not return the expected contract counts.'
}

function New-FixtureSdk {
    param([Parameter(Mandatory)][string]$Name)
    $root = Join-Path $fixtureRoot $Name
    foreach ($relative in $fixtureFiles) {
        $destination = Join-Path $root $relative
        [System.IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
        [System.IO.File]::Copy((Join-Path $sdkRoot $relative), $destination, $true)
    }
    return $root
}

function Set-FixtureText {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Old,
        [Parameter(Mandatory)][string]$New
    )
    $source = Get-Content -LiteralPath $Path -Raw
    if (-not $source.Contains($Old)) {
        throw "Fixture mutation source token was not found in '$Path'."
    }
    [System.IO.File]::WriteAllText(
        $Path,
        $source.Replace($Old, $New),
        [System.Text.UTF8Encoding]::new($false)
    )
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
    throw "Negative early-init fixture '$Name' was accepted."
}

Assert-Rejected -Name 'wrong-frequency' -Mutation {
    param($fixtureSdk)
    Set-FixtureText -Path (Join-Path $fixtureSdk 'src/system/runtime.cpp') `
        -Old 'chrono::Clock::set_guest_tick_frequency(50000000);' `
        -New 'chrono::Clock::set_guest_tick_frequency(49000000);'
}
Assert-Rejected -Name 'missing-startup-delay' -Mutation {
    param($fixtureSdk)
    Set-FixtureText -Path (Join-Path $fixtureSdk 'src/system/xthread.cpp') `
        -Old 'rex::thread::Sleep(kGuestThreadStartupDelay);' `
        -New 'rex::thread::MaybeYield();'
}
Assert-Rejected -Name 'reordered-apc-delivery' -Mutation {
    param($fixtureSdk)
    $path = Join-Path $fixtureSdk 'src/system/xthread.cpp'
    $source = Get-Content -LiteralPath $path -Raw
    $old = @'
  rex::thread::Sleep(kGuestThreadStartupDelay);

  // Dispatch any APCs that were queued before the thread was created first.
  DeliverAPCs();
'@
    $new = @'
  DeliverAPCs();
  rex::thread::Sleep(kGuestThreadStartupDelay);
'@
    if (-not $source.Contains($old)) {
        throw 'Fixture mutation could not locate the reviewed delay/APC sequence.'
    }
    [System.IO.File]::WriteAllText(
        $path,
        $source.Replace($old, $new),
        [System.Text.UTF8Encoding]::new($false)
    )
}
Assert-Rejected -Name 'wrong-raw-context' -Mutation {
    param($fixtureSdk)
    Set-FixtureText -Path (Join-Path $fixtureSdk 'src/system/xthread.cpp') `
        -Old '{params.start_context, 0}' `
        -New '{params.start_address, 0}'
}

[pscustomobject]@{ Passed = $true; Cases = 5 }
