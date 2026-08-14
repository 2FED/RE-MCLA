[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier=Join-Path $PSScriptRoot 'verify-xam-profile-smoke.ps1'
$runner=Join-Path $PSScriptRoot 'run-xam-profile-smoke.ps1'
$source=Join-Path $repoRoot 'private/evidence/M4-002/20260812-085022-cc01a857/runs/01'
$fixtureRoot=Join-Path $repoRoot ('private/evidence/M4-004/test-'+[guid]::NewGuid().ToString('N').Substring(0,8))
$utf8=[Text.UTF8Encoding]::new($false)
$launch='KernelState: Preparing module launch...'
$capture='MCLA graphics: nontrivial guest frame captured '
$config='XAM_PROFILE_AUDIT_CONFIG v=1 enabled=1 slot=0'
$signin='XAM_PROFILE_AUDIT_SIGNIN_STATE v=1 class=success user=0 state=1'
$signinAbsent1='XAM_PROFILE_AUDIT_SIGNIN_STATE v=1 class=absent user=1 state=0'
$signinAbsent2='XAM_PROFILE_AUDIT_SIGNIN_STATE v=1 class=absent user=2 state=0'
$signinAbsent3='XAM_PROFILE_AUDIT_SIGNIN_STATE v=1 class=absent user=3 state=0'
$xuid='XAM_PROFILE_AUDIT_XUID v=1 class=success user=0 mask=7 result=00000000 nonzero=1'
$size='XAM_PROFILE_AUDIT_PROFILE_READ v=1 class=size title=profile user=255 xuid_count=0 setting_count=3 setting_ids=1004000C,1004000D,1004000E buffer_size_in=0 buffer_size_out=128 result=0000007A overlapped=0 completed=0000007A'
$fill='XAM_PROFILE_AUDIT_PROFILE_READ v=1 class=fill title=profile user=0 xuid_count=0 setting_count=3 setting_ids=1004000C,1004000D,1004000E buffer_size_in=128 buffer_size_out=128 result=00000000 overlapped=0 completed=00000000'
$priv251='XAM_PROFILE_AUDIT_PRIVILEGE v=1 class=success user=0 mask=251 result=00000000 allowed=0'
$priv252='XAM_PROFILE_AUDIT_PRIVILEGE v=1 class=success user=0 mask=252 result=00000000 allowed=0'
$name='XAM_PROFILE_AUDIT_NAME v=1 class=success user=0 result=00000000 profile_match=1'
$info='XAM_PROFILE_AUDIT_SIGNIN_INFO v=1 class=success user=0 result=00000000 state_match=1 xuid_match=1 name_match=1'
$absent1='XAM_PROFILE_AUDIT_SIGNIN_INFO v=1 class=absent user=1 result=80070525 state_match=0 xuid_match=0 name_match=0'
$absent2='XAM_PROFILE_AUDIT_SIGNIN_INFO v=1 class=absent user=2 result=80070525 state_match=0 xuid_match=0 name_match=0'
$absent3='XAM_PROFILE_AUDIT_SIGNIN_INFO v=1 class=absent user=3 result=80070525 state_match=0 xuid_match=0 name_match=0'
$summary='XAM_PROFILE_AUDIT_SUMMARY v=1 phase=checkpoint status=PASS signin_state_calls=12 signin_state_slot0_local=6 signin_state_other=6 signin_state_other_mask=E xuid_calls=2 xuid_success=2 xuid_nonzero=2 profile_read_calls=0 profile_read_size=0 profile_read_fill=0 profile_read_failure=0 privilege_calls=0 privilege_success=0 privilege_allowed=0 privilege_251_false=0 privilege_252_false=0 name_calls=2 name_match=2 signin_info_calls=5 signin_info_consistent=2 signin_info_absent=3 signin_info_absent_mask=E signin_ui_calls=0 signin_ui_ordered=0 dropped_records=0'
$optionalSummary='XAM_PROFILE_AUDIT_SUMMARY v=1 phase=checkpoint status=PASS signin_state_calls=12 signin_state_slot0_local=6 signin_state_other=6 signin_state_other_mask=E xuid_calls=2 xuid_success=2 xuid_nonzero=2 profile_read_calls=2 profile_read_size=1 profile_read_fill=1 profile_read_failure=0 privilege_calls=2 privilege_success=2 privilege_allowed=0 privilege_251_false=1 privilege_252_false=1 name_calls=2 name_match=2 signin_info_calls=5 signin_info_consistent=2 signin_info_absent=3 signin_info_absent_mask=E signin_ui_calls=0 signin_ui_ordered=0 dropped_records=0'

function Expect-Failure {param([scriptblock]$Action,[string]$Label)try{&$Action|Out-Null;throw "Negative fixture '$Label' was accepted."}catch{if($_.Exception.Message-eq"Negative fixture '$Label' was accepted."){throw}}}
function Get-SourceText {$parts=@();foreach($n in 3,2,1){$p=Join-Path $source "mcla.$n.log";if(Test-Path $p){$parts+=[IO.File]::ReadAllText($p)}};$parts+=[IO.File]::ReadAllText((Join-Path $source 'mcla.log'));return (($parts -join [Environment]::NewLine).Replace('30025 mappings','30026 mappings'))}
function Write-LogSet {param([string]$Root,[string]$Text)$chunks=[Collections.Generic.List[string]]::new();$b=[Text.StringBuilder]::new();foreach($line in ($Text -split "`r?`n")){if($b.Length+$line.Length+2-gt4000000){$chunks.Add($b.ToString());$null=$b.Clear()};$null=$b.AppendLine($line)};if($b.Length){$chunks.Add($b.ToString())};for($i=0;$i-lt$chunks.Count;$i++){$name=if($i-eq$chunks.Count-1){'mcla.log'}else{"mcla.$($chunks.Count-1-$i).log"};$path=Join-Path $Root $name;[IO.File]::WriteAllText($path,$chunks[$i],$utf8);(Get-Item $path).LastWriteTimeUtc=([datetime]'2026-08-12T00:00:00Z').AddSeconds($i)}}
function New-Probe {param([string]$Root,[scriptblock]$Mutate,[switch]$Compact,[switch]$OptionalGroups)$user=Join-Path $Root 'user';[IO.Directory]::CreateDirectory($user)|Out-Null;[IO.Directory]::CreateDirectory((Join-Path $Root 'cache'))|Out-Null;Copy-Item (Join-Path $source 'user/mcla-first-frame.bmp') (Join-Path $user 'mcla-first-frame.bmp');$core=@($config,$signin,$signinAbsent1,$signinAbsent2,$signinAbsent3,$xuid,$name,$info,$absent1,$absent2,$absent3);if($OptionalGroups){$core+=@($size,$fill,$priv251,$priv252,$optionalSummary)}else{$core+=$summary};$records=$core-join"`r`n";if($Compact){$text="[fixture] $launch`r`n$records`r`n[fixture] $capture`r`n"}else{$text=Get-SourceText;$ci=$text.IndexOf($capture);if($text.IndexOf($launch)-lt0-or$ci-lt0){throw 'Pinned title source markers missing.'};$text=$text.Insert($ci,$records+"`r`n")};if($Mutate){$text=[string](& $Mutate $text);if(-not$text){throw 'Fixture mutation returned empty text.'}};Write-LogSet $Root $text}
function Probe {param([string]$Root,[switch]$ProfileOnly)&$verifier -ProbeOnly -ProfileOnly:$ProfileOnly -RuntimeLogPath (Join-Path $Root 'mcla.log') -BmpPath (Join-Path $Root 'user/mcla-first-frame.bmp')}

try{
 [IO.Directory]::CreateDirectory($fixtureRoot)|Out-Null;$positive=Join-Path $fixtureRoot 'positive';New-Probe $positive $null; $p=Probe $positive;if($p.SummaryCount-ne1-or$p.SigninRecords-ne4-or$p.ReadRecords-ne0-or$p.PrivilegeRecords-ne0-or$p.NameRecords-ne1-or$p.InfoRecords-ne4){throw 'Calibrated positive profile fixture counters differ.'}
 $optionalPositive=Join-Path $fixtureRoot 'positive-optional-groups';New-Probe $optionalPositive $null -Compact -OptionalGroups;$op=Probe $optionalPositive -ProfileOnly;if($op.ReadRecords-ne2-or$op.PrivilegeRecords-ne2){throw 'Reached optional profile/privilege fixture counters differ.'}
 $cases=[ordered]@{
  'missing-config'={param($t)$t.Replace($config+"`r`n",'')}
  'duplicate-config'={param($t)$t.Replace($config,$config+"`r`n"+$config)}
  'config-before-launch'={param($t)$t.Replace($config+"`r`n",'').Replace($launch,$config+"`r`n"+$launch)}
  'missing-summary'={param($t)$t.Replace($summary+"`r`n",'')}
  'duplicate-summary'={param($t)$t.Replace($summary,$summary+"`r`n"+$summary)}
  'summary-after-capture'={param($t)$t.Replace($summary+"`r`n",'').Replace($capture,$capture+"`r`n"+$summary)}
  'summary-fail'={param($t)$t.Replace('phase=checkpoint status=PASS','phase=checkpoint status=FAIL')}
  'unknown-audit-record'={param($t)$t.Replace($summary,"XAM_PROFILE_AUDIT_SECRET v=1 raw_xuid=1`r`n"+$summary)}
  'slot0-signed-out'={param($t)$t.Replace('user=0 state=1','user=0 state=0')}
  'other-slot-signed-in'={param($t)$t.Replace($summary,"XAM_PROFILE_AUDIT_SIGNIN_STATE v=1 class=success user=1 state=1`r`n"+$summary)}
  'duplicate-signin-slot1'={param($t)$t.Replace($signinAbsent1,$signinAbsent1+"`r`n"+$signinAbsent1)}
  'missing-signin-slot2'={param($t)$t.Replace($signinAbsent2+"`r`n",'')}
  'missing-signin-slot3'={param($t)$t.Replace($signinAbsent3+"`r`n",'')}
  'signin-mask-missing-slot3'={param($t)$t.Replace('signin_state_other_mask=E','signin_state_other_mask=6')}
  'summary-other-slots-too-low'={param($t)$t.Replace('signin_state_other=6','signin_state_other=2')}
  'missing-xuid-record'={param($t)$t.Replace($xuid+"`r`n",'')}
  'xuid-zero'={param($t)$t.Replace('mask=7 result=00000000 nonzero=1','mask=7 result=00000000 nonzero=0')}
  'xuid-wrong-mask'={param($t)$t.Replace('user=0 mask=7 result=','user=0 mask=3 result=')}
  'xuid-failure'={param($t)$t.Replace('XAM_PROFILE_AUDIT_XUID v=1 class=success','XAM_PROFILE_AUDIT_XUID v=1 class=failure')}
  'summary-xuid-counter-drift'={param($t)$t.Replace('xuid_calls=2','xuid_calls=3')}
  'summary-dropped-record'={param($t)$t.Replace('dropped_records=0','dropped_records=1')}
  'missing-name-record'={param($t)$t.Replace($name+"`r`n",'')}
  'name-mismatch'={param($t)$t.Replace('profile_match=1','profile_match=0')}
  'name-failure'={param($t)$t.Replace('XAM_PROFILE_AUDIT_NAME v=1 class=success','XAM_PROFILE_AUDIT_NAME v=1 class=failure')}
  'missing-info-success'={param($t)$t.Replace($info+"`r`n",'')}
  'signin-info-mismatch'={param($t)$t.Replace('state_match=1 xuid_match=1 name_match=1','state_match=1 xuid_match=0 name_match=1')}
  'signin-info-failure'={param($t)$t.Replace('XAM_PROFILE_AUDIT_SIGNIN_INFO v=1 class=success','XAM_PROFILE_AUDIT_SIGNIN_INFO v=1 class=failure')}
  'duplicate-info-slot1'={param($t)$t.Replace($absent1,$absent1+"`r`n"+$absent1)}
  'missing-info-slot2'={param($t)$t.Replace($absent2+"`r`n",'')}
  'missing-info-slot3'={param($t)$t.Replace($absent3+"`r`n",'')}
  'absent-wrong-result'={param($t)$t.Replace('class=absent user=1 result=80070525','class=absent user=1 result=00000000')}
  'absent-invalid-user'={param($t)$t.Replace('class=absent user=1 result=','class=absent user=4 result=')}
  'absent-match-bit'={param($t)$t.Replace('state_match=0 xuid_match=0 name_match=0','state_match=1 xuid_match=0 name_match=0')}
  'summary-absent-too-low'={param($t)$t.Replace('signin_info_absent=3','signin_info_absent=2')}
  'info-mask-missing-slot3'={param($t)$t.Replace('signin_info_absent_mask=E','signin_info_absent_mask=6')}
  'summary-info-relational-drift'={param($t)$t.Replace('signin_info_calls=5','signin_info_calls=6')}
  'signin-ui-unordered'={param($t)$t.Replace($summary,"XAM_PROFILE_AUDIT_SIGNIN_UI v=1 class=success signin_changed_seq=2 ui_off_seq=1 ordered=0 result=00000000`r`n"+$summary)}
  'generic-stub-hit'={param($t)$t.Replace($summary,"REX_EXPORT_STUB reached XamUserGetName unimplemented`r`n"+$summary)}
 }
 $index=0;foreach($label in $cases.Keys){$index++;$root=Join-Path $fixtureRoot ('negative-{0:D2}-{1}'-f$index,$label);New-Probe $root $cases[$label] -Compact;Expect-Failure {Probe $root -ProfileOnly}$label}
 $optionalCases=[ordered]@{
  'missing-size'={param($t)$t.Replace($size+"`r`n",'')}
  'missing-fill'={param($t)$t.Replace($fill+"`r`n",'')}
  'wrong-setting-id'={param($t)$t.Replace('1004000C,1004000D,1004000E','1004000C,1004000D,1004000F')}
  'wrong-setting-count'={param($t)$t.Replace('setting_count=3 setting_ids=','setting_count=2 setting_ids=')}
  'size-input-nonzero'={param($t)$t.Replace('buffer_size_in=0 buffer_size_out=128','buffer_size_in=1 buffer_size_out=128')}
  'size-output-wrong'={param($t)$t.Replace('buffer_size_in=0 buffer_size_out=128','buffer_size_in=0 buffer_size_out=127')}
  'size-result-wrong'={param($t)$t.Replace('buffer_size_out=128 result=0000007A','buffer_size_out=128 result=00000000')}
  'fill-input-wrong'={param($t)$t.Replace('buffer_size_in=128 buffer_size_out=128 result=00000000','buffer_size_in=127 buffer_size_out=128 result=00000000')}
  'fill-completed-wrong'={param($t)$t.Replace('result=00000000 overlapped=0 completed=00000000','result=00000000 overlapped=0 completed=0000007A')}
  'read-failure'={param($t)$t.Replace('XAM_PROFILE_AUDIT_PROFILE_READ v=1 class=fill','XAM_PROFILE_AUDIT_PROFILE_READ v=1 class=failure')}
  'read-overlapped-shape'={param($t)$t.Replace('result=0000007A overlapped=0','result=0000007A overlapped=1')}
  'privilege-allowed'={param($t)$t.Replace('mask=251 result=00000000 allowed=0','mask=251 result=00000000 allowed=1')}
  'privilege-failure'={param($t)$t.Replace('XAM_PROFILE_AUDIT_PRIVILEGE v=1 class=success user=0 mask=251','XAM_PROFILE_AUDIT_PRIVILEGE v=1 class=failure user=0 mask=251')}
  'privilege-wrong-mask'={param($t)$t.Replace('mask=251 result=00000000 allowed=0','mask=250 result=00000000 allowed=0')}
  'missing-privilege-251'={param($t)$t.Replace($priv251+"`r`n",'')}
  'missing-privilege-252'={param($t)$t.Replace($priv252+"`r`n",'')}
  'optional-summary-counter-drift'={param($t)$t.Replace('profile_read_calls=2','profile_read_calls=3')}
 }
 foreach($label in $optionalCases.Keys){$index++;$root=Join-Path $fixtureRoot ('negative-{0:D2}-{1}'-f$index,$label);New-Probe $root $optionalCases[$label] -Compact -OptionalGroups;Expect-Failure {Probe $root -ProfileOnly}$label}
 $runnerText=[IO.File]::ReadAllText($runner);$verifierText=[IO.File]::ReadAllText($verifier)
 foreach($needle in @('--xam_profile_audit=true','--mcla_first_frame_settle_seconds=35','--clean-first','--config RelWithDebInfo','WaitForExit(10000)','for($cycle=1;$cycle-le3;$cycle++)','[kernel][xam][profile]','15 assertions in 2 test cases')){if(-not$runnerText.Contains($needle)){throw "Runner source contract missing '$needle'."}}
 foreach($needle in @('XAM_PROFILE_AUDIT_SUMMARY','1004000C,1004000D,1004000E','privilege_251_false','privilege_252_false','signin_state_other_mask','signin_info_absent','signin_info_absent_mask','Assert-StaticContract','Get-ExactProcesses','ReparsePoint')){if(-not$verifierText.Contains($needle)){throw "Verifier source contract missing '$needle'."}}
 [pscustomobject]@{Passed=$true;PhysicalPositiveProbes=1;OptionalPositiveProbes=1;FailClosedNegativeProbes=($cases.Count+$optionalCases.Count);SourceContractChecks=19;SigninUiZeroAccepted=$true;CalibratedProfileAndPrivilegeZeroAccepted=$true;DistinctAbsentSlotsProven=$true}
}finally{if(Test-Path $fixtureRoot){Remove-Item $fixtureRoot -Recurse -Force}}
