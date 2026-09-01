[CmdletBinding()]
param(
  [string]$SuiteRun='20260901-153415-d747cf2d',
  [ValidateRange(300,600)][int]$StabilitySeconds=300,
  [switch]$ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build=Join-Path $repo 'out/build/win-amd64-release'
$game=Join-Path $repo 'private/game'
$suiteRoot=Join-Path $repo "private/evidence/M6-014/$SuiteRun"
$suitePath=Join-Path $suiteRoot 'suite.json'
$seed=Join-Path $repo 'private/save-archive/M6-014/20260831-133236-2cecb67b/free-roam/20260901-114045Z-A575F88F4FDFA19B-2FAEFBC7FF8CDEAD'
$saveWatcher=Join-Path $PSScriptRoot 'watch-soak-save.ps1'
$verify=Join-Path $PSScriptRoot 'verify-delivery-transition-regression.ps1'
$gameVerifier=Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$pwshHost=(Get-Process -Id $PID).Path
$utf8=[Text.UTF8Encoding]::new($false)
$artifactNames=@('mcla.exe','rexruntime.dll','TracyClient.dll','rexgpu-xenos.dll')
$saveRelative='B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
$headerRelative='B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'

if(-not('MclaDeliveryRegressionNative'-as[type])){Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;
public static class MclaDeliveryRegressionNative{
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
'@}

function Safe([string]$Path,[string]$Description,[switch]$Exists){
  $full=[IO.Path]::GetFullPath($Path);$prefix=$repo.TrimEnd('\')+'\'
  if(-not$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description escapes repository."}
  $cursor=$repo;foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object Length)){$cursor=Join-Path $cursor $part;if((Test-Path -LiteralPath $cursor)-and((Get-Item -LiteralPath $cursor -Force).Attributes-band[IO.FileAttributes]::ReparsePoint)){throw "$Description traverses a reparse point."}}
  if($Exists-and-not(Test-Path -LiteralPath $full)){throw "$Description is missing."};$full
}
function Hash([string]$Path){
  $stream=[IO.File]::OpenRead((Safe $Path 'Hash source' -Exists));$sha=[Security.Cryptography.SHA256]::Create()
  try{([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','')}finally{$sha.Dispose();$stream.Dispose()}
}
function Game([string]$Root){
  $verified=&$gameVerifier -GamePath $Root;$manifest=Get-Content -LiteralPath $verified.ManifestPath -Raw|ConvertFrom-Json;$hashes=0
  foreach($entry in @($manifest.Files)){$candidate=Join-Path $Root ([string]$entry.Path).Replace('/','\');if((Hash $candidate)-cne([string]$entry.Sha256).ToUpperInvariant()){throw "Source-game hash drifted for '$($entry.Path)'."};$hashes++}
  if($hashes-ne[int]$verified.FileCount){throw 'Source-game hash coverage is incomplete.'}
  [ordered]@{file_count=[int]$verified.FileCount;payload_bytes=[long]$verified.PayloadBytes;source_iso_sha256=[string]$verified.SourceIsoSha256}
}
function ArgQuote([string]$Value){'"'+$Value.Replace('"','\"')+'"'}
function Tree([string]$Root){
  $sha=[Security.Cryptography.SHA256]::Create();try{$files=@(Get-ChildItem -LiteralPath $Root -File -Recurse -Force|Sort-Object FullName);$bytes=0L;foreach($file in $files){$rel=$file.FullName.Substring($Root.TrimEnd('\').Length+1).Replace('\','/');$line=[Text.Encoding]::UTF8.GetBytes("$rel`t$($file.Length)`t$(Hash $file.FullName)`n");$null=$sha.TransformBlock($line,0,$line.Length,$line,0);$bytes+=$file.Length};$null=$sha.TransformFinalBlock([byte[]]::new(0),0,0);[ordered]@{sha256=([BitConverter]::ToString($sha.Hash)).Replace('-','');files=$files.Count;bytes=$bytes}}finally{$sha.Dispose()}
}
function WriteJson([string]$Path,$Value){[IO.File]::WriteAllText($Path,((ConvertTo-Json $Value -Depth 12)+[Environment]::NewLine),$utf8)}
function ReadLogs([string]$Root){
  $text='';foreach($file in @(Get-ChildItem -LiteralPath $Root -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue|Sort-Object Name)){try{$stream=[IO.File]::Open($file.FullName,'Open','Read','ReadWrite');try{$reader=[IO.StreamReader]::new($stream);try{$text+=$reader.ReadToEnd()}finally{$reader.Dispose()}}finally{$stream.Dispose()}}catch{}};$text
}
function WaitMarker([Diagnostics.Process]$Process,[string]$Root,[string]$Marker,[int]$Seconds,[string]$Step){
  $deadline=[DateTime]::UtcNow.AddSeconds($Seconds);while([DateTime]::UtcNow-lt$deadline){if($Process.HasExited){throw "Process exited during '$Step' (exit $($Process.ExitCode))."};if((ReadLogs $Root).Contains($Marker)){return};Start-Sleep -Milliseconds 200};throw "Timed out during '$Step'."
}
function ReadProcessAwareExact([Diagnostics.Process]$Process,[string]$Prompt,[string]$Exact){
  try{$null=[Console]::KeyAvailable}catch{throw 'Interactive console input is unavailable; run this script in a visible console.'}
  while($true){Write-Host ($Prompt+"`nType exactly: $Exact`: ") -ForegroundColor Yellow -NoNewline;$input=[Text.StringBuilder]::new();while($true){if($Process.HasExited){Write-Host;throw "Process exited while waiting for '$Exact' (exit $($Process.ExitCode))."};if(-not[Console]::KeyAvailable){Start-Sleep -Milliseconds 50;continue};$key=[Console]::ReadKey($true);if($key.Key-eq[ConsoleKey]::Enter){Write-Host;break};if($key.Key-eq[ConsoleKey]::Backspace){if($input.Length){$input.Length--;Write-Host "`b `b" -NoNewline};continue};if(-not[char]::IsControl($key.KeyChar)){$null=$input.Append($key.KeyChar);Write-Host $key.KeyChar -NoNewline}};if($input.ToString()-ceq$Exact){return};Write-Host 'Ignored; the exact phrase is required and the game remains running.' -ForegroundColor DarkYellow}
}
function CloseExact([Diagnostics.Process]$Process){
  $matches=@();foreach($handle in [MclaDeliveryRegressionNative]::Handles($Process.Id)){if([regex]::IsMatch([MclaDeliveryRegressionNative]::Title($handle),'^mcla \[rexglue-v[^\]]+\]$')){$matches+=$handle}}
  if($matches.Count-ne1-or-not[MclaDeliveryRegressionNative]::Close($matches[0])){throw 'Exact PID/window WM_CLOSE failed.'}
}
function Artifacts([string]$Root){@($artifactNames|ForEach-Object{[ordered]@{name=$_;sha256=(Hash (Join-Path $Root $_))}})}

$build=Safe $build 'Release build' -Exists;$game=Safe $game 'Game root' -Exists;$suiteRoot=Safe $suiteRoot 'M6-014 suite' -Exists;$suitePath=Safe $suitePath 'Suite state' -Exists;$seed=Safe $seed 'Progressed gameplay seed' -Exists
$suite=Get-Content -LiteralPath $suitePath -Raw|ConvertFrom-Json
if($suite.suite_id-cne$SuiteRun-or$suite.sdk_version-cne'0.10.0.1'-or$suite.sdk_commit-cne'7dd5cb33002a443b097c0f65d5566c0a0f2db838'){throw 'Delivery regression requires the current exact M6-014 suite identity.'}
$gameBefore=Game $game;if((ConvertTo-Json $gameBefore -Compress)-cne(ConvertTo-Json $suite.game -Compress)){throw 'Source-game payload drifted from the selected suite.'}
$before=@(Artifacts $build);if((ConvertTo-Json $before -Compress)-cne(ConvertTo-Json @($suite.build.artifacts) -Compress)){throw 'Release artifacts drifted from the selected suite.'}
if((Hash (Join-Path $seed $saveRelative))-cne'A575F88F4FDFA19B084BA3C5DBA3B4A15EBFBDCCBD6CCD09628201A9B84A6F82'-or(Hash (Join-Path $seed $headerRelative))-cne'2FAEFBC7FF8CDEADBCE025D9BE914436ED05E27D041F72C8C88798612CFB0D60'){throw 'Progressed gameplay seed identity drifted.'}
$exe=Join-Path $build 'mcla.exe';if(@((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{$_.Path-ceq$exe}catch{$false}}).Count){throw 'Canonical MCLA is already running.'}
if($ValidateOnly){[pscustomobject]@{Passed=$true;SuiteRun=$SuiteRun;SdkVersion=$suite.sdk_version;SdkCommit=$suite.sdk_commit;SeedSaveSha256=(Hash (Join-Path $seed $saveRelative));GameFiles=$gameBefore.file_count;Artifacts=$before.Count;StabilitySeconds=$StabilitySeconds};return}

$evidence=Safe (Join-Path $repo 'private/evidence/M6-014/delivery-regressions') 'Delivery evidence root';[IO.Directory]::CreateDirectory($evidence)|Out-Null
$runId=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8);$root=Join-Path $evidence $runId;$run=Join-Path $root 'run';$user=Join-Path $run 'user';$cache=Join-Path $run 'cache';[IO.Directory]::CreateDirectory($user)|Out-Null;[IO.Directory]::CreateDirectory($cache)|Out-Null;Copy-Item -LiteralPath (Join-Path $seed 'B13EBABEBABEBABE') -Destination $user -Recurse
$archive=Safe (Join-Path $repo "private/save-archive/M6-014/delivery-regression-$runId") 'Delivery save archive';$log=Join-Path $run 'mcla.log'
$args=@('--mcla_first_frame_probe=true','--mcla_first_frame_settle_seconds=35','--mcla_garage_lifecycle_probe=true','--mcla_garage_lifecycle_cycle=2','--xam_user_signin_state=1','--input_backend=sdl','--mnk_mode=false','--readback_resolve=fast','--async_shader_compilation=false','--d3d12_pipeline_creation_threads=0','--log_level=info','--log_max_file_size_mb=16','--log_max_files=20','--fullscreen=false',('--game_data_root="{0}"'-f$game),('--user_data_root="{0}"'-f$user),('--cache_root="{0}"'-f$cache),('--log_file="{0}"'-f$log))

Write-Host 'M6-014 DELIVERY [1/4]: exact current artifact and 37-race/31-win seed verified.' -ForegroundColor Cyan
Write-Host 'M6-014 DELIVERY [2/4]: launching saved gameplay...' -ForegroundColor Cyan
$process=$null;$watcher=$null;$forced=$false;$activeUtc=$null;$stableUtc=$null
try{
  $process=Start-Process $exe -ArgumentList $args -WorkingDirectory $build -PassThru
  $watcher=Start-Process $pwshHost -ArgumentList @('-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-File',(ArgQuote $saveWatcher),'-SourceUserRoot',(ArgQuote $user),'-ArchiveRoot',(ArgQuote $archive),'-MclaProcessId',$process.Id) -WorkingDirectory $repo -WindowStyle Hidden -PassThru
  Write-Host "SAVE WATCHER       | ACTIVE - $archive" -ForegroundColor Green
  WaitMarker $process $run 'MCLA graphics: nontrivial guest frame captured' 90 'title readiness'
  WaitMarker $process $run 'MCLA_GARAGE_LIFECYCLE_CONFIG v=1 cycle=2' 20 'startup control readiness'
  $request=Join-Path $user '.mcla-garage-control.request';$temporary=$request+'.tmp';[IO.File]::WriteAllText($temporary,'1 START',$utf8);Move-Item -LiteralPath $temporary -Destination $request -Force
  WaitMarker $process $run 'MCLA_GARAGE_CONTROL v=1 sequence=1 action=START capture=0 width=0 height=0' 30 'automatic START'
  Write-Host 'AUTO START         | PASS - waiting 30 seconds for saved gameplay...' -ForegroundColor Green
  $deadline=[DateTime]::UtcNow.AddSeconds(30);while([DateTime]::UtcNow-lt$deadline){if($process.HasExited){throw "Process exited while loading gameplay (exit $($process.ExitCode))."};Start-Sleep -Milliseconds 200}
  ReadProcessAwareExact $process 'Start a delivery mission. Confirm only after the delivery objective is active and the car is controllable.' 'DELIVERY ACTIVE';$activeUtc=[DateTime]::UtcNow
  Write-Host "DELIVERY ACTIVE    | PASS - keep playing for $StabilitySeconds seconds." -ForegroundColor Green
  $timer=[Diagnostics.Stopwatch]::StartNew();$nextStatus=30
  while($timer.Elapsed.TotalSeconds-lt$StabilitySeconds){if($process.HasExited){throw "Process exited during delivery stability window (exit $($process.ExitCode))."};$elapsed=[int]$timer.Elapsed.TotalSeconds;if($elapsed-ge$nextStatus){Write-Host ("DELIVERY STABILITY | {0}/{1}s | process alive"-f$elapsed,$StabilitySeconds) -ForegroundColor DarkCyan;$nextStatus+=30};Start-Sleep -Milliseconds 200}
  ReadProcessAwareExact $process 'The five-minute delivery stability window completed. Confirm the delivery remained playable without a crash.' 'DELIVERY STABLE';$stableUtc=[DateTime]::UtcNow
  Write-Host 'DELIVERY STABLE    | PASS - closing the console-style title externally.' -ForegroundColor Green
  CloseExact $process;if(-not$process.WaitForExit(10000)-or$process.ExitCode-ne0){throw 'Controlled external WM_CLOSE failed.'}
}catch{
  $failure=$_;if($process-and-not$process.HasExited){try{CloseExact $process;$null=$process.WaitForExit(10000)}catch{};if(-not$process.HasExited){$forced=$true;Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue;$null=$process.WaitForExit(5000)}}
  if($watcher-and-not$watcher.HasExited){$null=$watcher.WaitForExit(15000)}
  if($forced){throw "Delivery regression required force cleanup. $($failure.Exception.Message) Private run: '$root'."};throw $failure
}
if($watcher-and-not$watcher.HasExited-and-not$watcher.WaitForExit(15000)){throw 'Save watcher did not finish after title exit.'};if($watcher-and$watcher.ExitCode-ne0){throw "Save watcher failed with exit $($watcher.ExitCode)."}
$after=@(Artifacts $build);if((ConvertTo-Json $before -Compress)-cne(ConvertTo-Json $after -Compress)){throw 'Release artifacts changed during delivery regression.'}
$gameAfter=Game $game;if((ConvertTo-Json $gameBefore -Compress)-cne(ConvertTo-Json $gameAfter -Compress)){throw 'Source-game payload changed during delivery regression.'}
$logs=ReadLogs $run;$fatal=[regex]::Matches($logs,'(?i)\[FATAL\]|guest crash|PPC_UNIMPLEMENTED|invalid or unregistered function|device lost|DXGI_ERROR_DEVICE_REMOVED').Count;if($fatal){throw 'Runtime fatal markers were found after the delivery regression.'}
$logFiles=@(Get-ChildItem -LiteralPath $run -File -Filter 'mcla*.log'|Sort-Object Name);if($logFiles.Count-lt1){throw 'Delivery runtime log set is empty.'};$logManifest=@($logFiles|ForEach-Object{[ordered]@{name=$_.Name;sha256=(Hash $_.FullName);bytes=$_.Length}})
$result=[ordered]@{schema='mcla-delivery-transition-regression-v1';task='M6-014';known_issue='KI-024';decision='delivery-transition-current-artifact-pass';suite_id=$SuiteRun;sdk_version=$suite.sdk_version;sdk_commit=$suite.sdk_commit;build_configuration='Release';run_id=$runId;active_confirmation_utc=$activeUtc.ToString('O');stable_confirmation_utc=$stableUtc.ToString('O');stability_seconds=$StabilitySeconds;seed_save_sha256=(Hash (Join-Path $seed $saveRelative));seed_header_sha256=(Hash (Join-Path $seed $headerRelative));save_after_sha256=(Hash (Join-Path $user $saveRelative));header_after_sha256=(Hash (Join-Path $user $headerRelative));release_artifacts=$after;runtime_logs=$logManifest;runtime_fatal_markers=$fatal;controlled_external_close=$true;exit_code=0;force_cleanup=$false;save_archive=(Tree $archive);scope=[ordered]@{delivery_transition_physically_confirmed=$true;active_delivery_gameplay_physically_confirmed=$true;five_minute_stability_physically_confirmed=($StabilitySeconds-ge300);full_delivery_completion_claimed=$false;two_hour_soak_claimed=$false}}
WriteJson (Join-Path $root 'result.json') $result
Write-Host 'M6-014 DELIVERY [3/4]: runtime health, save preservation, and artifact immutability verified.' -ForegroundColor Cyan
$final=&$verify -ResultPath (Join-Path $root 'result.json')
Write-Host 'M6-014 DELIVERY [4/4]: PASS - KI-024 physical regression evidence persisted and revalidated.' -ForegroundColor Green
Write-Host "Result: '$root\result.json'." -ForegroundColor Green
$final
