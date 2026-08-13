[CmdletBinding(DefaultParameterSetName='Probe')]
param(
  [Parameter(Mandatory,ParameterSetName='Probe')][string]$RuntimeLogPath,
  [Parameter(Mandatory,ParameterSetName='Probe')][string]$BmpPath,
  [Parameter(Mandatory,ParameterSetName='Probe')][switch]$ProbeOnly,
  [Parameter(ParameterSetName='Probe')][switch]$AudioOnly,
  [Parameter(Mandatory,ParameterSetName='Result')][string]$ResultPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$titleVerifier=Join-Path $PSScriptRoot 'verify-render-path-smoke.ps1'
$utf8=[Text.UTF8Encoding]::new($false)

function Resolve-Safe([string]$Path,[string]$Description){
  $full=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($Path)){$Path}else{Join-Path $repo $Path}))
  $prefix=$repo.TrimEnd('\')+'\';if(-not$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description escapes the repository."}
  $current=$repo;foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object Length)){$current=Join-Path $current $part;if(-not(Test-Path $current)){throw "$Description is missing."};if((Get-Item $current -Force).Attributes-band[IO.FileAttributes]::ReparsePoint){throw "$Description traverses a reparse point."}}
  $full
}
function Get-LogSet([string]$Current){
  $current=Resolve-Safe $Current 'Runtime log';if((Split-Path $current -Leaf)-cne'mcla.log'){throw 'Current runtime log must be mcla.log.'}
  $files=@(Get-ChildItem (Split-Path $current) -File -Filter 'mcla*.log');$rot=@();$now=$null
  foreach($f in $files){if($f.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Runtime log is a reparse point.'};if($f.Name-ceq'mcla.log'){$now=$f;continue};$m=[regex]::Match($f.Name,'^mcla\.([1-9][0-9]*)\.log$');if(-not$m.Success){throw 'Malformed log rotation.'};$rot+=[pscustomobject]@{N=[int]$m.Groups[1].Value;F=$f}}
  if(-not$now-or$files.Count-lt1-or$files.Count-gt16){throw 'Runtime log topology is invalid.'};$ids=@($rot|ForEach-Object{$_.N}|Sort-Object);for($i=0;$i-lt$ids.Count;$i++){if($ids[$i]-ne$i+1){throw 'Runtime log rotations are not contiguous.'}}
  $ordered=@($rot|Sort-Object N -Descending|ForEach-Object F)+$now;$parts=@();$manifest=@();$bytes=0L
  foreach($f in $ordered){$bytes+=$f.Length;if($bytes-gt134217728){throw 'Runtime log set exceeds 128 MiB.'};$parts+=[IO.File]::ReadAllText($f.FullName);$manifest+=[ordered]@{name=$f.Name;bytes=$f.Length;sha256=(Get-FileHash $f.FullName -Algorithm SHA256).Hash}}
  $json=ConvertTo-Json -InputObject @($manifest) -Compress -Depth 3;$sha=[Security.Cryptography.SHA256]::Create();try{$hash=-join($sha.ComputeHash($utf8.GetBytes($json))|ForEach-Object{$_.ToString('X2')})}finally{$sha.Dispose()}
  [pscustomobject]@{Text=$parts-join"`n";Files=@($manifest);Count=$manifest.Count;Bytes=$bytes;Hash=$hash}
}
function One([regex]$Regex,[string]$Text,[string]$Name){$m=$Regex.Matches($Text);if($m.Count-ne1){throw "$Name must occur exactly once."};$m[0]}
function Num($Match,[string]$Name){[uint64]$Match.Groups[$Name].Value}
function Get-BmpIdentity([string]$Path){
  $path=Resolve-Safe $Path 'Audio title BMP';$bytes=[IO.File]::ReadAllBytes($path);if($bytes.Length-ne3686454-or$bytes[0]-ne0x42-or$bytes[1]-ne0x4D){throw 'Audio title BMP size/signature is invalid.'}
  $width=[BitConverter]::ToInt32($bytes,18);$height=[BitConverter]::ToInt32($bytes,22);$bpp=[BitConverter]::ToUInt16($bytes,28);if($width-ne1280-or$height-ne720-or$bpp-ne32){throw 'Audio title BMP geometry is invalid.'}
  [pscustomobject]@{Sha256=(Get-FileHash $path -Algorithm SHA256).Hash;Bytes=$bytes.Length;Width=$width;Height=$height}
}
function Get-AudioProbe([string]$LogPath,[string]$FramePath,[bool]$SkipTitle=$false){
  $log=Get-LogSet $LogPath;$text=$log.Text
  $config=One ([regex]'(?m)^.*SDL_AUDIO_AUDIT_CONFIG v=1 enabled=1 backend=sdl sample_rate=48000 source_channels=6\s*$') $text 'Audio config'
  $client=One ([regex]'(?m)^.*SDL_AUDIO_AUDIT_CLIENT v=1 event=register result=00000000 index_class=bounded\s*$') $text 'Audio client'
  $frames=[regex]::Matches($text,'(?m)^.*SDL_AUDIO_AUDIT_FRAME v=1 layer=(?<layer>submit|device|xma) class=(?<class>silence|nonzero) finite=(?<finite>[01]) channels=(?<channels>[0-9]+) peak_ppm=(?<peak>[0-9]+)\s*$')
  if($frames.Count-ne3){throw 'Audio audit must emit exactly three bounded layer records.'};$layers=@{};foreach($f in $frames){$layer=$f.Groups['layer'].Value;if($layers.ContainsKey($layer)){throw 'Duplicate audio layer record.'};$layers[$layer]=$f;if($f.Groups['finite'].Value-ne'1'){throw 'Audio layer record is non-finite.'}}
  foreach($layer in @('submit','device','xma')){if(-not$layers.ContainsKey($layer)){throw "Missing $layer audio record."}}
  if($layers.submit.Groups['channels'].Value-ne'6'-or$layers.device.Groups['channels'].Value-notin@('2','6')-or$layers.xma.Groups['channels'].Value-ne'0'){throw 'Audio layer channel classification is invalid.'}
  $start=One ([regex]'(?m)^.*MCLA audio: title soak started seconds 300\s*$') $text 'Audio soak start'
  $done=One ([regex]'(?m)^.*MCLA audio: title soak completed seconds 300\s*$') $text 'Audio soak completion'
  $summary=One ([regex]'(?m)^.*SDL_AUDIO_AUDIT_SUMMARY v=1 phase=title status=PASS client_calls=(?<cc>[0-9]+) client_success=(?<cs>[0-9]+) submit_frames=(?<sf>[0-9]+) submit_nonzero=(?<sn>[0-9]+) submit_invalid=(?<si>[0-9]+) submit_peak_ppm=(?<sp>[0-9]+) device_frames=(?<df>[0-9]+) device_nonzero=(?<dn>[0-9]+) device_invalid=(?<di>[0-9]+) device_submit_fail=(?<fail>[0-9]+) device_peak_ppm=(?<dp>[0-9]+) xma_frames=(?<xf>[0-9]+) xma_nonzero=(?<xn>[0-9]+) xma_invalid=(?<xi>[0-9]+) xma_peak_ppm=(?<xp>[0-9]+) max_queue_depth=(?<qd>[0-9]+) starvation_fills=(?<st>[0-9]+) max_consecutive_starvation_fills=(?<mc>[0-9]+) dropped_records=(?<drop>[0-9]+)\s*$') $text 'Audio summary'
  $capture=[regex]::Match($text,'MCLA graphics: nontrivial guest frame captured ');if(-not$capture.Success){throw 'Title capture marker is missing.'}
  if(-not($config.Index-lt$client.Index-and$client.Index-lt$capture.Index-and$capture.Index-lt$start.Index-and$start.Index-lt$summary.Index-and$summary.Index-lt$done.Index)){throw 'Audio/title/soak chronology is invalid.'}
  $cc=Num $summary cc;$cs=Num $summary cs;$sf=Num $summary sf;$sn=Num $summary sn;$df=Num $summary df;$dn=Num $summary dn;$xf=Num $summary xf;$xn=Num $summary xn;$qd=Num $summary qd;$mc=Num $summary mc
  if($cc-lt1-or$cs-lt1-or$cs-gt$cc-or$sf-lt1-or$sn-lt1-or$sn-gt$sf-or$df-lt1-or$dn-lt1-or$dn-gt$df-or$xf-lt1-or$xn-lt1-or$xn-gt$xf){throw 'Audio summary lacks sustained nonzero route coverage.'}
  foreach($n in @('si','di','fail','xi','drop')){if((Num $summary $n)-ne0){throw "Audio summary failure counter '$n' is nonzero."}}
  foreach($n in @('sp','dp','xp')){$v=Num $summary $n;if($v-lt1-or$v-gt1000000){throw "Audio peak '$n' is invalid."}}
  if($qd-lt1-or$qd-gt64-or$mc-gt2){throw 'Audio queue growth or consecutive starvation is out of bounds.'}
  if($text-match'(?i)(SDL_(?:OpenAudioDeviceStream|ResumeAudioDevice|PutAudioStreamData).*failed|audio.*(?:fatal|device lost)|PPC_UNIMPLEMENTED|guest crash)'){throw 'Runtime log contains a banned audio/fatal marker.'}
  $bmp=Get-BmpIdentity $FramePath
  [pscustomobject]@{Passed=$true;LogSet=$log;Bmp=$bmp;ClientCalls=$cc;SubmitFrames=$sf;SubmitNonzero=$sn;DeviceFrames=$df;DeviceNonzero=$dn;XmaFrames=$xf;XmaNonzero=$xn;MaxQueueDepth=$qd;StarvationFills=(Num $summary st);MaxConsecutiveStarvation=$mc;SubmitPeakPpm=(Num $summary sp);DevicePeakPpm=(Num $summary dp);XmaPeakPpm=(Num $summary xp)}
}

if($PSCmdlet.ParameterSetName-eq'Probe'){
  if(-not$ProbeOnly){throw 'Probe inputs require -ProbeOnly.'};Get-AudioProbe $RuntimeLogPath $BmpPath $AudioOnly.IsPresent;return
}
$result=Resolve-Safe $ResultPath 'Result';$json=[IO.File]::ReadAllText($result);if($json-match'(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])'){throw 'Result contains a private or absolute path.'};$r=$json|ConvertFrom-Json
if($r.schema-ne1-or$r.task-cne'M4-007'-or$r.soak_seconds-ne300){throw 'Audio aggregate identity is invalid.'}
if($r.decision-ceq'frontend-audio-route-pass'){
  if($r.cycle_count-ne1-or$r.clean_build_performed-ne$true){throw 'Canonical audio aggregate must bind one clean-build cycle.'}
  $root=Split-Path $result;$cycle=$r.cycle;$log=Join-Path $root 'runs\01\mcla.log';$bmp=Join-Path $root 'runs\01\user\mcla-first-frame.bmp';$p=Get-AudioProbe $log $bmp;$bmpIdentity=Get-BmpIdentity $bmp
  if($cycle.runtime_log_set_sha256-cne$p.LogSet.Hash-or$cycle.capture_sha256-cne$bmpIdentity.Sha256-or$cycle.submit_frames-ne$p.SubmitFrames-or$cycle.device_frames-ne$p.DeviceFrames-or$cycle.xma_frames-ne$p.XmaFrames-or$cycle.max_queue_depth-ne$p.MaxQueueDepth-or$cycle.max_consecutive_starvation_fills-ne$p.MaxConsecutiveStarvation-or$cycle.exit_code-ne0-or$cycle.harness_force_cleanup-ne$false-or$r.no_surviving_processes-ne$true-or$r.data_integrity_preserved-ne$true){throw 'Canonical audio aggregate is not bound to clean physical evidence.'}
  if($r.build.focused_test_cases-ne5-or$r.build.focused_test_assertions-ne16){throw 'Canonical focused audio test totals changed.'};foreach($pair in @(@('sdk-install.log','sdk_install_log_sha256'),@('sdk-audio-test.log','focused_test_log_sha256'),@('relwithdebinfo-clean-build.log','app_build_log_sha256'))){if((Get-FileHash (Resolve-Safe (Join-Path $root $pair[0]) 'Canonical build log') -Algorithm SHA256).Hash-cne$r.build.($pair[1])){throw 'Canonical clean-build log hash mismatch.'}}
  if(($r.game_identity.before|ConvertTo-Json -Compress)-cne($r.game_identity.after|ConvertTo-Json -Compress)){throw 'Canonical source-game identity changed.'};$canonicalBuild=Resolve-Safe (Join-Path $repo 'out/build/win-amd64-relwithdebinfo') 'Canonical build';foreach($a in $r.artifacts.after){if((Get-FileHash (Resolve-Safe (Join-Path $canonicalBuild $a.name) 'Canonical runtime artifact') -Algorithm SHA256).Hash-cne$a.sha256){throw 'Canonical runtime artifact hash mismatch.'}}
  [pscustomobject]@{Passed=$true;Decision=$r.decision;SoakSeconds=300;SubmitNonzero=$p.SubmitNonzero;DeviceNonzero=$p.DeviceNonzero;XmaNonzero=$p.XmaNonzero;MaxQueueDepth=$p.MaxQueueDepth;MaxConsecutiveStarvation=$p.MaxConsecutiveStarvation;ControlledLifecycleVerified=$true;MonolithicRunClaimed=$true};return
}
if($r.decision-cne'split-build-soak-lifecycle_audio-route-pass'-or$r.monolithic_run_claimed-ne$false){throw 'Recovered audio aggregate identity is invalid.'}
$root=Split-Path $result;$soak=$r.soak;$log=Join-Path $root 'runs\01\mcla.log';$bmp=Join-Path $root 'runs\01\user\mcla-first-frame.bmp';$p=Get-AudioProbe $log $bmp $true;$bmpIdentity=Get-BmpIdentity $bmp
if($soak.runtime_log_set_sha256-cne$p.LogSet.Hash-or$soak.capture_sha256-cne$bmpIdentity.Sha256-or$soak.submit_frames-ne$p.SubmitFrames-or$soak.device_frames-ne$p.DeviceFrames-or$soak.xma_frames-ne$p.XmaFrames-or$soak.max_queue_depth-ne$p.MaxQueueDepth-or$soak.max_consecutive_starvation_fills-ne$p.MaxConsecutiveStarvation-or$soak.harness_force_cleanup-ne$true-or$soak.controlled_exit_claimed-ne$false){throw 'Audio aggregate is not bound truthfully to physical soak evidence.'}
$lifeLog=Resolve-Safe (Join-Path $root 'lifecycle\mcla.log') 'Lifecycle log';$life=[IO.File]::ReadAllText($lifeLog);$close=$life.IndexOf('Window closing, shutting down...');$complete=$life.IndexOf('Execution complete');$hard=$life.IndexOf('Title terminated; hard-exiting process.');if($close-lt0-or$complete-le$close-or$hard-le$complete-or$life-match'FATAL|PPC_UNIMPLEMENTED|Guest crash'){throw 'Paired lifecycle evidence is invalid.'}
if($r.lifecycle.exit_code-ne0-or$r.lifecycle.harness_force_cleanup-ne$false-or$r.lifecycle.log_sha256-cne(Get-FileHash $lifeLog -Algorithm SHA256).Hash-or$r.build.focused_test_cases-ne5-or$r.build.focused_test_assertions-ne16-or$r.no_surviving_processes-ne$true-or$r.data_integrity_preserved-ne$true){throw 'Audio aggregate build/lifecycle/integrity contract failed.'}
$buildRoot=Resolve-Safe (Join-Path $repo ("private/evidence/M4-007/"+$r.build.run_id)) 'Audio build evidence';foreach($pair in @(@('sdk-install.log','sdk_install_log_sha256'),@('sdk-audio-test.log','focused_test_log_sha256'),@('relwithdebinfo-clean-build.log','app_build_log_sha256'))){$path=Resolve-Safe (Join-Path $buildRoot $pair[0]) 'Audio build log';if((Get-FileHash $path -Algorithm SHA256).Hash-cne$r.build.($pair[1])){throw 'Audio clean-build evidence hash mismatch.'}}
$canonicalBuild=Resolve-Safe (Join-Path $repo 'out/build/win-amd64-relwithdebinfo') 'Canonical build';if(@($r.runtime_artifacts).Count-ne4){throw 'Audio aggregate runtime artifact count is invalid.'};foreach($a in $r.runtime_artifacts){if($a.name-notin@('mcla.exe','rexruntimerd.dll','TracyClientrd.dll','rexgpu-xenosrd.dll')-or(Get-FileHash (Resolve-Safe (Join-Path $canonicalBuild $a.name) 'Runtime artifact') -Algorithm SHA256).Hash-cne$a.sha256){throw 'Audio runtime artifact binding failed.'}}
if(@((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{[string]::Equals($_.Path,(Join-Path $canonicalBuild 'mcla.exe'),[StringComparison]::OrdinalIgnoreCase)}catch{$false}}).Count){throw 'Canonical MCLA process still survives.'}
[pscustomobject]@{Passed=$true;Decision=$r.decision;SoakSeconds=300;SubmitNonzero=$p.SubmitNonzero;DeviceNonzero=$p.DeviceNonzero;XmaNonzero=$p.XmaNonzero;MaxQueueDepth=$p.MaxQueueDepth;MaxConsecutiveStarvation=$p.MaxConsecutiveStarvation;ControlledLifecycleVerified=$true;MonolithicRunClaimed=$false}
