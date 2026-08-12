[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game',
    [string]$AcceptedM4ResultPath =
        'private/evidence/M4-002/20260812-085022-cc01a857/result.json'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-intro-route-decision.ps1'
$m4Verifier = Join-Path $PSScriptRoot 'verify-render-path-smoke.ps1'
$gameVerifier = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$skipVerifier = Join-Path $PSScriptRoot 'verify-skip-intro-decision.ps1'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$windowPattern = '^mcla \[rexglue-v[^\]]+\]$'
$completionMarker = 'MCLA graphics: nontrivial guest frame captured '
$artifactNames = @('mcla.exe','rexruntimerd.dll','TracyClientrd.dll','rexgpu-xenosrd.dll')

if (-not ('MclaIntroRouteNativeWindow' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class MclaIntroRouteNativeWindow {
  private delegate bool EnumWindowsProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc c, IntPtr p);
  [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr h, out uint p);
  [DllImport("user32.dll")] private static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] private static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] private static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  public static IntPtr[] Handles(int pid) {
    var a=new List<IntPtr>(); EnumWindows(delegate(IntPtr h,IntPtr x){uint p; GetWindowThreadProcessId(h,out p);
      if(p==(uint)pid && IsWindowVisible(h)) a.Add(h); return true;},IntPtr.Zero); return a.ToArray(); }
  public static string Title(IntPtr h) { var s=new StringBuilder(1024); int n=GetWindowText(h,s,s.Capacity); return n>0?s.ToString(0,n):String.Empty; }
  public static bool Close(IntPtr h) { return PostMessage(h,0x0010,IntPtr.Zero,IntPtr.Zero); }
}
'@
}

function Get-ContainedPath {
    param([string]$Path,[string]$Description,[switch]$MustExist)
    $candidate = if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repoRoot $Path }
    $full = [IO.Path]::GetFullPath($candidate); $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay inside the repository."
    }
    $current=$repoRoot
    foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object Length)) {
        $current=Join-Path $current $part
        if ((Test-Path -LiteralPath $current) -and ((Get-Item $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "$Description traverses a reparse point."
        }
    }
    if($MustExist -and -not (Test-Path -LiteralPath $full)){throw "$Description is missing."}
    $full
}

function Get-TreeSnapshot {
    param([string]$Root)
    $items=@(Get-ChildItem $Root -Recurse -Force); $entries=@(); $files=@($items|Where-Object{-not $_.PSIsContainer}|Sort-Object FullName)
    foreach($item in @((Get-Item $Root -Force))+$items){if($item.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Tree contains a reparse point.'}}
    foreach($d in @($items|Where-Object PSIsContainer|Sort-Object FullName)){$entries+=[ordered]@{kind='directory';path=$d.FullName.Substring($Root.Length).TrimStart('\').Replace('\','/')}}
    foreach($f in $files){$entries+=[ordered]@{kind='file';path=$f.FullName.Substring($Root.Length).TrimStart('\').Replace('\','/');length=$f.Length;sha256=(Get-FileHash $f.FullName -Algorithm SHA256).Hash}}
    $json=ConvertTo-Json -InputObject @($entries) -Depth 4 -Compress; $sha=[Security.Cryptography.SHA256]::Create()
    try{$hash=-join($sha.ComputeHash($utf8.GetBytes($json))|ForEach-Object{$_.ToString('X2')})}finally{$sha.Dispose()}
    $bytes=0L;foreach($f in $files){$bytes+=[long]$f.Length}
    [pscustomobject]@{Hash=$hash;FileCount=$files.Count;DirectoryCount=@($items|Where-Object PSIsContainer).Count;Bytes=$bytes}
}

function Get-GameIdentity {
    param([string]$Root)
    $t=Get-TreeSnapshot $Root;$v=& $gameVerifier -GamePath $Root -VerifyHashes
    [ordered]@{file_count=$v.FileCount;payload_bytes=$v.PayloadBytes;hashes_verified=$v.HashesVerified;manifest_sha256=(Get-FileHash $v.ManifestPath -Algorithm SHA256).Hash;tree_sha256=$t.Hash;tree_file_count=$t.FileCount;tree_directory_count=$t.DirectoryCount;tree_bytes=$t.Bytes}
}
function Get-Artifacts { param([string]$Root) @($artifactNames|ForEach-Object{$p=Get-ContainedPath (Join-Path $Root $_) "Artifact $_" -MustExist;[ordered]@{name=$_;sha256=(Get-FileHash $p -Algorithm SHA256).Hash}}) }
function Get-Targets { param([string]$Exe) @((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{[string]::Equals($_.Path,$Exe,[StringComparison]::OrdinalIgnoreCase)}catch{$false}}) }
function Assert-NoTarget {param([string]$Exe) if(@(Get-Targets $Exe).Count){throw 'Exact canonical MCLA process is already running.'}}
function Get-LogText {
    param([string]$Current)
    $dir=Split-Path -Parent $Current;if(-not(Test-Path $dir)){return ''};$a=@()
    foreach($f in @(Get-ChildItem $dir -File -Filter 'mcla*.log')){if($f.Name-ceq'mcla.log'){$a+=[pscustomobject]@{N=-1;F=$f}}elseif($f.Name-match'^mcla\.([1-9][0-9]*)\.log$'){$a+=[pscustomobject]@{N=[int]$Matches[1];F=$f}}}
    (@($a|Sort-Object N -Descending|ForEach-Object{try{[IO.File]::ReadAllText($_.F.FullName)}catch{''}})-join[Environment]::NewLine)
}
function Close-ExactWindow {param([Diagnostics.Process]$Process)$m=@();foreach($h in [MclaIntroRouteNativeWindow]::Handles($Process.Id)){if([regex]::IsMatch([MclaIntroRouteNativeWindow]::Title($h),$windowPattern)){$m+=$h}};if($m.Count-ne1-or-not[MclaIntroRouteNativeWindow]::Close($m[0])){throw 'Exact PID/window WM_CLOSE contract failed.'}}
function Assert-Prior {param([Collections.ArrayList]$Prior)foreach($p in $Prior){if((Get-TreeSnapshot $p.Root).Hash-ne$p.Hash){throw 'A prior cycle mutated.'}}}

$build=Get-ContainedPath $BuildRoot 'Build root' -MustExist;$game=Get-ContainedPath $GameRoot 'Game root' -MustExist
$accepted=Get-ContainedPath $AcceptedM4ResultPath 'Accepted M4-002 result' -MustExist
if(-not[string]::Equals($build,[IO.Path]::GetFullPath((Join-Path $repoRoot 'out/build/win-amd64-relwithdebinfo')),[StringComparison]::OrdinalIgnoreCase)-or
   -not[string]::Equals($game,[IO.Path]::GetFullPath((Join-Path $repoRoot 'private/game')),[StringComparison]::OrdinalIgnoreCase)-or
   -not[string]::Equals($accepted,[IO.Path]::GetFullPath((Join-Path $repoRoot 'private/evidence/M4-002/20260812-085022-cc01a857/result.json')),[StringComparison]::OrdinalIgnoreCase)){throw 'M4-003 final gate requires canonical inputs.'}
foreach($p in @($verifier,$m4Verifier,$gameVerifier,$skipVerifier,(Join-Path $build 'mcla.exe'),(Join-Path $game 'default.xex'))){[void](Get-ContainedPath $p 'Required input' -MustExist)}
$skip=&$skipVerifier;if(-not$skip.Passed-or$skip.PatchImplemented-or$skip.PatchEnabled-or-not$skip.PriorAddressAudit){throw 'Prior no-patch decision failed.'}
$m4=&$m4Verifier -ResultPath $accepted;if(-not$m4.Passed-or$m4.Cycles-ne10){throw 'Accepted M4-002 result failed physical re-verification.'}
$exe=Join-Path $build 'mcla.exe';Assert-NoTarget $exe
$gameBefore=Get-GameIdentity $game;$artifactsBefore=Get-Artifacts $build
$evidence=Get-ContainedPath 'private/evidence/M4-003' 'M4-003 evidence root';[IO.Directory]::CreateDirectory($evidence)|Out-Null
$runRoot=Join-Path $evidence ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8));$runs=Join-Path $runRoot 'runs';[IO.Directory]::CreateDirectory($runs)|Out-Null
$records=@();$prior=[Collections.ArrayList]::new()
for($cycle=1;$cycle-le3;$cycle++){
    Assert-Prior $prior;Assert-NoTarget $exe;$name='{0:D2}'-f$cycle;$root=Join-Path $runs $name;$user=Join-Path $root 'user';$cache=Join-Path $root 'cache';[IO.Directory]::CreateDirectory($user)|Out-Null;[IO.Directory]::CreateDirectory($cache)|Out-Null
    $log=Join-Path $root 'mcla.log';$bmp=Join-Path $user 'mcla-first-frame.bmp';$process=$null;$forced=$false
    try{
        $capture=[Diagnostics.Stopwatch]::StartNew();$process=Start-Process $exe -ArgumentList @('--gpu_render_audit=true','--async_shader_compilation=false','--mcla_first_frame_probe=true','--mcla_first_frame_settle_seconds=35','--render_target_path_d3d12=rtv','--log_noisy=true','--log_max_file_size_mb=5','--log_max_files=15',"--game_data_root=`"$game`"","--user_data_root=`"$user`"","--cache_root=`"$cache`"","--log_file=`"$log`"",'--log_level=trace','--fullscreen=false') -WorkingDirectory $build -PassThru
        $ready=$false;while($capture.ElapsedMilliseconds-lt60000){if($process.HasExited){throw 'Process exited before title capture.'};if((Test-Path $bmp)-and((Get-LogText $log).Contains($completionMarker))){$ready=$true;break};Start-Sleep -Milliseconds 250};$capture.Stop();if(-not$ready){throw '60-second title capture deadline expired.'}
        $dwell=[Diagnostics.Stopwatch]::StartNew();while($dwell.ElapsedMilliseconds-lt2000){Start-Sleep -Milliseconds 50};$dwell.Stop();if($process.HasExited){throw 'Process exited during dwell.'}
        Close-ExactWindow $process;$exit=[Diagnostics.Stopwatch]::StartNew();$signaled=$process.WaitForExit(10000);$exit.Stop();if(-not$signaled-or$process.ExitCode-ne0){throw 'Controlled exit contract failed.'};Assert-NoTarget $exe
        $probe=&$verifier -ProbeOnly -RuntimeLogPath $log -BmpPath $bmp;$ut=Get-TreeSnapshot $user;$ct=Get-TreeSnapshot $cache
        $records+=[ordered]@{index=$cycle;capture_elapsed_milliseconds=$capture.ElapsedMilliseconds;dwell_elapsed_milliseconds=$dwell.ElapsedMilliseconds;exit_elapsed_milliseconds=$exit.ElapsedMilliseconds;exit_code=0;close_requested=$true;harness_force_cleanup=$false;process_signal_confirmed=$true;process_cleanup_confirmed=$true;prior_cycles_immutable=$true;runtime_logs=@($probe.LogSet.Files);runtime_log_file_count=$probe.LogSet.Count;runtime_log_bytes=$probe.LogSet.Bytes;runtime_log_set_sha256=$probe.LogSet.Hash;capture_relative_path="runs/$name/user/mcla-first-frame.bmp";capture_sha256=$probe.M4.Bmp.Sha256;capture_bytes=$probe.M4.Bmp.Bytes;preflight_resolution_count=$probe.PreflightCount;post_launch_bink_count=$probe.PostLaunchBinkCount;nt_create_file_count=$probe.NtCreateFileCount;nt_read_file_count=$probe.NtReadFileCount;nt_read_file_success_count=$probe.NtReadFileSuccessCount;title_coverage_success_count=$probe.TitleCoverageSuccessCount;logo_edge_correlation_ppm=$probe.M4.Roi.LogoCorrelationPpm;press_edge_correlation_ppm=$probe.M4.Roi.PressCorrelationPpm;resolve_calls=$probe.M4.Audit.ResolveCalls;draw_issued=$probe.M4.Audit.DrawIssued;user_tree_sha256=$ut.Hash;cache_tree_sha256=$ct.Hash;cycle_tree_sha256=('0'*64);user_file_count=$ut.FileCount;cache_file_count=$ct.FileCount;user_bytes=$ut.Bytes;cache_bytes=$ct.Bytes}
    }catch{$failure=$_;if($process-and-not$process.HasExited){$forced=$true;Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue;$null=$process.WaitForExit(5000)};if($forced){throw "M4-003 failure required force cleanup. $($failure.Exception.Message) Private run: '$runRoot'."};throw}
    $tree=Get-TreeSnapshot $root;$records[-1].cycle_tree_sha256=$tree.Hash;[void]$prior.Add([pscustomobject]@{Root=$root;Hash=$tree.Hash})
}
Assert-Prior $prior;Assert-NoTarget $exe;$gameAfter=Get-GameIdentity $game;$artifactsAfter=Get-Artifacts $build
$result=[ordered]@{schema=1;task='M4-003';decision='guest-bypass-no-patch';claim='title-reached-with-zero-post-launch-bink-not-playback-proof';cycle_count=3;capture_timeout_seconds=60;first_frame_settle_seconds=35;post_marker_dwell_milliseconds=2000;exit_timeout_seconds=10;accepted_m4_002=[ordered]@{name='M4-002-result.json';sha256=(Get-FileHash $accepted -Algorithm SHA256).Hash;cycles=10;verified=$true};game_identity=[ordered]@{before=$gameBefore;after=$gameAfter};artifacts=[ordered]@{before=$artifactsBefore;after=$artifactsAfter};cycles=$records;patch_implemented=$false;patch_enabled=$false;prior_word_audit=$true;all_write_roots_contained=$true;all_prior_cycles_immutable=$true;no_surviving_processes=$true;data_integrity_preserved=$true;all_title_probes_passed=$true;zero_post_launch_bink=$true}
$resultPath=Join-Path $runRoot 'result.json';[IO.File]::WriteAllText($resultPath,(($result|ConvertTo-Json -Depth 10)+[Environment]::NewLine),$utf8)
$verified=&$verifier -ResultPath $resultPath
[pscustomobject]@{Passed=$verified.Passed;Decision=$verified.Decision;PlaybackProven=$verified.PlaybackProven;Cycles=$verified.Cycles;PrivateRunRoot=$runRoot;ResultPath=$resultPath}
