[CmdletBinding(DefaultParameterSetName = 'Probe')]
param(
  [Parameter(Mandatory, ParameterSetName = 'Probe')]
  [string]$RuntimeLogPath,

  [Parameter(Mandatory, ParameterSetName = 'Probe')]
  [string]$UserRoot,

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
$sdkCommit = '53c16fcfcbfee83752b7689cf74aba1d69a185fa'
$pauseReferenceRelative = 'private/baseline/M4-011/frontend-reference/pause.bmp'
$pauseReferenceHash = '61584464CB5D8B4C5296903CDBB4F5CD03B8A1639E751DCAB2D2BA5DA06F7D19'
$seedSaveHash = 'E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68'
$seedHeaderHash = '1DAEF55FA78BDD3F1AA75A36772B801C6C714B6D1A115A97904F3D1C957BC4C9'
$framePhases = @(
  'neutral-before',
  'throttle',
  'throttle-release',
  'brake',
  'brake-release',
  'steer-left',
  'steer-right',
  'pause'
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
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object { $_.Length -gt 0 })) {
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

function Get-TreeSnapshot {
  param([Parameter(Mandatory)][string]$Root)

  $rootPath = Resolve-SafePath -Path $Root -Description 'Evidence tree' -Directory
  $items = @(Get-ChildItem -LiteralPath $rootPath -Recurse -Force)
  foreach ($item in $items) {
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
      throw 'Evidence tree contains a reparse point.'
    }
  }

  $entries = @()
  $files = @($items | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)
  foreach ($directory in @($items | Where-Object { $_.PSIsContainer } | Sort-Object FullName)) {
    $entries += [ordered]@{
      kind = 'directory'
      path = $directory.FullName.Substring($rootPath.Length).TrimStart('\').Replace('\', '/')
    }
  }
  foreach ($file in $files) {
    $entries += [ordered]@{
      kind = 'file'
      path = $file.FullName.Substring($rootPath.Length).TrimStart('\').Replace('\', '/')
      bytes = $file.Length
      sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
  }

  $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4
  $sha = [Security.Cryptography.SHA256]::Create()
  try {
    $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') })
  } finally {
    $sha.Dispose()
  }
  $bytes = 0L
  foreach ($file in $files) {
    $bytes += $file.Length
  }
  return [pscustomobject]@{
    Hash = $hash
    FileCount = $files.Count
    DirectoryCount = @($items | Where-Object { $_.PSIsContainer }).Count
    Bytes = $bytes
  }
}

function Get-GameIdentity {
  param([Parameter(Mandatory)][string]$Root)

  # Refuse reparse traversal before the manifest verifier recursively reads files.
  $tree = Get-TreeSnapshot $Root
  $verified = & (Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath $Root -VerifyHashes
  return [pscustomobject][ordered]@{
    file_count = [int]$verified.FileCount
    payload_bytes = [int64]$verified.PayloadBytes
    manifest_sha256 = (Get-FileHash -LiteralPath $verified.ManifestPath -Algorithm SHA256).Hash
    tree_sha256 = $tree.Hash
    tree_file_count = $tree.FileCount
    tree_directory_count = $tree.DirectoryCount
    tree_bytes = $tree.Bytes
  }
}

function Get-ArtifactIdentity {
  param([Parameter(Mandatory)][string]$BuildRoot)

  return @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll') | ForEach-Object {
    $path = Resolve-SafePath -Path (Join-Path $BuildRoot $_) -Description "Runtime artifact $_"
    [pscustomobject][ordered]@{
      name = $_
      sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    }
  }
}

function Get-LogSet {
  param([Parameter(Mandatory)][string]$CurrentLogPath)

  $currentLog = Resolve-SafePath -Path $CurrentLogPath -Description 'Runtime log'
  if ((Split-Path $currentLog -Leaf) -cne 'mcla.log') {
    throw 'Current runtime log must be named mcla.log.'
  }
  $all = @(Get-ChildItem -LiteralPath (Split-Path $currentLog -Parent) -File -Filter 'mcla*.log')
  if ($all.Count -lt 1 -or $all.Count -gt 16) {
    throw 'Runtime-log topology is invalid.'
  }

  $rotated = @()
  $current = $null
  foreach ($file in $all) {
    if ($file.Name -ceq 'mcla.log') {
      $current = $file
      continue
    }
    $match = [regex]::Match($file.Name, '^mcla\.([1-9][0-9]*)\.log$')
    if (-not $match.Success) {
      throw 'Malformed runtime-log rotation.'
    }
    $rotated += [pscustomobject]@{ Index = [int]$match.Groups[1].Value; File = $file }
  }
  if ($null -eq $current) {
    throw 'Current runtime log is missing.'
  }
  $indices = @($rotated | ForEach-Object { $_.Index } | Sort-Object)
  for ($index = 0; $index -lt $indices.Count; $index++) {
    if ($indices[$index] -ne ($index + 1)) {
      throw 'Runtime-log rotations are not contiguous.'
    }
  }

  $ordered = @($rotated | Sort-Object Index -Descending | ForEach-Object { $_.File }) + $current
  $parts = @()
  $manifest = @()
  $bytes = 0L
  foreach ($file in $ordered) {
    $bytes += $file.Length
    if ($bytes -gt 134217728) {
      throw 'Runtime logs exceed 128 MiB.'
    }
    $parts += [IO.File]::ReadAllText($file.FullName)
    $manifest += [ordered]@{
      name = $file.Name
      bytes = $file.Length
      sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
  }

  $manifestJson = ConvertTo-Json -InputObject @($manifest) -Compress -Depth 3
  $sha = [Security.Cryptography.SHA256]::Create()
  try {
    $hash = -join ($sha.ComputeHash($utf8.GetBytes($manifestJson)) | ForEach-Object { $_.ToString('X2') })
  } finally {
    $sha.Dispose()
  }
  return [pscustomobject]@{
    Text = $parts -join "`n"
    Files = @($manifest)
    Count = $manifest.Count
    Bytes = $bytes
    Hash = $hash
  }
}

function Get-OnlyMatch {
  param(
    [Parameter(Mandatory)][string]$Text,
    [Parameter(Mandatory)][string]$Pattern,
    [Parameter(Mandatory)][string]$Description
  )

  $matches = [regex]::Matches($Text, $Pattern)
  if ($matches.Count -ne 1) {
    throw "$Description count is $($matches.Count), expected 1."
  }
  return [pscustomobject]@{
    Index = $matches[0].Index
    Length = $matches[0].Length
    Value = $matches[0].Value
  }
}

function Get-Bmp {
  param([Parameter(Mandatory)][string]$Path)

  $resolved = Resolve-SafePath -Path $Path -Description 'Gameplay BMP'
  $bytes = [IO.File]::ReadAllBytes($resolved)
  if (
    $bytes.Length -ne 3686454 -or
    $bytes[0] -ne 0x42 -or $bytes[1] -ne 0x4D -or
    [BitConverter]::ToInt32($bytes, 18) -ne 1280 -or
    [Math]::Abs([BitConverter]::ToInt32($bytes, 22)) -ne 720 -or
    [BitConverter]::ToUInt16($bytes, 28) -ne 32
  ) {
    throw 'Gameplay capture is not the canonical 1280x720 BMP.'
  }
  return [pscustomobject]@{
    Path = $resolved
    Bytes = $bytes
    Length = $bytes.Length
    Sha256 = (Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash
  }
}

function Get-SampledDifference {
  param($Left, $Right)

  $count = 0
  for ($offset = 54; $offset -lt $Left.Bytes.Length; $offset += 16) {
    $delta =
      [Math]::Abs([int]$Left.Bytes[$offset] - [int]$Right.Bytes[$offset]) +
      [Math]::Abs([int]$Left.Bytes[$offset + 1] - [int]$Right.Bytes[$offset + 1]) +
      [Math]::Abs([int]$Left.Bytes[$offset + 2] - [int]$Right.Bytes[$offset + 2])
    if ($delta -gt 36) {
      $count++
    }
  }
  return $count
}

function Get-EdgeValues {
  param(
    [Parameter(Mandatory)][string]$Path,
    [Drawing.Rectangle]$Roi = [Drawing.Rectangle]::new(210, 325, 330, 155)
  )

  Add-Type -AssemblyName System.Drawing
  $bitmap = [Drawing.Bitmap]::new($Path)
  try {
    $values = [double[]]::new(($Roi.Width - 2) * ($Roi.Height - 2))
    $index = 0
    for ($y = $Roi.Top + 1; $y -lt $Roi.Bottom - 1; $y++) {
      for ($x = $Roi.Left + 1; $x -lt $Roi.Right - 1; $x++) {
        $left = $bitmap.GetPixel($x - 1, $y)
        $right = $bitmap.GetPixel($x + 1, $y)
        $up = $bitmap.GetPixel($x, $y - 1)
        $down = $bitmap.GetPixel($x, $y + 1)
        $leftLuma = (54 * $left.R + 183 * $left.G + 19 * $left.B + 128) -shr 8
        $rightLuma = (54 * $right.R + 183 * $right.G + 19 * $right.B + 128) -shr 8
        $upLuma = (54 * $up.R + 183 * $up.G + 19 * $up.B + 128) -shr 8
        $downLuma = (54 * $down.R + 183 * $down.G + 19 * $down.B + 128) -shr 8
        $gx = $rightLuma - $leftLuma
        $gy = $downLuma - $upLuma
        $values[$index++] = [Math]::Sqrt($gx * $gx + $gy * $gy)
      }
    }
    return ,$values
  } finally {
    $bitmap.Dispose()
  }
}

function Get-CorrelationPpm {
  param(
    [Parameter(Mandatory)][string]$Candidate,
    [Parameter(Mandatory)][string]$Reference
  )

  $left = Get-EdgeValues -Path $Candidate
  $right = Get-EdgeValues -Path $Reference
  $leftMean = ($left | Measure-Object -Average).Average
  $rightMean = ($right | Measure-Object -Average).Average
  $numerator = 0.0
  $leftDenominator = 0.0
  $rightDenominator = 0.0
  for ($index = 0; $index -lt $left.Count; $index++) {
    $x = $left[$index] - $leftMean
    $y = $right[$index] - $rightMean
    $numerator += $x * $y
    $leftDenominator += $x * $x
    $rightDenominator += $y * $y
  }
  if ($leftDenominator -le 0 -or $rightDenominator -le 0) {
    throw 'Pause ROI has zero variance.'
  }
  return [int][Math]::Floor(
    1000000.0 * $numerator / [Math]::Sqrt($leftDenominator * $rightDenominator))
}

function Assert-SourceContract {
  $source = [IO.File]::ReadAllText((Resolve-SafePath -Path 'src/mcla_app.cpp' -Description 'MCLA app source'))
  foreach ($needle in @(
    'mcla_gameplay_input_probe, false',
    '.lifecycle(rex::cvar::Lifecycle::kInitOnly)',
    'MCLA_GAMEPLAY_INPUT_CONFIG v=1',
    'MCLA_GAMEPLAY_INPUT_SUMMARY v=1 status=PASS frames=8',
    'dismiss_interval_ms=5000',
    'steer_left.thumb_lx = -32768',
    'steer_right.thumb_lx = 32767',
    'throttle.right_trigger = 255',
    'brake.left_trigger = 255'
  )) {
    if (-not $source.Contains($needle)) {
      throw "Gameplay input source contract is missing '$needle'."
    }
  }

  $sdkRoot = Join-Path $repo 'third_party/rexglue-sdk'
  if ((& git -C $sdkRoot rev-parse HEAD).Trim() -cne $sdkCommit) {
    throw 'SDK commit changed.'
  }
  & git -C $sdkRoot diff --quiet v0.9.0.13 v0.9.0.19 -- include/rex/input src/input src/kernel/xam/xam_input.cpp tests/unit/input
  if ($LASTEXITCODE -ne 0) {
    throw 'M4-006 to current SDK input-source parity changed.'
  }
}

function Get-Probe {
  param(
    [Parameter(Mandatory)][string]$LogPath,
    [Parameter(Mandatory)][string]$UserPath,
    [switch]$IsFixture
  )

  $logSet = Get-LogSet -CurrentLogPath $LogPath
  $text = $logSet.Text
  $launch = Get-OnlyMatch -Text $text -Pattern '(?m)^.*KernelState: Preparing module launch\.\.\.\s*$' -Description 'Module launch'
  $config = Get-OnlyMatch -Text $text -Pattern '(?m)^.*MCLA_GAMEPLAY_INPUT_CONFIG v=1 slot=0 gameplay_wait_seconds=45 dismiss_pulses=6 dismiss_interval_ms=5000 button_hold_ms=250 control_hold_ms=3000 steer_hold_ms=2000 frames=8\s*$' -Description 'Gameplay input config'
  $summary = Get-OnlyMatch -Text $text -Pattern '(?m)^.*MCLA_GAMEPLAY_INPUT_SUMMARY v=1 status=PASS frames=8 gameplay_input_records=24 dismiss_input_records=24 physical_reconnect_evidence=external external_close_required=1\s*$' -Description 'Gameplay input summary'
  $close = Get-OnlyMatch -Text $text -Pattern '(?m)^.*Window closing, shutting down\.\.\.\s*$' -Description 'WM_CLOSE'
  $complete = Get-OnlyMatch -Text $text -Pattern '(?m)^.*Execution complete\s*$' -Description 'Execution complete'
  $hardExit = Get-OnlyMatch -Text $text -Pattern '(?m)^.*Title terminated; hard-exiting process\.\s*$' -Description 'Hard exit'

  $inputMatches = [regex]::Matches(
    $text,
    '(?m)^.*MCLA_GAMEPLAY_INPUT v=1 side=(?<side>source|guest) sequence=(?<sequence>[1-6]) buttons=(?<buttons>[0-9A-F]{4}) lt=(?<lt>[0-9]+) rt=(?<rt>[0-9]+) lx=(?<lx>-?[0-9]+) ly=(?<ly>-?[0-9]+) rx=(?<rx>-?[0-9]+) ry=(?<ry>-?[0-9]+)\s*$')
  $states = @(
    @{ Sequence = 1; Buttons = '0010'; Lt = 0; Rt = 0; Lx = 0 },
    @{ Sequence = 2; Buttons = '0000'; Lt = 0; Rt = 255; Lx = 0 },
    @{ Sequence = 3; Buttons = '0000'; Lt = 255; Rt = 0; Lx = 0 },
    @{ Sequence = 4; Buttons = '0000'; Lt = 0; Rt = 96; Lx = -32768 },
    @{ Sequence = 5; Buttons = '0000'; Lt = 0; Rt = 96; Lx = 32767 },
    @{ Sequence = 6; Buttons = '0010'; Lt = 0; Rt = 0; Lx = 0 }
  )
  $expectedInputs = @()
  foreach ($state in $states) {
    $expectedInputs += @(
      @{ Side = 'source'; State = $state },
      @{ Side = 'guest'; State = $state },
      @{ Side = 'source'; State = @{ Sequence = $state.Sequence; Buttons = '0000'; Lt = 0; Rt = 0; Lx = 0 } },
      @{ Side = 'guest'; State = @{ Sequence = $state.Sequence; Buttons = '0000'; Lt = 0; Rt = 0; Lx = 0 } }
    )
  }
  if ($inputMatches.Count -ne $expectedInputs.Count) {
    throw 'Gameplay input record count is not exactly 24.'
  }
  for ($index = 0; $index -lt $expectedInputs.Count; $index++) {
    $actual = $inputMatches[$index]
    $expected = $expectedInputs[$index]
    $state = $expected.State
    if (
      $actual.Groups['side'].Value -cne $expected.Side -or
      [int]$actual.Groups['sequence'].Value -ne $state.Sequence -or
      $actual.Groups['buttons'].Value -cne $state.Buttons -or
      [int]$actual.Groups['lt'].Value -ne $state.Lt -or
      [int]$actual.Groups['rt'].Value -ne $state.Rt -or
      [int]$actual.Groups['lx'].Value -ne $state.Lx -or
      [int]$actual.Groups['ly'].Value -ne 0 -or
      [int]$actual.Groups['rx'].Value -ne 0 -or
      [int]$actual.Groups['ry'].Value -ne 0
    ) {
      throw "Gameplay input chronology failed at record $index."
    }
  }

  $dismissMatches = [regex]::Matches(
    $text,
    '(?m)^.*MCLA_FRONTEND_SMOKE_INPUT v=1 side=(?<side>source|guest) sequence=(?<sequence>1[1-6]) buttons=(?<buttons>1000|0000)\s*$')
  if ($dismissMatches.Count -ne 24) {
    throw 'Overlay-dismiss input record count is not exactly 24.'
  }
  for ($dismiss = 0; $dismiss -lt 6; $dismiss++) {
    $offset = $dismiss * 4
    $sequence = 11 + $dismiss
    foreach ($entry in @(
      @('source', '1000'), @('guest', '1000'),
      @('source', '0000'), @('guest', '0000')
    )) {
      $actual = $dismissMatches[$offset]
      if (
        $actual.Groups['side'].Value -cne $entry[0] -or
        [int]$actual.Groups['sequence'].Value -ne $sequence -or
        $actual.Groups['buttons'].Value -cne $entry[1]
      ) {
        throw "Overlay-dismiss chronology failed at record $offset."
      }
      $offset++
    }
  }

  $frameMatches = [regex]::Matches(
    $text,
    '(?m)^.*MCLA_GAMEPLAY_INPUT_FRAME v=1 phase=(?<phase>[a-z-]+) width=1280 height=720 status=PASS\s*$')
  if (
    $frameMatches.Count -ne 8 -or
    (($frameMatches | ForEach-Object { $_.Groups['phase'].Value }) -join ',') -cne ($framePhases -join ',')
  ) {
    throw 'Gameplay frame chronology failed.'
  }

  if (
    $launch.Index -ge $config.Index -or
    $config.Index -ge $inputMatches[0].Index -or
    $inputMatches[3].Index -ge $dismissMatches[0].Index -or
    $dismissMatches[$dismissMatches.Count - 1].Index -ge $frameMatches[0].Index -or
    $frameMatches[$frameMatches.Count - 1].Index -ge $summary.Index -or
    $summary.Index -ge $close.Index -or
    $close.Index -ge $complete.Index -or
    $close.Index -ge $hardExit.Index
  ) {
    throw 'Gameplay route outer chronology failed.'
  }

  $sequenceFrameMap = @(
    @{ Sequence = 2; ActiveFrame = 1; ReleasedFrame = 2 },
    @{ Sequence = 3; ActiveFrame = 3; ReleasedFrame = 4 },
    @{ Sequence = 4; ActiveFrame = 5; ReleasedFrame = -1 },
    @{ Sequence = 5; ActiveFrame = 6; ReleasedFrame = -1 },
    @{ Sequence = 6; ActiveFrame = -1; ReleasedFrame = 7 }
  )
  foreach ($mapping in $sequenceFrameMap) {
    $base = ($mapping.Sequence - 1) * 4
    if ($mapping.ActiveFrame -ge 0) {
      $frame = $frameMatches[$mapping.ActiveFrame]
      if ($inputMatches[$base + 1].Index -ge $frame.Index -or $frame.Index -ge $inputMatches[$base + 2].Index) {
        throw "Active frame for sequence $($mapping.Sequence) is not causally bound."
      }
    }
    if ($mapping.ReleasedFrame -ge 0) {
      $frame = $frameMatches[$mapping.ReleasedFrame]
      if ($inputMatches[$base + 3].Index -ge $frame.Index) {
        throw "Released frame for sequence $($mapping.Sequence) is not causally bound."
      }
    }
  }

  if ($text -match '(?i)(REX_GUEST_CRASH|GUEST_CRASH_REPORT|PPC_UNIMPLEMENTED|\[fatal\]|assertion failed|D3D12.*device (?:lost|removed)|MCLA gameplay input: .*failed)') {
    throw 'Fatal, unsupported, device-loss, or gameplay-input failure marker found.'
  }

  $user = Resolve-SafePath -Path $UserPath -Description 'Gameplay user root' -Directory
  $bmps = [ordered]@{}
  foreach ($phase in $framePhases) {
    $bmps[$phase] = Get-Bmp -Path (Join-Path $user "mcla-gameplay-$phase.bmp")
  }
  if (@(Get-ChildItem -LiteralPath $user -File -Filter 'mcla-gameplay-*.bmp').Count -ne 8) {
    throw 'Gameplay capture topology changed.'
  }

  $neutralThrottle = Get-SampledDifference $bmps['neutral-before'] $bmps['throttle-release']
  $throttleBrake = Get-SampledDifference $bmps['throttle-release'] $bmps['brake-release']
  $steerLeftRight = Get-SampledDifference $bmps['steer-left'] $bmps['steer-right']
  $pauseReference = Resolve-SafePath -Path $pauseReferenceRelative -Description 'Pause reference'
  if ((Get-FileHash -LiteralPath $pauseReference -Algorithm SHA256).Hash -cne $pauseReferenceHash) {
    throw 'Pause reference identity changed.'
  }
  $pauseCorrelation = if ($IsFixture) {
    1000000
  } else {
    Get-CorrelationPpm -Candidate $bmps.pause.Path -Reference $pauseReference
  }
  if (-not $IsFixture) {
    if (
      @($bmps.Values | ForEach-Object { $_.Sha256 } | Select-Object -Unique).Count -ne 8 -or
      $neutralThrottle -lt 20000 -or
      $throttleBrake -lt 20000 -or
      $steerLeftRight -lt 20000 -or
      $pauseCorrelation -lt 500000
    ) {
      throw 'Gameplay response or pause image evidence is below floor.'
    }
  }

  return [pscustomobject]@{
    Passed = $true
    Decision = 'saved-gameplay-input-pass'
    LogSet = $logSet
    Bmps = $bmps
    GameplayInputRecords = $inputMatches.Count
    DismissInputRecords = $dismissMatches.Count
    NeutralThrottleDifference = $neutralThrottle
    ThrottleBrakeDifference = $throttleBrake
    SteerLeftRightDifference = $steerLeftRight
    PauseCorrelationPpm = $pauseCorrelation
  }
}

function Assert-ExactProperties {
  param($Object, [string[]]$Expected, [string]$Description)
  if (($Object.PSObject.Properties.Name -join ',') -cne ($Expected -join ',')) {
    throw "$Description schema changed."
  }
}

if ($PSCmdlet.ParameterSetName -eq 'Probe') {
  if (-not $ProbeOnly) {
    throw 'Probe mode requires -ProbeOnly.'
  }
  Assert-SourceContract
  Get-Probe -LogPath $RuntimeLogPath -UserPath $UserRoot -IsFixture:$FixtureMode
  return
}

Assert-SourceContract
$result = Resolve-SafePath -Path $ResultPath -Description 'M5-006 result'
$raw = [IO.File]::ReadAllText($result)
if ($raw -match '(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])') {
  throw 'M5-006 result contains a private or absolute path.'
}
$record = $raw | ConvertFrom-Json
Assert-ExactProperties $record @(
  'schema', 'task', 'decision', 'sdk_version', 'route_id',
  'controller_baseline', 'build', 'seed', 'game_identity', 'artifacts',
  'cycle', 'scope', 'no_surviving_processes', 'data_integrity_preserved'
) 'M5-006 result'
if (
  $record.schema -isnot [int] -or $record.schema -ne 1 -or
  $record.task -cne 'M5-006' -or
  $record.decision -cne 'saved-gameplay-input-pass' -or
  $record.sdk_version -cne '0.9.0.18' -or
  $record.route_id -cne 'pinned-save-gameplay-input-v1' -or
  $record.no_surviving_processes -isnot [bool] -or -not $record.no_surviving_processes -or
  $record.data_integrity_preserved -isnot [bool] -or -not $record.data_integrity_preserved
) {
  throw 'M5-006 result identity failed.'
}

$resultRoot = Split-Path $result -Parent
if (
  (Split-Path $result -Leaf) -cne 'result.json' -or
  (Split-Path (Split-Path $resultRoot -Parent) -Leaf) -cne 'M5-006' -or
  (Split-Path $resultRoot -Leaf) -notmatch '^[0-9]{8}-[0-9]{6}-[0-9a-f]{8}$' -or
  ((@(Get-ChildItem -LiteralPath $resultRoot -Force | Sort-Object Name | ForEach-Object { $_.Name })) -join ',') -cne 'relwithdebinfo-clean-build.log,result.json,runs'
) {
  throw 'M5-006 result topology failed.'
}

Assert-ExactProperties $record.controller_baseline @(
  'digital_run', 'analog_focus_run', 'hotplug_run', 'physical_digital_pass',
  'physical_analog_pass', 'physical_reconnect_pass', 'source_parity_v13_v18',
  'multi_pad_physically_claimed'
) 'Controller baseline'
if (
  $record.controller_baseline.digital_run -cne '20260812-212030-5fc01c73' -or
  $record.controller_baseline.analog_focus_run -cne '20260813-124600-293c07b3' -or
  $record.controller_baseline.hotplug_run -cne '20260813-144406-2c1974da' -or
  $record.controller_baseline.physical_digital_pass -isnot [bool] -or
  $record.controller_baseline.physical_analog_pass -isnot [bool] -or
  $record.controller_baseline.physical_reconnect_pass -isnot [bool] -or
  $record.controller_baseline.source_parity_v13_v18 -isnot [bool] -or
  $record.controller_baseline.multi_pad_physically_claimed -isnot [bool] -or
  -not $record.controller_baseline.physical_digital_pass -or
  -not $record.controller_baseline.physical_analog_pass -or
  -not $record.controller_baseline.physical_reconnect_pass -or
  -not $record.controller_baseline.source_parity_v13_v18 -or
  $record.controller_baseline.multi_pad_physically_claimed
) {
  throw 'Controller baseline binding failed.'
}

$controller = & (Join-Path $PSScriptRoot 'verify-controller-matrix.ps1') `
  -RecoveredHotplugEvidenceRun '20260813-144406-2c1974da' `
  -RecoveredHotplugEvidenceOnly
if (-not $controller.Passed -or -not $controller.HotplugMatrixPassed -or -not $controller.ControlledExitPassed) {
  throw 'Physical M4-006 controller baseline failed.'
}

$cycleRoot = Resolve-SafePath -Path (Join-Path $resultRoot 'runs/01') -Description 'M5-006 cycle root' -Directory
$probe = Get-Probe -LogPath (Join-Path $cycleRoot 'mcla.log') -UserPath (Join-Path $cycleRoot 'user')
Assert-ExactProperties $record.build @('log_sha256', 'executable_sha256') 'Build'
$buildLog = Resolve-SafePath -Path (Join-Path $resultRoot 'relwithdebinfo-clean-build.log') -Description 'Build log'
$executable = Resolve-SafePath -Path 'out/build/win-amd64-relwithdebinfo/mcla.exe' -Description 'Canonical executable'
if (
  $record.build.log_sha256 -cne (Get-FileHash -LiteralPath $buildLog -Algorithm SHA256).Hash -or
  $record.build.executable_sha256 -cne (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
) {
  throw 'Build evidence mismatch.'
}

Assert-ExactProperties $record.seed @('save_sha256', 'header_sha256', 'unchanged') 'Seed'
$seedSave = Resolve-SafePath -Path 'private/baseline/M4-011/post-oobe-profile/B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav' -Description 'Seed save'
$seedHeader = Resolve-SafePath -Path 'private/baseline/M4-011/post-oobe-profile/B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header' -Description 'Seed header'
if (
  $record.seed.save_sha256 -cne $seedSaveHash -or
  $record.seed.header_sha256 -cne $seedHeaderHash -or
  (Get-FileHash -LiteralPath $seedSave -Algorithm SHA256).Hash -cne $seedSaveHash -or
  (Get-FileHash -LiteralPath $seedHeader -Algorithm SHA256).Hash -cne $seedHeaderHash -or
  -not $record.seed.unchanged
) {
  throw 'Pinned seed identity failed.'
}

Assert-ExactProperties $record.game_identity @('before', 'after') 'Game identity'
$gameFields = @(
  'file_count', 'payload_bytes', 'manifest_sha256', 'tree_sha256',
  'tree_file_count', 'tree_directory_count', 'tree_bytes'
)
Assert-ExactProperties $record.game_identity.before $gameFields 'Game identity before'
Assert-ExactProperties $record.game_identity.after $gameFields 'Game identity after'
$canonicalGame = Resolve-SafePath -Path 'private/game' -Description 'Canonical source game' -Directory
$currentGame = Get-GameIdentity $canonicalGame
foreach ($snapshot in @($record.game_identity.before, $record.game_identity.after)) {
  if (
    $snapshot.file_count -isnot [int] -or
    $snapshot.payload_bytes -isnot [long] -or
    $snapshot.tree_file_count -isnot [int] -or
    $snapshot.tree_directory_count -isnot [int] -or
    $snapshot.tree_bytes -isnot [long]
  ) {
    throw 'Game identity numeric types changed.'
  }
  foreach ($field in $gameFields) {
    if ($snapshot.$field -cne $currentGame.$field) {
      throw 'Canonical source-game identity mismatch.'
    }
  }
}
if (
  $currentGame.tree_file_count -ne $currentGame.file_count -or
  $currentGame.tree_bytes -ne $currentGame.payload_bytes
) {
  throw 'Canonical game manifest and physical tree disagree.'
}

Assert-ExactProperties $record.artifacts @('before', 'after') 'Runtime artifacts'
$artifactBefore = @($record.artifacts.before)
$artifactAfter = @($record.artifacts.after)
$currentArtifacts = @(Get-ArtifactIdentity 'out/build/win-amd64-relwithdebinfo')
if ($artifactBefore.Count -ne 4 -or $artifactAfter.Count -ne 4 -or $currentArtifacts.Count -ne 4) {
  throw 'Runtime artifact inventory count changed.'
}
for ($index = 0; $index -lt 4; $index++) {
  Assert-ExactProperties $artifactBefore[$index] @('name', 'sha256') 'Runtime artifact before'
  Assert-ExactProperties $artifactAfter[$index] @('name', 'sha256') 'Runtime artifact after'
  foreach ($snapshot in @($artifactBefore[$index], $artifactAfter[$index])) {
    if (
      $snapshot.name -cne $currentArtifacts[$index].name -or
      $snapshot.sha256 -cne $currentArtifacts[$index].sha256
    ) {
      throw 'Canonical runtime artifact identity mismatch.'
    }
  }
}

Assert-ExactProperties $record.cycle @(
  'exit_code', 'close_requested', 'harness_force_cleanup', 'runtime_logs',
  'runtime_log_set_sha256', 'runtime_log_bytes', 'captures',
  'gameplay_input_records', 'dismiss_input_records',
  'neutral_throttle_difference', 'throttle_brake_difference',
  'steer_left_right_difference', 'pause_correlation_ppm',
  'user_tree_sha256', 'cache_tree_sha256', 'cycle_tree_sha256'
) 'Cycle'
if (
  $record.cycle.exit_code -isnot [int] -or $record.cycle.exit_code -ne 0 -or
  $record.cycle.close_requested -isnot [bool] -or
  $record.cycle.harness_force_cleanup -isnot [bool] -or
  -not $record.cycle.close_requested -or
  $record.cycle.harness_force_cleanup -or
  $record.cycle.runtime_log_set_sha256 -cne $probe.LogSet.Hash -or
  $record.cycle.runtime_log_bytes -ne $probe.LogSet.Bytes -or
  $record.cycle.gameplay_input_records -ne $probe.GameplayInputRecords -or
  $record.cycle.dismiss_input_records -ne $probe.DismissInputRecords -or
  $record.cycle.neutral_throttle_difference -ne $probe.NeutralThrottleDifference -or
  $record.cycle.throttle_brake_difference -ne $probe.ThrottleBrakeDifference -or
  $record.cycle.steer_left_right_difference -ne $probe.SteerLeftRightDifference -or
  $record.cycle.pause_correlation_ppm -ne $probe.PauseCorrelationPpm
) {
  throw 'Cycle aggregate mismatch.'
}

$runtimeLogs = @($record.cycle.runtime_logs)
if ($runtimeLogs.Count -ne $probe.LogSet.Count) {
  throw 'Runtime log manifest count changed.'
}
for ($index = 0; $index -lt $runtimeLogs.Count; $index++) {
  Assert-ExactProperties $runtimeLogs[$index] @('name', 'bytes', 'sha256') 'Runtime log'
  foreach ($field in @('name', 'bytes', 'sha256')) {
    if ($runtimeLogs[$index].$field -ne $probe.LogSet.Files[$index].$field) {
      throw 'Runtime log manifest mismatch.'
    }
  }
}

Assert-ExactProperties $record.cycle.captures $framePhases 'Capture manifest'
foreach ($phase in $framePhases) {
  Assert-ExactProperties $record.cycle.captures.$phase @('sha256', 'bytes') "Capture $phase"
  if (
    $record.cycle.captures.$phase.sha256 -cne $probe.Bmps[$phase].Sha256 -or
    $record.cycle.captures.$phase.bytes -ne $probe.Bmps[$phase].Length
  ) {
    throw "Capture '$phase' mismatch."
  }
}

$userTree = Get-TreeSnapshot (Join-Path $cycleRoot 'user')
$cacheTree = Get-TreeSnapshot (Join-Path $cycleRoot 'cache')
$cycleTree = Get-TreeSnapshot $cycleRoot
if (
  $record.cycle.user_tree_sha256 -cne $userTree.Hash -or
  $record.cycle.cache_tree_sha256 -cne $cacheTree.Hash -or
  $record.cycle.cycle_tree_sha256 -cne $cycleTree.Hash
) {
  throw 'Cycle tree identity mismatch.'
}

Assert-ExactProperties $record.scope @(
  'saved_free_roam_only', 'synthetic_gameplay_probe',
  'physical_sdl_source_bound_external', 'race_maneuver_parity_claimed',
  'multi_pad_claimed', 'force_feedback_claimed'
) 'Scope'
if (
  $record.scope.saved_free_roam_only -isnot [bool] -or
  $record.scope.synthetic_gameplay_probe -isnot [bool] -or
  $record.scope.physical_sdl_source_bound_external -isnot [bool] -or
  $record.scope.race_maneuver_parity_claimed -isnot [bool] -or
  $record.scope.multi_pad_claimed -isnot [bool] -or
  $record.scope.force_feedback_claimed -isnot [bool] -or
  -not $record.scope.saved_free_roam_only -or
  -not $record.scope.synthetic_gameplay_probe -or
  -not $record.scope.physical_sdl_source_bound_external -or
  $record.scope.race_maneuver_parity_claimed -or
  $record.scope.multi_pad_claimed -or
  $record.scope.force_feedback_claimed
) {
  throw 'M5-006 scope overclaims coverage.'
}

[pscustomobject]@{
  Passed = $true
  Decision = $record.decision
  GameplayInputRecords = $probe.GameplayInputRecords
  DismissInputRecords = $probe.DismissInputRecords
  PauseCorrelationPpm = $probe.PauseCorrelationPpm
  PhysicalReconnectPassed = $true
  DataIntegrityVerified = $true
}
