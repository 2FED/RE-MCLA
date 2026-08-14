[CmdletBinding(DefaultParameterSetName = 'Run')]
param(
    [Parameter(ParameterSetName = 'Run')][string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [Parameter(ParameterSetName = 'Run')][string]$GameRoot = 'private/game',
    [Parameter(Mandatory, ParameterSetName = 'Finalize')][string]$FinalizeExistingRun,
    [Parameter(Mandatory, ParameterSetName = 'Finalize')][switch]$VisualPass
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-rendering-smoke.ps1'
$gameVerify = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$cmake = $toolchain.CMakePath
$utf8 = [Text.UTF8Encoding]::new($false)
$baselineResultRelative = 'private/evidence/M5-002/20260814-093131-ddca5b9d/result.json'
$baselineResultHash = 'A582A41BB254D9187BC6F8147E89E2FF2E92D0EA4B79945760BA6FBCF334BC28'
$xeniaRelative = 'private/tools/xenia-canary/artifacts/screenshots/545407F8/545407F8 - 2026-08-11T00-53-02.png'
$xeniaHash = 'A490553067A8F375191F618C34556B82C9FD244FF295519AB4383AAB40434F8B'
$phases = @('world','sky','street','particle-a','particle-b','particle-c')
$trafficPhases = @(1..30 | ForEach-Object { 'traffic-{0:D2}' -f $_ })

if (-not ('MclaRenderingWindow' -as [type])) { Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;public static class MclaRenderingWindow{delegate bool E(IntPtr h,IntPtr p);[DllImport("user32.dll")]static extern bool EnumWindows(E c,IntPtr p);[DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);[DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr h);[DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetWindowText(IntPtr h,StringBuilder s,int n);[DllImport("user32.dll")]static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);public static IntPtr[] Handles(int pid){var a=new List<IntPtr>();EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}public static string Title(IntPtr h){var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty;}public static bool Close(IntPtr h){return PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);}}
'@ }

function Safe([string]$Path,[string]$Description,[switch]$Exists) { $full=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($Path)){$Path}else{Join-Path $repo $Path}));$prefix=$repo.TrimEnd('\')+'\';if(-not$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description escapes repository."};$current=$repo;foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object Length)){$current=Join-Path $current $part;if((Test-Path $current)-and((Get-Item $current -Force).Attributes-band[IO.FileAttributes]::ReparsePoint)){throw "$Description traverses a reparse point."}};if($Exists-and-not(Test-Path $full)){throw "$Description is missing."};$full }
function NoReparse([string]$Root) { $pending=[Collections.Generic.Stack[string]]::new();$pending.Push($Root);while($pending.Count){foreach($item in @(Get-ChildItem -LiteralPath $pending.Pop() -Force)){if($item.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Tree contains a reparse point.'};if($item.PSIsContainer){$pending.Push($item.FullName)}}} }
function Tree([string]$Root) { $root=Safe $Root 'Tree' -Exists;NoReparse $root;$items=@(Get-ChildItem $root -Recurse -Force);$files=@($items|Where-Object{-not$_.PSIsContainer}|Sort-Object FullName);$entries=@();foreach($directory in @($items|Where-Object PSIsContainer|Sort-Object FullName)){$entries+=[ordered]@{kind='directory';path=$directory.FullName.Substring($root.Length).TrimStart('\').Replace('\','/')}};foreach($file in $files){$entries+=[ordered]@{kind='file';path=$file.FullName.Substring($root.Length).TrimStart('\').Replace('\','/');length=$file.Length;sha256=(Get-FileHash $file.FullName -Algorithm SHA256).Hash}};$json=ConvertTo-Json -InputObject @($entries) -Compress -Depth 4;$sha=[Security.Cryptography.SHA256]::Create();try{$hash=-join($sha.ComputeHash($utf8.GetBytes($json))|ForEach-Object{$_.ToString('X2')})}finally{$sha.Dispose()};$bytes=0L;foreach($file in $files){$bytes+=$file.Length};[pscustomobject]@{Hash=$hash;FileCount=$files.Count;DirectoryCount=@($items|Where-Object PSIsContainer).Count;Bytes=$bytes} }
function Game([string]$Root) { $tree=Tree $Root;$v=&$gameVerify -GamePath $Root -VerifyHashes;[ordered]@{file_count=$v.FileCount;payload_bytes=$v.PayloadBytes;manifest_sha256=(Get-FileHash $v.ManifestPath -Algorithm SHA256).Hash;tree_sha256=$tree.Hash;tree_file_count=$tree.FileCount;tree_directory_count=$tree.DirectoryCount;tree_bytes=$tree.Bytes} }
function Artifacts([string]$Root) { @('mcla.exe','rexruntimerd.dll','TracyClientrd.dll','rexgpu-xenosrd.dll')|ForEach-Object{[ordered]@{name=$_;sha256=(Get-FileHash (Safe (Join-Path $Root $_) "Artifact $_" -Exists)-Algorithm SHA256).Hash}} }
function Processes([string]$Exe) { @((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{[string]::Equals($_.Path,$Exe,[StringComparison]::OrdinalIgnoreCase)}catch{$false}}) }
function Contains([string]$Directory,[string]$Needle) { foreach($file in @(Get-ChildItem $Directory -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue)){try{$stream=[IO.File]::Open($file.FullName,'Open','Read','ReadWrite');try{$reader=[IO.StreamReader]::new($stream);$text=$reader.ReadToEnd();$reader.Dispose()}finally{$stream.Dispose()};if($text.Contains($Needle)){return $true}}catch{}};$false }
function Close-Exact([Diagnostics.Process]$Process) { $matches=@();foreach($handle in [MclaRenderingWindow]::Handles($Process.Id)){if([regex]::IsMatch([MclaRenderingWindow]::Title($handle),'^mcla \[rexglue-v[^\]]+\]$')){$matches+=$handle}};if($matches.Count-ne1-or-not[MclaRenderingWindow]::Close($matches[0])){throw 'Exact PID/window WM_CLOSE failed.'} }
function Logged([scriptblock]$Command,[string]$Log,[switch]$Append) { $prior=$ErrorActionPreference;$ErrorActionPreference='Continue';try{if($Append){&$Command *>&1|Tee-Object $Log -Append|Out-Null}else{&$Command *>&1|Tee-Object $Log|Out-Null};$code=$LASTEXITCODE}finally{$ErrorActionPreference=$prior};$code }

function New-ContactSheet([string]$Output,[string]$Xenia,[hashtable]$Captures) {
    Add-Type -AssemblyName System.Drawing
    $sheet=[Drawing.Bitmap]::new(1280,450,[Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $graphics=[Drawing.Graphics]::FromImage($sheet);$graphics.Clear([Drawing.Color]::FromArgb(24,24,24));$graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $font=[Drawing.Font]::new('Segoe UI',12,[Drawing.FontStyle]::Bold);$brush=[Drawing.Brushes]::White
    $tiles=@([pscustomobject]@{Label='Xenia reference';Path=$Xenia})
    foreach($phase in @('world','sky','street','traffic','particle-a','particle-b','particle-c')){$tiles+=[pscustomobject]@{Label="Native $phase";Path=$Captures[$phase]}}
    try { for($i=0;$i-lt$tiles.Count;$i++){ $column=$i%4;$row=[Math]::Floor($i/4);$x=$column*320;$y=$row*225;$graphics.DrawString($tiles[$i].Label,$font,$brush,$x+6,$y+3);$image=[Drawing.Image]::FromFile($tiles[$i].Path);try{$graphics.DrawImage($image,[Drawing.Rectangle]::new($x,$y+25,320,180))}finally{$image.Dispose()}};$sheet.Save($Output,[Drawing.Imaging.ImageFormat]::Png) } finally { $font.Dispose();$graphics.Dispose();$sheet.Dispose() }
}

if($PSCmdlet.ParameterSetName-eq'Finalize'){
    if(-not$VisualPass){throw 'Finalize requires an explicit owner VisualPass.'}
    $evidence=Safe 'private/evidence/M5-003' 'Evidence root' -Exists
    if($FinalizeExistingRun-notmatch'^[0-9]{8}-[0-9]{6}-[0-9a-f]{8}$'){throw 'FinalizeExistingRun is not a canonical run id.'}
    $root=Safe (Join-Path $evidence $FinalizeExistingRun) 'Candidate root' -Exists
    $candidate=Safe (Join-Path $root 'candidate.json') 'Candidate result' -Exists
    $raw=[IO.File]::ReadAllText($candidate);if($raw-match'(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])'){throw 'Candidate contains a private or absolute path.'}
    $record=$raw|ConvertFrom-Json
    if($record.task-cne'M5-003'-or$record.visual_review.status-cne'pending'-or$record.visual_review.reviewer-cne'none'){throw 'Candidate is not pending owner review.'}
    $cycle=Safe (Join-Path $root 'runs\01') 'Candidate cycle' -Exists;$null=&$verify -ProbeOnly -RuntimeLogPath (Join-Path $cycle 'mcla.log') -UserRoot (Join-Path $cycle 'user')
    if(-not$record.cycle.PSObject.Properties['tree_sha256']){$cycleTree=Tree $cycle;$record.cycle|Add-Member -NotePropertyName tree_sha256 -NotePropertyValue $cycleTree.Hash;$record.cycle|Add-Member -NotePropertyName tree_file_count -NotePropertyValue $cycleTree.FileCount;$record.cycle|Add-Member -NotePropertyName tree_directory_count -NotePropertyValue $cycleTree.DirectoryCount;$record.cycle|Add-Member -NotePropertyName tree_bytes -NotePropertyValue $cycleTree.Bytes}
    $record.visual_review.status='pass';$record.visual_review.reviewer='owner'
    foreach($category in @('road','buildings','player_vehicle','traffic','night_sky','shadows','particles','hud')){$record.visual_review.categories.$category=$true}
    $result=Join-Path $root 'result.json';if(Test-Path $result){throw 'Final result already exists; refusing to overwrite it.'};[IO.File]::WriteAllText($result,(ConvertTo-Json $record -Depth 14)+[Environment]::NewLine,$utf8)
    try{$final=&$verify -ResultPath $result}catch{Remove-Item $result -Force -ErrorAction SilentlyContinue;throw}
    [pscustomobject]@{Passed=$final.Passed;Decision=$final.Decision;OwnerVisualPass=$final.OwnerVisualPass;PrivateRunRoot=$root;ResultPath=$result}
    return
}

$build=Safe $BuildRoot 'Build root' -Exists;$game=Safe $GameRoot 'Game root' -Exists
if($build-cne(Safe 'out/build/win-amd64-relwithdebinfo' 'Canonical build')-or$game-cne(Safe 'private/game' 'Canonical game')){throw 'M5-003 requires canonical inputs.'}
$baseline=Safe $baselineResultRelative 'M5-002 baseline result' -Exists;$xenia=Safe $xeniaRelative 'Xenia rendering reference' -Exists
if((Get-FileHash $baseline -Algorithm SHA256).Hash-cne$baselineResultHash-or(Get-FileHash $xenia -Algorithm SHA256).Hash-cne$xeniaHash){throw 'Pinned rendering prerequisite identity failed.'}
$seed=Safe 'private/baseline/M4-011/post-oobe-profile' 'Pinned post-OOBE seed' -Exists;$seedBefore=Tree $seed
if($seedBefore.FileCount-ne2-or(Get-FileHash (Join-Path $seed 'B13EBABEBABEBABE\545407F8\00000001\mc4.sav\mc4.sav')-Algorithm SHA256).Hash-cne'E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68'){throw 'Pinned saved-game seed failed identity.'}
$evidence=Safe 'private/evidence/M5-003' 'Evidence root';[IO.Directory]::CreateDirectory($evidence)|Out-Null
$root=Join-Path $evidence ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8));[IO.Directory]::CreateDirectory($root)|Out-Null
$buildLog=Join-Path $root 'relwithdebinfo-clean-build.log'

Write-Host 'M5-003 [1/7]: validating pinned gameplay and emulator rendering references...' -ForegroundColor Cyan
$gameBefore=Game $game
Write-Host 'M5-003 [2/7]: clean-building the MCLA host...' -ForegroundColor Cyan
if(Logged {&$cmake --preset win-amd64-relwithdebinfo} $buildLog){throw 'App configure failed.'};if(Logged {&$cmake --build --preset win-amd64-relwithdebinfo --target mcla --clean-first --parallel 8} $buildLog -Append){throw "App clean build failed. Private run: '$root'."}
$exe=Safe (Join-Path $build 'mcla.exe') 'Executable' -Exists;if(@(Processes $exe).Count){throw 'Canonical MCLA is already running.'};$artifactsBefore=@(Artifacts $build)

Write-Host 'M5-003 [3/7]: launching the deterministic saved-game rendering route...' -ForegroundColor Cyan
$cycle=Join-Path $root 'runs\01';$user=Join-Path $cycle 'user';$cache=Join-Path $cycle 'cache';[IO.Directory]::CreateDirectory($user)|Out-Null;[IO.Directory]::CreateDirectory($cache)|Out-Null;Get-ChildItem $seed -Force|Copy-Item -Destination $user -Recurse -Force
if((Tree $user).Hash-cne$seedBefore.Hash){throw 'Cycle did not start from the exact pinned save.'}
$log=Join-Path $cycle 'mcla.log';$process=$null;$forced=$false
try{
    $args=@('--mcla_rendering_smoke_probe=true','--mcla_first_frame_settle_seconds=45','--mcla_frontend_gameplay_wait_seconds=45','--gpu_render_audit=true','--async_shader_compilation=false','--render_target_path_d3d12=rtv','--log_max_file_size_mb=8','--log_max_files=15','--log_level=trace','--fullscreen=false',"--game_data_root=$game","--user_data_root=$user","--cache_root=$cache","--log_file=$log")
    $process=Start-Process $exe -ArgumentList $args -WorkingDirectory $build -PassThru
    $deadline=[DateTime]::UtcNow.AddSeconds(200)
    while([DateTime]::UtcNow-lt$deadline){if($process.HasExited){throw "Process exited before rendering PASS (exit $($process.ExitCode)). Private run: '$root'."};if(Contains $cycle 'XENOS_AUDIT_RESOLVE_SUMMARY v=1 phase=checkpoint'){break};Start-Sleep -Milliseconds 250}
    if(-not(Contains $cycle 'MCLA_RENDER_SMOKE_SUMMARY v=1 status=PASS')-or-not(Contains $cycle 'XENOS_AUDIT_RESOLVE_SUMMARY v=1 phase=checkpoint')){throw "Rendering deadline expired. Private run: '$root'."}
    Close-Exact $process;if(-not$process.WaitForExit(10000)-or$process.ExitCode-ne0){throw 'Controlled external WM_CLOSE failed.'};if(@(Processes $exe).Count){throw 'Exact-path MCLA process survived close.'}
}catch{$failure=$_;if($process-and-not$process.HasExited){$forced=$true;Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue;$null=$process.WaitForExit(5000)};if($forced){throw "M5-003 failure required force cleanup. $($failure.Exception.Message) Private run: '$root'."};throw}

Write-Host 'M5-003 [4/7]: validating rendering chronology, GPU semantics, and 36 physical frames...' -ForegroundColor Cyan
$probe=&$verify -ProbeOnly -RuntimeLogPath $log -UserRoot $user
$capturePaths=@{};$captures=[ordered]@{};foreach($phase in $phases){$capturePaths[$phase]=Join-Path $user "mcla-render-$phase.bmp";$captures[$phase]=[ordered]@{sha256=$probe.Bmps[$phase].Sha256;bytes=$probe.Bmps[$phase].Bytes}}
$capturePaths.traffic=Join-Path $user "mcla-render-$($probe.TrafficSelected).bmp";$trafficCaptures=@();foreach($phase in $trafficPhases){$trafficCaptures+=[ordered]@{name=$phase;sha256=$probe.TrafficBmps[$phase].Sha256;bytes=$probe.TrafficBmps[$phase].Bytes}}
Write-Host 'M5-003 [5/7]: creating a private labeled Xenia/native contact sheet...' -ForegroundColor Cyan
$contact=Join-Path $user 'rendering-contact-sheet.png';New-ContactSheet $contact $xenia $capturePaths
Write-Host 'M5-003 [6/7]: checking source-game, runtime-artifact, and seed integrity...' -ForegroundColor Cyan
$gameAfter=Game $game;$artifactsAfter=@(Artifacts $build);$seedAfter=Tree $seed
if(($gameBefore|ConvertTo-Json -Compress)-cne($gameAfter|ConvertTo-Json -Compress)-or($artifactsBefore|ConvertTo-Json -Compress)-cne($artifactsAfter|ConvertTo-Json -Compress)-or$seedBefore.Hash-cne$seedAfter.Hash){throw 'Source-game, runtime-artifact, or pinned-seed identity changed.'}
$cycleTree=Tree $cycle
$candidate=[ordered]@{schema=1;task='M5-003';decision='rendering-categories-pass';sdk_version='0.9.0.18';route_id='pinned-save-sunset-strip-rendering-v1';baseline=[ordered]@{m5_002_result_sha256=$baselineResultHash;xenia_free_roam_sha256=$xeniaHash};build=[ordered]@{app_build_log_sha256=(Get-FileHash $buildLog -Algorithm SHA256).Hash;executable_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash};seed=[ordered]@{before_sha256=$seedBefore.Hash;after_sha256=$seedAfter.Hash;file_count=$seedBefore.FileCount};game_identity=[ordered]@{before=$gameBefore;after=$gameAfter};artifacts=[ordered]@{before=$artifactsBefore;after=$artifactsAfter};cycle=[ordered]@{relative_root='runs/01';exit_code=0;close_requested=$true;harness_force_cleanup=$false;runtime_logs=@($probe.LogSet.Files);runtime_log_set_sha256=$probe.LogSet.Hash;runtime_log_file_count=$probe.LogSet.Count;runtime_log_bytes=$probe.LogSet.Bytes;captures=$captures;traffic_captures=$trafficCaptures;traffic_selected_name=$probe.TrafficSelected;traffic_difference_pixels=$probe.TrafficDifference;contact_sheet_sha256=(Get-FileHash $contact -Algorithm SHA256).Hash;sky_difference_pixels=$probe.SkyDifference;particle_ab_difference_pixels=$probe.ParticleAB;particle_bc_difference_pixels=$probe.ParticleBC;pso_ok=$probe.PsoOk;draws=$probe.Draws;resolves=$probe.Resolves;shader_record_overflow=$probe.ShaderOverflow;tree_sha256=$cycleTree.Hash;tree_file_count=$cycleTree.FileCount;tree_directory_count=$cycleTree.DirectoryCount;tree_bytes=$cycleTree.Bytes};visual_review=[ordered]@{status='pending';reviewer='none';categories=[ordered]@{road=$false;buildings=$false;player_vehicle=$false;traffic=$false;night_sky=$false;shadows=$false;particles=$false;hud=$false};known_minor_issue='KI-013-green-vehicle-shadow-nonblocking'};no_surviving_processes=$true;data_integrity_preserved=$true}
$candidatePath=Join-Path $root 'candidate.json';[IO.File]::WriteAllText($candidatePath,(ConvertTo-Json $candidate -Depth 14)+[Environment]::NewLine,$utf8)
Write-Host 'M5-003 [7/7]: candidate is physically complete and awaits owner visual classification.' -ForegroundColor Yellow
[pscustomobject]@{Passed=$false;Decision='awaiting-owner-visual-review';Frames=36;PsoOk=$probe.PsoOk;Draws=$probe.Draws;Resolves=$probe.Resolves;PrivateRunRoot=$root;CandidatePath=$candidatePath;ContactSheet=$contact;FinalizeCommand=".\scripts\run-rendering-smoke.ps1 -FinalizeExistingRun '$([IO.Path]::GetFileName($root))' -VisualPass"}
