[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-logging-contract.ps1'
$fixtureRoot = Join-Path $repoRoot ('private/test-logging-contract-' + [guid]::NewGuid().ToString('N'))
$files = @('CMakeLists.txt', 'src/mcla_logging.h', 'src/mcla_logging.cpp', 'src/mcla_app.cpp')

function Reset-Fixture {
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        [System.IO.Directory]::Delete($fixtureRoot, $true)
    }
    foreach ($relative in $files) {
        $target = Join-Path $fixtureRoot $relative
        [System.IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
        [System.IO.File]::Copy((Join-Path $repoRoot $relative), $target)
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
        & $verifier -ProjectRoot $fixtureRoot | Out-Null
    } catch {
        return
    }
    throw "Negative logging-contract fixture '$Name' was accepted."
}

try {
    Reset-Fixture
    $positive = & $verifier -ProjectRoot $fixtureRoot
    if (-not $positive.Passed -or $positive.Categories -ne 9 -or
        $positive.DefaultOverride -ne 'inherit' -or $positive.ProbeDefault) {
        throw 'Positive logging-contract fixture returned an unexpected result.'
    }

    Assert-Rejected 'missing-category' {
        Replace-FixtureText 'src/mcla_logging.cpp' 'Register("audio")' 'Register("missing_audio")'
    }
    Assert-Rejected 'probe-default-on' {
        Replace-FixtureText 'src/mcla_app.cpp' 'mcla_logging_probe, false' 'mcla_logging_probe, true'
    }
    Assert-Rejected 'override-default-not-inherit' {
        Replace-FixtureText 'src/mcla_app.cpp' 'mcla_log_##name, "inherit"' 'mcla_log_##name, "info"'
    }
    Assert-Rejected 'missing-filter-application' {
        Replace-FixtureText 'src/mcla_app.cpp' 'REXCVAR_GET(mcla_log_xam)' 'REXCVAR_GET(mcla_log_app)'
    }
    Assert-Rejected 'generic-log-regression' {
        Replace-FixtureText 'src/mcla_app.cpp' 'MCLA_APP_INFO("MCLA lifecycle: shutdown")' 'REXLOG_INFO("MCLA lifecycle: shutdown")'
    }

    [pscustomobject]@{ Passed = $true; PositiveCases = 1; NegativeCases = 5 }
} finally {
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        [System.IO.Directory]::Delete($fixtureRoot, $true)
    }
}
