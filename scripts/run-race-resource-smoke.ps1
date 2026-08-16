[CmdletBinding()]
param(
  [string]$BuildRoot = 'out/build/win-amd64-release',
  [string]$GameRoot = 'private/game',
  [string]$InitialUserRoot = 'private/evidence/M5-012/20260816-132209-a316f851/runs/01/user',
  [switch]$CounterSelfTest,
  [string]$FinalizeRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdk = Join-Path $repo 'third_party/rexglue-sdk'
$verify = Join-Path $PSScriptRoot 'verify-race-resource-smoke.ps1'
$cmake = (& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath).CMakePath
$utf8 = [Text.UTF8Encoding]::new($false)
$sdkCommit = '3ef5b4f143d56b57e3c0e539cb0009ffe3a67e05'
$saveRelative = 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
$headerRelative = 'B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'
$saveSha256 = '711B94303445034A3B127F83E8143FF6DBA3FC96C2B6176F8720E49829AD5021'
$headerSha256 = '6DDB2A36CB2AF9FC76F80DBC7BCE9B17F71DDC153FABB47084086CB3FB0B1494'

if (-not ('MclaResourceWindow' -as [type])) {
  Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;
public static class MclaResourceWindow{delegate bool E(IntPtr h,IntPtr p);[DllImport("user32.dll")]static extern bool EnumWindows(E c,IntPtr p);[DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);[DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr h);[DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetWindowText(IntPtr h,StringBuilder s,int n);[DllImport("user32.dll")]static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);public static IntPtr[] Handles(int pid){var a=new List<IntPtr>();EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}public static string Title(IntPtr h){var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty;}public static bool Close(IntPtr h){return PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);}}
'@
}

function Resolve-Safe([string]$Path,[string]$Description,[switch]$Exists) {
  $full=if([IO.Path]::IsPathRooted($Path)){[IO.Path]::GetFullPath($Path)}else{[IO.Path]::GetFullPath((Join-Path $repo $Path))};$prefix=$repo.TrimEnd('\')+'\'
  if(-not$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description escapes repository."};$current=$repo
  foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object{$_.Length})){$current=Join-Path $current $part;if((Test-Path -LiteralPath $current)-and((Get-Item -LiteralPath $current -Force).Attributes-band[IO.FileAttributes]::ReparsePoint)){throw "$Description traverses a reparse point."}}
  if($Exists-and-not(Test-Path -LiteralPath $full)){throw "$Description is missing."};$full
}
function Invoke-Logged([scriptblock]$Command,[string]$Log,[switch]$Append){$prior=$ErrorActionPreference;$ErrorActionPreference='Continue';try{if($Append){&$Command *>&1|Tee-Object -FilePath $Log -Append|Out-Null}else{&$Command *>&1|Tee-Object -FilePath $Log|Out-Null};$LASTEXITCODE}finally{$ErrorActionPreference=$prior}}
function Read-LiveLogs([string]$Root){$text='';foreach($file in @(Get-ChildItem -LiteralPath $Root -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue)){try{$stream=[IO.File]::Open($file.FullName,'Open','Read','ReadWrite');try{$reader=[IO.StreamReader]::new($stream);$text+=$reader.ReadToEnd();$reader.Dispose()}finally{$stream.Dispose()}}catch{}};$text}
function Wait-Marker([Diagnostics.Process]$Process,[string]$Root,[string]$Marker,[int]$Seconds,[string]$Step){$deadline=[DateTime]::UtcNow.AddSeconds($Seconds);while([DateTime]::UtcNow-lt$deadline){if($Process.HasExited){throw "Process exited during '$Step'."};if((Read-LiveLogs $Root).Contains($Marker)){return};Start-Sleep -Milliseconds 200};throw "Timed out during '$Step'."}
function Close-ExactWindow([Diagnostics.Process]$Process){$matches=@();foreach($handle in [MclaResourceWindow]::Handles($Process.Id)){if([regex]::IsMatch([MclaResourceWindow]::Title($handle),'^mcla \[rexglue-v[^\]]+\]$')){$matches+=$handle}};if($matches.Count-ne1-or-not[MclaResourceWindow]::Close($matches[0])){throw 'Exact PID/window WM_CLOSE failed.'}}
function Get-Median([int64[]]$Values){$sorted=@($Values|Sort-Object);[int64]$sorted[[int][Math]::Floor($sorted.Count/2)]}
function Write-ResourceSamples([string]$Path,[object[]]$Samples){[IO.File]::WriteAllText($Path,(ConvertTo-Json -InputObject @($Samples) -Depth 4),$utf8)}
function Select-GpuProcessMemorySample([object[]]$Samples,[int]$ProcessId){
  $dedicated=0L;$shared=0L;$dedicatedCount=0;$sharedCount=0;$pattern='^pid_'+$ProcessId+'(_|$)'
  foreach($sample in $Samples){
    if($sample.InstanceName-notmatch$pattern-or[uint32]$sample.Status-ne0){continue}
    try{$value=[int64]$sample.CookedValue}catch{continue}
    if($sample.Path.EndsWith('\Dedicated Usage',[StringComparison]::OrdinalIgnoreCase)){$dedicated+=$value;$dedicatedCount++}
    elseif($sample.Path.EndsWith('\Shared Usage',[StringComparison]::OrdinalIgnoreCase)){$shared+=$value;$sharedCount++}
  }
  if($dedicatedCount-eq0-or$sharedCount-eq0){return $null}
  [pscustomobject]@{dedicated=[int64]$dedicated;shared=[int64]$shared;dedicated_samples=$dedicatedCount;shared_samples=$sharedCount}
}
function Get-GpuProcessMemorySample([Diagnostics.Process]$Process){
  $deadline=[DateTime]::UtcNow.AddSeconds(10);$lastIssue='no matching valid sample'
  while([DateTime]::UtcNow-lt$deadline){
    if($Process.HasExited){throw 'Process exited during GPU resource sampling.'}
    try{
      $counter=Get-Counter '\GPU Process Memory(*)\Dedicated Usage','\GPU Process Memory(*)\Shared Usage' -ErrorAction Stop
      $selected=Select-GpuProcessMemorySample @($counter.CounterSamples) $Process.Id
      if($null-ne$selected){return $selected}
      $lastIssue='the exact PID had no valid dedicated/shared pair'
    }catch{$lastIssue=$_.Exception.Message}
    Start-Sleep -Milliseconds 250
  }
  throw "GPU Process Memory counters did not provide a valid exact-PID dedicated/shared pair within 10 seconds: $lastIssue"
}
function Get-ResourceSample([Diagnostics.Process]$Process,[int]$Checkpoint){
  $observations=@()
  for($attempt=0;$attempt-lt3;$attempt++){
    if($Process.HasExited){throw 'Process exited during resource sampling.'};$Process.Refresh()
    $gpu=Get-GpuProcessMemorySample $Process
    $observations+=[pscustomobject]@{private=[int64]$Process.PrivateMemorySize64;working=[int64]$Process.WorkingSet64;handles=[int64]$Process.HandleCount;threads=[int64]$Process.Threads.Count;gpu_dedicated=[int64]$gpu.dedicated;gpu_shared=[int64]$gpu.shared};Start-Sleep -Milliseconds 500
  }
  [ordered]@{checkpoint=$Checkpoint;private_bytes=Get-Median @($observations.private);working_set_bytes=Get-Median @($observations.working);handle_count=Get-Median @($observations.handles);thread_count=Get-Median @($observations.threads);gpu_dedicated_bytes=Get-Median @($observations.gpu_dedicated);gpu_shared_bytes=Get-Median @($observations.gpu_shared)}
}
function Confirm-Race([Diagnostics.Process]$Process,[string]$CycleRoot,[string]$User,[int]$Index){
  $exact="RACE $Index COMPLETE";Write-Host ("RACE {0}/5           | finish one race event, reach a stable results/free-roam screen, then Alt-Tab"-f$Index) -ForegroundColor Yellow
  $answer=$null;while($answer-cne$exact){if($Process.HasExited){throw "Process exited before race $Index confirmation."};Write-Host "Type exactly: $exact" -ForegroundColor Green;$answer=Read-Host;if($answer-cne$exact){Write-Host 'Ignored; the game remains running.' -ForegroundColor DarkYellow}}
  Start-Sleep -Seconds 5;$request=Join-Path $User ".mcla-race-resource-$Index.request";[IO.File]::WriteAllText($request,'1',$utf8)
  Wait-Marker $Process $CycleRoot "MCLA_RACE_RESOURCE_FRAME v=1 checkpoint=$Index width=1280 height=720" 15 "race $Index capture";if(Test-Path -LiteralPath $request){throw "Race $Index request was not consumed."}
}
function Complete-Run([string]$RunRoot,[string]$Build){
  $probe=&$verify -RunPath $RunRoot -Fixture
  $artifacts=@('mcla.exe','rexruntime.dll','TracyClient.dll','rexgpu-xenos.dll')|ForEach-Object{$path=Join-Path $Build $_;[ordered]@{name=$_;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}}
  $record=[ordered]@{schema='mcla-race-resource-v1';task='M5-013';decision='five-race-bounded-resource-growth-pass';sdk_version='0.9.0.21';sdk_commit=$sdkCommit;build_configuration='Release';seed_save_sha256=$saveSha256;seed_header_sha256=$headerSha256;probe=$probe;release_artifacts=@($artifacts);scope='five operator-confirmed race completions in one Release process; bounded host private/working memory, handles, threads, and Windows GPU process-memory counters; the isolated working save may update; no claim of all-race or all-driver coverage'}
  $result=Join-Path $RunRoot 'result.json';[IO.File]::WriteAllText($result,(ConvertTo-Json $record -Depth 8),$utf8)
  &$verify -ResultPath $result
}

if($CounterSelfTest){
  $samples=@(
    [pscustomobject]@{InstanceName='pid_42_luid_0_phys_0';Status=[uint32]1;Path='\\host\GPU Process Memory(pid_42_luid_0_phys_0)\Dedicated Usage';CookedValue=[int64]999},
    [pscustomobject]@{InstanceName='pid_42_luid_0_phys_0';Status=[uint32]0;Path='\\host\GPU Process Memory(pid_42_luid_0_phys_0)\Dedicated Usage';CookedValue=[int64]100},
    [pscustomobject]@{InstanceName='pid_42_luid_1_phys_0';Status=[uint32]0;Path='\\host\GPU Process Memory(pid_42_luid_1_phys_0)\Dedicated Usage';CookedValue=[int64]20},
    [pscustomobject]@{InstanceName='pid_42_luid_0_phys_0';Status=[uint32]0;Path='\\host\GPU Process Memory(pid_42_luid_0_phys_0)\Shared Usage';CookedValue=[int64]30},
    [pscustomobject]@{InstanceName='pid_7_luid_0_phys_0';Status=[uint32]0;Path='\\host\GPU Process Memory(pid_7_luid_0_phys_0)\Shared Usage';CookedValue=[int64]777}
  )
  $valid=Select-GpuProcessMemorySample $samples 42;$invalid=Select-GpuProcessMemorySample @($samples|Where-Object{$_.Status-ne0}) 42;$wrong=Select-GpuProcessMemorySample $samples 99
  if($null-eq$valid-or$valid.dedicated-ne120-or$valid.shared-ne30-or$null-ne$invalid-or$null-ne$wrong){throw 'GPU counter selector self-test failed.'}
  return [pscustomobject]@{ValidAggregationVerified=$true;InvalidStatusIgnored=$true;WrongPidRejected=$true;RetryRequiredForIncompletePair=$true}
}
if($FinalizeRun){
  $runRoot=Resolve-Safe $FinalizeRun 'Finalize run' -Exists;$evidenceRoot=Resolve-Safe 'private/evidence/M5-013' 'M5-013 evidence root' -Exists
  if((Split-Path $runRoot -Parent)-cne$evidenceRoot-or(Split-Path $runRoot -Leaf)-notmatch '^20\d{6}-\d{6}-[0-9a-f]{8}$'){throw 'Finalize run must be one direct canonical M5-013 evidence child.'}
  $build=Resolve-Safe 'out/build/win-amd64-release' 'Canonical Release build' -Exists;$exe=Join-Path $build 'mcla.exe';if(@((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{$_.Path-ceq$exe}catch{$false}}).Count){throw 'Cannot finalize while canonical MCLA is running.'}
  Write-Host 'M5-013 finalize: revalidating the completed physical run without relaunch...' -ForegroundColor Cyan;$final=Complete-Run $runRoot $build
  Write-Host "M5-013 PASS: recovered the completed five-race result at '$runRoot\result.json'." -ForegroundColor Green;return $final
}

$build=Resolve-Safe $BuildRoot 'Build root' -Exists;$game=Resolve-Safe $GameRoot 'Game root' -Exists;$initial=Resolve-Safe $InitialUserRoot 'Completed user root' -Exists
if($build-cne(Resolve-Safe 'out/build/win-amd64-release' 'Canonical Release build' -Exists)-or$game-cne(Resolve-Safe 'private/game' 'Canonical game' -Exists)){throw 'M5-013 requires canonical Release/game roots.'}
if((Get-FileHash -LiteralPath (Join-Path $initial $saveRelative) -Algorithm SHA256).Hash-cne$saveSha256-or(Get-FileHash -LiteralPath (Join-Path $initial $headerRelative) -Algorithm SHA256).Hash-cne$headerSha256){throw 'M5-013 requires the exact completed M5-012 save and header.'}
$tag=(&git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim();$head=(&git -C $sdk rev-parse HEAD).Trim();if($LASTEXITCODE-ne0-or$tag-cne'v0.9.0.21'-or$head-cne$sdkCommit-or(git -C $sdk status --porcelain)){throw 'M5-013 requires clean exact ReXGlue v0.9.0.21.'}

$runRoot=Resolve-Safe ('private/evidence/M5-013/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)) 'Run root';[IO.Directory]::CreateDirectory($runRoot)|Out-Null
$buildLog=Join-Path $runRoot 'release-clean-build.log';Write-Host 'M5-013 [1/4]: clean-building the Release host...' -ForegroundColor Cyan
if((Invoke-Logged {&$cmake --preset win-amd64-release} $buildLog)-ne0-or(Invoke-Logged {&$cmake --build --preset win-amd64-release --target mcla --clean-first --parallel 8} $buildLog -Append)-ne0){throw "Release clean build failed. Private run: '$runRoot'."}
$exe=Resolve-Safe (Join-Path $build 'mcla.exe') 'Release executable' -Exists;if(@((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{$_.Path-ceq$exe}catch{$false}}).Count){throw 'Canonical Release MCLA is already running.'}
$cycleRoot=Join-Path $runRoot 'runs/01';$user=Join-Path $cycleRoot 'user';$cache=Join-Path $cycleRoot 'cache';[IO.Directory]::CreateDirectory($user)|Out-Null;[IO.Directory]::CreateDirectory($cache)|Out-Null;Copy-Item -LiteralPath (Join-Path $initial 'B13EBABEBABEBABE') -Destination $user -Recurse
$runtimeLog=Join-Path $cycleRoot 'mcla.log';$samplePath=Join-Path $runRoot 'resource-samples.json';$args=@('--mcla_race_resource_probe=true','--mcla_first_frame_settle_seconds=35','--input_backend=sdl','--mnk_mode=false','--async_shader_compilation=false','--d3d12_pipeline_creation_threads=0','--log_max_file_size_mb=8','--log_max_files=20','--log_level=info','--fullscreen=false',"--game_data_root=`"$game`"","--user_data_root=`"$user`"","--cache_root=`"$cache`"","--log_file=`"$runtimeLog`"")
Write-Host 'M5-013 [2/4]: launching one five-race resource session...' -ForegroundColor Cyan;$process=$null;$forced=$false;$samples=@()
try{
  $process=Start-Process $exe -ArgumentList $args -WorkingDirectory $build -PassThru;Wait-Marker $process $cycleRoot 'MCLA_RACE_RESOURCE_CONFIG v=1 checkpoints=5' 70 'title route'
  Write-Host 'TITLE              | PASS - enter saved gameplay and leave the controller neutral for the baseline.' -ForegroundColor Cyan;Start-Sleep -Seconds 5;$samples+=Get-ResourceSample $process 0;Write-ResourceSamples $samplePath $samples
  for($i=1;$i-le5;$i++){Confirm-Race $process $cycleRoot $user $i;$samples+=Get-ResourceSample $process $i;Write-ResourceSamples $samplePath $samples;Write-Host ("RACE {0}/5           | CAPTURED"-f$i) -ForegroundColor Cyan}
  Wait-Marker $process $cycleRoot 'MCLA_RACE_RESOURCE_SUMMARY v=1 status=PASS checkpoints=5 external_close_required=1' 10 'resource summary'
  Write-Host 'Closing the console-style title externally with WM_CLOSE...' -ForegroundColor DarkCyan;Close-ExactWindow $process;if(-not$process.WaitForExit(10000)-or$process.ExitCode-ne0){throw 'Controlled external WM_CLOSE failed.'}
}catch{$failure=$_;if($null-ne$process-and-not$process.HasExited){$forced=$true;Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue;$null=$process.WaitForExit(5000)};if($forced){throw "M5-013 failure required force cleanup. $($failure.Exception.Message) Private run: '$runRoot'."};throw}

Write-Host 'M5-013 [3/4]: verifying bounded growth and physical evidence...' -ForegroundColor Cyan;$final=Complete-Run $runRoot $build
Write-Host 'M5-013 [4/4]: persisted result revalidated.' -ForegroundColor Cyan
Write-Host "M5-013 PASS: five race checkpoints remained within bounded resource growth. Result: '$runRoot\result.json'." -ForegroundColor Green;$final
