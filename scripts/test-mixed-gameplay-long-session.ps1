[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify=Join-Path $PSScriptRoot 'verify-mixed-gameplay-long-session.ps1'
$runner=Join-Path $PSScriptRoot 'run-mixed-gameplay-long-session.ps1'
$utf8=[Text.UTF8Encoding]::new($false)
$root=Join-Path $repo ('private/evidence/M6-014/test-final-hour-'+[guid]::NewGuid().ToString('N').Substring(0,8))
[IO.Directory]::CreateDirectory($root)|Out-Null

function WriteJson([string]$Path,$Value){[IO.File]::WriteAllText($Path,((ConvertTo-Json $Value -Depth 15)+[Environment]::NewLine),$utf8)}
function BaseResult{
  $samples=@()
  for($i=0;$i-lt13;$i++){$samples+=[ordered]@{checkpoint=$i;elapsed_seconds=if($i){$i*300}else{0};private_bytes=1000000+($i*100);working_set_bytes=2000000+($i*100);handle_count=100+$i;thread_count=20;io_read_bytes=3000000+($i*1000)}}
  $captures=@()
  $attempts=@()
  for($i=0;$i-lt5;$i++){$capture=[ordered]@{name=('checkpoint-{0:D2}.bmp'-f$i);elapsed_seconds=$i*900;sha256=(([char](65+$i)).ToString()*64);bytes=1024;color_bins=100};$captures+=$capture;$attempts+=[ordered]@{status='captured';checkpoint=$i;scheduled_elapsed_seconds=$i*900;attempted_elapsed_seconds=$i*900;name=$capture.name;elapsed_seconds=$capture.elapsed_seconds;sha256=$capture.sha256;bytes=$capture.bytes;color_bins=$capture.color_bins}}
  [ordered]@{
    schema='mcla-one-hour-mixed-gameplay-long-session-v2';task='M6-014';decision='one-hour-mixed-gameplay-long-session-pass';run_id='20260903-120000-1234abcd';target_duration_seconds=3600;actual_duration_seconds=3600
    started_utc='2026-09-03T09:00:00.0000000Z';completed_utc='2026-09-03T10:00:05.0000000Z';attested_utc='2026-09-03T10:00:10.0000000Z'
    current_suite=[ordered]@{path='private/evidence/M6-014/20260901-153415-d747cf2d/suite.json';sha256='EB553F0B34F00837A7A0C2FA3FDDD4FBEA9444FE4268EAC266D889A80DCA37C6';sdk_version='0.10.0.1';sdk_commit='7dd5cb33002a443b097c0f65d5566c0a0f2db838'}
    historical_frontend=[ordered]@{suite_path='private/evidence/M6-014/20260831-133236-2cecb67b/suite.json';suite_sha256='009DB466CA39195C02B137C45F4A11EBA87080D1C60B15D54887C5CBD85E0F3C';stage_path='private/evidence/M6-014/20260831-133236-2cecb67b/scenarios/frontend/stage.json';stage_sha256='4E97A021296A0521F959CB9A37AE7BA1B0D3A93B6D311651E10DFCF75E39AC9C';tree_sha256='C44F1C9D0E47834BA745CE267500E826E490706B1BDFE4A081AE65FE88D6665D';duration_seconds=7202;executable_sha256='9202CDC09DA1402460312E0204AB0CF5B6D05D9C07B205CA136414CD1C655FD4'}
    delivery_seed=[ordered]@{result_path='private/evidence/M6-014/delivery-regressions/20260901-203448-d31ab681/result.json';result_sha256='E7200300CA38718DD6978C26E8F4CE5CF4EF1279441B5F032CE2075F387FF0F1';latest_sha256='AB0D674BC00C83734021A609F16539BCB7F9AF18F739EDA8A132AE7B3E876DCF';snapshot_directory='20260901-175116Z-FE788E35FDF69B59-2FAEFBC7FF8CDEAD';snapshot_tree_sha256='C9E80A1941C0A0216DD01F9C0978F17189FDD4A2DE53CB94A6738ABB15AACE8C';save_before_sha256='FE788E35FDF69B59F10E404886A043109349B1CD85B71F9FDFE15AA8E9AB02A3';header_before_sha256='2FAEFBC7FF8CDEADBCE025D9BE914436ED05E27D041F72C8C88798612CFB0D60';save_after_sha256=('1'*64);header_after_sha256=('2'*64);working_copy_tree_sha256=('A'*64)}
    game=[ordered]@{file_count=15;payload_bytes=6569586392L;source_iso_sha256='AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB'}
    release_artifacts=@([ordered]@{name='mcla.exe';sha256=('3'*64)},[ordered]@{name='rexruntime.dll';sha256=('4'*64)},[ordered]@{name='TracyClient.dll';sha256=('5'*64)},[ordered]@{name='rexgpu-xenos.dll';sha256=('6'*64)})
    resource_samples=$samples
    resource_bounds=[ordered]@{private_growth_bytes=1200;working_growth_bytes=1200;handle_growth=12;thread_growth=0;io_read_growth_bytes=12000;private_peak_growth_bytes=1200;working_peak_growth_bytes=1200}
    capture_attempts=$attempts
    captures=$captures
    runtime_logs=@([ordered]@{name='mcla.log';sha256=('7'*64);bytes=1024})
    runtime_fatal_markers=0
    host_display_audit=[ordered]@{schema='mcla-host-display-audit-v1';sha256=('8'*64);since_utc='2026-09-03T09:00:00.0000000Z';checked_utc='2026-09-03T10:00:02.0000000Z';audit_available=$true;nvidia_driver_errors=0;sunshine_application_errors=0}
    controlled_external_close=$true;exit_code=0;force_cleanup=$false
    save_archive=[ordered]@{sha256=('9'*64);files=4;bytes=4096}
    scope=[ordered]@{historical_frontend_two_hours=$true;current_artifact_interactive_mixed_gameplay_one_hour=$true;normal_moving_gameplay_owner_attested=$true;garage_enter_exit_owner_attested=$true;pause_resume_owner_attested=$true;alt_tab_return_owner_attested=$true;same_release_artifacts=$false;single_continuous_three_hour_run_claimed=$false;current_five_stage_soak_complete=$false;five_two_hour_suite_claimed=$false;two_hour_current_artifact_mixed_gameplay_claimed=$false;full_campaign_claimed=$false;rendering_parity_claimed=$false;music_continuity_claimed=$false;wheel_centering_stability_claimed=$false}
  }
}
function Clone($Value){(($Value|ConvertTo-Json -Depth 15)|ConvertFrom-Json)}
function ExpectFail([string]$Name,[scriptblock]$Mutate){$value=Clone (BaseResult);&$Mutate $value;$path=Join-Path $root "$Name.json";WriteJson $path $value;$failed=$false;try{&$verify -FixturePath $path -Fixture|Out-Null}catch{$failed=$true};if(-not$failed){throw "Negative fixture passed: $Name"};$script:negative++}

try{
  $positive=Join-Path $root 'positive.json';WriteJson $positive (BaseResult);$probe=&$verify -FixturePath $positive -Fixture
  if($probe.Decision-cne'one-hour-mixed-gameplay-long-session-pass'-or$probe.DurationSeconds-ne3600-or$probe.Samples-ne13-or$probe.Captures-ne5){throw 'Positive fixture failed.'}
  $skipped=Clone (BaseResult);$skipped.capture_attempts[1]=[pscustomobject][ordered]@{status='skipped';checkpoint=1;scheduled_elapsed_seconds=900;attempted_elapsed_seconds=900;reason='mcla-window-not-foreground'};$skipped.captures=@($skipped.captures|Where-Object name -CNE 'checkpoint-01.bmp');$skippedPath=Join-Path $root 'positive-skipped-capture.json';WriteJson $skippedPath $skipped;$skippedProbe=&$verify -FixturePath $skippedPath -Fixture;if($skippedProbe.Captures-ne4){throw 'Recoverable skipped-capture fixture failed.'}
  $negative=0
  ExpectFail bad-schema {param($r)$r.schema='bad'}
  ExpectFail short-duration {param($r)$r.actual_duration_seconds=3599}
  ExpectFail long-duration {param($r)$r.actual_duration_seconds=3901}
  ExpectFail string-duration {param($r)$r.actual_duration_seconds='3600'}
  ExpectFail bad-chronology {param($r)$r.completed_utc=$r.started_utc}
  ExpectFail bad-current-suite {param($r)$r.current_suite.sha256=('0'*64)}
  ExpectFail bad-historical-stage {param($r)$r.historical_frontend.stage_sha256=('0'*64)}
  ExpectFail same-artifact-history {param($r)$r.release_artifacts[0].sha256=$r.historical_frontend.executable_sha256}
  ExpectFail bad-delivery-seed {param($r)$r.delivery_seed.save_before_sha256=('0'*64)}
  ExpectFail bad-working-seed-copy {param($r)$r.delivery_seed.working_copy_tree_sha256='bad'}
  ExpectFail missing-sample {param($r)$r.resource_samples=@($r.resource_samples|Select-Object -First 12)}
  ExpectFail late-sample {param($r)$r.resource_samples[1].elapsed_seconds=331}
  ExpectFail string-sample {param($r)$r.resource_samples[1].elapsed_seconds='300'}
  ExpectFail duration-sample-mismatch {param($r)$r.resource_samples[12].elapsed_seconds=3601}
  ExpectFail forged-bounds {param($r)$r.resource_bounds.private_growth_bytes=1199}
  ExpectFail memory-limit {param($r)$r.resource_samples[1].private_bytes=1074741825L}
  ExpectFail handle-limit {param($r)$r.resource_samples[12].handle_count=229}
  ExpectFail missing-capture-attempt {param($r)$r.capture_attempts=@($r.capture_attempts|Select-Object -First 4)}
  ExpectFail missing-baseline-capture {param($r)$r.capture_attempts[0]=[pscustomobject][ordered]@{status='skipped';checkpoint=0;scheduled_elapsed_seconds=0;attempted_elapsed_seconds=0;reason='mcla-window-not-foreground'};$r.captures=@($r.captures|Select-Object -Skip 1)}
  ExpectFail unbound-capture {param($r)$r.captures=@($r.captures|Select-Object -First 4)}
  ExpectFail black-capture {param($r)$r.captures[1].color_bins=1}
  ExpectFail late-capture {param($r)$r.captures[1].elapsed_seconds=931}
  ExpectFail bad-skip-reason {param($r)$r.capture_attempts[1]=[pscustomobject][ordered]@{status='skipped';checkpoint=1;scheduled_elapsed_seconds=900;attempted_elapsed_seconds=900;reason='bad'};$r.captures=@($r.captures|Where-Object name -CNE 'checkpoint-01.bmp')}
  ExpectFail mismatched-attempt-time {param($r)$r.capture_attempts[1].elapsed_seconds=901}
  ExpectFail string-capture-elapsed {param($r)$r.captures[1].elapsed_seconds='900'}
  ExpectFail string-capture-bytes {param($r)$r.captures[1].bytes='1024'}
  ExpectFail string-capture-bins {param($r)$r.captures[1].color_bins='100'}
  ExpectFail empty-logs {param($r)$r.runtime_logs=@()}
  ExpectFail host-driver-failure {param($r)$r.host_display_audit.nvidia_driver_errors=1}
  ExpectFail host-schema {param($r)$r.host_display_audit.schema='bad'}
  ExpectFail host-chronology {param($r)$r.host_display_audit.since_utc='2026-09-03T09:00:01.0000000Z'}
  ExpectFail runtime-fatal {param($r)$r.runtime_fatal_markers=1}
  ExpectFail uncontrolled-exit {param($r)$r.controlled_external_close=$false}
  ExpectFail string-boolean {param($r)$r.controlled_external_close='true'}
  ExpectFail forced-cleanup {param($r)$r.force_cleanup=$true}
  ExpectFail empty-save-archive {param($r)$r.save_archive.files=0}
  ExpectFail missing-owner-attestation {param($r)$r.scope.garage_enter_exit_owner_attested=$false}
  ExpectFail string-scope {param($r)$r.scope.pause_resume_owner_attested='true'}
  ExpectFail same-artifact-overclaim {param($r)$r.scope.same_release_artifacts=$true}
  ExpectFail two-hour-current-overclaim {param($r)$r.scope.two_hour_current_artifact_mixed_gameplay_claimed=$true}
  ExpectFail wheel-overclaim {param($r)$r.scope.wheel_centering_stability_claimed=$true}

  $runnerText=[IO.File]::ReadAllText($runner);$verifierText=[IO.File]::ReadAllText($verify)
  $runnerNeedles=@('[ValidateRange(3600,3600)][int]$DurationSeconds=3600','checkpoint-le12','$checkpoint*300','$checkpoint%3-eq0','elapsed_seconds=$Elapsed','MIXED GAMEPLAY READY','MIXED GAMEPLAY STABLE','GetProcessIoCounters','CopyFromScreen','mcla-window-not-foreground','session continues','capture-attempts.json','Recovery save snapshot confirmed','No recovery snapshot was confirmed','HostDisplayAudit','watch-soak-save.ps1','verify-delivery-transition-regression.ps1','Copied gameplay seed failed physical identity verification.','working_copy_tree_sha256','Final save archive does not match the complete physical profile snapshot.','-WindowStyle Hidden','--xam_user_signin_state=1','--fullscreen=false','Assertion failed','GUEST_CRASH_REPORT','DXGI_ERROR_DEVICE_(?:REMOVED|HUNG|RESET)','normal moving gameplay plus one garage enter/exit, pause/resume, and Alt-Tab return','same_release_artifacts=$false','current_five_stage_soak_complete=$false')
  foreach($needle in $runnerNeedles){if(-not$runnerText.Contains($needle)){throw "Runner source contract missing: $needle"}}
  if($runnerText.Contains('Read-Host')-or$runnerText.Contains('Get-FileHash')){throw 'Runner contains a blocking prompt or unavailable hash cmdlet.'}
  $verifierNeedles=@('Exactly thirteen resource samples are required.','Exactly five capture attempts are required.','Captured frame count is outside the accepted range.','Captured frames do not match successful attempts.','Actual duration is not bound to the final sample.','Current physical artifact, game, or suite identity drifted.','Historical and current executable identities must remain distinct.','verify-delivery-transition-regression.ps1','Final archive physical snapshot drifted.','contains a reparse point.','Canonical MCLA process remains.')
  foreach($needle in $verifierNeedles){if(-not$verifierText.Contains($needle)){throw "Verifier source contract missing: $needle"}}
  if($verifierText.Contains('Get-FileHash')){throw 'Verifier uses an unavailable hash cmdlet.'}
  [pscustomobject][ordered]@{PositiveFixtures=2;FailClosedNegatives=$negative;SourceContractChecks=$runnerNeedles.Count+$verifierNeedles.Count+3;OneHourGateVerified=$true}
}finally{if(Test-Path $root){Remove-Item -LiteralPath $root -Recurse -Force}}
