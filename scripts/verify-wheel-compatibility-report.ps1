[CmdletBinding(DefaultParameterSetName = 'Result')]
param(
  [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$InputResultPath,
  [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$FfbLogPath,
  [Parameter(ParameterSetName = 'Probe')][switch]$ProbeOnly,
  [Parameter(ParameterSetName = 'Probe')][switch]$FixtureMode,
  [Parameter(Mandatory, ParameterSetName = 'Result')][string]$ResultPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdk = Join-Path $repo 'third_party/rexglue-sdk'

function Resolve-RepoFile([string]$Path, [string]$Description) {
  $full = if ([IO.Path]::IsPathRooted($Path)) { [IO.Path]::GetFullPath($Path) } else { [IO.Path]::GetFullPath((Join-Path $repo $Path)) }
  $prefix = $repo.TrimEnd('\') + '\'
  if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $full -PathType Leaf)) {
    throw "$Description is missing or escapes the repository."
  }
  $current = $repo
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object { $_ })) {
    $current = Join-Path $current $part
    if ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
      throw "$Description traverses a reparse point."
    }
  }
  $full
}

function Get-Sha256([string]$Path) {
  $stream = [IO.File]::OpenRead($Path)
  try {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') } finally { $sha.Dispose() }
  } finally { $stream.Dispose() }
}

function Assert-SourceContract {
  $referencePath = Resolve-RepoFile 'config/wheel-input-reference.json' 'Wheel reference'
  $reference = [IO.File]::ReadAllText($referencePath) | ConvertFrom-Json
  $implementedEffects = @('constant','ramp','square','sine','triangle','sawtooth-up','sawtooth-down','spring','damper')
  if (
    $reference.schema -cne 'mcla-wheel-input-reference-v1' -or
    $reference.guest_subtype -cne 'xbox-360-wheel' -or
    $reference.selection_policy -cne 'latest-meaningful-active-device' -or
    (@($reference.force_feedback.implemented_effects) -join ',') -cne ($implementedEffects -join ',') -or
    (@($reference.force_feedback.physically_verified_effects) -join ',') -cne 'constant,square,spring,damper' -or
    (@($reference.force_feedback.title_event_physically_verified_effects) -join ',') -cne 'square' -or
    $reference.force_feedback.event_effects_physical_verification_pending -or
    $reference.force_feedback.curb_effect_consistency -cne 'intermittent-title-submission' -or
    $reference.force_feedback.guest_effect_slots -ne 64 -or
    $reference.force_feedback.default_gain_percent -ne 100 -or
    $reference.force_feedback.continuous_periodic_default_gain_percent -ne 40 -or
    $reference.force_feedback.continuous_constant_default_gain_percent -ne 0 -or
    $reference.force_feedback.minimum_transient_strength_percent -ne 75 -or
    $reference.force_feedback.condition_translation -cne 'sign-preserving-sdl-condition-coefficient' -or
    $reference.force_feedback.minimum_gain_percent -ne 0 -or
    $reference.force_feedback.maximum_gain_percent -ne 100 -or
    -not $reference.force_feedback.resume_after_focus_gain -or
    -not $reference.force_feedback.wheel_basic_rumble_suppressed
  ) { throw 'Wheel reference policy changed.' }

  $template = [IO.File]::ReadAllText((Resolve-RepoFile 'config/mcla.toml.example' 'Host config template'))
  foreach ($needle in @(
    'wheel_force_feedback = true', 'wheel_force_feedback_gain = 100',
    'wheel_force_feedback_continuous_periodic_gain = 40',
    'wheel_force_feedback_continuous_constant_gain = 0',
    'wheel_force_feedback_minimum_transient_strength = 75',
    'wheel_steering_axis = 0', 'wheel_brake_axis = 1', 'wheel_accelerator_axis = 2',
    'wheel_button_left_shoulder = 0', 'wheel_button_right_shoulder = 1',
    'wheel_button_back = 6', 'wheel_button_start = 7',
    'wheel_button_left_thumb = 10', 'wheel_button_right_thumb = 11', 'wheel_button_guide = 12'
  )) { if (-not $template.Contains($needle)) { throw "Host config template is missing '$needle'." } }

  $driver = [IO.File]::ReadAllText((Resolve-RepoFile 'third_party/rexglue-sdk/src/input/sdl/sdl_input_driver.cpp' 'SDL input driver'))
  $driverHeader = [IO.File]::ReadAllText((Resolve-RepoFile 'third_party/rexglue-sdk/include/rex/input/sdl/sdl_input_driver.h' 'SDL input driver header'))
  $mapping = [IO.File]::ReadAllText((Resolve-RepoFile 'third_party/rexglue-sdk/include/rex/input/sdl/wheel_mapping.h' 'SDL wheel mapping'))
  if (-not $mapping.Contains('SDL_JOYSTICK_TYPE_WHEEL')) { throw 'SDL wheel admission contract is missing.' }
  foreach ($needle in @(
    'REXCVAR_DEFINE_INT32(wheel_force_feedback_gain, 100',
    'REXCVAR_DEFINE_INT32(wheel_force_feedback_continuous_periodic_gain,',
    'REXCVAR_DEFINE_INT32(wheel_force_feedback_continuous_constant_gain,',
    'wheel_force_feedback_minimum_transient_strength,',
    'state.is_wheel ? 0x02 : 0x01',
    'X_INPUT_CAPS_FFB_SUPPORTED',
    'SDL_WHEEL_FFB_DEVICE_INFO v=2 guest_slots={}',
    'action=resume slot={} type={} reason={}', 'ResumeControllerFeedback(',
    'StopControllerFeedback(previous, true)', 'ShouldPreserveWheelForceFeedback('
  )) { if (-not $driver.Contains($needle)) { throw "SDL wheel source contract is missing '$needle'." } }
  if ([regex]::Matches($driver, 'if \(controller[.]is_wheel\)').Count -lt 2) {
    throw 'Raw wheel events are not isolated from duplicate SDL gamepad axis/button events.'
  }
  if (-not $driverHeader.Contains('kWheelForceFeedbackEffectSlots = 64')) { throw 'SDL wheel source contract is missing the 64-slot bound.' }

  $hid = [IO.File]::ReadAllText((Resolve-RepoFile 'third_party/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_hid.cpp' 'XInputdFF bridge'))
  foreach ($name in @('GetDeviceInfo','SetEffect','UpdateEffect','EffectOperation','DeviceControl','SetDeviceGain','CancelIo','SetRumble')) {
    if (-not $hid.Contains("__imp__XInputdFF$name")) { throw "XInputdFF bridge is missing '$name'." }
  }
  if ($hid.Contains('REX_EXPORT_STUB(__imp__XInputdFF')) { throw 'An XInputdFF export regressed to a stub.' }
  if ($hid.Contains('REX_EXPORT_XINPUTD_FF_SUCCESS')) { throw 'An XInputdFF compatibility export lost its explicit audit hook.' }
  $inputContract = [IO.File]::ReadAllText((Resolve-RepoFile 'third_party/rexglue-sdk/include/rex/input/input.h' 'XInputdFF effect decoder'))
  foreach ($type in @('kConstant','kRamp','kSquare','kSine','kTriangle','kSawtoothUp','kSawtoothDown','kSpring','kDamper')) {
    if (-not $inputContract.Contains("ForceFeedbackEffectType::$type")) { throw "XInputdFF decoder is missing '$type'." }
  }
  if (-not $inputContract.Contains('array<ForceFeedbackCondition, 3>')) {
    throw 'XInputdFF decoder is missing the three-axis condition payload.'
  }

  $ffb = [IO.File]::ReadAllText((Resolve-RepoFile 'third_party/rexglue-sdk/include/rex/input/sdl/wheel_force_feedback.h' 'Wheel FFB scaling'))
  foreach ($needle in @('kMaximumWheelForceFeedbackGainPercent = 100','BuildSdlWheelEffect(','SDL_HAPTIC_CONSTANT','SDL_HAPTIC_RAMP','SDL_HAPTIC_DAMPER','(bounded_value * 32767) / 127')) {
    if (-not $ffb.Contains($needle)) { throw "Wheel FFB translation is missing '$needle'." }
  }
  [pscustomobject]@{
    ReferenceSha256 = Get-Sha256 $referencePath
    DefaultGainPercent = 100
    ContinuousPeriodicDefaultGainPercent = 40
    MaximumGainPercent = 100
    EventEffectsPhysicalVerificationPending = [bool]$reference.force_feedback.event_effects_physical_verification_pending
  }
}

function Get-PhysicalProbe([string]$InputPath, [string]$LogPath) {
  $inputFile = Resolve-RepoFile $InputPath 'Physical wheel result'
  $ffbFile = Resolve-RepoFile $LogPath 'Physical FFB log'
  $input = [IO.File]::ReadAllText($inputFile) | ConvertFrom-Json
  if (
    $input.schema -cne 'mcla-wheel-input-smoke-v1' -or $input.task -cne 'M6-015' -or
    -not $input.operator_confirmed -or $input.wheel.model -cne 'Thrustmaster T300RS Racing Wheel' -or
    $input.wheel.vendor_id -cne '044F' -or $input.wheel.product_id -cne 'B66E' -or
    $input.wheel.axes -ne 4 -or $input.wheel.buttons -ne 13 -or $input.wheel.hats -ne 1 -or
    -not $input.wheel.haptic_opened -or $input.physical_results.steering -cne 'pass' -or
    $input.physical_results.accelerator_pedal -cne 'pass' -or $input.physical_results.brake_pedal -cne 'pass' -or
    $input.physical_results.digital_menu_matrix -cne 'pass' -or $input.physical_results.hotplug_recovery -cne 'pass' -or
    -not $input.save.source_save_preserved
  ) { throw 'Physical wheel input/hotplug result failed.' }

  $text = [IO.File]::ReadAllText($ffbFile)
  $checks = [ordered]@{
    DeviceControl = [regex]::Matches($text, 'SDL_WHEEL_FFB_DEVICE_CONTROL v=1 command=4').Count
    Create = [regex]::Matches($text, 'SDL_WHEEL_FFB_EFFECT v=1 action=create slot=1 type=spring gain_percent=25').Count
    Start = [regex]::Matches($text, 'SDL_WHEEL_FFB_EFFECT v=1 action=start slot=1').Count
    FirstUpdate = [regex]::Matches($text, 'SDL_WHEEL_FFB_EFFECT v=1 action=first-update slot=1 .* gain_percent=25').Count
    Resume = [regex]::Matches($text, 'SDL_WHEEL_FFB_EFFECT v=1 action=resume slot=1 reason=focus').Count
    Complete = [regex]::Matches($text, '(?m)Execution complete\s*$').Count
  }
  if ($checks.DeviceControl -ne 1 -or $checks.Create -ne 1 -or $checks.Start -ne 1 -or $checks.FirstUpdate -ne 1 -or $checks.Resume -lt 1 -or $checks.Complete -ne 1) {
    throw 'Physical title-driven FFB/focus evidence is incomplete.'
  }
  if ($text -match '(?i)(REX_GUEST_CRASH|GUEST_CRASH_REPORT|PPC_UNIMPLEMENTED|\[fatal\]|D3D12.*device (?:lost|removed))') {
    throw 'Physical FFB evidence contains a fatal marker.'
  }
  [pscustomobject]@{
    InputResultSha256 = Get-Sha256 $inputFile; FfbLogSha256 = Get-Sha256 $ffbFile
    WheelConnectMarkers = [int]$input.runtime.wheel_connect_markers
    WheelRemoveMarkers = [int]$input.runtime.wheel_remove_markers
    ResumeMarkers = [int]$checks.Resume
    SaveTreeSha256 = [string]$input.save.final_tree_sha256
  }
}

function Get-EventProbe([string]$RunName) {
  if ($RunName -notmatch '^event-ffb-[0-9]{8}-[0-9]{6}-[0-9a-f]{8}$') {
    throw 'Wheel event evidence run name is invalid.'
  }
  $runRoot = Join-Path $repo "private/evidence/M6-015/$RunName"
  $resultFile = Resolve-RepoFile (Join-Path $runRoot 'result.json') 'Wheel event result'
  $logFile = Resolve-RepoFile (Join-Path $runRoot 'mcla.log') 'Wheel event runtime log'
  $observationFile = Resolve-RepoFile (Join-Path $runRoot 'operator-observation.json') 'Wheel event observation'
  $record = [IO.File]::ReadAllText($resultFile) | ConvertFrom-Json
  $eventVerifier = Join-Path $PSScriptRoot 'verify-wheel-event-ffb-smoke.ps1'
  $verified = & $eventVerifier -RuntimeLogPath $logFile -ObservationPath $observationFile
  if ($record.schema -cne 'mcla-wheel-event-ffb-smoke-v1' -or $record.task -cne 'M6-015' -or
      $record.decision -cne $verified.Decision -or $record.gain_percent -ne 100 -or
      $record.continuous_periodic_gain_percent -ne 40 -or
      $record.continuous_constant_gain_percent -ne 0 -or
      $record.minimum_transient_strength_percent -ne 75 -or
      $record.guest_effect_slots -ne 64 -or $record.compatibility_noop_calls -ne 0 -or
      -not $record.condition_polarity_verified -or -not $verified.ConditionPolarityVerified -or
      -not $record.continuous_constant_neutralized -or -not $verified.ContinuousConstantNeutralized -or
      (@($record.finite_event_types_verified) -join ',') -cne (@($verified.FiniteEventTypesVerified) -join ',') -or
      $record.curb_window_finite_starts -ne $verified.CurbWindowFiniteStarts -or
      $record.collision_window_finite_starts -ne $verified.CollisionWindowFiniteStarts -or
      $record.curb_window_finite_starts -lt 1 -or $record.collision_window_finite_starts -lt 1 -or
      $record.active_controller_transitions -lt 2 -or
      $record.runtime_log_sha256 -cne $verified.RuntimeLogSha256 -or
      $record.observation_sha256 -cne $verified.ObservationSha256 -or
      -not $record.controlled_external_close -or -not $record.source_save_preserved) {
    throw 'Wheel event result binding failed.'
  }
  [pscustomobject]@{
    ResultSha256 = Get-Sha256 $resultFile
    RuntimeLogSha256 = $verified.RuntimeLogSha256
    ObservationSha256 = $verified.ObservationSha256
    EffectTypes = @($verified.NonSpringActiveEffectTypes)
    FiniteEventTypes = @($verified.FiniteEventTypesVerified)
    CurbWindowFiniteStarts = $verified.CurbWindowFiniteStarts
    CollisionWindowFiniteStarts = $verified.CollisionWindowFiniteStarts
    ActiveControllerTransitions = $verified.ActiveControllerTransitions
  }
}

$source = Assert-SourceContract
if ($PSCmdlet.ParameterSetName -eq 'Probe') {
  if (-not $ProbeOnly) { throw 'Probe mode requires -ProbeOnly.' }
  Get-PhysicalProbe $InputResultPath $FfbLogPath
  return
}

$result = Resolve-RepoFile $ResultPath 'M6-015 result'
$raw = [IO.File]::ReadAllText($result)
if ($raw -match '(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])') { throw 'M6-015 result leaks a private or absolute path.' }
$record = $raw | ConvertFrom-Json
if ($record.schema -cne 'mcla-wheel-compatibility-report-v1' -or $record.task -cne 'M6-015' -or
    $record.decision -cne 'model-agnostic-wheel-input-and-title-event-ffb-pass') { throw 'M6-015 result identity failed.' }

$inputRun = '20260827-150416-d6e6c8e0'
$ffbRun = 'ffb-focus-retest-20260827-170345-1242fa05'
$physical = Get-PhysicalProbe "private/evidence/M6-015/$inputRun/result.json" "private/evidence/M6-015/$ffbRun/mcla.log"
$event = Get-EventProbe ([string]$record.event_evidence_run)
$sdkCommit = (& git -C $sdk rev-parse HEAD).Trim()
$sdkTag = (& git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim()
if ($LASTEXITCODE -ne 0 -or $sdkTag -notmatch '^v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$') { throw 'Current SDK commit is not exactly version-tagged.' }
if (
  $record.sdk_commit -cne $sdkCommit -or $record.sdk_version -cne $sdkTag.Substring(1) -or
  $record.reference_sha256 -cne $source.ReferenceSha256 -or
  $record.physical_input_result_sha256 -cne $physical.InputResultSha256 -or
  $record.physical_ffb_log_sha256 -cne $physical.FfbLogSha256 -or
  $record.physical_event_result_sha256 -cne $event.ResultSha256 -or
  $record.physical_event_log_sha256 -cne $event.RuntimeLogSha256 -or
  $record.physical_event_observation_sha256 -cne $event.ObservationSha256 -or
  (@($record.title_active_event_effect_types) -join ',') -cne (@($event.EffectTypes) -join ',') -or
  (@($record.finite_event_effect_types) -join ',') -cne (@($event.FiniteEventTypes) -join ',') -or
  $record.curb_window_finite_starts -ne $event.CurbWindowFiniteStarts -or
  $record.collision_window_finite_starts -ne $event.CollisionWindowFiniteStarts -or
  $record.wheel_model_physically_verified -cne 'Thrustmaster T300RS Racing Wheel' -or
  $record.other_wheel_models_physically_verified -ne 0 -or
  $record.default_gain_percent -ne 100 -or $record.maximum_gain_percent -ne 100 -or
  $record.continuous_periodic_default_gain_percent -ne 40 -or
  $record.continuous_constant_default_gain_percent -ne 0 -or
  $record.minimum_transient_strength_percent -ne 75 -or
  $record.direct_spring_confirmed_gain_percent -ne 25 -or
  $record.title_spring_confirmed_gain_percent -ne 25 -or
  $record.title_event_ffb_confirmed_gain_percent -ne 100 -or
  $record.guest_effect_slots -ne 64 -or $source.EventEffectsPhysicalVerificationPending -or
  -not $record.input_matrix_passed -or -not $record.hotplug_passed -or
  -not $record.title_spring_ffb_passed -or -not $record.title_event_ffb_passed -or
  -not $record.curb_effect_passed -or -not $record.collision_effect_passed -or
  -not $record.focus_resume_passed -or -not $record.latest_active_switch_passed -or
  $record.active_controller_transitions -ne $event.ActiveControllerTransitions -or
  $record.compatibility_noop_calls -ne 0 -or
  -not $record.external_operator_confirmation -or $record.exact_force_fidelity_claimed
) { throw 'M6-015 result claims or bindings failed.' }

$resultRoot = Split-Path $result -Parent
$unitLog = Resolve-RepoFile (Join-Path $resultRoot 'machine/sdl-wheel-tests.log') 'Wheel unit-test log'
$probeLog = Resolve-RepoFile (Join-Path $resultRoot 'machine/sdl-wheel-probe.log') 'Wheel enumeration log'
if ($record.unit_test_log_sha256 -cne (Get-Sha256 $unitLog) -or $record.probe_log_sha256 -cne (Get-Sha256 $probeLog)) {
  throw 'M6-015 machine-log binding failed.'
}
$unitText = [IO.File]::ReadAllText($unitLog)
$probeText = [IO.File]::ReadAllText($probeLog)
if ($unitText -notmatch 'All tests passed \(227 assertions in 9 test cases\)' -or
    $probeText -notmatch 'SDL_WHEEL_PROBE_RESULT status=PASS' -or
    $probeText -notmatch 'vendor=044F product=B66E' -or
    $probeText -notmatch 'effects=128 playing=128' -or
    $probeText -notmatch 'names=constant,sine,square,triangle,sawtooth-up,sawtooth-down,ramp,spring,damper') { throw 'M6-015 machine evidence failed.' }

[pscustomobject]@{
  Passed = $true; Decision = $record.decision; Wheel = $record.wheel_model_physically_verified
  DefaultGainPercent = $record.default_gain_percent; MaximumGainPercent = $record.maximum_gain_percent
  ContinuousPeriodicDefaultGainPercent = $record.continuous_periodic_default_gain_percent
  ContinuousConstantDefaultGainPercent = $record.continuous_constant_default_gain_percent
  MinimumTransientStrengthPercent = $record.minimum_transient_strength_percent
  FocusResumeMarkers = $physical.ResumeMarkers; EventEffectTypes = $event.EffectTypes
  OtherModelsPhysicallyVerified = 0
}
