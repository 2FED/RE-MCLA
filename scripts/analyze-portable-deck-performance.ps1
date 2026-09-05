[CmdletBinding()]
param(
    [string]$BundlePath,
    [string]$FirstF10,
    [string]$SecondF10,
    [switch]$AllowLegacyPidOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path

function Get-Percentile([long[]]$Values, [double]$Percentile) {
    if (-not $Values -or $Values.Count -eq 0) { throw 'Cannot calculate a percentile for an empty set.' }
    $sorted = @($Values | Sort-Object)
    $index = [Math]::Min($sorted.Count - 1, [Math]::Floor(($sorted.Count - 1) * $Percentile))
    return [long]$sorted[$index]
}

function Get-OptionalProperty([object]$Object, [string]$Name) {
    if ($null -eq $Object) { return $null }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) { return $null }
    return $property.Value
}

function Test-F10ProcessLifetime([object]$First, [object]$Second, [bool]$AllowLegacy) {
    if ($First.Pid -ne $Second.Pid) { return $false }
    if ($null -eq $First.CreationTime100ns -and $null -eq $Second.CreationTime100ns) {
        return $AllowLegacy
    }
    if ($null -eq $First.CreationTime100ns -or $null -eq $Second.CreationTime100ns) {
        return $false
    }
    return $First.CreationTime100ns -eq $Second.CreationTime100ns
}

function Measure-OptionalCounterSet([object]$First, [object]$Second, [string[]]$Names, [string]$Label) {
    $delta = [ordered]@{}
    foreach ($name in $Names) {
        $beforeProperty = $First.PSObject.Properties[$name]
        $afterProperty = $Second.PSObject.Properties[$name]
        if ($null -eq $beforeProperty -and $null -eq $afterProperty) { continue }
        if ($null -eq $beforeProperty -or $null -eq $afterProperty) { throw "Incomplete $Label diagnostic '$name'." }
        $before = $beforeProperty.Value
        $after = $afterProperty.Value
        # Only absent fields are legacy. JSON null, booleans, strings and
        # fractional/oversized numbers must not silently coerce to counters.
        foreach ($value in @($before, $after)) {
            if (-not ($value -is [int] -or $value -is [long])) { throw "Malformed $Label diagnostic '$name': integer required." }
        }
        if ([long]$before -lt 0 -or [long]$after -lt [long]$before) { throw "$Label diagnostic '$name' regressed or is negative." }
        $delta[$name] = [long]$after - [long]$before
    }
    if ($delta.Count -eq 0) { return $null }
    if ($delta.Count -ne $Names.Count) { throw "Incomplete $Label diagnostic counter set." }
    return [pscustomobject]$delta
}

function Measure-GraphicsDelta([object]$FirstState, [object]$SecondState, [double]$ElapsedSeconds) {
    $firstGraphics = Get-OptionalProperty $FirstState 'graphics'
    $secondGraphics = Get-OptionalProperty $SecondState 'graphics'
    if ($null -eq $firstGraphics -or $null -eq $secondGraphics) { return $null }
    $backendFirst = [long](Get-OptionalProperty $firstGraphics 'backend')
    $backendSecond = [long](Get-OptionalProperty $secondGraphics 'backend')
    if ($backendFirst -le 0 -or $backendFirst -ne $backendSecond) {
        throw 'Graphics diagnostic backend changed or is unavailable across the selected F10 pair.'
    }
    $names = @(
        'guest_frames_opened', 'guest_frames_closed', 'issue_swap_calls',
        'guest_output_refresh_successes', 'guest_output_refresh_failures',
        'async_placeholder_skips', 'swap_source_missing', 'submission_count',
        'frame_fence_wait_us', 'pipeline_end_submission_us',
        'deferred_execute_us', 'queue_submit_us', 'guest_frame_wall_us'
    )
    $delta = [ordered]@{}
    foreach ($name in $names) {
        $firstValue = Get-OptionalProperty $firstGraphics $name
        $secondValue = Get-OptionalProperty $secondGraphics $name
        if ($null -eq $firstValue -or $null -eq $secondValue) {
            throw "Graphics diagnostic counter '$name' is missing."
        }
        $difference = [long]$secondValue - [long]$firstValue
        if ($difference -lt 0) { throw "Graphics diagnostic counter '$name' regressed." }
        $delta[$name] = $difference
    }
    $opened = [Math]::Max(1L, [long]$delta.guest_frames_opened)
    # Optional on older packages, but partially present or regressing counters
    # are not trustworthy. These spans overlap frame timing and memory spans
    # aggregate multiple threads: never subtract them from guest frame wall.
    $waitNames = @('command_ring_idle_us', 'command_wait_reg_mem_us',
                  'memory_non_mmio_write_fault_count', 'memory_non_mmio_write_fault_us')
    $waitDelta = [ordered]@{}
    foreach ($name in $waitNames) {
        $before = Get-OptionalProperty $firstGraphics $name
        $after = Get-OptionalProperty $secondGraphics $name
        if ($null -eq $before -and $null -eq $after) { continue }
        if ($null -eq $before -or $null -eq $after) { throw "Incomplete wait diagnostic '$name'." }
        $difference = [long]$after - [long]$before
        if ($difference -lt 0) { throw "Wait diagnostic '$name' regressed." }
        $waitDelta[$name] = $difference
    }
    if ($waitDelta.Count -ne 0 -and $waitDelta.Count -ne $waitNames.Count) {
        throw 'Incomplete wait diagnostic counter set.'
    }
    $waitDiagnostics = $null
    if ($waitDelta.Count) {
        if ($ElapsedSeconds -le 0) { throw 'Wait diagnostics require a positive interval.' }
        $waitDiagnostics = [pscustomobject]@{
            CommandRingIdleSeconds = [Math]::Round($waitDelta.command_ring_idle_us / 1000000.0, 6)
            CommandWaitRegMemSeconds = [Math]::Round($waitDelta.command_wait_reg_mem_us / 1000000.0, 6)
            NonMmioWriteFaults = [long]$waitDelta.memory_non_mmio_write_fault_count
            NonMmioWriteFaultAggregateSeconds = [Math]::Round($waitDelta.memory_non_mmio_write_fault_us / 1000000.0, 6)
            NonMmioWriteFaultsPerSecond = [Math]::Round($waitDelta.memory_non_mmio_write_fault_count / $ElapsedSeconds, 2)
            RawDelta = [pscustomobject]$waitDelta
            Scope = 'completed-wait-episodes; fault-time-aggregates-threads-and-excludes-OS-exception-delivery; not-additive-frame-stages'
        }
    }
    # Added together as one versioned group. A partial set is not old evidence.
    # Primary-buffer, packet and fence spans nest; memory spans aggregate threads.
    # Keep their raw deltas separate, never infer an exclusive GPU/CPU remainder.
    $detailNames = @(
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
    $detailDelta = Measure-OptionalCounterSet $firstGraphics $secondGraphics $detailNames 'command/memory'
    $detailDiagnostics = if ($null -ne $detailDelta) {
        [pscustomobject]@{
            RawDelta = $detailDelta
            Scope = 'completed-primary-and-packet-spans-inclusive-and-nested; native-fence-call-wall-only; memory-aggregates-threads; not-additive'
        }
    } else { $null }
    # Resolve/copy/wait times overlap each other and the existing draw/frame
    # spans. They answer attribution questions, not an exclusive remainder.
    $resolveNames = @('vulkan_resolve_count', 'vulkan_resolve_us',
        'vulkan_resolve_readback_copy_count', 'vulkan_resolve_readback_bytes',
        'vulkan_resolve_readback_copy_us', 'vulkan_resolve_readback_wait_count',
        'vulkan_resolve_readback_wait_us')
    $resolveDelta = Measure-OptionalCounterSet $firstGraphics $secondGraphics $resolveNames 'Vulkan resolve'
    $resolveDiagnostics = if ($null -ne $resolveDelta) {
        [pscustomobject]@{
            BackendApplicable = ($backendSecond -eq 2)
            Calls = [long]$resolveDelta.vulkan_resolve_count
            InclusiveSeconds = [Math]::Round($resolveDelta.vulkan_resolve_us / 1000000.0, 6)
            ReadbackCopies = [long]$resolveDelta.vulkan_resolve_readback_copy_count
            ReadbackBytes = [long]$resolveDelta.vulkan_resolve_readback_bytes
            ReadbackMiBPerSecond = [Math]::Round($resolveDelta.vulkan_resolve_readback_bytes / 1048576.0 / $ElapsedSeconds, 3)
            ReadbackCopySeconds = [Math]::Round($resolveDelta.vulkan_resolve_readback_copy_us / 1000000.0, 6)
            ReadbackWaits = [long]$resolveDelta.vulkan_resolve_readback_wait_count
            ReadbackWaitSeconds = [Math]::Round($resolveDelta.vulkan_resolve_readback_wait_us / 1000000.0, 6)
            RawDelta = $resolveDelta
            Scope = 'completed-IssueCopy-inclusive; actual-memcpy-including-faults; explicit-resolve-waits-cold-full-resource-resize; nontransactional-nested-not-additive'
        }
    } else { $null }
    $closed = [Math]::Max(1L, [long]$delta.guest_frames_closed)
    $bindingNames = @('vulkan_binding_update_count','vulkan_binding_update_us',
        'vulkan_texture_descriptor_reuse_count','vulkan_texture_descriptor_write_count',
        'vulkan_descriptor_update_count','vulkan_descriptor_update_us')
    $bindingDelta = Measure-OptionalCounterSet $firstGraphics $secondGraphics $bindingNames 'Vulkan bindings'
    $bindingDiagnostics = if ($null -ne $bindingDelta) {
        $stageSets = [double]$bindingDelta.vulkan_texture_descriptor_reuse_count +
            [double]$bindingDelta.vulkan_texture_descriptor_write_count
        [pscustomobject]@{
            BackendApplicable = ($backendSecond -eq 2)
            Calls = [long]$bindingDelta.vulkan_binding_update_count
            InclusiveSeconds = [Math]::Round($bindingDelta.vulkan_binding_update_us / 1000000.0, 6)
            ReusedStageSets = [long]$bindingDelta.vulkan_texture_descriptor_reuse_count
            WrittenStageSets = [long]$bindingDelta.vulkan_texture_descriptor_write_count
            ReusePercent = if ($stageSets -gt 0) { [Math]::Round(100.0 * $bindingDelta.vulkan_texture_descriptor_reuse_count / $stageSets, 2) } else { $null }
            DriverUpdateCalls = [long]$bindingDelta.vulkan_descriptor_update_count
            DriverUpdateSeconds = [Math]::Round($bindingDelta.vulkan_descriptor_update_us / 1000000.0, 6)
            RawDelta = $bindingDelta
            Scope = 'completed-UpdateBindings-inclusive; successful-texture-stage-set-reuse-or-write; vkUpdateDescriptorSets-includes-constants; nontransactional-nested-not-additive'
        }
    } else { $null }
    $submissions = [Math]::Max(1L, [long]$delta.submission_count)
    $trackedSubmissionUs = [long]$delta.pipeline_end_submission_us +
        [long]$delta.deferred_execute_us + [long]$delta.queue_submit_us
    $unattributedFrameUs = [Math]::Max(0L, [long]$delta.guest_frame_wall_us - $trackedSubmissionUs)
    $stages = [ordered]@{
        frame_fence_wait = [long]$delta.frame_fence_wait_us
        pipeline_end = [long]$delta.pipeline_end_submission_us
        deferred_execute = [long]$delta.deferred_execute_us
        queue_submit = [long]$delta.queue_submit_us
        unattributed_frame_wall = $unattributedFrameUs
    }
    $dominantStage = ($stages.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 1).Key
    return [pscustomobject]@{
        Backend = $backendSecond
        GuestFramesOpened = [long]$delta.guest_frames_opened
        GuestFramesClosed = [long]$delta.guest_frames_closed
        IssueSwapCalls = [long]$delta.issue_swap_calls
        RefreshSuccesses = [long]$delta.guest_output_refresh_successes
        RefreshFailures = [long]$delta.guest_output_refresh_failures
        AsyncPlaceholderSkips = [long]$delta.async_placeholder_skips
        SwapSourceMissing = [long]$delta.swap_source_missing
        Submissions = [long]$delta.submission_count
        FenceWaitMsPerOpenedFrame = [Math]::Round([long]$delta.frame_fence_wait_us / $opened / 1000.0, 3)
        PipelineEndMsPerSubmission = [Math]::Round([long]$delta.pipeline_end_submission_us / $submissions / 1000.0, 3)
        DeferredExecuteMsPerSubmission = [Math]::Round([long]$delta.deferred_execute_us / $submissions / 1000.0, 3)
        QueueSubmitMsPerSubmission = [Math]::Round([long]$delta.queue_submit_us / $submissions / 1000.0, 3)
        GuestFrameWallMsPerClosedFrame = [Math]::Round([long]$delta.guest_frame_wall_us / $closed / 1000.0, 3)
        TrackedSubmissionPercentOfFrameWall = if ([long]$delta.guest_frame_wall_us) {
            [Math]::Round($trackedSubmissionUs * 100.0 / [long]$delta.guest_frame_wall_us, 2)
        } else { 0.0 }
        DominantMeasuredStage = [string]$dominantStage
        RawDelta = [pscustomobject]$delta
        WaitDiagnostics = $waitDiagnostics
        DetailDiagnostics = $detailDiagnostics
        ResolveDiagnostics = $resolveDiagnostics
        BindingDiagnostics = $bindingDiagnostics
    }
}

function Measure-ThreadCpuDelta([object]$FirstState, [object]$SecondState,
                                [double]$ElapsedSeconds) {
    $firstValue = Get-OptionalProperty $FirstState 'native_threads'
    $secondValue = Get-OptionalProperty $SecondState 'native_threads'
    if ($null -eq $firstValue -or $null -eq $secondValue) { return @() }
    $firstById = @{}
    foreach ($thread in @($firstValue)) { $firstById[[string][long]$thread.id] = $thread }
    $rows = @()
    foreach ($thread in @($secondValue)) {
        $key = [string][long]$thread.id
        if (-not $firstById.ContainsKey($key)) { continue }
        $before = $firstById[$key]
        $kernelTicks = [long]$thread.kernel_time_100ns - [long]$before.kernel_time_100ns
        $userTicks = [long]$thread.user_time_100ns - [long]$before.user_time_100ns
        if ($kernelTicks -lt 0 -or $userTicks -lt 0) { continue }
        $totalTicks = $kernelTicks + $userTicks
        $rows += [pscustomobject]@{
            ThreadId = [long]$thread.id
            Name = [string]$thread.name
            UserMs = [Math]::Round($userTicks / 10000.0, 2)
            KernelMs = [Math]::Round($kernelTicks / 10000.0, 2)
            TotalMs = [Math]::Round($totalTicks / 10000.0, 2)
            SingleCorePercent = [Math]::Round($totalTicks * 100.0 / ($ElapsedSeconds * 10000000.0), 2)
        }
    }
    return @($rows | Sort-Object TotalMs -Descending | Select-Object -First 12)
}

function Assert-PlainDirectory([string]$Path, [string]$Label) {
    $item = Get-Item -LiteralPath $Path -Force
    if (-not $item.PSIsContainer) { throw "$Label is not a directory." }
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "$Label is a reparse point." }
    return $item.FullName
}

function Assert-Artifact([string]$PackagePath, [object]$Manifest, [string]$Name) {
    $records = @($Manifest.artifacts | Where-Object { $_.name -ceq $Name })
    if ($records.Count -ne 1) { throw "Diagnostic package does not declare exactly one $Name artifact." }
    $path = Join-Path $PackagePath $Name
    $item = Get-Item -LiteralPath $path -Force
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "$Name is a reparse point." }
    if ([long]$records[0].bytes -ne $item.Length) { throw "$Name length does not match its diagnostic manifest." }
    $hash = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
    if ($records[0].sha256 -cne $hash) { throw "$Name hash does not match its diagnostic manifest." }
    return $item.FullName
}

function Read-F10Package([IO.DirectoryInfo]$Directory) {
    $manifestPath = Join-Path $Directory.FullName 'manifest.json'
    $statePath = Join-Path $Directory.FullName 'state.json'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf) -or
        -not (Test-Path -LiteralPath $statePath -PathType Leaf)) { return $null }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.schema -cne 'mcla-diagnostic-package-v1' -or
        $manifest.kind -cne 'live' -or $manifest.reason -cne 'f10') { return $null }
    $verifiedStatePath = Assert-Artifact $Directory.FullName $manifest 'state.json'
    [void](Assert-Artifact $Directory.FullName $manifest 'log-tail.txt')
    $state = Get-Content -LiteralPath $verifiedStatePath -Raw | ConvertFrom-Json
    if ($state.schema -cne 'mcla-diagnostic-state-v1' -or $state.reason -cne 'f10' -or
        -not [bool]$state.runtime.available) { throw 'F10 state identity or runtime availability is invalid.' }
    # A reused PID is not a process lifetime. Preserve absence only for explicit
    # legacy opt-in; present null, zero, strings, fractions and invalid types are
    # malformed rather than historical evidence. Native FILETIME creation ticks
    # fit in signed 64-bit integers for the supported Windows date range.
    $creationTime100ns = $null
    $creationProperty = $state.process.PSObject.Properties['creation_time_100ns']
    if ($null -ne $creationProperty) {
        $creationValue = $creationProperty.Value
        if (($creationValue -isnot [long] -and $creationValue -isnot [int]) -or $creationValue -le 0) {
            throw 'F10 process creation_time_100ns must be a positive integer when present.'
        }
        $creationTime100ns = [long]$creationValue
    }
    $styles = [Globalization.DateTimeStyles]::AssumeUniversal -bor [Globalization.DateTimeStyles]::AdjustToUniversal
    $captured = [DateTime]::ParseExact([string]$manifest.created_utc, "yyyyMMdd'T'HHmmss'Z'",
        [Globalization.CultureInfo]::InvariantCulture, $styles)
    return [pscustomobject]@{
        Directory = $Directory.FullName
        CapturedUtc = $captured
        Pid = [long]$state.process.pid
        CreationTime100ns = $creationTime100ns
        GuestOutputs = [long]$state.runtime.guest_output_sequence
        GuestVblanks = [long]$state.runtime.guest_vblank_sequence
        State = $state
    }
}

function Parse-TriggeredAudit([string]$LogTailPath) {
    $lines = @(Get-Content -LiteralPath $LogTailPath)
    $triggerIndex = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match 'REX_PERF_AUDIT_TRIGGER v=1 source=application-diagnostics') {
            $triggerIndex = $i
        }
    }
    if ($triggerIndex -lt 0) { throw 'The second F10 package has no application-triggered performance audit.' }
    $scope = @($lines[$triggerIndex..($lines.Count - 1)])
    $samples = @($scope | Where-Object { $_ -match 'REX_PERF_AUDIT_SAMPLE v=1' })
    $summaries = @($scope | Where-Object { $_ -match 'REX_PERF_AUDIT_SUMMARY v=1' })
    if ($samples.Count -ne 300) { throw "Expected 300 triggered frame samples; found $($samples.Count)." }
    if ($summaries.Count -ne 1) { throw 'Expected exactly one triggered performance summary.' }

    $samplePattern = 'REX_PERF_AUDIT_SAMPLE v=1 sample=(?<sample>\d+) host_us=(?<host>\d+) guest_frame=(?<frame>\d+) cpu_us=(?<cpu>\d+) gpu_us=(?<gpu>\d+) stream_reads=(?<reads>\d+) stream_bytes=(?<bytes>\d+) stream_stalls=(?<stalls>\d+) stream_max_us=(?<maxread>\d+) audio_underruns=(?<audio>\d+) shader_count=(?<shader>\d+) shader_fail=(?<shaderfail>\d+) shader_us=(?<shaderus>\d+) pso_count=(?<pso>\d+) pso_fail=(?<psofail>\d+) pso_us=(?<psous>\d+)'
    $parsed = @()
    $lastHost = -1L
    $lastFrame = -1L
    foreach ($i in 0..($samples.Count - 1)) {
        if ($samples[$i] -notmatch $samplePattern) { throw "Malformed triggered frame sample at index $i." }
        $row = [ordered]@{}
        foreach ($name in @('sample','host','frame','cpu','gpu','reads','bytes','stalls','maxread','audio','shader','shaderfail','shaderus','pso','psofail','psous')) {
            $row[$name] = [long]$Matches[$name]
        }
        if ($row.sample -ne $i -or $row.host -le $lastHost -or $row.frame -le $lastFrame -or
            $row.cpu -le 0 -or $row.gpu -le 0 -or $row.shaderfail -gt $row.shader -or
            $row.psofail -gt $row.pso) { throw "Invalid or non-monotonic triggered frame sample at index $i." }
        $lastHost = $row.host
        $lastFrame = $row.frame
        $parsed += [pscustomobject]$row
    }

    $summaryPattern = 'REX_PERF_AUDIT_SUMMARY v=1 status=PASS samples=300 .*shader_fail=(?<shaderfail>\d+) .*pso_fail=(?<psofail>\d+) .*marker_overflow=(?<overflow>\d+)'
    if ($summaries[0] -notmatch $summaryPattern -or [long]$Matches.shaderfail -ne 0 -or
        [long]$Matches.psofail -ne 0 -or [long]$Matches.overflow -ne 0) {
        throw 'Triggered performance summary reports failure, compilation errors, or marker overflow.'
    }

    $intervals = @()
    for ($i = 1; $i -lt $parsed.Count; $i++) { $intervals += [long]($parsed[$i].host - $parsed[$i - 1].host) }
    $windowUs = [long]($parsed[-1].host - $parsed[0].host)
    if ($windowUs -le 0) { throw 'Triggered performance window duration is invalid.' }
    $cpuP95 = Get-Percentile @($parsed.cpu) 0.95
    $gpuP95 = Get-Percentile @($parsed.gpu) 0.95
    $intervalP50 = Get-Percentile @($intervals) 0.50
    $classification = if ($gpuP95 -ge [long]($intervalP50 * 0.8) -or
        $cpuP95 -ge [long]($intervalP50 * 0.8)) {
        'long-submission-span-observed'
    } elseif ($gpuP95 -lt [long]($intervalP50 * 0.5) -and $cpuP95 -lt [long]($intervalP50 * 0.5)) {
        'outside-recorded-submission-span'
    } else {
        'submission-span-inconclusive'
    }
    return [pscustomobject]@{
        Samples = $parsed.Count
        SampleRateHz = [Math]::Round((($parsed.Count - 1) * 1000000.0) / $windowUs, 2)
        FrameIntervalP50Ms = [Math]::Round($intervalP50 / 1000.0, 2)
        FrameIntervalP95Ms = [Math]::Round((Get-Percentile @($intervals) 0.95) / 1000.0, 2)
        FrameIntervalMaxMs = [Math]::Round((($intervals | Measure-Object -Maximum).Maximum) / 1000.0, 2)
        CpuP50Ms = [Math]::Round((Get-Percentile @($parsed.cpu) 0.50) / 1000.0, 2)
        CpuP95Ms = [Math]::Round($cpuP95 / 1000.0, 2)
        CpuMaxMs = [Math]::Round((($parsed.cpu | Measure-Object -Maximum).Maximum) / 1000.0, 2)
        GpuP50Ms = [Math]::Round((Get-Percentile @($parsed.gpu) 0.50) / 1000.0, 2)
        GpuP95Ms = [Math]::Round($gpuP95 / 1000.0, 2)
        GpuMaxMs = [Math]::Round((($parsed.gpu | Measure-Object -Maximum).Maximum) / 1000.0, 2)
        StreamReads = [long](($parsed.reads | Measure-Object -Sum).Sum)
        StreamBytes = [long](($parsed.bytes | Measure-Object -Sum).Sum)
        StreamStalls = [long](($parsed.stalls | Measure-Object -Sum).Sum)
        AudioUnderruns = [long](($parsed.audio | Measure-Object -Sum).Sum)
        ShaderTranslations = [long](($parsed.shader | Measure-Object -Sum).Sum)
        PipelineCreations = [long](($parsed.pso | Measure-Object -Sum).Sum)
        TimingSemantics = 'wall-clock submission spans; includes queue idle and is not pure CPU/GPU active work'
        Classification = $classification
    }
}

if (-not $BundlePath) {
    $bundleRoot = Join-Path $repoRoot 'private\bundles\M7-016'
    $currentPath = Join-Path $bundleRoot 'current.txt'
    $bundleId = (Get-Content -LiteralPath $currentPath -Raw).Trim()
    if (-not $bundleId -or $bundleId.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0) {
        throw 'M7-016 current.txt does not contain a valid bundle child name.'
    }
    $BundlePath = Join-Path $bundleRoot $bundleId
}
$bundleFull = Assert-PlainDirectory ([IO.Path]::GetFullPath($BundlePath)) 'Bundle root'
$bundleManifest = Get-Content -LiteralPath (Join-Path $bundleFull 'bundle-manifest.json') -Raw | ConvertFrom-Json
if ($bundleManifest.schema -cne 'mcla-portable-campaign-bundle-v1' -or $bundleManifest.task -cne 'M7-016') {
    throw 'Portable bundle identity is invalid.'
}

$liveRoot = Assert-PlainDirectory (Join-Path $bundleFull 'diagnostics\live') 'Live diagnostics root'
$packages = @()
foreach ($directory in @(Get-ChildItem -LiteralPath $liveRoot -Directory | Sort-Object Name)) {
    if ($directory.Name -cnotmatch '^live-\d{8}T\d{6}Z-\d+-\d+$') { continue }
    $package = Read-F10Package $directory
    if ($null -ne $package) { $packages += $package }
}
if ($packages.Count -lt 2) { throw 'At least two complete F10 packages are required.' }

$first = $null
$second = $null
if ($FirstF10 -or $SecondF10) {
    if (-not $FirstF10 -or -not $SecondF10) { throw 'Explicit selection requires both FirstF10 and SecondF10.' }
    foreach ($name in @($FirstF10, $SecondF10)) {
        if ($name -cnotmatch '^live-\d{8}T\d{6}Z-\d+-\d+$') { throw 'Explicit F10 selection must name a complete direct package, not a path or partial.' }
    }
    $firstMatches = @($packages | Where-Object { (Split-Path -Leaf $_.Directory) -ceq $FirstF10 })
    $secondMatches = @($packages | Where-Object { (Split-Path -Leaf $_.Directory) -ceq $SecondF10 })
    if ($firstMatches.Count -ne 1 -or $secondMatches.Count -ne 1) { throw 'Explicit F10 package is missing or incomplete.' }
    $first = $firstMatches[0]; $second = $secondMatches[0]
    if (-not (Test-F10ProcessLifetime $first $second ([bool]$AllowLegacyPidOnly))) {
        throw 'Explicit F10 pair must share PID and positive process creation time; two legacy packages without creation time require -AllowLegacyPidOnly and independent session verification.'
    }
    if ($second.GuestOutputs -le $first.GuestOutputs -or
        $second.GuestVblanks -le $first.GuestVblanks) { throw 'Explicit F10 pair must be monotonic within one process lifetime.' }
} else {
 for ($i = $packages.Count - 1; $i -ge 1; $i--) {
    for ($j = $i - 1; $j -ge 0; $j--) {
        if ((Test-F10ProcessLifetime $packages[$j] $packages[$i] ([bool]$AllowLegacyPidOnly)) -and
            $packages[$i].GuestOutputs -gt $packages[$j].GuestOutputs -and
            $packages[$i].GuestVblanks -gt $packages[$j].GuestVblanks) {
            $first = $packages[$j]
            $second = $packages[$i]
            break
        }
    }
    if ($null -ne $second) { break }
 }
}
if ($null -eq $second) { throw 'No monotonic two-F10 pair from one verified process lifetime was found. Legacy PID-only comparison requires -AllowLegacyPidOnly and independent session verification.' }
$processLifetimeVerified = $null -ne $first.CreationTime100ns -and $null -ne $second.CreationTime100ns
$elapsedSeconds = ($second.CapturedUtc - $first.CapturedUtc).TotalSeconds
if ($elapsedSeconds -le 0) { throw 'The selected F10 pair has a non-positive elapsed time.' }
$outputDelta = $second.GuestOutputs - $first.GuestOutputs
$vblankDelta = $second.GuestVblanks - $first.GuestVblanks
$secondManifest = Get-Content -LiteralPath (Join-Path $second.Directory 'manifest.json') -Raw | ConvertFrom-Json
$secondLog = Assert-Artifact $second.Directory $secondManifest 'log-tail.txt'
$secondLogText = Get-Content -LiteralPath $secondLog -Raw
$auditAvailable = $secondLogText.Contains('REX_PERF_AUDIT_TRIGGER v=1 source=application-diagnostics')
$audit = if ($auditAvailable) { Parse-TriggeredAudit $secondLog } else { $null }
$graphics = Measure-GraphicsDelta $first.State $second.State $elapsedSeconds
$threadCpuTop = @(Measure-ThreadCpuDelta $first.State $second.State $elapsedSeconds)

[pscustomobject]@{
    Passed = $true
    BundleId = [string]$bundleManifest.bundle_id
    ProjectVersion = [string]$bundleManifest.project_version
    SdkVersion = [string]$bundleManifest.sdk_version
    ProcessId = $second.Pid
    ProcessCreationTime100ns = $second.CreationTime100ns
    ProcessLifetimeVerified = $processLifetimeVerified
    ProcessLifetimeEvidenceScope = if ($processLifetimeVerified) {
        'same-PID-and-positive-process-creation-time'
    } else {
        'legacy-PID-only; independently-verified-session-required; not-process-lifetime-proof'
    }
    FirstF10 = Split-Path -Leaf $first.Directory
    SecondF10 = Split-Path -Leaf $second.Directory
    FirstF10Utc = $first.CapturedUtc.ToString('o')
    SecondF10Utc = $second.CapturedUtc.ToString('o')
    F10ElapsedSeconds = [Math]::Round($elapsedSeconds, 2)
    GuestOutputDelta = $outputDelta
    GuestVblankDelta = $vblankDelta
    GuestOutputRateHz = [Math]::Round($outputDelta / $elapsedSeconds, 2)
    GuestVblankRateHz = [Math]::Round($vblankDelta / $elapsedSeconds, 2)
    OutputPerVblankPercent = [Math]::Round(($outputDelta * 100.0) / $vblankDelta, 2)
    AuditAvailable = $auditAvailable
    Audit = $audit
    Graphics = $graphics
    ThreadCpuTop = $threadCpuTop
}
