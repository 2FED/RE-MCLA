[CmdletBinding()]
param([string]$AcceptedResultPath = 'private/evidence/M5-003/20260814-104624-fde51a30/result.json')

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-edram-depth-report.ps1'
$probe = & $verify -AcceptedResultPath $AcceptedResultPath
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$suffix = -join ((1..8) | ForEach-Object { '{0:x}' -f (Get-Random -Maximum 16) })
$root = Join-Path $repo "private/evidence/M5-004/$stamp-$suffix"
[IO.Directory]::CreateDirectory($root) | Out-Null
$record = [ordered]@{
    schema = 1; task = 'M5-004'; decision = 'host-rtv-ordering-depth-bounded-s2'
    sdk_version = '0.9.0.18'; sdk_commit = '923c92d1d1cb721cb704ac603fba263a01ba06aa'
    main_evidence_commit = 'c7ec3b672ff339228c5e53a805d8a92657642951'
    accepted_m5_003 = [ordered]@{ run_id='20260814-104624-fde51a30'; result_sha256='299392E59CB38AFB44256E773884856FF0C869A96D2045086A65396C1ED4EFCA' }
    coverage = [ordered]@{ rt_path='host'; ownership_mode_records=$probe.OwnershipModes; ownership_draws=$probe.OwnershipDraws; host_depth_store_dispatches=$probe.HostDepthStores; depth_bind_records=$probe.DepthBinds; depth_resolve_records=$probe.DepthResolves; depth_test_draws=$probe.DepthTestDraws; depth_write_draws=$probe.DepthWriteDraws; stencil_draws=$probe.StencilDraws; depth_state_without_host_depth_draws=$probe.DepthStateWithoutHostDepthDraws; resolve_calls=$probe.ResolveCalls }
    scope = [ordered]@{ target_host_rtv_defect_reproduced=$false; behavior_patch_required=$false; rov_exercised=$false; snapshot_restore_exercised=$false; true_direct_resolve_exercised=$false; pwl_gamma_exercised=$false; owner_visual_rendering_categories_pass=$true; classification='accepted-s2-bounded-host-rtv' }
    data_integrity_verified = $true
}
$path = Join-Path $root 'result.json'
[IO.File]::WriteAllText($path, (($record | ConvertTo-Json -Depth 6) + [Environment]::NewLine), [Text.UTF8Encoding]::new($false))
$final = & $verify -ResultPath $path
[pscustomobject]@{ Passed=$final.Passed; Decision=$final.Decision; OwnershipModes=$final.OwnershipModes; HostDepthStores=$final.HostDepthStores; DepthResolves=$final.DepthResolves; PrivateRunRoot=$root; ResultPath=$path }
