[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$SourceUserRoot,
  [Parameter(Mandatory)][string]$ArchiveRoot,
  [int]$MclaProcessId,
  [ValidateRange(2,300)][int]$PollSeconds=10,
  [switch]$Once
)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$utf8=[Text.UTF8Encoding]::new($false)
$saveRelative='B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
$headerRelative='B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'

function Resolve-Contained([string]$Path,[string]$Description,[switch]$Exists){
  $full=if([IO.Path]::IsPathRooted($Path)){[IO.Path]::GetFullPath($Path)}else{[IO.Path]::GetFullPath((Join-Path $repo $Path))}
  $prefix=$repo.TrimEnd('\')+'\';if(-not$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description escapes repository."}
  $cursor=$repo;foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object Length)){$cursor=Join-Path $cursor $part;if((Test-Path $cursor)-and((Get-Item $cursor -Force).Attributes-band[IO.FileAttributes]::ReparsePoint)){throw "$Description traverses a reparse point."}}
  if($Exists-and-not(Test-Path -LiteralPath $full)){throw "$Description is missing."};$full
}
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
function Write-Json([string]$Path,$Value){[IO.File]::WriteAllText($Path,((ConvertTo-Json $Value -Depth 6)+[Environment]::NewLine),$utf8)}
function Get-Pair([string]$Save,[string]$Header){
  if(-not(Test-Path -LiteralPath $Save)-or-not(Test-Path -LiteralPath $Header)){return $null}
  try{[pscustomobject]@{save=(Hash $Save);header=(Hash $Header)}}catch{return $null}
}
function Save-Snapshot([string]$Source,[string]$DestinationRoot,[string]$Reason){
  $save=Join-Path $Source $saveRelative;$header=Join-Path $Source $headerRelative;$first=Get-Pair $save $header;if($null-eq$first){return $null}
  Start-Sleep -Seconds 2;$second=Get-Pair $save $header;if($null-eq$second-or$first.save-cne$second.save-or$first.header-cne$second.header){return $null}
  $identity="$($second.save)-$($second.header)";$latest=Join-Path $DestinationRoot 'latest.json'
  if(Test-Path -LiteralPath $latest){try{$known=Get-Content -LiteralPath $latest -Raw|ConvertFrom-Json;if($known.identity-cne$identity){$known=$null}else{return $known}}catch{}}
  $stamp=(Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmssZ');$name="$stamp-$($second.save.Substring(0,16))-$($second.header.Substring(0,16))";$partial=Join-Path $DestinationRoot ($name+'.partial');$final=Join-Path $DestinationRoot $name
  [IO.Directory]::CreateDirectory($partial)|Out-Null;Copy-Item -LiteralPath (Join-Path $Source 'B13EBABEBABEBABE') -Destination $partial -Recurse
  $copiedSave=Join-Path $partial $saveRelative;$copiedHeader=Join-Path $partial $headerRelative;if((Hash $copiedSave)-cne$second.save-or(Hash $copiedHeader)-cne$second.header){throw 'Copied soak save snapshot failed hash verification.'}
  $record=[ordered]@{schema='mcla-soak-save-snapshot-v1';identity=$identity;captured_utc=[DateTime]::UtcNow.ToString('O');reason=$Reason;source_user_root=$Source.Substring($repo.TrimEnd('\').Length+1).Replace('\','/');snapshot_directory=$name;save_sha256=$second.save;save_bytes=[long](Get-Item $copiedSave).Length;header_sha256=$second.header;header_bytes=[long](Get-Item $copiedHeader).Length;complete_profile_tree=$true}
  Write-Json (Join-Path $partial 'snapshot.json') $record;Move-Item -LiteralPath $partial -Destination $final;Write-Json $latest $record;Write-Output "SOAK_SAVE_SNAPSHOT status=PASS reason=$Reason directory=$name save=$($second.save)";$record
}

$source=Resolve-Contained $SourceUserRoot 'Source user root' -Exists;$archive=Resolve-Contained $ArchiveRoot 'Archive root';[IO.Directory]::CreateDirectory($archive)|Out-Null
if($Once){Save-Snapshot $source $archive 'manual-once'|Out-Null;return}
if($MclaProcessId-le0){throw 'MclaProcessId is required unless Once is selected.'}
$process=Get-Process -Id $MclaProcessId -ErrorAction Stop;$expected=(Resolve-Path (Join-Path $repo 'out/build/win-amd64-release/mcla.exe')).Path;if($process.Path-cne$expected){throw 'MclaProcessId does not identify the canonical Release executable.'}
Save-Snapshot $source $archive 'watch-start'|Out-Null
while(-not$process.HasExited){Start-Sleep -Seconds $PollSeconds;$process.Refresh();if(-not$process.HasExited){Save-Snapshot $source $archive 'save-change'|Out-Null}}
Save-Snapshot $source $archive 'process-exit'|Out-Null
