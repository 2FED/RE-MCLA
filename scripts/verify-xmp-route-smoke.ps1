[CmdletBinding(DefaultParameterSetName='Probe')]
param(
  [Parameter(Mandatory,ParameterSetName='Probe')][string]$RuntimeLogPath,
  [Parameter(Mandatory,ParameterSetName='Probe')][string]$BmpPath,
  [Parameter(Mandatory,ParameterSetName='Probe')][switch]$ProbeOnly,
  [Parameter(ParameterSetName='Probe')][switch]$XmpOnly,
  [Parameter(Mandatory,ParameterSetName='Result')][string]$ResultPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$utf8=[Text.UTF8Encoding]::new($false)

function Resolve-Safe([string]$Path,[string]$Description){
  $full=[IO.Path]::GetFullPath($(if([IO.Path]::IsPathRooted($Path)){$Path}else{Join-Path $repo $Path}))
  $prefix=$repo.TrimEnd('\')+'\';if(-not$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description escapes the repository."}
  $current=$repo;foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object Length)){$current=Join-Path $current $part;if(-not(Test-Path $current)){throw "$Description is missing."};if((Get-Item $current -Force).Attributes-band[IO.FileAttributes]::ReparsePoint){throw "$Description traverses a reparse point."}}
  $full
}
function Assert-Tree([string]$Root){$pending=[Collections.Generic.Stack[string]]::new();$pending.Push($Root);while($pending.Count){$dir=$pending.Pop();foreach($item in @(Get-ChildItem -LiteralPath $dir -Force)){if($item.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Tree contains a reparse point.'};if($item.PSIsContainer){$pending.Push($item.FullName)}}}}
function Get-Tree([string]$Root){
  $root=Resolve-Safe $Root 'Tree root';Assert-Tree $root;$items=@(Get-ChildItem $root -Recurse -Force);$entries=@();$files=@($items|Where-Object{-not$_.PSIsContainer}|Sort-Object FullName)
  foreach($dir in @($items|Where-Object PSIsContainer|Sort-Object FullName)){$entries+=[ordered]@{kind='directory';path=$dir.FullName.Substring($root.Length).TrimStart('\').Replace('\','/')}}
  foreach($file in $files){$entries+=[ordered]@{kind='file';path=$file.FullName.Substring($root.Length).TrimStart('\').Replace('\','/');length=$file.Length;sha256=(Get-FileHash $file.FullName -Algorithm SHA256).Hash}}
  $json=ConvertTo-Json -InputObject @($entries) -Compress -Depth 4;$sha=[Security.Cryptography.SHA256]::Create();try{$hash=-join($sha.ComputeHash($utf8.GetBytes($json))|ForEach-Object{$_.ToString('X2')})}finally{$sha.Dispose()}
  $bytes=0L;foreach($file in $files){$bytes+=$file.Length};[pscustomobject]@{Hash=$hash;FileCount=$files.Count;DirectoryCount=@($items|Where-Object PSIsContainer).Count;Bytes=$bytes}
}
function Get-LogSet([string]$Current){
  $current=Resolve-Safe $Current 'Runtime log';if((Split-Path $current -Leaf)-cne'mcla.log'){throw 'Current runtime log must be mcla.log.'}
  $files=@(Get-ChildItem (Split-Path $current) -File -Filter 'mcla*.log');$rot=@();$now=$null
  foreach($file in $files){if($file.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Runtime log is a reparse point.'};if($file.Name-ceq'mcla.log'){$now=$file;continue};$match=[regex]::Match($file.Name,'^mcla\.([1-9][0-9]*)\.log$');if(-not$match.Success){throw 'Malformed log rotation.'};$rot+=[pscustomobject]@{N=[int]$match.Groups[1].Value;F=$file}}
  if(-not$now-or$files.Count-lt1-or$files.Count-gt16){throw 'Runtime log topology is invalid.'};$ids=@($rot|ForEach-Object{$_.N}|Sort-Object);for($i=0;$i-lt$ids.Count;$i++){if($ids[$i]-ne$i+1){throw 'Runtime log rotations are not contiguous.'}}
  $ordered=@($rot|Sort-Object N -Descending|ForEach-Object F)+$now;$parts=@();$manifest=@();$bytes=0L
  foreach($file in $ordered){$bytes+=$file.Length;if($bytes-gt134217728){throw 'Runtime log set exceeds 128 MiB.'};$parts+=[IO.File]::ReadAllText($file.FullName);$manifest+=[ordered]@{name=$file.Name;bytes=$file.Length;sha256=(Get-FileHash $file.FullName -Algorithm SHA256).Hash}}
  $json=ConvertTo-Json -InputObject @($manifest) -Compress -Depth 3;$sha=[Security.Cryptography.SHA256]::Create();try{$hash=-join($sha.ComputeHash($utf8.GetBytes($json))|ForEach-Object{$_.ToString('X2')})}finally{$sha.Dispose()}
  [pscustomobject]@{Text=$parts-join"`n";Files=@($manifest);Count=$manifest.Count;Bytes=$bytes;Hash=$hash}
}
function One([regex]$Regex,[string]$Text,[string]$Name){$matches=$Regex.Matches($Text);if($matches.Count-ne1){throw "$Name must occur exactly once."};$matches[0]}
function Get-Bmp([string]$Path){
  $path=Resolve-Safe $Path 'Title BMP';$bytes=[IO.File]::ReadAllBytes($path);if($bytes.Length-ne3686454-or$bytes[0]-ne0x42-or$bytes[1]-ne0x4D-or[BitConverter]::ToInt32($bytes,18)-ne1280-or[BitConverter]::ToInt32($bytes,22)-ne720-or[BitConverter]::ToUInt16($bytes,28)-ne32){throw 'Capture is not the canonical 1280x720x32 BMP.'}
  [pscustomobject]@{Sha256=(Get-FileHash $path -Algorithm SHA256).Hash;Bytes=$bytes.Length;Width=1280;Height=720;BitsPerPixel=32}
}
function Get-Probe([string]$LogPath,[string]$FramePath,[bool]$SkipTitle=$false){
  $log=Get-LogSet $LogPath;$text=$log.Text
  $config=One ([regex]'(?m)^.*XMP_AUDIT_CONFIG v=1 enabled=1 policy=metadata-only-fallback decoder=0 record_limit=128\s*$') $text 'XMP config'
  $records=[regex]::Matches($text,'(?m)^.*XMP_AUDIT_MESSAGE v=1 id=(?<id>[0-9]+) msg=(?<msg>[0-9A-F]{8}) class=(?<class>query|playback|metadata|unknown) result=(?<result>[0-9A-F]{8}) before=(?<before>[0-9]+) after=(?<after>[0-9]+) client=(?<client>[0-9]+) playlists=(?<playlists>[0-9]+) active_songs=(?<songs>[0-9]+) decoder=(?<decoder>[01]) consistent=(?<consistent>[01])\s*$')
  if($records.Count-ne1){throw 'Expected exactly one bounded XMP record on the title route.'};$record=$records[0]
  if($record.Groups['id'].Value-ne'0'-or$record.Groups['msg'].Value-ne'00070009'-or$record.Groups['class'].Value-ne'query'-or$record.Groups['result'].Value-ne'00000000'-or$record.Groups['before'].Value-ne'0'-or$record.Groups['after'].Value-ne'0'-or$record.Groups['client'].Value-ne'1'-or$record.Groups['playlists'].Value-ne'0'-or$record.Groups['songs'].Value-ne'0'-or$record.Groups['decoder'].Value-ne'0'-or$record.Groups['consistent'].Value-ne'1'){throw 'XMP status record is not the exact idle metadata-only fallback.'}
  $summary=One ([regex]'(?m)^.*XMP_AUDIT_SUMMARY v=1 phase=title status=PASS calls=(?<calls>[0-9]+) known_calls=(?<known>[0-9]+) query_calls=(?<queries>[0-9]+) playback_calls=(?<playback>[0-9]+) unsupported_calls=(?<unsupported>[0-9]+) state_changes=(?<changes>[0-9]+) unexpected_calls=(?<unexpected>[0-9]+) inconsistent_calls=(?<inconsistent>[0-9]+) records=(?<records>[0-9]+) overflow=(?<overflow>[01]) dropped_records=(?<dropped>[0-9]+)\s*$') $text 'XMP summary'
  $calls=[uint64]$summary.Groups['calls'].Value;$known=[uint64]$summary.Groups['known'].Value;$queries=[uint64]$summary.Groups['queries'].Value;if($calls-lt1000-or$known-ne$calls-or$queries-ne$calls){throw 'XMP title query totals are incomplete.'}
  foreach($name in @('playback','unsupported','changes','unexpected','inconsistent','overflow','dropped')){if([uint64]$summary.Groups[$name].Value-ne0){throw "XMP title counter '$name' must be zero."}}
  if([uint64]$summary.Groups['records'].Value-ne1){throw 'XMP bounded summary cardinality failed.'}
  $capture=One ([regex]'(?m)^.*MCLA graphics: nontrivial guest frame captured 1280x720,.*$') $text 'Title capture marker'
  $project=One ([regex]'(?m)^.*MCLA audio: XMP title route summarized\s*$') $text 'Project XMP marker'
  $close=One ([regex]'(?m)^.*Window closing, shutting down\.\.\.\s*$') $text 'Window close marker'
  $complete=One ([regex]'(?m)^.*Execution complete\s*$') $text 'Execution complete marker'
  $hard=One ([regex]'(?m)^.*Title terminated; hard-exiting process\.\s*$') $text 'Hard-exit marker'
  if(-not($config.Index-lt$record.Index-and$record.Index-lt$capture.Index-and$capture.Index-lt$summary.Index-and$summary.Index-lt$project.Index-and$project.Index-lt$close.Index-and$close.Index-lt$complete.Index-and$complete.Index-lt$hard.Index)){throw 'XMP/title/lifecycle chronology is invalid.'}
  if($text-match'(?i)(Unimplemented XMP|XMP output not unimplemented|PPC_UNIMPLEMENTED|Guest crash|\[fatal\]|D3D12.*device (?:lost|removed))'){throw 'Fatal, device-loss, or unimplemented XMP marker present.'}
  if(-not$SkipTitle){$null=&(Join-Path $PSScriptRoot 'verify-render-path-smoke.ps1') -ProbeOnly -RuntimeLogPath $LogPath -BmpPath $FramePath}
  [pscustomobject]@{Passed=$true;Calls=$calls;QueryCalls=$queries;PlaybackCalls=0;StateChanges=0;LogSet=$log;Bmp=(Get-Bmp $FramePath)}
}

if($PSCmdlet.ParameterSetName-eq'Probe'){if(-not$ProbeOnly){throw 'Probe inputs require -ProbeOnly.'};Get-Probe $RuntimeLogPath $BmpPath $XmpOnly.IsPresent;return}
$result=Resolve-Safe $ResultPath 'Result';$json=[IO.File]::ReadAllText($result);if($json-match'(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])'){throw 'Result contains a private or absolute path.'};$r=$json|ConvertFrom-Json
if($r.schema-ne1-or$r.task-cne'M4-008'-or$r.decision-cne'xmp-metadata-only-fallback-pass'-or$r.cycle_count-ne1-or$r.clean_build_performed-ne$true-or$r.sdk_version-cne'0.9.0.15'){throw 'Canonical XMP aggregate identity is invalid.'}
$root=Split-Path $result;$cycleRoot=Resolve-Safe (Join-Path $root 'runs\01') 'Cycle root';$probe=Get-Probe (Join-Path $cycleRoot 'mcla.log') (Join-Path $cycleRoot 'user\mcla-first-frame.bmp')
if($r.cycle.runtime_log_set_sha256-cne$probe.LogSet.Hash-or$r.cycle.capture_sha256-cne$probe.Bmp.Sha256-or[uint64]$r.cycle.calls-ne$probe.Calls-or[uint64]$r.cycle.query_calls-ne$probe.QueryCalls-or[uint64]$r.cycle.playback_calls-ne0-or[uint64]$r.cycle.state_changes-ne0-or$r.cycle.exit_code-ne0-or$r.cycle.harness_force_cleanup-ne$false-or$r.no_surviving_processes-ne$true-or$r.data_integrity_preserved-ne$true){throw 'Canonical XMP aggregate is not bound to physical evidence.'}
$tree=Get-Tree $cycleRoot;if($r.cycle.cycle_tree_sha256-cne$tree.Hash-or[uint64]$r.cycle.cycle_file_count-ne$tree.FileCount-or[uint64]$r.cycle.cycle_bytes-ne$tree.Bytes){throw 'Cycle tree identity mismatch.'}
if(@($r.cycle.runtime_logs).Count-ne$probe.LogSet.Count){throw 'Runtime log manifest count mismatch.'};for($i=0;$i-lt$probe.LogSet.Count;$i++){foreach($field in @('name','bytes','sha256')){if($r.cycle.runtime_logs[$i].$field-cne$probe.LogSet.Files[$i].$field){throw 'Runtime log manifest mismatch.'}}}
if($r.build.focused_test_cases-ne4-or$r.build.focused_test_assertions-ne20){throw 'Focused XMP test totals changed.'};foreach($pair in @(@('sdk-install.log','sdk_install_log_sha256'),@('sdk-xmp-test.log','focused_test_log_sha256'),@('relwithdebinfo-clean-build.log','app_build_log_sha256'))){$path=Resolve-Safe (Join-Path $root $pair[0]) 'Build log';if((Get-FileHash $path -Algorithm SHA256).Hash-cne$r.build.($pair[1])){throw 'Clean-build log hash mismatch.'}}
if(($r.game_identity.before|ConvertTo-Json -Compress)-cne($r.game_identity.after|ConvertTo-Json -Compress)){throw 'Source-game identity changed.'}
$canonicalGame=Resolve-Safe (Join-Path $repo 'private\game') 'Canonical game';$physicalGameTree=Get-Tree $canonicalGame;$physicalGame=&(Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath $canonicalGame -VerifyHashes;$expectedGame=$r.game_identity.after;if([uint64]$expectedGame.hashes_verified-ne$physicalGame.FileCount-or[uint64]$expectedGame.file_count-ne$physicalGame.FileCount-or[uint64]$expectedGame.payload_bytes-ne$physicalGame.PayloadBytes-or$expectedGame.manifest_sha256-cne(Get-FileHash $physicalGame.ManifestPath -Algorithm SHA256).Hash-or$expectedGame.tree_sha256-cne$physicalGameTree.Hash-or[uint64]$expectedGame.tree_file_count-ne$physicalGameTree.FileCount-or[uint64]$expectedGame.tree_directory_count-ne$physicalGameTree.DirectoryCount-or[uint64]$expectedGame.tree_bytes-ne$physicalGameTree.Bytes){throw 'Canonical source-game physical identity mismatch.'}
$canonicalBuild=Resolve-Safe (Join-Path $repo 'out\build\win-amd64-relwithdebinfo') 'Canonical build';if(@($r.artifacts.before).Count-ne4-or@($r.artifacts.after).Count-ne4){throw 'Runtime artifact count is invalid.'};foreach($artifact in $r.artifacts.after){if($artifact.name-notin@('mcla.exe','rexruntimerd.dll','TracyClientrd.dll','rexgpu-xenosrd.dll')-or(Get-FileHash (Resolve-Safe (Join-Path $canonicalBuild $artifact.name) 'Runtime artifact') -Algorithm SHA256).Hash-cne$artifact.sha256){throw 'Runtime artifact hash mismatch.'}}
if(($r.artifacts.before|ConvertTo-Json -Compress)-cne($r.artifacts.after|ConvertTo-Json -Compress)){throw 'Runtime artifacts changed during the cycle.'}
$exe=Join-Path $canonicalBuild 'mcla.exe';if(@((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{[string]::Equals($_.Path,$exe,[StringComparison]::OrdinalIgnoreCase)}catch{$false}}).Count){throw 'Canonical MCLA process still survives.'}
[pscustomobject]@{Passed=$true;Decision=$r.decision;Calls=$probe.Calls;PlaybackCalls=0;State='idle';ControlledLifecycleVerified=$true;DataIntegrityVerified=$true}
