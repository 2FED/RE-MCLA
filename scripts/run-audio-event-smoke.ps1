[CmdletBinding()]
param(
  [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
  [string]$GameRoot = 'private/game'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdk = Join-Path $repo 'third_party/rexglue-sdk'
$seed = Join-Path $repo 'private/baseline/M4-011/post-oobe-profile'
$verifier = Join-Path $PSScriptRoot 'verify-audio-event-smoke.ps1'
$gameVerifier = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$cmake = $toolchain.CMakePath
$utf8 = [Text.UTF8Encoding]::new($false)
$sdkCommit = 'c4aa30c35386bb4d2ef051a59ea8e71bab667172'

if (-not ('MclaAudioEventWindow' -as [type])) {
  Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;
public static class MclaAudioEventWindow{delegate bool E(IntPtr h,IntPtr p);[DllImport("user32.dll")]static extern bool EnumWindows(E c,IntPtr p);[DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);[DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr h);[DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetWindowText(IntPtr h,StringBuilder s,int n);[DllImport("user32.dll")]static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);public static IntPtr[] Handles(int pid){var a=new List<IntPtr>();EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}public static string Title(IntPtr h){var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty;}public static bool Close(IntPtr h){return PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);}}
'@
}

function Resolve-SafePath([string]$Path, [string]$Description, [switch]$Exists) {
  $full = if ([IO.Path]::IsPathRooted($Path)) { [IO.Path]::GetFullPath($Path) } else { [IO.Path]::GetFullPath((Join-Path $repo $Path)) }
  $prefix = $repo.TrimEnd('\') + '\'
  if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes repository." }
  $current = $repo
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object { $_.Length })) {
    $current = Join-Path $current $part
    if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "$Description traverses a reparse point." }
  }
  if ($Exists -and -not (Test-Path -LiteralPath $full)) { throw "$Description is missing." }
  $full
}
function Assert-NoReparseTree([string]$Root) { $pending = [Collections.Generic.Stack[string]]::new(); $pending.Push($Root); while ($pending.Count) { foreach ($item in @(Get-ChildItem -LiteralPath $pending.Pop() -Force)) { if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Tree contains a reparse point.' }; if ($item.PSIsContainer) { $pending.Push($item.FullName) } } } }
function Get-TreeSnapshot([string]$Root) { $root = Resolve-SafePath $Root 'Tree' -Exists; Assert-NoReparseTree $root; $items = @(Get-ChildItem -LiteralPath $root -Recurse -Force); $files = @($items | Where-Object { -not $_.PSIsContainer -and -not ($_.FullName -ceq (Join-Path $root 'result.json')) } | Sort-Object FullName); $entries = @(); $bytes = 0L; foreach ($directory in @($items | Where-Object { $_.PSIsContainer } | Sort-Object FullName)) { $entries += [ordered]@{kind = 'directory'; path = $directory.FullName.Substring($root.Length).TrimStart('\').Replace('\', '/') } }; foreach ($file in $files) { $bytes += $file.Length; $entries += [ordered]@{kind = 'file'; path = $file.FullName.Substring($root.Length).TrimStart('\').Replace('\', '/'); bytes = $file.Length; sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash } }; $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4; $sha = [Security.Cryptography.SHA256]::Create(); try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }; [pscustomobject]@{Hash = $hash; FileCount = $files.Count; DirectoryCount = @($items | Where-Object { $_.PSIsContainer }).Count; Bytes = $bytes} }
function Get-GameIdentity([string]$Root) { $tree = Get-TreeSnapshot $Root; $verified = & $gameVerifier -GamePath $Root -VerifyHashes; [ordered]@{file_count = [int]$verified.FileCount; payload_bytes = [int64]$verified.PayloadBytes; manifest_sha256 = (Get-FileHash -LiteralPath $verified.ManifestPath -Algorithm SHA256).Hash; tree_sha256 = $tree.Hash; tree_file_count = $tree.FileCount; tree_directory_count = $tree.DirectoryCount; tree_bytes = $tree.Bytes} }
function Get-Artifacts([string]$Root) { @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll') | ForEach-Object { $path = Resolve-SafePath (Join-Path $Root $_) "Artifact $_" -Exists; [ordered]@{name = $_; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash} } }
function Get-ExactProcesses([string]$Executable) { @((Get-Process mcla -ErrorAction SilentlyContinue) | Where-Object { try { [string]::Equals($_.Path, $Executable, [StringComparison]::OrdinalIgnoreCase) } catch { $false } }) }
function Invoke-Logged([scriptblock]$Command, [string]$Log, [switch]$Append) { $prior = $ErrorActionPreference; $ErrorActionPreference = 'Continue'; try { if ($Append) { & $Command *>&1 | Tee-Object -FilePath $Log -Append | Out-Null } else { & $Command *>&1 | Tee-Object -FilePath $Log | Out-Null }; $LASTEXITCODE } finally { $ErrorActionPreference = $prior } }
function Read-LiveLogs([string]$Root) { $text = ''; foreach ($file in @(Get-ChildItem -LiteralPath $Root -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue)) { try { $stream = [IO.File]::Open($file.FullName, 'Open', 'Read', 'ReadWrite'); try { $reader = [IO.StreamReader]::new($stream); $text += $reader.ReadToEnd(); $reader.Dispose() } finally { $stream.Dispose() } } catch {} }; $text }
function Close-ExactWindow([Diagnostics.Process]$Process) { $matches = @(); foreach ($handle in [MclaAudioEventWindow]::Handles($Process.Id)) { if ([regex]::IsMatch([MclaAudioEventWindow]::Title($handle), '^mcla \[rexglue-v[^\]]+\]$')) { $matches += $handle } }; if ($matches.Count -ne 1 -or -not [MclaAudioEventWindow]::Close($matches[0])) { throw 'Exact PID/window WM_CLOSE failed.' } }

$build = Resolve-SafePath $BuildRoot 'Build root' -Exists
$game = Resolve-SafePath $GameRoot 'Game root' -Exists
if (-not [string]::Equals($build, (Resolve-SafePath 'out/build/win-amd64-relwithdebinfo' 'Canonical build' -Exists), [StringComparison]::OrdinalIgnoreCase) -or -not [string]::Equals($game, (Resolve-SafePath 'private/game' 'Canonical game' -Exists), [StringComparison]::OrdinalIgnoreCase)) { throw 'M5-009 requires canonical RelWithDebInfo and game roots.' }
Assert-NoReparseTree (Resolve-SafePath $seed 'Pinned post-OOBE save' -Exists)
$seedSave = (Get-FileHash -LiteralPath (Join-Path $seed 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav') -Algorithm SHA256).Hash
$seedHeader = (Get-FileHash -LiteralPath (Join-Path $seed 'B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header') -Algorithm SHA256).Hash
if ($seedSave -cne 'E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68' -or $seedHeader -cne '1DAEF55FA78BDD3F1AA75A36772B801C6C714B6D1A115A97904F3D1C957BC4C9') { throw 'Pinned post-OOBE save identity failed.' }
$tag = (& git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim()
$head = (& git -C $sdk rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $tag -cne 'v0.9.0.20' -or $head -cne $sdkCommit -or (git -C $sdk status --porcelain)) { throw 'M5-009 requires clean exact ReXGlue v0.9.0.20.' }

$evidence = Resolve-SafePath 'private/evidence/M5-009' 'Evidence root'
[IO.Directory]::CreateDirectory($evidence) | Out-Null
$runRoot = Join-Path $evidence ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
$cycleRoot = Join-Path $runRoot 'runs/01'
$user = Join-Path $cycleRoot 'user'
$cache = Join-Path $cycleRoot 'cache'
[IO.Directory]::CreateDirectory($user) | Out-Null
[IO.Directory]::CreateDirectory($cache) | Out-Null
Get-ChildItem -LiteralPath $seed -Force | Copy-Item -Destination $user -Recurse -Force
$sdkLog = Join-Path $runRoot 'sdk-install.log'
$testLog = Join-Path $runRoot 'sdk-audio-test.log'
$buildLog = Join-Path $runRoot 'app-clean-build.log'
$runtimeLog = Join-Path $cycleRoot 'mcla.log'

Write-Host 'M5-009 [1/6]: validating game, save, and exact SDK identity...' -ForegroundColor Cyan
$gameBefore = Get-GameIdentity $game
Write-Host 'M5-009 [2/6]: clean-building/installing ReXGlue and focused audio tests...' -ForegroundColor Cyan
$sdkBuildFailed = $false
Push-Location $sdk
try {
  if ((Invoke-Logged { & $cmake --preset win-amd64 -DREXGLUE_BUILD_TESTS=ON } $sdkLog) -ne 0 -or
      (Invoke-Logged { & $cmake --build out/build/win-amd64 --config RelWithDebInfo --target install --clean-first --parallel 8 } $sdkLog -Append) -ne 0 -or
      (Invoke-Logged { & $cmake --build out/build/win-amd64 --config RelWithDebInfo --target unit_tests --parallel 8 } $sdkLog -Append) -ne 0) { $sdkBuildFailed = $true }
} finally {
  Pop-Location
}
if ($sdkBuildFailed) { throw "SDK build/install failed. Private run: '$runRoot'." }
$unitExe = Resolve-SafePath 'third_party/rexglue-sdk/out/win-amd64/RelWithDebInfo/unit_tests.exe' 'Focused unit tests' -Exists
if ((Invoke-Logged { & $unitExe '[audio][route-audit],[audio][event-audit]' } $testLog) -ne 0) { throw "Focused SDK audio tests failed. Private run: '$runRoot'." }
$testText = [IO.File]::ReadAllText($testLog)
if ($testText -notmatch 'All tests passed \(30 assertions in 8 test cases\)') { throw 'Focused SDK audio-test totals changed.' }

Write-Host 'M5-009 [3/6]: clean-building the MCLA host...' -ForegroundColor Cyan
if ((Invoke-Logged { & $cmake --preset win-amd64-relwithdebinfo } $buildLog) -ne 0 -or (Invoke-Logged { & $cmake --build --preset win-amd64-relwithdebinfo --target mcla --clean-first --parallel 8 } $buildLog -Append) -ne 0) { throw "App clean build failed. Private run: '$runRoot'." }
$exe = Resolve-SafePath (Join-Path $build 'mcla.exe') 'Executable' -Exists
if (@(Get-ExactProcesses $exe).Count) { throw 'Canonical MCLA is already running.' }
$artifactsBefore = @(Get-Artifacts $build)

Write-Host 'M5-009 [4/6]: launching the six-class listening route...' -ForegroundColor Cyan
Write-Host 'Criterion: hear the named class during each window. Other simultaneous sounds are allowed.' -ForegroundColor Yellow
$arguments = @('--mcla_audio_event_probe=true', '--sdl_audio_event_audit=true', '--sdl_audio_route_audit=true', '--mcla_first_frame_settle_seconds=35', '--mcla_frontend_gameplay_wait_seconds=45', '--async_shader_compilation=false', '--d3d12_pipeline_creation_threads=0', '--log_max_file_size_mb=8', '--log_max_files=15', '--log_level=info', '--fullscreen=false', "--game_data_root=`"$game`"", "--user_data_root=`"$user`"", "--cache_root=`"$cache`"", "--log_file=`"$runtimeLog`"")
$process = $null
$forced = $false
$phaseNames = @('music', 'ambient', 'voice', 'engine', 'collision', 'ui')
$phaseInstructions = @('title/menu music', 'world ambience', 'character/radio voice', 'vehicle engine', 'impact/collision', 'pause/UI sound')
$seen = @{}
try {
  $process = Start-Process $exe -ArgumentList $arguments -WorkingDirectory $build -PassThru
  $deadline = [DateTime]::UtcNow.AddSeconds(360)
  $complete = $false
  while ([DateTime]::UtcNow -lt $deadline) {
    if ($process.HasExited) { throw "Process exited early with $($process.ExitCode)." }
    $text = Read-LiveLogs $cycleRoot
    for ($i = 0; $i -lt $phaseNames.Count; $i++) {
      $phase = $phaseNames[$i]
      if (-not $seen.ContainsKey($phase) -and $text.Contains("SDL_AUDIO_EVENT_AUDIT_PHASE v=1 event=BEGIN phase=$phase index=$i")) {
        $seen[$phase] = $true
        Write-Host ("LISTEN {0,-9} | {1,-28} | presence only; other sounds allowed" -f $phase.ToUpperInvariant(), $phaseInstructions[$i]) -ForegroundColor Magenta
      }
    }
    if ($text.Contains('MCLA_AUDIO_EVENT_SUMMARY v=1 status=COMPLETE phases=6') -and $text.Contains('SDL_AUDIO_AUDIT_SUMMARY v=1 phase=title status=PASS')) { $complete = $true; break }
    Start-Sleep -Milliseconds 250
  }
  if (-not $complete -or $seen.Count -ne 6) { throw 'Six-class audio route deadline expired or a listening window was missing.' }
  Write-Host 'All six machine-audited windows completed. Closing the game from outside, like a console title.' -ForegroundColor DarkCyan
  Close-ExactWindow $process
  if (-not $process.WaitForExit(10000) -or $process.ExitCode -ne 0) { throw 'Controlled external WM_CLOSE failed.' }
  if (@(Get-ExactProcesses $exe).Count) { throw 'MCLA process survived controlled close.' }
} catch {
  $failure = $_
  if ($null -ne $process -and -not $process.HasExited) { $forced = $true; Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue; $null = $process.WaitForExit(5000) }
  if ($forced) { throw "M5-009 failure required force cleanup. $($failure.Exception.Message) Private run: '$runRoot'." }
  throw
}

Write-Host 'M5-009 [5/6]: machine-verifying all event windows and current XMA/XAudio/SDL health...' -ForegroundColor Cyan
$probe = & $verifier -RunPath $cycleRoot
Write-Host 'Confirm only PRESENCE, not isolation or perfect mix. Type exactly:' -ForegroundColor Yellow
Write-Host 'PASS MUSIC AMBIENT VOICE ENGINE COLLISION UI' -ForegroundColor Green
$confirmation = Read-Host
if ($confirmation -cne 'PASS MUSIC AMBIENT VOICE ENGINE COLLISION UI') { throw "Listening confirmation was not accepted. Private run remains at '$runRoot'." }

$gameAfter = Get-GameIdentity $game
$artifactsAfter = @(Get-Artifacts $build)
if ((ConvertTo-Json $gameBefore -Compress -Depth 5) -cne (ConvertTo-Json $gameAfter -Compress -Depth 5) -or (ConvertTo-Json $artifactsBefore -Compress -Depth 4) -cne (ConvertTo-Json $artifactsAfter -Compress -Depth 4)) { throw 'Game or runtime artifacts drifted.' }
if ((Get-FileHash -LiteralPath (Join-Path $user 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav') -Algorithm SHA256).Hash -cne $seedSave -or (Get-FileHash -LiteralPath (Join-Path $user 'B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header') -Algorithm SHA256).Hash -cne $seedHeader) { throw 'Cycle save identity drifted.' }
$tree = Get-TreeSnapshot $runRoot
$record = [ordered]@{
  schema = 'mcla-audio-event-v1'; task = 'M5-009'; decision = 'six-class-audio-stream-presence-pass'; sdk_version = '0.9.0.20'; sdk_commit = $sdkCommit; build_configuration = 'RelWithDebInfo'
  listening_presence_only = $true; other_sounds_allowed = $true; operator_confirmation = $true; operator_categories = @('music', 'ambient', 'voice', 'engine', 'collision', 'ui')
  seed_save_sha256 = $seedSave; seed_header_sha256 = $seedHeader; game_before = $gameBefore; game_after = $gameAfter; artifacts_before = @($artifactsBefore); artifacts_after = @($artifactsAfter)
  build_logs = [ordered]@{'sdk-install_log' = (Get-FileHash -LiteralPath $sdkLog -Algorithm SHA256).Hash; 'sdk-audio-test_log' = (Get-FileHash -LiteralPath $testLog -Algorithm SHA256).Hash; 'app-clean-build_log' = (Get-FileHash -LiteralPath $buildLog -Algorithm SHA256).Hash}
  cycle = $probe; evidence_tree_sha256 = $tree.Hash; evidence_tree_file_count = $tree.FileCount; evidence_tree_directory_count = $tree.DirectoryCount; evidence_tree_bytes = $tree.Bytes
  scope = 'presence of six allowlisted audio event classes over the current XMA/XAudio/SDL route; isolation, mix balance, content fidelity, and exhaustive coverage are not claimed'
}
$result = Join-Path $runRoot 'result.json'
[IO.File]::WriteAllText($result, ((ConvertTo-Json $record -Depth 10) + [Environment]::NewLine), $utf8)
Write-Host 'M5-009 [6/6]: revalidating the persisted physical result...' -ForegroundColor Cyan
$verified = & $verifier -ResultPath $result
Write-Host "M5-009 PASS: six audio classes were machine-present and owner-heard. Result: '$result'." -ForegroundColor Green
$verified
