[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-rendering-smoke.ps1'
$utf8 = [Text.UTF8Encoding]::new($false)
$root = Join-Path $repo ('private\evidence\M5-003\test-' + [guid]::NewGuid().ToString('N').Substring(0,8))
[IO.Directory]::CreateDirectory($root)|Out-Null

function L([string]$Body){"[2026-08-14 10:00:00.000] [info] [test] [t1] $Body"}
function New-PositiveLines {
    $lines=@(
        (L 'XENOS_AUDIT_CONFIG v=1 backend=d3d12 rt_path=host bindless=1 scale_x=1 scale_y=1 native_2x_supported=1 gamma_rt_unorm16=1 depth_f24_ps=0 depth_f24_round=0 direct_host_resolve=1'),
        (L 'KernelState: Preparing module launch...'),
        (L 'MCLA_RENDER_SMOKE_CONFIG v=1 slot=0 gameplay_wait_seconds=45 traffic_samples=30 traffic_interval_ms=1000 camera_hold_ms=1200 particle_hold_ms=15000 dismiss_pulses=6 frames=36'),
        (L 'MCLA_FRONTEND_SMOKE_INPUT v=1 side=source sequence=1 buttons=0010'),
        (L 'MCLA_FRONTEND_SMOKE_INPUT v=1 side=guest sequence=1 buttons=0010'),
        (L 'MCLA_FRONTEND_SMOKE_INPUT v=1 side=source sequence=1 buttons=0000'),
        (L 'MCLA_FRONTEND_SMOKE_INPUT v=1 side=guest sequence=1 buttons=0000'),
        (L 'MCLA_RENDER_SMOKE_FRAME v=1 phase=world width=1280 height=720 status=PASS'),
        (L '__TRAFFIC_FRAMES__'),
        (L 'MCLA_RENDER_SMOKE_INPUT v=1 side=source sequence=1 buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=32767'),
        (L 'MCLA_RENDER_SMOKE_INPUT v=1 side=guest sequence=1 buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=32767'),
        (L 'MCLA_RENDER_SMOKE_FRAME v=1 phase=sky width=1280 height=720 status=PASS'),
        (L 'MCLA_RENDER_SMOKE_INPUT v=1 side=source sequence=1 buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=0'),
        (L 'MCLA_RENDER_SMOKE_INPUT v=1 side=guest sequence=1 buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=0'),
        (L 'MCLA_RENDER_SMOKE_FRAME v=1 phase=street width=1280 height=720 status=PASS'),
        (L 'MCLA_RENDER_SMOKE_INPUT v=1 side=source sequence=2 buttons=0000 lt=255 rt=255 lx=0 ly=0 rx=0 ry=0'),
        (L 'MCLA_RENDER_SMOKE_INPUT v=1 side=guest sequence=2 buttons=0000 lt=255 rt=255 lx=0 ly=0 rx=0 ry=0'),
        (L 'MCLA_RENDER_SMOKE_FRAME v=1 phase=particle-a width=1280 height=720 status=PASS'),
        (L 'MCLA_RENDER_SMOKE_FRAME v=1 phase=particle-b width=1280 height=720 status=PASS'),
        (L 'MCLA_RENDER_SMOKE_FRAME v=1 phase=particle-c width=1280 height=720 status=PASS'),
        (L 'MCLA_RENDER_SMOKE_INPUT v=1 side=source sequence=2 buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=0'),
        (L 'MCLA_RENDER_SMOKE_INPUT v=1 side=guest sequence=2 buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=0'),
        (L 'MCLA_RENDER_SMOKE_SUMMARY v=1 status=PASS frames=36 frontend_input_records=28 render_input_records=8 external_close_required=1'),
        (L 'XENOS_AUDIT_PIPELINE_SUMMARY v=1 phase=checkpoint shader_entries=400 translate_vs_ok=220 translate_ps_ok=240 translate_fail=0 pso_entries=400 pso_attempt=400 pso_ok=400 pso_fail=0 shader_records=256 shader_overflow=1 pso_records=400 pso_overflow=0'),
        (L 'XENOS_AUDIT_CP_SUMMARY v=1 phase=checkpoint draw_issued=200000 draw_indexed=100000 draw_nonindexed=100000 pso_pending_skip=0 pso_failed_skip=0 depth_test=100000 depth_write=90000 stencil=80000 depth_bound=190000 depth_without_bound=0 bind_records=40 bind_overflow=0 msaa1=20000 msaa2=100000 msaa4=80000 gamma_table_dispatch=100 gamma_pwl_dispatch=0 gamma_identity_dispatch=1 gamma_nonidentity_dispatch=99 gamma_table_writes=256 gamma_pwl_writes=0 gamma_uploads=2 gamma_records=3 gamma_overflow=0 refresh_fail=0'),
        (L 'XENOS_AUDIT_RT_SUMMARY v=1 phase=checkpoint create_attempt=30 create_ok=30 create_fail=0 records=30 overflow=0 host_depth_store=10 ownership_draws=100 ownership_modes=10 ownership_overflow=0'),
        (L 'XENOS_AUDIT_RESOLVE_SUMMARY v=1 phase=checkpoint calls=20000 info_ok=20000 info_fail=0 zero_area=0 shader_known=20000 shader_unknown=0 direct_preflight=20000 direct_preflight_dump_ok=20000 direct_reject=0 fallback_dump_ok=0 fallback_dump_fail=0 copy_dispatch=20000 final_ok=20000 final_fail=0 modes=10 overflow=0 true_direct_dispatch=0'),
        (L 'Window closing, shutting down...'),
        (L 'Execution complete'),
        (L 'Title terminated; hard-exiting process.')
    )
    foreach($line in $lines){if($line.Contains('__TRAFFIC_FRAMES__')){1..30|ForEach-Object{if($_%5-eq0){$sequence=1+$_/5;L "MCLA_FRONTEND_SMOKE_INPUT v=1 side=source sequence=$sequence buttons=1000";L "MCLA_FRONTEND_SMOKE_INPUT v=1 side=guest sequence=$sequence buttons=1000";L "MCLA_FRONTEND_SMOKE_INPUT v=1 side=source sequence=$sequence buttons=0000";L "MCLA_FRONTEND_SMOKE_INPUT v=1 side=guest sequence=$sequence buttons=0000"};L ('MCLA_RENDER_SMOKE_FRAME v=1 phase=traffic-{0:D2} width=1280 height=720 status=PASS'-f$_)}}else{$line}}
}

function Invoke-Case([string]$Name,[scriptblock]$Transform,[bool]$ShouldPass){
    $case=Join-Path $root $Name;[IO.Directory]::CreateDirectory($case)|Out-Null
    $lines=@(New-PositiveLines);if($Transform){$lines=@(& $Transform $lines)}
    [IO.File]::WriteAllLines((Join-Path $case 'mcla.log'),$lines,$utf8)
    $passed=$false
    try{$null=&$verify -ProbeOnly -FixtureMode -RuntimeLogPath (Join-Path $case 'mcla.log') -UserRoot $case;$passed=$true}catch{if($ShouldPass){throw "Positive '$Name' failed: $($_.Exception.Message)"}}
    if($passed-ne$ShouldPass){throw "Fixture '$Name' acceptance mismatch."}
}

function Remove-First([object[]]$Lines,[string]$Needle){$done=$false;@($Lines|Where-Object{if(-not$done-and$_.Contains($Needle)){$done=$true;$false}else{$true}})}
function Replace-First([object[]]$Lines,[string]$Old,[string]$New){$done=$false;@($Lines|ForEach-Object{if(-not$done-and$_.Contains($Old)){$done=$true;$_.Replace($Old,$New)}else{$_}})}
function Duplicate-First([object[]]$Lines,[string]$Needle){$result=[Collections.Generic.List[string]]::new();$done=$false;foreach($line in $Lines){$result.Add($line);if(-not$done-and$line.Contains($Needle)){$done=$true;$result.Add($line)}};@($result)}

try{
    Invoke-Case 'positive' $null $true
    $negatives=[ordered]@{
        'missing-config'={param($x)Remove-First $x 'MCLA_RENDER_SMOKE_CONFIG'}
        'duplicate-config'={param($x)Duplicate-First $x 'MCLA_RENDER_SMOKE_CONFIG'}
        'wrong-wait'={param($x)Replace-First $x 'gameplay_wait_seconds=45' 'gameplay_wait_seconds=44'}
        'wrong-particle-hold'={param($x)Replace-First $x 'particle_hold_ms=15000' 'particle_hold_ms=14999'}
        'wrong-traffic-samples'={param($x)Replace-First $x 'traffic_samples=30' 'traffic_samples=29'}
        'wrong-dismiss-count'={param($x)Replace-First $x 'dismiss_pulses=6' 'dismiss_pulses=5'}
        'missing-dismiss-input'={param($x)Remove-First $x 'side=guest sequence=4 buttons=1000'}
        'wrong-dismiss-input'={param($x)Replace-First $x 'side=source sequence=5 buttons=1000' 'side=source sequence=5 buttons=2000'}
        'missing-traffic-frame'={param($x)Remove-First $x 'phase=traffic-15'}
        'duplicate-traffic-frame'={param($x)Duplicate-First $x 'phase=traffic-15'}
        'missing-launch'={param($x)Remove-First $x 'Preparing module launch'}
        'duplicate-launch'={param($x)Duplicate-First $x 'Preparing module launch'}
        'missing-xenos-config'={param($x)Remove-First $x 'XENOS_AUDIT_CONFIG'}
        'wrong-backend'={param($x)Replace-First $x 'backend=d3d12' 'backend=vulkan'}
        'wrong-rt-path'={param($x)Replace-First $x 'rt_path=host' 'rt_path=rov'}
        'wrong-scale'={param($x)Replace-First $x 'scale_x=1' 'scale_x=2'}
        'missing-start-source'={param($x)Remove-First $x 'side=source sequence=1 buttons=0010'}
        'wrong-start-button'={param($x)Replace-First $x 'buttons=0010' 'buttons=1000'}
        'duplicate-start-guest'={param($x)Duplicate-First $x 'side=guest sequence=1 buttons=0010'}
        'missing-world'={param($x)Remove-First $x 'phase=world'}
        'missing-sky'={param($x)Remove-First $x 'phase=sky'}
        'missing-street'={param($x)Remove-First $x 'phase=street'}
        'missing-particle-a'={param($x)Remove-First $x 'phase=particle-a'}
        'missing-particle-b'={param($x)Remove-First $x 'phase=particle-b'}
        'missing-particle-c'={param($x)Remove-First $x 'phase=particle-c'}
        'wrong-frame-width'={param($x)Replace-First $x 'width=1280' 'width=1279'}
        'duplicate-frame'={param($x)Duplicate-First $x 'phase=particle-c'}
        'wrong-camera-axis'={param($x)Replace-First $x 'ry=32767' 'ry=32766'}
        'missing-camera-guest'={param($x)Remove-First $x 'side=guest sequence=1 buttons=0000 lt=0 rt=0 lx=0 ly=0 rx=0 ry=32767'}
        'wrong-trigger'={param($x)Replace-First $x 'lt=255 rt=255' 'lt=254 rt=255'}
        'missing-trigger-release'={param($x) $hits=@($x|Where-Object{$_.Contains('side=guest sequence=2 buttons=0000 lt=0 rt=0')});Remove-First $x $hits[0]}
        'summary-before-release'={param($x)$summary=@($x|Where-Object{$_.Contains('MCLA_RENDER_SMOKE_SUMMARY')})[0];$without=Remove-First $x 'MCLA_RENDER_SMOKE_SUMMARY';@($without[0..18])+$summary+@($without[19..($without.Count-1)])}
        'missing-summary'={param($x)Remove-First $x 'MCLA_RENDER_SMOKE_SUMMARY'}
        'summary-fail'={param($x)Replace-First $x 'MCLA_RENDER_SMOKE_SUMMARY v=1 status=PASS' 'MCLA_RENDER_SMOKE_SUMMARY v=1 status=FAIL'}
        'missing-pipeline-summary'={param($x)Remove-First $x 'XENOS_AUDIT_PIPELINE_SUMMARY'}
        'translation-fail'={param($x)Replace-First $x 'translate_fail=0' 'translate_fail=1'}
        'pso-fail'={param($x)Replace-First $x 'pso_fail=0' 'pso_fail=1'}
        'pso-overflow'={param($x)Replace-First $x 'pso_overflow=0' 'pso_overflow=1'}
        'unknown-pipeline-field'={param($x)Replace-First $x 'pso_overflow=0' 'pso_overflow=0 unknown=1'}
        'duplicate-pipeline-field'={param($x)Replace-First $x 'pso_overflow=0' 'pso_overflow=0 pso_ok=400'}
        'shader-no-saturation'={param($x)Replace-First $x 'shader_overflow=1' 'shader_overflow=0'}
        'shader-excessive-saturation'={param($x)Replace-First $x 'shader_overflow=1' 'shader_overflow=2049'}
        'draw-floor'={param($x)Replace-First $x 'draw_issued=200000' 'draw_issued=99999'}
        'draw-arithmetic'={param($x)Replace-First $x 'draw_indexed=100000' 'draw_indexed=100001'}
        'depth-floor'={param($x)Replace-First $x 'depth_test=100000' 'depth_test=0'}
        'msaa2-floor'={param($x)Replace-First $x 'msaa2=100000' 'msaa2=0'}
        'gamma-floor'={param($x)Replace-First $x 'gamma_nonidentity_dispatch=99' 'gamma_nonidentity_dispatch=0'}
        'rt-fail'={param($x)Replace-First $x 'create_fail=0' 'create_fail=1'}
        'rt-arithmetic'={param($x)Replace-First $x 'create_attempt=30' 'create_attempt=31'}
        'ownership-floor'={param($x)Replace-First $x 'ownership_draws=100' 'ownership_draws=0'}
        'resolve-floor'={param($x)Replace-First $x 'calls=20000' 'calls=9999'}
        'resolve-fail'={param($x)Replace-First $x 'final_fail=0' 'final_fail=1'}
        'resolve-arithmetic'={param($x)Replace-First $x 'info_ok=20000' 'info_ok=19999'}
        'true-direct-misclaim'={param($x)Replace-First $x 'true_direct_dispatch=0' 'true_direct_dispatch=1'}
        'missing-close'={param($x)Remove-First $x 'Window closing'}
        'missing-execution'={param($x)Remove-First $x 'Execution complete'}
        'missing-hard-exit'={param($x)Remove-First $x 'hard-exiting process'}
        'fatal'={param($x)@($x[0..20])+(L '[fatal] renderer crash')+@($x[21..($x.Count-1)])}
        'guest-crash'={param($x)@($x[0..20])+(L 'REX_GUEST_CRASH')+@($x[21..($x.Count-1)])}
        'device-removed'={param($x)@($x[0..20])+(L 'D3D12 device removed')+@($x[21..($x.Count-1)])}
        'detail-after-summary'={param($x)@($x[0..($x.Count-4)])+(L 'XENOS_AUDIT_DRAW v=1 id=999')+@($x[($x.Count-3)..($x.Count-1)])}
    }
    foreach($item in $negatives.GetEnumerator()){Invoke-Case $item.Key $item.Value $false}

    $app=[IO.File]::ReadAllText((Join-Path $repo 'src\mcla_app.cpp'))
    $runner=[IO.File]::ReadAllText((Join-Path $repo 'scripts\run-rendering-smoke.ps1'))
    $verifier=[IO.File]::ReadAllText($verify)
    $sourceNeedles=@(
        'mcla_rendering_smoke_probe, false',
        '.lifecycle(rex::cvar::Lifecycle::kInitOnly)',
        'MCLA_RENDER_SMOKE_CONFIG v=1',
        'camera.thumb_ry = 32767;',
        'particle.left_trigger = 255;',
        'particle.right_trigger = 255;',
        'mcla-render-particle-a.bmp',
        'mcla-render-particle-b.bmp',
        'mcla-render-particle-c.bmp',
        'graphics->RequestRenderAuditCheckpoint();',
        'traffic_samples=30 traffic_interval_ms=1000',
        'dismiss_pulses=6',
        'MCLA_RENDER_SMOKE_SUMMARY v=1 status=PASS frames=36',
        '--mcla_rendering_smoke_probe=true',
        '--gpu_render_audit=true',
        '--async_shader_compilation=false',
        '--render_target_path_d3d12=rtv',
        'rendering-contact-sheet.png',
        'awaiting-owner-visual-review',
        'AddSeconds(200)',
        'KI-013-green-vehicle-shadow-nonblocking',
        'shader_records-ne256',
        'shader_overflow-lt1')
    foreach($needle in $sourceNeedles){if(-not($app.Contains($needle)-or$runner.Contains($needle)-or$verifier.Contains($needle))){throw "Source contract missing '$needle'."}}
    [pscustomobject]@{Passed=$true;Positives=1;FailClosedNegatives=$negatives.Count;SourceChecks=$sourceNeedles.Count}
}finally{if(Test-Path $root){Remove-Item -LiteralPath $root -Recurse -Force}}
