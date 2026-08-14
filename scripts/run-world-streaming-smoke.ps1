[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-world-streaming-smoke.ps1'
$gameVerify = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$cmake = $toolchain.CMakePath
$utf8 = [Text.UTF8Encoding]::new($false)

if (-not ('MclaWorldStreamingWindow' -as [type])) { Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;public static class MclaWorldStreamingWindow{delegate bool E(IntPtr h,IntPtr p);[DllImport("user32.dll")]static extern bool EnumWindows(E c,IntPtr p);[DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);[DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr h);[DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetWindowText(IntPtr h,StringBuilder s,int n);[DllImport("user32.dll")]static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);public static IntPtr[] Handles(int pid){var a=new List<IntPtr>();EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}public static string Title(IntPtr h){var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty;}public static bool Close(IntPtr h){return PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);}}
'@ }

function Safe([string]$Path,[string]$Description,[switch]$Exists) { $full=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($Path)){$Path}else{Join-Path $repo $Path}));$prefix=$repo.TrimEnd('\')+'\';if(-not $full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description escapes repository."};$current=$repo;foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object Length)){$current=Join-Path $current $part;if((Test-Path $current)-and((Get-Item $current -Force).Attributes-band[IO.FileAttributes]::ReparsePoint)){throw "$Description traverses a reparse point."}};if($Exists-and-not(Test-Path $full)){throw "$Description is missing."};$full }
function NoReparse([string]$Root) { $pending=[Collections.Generic.Stack[string]]::new();$pending.Push($Root);while($pending.Count){foreach($item in @(Get-ChildItem -LiteralPath $pending.Pop() -Force)){if($item.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Tree contains a reparse point.'};if($item.PSIsContainer){$pending.Push($item.FullName)}}} }
function Tree([string]$Root) { $root=Safe $Root 'Tree' -Exists;NoReparse $root;$items=@(Get-ChildItem $root -Recurse -Force);$files=@($items|Where-Object{-not $_.PSIsContainer}|Sort-Object FullName);$entries=@();foreach($directory in @($items|Where-Object PSIsContainer|Sort-Object FullName)){$entries+=[ordered]@{kind='directory';path=$directory.FullName.Substring($root.Length).TrimStart('\').Replace('\','/')}};foreach($file in $files){$entries+=[ordered]@{kind='file';path=$file.FullName.Substring($root.Length).TrimStart('\').Replace('\','/');length=$file.Length;sha256=(Get-FileHash $file.FullName -Algorithm SHA256).Hash}};$json=ConvertTo-Json -InputObject @($entries) -Compress -Depth 4;$sha=[Security.Cryptography.SHA256]::Create();try{$hash=-join($sha.ComputeHash($utf8.GetBytes($json))|ForEach-Object{$_.ToString('X2')})}finally{$sha.Dispose()};$bytes=0L;foreach($file in $files){$bytes+=$file.Length};[pscustomobject]@{Hash=$hash;FileCount=$files.Count;DirectoryCount=@($items|Where-Object PSIsContainer).Count;Bytes=$bytes} }
function Game([string]$Root) { $tree=Tree $Root;$v=&$gameVerify -GamePath $Root -VerifyHashes;[ordered]@{file_count=$v.FileCount;payload_bytes=$v.PayloadBytes;manifest_sha256=(Get-FileHash $v.ManifestPath -Algorithm SHA256).Hash;tree_sha256=$tree.Hash;tree_file_count=$tree.FileCount;tree_directory_count=$tree.DirectoryCount;tree_bytes=$tree.Bytes} }
function Artifacts([string]$Root) { @('mcla.exe','rexruntimerd.dll','TracyClientrd.dll','rexgpu-xenosrd.dll')|ForEach-Object{[ordered]@{name=$_;sha256=(Get-FileHash (Safe (Join-Path $Root $_) "Artifact $_" -Exists)-Algorithm SHA256).Hash}} }
function Processes([string]$Exe) { @((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{[string]::Equals($_.Path,$Exe,[StringComparison]::OrdinalIgnoreCase)}catch{$false}}) }
function Contains([string]$Directory,[string]$Needle) { foreach($file in @(Get-ChildItem $Directory -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue)){try{$stream=[IO.File]::Open($file.FullName,'Open','Read','ReadWrite');try{$reader=[IO.StreamReader]::new($stream);$text=$reader.ReadToEnd();$reader.Dispose()}finally{$stream.Dispose()};if($text.Contains($Needle)){return $true}}catch{}};$false }
function Close-Exact([Diagnostics.Process]$Process) { $matches=@();foreach($handle in [MclaWorldStreamingWindow]::Handles($Process.Id)){if([regex]::IsMatch([MclaWorldStreamingWindow]::Title($handle),'^mcla \[rexglue-v[^\]]+\]$')){$matches+=$handle}};if($matches.Count-ne 1-or-not[MclaWorldStreamingWindow]::Close($matches[0])){throw 'Exact PID/window WM_CLOSE failed.'} }
function Logged([scriptblock]$Command,[string]$Log,[switch]$Append) { $prior=$ErrorActionPreference;$ErrorActionPreference='Continue';try{if($Append){&$Command *>&1|Tee-Object $Log -Append|Out-Null}else{&$Command *>&1|Tee-Object $Log|Out-Null};$code=$LASTEXITCODE}finally{$ErrorActionPreference=$prior};$code }

$build=Safe $BuildRoot 'Build root' -Exists;$game=Safe $GameRoot 'Game root' -Exists
if($build-cne(Safe 'out/build/win-amd64-relwithdebinfo' 'Canonical build')-or$game-cne(Safe 'private/game' 'Canonical game')){throw 'M5-002 requires canonical inputs.'}
$seed=Safe 'private/baseline/M4-011/post-oobe-profile' 'Pinned post-OOBE seed' -Exists;$seedBefore=Tree $seed
if($seedBefore.FileCount-ne 2-or(Get-FileHash (Join-Path $seed 'B13EBABEBABEBABE\545407F8\00000001\mc4.sav\mc4.sav')-Algorithm SHA256).Hash-cne'E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68'){throw 'Pinned saved-game seed failed identity.'}
$evidence=Safe 'private/evidence/M5-002' 'Evidence root';[IO.Directory]::CreateDirectory($evidence)|Out-Null
$root=Join-Path $evidence ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8));[IO.Directory]::CreateDirectory($root)|Out-Null
$testLog=Join-Path $root 'sdk-vfs-test.log';$buildLog=Join-Path $root 'relwithdebinfo-clean-build.log'

Write-Host 'M5-002 [1/7]: validating the pinned saved-game route and source-game integrity...' -ForegroundColor Cyan
$gameBefore=Game $game
Write-Host 'M5-002 [2/7]: running focused VFS tests...' -ForegroundColor Cyan
$unit=Safe 'third_party/rexglue-sdk/out/win-amd64/RelWithDebInfo/unit_tests.exe' 'SDK unit tests' -Exists
if((Logged {&$unit '[filesystem][vfs]' --order declared} $testLog)-or([IO.File]::ReadAllText($testLog)-notmatch'All tests passed \(33 assertions in 2 test cases\)')){throw 'Focused VFS tests failed or totals changed.'}
Write-Host 'M5-002 [3/7]: clean-building the MCLA host...' -ForegroundColor Cyan
if(Logged {&$cmake --preset win-amd64-relwithdebinfo} $buildLog){throw 'App configure failed.'};if(Logged {&$cmake --build --preset win-amd64-relwithdebinfo --target mcla --clean-first --parallel 8} $buildLog -Append){throw "App clean build failed. Private run: '$root'."}
$exe=Safe (Join-Path $build 'mcla.exe') 'Executable' -Exists;if(@(Processes $exe).Count){throw 'Canonical MCLA is already running.'};$artifactsBefore=@(Artifacts $build)

Write-Host 'M5-002 [4/7]: launching saved gameplay with private noisy RPF I/O tracing...' -ForegroundColor Cyan
$cycle=Join-Path $root 'runs\01';$user=Join-Path $cycle 'user';$cache=Join-Path $cycle 'cache';[IO.Directory]::CreateDirectory($user)|Out-Null;[IO.Directory]::CreateDirectory($cache)|Out-Null;Get-ChildItem $seed -Force|Copy-Item -Destination $user -Recurse -Force
if((Tree $user).Hash-cne$seedBefore.Hash){throw 'Cycle did not start from the exact pinned save.'}
$log=Join-Path $cycle 'mcla.log';$process=$null;$forced=$false
try {
    $args=@('--mcla_frontend_smoke_probe=true','--mcla_first_frame_settle_seconds=45','--mcla_frontend_gameplay_wait_seconds=45','--mcla_frontend_pause_wait_seconds=4','--gpu_render_audit=true','--async_shader_compilation=false','--render_target_path_d3d12=rtv','--log_noisy=true','--log_max_file_size_mb=8','--log_max_files=15','--log_level=trace','--fullscreen=false',"--game_data_root=$game","--user_data_root=$user","--cache_root=$cache","--log_file=$log")
    $process=Start-Process $exe -ArgumentList $args -WorkingDirectory $build -PassThru
    $deadline=[DateTime]::UtcNow.AddSeconds(155)
    while([DateTime]::UtcNow-lt$deadline){if($process.HasExited){throw "Process exited before saved gameplay/frontend PASS (exit $($process.ExitCode)). Private run: '$root'."};if((Test-Path (Join-Path $user 'mcla-frontend-options.bmp'))-and(Contains $cycle 'MCLA_FRONTEND_SMOKE_SUMMARY v=1 status=PASS')){break};Start-Sleep -Milliseconds 250}
    if(-not(Test-Path (Join-Path $user 'mcla-frontend-options.bmp'))-or-not(Contains $cycle 'MCLA_FRONTEND_SMOKE_SUMMARY v=1 status=PASS')){throw "Saved-gameplay/frontend deadline expired. Private run: '$root'."}
    Close-Exact $process;if(-not$process.WaitForExit(10000)-or$process.ExitCode-ne 0){throw 'Controlled external WM_CLOSE failed.'};if(@(Processes $exe).Count){throw 'Exact-path MCLA process survived close.'}
} catch { $failure=$_;if($process-and-not$process.HasExited){$forced=$true;Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue;$null=$process.WaitForExit(5000)};if($forced){throw "M5-002 failure required force cleanup. $($failure.Exception.Message) Private run: '$root'."};throw }

Write-Host 'M5-002 [5/7]: verifying physical RPF reads, path-case behavior, and expected retail misses...' -ForegroundColor Cyan
$probe=&$verify -ProbeOnly -RuntimeLogPath $log -UserRoot $user
$captures=[ordered]@{};foreach($phase in @('title','gameplay','pause','options')){$bmp=$probe.Frontend.Bmps[$phase];$captures[$phase]=[ordered]@{sha256=$bmp.Sha256;bytes=$bmp.Bytes}}
Write-Host 'M5-002 [6/7]: checking immutable source/runtime/seed state...' -ForegroundColor Cyan
$gameAfter=Game $game;$artifactsAfter=@(Artifacts $build);$seedAfter=Tree $seed
if(($gameBefore|ConvertTo-Json -Compress)-cne($gameAfter|ConvertTo-Json -Compress)-or($artifactsBefore|ConvertTo-Json -Compress)-cne($artifactsAfter|ConvertTo-Json -Compress)-or$seedBefore.Hash-cne$seedAfter.Hash){throw 'Source-game, runtime-artifact, or pinned-seed identity changed.'}
$result=[ordered]@{schema=1;task='M5-002';decision='world-rpf-streaming-pass';sdk_version='0.9.0.18';route_id='pinned-save-sunset-strip-race-v1:free-roam-prerequisite';cycle_count=1;build=[ordered]@{focused_test_cases=2;focused_test_assertions=33;focused_test_log_sha256=(Get-FileHash $testLog -Algorithm SHA256).Hash;app_build_log_sha256=(Get-FileHash $buildLog -Algorithm SHA256).Hash;executable_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash};seed=[ordered]@{before_sha256=$seedBefore.Hash;after_sha256=$seedAfter.Hash;file_count=$seedBefore.FileCount};game_identity=[ordered]@{before=$gameBefore;after=$gameAfter};artifacts=[ordered]@{before=$artifactsBefore;after=$artifactsAfter};cycle=[ordered]@{relative_root='runs/01';exit_code=0;close_requested=$true;harness_force_cleanup=$false;runtime_logs=@($probe.LogSet.Files);runtime_log_set_sha256=$probe.LogSet.Hash;runtime_log_file_count=$probe.LogSet.Count;runtime_log_bytes=$probe.LogSet.Bytes;cache_read_calls=$probe.Cache.ReadCalls;cache_read_bytes=$probe.Cache.ReadBytes;cache_highest_end=$probe.Cache.HighestEnd;audlo_read_calls=$probe.Audlo.ReadCalls;audlo_read_bytes=$probe.Audlo.ReadBytes;audlo_highest_end=$probe.Audlo.HighestEnd;expected_development_misses=7;captures=$captures};no_surviving_processes=$true;data_integrity_preserved=$true}
$resultPath=Join-Path $root 'result.json';[IO.File]::WriteAllText($resultPath,(ConvertTo-Json $result -Depth 12)+[Environment]::NewLine,$utf8)
Write-Host 'M5-002 [7/7]: final physical/result verification...' -ForegroundColor Cyan
try{$final=&$verify -ResultPath $resultPath}catch{Remove-Item $resultPath -Force -ErrorAction SilentlyContinue;throw}
[pscustomobject]@{Passed=$final.Passed;Decision=$final.Decision;CacheReadCalls=$final.CacheReadCalls;CacheReadBytes=$final.CacheReadBytes;AudloReadCalls=$final.AudloReadCalls;ExpectedDevelopmentMisses=$final.ExpectedDevelopmentMisses;MixedCaseVerified=$final.MixedCaseVerified;GameplayVerified=$final.GameplayVerified;DataIntegrityVerified=$final.DataIntegrityVerified;PrivateRunRoot=$root;ResultPath=$resultPath}
