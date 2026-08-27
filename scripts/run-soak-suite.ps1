[CmdletBinding()]
param(
  [ValidateSet('frontend','free-roam','races','garage','lifecycle')][string]$Scenario='frontend',
  [string]$SuiteRun,
  [switch]$InitializeOnly,
  [switch]$Finalize,
  [switch]$RecoverCompletedScenario,
  [int]$DurationSeconds=7200,
  [switch]$Calibration
)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdk=Join-Path $repo 'third_party/rexglue-sdk'
$verify=Join-Path $PSScriptRoot 'verify-soak-suite.ps1'
$saveWatcher=Join-Path $PSScriptRoot 'watch-soak-save.ps1'
$pwshHost=(Get-Process -Id $PID).Path
$utf8=[Text.UTF8Encoding]::new($false)
$scenarioNames=@('frontend','free-roam','races','garage','lifecycle')
$artifactNames=@('mcla.exe','rexruntime.dll','TracyClient.dll','rexgpu-xenos.dll')
$priorEvidence=@(
  [ordered]@{task='M6-001';path='private/evidence/M6-001/20260817-115619-d269e2a9/result.json';decision='all-major-city-regions-streaming-pass';sha256='519B84FF456BDD3220BFC8BE3DD230CCB209A56CF2B203D51CFF5454729E178F'},
  [ordered]@{task='M6-002';path='private/evidence/M6-002/20260817-155005-1dd57bd3/result.json';decision='representative-garage-purchase-customization-persistence-pass';sha256='21FCBE38695B45D22AE516E33E51AD0A292286985549673811E0FE3E33900644'},
  [ordered]@{task='M6-003';path='private/evidence/M6-003/20260818-180605-e3b74fb5/result.json';decision='representative-race-system-matrix-pass';sha256='AD3573B56412D7DBC10650BE0D250AFD4AACA22EB8EEEB6F9816E94EC3007E3A'},
  [ordered]@{task='M6-005';path='private/evidence/M6-005/20260818-201702-5d089d9c/result.json';decision='native-autosave-load-overwrite-plus-isolated-creation-destructive-save-matrix-pass';sha256='5F46FA6657F39EE5AF990BD505B67D5EDBE6A781E8283D1D51BE07F3543F169E'},
  [ordered]@{task='M6-007';path='private/evidence/M6-007/20260819-122150-2ad3b961/result.json';decision='two-hour-audio-and-device-recovery-pass';sha256='CE4B4700CEA1108243989165211A8C70AC0C67F72130708A129C1BD834948B5B'},
  [ordered]@{task='M6-010';path='private/evidence/M6-010/20260819-180446-730942f2/result.json';decision='split-window-device-lifecycle-matrix-pass';sha256='746D7DAECD475E48EEA9EE8342319A84CC2E059992A635579AB55AF7A719C71B'},
  [ordered]@{task='M6-012';path='private/evidence/M6-012/20260819-192540-2a9e82fe/result.json';decision='timestamped-performance-telemetry-pass';sha256='BFEA11FB1A7E8C5D380343C252612BC3BCBE471F0DC1BFB1FCA56B771FB27CB3'},
  [ordered]@{task='M6-013';path='private/evidence/M6-013/20260819-200712-a4cc8715/result.json';decision='reached-unsupported-surface-fixed-or-bounded-nonblocking';sha256='279DA2FDEDF20D8C69D3DE6B5993A26AC625FBA9B624BF52BC3F800F8F1BC4ED'}
)
$frontendSeedRelative='private/evidence/M5-013/20260817-013319-c2e7223f/runs/01/user'
$gameplaySeedRelative='private/evidence/M6-002/20260817-155005-1dd57bd3/runs/02/user'
$saveRelative='B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
$headerRelative='B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'
$sampleInterval=300
$captureInterval=900
$Scenario=$Scenario.ToLowerInvariant()

if(-not$Calibration-and$DurationSeconds-ne7200){throw 'Canonical M6-014 scenarios require exactly 7200 seconds.'}
if($Calibration-and($DurationSeconds-lt30-or$DurationSeconds-gt600)){throw 'Calibration duration must be 30..600 seconds.'}
if($Finalize-and-not$SuiteRun){throw 'Finalize requires an explicit existing SuiteRun.'}
if($RecoverCompletedScenario-and(-not$SuiteRun-or$InitializeOnly-or$Finalize-or$Calibration)){throw 'Recovery requires an explicit canonical SuiteRun and cannot be combined with initialize, finalize, or calibration.'}

if(-not('MclaSoakNative'-as[type])){Add-Type -AssemblyName System.Drawing;Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;
public static class MclaSoakNative{
  public struct RECT{public int Left,Top,Right,Bottom;}
  public struct POINT{public int X,Y;}
  [StructLayout(LayoutKind.Sequential)]public struct IO{public ulong ReadOperationCount,WriteOperationCount,OtherOperationCount,ReadTransferCount,WriteTransferCount,OtherTransferCount;}
  delegate bool E(IntPtr h,IntPtr p);
  [DllImport("user32.dll")]static extern bool EnumWindows(E c,IntPtr p);
  [DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
  [DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetWindowText(IntPtr h,StringBuilder s,int n);
  [DllImport("user32.dll")]static extern bool GetClientRect(IntPtr h,out RECT r);
  [DllImport("user32.dll")]static extern bool ClientToScreen(IntPtr h,ref POINT p);
  [DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")]public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")]static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")]static extern bool ShowWindow(IntPtr h,int command);
  [DllImport("user32.dll")]static extern IntPtr SetFocus(IntPtr h);
  [DllImport("user32.dll")]static extern bool AttachThreadInput(uint attach,uint attachTo,bool value);
  [DllImport("kernel32.dll")]static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")]public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
  [DllImport("kernel32.dll",SetLastError=true)]public static extern bool GetProcessIoCounters(IntPtr h,out IO c);
  public static IntPtr[] Handles(int pid){var a=new List<IntPtr>();EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}
  public static string Title(IntPtr h){var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty;}
  public static bool FocusWindow(IntPtr h){IntPtr foreground=GetForegroundWindow();uint unused;uint foregroundThread=foreground==IntPtr.Zero?0:GetWindowThreadProcessId(foreground,out unused);uint targetThread=GetWindowThreadProcessId(h,out unused);uint currentThread=GetCurrentThreadId();bool foregroundAttached=foregroundThread!=0&&foregroundThread!=currentThread&&AttachThreadInput(currentThread,foregroundThread,true);bool targetAttached=targetThread!=0&&targetThread!=currentThread&&targetThread!=foregroundThread&&AttachThreadInput(currentThread,targetThread,true);try{ShowWindow(h,9);BringWindowToTop(h);SetForegroundWindow(h);SetFocus(h);return GetForegroundWindow()==h;}finally{if(targetAttached)AttachThreadInput(currentThread,targetThread,false);if(foregroundAttached)AttachThreadInput(currentThread,foregroundThread,false);}}
  public static int[] Client(IntPtr h){RECT r;if(!GetClientRect(h,out r))return new int[0];var p=new POINT();if(!ClientToScreen(h,ref p))return new int[0];return new[]{p.X,p.Y,r.Right-r.Left,r.Bottom-r.Top};}
}
'@}

function Safe([string]$Path,[string]$Description,[switch]$Exists){$full=if([IO.Path]::IsPathRooted($Path)){[IO.Path]::GetFullPath($Path)}else{[IO.Path]::GetFullPath((Join-Path $repo $Path))};$prefix=$repo.TrimEnd('\')+'\';if(-not$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description escapes repository."};$cursor=$repo;foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object Length)){$cursor=Join-Path $cursor $part;if((Test-Path $cursor)-and((Get-Item $cursor -Force).Attributes-band[IO.FileAttributes]::ReparsePoint)){throw "$Description traverses a reparse point."}};if($Exists-and-not(Test-Path $full)){throw "$Description is missing."};$full}
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
function Invoke-Logged([scriptblock]$Command,[string]$Log,[switch]$Append){$prior=$ErrorActionPreference;$ErrorActionPreference='Continue';try{if($Append){&$Command *>&1|Tee-Object -FilePath $Log -Append|Out-Null}else{&$Command *>&1|Tee-Object -FilePath $Log|Out-Null};$LASTEXITCODE}finally{$ErrorActionPreference=$prior}}
function Hex([byte[]]$Bytes){([BitConverter]::ToString($Bytes)).Replace('-','')}
function Tree([string]$Root){$sha=[Security.Cryptography.SHA256]::Create();try{$files=@(Get-ChildItem $Root -File -Recurse -Force|Sort-Object FullName);$bytes=0L;foreach($f in $files){$rel=$f.FullName.Substring($Root.TrimEnd('\').Length+1).Replace('\','/');$h=Hash $f.FullName;$line=[Text.Encoding]::UTF8.GetBytes("$rel`t$($f.Length)`t$h`n");$null=$sha.TransformBlock($line,0,$line.Length,$line,0);$bytes+=$f.Length};$null=$sha.TransformFinalBlock([byte[]]::new(0),0,0);[pscustomobject]@{hash=(Hex $sha.Hash);files=$files.Count;bytes=$bytes}}finally{$sha.Dispose()}}
function Read-Shared([string]$Path){$stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite);try{$reader=[IO.StreamReader]::new($stream);try{$reader.ReadToEnd()}finally{$reader.Dispose()}}finally{$stream.Dispose()}}
function Hash-Shared([string]$Path){$stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite);$sha=[Security.Cryptography.SHA256]::Create();try{Hex $sha.ComputeHash($stream)}finally{$sha.Dispose();$stream.Dispose()}}
function LogSet([string]$Root){$files=@(Get-ChildItem $Root -File -Filter 'mcla*.log'|Sort-Object @{Expression={if($_.Name-eq'mcla.log'){999}else{[int]([regex]::Match($_.Name,'\.(\d+)\.log$').Groups[1].Value)}}});$sha=[Security.Cryptography.SHA256]::Create();$text='';try{foreach($f in $files){$h=Hash-Shared $f.FullName;$text+=Read-Shared $f.FullName;$line=[Text.Encoding]::UTF8.GetBytes("$($f.Name)`t$($f.Length)`t$h`n");$null=$sha.TransformBlock($line,0,$line.Length,$line,0)};$null=$sha.TransformFinalBlock([byte[]]::new(0),0,0);[pscustomobject]@{hash=(Hex $sha.Hash);text=$text}}finally{$sha.Dispose()}}
function Artifacts([string]$Build){@($artifactNames|ForEach-Object{[ordered]@{name=$_;sha256=(Hash (Join-Path $Build $_))}})}
function GameIdentity([string]$Root){$v=&(Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath $Root -VerifyHashes;[ordered]@{file_count=[long]$v.FileCount;payload_bytes=[long]$v.PayloadBytes;source_iso_sha256=$v.SourceIsoSha256}}
function ExactProcesses([string]$Exe){@((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{$_.Path-ceq$Exe}catch{$false}})}
function Window([Diagnostics.Process]$Process){$hits=@();foreach($h in [MclaSoakNative]::Handles($Process.Id)){if([regex]::IsMatch([MclaSoakNative]::Title($h),'^mcla \[rexglue-v[^\]]+\]$')){$hits+=$h}};if($hits.Count-ne1){throw 'Exact visible MCLA window was not found.'};$hits[0]}
function Close-Exact([Diagnostics.Process]$Process){$h=Window $Process;if(-not[MclaSoakNative]::PostMessage($h,0x10,[IntPtr]::Zero,[IntPtr]::Zero)){throw 'Exact PID/window WM_CLOSE failed.'}}
function Capture([Diagnostics.Process]$Process,[string]$Path){
  $h=Window $Process;$deadline=[DateTime]::UtcNow.AddSeconds(20)
  do{
    do{$null=[MclaSoakNative]::FocusWindow($h);Start-Sleep -Milliseconds 250}while([MclaSoakNative]::GetForegroundWindow()-ne$h-and[DateTime]::UtcNow-lt$deadline)
    if([MclaSoakNative]::GetForegroundWindow()-ne$h){break}
    Start-Sleep -Milliseconds 500;$r=[MclaSoakNative]::Client($h);if($r.Count-ne4-or$r[2]-lt640-or$r[3]-lt360){throw 'MCLA client geometry is invalid.'}
    $source=[Drawing.Bitmap]::new($r[2],$r[3]);try{$g=[Drawing.Graphics]::FromImage($source);try{$g.CopyFromScreen($r[0],$r[1],0,0,[Drawing.Size]::new($r[2],$r[3]))}finally{$g.Dispose()};$bmp=[Drawing.Bitmap]::new(1280,720);try{$g2=[Drawing.Graphics]::FromImage($bmp);try{$g2.DrawImage($source,0,0,1280,720)}finally{$g2.Dispose()};$bmp.Save($Path,[Drawing.Imaging.ImageFormat]::Bmp)}finally{$bmp.Dispose()}}finally{$source.Dispose()}
    $image=[Drawing.Bitmap]::FromFile($Path);try{$bins=New-Object 'Collections.Generic.HashSet[int]';for($y=0;$y-lt720;$y+=12){for($x=0;$x-lt1280;$x+=12){$c=$image.GetPixel($x,$y);$null=$bins.Add((($c.R-shr3)-shl10)-bor(($c.G-shr3)-shl5)-bor($c.B-shr3))}}}finally{$image.Dispose()}
    if($bins.Count-ge80){return [ordered]@{name=(Split-Path $Path -Leaf);sha256=Hash $Path;bytes=[long](Get-Item $Path).Length;color_bins=$bins.Count}}
    Start-Sleep -Milliseconds 500
  }while([DateTime]::UtcNow-lt$deadline)
  throw 'Exact MCLA window did not yield a nontrivial physical capture within 20 seconds.'
}
function Sample([Diagnostics.Process]$Process,[int]$Checkpoint,[long]$Elapsed){if($Process.HasExited){throw 'Process exited during resource sampling.'};$Process.Refresh();$io=[MclaSoakNative+IO]::new();if(-not[MclaSoakNative]::GetProcessIoCounters($Process.Handle,[ref]$io)){throw 'GetProcessIoCounters failed.'};[ordered]@{checkpoint=$Checkpoint;elapsed_seconds=$Elapsed;private_bytes=[long]$Process.PrivateMemorySize64;working_set_bytes=[long]$Process.WorkingSet64;handle_count=[long]$Process.HandleCount;thread_count=[long]$Process.Threads.Count;io_read_bytes=[long]$io.ReadTransferCount}}
function WriteJson([string]$Path,$Value){[IO.File]::WriteAllText($Path,((ConvertTo-Json $Value -Depth 12)+[Environment]::NewLine),$utf8)}
function SeedRecord([string]$Role,[string]$Source,[string]$Context,[string]$PriorPath,[string]$PriorHash,[string]$Root){[ordered]@{role=$Role;source=$Source;source_context=$Context;upstream_result_path=$PriorPath;upstream_result_sha256=$PriorHash;tree_sha256=(Tree $Root).hash;save_sha256=(Hash (Join-Path $Root $saveRelative));header_sha256=(Hash (Join-Path $Root $headerRelative))}}
function MigrateCompletedStages($Suite,[string]$SuiteRoot,[hashtable]$SeedRoots){foreach($name in @($Suite.completed)){$stagePath=Join-Path $SuiteRoot "scenarios/$name/stage.json";if(-not(Test-Path -LiteralPath $stagePath)){continue};$old=Get-Content -LiteralPath $stagePath -Raw|ConvertFrom-Json;$seedClass=if($name-ceq'frontend'){'frontend'}else{'gameplay'};$seed=$SeedRoots[$seedClass];$user=Join-Path $SuiteRoot "scenarios/$name/run/user";$stage=[ordered]@{schema='mcla-two-hour-soak-stage-v2';name=$old.name;seed_class=$seedClass;decision=$old.decision;duration_seconds=[long]$old.duration_seconds;sample_count=[int]$old.sample_count;capture_count=[int]$old.capture_count;activity_primary=[int]$old.activity_primary;activity_secondary=[int]$old.activity_secondary;distinct_labels=@($old.distinct_labels);resource_bounds=$old.resource_bounds;runtime_log_set_sha256=$old.runtime_log_set_sha256;save_before_sha256=(Hash (Join-Path $seed $saveRelative));save_after_sha256=(Hash (Join-Path $user $saveRelative));header_before_sha256=(Hash (Join-Path $seed $headerRelative));header_after_sha256=(Hash (Join-Path $user $headerRelative));controlled_exit=[bool]$old.controlled_exit;exit_code=[int]$old.exit_code;force_cleanup=[bool]$old.force_cleanup;fatal_markers=[int]$old.fatal_markers};WriteJson $stagePath $stage}}
function WaitMarker([Diagnostics.Process]$Process,[string]$Root,[string]$Needle,[int]$Seconds){$deadline=[DateTime]::UtcNow.AddSeconds($Seconds);while([DateTime]::UtcNow-lt$deadline){if($Process.HasExited){throw 'Process exited before startup marker.'};if((LogSet $Root).text.Contains($Needle)){return};Start-Sleep -Milliseconds 250};throw "Startup marker timed out: $Needle"}
function Enter-Gameplay([Diagnostics.Process]$Process,[string]$Root,[string]$UserRoot){
  WaitMarker $Process $Root 'MCLA_GARAGE_LIFECYCLE_CONFIG v=1 cycle=2' 20
  $request=Join-Path $UserRoot '.mcla-garage-control.request';$temporary=$request+'.tmp'
  [IO.File]::WriteAllText($temporary,'1 START',$utf8);Move-Item -LiteralPath $temporary -Destination $request -Force
  WaitMarker $Process $Root 'MCLA_GARAGE_CONTROL v=1 sequence=1 action=START capture=0 width=0 height=0' 30
  Write-Host 'AUTO START         | PASS - loading the local signed-in save; waiting 30 seconds for gameplay...' -ForegroundColor Green
  $deadline=[DateTime]::UtcNow.AddSeconds(30);while([DateTime]::UtcNow-lt$deadline){if($Process.HasExited){throw 'Process exited while loading saved gameplay.'};Start-Sleep -Milliseconds 250}
}
function PromptReady([string]$Name){if($Name-eq'frontend'){return};$instructions=@{'free-roam'='Saved gameplay should now be loaded. Play normally: races and story progression are allowed; keep moving through the city.';'races'='Saved gameplay should now be loaded. Begin completing race events.';'garage'='Saved gameplay should now be loaded. Open the garage/customization loop.';'lifecycle'='Saved gameplay should now be loaded. Begin pause/focus/minimize/controller lifecycle cycles.'};$exact="SOAK $($Name.ToUpperInvariant()) READY";Write-Host $instructions[$Name] -ForegroundColor Yellow;do{$answer=Read-Host "Type exactly: $exact"}while($answer-cne$exact)}
function PromptActivity([string]$Name,[int]$Checkpoint){
  switch($Name){
    'free-roam'{do{$v=Read-Host "15-minute checkpoint $Checkpoint/8: type FREEROAM <REGION-LABEL>"}while($v-cnotmatch'^FREEROAM ([A-Z0-9-]{2,40})$');[ordered]@{primary=1;secondary=0;label=$Matches[1]}}
    'races'{do{$v=Read-Host "15-minute checkpoint $Checkpoint/8: type RACES <completed> <series-completed>"}while($v-cnotmatch'^RACES ([0-9]{1,2}) ([0-9]{1,2})$');[ordered]@{primary=[int]$Matches[1];secondary=[int]$Matches[2];label='race-events'}}
    'garage'{do{$v=Read-Host "15-minute checkpoint $Checkpoint/8: type GARAGE <entries> <committed-changes>"}while($v-cnotmatch'^GARAGE ([0-9]{1,2}) ([0-9]{1,2})$');[ordered]@{primary=[int]$Matches[1];secondary=[int]$Matches[2];label='garage-cycles'}}
    'lifecycle'{do{$v=Read-Host "15-minute checkpoint $Checkpoint/8: type LIFECYCLE <complete-cycles> [LONG]"}while($v-cnotmatch'^LIFECYCLE ([0-9]{1,2})( LONG)?$');[ordered]@{primary=[int]$Matches[1];secondary=if($Matches[2]){1}else{0};label='lifecycle-cycles'}}
    default{[ordered]@{primary=0;secondary=0;label='title-frontend'}}
  }
}
function Bounds($Samples){$first=$Samples[0];$last=$Samples[-1];$maxPrivate=[long](($Samples|ForEach-Object{[long]$_.private_bytes}|Measure-Object -Maximum).Maximum);$maxWorking=[long](($Samples|ForEach-Object{[long]$_.working_set_bytes}|Measure-Object -Maximum).Maximum);[ordered]@{private_growth_bytes=[Math]::Max(0L,[long]$last.private_bytes-[long]$first.private_bytes);working_growth_bytes=[Math]::Max(0L,[long]$last.working_set_bytes-[long]$first.working_set_bytes);handle_growth=[Math]::Max(0L,[long]$last.handle_count-[long]$first.handle_count);thread_growth=[Math]::Max(0L,[long]$last.thread_count-[long]$first.thread_count);io_read_growth_bytes=[Math]::Max(0L,[long]$last.io_read_bytes-[long]$first.io_read_bytes);private_peak_growth_bytes=[Math]::Max(0L,$maxPrivate-[long]$first.private_bytes);working_peak_growth_bytes=[Math]::Max(0L,$maxWorking-[long]$first.working_set_bytes)}}
function Recover-CompletedScenario([string]$Name,$Suite,[string]$SuitePath,[string]$SuiteRoot,[string]$Seed,[string]$Exe){
  if($Name-ceq'frontend'){throw 'Frontend recovery is unsupported because its in-process audio completion marker is authoritative.'}
  if(@($Suite.completed)-ccontains$Name){throw "Scenario '$Name' is already complete."}
  if(@(ExactProcesses $Exe).Count){throw 'Canonical MCLA is still running; recovery requires the process to have exited.'}
  $scenarioRoot=Safe (Join-Path $SuiteRoot "scenarios/$Name") 'Recovery scenario root' -Exists
  $stagePath=Join-Path $scenarioRoot 'stage.json';if(Test-Path -LiteralPath $stagePath){throw 'Recovery refuses to replace an existing stage.'}
  $runRoot=Safe (Join-Path $scenarioRoot 'run') 'Recovery run root' -Exists
  $user=Safe (Join-Path $runRoot 'user') 'Recovery user root' -Exists
  $samples=@(Get-Content -LiteralPath (Safe (Join-Path $scenarioRoot 'resource-samples.json') 'Recovery samples' -Exists) -Raw|ConvertFrom-Json)
  if($samples.Count-lt25-or$samples.Count-gt30){throw 'Recovery sample count is outside 25..30.'}
  for($i=0;$i-lt$samples.Count;$i++){
    if([int]$samples[$i].checkpoint-ne$i){throw 'Recovery sample checkpoints are not contiguous.'}
    if($i-eq0-and[long]$samples[$i].elapsed_seconds-ne0){throw 'Recovery baseline elapsed time is not zero.'}
    if($i-gt0){$gap=[long]$samples[$i].elapsed_seconds-[long]$samples[$i-1].elapsed_seconds;if($gap-lt0-or$gap-gt900){throw 'Recovery interactive sample chronology is invalid.'}}
    foreach($field in @('private_bytes','working_set_bytes','handle_count','thread_count','io_read_bytes')){if([long]$samples[$i].$field-lt0){throw "Recovery sample field '$field' is invalid."}}
  }
  if([long]$samples[-1].elapsed_seconds-lt7200){throw 'Recovery resource timeline is shorter than two hours.'}
  $bounds=Bounds $samples;if($bounds.private_growth_bytes-gt1073741824L-or$bounds.working_growth_bytes-gt536870912L-or$bounds.handle_growth-gt128-or$bounds.thread_growth-gt32){throw 'Recovery resource growth exceeds canonical bounds.'}
  $captureRecords=@(Get-Content -LiteralPath (Safe (Join-Path $scenarioRoot 'captures.json') 'Recovery capture manifest' -Exists) -Raw|ConvertFrom-Json)
  $captureRoot=Safe (Join-Path $scenarioRoot 'captures') 'Recovery capture root' -Exists
  $captureFiles=@(Get-ChildItem -LiteralPath $captureRoot -File -Filter '*.bmp'|Sort-Object Name)
  if($captureRecords.Count-lt9-or$captureRecords.Count-ne$captureFiles.Count){throw 'Recovery capture sequence is incomplete.'}
  for($i=0;$i-lt$captureRecords.Count;$i++){$expected="checkpoint-{0:D2}.bmp"-f$i;$file=$captureFiles[$i];$record=$captureRecords[$i];if($file.Name-cne$expected-or$record.name-cne$expected-or(Hash $file.FullName)-cne$record.sha256-or[long]$file.Length-ne[long]$record.bytes-or[int]$record.color_bins-lt80){throw 'Recovery capture manifest does not match physical frames.'}}
  if(@($captureRecords.sha256|Sort-Object -Unique).Count-lt3){throw 'Recovery capture sequence lacks temporal variation.'}
  $activity=@(Get-Content -LiteralPath (Safe (Join-Path $scenarioRoot 'activity.json') 'Recovery activity journal' -Exists) -Raw|ConvertFrom-Json)
  if($activity.Count-ne8){throw 'Recovery requires all eight operator activity checkpoints.'}
  $primary=[int](($activity|Measure-Object primary -Sum).Sum);$secondary=[int](($activity|Measure-Object secondary -Sum).Sum);$labels=@($activity|ForEach-Object label|Sort-Object -Unique)
  $minimum=@{'free-roam'=@(8,0);'races'=@(10,2);'garage'=@(20,10);'lifecycle'=@(30,1)}[$Name]
  if($primary-lt$minimum[0]-or$secondary-lt$minimum[1]-or($Name-ceq'free-roam'-and$labels.Count-lt5)){throw 'Recovery semantic activity is below the canonical floor.'}
  foreach($label in $labels){if($label-cnotmatch'^[A-Z0-9-]{2,40}$'-and$label-cnotin@('race-events','garage-cycles','lifecycle-cycles')){throw 'Recovery activity label is invalid or privacy-unsafe.'}}
  $set=LogSet $runRoot;$fatal=[regex]::Matches($set.text,'(?i)\[FATAL\]|guest crash|PPC_UNIMPLEMENTED|invalid or unregistered function|device lost|DXGI_ERROR_DEVICE_REMOVED').Count
  foreach($marker in @('MCLA graphics: nontrivial guest frame captured','Window closing, shutting down...','Execution complete','Title terminated; hard-exiting process.')){if(-not$set.text.Contains($marker)){throw "Recovery runtime marker is missing: $marker"}}
  if($fatal){throw 'Recovery runtime logs contain fatal markers.'}
  $save=Join-Path $user $saveRelative;$header=Join-Path $user $headerRelative;$saveHash=Hash $save;$headerHash=Hash $header
  $archive=Safe "private/save-archive/M6-014/$($Suite.suite_id)/$Name" 'Recovery save archive' -Exists;$latest=Get-Content -LiteralPath (Safe (Join-Path $archive 'latest.json') 'Recovery save snapshot' -Exists) -Raw|ConvertFrom-Json
  $snapshot=Safe (Join-Path $archive $latest.snapshot_directory) 'Recovery snapshot directory' -Exists;$snapshotSave=Join-Path $snapshot $saveRelative;$snapshotHeader=Join-Path $snapshot $headerRelative
  if($latest.schema-cne'mcla-soak-save-snapshot-v1'-or-not$latest.complete_profile_tree-or$latest.save_sha256-cne$saveHash-or$latest.header_sha256-cne$headerHash-or(Hash $snapshotSave)-cne$saveHash-or(Hash $snapshotHeader)-cne$headerHash){throw 'Recovery save snapshot does not match the final complete profile tree.'}
  $recovery=[ordered]@{schema='mcla-soak-stage-recovery-v1';scenario=$Name;reason='interactive-wall-clock-complete-frontend-audio-marker-not-applicable';recovered_utc=[DateTime]::UtcNow.ToString('O');sample_count=$samples.Count;capture_count=$captureRecords.Count;activity_checkpoints=$activity.Count;runtime_shutdown_path='WM_CLOSE-to-SDK-hard-exit-0';save_snapshot_identity=$latest.identity};WriteJson (Join-Path $scenarioRoot 'recovery.json') $recovery
  $stage=[ordered]@{schema='mcla-two-hour-soak-stage-v2';name=$Name;seed_class='gameplay';decision="two-hour-$Name-soak-pass";duration_seconds=[long]$samples[-1].elapsed_seconds;sample_count=$samples.Count;capture_count=$captureRecords.Count;activity_primary=$primary;activity_secondary=$secondary;distinct_labels=@($labels);resource_bounds=$bounds;runtime_log_set_sha256=$set.hash;save_before_sha256=(Hash (Join-Path $Seed $saveRelative));save_after_sha256=$saveHash;header_before_sha256=(Hash (Join-Path $Seed $headerRelative));header_after_sha256=$headerHash;controlled_exit=$true;exit_code=0;force_cleanup=$false;fatal_markers=0};WriteJson $stagePath $stage
  $Suite.completed=@($Suite.completed)+$Name;WriteJson $SuitePath $Suite
  Write-Host "M6-014 scenario RECOVERED PASS: $Name. Full journals, captures, WM_CLOSE hard-exit-0, and archived save were revalidated." -ForegroundColor Green
}

$evidenceRoot=Safe 'private/evidence/M6-014' 'M6-014 evidence root';[IO.Directory]::CreateDirectory($evidenceRoot)|Out-Null
$build=Safe 'out/build/win-amd64-release' 'Release build';$game=Safe 'private/game' 'Game root' -Exists;$frontendSeed=Safe $frontendSeedRelative 'Frontend save root' -Exists;$gameplaySeed=Safe $gameplaySeedRelative 'Gameplay save root' -Exists;$seedRoots=@{frontend=$frontendSeed;gameplay=$gameplaySeed}
$expectedSeeds=@(
  (SeedRecord 'frontend' $frontendSeedRelative 'immutable-five-race-resource-seed' 'private/evidence/M5-013/20260817-013319-c2e7223f/result.json' 'D890A903775A3B53262B9957E7B7DC1D4B76B49B8735360BECD8233842446298' $frontendSeed),
  (SeedRecord 'gameplay' $gameplaySeedRelative 'latest-verified-persisted-hangout-plus-garage-progression' 'private/evidence/M6-002/20260817-155005-1dd57bd3/result.json' '21FCBE38695B45D22AE516E33E51AD0A292286985549673811E0FE3E33900644' $gameplaySeed)
)
$sdkVersion=(&git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim().TrimStart('v');$sdkCommit=(&git -C $sdk rev-parse HEAD).Trim();if($LASTEXITCODE-ne0-or$sdkVersion-cnotmatch'^0\.9\.0\.[0-9]+$'-or(git -C $sdk status --porcelain)){throw 'M6-014 requires a clean exact tagged SDK.'}
if($SuiteRun){$suiteRoot=Safe (Join-Path $evidenceRoot $SuiteRun) 'Suite root'}else{$SuiteRun=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8);$suiteRoot=Safe (Join-Path $evidenceRoot $SuiteRun) 'Suite root'}
[IO.Directory]::CreateDirectory($suiteRoot)|Out-Null
$suitePath=Join-Path $suiteRoot 'suite.json';$buildLog=Join-Path $suiteRoot 'release-clean-build.log'
if(-not(Test-Path $suitePath)){
  Write-Host 'M6-014 [1/4]: clean-building one shared Release artifact set...' -ForegroundColor Cyan;$cmake=(& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath).CMakePath
  if((Invoke-Logged {&$cmake --preset win-amd64-release} $buildLog)-ne0-or(Invoke-Logged {&$cmake --build --preset win-amd64-release --target mcla --clean-first --parallel 8} $buildLog -Append)-ne0){throw "Release clean build failed. Suite: '$suiteRoot'."}
  $suite=[ordered]@{schema='mcla-soak-suite-state-v2';suite_id=$SuiteRun;sdk_version=$sdkVersion;sdk_commit=$sdkCommit;build_configuration='Release';duration_seconds=$DurationSeconds;game=(GameIdentity $game);seeds=@($expectedSeeds);build=[ordered]@{clean_build_log_sha256=(Hash $buildLog);artifacts=@(Artifacts $build)};completed=@()};WriteJson $suitePath $suite
}else{$suite=Get-Content $suitePath -Raw|ConvertFrom-Json;if($suite.suite_id-cne$SuiteRun-or$suite.sdk_commit-cne$sdkCommit-or$suite.duration_seconds-ne$DurationSeconds){throw 'Suite state identity changed.'};if((ConvertTo-Json @($suite.build.artifacts)-Compress)-cne(ConvertTo-Json @(Artifacts $build)-Compress)){throw 'Shared Release artifacts drifted.'};if('game'-notin$suite.PSObject.Properties.Name){$suite|Add-Member -NotePropertyName game -NotePropertyValue (GameIdentity $game)};if((ConvertTo-Json $suite.game -Compress)-cne(ConvertTo-Json (GameIdentity $game) -Compress)){throw 'Source-game identity drifted.'};if('seeds'-notin$suite.PSObject.Properties.Name){if('seed'-notin$suite.PSObject.Properties.Name-or(ConvertTo-Json $suite.seed -Compress)-cne(ConvertTo-Json ([ordered]@{source=$frontendSeedRelative;tree_sha256=$expectedSeeds[0].tree_sha256;save_sha256=$expectedSeeds[0].save_sha256;header_sha256=$expectedSeeds[0].header_sha256}) -Compress)){throw 'Legacy frontend seed identity drifted.'};$suite.PSObject.Properties.Remove('seed');$suite|Add-Member -NotePropertyName seeds -NotePropertyValue @($expectedSeeds);$suite.schema='mcla-soak-suite-state-v2'};if((ConvertTo-Json @($suite.seeds)-Compress)-cne(ConvertTo-Json @($expectedSeeds)-Compress)){throw 'Explicit seed lineage drifted.'};MigrateCompletedStages $suite $suiteRoot $seedRoots;WriteJson $suitePath $suite}
if($InitializeOnly){Write-Host "M6-014 suite initialized: $SuiteRun" -ForegroundColor Green;return}
if($Finalize){if(@($suite.completed).Count-ne5){throw 'All five scenarios must complete before finalization.'}}

if($RecoverCompletedScenario){
  $seed=$seedRoots['gameplay'];$exe=Join-Path $build 'mcla.exe';Recover-CompletedScenario $Scenario $suite $suitePath $suiteRoot $seed $exe
}elseif(-not$Finalize){
  $seedClass=if($Scenario-ceq'frontend'){'frontend'}else{'gameplay'};$seed=$seedRoots[$seedClass]
  if(@($suite.completed)-ccontains$Scenario){throw "Scenario '$Scenario' is already complete."};$scenarioRoot=Join-Path $suiteRoot "scenarios/$Scenario";if(Test-Path $scenarioRoot){throw 'Incomplete scenario root already exists; preserve/classify it and start a new suite or remove only with explicit review.'};$runRoot=Join-Path $scenarioRoot 'run';$user=Join-Path $runRoot 'user';$cache=Join-Path $runRoot 'cache';$captures=Join-Path $scenarioRoot 'captures';[IO.Directory]::CreateDirectory($user)|Out-Null;[IO.Directory]::CreateDirectory($cache)|Out-Null;[IO.Directory]::CreateDirectory($captures)|Out-Null;Copy-Item (Join-Path $seed 'B13EBABEBABEBABE') $user -Recurse
  $exe=Join-Path $build 'mcla.exe';if(@(ExactProcesses $exe).Count){throw 'Canonical MCLA is already running.'};$log=Join-Path $runRoot 'mcla.log';$args=@('--mcla_first_frame_probe=true','--sdl_audio_route_audit=true',"--mcla_audio_route_soak_seconds=$DurationSeconds",'--mcla_first_frame_settle_seconds=35','--xam_user_signin_state=1','--input_backend=sdl','--mnk_mode=false','--async_shader_compilation=false','--d3d12_pipeline_creation_threads=0','--log_level=info','--log_max_file_size_mb=16','--log_max_files=50','--fullscreen=false',"--game_data_root=`"$game`"","--user_data_root=`"$user`"","--cache_root=`"$cache`"","--log_file=`"$log`"");if($Scenario-cne'frontend'){$args=@($args)+@('--mcla_garage_lifecycle_probe=true','--mcla_garage_lifecycle_cycle=2')}
  Write-Host "M6-014 [2/4]: launching '$Scenario' for one continuous two-hour process..." -ForegroundColor Cyan;$process=$null;$watcher=$null;$forced=$false;$samples=@();$captureRecords=@();$activity=@();$timer=$null
  try{$process=Start-Process $exe -ArgumentList $args -WorkingDirectory $build -PassThru;if($Scenario-cne'frontend'){$archive=Safe "private/save-archive/M6-014/$SuiteRun/$Scenario" 'Progress archive';$watcher=Start-Process $pwshHost -ArgumentList @('-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-File',$saveWatcher,'-SourceUserRoot',$user,'-ArchiveRoot',$archive,'-MclaProcessId',$process.Id) -WorkingDirectory $repo -WindowStyle Hidden -PassThru;Write-Host "SAVE WATCHER       | ACTIVE - recoverable profile snapshots: $archive" -ForegroundColor Green};WaitMarker $process $runRoot 'MCLA graphics: nontrivial guest frame captured' 90;if($Scenario-cne'frontend'){Enter-Gameplay $process $runRoot $user};PromptReady $Scenario;$timer=[Diagnostics.Stopwatch]::StartNew();$samples+=Sample $process 0 0;$captureRecords+=Capture $process (Join-Path $captures 'checkpoint-00.bmp');WriteJson (Join-Path $scenarioRoot 'resource-samples.json') @($samples);WriteJson (Join-Path $scenarioRoot 'activity.json') @($activity)
    $nextSample=$sampleInterval;$nextCapture=$captureInterval;$nextStatus=60;$sampleIndex=1;$captureIndex=1
    while($timer.Elapsed.TotalSeconds-lt$DurationSeconds){if($process.HasExited){throw "Process exited during '$Scenario' soak."};$elapsed=[int][Math]::Floor($timer.Elapsed.TotalSeconds)
      if($elapsed-ge$nextStatus){Write-Host ("SOAK {0,-10} | {1,4}/{2}s | samples {3}/25 | captures {4}/9 | health PASS"-f$Scenario,[Math]::Min($elapsed,$DurationSeconds),$DurationSeconds,$samples.Count,$captureRecords.Count) -ForegroundColor DarkCyan;$nextStatus+=60}
      if($elapsed-ge$nextSample){$samples+=Sample $process $sampleIndex $elapsed;$sampleIndex++;$nextSample+=$sampleInterval;WriteJson (Join-Path $scenarioRoot 'resource-samples.json') @($samples)}
      if($elapsed-ge$nextCapture){$checkpoint=[int]($nextCapture/$captureInterval);if($Scenario-ne'frontend'){$activity+=PromptActivity $Scenario $checkpoint;WriteJson (Join-Path $scenarioRoot 'activity.json') @($activity)};$captureRecords+=Capture $process (Join-Path $captures ("checkpoint-{0:D2}.bmp"-f$captureIndex));$captureIndex++;$nextCapture+=$captureInterval;WriteJson (Join-Path $scenarioRoot 'captures.json') @($captureRecords)}
      Start-Sleep -Milliseconds 250
    }
    if($samples.Count-lt25){$samples+=Sample $process $sampleIndex $DurationSeconds};if($captureRecords.Count-lt9){if($Scenario-ne'frontend'){$activity+=PromptActivity $Scenario 8};$captureRecords+=Capture $process (Join-Path $captures 'checkpoint-08.bmp')};WriteJson (Join-Path $scenarioRoot 'resource-samples.json') @($samples);WriteJson (Join-Path $scenarioRoot 'captures.json') @($captureRecords);WriteJson (Join-Path $scenarioRoot 'activity.json') @($activity)
    if($Scenario-ceq'frontend'){$deadline=[DateTime]::UtcNow.AddSeconds(30);while([DateTime]::UtcNow-lt$deadline-and-not(LogSet $runRoot).text.Contains("MCLA audio: title soak completed seconds $DurationSeconds")){if($process.HasExited){throw 'Process exited before frontend audio completion marker.'};Start-Sleep -Milliseconds 250};if(-not(LogSet $runRoot).text.Contains("MCLA audio: title soak completed seconds $DurationSeconds")){throw 'Frontend audio completion marker is missing.'}}else{Write-Host 'SOAK COMPLETION    | PASS - interactive wall-clock, resource, capture, and activity journals complete; frontend audio marker is not applicable.' -ForegroundColor Green};Close-Exact $process;if(-not$process.WaitForExit(10000)-or$process.ExitCode-ne0){throw 'Controlled external WM_CLOSE failed.'}
  }catch{$failure=$_;if($process-and-not$process.HasExited){try{Close-Exact $process;$null=$process.WaitForExit(10000)}catch{};if(-not$process.HasExited){$forced=$true;Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue;$null=$process.WaitForExit(5000)}};if($watcher-and-not$watcher.HasExited){$null=$watcher.WaitForExit(15000)};if($forced){throw "M6-014 failure required force cleanup. $($failure.Exception.Message) Scenario root: '$scenarioRoot'."};throw}
  if($watcher-and-not$watcher.HasExited-and-not$watcher.WaitForExit(15000)){throw 'Save watcher did not finish after the game process exited.'};if($watcher-and$watcher.ExitCode-ne0){throw "Save watcher failed with exit $($watcher.ExitCode)."}
  $timer.Stop();$set=LogSet $runRoot;$fatal=[regex]::Matches($set.text,'(?i)\[FATAL\]|guest crash|PPC_UNIMPLEMENTED|invalid or unregistered function|device lost|DXGI_ERROR_DEVICE_REMOVED').Count;if($fatal){throw "Runtime fatal markers found after '$Scenario'."};$primary=if($activity.Count){[int](($activity|Measure-Object primary -Sum).Sum)}else{0};$secondary=if($activity.Count){[int](($activity|Measure-Object secondary -Sum).Sum)}else{0};$labels=@($activity|ForEach-Object label|Sort-Object -Unique);if($Scenario-eq'frontend'){$labels=@('title-frontend')}
  $stage=[ordered]@{schema='mcla-two-hour-soak-stage-v2';name=$Scenario;seed_class=$seedClass;decision="two-hour-$Scenario-soak-pass";duration_seconds=[long][Math]::Floor($timer.Elapsed.TotalSeconds);sample_count=$samples.Count;capture_count=$captureRecords.Count;activity_primary=$primary;activity_secondary=$secondary;distinct_labels=@($labels);resource_bounds=(Bounds $samples);runtime_log_set_sha256=$set.hash;save_before_sha256=(Hash (Join-Path $seed $saveRelative));save_after_sha256=(Hash (Join-Path $user $saveRelative));header_before_sha256=(Hash (Join-Path $seed $headerRelative));header_after_sha256=(Hash (Join-Path $user $headerRelative));controlled_exit=$true;exit_code=0;force_cleanup=$false;fatal_markers=$fatal};WriteJson (Join-Path $scenarioRoot 'stage.json') $stage
  $suite.completed=@($suite.completed)+$Scenario;WriteJson $suitePath $suite;Write-Host "M6-014 scenario PASS: $Scenario. Resume suite '$SuiteRun'." -ForegroundColor Green
}

$suite=Get-Content $suitePath -Raw|ConvertFrom-Json;if(@($suite.completed).Count-eq5){
  $records=@();foreach($name in $scenarioNames){$root=Join-Path $suiteRoot "scenarios/$name";$stage=Get-Content (Join-Path $root 'stage.json') -Raw|ConvertFrom-Json;$records+=[ordered]@{name=$name;seed_class=$stage.seed_class;decision=$stage.decision;duration_seconds=[long]$stage.duration_seconds;sample_count=[int]$stage.sample_count;capture_count=[int]$stage.capture_count;activity_primary=[int]$stage.activity_primary;activity_secondary=[int]$stage.activity_secondary;distinct_labels=@($stage.distinct_labels);resource_bounds=$stage.resource_bounds;runtime_log_set_sha256=$stage.runtime_log_set_sha256;scenario_tree_sha256=(Tree $root).hash;save_before_sha256=$stage.save_before_sha256;save_after_sha256=$stage.save_after_sha256;header_before_sha256=$stage.header_before_sha256;header_after_sha256=$stage.header_after_sha256;controlled_exit=$true;exit_code=0;force_cleanup=$false;fatal_markers=0;stage_path="scenarios/$name/stage.json"}}
  $result=[ordered]@{schema='mcla-two-hour-soak-suite-v2';task='M6-014';decision='five-two-hour-soak-suite-pass';suite_id=$SuiteRun;sdk_version=$suite.sdk_version;sdk_commit=$suite.sdk_commit;build_configuration='Release';duration_seconds_per_scenario=7200;sample_interval_seconds=300;capture_interval_seconds=900;game=$suite.game;seeds=@($suite.seeds);build=$suite.build;prior_evidence=@($priorEvidence);scenarios=@($records);scope=[ordered]@{five_independent_processes=$true;same_release_artifacts=$true;two_explicit_seed_lineages=$true;same_gameplay_seed_identity=$true;same_seed_identity=$false;frontend_two_hours=$true;free_roam_two_hours=$true;races_two_hours=$true;garage_two_hours=$true;lifecycle_two_hours=$true;monolithic_ten_hour_run_claimed=$false;full_campaign_claimed=$false;music_continuity_claimed=$false}};WriteJson (Join-Path $suiteRoot 'result.json') $result;Write-Host 'M6-014 [3/4]: verifying all five physical scenario roots...' -ForegroundColor Cyan;$final=&$verify -ResultPath (Join-Path $suiteRoot 'result.json');Write-Host 'M6-014 [4/4]: persisted suite revalidated.' -ForegroundColor Cyan;Write-Host "M6-014 PASS: '$suiteRoot/result.json'." -ForegroundColor Green;$final
}elseif($Finalize){throw 'Suite is not complete.'}
