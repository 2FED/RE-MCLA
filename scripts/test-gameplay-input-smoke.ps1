[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-gameplay-input-smoke.ps1'
$fixtureRoot = Join-Path $repo ('private/evidence/M5-006/test-' + [guid]::NewGuid().ToString('N'))
$userRoot = Join-Path $fixtureRoot 'user'
$logPath = Join-Path $fixtureRoot 'mcla.log'
$pauseReference = Join-Path $repo 'private/baseline/M4-011/frontend-reference/pause.bmp'
$utf8 = [Text.UTF8Encoding]::new($false)
$phases = @(
  'neutral-before', 'throttle', 'throttle-release', 'brake',
  'brake-release', 'steer-left', 'steer-right', 'pause'
)

function Add-InputState {
  param(
    [Collections.Generic.List[string]]$Lines,
    [int]$Sequence,
    [string]$Buttons,
    [int]$Lt,
    [int]$Rt,
    [int]$Lx,
    [string]$ActiveFrame,
    [string]$ReleasedFrame
  )
  $Lines.Add("MCLA_GAMEPLAY_INPUT v=1 side=source sequence=$Sequence buttons=$Buttons lt=$Lt rt=$Rt lx=$Lx ly=0 rx=0 ry=0")
  $Lines.Add("MCLA_GAMEPLAY_INPUT v=1 side=guest sequence=$Sequence buttons=$Buttons lt=$Lt rt=$Rt lx=$Lx ly=0 rx=0 ry=0")
  if ($ActiveFrame) {
    $Lines.Add("MCLA_GAMEPLAY_INPUT_FRAME v=1 phase=$ActiveFrame width=1280 height=720 status=PASS")
  }
  $Lines.Add("MCLA_GAMEPLAY_INPUT v=1 side=source sequence=$Sequence buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=0")
  $Lines.Add("MCLA_GAMEPLAY_INPUT v=1 side=guest sequence=$Sequence buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=0")
  if ($ReleasedFrame) {
    $Lines.Add("MCLA_GAMEPLAY_INPUT_FRAME v=1 phase=$ReleasedFrame width=1280 height=720 status=PASS")
  }
}

function New-ValidLines {
  $lines = [Collections.Generic.List[string]]::new()
  $lines.Add('KernelState: Preparing module launch...')
  $lines.Add('MCLA_GAMEPLAY_INPUT_CONFIG v=1 slot=0 gameplay_wait_seconds=45 dismiss_pulses=6 dismiss_interval_ms=5000 button_hold_ms=250 control_hold_ms=3000 steer_hold_ms=2000 frames=8')
  Add-InputState $lines 1 '0010' 0 0 0 '' ''
  for ($sequence = 11; $sequence -le 16; $sequence++) {
    foreach ($entry in @(@('source', '1000'), @('guest', '1000'), @('source', '0000'), @('guest', '0000'))) {
      $lines.Add("MCLA_FRONTEND_SMOKE_INPUT v=1 side=$($entry[0]) sequence=$sequence buttons=$($entry[1])")
    }
  }
  $lines.Add('MCLA_GAMEPLAY_INPUT_FRAME v=1 phase=neutral-before width=1280 height=720 status=PASS')
  Add-InputState $lines 2 '0000' 0 255 0 'throttle' 'throttle-release'
  Add-InputState $lines 3 '0000' 255 0 0 'brake' 'brake-release'
  Add-InputState $lines 4 '0000' 0 96 -32768 'steer-left' ''
  Add-InputState $lines 5 '0000' 0 96 32767 'steer-right' ''
  Add-InputState $lines 6 '0010' 0 0 0 '' 'pause'
  $lines.Add('MCLA_GAMEPLAY_INPUT_SUMMARY v=1 status=PASS frames=8 gameplay_input_records=24 dismiss_input_records=24 physical_reconnect_evidence=external external_close_required=1')
  $lines.Add('Window closing, shutting down...')
  $lines.Add('Execution complete')
  $lines.Add('Title terminated; hard-exiting process.')
  return ,$lines.ToArray()
}

function Invoke-Fixture {
  param([string[]]$Lines)
  [IO.File]::WriteAllLines($logPath, $Lines, $utf8)
  return & $verifier -ProbeOnly -FixtureMode -RuntimeLogPath $logPath -UserRoot $userRoot
}

function Assert-Rejected {
  param([string]$Name, [scriptblock]$Mutation)
  $lines = [Collections.Generic.List[string]]::new()
  $lines.AddRange([string[]](New-ValidLines))
  & $Mutation $lines
  try {
    $null = Invoke-Fixture $lines.ToArray()
  } catch {
    $script:negativeCount++
    return
  }
  throw "Negative fixture '$Name' was accepted."
}

[IO.Directory]::CreateDirectory($userRoot) | Out-Null
try {
  foreach ($phase in $phases) {
    Copy-Item -LiteralPath $pauseReference -Destination (Join-Path $userRoot "mcla-gameplay-$phase.bmp")
  }

  $positive = Invoke-Fixture (New-ValidLines)
  if (-not $positive.Passed -or $positive.GameplayInputRecords -ne 24 -or $positive.DismissInputRecords -ne 24) {
    throw 'Positive gameplay-input fixture failed.'
  }

  $negativeCount = 0
  Assert-Rejected 'wrong-config' { param($l) $l[1] = $l[1].Replace('dismiss_interval_ms=5000', 'dismiss_interval_ms=500') }
  Assert-Rejected 'missing-launch' { param($l) $l.RemoveAt(0) }
  Assert-Rejected 'wrong-start' { param($l) $l[2] = $l[2].Replace('buttons=0010', 'buttons=1000') }
  Assert-Rejected 'wrong-throttle' { param($l) $i=$l.FindIndex([Predicate[string]]{param($x)$x -match 'sequence=2 .*rt=255'});$l[$i]=$l[$i].Replace('rt=255','rt=254') }
  Assert-Rejected 'wrong-brake' { param($l) $i=$l.FindIndex([Predicate[string]]{param($x)$x -match 'sequence=3 .*lt=255'});$l[$i]=$l[$i].Replace('lt=255','lt=254') }
  Assert-Rejected 'wrong-steer-left' { param($l) $i=$l.FindIndex([Predicate[string]]{param($x)$x -match 'sequence=4 .*lx=-32768'});$l[$i]=$l[$i].Replace('lx=-32768','lx=-32767') }
  Assert-Rejected 'wrong-steer-right' { param($l) $i=$l.FindIndex([Predicate[string]]{param($x)$x -match 'sequence=5 .*lx=32767'});$l[$i]=$l[$i].Replace('lx=32767','lx=32766') }
  Assert-Rejected 'wrong-side' { param($l) $i=$l.FindIndex([Predicate[string]]{param($x)$x -match 'side=guest sequence=2'});$l[$i]=$l[$i].Replace('side=guest','side=source') }
  Assert-Rejected 'missing-dismiss' { param($l) $i=$l.FindIndex([Predicate[string]]{param($x)$x -match 'sequence=11 buttons=1000'});$l.RemoveAt($i) }
  Assert-Rejected 'wrong-dismiss-sequence' { param($l) $i=$l.FindIndex([Predicate[string]]{param($x)$x -match 'sequence=16 buttons=1000'});$l[$i]=$l[$i].Replace('sequence=16','sequence=15') }
  Assert-Rejected 'missing-frame' { param($l) $i=$l.FindIndex([Predicate[string]]{param($x)$x -match 'phase=steer-right'});$l.RemoveAt($i) }
  Assert-Rejected 'reordered-frame' { param($l) $a=$l.FindIndex([Predicate[string]]{param($x)$x -match 'phase=steer-left'});$b=$l.FindIndex([Predicate[string]]{param($x)$x -match 'phase=steer-right'});$t=$l[$a];$l[$a]=$l[$b];$l[$b]=$t }
  Assert-Rejected 'duplicate-summary' { param($l) $i=$l.FindIndex([Predicate[string]]{param($x)$x -match 'MCLA_GAMEPLAY_INPUT_SUMMARY'});$l.Insert($i,$l[$i]) }
  Assert-Rejected 'summary-count-drift' { param($l) $i=$l.FindIndex([Predicate[string]]{param($x)$x -match 'MCLA_GAMEPLAY_INPUT_SUMMARY'});$l[$i]=$l[$i].Replace('gameplay_input_records=24','gameplay_input_records=23') }
  Assert-Rejected 'missing-close' { param($l) $i=$l.IndexOf('Window closing, shutting down...');$l.RemoveAt($i) }
  Assert-Rejected 'missing-complete' { param($l) $i=$l.IndexOf('Execution complete');$l.RemoveAt($i) }
  Assert-Rejected 'missing-hard-exit' { param($l) $i=$l.IndexOf('Title terminated; hard-exiting process.');$l.RemoveAt($i) }
  Assert-Rejected 'fatal-marker' { param($l) $l.Insert(2,'[fatal] injected') }
  Assert-Rejected 'guest-crash' { param($l) $l.Insert(2,'REX_GUEST_CRASH injected') }
  Assert-Rejected 'input-failure' { param($l) $l.Insert(2,'MCLA gameplay input: injected failed') }
  Assert-Rejected 'extra-capture' { param($l) Copy-Item -LiteralPath $pauseReference -Destination (Join-Path $userRoot 'mcla-gameplay-extra.bmp') -Force }
  Remove-Item -LiteralPath (Join-Path $userRoot 'mcla-gameplay-extra.bmp') -ErrorAction SilentlyContinue

  [pscustomobject]@{
    Passed = $true
    FixturePositives = 1
    FailClosedNegatives = $negativeCount
    SourceContractVerified = $true
  }
} finally {
  if (Test-Path -LiteralPath $fixtureRoot) {
    Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
  }
}
