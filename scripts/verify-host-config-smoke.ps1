[CmdletBinding()]
param(
    [string]$TemplatePath,
    [string]$ResultPath,
    [switch]$TemplateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $TemplatePath) { $TemplatePath = Join-Path $repoRoot 'config/mcla.toml.example' }
$TemplatePath = (Resolve-Path -LiteralPath $TemplatePath).Path
if ((Get-Item -LiteralPath $TemplatePath -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
    throw 'Host config template must not be a reparse point.'
}

$expected = [ordered]@{
    resolution='""'; video_mode_width='1280'; video_mode_height='720'; video_mode_refresh_rate='60.0'
    window_width='0'; window_height='0'; fullscreen='true'; monitor='0'
    audio_device='"default"'; audio_volume='1.0'; audio_mute='false'
    input_backend='"sdl"'; mnk_mode='false'
    log_level='"info"'; log_file='""'; log_verbose='false'; log_noisy='false'
    log_flush_interval='0'; log_max_file_size_mb='5'; log_max_files='20'
    game_data_root='""'; user_data_root='""'; update_data_root='""'; cache_root='""'; metadata_root='""'
}
$actual = [ordered]@{}
$lineNumber = 0
foreach ($line in [IO.File]::ReadAllLines($TemplatePath)) {
    $lineNumber++
    $trimmed = $line.Trim()
    if (-not $trimmed -or $trimmed.StartsWith('#')) { continue }
    if ($trimmed -notmatch '^([a-z][a-z0-9_]*)\s*=\s*(.+?)\s*$') {
        throw "Malformed host config line $lineNumber."
    }
    $name = $Matches[1]; $value = $Matches[2]
    if ($actual.Contains($name)) { throw "Duplicate host config key '$name'." }
    if (-not $expected.Contains($name)) { throw "Unknown host config key '$name'." }
    $actual[$name] = $value
}
foreach ($entry in $expected.GetEnumerator()) {
    if (-not $actual.Contains($entry.Key)) { throw "Missing host config key '$($entry.Key)'." }
    if ($actual[$entry.Key] -cne $entry.Value) {
        throw "Host config default '$($entry.Key)' is '$($actual[$entry.Key])'; expected '$($entry.Value)'."
    }
}
if ($actual.Count -ne $expected.Count) { throw 'Host config key cardinality mismatch.' }

$sourceChecks = 0
if (-not $TemplateOnly) {
    $audio = [IO.File]::ReadAllText((Join-Path $repoRoot 'third_party/rexglue-sdk/src/audio/sdl/sdl_audio_driver.cpp'))
    $window = [IO.File]::ReadAllText((Join-Path $repoRoot 'third_party/rexglue-sdk/src/ui/window.cpp'))
    $input = [IO.File]::ReadAllText((Join-Path $repoRoot 'third_party/rexglue-sdk/src/input/input_system.cpp'))
    $logging = [IO.File]::ReadAllText((Join-Path $repoRoot 'third_party/rexglue-sdk/src/core/logging.cpp'))
    $runtime = [IO.File]::ReadAllText((Join-Path $repoRoot 'third_party/rexglue-sdk/src/system/runtime.cpp'))
    $app = [IO.File]::ReadAllText((Join-Path $repoRoot 'third_party/rexglue-sdk/src/ui/rex_app.cpp'))
    $cmake = [IO.File]::ReadAllText((Join-Path $repoRoot 'CMakeLists.txt'))
    $tests = [IO.File]::ReadAllText((Join-Path $repoRoot 'third_party/rexglue-sdk/tests/unit/audio/sdl_audio_config_test.cpp'))
    $needles = @(
        @($audio, 'REXCVAR_DEFINE_STRING(audio_device, "default"'),
        @($audio, 'REXCVAR_DEFINE_DOUBLE(audio_volume, 1.0'),
        @($audio, '.range(0.0, 1.0)'),
        @($audio, 'FindPlaybackDeviceByName(configured_device, device_entries)'),
        @($audio, 'ApplyOutputVolume(data, static_cast<size_t>(sample_count)'),
        @($window, 'REXCVAR_DEFINE_BOOL(fullscreen, true'),
        @($window, 'REXCVAR_DEFINE_STRING(resolution, ""'),
        @($input, 'REXCVAR_DEFINE_STRING(input_backend, "sdl"'),
        @($logging, 'REXCVAR_DEFINE_STRING(log_level, "info"'),
        @($runtime, 'REXCVAR_DEFINE_STRING(game_data_root, ""'),
        @($app, 'cvar::LoadConfig(config_path_)'),
        @($cmake, 'config/mcla.toml.example'),
        @($cmake, '$<TARGET_FILE_DIR:mcla>/mcla.toml.example'),
        @($tests, 'CHECK_FALSE(rex::cvar::SetFlagByName("audio_volume", "-0.01"))'),
        @($tests, 'CHECK_FALSE(rex::cvar::SetFlagByName("audio_volume", "1.01"))'),
        @($tests, 'CHECK_FALSE(rex::cvar::SetFlagByName("audio_volume", "loud"))')
    )
    foreach ($pair in $needles) {
        if (-not $pair[0].Contains($pair[1])) { throw "Host config source contract is missing '$($pair[1])'." }
        $sourceChecks++
    }
}

if ($ResultPath) {
    $ResultPath = (Resolve-Path -LiteralPath $ResultPath).Path
    $resultRoot = Split-Path -Parent $ResultPath
    $canonicalEvidenceRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'private/evidence/M6-011'))
    if (-not ([IO.Path]::GetFullPath($resultRoot).StartsWith($canonicalEvidenceRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase))) { throw 'Host config result is outside canonical private evidence.' }
    $children = @(Get-ChildItem -LiteralPath $resultRoot -Force)
    $expectedChildren = @('app-clean-build.log','focused-tests.log','result.json','sdk-install.log')
    $actualChildNames = (@($children.Name | Sort-Object) -join '|')
    $expectedChildNames = (@($expectedChildren | Sort-Object) -join '|')
    if ($children.Count -ne $expectedChildren.Count -or $actualChildNames -cne $expectedChildNames) { throw 'Host config result topology is not exact.' }
    foreach ($child in $children) {
        if ($child.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Host config evidence contains a reparse point.' }
    }
    $result = Get-Content -LiteralPath $ResultPath -Raw | ConvertFrom-Json
    if ($result.schema -ne 1 -or $result.task -ne 'M6-011' -or $result.decision -ne 'host-config-contract-pass' -or $result.sdk_version -ne '0.9.0.27') { throw 'Invalid host config result identity.' }
    if ($result.sdk_install_exit_code -ne 0 -or $result.focused_test_exit_code -ne 0 -or $result.app_clean_build_exit_code -ne 0) { throw 'Host config build/test result is not clean.' }
    if ($result.focused_test_cases -ne 3 -or $result.focused_test_assertions -ne 27) { throw 'Unexpected focused audio config test totals.' }
    $templateHash = (Get-FileHash -LiteralPath $TemplatePath -Algorithm SHA256).Hash
    if ($result.template_sha256 -cne $templateHash) { throw 'Host config template hash drifted.' }
    $copied = Join-Path $repoRoot 'out/build/win-amd64-relwithdebinfo/mcla.toml.example'
    if (-not (Test-Path -LiteralPath $copied -PathType Leaf)) { throw 'Built host config example is missing.' }
    if ((Get-FileHash -LiteralPath $copied -Algorithm SHA256).Hash -cne $templateHash) { throw 'Built host config example differs from source.' }
    $hashBindings = [ordered]@{
        'sdk-install.log'='sdk_install_log_sha256'
        'focused-tests.log'='focused_test_log_sha256'
        'app-clean-build.log'='app_clean_build_log_sha256'
    }
    foreach ($binding in $hashBindings.GetEnumerator()) {
        $physicalHash = (Get-FileHash -LiteralPath (Join-Path $resultRoot $binding.Key) -Algorithm SHA256).Hash
        if ($result.($binding.Value) -cne $physicalHash) { throw "Host config evidence hash mismatch for '$($binding.Key)'." }
    }
}

[pscustomobject]@{ Passed=$true; Keys=$actual.Count; SourceContractChecks=$sourceChecks; BadInputCoverage=$true }
