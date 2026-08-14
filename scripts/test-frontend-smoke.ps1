[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-frontend-smoke.ps1'
$fixture = Join-Path $repo ('private\evidence\M4-011\test-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
$user = Join-Path $fixture 'user'; [IO.Directory]::CreateDirectory($user) | Out-Null
$reference = Join-Path $repo 'private\baseline\M4-011\frontend-reference'
$calibration = Join-Path $repo 'private\evidence\M4-011\calibration-options-v4\runs\01\user'
Copy-Item (Join-Path $calibration 'mcla-first-frame.bmp') (Join-Path $user 'mcla-first-frame.bmp')
Copy-Item (Join-Path $reference 'gameplay.bmp') (Join-Path $user 'mcla-frontend-gameplay.bmp')
Copy-Item (Join-Path $reference 'pause.bmp') (Join-Path $user 'mcla-frontend-pause.bmp')
Copy-Item (Join-Path $reference 'options.bmp') (Join-Path $user 'mcla-frontend-options.bmp')
$log = Join-Path $fixture 'mcla.log'
$lines = [Collections.Generic.List[string]]::new()
$lines.Add('[info] MCLA_FRONTEND_SMOKE_CONFIG v=1 slot=0 hold_ms=200 gameplay_wait_seconds=30 intertab_wait_seconds=2')
$lines.Add('[info] MCLA graphics: nontrivial guest frame captured 1280x720, rgb555 bins 100, luma p05 1, luma p95 200, modal permille 1, nonmodal grid cells 144')
foreach ($entry in @(@(1, '0010'), @(2, '0010'), @(3, '0200'), @(4, '0200'))) {
    $lines.Add("[info] MCLA_FRONTEND_SMOKE_INPUT v=1 side=source sequence=$($entry[0]) buttons=$($entry[1])")
    $lines.Add("[info] MCLA_FRONTEND_SMOKE_INPUT v=1 side=guest sequence=$($entry[0]) buttons=$($entry[1])")
    $lines.Add("[info] MCLA_FRONTEND_SMOKE_INPUT v=1 side=source sequence=$($entry[0]) buttons=0000")
    $lines.Add("[info] MCLA_FRONTEND_SMOKE_INPUT v=1 side=guest sequence=$($entry[0]) buttons=0000")
    if ($entry[0] -eq 1) { $lines.Add('[info] MCLA_FRONTEND_SMOKE_FRAME v=1 phase=gameplay width=1280 height=720 status=PASS') }
    if ($entry[0] -eq 2) { $lines.Add('[info] MCLA_FRONTEND_SMOKE_FRAME v=1 phase=pause width=1280 height=720 status=PASS') }
}
$lines.Add('[info] MCLA_FRONTEND_SMOKE_FRAME v=1 phase=options width=1280 height=720 status=PASS')
$lines.Add('[info] MCLA_FRONTEND_SMOKE_SUMMARY v=1 status=PASS pulses=4 source_records=8 guest_records=8 frames=3 gameplay=1 pause=1 options=1 external_close_required=1')
$lines.Add('[info] Window closing, shutting down...')
$lines.Add('[info] Execution complete')
$lines.Add('[info] Title terminated; hard-exiting process.')
$positive = $lines -join "`r`n"
[IO.File]::WriteAllText($log, $positive + "`r`n", [Text.UTF8Encoding]::new($false))
try {
    $probe = & $verify -ProbeOnly -FixtureMode -ClosureProbe -RuntimeLogPath $log -UserRoot $user
    if (-not $probe.Passed -or $probe.PauseCorrelationPpm -lt 500000 -or $probe.OptionsCorrelationPpm -lt 550000 -or [Math]::Abs($probe.PauseRegistrationDx) -gt 8 -or [Math]::Abs($probe.PauseRegistrationDy) -gt 4) { throw 'Positive frontend fixture failed.' }
    $postHardExitComplete = $positive.Replace("[info] Execution complete`r`n[info] Title terminated; hard-exiting process.", "[info] Title terminated; hard-exiting process.`r`n[2026-08-14 03:31:13.228] [info] [core] [t48312] Execution complete")
    [IO.File]::WriteAllText($log, $postHardExitComplete + "`r`n", [Text.UTF8Encoding]::new($false))
    $postHardExitProbe = & $verify -ProbeOnly -FixtureMode -ClosureProbe -RuntimeLogPath $log -UserRoot $user
    if (-not $postHardExitProbe.Passed) { throw 'Valid post-hard-exit Execution complete tail was rejected.' }
    $postHardExitGpuTrace = $positive + "`r`n[2026-08-14 03:23:24.037] [trace] [gpu] [t24792] Resolve: bounded synthetic trace"
    [IO.File]::WriteAllText($log, $postHardExitGpuTrace + "`r`n", [Text.UTF8Encoding]::new($false))
    $postHardExitGpuProbe = & $verify -ProbeOnly -FixtureMode -ClosureProbe -RuntimeLogPath $log -UserRoot $user
    if (-not $postHardExitGpuProbe.Passed) { throw 'Valid post-hard-exit GPU trace tail was rejected.' }
    [IO.File]::WriteAllText($log, $positive + "`r`n", [Text.UTF8Encoding]::new($false))

    Add-Type -AssemblyName System.Drawing
    $pausePath = Join-Path $user 'mcla-frontend-pause.bmp'
    $pauseOriginal = [IO.File]::ReadAllBytes($pausePath)
    $source = [Drawing.Bitmap]::new((Join-Path $reference 'pause.bmp'))
    $shifted = [Drawing.Bitmap]::new(1280, 720)
    try {
        $graphics = [Drawing.Graphics]::FromImage($shifted)
        try { $graphics.Clear([Drawing.Color]::Black); $graphics.DrawImageUnscaled($source, 40, 0) } finally { $graphics.Dispose() }
        $shifted.Save($pausePath, [Drawing.Imaging.ImageFormat]::Bmp)
    } finally { $shifted.Dispose(); $source.Dispose() }
    $outOfBoundsRejected = $false
    try {
        try { $null = & $verify -ProbeOnly -FixtureMode -ClosureProbe -RuntimeLogPath $log -UserRoot $user } catch { $outOfBoundsRejected = $true }
        if (-not $outOfBoundsRejected) { throw 'Out-of-bound pause translation was accepted.' }
    } finally { [IO.File]::WriteAllBytes($pausePath, $pauseOriginal) }
    $cases = [ordered]@{
        'duplicate-config' = $positive + "`r`n" + $lines[0]
        'wrong-hold' = $positive.Replace('hold_ms=200', 'hold_ms=20')
        'wrong-gameplay-wait' = $positive.Replace('gameplay_wait_seconds=30', 'gameplay_wait_seconds=3')
        'wrong-intertab-wait' = $positive.Replace('intertab_wait_seconds=2', 'intertab_wait_seconds=0')
        'wrong-first-button' = $positive.Replace('sequence=1 buttons=0010', 'sequence=1 buttons=1000')
        'wrong-rb-button' = $positive.Replace('sequence=3 buttons=0200', 'sequence=3 buttons=0100')
        'wrong-side' = $positive.Replace('side=guest sequence=2 buttons=0010', 'side=source sequence=2 buttons=0010')
        'wrong-sequence' = $positive.Replace('sequence=4 buttons=0200', 'sequence=3 buttons=0200')
        'missing-release' = $positive.Replace("[info] MCLA_FRONTEND_SMOKE_INPUT v=1 side=guest sequence=4 buttons=0000`r`n", '')
        'duplicate-input' = $positive.Replace('[info] MCLA_FRONTEND_SMOKE_FRAME v=1 phase=options', "[info] MCLA_FRONTEND_SMOKE_INPUT v=1 side=guest sequence=4 buttons=0000`r`n[info] MCLA_FRONTEND_SMOKE_FRAME v=1 phase=options")
        'wrong-gameplay-frame' = $positive.Replace('phase=gameplay', 'phase=pause')
        'wrong-pause-frame' = $positive.Replace('phase=pause', 'phase=options')
        'missing-options-frame' = $positive.Replace("[info] MCLA_FRONTEND_SMOKE_FRAME v=1 phase=options width=1280 height=720 status=PASS`r`n", '')
        'bad-summary' = $positive.Replace('frames=3 gameplay=1', 'frames=2 gameplay=1')
        'internal-exit-claim' = $positive.Replace('external_close_required=1', 'external_close_required=0')
        'missing-close' = $positive.Replace("[info] Window closing, shutting down...`r`n", '')
        'missing-complete' = $positive.Replace("[info] Execution complete`r`n", '')
        'missing-hard-exit' = $positive.Replace("[info] Title terminated; hard-exiting process.", '')
        'unexpected-benign-post-hard-exit-tail' = $positive + "`r`n[info] unexpected benign tail"
        'malformed-post-hard-exit-complete' = $positive.Replace("[info] Execution complete`r`n[info] Title terminated; hard-exiting process.", "[info] Title terminated; hard-exiting process.`r`n[info] Execution complete")
        'fatal-tail' = $positive + "`r`n[fatal] synthetic"
        'guest-crash-tail' = $positive + "`r`nREX_GUEST_CRASH schema=1"
    }
    $failed = 0
    foreach ($case in $cases.GetEnumerator()) {
        [IO.File]::WriteAllText($log, $case.Value + "`r`n", [Text.UTF8Encoding]::new($false))
        try { $null = & $verify -ProbeOnly -FixtureMode -ClosureProbe -RuntimeLogPath $log -UserRoot $user } catch { $failed++; continue }
        throw "Fail-closed fixture '$($case.Key)' was accepted."
    }
    $sources = [ordered]@{
        'vfs-root-fix' = @('third_party/rexglue-sdk/src/filesystem/virtual_file_system.cpp', 'relative_path = rex::string::utf8_canonicalize_guest_path(relative_path);')
        'vfs-test' = @('third_party/rexglue-sdk/tests/unit/core/filesystem_test.cpp', 'VFS opens content symlink roots with trailing separators')
        'init-only-cvar' = @('src/mcla_app.cpp', 'mcla_frontend_smoke_probe')
        'hold-contract' = @('src/mcla_app.cpp', 'std::chrono::milliseconds(200)')
        'gameplay-wait' = @('src/mcla_app.cpp', 'mcla_frontend_gameplay_wait_seconds')
        'options-input' = @('src/mcla_app.cpp', 'X_INPUT_GAMEPAD_RIGHT_SHOULDER')
        'summary-schema' = @('src/mcla_app.cpp', 'MCLA_FRONTEND_SMOKE_SUMMARY v=1 status=PASS')
        'runner-seed-hash' = @('scripts/run-frontend-smoke.ps1', 'E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68')
        'runner-cycle-count' = @('scripts/run-frontend-smoke.ps1', 'for ($index = 1; $index -le $CycleCount; $index++)')
        'closure-twenty-cycles' = @('scripts/run-frontend-smoke.ps1', "[ValidateSet(3, 20)][int]`$CycleCount = 3")
        'closure-stable-timing' = @('scripts/run-frontend-smoke.ps1', "'--mcla_frontend_gameplay_wait_seconds=45'")
        'closure-pause-settle' = @('scripts/run-frontend-smoke.ps1', "'--mcla_frontend_pause_wait_seconds=4'")
        'pause-wait-cvar' = @('src/mcla_app.cpp', 'mcla_frontend_pause_wait_seconds')
        'closure-timing-verifier' = @('scripts/verify-frontend-smoke.ps1', 'Get-ClosureProbe')
        'runner-external-close' = @('scripts/run-frontend-smoke.ps1', 'Close-Exact $process')
        'runner-no-physical-input' = @('scripts/run-frontend-smoke.ps1', '--mcla_frontend_smoke_probe=true')
        'verifier-physical-game' = @('scripts/verify-frontend-smoke.ps1', 'Canonical source-game physical identity mismatch.')
        'verifier-runtime-artifacts' = @('scripts/verify-frontend-smoke.ps1', 'Canonical runtime artifact mismatch.')
        'verifier-focused-totals' = @('scripts/verify-frontend-smoke.ps1', 'Focused VFS test totals mismatch.')
        'verifier-process-cleanup' = @('scripts/verify-frontend-smoke.ps1', 'Canonical MCLA process is still running.')
        'closure-recovery-provenance' = @('scripts/verify-frontend-smoke.ps1', 'Recovered closure provenance is invalid.')
        'closure-exact-topology' = @('scripts/verify-frontend-smoke.ps1', 'Closure evidence-root topology is not exact.')
    }
    foreach ($source in $sources.GetEnumerator()) { if (-not ([IO.File]::ReadAllText((Join-Path $repo $source.Value[0])).Contains($source.Value[1]))) { throw "Source contract '$($source.Key)' failed." } }
    [pscustomobject]@{ Passed = $true; PositiveFixtures = 3; FailClosedNegatives = $failed + 1; SourceContractChecks = $sources.Count }
} finally {
    if (Test-Path $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
}
