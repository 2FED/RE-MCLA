[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$root = Join-Path $repo ('private\evidence\M7-016\deck-perf-test-' + [guid]::NewGuid().ToString('N').Substring(0,8))
$utf8 = [Text.UTF8Encoding]::new($false)

function Write-Utf8([string]$Path, [string]$Text) {
    $parent = Split-Path $Path -Parent
    [IO.Directory]::CreateDirectory($parent) | Out-Null
    [IO.File]::WriteAllText($Path, $Text, $utf8)
}

function Write-Package([string]$Name, [int]$ProcessId, [long]$Outputs, [long]$Vblanks, [bool]$WithAudit, [bool]$WithWait=$false, [long]$WaitScale=1, [string]$Detail='absent', [string]$Resolve='absent', [object]$CreationTime100ns=134330000000000000L) {
    $dir = Join-Path $root "diagnostics\live\$Name"
    [IO.Directory]::CreateDirectory($dir) | Out-Null
    $stamp = [regex]::Match($Name, 'live-(\d{8}T\d{6}Z)-').Groups[1].Value
    $state = [ordered]@{
        schema='mcla-diagnostic-state-v1'
        reason='f10'
        process=[ordered]@{pid=$ProcessId}
        runtime=[ordered]@{available=$true;guest_output_sequence=$Outputs;guest_vblank_sequence=$Vblanks}
        graphics=[ordered]@{
            backend=2;guest_frames_opened=$Outputs;guest_frames_closed=$Outputs;issue_swap_calls=$Outputs
            guest_output_refresh_successes=$Outputs;guest_output_refresh_failures=0
            async_placeholder_skips=0;swap_source_missing=0;submission_count=($Outputs * 2)
            frame_fence_wait_us=($Outputs * 100);pipeline_end_submission_us=($Outputs * 200)
            deferred_execute_us=($Outputs * 300);queue_submit_us=($Outputs * 400)
            guest_frame_wall_us=($Outputs * 5000)
        }
        native_threads=@(
            [ordered]@{id=100;base_priority=8;kernel_time_100ns=($Outputs * 1000);user_time_100ns=($Outputs * 10000);name='GPU Commands'}
            [ordered]@{id=101;base_priority=8;kernel_time_100ns=($Outputs * 500);user_time_100ns=($Outputs * 1000);name='Main XThread'}
        )
    }
    if (-not ($CreationTime100ns -is [string] -and $CreationTime100ns -ceq 'absent')) {
        $state.process.creation_time_100ns = $CreationTime100ns
    }
    if ($WithWait) {
        $state.graphics.command_ring_idle_us = $Outputs * 10000 * $WaitScale
        $state.graphics.command_wait_reg_mem_us = $Outputs * 2000 * $WaitScale
        $state.graphics.memory_non_mmio_write_fault_count = $Outputs * 10 * $WaitScale
        $state.graphics.memory_non_mmio_write_fault_us = $Outputs * 200000 * $WaitScale
    }
    if ($Detail -cne 'absent') {
        $detailFields = @(
            'command_primary_buffer_count',
            'command_primary_buffer_us',
            'command_draw_packet_count',
            'command_draw_packet_us',
            'command_swap_packet_count',
            'command_swap_packet_us',
            'command_interrupt_packet_count',
            'command_interrupt_packet_us',
            'vulkan_fence_call_count',
            'vulkan_fence_call_us',
            'memory_write_fault_lock_wait_us',
            'memory_write_fault_query_us',
            'memory_write_fault_callback_us',
            'memory_write_fault_already_cleared_count',
            'memory_watch_arm_calls',
            'memory_watch_arm_us',
            'memory_watch_arm_lock_wait_us',
            'memory_watch_disarm_calls',
            'memory_watch_disarm_us',
            'memory_watch_protect_failures',
            'memory_watch_arm_gap_splits'
        )
        foreach ($field in $detailFields) {
            $state.graphics[$field] = if ($field.EndsWith('_us')) { $Outputs * 200000 } else { $Outputs * 10 }
        }
        if ($Detail -ceq 'partial') { $state.graphics.Remove('memory_watch_arm_calls') }
        if ($Detail -ceq 'regressed') { $state.graphics.memory_watch_arm_us = 0 }
        if ($Detail -ceq 'negative') { $state.graphics.memory_watch_arm_us = -1 }
    }
    if ($Resolve -cne 'absent') {
        foreach ($field in @('vulkan_resolve_count','vulkan_resolve_us',
            'vulkan_resolve_readback_copy_count','vulkan_resolve_readback_bytes',
            'vulkan_resolve_readback_copy_us','vulkan_resolve_readback_wait_count',
            'vulkan_resolve_readback_wait_us')) {
            $state.graphics[$field] = if ($Resolve -ceq 'zero') { 0L } elseif ($field.EndsWith('_us')) { $Outputs * 200000L } else { $Outputs * 10L }
        }
        if ($Resolve -ceq 'partial') { $state.graphics.Remove('vulkan_resolve_readback_bytes') }
        if ($Resolve -ceq 'regressed') { $state.graphics.vulkan_resolve_readback_copy_us = 0L }
        if ($Resolve -ceq 'negative') { $state.graphics.vulkan_resolve_readback_copy_us = -1L }
    }
    Write-Utf8 (Join-Path $dir 'state.json') ($state | ConvertTo-Json -Depth 8)
    $lines = @('[core] ordinary log')
    if ($WithAudit) {
        $lines += '[core] REX_PERF_AUDIT_CONFIG v=1 enabled=1 backend=d3d12 sample_limit=300 streaming_stall_us=5000'
        $lines += '[core] REX_PERF_AUDIT_TRIGGER v=1 source=application-diagnostics'
        for ($i=0; $i -lt 300; $i++) {
            $hostUs = 1000000 + ($i * 50000)
            $lines += "[core] REX_PERF_AUDIT_SAMPLE v=1 sample=$i host_us=$hostUs guest_frame=$($i+10) cpu_us=12000 gpu_us=18000 stream_reads=1 stream_bytes=4096 stream_stalls=0 stream_max_us=100 audio_underruns=0 shader_count=0 shader_fail=0 shader_us=0 pso_count=0 pso_fail=0 pso_us=0"
        }
        $lines += '[core] REX_PERF_AUDIT_SUMMARY v=1 status=PASS samples=300 stream_reads=300 stream_bytes=1228800 stream_stalls=0 audio_underruns=0 shader_count=0 shader_fail=0 shader_us=0 pso_count=0 pso_fail=0 pso_us=0 marker_overflow=0'
    }
    Write-Utf8 (Join-Path $dir 'log-tail.txt') (($lines -join "`n") + "`n")
    $artifacts = @()
    foreach ($file in @('state.json','log-tail.txt')) {
        $item = Get-Item -LiteralPath (Join-Path $dir $file)
        $artifacts += [ordered]@{name=$file;bytes=$item.Length;sha256=(Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash;safe_to_share=$false}
    }
    $manifest = [ordered]@{schema='mcla-diagnostic-package-v1';kind='live';reason='f10';created_utc=$stamp;artifacts=$artifacts}
    Write-Utf8 (Join-Path $dir 'manifest.json') ($manifest | ConvertTo-Json -Depth 5)
    return $dir
}

try {
    [IO.Directory]::CreateDirectory($root) | Out-Null
    Write-Utf8 (Join-Path $root 'bundle-manifest.json') (([ordered]@{schema='mcla-portable-campaign-bundle-v1';task='M7-016';bundle_id='fixture';project_version='0.9.1.0';sdk_version='0.10.1.0'} | ConvertTo-Json))
    $first = Write-Package 'live-20260904T120000Z-10-1' 10 100 600 $false
    $second = Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $true
    $result = & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root
    if (-not $result.Passed -or -not $result.ProcessLifetimeVerified -or
        $result.ProcessCreationTime100ns -ne 134330000000000000L -or
        $result.GuestOutputRateHz -ne 20 -or $result.GuestVblankRateHz -ne 60 -or
        $result.Audit.Samples -ne 300 -or $result.Audit.SampleRateHz -ne 20 -or
        -not $result.AuditAvailable -or
        $result.Graphics.GuestFramesClosed -ne 600 -or
        $result.Graphics.DominantMeasuredStage -cne 'unattributed_frame_wall' -or
        $result.ThreadCpuTop[0].Name -cne 'GPU Commands' -or
        $result.Audit.Classification -cne 'outside-recorded-submission-span') { throw 'Positive Deck performance analysis fixture did not produce the expected result.' }

    Remove-Item -LiteralPath $second -Recurse -Force
    $second = Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false
    $nativeResult = & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root
    if (-not $nativeResult.Passed -or $nativeResult.AuditAvailable -or $null -ne $nativeResult.Audit -or
        $nativeResult.GuestOutputRateHz -ne 20 -or $null -eq $nativeResult.Graphics -or
        $nativeResult.ThreadCpuTop.Count -ne 2 -or
        $null -ne $nativeResult.Graphics.WaitDiagnostics -or
        $null -ne $nativeResult.Graphics.DetailDiagnostics -or
        $null -ne $nativeResult.Graphics.ResolveDiagnostics) { throw 'Audit-optional Vulkan fixture did not produce the expected result.' }

    $negative = 0
    [void](Write-Package 'live-20260904T120000Z-10-1' 10 100 600 $false $true)
    try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root | Out-Null; throw 'Incomplete wait fixture unexpectedly passed.' }
    catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true)
    $waitResult = & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root
    if ($waitResult.Graphics.WaitDiagnostics.CommandRingIdleSeconds -ne 6 -or
        $waitResult.Graphics.WaitDiagnostics.CommandWaitRegMemSeconds -ne 1.2 -or
        $waitResult.Graphics.WaitDiagnostics.NonMmioWriteFaults -ne 6000 -or
        $waitResult.Graphics.WaitDiagnostics.NonMmioWriteFaultAggregateSeconds -ne 120 -or
        $waitResult.Graphics.WaitDiagnostics.NonMmioWriteFaultsPerSecond -ne 200 -or
        $waitResult.Graphics.DominantMeasuredStage -cne 'unattributed_frame_wall') {
        throw 'Wait fixture failed or overlapping thread time was treated as a frame stage.'
    }
    [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true 0)
    try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root | Out-Null; throw 'Regressing wait fixture unexpectedly passed.' }
    catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true)
    [void](Write-Package 'live-20260904T120000Z-10-1' 10 100 600 $false $true 1 'complete')
    try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root | Out-Null; throw 'One-sided detail fixture unexpectedly passed.' }
    catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true 1 'complete')
    $detailResult = & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root
    $raw = $detailResult.Graphics.DetailDiagnostics.RawDelta
    if ($raw.command_primary_buffer_count -ne 6000 -or $raw.command_draw_packet_us -ne 120000000 -or
        $raw.vulkan_fence_call_us -ne 120000000 -or $raw.memory_watch_arm_us -ne 120000000 -or
        $raw.memory_write_fault_lock_wait_us -ne 120000000 -or
        $detailResult.Graphics.DominantMeasuredStage -cne 'unattributed_frame_wall') {
        throw 'Detailed fixture failed or nested/aggregate spans were subtracted from frame wall.'
    }
    foreach ($mode in @('partial','regressed','negative')) {
        [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true 1 $mode)
        try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root | Out-Null; throw "$mode detail fixture unexpectedly passed." }
        catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    }
    [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true 1 'complete')
    [void](Write-Package 'live-20260904T120000Z-10-1' 10 100 600 $false $true 1 'complete' 'complete')
    try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root | Out-Null; throw 'One-sided resolve fixture unexpectedly passed.' }
    catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true 1 'complete' 'complete')
    $resolveResult = & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root
    $resolve = $resolveResult.Graphics.ResolveDiagnostics
    if (-not $resolve.BackendApplicable -or $resolve.Calls -ne 6000 -or
        $resolve.ReadbackCopies -ne 6000 -or $resolve.ReadbackBytes -ne 6000 -or
        $resolve.InclusiveSeconds -ne 120 -or $resolve.ReadbackCopySeconds -ne 120 -or
        $resolve.ReadbackWaitSeconds -ne 120 -or $resolve.ReadbackWaits -ne 6000 -or
        $resolveResult.Graphics.DominantMeasuredStage -cne 'unattributed_frame_wall') { throw 'Resolve deltas failed or nested totals were subtracted from frame wall.' }
    foreach ($mode in @('partial','regressed','negative')) {
        [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true 1 'complete' $mode)
        try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root | Out-Null; throw "$mode resolve fixture unexpectedly passed." }
        catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    }
    [void](Write-Package 'live-20260904T120000Z-10-1' 10 100 600 $false $true 1 'complete' 'zero')
    [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true 1 'complete' 'zero')
    $zeroResult = & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root
    if ($null -eq $zeroResult.Graphics.ResolveDiagnostics -or $zeroResult.Graphics.ResolveDiagnostics.ReadbackBytes -ne 0) { throw 'Valid all-zero resolve counters were rejected or treated as absent.' }
    $explicit = & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root -FirstF10 'live-20260904T120000Z-10-1' -SecondF10 'live-20260904T120030Z-10-2' -AllowLegacyPidOnly
    if ($explicit.GuestOutputRateHz -ne 20 -or $explicit.FirstF10 -cne 'live-20260904T120000Z-10-1' -or
        -not $explicit.ProcessLifetimeVerified) { throw 'Explicit pair selection failed or legacy opt-in weakened a verified lifetime.' }
    $later = Write-Package 'live-20260904T120050Z-10-3' 10 900 3600 $false $true 1 'complete' 'zero'
    $partial = Write-Package 'live-20260904T120055Z-10-4.partial' 10 1100 3900 $false $true 1 'complete' 'zero'
    $latest = & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root
    $selected = & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root -FirstF10 'live-20260904T120000Z-10-1' -SecondF10 'live-20260904T120030Z-10-2'
    if ($latest.SecondF10 -cne 'live-20260904T120050Z-10-3' -or $latest.GuestOutputRateHz -ne 10 -or
        $selected.SecondF10 -cne 'live-20260904T120030Z-10-2' -or $selected.GuestOutputRateHz -ne 20) { throw 'Historical explicit pair or complete-only default selection drifted.' }
    Remove-Item -LiteralPath $later,$partial -Recurse -Force
    # A reused PID with larger counters must not look like the same process.
    [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true 1 'complete' 'zero' 134330000100000000L)
    foreach ($legacySwitch in @($false, $true)) {
        try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root -AllowLegacyPidOnly:$legacySwitch | Out-Null; throw 'Reused PID default fixture unexpectedly passed.' }
        catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
        try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root -FirstF10 'live-20260904T120000Z-10-1' -SecondF10 'live-20260904T120030Z-10-2' -AllowLegacyPidOnly:$legacySwitch | Out-Null; throw 'Reused PID explicit fixture unexpectedly passed.' }
        catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    }
    [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true 1 'complete' 'zero')
    $restartedFirst = Write-Package 'live-20260904T120050Z-10-1' 10 900 3600 $false $true 1 'complete' 'zero' 134330000100000000L
    $fallback = & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root
    if (-not $fallback.ProcessLifetimeVerified -or $fallback.SecondF10 -cne 'live-20260904T120030Z-10-2') { throw 'Default selection crossed a reused PID instead of choosing the last valid pair.' }
    $restartedSecond = Write-Package 'live-20260904T120110Z-10-2' 10 1100 4800 $false $true 1 'complete' 'zero' 134330000100000000L
    $newLifetime = & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root
    if (-not $newLifetime.ProcessLifetimeVerified -or $newLifetime.FirstF10 -cne 'live-20260904T120050Z-10-1' -or
        $newLifetime.SecondF10 -cne 'live-20260904T120110Z-10-2' -or
        $newLifetime.ProcessCreationTime100ns -ne 134330000100000000L) { throw 'Default selection did not choose the latest valid process lifetime.' }
    Remove-Item -LiteralPath $restartedFirst,$restartedSecond -Recurse -Force
    # A one-sided creation field is not a legacy pair, even with the override.
    foreach ($missingFirst in @($true, $false)) {
        $firstCreation = if ($missingFirst) { 'absent' } else { 134330000000000000L }
        $secondCreation = if ($missingFirst) { 134330000000000000L } else { 'absent' }
        [void](Write-Package 'live-20260904T120000Z-10-1' 10 100 600 $false $true 1 'complete' 'zero' $firstCreation)
        [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true 1 'complete' 'zero' $secondCreation)
        try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root | Out-Null; throw 'Partial process lifetime default fixture unexpectedly passed.' }
        catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
        try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root -FirstF10 'live-20260904T120000Z-10-1' -SecondF10 'live-20260904T120030Z-10-2' -AllowLegacyPidOnly | Out-Null; throw 'Partial process lifetime explicit fixture unexpectedly passed.' }
        catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    }
    [void](Write-Package 'live-20260904T120000Z-10-1' 10 100 600 $false $true 1 'complete' 'zero')
    foreach ($invalidCreation in @(
        @{Value=0L}, @{Value=-1L}, @{Value=$null}, @{Value=1.5},
        @{Value='invalid'}, @{Value='134330000000000000'}, @{Value=$true}, @{Value=@(1,2)})) {
        [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true 1 'complete' 'zero' $invalidCreation.Value)
        try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root -AllowLegacyPidOnly | Out-Null; throw 'Invalid present creation time unexpectedly passed.' }
        catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    }
    [void](Write-Package 'live-20260904T120000Z-10-1' 10 100 600 $false $true 1 'complete' 'zero' 'absent')
    [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true 1 'complete' 'zero' 'absent')
    try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root | Out-Null; throw 'Legacy default without opt-in unexpectedly passed.' }
    catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root -FirstF10 'live-20260904T120000Z-10-1' -SecondF10 'live-20260904T120030Z-10-2' | Out-Null; throw 'Legacy explicit without opt-in unexpectedly passed.' }
    catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    $legacyDefault = & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root -AllowLegacyPidOnly
    $legacyExplicit = & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root -FirstF10 'live-20260904T120000Z-10-1' -SecondF10 'live-20260904T120030Z-10-2' -AllowLegacyPidOnly
    foreach ($legacyResult in @($legacyDefault,$legacyExplicit)) {
        if (-not $legacyResult.Passed -or $legacyResult.ProcessLifetimeVerified -or
            $null -ne $legacyResult.ProcessCreationTime100ns -or $legacyResult.GuestOutputRateHz -ne 20 -or
            $legacyResult.ProcessLifetimeEvidenceScope -notlike '*independently-verified-session-required*') { throw 'Legacy opt-in did not remain explicitly unverified.' }
    }
    [void](Write-Package 'live-20260904T120000Z-10-1' 10 100 600 $false $true 1 'complete' 'zero')
    [void](Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $false $true 1 'complete' 'zero')
    foreach ($selection in @(
        @{FirstF10='live-20260904T120000Z-10-1'},
        @{FirstF10='../live-20260904T120000Z-10-1';SecondF10='live-20260904T120030Z-10-2'},
        @{FirstF10='live-20260904T120000Z-10-1';SecondF10='live-20260904T120030Z-10-2.partial'},
        @{FirstF10='live-20260904T120000Z-10-1';SecondF10='live-20260904T120035Z-10-3'},
        @{FirstF10='live-20260904T120030Z-10-2';SecondF10='live-20260904T120000Z-10-1'},
        @{FirstF10='live-20260904T120000Z-10-1';SecondF10='live-20260904T120000Z-10-1'})) {
        try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root @selection | Out-Null; throw 'Invalid explicit pair unexpectedly passed.' }
        catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    }
    Add-Content -LiteralPath (Join-Path $first 'state.json') -Value 'tamper'
    try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root | Out-Null; throw 'Tampered state fixture unexpectedly passed.' }
    catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    Remove-Item -LiteralPath $first -Recurse -Force
    [void](Write-Package 'live-20260904T120000Z-11-1' 11 100 600 $false)
    try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root | Out-Null; throw 'Mixed-process fixture unexpectedly passed.' }
    catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root -FirstF10 'live-20260904T120000Z-11-1' -SecondF10 'live-20260904T120030Z-10-2' | Out-Null; throw 'Explicit mixed-process fixture unexpectedly passed.' }
    catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    [pscustomobject]@{Passed=$true;PositiveFixtures=12;FailClosedNegatives=$negative}
} finally {
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
