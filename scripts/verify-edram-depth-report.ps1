[CmdletBinding(DefaultParameterSetName = 'Evidence')]
param(
    [Parameter(ParameterSetName = 'Evidence')][string]$AcceptedResultPath =
        'private/evidence/M5-003/20260814-104624-fde51a30/result.json',
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$RuntimeLogPath,
    [Parameter(ParameterSetName = 'Probe')][switch]$FixtureMode,
    [Parameter(Mandatory, ParameterSetName = 'Result')][string]$ResultPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$acceptedHash = '299392E59CB38AFB44256E773884856FF0C869A96D2045086A65396C1ED4EFCA'
$acceptedRun = '20260814-104624-fde51a30'
$sdkCommit = '923c92d1d1cb721cb704ac603fba263a01ba06aa'
$mainCommit = 'c7ec3b672ff339228c5e53a805d8a92657642951'

function Resolve-Safe([string]$Path, [string]$Description, [bool]$Directory = $false) {
    $full = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repo $Path }))
    $prefix = $repo.TrimEnd('\') + '\'
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes repository." }
    $current = $repo
    foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object Length)) {
        $current = Join-Path $current $part
        if (-not (Test-Path -LiteralPath $current)) { throw "$Description is missing." }
        if ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "$Description traverses a reparse point." }
    }
    if ($Directory -and -not (Test-Path -LiteralPath $full -PathType Container)) { throw "$Description is not a directory." }
    if (-not $Directory -and -not (Test-Path -LiteralPath $full -PathType Leaf)) { throw "$Description is not a file." }
    $full
}

function Get-LogLines([string]$Current) {
    $current = Resolve-Safe $Current 'Runtime log'
    if ((Split-Path $current -Leaf) -cne 'mcla.log') { throw 'Current runtime log must be mcla.log.' }
    $files = @(Get-ChildItem -LiteralPath (Split-Path $current) -File -Filter 'mcla*.log')
    $rotated = @(); $now = $null
    foreach ($file in $files) {
        if ($file.Name -ceq 'mcla.log') { $now = $file; continue }
        $match = [regex]::Match($file.Name, '^mcla\.([1-9][0-9]*)\.log$')
        if (-not $match.Success) { throw 'Malformed runtime-log rotation.' }
        $rotated += [pscustomobject]@{ Index = [int]$match.Groups[1].Value; File = $file }
    }
    if (-not $now -or $files.Count -gt 24) { throw 'Runtime-log topology is invalid.' }
    $indices = @($rotated | ForEach-Object Index | Sort-Object)
    for ($i = 0; $i -lt $indices.Count; $i++) { if ($indices[$i] -ne $i + 1) { throw 'Runtime-log rotations are not contiguous.' } }
    $ordered = @($rotated | Sort-Object Index -Descending | ForEach-Object File) + $now
    $lines = [Collections.Generic.List[string]]::new(); $bytes = 0L
    foreach ($file in $ordered) {
        $bytes += $file.Length
        if ($bytes -gt 268435456) { throw 'Runtime logs exceed 256 MiB.' }
        foreach ($line in [IO.File]::ReadLines($file.FullName)) { $lines.Add($line) }
    }
    $lines
}

function Get-Fields([string]$Line) {
    $fields = [ordered]@{}
    foreach ($match in [regex]::Matches($Line, '(?<key>[a-z0-9_]+)=(?<value>[^\s]+)')) {
        $key = $match.Groups['key'].Value
        if ($fields.Contains($key)) { throw "Duplicate field '$key'." }
        $fields[$key] = $match.Groups['value'].Value
    }
    $fields
}

function Get-Only([Collections.Generic.List[string]]$Lines, [string]$Pattern, [string]$Description) {
    $hits = @($Lines | Where-Object { $_ -match $Pattern })
    if ($hits.Count -ne 1) { throw "$Description count is $($hits.Count), expected 1." }
    $hits[0]
}

function Get-Probe([string]$Log) {
    $lines = Get-LogLines $Log
    $text = $lines -join "`n"
    $config = Get-Only $lines 'XENOS_AUDIT_CONFIG v=1 backend=d3d12 rt_path=host .*scale_x=1 scale_y=1 .*native_2x_supported=1 .*direct_host_resolve=1$' 'Host-RTV config'
    $cp = Get-Fields (Get-Only $lines 'XENOS_AUDIT_CP_SUMMARY v=1 phase=checkpoint ' 'CP summary')
    $rt = Get-Fields (Get-Only $lines 'XENOS_AUDIT_RT_SUMMARY v=1 phase=checkpoint ' 'RT summary')
    $resolve = Get-Fields (Get-Only $lines 'XENOS_AUDIT_RESOLVE_SUMMARY v=1 phase=checkpoint ' 'Resolve summary')
    $ownership = @($lines | Where-Object { $_ -match 'XENOS_AUDIT_OWNERSHIP v=1 ' })
    $bind = @($lines | Where-Object { $_ -match 'XENOS_AUDIT_BIND v=1 ' })
    $depthBind = @($bind | Where-Object { $_ -match ' slot=depth ' -and $_ -match ' host_bound=1$' })
    $depthResolve = @($lines | Where-Object { $_ -match 'XENOS_AUDIT_RESOLVE v=1 .* src_kind=depth ' })
    if ($ownership.Count -ne 47 -or [int64]$rt.ownership_modes -ne 47 -or [int64]$rt.ownership_draws -lt 500000) { throw 'Ownership-transfer coverage changed.' }
    $modes = @($ownership | ForEach-Object { [int](Get-Fields $_).mode } | Sort-Object -Unique)
    if (($modes -join ',') -cne '0,1,2,3,4,5,6,7') { throw 'Ownership mode coverage is incomplete.' }
    foreach ($line in $ownership) {
        $f = Get-Fields $line
        if ([int]$f.src_guest_msaa -notin 1,2,4 -or [int]$f.dst_guest_msaa -notin 1,2,4 -or
            $f.src_guest_msaa -cne $f.src_host_samples -or $f.dst_guest_msaa -cne $f.dst_host_samples) { throw 'Ownership sample mapping changed.' }
    }
    if ($depthBind.Count -ne 17 -or @($depthBind | Where-Object { $_ -match ' depth_test=1 ' }).Count -lt 12 -or
        @($depthBind | Where-Object { $_ -match ' depth_write=1 ' }).Count -lt 9 -or
        @($depthBind | Where-Object { $_ -match ' stencil=1 ' }).Count -lt 12) { throw 'Depth binding coverage changed.' }
    $depthMsaa = @($depthResolve | ForEach-Object { [int](Get-Fields $_).guest_msaa } | Sort-Object -Unique)
    if ($depthResolve.Count -ne 3 -or ($depthMsaa -join ',') -cne '1,2,4') { throw 'Depth resolve coverage changed.' }
    foreach ($field in @(@($cp, 'pso_failed_skip'), @($cp, 'bind_overflow'), @($cp, 'refresh_fail'),
            @($rt, 'create_fail'), @($rt, 'overflow'), @($rt, 'ownership_overflow'),
            @($resolve, 'info_fail'), @($resolve, 'shader_unknown'), @($resolve, 'fallback_dump_fail'),
            @($resolve, 'final_fail'), @($resolve, 'overflow'), @($resolve, 'true_direct_dispatch'))) {
        if ([int64]$field[0][$field[1]] -ne 0) { throw "GPU failure field '$($field[1])' is nonzero." }
    }
    if ([int64]$cp.depth_test -lt 5000000 -or [int64]$cp.depth_write -lt 5000000 -or
        [int64]$cp.stencil -lt 5000000 -or [int64]$cp.depth_bound -lt 5000000 -or [int64]$cp.depth_without_bound -gt 2000 -or
        [int64]$rt.host_depth_store -lt 10000 -or [int64]$resolve.calls -lt 90000 -or
        [int64]$resolve.calls -ne [int64]$resolve.final_ok) { throw 'Depth/resolve execution floor changed.' }
    if ($text -match '(?i)(XENOS_AUDIT_FAILURE|\[fatal\]|PPC_UNIMPLEMENTED|D3D12.*device (?:lost|removed)|EDRAM snapshot)') { throw 'Fatal, device-loss, or snapshot-route marker found.' }
    [pscustomobject]@{
        Passed = $true; Decision = 'host-rtv-ordering-depth-bounded-s2'; OwnershipModes = $ownership.Count
        OwnershipDraws = [int64]$rt.ownership_draws; HostDepthStores = [int64]$rt.host_depth_store
        DepthBinds = $depthBind.Count; DepthResolves = $depthResolve.Count; DepthTestDraws = [int64]$cp.depth_test
        DepthWriteDraws = [int64]$cp.depth_write; StencilDraws = [int64]$cp.stencil
        DepthStateWithoutHostDepthDraws = [int64]$cp.depth_without_bound
        ResolveCalls = [int64]$resolve.calls; RovExercised = $false; SnapshotRestoreExercised = $false
    }
}

function Assert-SourceContract {
    if ((git -C (Join-Path $repo 'third_party/rexglue-sdk') rev-parse HEAD).Trim() -cne $sdkCommit) { throw 'SDK commit changed.' }
    $common = [IO.File]::ReadAllText((Resolve-Safe 'third_party/rexglue-sdk/src/graphics/pipeline/render_target/cache.cpp' 'Common RT cache'))
    $d3d = [IO.File]::ReadAllText((Resolve-Safe 'third_party/rexglue-sdk/src/graphics/d3d12/render_target_cache.cpp' 'D3D12 RT cache'))
    $trace = [IO.File]::ReadAllText((Resolve-Safe 'third_party/rexglue-sdk/src/graphics/trace_player.cpp' 'Trace player'))
    foreach ($needle in @('PrepareFullEdram1280xRenderTargetForSnapshotRestoration', 'Change ownership, but don''t transfer the contents', 'PixelShaderInterlockFullEdramBarrierPlaced')) { if (-not $common.Contains($needle)) { throw "Common ownership source contract missing '$needle'." } }
    $store = $d3d.IndexOf('Do host depth storing for the depth destination')
    $dispatch = $d3d.IndexOf('++render_audit_host_depth_store_', $store)
    $transfer = $d3d.IndexOf('Try to insert as many barriers as possible', $store)
    if ($store -lt 0 -or $dispatch -lt $store -or $transfer -lt $dispatch) { throw 'Host-depth store is not ordered before ownership transfers.' }
    foreach ($needle in @('CommitEdramBufferUAVWrites(EdramBufferModificationStatus::kAsUAV)', 'MarkEdramBufferModified(EdramBufferModificationStatus::kAsROV)', 'RestoreEdramSnapshot(const void* snapshot)')) { if (-not $d3d.Contains($needle)) { throw "D3D12 source contract missing '$needle'." } }
    if (-not $trace.Contains('command_processor->RestoreEdramSnapshot(edram_snapshot.get())')) { throw 'Trace-only snapshot restoration call changed.' }
    $calls = @(rg -n 'command_processor->RestoreEdramSnapshot\(' (Join-Path $repo 'third_party/rexglue-sdk/src/graphics'))
    if ($LASTEXITCODE -ne 0 -or $calls.Count -ne 1 -or $calls[0] -notmatch 'trace_player\.cpp') { throw 'Snapshot restoration external entry escaped the trace-player route.' }
}

function Get-Evidence([string]$Path) {
    Assert-SourceContract
    $accepted = Resolve-Safe $Path 'Accepted M5-003 result'
    if ((Split-Path (Split-Path $accepted -Parent) -Leaf) -cne $acceptedRun -or (Get-FileHash $accepted -Algorithm SHA256).Hash -cne $acceptedHash) { throw 'Accepted M5-003 evidence identity changed.' }
    $render = & (Join-Path $PSScriptRoot 'verify-rendering-smoke.ps1') -ResultPath $accepted
    if (-not $render.Passed -or -not $render.OwnerVisualPass) { throw 'Accepted rendering evidence failed physical re-verification.' }
    $probe = Get-Probe (Join-Path (Split-Path $accepted -Parent) 'runs/01/mcla.log')
    $probe
}

if ($PSCmdlet.ParameterSetName -eq 'Probe') { Get-Probe $RuntimeLogPath; return }
if ($PSCmdlet.ParameterSetName -eq 'Evidence') { Get-Evidence $AcceptedResultPath; return }

$result = Resolve-Safe $ResultPath 'M5-004 report'
$raw = [IO.File]::ReadAllText($result)
if ($raw -match '(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])') { throw 'Report contains a private or absolute path.' }
$record = $raw | ConvertFrom-Json
$expected = @('schema','task','decision','sdk_version','sdk_commit','main_evidence_commit','accepted_m5_003','coverage','scope','data_integrity_verified')
if (($record.PSObject.Properties.Name -join ',') -cne ($expected -join ',') -or $record.schema -ne 1 -or
    $record.task -cne 'M5-004' -or $record.decision -cne 'host-rtv-ordering-depth-bounded-s2' -or
    $record.sdk_version -cne '0.9.0.18' -or $record.sdk_commit -cne $sdkCommit -or
    $record.main_evidence_commit -cne $mainCommit -or $record.data_integrity_verified -ne $true) { throw 'M5-004 report identity changed.' }
$root = Split-Path $result -Parent
if ((Split-Path $result -Leaf) -cne 'result.json' -or (Split-Path (Split-Path $root -Parent) -Leaf) -cne 'M5-004' -or
    (Split-Path $root -Leaf) -notmatch '^[0-9]{8}-[0-9]{6}-[0-9a-f]{8}$' -or
    (@(Get-ChildItem -LiteralPath $root -Force | ForEach-Object Name) -join ',') -cne 'result.json') { throw 'M5-004 report topology changed.' }
if (($record.accepted_m5_003.PSObject.Properties.Name -join ',') -cne 'run_id,result_sha256' -or
    $record.accepted_m5_003.run_id -cne $acceptedRun -or $record.accepted_m5_003.result_sha256 -cne $acceptedHash) { throw 'Accepted-evidence binding changed.' }
$probe = Get-Evidence "private/evidence/M5-003/$acceptedRun/result.json"
$coverage = $record.coverage
$coverageExpected = @('rt_path','ownership_mode_records','ownership_draws','host_depth_store_dispatches','depth_bind_records','depth_resolve_records','depth_test_draws','depth_write_draws','stencil_draws','depth_state_without_host_depth_draws','resolve_calls')
if (($coverage.PSObject.Properties.Name -join ',') -cne ($coverageExpected -join ',') -or $coverage.rt_path -cne 'host' -or
    $coverage.ownership_mode_records -ne $probe.OwnershipModes -or $coverage.ownership_draws -ne $probe.OwnershipDraws -or
    $coverage.host_depth_store_dispatches -ne $probe.HostDepthStores -or $coverage.depth_bind_records -ne $probe.DepthBinds -or
    $coverage.depth_resolve_records -ne $probe.DepthResolves -or $coverage.depth_test_draws -ne $probe.DepthTestDraws -or
    $coverage.depth_write_draws -ne $probe.DepthWriteDraws -or $coverage.stencil_draws -ne $probe.StencilDraws -or
    $coverage.depth_state_without_host_depth_draws -ne $probe.DepthStateWithoutHostDepthDraws -or $coverage.depth_state_without_host_depth_draws -gt 2000 -or
    $coverage.resolve_calls -ne $probe.ResolveCalls) { throw 'M5-004 coverage does not match physical evidence.' }
$scope = $record.scope
$scopeExpected = @('target_host_rtv_defect_reproduced','behavior_patch_required','rov_exercised','snapshot_restore_exercised','true_direct_resolve_exercised','pwl_gamma_exercised','owner_visual_rendering_categories_pass','classification')
if (($scope.PSObject.Properties.Name -join ',') -cne ($scopeExpected -join ',') -or $scope.target_host_rtv_defect_reproduced -ne $false -or
    $scope.behavior_patch_required -ne $false -or $scope.rov_exercised -ne $false -or $scope.snapshot_restore_exercised -ne $false -or
    $scope.true_direct_resolve_exercised -ne $false -or $scope.pwl_gamma_exercised -ne $false -or
    $scope.owner_visual_rendering_categories_pass -ne $true -or $scope.classification -cne 'accepted-s2-bounded-host-rtv') { throw 'M5-004 scope overclaims coverage.' }
[pscustomobject]@{ Passed=$true; Decision=$record.decision; OwnershipModes=$probe.OwnershipModes; HostDepthStores=$probe.HostDepthStores; DepthResolves=$probe.DepthResolves; RovExercised=$false; SnapshotRestoreExercised=$false; DataIntegrityVerified=$true }
