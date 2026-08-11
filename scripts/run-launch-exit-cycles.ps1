[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game',
    [ValidateRange(1, 10)][int]$CycleCount = 10,
    [ValidateRange(10, 60)][int]$StartupTimeoutSeconds = 20,
    [ValidateRange(1, 30)][int]$ExitTimeoutSeconds = 10,
    [ValidateRange(5, 60)][int]$CrashProbeTimeoutSeconds = 20,
    [switch]$SkipCleanBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$startupVerifier = Join-Path $PSScriptRoot 'verify-startup-smoke.ps1'
$crashVerifier = Join-Path $PSScriptRoot 'verify-crash-report.ps1'
$gameVerifier = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$resultVerifier = Join-Path $PSScriptRoot 'verify-launch-exit-cycles.ps1'
$toolchainResolver = Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1'
$utf8 = [System.Text.UTF8Encoding]::new($false)

function Assert-LexicalContainedPathWithoutReparse {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Description)
    $candidate = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repoRoot $Path }
    $fullPath = [System.IO.Path]::GetFullPath($candidate)
    $repoPrefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $fullPath.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay lexically inside the repository: '$fullPath'."
    }
    $repoItem = Get-Item -LiteralPath $repoRoot -Force
    if ($repoItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Repository root must not be a reparse point: '$repoRoot'."
    }
    $relative = $fullPath.Substring($repoPrefix.Length)
    $current = $repoRoot
    foreach ($component in @($relative.Split('\') | Where-Object { $_.Length -ne 0 })) {
        $current = Join-Path $current $component
        if (Test-Path -LiteralPath $current) {
            $item = Get-Item -LiteralPath $current -Force
            if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
                throw "$Description traverses a reparse point: '$current'."
            }
        }
    }
    return $fullPath
}

function Assert-ExistingTreeWithoutReparse {
    param([Parameter(Mandatory)][string]$Root, [Parameter(Mandatory)][string]$Description)
    if (-not (Test-Path -LiteralPath $Root)) { return }
    foreach ($item in @(Get-ChildItem -LiteralPath $Root -Recurse -Force)) {
        if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "$Description contains a reparse point: '$($item.FullName)'."
        }
    }
}

function Resolve-ContainedPath {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Description)
    $candidate = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repoRoot $Path }
    $resolved = (Resolve-Path -LiteralPath $candidate).Path
    $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay inside the repository: '$resolved'."
    }
    return $resolved
}

function Get-TreeSnapshot {
    param([Parameter(Mandatory)][string]$Root)
    $rootItem = Get-Item -LiteralPath $Root -Force
    if ($rootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Snapshot root must not be a reparse point: '$Root'."
    }
    $allItems = @(Get-ChildItem -LiteralPath $Root -Recurse -Force)
    foreach ($item in $allItems) {
        if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "Snapshot tree contains a reparse point: '$($item.FullName)'."
        }
    }
    $entries = @()
    foreach ($item in @($allItems | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)) {
        $entries += [ordered]@{
            path = $item.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/')
            length = $item.Length
            sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
        }
    }
    $serialized = ConvertTo-Json -InputObject @($entries) -Depth 4 -Compress
    $bytes = $utf8.GetBytes($serialized)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $treeHash = -join ($sha.ComputeHash($bytes) | ForEach-Object { $_.ToString('X2') })
    } finally {
        $sha.Dispose()
    }
    $totalBytes = [long](($entries | ForEach-Object { [long]$_['length'] } |
                Measure-Object -Sum).Sum)
    [pscustomobject]@{
        Hash = $treeHash
        FileCount = $entries.Count
        Bytes = $totalBytes
    }
}

function Get-GameIdentity {
    param([Parameter(Mandatory)][string]$Root)
    $verified = & $gameVerifier -GamePath $Root -VerifyHashes
    [ordered]@{
        file_count = $verified.FileCount
        payload_bytes = $verified.PayloadBytes
        hashes_verified = $verified.HashesVerified
        manifest_sha256 = (Get-FileHash -LiteralPath $verified.ManifestPath -Algorithm SHA256).Hash
    }
}

function Get-ArtifactSnapshot {
    param([Parameter(Mandatory)][string]$Root)
    $names = @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll')
    @($names | ForEach-Object {
        $path = Join-Path $Root $_
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Required repeated-cycle artifact was not found: '$path'."
        }
        [ordered]@{ name = $_; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
    })
}

function Get-TargetProcesses {
    param([Parameter(Mandatory)][string]$ExecutablePath)
    @((Get-Process -Name ([System.IO.Path]::GetFileNameWithoutExtension($ExecutablePath)) `
                -ErrorAction SilentlyContinue) | Where-Object {
            try {
                [string]::Equals($_.Path, $ExecutablePath,
                    [System.StringComparison]::OrdinalIgnoreCase)
            } catch {
                $false
            }
        })
}

function Assert-NoTargetProcess {
    param([Parameter(Mandatory)][string]$ExecutablePath)
    if (@(Get-TargetProcesses -ExecutablePath $ExecutablePath).Count -ne 0) {
        throw "The exact MCLA executable still has a live process: '$ExecutablePath'."
    }
}

function Complete-OwnedProcessCleanupAfterFailure {
    param([System.Diagnostics.Process]$Process)
    if (-not $Process) {
        return [pscustomobject]@{ ForceIssued = $false; Cleaned = $true }
    }
    $forceIssued = $false
    if (-not $Process.HasExited) {
        $forceIssued = $true
        try {
            Stop-Process -Id $Process.Id -Force -ErrorAction Stop
        } catch {
            # Wait below even if the force request raced with natural process exit.
        }
    }
    $signaled = $Process.HasExited -or $Process.WaitForExit(5000)
    $pidStillPresent = [bool](Get-Process -Id $Process.Id -ErrorAction SilentlyContinue)
    [pscustomobject]@{
        ForceIssued = $forceIssued
        Cleaned = $signaled -and -not $pidStillPresent
    }
}

function Rethrow-WithOwnedCleanup {
    param(
        [Parameter(Mandatory)][System.Management.Automation.ErrorRecord]$Failure,
        [System.Diagnostics.Process]$Process,
        [Parameter(Mandatory)][string]$RunRoot
    )
    $cleanup = Complete-OwnedProcessCleanupAfterFailure -Process $Process
    if (-not $cleanup.Cleaned) {
        throw "Repeated-cycle failure left its owned PID alive after the 5-second force-cleanup bound. Original failure: $($Failure.Exception.Message) Private run: '$RunRoot'."
    }
    if ($cleanup.ForceIssued) {
        throw "Repeated-cycle failure required fallback termination; owned PID cleanup was confirmed within 5 seconds. Original failure: $($Failure.Exception.Message) Private run: '$RunRoot'."
    }
    throw $Failure
}

function Assert-PriorTreesImmutable {
    param(
        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [System.Collections.ArrayList]$Snapshots
    )
    foreach ($snapshot in $Snapshots) {
        $current = Get-TreeSnapshot -Root $snapshot.Root
        if ($current.Hash -ne $snapshot.Hash) {
            throw "A completed cycle tree changed after its process exited: '$($snapshot.Label)'."
        }
    }
}

$buildCandidate = Assert-LexicalContainedPathWithoutReparse -Path $BuildRoot -Description 'Build root'
$gameCandidate = Assert-LexicalContainedPathWithoutReparse -Path $GameRoot -Description 'Game root'
$evidenceParent = Assert-LexicalContainedPathWithoutReparse `
    -Path 'private/evidence/M3-015' -Description 'M3-015 evidence root'
foreach ($path in @($startupVerifier, $crashVerifier, $gameVerifier, $resultVerifier,
        $toolchainResolver, (Join-Path $gameCandidate 'default.xex'),
        (Join-Path $repoRoot 'private/game-manifest.json'))) {
    [void](Assert-LexicalContainedPathWithoutReparse -Path $path -Description 'Required input')
}
Assert-ExistingTreeWithoutReparse -Root $buildCandidate -Description 'Build root'
Assert-ExistingTreeWithoutReparse -Root $gameCandidate -Description 'Game root'
Assert-ExistingTreeWithoutReparse -Root $evidenceParent -Description 'M3-015 evidence root'

$buildRootPath = Resolve-ContainedPath -Path $buildCandidate -Description 'Build root'
$gameRootPath = Resolve-ContainedPath -Path $gameCandidate -Description 'Game root'
foreach ($path in @($startupVerifier, $crashVerifier, $gameVerifier, $resultVerifier,
        $toolchainResolver, (Join-Path $gameRootPath 'default.xex'))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required repeated-cycle input was not found: '$path'."
    }
}
$executablePath = Join-Path $buildRootPath 'mcla.exe'
Assert-NoTargetProcess -ExecutablePath $executablePath
$gameBefore = Get-GameIdentity -Root $gameRootPath

$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $repoRoot "private/evidence/M3-015/$runId"
[System.IO.Directory]::CreateDirectory($runRoot) | Out-Null
$resultPath = Join-Path $runRoot 'result.json'
$buildLogPath = Join-Path $runRoot 'relwithdebinfo-clean-build.log'
$cleanBuildRecord = [ordered]@{
    performed = $false
    success = $false
    exit_code = -1
    duration_milliseconds = 0
    build_log_sha256 = ('0' * 64)
    executable_sha256 = ('0' * 64)
}

if (-not $SkipCleanBuild) {
    $expectedBuildRoot = [System.IO.Path]::GetFullPath(
        (Join-Path $repoRoot 'out/build/win-amd64-relwithdebinfo'))
    if (-not [string]::Equals($buildRootPath, $expectedBuildRoot,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'The final clean-build gate requires the canonical RelWithDebInfo build root.'
    }
    $toolchain = & $toolchainResolver -ExportPath
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    & $toolchain.CMakePath --build --preset win-amd64-relwithdebinfo --target mcla `
        --clean-first --parallel *>&1 | Tee-Object -FilePath $buildLogPath | Out-Null
    $buildExit = $LASTEXITCODE
    $timer.Stop()
    if ($buildExit -ne 0) {
        throw "RelWithDebInfo clean-first build failed with exit $buildExit. Private run: '$runRoot'."
    }
    $cleanBuildRecord = [ordered]@{
        performed = $true
        success = $true
        exit_code = 0
        duration_milliseconds = $timer.ElapsedMilliseconds
        build_log_sha256 = (Get-FileHash -LiteralPath $buildLogPath -Algorithm SHA256).Hash
        executable_sha256 = (Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash
    }
}

$artifactsBefore = Get-ArtifactSnapshot -Root $buildRootPath
Assert-NoTargetProcess -ExecutablePath $executablePath
$normalRecords = @()
$crashRecords = @()
$completedTrees = [System.Collections.ArrayList]::new()

for ($cycle = 1; $cycle -le $CycleCount; $cycle++) {
    Assert-PriorTreesImmutable -Snapshots $completedTrees
    $crashRoot = Join-Path $runRoot ('crash-probes/{0:D2}' -f $cycle)
    $crashUser = Join-Path $crashRoot 'user'
    $crashCache = Join-Path $crashRoot 'cache'
    [System.IO.Directory]::CreateDirectory($crashUser) | Out-Null
    [System.IO.Directory]::CreateDirectory($crashCache) | Out-Null
    $crashLog = Join-Path $crashRoot 'mcla.log'
    $crashReport = Join-Path $crashUser 'mcla-crash-report.txt'
    $process = $null
    try {
        $timer = [System.Diagnostics.Stopwatch]::StartNew()
        $process = Start-Process -FilePath $executablePath -ArgumentList @(
            '--mcla_crash_probe', "--game_data_root=$gameRootPath",
            "--user_data_root=$crashUser", "--cache_root=$crashCache",
            "--log_file=$crashLog", '--log_level=trace', '--fullscreen=false'
        ) -WorkingDirectory $buildRootPath -PassThru
        $signaled = $process.WaitForExit($CrashProbeTimeoutSeconds * 1000)
        $timer.Stop()
        if (-not $signaled) {
            throw "Crash probe cycle $cycle did not signal exit within its bound. Private run: '$runRoot'."
        }
        if ($process.ExitCode -ne 0) {
            throw "Crash probe cycle $cycle exited with code $($process.ExitCode). Private run: '$runRoot'."
        }
        $verified = & $crashVerifier -ReportPath $crashReport -RuntimeLogPath $crashLog
        $crashLogText = Get-Content -LiteralPath $crashLog -Raw
        $crashHardExitMarker = 'MCLA crash probe: controlled hard exit'
        $crashHardExitMatches = [regex]::Matches(
            $crashLogText, [regex]::Escape($crashHardExitMarker))
        $shutdownOffset = $crashLogText.IndexOf('MCLA lifecycle: shutdown',
            [System.StringComparison]::Ordinal)
        $hardExitOffset = $crashLogText.IndexOf($crashHardExitMarker,
            [System.StringComparison]::Ordinal)
        $afterHardExit = if ($hardExitOffset -ge 0) {
            $crashLogText.Substring($hardExitOffset + $crashHardExitMarker.Length).Trim()
        } else { 'missing' }
        if ($crashHardExitMatches.Count -ne 1 -or $hardExitOffset -le $shutdownOffset -or
            $afterHardExit.Length -ne 0) {
            throw "Crash probe cycle $cycle lacks its exact ordered hard-exit marker. Private run: '$runRoot'."
        }
        Assert-NoTargetProcess -ExecutablePath $executablePath
        $userTree = Get-TreeSnapshot -Root $crashUser
        $cacheTree = Get-TreeSnapshot -Root $crashCache
        $crashRecords += [ordered]@{
            index = $cycle
            elapsed_milliseconds = $timer.ElapsedMilliseconds
            exit_code = 0
            report_verified = $verified.Passed
            required_fields = $verified.RequiredFields
            host_stack_frames = $verified.HostStackFrames
            guest_memory_included = $verified.GuestMemoryIncluded
            ordered_markers = $verified.OrderedMarkers
            hard_exit_marker_occurrences = $crashHardExitMatches.Count
            harness_force_cleanup = $false
            process_signal_confirmed = $true
            process_cleanup_confirmed = $true
            prior_cycles_immutable = $true
            runtime_log_sha256 = (Get-FileHash -LiteralPath $crashLog -Algorithm SHA256).Hash
            report_sha256 = (Get-FileHash -LiteralPath $crashReport -Algorithm SHA256).Hash
            user_tree_sha256 = $userTree.Hash
            cache_tree_sha256 = $cacheTree.Hash
        }
    } catch {
        Rethrow-WithOwnedCleanup -Failure $_ -Process $process -RunRoot $runRoot
    }
    $crashTree = Get-TreeSnapshot -Root $crashRoot
    [void]$completedTrees.Add([pscustomobject]@{
        Label = ('crash-probe-{0:D2}' -f $cycle)
        Root = $crashRoot
        Hash = $crashTree.Hash
    })
}

for ($cycle = 1; $cycle -le $CycleCount; $cycle++) {
    Assert-PriorTreesImmutable -Snapshots $completedTrees
    $normalRoot = Join-Path $runRoot ('normal-cycles/{0:D2}' -f $cycle)
    $normalUser = Join-Path $normalRoot 'user'
    $normalCache = Join-Path $normalRoot 'cache'
    [System.IO.Directory]::CreateDirectory($normalUser) | Out-Null
    [System.IO.Directory]::CreateDirectory($normalCache) | Out-Null
    $normalLog = Join-Path $normalRoot 'mcla.log'
    $process = $null
    try {
        $startupTimer = [System.Diagnostics.Stopwatch]::StartNew()
        $process = Start-Process -FilePath $executablePath -ArgumentList @(
            "--game_data_root=$gameRootPath", "--user_data_root=$normalUser",
            "--cache_root=$normalCache", "--log_file=$normalLog",
            '--log_level=trace', '--fullscreen=false'
        ) -WorkingDirectory $buildRootPath -PassThru
        $startup = $null
        while ($startupTimer.Elapsed.TotalSeconds -lt $StartupTimeoutSeconds) {
            if ($process.HasExited) {
                throw "Normal cycle $cycle exited before startup completed. Private run: '$runRoot'."
            }
            if (Test-Path -LiteralPath $normalLog -PathType Leaf) {
                $text = Get-Content -LiteralPath $normalLog -Raw
                if ($text.IndexOf('AudioWorker: dispatching callback ',
                        [System.StringComparison]::Ordinal) -ge 0) {
                    $startup = & $startupVerifier -RuntimeLogPath $normalLog
                    break
                }
            }
            Start-Sleep -Milliseconds 250
        }
        $startupTimer.Stop()
        if (-not $startup) {
            throw "Normal cycle $cycle missed the bounded startup markers. Private run: '$runRoot'."
        }
        $process.Refresh()
        if ($process.HasExited -or $process.MainWindowHandle -eq [IntPtr]::Zero -or
            -not $process.CloseMainWindow()) {
            throw "Normal cycle $cycle could not request WM_CLOSE on its owned window. Private run: '$runRoot'."
        }
        $exitTimer = [System.Diagnostics.Stopwatch]::StartNew()
        $signaled = $process.WaitForExit($ExitTimeoutSeconds * 1000)
        $exitTimer.Stop()
        if (-not $signaled) {
            throw "Normal cycle $cycle did not signal controlled exit. Private run: '$runRoot'."
        }
        if ($process.ExitCode -ne 0) {
            throw "Normal cycle $cycle exited with code $($process.ExitCode). Private run: '$runRoot'."
        }
        $startup = & $startupVerifier -RuntimeLogPath $normalLog
        $log = Get-Content -LiteralPath $normalLog -Raw
        $windowCloseMarker = 'Window closing, shutting down...'
        $hardExitMarker = 'Title terminated; hard-exiting process.'
        $windowCloseMatches = [regex]::Matches($log, [regex]::Escape($windowCloseMarker))
        $hardExitMatches = [regex]::Matches($log, [regex]::Escape($hardExitMarker))
        $closeOffset = $log.IndexOf($windowCloseMarker,
            [System.StringComparison]::Ordinal)
        $exitOffset = $log.IndexOf($hardExitMarker,
            [System.StringComparison]::Ordinal)
        if ($windowCloseMatches.Count -ne 1 -or $hardExitMatches.Count -ne 1 -or
            $closeOffset -lt 0 -or $exitOffset -le $closeOffset) {
            throw "Normal cycle $cycle lacks exactly one ordered occurrence of each controlled-close marker. Private run: '$runRoot'."
        }
        Assert-NoTargetProcess -ExecutablePath $executablePath
        $userTree = Get-TreeSnapshot -Root $normalUser
        $cacheTree = Get-TreeSnapshot -Root $normalCache
        $normalRecords += [ordered]@{
            index = $cycle
            startup_elapsed_milliseconds = $startupTimer.ElapsedMilliseconds
            exit_elapsed_milliseconds = $exitTimer.ElapsedMilliseconds
            exit_code = 0
            startup_marker_count = $startup.MarkerCount
            close_requested = $true
            close_marker_count = 2
            window_close_marker_occurrences = $windowCloseMatches.Count
            hard_exit_marker_occurrences = $hardExitMatches.Count
            harness_force_cleanup = $false
            process_signal_confirmed = $true
            process_cleanup_confirmed = $true
            prior_cycles_immutable = $true
            runtime_log_sha256 = (Get-FileHash -LiteralPath $normalLog -Algorithm SHA256).Hash
            user_tree_sha256 = $userTree.Hash
            cache_tree_sha256 = $cacheTree.Hash
            user_file_count = $userTree.FileCount
            cache_file_count = $cacheTree.FileCount
            user_bytes = $userTree.Bytes
            cache_bytes = $cacheTree.Bytes
        }
    } catch {
        Rethrow-WithOwnedCleanup -Failure $_ -Process $process -RunRoot $runRoot
    }
    $normalTree = Get-TreeSnapshot -Root $normalRoot
    [void]$completedTrees.Add([pscustomobject]@{
        Label = ('normal-cycle-{0:D2}' -f $cycle)
        Root = $normalRoot
        Hash = $normalTree.Hash
    })
}

Assert-PriorTreesImmutable -Snapshots $completedTrees
Assert-NoTargetProcess -ExecutablePath $executablePath
$gameAfter = Get-GameIdentity -Root $gameRootPath
$artifactsAfter = Get-ArtifactSnapshot -Root $buildRootPath
$result = [ordered]@{
    schema = 1
    task = 'M3-015'
    normal_cycle_count = $normalRecords.Count
    crash_probe_cycle_count = $crashRecords.Count
    execution_order = if ($CycleCount -eq 10) {
        'clean_build_then_10_crash_probes_then_10_normal_cycles'
    } else {
        'development_crash_probes_then_normal_cycles'
    }
    startup_timeout_seconds = $StartupTimeoutSeconds
    exit_timeout_seconds = $ExitTimeoutSeconds
    crash_probe_timeout_seconds = $CrashProbeTimeoutSeconds
    clean_build = $cleanBuildRecord
    first_crash_probe_post_clean_build = (-not $SkipCleanBuild)
    game_identity = [ordered]@{ before = $gameBefore; after = $gameAfter }
    artifacts = [ordered]@{ before = $artifactsBefore; after = $artifactsAfter }
    normal_cycles = $normalRecords
    crash_probe_cycles = $crashRecords
    all_write_roots_contained = $true
    all_prior_cycles_immutable = $true
    no_surviving_processes = $true
    data_integrity_preserved = $true
}
$resultJson = ($result | ConvertTo-Json -Depth 10) + [Environment]::NewLine
[System.IO.File]::WriteAllText($resultPath, $resultJson, $utf8)

if ($CycleCount -eq 10 -and -not $SkipCleanBuild) {
    $verifiedResult = & $resultVerifier -ResultPath $resultPath
    [pscustomobject]@{
        Passed = $verifiedResult.Passed
        NormalCycles = $verifiedResult.NormalCycles
        CrashProbeCycles = $verifiedResult.CrashProbeCycles
        CleanBuildVerified = $verifiedResult.CleanBuildVerified
        ProcessCleanupVerified = $verifiedResult.ProcessCleanupVerified
        DataIntegrityVerified = $verifiedResult.DataIntegrityVerified
        PrivateRunRoot = $runRoot
        ResultPath = $resultPath
    }
} else {
    [pscustomobject]@{
        Passed = $false
        DevelopmentOnly = $true
        NormalCycles = $normalRecords.Count
        CrashProbeCycles = $crashRecords.Count
        PrivateRunRoot = $runRoot
        ResultPath = $resultPath
    }
}
