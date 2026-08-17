[CmdletBinding()]
param(
  [string]$BuildRoot='out/build/win-amd64-release',
  [string]$GameRoot='private/game',
  [string]$InitialUserRoot='private/evidence/M5-013/20260817-013319-c2e7223f/runs/01/user',
  [string]$FinalizeRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify=Join-Path $PSScriptRoot 'verify-garage-lifecycle-smoke.ps1'
$cmake=(& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath).CMakePath
$utf8=[Text.UTF8Encoding]::new($false)
$sdkCommit='576b34fd233acf4579dd2375691dbe86fb4bf8e1'
$seedSaveSha='126F7482878C7AACB09AA6795331C906DFB9C4218BE94EDB1D8E51B27CA78AB2'
$seedHeaderSha='5827A913515AC0E5D55BB56AEC56DE99CACC0ABB7C8061F59336DF4CEA4A8731'
$seedSourceRunId='20260817-013319-c2e7223f'
$seedSourceLogSha='F6D9B7C09C730242B7D96C7F9FE3C6754A4FE726D22093A2BDAF089D2EABE04A'
$seedSourceCaptureSha='AAB64D0118D75F9EAECA50805DF3FB9EAC56B75726F4D2B97484C681509530A3'
$priorResultSha='D993E2612D1AC769D88264C83FD9C9186BC761E2067317F5A7EB66038C250E58'
$scope='one affordable unlocked second vehicle, one representative visual item, one performance item, one changed paint color, vehicle switching, free-roam return, and fresh-process persistence under explicit guest sign-in compatibility state 2; no real network service and no exhaustive vehicle, item, price, economy, garage, or campaign coverage'

if(-not('MclaGarageLifecycleNative'-as[type])){Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;
public static class MclaGarageLifecycleNative{
  delegate bool EnumProc(IntPtr h,IntPtr p);
  [DllImport("user32.dll")]static extern bool EnumWindows(EnumProc c,IntPtr p);[DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);[DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr h);[DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetWindowText(IntPtr h,StringBuilder s,int n);[DllImport("user32.dll")]static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
  public static IntPtr[] Handles(int pid){var a=new List<IntPtr>();EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}
  public static string Title(IntPtr h){var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty;}public static bool Close(IntPtr h){return PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);}
}
'@}

function Resolve-Safe([string]$Path,[string]$Description,[switch]$Exists){$full=if([IO.Path]::IsPathRooted($Path)){[IO.Path]::GetFullPath($Path)}else{[IO.Path]::GetFullPath((Join-Path $repo $Path))};$prefix=$repo.TrimEnd('\')+'\';if(-not$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description escapes repository."};$current=$repo;foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object{$_.Length})){$current=Join-Path $current $part;if((Test-Path -LiteralPath $current)-and((Get-Item -LiteralPath $current -Force).Attributes-band[IO.FileAttributes]::ReparsePoint)){throw "$Description traverses a reparse point."}};if($Exists-and-not(Test-Path -LiteralPath $full)){throw "$Description is missing."};$full}
function Invoke-Logged([scriptblock]$Command,[string]$Log,[switch]$Append){$prior=$ErrorActionPreference;$ErrorActionPreference='Continue';try{if($Append){&$Command *>&1|Tee-Object -FilePath $Log -Append|Out-Null}else{&$Command *>&1|Tee-Object -FilePath $Log|Out-Null};$LASTEXITCODE}finally{$ErrorActionPreference=$prior}}
function Read-LiveLogs([string]$Root){$text='';foreach($file in @(Get-ChildItem -LiteralPath $Root -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue)){try{$stream=[IO.File]::Open($file.FullName,'Open','Read','ReadWrite');try{$reader=[IO.StreamReader]::new($stream);$text+=$reader.ReadToEnd();$reader.Dispose()}finally{$stream.Dispose()}}catch{}};$text}
function Wait-Marker([Diagnostics.Process]$Process,[string]$Root,[string]$Marker,[int]$Seconds,[string]$Step){$deadline=[DateTime]::UtcNow.AddSeconds($Seconds);while([DateTime]::UtcNow-lt$deadline){if($Process.HasExited){throw "Process exited during '$Step'."};if((Read-LiveLogs $Root).Contains($Marker)){return};Start-Sleep -Milliseconds 200};throw "Timed out during '$Step'."}
function Close-ExactWindow([Diagnostics.Process]$Process){$matches=@();foreach($handle in [MclaGarageLifecycleNative]::Handles($Process.Id)){if([regex]::IsMatch([MclaGarageLifecycleNative]::Title($handle),'^mcla \[rexglue-v[^\]]+\]$')){$matches+=$handle}};if($matches.Count-ne1-or-not[MclaGarageLifecycleNative]::Close($matches[0])){throw 'Exact PID/window WM_CLOSE failed.'}}
function Get-Artifacts([string]$Build){@('mcla.exe','rexruntime.dll','TracyClient.dll','rexgpu-xenos.dll')|ForEach-Object{$path=Resolve-Safe (Join-Path $Build $_) "Artifact $_" -Exists;[ordered]@{name=$_;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}}}
function Write-Json([string]$Path,$Value){[IO.File]::WriteAllText($Path,(ConvertTo-Json $Value -Depth 14)+[Environment]::NewLine,$utf8)}
function Get-SavePaths([string]$UserRoot){[ordered]@{save=Join-Path $UserRoot 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav';header=Join-Path $UserRoot 'B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'}}
function Complete-Run([string]$Root,[string]$Build){
  $probe=&$verify -RunPath $Root;$record=[ordered]@{schema='mcla-garage-lifecycle-v1';task='M6-002';decision='representative-garage-purchase-customization-persistence-pass';sdk_version='0.9.0.22';sdk_commit=$sdkCommit;build_configuration='Release';route_id='representative-two-process-garage-v1';prior_race_restart_result_sha256=$priorResultSha;seed_source_run_id=$seedSourceRunId;seed_source_context='external-owner-identified-hangout-save-after-race-1';seed_source_log_sha256=$seedSourceLogSha;seed_source_capture_sha256=$seedSourceCaptureSha;seed_save_sha256=$seedSaveSha;seed_header_sha256=$seedHeaderSha;signin_compatibility_state=2;network_services_claimed=$false;probe=$probe;release_artifacts=@(Get-Artifacts $Build);scope=$scope}
  $result=Join-Path $Root 'result.json';Write-Json $result $record;&$verify -ResultPath $result
}
function Invoke-GarageCycle([int]$Cycle,$CycleConfig,[string]$CycleRoot,[string]$UserRoot,[string]$Game,[string]$Build,[string]$Exe,[string]$RunRoot){
  $cache=Join-Path $CycleRoot 'cache';[IO.Directory]::CreateDirectory($cache)|Out-Null;$runtimeLog=Join-Path $CycleRoot 'mcla.log';$process=$null;$forced=$false
  try{
    $args=@('--mcla_garage_lifecycle_probe=true',"--mcla_garage_lifecycle_cycle=$Cycle",'--xam_user_signin_state=2','--mcla_first_frame_settle_seconds=35','--input_backend=sdl','--mnk_mode=false','--async_shader_compilation=false','--d3d12_pipeline_creation_threads=0','--log_max_file_size_mb=8','--log_max_files=30','--log_level=info','--fullscreen=false',"--game_data_root=`"$Game`"","--user_data_root=`"$UserRoot`"","--cache_root=`"$cache`"","--log_file=`"$runtimeLog`"")
    $process=Start-Process $Exe -ArgumentList $args -WorkingDirectory $Build -PassThru;Wait-Marker $process $CycleRoot 'XAM_USER_SIGNIN_CONFIG v=1 state=2 mode=online-compatible' 20 "cycle $Cycle sign-in compatibility config";Wait-Marker $process $CycleRoot "MCLA_GARAGE_LIFECYCLE_CONFIG v=1 cycle=$Cycle phases=$($CycleConfig.phases.Count)" 90 "cycle $Cycle title/garage config"
    $state=[pscustomobject]@{sequence=0}
    $send={param([string]$Action);$state.sequence++;$sequence=$state.sequence;$request=Join-Path $UserRoot '.mcla-garage-control.request';$temporary=$request+'.tmp';[IO.File]::WriteAllText($temporary,"$sequence $Action",$utf8);Move-Item -LiteralPath $temporary -Destination $request -Force;$marker="MCLA_GARAGE_CONTROL v=1 sequence=$sequence action=$Action capture=0 width=0 height=0";Wait-Marker $process $CycleRoot $marker 45 "cycle $Cycle control $sequence $Action";if(Test-Path -LiteralPath $request){throw "Cycle $Cycle control $sequence request was not consumed."};Write-Host ("CYCLE {0}/2 CONTROL {1:D2} | {2} PASS" -f $Cycle,$sequence,$Action) -ForegroundColor DarkGreen}
    $capture={param([int]$Index);$phase=$CycleConfig.phases[$Index-1];$request=Join-Path $UserRoot ".mcla-garage-lifecycle-$($phase.id).request";[IO.File]::WriteAllText($request,'1',$utf8);$marker="MCLA_GARAGE_LIFECYCLE_FRAME v=1 cycle=$Cycle phase=$Index id=$($phase.id) width=1280 height=720";Wait-Marker $process $CycleRoot $marker 20 "cycle $Cycle phase $Index capture";if(Test-Path -LiteralPath $request){throw "Cycle $Cycle phase $Index request was not consumed."};Write-Host ("CYCLE {0}/2 PHASE {1}/{2} | {3} CAPTURED" -f $Cycle,$Index,$CycleConfig.phases.Count,$phase.id) -ForegroundColor Green}
    Write-Host ("CYCLE {0}/2 TITLE      | PASS - deterministic slot-0 automation is entering saved gameplay and the garage." -f $Cycle) -ForegroundColor Green
    &$send 'START';Start-Sleep -Seconds 25;&$send 'START';&$send 'DPAD_DOWN';&$send 'A';Start-Sleep -Seconds 15
    &$capture 1
    if($Cycle-eq1){
      &$send 'DPAD_DOWN';&$send 'DPAD_DOWN';&$send 'A';&$send 'DPAD_LEFT';&$send 'A';&$send 'A';Start-Sleep -Seconds 6;&$capture 2
      &$send 'DPAD_UP';&$send 'A';&$send 'A';&$send 'DPAD_RIGHT';&$send 'A';&$send 'A';Start-Sleep -Seconds 3
      &$send 'DPAD_DOWN';&$send 'A';&$send 'A';&$send 'DPAD_DOWN';&$send 'A';&$send 'A';&$send 'B';&$send 'A';Start-Sleep -Seconds 3;&$capture 3
      &$send 'DPAD_UP';&$send 'A';Start-Sleep -Seconds 3;&$capture 4
      &$send 'B';&$send 'DPAD_DOWN';&$send 'DPAD_DOWN';&$send 'DPAD_DOWN';&$send 'A';&$send 'A';&$send 'DPAD_DOWN';&$send 'DPAD_DOWN';&$send 'DPAD_DOWN';&$send 'A';&$send 'DPAD_RIGHT';&$send 'DPAD_RIGHT';&$send 'DPAD_RIGHT';&$send 'DPAD_RIGHT';&$send 'A';&$send 'B';Start-Sleep -Seconds 3;&$capture 5
      &$send 'B';&$send 'DPAD_UP';&$send 'A';&$send 'DPAD_UP';&$send 'A';Start-Sleep -Seconds 6;&$capture 6
      &$send 'DPAD_DOWN';&$send 'DPAD_DOWN';&$send 'DPAD_DOWN';&$send 'A';&$send 'A';Start-Sleep -Seconds 15;&$send 'A';Start-Sleep -Seconds 3;&$capture 7
    }else{
      &$send 'A';&$capture 2
      &$send 'DPAD_DOWN';&$send 'A';Start-Sleep -Seconds 6;&$capture 3
      &$send 'A';&$send 'DPAD_UP';&$send 'A';Start-Sleep -Seconds 6;&$capture 4
      &$send 'DPAD_DOWN';&$send 'DPAD_DOWN';&$send 'DPAD_DOWN';&$send 'A';&$send 'A';Start-Sleep -Seconds 15;&$send 'A';Start-Sleep -Seconds 3;&$capture 5
    }
    Wait-Marker $process $CycleRoot "MCLA_GARAGE_LIFECYCLE_SUMMARY v=1 status=PASS cycle=$Cycle phases=$($CycleConfig.phases.Count)" 10 "cycle $Cycle garage summary";Start-Sleep -Seconds 8;Write-Host "CYCLE $Cycle/2           | closing externally with WM_CLOSE..." -ForegroundColor DarkCyan;Close-ExactWindow $process;if(-not$process.WaitForExit(10000)-or$process.ExitCode-ne0){throw "Cycle $Cycle controlled external WM_CLOSE failed."}
  }catch{$failure=$_;if($null-ne$process-and-not$process.HasExited){$forced=$true;Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue;$null=$process.WaitForExit(5000)};if($forced){throw "M6-002 cycle $Cycle failure required force cleanup. $($failure.Exception.Message) Private run: '$RunRoot'."};throw}
}

$build=Resolve-Safe $BuildRoot 'Build root' -Exists
if($FinalizeRun){$root=Resolve-Safe $FinalizeRun 'M6-002 run root' -Exists;$final=Complete-Run $root $build;Write-Host "M6-002 recovered PASS: '$root\result.json'." -ForegroundColor Green;$final;return}
$game=Resolve-Safe $GameRoot 'Game root' -Exists;$initial=Resolve-Safe $InitialUserRoot 'Initial user root' -Exists
if($build-cne(Resolve-Safe 'out/build/win-amd64-release' 'Canonical Release build')-or$game-cne(Resolve-Safe 'private/game' 'Canonical game')-or$initial-cne(Resolve-Safe 'private/evidence/M5-013/20260817-013319-c2e7223f/runs/01/user' 'Canonical HANGOUT save root')){throw 'M6-002 requires canonical build, game, and HANGOUT save inputs.'}
$seedPaths=Get-SavePaths $initial;foreach($path in @($seedPaths.save,$seedPaths.header)){Resolve-Safe $path 'Completed seed file' -Exists|Out-Null};if((Get-FileHash $seedPaths.save -Algorithm SHA256).Hash-cne$seedSaveSha-or(Get-FileHash $seedPaths.header -Algorithm SHA256).Hash-cne$seedHeaderSha){throw 'Completed save identity drifted.'}
$seedSourceRoot=Resolve-Safe "private/evidence/M5-013/$seedSourceRunId" 'HANGOUT seed source run' -Exists;$seedSourceLog=Resolve-Safe (Join-Path $seedSourceRoot 'runs/01/mcla.log') 'HANGOUT seed source log' -Exists;$seedSourceCapture=Resolve-Safe (Join-Path $seedSourceRoot 'runs/01/user/mcla-race-resource-1.bmp') 'HANGOUT seed source capture' -Exists;if((Get-FileHash $seedSourceLog -Algorithm SHA256).Hash-cne$seedSourceLogSha-or(Get-FileHash $seedSourceCapture -Algorithm SHA256).Hash-cne$seedSourceCaptureSha){throw 'HANGOUT seed source evidence drifted.'};$seedSourceText=[IO.File]::ReadAllText($seedSourceLog);if(([regex]::Matches($seedSourceText,'MCLA_RACE_RESOURCE_FRAME v=1 checkpoint=1 .* status=PASS').Count-ne1)-or([regex]::Matches($seedSourceText,'\[FATAL\] Call to invalid or unregistered function at guest address 0x8220B810').Count-ne1)){throw 'HANGOUT seed source classification drifted.'}
$prior=Resolve-Safe 'private/evidence/M5-012/20260817-001225-ade395f8/result.json' 'M5-012 restart prerequisite' -Exists;if((Get-FileHash $prior -Algorithm SHA256).Hash-cne$priorResultSha){throw 'M5-012 restart prerequisite drifted.'}
$route=Get-Content -LiteralPath (Resolve-Safe 'config/garage-lifecycle-route.json' 'Garage route config' -Exists)-Raw|ConvertFrom-Json
$evidence=Resolve-Safe 'private/evidence/M6-002' 'M6-002 evidence root';[IO.Directory]::CreateDirectory($evidence)|Out-Null;$runRoot=Join-Path $evidence ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8));[IO.Directory]::CreateDirectory($runRoot)|Out-Null;$buildLog=Join-Path $runRoot 'release-clean-build.log'
Write-Host 'M6-002 [1/6]: validating the completed save, garage route, and restart prerequisite...' -ForegroundColor Cyan;$gameBefore=& (Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath $game -VerifyHashes
Write-Host 'M6-002 [2/6]: clean-building the Release host...' -ForegroundColor Cyan;if(Invoke-Logged {&$cmake --preset win-amd64-release} $buildLog){throw 'Release configure failed.'};if(Invoke-Logged {&$cmake --build --preset win-amd64-release --target mcla --clean-first --parallel 8} $buildLog -Append){throw "Release clean build failed. Private run: '$runRoot'."}
$exe=Resolve-Safe (Join-Path $build 'mcla.exe') 'Release executable' -Exists;if(@((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{$_.Path-ceq$exe}catch{$false}}).Count){throw 'Canonical Release MCLA is already running.'};$artifactsBefore=@(Get-Artifacts $build)
$cycle1=Join-Path $runRoot 'runs/01';$user1=Join-Path $cycle1 'user';[IO.Directory]::CreateDirectory($user1)|Out-Null;Copy-Item -LiteralPath (Join-Path $initial 'B13EBABEBABEBABE') -Destination $user1 -Recurse -Force
Write-Host 'M6-002 [3/6]: running purchase/customization cycle 1...' -ForegroundColor Cyan;Invoke-GarageCycle 1 $route.cycles[0] $cycle1 $user1 $game $build $exe $runRoot
$paths1=Get-SavePaths $user1;if((Get-Item $paths1.save).Length-ne537428-or(Get-Item $paths1.header).Length-ne328){throw 'Cycle-1 save/header shape changed unexpectedly.'};$cycle1Save=(Get-FileHash $paths1.save -Algorithm SHA256).Hash;$cycle1Header=(Get-FileHash $paths1.header -Algorithm SHA256).Hash;if($cycle1Save-ceq$seedSaveSha){throw "Cycle 1 completed without changing the save; purchases were not durably recorded. Private run: '$runRoot'."}
$cycle2=Join-Path $runRoot 'runs/02';$user2=Join-Path $cycle2 'user';[IO.Directory]::CreateDirectory($user2)|Out-Null;Copy-Item -LiteralPath (Join-Path $user1 'B13EBABEBABEBABE') -Destination $user2 -Recurse -Force;$paths2=Get-SavePaths $user2;$cycle2PreSave=(Get-FileHash $paths2.save -Algorithm SHA256).Hash;$cycle2PreHeader=(Get-FileHash $paths2.header -Algorithm SHA256).Hash;if($cycle2PreSave-cne$cycle1Save-or$cycle2PreHeader-cne$cycle1Header){throw 'Cycle-2 handoff does not exactly match cycle-1 output.'}
Write-Host 'M6-002 [4/6]: running fresh-process persistence cycle 2...' -ForegroundColor Cyan;Invoke-GarageCycle 2 $route.cycles[1] $cycle2 $user2 $game $build $exe $runRoot
if((Get-Item $paths2.save).Length-ne537428-or(Get-Item $paths2.header).Length-ne328){throw 'Cycle-2 save/header shape changed unexpectedly.'};$cycle2PostSave=(Get-FileHash $paths2.save -Algorithm SHA256).Hash;$cycle2PostHeader=(Get-FileHash $paths2.header -Algorithm SHA256).Hash
$chain=[ordered]@{schema='mcla-garage-save-chain-v1';seed_save_sha256=$seedSaveSha;seed_header_sha256=$seedHeaderSha;cycle1_post_save_sha256=$cycle1Save;cycle1_post_header_sha256=$cycle1Header;cycle2_pre_save_sha256=$cycle2PreSave;cycle2_pre_header_sha256=$cycle2PreHeader;cycle2_post_save_sha256=$cycle2PostSave;cycle2_post_header_sha256=$cycle2PostHeader;save_bytes=537428;header_bytes=328;cycle1_save_changed=$true;handoff_exact=$true};Write-Json (Join-Path $runRoot 'save-chain.json') $chain
Write-Host 'M6-002 [5/6]: verifying two-process chronology, captures, save handoff, and persistence...' -ForegroundColor Cyan;$gameAfter=& (Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath $game -VerifyHashes;$artifactsAfter=@(Get-Artifacts $build);if(($gameBefore|ConvertTo-Json -Compress)-cne($gameAfter|ConvertTo-Json -Compress)-or($artifactsBefore|ConvertTo-Json -Compress)-cne($artifactsAfter|ConvertTo-Json -Compress)){throw 'Source-game or Release artifact identity changed.'};$final=Complete-Run $runRoot $build
Write-Host 'M6-002 [6/6]: persisted result revalidated.' -ForegroundColor Cyan;Write-Host "M6-002 PASS: representative garage purchases, switching, and fresh-process persistence verified. Result: '$runRoot\result.json'." -ForegroundColor Green;$final
