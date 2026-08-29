[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$EventEvidenceRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdk = Join-Path $repo 'third_party/rexglue-sdk'
$evidenceRoot = Join-Path $repo 'private/evidence/M6-015'
$inputRun = '20260827-150416-d6e6c8e0'
$ffbRun = 'ffb-focus-retest-20260827-170345-1242fa05'
$deviceProbeRun = 'device-probe-20260827-172658-08568c74'
$inputResult = Join-Path $evidenceRoot "$inputRun/result.json"
$ffbLog = Join-Path $evidenceRoot "$ffbRun/mcla.log"
$deviceProbeLog = Join-Path $evidenceRoot "$deviceProbeRun/sdl-wheel-probe.log"
$eventResult = Join-Path $evidenceRoot "$EventEvidenceRun/result.json"
$eventLog = Join-Path $evidenceRoot "$EventEvidenceRun/mcla.log"
$eventObservation = Join-Path $evidenceRoot "$EventEvidenceRun/operator-observation.json"
$verifier = Join-Path $PSScriptRoot 'verify-wheel-compatibility-report.ps1'
$unitExe = Join-Path $sdk 'out/win-amd64/RelWithDebInfo/unit_tests.exe'
$utf8 = [Text.UTF8Encoding]::new($false)

function Get-Sha256([string]$Path) {
  $stream = [IO.File]::OpenRead($Path)
  try {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') } finally { $sha.Dispose() }
  } finally { $stream.Dispose() }
}

function Assert-SafeWriteRoot([string]$Path) {
  $full = [IO.Path]::GetFullPath($Path)
  $allowed = [IO.Path]::GetFullPath($evidenceRoot).TrimEnd('\')
  if (-not ($full -ceq $allowed -or $full.StartsWith($allowed + '\', [StringComparison]::OrdinalIgnoreCase))) {
    throw 'M6-015 write root escapes its private evidence directory.'
  }
  $prefix = $repo.TrimEnd('\') + '\'
  $current = $repo
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object { $_ })) {
    $current = Join-Path $current $part
    if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
      throw 'M6-015 write root traverses a reparse point.'
    }
  }
}

function Invoke-Captured([string]$Executable, [string[]]$Arguments, [string]$WorkingDirectory) {
  $start = [Diagnostics.ProcessStartInfo]::new()
  $start.FileName = $Executable
  $start.WorkingDirectory = $WorkingDirectory
  $start.UseShellExecute = $false
  $start.CreateNoWindow = $true
  $start.RedirectStandardOutput = $true
  $start.RedirectStandardError = $true
  foreach ($argument in $Arguments) { $null = $start.ArgumentList.Add($argument) }
  $process = [Diagnostics.Process]::new()
  $process.StartInfo = $start
  if (-not $process.Start()) { throw "Failed to start '$Executable'." }
  $stdout = $process.StandardOutput.ReadToEnd()
  $stderr = $process.StandardError.ReadToEnd()
  $process.WaitForExit()
  [pscustomobject]@{ ExitCode = $process.ExitCode; Text = ($stdout + $stderr) }
}

Write-Host 'M6-015 [1/4]: verifying immutable physical input, hotplug, title FFB, and focus-resume evidence...' -ForegroundColor Cyan
$physical = & $verifier -ProbeOnly -InputResultPath $inputResult -FfbLogPath $ffbLog
$event = & (Join-Path $PSScriptRoot 'verify-wheel-event-ffb-smoke.ps1') -RuntimeLogPath $eventLog -ObservationPath $eventObservation
$eventRecord = [IO.File]::ReadAllText($eventResult) | ConvertFrom-Json
if ($eventRecord.schema -cne 'mcla-wheel-event-ffb-smoke-v1' -or
    $eventRecord.decision -cne $event.Decision) { throw 'M6-015 event evidence result failed.' }

$sdkCommit = (& git -C $sdk rev-parse HEAD).Trim()
$sdkTag = (& git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim()
if ($LASTEXITCODE -ne 0 -or $sdkTag -notmatch '^v[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$') {
  throw 'M6-015 requires the reviewed SDK commit to have its exact release tag.'
}
& git -C $sdk diff --quiet
if ($LASTEXITCODE -ne 0) { throw 'M6-015 requires a clean SDK worktree.' }

foreach ($path in @($unitExe, $deviceProbeLog)) {
  if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required wheel test artifact is missing: $(Split-Path $path -Leaf)." }
}

Write-Host 'M6-015 [2/4]: running focused wheel tests and re-verifying the captured SDL device/capability probe...' -ForegroundColor Cyan
$unit = Invoke-Captured $unitExe @('[input][sdl][wheel]') $sdk
if ($unit.ExitCode -ne 0 -or $unit.Text -notmatch 'All tests passed \(227 assertions in 9 test cases\)') { throw 'Focused wheel tests failed.' }
$probeText = [IO.File]::ReadAllText($deviceProbeLog)
if ($probeText -notmatch 'SDL_WHEEL_PROBE_RESULT status=PASS' -or
    $probeText -notmatch 'vendor=044F product=B66E' -or
    $probeText -notmatch 'effects=128 playing=128' -or
    $probeText -notmatch 'names=constant,sine,square,triangle,sawtooth-up,sawtooth-down,ramp,spring,damper') {
  throw 'Captured read-only physical wheel capability probe failed.'
}

Write-Host 'M6-015 [3/4]: writing a privacy-safe compatibility result...' -ForegroundColor Cyan
Assert-SafeWriteRoot $evidenceRoot
[IO.Directory]::CreateDirectory($evidenceRoot) | Out-Null
$runRoot = Join-Path $evidenceRoot ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
Assert-SafeWriteRoot $runRoot
$machineRoot = Join-Path $runRoot 'machine'
[IO.Directory]::CreateDirectory($machineRoot) | Out-Null
$unitLog = Join-Path $machineRoot 'sdl-wheel-tests.log'
$probeLog = Join-Path $machineRoot 'sdl-wheel-probe.log'
[IO.File]::WriteAllText($unitLog, $unit.Text, $utf8)
[IO.File]::WriteAllText($probeLog, $probeText, $utf8)

$reference = Join-Path $repo 'config/wheel-input-reference.json'
$referenceRecord = [IO.File]::ReadAllText($reference) | ConvertFrom-Json
if ($referenceRecord.force_feedback.event_effects_physical_verification_pending) {
  throw 'Wheel reference still marks title event effects as physically pending.'
}
$record = [ordered]@{
  schema = 'mcla-wheel-compatibility-report-v1'
  task = 'M6-015'
  decision = 'model-agnostic-wheel-input-and-title-event-ffb-pass'
  sdk_version = $sdkTag.Substring(1)
  sdk_commit = $sdkCommit
  reference_sha256 = Get-Sha256 $reference
  physical_input_result_sha256 = $physical.InputResultSha256
  physical_ffb_log_sha256 = $physical.FfbLogSha256
  event_evidence_run = $EventEvidenceRun
  physical_event_result_sha256 = Get-Sha256 $eventResult
  physical_event_log_sha256 = $event.RuntimeLogSha256
  physical_event_observation_sha256 = $event.ObservationSha256
  unit_test_log_sha256 = Get-Sha256 $unitLog
  probe_log_sha256 = Get-Sha256 $probeLog
  wheel_model_physically_verified = 'Thrustmaster T300RS Racing Wheel'
  other_wheel_models_physically_verified = 0
  input_matrix_passed = $true
  hotplug_passed = $true
  title_spring_ffb_passed = $true
  title_event_ffb_passed = $true
  curb_effect_passed = $true
  collision_effect_passed = $true
  focus_resume_passed = $true
  latest_active_switch_passed = $true
  direct_spring_physically_confirmed = $true
  direct_spring_confirmed_gain_percent = 25
  title_spring_confirmed_gain_percent = 25
  title_event_ffb_confirmed_gain_percent = 100
  default_gain_percent = 100
  continuous_periodic_default_gain_percent = 40
  continuous_constant_default_gain_percent = 0
  minimum_transient_strength_percent = 75
  maximum_gain_percent = 100
  guest_effect_slots = 64
  title_active_event_effect_types = @($event.NonSpringActiveEffectTypes)
  finite_event_effect_types = @($event.FiniteEventTypesVerified)
  curb_window_finite_starts = $event.CurbWindowFiniteStarts
  collision_window_finite_starts = $event.CollisionWindowFiniteStarts
  active_controller_transitions = $event.ActiveControllerTransitions
  compatibility_noop_calls = 0
  wheel_connect_markers = $physical.WheelConnectMarkers
  wheel_remove_markers = $physical.WheelRemoveMarkers
  focus_resume_markers = $physical.ResumeMarkers
  preserved_save_tree_sha256 = $physical.SaveTreeSha256
  external_operator_confirmation = $true
  exact_force_fidelity_claimed = $false
  cross_model_physical_compatibility_claimed = $false
  unmapped_l2_r2_on_two_pedal_reference = $true
}
$resultPath = Join-Path $runRoot 'result.json'
[IO.File]::WriteAllText($resultPath, ($record | ConvertTo-Json -Depth 6) + [Environment]::NewLine, $utf8)

Write-Host 'M6-015 [4/4]: re-verifying source, physical evidence, machine logs, and bounded claims...' -ForegroundColor Cyan
$verified = & $verifier -ResultPath $resultPath
[pscustomobject]@{
  Passed = $verified.Passed
  Decision = $verified.Decision
  Wheel = $verified.Wheel
  DefaultGainPercent = $verified.DefaultGainPercent
  MaximumGainPercent = $verified.MaximumGainPercent
  FocusResumeMarkers = $verified.FocusResumeMarkers
  EventEffectTypes = $verified.EventEffectTypes
  OtherModelsPhysicallyVerified = $verified.OtherModelsPhysicallyVerified
  PrivateRunRoot = $runRoot
  ResultPath = $resultPath
}
