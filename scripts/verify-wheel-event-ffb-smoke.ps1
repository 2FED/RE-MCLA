[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$RuntimeLogPath,
  [Parameter(Mandatory)][string]$ObservationPath,
  [switch]$FixtureMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Resolve-RepoFile([string]$Path, [string]$Description) {
  $full = if ([IO.Path]::IsPathRooted($Path)) {
    [IO.Path]::GetFullPath($Path)
  } else {
    [IO.Path]::GetFullPath((Join-Path $repo $Path))
  }
  $prefix = $repo.TrimEnd('\') + '\'
  if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or
      -not (Test-Path -LiteralPath $full -PathType Leaf)) {
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

$logFile = Resolve-RepoFile $RuntimeLogPath 'Wheel event runtime log'
$observationFile = Resolve-RepoFile $ObservationPath 'Wheel event observation'
$log = [IO.File]::ReadAllText($logFile)
$observation = [IO.File]::ReadAllText($observationFile) | ConvertFrom-Json
if ($observation.schema -cne 'mcla-wheel-event-ffb-observation-v3' -or
    $observation.task -cne 'M6-015' -or
    $observation.wheel_model -cne 'Thrustmaster T300RS Racing Wheel' -or
    -not $observation.operator_confirmed) {
  throw 'Wheel event physical observation identity is invalid.'
}
$physicalFields = @('centering', 'curb_or_rough_surface', 'collision', 'focus_resume',
  'latest_active_wheel_gamepad_wheel')
foreach ($field in $physicalFields) {
  if ($observation.$field -isnot [bool]) {
    throw "Wheel event physical observation field '$field' is not Boolean."
  }
}
$physicalComplete = -not @($physicalFields | Where-Object { -not $observation.$_ }).Count

$device = [regex]::Match(
  $log,
  'SDL_WHEEL_FFB_DEVICE_INFO v=2 guest_slots=(?<guest>\d+) host_effects=(?<host>\d+) host_playing=(?<playing>\d+) features=0x(?<features>[0-9A-Fa-f]{8})'
)
if (-not $device.Success -or [int]$device.Groups['guest'].Value -ne 64 -or
    [int]$device.Groups['host'].Value -lt 64) {
  throw 'The physical wheel did not expose the required 64-slot force-feedback route.'
}
$features = [Convert]::ToUInt32($device.Groups['features'].Value, 16)
if (($features -band 0x1FF) -ne 0x1FF) {
  throw 'The physical wheel lacks one or more MCLA force-feedback effect families.'
}

$created = @([regex]::Matches($log, 'SDL_WHEEL_FFB_EFFECT v=2 action=create slot=\d+ type=(?<type>[a-z-]+)') |
  ForEach-Object { $_.Groups['type'].Value } | Sort-Object -Unique)
$started = @([regex]::Matches($log, 'SDL_WHEEL_FFB_EFFECT v=2 action=(?:start|resume) slot=\d+ type=(?<type>[a-z-]+)') |
  ForEach-Object { $_.Groups['type'].Value } | Sort-Object -Unique)
$updated = @([regex]::Matches($log, 'SDL_WHEEL_FFB_EFFECT v=2 action=first-update slot=\d+ type=(?<type>[a-z-]+)') |
  ForEach-Object { $_.Groups['type'].Value } | Sort-Object -Unique)
$parameterized = @([regex]::Matches($log, 'SDL_WHEEL_FFB_PARAMS v=1 action=(?:create|first-update) slot=\d+ type=(?<type>[a-z-]+)') |
  ForEach-Object { $_.Groups['type'].Value } | Sort-Object -Unique)
$nonSpringActivity = @($started + $updated | Where-Object { $_ -cne 'spring' } | Sort-Object -Unique)
if ($created -cnotcontains 'spring' -or
    $log -notmatch 'SDL_WHEEL_FFB_EFFECT v=2 action=start slot=\d+ type=spring' -or
    $nonSpringActivity.Count -lt 1) {
  throw 'Title telemetry did not prove any active event effect beyond spring centering.'
}
if (@($parameterized | Where-Object { $_ -cne 'spring' }).Count -lt 1) {
  throw 'No bounded parameter telemetry was captured for a non-spring title effect.'
}
$continuousConstant = [regex]::Match(
  $log,
  'SDL_WHEEL_FFB_PARAMS v=1 action=(?:create|first-update) slot=\d+ type=constant duration=4294967295 .*host_level=0 .*gain_percent=0'
)
if (-not $continuousConstant.Success) {
  throw 'The title directional constant was not safely neutralized while infinite.'
}
$finiteSquare = [regex]::Match(
  $log,
  'SDL_WHEEL_FFB_PARAMS v=1 action=(?:create|first-update) slot=(?<slot>\d+) type=square duration=(?!4294967295)\d+ .*host_magnitude=(?<magnitude>[1-9]\d*) .*gain_percent=100'
)
$finiteConstant = [regex]::Match(
  $log,
  'SDL_WHEEL_FFB_PARAMS v=1 action=(?:create|first-update) slot=(?<slot>\d+) type=constant duration=(?!4294967295)\d+ .*host_level=(?<level>-?[1-9]\d*) .*gain_percent=100'
)
$finiteEventTypes = @()
if ($finiteSquare.Success -and [int]$finiteSquare.Groups['magnitude'].Value -ge 24575 -and
    $log -match ('SDL_WHEEL_FFB_EFFECT v=2 action=start slot=' + $finiteSquare.Groups['slot'].Value + ' type=square')) {
  $finiteEventTypes += 'square'
}
if ($finiteConstant.Success -and
    [Math]::Abs([int]$finiteConstant.Groups['level'].Value) -ge 24575 -and
    $log -match ('SDL_WHEEL_FFB_EFFECT v=2 action=start slot=' + $finiteConstant.Groups['slot'].Value + ' type=constant')) {
  $finiteEventTypes += 'constant'
}
if ($finiteEventTypes.Count -lt 1) {
  throw 'No perceptible-floor finite title event edge was submitted.'
}

$curbWindowStarts = 0
$collisionWindowStarts = 0
if (-not $FixtureMode) {
  $format = 'yyyy-MM-dd HH:mm:ss.fff'
  $culture = [Globalization.CultureInfo]::InvariantCulture
  try {
    $curbStart = [DateTime]::ParseExact([string]$observation.curb_window_start, $format, $culture)
    $curbEnd = [DateTime]::ParseExact([string]$observation.curb_window_end, $format, $culture)
    $collisionStart = [DateTime]::ParseExact([string]$observation.collision_window_start, $format, $culture)
    $collisionEnd = [DateTime]::ParseExact([string]$observation.collision_window_end, $format, $culture)
  } catch {
    throw 'Physical action-window timestamps are malformed.'
  }
  if ($curbStart -ge $curbEnd -or $collisionStart -ge $collisionEnd -or $curbEnd -gt $collisionStart) {
    throw 'Physical action-window chronology is invalid.'
  }
  $finiteSlots = @{}
  foreach ($match in [regex]::Matches(
      $log,
      'SDL_WHEEL_FFB_PARAMS v=1 action=(?:create|first-update) slot=(?<slot>\d+) type=(?<type>square|constant) duration=(?<duration>\d+)')) {
    if ([uint64]$match.Groups['duration'].Value -ne 4294967295) {
      $finiteSlots[($match.Groups['slot'].Value + ':' + $match.Groups['type'].Value)] = $true
    }
  }
  $starts = @()
  foreach ($match in [regex]::Matches(
      $log,
      '(?m)^\[(?<time>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}[.]\d{3})\].*SDL_WHEEL_FFB_EFFECT v=2 action=start slot=(?<slot>\d+) type=(?<type>square|constant)')) {
    $key = $match.Groups['slot'].Value + ':' + $match.Groups['type'].Value
    if ($finiteSlots.ContainsKey($key)) {
      $starts += [DateTime]::ParseExact($match.Groups['time'].Value, $format, $culture)
    }
  }
  $curbWindowStarts = @($starts | Where-Object { $_ -ge $curbStart -and $_ -le $curbEnd }).Count
  $collisionWindowStarts = @($starts | Where-Object { $_ -ge $collisionStart -and $_ -le $collisionEnd }).Count
  if ($curbWindowStarts -lt 1) {
    throw 'No finite title event was submitted during the bounded curb action window.'
  }
  if ($collisionWindowStarts -lt 1) {
    throw 'No finite title event was submitted during the bounded collision action window.'
  }
}
$conditionParameters = @([regex]::Matches(
  $log,
  'SDL_WHEEL_FFB_PARAMS v=1 action=(?:create|first-update) slot=\d+ type=(?<type>spring|damper) .*guest_right_coeff=(?<guest_right>-?\d+) guest_left_coeff=(?<guest_left>-?\d+) .*host_right_coeff=(?<host_right>-?\d+) host_left_coeff=(?<host_left>-?\d+)'
))
foreach ($conditionType in @('spring', 'damper')) {
  $restoring = @($conditionParameters | Where-Object {
    $_.Groups['type'].Value -ceq $conditionType -and
    [int]$_.Groups['guest_right'].Value -gt 0 -and [int]$_.Groups['guest_left'].Value -gt 0 -and
    [int]$_.Groups['host_right'].Value -gt 0 -and [int]$_.Groups['host_left'].Value -gt 0
  })
  if ($restoring.Count -lt 1) {
    throw "Title telemetry did not prove sign-preserving SDL condition polarity for $conditionType."
  }
}
if ($log -notmatch 'SDL_WHEEL_FFB_EFFECT v=2 action=resume slot=\d+ type=[a-z-]+ reason=focus') {
  throw 'No post-focus force-feedback resume marker was observed.'
}
if ($log -notmatch 'SDL_WHEEL_FFB_EFFECT v=2 action=resume slot=\d+ type=[a-z-]+ reason=active-controller') {
  throw 'No force-feedback resume marker was observed after returning control to the wheel.'
}

$wheelConnection = [regex]::Match($log, 'SDL_WHEEL_CONNECTED v=1 physical_slot=(?<slot>\d+)')
if (-not $wheelConnection.Success) {
  throw 'No physical wheel connection marker was observed.'
}
$wheelSlot = [int]$wheelConnection.Groups['slot'].Value
$activeMatches = @([regex]::Matches(
  $log,
  'SDL_ACTIVE_CONTROLLER v=1 guest_slot=0 previous_physical_slot=(?<previous>-?\d+) physical_slot=(?<current>\d+) reason=(?<reason>[a-z-]+)'
))
$leftWheelAt = -1
$returnedToWheelAt = -1
for ($index = 0; $index -lt $activeMatches.Count; $index++) {
  $previous = [int]$activeMatches[$index].Groups['previous'].Value
  $current = [int]$activeMatches[$index].Groups['current'].Value
  if ($leftWheelAt -lt 0 -and $previous -eq $wheelSlot -and $current -ne $wheelSlot) {
    $leftWheelAt = $index
    continue
  }
  if ($leftWheelAt -ge 0 -and $index -gt $leftWheelAt -and
      $previous -ne $wheelSlot -and $current -eq $wheelSlot) {
    $returnedToWheelAt = $index
    break
  }
}
if ($leftWheelAt -lt 0 -or $returnedToWheelAt -lt 0) {
  throw 'Telemetry did not prove latest-active wheel-to-gamepad-to-wheel switching.'
}

$compatNoOps = @([regex]::Matches($log, 'XINPUTD_FF_AUDIT v=1 export=(?<name>\w+) action=compat-success-noop') |
  ForEach-Object { $_.Groups['name'].Value } | Sort-Object -Unique)
if ($compatNoOps.Count -ne 0) {
  throw "An unaudited compatibility no-op was reached: $($compatNoOps -join ', ')."
}
if ($log -match '(?i)(REX_GUEST_CRASH|GUEST_CRASH_REPORT|PPC_UNIMPLEMENTED|\[fatal\]|D3D12.*device (?:lost|removed))') {
  throw 'Wheel event runtime log contains a fatal marker.'
}
if ($log -match '(?i)SDL wheel FFB (?:create|update|start|stop).*failed') {
  throw 'Wheel event runtime log contains an SDL haptic submission failure.'
}
if (-not $FixtureMode -and $log -notmatch '(?m)Execution complete\s*$') {
  throw 'Wheel event title did not complete controlled external shutdown.'
}
if (-not $physicalComplete) {
  throw 'Wheel event physical observation is incomplete.'
}

[pscustomobject]@{
  Passed = $true
  Decision = 'title-wheel-events-focus-latest-active-pass'
  GuestEffectSlots = [int]$device.Groups['guest'].Value
  HostEffectSlots = [int]$device.Groups['host'].Value
  HostPlayingSlots = [int]$device.Groups['playing'].Value
  HapticFeatures = ('0x' + $device.Groups['features'].Value.ToUpperInvariant())
  CreatedEffectTypes = $created
  StartedEffectTypes = $started
  UpdatedEffectTypes = $updated
  ParameterizedEffectTypes = $parameterized
  ConditionPolarityVerified = $true
  ContinuousConstantNeutralized = $true
  FiniteEventTypesVerified = @($finiteEventTypes)
  CurbWindowFiniteStarts = $curbWindowStarts
  CollisionWindowFiniteStarts = $collisionWindowStarts
  NonSpringActiveEffectTypes = $nonSpringActivity
  ActiveControllerTransitions = $activeMatches.Count
  CompatibilityNoOpCalls = 0
  RuntimeLogSha256 = Get-Sha256 $logFile
  ObservationSha256 = Get-Sha256 $observationFile
}
