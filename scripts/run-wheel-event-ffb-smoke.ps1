[CmdletBinding()]
param(
  [string]$BuildRoot = 'out/build/win-amd64-release',
  [string]$GameRoot = 'private/game',
  [string]$InitialUserRoot = 'private/evidence/M6-014/20260826-100036-1b80ac82/scenarios/free-roam/run/user'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdk = Join-Path $repo 'third_party/rexglue-sdk'
$cmake = (& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath).CMakePath
$verifier = Join-Path $PSScriptRoot 'verify-wheel-event-ffb-smoke.ps1'
$evidenceRoot = Join-Path $repo 'private/evidence/M6-015'
$utf8 = [Text.UTF8Encoding]::new($false)
$saveRelative = 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
$headerRelative = 'B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'
$expectedSaveSha = '6734627B56ECD9B35E7B6FC362D374804480BEC2E11DA272BBCBC223719426B7'
$expectedHeaderSha = '3B0EE0632B04FC622A9EB4EA886EDB2D96B33131F9C0C77D10F07B71B7C722F4'

if (-not ('MclaWheelEventNative' -as [type])) {
  Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;
public static class MclaWheelEventNative {
  delegate bool E(IntPtr h,IntPtr p);
  [DllImport("user32.dll")]static extern bool EnumWindows(E c,IntPtr p);
  [DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
  [DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetWindowText(IntPtr h,StringBuilder s,int n);
  [DllImport("user32.dll")]static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
  public static IntPtr[] Handles(int pid){var a=new List<IntPtr>();EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}
  public static string Title(IntPtr h){var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty;}
  public static bool Close(IntPtr h){return PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);}
}
'@
}

function Resolve-Safe([string]$Path, [string]$Description, [switch]$Exists) {
  $full = if ([IO.Path]::IsPathRooted($Path)) { [IO.Path]::GetFullPath($Path) } else { [IO.Path]::GetFullPath((Join-Path $repo $Path)) }
  $prefix = $repo.TrimEnd('\') + '\'
  if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes the repository." }
  $current = $repo
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object { $_ })) {
    $current = Join-Path $current $part
    if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "$Description traverses a reparse point." }
  }
  if ($Exists -and -not (Test-Path -LiteralPath $full)) { throw "$Description is missing." }
  $full
}

function Get-Sha256([string]$Path) {
  $stream = [IO.File]::OpenRead($Path)
  try {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') } finally { $sha.Dispose() }
  } finally { $stream.Dispose() }
}

function Invoke-Logged([scriptblock]$Command, [string]$Log, [switch]$Append) {
  $prior = $ErrorActionPreference
  $ErrorActionPreference = 'Continue'
  try {
    if ($Append) { & $Command *>&1 | Tee-Object -FilePath $Log -Append | Out-Null }
    else { & $Command *>&1 | Tee-Object -FilePath $Log | Out-Null }
    $LASTEXITCODE
  } finally { $ErrorActionPreference = $prior }
}

function Read-LiveLog([string]$Path) {
  if (-not (Test-Path -LiteralPath $Path)) { return '' }
  try {
    $stream = [IO.File]::Open($Path, 'Open', 'Read', 'ReadWrite')
    try { $reader = [IO.StreamReader]::new($stream); try { $reader.ReadToEnd() } finally { $reader.Dispose() } }
    finally { $stream.Dispose() }
  } catch { '' }
}

function Wait-LogMarker([Diagnostics.Process]$Process, [string]$Log, [string]$Marker, [int]$Seconds) {
  $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
  while ([DateTime]::UtcNow -lt $deadline) {
    if ($Process.HasExited) { throw "Game exited before marker '$Marker'." }
    if ((Read-LiveLog $Log).Contains($Marker)) { return }
    Start-Sleep -Milliseconds 250
  }
  throw "Timed out waiting for marker '$Marker'."
}

function Read-YesNo([Diagnostics.Process]$Process, [string]$Question) {
  while ($true) {
    if ($Process.HasExited) { throw 'Game exited while waiting for physical FFB confirmation.' }
    Write-Host $Question -ForegroundColor Yellow
    $answer = Read-Host 'Type exactly YES or NO'
    if ($answer -ceq 'YES') { return $true }
    if ($answer -ceq 'NO') { return $false }
    Write-Host 'Ignored; the game remains running.' -ForegroundColor DarkYellow
  }
}

function Read-ExactToken([Diagnostics.Process]$Process, [string]$Token, [string]$Instruction) {
  while ($true) {
    if ($Process.HasExited) { throw 'Game exited while waiting for the bounded action marker.' }
    Write-Host $Instruction -ForegroundColor Yellow
    $answer = Read-Host "Type exactly $Token"
    if ($answer -ceq $Token) { return }
    Write-Host 'Ignored; the game remains running.' -ForegroundColor DarkYellow
  }
}

function Write-ObservationSnapshot([object]$Observation, [string]$Path) {
  [IO.File]::WriteAllText(
    $Path,
    ($Observation | ConvertTo-Json -Depth 4) + [Environment]::NewLine,
    $utf8
  )
}

function Close-ExactWindow([Diagnostics.Process]$Process) {
  $matches = @()
  foreach ($handle in [MclaWheelEventNative]::Handles($Process.Id)) {
    if ([regex]::IsMatch([MclaWheelEventNative]::Title($handle), '^mcla \[rexglue-v[^\]]+\]$')) { $matches += $handle }
  }
  if ($matches.Count -ne 1 -or -not [MclaWheelEventNative]::Close($matches[0])) { throw 'Exact PID/window WM_CLOSE failed.' }
}

$game = Resolve-Safe $GameRoot 'Game root' -Exists
$build = Resolve-Safe $BuildRoot 'Build root'
$initial = Resolve-Safe $InitialUserRoot 'Progressed user root' -Exists
$seedSave = Resolve-Safe (Join-Path $initial $saveRelative) 'Progressed save' -Exists
$seedHeader = Resolve-Safe (Join-Path $initial $headerRelative) 'Progressed save header' -Exists
if ((Get-Sha256 $seedSave) -cne $expectedSaveSha -or (Get-Sha256 $seedHeader) -cne $expectedHeaderSha) {
  throw 'The archived two-hour progressed save identity drifted.'
}

[IO.Directory]::CreateDirectory($evidenceRoot) | Out-Null
$runRoot = Join-Path $evidenceRoot ('event-ffb-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
[IO.Directory]::CreateDirectory($runRoot) | Out-Null
$sdkLog = Join-Path $runRoot 'sdk-build.log'
$appLog = Join-Path $runRoot 'app-build.log'
$unitLog = Join-Path $runRoot 'wheel-unit-tests.log'
$observationPath = Join-Path $runRoot 'operator-observation.json'
$observation = [ordered]@{
  schema = 'mcla-wheel-event-ffb-observation-v3'; task = 'M6-015'
  wheel_model = 'Thrustmaster T300RS Racing Wheel'
  centering = $null; curb_or_rough_surface = $null; collision = $null
  focus_resume = $null; latest_active_wheel_gamepad_wheel = $null
  curb_window_start = $null; curb_window_end = $null
  collision_window_start = $null; collision_window_end = $null
  operator_confirmed = $false; capture_status = 'initialized'
}
Write-ObservationSnapshot $observation $observationPath

Write-Host 'M6-015 EVENT [1/5]: clean-building/testing SDK RelWithDebInfo and installing matching Release runtime...' -ForegroundColor Cyan
Push-Location $sdk
try {
  if ((Invoke-Logged { & $cmake --preset win-amd64 -DREXGLUE_BUILD_TESTS=ON } $sdkLog) -ne 0 -or
      (Invoke-Logged { & $cmake --build out/build/win-amd64 --config RelWithDebInfo --target install unit_tests sdl_wheel_probe --clean-first --parallel 8 } $sdkLog -Append) -ne 0) {
    throw "SDK RelWithDebInfo build/install failed. Private run: '$runRoot'."
  }
  $unitExe = Join-Path $sdk 'out/win-amd64/RelWithDebInfo/unit_tests.exe'
  if ((Invoke-Logged { & $unitExe '[input][sdl][wheel]' } $unitLog) -ne 0 -or
      [IO.File]::ReadAllText($unitLog) -notmatch 'All tests passed \(227 assertions in 9 test cases\)') {
    throw "Focused wheel tests failed. Private run: '$runRoot'."
  }
  if ((Invoke-Logged { & $cmake --build out/build/win-amd64 --config Release --target install --parallel 8 } $sdkLog -Append) -ne 0) {
    throw "SDK Release build/install failed. Private run: '$runRoot'."
  }
} finally { Pop-Location }

Write-Host 'M6-015 EVENT [2/5]: clean-building the Release title...' -ForegroundColor Cyan
Push-Location $repo
try {
  if ((Invoke-Logged { & $cmake --preset win-amd64-release } $appLog) -ne 0 -or
      (Invoke-Logged { & $cmake --build --preset win-amd64-release --target mcla --clean-first --parallel 8 } $appLog -Append) -ne 0) {
    throw "Release build failed. Private run: '$runRoot'."
  }
} finally { Pop-Location }

$exe = Resolve-Safe (Join-Path $build 'mcla.exe') 'Release executable' -Exists
if (@(Get-Process mcla -ErrorAction SilentlyContinue | Where-Object { try { $_.Path -ceq $exe } catch { $false } }).Count) {
  throw 'Canonical Release MCLA is already running.'
}
$user = Join-Path $runRoot 'user'
$cache = Join-Path $runRoot 'cache'
[IO.Directory]::CreateDirectory($user) | Out-Null
[IO.Directory]::CreateDirectory($cache) | Out-Null
Copy-Item -LiteralPath (Join-Path $initial 'B13EBABEBABEBABE') -Destination $user -Recurse -Force
if (Test-Path -LiteralPath (Join-Path $initial 'achievements')) {
  Copy-Item -LiteralPath (Join-Path $initial 'achievements') -Destination $user -Recurse -Force
}
$runtimeLog = Join-Path $runRoot 'mcla.log'
$arguments = @(
  '--xam_user_signin_state=2', '--input_backend=sdl', '--mnk_mode=false',
  '--wheel_force_feedback=true', '--wheel_force_feedback_gain=100',
  '--wheel_force_feedback_continuous_periodic_gain=40',
  '--wheel_force_feedback_continuous_constant_gain=0',
  '--wheel_force_feedback_minimum_transient_strength=75',
  '--async_shader_compilation=false', '--d3d12_pipeline_creation_threads=0',
  '--log_level=info', '--log_max_file_size_mb=16', '--log_max_files=30', '--fullscreen=false',
  "--game_data_root=`"$game`"", "--user_data_root=`"$user`"",
  "--cache_root=`"$cache`"", "--log_file=`"$runtimeLog`""
)

Write-Host 'M6-015 EVENT [3/5]: launching the progressed save with the 64-slot title FFB route...' -ForegroundColor Cyan
$process = $null
$forced = $false
try {
  $process = Start-Process $exe -ArgumentList $arguments -WorkingDirectory $build -PassThru
  Wait-LogMarker $process $runtimeLog 'SDL_WHEEL_FFB_DEVICE_INFO v=2 guest_slots=64' 120
  Write-Host 'TITLE/WHEEL READY. Press OPTIONS to load the saved game.' -ForegroundColor Green
  Read-ExactToken $process 'GAMEPLAY READY' 'When saved gameplay is fully controllable, Alt-Tab here.'
  $observation['capture_status'] = 'gameplay-ready'
  Write-ObservationSnapshot $observation $observationPath
  $curbStart = Get-Date
  $observation['curb_window_start'] = $curbStart.ToString('yyyy-MM-dd HH:mm:ss.fff')
  $observation['capture_status'] = 'curb-in-progress'
  Write-ObservationSnapshot $observation $observationPath
  Read-ExactToken $process 'CURB DONE' @'
Return to the game. Steer at speed and release the wheel, then deliberately cross a clear curb or rough road edge at least three times. Alt-Tab here only after all three attempts.
'@
  $curbEnd = Get-Date
  $observation['curb_window_end'] = $curbEnd.ToString('yyyy-MM-dd HH:mm:ss.fff')
  $observation['capture_status'] = 'curb-actions-complete'
  Write-ObservationSnapshot $observation $observationPath
  $centering = Read-YesNo $process 'Did the moving car produce speed-dependent centering force?'
  $observation['centering'] = $centering
  $observation['capture_status'] = 'centering-recorded'
  Write-ObservationSnapshot $observation $observationPath
  $curb = Read-YesNo $process 'Did a curb/rough edge produce a distinct transient or texture force?'
  $observation['curb_or_rough_surface'] = $curb
  $observation['capture_status'] = 'curb-recorded'
  Write-ObservationSnapshot $observation $observationPath
  $collisionStart = Get-Date
  $observation['collision_window_start'] = $collisionStart.ToString('yyyy-MM-dd HH:mm:ss.fff')
  $observation['capture_status'] = 'collision-in-progress'
  Write-ObservationSnapshot $observation $observationPath
  Read-ExactToken $process 'COLLISION DONE' @'
Return to the game. Make at least three controlled contacts with a wall or traffic vehicle. Alt-Tab here only after all three attempts.
'@
  $collisionEnd = Get-Date
  $observation['collision_window_end'] = $collisionEnd.ToString('yyyy-MM-dd HH:mm:ss.fff')
  $observation['capture_status'] = 'collision-actions-complete'
  Write-ObservationSnapshot $observation $observationPath
  $collision = Read-YesNo $process 'Did controlled contact produce a distinct impact force?'
  $observation['collision'] = $collision
  $observation['capture_status'] = 'collision-recorded'
  Write-ObservationSnapshot $observation $observationPath
  $focus = Read-YesNo $process 'Across the bounded Alt-Tab action windows, did force feedback return after every return to the game?'
  $observation['focus_resume'] = $focus
  $observation['capture_status'] = 'focus-recorded'
  Write-ObservationSnapshot $observation $observationPath
  $latestActive = Read-YesNo $process 'Return to the game: use a connected gamepad until it controls, then use the wheel until wheel control/force returns. Alt-Tab here and answer whether wheel -> gamepad -> wheel switching worked.'
  $observation['latest_active_wheel_gamepad_wheel'] = $latestActive
  $observation['operator_confirmed'] = $true
  $observation['capture_status'] = 'complete'
  Write-ObservationSnapshot $observation $observationPath
  Write-Host 'Closing the console-style title externally with WM_CLOSE...' -ForegroundColor Cyan
  Close-ExactWindow $process
  if (-not $process.WaitForExit(15000) -or $process.ExitCode -ne 0) { throw 'Controlled external close failed.' }

  Write-Host 'M6-015 EVENT [4/5]: correlating physical observations with title-created effect types...' -ForegroundColor Cyan
  $verified = & $verifier -RuntimeLogPath $runtimeLog -ObservationPath $observationPath
  $result = [ordered]@{
    schema = 'mcla-wheel-event-ffb-smoke-v1'; task = 'M6-015'; decision = $verified.Decision
    wheel_model = 'Thrustmaster T300RS Racing Wheel'; gain_percent = 100
    continuous_periodic_gain_percent = 40
    continuous_constant_gain_percent = 0
    minimum_transient_strength_percent = 75
    guest_effect_slots = $verified.GuestEffectSlots; host_effect_slots = $verified.HostEffectSlots
    haptic_features = $verified.HapticFeatures
    created_effect_types = @($verified.CreatedEffectTypes)
    started_effect_types = @($verified.StartedEffectTypes)
    updated_effect_types = @($verified.UpdatedEffectTypes)
    non_spring_active_effect_types = @($verified.NonSpringActiveEffectTypes)
    condition_polarity_verified = $verified.ConditionPolarityVerified
    continuous_constant_neutralized = $verified.ContinuousConstantNeutralized
    finite_event_types_verified = @($verified.FiniteEventTypesVerified)
    curb_window_finite_starts = $verified.CurbWindowFiniteStarts
    collision_window_finite_starts = $verified.CollisionWindowFiniteStarts
    active_controller_transitions = $verified.ActiveControllerTransitions
    compatibility_noop_calls = $verified.CompatibilityNoOpCalls
    runtime_log_sha256 = $verified.RuntimeLogSha256; observation_sha256 = $verified.ObservationSha256
    seed_save_sha256 = $expectedSaveSha; seed_header_sha256 = $expectedHeaderSha
    controlled_external_close = $true; source_save_preserved = $true
  }
  $resultPath = Join-Path $runRoot 'result.json'
  [IO.File]::WriteAllText($resultPath, ($result | ConvertTo-Json -Depth 6) + [Environment]::NewLine, $utf8)
  if ((Get-Sha256 $seedSave) -cne $expectedSaveSha -or (Get-Sha256 $seedHeader) -cne $expectedHeaderSha) { throw 'Archived progressed source save changed.' }
  Write-Host 'M6-015 EVENT [5/5]: PASS - event FFB, focus recovery, and latest-active wheel/gamepad switching are physically bound.' -ForegroundColor Green
  [pscustomobject]@{ Passed=$true; Decision=$verified.Decision; EffectTypes=$verified.NonSpringActiveEffectTypes; ResultPath=$resultPath; ProgressedRunUserRoot=$user }
} catch {
  $failure = $_
  if ($null -ne $process -and -not $process.HasExited) {
    $forced = $true
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    $null = $process.WaitForExit(5000)
  }
  if ($forced) { throw "M6-015 event failure required force cleanup. $($failure.Exception.Message) Private run: '$runRoot'." }
  throw
}
