[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier=Join-Path $PSScriptRoot 'verify-input-slot-smoke.ps1'
$runner=Join-Path $PSScriptRoot 'run-input-slot-smoke.ps1'
$source=Join-Path $repoRoot 'private/evidence/M4-004/20260812-123316-db0f1cf4/runs/01'
$fixtureRoot=Join-Path $repoRoot ('private/evidence/M4-005/test-'+[guid]::NewGuid().ToString('N').Substring(0,8))
$utf8=[Text.UTF8Encoding]::new($false)
$launch='KernelState: Preparing module launch...'
$capture='MCLA graphics: nontrivial guest frame captured '
$config='SDL_INPUT_SLOT_AUDIT_CONFIG v=1 enabled=1 expected_slot=0'
$device='SDL_INPUT_SLOT_AUDIT_DEVICE v=1 event=assigned slot=0 active_devices=1'
$arm='SDL_INPUT_SLOT_AUDIT_ARM v=1 phase=title status=READY active_devices=1 slot_mask=1 neutral=1'
$sdlDown='SDL_INPUT_SLOT_AUDIT_SDL_EDGE v=1 control=A edge=down slot=0 source_seq=1'
$guestDown='SDL_INPUT_SLOT_AUDIT_GUEST_STATE v=1 control=A edge=down slot=0 result=00000000 source_seq=1'
$sdlUp='SDL_INPUT_SLOT_AUDIT_SDL_EDGE v=1 control=A edge=up slot=0 source_seq=2'
$guestUp='SDL_INPUT_SLOT_AUDIT_GUEST_STATE v=1 control=A edge=up slot=0 result=00000000 source_seq=2'
$summary='SDL_INPUT_SLOT_AUDIT_SUMMARY v=1 phase=title status=PASS active_devices=1 slot_mask=1 query_mask=F success_mask=1 disconnected_mask=E sdl_down_seq=1 guest_down_seq=1 sdl_up_seq=2 guest_up_seq=2 removals=0 rejected=0 unexpected=0 failures=0 dropped_records=0'

function Expect-Failure {param([scriptblock]$Action,[string]$Label)try{&$Action|Out-Null;throw "Negative fixture '$Label' was accepted."}catch{if($_.Exception.Message-eq"Negative fixture '$Label' was accepted."){throw}}}
function Get-SourceText {$parts=@();foreach($n in 3,2,1){$p=Join-Path $source "mcla.$n.log";if(Test-Path $p){$parts+=[IO.File]::ReadAllText($p)}};$parts+=[IO.File]::ReadAllText((Join-Path $source 'mcla.log'));($parts-join[Environment]::NewLine).Replace('30025 mappings','30026 mappings')}
function Write-LogSet {param([string]$Root,[string]$Text)$chunks=[Collections.Generic.List[string]]::new();$b=[Text.StringBuilder]::new();foreach($line in($Text-split"`r?`n")){if($b.Length+$line.Length+2-gt4000000){$chunks.Add($b.ToString());$null=$b.Clear()};$null=$b.AppendLine($line)};if($b.Length){$chunks.Add($b.ToString())};for($i=0;$i-lt$chunks.Count;$i++){$name=if($i-eq$chunks.Count-1){'mcla.log'}else{"mcla.$($chunks.Count-1-$i).log"};$path=Join-Path $Root $name;[IO.File]::WriteAllText($path,$chunks[$i],$utf8);(Get-Item $path).LastWriteTimeUtc=([datetime]'2026-08-12T00:00:00Z').AddSeconds($i)}}
function New-Probe {param([string]$Root,[scriptblock]$Mutate,[switch]$Compact)$user=Join-Path $Root 'user';[IO.Directory]::CreateDirectory($user)|Out-Null;[IO.Directory]::CreateDirectory((Join-Path $Root 'cache'))|Out-Null;Copy-Item (Join-Path $source 'user/mcla-first-frame.bmp') (Join-Path $user 'mcla-first-frame.bmp');$tail=@($sdlDown,$guestDown,$sdlUp,$guestUp,$summary)-join"`r`n";if($Compact){$text="$config`r`n$device`r`n[fixture] $launch`r`n$arm`r`n[fixture] $capture`r`n$tail`r`n"}else{$text=Get-SourceText;$li=$text.IndexOf($launch);$ci=$text.IndexOf($capture);if($li-lt0-or$ci-lt0){throw 'Pinned title source markers missing.'};$text=$text.Insert($li,$config+"`r`n"+$device+"`r`n");$ci=$text.IndexOf($capture);$text=$text.Insert($ci,$arm+"`r`n");$ci=$text.IndexOf($capture);$end=$text.IndexOf("`n",$ci);if($end-lt0){$end=$text.Length-1};$text=$text.Insert($end+1,$tail+"`r`n")};if($Mutate){$text=[string](&$Mutate $text);if(-not$text){throw 'Fixture mutation returned empty text.'}};Write-LogSet $Root $text}
function Probe {param([string]$Root,[switch]$AuditOnly)&$verifier -ProbeOnly -AuditOnly:$AuditOnly -RuntimeLogPath(Join-Path $Root 'mcla.log') -BmpPath(Join-Path $Root 'user/mcla-first-frame.bmp')}

try{
 [IO.Directory]::CreateDirectory($fixtureRoot)|Out-Null;$positive=Join-Path $fixtureRoot 'positive';New-Probe $positive $null;$p=Probe $positive;if($p.SummaryCount-ne1-or$p.SdlEdgeCount-ne2-or$p.GuestEdgeCount-ne2){throw 'Physical positive counters differ.'};$compact=Join-Path $fixtureRoot 'positive-compact';New-Probe $compact $null -Compact;$cp=Probe $compact -AuditOnly;if($cp.DeviceCount-ne1-or$cp.ArmCount-ne1){throw 'Compact positive counters differ.'}
 $cases=[ordered]@{
  'missing-config'={param($t)$t.Replace($config+"`r`n",'')}
  'duplicate-config'={param($t)$t.Replace($config,$config+"`r`n"+$config)}
  'config-after-arm'={param($t)$t.Replace($config+"`r`n",'').Replace($arm,$arm+"`r`n"+$config)}
  'missing-device'={param($t)$t.Replace($device+"`r`n",'')}
  'duplicate-device'={param($t)$t.Replace($device,$device+"`r`n"+$device)}
  'device-before-config'={param($t)$t.Replace($config+"`r`n"+$device,$device+"`r`n"+$config)}
  'device-slot1'={param($t)$t.Replace('event=assigned slot=0','event=assigned slot=1')}
  'device-count2'={param($t)$t.Replace('event=assigned slot=0 active_devices=1','event=assigned slot=0 active_devices=2')}
  'device-rejected'={param($t)$t.Replace($device,'SDL_INPUT_SLOT_AUDIT_DEVICE v=1 event=rejected slot=0 active_devices=1')}
  'device-removed-extra'={param($t)$t.Replace($arm,"SDL_INPUT_SLOT_AUDIT_DEVICE v=1 event=removed slot=0 active_devices=0`r`n"+$arm)}
  'missing-arm'={param($t)$t.Replace($arm+"`r`n",'')}
  'duplicate-arm'={param($t)$t.Replace($arm,$arm+"`r`n"+$arm)}
  'arm-before-device'={param($t)$t.Replace($device+"`r`n[fixture] $launch`r`n"+$arm,$arm+"`r`n[fixture] $launch`r`n"+$device)}
  'arm-fail'={param($t)$t.Replace('phase=title status=READY','phase=title status=FAIL')}
  'arm-wrong-phase'={param($t)$t.Replace('ARM v=1 phase=title','ARM v=1 phase=boot')}
  'arm-two-devices'={param($t)$t.Replace('status=READY active_devices=1','status=READY active_devices=2')}
  'arm-slotmask2'={param($t)$t.Replace('slot_mask=1 neutral=1','slot_mask=2 neutral=1')}
  'arm-nonneutral'={param($t)$t.Replace('slot_mask=1 neutral=1','slot_mask=1 neutral=0')}
  'arm-after-capture'={param($t)$t.Replace($arm+"`r`n",'').Replace("[fixture] $capture","[fixture] $capture`r`n"+$arm)}
  'missing-sdl-down'={param($t)$t.Replace($sdlDown+"`r`n",'')}
  'duplicate-sdl-down'={param($t)$t.Replace($sdlDown,$sdlDown+"`r`n"+$sdlDown)}
  'missing-guest-down'={param($t)$t.Replace($guestDown+"`r`n",'')}
  'guest-before-sdl-down'={param($t)$t.Replace($sdlDown+"`r`n"+$guestDown,$guestDown+"`r`n"+$sdlDown)}
  'down-wrong-control'={param($t)$t.Replace('control=A edge=down','control=B edge=down')}
  'down-wrong-slot'={param($t)$t.Replace('edge=down slot=0','edge=down slot=1')}
  'down-wrong-seq'={param($t)$t.Replace('edge=down slot=0 source_seq=1','edge=down slot=0 source_seq=2')}
  'guest-down-failure'={param($t)$t.Replace('edge=down slot=0 result=00000000','edge=down slot=0 result=8007048F')}
  'missing-sdl-up'={param($t)$t.Replace($sdlUp+"`r`n",'')}
  'missing-guest-up'={param($t)$t.Replace($guestUp+"`r`n",'')}
  'up-before-guest-down'={param($t)$t.Replace($guestDown+"`r`n"+$sdlUp,$sdlUp+"`r`n"+$guestDown)}
  'guest-before-sdl-up'={param($t)$t.Replace($sdlUp+"`r`n"+$guestUp,$guestUp+"`r`n"+$sdlUp)}
  'up-wrong-seq'={param($t)$t.Replace('edge=up slot=0 source_seq=2','edge=up slot=0 source_seq=1')}
  'missing-summary'={param($t)$t.Replace($summary+"`r`n",'')}
  'duplicate-summary'={param($t)$t.Replace($summary,$summary+"`r`n"+$summary)}
  'summary-before-edges'={param($t)$t.Replace($summary+"`r`n",'').Replace($sdlDown,$summary+"`r`n"+$sdlDown)}
  'summary-fail'={param($t)$t.Replace('SUMMARY v=1 phase=title status=PASS','SUMMARY v=1 phase=title status=FAIL')}
  'query-mask7'={param($t)$t.Replace('query_mask=F','query_mask=7')}
  'success-mask3'={param($t)$t.Replace('success_mask=1','success_mask=3')}
  'disconnected-mask6'={param($t)$t.Replace('disconnected_mask=E','disconnected_mask=6')}
  'summary-down-drift'={param($t)$t.Replace('sdl_down_seq=1','sdl_down_seq=2')}
  'summary-up-drift'={param($t)$t.Replace('guest_up_seq=2','guest_up_seq=1')}
  'summary-removal'={param($t)$t.Replace('removals=0','removals=1')}
  'summary-rejected'={param($t)$t.Replace('rejected=0','rejected=1')}
  'summary-unexpected'={param($t)$t.Replace('unexpected=0','unexpected=1')}
  'summary-failure'={param($t)$t.Replace('failures=0','failures=1')}
  'summary-drop'={param($t)$t.Replace('dropped_records=0','dropped_records=1')}
  'unknown-record'={param($t)$t.Replace($summary,"SDL_INPUT_SLOT_AUDIT_SECRET v=1 guid=private`r`n"+$summary)}
  'generic-input-stub'={param($t)$t.Replace($summary,"REX_EXPORT_STUB XamInputGetState unimplemented`r`n"+$summary)}
 }
 $i=0;foreach($label in $cases.Keys){$i++;$root=Join-Path $fixtureRoot ('negative-{0:D2}-{1}'-f$i,$label);New-Probe $root $cases[$label] -Compact;Expect-Failure{Probe $root -AuditOnly}$label}
 $runnerText=[IO.File]::ReadAllText($runner);$verifierText=[IO.File]::ReadAllText($verifier)
 foreach($needle in @('--input_backend=sdl','--mnk_mode=false','--sdl_input_slot_audit=true','[input][sdl][slot]','HOLD A','RELEASE A','--clean-first','--mcla_first_frame_settle_seconds=35','WaitForExit(10000)')){if(-not$runnerText.Contains($needle)){throw "Runner source contract missing '$needle'."}}
 foreach($needle in @('SDL_INPUT_SLOT_AUDIT_SUMMARY','query_mask=F','success_mask=1','disconnected_mask=E','XamInputGetState=1','ArmInputSlotAudit','Assert-StaticContract','Get-ExactProcesses','ReparsePoint')){if(-not$verifierText.Contains($needle)){throw "Verifier source contract missing '$needle'."}}
 [pscustomobject]@{Passed=$true;PhysicalPositiveProbes=1;CompactPositiveProbes=1;FailClosedNegativeProbes=$cases.Count;SourceContractChecks=18;OperatorChallenge='neutral-hold-A-release'}
}finally{if(Test-Path $fixtureRoot){Remove-Item $fixtureRoot -Recurse -Force}}
