Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$source = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'run-m6-014-regression-smoke.ps1'))
$needles = @(
  '-Calibration -DurationSeconds 600',
  'SOAK FREE-ROAM READY',
  'FREEROAM REGRESSION',
  'verify-photo-mode-readback.ps1',
  "photo-mode-cpu-readback-nonblack-pass",
  'FFB STABLE YES',
  'FFB STABLE NO',
  'wheel_gamepad_wheel_recovery = $wheelStable',
  'photo-mode-nonblack-and-wheel-ownership-regression-pass',
  'photo-mode-readback-pass-wheel-centering-regression-open',
  'RecoverSuiteRun',
  "WheelObservation='prompt'",
  'Recovery requires baseline, 300-second, and 600-second resource samples.',
  'Recovery host-display audit is incomplete or unhealthy.',
  'Window closing, shutting down...',
  'wheel_centering_stable = $wheelStable'
)
foreach ($needle in $needles) {
  if (-not $source.Contains($needle)) { throw "Regression source contract missing: $needle" }
}
if ($source.Contains('Stop-Process') -or $source.Contains('-Force')) {
  throw 'Regression wrapper must preserve the soak runner controlled-close policy.'
}
[pscustomobject][ordered]@{ SourceContractChecks = $needles.Count + 1; Passed = $true }
