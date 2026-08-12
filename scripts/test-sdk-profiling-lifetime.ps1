[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-sdk-profiling-lifetime.ps1'
$sourceRoot = Join-Path $repoRoot 'third_party/rexglue-sdk'
$fixtureRoot = Join-Path $repoRoot ('private/test-sdk-profiling-lifetime-' + [guid]::NewGuid().ToString('N'))
$files = @('include/rex/dbg.h', 'include/rex/hook.h')

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
        $path, $text.Replace($Old, $New), [System.Text.UTF8Encoding]::new($false)
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
    throw "Negative SDK profiling-lifetime fixture '$Name' was accepted."
}

try {
    Reset-Fixture
    $positive = & $verifier -SdkRoot $fixtureRoot
    if (-not $positive.Passed -or $positive.GuardedCpuZones -ne 2 -or
        $positive.GuardedHookZones -ne 1 -or $positive.GuardedAuxiliaryMacros -ne 4) {
        throw 'Positive SDK profiling-lifetime fixture did not return the reviewed contract.'
    }

    Assert-Rejected 'unguarded-hook-zone' {
        Replace-FixtureText 'include/rex/hook.h' `
            'ZoneNamedN(___tracy_hook_zone, #subroutine, TracyIsStarted);' `
            'ZoneNamedN(___tracy_hook_zone, #subroutine, true);'
    }
    Assert-Rejected 'unguarded-cpu-zone' {
        Replace-FixtureText 'include/rex/dbg.h' `
            'ZoneNamedN(___tracy_cpu_zone, name, TracyIsStarted)' `
            'ZoneNamedN(___tracy_cpu_zone, name, true)'
    }
    Assert-Rejected 'unguarded-thread-name' {
        Replace-FixtureText 'include/rex/dbg.h' 'if (TracyIsStarted) {' 'if (true) {'
    }

    [pscustomobject]@{ Passed = $true; PositiveCases = 1; NegativeCases = 3 }
} finally {
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        [System.IO.Directory]::Delete($fixtureRoot, $true)
    }
}
