[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify=Join-Path $PSScriptRoot 'verify-race-system-smoke.ps1'
$run=Join-Path $PSScriptRoot 'run-race-system-smoke.ps1'
$app=Join-Path $repo 'src/mcla_app.cpp'
$root=Join-Path $repo ('private/evidence/M6-003/test-'+[guid]::NewGuid().ToString('N'))
$calibrationRoot=$root+'-calibration'
$utf8=[Text.UTF8Encoding]::new($false)
$scope='one current representative Ian or Martin head-to-head event with machine-observed Race_Finish invocation, current start/reward frames, and external owner completion/police observation; prior immutable Ian series, five-race reward, and traffic evidence; optional description/checkpoint/UI-result wrappers may be zero-hit and are not required or claimed as current machine telemetry; no claim of every race type, opponent, checkpoint implementation, police behavior, reward amount, difficulty, or campaign branch'

function Write-Bmp([string]$Path,[byte]$Value){$bytes=[byte[]]::new(3686454);$bytes[0]=0x42;$bytes[1]=0x4D;[BitConverter]::GetBytes([int]1280).CopyTo($bytes,18);[BitConverter]::GetBytes([int]720).CopyTo($bytes,22);[BitConverter]::GetBytes([uint16]32).CopyTo($bytes,28);for($i=54;$i-lt$bytes.Length;$i+=4096){$bytes[$i]=$Value};[IO.File]::WriteAllBytes($Path,$bytes)}
function Write-Json([string]$Path,$Value){[IO.File]::WriteAllText($Path,(ConvertTo-Json $Value -Depth 14)+[Environment]::NewLine,$utf8)}
function New-Fixture([string]$Path,[switch]$Calibration){$cycle=Join-Path $Path 'runs/01';$user=Join-Path $cycle 'user';$cache=Join-Path $cycle 'cache';[IO.Directory]::CreateDirectory($user)|Out-Null;[IO.Directory]::CreateDirectory($cache)|Out-Null;Copy-Item -LiteralPath (Join-Path $repo 'private/evidence/M5-013/20260817-013319-c2e7223f/runs/01/user/B13EBABEBABEBABE') -Destination $user -Recurse;[IO.File]::WriteAllLines((Join-Path $Path 'release-clean-build.log'),@('CMAKE_BUILD_TYPE="Release"','REXSDK_VERSION="0.9.0.22"','Cleaning all built files','Linking CXX executable mcla.exe'),$utf8);Write-Json (Join-Path $Path 'operator-observation.json') ([ordered]@{schema='mcla-race-system-operator-observation-v1';opponent='MARTIN';event_completed=$true;police_observed=$false;prior_police_observed=$true;source='owner-console-transcript-and-chat-report';recorded_in_runtime_log=$false;machine_verified=$false});Write-Json (Join-Path $Path 'physical-artifacts.json') @([ordered]@{name='mcla.exe';sha256=('A'*64)},[ordered]@{name='rexruntime.dll';sha256=('B'*64)},[ordered]@{name='TracyClient.dll';sha256=('C'*64)},[ordered]@{name='rexgpu-xenos.dll';sha256=('D'*64)});$status=if($Calibration){'FAIL'}else{'PASS'};$lines=@(
  'XAM_USER_SIGNIN_CONFIG v=1 state=2 mode=online-compatible',
  'MCLA_RACE_SYSTEM_CONFIG v=1 enabled=1 desc_type=822C9230 desc_subtype=822C9270 cop_zones=822C92B0 checkpoint_count=82267528 checkpoint_hit=82263930 finish=82256BE0 result=821FBB40 detail_cap=32',
  'KernelState: Preparing module launch...',
  'MCLA_RACE_SYSTEM_READY v=1 phases=start,rewards external_close_required=1',
  'MCLA_RACE_SYSTEM_FRAME v=1 phase=start width=1280 height=720 present_seq=100 status=PASS',
  'MCLA_RACE_SYSTEM_FINISH v=1 record=1 category=0 finished=1 arrested=0 winning_time=4294967295 desc_known=0 race_type=4294967295 race_subtype=4294967295 cop_zones=4294967295 checkpoint_max=0',
  'MCLA_RACE_SYSTEM_FRAME v=1 phase=rewards width=1280 height=720 present_seq=200 status=PASS',
  "MCLA_RACE_SYSTEM_SUMMARY v=1 status=$status frames=2 desc_calls=0 desc_complete=0 checkpoint_count_calls=0 checkpoint_max=0 checkpoint_hits=0 finish_calls=1 result_calls=0 arrested_finishes=0 category=0 race_type=4294967295 race_subtype=4294967295 cop_zones=4294967295 winning_time=4294967295 detail_records=1 dropped_records=0 external_close_required=1"
);if(-not$Calibration){$lines+=@('Window closing, shutting down...','Execution complete','Title terminated; hard-exiting process.')};[IO.File]::WriteAllLines((Join-Path $cycle 'mcla.log'),$lines,$utf8);Write-Bmp (Join-Path $user 'mcla-race-system-start.bmp') 1;Write-Bmp (Join-Path $user 'mcla-race-system-rewards.bmp') 2;if($Calibration){$recovery=Join-Path $Path 'runs/02';[IO.Directory]::CreateDirectory($recovery)|Out-Null;[IO.File]::WriteAllLines((Join-Path $recovery 'mcla.log'),@('MCLA graphics: nontrivial guest frame captured 1280x720','Window closing, shutting down...','Execution complete','Title terminated; hard-exiting process.'),$utf8)}}
function Fail([scriptblock]$Action,[string]$Name){try{&$Action|Out-Null;throw "Negative '$Name' accepted."}catch{if($_.Exception.Message-ceq"Negative '$Name' accepted."){throw}}}

try{
  New-Fixture $root;$probe=&$verify -RunPath $root -Fixture;if($probe.decision-cne'representative-race-system-matrix-pass'-or$probe.current_event.finish_wrapper_calls-ne1-or$probe.current_event.telemetry_gap_reclassified-or-not$probe.current_event.event_process_controlled_exit){throw 'Positive run fixture failed.'}
  New-Fixture $calibrationRoot -Calibration;$calibrationProbe=&$verify -RunPath $calibrationRoot -Fixture;if(-not$calibrationProbe.current_event.telemetry_gap_reclassified-or$calibrationProbe.current_event.event_process_controlled_exit-or-not$calibrationProbe.lifecycle_recovery.controlled_exit){throw 'Calibration recovery fixture failed.'}
  $record=[ordered]@{schema='mcla-race-system-v1';task='M6-003';decision='representative-race-system-matrix-pass';sdk_version='0.9.0.22';sdk_commit='576b34fd233acf4579dd2375691dbe86fb4bf8e1';build_configuration='Release';route_id='representative-ian-martin-race-v1';opponent='MARTIN';police_observation='not-seen';traffic_result_sha256='299392E59CB38AFB44256E773884856FF0C869A96D2045086A65396C1ED4EFCA';series_result_sha256='D993E2612D1AC769D88264C83FD9C9186BC761E2067317F5A7EB66038C250E58';resource_result_sha256='D890A903775A3B53262B9957E7B7DC1D4B76B49B8735360BECD8233842446298';seed_save_sha256='126F7482878C7AACB09AA6795331C906DFB9C4218BE94EDB1D8E51B27CA78AB2';seed_header_sha256='5827A913515AC0E5D55BB56AEC56DE99CACC0ABB7C8061F59336DF4CEA4A8731';signin_compatibility_state=2;probe=$probe;release_artifacts=@();scope=$scope};Write-Json (Join-Path $root 'result.json') $record;$persisted=&$verify -ResultPath (Join-Path $root 'result.json') -Fixture;if(-not$persisted.DataIntegrityVerified-or$persisted.CurrentOpponentObserved-cne'MARTIN'){throw 'Persisted result fixture failed.'}
  $record.opponent='IAN';Write-Json (Join-Path $root 'result.json') $record;Fail {&$verify -ResultPath (Join-Path $root 'result.json') -Fixture} 'result opponent observation mismatch';$negative=1;$record.opponent='MARTIN';$record.police_observation='seen';Write-Json (Join-Path $root 'result.json') $record;Fail {&$verify -ResultPath (Join-Path $root 'result.json') -Fixture} 'result police observation mismatch';$negative++;$record.police_observation='not-seen';Write-Json (Join-Path $root 'result.json') $record
  $log=Join-Path $root 'runs/01/mcla.log';$original=[IO.File]::ReadAllText($log);$cases=@(
    @{n='fatal';f={param($x)$x+"`n[FATAL] fixture"}},
    @{n='missing config';f={param($x)$x.Replace('MCLA_RACE_SYSTEM_CONFIG v=1 enabled=1 desc_type=822C9230 desc_subtype=822C9270 cop_zones=822C92B0 checkpoint_count=82267528 checkpoint_hit=82263930 finish=82256BE0 result=821FBB40 detail_cap=32','')}},
    @{n='wrong config address';f={param($x)$x.Replace('finish=82256BE0','finish=82256BE4')}},
    @{n='missing ready';f={param($x)$x.Replace('MCLA_RACE_SYSTEM_READY v=1 phases=start,rewards external_close_required=1','')}},
    @{n='missing finish';f={param($x)$x.Replace('MCLA_RACE_SYSTEM_FINISH v=1 record=1 category=0 finished=1 arrested=0 winning_time=4294967295 desc_known=0 race_type=4294967295 race_subtype=4294967295 cop_zones=4294967295 checkpoint_max=0','')}},
    @{n='zero finish calls';f={param($x)$x.Replace('finish_calls=1','finish_calls=0')}},
    @{n='unfinished';f={param($x)$x.Replace('finished=1','finished=0')}},
    @{n='summary fail';f={param($x)$x.Replace('MCLA_RACE_SYSTEM_SUMMARY v=1 status=PASS','MCLA_RACE_SYSTEM_SUMMARY v=1 status=FAIL')}},
    @{n='dropped record';f={param($x)$x.Replace('dropped_records=0','dropped_records=1')}},
    @{n='detail mismatch';f={param($x)$x.Replace('detail_records=1','detail_records=2')}},
    @{n='tuple mismatch';f={param($x)$x.Replace('arrested_finishes=0 category=0 race_type=4294967295','arrested_finishes=0 category=9 race_type=4294967295')}},
    @{n='reversed frames';f={param($x)$x.Replace('phase=start width=1280 height=720 present_seq=100','phase=rewards width=1280 height=720 present_seq=100')}},
    @{n='duplicate frame';f={param($x)$x+"`nMCLA_RACE_SYSTEM_FRAME v=1 phase=rewards width=1280 height=720 present_seq=300 status=PASS"}},
    @{n='missing close';f={param($x)$x.Replace('Window closing, shutting down...','')}},
    @{n='missing execution complete';f={param($x)$x.Replace('Execution complete','')}},
    @{n='wrong signin';f={param($x)$x.Replace('state=2 mode=online-compatible','state=1 mode=local')}},
    @{n='unregistered';f={param($x)$x+"`nunregistered guest function"}},
    @{n='unknown schema';f={param($x)$x.Replace('MCLA_RACE_SYSTEM_SUMMARY v=1','MCLA_RACE_SYSTEM_SUMMARY v=2')}},
    @{n='unknown marker family';f={param($x)$x+"`nMCLA_RACE_SYSTEM_SECRET v=1 value=1"}},
    @{n='detail record gap';f={param($x)$x.Replace('MCLA_RACE_SYSTEM_FINISH v=1 record=1','MCLA_RACE_SYSTEM_FINISH v=1 record=2')}}
  );foreach($case in $cases){[IO.File]::WriteAllText($log,(&$case.f $original),$utf8);Fail {&$verify -RunPath $root -Fixture} $case.n;[IO.File]::WriteAllText($log,$original,$utf8);$negative++}
  $reward=Join-Path $root 'runs/01/user/mcla-race-system-rewards.bmp';$rewardBytes=[IO.File]::ReadAllBytes($reward);$rewardBytes[0]=0;[IO.File]::WriteAllBytes($reward,$rewardBytes);Fail {&$verify -RunPath $root -Fixture} 'invalid reward bmp';$negative++;Write-Bmp $reward 2
  Copy-Item (Join-Path $root 'runs/01/user/mcla-race-system-start.bmp') $reward -Force;Fail {&$verify -RunPath $root -Fixture} 'identical captures';$negative++;Write-Bmp $reward 2
  [IO.File]::WriteAllText((Join-Path $root 'runs/01/user/unconsumed.request'),'1',$utf8);Fail {&$verify -RunPath $root -Fixture} 'unconsumed request';$negative++;Remove-Item (Join-Path $root 'runs/01/user/unconsumed.request')
  $observationPath=Join-Path $root 'operator-observation.json';$observationOriginal=[IO.File]::ReadAllText($observationPath);[IO.File]::WriteAllText($observationPath,$observationOriginal.Replace('"machine_verified": false','"machine_verified": true'),$utf8);Fail {&$verify -RunPath $root -Fixture} 'machine-verified external observation';$negative++;[IO.File]::WriteAllText($observationPath,$observationOriginal,$utf8)
  $artifactsPath=Join-Path $root 'physical-artifacts.json';$artifactsOriginal=[IO.File]::ReadAllText($artifactsPath);[IO.File]::WriteAllText($artifactsPath,$artifactsOriginal.Replace(('D'*64),'NOT-A-HASH'),$utf8);Fail {&$verify -RunPath $root -Fixture} 'invalid physical artifact hash';$negative++;[IO.File]::WriteAllText($artifactsPath,$artifactsOriginal,$utf8)
  [IO.Directory]::CreateDirectory((Join-Path $root 'runs/03'))|Out-Null;Fail {&$verify -RunPath $root -Fixture} 'extra run cycle';$negative++;Remove-Item (Join-Path $root 'runs/03') -Recurse -Force
  $parseErrors=0;foreach($script in @($run,$verify,$PSCommandPath)){$tokens=$null;$errors=$null;[Management.Automation.Language.Parser]::ParseFile($script,[ref]$tokens,[ref]$errors)|Out-Null;$parseErrors+=@($errors).Count};if($parseErrors){throw 'PowerShell parser errors found.'}
  $texts=[IO.File]::ReadAllText($run)+[IO.File]::ReadAllText($verify)+[IO.File]::ReadAllText($app);$needles=@('mcla_race_system_probe','kRaceDescriptionTypeAddress = 0x822C9230','kRaceDescriptionSubtypeAddress = 0x822C9270','kRaceDescriptionCopZonesAddress = 0x822C92B0','kCheckpointListCountAddress = 0x82267528','kCheckpointHitAddress = 0x82263930','kRaceFinishAddress = 0x82256BE0','kRaceResultAddress = 0x821FBB40','MCLA_RACE_SYSTEM_CONFIG v=1','MCLA_RACE_SYSTEM_DESC v=1','MCLA_RACE_SYSTEM_CHECKPOINTS v=1','MCLA_RACE_SYSTEM_FINISH v=1','MCLA_RACE_SYSTEM_SUMMARY v=1','kRaceSystemDetailCapacity = 32','race_system_audit_enabled = false','RACE SYSTEM START IAN','RACE SYSTEM START MARTIN','POLICE SEEN','POLICE NOT SEEN','representative-race-system-matrix-pass','299392E59CB38AFB44256E773884856FF0C869A96D2045086A65396C1ED4EFCA','D993E2612D1AC769D88264C83FD9C9186BC761E2067317F5A7EB66038C250E58','D890A903775A3B53262B9957E7B7DC1D4B76B49B8735360BECD8233842446298','no claim of every race type');foreach($needle in $needles){if(-not$texts.Contains($needle)){throw "Source contract missing '$needle'."}}
  [pscustomobject][ordered]@{PositiveFixtures=3;FailClosedNegatives=$negative;SourceChecks=$needles.Count;ParserErrors=0}
}finally{if(Test-Path $root){Remove-Item $root -Recurse -Force};if(Test-Path $calibrationRoot){Remove-Item $calibrationRoot -Recurse -Force}}
