[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-crash-report-contract.ps1'
$sourceRoot = Join-Path $repoRoot 'third_party/rexglue-sdk'
$fixtureRoot = Join-Path $repoRoot ('private/test-crash-report-contract-' + [guid]::NewGuid().ToString('N'))
$files = @(
    'include/rex/ppc/context.h',
    'include/rex/hook.h',
    'include/rex/system/crash_report.h',
    'src/system/crash_report.cpp',
    'src/system/xthread.cpp',
    'src/codegen/function_graph.cpp',
    'resources/templates/codegen/init_h.inja',
    'src/system/CMakeLists.txt',
    'tests/unit/CMakeLists.txt'
)

function Reset-Fixture {
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        [System.IO.Directory]::Delete($fixtureRoot, $true)
    }
    foreach ($relative in $files) {
        $target = Join-Path $fixtureRoot $relative
        [System.IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
        [System.IO.File]::Copy((Join-Path $sourceRoot $relative), $target)
    }
}

function Replace-FixtureText {
    param(
        [Parameter(Mandatory)][string]$RelativePath,
        [Parameter(Mandatory)][string]$Old,
        [Parameter(Mandatory)][string]$New
    )
    $path = Join-Path $fixtureRoot $RelativePath
    $text = Get-Content -LiteralPath $path -Raw
    if (-not $text.Contains($Old)) {
        throw "Fixture mutation input was not found in '$RelativePath'."
    }
    [System.IO.File]::WriteAllText(
        $path,
        $text.Replace($Old, $New),
        [System.Text.UTF8Encoding]::new($false)
    )
}

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Mutation)
    Reset-Fixture
    & $Mutation
    try {
        & $verifier -SdkRoot $fixtureRoot | Out-Null
    } catch {
        return
    }
    throw "Negative crash-report contract fixture '$Name' was accepted."
}

try {
    Reset-Fixture
    $positive = & $verifier -SdkRoot $fixtureRoot
    if (-not $positive.Passed -or $positive.MaxHostFrames -ne 16 -or
        $positive.GuestMemoryIncludedByDefault) {
        throw 'Positive crash-report contract fixture did not return the reviewed result.'
    }

    Assert-Rejected 'missing-xthread-catch' {
        Replace-FixtureText 'src/system/xthread.cpp' 'catch (const std::exception& error)' 'catch (const int& error)'
    }
    Assert-Rejected 'guest-memory-default-enabled' {
        Replace-FixtureText 'include/rex/system/crash_report.h' 'bool guest_memory_included = false;' 'bool guest_memory_included = true;'
    }
    Assert-Rejected 'missing-generated-pc' {
        Replace-FixtureText 'src/codegen/function_graph.cpp' 'rex::ppc::SetGuestProgramCounter' 'rex::ppc::DiscardedGuestProgramCounter'
    }
    Assert-Rejected 'missing-raw-hook-breadcrumb' {
        Replace-FixtureText 'include/rex/hook.h' 'RecordGuestImport(ctx, #name)' 'DiscardGuestImport(ctx, #name)'
    }

    [pscustomobject]@{ Passed = $true; PositiveCases = 1; NegativeCases = 4 }
} finally {
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        [System.IO.Directory]::Delete($fixtureRoot, $true)
    }
}
