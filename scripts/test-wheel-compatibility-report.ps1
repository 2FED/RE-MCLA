[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-wheel-compatibility-report.ps1'
$root = Join-Path $repo ('private/evidence/M6-015/test-' + [guid]::NewGuid().ToString('N'))
$inputPath = Join-Path $root 'input-result.json'
$logPath = Join-Path $root 'mcla.log'
$utf8 = [Text.UTF8Encoding]::new($false)

function New-Input {
  [pscustomobject]@{
    schema = 'mcla-wheel-input-smoke-v1'; task = 'M6-015'; operator_confirmed = $true
    wheel = [pscustomobject]@{ model='Thrustmaster T300RS Racing Wheel'; vendor_id='044F'; product_id='B66E'; axes=4; buttons=13; hats=1; haptic_opened=$true }
    physical_results = [pscustomobject]@{ steering='pass'; accelerator_pedal='pass'; brake_pedal='pass'; digital_menu_matrix='pass'; hotplug_recovery='pass' }
    runtime = [pscustomobject]@{ wheel_connect_markers=2; wheel_remove_markers=1 }
    save = [pscustomobject]@{ source_save_preserved=$true; final_tree_sha256=('A' * 64) }
  }
}

function New-Lines {
  @(
    'SDL_WHEEL_FFB_DEVICE_CONTROL v=1 command=4',
    'SDL_WHEEL_FFB_EFFECT v=1 action=create slot=1 type=spring gain_percent=25',
    'SDL_WHEEL_FFB_EFFECT v=1 action=start slot=1',
    'SDL_WHEEL_FFB_EFFECT v=1 action=first-update slot=1 right_coeff=22962 right_saturation=16383 gain_percent=25',
    'SDL_WHEEL_FFB_EFFECT v=1 action=resume slot=1 reason=focus',
    'Execution complete'
  )
}

function Invoke-Probe($Record, [string[]]$Lines) {
  [IO.File]::WriteAllText($inputPath, ($Record | ConvertTo-Json -Depth 5) + [Environment]::NewLine, $utf8)
  [IO.File]::WriteAllLines($logPath, $Lines, $utf8)
  & $verifier -ProbeOnly -FixtureMode -InputResultPath $inputPath -FfbLogPath $logPath
}

function Assert-Rejected([string]$Name, [scriptblock]$Mutation) {
  $record = New-Input
  $lines = [Collections.Generic.List[string]]::new()
  $lines.AddRange([string[]](New-Lines))
  & $Mutation $record $lines
  try { $null = Invoke-Probe $record $lines.ToArray() } catch { $script:negatives++; return }
  throw "Negative fixture '$Name' was accepted."
}

[IO.Directory]::CreateDirectory($root) | Out-Null
try {
  $positive = Invoke-Probe (New-Input) (New-Lines)
  if ($positive.ResumeMarkers -ne 1 -or $positive.WheelConnectMarkers -ne 2 -or $positive.WheelRemoveMarkers -ne 1) {
    throw 'Positive M6-015 fixture failed.'
  }

  $negatives = 0
  Assert-Rejected 'wrong-model' { param($i,$l) $i.wheel.model='Other' }
  Assert-Rejected 'wrong-vendor' { param($i,$l) $i.wheel.vendor_id='0000' }
  Assert-Rejected 'wrong-axis-count' { param($i,$l) $i.wheel.axes=3 }
  Assert-Rejected 'steering-failed' { param($i,$l) $i.physical_results.steering='fail' }
  Assert-Rejected 'hotplug-failed' { param($i,$l) $i.physical_results.hotplug_recovery='fail' }
  Assert-Rejected 'save-not-preserved' { param($i,$l) $i.save.source_save_preserved=$false }
  Assert-Rejected 'missing-device-control' { param($i,$l) $l.RemoveAt(0) }
  Assert-Rejected 'missing-create' { param($i,$l) $l.RemoveAt(1) }
  Assert-Rejected 'missing-start' { param($i,$l) $l.RemoveAt(2) }
  Assert-Rejected 'missing-update' { param($i,$l) $l.RemoveAt(3) }
  Assert-Rejected 'missing-resume' { param($i,$l) $l.RemoveAt(4) }
  Assert-Rejected 'missing-complete' { param($i,$l) $l.RemoveAt(5) }
  Assert-Rejected 'fatal-marker' { param($i,$l) $l.Insert(5,'REX_GUEST_CRASH injected') }

  $runnerText = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'run-wheel-compatibility-report.ps1'))
  $verifierText = [IO.File]::ReadAllText($verifier)
  $referenceText = [IO.File]::ReadAllText((Join-Path $repo 'config/wheel-input-reference.json'))
  $sourceNeedles = @(
    @($runnerText, "device-probe-20260827-172658-08568c74"),
    @($runnerText, "model-agnostic-wheel-input-and-title-event-ffb-pass"),
    @($runnerText, "EventEvidenceRun"),
    @($runnerText, "guest_effect_slots = 64"),
    @($runnerText, "title_event_ffb_passed = `$true"),
    @($runnerText, "finite_event_effect_types = @(`$event.FiniteEventTypesVerified)"),
    @($runnerText, "curb_window_finite_starts = `$event.CurbWindowFiniteStarts"),
    @($runnerText, "collision_window_finite_starts = `$event.CollisionWindowFiniteStarts"),
    @($runnerText, "direct_spring_confirmed_gain_percent = 25"),
    @($runnerText, "title_spring_confirmed_gain_percent = 25"),
    @($runnerText, "title_event_ffb_confirmed_gain_percent = 100"),
    @($runnerText, "default_gain_percent = 100"),
    @($runnerText, "continuous_periodic_default_gain_percent = 40"),
    @($runnerText, "continuous_constant_default_gain_percent = 0"),
    @($runnerText, "minimum_transient_strength_percent = 75"),
    @($runnerText, "latest_active_switch_passed = `$true"),
    @($runnerText, "227 assertions in 9 test cases"),
    @($runnerText, "exact_force_fidelity_claimed = `$false"),
    @($verifierText, "An XInputdFF export regressed to a stub."),
    @($verifierText, "Raw wheel events are not isolated from duplicate SDL gamepad axis/button events."),
    @($verifierText, "other_wheel_models_physically_verified -ne 0"),
    @($verifierText, "Get-Sha256"),
    @($referenceText, '"selection_policy": "latest-meaningful-active-device"'),
    @($referenceText, '"guest_effect_slots": 64'),
    @($referenceText, '"implemented_effects"'),
    @($referenceText, '"title_event_physically_verified_effects": ["square"]'),
    @($referenceText, '"event_effects_physical_verification_pending": false'),
    @($referenceText, '"curb_effect_consistency": "intermittent-title-submission"'),
    @($referenceText, '"condition_translation": "sign-preserving-sdl-condition-coefficient"'),
    @($referenceText, '"maximum_gain_percent": 100')
  )
  foreach ($check in $sourceNeedles) { if (-not $check[0].Contains($check[1])) { throw "M6-015 source contract is missing '$($check[1])'." } }
  if ($runnerText.Contains('Get-FileHash') -or $verifierText.Contains('Get-FileHash')) { throw 'M6-015 scripts must not depend on Get-FileHash.' }

  [pscustomobject]@{ Passed=$true; FixturePositives=1; FailClosedNegatives=$negatives; SourceContractChecks=$sourceNeedles.Count + 1 }
} finally {
  if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
