[CmdletBinding(DefaultParameterSetName='Result')]
param(
  [Parameter(Mandatory,ParameterSetName='Probe')][switch]$ProbeOnly,
  [Parameter(Mandatory,ParameterSetName='Probe')][string]$RuntimeLogPath,
  [Parameter(Mandatory,ParameterSetName='Probe')][string]$UserRoot,
  [Parameter(Mandatory,ParameterSetName='Result')][string]$ResultPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$utf8=[Text.UTF8Encoding]::new($false)
$phaseFiles=[ordered]@{
  'dry-night-baseline'='mcla-environment-dry-night.bmp'
  'rain-dawn-options'='mcla-environment-options.bmp'
  'rain-dawn-stationary'='mcla-environment-stationary.bmp'
  'rain-dawn-moving'='mcla-environment-moving.bmp'
  'rain-dawn-stopped'='mcla-environment-stopped.bmp'
  'rain-dawn-particle'='mcla-environment-particle.bmp'
}
$prerequisiteHashes=[ordered]@{
  m5_003='299392E59CB38AFB44256E773884856FF0C869A96D2045086A65396C1ED4EFCA'
  m6_001='519B84FF456BDD3220BFC8BE3DD230CCB209A56CF2B203D51CFF5454729E178F'
  m5_012='D993E2612D1AC769D88264C83FD9C9186BC761E2067317F5A7EB66038C250E58'
  m5_013='D890A903775A3B53262B9957E7B7DC1D4B76B49B8735360BECD8233842446298'
  traffic_red='0ECD702B94586BDE2E9C1566B8C19BE45F4796CBD21C3C736FECD1FED0DBE484'
  traffic_green='72EA8F650313735F72C4C1E1F0B1EFA325F2402A682FDFDF4247E7197D2D6E9C'
  traffic_off='D7391B5BA9F963721F178418395E0639FBF91A12DED2DEEDCB9189ECF48A36E7'
}
$prerequisitePaths=[ordered]@{
  m5_003='private/evidence/M5-003/20260814-104624-fde51a30/result.json'
  m6_001='private/evidence/M6-001/20260817-115619-d269e2a9/result.json'
  m5_012='private/evidence/M5-012/20260817-001225-ade395f8/result.json'
  m5_013='private/evidence/M5-013/20260817-015958-36eec226/result.json'
  traffic_red='private/evidence/M6-004/diagnostic-colored-reflections-20260817-000206/user-captures/traffic-light-000523.png'
  traffic_green='private/evidence/M6-004/diagnostic-colored-reflections-20260817-000206/user-captures/traffic-light-000529.png'
  traffic_off='private/evidence/M6-004/diagnostic-colored-reflections-20260817-000206/user-captures/traffic-light-000538.png'
}

function Safe([string]$Path,[string]$Description,[switch]$Exists){
  $full=if([IO.Path]::IsPathRooted($Path)){[IO.Path]::GetFullPath($Path)}else{[IO.Path]::GetFullPath((Join-Path $repo $Path))}
  $prefix=$repo.TrimEnd('\')+'\';if(-not $full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description escapes repository."}
  $current=$repo;foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object{$_.Length})){$current=Join-Path $current $part;if((Test-Path -LiteralPath $current)-and((Get-Item -LiteralPath $current -Force).Attributes-band[IO.FileAttributes]::ReparsePoint)){throw "$Description traverses a reparse point."}}
  if($Exists-and-not(Test-Path -LiteralPath $full)){throw "$Description is missing."};$full
}
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
function Tree([string]$Root){$root=Safe $Root 'Tree' -Exists;$items=@(Get-ChildItem -LiteralPath $root -Recurse -Force);foreach($item in $items){if($item.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Tree contains a reparse point.'}};$files=@($items|Where-Object{-not$_.PSIsContainer}|Sort-Object FullName);$entries=@();foreach($directory in @($items|Where-Object PSIsContainer|Sort-Object FullName)){$entries+=[ordered]@{kind='directory';path=$directory.FullName.Substring($root.Length).TrimStart('\').Replace('\','/')}};foreach($file in $files){$entries+=[ordered]@{kind='file';path=$file.FullName.Substring($root.Length).TrimStart('\').Replace('\','/');length=[long]$file.Length;sha256=Hash $file.FullName}};$json=ConvertTo-Json -InputObject @($entries) -Compress -Depth 4;$sha=[Security.Cryptography.SHA256]::Create();try{$value=-join($sha.ComputeHash($utf8.GetBytes($json))|ForEach-Object{$_.ToString('X2')})}finally{$sha.Dispose()};$bytes=0L;foreach($file in $files){$bytes+=$file.Length};[pscustomobject]@{Hash=$value;FileCount=$files.Count;DirectoryCount=@($items|Where-Object PSIsContainer).Count;Bytes=$bytes}}
function Game([string]$Root){$tree=Tree $Root;$verified=& (Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath $Root -VerifyHashes;[ordered]@{file_count=$verified.FileCount;payload_bytes=[long]$verified.PayloadBytes;manifest_sha256=Hash $verified.ManifestPath;tree_sha256=$tree.Hash;tree_file_count=$tree.FileCount;tree_directory_count=$tree.DirectoryCount;tree_bytes=$tree.Bytes}}
function Artifacts([string]$Build){@('mcla.exe','rexruntime.dll','TracyClient.dll','rexgpu-xenos.dll')|ForEach-Object{$path=Safe (Join-Path $Build $_) "Artifact $_" -Exists;[ordered]@{name=$_;sha256=Hash $path}}}
function LogSet([string]$Path){
  $path=Safe $Path 'Runtime log' -Exists;$directory=Split-Path $path;$current=Split-Path $path -Leaf;$stem=[IO.Path]::GetFileNameWithoutExtension($current);$ext=[IO.Path]::GetExtension($current)
  $rotated=@{};foreach($file in @(Get-ChildItem -LiteralPath $directory -File)){if($file.Name-match ('^'+[regex]::Escape($stem)+'\.(\d+)'+[regex]::Escape($ext)+'$')){$rotated[[int]$Matches[1]]=$file}}
  if($rotated.Count){$maximum=($rotated.Keys|Measure-Object -Maximum).Maximum;foreach($n in 1..$maximum){if(-not$rotated.ContainsKey($n)){throw 'Runtime log rotations contain a gap.'}}}
  $files=@();if($rotated.Count){foreach($n in ($rotated.Keys|Sort-Object -Descending)){$files+=$rotated[$n]}};$files+=Get-Item -LiteralPath $path
  $builder=[Text.StringBuilder]::new();$manifest=@();$bytes=0L;foreach($file in $files){$text=[IO.File]::ReadAllText($file.FullName);[void]$builder.Append($text);$manifest+=[ordered]@{name=$file.Name;bytes=[long]$file.Length;sha256=Hash $file.FullName};$bytes+=$file.Length}
  $manifestJson=ConvertTo-Json -InputObject @($manifest) -Compress -Depth 4;$sha=[Security.Cryptography.SHA256]::Create();try{$setHash=-join($sha.ComputeHash($utf8.GetBytes($manifestJson))|ForEach-Object{$_.ToString('X2')})}finally{$sha.Dispose()}
  [pscustomobject]@{Text=$builder.ToString();Files=$manifest;Count=$files.Count;Bytes=$bytes;Hash=$setHash}
}
function Bmp([string]$Path){
  Add-Type -AssemblyName System.Drawing;$path=Safe $Path 'Environment BMP' -Exists;$image=[Drawing.Bitmap]::new($path);try{if($image.Width-ne1280-or$image.Height-ne720){throw 'Environment BMP is not 1280x720.'}}finally{$image.Dispose()};[ordered]@{sha256=Hash $path;bytes=[long](Get-Item -LiteralPath $path).Length;width=1280;height=720}
}
function Compare-Bmp([string]$Left,[string]$Right){
  Add-Type -AssemblyName System.Drawing;$a=[Drawing.Bitmap]::new($Left);$b=[Drawing.Bitmap]::new($Right);$different=0;$sum=0L;try{for($y=0;$y-lt720;$y+=4){for($x=0;$x-lt1280;$x+=4){$p=$a.GetPixel($x,$y);$q=$b.GetPixel($x,$y);$delta=[math]::Abs([int]$p.R-[int]$q.R)+[math]::Abs([int]$p.G-[int]$q.G)+[math]::Abs([int]$p.B-[int]$q.B);if($delta-ge30){$different++};$sum+=$delta}}}finally{$a.Dispose();$b.Dispose()};[ordered]@{different_samples=$different;mean_channel_delta_milli=[int][math]::Floor(($sum*1000.0)/(320*180*3))}
}
function Probe([string]$LogPath,[string]$Captures){
  $logs=LogSet $LogPath;$text=$logs.Text
  if($text-match'(?i)(\[fatal\]|PPC_UNIMPLEMENTED|guest crash|device removed|DXGI_ERROR_DEVICE|DRED|assertion failed)'){throw 'Fatal/device-loss/crash marker exists.'}
  $config=[regex]::Matches($text,'(?m)^.*MCLA_ENVIRONMENT_EFFECTS_CONFIG v=1 route=arcade-ordered-sunset-and-vine weather=rain time=dawn frames=6 external_close_required=1\s*$');if($config.Count-ne1){throw 'Expected exactly one environment config.'}
  $framePattern='(?m)^.*MCLA_ENVIRONMENT_EFFECTS_FRAME v=1 phase=(?<phase>[a-z-]+) width=1280 height=720 present_seq=(?<seq>\d+) status=PASS\s*$';$frameMatches=[regex]::Matches($text,$framePattern);if($frameMatches.Count-ne6){throw 'Expected exactly six environment frames.'}
  $frames=@();$last=-1L;$index=0;foreach($phase in $phaseFiles.Keys){$match=$frameMatches[$index];if($match.Groups['phase'].Value-cne$phase){throw 'Environment frame chronology is invalid.'};$seq=[long]$match.Groups['seq'].Value;if($seq-le$last){throw 'Present sequences are not strictly increasing.'};$last=$seq;$bmpPath=Join-Path (Safe $Captures 'Environment user root' -Exists) $phaseFiles[$phase];$frames+=[ordered]@{phase=$phase;file=$phaseFiles[$phase];present_seq=$seq;bmp=Bmp $bmpPath};$index++}
  $inputs=[regex]::Matches($text,'(?m)^.*MCLA_FRONTEND_SMOKE_INPUT v=1 side=(?<side>source|guest) sequence=(?<seq>\d+) buttons=(?<buttons>[0-9A-F]{4})\s*$');if($inputs.Count-ne72){throw 'Expected exactly 72 frontend input records.'}
  $buttons=@('0010','0010','0200','1000','1000','1000','0002','0002','0002','0002','0008','0008','0008','0008','0002','0008','0008','1000');$record=0;for($sequence=1;$sequence-le18;$sequence++){foreach($expected in @(@('source',$buttons[$sequence-1]),@('guest',$buttons[$sequence-1]),@('source','0000'),@('guest','0000'))){$m=$inputs[$record];if([int]$m.Groups['seq'].Value-ne$sequence-or$m.Groups['side'].Value-cne$expected[0]-or$m.Groups['buttons'].Value-cne$expected[1]){throw "Frontend causal input record $record is invalid."};$record++}}
  $render=[regex]::Matches($text,'(?m)^.*MCLA_RENDER_SMOKE_INPUT v=1 side=(?<side>source|guest) sequence=(?<seq>[12]) buttons=0000 lt=(?<lt>\d+) rt=(?<rt>\d+) lx=0 ly=0 rx=0 ry=0\s*$');if($render.Count-ne8){throw 'Expected exactly eight render input records.'}
  $expectedRender=@(@('source',1,0,192),@('guest',1,0,192),@('source',1,0,0),@('guest',1,0,0),@('source',2,255,255),@('guest',2,255,255),@('source',2,0,0),@('guest',2,0,0));for($i=0;$i-lt8;$i++){if($render[$i].Groups['side'].Value-cne$expectedRender[$i][0]-or[int]$render[$i].Groups['seq'].Value-ne$expectedRender[$i][1]-or[int]$render[$i].Groups['lt'].Value-ne$expectedRender[$i][2]-or[int]$render[$i].Groups['rt'].Value-ne$expectedRender[$i][3]){throw "Render causal input record $i is invalid."}}
  $summary='MCLA_ENVIRONMENT_EFFECTS_SUMMARY v=1 status=PASS frames=6 frontend_input_records=72 render_input_records=8 weather=rain time=dawn external_close_required=1';if(([regex]::Matches($text,[regex]::Escape($summary))).Count-ne1){throw 'Expected exactly one environment PASS summary.'}
  $orderedNeedles=@('MCLA_ENVIRONMENT_EFFECTS_CONFIG v=1','side=source sequence=1 buttons=0010','phase=dry-night-baseline','side=source sequence=2 buttons=0010','side=guest sequence=17 buttons=0000','phase=rain-dawn-options','side=source sequence=18 buttons=1000','phase=rain-dawn-stationary','side=source sequence=1 buttons=0000 lt=0 rt=192','phase=rain-dawn-moving','side=guest sequence=1 buttons=0000 lt=0 rt=0','phase=rain-dawn-stopped','side=source sequence=2 buttons=0000 lt=255 rt=255','phase=rain-dawn-particle','side=guest sequence=2 buttons=0000 lt=0 rt=0',$summary);$orderedAt=-1;foreach($needle in $orderedNeedles){$next=$text.IndexOf($needle,$orderedAt+1,[StringComparison]::Ordinal);if($next-le$orderedAt){throw "Environment chronology failed at '$needle'."};$orderedAt=$next}
  $summaryAt=$text.IndexOf($summary,[StringComparison]::Ordinal);$closeAt=$text.IndexOf('Window closing, shutting down...',[StringComparison]::Ordinal);$completeAt=$text.IndexOf('Execution complete',[StringComparison]::Ordinal);$exitAt=$text.IndexOf('Title terminated; hard-exiting process.',[StringComparison]::Ordinal);if($summaryAt-lt0-or$closeAt-le$summaryAt-or$completeAt-le$closeAt-or$exitAt-le$completeAt){throw 'Controlled lifecycle chronology is invalid.'};foreach($needle in @('Window closing, shutting down...','Execution complete','Title terminated; hard-exiting process.')){if(([regex]::Matches($text,[regex]::Escape($needle))).Count-ne1){throw "Lifecycle marker '$needle' is not unique."}}
  $root=Safe $Captures 'Environment user root' -Exists;$metrics=[ordered]@{dry_to_rain=Compare-Bmp (Join-Path $root $phaseFiles['dry-night-baseline']) (Join-Path $root $phaseFiles['rain-dawn-stationary']);stationary_to_moving=Compare-Bmp (Join-Path $root $phaseFiles['rain-dawn-stationary']) (Join-Path $root $phaseFiles['rain-dawn-moving']);moving_to_stopped=Compare-Bmp (Join-Path $root $phaseFiles['rain-dawn-moving']) (Join-Path $root $phaseFiles['rain-dawn-stopped']);stopped_to_particle=Compare-Bmp (Join-Path $root $phaseFiles['rain-dawn-stopped']) (Join-Path $root $phaseFiles['rain-dawn-particle'])}
  foreach($metric in $metrics.Values){if($metric.different_samples-lt10000-or$metric.mean_channel_delta_milli-lt5000){throw 'Environment comparison is insufficiently distinct.'}}
  [pscustomobject]@{Passed=$true;LogSet=$logs;Frames=$frames;Metrics=$metrics;FrontendInputRecords=72;RenderInputRecords=8;ControlledExit=$true}
}

if($PSCmdlet.ParameterSetName-eq'Probe'){Probe $RuntimeLogPath $UserRoot;return}
$result=Safe $ResultPath 'M6-004 result' -Exists;$raw=[IO.File]::ReadAllText($result);if($raw-match'(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])'){throw 'Result contains an absolute or private path.'};$record=$raw|ConvertFrom-Json
$required=@('schema','task','decision','sdk_version','sdk_commit','build_configuration','build_log','run_id','prerequisites','probe','coverage','known_issues','game_before','game_after','artifacts_before','artifacts_after','seed_before_sha256','seed_after_sha256','cycle_tree_sha256','cycle_file_count','cycle_directory_count','cycle_bytes','scope');foreach($name in $required){if(-not$record.PSObject.Properties[$name]){throw "Result property '$name' is missing."}};if(@($record.PSObject.Properties).Count-ne$required.Count){throw 'Result contains unknown top-level properties.'}
if($record.schema-cne'mcla-environment-effects-v1'-or$record.task-cne'M6-004'-or$record.decision-cne'representative-environment-effects-pass-open-s2-defects'-or$record.sdk_version-cne'0.9.0.22'-or$record.sdk_commit-cne'576b34fd233acf4579dd2375691dbe86fb4bf8e1'-or$record.build_configuration-cne'Release'){throw 'Result identity is invalid.'}
if((Split-Path (Split-Path $result) -Leaf)-cne$record.run_id){throw 'Result run ID does not match its physical directory.'};foreach($name in $prerequisiteHashes.Keys){$physical=Safe $prerequisitePaths[$name] "Prerequisite $name" -Exists;if([string]$record.prerequisites.$name-cne$prerequisiteHashes[$name]-or(Hash $physical)-cne$prerequisiteHashes[$name]){throw "Prerequisite '$name' hash is invalid."}}
if(-not$record.coverage.day_night-or-not$record.coverage.rainy_weather-or-not$record.coverage.wet_reflections-or-not$record.coverage.post_processing-or-not$record.coverage.motion_blur-or-not$record.coverage.shadows-or-not$record.coverage.particles-or$record.coverage.whole_frame_console_parity_claimed){throw 'Coverage declaration is invalid.'}
if($record.known_issues.ki_013-cne'open-s2-preserved'-or$record.known_issues.ki_015-cne'open-s2-preserved'-or$record.known_issues.ki_016-cne'open-s2-intermittent-preserved'){throw 'Known-issue disposition is invalid.'}
$runRoot=Split-Path $result;$children=@(Get-ChildItem -LiteralPath $runRoot -Force|Sort-Object Name);if(($children.Name-join'|')-cne'release-clean-build.log|result.json|runs'){throw 'M6-004 run topology is invalid.'};$buildLog=Safe (Join-Path $runRoot 'release-clean-build.log') 'Release build log' -Exists;if($record.build_log.sha256-cne(Hash $buildLog)-or[long]$record.build_log.bytes-ne(Get-Item -LiteralPath $buildLog).Length){throw 'Release build log binding is invalid.'};$runs=Safe (Join-Path $runRoot 'runs') 'Runs root' -Exists;if((@(Get-ChildItem -LiteralPath $runs -Force).Name-join'|')-cne'01'){throw 'Runs topology is invalid.'}
$cycle=Safe (Join-Path $runs '01') 'M6-004 cycle' -Exists;$probe=Probe (Join-Path $cycle 'mcla.log') (Join-Path $cycle 'user');if(($probe.Frames|ConvertTo-Json -Compress -Depth 8)-cne($record.probe.frames|ConvertTo-Json -Compress -Depth 8)-or($probe.Metrics|ConvertTo-Json -Compress -Depth 8)-cne($record.probe.metrics|ConvertTo-Json -Compress -Depth 8)-or($probe.LogSet.Files|ConvertTo-Json -Compress -Depth 8)-cne($record.probe.runtime_logs|ConvertTo-Json -Compress -Depth 8)-or$record.probe.runtime_log_set_sha256-cne$probe.LogSet.Hash-or[long]$record.probe.runtime_log_bytes-ne$probe.LogSet.Bytes-or[int]$record.probe.runtime_log_file_count-ne$probe.LogSet.Count-or-not$record.probe.controlled_exit-or[int]$record.probe.exit_code-ne0-or$record.probe.harness_force_cleanup-or$record.probe.condition_selection-cne'deterministic-arcade-menu-rainy-dawn'-or$record.probe.machine_ocr_claimed){throw 'Physical probe binding is invalid.'}
$cycleTree=Tree $cycle;if($record.cycle_tree_sha256-cne$cycleTree.Hash-or[int]$record.cycle_file_count-ne$cycleTree.FileCount-or[int]$record.cycle_directory_count-ne$cycleTree.DirectoryCount-or[long]$record.cycle_bytes-ne$cycleTree.Bytes){throw 'Cycle tree binding is invalid.'}
if(($record.game_before|ConvertTo-Json -Compress)-cne($record.game_after|ConvertTo-Json -Compress)-or($record.artifacts_before|ConvertTo-Json -Compress)-cne($record.artifacts_after|ConvertTo-Json -Compress)-or$record.seed_before_sha256-cne$record.seed_after_sha256){throw 'Data-integrity declaration is invalid.'}
$currentGame=Game (Safe 'private/game' 'Canonical game' -Exists);$currentArtifacts=@(Artifacts (Safe 'out/build/win-amd64-release' 'Canonical Release build' -Exists));$currentSeed=Tree (Safe 'private/evidence/M5-013/20260817-013319-c2e7223f/runs/01/user' 'Completed-save seed' -Exists);if(($currentGame|ConvertTo-Json -Compress)-cne($record.game_after|ConvertTo-Json -Compress)-or($currentArtifacts|ConvertTo-Json -Compress)-cne($record.artifacts_after|ConvertTo-Json -Compress)-or$currentSeed.Hash-cne$record.seed_after_sha256){throw 'Current game, runtime artifacts, or seed no longer match the result.'}
$sdk=Safe 'third_party/rexglue-sdk' 'SDK checkout' -Exists;if((&git -C $sdk rev-parse HEAD).Trim()-cne$record.sdk_commit-or(&git -C $sdk describe --tags --exact-match HEAD).Trim()-cne('v'+$record.sdk_version)){throw 'Current SDK identity does not match the result.'};$exe=Safe 'out/build/win-amd64-release/mcla.exe' 'Canonical Release executable' -Exists;$live=@((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{[string]::Equals($_.Path,$exe,[StringComparison]::OrdinalIgnoreCase)}catch{$false}});if($live.Count){throw 'Canonical MCLA process is still running.'}
[pscustomobject]@{Passed=$true;Decision=$record.decision;DayNightVerified=$true;RainyWeatherVerified=$true;MotionAndParticlesVerified=$true;KnownS2DefectsRemainOpen=$true;ControlledExitVerified=$true;DataIntegrityVerified=$true}
