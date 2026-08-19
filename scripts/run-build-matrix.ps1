[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$toolchainResolver = Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1'
$integrationVerifier = Join-Path $PSScriptRoot 'verify-generated-integration.ps1'
$matrixVerifier = Join-Path $PSScriptRoot 'verify-build-matrix.ps1'
$toolchain = & $toolchainResolver -ExportPath
$cmake = $toolchain.CMakePath
$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $repoRoot "private/evidence/M3-012/$runId"
[System.IO.Directory]::CreateDirectory($runRoot) | Out-Null
$utf8 = [System.Text.UTF8Encoding]::new($false)

$matrix = @(
    [pscustomobject]@{ Name = 'Debug'; Preset = 'win-amd64-debug'; Runtime = 'rexruntimed.dll'; Tracy = 'TracyClientd.dll'; Gpu = 'rexgpu-xenosd.dll' },
    [pscustomobject]@{ Name = 'RelWithDebInfo'; Preset = 'win-amd64-relwithdebinfo'; Runtime = 'rexruntimerd.dll'; Tracy = 'TracyClientrd.dll'; Gpu = 'rexgpu-xenosrd.dll' },
    [pscustomobject]@{ Name = 'Release'; Preset = 'win-amd64-release'; Runtime = 'rexruntime.dll'; Tracy = 'TracyClient.dll'; Gpu = 'rexgpu-xenos.dll' }
)
$knownStagedDlls = @(
    'rexruntimed.dll', 'rexruntimerd.dll', 'rexruntime.dll',
    'TracyClientd.dll', 'TracyClientrd.dll', 'TracyClient.dll',
    'rexgpu-xenosd.dll', 'rexgpu-xenosrd.dll', 'rexgpu-xenos.dll'
)
$records = @()

foreach ($configuration in $matrix) {
    $buildRoot = Join-Path $repoRoot "out/build/$($configuration.Preset)"
    [System.IO.Directory]::CreateDirectory($buildRoot) | Out-Null
    foreach ($dll in $knownStagedDlls) {
        $stalePath = Join-Path $buildRoot $dll
        if (Test-Path -LiteralPath $stalePath -PathType Leaf) {
            Remove-Item -LiteralPath $stalePath -Force
        }
    }

    $configureLog = Join-Path $runRoot "$($configuration.Preset)-configure.log"
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    & $cmake --preset $configuration.Preset *>&1 | Tee-Object -FilePath $configureLog
    $configureExit = $LASTEXITCODE
    $timer.Stop()
    $configureDuration = $timer.ElapsedMilliseconds
    if ($configureExit -ne 0) {
        throw "Configure failed for '$($configuration.Name)' with exit $configureExit. Private run: '$runRoot'."
    }

    $buildLog = Join-Path $runRoot "$($configuration.Preset)-clean-build.log"
    $timer.Restart()
    & $cmake --build --preset $configuration.Preset --target mcla --clean-first --parallel *>&1 |
        Tee-Object -FilePath $buildLog
    $buildExit = $LASTEXITCODE
    $timer.Stop()
    $buildDuration = $timer.ElapsedMilliseconds
    if ($buildExit -ne 0) {
        throw "Clean build failed for '$($configuration.Name)' with exit $buildExit. Private run: '$runRoot'."
    }

    $integration = & $integrationVerifier -BuildRoot $buildRoot
    $executablePath = Join-Path $buildRoot 'mcla.exe'
    $runtimePath = Join-Path $buildRoot $configuration.Runtime
    $tracyPath = Join-Path $buildRoot $configuration.Tracy
    $gpuPath = Join-Path $buildRoot $configuration.Gpu
    foreach ($artifact in @($executablePath, $runtimePath, $tracyPath, $gpuPath)) {
        if (-not (Test-Path -LiteralPath $artifact -PathType Leaf)) {
            throw "Required '$($configuration.Name)' artifact was not staged: '$artifact'."
        }
    }

    $records += [ordered]@{
        name = $configuration.Name
        preset = $configuration.Preset
        build_root = "out/build/$($configuration.Preset)"
        configure_exit_code = $configureExit
        clean_build_exit_code = $buildExit
        configure_duration_ms = $configureDuration
        clean_build_duration_ms = $buildDuration
        generated_object_count = $integration.BuildObjectCount
        executable_sha256 = (Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash
        runtime_dll = $configuration.Runtime
        runtime_sha256 = (Get-FileHash -LiteralPath $runtimePath -Algorithm SHA256).Hash
        tracy_dll = $configuration.Tracy
        tracy_sha256 = (Get-FileHash -LiteralPath $tracyPath -Algorithm SHA256).Hash
        gpu_plugin_dll = $configuration.Gpu
        gpu_plugin_sha256 = (Get-FileHash -LiteralPath $gpuPath -Algorithm SHA256).Hash
    }
}

$result = [ordered]@{
    schema = 1
    task = 'M3-012'
    sdk_version = '0.9.0.29'
    generated_cpp_expected = 65
    configurations = $records
}
$resultPath = Join-Path $runRoot 'result.json'
[System.IO.File]::WriteAllText(
    $resultPath,
    (($result | ConvertTo-Json -Depth 8) + [Environment]::NewLine),
    $utf8
)
$verification = & $matrixVerifier -ResultPath $resultPath

[pscustomobject]@{
    Passed = $verification.Passed
    Configurations = $verification.Configurations
    CleanBuilds = $verification.CleanBuilds
    PrivateRunRoot = $runRoot
    ResultPath = $resultPath
}
