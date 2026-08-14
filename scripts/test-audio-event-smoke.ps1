[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-audio-event-smoke.ps1'
$runner = Join-Path $PSScriptRoot 'run-audio-event-smoke.ps1'
$diagnostic = Join-Path $repo 'private/evidence/M5-009/diagnostic-20260814-153755'
$testRoot = Join-Path $repo ('private/evidence/M5-009/test-' + [guid]::NewGuid().ToString('N'))
$utf8 = [Text.UTF8Encoding]::new($false)
$positive = 0
$negative = 0
$sourceChecks = 0
$routeSummary = 'SDL_AUDIO_AUDIT_SUMMARY v=1 phase=title status=PASS client_calls=1 client_success=1 submit_frames=60000 submit_nonzero=59000 submit_invalid=0 submit_peak_ppm=12000 device_frames=59998 device_nonzero=58998 device_invalid=0 device_submit_fail=0 device_peak_ppm=10000 xma_frames=300000 xma_nonzero=300000 xma_invalid=0 xma_peak_ppm=900000 max_queue_depth=8 starvation_fills=0 max_consecutive_starvation_fills=0 dropped_records=0'

if (-not (Test-Path -LiteralPath (Join-Path $diagnostic 'mcla.log')) -or -not (Test-Path -LiteralPath (Join-Path $diagnostic 'user/mcla-first-frame.bmp'))) { throw 'Pinned private M5-009 diagnostic is missing.' }

function New-Probe([string]$Name) {
  $root = Join-Path $testRoot $Name
  [IO.Directory]::CreateDirectory((Join-Path $root 'cache')) | Out-Null
  [IO.Directory]::CreateDirectory((Join-Path $root 'user')) | Out-Null
  Copy-Item -LiteralPath (Join-Path $diagnostic 'user/mcla-first-frame.bmp') -Destination (Join-Path $root 'user/mcla-first-frame.bmp')
  $text = [IO.File]::ReadAllText((Join-Path $diagnostic 'mcla.log'))
  foreach ($phase in @('engine', 'collision', 'ui')) {
    $index = @{engine = 3; collision = 4; ui = 5}[$phase]
    $needle = "SDL_AUDIO_EVENT_AUDIT_PHASE v=1 event=BEGIN phase=$phase index=$index"
    if (-not $text.Contains($needle)) { throw "Diagnostic marker missing: $needle" }
    $text = $text.Replace($needle, $needle + "`r`nMCLA_AUDIO_EVENT_WINDOW v=1 phase=$phase event=LISTEN")
  }
  $appSummary = 'MCLA_AUDIO_EVENT_SUMMARY v=1 status=COMPLETE phases=6 external_listening_required=1 external_close_required=1'
  if (-not $text.Contains($appSummary)) { throw 'Diagnostic app summary is missing.' }
  $text = $text.Replace($appSummary, $routeSummary + "`r`n" + $appSummary)
  [IO.File]::WriteAllText((Join-Path $root 'mcla.log'), $text, $utf8)
  $root
}
function Rewrite([string]$Path, [string]$Old, [string]$New) { $text = [IO.File]::ReadAllText($Path); if (-not $text.Contains($Old)) { throw "Fixture needle missing: $Old" }; [IO.File]::WriteAllText($Path, $text.Replace($Old, $New), $utf8) }
function Log([string]$Root) { Join-Path $Root 'mcla.log' }
function Expect-Failure([string]$Name, [scriptblock]$Mutation) { $root = New-Probe $Name; & $Mutation $root; $failed = $false; try { & $verifier -RunPath $root -Fixture | Out-Null } catch { $failed = $true }; if (-not $failed) { throw "Negative fixture '$Name' was accepted." }; $script:negative++ }
function Assert-Source([bool]$Condition, [string]$Description) { if (-not $Condition) { throw "Source contract failed: $Description" }; $script:sourceChecks++ }

try {
  $root = New-Probe 'positive'
  $probe = & $verifier -RunPath $root -Fixture
  if ($probe.phases.Count -ne 6 -or -not $probe.controlled_exit -or $probe.xma_frames -ne 300000) { throw 'Positive fixture returned the wrong result.' }
  $positive++

  Expect-Failure 'wrong-event-config' { param($r) Rewrite (Log $r) 'sample_rate=48000' 'sample_rate=44100' }
  Expect-Failure 'duplicate-event-config' { param($r) Add-Content -LiteralPath (Log $r) -Value 'SDL_AUDIO_EVENT_AUDIT_CONFIG v=1 enabled=1 backend=sdl sample_rate=48000 phases=music,ambient,voice,engine,collision,ui' }
  Expect-Failure 'wrong-app-config' { param($r) Rewrite (Log $r) 'dismiss_pulses=6 music_seconds=8' 'dismiss_pulses=5 music_seconds=8' }
  Expect-Failure 'config-order' { param($r) $p = Log $r; $t = [IO.File]::ReadAllText($p); $a = 'MCLA_AUDIO_EVENT_CONFIG v=1'; $b = 'SDL_AUDIO_EVENT_AUDIT_CONFIG v=1'; $t = $t.Replace($a, 'TEMP_AUDIO_CONFIG').Replace($b, $a).Replace('TEMP_AUDIO_CONFIG', $b); [IO.File]::WriteAllText($p, $t, $utf8) }
  Expect-Failure 'missing-music-begin' { param($r) Rewrite (Log $r) 'event=BEGIN phase=music index=0' 'event=OMITTED phase=music index=0' }
  Expect-Failure 'missing-engine-listen' { param($r) Rewrite (Log $r) 'MCLA_AUDIO_EVENT_WINDOW v=1 phase=engine event=LISTEN' 'MCLA_AUDIO_EVENT_WINDOW v=1 phase=engine event=OMITTED' }
  Expect-Failure 'wrong-phase-order' { param($r) Rewrite (Log $r) 'event=BEGIN phase=collision index=4' 'event=BEGIN phase=collision index=3' }
  Expect-Failure 'music-too-short' { param($r) Rewrite (Log $r) 'phase=music index=0 status=PASS device_frames=1500' 'phase=music index=0 status=PASS device_frames=899' }
  Expect-Failure 'voice-too-short' { param($r) Rewrite (Log $r) 'phase=voice index=2 status=PASS device_frames=5625' 'phase=voice index=2 status=PASS device_frames=3499' }
  Expect-Failure 'collision-too-short' { param($r) Rewrite (Log $r) 'phase=collision index=4 status=PASS device_frames=2826' 'phase=collision index=4 status=PASS device_frames=1799' }
  Expect-Failure 'low-nonzero-ratio' { param($r) Rewrite (Log $r) 'phase=ambient index=1 status=PASS device_frames=1500 nonzero=1500' 'phase=ambient index=1 status=PASS device_frames=1500 nonzero=1349' }
  Expect-Failure 'invalid-event-frame' { param($r) Rewrite (Log $r) 'phase=voice index=2 status=PASS device_frames=5625 nonzero=5625 invalid=0' 'phase=voice index=2 status=PASS device_frames=5625 nonzero=5625 invalid=1' }
  Expect-Failure 'event-submit-failure' { param($r) Rewrite (Log $r) 'phase=engine index=3 status=PASS device_frames=1505 nonzero=1505 invalid=0 submit_failures=0' 'phase=engine index=3 status=PASS device_frames=1505 nonzero=1505 invalid=0 submit_failures=1' }
  Expect-Failure 'music-peak-low' { param($r) Rewrite (Log $r) 'phase=music index=0 status=PASS device_frames=1500 nonzero=1500 invalid=0 submit_failures=0 peak_ppm=3215' 'phase=music index=0 status=PASS device_frames=1500 nonzero=1500 invalid=0 submit_failures=0 peak_ppm=999' }
  Expect-Failure 'engine-peak-low' { param($r) Rewrite (Log $r) 'phase=engine index=3 status=PASS device_frames=1505 nonzero=1505 invalid=0 submit_failures=0 peak_ppm=11943' 'phase=engine index=3 status=PASS device_frames=1505 nonzero=1505 invalid=0 submit_failures=0 peak_ppm=4999' }
  Expect-Failure 'ui-peak-low' { param($r) Rewrite (Log $r) 'phase=ui index=5 status=PASS device_frames=872 nonzero=872 invalid=0 submit_failures=0 peak_ppm=8641' 'phase=ui index=5 status=PASS device_frames=872 nonzero=872 invalid=0 submit_failures=0 peak_ppm=1999' }
  Expect-Failure 'event-summary-fail' { param($r) Rewrite (Log $r) 'SDL_AUDIO_EVENT_AUDIT_SUMMARY v=1 status=PASS' 'SDL_AUDIO_EVENT_AUDIT_SUMMARY v=1 status=FAIL' }
  Expect-Failure 'unknown-event-marker' { param($r) Add-Content -LiteralPath (Log $r) -Value 'SDL_AUDIO_EVENT_AUDIT_UNKNOWN v=1 private=0' }
  Expect-Failure 'route-client-count' { param($r) Rewrite (Log $r) 'client_calls=1 client_success=1' 'client_calls=2 client_success=1' }
  Expect-Failure 'route-submit-low' { param($r) Rewrite (Log $r) 'submit_frames=60000 submit_nonzero=59000' 'submit_frames=9999 submit_nonzero=9999' }
  Expect-Failure 'route-submit-ratio' { param($r) Rewrite (Log $r) 'submit_frames=60000 submit_nonzero=59000' 'submit_frames=60000 submit_nonzero=53999' }
  Expect-Failure 'route-device-invalid' { param($r) Rewrite (Log $r) 'device_invalid=0 device_submit_fail=0' 'device_invalid=1 device_submit_fail=0' }
  Expect-Failure 'route-device-failure' { param($r) Rewrite (Log $r) 'device_invalid=0 device_submit_fail=0' 'device_invalid=0 device_submit_fail=1' }
  Expect-Failure 'route-xma-low' { param($r) Rewrite (Log $r) 'xma_frames=300000 xma_nonzero=300000' 'xma_frames=49999 xma_nonzero=49999' }
  Expect-Failure 'route-queue-high' { param($r) Rewrite (Log $r) 'max_queue_depth=8' 'max_queue_depth=65' }
  Expect-Failure 'route-starvation-high' { param($r) Rewrite (Log $r) 'starvation_fills=0 max_consecutive_starvation_fills=0' 'starvation_fills=3 max_consecutive_starvation_fills=1' }
  Expect-Failure 'missing-app-summary' { param($r) Rewrite (Log $r) 'MCLA_AUDIO_EVENT_SUMMARY v=1 status=COMPLETE' 'MCLA_AUDIO_EVENT_SUMMARY v=1 status=OMITTED' }
  Expect-Failure 'missing-close' { param($r) Rewrite (Log $r) 'Window closing, shutting down...' 'Window close omitted' }
  Expect-Failure 'missing-hard-exit' { param($r) Rewrite (Log $r) 'Title terminated; hard-exiting process.' 'Hard exit omitted' }
  Expect-Failure 'fatal-tail' { param($r) Add-Content -LiteralPath (Log $r) -Value 'FATAL: fixture' }
  Expect-Failure 'malformed-rotation' { param($r) Move-Item -LiteralPath (Log $r) -Destination (Join-Path $r 'mcla.bad.log') }
  Expect-Failure 'bad-bmp' { param($r) $p = Join-Path $r 'user/mcla-first-frame.bmp'; $b = [IO.File]::ReadAllBytes($p); $b[18] = 1; [IO.File]::WriteAllBytes($p, $b) }

  $app = [IO.File]::ReadAllText((Join-Path $repo 'src/mcla_app.cpp'))
  $sdkSource = [IO.File]::ReadAllText((Join-Path $repo 'third_party/rexglue-sdk/src/audio/audio_route_audit.cpp'))
  $sdkHeader = [IO.File]::ReadAllText((Join-Path $repo 'third_party/rexglue-sdk/include/rex/audio/audio_route_audit.h'))
  $runnerText = [IO.File]::ReadAllText($runner)
  $verifierText = [IO.File]::ReadAllText($verifier)
  Assert-Source ($app.Contains('mcla_audio_event_probe, false')) 'app probe defaults off'
  Assert-Source ($app.Contains('.lifecycle(rex::cvar::Lifecycle::kInitOnly)')) 'app probe is InitOnly'
  Assert-Source ($app.Contains('dismiss_pulses=6') -and $app.Contains('voice_seconds=30')) 'app route declares bounded durations'
  foreach ($phase in @('Music', 'Ambient', 'Voice', 'Engine', 'Collision', 'Ui')) { Assert-Source ($app.Contains("AudioEventPhase::k$phase")) "app contains $phase listening phase" }
  Assert-Source ($app.Contains('right_trigger = 255')) 'engine challenge applies full throttle'
  Assert-Source ($app.Contains('collision.thumb_lx = 32767')) 'collision challenge applies steering'
  Assert-Source ($app.Contains('X_INPUT_GAMEPAD_DPAD_DOWN')) 'UI challenge includes navigation'
  Assert-Source ($sdkSource.Contains('sdl_audio_event_audit, false')) 'SDK event audit defaults off'
  Assert-Source ($sdkSource.Contains('Lifecycle::kInitOnly')) 'SDK event audit is InitOnly'
  Assert-Source ($sdkHeader.Contains('enum class AudioEventPhase')) 'event vocabulary is allowlisted'
  Assert-Source ($sdkSource.Contains('SDL_AUDIO_EVENT_AUDIT_SUMMARY')) 'SDK emits bounded summary'
  Assert-Source (-not $sdkSource.Contains('device_name') -and -not $sdkSource.Contains('pcm_data')) 'event telemetry avoids identity and PCM'
  Assert-Source ($runnerText.Contains("v0.9.0.20") -and $runnerText.Contains('c4aa30c35386bb4d2ef051a59ea8e71bab667172')) 'runner pins exact SDK release'
  Assert-Source ($runnerText.Contains("'--sdl_audio_event_audit=true'") -and $runnerText.Contains("'--sdl_audio_route_audit=true'")) 'runner enables both bounded audits'
  Assert-Source ($runnerText.Contains('Other simultaneous sounds are allowed')) 'runner states presence-only criterion'
  Assert-Source ($runnerText.Contains('PASS MUSIC AMBIENT VOICE ENGINE COLLISION UI')) 'runner requires exact operator confirmation'
  Assert-Source ($verifierText.Contains('minimumFrames=@(900,900,3500,900,1800,500)')) 'verifier pins calibrated duration floors'
  Assert-Source ($verifierText.Contains('Unknown or malformed audio-event marker.')) 'verifier rejects unknown markers'
  Assert-Source ($verifierText.Contains('Get-ExactProcesses')) 'verifier checks process cleanup'
  Assert-Source ($verifierText.Contains('$root=Split-Path $result -Parent;Assert-NoReparseTree $root')) 'result tree is reparse-checked before sibling evidence reads'

  [pscustomobject][ordered]@{Status = 'PASS'; PhysicalPositives = $positive; FailClosedNegatives = $negative; SourceContractChecks = $sourceChecks; Decision = 'six-class-audio-stream-presence-pass'}
} finally {
  $full = [IO.Path]::GetFullPath($testRoot)
  $allowed = (Join-Path $repo 'private/evidence/M5-009').TrimEnd('\') + '\'
  if ((Test-Path -LiteralPath $full) -and $full.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) { Remove-Item -LiteralPath $full -Recurse -Force }
}
