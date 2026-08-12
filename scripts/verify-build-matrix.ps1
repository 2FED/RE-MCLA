[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ResultPath,
    [switch]$SkipArtifactCheck
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not (Test-Path -LiteralPath $ResultPath -PathType Leaf)) {
    throw "Build-matrix result was not found: '$ResultPath'."
}

$result = Get-Content -LiteralPath $ResultPath -Raw | ConvertFrom-Json
if ($result.schema -ne 1 -or $result.task -ne 'M3-012' -or
    $result.sdk_version -ne '0.9.0.12' -or $result.generated_cpp_expected -ne 65) {
    throw 'Build-matrix result header is invalid.'
}

$expected = @(
    [pscustomobject]@{ Name = 'Debug'; Preset = 'win-amd64-debug'; Runtime = 'rexruntimed.dll'; Tracy = 'TracyClientd.dll'; Gpu = 'rexgpu-xenosd.dll' },
    [pscustomobject]@{ Name = 'RelWithDebInfo'; Preset = 'win-amd64-relwithdebinfo'; Runtime = 'rexruntimerd.dll'; Tracy = 'TracyClientrd.dll'; Gpu = 'rexgpu-xenosrd.dll' },
    [pscustomobject]@{ Name = 'Release'; Preset = 'win-amd64-release'; Runtime = 'rexruntime.dll'; Tracy = 'TracyClient.dll'; Gpu = 'rexgpu-xenos.dll' }
)
$configurations = @($result.configurations)
if ($configurations.Count -ne $expected.Count) {
    throw "Expected exactly three build configurations, found $($configurations.Count)."
}

$hashPattern = '^[0-9A-Fa-f]{64}$'
$knownStagedDlls = @(
    'rexruntimed.dll', 'rexruntimerd.dll', 'rexruntime.dll',
    'TracyClientd.dll', 'TracyClientrd.dll', 'TracyClient.dll',
    'rexgpu-xenosd.dll', 'rexgpu-xenosrd.dll', 'rexgpu-xenos.dll'
)
for ($index = 0; $index -lt $expected.Count; $index++) {
    $actual = $configurations[$index]
    $wanted = $expected[$index]
    if ($actual.name -ne $wanted.Name -or $actual.preset -ne $wanted.Preset -or
        $actual.configure_exit_code -ne 0 -or $actual.clean_build_exit_code -ne 0 -or
        $actual.generated_object_count -ne 65) {
        throw "Build result for '$($wanted.Name)' is incomplete or out of order."
    }
    if ($actual.runtime_dll -ne $wanted.Runtime -or $actual.tracy_dll -ne $wanted.Tracy -or
        $actual.gpu_plugin_dll -ne $wanted.Gpu) {
        throw "Build result for '$($wanted.Name)' names the wrong configuration artifacts."
    }
    foreach ($property in @('executable_sha256', 'runtime_sha256', 'tracy_sha256', 'gpu_plugin_sha256')) {
        if ([string]$actual.$property -notmatch $hashPattern) {
            throw "Build result for '$($wanted.Name)' has an invalid $property."
        }
    }
    if ($actual.configure_duration_ms -lt 0 -or $actual.clean_build_duration_ms -le 0) {
        throw "Build result for '$($wanted.Name)' has invalid timing data."
    }

    if ($SkipArtifactCheck) { continue }
    $candidate = Join-Path $repoRoot ([string]$actual.build_root)
    $buildRoot = (Resolve-Path -LiteralPath $candidate).Path
    $repoPrefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $buildRoot.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Build root escapes the repository: '$buildRoot'."
    }
    $artifactMap = [ordered]@{
        'mcla.exe' = [string]$actual.executable_sha256
        $wanted.Runtime = [string]$actual.runtime_sha256
        $wanted.Tracy = [string]$actual.tracy_sha256
        $wanted.Gpu = [string]$actual.gpu_plugin_sha256
    }
    foreach ($entry in $artifactMap.GetEnumerator()) {
        $path = Join-Path $buildRoot $entry.Key
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required '$($wanted.Name)' artifact is missing: '$path'."
        }
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.Value) {
            throw "Artifact hash changed after the '$($wanted.Name)' matrix run: '$path'."
        }
    }
    $stream = [System.IO.File]::OpenRead((Join-Path $buildRoot 'mcla.exe'))
    try {
        if ($stream.ReadByte() -ne 0x4D -or $stream.ReadByte() -ne 0x5A) {
            throw "'$($wanted.Name)' output is not a PE executable."
        }
    } finally {
        $stream.Dispose()
    }
    $expectedDlls = @($wanted.Runtime, $wanted.Tracy, $wanted.Gpu)
    foreach ($dll in $knownStagedDlls) {
        if ($dll -notin $expectedDlls -and (Test-Path -LiteralPath (Join-Path $buildRoot $dll))) {
            throw "'$($wanted.Name)' build root contains cross-configuration artifact '$dll'."
        }
    }
    $objectCount = @(Get-ChildItem -LiteralPath $buildRoot -File -Recurse -Filter '*.cpp.obj' |
        Where-Object { $_.FullName.Replace('\', '/') -match '/generated/default/' }).Count
    if ($objectCount -ne 65) {
        throw "'$($wanted.Name)' contains $objectCount generated objects; expected 65."
    }
}

[pscustomobject]@{
    Passed = $true
    Configurations = $configurations.Count
    CleanBuilds = @($configurations | Where-Object { $_.clean_build_exit_code -eq 0 }).Count
    GeneratedObjectsPerConfiguration = 65
    GpuPlugin = 'xenos'
}
