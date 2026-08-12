[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-intro-route-decision.ps1'
$runner = Join-Path $PSScriptRoot 'run-intro-route-decision.ps1'
$source = Join-Path $repoRoot 'private/evidence/M4-002/20260812-085022-cc01a857/runs/01'
$accepted = Join-Path $repoRoot 'private/evidence/M4-002/20260812-085022-cc01a857/result.json'
$utf8 = [Text.UTF8Encoding]::new($false)
$fixtureRoot = Join-Path $repoRoot ('private/evidence/M4-003/test-' + [guid]::NewGuid().ToString('N').Substring(0,8))

function Expect-Failure {
    param([scriptblock]$Action,[string]$Label)
    try { & $Action | Out-Null; throw "Negative fixture '$Label' was accepted." }
    catch { if ($_.Exception.Message -eq "Negative fixture '$Label' was accepted.") { throw } }
}
function Get-TreeSnapshot {
    param([string]$Root)
    $items=@(Get-ChildItem $Root -Recurse -Force);$entries=@();$files=@($items|Where-Object{-not$_.PSIsContainer}|Sort-Object FullName)
    foreach($i in @((Get-Item $Root -Force))+$items){if($i.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'fixture reparse'}}
    foreach($d in @($items|Where-Object PSIsContainer|Sort-Object FullName)){$entries+=[ordered]@{kind='directory';path=$d.FullName.Substring($Root.Length).TrimStart('\').Replace('\','/')}}
    foreach($f in $files){$entries+=[ordered]@{kind='file';path=$f.FullName.Substring($Root.Length).TrimStart('\').Replace('\','/');length=$f.Length;sha256=(Get-FileHash $f.FullName -Algorithm SHA256).Hash}}
    $json=ConvertTo-Json -InputObject @($entries) -Depth 4 -Compress;$sha=[Security.Cryptography.SHA256]::Create();try{$hash=-join($sha.ComputeHash($utf8.GetBytes($json))|ForEach-Object{$_.ToString('X2')})}finally{$sha.Dispose()};$bytes=0L;foreach($f in $files){$bytes+=$f.Length};[pscustomobject]@{Hash=$hash;FileCount=$files.Count;DirectoryCount=@($items|Where-Object PSIsContainer).Count;Bytes=$bytes}
}
function Get-LogText {
    $parts=@();foreach($n in 3,2,1){$p=Join-Path $source "mcla.$n.log";if(Test-Path $p){$parts+=[IO.File]::ReadAllText($p)}};$parts+=[IO.File]::ReadAllText((Join-Path $source 'mcla.log'));$parts-join[Environment]::NewLine
}
function Write-LogSet {
    param([string]$Root,[string]$Text)
    $chunks=[Collections.Generic.List[string]]::new();$builder=[Text.StringBuilder]::new()
    foreach($line in $Text -split "`r?`n"){
        if($builder.Length+$line.Length+2-gt4000000){$chunks.Add($builder.ToString());$null=$builder.Clear()}
        $null=$builder.AppendLine($line)
    }
    if($builder.Length){$chunks.Add($builder.ToString())};$count=$chunks.Count
    for($i=0;$i-lt$count;$i++){$name=if($i-eq$count-1){'mcla.log'}else{"mcla.$($count-1-$i).log"};$path=Join-Path $Root $name;[IO.File]::WriteAllText($path,$chunks[$i],$utf8);(Get-Item $path).LastWriteTimeUtc=([datetime]'2026-08-12T00:00:00Z').AddSeconds($i)}
}
function Make-Probe {
    param([string]$Root,[ValidateSet('positive','misplaced','extra-prelaunch-bik','bink','noisy','title')]$Mode='positive')
    [IO.Directory]::CreateDirectory((Join-Path $Root 'user'))|Out-Null;[IO.Directory]::CreateDirectory((Join-Path $Root 'cache'))|Out-Null
    Copy-Item (Join-Path $source 'user/mcla-first-frame.bmp') (Join-Path $Root 'user/mcla-first-frame.bmp')
    $text=Get-LogText;$launch='KernelState: Preparing module launch...';$li=$text.IndexOf($launch);if($li-lt0){throw 'source launch missing'}
    $noise=(1..100|ForEach-Object{"[NtCreateFile] path=game:\fixture$_.bin`r`n[NtReadFile] handle=0x1 len=0x10`r`n[NtReadFile] -> 0x0 (fixture)`r`n"})-join''
    $text=$text.Insert($li+$launch.Length,$noise)
    $capture='MCLA graphics: nontrivial guest frame captured ';$ci=$text.IndexOf($capture);$text=$text.Insert($ci,"[NtReadFile] handle=0x1 len=0x10`r`n[NtReadFile] -> 0x0 (title coverage)`r`n")
    switch($Mode){
      'misplaced'{$needle="VFS resolved 'game:\intro720.bik'";$at=$text.IndexOf($needle);$lineEnd=$text.IndexOf("`n",$at)+1;$line=$text.Substring($at,$lineEnd-$at);$text=$text.Remove($at,$lineEnd-$at);$li=$text.IndexOf($launch);$text=$text.Insert($li+$launch.Length,$line)}
      'extra-prelaunch-bik'{$li=$text.IndexOf($launch);$text=$text.Insert($li,"Project diagnostic resolved attract720.bik`r`n")}
      'bink'{$li=$text.IndexOf($launch);$text=$text.Insert($li+$launch.Length,"`r`nGuest opened game:\intro720.bik`r`n")}
      'noisy'{$text=$text -replace '(?m)^\[Nt(?:Create|Read)File\].*\r?\n?',''}
      'title'{$text=$text.Replace($capture,'MCLA graphics: fixture capture removed ')}
    }
    Write-LogSet $Root $text
}
function Get-Manifest {
    param([string]$Root)
    $files=@(Get-ChildItem $Root -File -Filter 'mcla*.log');$rots=@($files|Where-Object Name -ne 'mcla.log'|ForEach-Object{[pscustomobject]@{N=[int]([regex]::Match($_.Name,'\.(\d+)\.').Groups[1].Value);F=$_}}|Sort-Object N -Descending);$ordered=@($rots|ForEach-Object{$_.F})+@($files|Where-Object Name -eq 'mcla.log');$m=@($ordered|ForEach-Object{[ordered]@{name=$_.Name;bytes=$_.Length;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash}});$json=ConvertTo-Json -InputObject @($m) -Depth 3 -Compress;$sha=[Security.Cryptography.SHA256]::Create();try{$h=-join($sha.ComputeHash($utf8.GetBytes($json))|ForEach-Object{$_.ToString('X2')})}finally{$sha.Dispose()};[pscustomobject]@{Files=$m;Count=$m.Count;Bytes=($ordered|Measure-Object Length -Sum).Sum;Hash=$h}
}
function Get-GameIdentity {
    $root=Join-Path $repoRoot 'private/game';$t=Get-TreeSnapshot $root;$v=& (Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath $root -VerifyHashes
    [ordered]@{file_count=$v.FileCount;payload_bytes=$v.PayloadBytes;hashes_verified=$v.HashesVerified;manifest_sha256=(Get-FileHash $v.ManifestPath -Algorithm SHA256).Hash;tree_sha256=$t.Hash;tree_file_count=$t.FileCount;tree_directory_count=$t.DirectoryCount;tree_bytes=$t.Bytes}
}

try {
    [IO.Directory]::CreateDirectory((Join-Path $fixtureRoot 'runs'))|Out-Null
    $records=@()
    for($i=1;$i-le3;$i++){
        $name='{0:D2}'-f$i;$root=Join-Path $fixtureRoot "runs/$name";Make-Probe $root
        $probe=&$verifier -ProbeOnly -RuntimeLogPath (Join-Path $root 'mcla.log') -BmpPath (Join-Path $root 'user/mcla-first-frame.bmp')
        $manifest=Get-Manifest $root;$ut=Get-TreeSnapshot (Join-Path $root 'user');$ct=Get-TreeSnapshot (Join-Path $root 'cache')
        $records+=[ordered]@{index=$i;capture_elapsed_milliseconds=42000;dwell_elapsed_milliseconds=2000;exit_elapsed_milliseconds=100;exit_code=0;close_requested=$true;harness_force_cleanup=$false;process_signal_confirmed=$true;process_cleanup_confirmed=$true;prior_cycles_immutable=$true;runtime_logs=@($manifest.Files);runtime_log_file_count=$manifest.Count;runtime_log_bytes=[long]$manifest.Bytes;runtime_log_set_sha256=$manifest.Hash;capture_relative_path="runs/$name/user/mcla-first-frame.bmp";capture_sha256=$probe.M4.Bmp.Sha256;capture_bytes=$probe.M4.Bmp.Bytes;preflight_resolution_count=3;post_launch_bink_count=0;nt_create_file_count=$probe.NtCreateFileCount;nt_read_file_count=$probe.NtReadFileCount;nt_read_file_success_count=$probe.NtReadFileSuccessCount;title_coverage_success_count=$probe.TitleCoverageSuccessCount;logo_edge_correlation_ppm=$probe.M4.Roi.LogoCorrelationPpm;press_edge_correlation_ppm=$probe.M4.Roi.PressCorrelationPpm;resolve_calls=$probe.M4.Audit.ResolveCalls;draw_issued=$probe.M4.Audit.DrawIssued;user_tree_sha256=$ut.Hash;cache_tree_sha256=$ct.Hash;cycle_tree_sha256=('0'*64);user_file_count=$ut.FileCount;cache_file_count=$ct.FileCount;user_bytes=$ut.Bytes;cache_bytes=$ct.Bytes}
        $records[-1].cycle_tree_sha256=(Get-TreeSnapshot $root).Hash
    }
    $identity=Get-GameIdentity;$acceptedObject=Get-Content $accepted -Raw|ConvertFrom-Json;$artifacts=@($acceptedObject.artifacts.after|ForEach-Object{[ordered]@{name=$_.name;sha256=$_.sha256}})
    $result=[ordered]@{schema=1;task='M4-003';decision='guest-bypass-no-patch';claim='title-reached-with-zero-post-launch-bink-not-playback-proof';cycle_count=3;capture_timeout_seconds=60;first_frame_settle_seconds=35;post_marker_dwell_milliseconds=2000;exit_timeout_seconds=10;accepted_m4_002=[ordered]@{name='M4-002-result.json';sha256=(Get-FileHash $accepted -Algorithm SHA256).Hash;cycles=10;verified=$true};game_identity=[ordered]@{before=$identity;after=$identity};artifacts=[ordered]@{before=$artifacts;after=$artifacts};cycles=$records;patch_implemented=$false;patch_enabled=$false;prior_word_audit=$true;all_write_roots_contained=$true;all_prior_cycles_immutable=$true;no_surviving_processes=$true;data_integrity_preserved=$true;all_title_probes_passed=$true;zero_post_launch_bink=$true}
    $resultPath=Join-Path $fixtureRoot 'result.json';[IO.File]::WriteAllText($resultPath,(($result|ConvertTo-Json -Depth 10)+[Environment]::NewLine),$utf8)
    $positive=&$verifier -ResultPath $resultPath -HistoricalEvidenceOnly;if(-not$positive.Passed){throw 'Physical positive failed.'}

    foreach($mode in 'misplaced','extra-prelaunch-bik','bink','noisy','title'){$p=Join-Path $fixtureRoot "negative-$mode";Make-Probe $p $mode;Expect-Failure {&$verifier -ProbeOnly -RuntimeLogPath (Join-Path $p 'mcla.log') -BmpPath (Join-Path $p 'user/mcla-first-frame.bmp')} $mode}
    $runnerText=Get-Content $runner -Raw;$verifierText=Get-Content $verifier -Raw
    foreach($needle in @("--mcla_first_frame_settle_seconds=35","--log_noisy=true","--render_target_path_d3d12=rtv","WaitForExit(10000)","for(`$cycle=1;`$cycle-le3")){if(-not$runnerText.Contains($needle)){throw "Runner source contract missing $needle"}}
    foreach($needle in @('guest-bypass-no-patch','not-playback-proof','NtReadFileSuccessCount','TitleCoverageSuccessCount','ResultPath = $acceptedPath','HistoricalEvidenceOnly','-ProbeOnly')){if(-not$verifierText.Contains($needle)){throw "Verifier source contract missing $needle"}}
    $mutated=Get-Content $resultPath -Raw|ConvertFrom-Json;$mutated.patch_enabled=$true;$bad=Join-Path $fixtureRoot 'bad.json';[IO.File]::WriteAllText($bad,(ConvertTo-Json $mutated -Depth 10),$utf8);Expect-Failure {&$verifier -ResultPath $bad} 'noncanonical-result-path-and-patch'
    $original=[IO.File]::ReadAllText($resultPath)
    $mutated=$original|ConvertFrom-Json;$mutated.game_identity.after.tree_sha256='0'*64
    [IO.File]::WriteAllText($resultPath,(ConvertTo-Json $mutated -Depth 10),$utf8)
    Expect-Failure {&$verifier -ResultPath $resultPath -HistoricalEvidenceOnly} 'game-drift'
    [IO.File]::WriteAllText($resultPath,$original,$utf8)
    $extra=Join-Path $fixtureRoot 'runs/01/unexpected';[IO.Directory]::CreateDirectory($extra)|Out-Null
    Expect-Failure {&$verifier -ResultPath $resultPath -HistoricalEvidenceOnly} 'physical-extra-child'
    Remove-Item -LiteralPath $extra -Force
    foreach($needle in @('ReparsePoint','Get-ExactProcesses','no_surviving_processes')){if(-not$verifierText.Contains($needle)){throw "Integrity source contract missing $needle"}}
    [pscustomobject]@{Passed=$true;PhysicalPositives=4;FailClosedNegatives=8;SourceContractChecks=14;PlaybackProven=$false}
} finally {
    if(Test-Path $fixtureRoot){Remove-Item $fixtureRoot -Recurse -Force}
}
