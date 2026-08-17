[CmdletBinding()]
param(
  [string]$BuildRoot='out/build/win-amd64-release',
  [string]$GameRoot='private/game',
  [string]$InitialUserRoot='private/evidence/M5-012/20260816-132209-a316f851/runs/01/user',
  [switch]$CounterSelfTest,
  [string]$FinalizeRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify=Join-Path $PSScriptRoot 'verify-city-streaming-smoke.ps1'
$cmake=(& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath).CMakePath
$utf8=[Text.UTF8Encoding]::new($false)
$sdkCommit='3ef5b4f143d56b57e3c0e539cb0009ffe3a67e05'
$seedSaveSha='711B94303445034A3B127F83E8143FF6DBA3FC96C2B6176F8720E49829AD5021'
$seedHeaderSha='6DDB2A36CB2AF9FC76F80DBC7BCE9B17F71DDC153FABB47084086CB3FB0B1494'
$priorResultSha='A582A41BB254D9187BC6F8147E89E2FF2E92D0EA4B79945760BA6FBCF334BC28'

if(-not('MclaCityStreamingNative'-as[type])){Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Diagnostics;using System.Runtime.InteropServices;using System.Text;
public static class MclaCityStreamingNative{
  delegate bool EnumProc(IntPtr h,IntPtr p);
  [StructLayout(LayoutKind.Sequential)]public struct IoCounters{public ulong ReadOperationCount,WriteOperationCount,OtherOperationCount,ReadTransferCount,WriteTransferCount,OtherTransferCount;}
  [DllImport("kernel32.dll",SetLastError=true)]public static extern bool GetProcessIoCounters(IntPtr h,out IoCounters counters);
  [DllImport("user32.dll")]static extern bool EnumWindows(EnumProc c,IntPtr p);[DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);[DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr h);[DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetWindowText(IntPtr h,StringBuilder s,int n);[DllImport("user32.dll")]static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
  public static IntPtr[] Handles(int pid){var a=new List<IntPtr>();EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}
  public static string Title(IntPtr h){var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty;}public static bool Close(IntPtr h){return PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);}
}
'@}

function Resolve-Safe([string]$Path,[string]$Description,[switch]$Exists){$full=if([IO.Path]::IsPathRooted($Path)){[IO.Path]::GetFullPath($Path)}else{[IO.Path]::GetFullPath((Join-Path $repo $Path))};$prefix=$repo.TrimEnd('\')+'\';if(-not$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description escapes repository."};$current=$repo;foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object{$_.Length})){$current=Join-Path $current $part;if((Test-Path -LiteralPath $current)-and((Get-Item -LiteralPath $current -Force).Attributes-band[IO.FileAttributes]::ReparsePoint)){throw "$Description traverses a reparse point."}};if($Exists-and-not(Test-Path -LiteralPath $full)){throw "$Description is missing."};$full}
function Invoke-Logged([scriptblock]$Command,[string]$Log,[switch]$Append){$prior=$ErrorActionPreference;$ErrorActionPreference='Continue';try{if($Append){&$Command *>&1|Tee-Object -FilePath $Log -Append|Out-Null}else{&$Command *>&1|Tee-Object -FilePath $Log|Out-Null};$LASTEXITCODE}finally{$ErrorActionPreference=$prior}}
function Read-LiveLogs([string]$Root){$text='';foreach($file in @(Get-ChildItem -LiteralPath $Root -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue)){try{$stream=[IO.File]::Open($file.FullName,'Open','Read','ReadWrite');try{$reader=[IO.StreamReader]::new($stream);$text+=$reader.ReadToEnd();$reader.Dispose()}finally{$stream.Dispose()}}catch{}};$text}
function Wait-Marker([Diagnostics.Process]$Process,[string]$Root,[string]$Marker,[int]$Seconds,[string]$Step){$deadline=[DateTime]::UtcNow.AddSeconds($Seconds);while([DateTime]::UtcNow-lt$deadline){if($Process.HasExited){throw "Process exited during '$Step'."};if((Read-LiveLogs $Root).Contains($Marker)){return};Start-Sleep -Milliseconds 200};throw "Timed out during '$Step'."}
function Close-ExactWindow([Diagnostics.Process]$Process){$matches=@();foreach($handle in [MclaCityStreamingNative]::Handles($Process.Id)){if([regex]::IsMatch([MclaCityStreamingNative]::Title($handle),'^mcla \[rexglue-v[^\]]+\]$')){$matches+=$handle}};if($matches.Count-ne1-or-not[MclaCityStreamingNative]::Close($matches[0])){throw 'Exact PID/window WM_CLOSE failed.'}}
function Get-ResourceSample([Diagnostics.Process]$Process,[int]$Checkpoint,[string]$Id){$Process.Refresh();$io=[MclaCityStreamingNative+IoCounters]::new();if(-not[MclaCityStreamingNative]::GetProcessIoCounters($Process.Handle,[ref]$io)){throw 'GetProcessIoCounters failed.'};[ordered]@{checkpoint=$Checkpoint;id=$Id;private_bytes=[int64]$Process.PrivateMemorySize64;working_set_bytes=[int64]$Process.WorkingSet64;handle_count=[int64]$Process.HandleCount;thread_count=[int64]$Process.Threads.Count;io_read_bytes=[int64]$io.ReadTransferCount}}
function Write-Samples([string]$Path,$Samples){[IO.File]::WriteAllText($Path,(ConvertTo-Json @($Samples)-Depth 5)+[Environment]::NewLine,$utf8)}
function Get-Artifacts([string]$Build){@('mcla.exe','rexruntime.dll','TracyClient.dll','rexgpu-xenos.dll')|ForEach-Object{$path=Resolve-Safe (Join-Path $Build $_) "Artifact $_" -Exists;[ordered]@{name=$_;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}}}
function Complete-Run([string]$Root,[string]$Build){
  $probe=&$verify -RunPath $Root;$record=[ordered]@{schema='mcla-city-streaming-v1';task='M6-001';decision='all-major-city-regions-streaming-pass';sdk_version='0.9.0.21';sdk_commit=$sdkCommit;build_configuration='Release';route_id='major-city-regions-v1';prior_world_streaming_result_sha256=$priorResultSha;seed_save_sha256=$seedSaveSha;seed_header_sha256=$seedHeaderSha;probe=$probe;release_artifacts=@(Get-Artifacts $Build);scope='eight project-defined geographic coverage zones plus return-to-start in one operator-driven Release process; GPS checkpoint captures and bounded host/process-I/O telemetry; archive-backed RPF behavior inherited from immutable M5-002 evidence; no claim of event, road, collectible, or campaign-content completeness'}
  $result=Join-Path $Root 'result.json';[IO.File]::WriteAllText($result,(ConvertTo-Json $record -Depth 14)+[Environment]::NewLine,$utf8);&$verify -ResultPath $result
}

if($CounterSelfTest){$sample=Get-ResourceSample (Get-Process -Id $PID) 0 'baseline';if($sample.io_read_bytes-lt0-or$sample.private_bytes-le0-or$sample.working_set_bytes-le0){throw 'Process counter self-test returned invalid data.'};[pscustomobject]@{CounterSelfTestPassed=$true;PrivateBytes=$sample.private_bytes;WorkingSetBytes=$sample.working_set_bytes;IoReadBytes=$sample.io_read_bytes};return}

$build=Resolve-Safe $BuildRoot 'Build root' -Exists
if($FinalizeRun){$root=Resolve-Safe $FinalizeRun 'M6-001 run root' -Exists;$final=Complete-Run $root $build;Write-Host "M6-001 recovered PASS: '$root\result.json'." -ForegroundColor Green;$final;return}
$game=Resolve-Safe $GameRoot 'Game root' -Exists;$initial=Resolve-Safe $InitialUserRoot 'Initial user root' -Exists
if($build-cne(Resolve-Safe 'out/build/win-amd64-release' 'Canonical Release build')-or$game-cne(Resolve-Safe 'private/game' 'Canonical game')-or$initial-cne(Resolve-Safe 'private/evidence/M5-012/20260816-132209-a316f851/runs/01/user' 'Canonical completed save root')){throw 'M6-001 requires canonical build, game, and completed save inputs.'}
$save=Resolve-Safe (Join-Path $initial 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav') 'Seed save' -Exists;$header=Resolve-Safe (Join-Path $initial 'B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header') 'Seed header' -Exists;if((Get-FileHash $save -Algorithm SHA256).Hash-cne$seedSaveSha-or(Get-FileHash $header -Algorithm SHA256).Hash-cne$seedHeaderSha){throw 'Completed save identity drifted.'}
$prior=Resolve-Safe 'private/evidence/M5-002/20260814-093131-ddca5b9d/result.json' 'M5-002 prerequisite' -Exists;if((Get-FileHash $prior -Algorithm SHA256).Hash-cne$priorResultSha){throw 'M5-002 prerequisite drifted.'}
$route=Get-Content -LiteralPath (Resolve-Safe 'config/city-streaming-route.json' 'Route config' -Exists)-Raw|ConvertFrom-Json
$evidence=Resolve-Safe 'private/evidence/M6-001' 'M6-001 evidence root';[IO.Directory]::CreateDirectory($evidence)|Out-Null;$runRoot=Join-Path $evidence ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8));[IO.Directory]::CreateDirectory($runRoot)|Out-Null;$buildLog=Join-Path $runRoot 'release-clean-build.log'

Write-Host 'M6-001 [1/5]: validating the completed save, city route, and M5 archive prerequisite...' -ForegroundColor Cyan
$gameBefore=& (Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath $game -VerifyHashes
Write-Host 'M6-001 [2/5]: clean-building the Release host...' -ForegroundColor Cyan
if(Invoke-Logged {&$cmake --preset win-amd64-release} $buildLog){throw 'Release configure failed.'};if(Invoke-Logged {&$cmake --build --preset win-amd64-release --target mcla --clean-first --parallel 8} $buildLog -Append){throw "Release clean build failed. Private run: '$runRoot'."}
$exe=Resolve-Safe (Join-Path $build 'mcla.exe') 'Release executable' -Exists;if(@((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{$_.Path-ceq$exe}catch{$false}}).Count){throw 'Canonical Release MCLA is already running.'};$artifactsBefore=@(Get-Artifacts $build)
$cycleRoot=Join-Path $runRoot 'runs/01';$user=Join-Path $cycleRoot 'user';$cache=Join-Path $cycleRoot 'cache';[IO.Directory]::CreateDirectory($user)|Out-Null;[IO.Directory]::CreateDirectory($cache)|Out-Null;Copy-Item -LiteralPath (Join-Path $initial 'B13EBABEBABEBABE') -Destination $user -Recurse -Force
$runtimeLog=Join-Path $cycleRoot 'mcla.log';$samplePath=Join-Path $runRoot 'resource-samples.json';$samples=@();$process=$null;$forced=$false
Write-Host 'M6-001 [3/5]: launching one continuous all-regions free-roam session...' -ForegroundColor Cyan
try{
  $args=@('--mcla_city_streaming_probe=true','--mcla_first_frame_settle_seconds=35','--input_backend=sdl','--mnk_mode=false','--async_shader_compilation=false','--d3d12_pipeline_creation_threads=0','--log_max_file_size_mb=8','--log_max_files=30','--log_level=info','--fullscreen=false',"--game_data_root=`"$game`"","--user_data_root=`"$user`"","--cache_root=`"$cache`"","--log_file=`"$runtimeLog`"")
  $process=Start-Process $exe -ArgumentList $args -WorkingDirectory $build -PassThru;Wait-Marker $process $cycleRoot 'MCLA_CITY_STREAMING_CONFIG v=1 checkpoints=9' 90 'title/city-streaming config'
  Write-Host 'TITLE              | PASS - press START into saved free roam. At every checkpoint stop safely and open the full GPS map before typing.' -ForegroundColor Green
  Start-Sleep -Seconds 3;$samples+=Get-ResourceSample $process 0 'baseline';Write-Samples $samplePath $samples
  foreach($checkpoint in $route.checkpoints){
    $expected="REGION $($checkpoint.index)/9 $($checkpoint.id.ToUpperInvariant()) READY"
    $short="$($checkpoint.id.ToUpperInvariant()) READY"
    Write-Host ("REGION {0}/9         | {1}" -f $checkpoint.index,$checkpoint.instruction) -ForegroundColor Cyan
    Write-Host "Type either: $expected" -ForegroundColor Yellow
    Write-Host "       or: $short" -ForegroundColor DarkYellow
    while($true){
      $answer=Read-Host
      if($answer-ceq$expected-or$answer-ceq$short){break}
      Write-Host "Not accepted. The game is still running; type one of the two confirmation lines shown above." -ForegroundColor Red
    }
    $request=Join-Path $user ".mcla-city-streaming-$($checkpoint.id).request"
    [IO.File]::WriteAllText($request,'1',$utf8)
    $marker="MCLA_CITY_STREAMING_FRAME v=1 checkpoint=$($checkpoint.index) id=$($checkpoint.id) width=1280 height=720"
    Wait-Marker $process $cycleRoot $marker 15 "region $($checkpoint.index) capture"
    if(Test-Path -LiteralPath $request){throw "Region $($checkpoint.index) request was not consumed."}
    $samples+=Get-ResourceSample $process ([int]$checkpoint.index) ([string]$checkpoint.id)
    Write-Samples $samplePath $samples
    Write-Host ("REGION {0}/9         | CAPTURED" -f $checkpoint.index) -ForegroundColor Green
  }
  Wait-Marker $process $cycleRoot 'MCLA_CITY_STREAMING_SUMMARY v=1 status=PASS checkpoints=9' 10 'city summary';Write-Host 'Closing the console-style title externally with WM_CLOSE...' -ForegroundColor DarkCyan;Close-ExactWindow $process;if(-not$process.WaitForExit(10000)-or$process.ExitCode-ne0){throw 'Controlled external WM_CLOSE failed.'};if(@((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{$_.Path-ceq$exe}catch{$false}}).Count){throw 'Release MCLA process survived close.'}
}catch{$failure=$_;if($null-ne$process-and-not$process.HasExited){$forced=$true;Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue;$null=$process.WaitForExit(5000)};if($forced){throw "M6-001 failure required force cleanup. $($failure.Exception.Message) Private run: '$runRoot'."};throw}
Write-Host 'M6-001 [4/5]: verifying route chronology, frames, resource bounds, and archive prerequisite...' -ForegroundColor Cyan;$gameAfter=& (Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath $game -VerifyHashes;$artifactsAfter=@(Get-Artifacts $build);if(($gameBefore|ConvertTo-Json -Compress)-cne($gameAfter|ConvertTo-Json -Compress)-or($artifactsBefore|ConvertTo-Json -Compress)-cne($artifactsAfter|ConvertTo-Json -Compress)){throw 'Source-game or Release artifact identity changed.'};$final=Complete-Run $runRoot $build
Write-Host 'M6-001 [5/5]: persisted result revalidated.' -ForegroundColor Cyan;Write-Host "M6-001 PASS: all eight coverage zones and return-to-start completed. Result: '$runRoot\result.json'." -ForegroundColor Green;$final
