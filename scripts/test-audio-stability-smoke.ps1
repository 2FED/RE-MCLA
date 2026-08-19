[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-audio-stability-smoke.ps1'
$run = Join-Path $PSScriptRoot 'run-audio-stability-smoke.ps1'
$utf8 = [Text.UTF8Encoding]::new($false)
$root = Join-Path $repo ('private/evidence/M6-007/test-' + [guid]::NewGuid().ToString('N'))
$soak = Join-Path $root 'long-soak'; $soakCycle = Join-Path $soak 'runs\soak'; $soakUser = Join-Path $soakCycle 'user'
$stability = Join-Path $root 'device-switch'; $stabilityCycle = Join-Path $stability 'runs\stability'
[IO.Directory]::CreateDirectory($soakUser) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $soakCycle 'cache')) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $stabilityCycle 'user')) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $stabilityCycle 'cache')) | Out-Null

function Write-Text([string]$Path, [string]$Text) { [IO.File]::WriteAllText($Path, $Text.Replace("`n", [Environment]::NewLine), $utf8) }
function Expect-Failure([scriptblock]$Action, [string]$Name) { try { & $Action; throw "Negative '$Name' was accepted." } catch { if ($_.Exception.Message -ceq "Negative '$Name' was accepted.") { throw } }; $script:negativeCount++ }

$routePrefix = @'
SDL_AUDIO_AUDIT_CONFIG v=1 enabled=1 backend=sdl sample_rate=48000 source_channels=6
SDL_AUDIO_AUDIT_CLIENT v=1 event=register result=00000000 index_class=bounded
SDL_AUDIO_AUDIT_FRAME v=1 layer=submit class=nonzero finite=1 channels=6 peak_ppm=100
SDL_AUDIO_AUDIT_FRAME v=1 layer=device class=nonzero finite=1 channels=2 peak_ppm=100
SDL_AUDIO_AUDIT_FRAME v=1 layer=xma class=nonzero finite=1 channels=0 peak_ppm=100
'@
$routeSummary = 'SDL_AUDIO_AUDIT_SUMMARY v=1 phase=title status=PASS client_calls=1 client_success=1 submit_frames=1000 submit_nonzero=999 submit_invalid=0 submit_peak_ppm=100 device_frames=1000 device_nonzero=999 device_invalid=0 device_submit_fail=0 device_peak_ppm=100 xma_frames=1000 xma_nonzero=999 xma_invalid=0 xma_peak_ppm=100 max_queue_depth=8 starvation_fills=1 max_consecutive_starvation_fills=1 dropped_records=0'
$lifecycle = @'
Window closing, shutting down...
Execution complete
Title terminated; hard-exiting process.
'@
$soakLog = $routePrefix + "`n" + @'
MCLA graphics: nontrivial guest frame captured 1280x720, rgb555 bins 100, luma p05 1, luma p95 200, modal permille 10, nonmodal grid cells 20
MCLA audio: title soak started seconds 7200
'@ + "`n" + $routeSummary + "`nMCLA audio: title soak completed seconds 7200`n" + $lifecycle
$stabilityLog = $routePrefix + "`n" + @'
SDL_AUDIO_STABILITY_CONFIG v=1 enabled=1 backend=sdl default_device=1 pause_cycles=2 device_identity=redacted
SDL_AUDIO_STABILITY_DRIVER v=1 event=open result=00000000 source_channels=2
MCLA_AUDIO_STABILITY_CONFIG v=1 pause_cycles=2 pause_ms=2000 recovery_ms=5000 device_switch=external identity=redacted
MCLA_AUDIO_STABILITY_PROBE v=1 cycle=1 event=PAUSE
SDL_AUDIO_STABILITY_STATE v=1 event=pause result=00000000 order_valid=1 cycle=1
MCLA_AUDIO_STABILITY_PROBE v=1 cycle=1 event=RESUME
SDL_AUDIO_STABILITY_STATE v=1 event=resume result=00000000 order_valid=1 cycle=1
SDL_AUDIO_STABILITY_RECOVERY v=1 source=pause cycle=1 nonzero=1 result=00000000
MCLA_AUDIO_STABILITY_PROBE v=1 cycle=2 event=PAUSE
SDL_AUDIO_STABILITY_STATE v=1 event=pause result=00000000 order_valid=1 cycle=2
MCLA_AUDIO_STABILITY_PROBE v=1 cycle=2 event=RESUME
SDL_AUDIO_STABILITY_STATE v=1 event=resume result=00000000 order_valid=1 cycle=2
SDL_AUDIO_STABILITY_RECOVERY v=1 source=pause cycle=2 nonzero=1 result=00000000
SDL_AUDIO_STABILITY_READY v=1 phase=device-switch pause_recoveries=2 output_nonzero=1000 status=READY
MCLA_AUDIO_STABILITY_READY v=1 phase=device-switch status=READY external_confirmation_required=1
SDL_AUDIO_STABILITY_DEVICE v=1 event=migrated playback=1 sequence=1
SDL_AUDIO_STABILITY_RECOVERY v=1 source=device cycle=1 nonzero=1 result=00000000
MCLA_AUDIO_STABILITY_CONFIRM v=1 machine_recovered=1 operator_heard=1 identity=redacted
SDL_AUDIO_STABILITY_SUMMARY v=1 phase=title status=PASS driver_calls=1 driver_success=1 output_nonzero=2000 pause_calls=2 pause_success=2 resume_calls=2 resume_success=2 resume_recoveries=2 device_events=1 device_recoveries=1 failures=0 dropped_records=0
'@ + "`n" + $routeSummary + "`n" + @'

XMP_AUDIT_SUMMARY v=1 phase=title status=PASS calls=2 known_calls=2 query_calls=2 playback_calls=0 unsupported_calls=0 state_changes=0 unexpected_calls=0 inconsistent_calls=0 records=1 overflow=0 dropped_records=0
MCLA_AUDIO_STABILITY_SUMMARY v=1 status=COMPLETE pause_cycles=2 device_switch=external prior_routes_bound=1
'@ + "`n" + $lifecycle

try {
  $resourceSelfTest = & $run -ResourceSelfTest
  if (-not $resourceSelfTest.ProcessResourceSamplingVerified -or $resourceSelfTest.MedianObservations -ne 3 -or $resourceSelfTest.GpuCounterDependency -ne $false) { throw 'Runner resource-sampling self-test failed.' }
  $sourceBmp = Join-Path $repo 'private/evidence/M4-007/20260813-170202-44d2c7d8/runs/01/user/mcla-first-frame.bmp'
  Copy-Item -LiteralPath $sourceBmp -Destination (Join-Path $soakUser 'mcla-first-frame.bmp')
  $soakLogPath = Join-Path $soakCycle 'mcla.log'; $stabilityLogPath = Join-Path $stabilityCycle 'mcla.log'
  $resourcePath = Join-Path $soak 'resource-samples.json'
  $resourceSamples = @(for ($i = 0; $i -lt 13; ++$i) { [ordered]@{ checkpoint = $i; elapsed_seconds = $i * 600; private_bytes = 100000000L + $i * 1000000L; working_set_bytes = 90000000L + $i * 500000L; handle_count = 200L + $i; thread_count = 20L } })
  Write-Text $resourcePath ((ConvertTo-Json -InputObject $resourceSamples -Depth 4) + "`n")
  Write-Text $soakLogPath $soakLog
  Write-Text $stabilityLogPath $stabilityLog
  $soakProbe = & $verify -LongSoakPath $soak -Fixture
  $stabilityProbe = & $verify -RunPath $stability -Fixture
  if (-not $soakProbe.Passed -or -not $stabilityProbe.Passed) { throw 'Positive audio fixture failed.' }
  $positiveCount = 2

  $negativeCount = 0
  $soakMutations = @(
    @('missing-soak-start', 'MCLA audio: title soak started seconds 7200', 'REMOVED'),
    @('wrong-soak-duration', 'seconds 7200', 'seconds 7199'),
    @('route-fail', 'phase=title status=PASS client_calls', 'phase=title status=FAIL client_calls'),
    @('invalid-samples', 'submit_invalid=0', 'submit_invalid=1'),
    @('queue-overflow', 'max_queue_depth=8', 'max_queue_depth=65'),
    @('starvation-run', 'max_consecutive_starvation_fills=1', 'max_consecutive_starvation_fills=3'),
    @('missing-controlled-close', 'Window closing, shutting down...', 'REMOVED'),
    @('fatal-tail', 'Execution complete', "FATAL synthetic`nExecution complete")
  )
  foreach ($mutation in $soakMutations) { Write-Text $soakLogPath ($soakLog.Replace($mutation[1], $mutation[2])); Expect-Failure { & $verify -LongSoakPath $soak -Fixture | Out-Null } $mutation[0] }
  Write-Text $soakLogPath $soakLog
  Write-Text $resourcePath ((ConvertTo-Json -InputObject @($resourceSamples[0..11]) -Depth 4) + "`n")
  Expect-Failure { & $verify -LongSoakPath $soak -Fixture | Out-Null } 'missing-resource-checkpoint'
  $growthSamples = @($resourceSamples | ForEach-Object { [ordered]@{ checkpoint = $_.checkpoint; elapsed_seconds = $_.elapsed_seconds; private_bytes = $_.private_bytes; working_set_bytes = $_.working_set_bytes; handle_count = $_.handle_count; thread_count = $_.thread_count } })
  $growthSamples[12].private_bytes = $growthSamples[0].private_bytes + 1100MB
  Write-Text $resourcePath ((ConvertTo-Json -InputObject $growthSamples -Depth 4) + "`n")
  Expect-Failure { & $verify -LongSoakPath $soak -Fixture | Out-Null } 'private-memory-growth'
  Write-Text $resourcePath ((ConvertTo-Json -InputObject $resourceSamples -Depth 4) + "`n")

  $stabilityMutations = @(
    @('missing-driver', 'SDL_AUDIO_STABILITY_DRIVER', 'REMOVED_DRIVER'),
    @('pause-failure', 'event=pause result=00000000', 'event=pause result=C0000001'),
    @('pause-order', 'event=resume result=00000000 order_valid=1 cycle=1', 'event=resume result=00000000 order_valid=0 cycle=1'),
    @('missing-pause-recovery', 'source=pause cycle=2', 'source=pause cycle=3'),
    @('ready-before-recovery', 'SDL_AUDIO_STABILITY_READY v=1', 'SDL_AUDIO_STABILITY_READY v=1'),
    @('missing-device', 'SDL_AUDIO_STABILITY_DEVICE', 'REMOVED_DEVICE'),
    @('non-migrated-device-event', 'event=migrated playback=1', 'event=added playback=1'),
    @('recording-device', 'playback=1 sequence=1', 'playback=0 sequence=1'),
    @('missing-device-recovery', 'source=device cycle=1', 'source=other cycle=1'),
    @('device-recovery-counter-drift', 'device_events=1 device_recoveries=1', 'device_events=1 device_recoveries=2'),
    @('unknown-stability-marker', 'MCLA_AUDIO_STABILITY_CONFIRM v=1', "SDL_AUDIO_STABILITY_UNKNOWN v=1`nMCLA_AUDIO_STABILITY_CONFIRM v=1"),
    @('duplicate-device-class', 'SDL_AUDIO_STABILITY_RECOVERY v=1 source=device', "SDL_AUDIO_STABILITY_DEVICE v=1 event=migrated playback=1 sequence=2`nSDL_AUDIO_STABILITY_RECOVERY v=1 source=device"),
    @('operator-not-heard', 'operator_heard=1', 'operator_heard=0'),
    @('summary-failure', 'failures=0 dropped_records=0', 'failures=1 dropped_records=0'),
    @('missing-xmp', 'XMP_AUDIT_SUMMARY', 'REMOVED_XMP_SUMMARY'),
    @('xmp-playback-overclaim', 'playback_calls=0', 'playback_calls=1'),
    @('duplicate-app-summary', 'MCLA_AUDIO_STABILITY_SUMMARY v=1', "MCLA_AUDIO_STABILITY_SUMMARY v=1 status=COMPLETE pause_cycles=2 device_switch=external prior_routes_bound=1`nMCLA_AUDIO_STABILITY_SUMMARY v=1"),
    @('guest-crash-tail', 'Execution complete', "Guest crash`nExecution complete")
  )
  foreach ($mutation in $stabilityMutations) {
    $mutated = if ($mutation[0] -ceq 'ready-before-recovery') {
      $stabilityLog.Replace("SDL_AUDIO_STABILITY_RECOVERY v=1 source=pause cycle=2 nonzero=1 result=00000000`nSDL_AUDIO_STABILITY_READY", "SDL_AUDIO_STABILITY_READY").Replace("MCLA_AUDIO_STABILITY_READY", "SDL_AUDIO_STABILITY_RECOVERY v=1 source=pause cycle=2 nonzero=1 result=00000000`nMCLA_AUDIO_STABILITY_READY")
    } else { $stabilityLog.Replace($mutation[1], $mutation[2]) }
    Write-Text $stabilityLogPath $mutated
    Expect-Failure { & $verify -RunPath $stability -Fixture | Out-Null } $mutation[0]
  }
  Write-Text $stabilityLogPath $stabilityLog

  $prior = @(
    [ordered]@{ task = 'M4-007'; run_id = '20260813-170202-44d2c7d8'; sha256 = 'A55CD1CAED7063CC811BB5F45EAD52B6DB971F8A80BF94C61F534B6BCA9F0A7A' },
    [ordered]@{ task = 'M4-008'; run_id = '20260813-182745-5b65003b'; sha256 = '578B0F7CA1E531A9F56E172A9625E37D549E69FDD23D7D5D77BBF0C33B85A1EB' },
    [ordered]@{ task = 'M5-009'; run_id = '20260814-170657-f44949d7'; sha256 = '4E3D514386501D92B43CD4F2C4C89ECD8BA000ACF23D8B168CFA431C8F67C62F' },
    [ordered]@{ task = 'M5-013'; run_id = '20260817-015958-36eec226'; sha256 = 'D890A903775A3B53262B9957E7B7DC1D4B76B49B8735360BECD8233842446298' }
  )
  $resultRecord = [ordered]@{ schema = 'mcla-audio-stability-v1'; task = 'M6-007'; decision = 'two-hour-audio-and-device-recovery-pass'; run_id = 'fixture'; prior_evidence = $prior; long_soak = [ordered]@{ run_id = 'long-soak'; log_set_sha256 = $soakProbe.LogSet.Hash; soak_seconds = 7200; controlled_exit = $true; route = $soakProbe.Route; resources = $soakProbe.Resources }; device_switch = [ordered]@{ run_id = 'device-switch'; log_set_sha256 = $stabilityProbe.LogSet.Hash; pause_cycles = 2; device_events = 1; device_recoveries = 1; operator_heard = $true; controlled_exit = $true }; scope = [ordered]@{ two_hour_soak = $true; current_pause_resume = $true; current_default_device_recovery = $true; operator_heard_recovery = $true; prior_stream_transition_bound = $true; prior_xmp_metadata_fallback_bound = $true; monolithic_run_claimed = $false; device_identity_recorded = $false; audio_mix_fidelity_claimed = $false } }
  $resultPath = Join-Path $root 'result.json'
  Write-Text $resultPath ((ConvertTo-Json $resultRecord -Depth 10) + "`n")
  $resultProbe = & $verify -ResultPath $resultPath -Fixture
  if (-not $resultProbe.Passed) { throw 'Positive result fixture failed.' }
  ++$positiveCount
  $badResult = (ConvertTo-Json $resultRecord -Depth 10).Replace('"operator_heard": true', '"operator_heard": false').Replace('"operator_heard":  true', '"operator_heard":  false')
  Write-Text $resultPath $badResult
  Expect-Failure { & $verify -ResultPath $resultPath -Fixture | Out-Null } 'result-operator-false'
  $resultRecord.long_soak.route.submit_frames++
  Write-Text $resultPath ((ConvertTo-Json $resultRecord -Depth 10) + "`n")
  Expect-Failure { & $verify -ResultPath $resultPath -Fixture | Out-Null } 'result-route-counter-drift'
  $resultRecord.long_soak.route.submit_frames--

  $source = [IO.File]::ReadAllText((Join-Path $repo 'src/mcla_app.cpp')) + [IO.File]::ReadAllText((Join-Path $repo 'third_party/rexglue-sdk/src/audio/audio_route_audit.cpp')) + [IO.File]::ReadAllText((Join-Path $repo 'third_party/rexglue-sdk/src/audio/audio_system.cpp')) + [IO.File]::ReadAllText((Join-Path $repo 'third_party/rexglue-sdk/src/audio/sdl/sdl_audio_driver.cpp')) + [IO.File]::ReadAllText($run) + [IO.File]::ReadAllText($verify)
  $needles = @('mcla_audio_stability_probe', 'sdl_audio_stability_audit', 'ArmAudioStabilityDeviceSwitch', 'PrepareAudioStabilityResume', 'PollDefaultDeviceChanges', 'AudioDeviceEvent::kMigrated', 'SDL_AUDIO_STABILITY_READY v=1', 'SDL_AUDIO_STABILITY_DEVICE v=1', 'SDL_AUDIO_STABILITY_RECOVERY v=1', 'SDL_AUDIO_STABILITY_SUMMARY v=1', 'SDL_PauseAudioStreamDevice', 'SDL_ResumeAudioStreamDevice', 'SDL_EVENT_AUDIO_DEVICE_ADDED', 'SDL_EVENT_AUDIO_DEVICE_REMOVED', 'SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED', '--mcla_audio_route_soak_seconds=7200', 'AUDIO DEVICE RECOVERED', 'two-hour-audio-and-device-recovery-pass', 'device_identity=redacted', 'pause_cycles=2', 'soak_seconds = 7200', '42 assertions in 10 test cases', 'ResourceSelfTest', 'resource-samples.json', 'sample_count = 13', 'private_bytes = 1024 * $MiB', 'AddMinutes(15)')
  foreach ($needle in $needles) { if (-not $source.Contains($needle)) { throw "Source contract missing '$needle'." } }
  if ($source -notmatch '(?s)REXCVAR_DEFINE_BOOL\(sdl_audio_stability_audit, false,.*?Lifecycle::kInitOnly\)') { throw 'Stability audit must remain default-off and InitOnly.' }
  if ($source.IndexOf('if (REXCVAR_GET(sdl_audio_stability_audit))', [StringComparison]::Ordinal) -lt 0 -or $source.IndexOf('if (REXCVAR_GET(sdl_audio_stability_audit))', [StringComparison]::Ordinal) -gt $source.IndexOf('SDL_AddEventWatch(SDLEventWatch, this)', [StringComparison]::Ordinal)) { throw 'SDL event monitoring is not audit-gated.' }
  foreach ($forbiddenIdentityField in @('device_name=', 'device_guid=', 'device_id=', 'physical_device_name=')) { if ($source.Contains($forbiddenIdentityField, [StringComparison]::OrdinalIgnoreCase)) { throw 'Stability telemetry exposes device identity.' } }
  $sourceChecks = $needles.Count + 3
  [pscustomobject]@{ Passed = $true; PhysicalPositives = $positiveCount; FailClosedNegatives = $negativeCount; SourceContractChecks = $sourceChecks }
} finally {
  if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
