[CmdletBinding(DefaultParameterSetName = 'Result')]
param(
  [Parameter(Mandatory, ParameterSetName = 'Probe')]
  [string]$RuntimeLogPath,

  [Parameter(ParameterSetName = 'Probe')]
  [switch]$ProbeOnly,

  [Parameter(ParameterSetName = 'Probe')]
  [switch]$FixtureMode,

  [Parameter(Mandatory, ParameterSetName = 'Result')]
  [string]$ResultPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$utf8 = [Text.UTF8Encoding]::new($false)
$evidenceSdkCommit = '923c92d1d1cb721cb704ac603fba263a01ba06aa'
$currentSdkCommit = '53c16fcfcbfee83752b7689cf74aba1d69a185fa'
$gameplayRun = '20260814-130533-0b95f6b6'
$gameplayResultHash = 'A89C0CC3E02C8D264B0DA29157021D050276BF46F028BBAAAD9B1FFC220CCEAB'
$digitalRun = '20260812-212030-5fc01c73'
$xinputdffNames = @(
  'XInputdFFGetDeviceInfo', 'XInputdFFSetEffect', 'XInputdFFUpdateEffect',
  'XInputdFFEffectOperation', 'XInputdFFDeviceControl',
  'XInputdFFSetDeviceGain', 'XInputdFFCancelIo', 'XInputdFFSetRumble'
)

function Resolve-SafePath {
  param(
    [Parameter(Mandatory)][string]$Path,
    [Parameter(Mandatory)][string]$Description,
    [switch]$Directory
  )

  $full = if ([IO.Path]::IsPathRooted($Path)) {
    [IO.Path]::GetFullPath($Path)
  } else {
    [IO.Path]::GetFullPath((Join-Path $repo $Path))
  }
  $prefix = $repo.TrimEnd('\') + '\'
  if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "$Description escapes the repository."
  }
  $current = $repo
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object { $_ })) {
    $current = Join-Path $current $part
    if (-not (Test-Path -LiteralPath $current)) {
      throw "$Description is missing."
    }
    if ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
      throw "$Description traverses a reparse point."
    }
  }
  if ($Directory) {
    if (-not (Test-Path -LiteralPath $full -PathType Container)) {
      throw "$Description is not a directory."
    }
  } elseif (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
    throw "$Description is not a file."
  }
  return $full
}

function Get-LogSet {
  param([Parameter(Mandatory)][string]$CurrentLogPath)

  $current = Resolve-SafePath $CurrentLogPath 'Current runtime log'
  if ((Split-Path $current -Leaf) -cne 'mcla.log') {
    throw 'Current runtime log must be named mcla.log.'
  }
  $files = @(Get-ChildItem -LiteralPath (Split-Path $current -Parent) -File -Filter 'mcla*.log')
  $rotated = @()
  $currentFile = $null
  foreach ($file in $files) {
    if ($file.Name -ceq 'mcla.log') {
      $currentFile = $file
    } elseif ($file.Name -match '^mcla\.([1-9][0-9]*)\.log$') {
      $rotated += [pscustomobject]@{ Index = [int]$Matches[1]; File = $file }
    } else {
      throw 'Unexpected runtime-log name.'
    }
  }
  if ($null -eq $currentFile -or $files.Count -lt 1 -or $files.Count -gt 16) {
    throw 'Runtime-log topology is invalid.'
  }
  $indices = @($rotated | ForEach-Object { $_.Index } | Sort-Object)
  if ($indices.Count -and (($indices -join ',') -cne ((1..$indices.Count) -join ','))) {
    throw 'Runtime-log rotations are not contiguous.'
  }
  $ordered = @($rotated | Sort-Object Index -Descending | ForEach-Object { $_.File }) + @($currentFile)
  $builder = [Text.StringBuilder]::new()
  foreach ($file in $ordered) {
    $stream = [IO.File]::Open($file.FullName, 'Open', 'Read', 'ReadWrite')
    try {
      $reader = [IO.StreamReader]::new($stream, $utf8, $true)
      try { $null = $builder.AppendLine($reader.ReadToEnd()) } finally { $reader.Dispose() }
    } finally {
      $stream.Dispose()
    }
  }
  return [pscustomobject]@{ Text = $builder.ToString(); Count = $ordered.Count }
}

function Get-FunctionBody {
  param([string]$Text, [string]$Signature)

  $start = $Text.IndexOf($Signature, [StringComparison]::Ordinal)
  if ($start -lt 0) { throw "Source signature '$Signature' is missing." }
  $brace = $Text.IndexOf('{', $start)
  if ($brace -lt 0) { throw "Source signature '$Signature' has no body." }
  $depth = 0
  for ($index = $brace; $index -lt $Text.Length; $index++) {
    if ($Text[$index] -eq '{') { $depth++ }
    if ($Text[$index] -eq '}') {
      $depth--
      if ($depth -eq 0) { return $Text.Substring($start, $index - $start + 1) }
    }
  }
  throw "Source signature '$Signature' is unterminated."
}

function Assert-SourceContract {
  $sdk = Join-Path $repo 'third_party/rexglue-sdk'
  if ((& git -C $sdk rev-parse HEAD).Trim() -cne $currentSdkCommit) {
    throw 'SDK commit changed.'
  }
  & git -C $sdk diff --quiet v0.9.0.18 v0.9.0.19 -- include/rex/input src/input src/kernel/xam/xam_input.cpp tests/unit/input
  if ($LASTEXITCODE -ne 0) { throw 'Force-feedback source changed after accepted evidence.' }

  $sdl = [IO.File]::ReadAllText((Join-Path $sdk 'src/input/sdl/sdl_input_driver.cpp'))
  $caps = Get-FunctionBody $sdl 'void SDLInputDriver::UpdateXCapabilities'
  if (
    $caps.Contains('cap_flags |= X_INPUT_CAPS_FFB_SUPPORTED') -or
    -not $caps.Contains("Don't advertise the Xbox 360 force-feedback interface")
  ) {
    throw 'Still-stubbed XInputdFF capability is advertised.'
  }
  $setState = Get-FunctionBody $sdl 'X_RESULT SDLInputDriver::SetState'
  foreach ($needle in @(
    'return X_ERROR_DEVICE_NOT_CONNECTED;',
    'SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN',
    'SdlBoolToXResult(SDL_RumbleGamepad(',
    'return X_ERROR_FUNCTION_FAILED;',
    'return X_ERROR_SUCCESS;'
  )) {
    if (-not $setState.Contains($needle)) {
      throw "SDL rumble contract is missing '$needle'."
    }
  }

  $hid = [IO.File]::ReadAllText((Join-Path $sdk 'src/kernel/xboxkrnl/xboxkrnl_hid.cpp'))
  $stubMatches = [regex]::Matches($hid, 'REX_EXPORT_STUB\(__imp__(XInputdFF[A-Za-z]+)\);')
  if (
    $stubMatches.Count -ne 8 -or
    (($stubMatches | ForEach-Object { $_.Groups[1].Value }) -join ',') -cne ($xinputdffNames -join ',')
  ) {
    throw 'XInputdFF stub inventory changed.'
  }

  $xam = [IO.File]::ReadAllText((Join-Path $sdk 'src/kernel/xam/xam_input.cpp'))
  $xamSetState = Get-FunctionBody $xam 'u32 XamInputSetState_entry'
  foreach ($needle in @('if (!vibration)', 'return X_ERROR_BAD_ARGUMENTS;', 'return is->SetState(actual_user_index, vibration);')) {
    if (-not $xamSetState.Contains($needle)) {
      throw "Concrete XamInputSetState contract is missing '$needle'."
    }
  }

  $inputSystem = [IO.File]::ReadAllText((Join-Path $sdk 'src/input/input_system.cpp'))
  $aggregate = Get-FunctionBody $inputSystem 'X_RESULT InputSystem::SetState'
  foreach ($needle in @('result != X_ERROR_DEVICE_NOT_CONNECTED', 'return result;', 'return X_ERROR_DEVICE_NOT_CONNECTED;')) {
    if (-not $aggregate.Contains($needle)) {
      throw "InputSystem SetState degradation contract is missing '$needle'."
    }
  }
  return [pscustomobject]@{
    XInputdFFStubExports = $stubMatches.Count
    FfbCapabilityAdvertised = $false
    XamInputSetStateConcrete = $true
    SdlRumbleConcrete = $true
    DisconnectedResult = '0000048F'
  }
}

function Get-GameplayProbe {
  param([string]$LogPath)

  $set = Get-LogSet $LogPath
  $text = $set.Text
  $resolutions = [regex]::Matches(
    $text,
    '(?m)^.*GetProcAddressByOrdinal: (?<name>XInputdFF[A-Za-z]+) \((?<ordinal>028[2-9])\) in xboxkrnl -> thunk at [0-9A-Fa-f]+\s*$')
  if ($resolutions.Count -ne 8) { throw 'Current route does not resolve exactly eight XInputdFF imports.' }
  for ($index = 0; $index -lt 8; $index++) {
    if (
      $resolutions[$index].Groups['name'].Value -cne $xinputdffNames[$index] -or
      $resolutions[$index].Groups['ordinal'].Value -cne ('028{0}' -f ($index + 2))
    ) {
      throw 'XInputdFF import-resolution inventory changed.'
    }
  }
  $stubCalls = [regex]::Matches($text, '(?m)^.*__imp__XInputdFF[A-Za-z]+ STUB\s*$')
  $summary = [regex]::Matches($text, '(?m)^.*MCLA_GAMEPLAY_INPUT_SUMMARY v=1 status=PASS frames=8 gameplay_input_records=24 dismiss_input_records=24 physical_reconnect_evidence=external external_close_required=1\s*$')
  $complete = [regex]::Matches($text, '(?m)^.*Execution complete\s*$')
  if ($stubCalls.Count -ne 0 -or $summary.Count -ne 1 -or $complete.Count -ne 1) {
    throw 'Current saved-gameplay force-feedback degradation route failed.'
  }
  if ($text -match '(?i)(REX_GUEST_CRASH|GUEST_CRASH_REPORT|PPC_UNIMPLEMENTED|\[fatal\]|D3D12.*device (?:lost|removed))') {
    throw 'Current saved-gameplay route contains a fatal marker.'
  }
  return [pscustomobject]@{
    ModuleResolutionCount = $resolutions.Count
    StubCallMarkers = $stubCalls.Count
    GameplayPassed = $true
    ControlledExit = $true
    LogFileCount = $set.Count
  }
}

function Get-PhysicalRumbleProbe {
  # The accepted M5-006 verifier invoked immediately before this probe already
  # re-verifies the complete recovered M4-006 controller chain. Re-parse the
  # exact digital layer here only to bind the six rumble records themselves.
  $digitalLog = Join-Path $repo "private/evidence/M4-006/$digitalRun/runs/01/mcla.log"
  $text = (Get-LogSet $digitalLog).Text
  $rumble = [regex]::Matches(
    $text,
    '(?m)^.*SDL_CONTROLLER_MATRIX_AUDIT_RUMBLE v=1 event=(?<event>start|stop) slot=0 pattern=(?<pattern>left|right|both) result=00000000 supported=1\s*$')
  $expected = @('start:left', 'stop:left', 'start:right', 'stop:right', 'start:both', 'stop:both')
  if ($rumble.Count -ne 6) { throw 'Physical rumble command matrix is incomplete.' }
  for ($index = 0; $index -lt 6; $index++) {
    if (($rumble[$index].Groups['event'].Value + ':' + $rumble[$index].Groups['pattern'].Value) -cne $expected[$index]) {
      throw 'Physical rumble command order changed.'
    }
  }
  return [pscustomobject]@{
    RunId = $digitalRun
    CommandRecords = 6
    Patterns = 'left-right-both'
    AllResultsSuccess = $true
    SupportedProperty = $true
    PhysicalDisconnectObserved = $true
    PhysicalReconnectObserved = $true
  }
}

function Assert-ExactProperties {
  param($Object, [string[]]$Expected, [string]$Description)
  if (($Object.PSObject.Properties.Name -join ',') -cne ($Expected -join ',')) {
    throw "$Description schema changed."
  }
}

function Assert-PropertyTypes {
  param(
    $Object,
    [string[]]$Booleans = @(),
    [string[]]$Integers = @(),
    [string]$Description
  )
  foreach ($name in $Booleans) {
    if ($Object.$name -isnot [bool]) { throw "$Description '$name' is not Boolean." }
  }
  foreach ($name in $Integers) {
    if ($Object.$name -isnot [int]) { throw "$Description '$name' is not Int32." }
  }
}

$source = Assert-SourceContract
if ($PSCmdlet.ParameterSetName -eq 'Probe') {
  if (-not $ProbeOnly) { throw 'Probe mode requires -ProbeOnly.' }
  Get-GameplayProbe $RuntimeLogPath
  return
}

$result = Resolve-SafePath $ResultPath 'M5-007 result'
$raw = [IO.File]::ReadAllText($result)
if ($raw -match '(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])') {
  throw 'M5-007 result contains a private or absolute path.'
}
$record = $raw | ConvertFrom-Json
Assert-ExactProperties $record @(
  'schema', 'task', 'decision', 'sdk_version', 'sdk_commit',
  'gameplay_run_id', 'gameplay_result_sha256', 'current_gameplay',
  'ffb_surface', 'physical_rumble', 'unsupported_degradation', 'scope',
  'data_integrity_verified'
) 'M5-007 result'
if (
  $record.schema -isnot [int] -or $record.schema -ne 1 -or
  $record.task -cne 'M5-007' -or
  $record.decision -cne 'ffb-withheld-host-rumble-bounded' -or
  $record.sdk_version -cne '0.9.0.18' -or
  $record.sdk_commit -cne $evidenceSdkCommit -or
  $record.gameplay_run_id -cne $gameplayRun -or
  $record.gameplay_result_sha256 -cne $gameplayResultHash -or
  $record.data_integrity_verified -isnot [bool] -or -not $record.data_integrity_verified
) {
  throw 'M5-007 result identity failed.'
}
$resultRoot = Split-Path $result -Parent
if (
  (Split-Path $result -Leaf) -cne 'result.json' -or
  (Split-Path (Split-Path $resultRoot -Parent) -Leaf) -cne 'M5-007' -or
  (Split-Path $resultRoot -Leaf) -notmatch '^[0-9]{8}-[0-9]{6}-[0-9a-f]{8}$' -or
  ((@(Get-ChildItem -LiteralPath $resultRoot -Force | ForEach-Object Name | Sort-Object) -join ',') -cne 'result.json')
) {
  throw 'M5-007 result topology failed.'
}

$gameplayResult = Resolve-SafePath "private/evidence/M5-006/$gameplayRun/result.json" 'Accepted M5-006 result'
if ((Get-FileHash $gameplayResult -Algorithm SHA256).Hash -cne $gameplayResultHash) {
  throw 'Accepted M5-006 result identity changed.'
}
$gameplayVerified = & (Join-Path $PSScriptRoot 'verify-gameplay-input-smoke.ps1') -ResultPath $gameplayResult
if (-not $gameplayVerified.Passed -or -not $gameplayVerified.PhysicalReconnectPassed) {
  throw 'Accepted saved-gameplay input route failed re-verification.'
}
$gameplay = Get-GameplayProbe (Join-Path (Split-Path $gameplayResult -Parent) 'runs/01/mcla.log')
$physical = Get-PhysicalRumbleProbe

Assert-ExactProperties $record.current_gameplay @(
  'route_passed', 'module_resolution_count', 'module_resolution_ordinals',
  'xinputdff_stub_call_markers', 'gameplay_input_records',
  'pause_correlation_ppm', 'controlled_exit'
) 'Current gameplay'
Assert-PropertyTypes $record.current_gameplay `
  -Booleans @('route_passed', 'controlled_exit') `
  -Integers @('module_resolution_count', 'xinputdff_stub_call_markers', 'gameplay_input_records', 'pause_correlation_ppm') `
  -Description 'Current gameplay'
if (
  $record.current_gameplay.route_passed -isnot [bool] -or -not $record.current_gameplay.route_passed -or
  $record.current_gameplay.module_resolution_count -ne $gameplay.ModuleResolutionCount -or
  $record.current_gameplay.module_resolution_ordinals -cne '0282-0289' -or
  $record.current_gameplay.xinputdff_stub_call_markers -ne 0 -or
  $record.current_gameplay.gameplay_input_records -ne $gameplayVerified.GameplayInputRecords -or
  $record.current_gameplay.pause_correlation_ppm -ne $gameplayVerified.PauseCorrelationPpm -or
  $record.current_gameplay.controlled_exit -isnot [bool] -or -not $record.current_gameplay.controlled_exit
) { throw 'Current gameplay aggregate mismatch.' }

Assert-ExactProperties $record.ffb_surface @(
  'xinputdff_stub_exports', 'ffb_capability_advertised',
  'xam_input_set_state_concrete', 'sdl_rumble_concrete',
  'device_disconnected_result', 'basic_rumble_uses_xam_set_state'
) 'FFB surface'
Assert-PropertyTypes $record.ffb_surface `
  -Booleans @('ffb_capability_advertised', 'xam_input_set_state_concrete', 'sdl_rumble_concrete', 'basic_rumble_uses_xam_set_state') `
  -Integers @('xinputdff_stub_exports') `
  -Description 'FFB surface'
if (
  $record.ffb_surface.xinputdff_stub_exports -ne $source.XInputdFFStubExports -or
  $record.ffb_surface.ffb_capability_advertised -isnot [bool] -or $record.ffb_surface.ffb_capability_advertised -or
  $record.ffb_surface.xam_input_set_state_concrete -isnot [bool] -or -not $record.ffb_surface.xam_input_set_state_concrete -or
  $record.ffb_surface.sdl_rumble_concrete -isnot [bool] -or -not $record.ffb_surface.sdl_rumble_concrete -or
  $record.ffb_surface.device_disconnected_result -cne $source.DisconnectedResult -or
  $record.ffb_surface.basic_rumble_uses_xam_set_state -isnot [bool] -or -not $record.ffb_surface.basic_rumble_uses_xam_set_state
) { throw 'FFB source surface mismatch.' }

Assert-ExactProperties $record.physical_rumble @(
  'run_id', 'command_records', 'patterns', 'all_results_success',
  'supported_property', 'user_report_source',
  'confirmation_recorded_in_run', 'attestation_machine_verified'
) 'Physical rumble'
Assert-PropertyTypes $record.physical_rumble `
  -Booleans @('all_results_success', 'supported_property', 'confirmation_recorded_in_run', 'attestation_machine_verified') `
  -Integers @('command_records') `
  -Description 'Physical rumble'
if (
  $record.physical_rumble.run_id -cne $physical.RunId -or
  $record.physical_rumble.command_records -ne 6 -or
  $record.physical_rumble.patterns -cne 'left-right-both' -or
  -not $record.physical_rumble.all_results_success -or
  -not $record.physical_rumble.supported_property -or
  $record.physical_rumble.user_report_source -cne 'external-user-report' -or
  $record.physical_rumble.confirmation_recorded_in_run -or
  $record.physical_rumble.attestation_machine_verified
) { throw 'Physical rumble evidence mismatch.' }

Assert-ExactProperties $record.unsupported_degradation @(
  'capability_withheld', 'stub_path_entered', 'saved_gameplay_blocked',
  'physical_disconnect_observed', 'physical_reconnect_observed',
  'nop_masking_present'
) 'Unsupported degradation'
Assert-PropertyTypes $record.unsupported_degradation `
  -Booleans @(
    'capability_withheld', 'stub_path_entered', 'saved_gameplay_blocked',
    'physical_disconnect_observed', 'physical_reconnect_observed', 'nop_masking_present') `
  -Description 'Unsupported degradation'
if (
  -not $record.unsupported_degradation.capability_withheld -or
  $record.unsupported_degradation.stub_path_entered -or
  $record.unsupported_degradation.saved_gameplay_blocked -or
  -not $record.unsupported_degradation.physical_disconnect_observed -or
  -not $record.unsupported_degradation.physical_reconnect_observed -or
  $record.unsupported_degradation.nop_masking_present
) { throw 'Unsupported-device degradation claim failed.' }

Assert-ExactProperties $record.scope @(
  'host_rumble_diagnostic_only', 'title_driven_force_feedback_claimed',
  'new_physical_rumble_test', 'multi_pad_claimed'
) 'Scope'
Assert-PropertyTypes $record.scope `
  -Booleans @(
    'host_rumble_diagnostic_only', 'title_driven_force_feedback_claimed',
    'new_physical_rumble_test', 'multi_pad_claimed') `
  -Description 'Scope'
if (
  -not $record.scope.host_rumble_diagnostic_only -or
  $record.scope.title_driven_force_feedback_claimed -or
  $record.scope.new_physical_rumble_test -or
  $record.scope.multi_pad_claimed
) { throw 'M5-007 scope overclaims coverage.' }

[pscustomobject]@{
  Passed = $true
  Decision = $record.decision
  XInputdFFStubExports = 8
  XInputdFFStubCallMarkers = 0
  PhysicalRumbleCommandRecords = 6
  PhysicalDisconnectReconnectPassed = $true
  SavedGameplayPassed = $true
  TitleDrivenForceFeedbackClaimed = $false
}
