[CmdletBinding()]
param(
  [string]$BuildRoot = 'out/build/win-amd64-release',
  [string]$GameRoot = 'private/game'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdk = Join-Path $repo 'third_party/rexglue-sdk'
$completedRoute = Join-Path $repo 'private/evidence/M5-012/20260816-132209-a316f851'
$completedUser = Join-Path $completedRoute 'runs/01/user'
$completedSaveSha256 = '711B94303445034A3B127F83E8143FF6DBA3FC96C2B6176F8720E49829AD5021'
$saveRelative = 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
$verifier = Join-Path $PSScriptRoot 'verify-race-restart-smoke.ps1'
$utf8 = [Text.UTF8Encoding]::new($false)

if (-not ('MclaRaceRestartWindow' -as [type])) {
  Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;
public static class MclaRaceRestartWindow{delegate bool E(IntPtr h,IntPtr p);[DllImport("user32.dll")]static extern bool EnumWindows(E c,IntPtr p);[DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);[DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr h);[DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetWindowText(IntPtr h,StringBuilder s,int n);[DllImport("user32.dll")]static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);public static IntPtr[] Handles(int pid){var a=new List<IntPtr>();EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}public static string Title(IntPtr h){var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty;}public static bool Close(IntPtr h){return PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);}}
'@
}

function Resolve-Safe([string]$Path, [string]$Description, [switch]$Exists) {
  $full = if ([IO.Path]::IsPathRooted($Path)) { [IO.Path]::GetFullPath($Path) } else { [IO.Path]::GetFullPath((Join-Path $repo $Path)) }
  $prefix = $repo.TrimEnd('\') + '\'
  if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes the repository." }
  $current = $repo
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object { $_.Length })) {
    $current = Join-Path $current $part
    if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "$Description traverses a reparse point." }
  }
  if ($Exists -and -not (Test-Path -LiteralPath $full)) { throw "$Description is missing." }
  $full
}

function Read-LiveLogs([string]$Root) {
  $text = ''
  foreach ($file in @(Get-ChildItem -LiteralPath $Root -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue)) {
    try { $stream = [IO.File]::Open($file.FullName, 'Open', 'Read', 'ReadWrite'); try { $reader = [IO.StreamReader]::new($stream); $text += $reader.ReadToEnd(); $reader.Dispose() } finally { $stream.Dispose() } } catch {}
  }
  $text
}

function Close-ExactWindow([Diagnostics.Process]$Process) {
  $matches = @()
  foreach ($handle in [MclaRaceRestartWindow]::Handles($Process.Id)) { if ([regex]::IsMatch([MclaRaceRestartWindow]::Title($handle), '^mcla \[rexglue-v[^\]]+\]$')) { $matches += $handle } }
  if ($matches.Count -ne 1 -or -not [MclaRaceRestartWindow]::Close($matches[0])) { throw 'Exact PID/window WM_CLOSE failed.' }
}

function Get-TreeIdentity([string]$Root) {
  $rootPath = Resolve-Safe $Root 'Evidence tree' -Exists; $entries = @(); $bytes = 0L
  foreach ($item in @(Get-ChildItem -LiteralPath $rootPath -Recurse -Force | Sort-Object FullName)) {
    $relative = $item.FullName.Substring($rootPath.Length).TrimStart('\').Replace('\', '/')
    if ($relative -ceq 'result.json') { continue }
    if ($item.PSIsContainer) { $entries += [ordered]@{ kind = 'directory'; path = $relative }; continue }
    $bytes += $item.Length; $entries += [ordered]@{ kind = 'file'; path = $relative; bytes = $item.Length; sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash }
  }
  $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4; $sha = [Security.Cryptography.SHA256]::Create()
  try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
  [pscustomobject]@{ sha256 = $hash; files = @($entries | Where-Object { $_.kind -ceq 'file' }).Count; directories = @($entries | Where-Object { $_.kind -ceq 'directory' }).Count; bytes = $bytes }
}

$build = Resolve-Safe $BuildRoot 'Release build' -Exists; $game = Resolve-Safe $GameRoot 'Game root' -Exists
if (-not [string]::Equals($build, (Resolve-Safe 'out/build/win-amd64-release' 'Canonical Release build' -Exists), [StringComparison]::OrdinalIgnoreCase) -or -not [string]::Equals($game, (Resolve-Safe 'private/game' 'Canonical game' -Exists), [StringComparison]::OrdinalIgnoreCase)) { throw 'M5-012 restart requires canonical Release/game roots.' }
$routeProbe = & (Join-Path $PSScriptRoot 'verify-race-results-smoke.ps1') -RunPath (Join-Path $completedRoute 'runs/01')
if (-not $routeProbe.controlled_exit -or @($routeProbe.checkpoint_captures).Count -ne 3) { throw 'Completed route evidence is invalid.' }
if ((Get-FileHash -LiteralPath (Join-Path $completedUser $saveRelative) -Algorithm SHA256).Hash -cne $completedSaveSha256) { throw 'Completed save identity drifted.' }
$tag = (& git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim(); $head = (& git -C $sdk rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $tag -cne 'v0.9.0.21' -or $head -cne '3ef5b4f143d56b57e3c0e539cb0009ffe3a67e05' -or (git -C $sdk status --porcelain)) { throw 'M5-012 restart requires clean exact ReXGlue v0.9.0.21.' }
$artifacts = @('mcla.exe', 'rexruntime.dll', 'TracyClient.dll', 'rexgpu-xenos.dll') | ForEach-Object { $path = Resolve-Safe (Join-Path $build $_) "Release artifact $_" -Exists; [pscustomobject][ordered]@{ name = $_; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash } }
$exe = Join-Path $build 'mcla.exe'
if (@((Get-Process mcla -ErrorAction SilentlyContinue) | Where-Object { try { [string]::Equals($_.Path, $exe, [StringComparison]::OrdinalIgnoreCase) } catch { $false } }).Count) { throw 'Canonical Release MCLA is already running.' }

$runRoot = Resolve-Safe ('private/evidence/M5-012/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)) 'Restart root'
$cycle = Join-Path $runRoot 'runs/01'; $user = Join-Path $cycle 'user'; $cache = Join-Path $cycle 'cache'
[IO.Directory]::CreateDirectory($user) | Out-Null; [IO.Directory]::CreateDirectory($cache) | Out-Null
Copy-Item -LiteralPath (Join-Path $completedUser 'B13EBABEBABEBABE') -Destination $user -Recurse -Force
$log = Join-Path $cycle 'mcla.log'
$arguments = @('--mcla_physics_timing_probe=true', '--mcla_first_frame_settle_seconds=45', '--mcla_frontend_gameplay_wait_seconds=45', '--async_shader_compilation=false', '--d3d12_pipeline_creation_threads=0', '--input_backend=sdl', '--mnk_mode=false', '--log_max_file_size_mb=8', '--log_max_files=15', '--log_level=info', '--fullscreen=false', "--game_data_root=`"$game`"", "--user_data_root=`"$user`"", "--cache_root=`"$cache`"", "--log_file=`"$log`"")
Write-Host 'M5-012 restart: loading the completed save in the prepared full Release runtime...' -ForegroundColor Cyan
Write-Host 'No input is needed. The probe will enter free roam, sample stock timing for 10 seconds, and close externally.' -ForegroundColor DarkCyan
$process = $null; $forced = $false
try {
  $process = Start-Process $exe -ArgumentList $arguments -WorkingDirectory $build -PassThru
  $deadline = [DateTime]::UtcNow.AddSeconds(210)
  while ([DateTime]::UtcNow -lt $deadline -and -not $process.HasExited -and -not (Read-LiveLogs $cycle).Contains('MCLA_PHYSICS_TIMING_SUMMARY v=1 status=COMPLETE')) { Start-Sleep -Milliseconds 250 }
  if ($process.HasExited) { throw "Process exited early with code $($process.ExitCode)." }
  if (-not (Read-LiveLogs $cycle).Contains('MCLA_PHYSICS_TIMING_SUMMARY v=1 status=COMPLETE')) { throw 'Restart/timing deadline expired.' }
  Close-ExactWindow $process
  if (-not $process.WaitForExit(10000) -or $process.ExitCode -ne 0) { throw 'Controlled external WM_CLOSE failed.' }
} catch {
  $failure = $_
  if ($null -ne $process -and -not $process.HasExited) { $forced = $true; Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue; $null = $process.WaitForExit(5000) }
  if ($forced) { throw "M5-012 restart failure required force cleanup. $($failure.Exception.Message) Private run: '$runRoot'." }
  throw
}

$probe = & $verifier -RunPath $cycle
$record = [ordered]@{
  schema = 'mcla-race-restart-v1'; task = 'M5-012'; decision = 'first-series-results-return-and-release-restart-pass'
  sdk_version = '0.9.0.21'; sdk_commit = '3ef5b4f143d56b57e3c0e539cb0009ffe3a67e05'; build_configuration = 'Release'
  completed_route_run_id = '20260816-132209-a316f851'; completed_route_tree = Get-TreeIdentity $completedRoute; completed_save_sha256 = $completedSaveSha256
  restart_probe = $probe; release_artifacts = @($artifacts); restart_tree = Get-TreeIdentity $runRoot
  scope = 'one operator-confirmed Ian event series reached final rewards/results and controllable free roam; the changed completed save then loaded in a fresh optimized Release process that sustained the stock 30-FPS timing sample; five repeated race/resource checks remain M5-013'
}
$result = Join-Path $runRoot 'result.json'; [IO.File]::WriteAllText($result, ((ConvertTo-Json $record -Depth 10) + [Environment]::NewLine), $utf8)
$verified = & $verifier -ResultPath $result
Write-Host "M5-012 PASS: completed route, changed save, fresh-process restart, and Release 30-FPS sample verified. Result: '$result'." -ForegroundColor Green
$verified
