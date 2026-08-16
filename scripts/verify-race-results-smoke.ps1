[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$RunPath,
  [switch]$Fixture
)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$utf8=[Text.UTF8Encoding]::new($false)

function Resolve-Safe([string]$Path,[string]$Description,[switch]$Exists){$full=if([IO.Path]::IsPathRooted($Path)){[IO.Path]::GetFullPath($Path)}else{[IO.Path]::GetFullPath((Join-Path $repo $Path))};$prefix=$repo.TrimEnd('\')+'\';if(-not$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description escapes repository."};$current=$repo;foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object{$_.Length})){$current=Join-Path $current $part;if((Test-Path -LiteralPath $current)-and((Get-Item -LiteralPath $current -Force).Attributes-band[IO.FileAttributes]::ReparsePoint)){throw "$Description traverses a reparse point."}};if($Exists-and-not(Test-Path -LiteralPath $full)){throw "$Description is missing."};$full}
function Assert-NoReparse([string]$Root){$stack=[Collections.Generic.Stack[string]]::new();$stack.Push($Root);while($stack.Count){foreach($item in @(Get-ChildItem -LiteralPath $stack.Pop() -Force)){if($item.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Evidence contains a reparse point.'};if($item.PSIsContainer){$stack.Push($item.FullName)}}}}
function One([string]$Text,[string]$Pattern,[string]$Name){$m=[regex]::Matches($Text,$Pattern);if($m.Count-ne1){throw "$Name count is $($m.Count), expected 1."};$m[0]}
function Get-Logs([string]$Root){$files=@(Get-ChildItem -LiteralPath $Root -File -Filter 'mcla*.log');if($files.Count-lt1-or$files.Count-gt16){throw 'Runtime log count is invalid.'};$current=@($files|Where-Object{$_.Name-ceq'mcla.log'});if($current.Count-ne1){throw 'Current log count is invalid.'};$rotated=@();foreach($file in @($files|Where-Object{$_.Name-cne'mcla.log'})){$m=[regex]::Match($file.Name,'^mcla\.([1-9][0-9]*)\.log$');if(-not$m.Success){throw 'Malformed rotation.'};$rotated+=[pscustomobject]@{Index=[int]$m.Groups[1].Value;File=$file}};$indices=@($rotated|ForEach-Object{$_.Index}|Sort-Object);for($i=0;$i-lt$indices.Count;$i++){if($indices[$i]-ne$i+1){throw 'Log rotations are not contiguous.'}};$ordered=@($rotated|Sort-Object Index -Descending|ForEach-Object{$_.File})+$current[0];$text='';$manifest=@();$bytes=0L;foreach($file in $ordered){$bytes+=$file.Length;if($bytes-gt134217728){throw 'Logs exceed 128 MiB.'};$text+=[IO.File]::ReadAllText($file.FullName)+"`n";$manifest+=[ordered]@{name=$file.Name;bytes=$file.Length;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}};[pscustomobject]@{Text=$text;Manifest=@($manifest);Bytes=$bytes}}
function Get-Bmp([string]$Path){$p=Resolve-Safe $Path 'BMP' -Exists;$bytes=[IO.File]::ReadAllBytes($p);if($bytes.Length-ne3686454-or$bytes[0]-ne0x42-or$bytes[1]-ne0x4D-or[BitConverter]::ToInt32($bytes,18)-ne1280-or[Math]::Abs([BitConverter]::ToInt32($bytes,22))-ne720-or[BitConverter]::ToUInt16($bytes,28)-ne32){throw 'Capture is not canonical 1280x720 BGRA.'};[ordered]@{name=(Split-Path $p -Leaf);bytes=$bytes.Length;sha256=(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash}}

function Get-Probe([string]$Path){
  $root=Resolve-Safe $Path 'M5-012 cycle' -Exists;Assert-NoReparse $root;$logs=Get-Logs $root;$text=$logs.Text
  foreach($bad in @('status=FAIL','Assertion failed','PPC_UNIMPLEMENTED','Guest crash','DXGI_ERROR_DEVICE_REMOVED','[FATAL]','FATAL:','Invalid or unregistered guest function','MCLA race route: failed')){if($text.Contains($bad)){throw "Cycle contains banned marker '$bad'."}}
  $known='(?:MCLA_RACE_ROUTE_CONFIG v=1 phases=race-start,results,return operator_confirmed=1 external_close_required=1|MCLA_RACE_ROUTE_FRAME v=1 phase=(?:race-start|results|return) width=1280 height=720 present_seq=\d+ status=PASS|MCLA_RACE_ROUTE_SUMMARY v=1 status=PASS frames=3 external_close_required=1)'
  foreach($m in [regex]::Matches($text,'(?m)^.*MCLA_RACE_ROUTE_.*$')){if($m.Value.TrimEnd()-notmatch($known+'$')){throw 'Unknown or malformed race-route marker.'}}
  $config=One $text 'MCLA_RACE_ROUTE_CONFIG v=1 phases=race-start,results,return operator_confirmed=1 external_close_required=1' 'Race config';$last=$config.Index;$captures=@()
  foreach($phase in @('race-start','results','return')){$m=One $text "MCLA_RACE_ROUTE_FRAME v=1 phase=$phase width=1280 height=720 present_seq=(\d+) status=PASS" "$phase frame";if($m.Index-le$last){throw 'Race checkpoint chronology is invalid.'};$seq=[int64]$m.Groups[1].Value;if($seq-le0){throw 'Race checkpoint present sequence is invalid.'};$captures+=Get-Bmp (Join-Path $root "user/mcla-race-$phase.bmp");$last=$m.Index}
  $summary=One $text 'MCLA_RACE_ROUTE_SUMMARY v=1 status=PASS frames=3 external_close_required=1' 'Race summary';$closing=One $text 'Window closing, shutting down\.\.\.' 'Window close';$complete=One $text 'Execution complete' 'Execution complete';$hard=One $text 'Title terminated; hard-exiting process\.' 'Hard exit';if(-not($summary.Index-gt$last-and$closing.Index-gt$summary.Index-and$complete.Index-gt$closing.Index-and$hard.Index-gt$complete.Index)){throw 'Summary/external-close chronology is invalid.'}
  $title=Get-Bmp (Join-Path $root 'user/mcla-first-frame.bmp')
  [ordered]@{controlled_exit=$true;runtime_logs=@($logs.Manifest);runtime_log_bytes=$logs.Bytes;title_capture=$title;checkpoint_captures=@($captures)}
}

$probe=Get-Probe $RunPath
if(-not$Fixture){$root=Resolve-Safe $RunPath 'Cycle' -Exists;$children=@(Get-ChildItem -LiteralPath $root -Force);$dirs=@($children|Where-Object{$_.PSIsContainer}|ForEach-Object{$_.Name}|Sort-Object);$extra=@($children|Where-Object{-not$_.PSIsContainer-and$_.Name-notmatch'^mcla(?:\.[1-9][0-9]*)?\.log$'});if(($dirs-join',')-cne'cache,user'-or$extra.Count){throw 'Cycle topology is invalid.'}}
$probe
