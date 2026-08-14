[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-force-feedback-report.ps1'
$root = Join-Path $repo ('private/evidence/M5-007/test-' + [guid]::NewGuid().ToString('N'))
$log = Join-Path $root 'mcla.log'
$utf8 = [Text.UTF8Encoding]::new($false)
$names = @(
  'XInputdFFGetDeviceInfo', 'XInputdFFSetEffect', 'XInputdFFUpdateEffect',
  'XInputdFFEffectOperation', 'XInputdFFDeviceControl',
  'XInputdFFSetDeviceGain', 'XInputdFFCancelIo', 'XInputdFFSetRumble'
)

function New-ValidLines {
  $lines = [Collections.Generic.List[string]]::new()
  for ($index = 0; $index -lt 8; $index++) {
    $ordinal = '028{0}' -f ($index + 2)
    $lines.Add("GetProcAddressByOrdinal: $($names[$index]) ($ordinal) in xboxkrnl -> thunk at 827CD0$($index + 5)C")
  }
  $lines.Add('MCLA_GAMEPLAY_INPUT_SUMMARY v=1 status=PASS frames=8 gameplay_input_records=24 dismiss_input_records=24 physical_reconnect_evidence=external external_close_required=1')
  $lines.Add('Execution complete')
  return ,$lines.ToArray()
}

function Invoke-Probe {
  param([string[]]$Lines)
  [IO.File]::WriteAllLines($log, $Lines, $utf8)
  return & $verifier -ProbeOnly -FixtureMode -RuntimeLogPath $log
}

function Assert-Rejected {
  param([string]$Name, [scriptblock]$Mutation)
  $lines = [Collections.Generic.List[string]]::new()
  $lines.AddRange([string[]](New-ValidLines))
  & $Mutation $lines
  try {
    $null = Invoke-Probe $lines.ToArray()
  } catch {
    $script:negatives++
    return
  }
  throw "Negative fixture '$Name' was accepted."
}

[IO.Directory]::CreateDirectory($root) | Out-Null
try {
  $positive = Invoke-Probe (New-ValidLines)
  if (-not $positive.GameplayPassed -or $positive.ModuleResolutionCount -ne 8 -or $positive.StubCallMarkers -ne 0) {
    throw 'Positive force-feedback probe fixture failed.'
  }

  $negatives = 0
  Assert-Rejected 'missing-import' { param($l) $l.RemoveAt(0) }
  Assert-Rejected 'duplicate-import' { param($l) $l.Insert(0, $l[0]) }
  Assert-Rejected 'wrong-name' { param($l) $l[0] = $l[0].Replace('XInputdFFGetDeviceInfo', 'XInputdFFSetEffect') }
  Assert-Rejected 'wrong-ordinal' { param($l) $l[0] = $l[0].Replace('(0282)', '(0283)') }
  Assert-Rejected 'reordered-imports' { param($l) $t=$l[0];$l[0]=$l[1];$l[1]=$t }
  Assert-Rejected 'stub-call' { param($l) $l.Insert(8, '__imp__XInputdFFSetRumble STUB') }
  Assert-Rejected 'missing-summary' { param($l) $l.RemoveAt(8) }
  Assert-Rejected 'duplicate-summary' { param($l) $l.Insert(8, $l[8]) }
  Assert-Rejected 'missing-exit' { param($l) $l.RemoveAt($l.Count - 1) }
  Assert-Rejected 'guest-crash' { param($l) $l.Insert(8, 'REX_GUEST_CRASH injected') }
  Assert-Rejected 'unimplemented' { param($l) $l.Insert(8, 'PPC_UNIMPLEMENTED injected') }
  Assert-Rejected 'device-loss' { param($l) $l.Insert(8, 'D3D12 device removed') }

  [IO.File]::WriteAllLines($log, (New-ValidLines), $utf8)
  [IO.File]::WriteAllText((Join-Path $root 'mcla.2.log'), 'gap', $utf8)
  try {
    $null = & $verifier -ProbeOnly -FixtureMode -RuntimeLogPath $log
    throw "Negative fixture 'rotation-gap' was accepted."
  } catch {
    if ($_.Exception.Message -eq "Negative fixture 'rotation-gap' was accepted.") { throw }
    $negatives++
  }
  Remove-Item -LiteralPath (Join-Path $root 'mcla.2.log')

  $verifierText = [IO.File]::ReadAllText($verifier)
  $sourceNeedles = @(
    "decision -cne 'ffb-withheld-host-rumble-bounded'",
    "xinputdff_stub_exports",
    "ffb_capability_advertised",
    "title_driven_force_feedback_claimed",
    "confirmation_recorded_in_run",
    "attestation_machine_verified",
    "XInputdFF stub inventory changed.",
    "SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN"
  )
  foreach ($needle in $sourceNeedles) {
    if (-not $verifierText.Contains($needle)) {
      throw "Verifier source contract is missing '$needle'."
    }
  }

  [pscustomobject]@{
    Passed = $true
    FixturePositives = 1
    FailClosedNegatives = $negatives
    SourceContractChecks = $sourceNeedles.Count
  }
} finally {
  if (Test-Path -LiteralPath $root) {
    Remove-Item -LiteralPath $root -Recurse -Force
  }
}
