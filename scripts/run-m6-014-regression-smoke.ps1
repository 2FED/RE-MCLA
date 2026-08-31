[CmdletBinding()]
param(
  [string]$RecoverSuiteRun,
  [ValidateSet('prompt','pass','fail')][string]$WheelObservation='prompt'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$suiteRun = if ($RecoverSuiteRun) { $RecoverSuiteRun } else {
  'regression-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' +
    [guid]::NewGuid().ToString('N').Substring(0, 8)
}
$suiteRoot = Join-Path $repo "private/evidence/M6-014/$suiteRun"
$scenarioRoot = Join-Path $suiteRoot 'scenarios/free-roam'
$album = Join-Path $suiteRoot 'scenarios/free-roam/run/user/B13EBABEBABEBABE/545407F8/00000001/PHOTO_ALBUM/PHOTO_ALBUM'
$resultPath = Join-Path $suiteRoot 'regression-result.json'
$utf8 = [Text.UTF8Encoding]::new($false)

Write-Host 'M6-014 REGRESSION: one 10-minute current-artifact calibration.' -ForegroundColor Cyan
Write-Host @'
After AUTO START reaches gameplay:
  1. Type SOAK FREE-ROAM READY in this console.
  2. Open Photo Mode, take one NEW photo, and confirm that its album preview is visible (not black).
  3. With the T300 and gamepad both connected, drive using the wheel for several minutes.
  4. Deliberately use the gamepad until it controls, then move the wheel until wheel control and centering return.
  5. Keep driving and check that centering does not randomly disappear while the wheel remains active.
At the final checkpoint type: FREEROAM REGRESSION
'@ -ForegroundColor Yellow

if ($RecoverSuiteRun) {
  if (-not (Test-Path -LiteralPath $scenarioRoot -PathType Container)) {
    throw "Regression recovery root is missing: '$scenarioRoot'."
  }
  $samples = @(Get-Content -LiteralPath (Join-Path $scenarioRoot 'resource-samples.json') -Raw | ConvertFrom-Json)
  $captures = @(Get-Content -LiteralPath (Join-Path $scenarioRoot 'captures.json') -Raw | ConvertFrom-Json)
  $activity = @(Get-Content -LiteralPath (Join-Path $scenarioRoot 'activity.json') -Raw | ConvertFrom-Json)
  $audit = Get-Content -LiteralPath (Join-Path $scenarioRoot 'host-display-audit.json') -Raw | ConvertFrom-Json
  $log = [IO.File]::ReadAllText((Join-Path $scenarioRoot 'run/mcla.log'))
  if ($samples.Count -ne 3 -or [int]$samples[-1].elapsed_seconds -ne 600) { throw 'Recovery requires baseline, 300-second, and 600-second resource samples.' }
  if ($captures.Count -ne 2) { throw 'Recovery requires baseline and final physical captures.' }
  if ($activity.Count -ne 1 -or [int]$activity[0].primary -ne 1) { throw 'Recovery requires the final regression activity checkpoint.' }
  if (-not [bool]$audit.audit_available -or @($audit.nvidia_driver_errors).Count -ne 0 -or @($audit.sunshine_application_errors).Count -ne 0) { throw 'Recovery host-display audit is incomplete or unhealthy.' }
  foreach ($marker in @('Window closing, shutting down...', 'Execution complete', 'Title terminated; hard-exiting process.')) {
    if (-not $log.Contains($marker)) { throw "Recovery runtime log is missing controlled-close marker: $marker" }
  }
  if ($log -match '(?i)\[FATAL\]|guest crash|PPC_UNIMPLEMENTED|invalid or unregistered function|device lost|DXGI_ERROR_DEVICE_REMOVED') { throw 'Recovery runtime log contains a fatal marker.' }
  Write-Host "M6-014 RECOVERY: accepting completed calibration journals from '$suiteRun'." -ForegroundColor Cyan
} else {
  & (Join-Path $PSScriptRoot 'run-soak-suite.ps1') -Scenario free-roam -SuiteRun $suiteRun -Calibration -DurationSeconds 600
}

if (-not (Test-Path -LiteralPath $album -PathType Leaf)) {
  throw "No Photo Album container was written. Private suite: '$suiteRoot'."
}
$photo = & (Join-Path $PSScriptRoot 'verify-photo-mode-readback.ps1') -AlbumPath $album
if ($photo.Decision -cne 'photo-mode-cpu-readback-nonblack-pass') {
  throw 'Photo Mode readback did not pass.'
}

$answer = if ($WheelObservation -ceq 'pass') { 'FFB STABLE YES' } elseif ($WheelObservation -ceq 'fail') { 'FFB STABLE NO' } else {
  do { $entered = Read-Host 'Type exactly FFB STABLE YES or FFB STABLE NO' } while ($entered -cne 'FFB STABLE YES' -and $entered -cne 'FFB STABLE NO')
  $entered
}
$wheelStable = $answer -ceq 'FFB STABLE YES'

$result = [ordered]@{
  schema = 'mcla-m6-014-photo-wheel-regression-v1'
  task = 'M6-014'
  decision = if ($wheelStable) { 'photo-mode-nonblack-and-wheel-ownership-regression-pass' } else { 'photo-mode-readback-pass-wheel-centering-regression-open' }
  suite_run = $suiteRun
  duration_seconds = 600
  photo_album_sha256 = $photo.AlbumSha256
  photo_jpeg_payloads = $photo.JpegPayloads
  photo_nonblack_payloads = $photo.NonblackPayloads
  wheel_centering_stable = $wheelStable
  wheel_gamepad_wheel_recovery = $wheelStable
  operator_confirmed = $true
}
[IO.File]::WriteAllText($resultPath, ($result | ConvertTo-Json -Depth 6) + [Environment]::NewLine, $utf8)
if ($wheelStable) {
  Write-Host "M6-014 REGRESSION PASS: '$resultPath'." -ForegroundColor Green
} else {
  Write-Host "M6-014 PARTIAL REGRESSION: Photo Mode passed; wheel centering remains open. '$resultPath'." -ForegroundColor Yellow
}
[pscustomobject]$result
