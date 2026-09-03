[CmdletBinding()]
param([switch]$ValidateOnly)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$archiveRoot = Join-Path $repo 'private/save-archive/M6-014'
$build = Join-Path $repo 'out/build/win-amd64-release'
$game = Join-Path $repo 'private/game'
$saveWatcher = Join-Path $PSScriptRoot 'watch-soak-save.ps1'
$pwshHost = (Get-Process -Id $PID).Path
$utf8 = [Text.UTF8Encoding]::new($false)
$saveRelative = 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
$headerRelative = 'B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'

if (-not ('MclaRaceBackDiagnosticNative' -as [type])) {
  Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;
public static class MclaRaceBackDiagnosticNative{
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

function Safe([string]$Path, [string]$Description, [switch]$Exists) {
  $full = [IO.Path]::GetFullPath($Path)
  $prefix = $repo.TrimEnd('\') + '\'
  if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes the repository." }
  $cursor = $repo
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object Length)) {
    $cursor = Join-Path $cursor $part
    if ((Test-Path -LiteralPath $cursor) -and ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "$Description traverses a reparse point." }
  }
  if ($Exists -and -not (Test-Path -LiteralPath $full)) { throw "$Description is missing." }
  $full
}

function Hash([string]$Path) {
  $stream = [IO.File]::OpenRead((Safe $Path 'Hash source' -Exists))
  $sha = [Security.Cryptography.SHA256]::Create()
  try { ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') }
  finally { $sha.Dispose(); $stream.Dispose() }
}

function WriteJson([string]$Path, $Value) {
  [IO.File]::WriteAllText($Path, (($Value | ConvertTo-Json -Depth 12) + [Environment]::NewLine), $utf8)
}

function ReadLogs([string]$Root) {
  $text = ''
  foreach ($file in @(Get-ChildItem -LiteralPath $Root -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue | Sort-Object Name)) {
    try {
      $stream = [IO.File]::Open($file.FullName, 'Open', 'Read', 'ReadWrite')
      try { $reader = [IO.StreamReader]::new($stream); try { $text += $reader.ReadToEnd() } finally { $reader.Dispose() } }
      finally { $stream.Dispose() }
    } catch {}
  }
  $text
}

function WaitMarker([Diagnostics.Process]$Process, [string]$Root, [string]$Marker, [int]$Seconds, [string]$Step) {
  $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
  while ([DateTime]::UtcNow -lt $deadline) {
    if ($Process.HasExited) { throw "Process exited during '$Step' (exit $($Process.ExitCode))." }
    if ((ReadLogs $Root).Contains($Marker)) { return }
    Start-Sleep -Milliseconds 200
  }
  throw "Timed out during '$Step'."
}

function ReadOutcome([Diagnostics.Process]$Process) {
  try { $null = [Console]::KeyAvailable } catch { throw 'Interactive console input is unavailable; run this script in a visible console.' }
  while ($true) {
    Write-Host "Lose one Red Light Driver event and select RACE BACK. After the map zoom finishes or sticks, Alt-Tab here.`nType exactly RACE BACK RETURNED or RACE BACK STUCK: " -ForegroundColor Yellow -NoNewline
    $input = [Text.StringBuilder]::new()
    while ($true) {
      if ($Process.HasExited) { Write-Host; throw "Process exited while waiting for the Race Back outcome (exit $($Process.ExitCode))." }
      if (-not [Console]::KeyAvailable) { Start-Sleep -Milliseconds 50; continue }
      $key = [Console]::ReadKey($true)
      if ($key.Key -eq [ConsoleKey]::Enter) { Write-Host; break }
      if ($key.Key -eq [ConsoleKey]::Backspace) { if ($input.Length) { $input.Length--; Write-Host "`b `b" -NoNewline }; continue }
      if (-not [char]::IsControl($key.KeyChar)) { $null = $input.Append($key.KeyChar); Write-Host $key.KeyChar -NoNewline }
    }
    if ($input.ToString() -ceq 'RACE BACK RETURNED') { return 'returned' }
    if ($input.ToString() -ceq 'RACE BACK STUCK') { return 'stuck' }
    Write-Host 'Ignored; the exact phrase is required and the game remains running.' -ForegroundColor DarkYellow
  }
}

function CloseExact([Diagnostics.Process]$Process) {
  $matches = @()
  foreach ($handle in [MclaRaceBackDiagnosticNative]::Handles($Process.Id)) {
    if ([regex]::IsMatch([MclaRaceBackDiagnosticNative]::Title($handle), '^mcla \[rexglue-v[^\]]+\]$')) { $matches += $handle }
  }
  if ($matches.Count -ne 1 -or -not [MclaRaceBackDiagnosticNative]::Close($matches[0])) { throw 'Exact PID/window WM_CLOSE failed.' }
}

function LatestSeed([string]$Root) {
  $candidates = @()
  foreach ($directory in @(Get-ChildItem -LiteralPath $Root -Directory -ErrorAction SilentlyContinue)) {
    $latestPath = Join-Path $directory.FullName 'latest.json'
    if (-not (Test-Path -LiteralPath $latestPath)) { continue }
    try {
      $manifest = Get-Content -LiteralPath $latestPath -Raw | ConvertFrom-Json
      if ($manifest.schema -cne 'mcla-soak-save-snapshot-v1' -or -not $manifest.complete_profile_tree) { continue }
      $snapshot = Safe (Join-Path $directory.FullName ([string]$manifest.snapshot_directory)) 'Candidate snapshot' -Exists
      if ((Split-Path $snapshot -Parent) -cne $directory.FullName) { continue }
      if ((Hash (Join-Path $snapshot $saveRelative)) -cne $manifest.save_sha256 -or (Hash (Join-Path $snapshot $headerRelative)) -cne $manifest.header_sha256) { continue }
      $candidates += [pscustomobject]@{ Captured = [DateTime]::Parse([string]$manifest.captured_utc).ToUniversalTime(); Snapshot = $snapshot; Manifest = $manifest; Latest = $latestPath }
    } catch {}
  }
  if (-not $candidates.Count) { throw 'No complete hash-verified M6-014 recovery snapshot exists.' }
  @($candidates | Sort-Object Captured -Descending)[0]
}

$archiveRoot = Safe $archiveRoot 'M6-014 save archive' -Exists
$game = Safe $game 'Game root' -Exists
$build = Safe $build 'Release build'
$seed = LatestSeed $archiveRoot
if ($ValidateOnly) {
  [pscustomobject]@{ Passed = $true; Seed = $seed.Snapshot; SaveSha256 = $seed.Manifest.save_sha256; HeaderSha256 = $seed.Manifest.header_sha256 }
  return
}

$evidence = Safe (Join-Path $repo 'private/evidence/M6-014/race-back-camera') 'Race Back evidence root'
[IO.Directory]::CreateDirectory($evidence) | Out-Null
$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$root = Join-Path $evidence $runId
$run = Join-Path $root 'run'
$user = Join-Path $run 'user'
$cache = Join-Path $run 'cache'
[IO.Directory]::CreateDirectory($user) | Out-Null
[IO.Directory]::CreateDirectory($cache) | Out-Null
Copy-Item -LiteralPath (Join-Path $seed.Snapshot 'B13EBABEBABEBABE') -Destination $user -Recurse
$buildLog = Join-Path $root 'build.log'
$cmake = (& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath).CMakePath
Write-Host 'RACE BACK [1/4]: clean-building the traced Release title...' -ForegroundColor Cyan
& $cmake --preset win-amd64-release *>&1 | Tee-Object -FilePath $buildLog | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Release configure failed. Private run: '$root'." }
& $cmake --build --preset win-amd64-release --target mcla --clean-first --parallel 8 *>&1 | Tee-Object -FilePath $buildLog -Append | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Release build failed. Private run: '$root'." }
$exe = Safe (Join-Path $build 'mcla.exe') 'Release executable' -Exists
if (@((Get-Process mcla -ErrorAction SilentlyContinue) | Where-Object { try { $_.Path -ceq $exe } catch { $false } }).Count) { throw 'Canonical MCLA is already running.' }

$saveArchive = Safe (Join-Path $archiveRoot "race-back-$runId") 'Race Back save archive'
$log = Join-Path $run 'mcla.log'
$args = @('--mcla_first_frame_probe=true','--mcla_garage_lifecycle_probe=true','--mcla_garage_lifecycle_cycle=2','--mcla_race_back_probe=true','--xam_user_signin_state=1','--input_backend=sdl','--mnk_mode=false','--readback_resolve=fast','--async_shader_compilation=false','--d3d12_pipeline_creation_threads=0','--log_level=info','--log_max_file_size_mb=16','--log_max_files=20','--fullscreen=false',('--game_data_root="{0}"' -f $game),('--user_data_root="{0}"' -f $user),('--cache_root="{0}"' -f $cache),('--log_file="{0}"' -f $log))
Write-Host "RACE BACK [2/4]: launching the latest recovered save ($($seed.Manifest.captured_utc))..." -ForegroundColor Cyan
$process = $null
$watcher = $null
$forced = $false
try {
  $process = Start-Process $exe -ArgumentList $args -WorkingDirectory $build -PassThru
  $watcher = Start-Process $pwshHost -ArgumentList @('-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-File',('"' + $saveWatcher + '"'),'-SourceUserRoot',('"' + $user + '"'),'-ArchiveRoot',('"' + $saveArchive + '"'),'-MclaProcessId',$process.Id) -WorkingDirectory $repo -WindowStyle Hidden -PassThru
  WaitMarker $process $run 'MCLA_RACE_BACK_CONFIG v=1' 90 'Race Back trace readiness'
  WaitMarker $process $run 'MCLA_GARAGE_LIFECYCLE_CONFIG v=1 cycle=2' 20 'automatic START readiness'
  $request = Join-Path $user '.mcla-garage-control.request'
  $temporary = $request + '.tmp'
  [IO.File]::WriteAllText($temporary, '1 START', $utf8)
  Move-Item -LiteralPath $temporary -Destination $request -Force
  WaitMarker $process $run 'MCLA_GARAGE_CONTROL v=1 sequence=1 action=START capture=0 width=0 height=0' 30 'automatic START'
  Write-Host 'AUTO START         | PASS - waiting 30 seconds for saved gameplay...' -ForegroundColor Green
  $deadline = [DateTime]::UtcNow.AddSeconds(30)
  while ([DateTime]::UtcNow -lt $deadline) { if ($process.HasExited) { throw "Process exited while loading gameplay (exit $($process.ExitCode))." }; Start-Sleep -Milliseconds 200 }
  $outcome = ReadOutcome $process
  Start-Sleep -Seconds 2
  $logs = ReadLogs $run
  $selectCount = [regex]::Matches($logs, 'MCLA_RACE_BACK_SELECT v=1').Count
  $returnCount = [regex]::Matches($logs, 'MCLA_RACE_BACK_COMMAND_RETURN v=1').Count
  $handlerCount = [regex]::Matches($logs, 'MCLA_RACE_BACK_CAMERA_HANDLER v=1').Count
  $applyCount = [regex]::Matches($logs, 'MCLA_RACE_BACK_CAMERA_APPLY_EDGE v=1').Count
  if ($selectCount -lt 1 -or $returnCount -lt 1) { throw 'The owner confirmed an outcome, but the Race Back command hook was not reached.' }
  Write-Host "RACE BACK TRACE     | select=$selectCount return=$returnCount handler=$handlerCount apply=$applyCount outcome=$outcome" -ForegroundColor DarkCyan
  CloseExact $process
  if (-not $process.WaitForExit(10000) -or $process.ExitCode -ne 0) { throw 'Controlled external WM_CLOSE failed.' }
} catch {
  $failure = $_
  if ($process -and -not $process.HasExited) {
    try { CloseExact $process; $null = $process.WaitForExit(10000) } catch {}
    if (-not $process.HasExited) { $forced = $true; Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue; $null = $process.WaitForExit(5000) }
  }
  if ($watcher -and -not $watcher.HasExited) { $null = $watcher.WaitForExit(15000) }
  if ($forced) { throw "Race Back diagnostic required force cleanup. $($failure.Exception.Message) Private run: '$root'." }
  throw $failure
}
if ($watcher -and -not $watcher.HasExited -and -not $watcher.WaitForExit(15000)) { throw 'Save watcher did not finish.' }
if ($watcher -and $watcher.ExitCode -ne 0) { throw "Save watcher failed with exit $($watcher.ExitCode)." }
$latestRecovery = Safe (Join-Path $saveArchive 'latest.json') 'Race Back recovery manifest' -Exists
$recovery = Get-Content -LiteralPath $latestRecovery -Raw | ConvertFrom-Json
$result = [ordered]@{
  schema = 'mcla-race-back-camera-diagnostic-v1'; task = 'M6-014'; known_issue = 'KI-026'; run_id = $runId
  decision = if ($outcome -ceq 'returned') { 'race-back-camera-return-pass' } else { 'race-back-camera-softlock-reproduced' }
  operator_outcome = $outcome; command_select_calls = $selectCount; command_return_calls = $returnCount
  camera_handler_calls = $handlerCount; camera_apply_calls = $applyCount; controlled_external_close = $true
  force_cleanup = $false; seed_snapshot = $seed.Snapshot.Substring($repo.Length + 1).Replace('\','/')
  seed_save_sha256 = $seed.Manifest.save_sha256; recovery_snapshot = $recovery.snapshot_directory
  recovery_save_sha256 = $recovery.save_sha256
}
WriteJson (Join-Path $root 'result.json') $result
Write-Host 'RACE BACK [3/4]: command/camera trace and recoverable save snapshot persisted.' -ForegroundColor Cyan
Write-Host "RACE BACK [4/4]: diagnostic complete - $($result.decision). Result: '$root\result.json'." -ForegroundColor $(if ($outcome -ceq 'returned') { 'Green' } else { 'Yellow' })
[pscustomobject]$result
