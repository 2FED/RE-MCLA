[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game',
    [ValidateRange(1, 20)][int]$CycleCount = 20,
    [ValidateRange(15, 120)][int]$FirstFrameTimeoutSeconds = 60,
    [ValidateRange(2000, 10000)][int]$PostMarkerDwellMilliseconds = 2000,
    [ValidateRange(1, 30)][int]$ExitTimeoutSeconds = 10,
    [switch]$SkipCleanBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$firstFrameVerifier = Join-Path $PSScriptRoot 'verify-first-frame-smoke.ps1'
$gameVerifier = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$toolchainResolver = Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$cleanupTimeoutMilliseconds = 5000
$completionMarker = 'MCLA graphics: nontrivial guest frame captured '
$gameWindowTitlePattern = '^mcla \[rexglue-v[^\]]+\]$'

if (-not ('MclaFirstFrameNativeWindow' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class MclaFirstFrameNativeWindow {
    private delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint processId);

    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern int GetWindowText(IntPtr hWnd, StringBuilder text, int maximumCount);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern bool PostMessage(IntPtr hWnd, uint message, IntPtr wParam, IntPtr lParam);

    public static IntPtr[] GetVisibleWindowHandlesForProcess(int expectedProcessId) {
        List<IntPtr> handles = new List<IntPtr>();
        EnumWindows(delegate(IntPtr hWnd, IntPtr unused) {
            uint processId;
            GetWindowThreadProcessId(hWnd, out processId);
            if (processId == (uint)expectedProcessId && IsWindowVisible(hWnd)) {
                handles.Add(hWnd);
            }
            return true;
        }, IntPtr.Zero);
        return handles.ToArray();
    }

    public static string GetTitle(IntPtr hWnd) {
        StringBuilder text = new StringBuilder(1024);
        int length = GetWindowText(hWnd, text, text.Capacity);
        return length > 0 ? text.ToString(0, length) : String.Empty;
    }

    public static bool PostClose(IntPtr hWnd) {
        return PostMessage(hWnd, 0x0010, IntPtr.Zero, IntPtr.Zero);
    }
}
'@
}

function Assert-LexicalContainedPathWithoutReparse {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Description)
    $candidate = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repoRoot $Path }
    $fullPath = [System.IO.Path]::GetFullPath($candidate)
    $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $fullPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay lexically inside the repository: '$fullPath'."
    }
    $repoItem = Get-Item -LiteralPath $repoRoot -Force
    if ($repoItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Repository root must not be a reparse point: '$repoRoot'."
    }
    $relative = $fullPath.Substring($prefix.Length)
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
    $rootItem = Get-Item -LiteralPath $Root -Force
    if ($rootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "$Description is a reparse point: '$Root'."
    }
    foreach ($item in @(Get-ChildItem -LiteralPath $Root -Recurse -Force)) {
        if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "$Description contains a reparse point: '$($item.FullName)'."
        }
    }
}

function Resolve-ContainedPath {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Description)
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay inside the repository: '$resolved'."
    }
    return $resolved
}

function Get-TreeSnapshot {
    param([Parameter(Mandatory)][string]$Root)
    Assert-ExistingTreeWithoutReparse -Root $Root -Description 'Snapshot tree'
    $allItems = @(Get-ChildItem -LiteralPath $Root -Recurse -Force)
    $entries = @()
    foreach ($directory in @($allItems | Where-Object { $_.PSIsContainer } | Sort-Object FullName)) {
        $entries += [ordered]@{
            kind = 'directory'
            path = $directory.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/')
        }
    }
    $files = @($allItems | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)
    foreach ($file in $files) {
        $entries += [ordered]@{
            kind = 'file'
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
    $bytes = [long]0
    foreach ($file in $files) { $bytes += [long]$file.Length }
    [pscustomobject]@{
        Hash = $treeHash
        FileCount = $files.Count
        DirectoryCount = @($allItems | Where-Object { $_.PSIsContainer }).Count
        Bytes = $bytes
    }
}

function Get-GameIdentity {
    param([Parameter(Mandatory)][string]$Root)
    $tree = Get-TreeSnapshot -Root $Root
    $verified = & $gameVerifier -GamePath $Root -VerifyHashes
    [ordered]@{
        file_count = $verified.FileCount
        payload_bytes = $verified.PayloadBytes
        hashes_verified = $verified.HashesVerified
        manifest_sha256 = (Get-FileHash -LiteralPath $verified.ManifestPath -Algorithm SHA256).Hash
        tree_sha256 = $tree.Hash
        tree_file_count = $tree.FileCount
        tree_directory_count = $tree.DirectoryCount
        tree_bytes = $tree.Bytes
    }
}

function Get-ArtifactSnapshot {
    param([Parameter(Mandatory)][string]$Root)
    @(@('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll') |
        ForEach-Object {
            $path = Join-Path $Root $_
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
                throw "Required first-frame artifact was not found: '$path'."
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
            } catch { $false }
        })
}

function Assert-NoTargetProcess {
    param([Parameter(Mandatory)][string]$ExecutablePath)
    if (@(Get-TargetProcesses -ExecutablePath $ExecutablePath).Count -ne 0) {
        throw "The exact MCLA executable still has a live process: '$ExecutablePath'."
    }
}

function Send-WmCloseToExactGameWindow {
    param([Parameter(Mandatory)][System.Diagnostics.Process]$Process)
    $matches = @()
    foreach ($handle in [MclaFirstFrameNativeWindow]::GetVisibleWindowHandlesForProcess($Process.Id)) {
        $title = [MclaFirstFrameNativeWindow]::GetTitle($handle)
        if ([regex]::IsMatch($title, $gameWindowTitlePattern,
                [System.Text.RegularExpressions.RegexOptions]::CultureInvariant)) {
            $matches += [pscustomobject]@{ Handle = $handle; Title = $title }
        }
    }
    if ($matches.Count -ne 1) {
        throw "Expected exactly one visible exact-PID MCLA game window, found $($matches.Count)."
    }
    if (-not [MclaFirstFrameNativeWindow]::PostClose($matches[0].Handle)) {
        throw "PostMessage(WM_CLOSE) failed for the exact MCLA game window."
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
        try { Stop-Process -Id $Process.Id -Force -ErrorAction Stop } catch {}
    }
    $signaled = $Process.HasExited -or $Process.WaitForExit($cleanupTimeoutMilliseconds)
    $stillPresent = [bool](Get-Process -Id $Process.Id -ErrorAction SilentlyContinue)
    [pscustomobject]@{ ForceIssued = $forceIssued; Cleaned = $signaled -and -not $stillPresent }
}

function Rethrow-WithOwnedCleanup {
    param(
        [Parameter(Mandatory)][System.Management.Automation.ErrorRecord]$Failure,
        [System.Diagnostics.Process]$Process,
        [Parameter(Mandatory)][string]$RunRoot
    )
    $cleanup = Complete-OwnedProcessCleanupAfterFailure -Process $Process
    if (-not $cleanup.Cleaned) {
        throw "First-frame failure left its owned PID alive after the 5-second cleanup bound. Original failure: $($Failure.Exception.Message) Private run: '$RunRoot'."
    }
    if ($cleanup.ForceIssued) {
        throw "First-frame failure required force cleanup; its owned PID was removed within 5 seconds. Original failure: $($Failure.Exception.Message) Private run: '$RunRoot'."
    }
    throw $Failure
}

function Assert-PriorTreesImmutable {
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()]
        [System.Collections.ArrayList]$Snapshots
    )
    foreach ($snapshot in $Snapshots) {
        $current = Get-TreeSnapshot -Root $snapshot.Root
        if ($current.Hash -ne $snapshot.Hash) {
            throw "A completed first-frame cycle changed later: '$($snapshot.Label)'."
        }
    }
}

$buildCandidate = Assert-LexicalContainedPathWithoutReparse -Path $BuildRoot -Description 'Build root'
$gameCandidate = Assert-LexicalContainedPathWithoutReparse -Path $GameRoot -Description 'Game root'
$evidenceParent = Assert-LexicalContainedPathWithoutReparse `
    -Path 'private/evidence/M4-001' -Description 'M4-001 evidence root'
foreach ($path in @($firstFrameVerifier, $gameVerifier, $toolchainResolver,
        (Join-Path $gameCandidate 'default.xex'),
        (Join-Path $repoRoot 'private/game-manifest.json'))) {
    [void](Assert-LexicalContainedPathWithoutReparse -Path $path -Description 'Required input')
}
Assert-ExistingTreeWithoutReparse -Root $buildCandidate -Description 'Build root'
Assert-ExistingTreeWithoutReparse -Root $gameCandidate -Description 'Game root'
Assert-ExistingTreeWithoutReparse -Root $evidenceParent -Description 'M4-001 evidence root'

$buildRootPath = Resolve-ContainedPath -Path $buildCandidate -Description 'Build root'
$gameRootPath = Resolve-ContainedPath -Path $gameCandidate -Description 'Game root'
foreach ($path in @($firstFrameVerifier, $gameVerifier, $toolchainResolver,
        (Join-Path $gameRootPath 'default.xex'))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required first-frame input was not found: '$path'."
    }
}
$executablePath = Join-Path $buildRootPath 'mcla.exe'
Assert-NoTargetProcess -ExecutablePath $executablePath
$gameBefore = Get-GameIdentity -Root $gameRootPath

$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $repoRoot "private/evidence/M4-001/$runId"
[System.IO.Directory]::CreateDirectory($runRoot) | Out-Null
$runsRoot = Join-Path $runRoot 'runs'
[System.IO.Directory]::CreateDirectory($runsRoot) | Out-Null
$resultPath = Join-Path $runRoot 'result.json'
$buildLogPath = Join-Path $runRoot 'relwithdebinfo-clean-build.log'
$cleanBuild = [ordered]@{
    performed = $false
    success = $false
    exit_code = -1
    duration_milliseconds = 0
    build_log_sha256 = ('0' * 64)
    executable_sha256 = ('0' * 64)
}

if (-not $SkipCleanBuild) {
    $canonicalBuildRoot = [System.IO.Path]::GetFullPath(
        (Join-Path $repoRoot 'out/build/win-amd64-relwithdebinfo'))
    if (-not [string]::Equals($buildRootPath, $canonicalBuildRoot,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'The final first-frame gate requires the canonical RelWithDebInfo build root.'
    }
    $toolchain = & $toolchainResolver -ExportPath
    $buildTimer = [System.Diagnostics.Stopwatch]::StartNew()
    & $toolchain.CMakePath --build --preset win-amd64-relwithdebinfo --target mcla `
        --clean-first --parallel *>&1 | Tee-Object -FilePath $buildLogPath | Out-Null
    $buildExit = $LASTEXITCODE
    $buildTimer.Stop()
    if ($buildExit -ne 0) {
        throw "RelWithDebInfo clean-first build failed with exit $buildExit. Private run: '$runRoot'."
    }
    $cleanBuild = [ordered]@{
        performed = $true
        success = $true
        exit_code = 0
        duration_milliseconds = $buildTimer.ElapsedMilliseconds
        build_log_sha256 = (Get-FileHash -LiteralPath $buildLogPath -Algorithm SHA256).Hash
        executable_sha256 = (Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash
    }
} else {
    [System.IO.File]::WriteAllText($buildLogPath, 'development-only: clean build skipped', $utf8)
}

$artifactsBefore = Get-ArtifactSnapshot -Root $buildRootPath
Assert-NoTargetProcess -ExecutablePath $executablePath
$records = @()
$completedTrees = [System.Collections.ArrayList]::new()

for ($cycle = 1; $cycle -le $CycleCount; $cycle++) {
    Assert-PriorTreesImmutable -Snapshots $completedTrees
    $cycleName = '{0:D2}' -f $cycle
    $cycleRoot = Join-Path $runsRoot $cycleName
    $userRoot = Join-Path $cycleRoot 'user'
    $cacheRoot = Join-Path $cycleRoot 'cache'
    [System.IO.Directory]::CreateDirectory($userRoot) | Out-Null
    [System.IO.Directory]::CreateDirectory($cacheRoot) | Out-Null
    $logPath = Join-Path $cycleRoot 'mcla.log'
    $bmpPath = Join-Path $userRoot 'mcla-first-frame.bmp'
    $process = $null
    try {
        $firstFrameTimer = [System.Diagnostics.Stopwatch]::StartNew()
        $process = Start-Process -FilePath $executablePath -ArgumentList @(
            '--mcla_first_frame_probe=true', "--game_data_root=`"$gameRootPath`"",
            "--user_data_root=`"$userRoot`"", "--cache_root=`"$cacheRoot`"",
            "--log_file=`"$logPath`"", '--log_level=trace', '--fullscreen=false'
        ) -WorkingDirectory $buildRootPath -PassThru
        $markerReached = $false
        while ($firstFrameTimer.Elapsed.TotalSeconds -lt $FirstFrameTimeoutSeconds) {
            if ($process.HasExited) {
                throw "First-frame cycle $cycle exited before its frame marker. Private run: '$runRoot'."
            }
            if ((Test-Path -LiteralPath $logPath -PathType Leaf) -and
                (Test-Path -LiteralPath $bmpPath -PathType Leaf)) {
                $text = Get-Content -LiteralPath $logPath -Raw
                if ($text.IndexOf($completionMarker, [System.StringComparison]::Ordinal) -ge 0) {
                    $markerReached = $true
                    break
                }
            }
            Start-Sleep -Milliseconds 250
        }
        $firstFrameTimer.Stop()
        if (-not $markerReached -or
            $firstFrameTimer.ElapsedMilliseconds -gt ($FirstFrameTimeoutSeconds * 1000)) {
            throw "First-frame cycle $cycle missed its 60-second marker/BMP deadline. Private run: '$runRoot'."
        }

        $dwellTimer = [System.Diagnostics.Stopwatch]::StartNew()
        while ($dwellTimer.ElapsedMilliseconds -lt $PostMarkerDwellMilliseconds) {
            $remaining = $PostMarkerDwellMilliseconds - $dwellTimer.ElapsedMilliseconds
            Start-Sleep -Milliseconds ([Math]::Max(1, [int]$remaining))
        }
        $dwellTimer.Stop()
        $process.Refresh()
        if ($process.HasExited) {
            throw "First-frame cycle $cycle could not request WM_CLOSE after dwell. Private run: '$runRoot'."
        }
        Send-WmCloseToExactGameWindow -Process $process
        $exitTimer = [System.Diagnostics.Stopwatch]::StartNew()
        $signaled = $process.WaitForExit($ExitTimeoutSeconds * 1000)
        $exitTimer.Stop()
        if (-not $signaled) {
            throw "First-frame cycle $cycle did not signal controlled exit. Private run: '$runRoot'."
        }
        if ($process.ExitCode -ne 0) {
            throw "First-frame cycle $cycle exited with code $($process.ExitCode). Private run: '$runRoot'."
        }
        Assert-NoTargetProcess -ExecutablePath $executablePath

        $probe = & $firstFrameVerifier -ProbeOnly -RuntimeLogPath $logPath -BmpPath $bmpPath
        $userTree = Get-TreeSnapshot -Root $userRoot
        $cacheTree = Get-TreeSnapshot -Root $cacheRoot
        $metrics = [ordered]@{
            width = $probe.Bmp.Width
            height = $probe.Bmp.Height
            stride = $probe.Bmp.Stride
            pixel_count = $probe.Bmp.PixelCount
            occupied_rgb555_bins = $probe.Bmp.OccupiedRgb555Bins
            luma_p05 = $probe.Bmp.LumaP05
            luma_p95 = $probe.Bmp.LumaP95
            luma_spread = $probe.Bmp.LumaSpread
            modal_pixels = $probe.Bmp.ModalPixels
            modal_per_mille = $probe.Bmp.ModalPermille
            nonmodal_grid_cells = $probe.Bmp.NonmodalGridCells
        }
        $record = [ordered]@{
            index = $cycle
            first_frame_elapsed_milliseconds = $firstFrameTimer.ElapsedMilliseconds
            dwell_elapsed_milliseconds = $dwellTimer.ElapsedMilliseconds
            exit_elapsed_milliseconds = $exitTimer.ElapsedMilliseconds
            exit_code = 0
            startup_marker_count = $probe.Log.StartupMarkerCount
            first_active_refresh_count = $probe.Log.FirstActiveRefreshCount
            present_count_1_sequence = $probe.Log.PresentOneSequence
            present_count_3_sequence = $probe.Log.PresentThreeSequence
            present_count_1_hresult = $probe.Log.PresentOneHresult
            present_count_3_hresult = $probe.Log.PresentThreeHresult
            minimum_successful_guest_present_count = $probe.Log.MinimumSuccessfulPresentCount
            source_width = $probe.Log.SourceWidth
            source_height = $probe.Log.SourceHeight
            swapchain_width = $probe.Log.SwapchainWidth
            swapchain_height = $probe.Log.SwapchainHeight
            capture_sequence = $probe.Log.CaptureSequence
            capture_last_presented_sequence = $probe.Log.CaptureLastPresentedSequence
            capture_success_marker_count = $probe.Log.CaptureSuccessMarkerCount
            capture_width = $probe.Log.CaptureWidth
            capture_height = $probe.Log.CaptureHeight
            present_result_class = $probe.Log.PresentResultClass
            close_requested = $true
            window_close_marker_occurrences = $probe.Log.WindowCloseMarkers
            hard_exit_marker_occurrences = $probe.Log.HardExitMarkers
            post_hard_exit_execution_complete_occurrences =
                $probe.Log.PostHardExitExecutionCompleteMarkers
            harness_force_cleanup = $false
            process_signal_confirmed = $true
            process_cleanup_confirmed = $true
            prior_cycles_immutable = $true
            runtime_log_sha256 = $probe.Log.Sha256
            capture_relative_path = "runs/$cycleName/user/mcla-first-frame.bmp"
            capture_sha256 = $probe.Bmp.Sha256
            capture_bytes = $probe.Bmp.Bytes
            capture_metrics = $metrics
            user_tree_sha256 = $userTree.Hash
            cache_tree_sha256 = $cacheTree.Hash
            user_file_count = $userTree.FileCount
            cache_file_count = $cacheTree.FileCount
            user_bytes = $userTree.Bytes
            cache_bytes = $cacheTree.Bytes
            cycle_tree_sha256 = ('0' * 64)
        }
        $records += $record
    } catch {
        Rethrow-WithOwnedCleanup -Failure $_ -Process $process -RunRoot $runRoot
    }
    $cycleTree = Get-TreeSnapshot -Root $cycleRoot
    $records[$records.Count - 1].cycle_tree_sha256 = $cycleTree.Hash
    [void]$completedTrees.Add([pscustomobject]@{
        Label = "first-frame-$cycleName"
        Root = $cycleRoot
        Hash = $cycleTree.Hash
    })
}

Assert-PriorTreesImmutable -Snapshots $completedTrees
Assert-NoTargetProcess -ExecutablePath $executablePath
$gameAfter = Get-GameIdentity -Root $gameRootPath
$artifactsAfter = Get-ArtifactSnapshot -Root $buildRootPath
$isFinal = $CycleCount -eq 20 -and -not $SkipCleanBuild -and
    $FirstFrameTimeoutSeconds -eq 60 -and $PostMarkerDwellMilliseconds -eq 2000 -and
    $ExitTimeoutSeconds -eq 10
$result = [ordered]@{
    schema = 1
    task = 'M4-001'
    cycle_count = $records.Count
    execution_order = if ($isFinal) {
        'clean_build_then_20_serial_first_frame_cycles'
    } else { 'development_first_frame_cycles' }
    development_only = (-not $isFinal)
    first_frame_timeout_seconds = $FirstFrameTimeoutSeconds
    post_marker_dwell_milliseconds = $PostMarkerDwellMilliseconds
    exit_timeout_seconds = $ExitTimeoutSeconds
    failure_cleanup_timeout_seconds = 5
    clean_build = $cleanBuild
    first_cycle_post_clean_build = (-not $SkipCleanBuild)
    game_identity = [ordered]@{ before = $gameBefore; after = $gameAfter }
    artifacts = [ordered]@{ before = $artifactsBefore; after = $artifactsAfter }
    cycles = $records
    all_write_roots_contained = $true
    all_prior_cycles_immutable = $true
    no_surviving_processes = $true
    data_integrity_preserved = $true
    all_captures_nontrivial = $true
}
[System.IO.File]::WriteAllText(
    $resultPath, (($result | ConvertTo-Json -Depth 10) + [Environment]::NewLine), $utf8)

if ($isFinal) {
    $verified = & $firstFrameVerifier -ResultPath $resultPath
    [pscustomobject]@{
        Passed = $verified.Passed
        Cycles = $verified.Cycles
        PhysicalCapturesVerified = $verified.PhysicalCapturesVerified
        ProcessCleanupVerified = $verified.ProcessCleanupVerified
        DataIntegrityVerified = $verified.DataIntegrityVerified
        PrivateRunRoot = $runRoot
        ResultPath = $resultPath
    }
} else {
    [pscustomobject]@{
        Passed = $false
        DevelopmentOnly = $true
        Cycles = $records.Count
        PrivateRunRoot = $runRoot
        ResultPath = $resultPath
    }
}
