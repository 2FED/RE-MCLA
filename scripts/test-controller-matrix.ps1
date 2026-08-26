[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier=Join-Path $PSScriptRoot 'verify-controller-matrix.ps1'
$runner=Join-Path $PSScriptRoot 'run-controller-matrix.ps1'
$sourceRoot=Join-Path $repoRoot ('private/evidence/'+('M4-'+'005')+'/20260812-140305-f0bd8e1b/runs/01')
$digitalRun='20260812-212030-5fc01c73'
$digitalRoot=Join-Path $repoRoot "private/evidence/M4-006/$digitalRun"
$analogFocusRun='20260813-124600-293c07b3'
$analogFocusRoot=Join-Path $repoRoot "private/evidence/M4-006/$analogFocusRun"
$recoveredHotplugRun='20260813-144406-2c1974da'
$recoveredHotplugRoot=Join-Path $repoRoot "private/evidence/M4-006/$recoveredHotplugRun"
$fixtureRoot=Join-Path $repoRoot ('private/evidence/M4-006/test-'+[guid]::NewGuid().ToString('N').Substring(0,8))
$utf8=[Text.UTF8Encoding]::new($false)

function Expect-Failure {param([scriptblock]$Action,[string]$Label)$failed=$false;try{&$Action|Out-Null}catch{$failed=$true};if(-not$failed){throw "Negative fixture unexpectedly passed: $Label"}}
function Probe {param([string]$Root,[switch]$AuditOnly)&$verifier -ProbeOnly -RuntimeLogPath (Join-Path $Root 'mcla.log') -BmpPath (Join-Path $Root 'mcla-first-frame.bmp') -AuditOnly:$AuditOnly}

$config='SDL_CONTROLLER_MATRIX_AUDIT_CONFIG v=1 enabled=1 expected_slot=0 button_mask=F3FF analog_mask=3FF'
$assigned='SDL_CONTROLLER_MATRIX_AUDIT_DEVICE v=1 event=assigned slot=0 active_mask=1'
$multi='SDL_CONTROLLER_MATRIX_AUDIT_MULTIPAD v=1 assigned_mask=1 active_mask=1 rejected=0'
$arm='SDL_CONTROLLER_MATRIX_AUDIT_ARM v=1 phase=title status=READY active_mask=1 neutral=1'
$rtlCompatibility='RtlLeaveCriticalSection: allowing one or more cross-thread leaves under the explicit compatibility option'
$capture='MCLA graphics: nontrivial guest frame captured fixture'
$resume='SDL_CONTROLLER_MATRIX_AUDIT_HOTPLUG_RESUME v=1 phase=title status=READY prior_digital=1 prior_rumble=1 prior_analog=1 prior_focus=1 button_sdl_mask=F3FF button_guest_mask=F3FF analog_sdl_mask=3FF analog_guest_mask=3FF rumble_pattern_mask=7 rumble_stop_mask=7 focus_lost=1 focus_neutral=1 focus_gained=1 source_seq=24'
$analogBits=@('001','002','004','008','010','020','040','080','100','200')
$summary='SDL_CONTROLLER_MATRIX_AUDIT_SUMMARY v=1 phase=title status=PASS button_sdl_mask=F3FF button_guest_mask=F3FF analog_sdl_mask=3FF analog_guest_mask=3FF focus_lost=1 focus_neutral=1 focus_gained=1 hotplug_removed=1 hotplug_disconnected=1 hotplug_reconnected=1 hotplug_success=1 assigned_mask=1 rumble_pattern_mask=7 rumble_stop_mask=7 digital_source=prior-evidence digital_current_records=0 analog_source=prior-evidence analog_current_records=0 focus_source=prior-evidence focus_current_records=0 rumble_source=prior-evidence rumble_current_records=0 unexpected=0 failures=0 dropped_records=0'

$pre=@($config,$assigned,$multi)
$post=[Collections.Generic.List[string]]::new()
$post.Add($resume)
foreach($line in @(
 'SDL_CONTROLLER_MATRIX_AUDIT_DEVICE v=1 event=removed slot=0 active_mask=0',
 'SDL_CONTROLLER_MATRIX_AUDIT_HOTPLUG v=1 event=removed slot=0 seq=3',
 'SDL_CONTROLLER_MATRIX_AUDIT_HOTPLUG v=1 event=disconnected slot=0 result=0000048F seq=3',
 'SDL_CONTROLLER_MATRIX_AUDIT_DEVICE v=1 event=reconnected slot=0 active_mask=1',
 $multi,
 'SDL_CONTROLLER_MATRIX_AUDIT_HOTPLUG v=1 event=reconnected slot=0 active_mask=1 seq=4',
 'SDL_CONTROLLER_MATRIX_AUDIT_HOTPLUG v=1 event=success slot=0 result=00000000 seq=4',
 $summary)){$post.Add($line)}

function New-CompactProbe {param([string]$Root,[scriptblock]$Mutate)
 [IO.Directory]::CreateDirectory($Root)|Out-Null
 [IO.File]::WriteAllBytes((Join-Path $Root 'mcla-first-frame.bmp'),[byte[]](0))
 $lines=@('KernelState: Preparing module launch...',$rtlCompatibility)+$pre+@($arm,$resume,$capture)+@($post|Select-Object -Skip 1)
 $text=($lines-join"`r`n")+"`r`n"
 if($Mutate){$changed=&$Mutate $text;if($changed-isnot[string]){throw 'Fixture mutation must return one string.'};$text=$changed}
 [IO.File]::WriteAllText((Join-Path $Root 'mcla.log'),$text,$utf8)
}
function New-PhysicalProbe {param([string]$Root)
 if(-not(Test-Path $sourceRoot)){throw 'Accepted prior physical controller fixture is missing.'}
 [IO.Directory]::CreateDirectory($Root)|Out-Null
 foreach($f in Get-ChildItem $sourceRoot -File -Filter 'mcla*.log'){$target=Join-Path $Root $f.Name;Copy-Item $f.FullName $target;$stamp=(Get-Item $target).LastWriteTimeUtc;$text=[IO.File]::ReadAllText($target).Replace('30025 mappings','30026 mappings');[IO.File]::WriteAllText($target,$text,$utf8);(Get-Item $target).LastWriteTimeUtc=$stamp}
 Copy-Item (Join-Path $sourceRoot 'user/mcla-first-frame.bmp') (Join-Path $Root 'mcla-first-frame.bmp')
 $matches=@();foreach($candidate in Get-ChildItem $Root -File -Filter 'mcla*.log'){$text=[IO.File]::ReadAllText($candidate.FullName);$marker=[regex]::Match($text,'(?m)^.*MCLA graphics: nontrivial guest frame captured .*$');if($marker.Success){$matches+=@($candidate,$text,$marker)}};if($matches.Count-ne3){throw 'Physical source requires exactly one capture marker.'}
 $injected=$rtlCompatibility+"`r`n"+($pre-join"`r`n")+"`r`n"+$arm+"`r`n"+$resume+"`r`n"+$matches[2].Value+"`r`n"+(@($post|Select-Object -Skip 1)-join"`r`n")
 $text=$matches[1].Remove($matches[2].Index,$matches[2].Length).Insert($matches[2].Index,$injected)
 $stamp=$matches[0].LastWriteTimeUtc;[IO.File]::WriteAllText($matches[0].FullName,$text,$utf8);(Get-Item $matches[0].FullName).LastWriteTimeUtc=$stamp
}

try{
 $digitalPositive=&$verifier -DigitalEvidenceOnly -DigitalEvidenceRun $digitalRun;if($digitalPositive.ButtonCount-ne56-or$digitalPositive.FatalCount-ne1-or$digitalPositive.ControlledExitPassed){throw 'Immutable digital positive classification differs.'}
 $digitalMutated=Join-Path $fixtureRoot 'negative-digital-mutated';Copy-Item -LiteralPath $digitalRoot -Destination $digitalMutated -Recurse;$mutatedLog=Join-Path $digitalMutated 'runs/01/mcla.log';$mutatedText=[IO.File]::ReadAllText($mutatedLog).Replace('guest address 0x82554080','guest address 0x82554081');[IO.File]::WriteAllText($mutatedLog,$mutatedText,$utf8);Expect-Failure {&$verifier -DigitalEvidenceOnly -DigitalEvidenceRun $digitalRun -DigitalEvidenceRoot $digitalMutated} 'mutated-prior-digital-evidence'
 $digitalDeleted=Join-Path $fixtureRoot 'negative-digital-deleted';Copy-Item -LiteralPath $digitalRoot -Destination $digitalDeleted -Recurse;Remove-Item -LiteralPath (Join-Path $digitalDeleted 'runs/01/user/mcla-first-frame.bmp') -Force;Expect-Failure {&$verifier -DigitalEvidenceOnly -DigitalEvidenceRun $digitalRun -DigitalEvidenceRoot $digitalDeleted} 'deleted-prior-digital-evidence'
 $analogFocusPositive=&$verifier -AnalogFocusEvidenceOnly -AnalogFocusEvidenceRun $analogFocusRun;if($analogFocusPositive.AnalogCount-ne40-or$analogFocusPositive.FocusCount-ne3-or$analogFocusPositive.ControlledExitPassed){throw 'Immutable analog/focus positive classification differs.'}
 $analogFocusMutated=Join-Path $fixtureRoot 'negative-analog-focus-mutated';Copy-Item -LiteralPath $analogFocusRoot -Destination $analogFocusMutated -Recurse;$afLog=Join-Path $analogFocusMutated 'runs/01/mcla.1.log';$afText=[IO.File]::ReadAllText($afLog).Replace('class=focus_order count=1','class=focus_order count=2');[IO.File]::WriteAllText($afLog,$afText,$utf8);Expect-Failure {&$verifier -AnalogFocusEvidenceOnly -AnalogFocusEvidenceRun $analogFocusRun -AnalogFocusEvidenceRoot $analogFocusMutated} 'mutated-prior-analog-focus-evidence'
 $analogFocusDeleted=Join-Path $fixtureRoot 'negative-analog-focus-deleted';Copy-Item -LiteralPath $analogFocusRoot -Destination $analogFocusDeleted -Recurse;Remove-Item -LiteralPath (Join-Path $analogFocusDeleted 'runs/01/user/mcla-first-frame.bmp') -Force;Expect-Failure {&$verifier -AnalogFocusEvidenceOnly -AnalogFocusEvidenceRun $analogFocusRun -AnalogFocusEvidenceRoot $analogFocusDeleted} 'deleted-prior-analog-focus-evidence'
 $recoveredPositive=&$verifier -RecoveredHotplugEvidenceOnly -RecoveredHotplugEvidenceRun $recoveredHotplugRun;if(-not$recoveredPositive.HotplugMatrixPassed-or-not$recoveredPositive.ControlledExitPassed-or$recoveredPositive.RuntimeArtifactSnapshotClaimed){throw 'Recovered hotplug positive classification differs.'}
 $recoveredMutated=Join-Path $fixtureRoot 'negative-recovered-hotplug-mutated';Copy-Item -LiteralPath $recoveredHotplugRoot -Destination $recoveredMutated -Recurse;$recoveredLog=Join-Path $recoveredMutated 'runs/01/mcla.log';$recoveredText=[IO.File]::ReadAllText($recoveredLog).Replace('hotplug_success=1','hotplug_success=0');[IO.File]::WriteAllText($recoveredLog,$recoveredText,$utf8);Expect-Failure {&$verifier -RecoveredHotplugEvidenceOnly -RecoveredHotplugEvidenceRun $recoveredHotplugRun -RecoveredHotplugEvidenceRoot $recoveredMutated} 'mutated-recovered-hotplug-evidence'
 $recoveredDeleted=Join-Path $fixtureRoot 'negative-recovered-hotplug-deleted';Copy-Item -LiteralPath $recoveredHotplugRoot -Destination $recoveredDeleted -Recurse;Remove-Item -LiteralPath (Join-Path $recoveredDeleted 'runs/01/user/mcla-first-frame.bmp') -Force;Expect-Failure {&$verifier -RecoveredHotplugEvidenceOnly -RecoveredHotplugEvidenceRun $recoveredHotplugRun -RecoveredHotplugEvidenceRoot $recoveredDeleted} 'deleted-recovered-hotplug-evidence'
 $physical=Join-Path $fixtureRoot 'positive-physical';New-PhysicalProbe $physical;$p=Probe $physical;if($p.ResumeCount-ne1-or$p.ButtonCount-ne0-or$p.InputReadyCount-ne0-or$p.AnalogCount-ne0-or$p.FocusCount-ne0-or$p.RumbleCount-ne0){throw 'Physical hotplug continuation positive counters differ.'}
 $compact=Join-Path $fixtureRoot 'positive-compact';New-CompactProbe $compact $null;$c=Probe $compact -AuditOnly;if($c.SummaryCount-ne1){throw 'Compact positive summary differs.'}

 $cases=[Collections.Generic.List[object]]::new()
 function Add-Negative {param([string]$Label,[scriptblock]$Mutation)$cases.Add([pscustomobject]@{Label=$Label;Mutation=$Mutation})}
 Add-Negative 'missing-config' {param($t)$t.Replace($config+"`r`n",'')}
 Add-Negative 'missing-resume' {param($t)$t.Replace($resume+"`r`n",'')}
 Add-Negative 'duplicate-resume' {param($t)$t.Replace($resume,$resume+"`r`n"+$resume)}
 Add-Negative 'resume-before-arm' {param($t)$t.Replace($resume+"`r`n",'').Replace($arm,$resume+"`r`n"+$arm)}
 Add-Negative 'resume-prior-digital-zero' {param($t)$t.Replace('prior_digital=1 prior_rumble=1','prior_digital=0 prior_rumble=1')}
 Add-Negative 'resume-prior-rumble-zero' {param($t)$t.Replace('prior_digital=1 prior_rumble=1','prior_digital=1 prior_rumble=0')}
 Add-Negative 'resume-prior-analog-zero' {param($t)$t.Replace('prior_analog=1 prior_focus=1','prior_analog=0 prior_focus=1')}
 Add-Negative 'resume-prior-focus-zero' {param($t)$t.Replace('prior_analog=1 prior_focus=1','prior_analog=1 prior_focus=0')}
 Add-Negative 'resume-rumble-mask-drift' {param($t)$t.Replace('rumble_pattern_mask=7 rumble_stop_mask=7','rumble_pattern_mask=3 rumble_stop_mask=7')}
 Add-Negative 'resume-mask-drift' {param($t)$t.Replace('button_sdl_mask=F3FF button_guest_mask=F3FF','button_sdl_mask=73FF button_guest_mask=F3FF')}
 Add-Negative 'resume-seq-drift' {param($t)$t.Replace('focus_gained=1 source_seq=24','focus_gained=1 source_seq=23')}
 Add-Negative 'continuation-button-record' {param($t)$t.Replace($resume,$resume+"`r`nSDL_CONTROLLER_MATRIX_AUDIT_BUTTON v=1 layer=sdl event=down slot=0 bit=0001 seen_mask=0001 source_seq=1")}
 Add-Negative 'continuation-rumble-record' {param($t)$t.Replace($resume,$resume+"`r`nSDL_CONTROLLER_MATRIX_AUDIT_RUMBLE v=1 event=start slot=0 pattern=left result=00000000 supported=1")}
 Add-Negative 'continuation-suppressed-rumble-attempt' {param($t)$t.Replace($resume,$resume+"`r`nSDL_CONTROLLER_MATRIX_AUDIT_RUMBLE_SUPPRESSED v=1 slot=0 motor_mask=1 result=0000065B count=1`r`nSDL_CONTROLLER_MATRIX_AUDIT_FAILURE v=1 class=resume_rumble_attempt count=1")}
 Add-Negative 'continuation-input-record' {param($t)$t.Replace($resume,$resume+"`r`nSDL_CONTROLLER_MATRIX_AUDIT_INPUT v=1 status=READY neutral=1 rumble_pattern_mask=7 rumble_stop_mask=7")}
 Add-Negative 'missing-rtl-compatibility' {param($t)$t.Replace($rtlCompatibility+"`r`n",'')}
 Add-Negative 'duplicate-rtl-compatibility' {param($t)$t.Replace($rtlCompatibility,$rtlCompatibility+"`r`n"+$rtlCompatibility)}
 Add-Negative 'rtl-compatibility-before-launch' {param($t)$t.Replace($rtlCompatibility+"`r`n",'').Replace('KernelState: Preparing module launch...',$rtlCompatibility+"`r`n"+'KernelState: Preparing module launch...')}
 Add-Negative 'rtl-compatibility-after-capture' {param($t)$t.Replace($rtlCompatibility+"`r`n",'').Replace($capture,$capture+"`r`n"+$rtlCompatibility)}
 Add-Negative 'duplicate-config' {param($t)$t.Replace($config,$config+"`r`n"+$config)}
 Add-Negative 'missing-assigned' {param($t)$t.Replace($assigned+"`r`n",'')}
 Add-Negative 'assigned-slot1' {param($t)$t.Replace('event=assigned slot=0 active_mask=1','event=assigned slot=1 active_mask=2')}
 Add-Negative 'missing-arm' {param($t)$t.Replace($arm+"`r`n",'')}
 Add-Negative 'arm-fail' {param($t)$t.Replace('phase=title status=READY','phase=title status=FAIL')}
 Add-Negative 'capture-before-arm' {param($t)$t.Replace($capture+"`r`n",'').Replace($arm,$capture+"`r`n"+$arm)}
 Add-Negative 'resume-after-capture' {param($t)$t.Replace($resume+"`r`n",'').Replace($capture,$capture+"`r`n"+$resume)}
 Add-Negative 'missing-summary' {param($t)$t.Replace($summary+"`r`n",'')}
 Add-Negative 'duplicate-summary' {param($t)$t.Replace($summary,$summary+"`r`n"+$summary)}
 Add-Negative 'summary-fail' {param($t)$t.Replace('SUMMARY v=1 phase=title status=PASS','SUMMARY v=1 phase=title status=FAIL')}
 Add-Negative 'summary-button-mask' {param($t)$t.Replace('button_guest_mask=F3FF','button_guest_mask=73FF')}
 Add-Negative 'summary-analog-mask' {param($t)$t.Replace('analog_guest_mask=3FF','analog_guest_mask=1FF')}
 Add-Negative 'summary-rumble-mask' {param($t)$t.Replace('rumble_pattern_mask=7','rumble_pattern_mask=3')}
 Add-Negative 'summary-rumble-stop-mask' {param($t)$t.Replace('rumble_stop_mask=7','rumble_stop_mask=3')}
 Add-Negative 'summary-unexpected' {param($t)$t.Replace('unexpected=0','unexpected=1')}
 Add-Negative 'summary-failures' {param($t)$t.Replace('failures=0','failures=1')}
 Add-Negative 'summary-drop' {param($t)$t.Replace('dropped_records=0','dropped_records=1')}
 Add-Negative 'failure-record' {param($t)$t.Replace($summary,"SDL_CONTROLLER_MATRIX_AUDIT_FAILURE v=1 class=guest_button_mismatch count=1`r`n"+$summary)}
 Add-Negative 'unknown-record' {param($t)$t.Replace($summary,"SDL_CONTROLLER_MATRIX_AUDIT_SECRET v=1 controller_guid=private`r`n"+$summary)}
 Add-Negative 'input-stub' {param($t)$t.Replace($summary,"REX_EXPORT_STUB XamInputGetState unimplemented`r`n"+$summary)}
 for($i=0;$i-lt10;$i++){$bit=$analogBits[$i];$seq=$i+15;foreach($layer in @('sdl','guest')){$forged="SDL_CONTROLLER_MATRIX_AUDIT_ANALOG v=1 layer=$layer event=extreme slot=0 bit=$bit seen_mask=$bit source_seq=$seq";Add-Negative "hotplug-resume-forged-analog-$layer-$bit" ({param($t)$t.Replace($resume,$resume+"`r`n"+$forged)}.GetNewClosure())}}
 Add-Negative 'analog-orphan-neutral' {param($t)$t.Replace($resume,$resume+"`r`nSDL_CONTROLLER_MATRIX_AUDIT_ANALOG v=1 layer=guest event=neutral slot=0 bit=001 seen_mask=000 source_seq=15")}
 Add-Negative 'analog-seq-drift' {param($t)$t.Replace($resume,$resume+"`r`nSDL_CONTROLLER_MATRIX_AUDIT_ANALOG v=1 layer=sdl event=extreme slot=0 bit=001 seen_mask=001 source_seq=16")}
 Add-Negative 'hotplug-resume-forged-focus-lost' {param($t)$t.Replace($resume,$resume+"`r`nSDL_CONTROLLER_MATRIX_AUDIT_FOCUS v=1 event=lost seq=1")}
 Add-Negative 'hotplug-resume-forged-focus-neutral' {param($t)$t.Replace($resume,$resume+"`r`nSDL_CONTROLLER_MATRIX_AUDIT_FOCUS v=1 event=neutral slot=0 result=00000000 packet_changed=1 seq=1")}
 Add-Negative 'hotplug-resume-forged-focus-gained' {param($t)$t.Replace($resume,$resume+"`r`nSDL_CONTROLLER_MATRIX_AUDIT_FOCUS v=1 event=gained seq=2")}
 Add-Negative 'hotplug-resume-mode-conflict' {param($t)$t.Replace($arm,"SDL_CONTROLLER_MATRIX_AUDIT_ARM v=1 phase=title status=FAIL active_mask=1 neutral=1`r`nSDL_CONTROLLER_MATRIX_AUDIT_FAILURE v=1 class=resume_mode_conflict count=1")}
 Add-Negative 'focus-packet-static' {param($t)$t.Replace($resume,$resume+"`r`nSDL_CONTROLLER_MATRIX_AUDIT_FOCUS v=1 event=neutral slot=0 result=00000000 packet_changed=0 seq=1")}
 Add-Negative 'focus-seq-drift' {param($t)$t.Replace($resume,$resume+"`r`nSDL_CONTROLLER_MATRIX_AUDIT_FOCUS v=1 event=neutral slot=0 result=00000000 packet_changed=1 seq=2")}
 Add-Negative 'summary-rumble-source-current' {param($t)$t.Replace('rumble_source=prior-evidence rumble_current_records=0','rumble_source=current rumble_current_records=0')}
 Add-Negative 'summary-rumble-current-six' {param($t)$t.Replace('rumble_source=prior-evidence rumble_current_records=0','rumble_source=prior-evidence rumble_current_records=6')}
 Add-Negative 'hotplug-disconnect-result' {param($t)$t.Replace('result=0000048F seq=3','result=00000000 seq=3')}
 Add-Negative 'hotplug-seq-drift' {param($t)$t.Replace('event=success slot=0 result=00000000 seq=4','event=success slot=0 result=00000000 seq=5')}
 Add-Negative 'rejected-pad' {param($t)$t.Replace($multi,"SDL_CONTROLLER_MATRIX_AUDIT_MULTIPAD v=1 assigned_mask=1 active_mask=1 rejected=1")}
 if($cases.Count-lt60){throw "Fixture suite only defines $($cases.Count) negatives."}
 $i=0;foreach($case in $cases){$i++;$root=Join-Path $fixtureRoot ('negative-{0:D2}-{1}'-f$i,$case.Label);$baseline=((@('KernelState: Preparing module launch...',$rtlCompatibility)+$pre+@($arm,$resume,$capture)+@($post|Select-Object -Skip 1))-join"`r`n")+"`r`n";$mutated=&$case.Mutation $baseline;if($mutated-ceq$baseline){throw "Negative fixture mutation was a no-op: $($case.Label)"};New-CompactProbe $root $case.Mutation;Expect-Failure {Probe $root -AuditOnly} $case.Label}

 $runnerText=[IO.File]::ReadAllText($runner);$verifierText=[IO.File]::ReadAllText($verifier)
 foreach($needle in @('DigitalEvidenceRun','AnalogFocusEvidenceRun','--sdl_controller_matrix_audit=true','--sdl_controller_matrix_hotplug_resume=true','--input_backend=sdl','--mnk_mode=false','[input][sdl][controller-matrix],[kernel][rtl][critical-section]','keep exactly one selected controller connected, A released, and the game window focused','prior digital/rumble/analog/focus bound; begin hotplug only','audit_resume_count=$p.ResumeCount','audit_button_records=$p.ButtonCount','audit_input_ready_count=$p.InputReadyCount','audit_rumble_records=$p.RumbleCount','nonzero_rumble_pulses_submitted_to_device=0','rumble_operator_prompted=$false','rumble_physical_patterns_user_reported=''left-right-both''','rumble_operator_attestation=''external-user-report''','rumble_operator_confirmation_recorded_in_run=$false','rumble_attestation_machine_verified=$false','matrix_elapsed_milliseconds=$matrix.ElapsedMilliseconds','split-digital-analog-focus-hotplug-matrix','gameplay_transition_blocked=$true','controlled_exit_passed=$false','monolithic_physical_run_claimed=$false','rtl_compatibility_markers=$p.RtlCompatibilityCount','left_deadzone=7849','right_deadzone=8689','trigger_threshold=30','private/evidence/M4-006','--clean-first --parallel 8','inputAssertions-ne104','inputCases-ne15')){if(-not$runnerText.Contains($needle)){throw "Runner source contract missing '$needle'."}}
 foreach($needle in @('DigitalEvidenceOnly','Get-DigitalEvidenceProbe','20260812-212030-5fc01c73','digital-pass_gameplay-transition-blocker','0x82554080','0x8255409C','sub_82554080','canonicalDigitalTreeSha256','SDL_CONTROLLER_MATRIX_AUDIT_HOTPLUG_RESUME v=1 phase=title status=READY prior_digital=1 prior_rumble=1 prior_analog=1 prior_focus=1','SDL_CONTROLLER_MATRIX_AUDIT_RUMBLE_SUPPRESSED','resume_rumble_attempt','ShouldSuppressControllerMatrixResumeRumble(stopping)','RecordControllerMatrixResumeRumbleSuppressed(user_index, motor_mask)','rumble_source=prior-evidence rumble_current_records=0','RtlCompatibilityCount','rtl_compatibility_markers','REXCVAR_SET(rtl_allow_cross_thread_critical_section_leave, true)','IsCriticalSectionLeaveAllowed(','IsControllerMatrixInputReady(','IsControllerMatrixSurfaceComplete(','SDL_CONTROLLER_MATRIX_AUDIT_INPUT v=1 status=READY','explicit compatibility option','source_seq','event=(?<event>extreme|neutral)','rumble_pattern_mask=7','rumble_stop_mask=7','rumble_physical_patterns_user_reported','rumble_operator_attestation','rumble_operator_confirmation_recorded_in_run','rumble_attestation_machine_verified','nonzero_rumble_pulses_submitted_to_device','rumble_operator_prompted','sdk_input_test_assertions-ne90','90 assertions in 13 test cases','kControllerMatrixLeftDeadzone = 7849','ClassifyControllerMatrixAnalog','Assert-StaticContract','Get-ExactProcesses','ReparsePoint')){if(-not$verifierText.Contains($needle)){throw "Verifier source contract missing '$needle'."}}
 foreach($forbidden in @('--mcla_controller_matrix_probe=true','Read-Host','prior_rumble_physically_confirmed','prior_rumble_confirmation_source','continuation_rumble_executed','continuation_operator_prompted')){if($runnerText.Contains($forbidden)-or$verifierText.Contains($forbidden)){throw "Split continuation retains forbidden rumble semantics '$forbidden'."}}
 foreach($forbidden in @(('M4-'+'005'),('SDL_INPUT_'+'SLOT_AUDIT'),('single-sdl-controller-'+'slot0'))){if($runnerText.Contains($forbidden)-or$verifierText.Contains($forbidden)){throw "M4-006 scripts retain copied logic."}}
 [pscustomobject]@{Passed=$true;ImmutableDigitalPositiveProbes=1;PhysicalContinuationPositiveProbes=1;CompactContinuationPositiveProbes=1;FailClosedContinuationNegativeProbes=$cases.Count;FailClosedDigitalEvidenceNegativeProbes=2;ImmutableAnalogFocusPositiveProbes=1;FailClosedAnalogFocusEvidenceNegativeProbes=2;RecoveredHotplugPositiveProbes=1;FailClosedRecoveredHotplugNegativeProbes=2;SourceContractChecks=60;Decision='split-digital-analog-focus-hotplug-matrix';MonolithicPhysicalRunClaimed=$false;GameplayTransitionBlockerPreserved=$true;MultiPadPhysicallyClaimed=$false}
}finally{if(Test-Path $fixtureRoot){Remove-Item $fixtureRoot -Recurse -Force}}
