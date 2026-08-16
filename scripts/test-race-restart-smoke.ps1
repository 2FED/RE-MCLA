[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-race-restart-smoke.ps1'
$run = Join-Path $PSScriptRoot 'run-race-restart-smoke.ps1'
$root = Join-Path $repo ('private/evidence/M5-012/test-restart-' + [guid]::NewGuid().ToString('N'))
$utf8 = [Text.UTF8Encoding]::new($false)

function Write-Bmp([string]$Path, [switch]$Different) {
  $bytes = [byte[]]::new(3686454); $bytes[0] = 0x42; $bytes[1] = 0x4D
  [BitConverter]::GetBytes([int]1280).CopyTo($bytes, 18); [BitConverter]::GetBytes([int]720).CopyTo($bytes, 22); [BitConverter]::GetBytes([uint16]32).CopyTo($bytes, 28)
  if ($Different) { for ($offset = 54; $offset -lt $bytes.Length; $offset += 16) { $bytes[$offset] = 255 } }
  [IO.File]::WriteAllBytes($Path, $bytes)
}

function New-Fixture([string]$Path) {
  $user = Join-Path $Path 'user'; $cache = Join-Path $Path 'cache'
  [IO.Directory]::CreateDirectory($user) | Out-Null; [IO.Directory]::CreateDirectory($cache) | Out-Null
  Copy-Item -LiteralPath (Join-Path $repo 'private/evidence/M5-012/20260816-132209-a316f851/runs/01/user/B13EBABEBABEBABE') -Destination $user -Recurse
  Write-Bmp (Join-Path $user 'mcla-physics-start.bmp'); Write-Bmp (Join-Path $user 'mcla-physics-end.bmp') -Different
  $lines = @(
    'MCLA_PHYSICS_TIMING_CONFIG v=1 slot=0 gameplay_wait_seconds=45 dismiss_pulses=6 dismiss_interval_ms=5000 sample_seconds=10 guest_tick_frequency=50000000 expected_vblank_millihz=60000 expected_present_millihz=30000',
    'MCLA_PHYSICS_TIMING_FRAME v=1 phase=start width=1280 height=720 status=PASS'
  )
  for ($i = 0; $i -lt 16; $i++) { $lines += "MCLA_PHYSICS_TIMER_RECORD v=1 id=$i effective_bits=3D088889 clamped_bits=3D088889 raw_bits=3D088889" }
  for ($i = 0; $i -lt 8; $i++) { $lines += "MCLA_GAMEPLAY_INPUT v=1 fixture=$i" }
  $lines += @(
    'MCLA_PHYSICS_TIMING_FRAME v=1 phase=end width=1280 height=720 status=PASS',
    'MCLA_PHYSICS_TIMER_SUMMARY v=1 calls=300 records=16 invalid_values=0 effective_us_min=33333 effective_us_max=33333 clamped_us_min=33333 clamped_us_max=33333 raw_us_min=30000 raw_us_max=40000',
    'MCLA_PHYSICS_TIMING_SAMPLE v=1 host_us=10000000 guest_ticks=500000000 guest_host_ratio_ppm=1000000 vblank_delta=600 vblank_millihz=60000 present_delta=300 present_millihz=30000 present_to_vblank_ppm=500000 simulated_time_to_wall_ppm=1000000',
    'MCLA_PHYSICS_TIMING_SUMMARY v=1 status=COMPLETE samples=1 frames=2 gameplay_input_records=8 external_close_required=1',
    'Window closing, shutting down...', 'Execution complete', 'Title terminated; hard-exiting process.'
  )
  [IO.File]::WriteAllLines((Join-Path $Path 'mcla.log'), $lines, $utf8)
}

function Expect-Failure([scriptblock]$Action, [string]$Description) {
  try { & $Action | Out-Null; throw "Negative fixture '$Description' was accepted." } catch { if ($_.Exception.Message -ceq "Negative fixture '$Description' was accepted.") { throw } }
}

try {
  New-Fixture $root
  $positive = & $verify -RunPath $root -Fixture
  if ($positive.decision -cne 'completed-save-release-restart-pass' -or $positive.present_millihz -ne 30000 -or -not $positive.controlled_exit) { throw 'Positive restart fixture failed.' }
  $log = Join-Path $root 'mcla.log'; $original = [IO.File]::ReadAllText($log); $negative = 0
  foreach ($case in @(
    @{ Name = 'slow output'; Change = { param($x) $x.Replace('present_millihz=30000', 'present_millihz=20000') } },
    @{ Name = 'fatal'; Change = { param($x) $x + "`n[FATAL] fixture" } },
    @{ Name = 'missing hard exit'; Change = { param($x) $x.Replace('Title terminated; hard-exiting process.', '') } },
    @{ Name = 'bad fixed step'; Change = { param($x) $x.Replace('effective_bits=3D088889', 'effective_bits=00000000') } },
    @{ Name = 'duplicate summary'; Change = { param($x) $x + "`nMCLA_PHYSICS_TIMING_SUMMARY v=1 status=COMPLETE samples=1 frames=2 gameplay_input_records=8 external_close_required=1" } }
  )) {
    [IO.File]::WriteAllText($log, (& $case.Change $original), $utf8); Expect-Failure { & $verify -RunPath $root -Fixture } $case.Name; $negative++
  }
  [IO.File]::WriteAllText($log, $original, $utf8)
  $end = Join-Path $root 'user/mcla-physics-end.bmp'; $endBytes = [IO.File]::ReadAllBytes($end); [Array]::Clear($endBytes, 54, $endBytes.Length - 54); [IO.File]::WriteAllBytes($end, $endBytes)
  Expect-Failure { & $verify -RunPath $root -Fixture } 'static gameplay'; $negative++
  $runner = [IO.File]::ReadAllText($run); $validator = [IO.File]::ReadAllText($verify)
  $needles = @('win-amd64-release', 'mcla_physics_timing_probe=true', '20260816-132209-a316f851', '711B94303445034A3B127F83E8143FF6DBA3FC96C2B6176F8720E49829AD5021', 'present_millihz', '29400', '30600', 'first-series-results-return-and-release-restart-pass', 'five repeated race/resource checks remain M5-013', 'completed-save-release-restart-pass')
  foreach ($needle in $needles) { if (-not ($runner.Contains($needle) -or $validator.Contains($needle))) { throw "Source contract missing '$needle'." } }
  [pscustomobject][ordered]@{ PositiveFixtures = 1; FailClosedNegatives = $negative; SourceChecks = $needles.Count }
} finally {
  if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
