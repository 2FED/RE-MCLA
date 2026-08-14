[CmdletBinding(DefaultParameterSetName='Run')]
param(
  [Parameter(Mandatory,ParameterSetName='Run')][string]$RunPath,
  [Parameter(Mandatory,ParameterSetName='Result')][string]$ResultPath,
  [switch]$Fixture
)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$utf8=[Text.UTF8Encoding]::new($false)

function Resolve-SafePath([string]$Path,[string]$Description,[switch]$Exists){
  $full=if([IO.Path]::IsPathRooted($Path)){[IO.Path]::GetFullPath($Path)}else{[IO.Path]::GetFullPath((Join-Path $repo $Path))}
  $prefix=$repo.TrimEnd('\')+'\'
  if(-not $full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description escapes repository."}
  $current=$repo
  foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object{$_.Length})){
    $current=Join-Path $current $part
    if((Test-Path -LiteralPath $current)-and((Get-Item -LiteralPath $current -Force).Attributes-band[IO.FileAttributes]::ReparsePoint)){throw "$Description traverses a reparse point."}
  }
  if($Exists-and-not(Test-Path -LiteralPath $full)){throw "$Description is missing."}
  $full
}

function Assert-NoReparseTree([string]$Root){
  $pending=[Collections.Generic.Stack[string]]::new();$pending.Push($Root)
  while($pending.Count){foreach($item in @(Get-ChildItem -LiteralPath $pending.Pop() -Force)){if($item.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Tree contains a reparse point.'};if($item.PSIsContainer){$pending.Push($item.FullName)}}}
}

function Get-TreeSnapshot([string]$Root){
  $rootPath=Resolve-SafePath $Root 'Tree' -Exists;Assert-NoReparseTree $rootPath
  $items=@(Get-ChildItem -LiteralPath $rootPath -Recurse -Force);$files=@($items|Where-Object{-not$_.PSIsContainer-and-not($_.FullName-ceq(Join-Path $rootPath 'result.json'))}|Sort-Object FullName);$entries=@();$bytes=0L
  foreach($directory in @($items|Where-Object{$_.PSIsContainer}|Sort-Object FullName)){$entries+=[ordered]@{kind='directory';path=$directory.FullName.Substring($rootPath.Length).TrimStart('\').Replace('\','/')}}
  foreach($file in $files){$bytes+=$file.Length;$entries+=[ordered]@{kind='file';path=$file.FullName.Substring($rootPath.Length).TrimStart('\').Replace('\','/');bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}}
  $json=ConvertTo-Json -InputObject @($entries) -Compress -Depth 4;$sha=[Security.Cryptography.SHA256]::Create()
  try{$hash=-join($sha.ComputeHash($utf8.GetBytes($json))|ForEach-Object{$_.ToString('X2')})}finally{$sha.Dispose()}
  [pscustomobject]@{Hash=$hash;FileCount=$files.Count;DirectoryCount=@($items|Where-Object{$_.PSIsContainer}).Count;Bytes=$bytes}
}

function Get-GameIdentity([string]$Root){
  $tree=Get-TreeSnapshot $Root;$verified=& (Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath $Root -VerifyHashes
  [pscustomobject][ordered]@{file_count=[int]$verified.FileCount;payload_bytes=[int64]$verified.PayloadBytes;manifest_sha256=(Get-FileHash -LiteralPath $verified.ManifestPath -Algorithm SHA256).Hash;tree_sha256=$tree.Hash;tree_file_count=$tree.FileCount;tree_directory_count=$tree.DirectoryCount;tree_bytes=$tree.Bytes}
}

function Get-ArtifactIdentity([string]$BuildRoot){
  @('mcla.exe','rexruntime.dll','TracyClient.dll','rexgpu-xenos.dll')|ForEach-Object{$path=Resolve-SafePath (Join-Path $BuildRoot $_) "Artifact $_" -Exists;[pscustomobject][ordered]@{name=$_;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}}
}

function Get-LogSet([string]$CycleRoot){
  $files=@(Get-ChildItem -LiteralPath $CycleRoot -File -Filter 'mcla*.log')
  if($files.Count-lt1-or$files.Count-gt16){throw 'Runtime-log topology is invalid.'}
  $current=@($files|Where-Object{$_.Name-ceq'mcla.log'});if($current.Count-ne1){throw 'Current runtime log count is invalid.'}
  $rotated=@();foreach($file in @($files|Where-Object{$_.Name-cne'mcla.log'})){$m=[regex]::Match($file.Name,'^mcla\.([1-9][0-9]*)\.log$');if(-not$m.Success){throw 'Malformed runtime-log rotation.'};$rotated+=[pscustomobject]@{Index=[int]$m.Groups[1].Value;File=$file}}
  $indices=@($rotated|ForEach-Object{$_.Index}|Sort-Object);for($i=0;$i-lt$indices.Count;$i++){if($indices[$i]-ne$i+1){throw 'Runtime-log rotations are not contiguous.'}}
  $ordered=@($rotated|Sort-Object Index -Descending|ForEach-Object{$_.File})+$current[0];$text=@();$manifest=@();$bytes=0L
  foreach($file in $ordered){$bytes+=$file.Length;if($bytes-gt134217728){throw 'Runtime logs exceed 128 MiB.'};$text+=[IO.File]::ReadAllText($file.FullName);$manifest+=[ordered]@{name=$file.Name;bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}}
  $json=ConvertTo-Json -InputObject @($manifest) -Compress -Depth 3;$sha=[Security.Cryptography.SHA256]::Create();try{$hash=-join($sha.ComputeHash($utf8.GetBytes($json))|ForEach-Object{$_.ToString('X2')})}finally{$sha.Dispose()}
  [pscustomobject]@{Text=$text-join"`n";Files=@($manifest);Count=$manifest.Count;Bytes=$bytes;Hash=$hash}
}

function Get-OnlyMatch([string]$Text,[string]$Pattern,[string]$Description){
  $matches=[regex]::Matches($Text,$Pattern);if($matches.Count-ne1){throw "$Description count is $($matches.Count), expected 1."};$matches[0]
}

function Get-Bmp([string]$Path){
  $resolved=Resolve-SafePath $Path 'Physics BMP' -Exists;$bytes=[IO.File]::ReadAllBytes($resolved)
  if($bytes.Length-ne3686454-or$bytes[0]-ne0x42-or$bytes[1]-ne0x4D-or[BitConverter]::ToInt32($bytes,18)-ne1280-or[Math]::Abs([BitConverter]::ToInt32($bytes,22))-ne720-or[BitConverter]::ToUInt16($bytes,28)-ne32){throw 'Physics capture is not canonical 1280x720 BMP.'}
  [pscustomobject]@{Path=$resolved;Bytes=$bytes;Length=$bytes.Length;Sha256=(Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash}
}

function Get-SampledDifference($Left,$Right){$count=0;for($offset=54;$offset-lt$Left.Bytes.Length;$offset+=16){$delta=[Math]::Abs([int]$Left.Bytes[$offset]-[int]$Right.Bytes[$offset])+[Math]::Abs([int]$Left.Bytes[$offset+1]-[int]$Right.Bytes[$offset+1])+[Math]::Abs([int]$Left.Bytes[$offset+2]-[int]$Right.Bytes[$offset+2]);if($delta-gt36){$count++}};$count}

function Assert-RunTopology([string]$Root){
  $children=@(Get-ChildItem -LiteralPath $Root -Force|ForEach-Object{$_.Name}|Sort-Object)
  if(($children-join',')-cne'release-clean-build.log,result.json,runs'){throw 'Result-root topology is invalid.'}
  $cycleNames=@(Get-ChildItem -LiteralPath (Join-Path $Root 'runs') -Directory -Force|ForEach-Object{$_.Name}|Sort-Object)
  if(($cycleNames-join',')-cne'01,02,03'){throw 'Cycle topology is invalid.'}
}

function Get-CycleProbe([string]$CycleRoot,[int]$Cycle){
  $root=Resolve-SafePath $CycleRoot "Cycle $Cycle" -Exists;Assert-NoReparseTree $root;$logs=Get-LogSet $root;$text=$logs.Text
  $cycleSave=Resolve-SafePath (Join-Path $root 'user/B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav') "Cycle $Cycle save" -Exists
  $cycleHeader=Resolve-SafePath (Join-Path $root 'user/B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header') "Cycle $Cycle save header" -Exists
  if((Get-FileHash -LiteralPath $cycleSave -Algorithm SHA256).Hash-cne'E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68'-or(Get-FileHash -LiteralPath $cycleHeader -Algorithm SHA256).Hash-cne'1DAEF55FA78BDD3F1AA75A36772B801C6C714B6D1A115A97904F3D1C957BC4C9'){throw "Cycle $Cycle save identity mismatch."}
  foreach($bad in @('Assertion failed','PPC_UNIMPLEMENTED','Guest crash','device removed','DXGI_ERROR_DEVICE_REMOVED','MCLA physics timing: final sample failed')){if($text.Contains($bad)){throw "Cycle $Cycle contains banned marker '$bad'."}}
  $hook=Get-OnlyMatch $text 'MCLA_PHYSICS_TIMING_HOOK v=1 address=821BDA90 status=READY' 'Timer hook'
  $config=Get-OnlyMatch $text 'MCLA_PHYSICS_TIMING_CONFIG v=1 slot=0 gameplay_wait_seconds=45 dismiss_pulses=6 dismiss_interval_ms=5000 sample_seconds=10 guest_tick_frequency=50000000 expected_vblank_millihz=60000 expected_present_millihz=30000' 'Timing config'
  $start=Get-OnlyMatch $text 'MCLA_PHYSICS_TIMING_FRAME v=1 phase=start width=1280 height=720 status=PASS' 'Start frame'
  $end=Get-OnlyMatch $text 'MCLA_PHYSICS_TIMING_FRAME v=1 phase=end width=1280 height=720 status=PASS' 'End frame'
  $timer=Get-OnlyMatch $text 'MCLA_PHYSICS_TIMER_SUMMARY v=1 calls=(\d+) records=(\d+) invalid_values=(\d+) effective_us_min=(\d+) effective_us_max=(\d+) clamped_us_min=(\d+) clamped_us_max=(\d+) raw_us_min=(\d+) raw_us_max=(\d+)' 'Timer summary'
  $sample=Get-OnlyMatch $text 'MCLA_PHYSICS_TIMING_SAMPLE v=1 host_us=(\d+) guest_ticks=(\d+) guest_host_ratio_ppm=(\d+) vblank_delta=(\d+) vblank_millihz=(\d+) present_delta=(\d+) present_millihz=(\d+) present_to_vblank_ppm=(\d+) simulated_time_to_wall_ppm=(\d+)' 'Timing sample'
  $summary=Get-OnlyMatch $text 'MCLA_PHYSICS_TIMING_SUMMARY v=1 status=COMPLETE samples=1 frames=2 gameplay_input_records=8 external_close_required=1' 'Timing summary'
  $closing=Get-OnlyMatch $text 'Window closing, shutting down\.\.\.' 'Window close';$complete=Get-OnlyMatch $text 'Execution complete' 'Execution complete';$hard=Get-OnlyMatch $text 'Title terminated; hard-exiting process\.' 'Hard exit'
  if(-not($hook.Index-lt$config.Index-and$config.Index-lt$start.Index-and$start.Index-lt$end.Index-and$end.Index-lt$timer.Index-and$timer.Index-lt$sample.Index-and$sample.Index-lt$summary.Index-and$summary.Index-lt$closing.Index-and$closing.Index-lt$complete.Index-and$complete.Index-lt$hard.Index)){throw 'Physics chronology is invalid.'}
  $records=[regex]::Matches($text,'MCLA_PHYSICS_TIMER_RECORD v=1 id=(\d+) effective_bits=([0-9A-F]{8}) clamped_bits=([0-9A-F]{8}) raw_bits=([0-9A-F]{8})')
  if($records.Count-ne16){throw 'Timer record count is invalid.'};for($i=0;$i-lt16;$i++){if([int]$records[$i].Groups[1].Value-ne$i-or$records[$i].Groups[2].Value-cne'3D088889'-or$records[$i].Groups[3].Value-cne'3D088889'){throw 'Timer fixed-step record is invalid.'}}
  if([regex]::Matches($text,'MCLA_GAMEPLAY_INPUT v=1').Count-ne8){throw 'Gameplay input record count is invalid.'}
  $calls=[int64]$timer.Groups[1].Value;$recordCount=[int]$timer.Groups[2].Value;$invalid=[int64]$timer.Groups[3].Value;$effectiveMin=[int]$timer.Groups[4].Value;$effectiveMax=[int]$timer.Groups[5].Value;$clampedMin=[int]$timer.Groups[6].Value;$clampedMax=[int]$timer.Groups[7].Value;$rawMin=[int]$timer.Groups[8].Value;$rawMax=[int]$timer.Groups[9].Value
  $hostUs=[int64]$sample.Groups[1].Value;$guestTicks=[int64]$sample.Groups[2].Value;$ratio=[int]$sample.Groups[3].Value;$vblankDelta=[int]$sample.Groups[4].Value;$vblankRate=[int]$sample.Groups[5].Value;$presentDelta=[int]$sample.Groups[6].Value;$presentRate=[int]$sample.Groups[7].Value;$presentRatio=[int]$sample.Groups[8].Value;$simulatedRatio=[int]$sample.Groups[9].Value
  if($calls-lt294-or$calls-gt306-or$recordCount-ne16-or$invalid-ne0-or$effectiveMin-ne33333-or$effectiveMax-ne33333-or$clampedMin-ne33333-or$clampedMax-ne33333-or$rawMin-lt25000-or$rawMax-gt75000-or$rawMax-lt$rawMin){throw 'Timer measurements violate the stock fixed-step contract.'}
  if($hostUs-lt9900000-or$hostUs-gt10100000-or$ratio-lt999000-or$ratio-gt1001000-or$vblankDelta-lt594-or$vblankDelta-gt606-or$vblankRate-lt59400-or$vblankRate-gt60600-or$presentDelta-lt294-or$presentDelta-gt306-or$presentRate-lt29400-or$presentRate-gt30600-or$presentRatio-lt490000-or$presentRatio-gt510000-or$simulatedRatio-lt980000-or$simulatedRatio-gt1020000){throw 'Clock/vblank/output/game-speed measurements are outside calibrated bounds.'}
  $startBmp=Get-Bmp (Join-Path $root 'user/mcla-physics-start.bmp');$endBmp=Get-Bmp (Join-Path $root 'user/mcla-physics-end.bmp');$difference=Get-SampledDifference $startBmp $endBmp;if($difference-lt20000){throw 'Vehicle response frames are insufficiently distinct.'}
  [pscustomobject][ordered]@{cycle=$Cycle;host_us=$hostUs;guest_ticks=$guestTicks;guest_host_ratio_ppm=$ratio;vblank_delta=$vblankDelta;vblank_millihz=$vblankRate;present_delta=$presentDelta;present_millihz=$presentRate;present_to_vblank_ppm=$presentRatio;simulated_time_to_wall_ppm=$simulatedRatio;timer_calls=$calls;effective_us=33333;raw_us_min=$rawMin;raw_us_max=$rawMax;sampled_frame_difference=$difference;start_sha256=$startBmp.Sha256;end_sha256=$endBmp.Sha256;log_set_sha256=$logs.Hash;log_file_count=$logs.Count;log_bytes=$logs.Bytes;log_manifest=@($logs.Files);controlled_exit=$true}
}

function Get-RunProbe([string]$Path){
  $root=Resolve-SafePath $Path 'M5-008 run' -Exists;Assert-NoReparseTree $root;$cycles=@();for($i=1;$i-le3;$i++){$cycles+=Get-CycleProbe (Join-Path $root ('runs/{0:D2}'-f$i)) $i}
  [pscustomobject][ordered]@{cycles=@($cycles);cycle_count=3;decision='stock-30-fixed-step-and-real-time-throughput-pass'}
}

if($PSCmdlet.ParameterSetName-eq'Run'){
  $probe=Get-RunProbe $RunPath
  if(-not$Fixture){$root=Resolve-SafePath $RunPath 'M5-008 run' -Exists;$children=@(Get-ChildItem -LiteralPath $root -Force|ForEach-Object{$_.Name}|Sort-Object);if(($children-join',')-cne'release-clean-build.log,runs'){throw 'Run-root topology is invalid.'};$cycleNames=@(Get-ChildItem -LiteralPath (Join-Path $root 'runs') -Directory -Force|ForEach-Object{$_.Name}|Sort-Object);if(($cycleNames-join',')-cne'01,02,03'){throw 'Cycle topology is invalid.'}}
  $probe
  return
}

$result=Resolve-SafePath $ResultPath 'M5-008 result' -Exists
if((Get-Item -LiteralPath $result -Force).Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Result is a reparse point.'}
$record=Get-Content -LiteralPath $result -Raw|ConvertFrom-Json
$properties=@('schema','task','decision','sdk_version','sdk_commit','build_configuration','stock_30_target','stock_speed_sustained','actual_output_30fps_sustained','fixed_step_microseconds','cycle_count','seed_save_sha256','seed_header_sha256','game_before','game_after','artifacts_before','artifacts_after','build_log_sha256','cycles','evidence_tree_sha256','evidence_tree_file_count','evidence_tree_directory_count','evidence_tree_bytes','scope')
if(($record.PSObject.Properties.Name-join',')-cne($properties-join',')-or$record.schema-cne'mcla-physics-timing-v1'-or$record.task-cne'M5-008'-or$record.decision-cne'stock-30-fixed-step-and-real-time-throughput-pass'-or$record.cycle_count-ne3-or-not($record.cycle_count-is[int]-or$record.cycle_count-is[long])-or$record.fixed_step_microseconds-ne33333-or-not($record.fixed_step_microseconds-is[int]-or$record.fixed_step_microseconds-is[long])-or$record.sdk_version-cne'0.9.0.19'-or$record.sdk_commit-cnotmatch'^[0-9a-f]{40}$'-or$record.build_configuration-cne'Release'-or-not($record.stock_30_target-is[bool])-or$record.stock_30_target-ne$true-or-not($record.stock_speed_sustained-is[bool])-or$record.stock_speed_sustained-ne$true-or-not($record.actual_output_30fps_sustained-is[bool])-or$record.actual_output_30fps_sustained-ne$true){throw 'Result schema, types, or decision are invalid.'}
$runRoot=Split-Path $result -Parent;Assert-RunTopology $runRoot;$probe=Get-RunProbe $runRoot
if((ConvertTo-Json $record.cycles -Compress -Depth 8)-cne(ConvertTo-Json $probe.cycles -Compress -Depth 8)){throw 'Result cycles do not match physical evidence.'}
$game=Get-GameIdentity (Join-Path $repo 'private/game');if((ConvertTo-Json $record.game_after -Compress -Depth 5)-cne(ConvertTo-Json $game -Compress -Depth 5)-or(ConvertTo-Json $record.game_before -Compress -Depth 5)-cne(ConvertTo-Json $game -Compress -Depth 5)){throw 'Canonical game identity mismatch.'}
$artifacts=@(Get-ArtifactIdentity (Join-Path $repo 'out/build/win-amd64-release'));if((ConvertTo-Json $record.artifacts_after -Compress -Depth 4)-cne(ConvertTo-Json $artifacts -Compress -Depth 4)-or(ConvertTo-Json $record.artifacts_before -Compress -Depth 4)-cne(ConvertTo-Json $artifacts -Compress -Depth 4)){throw 'Runtime artifact identity mismatch.'}
$seed=Join-Path $repo 'private/baseline/M4-011/post-oobe-profile';if((Get-FileHash -LiteralPath (Join-Path $seed 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav') -Algorithm SHA256).Hash-cne$record.seed_save_sha256-or(Get-FileHash -LiteralPath (Join-Path $seed 'B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header') -Algorithm SHA256).Hash-cne$record.seed_header_sha256){throw 'Pinned save identity mismatch.'}
$buildLog=Join-Path $runRoot 'release-clean-build.log';if((Get-FileHash -LiteralPath $buildLog -Algorithm SHA256).Hash-cne$record.build_log_sha256){throw 'Build-log identity mismatch.'}
$sdkHead=(& git -C (Join-Path $repo 'third_party/rexglue-sdk') rev-parse HEAD).Trim();if($LASTEXITCODE-ne0-or$sdkHead-cne$record.sdk_commit){throw 'SDK commit identity mismatch.'}
$tree=Get-TreeSnapshot $runRoot;if($record.evidence_tree_sha256-cne$tree.Hash-or$record.evidence_tree_file_count-ne$tree.FileCount-or$record.evidence_tree_directory_count-ne$tree.DirectoryCount-or$record.evidence_tree_bytes-ne$tree.Bytes){throw 'Result evidence-tree identity mismatch.'}
[pscustomobject][ordered]@{Decision=$record.decision;CycleCount=3;FixedStepMicroseconds=33333;GuestClockSynchronized=$true;Vblank60HzVerified=$true;StockSpeedSustained=$true;ActualOutput30FpsSustained=$true;ControlledExitVerified=$true;DataIntegrityVerified=$true}
