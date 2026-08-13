[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-launch-exit-cycles.ps1'
$runner = Join-Path $PSScriptRoot 'run-launch-exit-cycles.ps1'
$fixtureRoot = Join-Path $repoRoot ('private/test-launch-exit-cycles-' + [guid]::NewGuid().ToString('N'))
$resultPath = Join-Path $fixtureRoot 'result.json'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$hashA = 'A' * 64
$hashB = 'B' * 64
$junctionTargetRoot = Join-Path $repoRoot (
    'private/test-launch-exit-junction-target-' + [guid]::NewGuid().ToString('N'))
$positiveCrashReport = @'
REX_GUEST_CRASH schema=1
reason=MCLA synthetic crash probe
guest_pc=0x821322BC
ppc_function=sub_821322B8
thread_id=0x4D434C41
last_import=__imp__XGetAVPack
host_stack_count=2
host_stack[0]=mcla.exe+0x1234 (0x7FF612341234)
host_stack[1]=rexruntimerd.dll+0x5678 (0x7FF856785678)
guest_memory_included=false
'@
$positiveCrashLog = @'
MCLA module config: loaded XEX base 82000000, entry 821322B8
MCLA crash probe: privacy-safe report written
MCLA crash probe: complete; guest launch skipped
MCLA lifecycle: shutdown
MCLA crash probe: controlled hard exit
'@
$positiveNormalLog = @'
[info] [app] MCLA lifecycle: logging ready
[info] [gpu] MCLA graphics: selected GPU plugin 'xenos'
[info] [sys] GPU plugin 'xenos' loaded (rexgpu-xenosrd.dll)
[info] [ppc] MCLA module config: static image 82000000-829E0000, code 82130000-827CD054, 30026 mappings
[info] [sys] Runtime initialized successfully
[info] [sys] Loading XEX image: game:\default.xex
[info] [ppc] MCLA module identity: title 545407F8, media 5940C9DB, image 82000000-829E0000, entry 821322B8
[info] [ppc] MCLA module config: entry 821322B8 registered in dispatch range 82130000-827CD054
[info] [vfs] MCLA VFS: game: and d: resolve 3/3 expected disc files on \Device\Harddisk0\Partition1
[info] [vfs] MCLA VFS: write, create, delete, and writable-map requests denied
[info] [sys] KernelState: Preparing module launch...
[info] [core] Initializing shader storage for title 545407F8...
[info] [gpu] SetInterruptCallback(82411478, 40002080)
[debug] [gpu] Creating graphics pipeline with VS A, PS B
[debug] [apu] AudioWorker: dispatching callback 823F56D0 with arg 1 for client 0
[info] Window closing, shutting down...
[info] Title terminated; hard-exiting process.
'@

function Get-TestTreeSnapshot {
    param([Parameter(Mandatory)][string]$Root)
    $entries = @()
    foreach ($file in @(Get-ChildItem -LiteralPath $Root -File -Recurse -Force |
            Sort-Object FullName)) {
        $entries += [ordered]@{
            path = $file.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/')
            length = $file.Length
            sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        }
    }
    $serialized = ConvertTo-Json -InputObject @($entries) -Depth 4 -Compress
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $treeHash = -join ($sha.ComputeHash($utf8.GetBytes($serialized)) |
            ForEach-Object { $_.ToString('X2') })
    } finally {
        $sha.Dispose()
    }
    $totalBytes = [long]0
    foreach ($entry in $entries) { $totalBytes += [long]$entry.length }
    [pscustomobject]@{
        Hash = $treeHash
        FileCount = $entries.Count
        Bytes = $totalBytes
    }
}

function Remove-TestTreeSafely {
    param([Parameter(Mandatory)][string]$Root)
    if (-not (Test-Path -LiteralPath $Root -PathType Container)) { return }
    $reparseItems = @(Get-ChildItem -LiteralPath $Root -Recurse -Force |
        Where-Object { $_.Attributes -band [System.IO.FileAttributes]::ReparsePoint } |
        Sort-Object { $_.FullName.Length } -Descending)
    foreach ($item in $reparseItems) {
        if ($item.PSIsContainer) {
            [System.IO.Directory]::Delete($item.FullName, $false)
        } else {
            [System.IO.File]::Delete($item.FullName)
        }
    }
    [System.IO.Directory]::Delete($Root, $true)
}

function Initialize-EvidenceTree {
    Remove-TestTreeSafely -Root $fixtureRoot
    [System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
    [System.IO.File]::WriteAllText(
        (Join-Path $fixtureRoot 'relwithdebinfo-clean-build.log'), 'clean build passed', $utf8)
    foreach ($index in 1..10) {
        $name = '{0:D2}' -f $index
        $crashRoot = Join-Path $fixtureRoot "crash-probes/$name"
        $crashUser = Join-Path $crashRoot 'user'
        [System.IO.Directory]::CreateDirectory($crashUser) | Out-Null
        [System.IO.Directory]::CreateDirectory((Join-Path $crashRoot 'cache')) | Out-Null
        [System.IO.File]::WriteAllText((Join-Path $crashRoot 'mcla.log'), $positiveCrashLog, $utf8)
        [System.IO.File]::WriteAllText(
            (Join-Path $crashUser 'mcla-crash-report.txt'), $positiveCrashReport, $utf8)

        $normalRoot = Join-Path $fixtureRoot "normal-cycles/$name"
        [System.IO.Directory]::CreateDirectory((Join-Path $normalRoot 'user')) | Out-Null
        [System.IO.Directory]::CreateDirectory((Join-Path $normalRoot 'cache')) | Out-Null
        [System.IO.File]::WriteAllText((Join-Path $normalRoot 'mcla.log'), $positiveNormalLog, $utf8)
    }
}

function New-NormalCycles {
    $records = @()
    for ($index = 1; $index -le 10; $index++) {
        $records += [ordered]@{
            index = $index
            startup_elapsed_milliseconds = 7000
            exit_elapsed_milliseconds = 100
            exit_code = 0
            startup_marker_count = 15
            close_requested = $true
            close_marker_count = 2
            window_close_marker_occurrences = 1
            hard_exit_marker_occurrences = 1
            harness_force_cleanup = $false
            process_signal_confirmed = $true
            process_cleanup_confirmed = $true
            prior_cycles_immutable = $true
            runtime_log_sha256 = $hashA
            user_tree_sha256 = $hashA
            cache_tree_sha256 = $hashA
            user_file_count = 1
            cache_file_count = 2
            user_bytes = 100
            cache_bytes = 200
        }
    }
    return $records
}

function New-CrashCycles {
    $records = @()
    for ($index = 1; $index -le 10; $index++) {
        $records += [ordered]@{
            index = $index
            elapsed_milliseconds = 1000
            exit_code = 0
            report_verified = $true
            required_fields = 7
            host_stack_frames = 2
            guest_memory_included = $false
            ordered_markers = 4
            hard_exit_marker_occurrences = 1
            harness_force_cleanup = $false
            process_signal_confirmed = $true
            process_cleanup_confirmed = $true
            prior_cycles_immutable = $true
            runtime_log_sha256 = $hashA
            report_sha256 = $hashA
            user_tree_sha256 = $hashA
            cache_tree_sha256 = $hashA
        }
    }
    return $records
}

function New-ValidResult {
    Initialize-EvidenceTree
    $artifacts = @(
        [ordered]@{ name = 'mcla.exe'; sha256 = $hashA },
        [ordered]@{ name = 'rexruntimerd.dll'; sha256 = $hashA },
        [ordered]@{ name = 'TracyClientrd.dll'; sha256 = $hashA },
        [ordered]@{ name = 'rexgpu-xenosrd.dll'; sha256 = $hashA }
    )
    [ordered]@{
        schema = 1
        task = 'M3-015'
        normal_cycle_count = 10
        crash_probe_cycle_count = 10
        execution_order = 'clean_build_then_10_crash_probes_then_10_normal_cycles'
        startup_timeout_seconds = 20
        exit_timeout_seconds = 10
        crash_probe_timeout_seconds = 20
        clean_build = [ordered]@{
            performed = $true
            success = $true
            exit_code = 0
            duration_milliseconds = 1000
            build_log_sha256 = (Get-FileHash -LiteralPath (
                Join-Path $fixtureRoot 'relwithdebinfo-clean-build.log') -Algorithm SHA256).Hash
            executable_sha256 = $hashA
        }
        first_crash_probe_post_clean_build = $true
        game_identity = [ordered]@{
            before = [ordered]@{
                file_count = 15
                payload_bytes = 6569586392
                hashes_verified = 15
                manifest_sha256 = $hashA
            }
            after = [ordered]@{
                file_count = 15
                payload_bytes = 6569586392
                hashes_verified = 15
                manifest_sha256 = $hashA
            }
        }
        artifacts = [ordered]@{ before = $artifacts; after = $artifacts }
        normal_cycles = New-NormalCycles
        crash_probe_cycles = New-CrashCycles
        all_write_roots_contained = $true
        all_prior_cycles_immutable = $true
        no_surviving_processes = $true
        data_integrity_preserved = $true
    } | ForEach-Object {
        $value = $_
        foreach ($index in 0..9) {
            $name = '{0:D2}' -f ($index + 1)
            $crashRoot = Join-Path $fixtureRoot "crash-probes/$name"
            $crashUser = Join-Path $crashRoot 'user'
            $crashCache = Join-Path $crashRoot 'cache'
            $value.crash_probe_cycles[$index].runtime_log_sha256 =
                (Get-FileHash -LiteralPath (Join-Path $crashRoot 'mcla.log') -Algorithm SHA256).Hash
            $value.crash_probe_cycles[$index].report_sha256 =
                (Get-FileHash -LiteralPath (
                    Join-Path $crashUser 'mcla-crash-report.txt') -Algorithm SHA256).Hash
            $value.crash_probe_cycles[$index].user_tree_sha256 =
                (Get-TestTreeSnapshot -Root $crashUser).Hash
            $value.crash_probe_cycles[$index].cache_tree_sha256 =
                (Get-TestTreeSnapshot -Root $crashCache).Hash

            $normalRoot = Join-Path $fixtureRoot "normal-cycles/$name"
            $normalUser = Join-Path $normalRoot 'user'
            $normalCache = Join-Path $normalRoot 'cache'
            $normalUserTree = Get-TestTreeSnapshot -Root $normalUser
            $normalCacheTree = Get-TestTreeSnapshot -Root $normalCache
            $value.normal_cycles[$index].runtime_log_sha256 =
                (Get-FileHash -LiteralPath (Join-Path $normalRoot 'mcla.log') -Algorithm SHA256).Hash
            $value.normal_cycles[$index].user_tree_sha256 = $normalUserTree.Hash
            $value.normal_cycles[$index].cache_tree_sha256 = $normalCacheTree.Hash
            $value.normal_cycles[$index].user_file_count = $normalUserTree.FileCount
            $value.normal_cycles[$index].cache_file_count = $normalCacheTree.FileCount
            $value.normal_cycles[$index].user_bytes = $normalUserTree.Bytes
            $value.normal_cycles[$index].cache_bytes = $normalCacheTree.Bytes
        }
        $value
    }
}

function Write-Result {
    param([Parameter(Mandatory)][object]$Value)
    [System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
    [System.IO.File]::WriteAllText(
        $resultPath,
        (($Value | ConvertTo-Json -Depth 10) + [Environment]::NewLine),
        $utf8
    )
}

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Mutate)
    $candidate = New-ValidResult | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    & $Mutate $candidate
    Write-Result $candidate
    try {
        & $verifier -ResultPath $resultPath | Out-Null
    } catch {
        return
    }
    throw "Negative launch/exit fixture '$Name' was accepted."
}

function Assert-EvidenceRejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Mutate)
    $candidate = New-ValidResult
    Write-Result $candidate
    & $Mutate
    try {
        & $verifier -ResultPath $resultPath | Out-Null
    } catch {
        return
    }
    throw "Negative launch/exit evidence fixture '$Name' was accepted."
}

try {
    $runnerText = Get-Content -LiteralPath $runner -Raw
    $appSource = Get-Content -LiteralPath (Join-Path $repoRoot 'src/mcla_app.cpp') -Raw
    $appSource = Get-Content -LiteralPath (Join-Path $repoRoot 'src/mcla_app.cpp') -Raw
    foreach ($required in @(
            '--clean-first',
            'CloseMainWindow()',
            'Window closing, shutting down...',
            'Title terminated; hard-exiting process.',
            'Complete-OwnedProcessCleanupAfterFailure',
            'Rethrow-WithOwnedCleanup',
            'left its owned PID alive after the 5-second force-cleanup bound',
            'owned PID cleanup was confirmed within 5 seconds',
            'WaitForExit(5000)',
            'Assert-PriorTreesImmutable',
            'Get-GameIdentity -Root $gameRootPath',
            'verify-startup-smoke.ps1',
            'verify-crash-report.ps1'
        )) {
        if ($runnerText.IndexOf($required, [System.StringComparison]::Ordinal) -lt 0) {
            throw "Launch/exit runner contract is missing '$required'."
        }
    }
    if ($appSource -notmatch '(?s)MCLA crash probe: controlled hard exit.*?FlushLogging\(\);\s*std::_Exit\(0\);') {
        throw 'Crash probe hard-exit marker is not immediately flushed before _Exit.'
    }
    $crashPhaseOffset = $runnerText.IndexOf("'crash-probes/{0:D2}'",
        [System.StringComparison]::Ordinal)
    $normalPhaseOffset = $runnerText.IndexOf("'normal-cycles/{0:D2}'",
        [System.StringComparison]::Ordinal)
    if ($crashPhaseOffset -lt 0 -or $normalPhaseOffset -le $crashPhaseOffset) {
        throw 'Launch/exit runner does not keep all crash probes before the consecutive normal phase.'
    }
    foreach ($required in @(
            'HardExitCrashProbeFromUIThread()',
            'OnShutdown();',
            'rex::FlushLogging();',
            'std::_Exit(0);'
        )) {
        if ($appSource.IndexOf($required, [System.StringComparison]::Ordinal) -lt 0) {
            throw "Crash-probe teardown containment is missing '$required'."
        }
    }

    Write-Result (New-ValidResult)
    $verified = & $verifier -ResultPath $resultPath
    if (-not $verified.Passed -or $verified.NormalCycles -ne 10 -or
        $verified.CrashProbeCycles -ne 10 -or -not $verified.CleanBuildVerified -or
        -not $verified.ProcessCleanupVerified -or -not $verified.DataIntegrityVerified) {
        throw 'Positive launch/exit fixture returned an unexpected result.'
    }

    Assert-Rejected nine-normal-cycles {
        param($value) $value.normal_cycle_count = 9
    }
    Assert-Rejected wrong-execution-order {
        param($value) $value.execution_order = 'interleaved'
    }
    Assert-Rejected clean-build-missing {
        param($value) $value.clean_build.performed = $false
    }
    Assert-Rejected not-post-clean-build {
        param($value) $value.first_crash_probe_post_clean_build = $false
    }
    Assert-Rejected game-drift {
        param($value) $value.game_identity.after.manifest_sha256 = $hashB
    }
    Assert-Rejected binary-drift {
        param($value) $value.artifacts.after[3].sha256 = $hashB
    }
    Assert-Rejected normal-nonzero-exit {
        param($value) $value.normal_cycles[2].exit_code = 1
    }
    Assert-Rejected missing-close-marker {
        param($value) $value.normal_cycles[3].close_marker_count = 1
    }
    Assert-Rejected duplicate-close-marker {
        param($value) $value.normal_cycles[3].window_close_marker_occurrences = 2
    }
    Assert-Rejected harness-force-cleanup {
        param($value) $value.normal_cycles[4].harness_force_cleanup = $true
    }
    Assert-Rejected crash-probe-timeout {
        param($value) $value.crash_probe_cycles[0].elapsed_milliseconds = 20001
    }
    Assert-Rejected crash-probe-not-signaled {
        param($value) $value.crash_probe_cycles[1].process_signal_confirmed = $false
    }
    Assert-Rejected forged-host-stack-frames {
        param($value) $value.crash_probe_cycles[1].host_stack_frames = 1
    }
    Assert-Rejected prior-cycle-mutated {
        param($value) $value.normal_cycles[5].prior_cycles_immutable = $false
    }
    Assert-Rejected surviving-process {
        param($value) $value.no_surviving_processes = $false
    }
    Assert-Rejected absolute-private-path {
        param($value)
        $value.task = 'C:\private\M3-015'
    }
    Assert-Rejected wrong-cycle-index {
        param($value) $value.crash_probe_cycles[7].index = 9
    }

    Assert-EvidenceRejected mutated-normal-log {
        [System.IO.File]::AppendAllText(
            (Join-Path $fixtureRoot 'normal-cycles/04/mcla.log'), 'mutation', $utf8)
    }
    Assert-EvidenceRejected deleted-crash-report {
        [System.IO.File]::Delete(
            (Join-Path $fixtureRoot 'crash-probes/03/user/mcla-crash-report.txt'))
    }
    Assert-EvidenceRejected extra-cycle-directory {
        [System.IO.Directory]::CreateDirectory(
            (Join-Path $fixtureRoot 'normal-cycles/11')) | Out-Null
    }
    Assert-EvidenceRejected extra-run-artifact {
        [System.IO.File]::WriteAllText(
            (Join-Path $fixtureRoot 'unexpected.txt'), 'unexpected', $utf8)
    }

    $reparseCases = 0
    try {
        [System.IO.Directory]::CreateDirectory($junctionTargetRoot) | Out-Null
        $candidate = New-ValidResult
        Write-Result $candidate
        $cachePath = Join-Path $fixtureRoot 'crash-probes/05/cache'
        [System.IO.Directory]::Delete($cachePath, $true)
        New-Item -ItemType Junction -Path $cachePath -Target $junctionTargetRoot `
            -ErrorAction Stop | Out-Null
        try {
            & $verifier -ResultPath $resultPath | Out-Null
            throw 'Negative reparse evidence fixture was accepted.'
        } catch {
            if ($_.Exception.Message -eq 'Negative reparse evidence fixture was accepted.') { throw }
            $reparseCases++
        }

        $preflightRoot = Join-Path $repoRoot (
            'private/test-launch-exit-preflight-' + [guid]::NewGuid().ToString('N'))
        $preflightTarget = Join-Path $preflightRoot 'target'
        $preflightLink = Join-Path $preflightRoot 'build-link'
        [System.IO.Directory]::CreateDirectory($preflightTarget) | Out-Null
        New-Item -ItemType Junction -Path $preflightLink -Target $preflightTarget `
            -ErrorAction Stop | Out-Null
        try {
            & $runner -BuildRoot $preflightLink -GameRoot 'private/game' `
                -CycleCount 1 -SkipCleanBuild | Out-Null
            throw 'Runner accepted a contained BuildRoot junction.'
        } catch {
            if ($_.Exception.Message -eq 'Runner accepted a contained BuildRoot junction.') { throw }
            if ($_.Exception.Message -notmatch 'reparse point') {
                throw "Runner rejected the junction for the wrong reason: $($_.Exception.Message)"
            }
            $reparseCases++
        } finally {
            Remove-TestTreeSafely -Root $preflightRoot
        }
    } catch [System.UnauthorizedAccessException] {
        if ($runnerText.IndexOf('[System.IO.FileAttributes]::ReparsePoint',
                [System.StringComparison]::Ordinal) -lt 0) {
            throw 'Junction creation is unavailable and the static reparse guard is missing.'
        }
    }

    [pscustomobject]@{
        Passed = $true
        RunnerContractVerified = $true
        PositiveCases = 1
        NegativeCases = 21 + $reparseCases
        ReparseCases = $reparseCases
    }
} finally {
    Remove-TestTreeSafely -Root $fixtureRoot
    Remove-TestTreeSafely -Root $junctionTargetRoot
}
