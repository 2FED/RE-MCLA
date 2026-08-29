[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-wheel-event-ffb-smoke.ps1'
$root = Join-Path $repo ('private/evidence/M6-015/event-test-' + [guid]::NewGuid().ToString('N'))
$logPath = Join-Path $root 'mcla.log'
$observationPath = Join-Path $root 'operator-observation.json'
$utf8 = [Text.UTF8Encoding]::new($false)

function New-Observation {
  [pscustomobject]@{
    schema = 'mcla-wheel-event-ffb-observation-v3'
    task = 'M6-015'
    wheel_model = 'Thrustmaster T300RS Racing Wheel'
    centering = $true
    curb_or_rough_surface = $true
    collision = $true
    focus_resume = $true
    latest_active_wheel_gamepad_wheel = $true
    curb_window_start = '2026-08-29 12:00:00.000'
    curb_window_end = '2026-08-29 12:01:00.000'
    collision_window_start = '2026-08-29 12:02:00.000'
    collision_window_end = '2026-08-29 12:03:00.000'
    operator_confirmed = $true
  }
}

function New-LogLines {
  @(
    'SDL OnControllerDeviceAdded: Added at index 0.',
    'SDL_WHEEL_CONNECTED v=1 physical_slot=2 vendor=0x044F product=0xB66E axes=3 buttons=25 hats=1 haptic=1',
    'SDL_ACTIVE_CONTROLLER v=1 guest_slot=0 previous_physical_slot=2 physical_slot=0 reason=button-down activity_sequence=10',
    'SDL_ACTIVE_CONTROLLER v=1 guest_slot=0 previous_physical_slot=0 physical_slot=2 reason=wheel-button-down activity_sequence=11',
    'SDL_WHEEL_FFB_DEVICE_INFO v=2 guest_slots=64 host_effects=128 host_playing=128 features=0x00007FFF',
    'SDL_WHEEL_FFB_EFFECT v=2 action=create slot=1 type=spring gain_percent=100',
    'SDL_WHEEL_FFB_EFFECT v=2 action=start slot=1 type=spring',
    'SDL_WHEEL_FFB_EFFECT v=2 action=create slot=2 type=square gain_percent=100',
    'SDL_WHEEL_FFB_EFFECT v=2 action=start slot=2 type=square',
    'SDL_WHEEL_FFB_EFFECT v=2 action=first-update slot=2 type=square gain_percent=100',
    'SDL_WHEEL_FFB_PARAMS v=1 action=first-update slot=2 type=square duration=150 delay=0 guest_magnitude=76 guest_offset=0 guest_period=75 host_magnitude=24575 host_offset=0 host_period=75 gain_percent=100',
    'SDL_WHEEL_FFB_EFFECT v=2 action=resume slot=1 type=spring reason=focus',
    'SDL_WHEEL_FFB_EFFECT v=2 action=resume slot=1 type=spring reason=active-controller',
    'SDL_WHEEL_FFB_PARAMS v=1 action=create slot=1 type=spring duration=4294967295 delay=0 guest_center=0 guest_right_coeff=114 guest_left_coeff=114 guest_right_sat=25 guest_left_sat=25 guest_deadband=0 host_center=0 host_right_coeff=29412 host_left_coeff=29412 host_right_sat=6425 host_left_sat=6425 host_deadband=0 gain_percent=100',
    'SDL_WHEEL_FFB_PARAMS v=1 action=create slot=3 type=damper duration=4294967295 delay=0 guest_center=0 guest_right_coeff=76 guest_left_coeff=76 guest_right_sat=255 guest_left_sat=255 guest_deadband=0 host_center=0 host_right_coeff=19608 host_left_coeff=19608 host_right_sat=65535 host_left_sat=65535 host_deadband=0 gain_percent=100',
    'SDL_WHEEL_FFB_PARAMS v=1 action=first-update slot=4 type=constant duration=4294967295 delay=0 guest_level=-25 host_level=0 guest_attack=0 host_attack=0 guest_fade=0 host_fade=0 gain_percent=0',
    'SDL_WHEEL_FFB_EFFECT v=2 action=create slot=4 type=constant gain_percent=0',
    'SDL_WHEEL_FFB_EFFECT v=2 action=start slot=4 type=constant',
    'SDL_WHEEL_FFB_PARAMS v=1 action=create slot=5 type=constant duration=150 delay=0 guest_level=-12 host_level=-24575 guest_attack=0 host_attack=0 guest_fade=0 host_fade=0 gain_percent=100',
    'SDL_WHEEL_FFB_EFFECT v=2 action=create slot=5 type=constant gain_percent=100',
    'SDL_WHEEL_FFB_EFFECT v=2 action=start slot=5 type=constant',
    '[2026-08-29 12:00:30.000] [info] SDL_WHEEL_FFB_EFFECT v=2 action=start slot=2 type=square',
    '[2026-08-29 12:02:30.000] [info] SDL_WHEEL_FFB_EFFECT v=2 action=start slot=5 type=constant',
    'Execution complete'
  )
}

function Invoke-Fixture($Observation, [string[]]$Lines) {
  [IO.File]::WriteAllText($observationPath, ($Observation | ConvertTo-Json -Depth 4) + [Environment]::NewLine, $utf8)
  [IO.File]::WriteAllLines($logPath, $Lines, $utf8)
  & $verifier -FixtureMode -RuntimeLogPath $logPath -ObservationPath $observationPath
}

function Assert-Rejected([string]$Name, [scriptblock]$Mutation) {
  $observation = New-Observation
  $lines = [Collections.Generic.List[string]]::new()
  $lines.AddRange([string[]](New-LogLines))
  & $Mutation $observation $lines
  try { $null = Invoke-Fixture $observation $lines.ToArray() } catch { $script:negatives++; return }
  throw "Negative wheel-event fixture '$Name' was accepted."
}

function Invoke-WindowFixture($Observation, [string[]]$Lines) {
  [IO.File]::WriteAllText($observationPath, ($Observation | ConvertTo-Json -Depth 4) + [Environment]::NewLine, $utf8)
  [IO.File]::WriteAllLines($logPath, $Lines, $utf8)
  & $verifier -RuntimeLogPath $logPath -ObservationPath $observationPath
}

function Assert-WindowRejected([string]$Name, [scriptblock]$Mutation) {
  $observation = New-Observation
  $lines = [Collections.Generic.List[string]]::new()
  $lines.AddRange([string[]](New-LogLines))
  & $Mutation $observation $lines
  try { $null = Invoke-WindowFixture $observation $lines.ToArray() } catch { $script:negatives++; return }
  throw "Negative wheel-event window fixture '$Name' was accepted."
}

[IO.Directory]::CreateDirectory($root) | Out-Null
try {
  $positive = Invoke-Fixture (New-Observation) (New-LogLines)
  if (-not $positive.Passed -or $positive.GuestEffectSlots -ne 64 -or
      $positive.NonSpringActiveEffectTypes -cnotcontains 'square') {
    throw 'Positive wheel-event fixture failed.'
  }

  $negatives = 0
  Assert-Rejected 'centering-missing' { param($o,$l) $o.centering=$false }
  Assert-Rejected 'centering-not-boolean' { param($o,$l) $o.centering='true' }
  Assert-Rejected 'curb-missing' { param($o,$l) $o.curb_or_rough_surface=$false }
  Assert-Rejected 'collision-missing' { param($o,$l) $o.collision=$false }
  Assert-Rejected 'focus-missing' { param($o,$l) $o.focus_resume=$false }
  Assert-Rejected 'latest-active-missing' { param($o,$l) $o.latest_active_wheel_gamepad_wheel=$false }
  Assert-Rejected 'wrong-wheel' { param($o,$l) $o.wheel_model='Other' }
  Assert-Rejected 'slot-cap' { param($o,$l) $l[4]=$l[4].Replace('guest_slots=64','guest_slots=1') }
  Assert-Rejected 'host-cap' { param($o,$l) $l[4]=$l[4].Replace('host_effects=128','host_effects=32') }
  Assert-Rejected 'feature-family' { param($o,$l) $l[4]=$l[4].Replace('features=0x00007FFF','features=0x000001FE') }
  Assert-Rejected 'spring-create' { param($o,$l) $l.RemoveAt(5) }
  Assert-Rejected 'spring-start' { param($o,$l) $l.RemoveAt(6) }
  Assert-Rejected 'non-spring-activity' { param($o,$l) $l.RemoveAt(22);$l.RemoveAt(21);$l.RemoveAt(20);$l.RemoveAt(9);$l.RemoveAt(8) }
  Assert-Rejected 'parameter-telemetry' { param($o,$l) $l.RemoveAt(14);$l.RemoveAt(10) }
  Assert-Rejected 'focus-resume-marker' { param($o,$l) $l.RemoveAt(11) }
  Assert-Rejected 'active-controller-resume-marker' { param($o,$l) $l.RemoveAt(12) }
  Assert-Rejected 'wheel-to-gamepad-transition' { param($o,$l) $l.RemoveAt(2) }
  Assert-Rejected 'gamepad-to-wheel-transition' { param($o,$l) $l.RemoveAt(3) }
  Assert-Rejected 'compat-noop' { param($o,$l) $l.Insert(11,'XINPUTD_FF_AUDIT v=1 export=SetRumble action=compat-success-noop') }
  Assert-Rejected 'haptic-failure' { param($o,$l) $l.Insert(11,'SDL wheel FFB update failed for slot 2 type square: injected') }
  Assert-Rejected 'fatal' { param($o,$l) $l.Insert(11,'REX_GUEST_CRASH injected') }
  Assert-Rejected 'spring-sign-inverted' { param($o,$l) $l[13]=$l[13].Replace('host_right_coeff=29412 host_left_coeff=29412','host_right_coeff=-29412 host_left_coeff=-29412') }
  Assert-Rejected 'damper-sign-inverted' { param($o,$l) $l[14]=$l[14].Replace('host_right_coeff=19608 host_left_coeff=19608','host_right_coeff=-19608 host_left_coeff=-19608') }
  Assert-Rejected 'continuous-constant-not-neutral' { param($o,$l) $l[15]=$l[15].Replace('host_level=0','host_level=-6450').Replace('gain_percent=0','gain_percent=100') }
  Assert-Rejected 'finite-event-parameters' { param($o,$l) $l.RemoveAt(18);$l.RemoveAt(10) }
  Assert-Rejected 'finite-event-too-weak' { param($o,$l) $l[10]=$l[10].Replace('host_magnitude=24575','host_magnitude=8994');$l[18]=$l[18].Replace('host_level=-24575','host_level=-3096') }
  Assert-Rejected 'finite-event-start' { param($o,$l) $l.RemoveAt(22);$l.RemoveAt(21);$l.RemoveAt(20);$l.RemoveAt(8) }

  $windowPositive = Invoke-WindowFixture (New-Observation) (New-LogLines)
  if ($windowPositive.CurbWindowFiniteStarts -ne 1 -or $windowPositive.CollisionWindowFiniteStarts -ne 1) {
    throw 'Positive bounded-action wheel-event fixture failed.'
  }
  Assert-WindowRejected 'curb-window-start' { param($o,$l) $l.RemoveAt(21) }
  Assert-WindowRejected 'collision-window-start' { param($o,$l) $l.RemoveAt(22) }
  Assert-WindowRejected 'window-chronology' { param($o,$l) $o.curb_window_end='2026-08-29 12:02:30.000' }

  $runnerText = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'run-wheel-event-ffb-smoke.ps1'))
  $verifierText = [IO.File]::ReadAllText($verifier)
  $needles = @('guest_slots=', 'host_effects=', 'features=0x', 'NonSpringActiveEffectTypes',
    'compat-success-noop', 'curb_or_rough_surface', 'collision', 'focus_resume',
    'latest_active_wheel_gamepad_wheel', 'SDL_ACTIVE_CONTROLLER', 'Get-Sha256',
    'SDL_WHEEL_FFB_PARAMS', 'ParameterizedEffectTypes', 'reason=focus',
    'reason=active-controller', 'guest_right_coeff=', 'ConditionPolarityVerified',
    'ContinuousConstantNeutralized', 'FiniteEventTypesVerified',
    'curb_window_start', 'CurbWindowFiniteStarts',
    'SDL wheel FFB (?:create|update|start|stop).*failed')
  foreach ($needle in $needles) {
    if (-not $verifierText.Contains($needle)) { throw "Wheel-event source contract is missing '$needle'." }
  }
  $runnerNeedles = @('wheel_force_feedback_gain=100', 'wheel_force_feedback_continuous_periodic_gain=40',
    'wheel_force_feedback_continuous_constant_gain=0',
    'wheel_force_feedback_minimum_transient_strength=75', '227 assertions in 9 test cases',
    '--config Release --target install --parallel 8',
    'Press OPTIONS', 'curb or rough road edge', 'controlled contact',
    'GAMEPLAY READY', 'CURB DONE', 'COLLISION DONE', 'wheel -> gamepad -> wheel',
    'Write-ObservationSnapshot', "capture_status = 'initialized'",
    'xam_user_signin_state=2', '6734627B56ECD9B35E7B6FC362D374804480BEC2E11DA272BBCBC223719426B7',
    'continuous_constant_neutralized', 'finite_event_types_verified',
    'WM_CLOSE', "source_save_preserved = `$true")
  foreach ($needle in $runnerNeedles) {
    if (-not $runnerText.Contains($needle)) { throw "Wheel-event runner contract is missing '$needle'." }
  }
  if ($verifierText.Contains('Get-FileHash') -or $runnerText.Contains('Get-FileHash')) {
    throw 'Wheel-event scripts must not require Get-FileHash.'
  }

  [pscustomobject]@{
    Passed = $true
    FixturePositives = 2
    FailClosedNegatives = $negatives
    SourceContractChecks = $needles.Count + $runnerNeedles.Count + 1
  }
} finally {
  if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
