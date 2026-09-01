[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify=Join-Path $PSScriptRoot 'verify-race-results-smoke.ps1'
$run=Join-Path $PSScriptRoot 'run-race-results-smoke.ps1'
$root=Join-Path $repo ('private/evidence/M5-012/test-'+[guid]::NewGuid().ToString('N'))
$utf8=[Text.UTF8Encoding]::new($false)
function Write-Bmp([string]$Path){$bytes=[byte[]]::new(3686454);$bytes[0]=0x42;$bytes[1]=0x4D;[BitConverter]::GetBytes([int]1280).CopyTo($bytes,18);[BitConverter]::GetBytes([int]720).CopyTo($bytes,22);[BitConverter]::GetBytes([uint16]32).CopyTo($bytes,28);[IO.File]::WriteAllBytes($Path,$bytes)}
function Get-GeneratedFunction([string]$Text,[string]$Name){$start=$Text.IndexOf("DEFINE_REX_FUNC($Name)",[StringComparison]::Ordinal);if($start-lt0){return ''};$next=$Text.IndexOf('DEFINE_REX_FUNC(',$start+1,[StringComparison]::Ordinal);if($next-lt0){$next=$Text.Length};$Text.Substring($start,$next-$start)}
function New-Probe([string]$Path){[IO.Directory]::CreateDirectory((Join-Path $Path 'cache'))|Out-Null;[IO.Directory]::CreateDirectory((Join-Path $Path 'user'))|Out-Null;foreach($name in @('mcla-first-frame.bmp','mcla-race-race-start.bmp','mcla-race-results.bmp','mcla-race-return.bmp')){Write-Bmp (Join-Path $Path "user/$name")};$lines=@('MCLA_RACE_ROUTE_CONFIG v=1 phases=race-start,results,return operator_confirmed=1 external_close_required=1','MCLA_RACE_ROUTE_FRAME v=1 phase=race-start width=1280 height=720 present_seq=10 status=PASS','MCLA_RACE_ROUTE_FRAME v=1 phase=results width=1280 height=720 present_seq=20 status=PASS','MCLA_RACE_ROUTE_FRAME v=1 phase=return width=1280 height=720 present_seq=30 status=PASS','MCLA_RACE_ROUTE_SUMMARY v=1 status=PASS frames=3 external_close_required=1','Window closing, shutting down...','Execution complete','Title terminated; hard-exiting process.');[IO.File]::WriteAllLines((Join-Path $Path 'mcla.log'),$lines,$utf8)}
try{
  New-Probe $root
  $positive=&$verify -RunPath $root -Fixture
  if(-not$positive.controlled_exit-or@($positive.checkpoint_captures).Count-ne3){throw 'Positive fixture failed.'}
  $log=Join-Path $root 'mcla.log';$original=[IO.File]::ReadAllText($log)
  $negative=0
  foreach($mutation in @(
    {param($x)$x.Replace('phase=results width=1280','phase=wrong width=1280')},
    {param($x)$x.Replace('status=PASS frames=3','status=FAIL frames=3')},
    {param($x)$x.Replace("Window closing, shutting down...`r`n",'')},
    {param($x)$x.Replace('present_seq=20','present_seq=0')},
    {param($x)$x.Replace('MCLA_RACE_ROUTE_SUMMARY','MCLA_RACE_ROUTE_UNKNOWN')},
    {param($x)$x+"`r`nFATAL: Invalid or unregistered guest function 0xDEADBEEF"}
  )){[IO.File]::WriteAllText($log,(&$mutation $original),$utf8);try{&$verify -RunPath $root -Fixture|Out-Null;throw 'Negative fixture was accepted.'}catch{if($_.Exception.Message-ceq'Negative fixture was accepted.'){throw};$negative++}}
  [IO.File]::WriteAllText($log,$original,$utf8)
  $app=[IO.File]::ReadAllText((Join-Path $repo 'src/mcla_app.cpp'));$runner=[IO.File]::ReadAllText($run);$validator=[IO.File]::ReadAllText($verify)
  $functionConfig=[IO.File]::ReadAllText((Join-Path $repo 'config/mcla_functions.toml'))
  $register=[IO.File]::ReadAllText((Join-Path $repo 'generated/default/mcla_register.cpp'))
  $generated10=[IO.File]::ReadAllText((Join-Path $repo 'generated/default/mcla_recomp.10.cpp'))
  $generated14=[IO.File]::ReadAllText((Join-Path $repo 'generated/default/mcla_recomp.14.cpp'))
  $needles=@('mcla_race_route_probe','.mcla-race-','MCLA_RACE_ROUTE_CONFIG','MCLA_RACE_ROUTE_FRAME','MCLA_RACE_ROUTE_SUMMARY','race_capture_deadline','RACE START 2/2 IAN','RACE START 2/2 MARTIN','FINAL SERIES RESULTS','every NEXT RACE event','RETURN FREE ROAM','CycleCount = 1','InitialUserRoot','ReuseCurrentBuild','0x82264770','0x82262320','30034-entry build','IGNORED - game remains running','while ($Exact -cnotcontains $confirmation)','single-series-route-evidence-pass','repeated-race resource checks belong to M5-013','separate Release restart gate closes persistence')
  foreach($needle in $needles){if(-not($app.Contains($needle)-or$runner.Contains($needle)-or$validator.Contains($needle))){throw "Source contract missing '$needle'."}}
  $functionContracts=@(
    @('"0x82262320" = { end = 0x8226233C, name = "sub_82262320" }','DEFINE_REX_FUNC(sub_82262320)','// blr'),
    @('"0x82264760" = { end = 0x82264770, name = "sub_82264760" }','DEFINE_REX_FUNC(sub_82264760)','// b 0x822646e8'),
    @('"0x82264770" = { end = 0x82264780, name = "sub_82264770" }','DEFINE_REX_FUNC(sub_82264770)','// b 0x822c9b10'),
    @('"0x822C9FE8" = { end = 0x822CA04C, name = "sub_822C9FE8" }','DEFINE_REX_FUNC(sub_822C9FE8)','// b 0x823889b0')
  )
  foreach($contract in $functionContracts){$address=[regex]::Match($contract[0],'0x[0-9A-F]+').Value;$name=[regex]::Match($contract[1],'sub_[0-9A-F]+').Value;$body10=Get-GeneratedFunction $generated10 $name;$body14=Get-GeneratedFunction $generated14 $name;if(-not$functionConfig.Contains($contract[0])-or-not$register.Contains("registrar->SetFunction($address")-or-not(($body10.Contains($contract[2]))-or($body14.Contains($contract[2])))){throw "Generated function contract failed for $address."}}
  [pscustomobject][ordered]@{PositiveFixtures=1;FailClosedNegatives=$negative;SourceChecks=($needles.Count+$functionContracts.Count)}
}finally{if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force}}
