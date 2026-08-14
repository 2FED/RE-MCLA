[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-edram-depth-report.ps1'
$root = Join-Path $repo ('private/evidence/M5-004/test-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)

function New-Lines {
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add('XENOS_AUDIT_CONFIG v=1 backend=d3d12 rt_path=host bindless=1 scale_x=1 scale_y=1 native_2x_supported=1 gamma_rt_unorm16=1 depth_f24_ps=0 depth_f24_round=0 direct_host_resolve=1')
    foreach ($i in 0..46) { $mode=$i%8;$src=@(1,2,4)[$i%3];$dst=@(1,2,4)[($i+1)%3];$lines.Add("XENOS_AUDIT_OWNERSHIP v=1 id=$i mode=$mode src_guest_msaa=$src dst_guest_msaa=$dst src_host_samples=$src dst_host_samples=$dst") }
    foreach ($i in 0..16) { $msaa=@(1,2,4)[$i%3];$test=[int]($i-lt12);$write=[int]($i-lt9);$stencil=[int]($i-lt12);$lines.Add("XENOS_AUDIT_BIND v=1 id=$i slot=depth guest_fmt=1 storage_fmt=1 guest_msaa=$msaa depth_test=$test depth_write=$write stencil=$stencil host_bound=1") }
    foreach ($i in 0..2) { $msaa=@(1,2,4)[$i];$lines.Add("XENOS_AUDIT_RESOLVE v=1 id=$i src_kind=depth src_fmt=1 guest_msaa=$msaa dest_fmt=23 shader=0 scaled=0") }
    $lines.Add('XENOS_AUDIT_CP_SUMMARY v=1 phase=checkpoint draw_issued=6000000 draw_indexed=4000000 draw_nonindexed=2000000 pso_pending_skip=0 pso_failed_skip=0 depth_test=5800000 depth_write=5700000 stencil=5400000 depth_bound=5900000 depth_without_bound=1 bind_records=17 bind_overflow=0 msaa1=1000000 msaa2=4000000 msaa4=1000000 gamma_table_dispatch=1 gamma_pwl_dispatch=0 gamma_identity_dispatch=0 gamma_nonidentity_dispatch=1 gamma_table_writes=0 gamma_pwl_writes=0 gamma_uploads=0 gamma_records=1 gamma_overflow=0 refresh_fail=0')
    $lines.Add('XENOS_AUDIT_RT_SUMMARY v=1 phase=checkpoint create_attempt=39 create_ok=39 create_fail=0 records=39 overflow=0 host_depth_store=19814 ownership_draws=536929 ownership_modes=47 ownership_overflow=0')
    $lines.Add('XENOS_AUDIT_RESOLVE_SUMMARY v=1 phase=checkpoint calls=93261 info_ok=93261 info_fail=0 zero_area=0 shader_known=93261 shader_unknown=0 direct_preflight=93261 direct_preflight_dump_ok=93261 direct_reject=0 fallback_dump_ok=0 fallback_dump_fail=0 copy_dispatch=93261 final_ok=93261 final_fail=0 modes=14 overflow=0 true_direct_dispatch=0')
    ,$lines
}
function Invoke-Probe([Collections.Generic.List[string]]$Lines, [string]$Name) {
    $dir=Join-Path $root $Name;[IO.Directory]::CreateDirectory($dir)|Out-Null;$path=Join-Path $dir 'mcla.log';[IO.File]::WriteAllLines($path,$Lines,$utf8);&$verify -RuntimeLogPath $path -FixtureMode
}
function Expect-Failure([scriptblock]$Action,[string]$Name){try{&$Action|Out-Null;throw "Negative '$Name' was accepted."}catch{if($_.Exception.Message-eq"Negative '$Name' was accepted."){throw}}}
function Replace-First([Collections.Generic.List[string]]$Lines,[string]$Old,[string]$New){for($i=0;$i-lt$Lines.Count;$i++){if($Lines[$i].Contains($Old)){$Lines[$i]=$Lines[$i].Replace($Old,$New);return}};throw "Fixture token '$Old' missing."}

try {
    $positive=Invoke-Probe (New-Lines) 'positive';if(-not$positive.Passed){throw 'Positive probe failed.'}
    $cases=[ordered]@{
        'rov-path'={param($x)Replace-First $x 'rt_path=host' 'rt_path=rov'}
        'ownership-missing'={param($x)$x.RemoveAt(1)}
        'ownership-mode-missing'={param($x)for($i=1;$i-lt48;$i++){if($x[$i]-match' mode=7 '){$x[$i]=$x[$i].Replace(' mode=7 ',' mode=6 ')}}}
        'ownership-host-sample'={param($x)Replace-First $x 'src_host_samples=1' 'src_host_samples=4'}
        'depth-bind-missing'={param($x)$x.RemoveAt(48)}
        'depth-test-low'={param($x)for($i=48;$i-lt60;$i++){$x[$i]=$x[$i].Replace(' depth_test=1 ',' depth_test=0 ')}}
        'depth-write-low'={param($x)for($i=48;$i-lt57;$i++){$x[$i]=$x[$i].Replace(' depth_write=1 ',' depth_write=0 ')}}
        'stencil-low'={param($x)for($i=48;$i-lt60;$i++){$x[$i]=$x[$i].Replace(' stencil=1 ',' stencil=0 ')}}
        'depth-resolve-missing'={param($x)$x.RemoveAt(65)}
        'depth-resolve-msaa'={param($x)Replace-First $x 'guest_msaa=4 dest_fmt=23' 'guest_msaa=2 dest_fmt=23'}
        'host-depth-store-zero'={param($x)Replace-First $x 'host_depth_store=19814' 'host_depth_store=0'}
        'ownership-draw-low'={param($x)Replace-First $x 'ownership_draws=536929' 'ownership_draws=1'}
        'ownership-summary-drift'={param($x)Replace-First $x 'ownership_modes=47' 'ownership_modes=46'}
        'depth-draw-low'={param($x)Replace-First $x 'depth_test=5800000' 'depth_test=1'}
        'depth-write-draw-low'={param($x)Replace-First $x 'depth_write=5700000' 'depth_write=1'}
        'stencil-draw-low'={param($x)Replace-First $x 'stencil=5400000' 'stencil=1'}
        'depth-without-bound-impossible'={param($x)Replace-First $x 'depth_without_bound=1' 'depth_without_bound=6000001'}
        'resolve-low'={param($x)Replace-First $x 'calls=93261' 'calls=1'}
        'resolve-failure'={param($x)Replace-First $x 'final_fail=0' 'final_fail=1'}
        'create-failure'={param($x)Replace-First $x 'create_fail=0' 'create_fail=1'}
        'bind-overflow'={param($x)Replace-First $x 'bind_overflow=0' 'bind_overflow=1'}
        'audit-failure'={param($x)$x.Add('XENOS_AUDIT_FAILURE v=1 class=test')}
        'device-loss'={param($x)$x.Add('[gpu] D3D12 device removed')}
        'snapshot-route'={param($x)$x.Add('[gpu] EDRAM snapshot restore')}
        'duplicate-config'={param($x)$x.Insert(1,$x[0])}
    }
    $index=0;foreach($entry in $cases.GetEnumerator()){$lines=New-Lines;&$entry.Value -x $lines;Expect-Failure {Invoke-Probe $lines ('negative-{0:D2}'-f$index)} $entry.Key;$index++}
    $source=[IO.File]::ReadAllText($verify)
    foreach($needle in @('host-rtv-ordering-depth-bounded-s2','Snapshot restoration external entry escaped the trace-player route','Host-depth store is not ordered before ownership transfers','accepted-s2-bounded-host-rtv','rov_exercised','snapshot_restore_exercised')){if(-not$source.Contains($needle)){throw "Source contract missing '$needle'."}}
    [pscustomobject]@{Passed=$true;PositiveFixtures=1;FailClosedNegatives=$cases.Count;SourceChecks=6}
} finally { if(Test-Path $root){Remove-Item -LiteralPath $root -Recurse -Force} }
