[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-host-config-smoke.ps1'
$source = Join-Path $repoRoot 'config/mcla.toml.example'
$testRoot = Join-Path $repoRoot ('private/evidence/M6-011/test-' + [guid]::NewGuid().ToString('N').Substring(0,8))
[IO.Directory]::CreateDirectory($testRoot) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)
$negativeCount = 0
try {
    $positive = & $verifier -TemplatePath $source
    $baseline = [IO.File]::ReadAllText($source)
    $mutations = @(
        { param($s) $s.Replace('audio_volume = 1.0','audio_volume = 1.5') },
        { param($s) $s.Replace('audio_device = "default"','audio_device = ""') },
        { param($s) $s.Replace('input_backend = "sdl"','input_backend = "nop"') },
        { param($s) $s.Replace('wheel_force_feedback_gain = 100','wheel_force_feedback_gain = 101') },
        { param($s) $s.Replace('wheel_force_feedback_continuous_periodic_gain = 40','wheel_force_feedback_continuous_periodic_gain = 101') },
        { param($s) $s.Replace('wheel_force_feedback_continuous_constant_gain = 0','wheel_force_feedback_continuous_constant_gain = 101') },
        { param($s) $s.Replace('wheel_force_feedback_minimum_transient_strength = 75','wheel_force_feedback_minimum_transient_strength = 101') },
        { param($s) $s.Replace('wheel_steering_axis = 0','wheel_steering_axis = 32') },
        { param($s) $s.Replace('wheel_button_start = 7','') },
        { param($s) $s.Replace('mcla_diagnostics_enabled = true','mcla_diagnostics_enabled = false') },
        { param($s) $s.Replace('mcla_crash_reporter_dialog = true','mcla_crash_reporter_dialog = false') },
        { param($s) $s.Replace('mcla_diagnostics_root = ""','mcla_diagnostics_root = "C:\\private\\diagnostics"') },
        { param($s) $s.Replace('bind_mcla_debug_snapshot = "F10"','bind_mcla_debug_snapshot = "F9"') },
        { param($s) $s.Replace('fullscreen = true','fullscreen = maybe') },
        { param($s) $s.Replace('log_level = "info"','log_level = "verbose"') },
        { param($s) $s.Replace('log_max_files = 20','log_max_files = 0') },
        { param($s) $s.Replace('video_mode_width = 1280','video_mode_width = 320') },
        { param($s) $s.Replace('audio_mute = false','') },
        { param($s) $s + "`r`naudio_volume = 1.0`r`n" },
        { param($s) $s + "`r`n" + 'controller_serial = "private"' + "`r`n" },
        { param($s) $s.Replace('audio_volume = 1.0','audio_volume: 1.0') },
        { param($s) $s.Replace('game_data_root = ""','game_data_root = "C:\\private\\game"') }
    )
    $index = 0
    foreach ($mutation in $mutations) {
        $index++
        $path = Join-Path $testRoot ("bad-{0:D2}.toml" -f $index)
        [IO.File]::WriteAllText($path, (& $mutation $baseline), $utf8)
        $failed = $false
        try { & $verifier -TemplatePath $path -TemplateOnly | Out-Null } catch { $failed = $true }
        if (-not $failed) { throw "Bad host config fixture $index was accepted." }
        $negativeCount++
    }
    [pscustomobject]@{ Passed=$true; Positive=1; FailClosedNegatives=$negativeCount; SourceContractChecks=$positive.SourceContractChecks }
} finally {
    if (Test-Path -LiteralPath $testRoot -PathType Container) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}
