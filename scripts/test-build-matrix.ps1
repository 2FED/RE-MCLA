[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-build-matrix.ps1'
$fixtureRoot = Join-Path $repoRoot ('private/test-build-matrix-' + [guid]::NewGuid().ToString('N'))
$fixturePath = Join-Path $fixtureRoot 'result.json'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$hash = 'A' * 64

function New-Configuration {
    param([string]$Name, [string]$Preset, [string]$Runtime, [string]$Tracy, [string]$Gpu)
    return [ordered]@{
        name = $Name
        preset = $Preset
        build_root = "out/build/$Preset"
        configure_exit_code = 0
        clean_build_exit_code = 0
        configure_duration_ms = 1
        clean_build_duration_ms = 1
        generated_object_count = 65
        executable_sha256 = $hash
        runtime_dll = $Runtime
        runtime_sha256 = $hash
        tracy_dll = $Tracy
        tracy_sha256 = $hash
        gpu_plugin_dll = $Gpu
        gpu_plugin_sha256 = $hash
    }
}

$baseline = [ordered]@{
    schema = 1
    task = 'M3-012'
    sdk_version = '0.9.0.25'
    generated_cpp_expected = 65
    configurations = @(
        (New-Configuration Debug win-amd64-debug rexruntimed.dll TracyClientd.dll rexgpu-xenosd.dll),
        (New-Configuration RelWithDebInfo win-amd64-relwithdebinfo rexruntimerd.dll TracyClientrd.dll rexgpu-xenosrd.dll),
        (New-Configuration Release win-amd64-release rexruntime.dll TracyClient.dll rexgpu-xenos.dll)
    )
}

function Write-Fixture {
    param([Parameter(Mandatory)]$Value)
    [System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
    [System.IO.File]::WriteAllText(
        $fixturePath,
        (($Value | ConvertTo-Json -Depth 8) + [Environment]::NewLine),
        $utf8
    )
}

function Copy-Baseline {
    return ($baseline | ConvertTo-Json -Depth 8 | ConvertFrom-Json)
}

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)]$Value)
    Write-Fixture $Value
    try {
        & $verifier -ResultPath $fixturePath -SkipArtifactCheck | Out-Null
    } catch {
        return
    }
    throw "Negative build-matrix fixture '$Name' was accepted."
}

try {
    Write-Fixture $baseline
    $verified = & $verifier -ResultPath $fixturePath -SkipArtifactCheck
    if (-not $verified.Passed -or $verified.Configurations -ne 3 -or $verified.CleanBuilds -ne 3) {
        throw 'Positive build-matrix fixture returned an unexpected result.'
    }

    $fixture = Copy-Baseline; $fixture.task = 'M3-999'
    Assert-Rejected wrong-task $fixture
    $fixture = Copy-Baseline; $fixture.configurations = @($fixture.configurations | Select-Object -First 2)
    Assert-Rejected missing-configuration $fixture
    $fixture = Copy-Baseline; $fixture.configurations[0].gpu_plugin_dll = 'rexgpu-xenos.dll'
    Assert-Rejected cross-configuration-gpu $fixture
    $fixture = Copy-Baseline; $fixture.configurations[1].clean_build_exit_code = 1
    Assert-Rejected failed-build $fixture
    $fixture = Copy-Baseline; $fixture.configurations[2].executable_sha256 = 'BAD'
    Assert-Rejected invalid-hash $fixture
    $fixture = Copy-Baseline; $fixture.configurations[0].generated_object_count = 64
    Assert-Rejected wrong-object-count $fixture

    [pscustomobject]@{ Passed = $true; PositiveCases = 1; NegativeCases = 6 }
} finally {
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        [System.IO.Directory]::Delete($fixtureRoot, $true)
    }
}
