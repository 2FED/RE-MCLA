[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-frontend-parity.ps1'
$fixture = Join-Path $repo ('private\evidence\M4-012\test-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
$user = Join-Path $fixture 'user'; [IO.Directory]::CreateDirectory($user) | Out-Null
$source = Join-Path $repo 'private\evidence\M4-011\20260813-230502-1f0de720\runs\02\user'

function Scale([string]$SourcePath, [string]$DestinationPath) {
    Add-Type -AssemblyName System.Drawing; $sourceImage = [Drawing.Image]::FromFile((Resolve-Path $SourcePath).Path); $target = [Drawing.Bitmap]::new(2560, 1440); $graphics = [Drawing.Graphics]::FromImage($target)
    try { $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic; $graphics.DrawImage($sourceImage, 0, 0, 2560, 1440); $target.Save($DestinationPath, [Drawing.Imaging.ImageFormat]::Bmp) } finally { $graphics.Dispose(); $target.Dispose(); $sourceImage.Dispose() }
}

try {
    foreach ($name in @('mcla-first-frame.bmp', 'mcla-frontend-gameplay.bmp', 'mcla-frontend-pause.bmp', 'mcla-frontend-options.bmp')) { Scale (Join-Path $source $name) (Join-Path $user $name) }
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add('[info] MCLA_FRONTEND_SMOKE_CONFIG v=1 slot=0 hold_ms=200 gameplay_wait_seconds=30 intertab_wait_seconds=2')
    $lines.Add('[info] MCLA_FRONTEND_SMOKE_TIMING v=1 first_frame_settle_seconds=45 gameplay_wait_seconds=45')
    $lines.Add('[info] MCLA graphics: nontrivial guest frame captured 2560x1440, rgb555 bins 100, luma p05 1, luma p95 200, modal permille 1, nonmodal grid cells 144')
    $lines.Add('[info] MCLA_FRONTEND_SMOKE_FRAME v=1 phase=gameplay width=2560 height=1440 status=PASS')
    $lines.Add('[info] MCLA_FRONTEND_SMOKE_FRAME v=1 phase=pause width=2560 height=1440 status=PASS')
    $lines.Add('[info] MCLA_FRONTEND_SMOKE_FRAME v=1 phase=options width=2560 height=1440 status=PASS')
    $lines.Add('[info] MCLA_FRONTEND_SMOKE_SUMMARY v=1 status=PASS pulses=4 source_records=8 guest_records=8 frames=3 gameplay=1 pause=1 options=1 external_close_required=1')
    $lines.Add('[info] Window closing, shutting down...')
    $lines.Add('[info] Execution complete')
    $lines.Add('[info] Title terminated; hard-exiting process.')
    $positive = $lines -join "`r`n"; $log = Join-Path $fixture 'mcla.log'; [IO.File]::WriteAllText($log, $positive + "`r`n", [Text.UTF8Encoding]::new($false))
    $probe = & $verify -ProbeOnly -RuntimeLogPath $log -UserRoot $user
    if (-not $probe.Passed -or $probe.Metrics.title_logo_ppm -lt 900000 -or $probe.Metrics.pause_footer_ppm -lt 500000 -or $probe.Metrics.gameplay_hud_ppm -lt 650000 -or $probe.Metrics.options_native_ppm -lt 550000) { throw 'Positive parity fixture failed.' }
    $cases = [ordered]@{
        'missing-timing' = $positive.Replace("[info] MCLA_FRONTEND_SMOKE_TIMING v=1 first_frame_settle_seconds=45 gameplay_wait_seconds=45`r`n", '')
        'wrong-first-frame-settle' = $positive.Replace('first_frame_settle_seconds=45', 'first_frame_settle_seconds=35')
        'short-timing' = $positive.Replace('gameplay_wait_seconds=45', 'gameplay_wait_seconds=30')
        'duplicate-timing' = $positive + "`r`n[info] MCLA_FRONTEND_SMOKE_TIMING v=1 first_frame_settle_seconds=45 gameplay_wait_seconds=45"
        'wrong-gameplay-size' = $positive.Replace('phase=gameplay width=2560 height=1440', 'phase=gameplay width=1280 height=720')
        'wrong-pause-size' = $positive.Replace('phase=pause width=2560 height=1440', 'phase=pause width=1280 height=720')
        'wrong-options-size' = $positive.Replace('phase=options width=2560 height=1440', 'phase=options width=1280 height=720')
        'wrong-frame-order' = $positive.Replace('phase=gameplay', 'phase=options')
        'missing-summary' = $positive.Replace("[info] MCLA_FRONTEND_SMOKE_SUMMARY v=1 status=PASS pulses=4 source_records=8 guest_records=8 frames=3 gameplay=1 pause=1 options=1 external_close_required=1`r`n", '')
        'summary-after-close' = $positive.Replace("[info] MCLA_FRONTEND_SMOKE_SUMMARY v=1 status=PASS pulses=4 source_records=8 guest_records=8 frames=3 gameplay=1 pause=1 options=1 external_close_required=1`r`n", '').Replace('[info] Execution complete', "[info] Execution complete`r`n[info] MCLA_FRONTEND_SMOKE_SUMMARY v=1 status=PASS pulses=4 source_records=8 guest_records=8 frames=3 gameplay=1 pause=1 options=1 external_close_required=1")
        'missing-close' = $positive.Replace("[info] Window closing, shutting down...`r`n", '')
        'missing-complete' = $positive.Replace("[info] Execution complete`r`n", '')
        'missing-hard-exit' = $positive.Replace('[info] Title terminated; hard-exiting process.', '')
        'fatal-tail' = $positive + "`r`n[fatal] synthetic"
        'guest-crash-tail' = $positive + "`r`nREX_GUEST_CRASH schema=1"
        'ppc-unimplemented-tail' = $positive + "`r`nPPC_UNIMPLEMENTED"
    }
    $pausePath = Join-Path $user 'mcla-frontend-pause.bmp'
    $pauseBackup = Join-Path $fixture 'pause-backup.bmp'
    Copy-Item -LiteralPath $pausePath -Destination $pauseBackup
    $pauseImage = [Drawing.Bitmap]::new($pausePath)
    try {
        $graphics = [Drawing.Graphics]::FromImage($pauseImage)
        try { $graphics.FillRectangle([Drawing.Brushes]::Black, 560, 920, 520, 140) }
        finally { $graphics.Dispose() }
        $pauseImage.Save((Join-Path $fixture 'pause-mutated.bmp'), [Drawing.Imaging.ImageFormat]::Bmp)
    } finally { $pauseImage.Dispose() }
    $failed = 0
    foreach ($case in $cases.GetEnumerator()) { [IO.File]::WriteAllText($log, $case.Value + "`r`n", [Text.UTF8Encoding]::new($false)); try { $null = & $verify -ProbeOnly -RuntimeLogPath $log -UserRoot $user } catch { $failed++; continue }; throw "Fail-closed fixture '$($case.Key)' was accepted." }
    [IO.File]::WriteAllText($log, $positive + "`r`n", [Text.UTF8Encoding]::new($false))
    Copy-Item -LiteralPath (Join-Path $fixture 'pause-mutated.bmp') -Destination $pausePath -Force
    try { $null = & $verify -ProbeOnly -RuntimeLogPath $log -UserRoot $user } catch { $failed++ }
    if ($failed -ne $cases.Count + 1) { throw 'Mutated pause ROI was accepted.' }
    Copy-Item -LiteralPath $pauseBackup -Destination $pausePath -Force
    $sources = [ordered]@{
        'timing-cvar' = @('src/mcla_app.cpp', 'mcla_frontend_gameplay_wait_seconds')
        'timing-range' = @('src/mcla_app.cpp', '.range(30, 60)')
        'timing-marker' = @('src/mcla_app.cpp', 'MCLA_FRONTEND_SMOKE_TIMING v=1 first_frame_settle_seconds={} ')
        'runner-scale2' = @('scripts/run-frontend-parity.ps1', '--resolution_scale=2')
        'runner-wait45' = @('scripts/run-frontend-parity.ps1', '--mcla_frontend_gameplay_wait_seconds=45')
        'runner-settle45' = @('scripts/run-frontend-parity.ps1', '--mcla_first_frame_settle_seconds=45')
        'runner-external-close' = @('scripts/run-frontend-parity.ps1', 'Close-Exact $process')
        'runner-no-audio-prompt' = @('scripts/run-frontend-parity.ps1', "individual_event_identity = 'not-observed'")
        'xenia-title-hash' = @('scripts/verify-frontend-parity.ps1', '7F0293842A6AA30EF0B0EA7C7954FF5130A03ECF6E3A112EEFCAA4A6B11C613E')
        'xenia-pause-hash' = @('scripts/verify-frontend-parity.ps1', 'EA85BD3AECFAD647819682D19E1951097A22F59360500ACD70A0DAF4DC80BFE2')
        'xenia-gameplay-hash' = @('scripts/verify-frontend-parity.ps1', 'A490553067A8F375191F618C34556B82C9FD244FF295519AB4383AAB40434F8B')
        'no-whole-frame-claim' = @('scripts/verify-frontend-parity.ps1', 'whole_frame_equivalence_claimed')
        'no-event-identity-claim' = @('scripts/verify-frontend-parity.ps1', 'audio_event_identity_claimed')
        'audio-physical-verifier' = @('scripts/verify-frontend-parity.ps1', 'verify-audio-route-smoke.ps1')
        'two-resolution-gate' = @('scripts/verify-frontend-parity.ps1', 'resolution_count -ne 2')
        'contact-sheet-binding' = @('scripts/verify-frontend-parity.ps1', 'Contact-sheet binding failed.')
        'bounded-pause-registration' = @('scripts/verify-frontend-parity.ps1', 'Get-RegisteredCorrelation $pause.Path $xPause')
        'build-log-binding' = @('scripts/verify-frontend-parity.ps1', 'Clean-build log binding failed.')
    }
    foreach ($sourceCheck in $sources.GetEnumerator()) { if (-not [IO.File]::ReadAllText((Join-Path $repo $sourceCheck.Value[0])).Contains($sourceCheck.Value[1])) { throw "Source contract '$($sourceCheck.Key)' failed." } }
    [pscustomobject]@{ Passed = $true; PositiveFixtures = 1; FailClosedNegatives = $failed; SourceContractChecks = $sources.Count }
} finally { if (Test-Path $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force } }
