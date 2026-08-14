[CmdletBinding()]
param(
  [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
  [string]$GameRoot = 'private/game'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-gameplay-input-smoke.ps1'
$gameVerifier = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$cmake = $toolchain.CMakePath
$utf8 = [Text.UTF8Encoding]::new($false)
$seedRoot = Join-Path $repo 'private/baseline/M4-011/post-oobe-profile'
$framePhases = @(
  'neutral-before', 'throttle', 'throttle-release', 'brake',
  'brake-release', 'steer-left', 'steer-right', 'pause'
)

if (-not ('MclaGameplayInputWindow' -as [type])) {
  Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;
public static class MclaGameplayInputWindow {
  delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc c, IntPtr p);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  public static IntPtr[] Handles(int pid) { var a=new List<IntPtr>(); EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray(); }
  public static string Title(IntPtr h) { var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty; }
  public static bool Close(IntPtr h) { return PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero); }
}
'@
}

function Resolve-SafePath {
  param([string]$Path, [string]$Description, [switch]$Exists)
  $full = if ([IO.Path]::IsPathRooted($Path)) {
    [IO.Path]::GetFullPath($Path)
  } else {
    [IO.Path]::GetFullPath((Join-Path $repo $Path))
  }
  $prefix = $repo.TrimEnd('\') + '\'
  if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "$Description escapes repository."
  }
  $current = $repo
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object { $_.Length -gt 0 })) {
    $current = Join-Path $current $part
    if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
      throw "$Description traverses a reparse point."
    }
  }
  if ($Exists -and -not (Test-Path -LiteralPath $full)) {
    throw "$Description is missing."
  }
  return $full
}

function Assert-NoReparseTree {
  param([string]$Root)
  $pending = [Collections.Generic.Stack[string]]::new()
  $pending.Push($Root)
  while ($pending.Count) {
    foreach ($item in @(Get-ChildItem -LiteralPath $pending.Pop() -Force)) {
      if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw 'Tree contains a reparse point.'
      }
      if ($item.PSIsContainer) {
        $pending.Push($item.FullName)
      }
    }
  }
}

function Get-TreeSnapshot {
  param([string]$Root)
  $rootPath = Resolve-SafePath $Root 'Tree' -Exists
  Assert-NoReparseTree $rootPath
  $items = @(Get-ChildItem -LiteralPath $rootPath -Recurse -Force)
  $files = @($items | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)
  $entries = @()
  foreach ($directory in @($items | Where-Object { $_.PSIsContainer } | Sort-Object FullName)) {
    $entries += [ordered]@{ kind = 'directory'; path = $directory.FullName.Substring($rootPath.Length).TrimStart('\').Replace('\', '/') }
  }
  foreach ($file in $files) {
    $entries += [ordered]@{
      kind = 'file'
      path = $file.FullName.Substring($rootPath.Length).TrimStart('\').Replace('\', '/')
      bytes = $file.Length
      sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
  }
  $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4
  $sha = [Security.Cryptography.SHA256]::Create()
  try {
    $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') })
  } finally {
    $sha.Dispose()
  }
  return [pscustomobject]@{
    Hash = $hash
    FileCount = $files.Count
    DirectoryCount = @($items | Where-Object { $_.PSIsContainer }).Count
    Bytes = [int64](($files | Measure-Object Length -Sum).Sum)
  }
}

function Get-GameIdentity {
  param([string]$Root)
  $tree = Get-TreeSnapshot $Root
  $verified = & $gameVerifier -GamePath $Root -VerifyHashes
  return [ordered]@{
    file_count = $verified.FileCount
    payload_bytes = $verified.PayloadBytes
    manifest_sha256 = (Get-FileHash -LiteralPath $verified.ManifestPath -Algorithm SHA256).Hash
    tree_sha256 = $tree.Hash
    tree_file_count = $tree.FileCount
    tree_directory_count = $tree.DirectoryCount
    tree_bytes = $tree.Bytes
  }
}

function Get-Artifacts {
  param([string]$Root)
  return @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll') | ForEach-Object {
    $path = Resolve-SafePath (Join-Path $Root $_) "Artifact $_" -Exists
    [ordered]@{ name = $_; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
  }
}

function Get-ExactProcesses {
  param([string]$Executable)
  return @((Get-Process mcla -ErrorAction SilentlyContinue) | Where-Object {
    try { [string]::Equals($_.Path, $Executable, [StringComparison]::OrdinalIgnoreCase) } catch { $false }
  })
}

function Test-LogContains {
  param([string]$Directory, [string]$Needle)
  foreach ($file in @(Get-ChildItem -LiteralPath $Directory -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue)) {
    try {
      $stream = [IO.File]::Open($file.FullName, 'Open', 'Read', 'ReadWrite')
      try {
        $reader = [IO.StreamReader]::new($stream)
        $text = $reader.ReadToEnd()
        $reader.Dispose()
      } finally {
        $stream.Dispose()
      }
      if ($text.Contains($Needle)) {
        return $true
      }
    } catch {
    }
  }
  return $false
}

function Close-ExactWindow {
  param([Diagnostics.Process]$Process)
  $matches = @()
  foreach ($handle in [MclaGameplayInputWindow]::Handles($Process.Id)) {
    if ([regex]::IsMatch([MclaGameplayInputWindow]::Title($handle), '^mcla \[rexglue-v[^\]]+\]$')) {
      $matches += $handle
    }
  }
  if ($matches.Count -ne 1 -or -not [MclaGameplayInputWindow]::Close($matches[0])) {
    throw 'Exact PID/window WM_CLOSE failed.'
  }
}

function Invoke-Logged {
  param([scriptblock]$Command, [string]$Log, [switch]$Append)
  $prior = $ErrorActionPreference
  $ErrorActionPreference = 'Continue'
  try {
    if ($Append) {
      & $Command *>&1 | Tee-Object -FilePath $Log -Append | Out-Null
    } else {
      & $Command *>&1 | Tee-Object -FilePath $Log | Out-Null
    }
    return $LASTEXITCODE
  } finally {
    $ErrorActionPreference = $prior
  }
}

$build = Resolve-SafePath $BuildRoot 'Build root' -Exists
$game = Resolve-SafePath $GameRoot 'Game root' -Exists
if (
  -not [string]::Equals($build, (Resolve-SafePath 'out/build/win-amd64-relwithdebinfo' 'Canonical build' -Exists), [StringComparison]::OrdinalIgnoreCase) -or
  -not [string]::Equals($game, (Resolve-SafePath 'private/game' 'Canonical game' -Exists), [StringComparison]::OrdinalIgnoreCase)
) {
  throw 'M5-006 requires canonical build and game roots.'
}

$seed = Resolve-SafePath $seedRoot 'Pinned post-OOBE seed' -Exists
if (
  (Get-FileHash -LiteralPath (Join-Path $seed 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav') -Algorithm SHA256).Hash -cne 'E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68' -or
  (Get-FileHash -LiteralPath (Join-Path $seed 'B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header') -Algorithm SHA256).Hash -cne '1DAEF55FA78BDD3F1AA75A36772B801C6C714B6D1A115A97904F3D1C957BC4C9'
) {
  throw 'Pinned seed identity failed.'
}

$evidenceRoot = Resolve-SafePath 'private/evidence/M5-006' 'M5-006 evidence root'
[IO.Directory]::CreateDirectory($evidenceRoot) | Out-Null
$runRoot = Join-Path $evidenceRoot ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
[IO.Directory]::CreateDirectory($runRoot) | Out-Null
$buildLog = Join-Path $runRoot 'relwithdebinfo-clean-build.log'

Write-Host 'M5-006 [1/6]: validating source game, pinned save, and controller evidence...' -ForegroundColor Cyan
$gameBefore = Get-GameIdentity $game

Write-Host 'M5-006 [2/6]: clean-building the current MCLA host...' -ForegroundColor Cyan
if ((Invoke-Logged { & $cmake --preset win-amd64-relwithdebinfo } $buildLog) -ne 0) {
  throw 'App configure failed.'
}
if ((Invoke-Logged { & $cmake --build --preset win-amd64-relwithdebinfo --target mcla --clean-first --parallel 8 } $buildLog -Append) -ne 0) {
  throw "App clean build failed. Private run: '$runRoot'."
}

$executable = Resolve-SafePath (Join-Path $build 'mcla.exe') 'Executable' -Exists
if (@(Get-ExactProcesses $executable).Count) {
  throw 'Canonical MCLA process is already running.'
}
$artifactsBefore = @(Get-Artifacts $build)

Write-Host 'M5-006 [3/6]: running autonomous throttle, brake, steering, and pause route...' -ForegroundColor Cyan
$cycleRoot = Join-Path $runRoot 'runs/01'
$userRoot = Join-Path $cycleRoot 'user'
$cacheRoot = Join-Path $cycleRoot 'cache'
[IO.Directory]::CreateDirectory($userRoot) | Out-Null
[IO.Directory]::CreateDirectory($cacheRoot) | Out-Null
Get-ChildItem -LiteralPath $seed -Force | Copy-Item -Destination $userRoot -Recurse -Force
$log = Join-Path $cycleRoot 'mcla.log'
$process = $null
$forced = $false
try {
  $arguments = @(
    '--mcla_gameplay_input_probe=true',
    '--mcla_first_frame_settle_seconds=45',
    '--mcla_frontend_gameplay_wait_seconds=45',
    '--log_max_file_size_mb=8',
    '--log_max_files=15',
    '--log_level=trace',
    '--fullscreen=false',
    "--game_data_root=$game",
    "--user_data_root=$userRoot",
    "--cache_root=$cacheRoot",
    "--log_file=$log"
  )
  $process = Start-Process $executable -ArgumentList $arguments -WorkingDirectory $build -PassThru
  $deadline = [DateTime]::UtcNow.AddSeconds(210)
  while ([DateTime]::UtcNow -lt $deadline) {
    if ($process.HasExited) {
      throw "Process exited before gameplay-input PASS (exit $($process.ExitCode))."
    }
    if (Test-LogContains $cycleRoot 'MCLA_GAMEPLAY_INPUT_SUMMARY v=1 status=PASS') {
      break
    }
    Start-Sleep -Milliseconds 250
  }
  if (-not (Test-LogContains $cycleRoot 'MCLA_GAMEPLAY_INPUT_SUMMARY v=1 status=PASS')) {
    throw 'Gameplay-input route deadline expired.'
  }
  Close-ExactWindow $process
  if (-not $process.WaitForExit(10000) -or $process.ExitCode -ne 0) {
    throw 'Controlled external WM_CLOSE failed.'
  }
  if (@(Get-ExactProcesses $executable).Count) {
    throw 'Exact-path MCLA process survived close.'
  }
} catch {
  $failure = $_
  if ($null -ne $process -and -not $process.HasExited) {
    $forced = $true
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    $null = $process.WaitForExit(5000)
  }
  if ($forced) {
    throw "M5-006 failure required force cleanup. $($failure.Exception.Message) Private run: '$runRoot'."
  }
  throw
}

Write-Host 'M5-006 [4/6]: verifying causal input records and physical response frames...' -ForegroundColor Cyan
$probe = & $verifier -ProbeOnly -RuntimeLogPath $log -UserRoot $userRoot

Write-Host 'M5-006 [5/6]: checking game, save, artifact, and evidence integrity...' -ForegroundColor Cyan
$gameAfter = Get-GameIdentity $game
$artifactsAfter = @(Get-Artifacts $build)
if (
  ($gameBefore | ConvertTo-Json -Compress) -cne ($gameAfter | ConvertTo-Json -Compress) -or
  ($artifactsBefore | ConvertTo-Json -Compress) -cne ($artifactsAfter | ConvertTo-Json -Compress)
) {
  throw 'Source-game or runtime-artifact identity changed.'
}
$cycleSave = Join-Path $userRoot 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
$cycleHeader = Join-Path $userRoot 'B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'
if (
  (Get-FileHash -LiteralPath $cycleSave -Algorithm SHA256).Hash -cne 'E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68' -or
  (Get-FileHash -LiteralPath $cycleHeader -Algorithm SHA256).Hash -cne '1DAEF55FA78BDD3F1AA75A36772B801C6C714B6D1A115A97904F3D1C957BC4C9'
) {
  throw 'Cycle modified the pinned save.'
}

$captures = [ordered]@{}
foreach ($phase in $framePhases) {
  $captures[$phase] = [ordered]@{
    sha256 = $probe.Bmps[$phase].Sha256
    bytes = $probe.Bmps[$phase].Length
  }
}
$userTree = Get-TreeSnapshot $userRoot
$cacheTree = Get-TreeSnapshot $cacheRoot
$cycleTree = Get-TreeSnapshot $cycleRoot

$result = [ordered]@{
  schema = 1
  task = 'M5-006'
  decision = 'saved-gameplay-input-pass'
  sdk_version = '0.9.0.18'
  route_id = 'pinned-save-gameplay-input-v1'
  controller_baseline = [ordered]@{
    digital_run = '20260812-212030-5fc01c73'
    analog_focus_run = '20260813-124600-293c07b3'
    hotplug_run = '20260813-144406-2c1974da'
    physical_digital_pass = $true
    physical_analog_pass = $true
    physical_reconnect_pass = $true
    source_parity_v13_v18 = $true
    multi_pad_physically_claimed = $false
  }
  build = [ordered]@{
    log_sha256 = (Get-FileHash -LiteralPath $buildLog -Algorithm SHA256).Hash
    executable_sha256 = (Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash
  }
  seed = [ordered]@{
    save_sha256 = 'E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68'
    header_sha256 = '1DAEF55FA78BDD3F1AA75A36772B801C6C714B6D1A115A97904F3D1C957BC4C9'
    unchanged = $true
  }
  game_identity = [ordered]@{ before = $gameBefore; after = $gameAfter }
  artifacts = [ordered]@{ before = $artifactsBefore; after = $artifactsAfter }
  cycle = [ordered]@{
    exit_code = 0
    close_requested = $true
    harness_force_cleanup = $false
    runtime_logs = @($probe.LogSet.Files)
    runtime_log_set_sha256 = $probe.LogSet.Hash
    runtime_log_bytes = $probe.LogSet.Bytes
    captures = $captures
    gameplay_input_records = $probe.GameplayInputRecords
    dismiss_input_records = $probe.DismissInputRecords
    neutral_throttle_difference = $probe.NeutralThrottleDifference
    throttle_brake_difference = $probe.ThrottleBrakeDifference
    steer_left_right_difference = $probe.SteerLeftRightDifference
    pause_correlation_ppm = $probe.PauseCorrelationPpm
    user_tree_sha256 = $userTree.Hash
    cache_tree_sha256 = $cacheTree.Hash
    cycle_tree_sha256 = $cycleTree.Hash
  }
  scope = [ordered]@{
    saved_free_roam_only = $true
    synthetic_gameplay_probe = $true
    physical_sdl_source_bound_external = $true
    race_maneuver_parity_claimed = $false
    multi_pad_claimed = $false
    force_feedback_claimed = $false
  }
  no_surviving_processes = $true
  data_integrity_preserved = $true
}

$resultPath = Join-Path $runRoot 'result.json'
[IO.File]::WriteAllText($resultPath, ($result | ConvertTo-Json -Depth 12) + [Environment]::NewLine, $utf8)

Write-Host 'M5-006 [6/6]: re-verifying result and immutable physical controller baseline...' -ForegroundColor Cyan
$verified = & $verifier -ResultPath $resultPath
[pscustomobject]@{
  Passed = $verified.Passed
  Decision = $verified.Decision
  GameplayInputRecords = $verified.GameplayInputRecords
  PauseCorrelationPpm = $verified.PauseCorrelationPpm
  PhysicalReconnectPassed = $verified.PhysicalReconnectPassed
  PrivateRunRoot = $runRoot
  ResultPath = $resultPath
}
