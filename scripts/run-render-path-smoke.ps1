[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game',
    [ValidateRange(1, 10)][int]$CycleCount = 10,
    [ValidateRange(30, 120)][int]$CaptureTimeoutSeconds = 60,
    [ValidateRange(2000, 10000)][int]$PostMarkerDwellMilliseconds = 2000,
    [ValidateRange(1, 30)][int]$ExitTimeoutSeconds = 10,
    [switch]$SkipCleanBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-render-path-smoke.ps1'
$gameVerifier = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$toolchainResolver = Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1'
$referencePath = Join-Path $repoRoot (
    'private/tools/xenia-canary/artifacts/screenshots/545407F8/' +
    '545407F8 - 2026-08-11T00-59-52.png')
$expectedReferenceSha256 = '7F0293842A6AA30EF0B0EA7C7954FF5130A03ECF6E3A112EEFCAA4A6B11C613E'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$cleanupTimeoutMilliseconds = 5000
$completionMarker = 'MCLA graphics: nontrivial guest frame captured '
$gameWindowTitlePattern = '^mcla \[rexglue-v[^\]]+\]$'

if (-not ('MclaRenderPathNativeWindow' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class MclaRenderPathNativeWindow {
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
    param([string]$Path, [string]$Description)
    $candidate = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repoRoot $Path }
    $fullPath = [System.IO.Path]::GetFullPath($candidate)
    $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $fullPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay lexically inside the repository: '$fullPath'."
    }
    $current = $repoRoot
    $relative = $fullPath.Substring($prefix.Length)
    foreach ($component in @($relative.Split('\') | Where-Object { $_.Length -ne 0 })) {
        $current = Join-Path $current $component
        if ((Test-Path -LiteralPath $current) -and
            ((Get-Item -LiteralPath $current -Force).Attributes -band
                [System.IO.FileAttributes]::ReparsePoint)) {
            throw "$Description traverses a reparse point: '$current'."
        }
    }
    return $fullPath
}

function Assert-ExistingTreeWithoutReparse {
    param([string]$Root, [string]$Description)
    if (-not (Test-Path -LiteralPath $Root)) { return }
    foreach ($item in @((Get-Item -LiteralPath $Root -Force)) +
        @(Get-ChildItem -LiteralPath $Root -Recurse -Force)) {
        if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "$Description contains a reparse point: '$($item.FullName)'."
        }
    }
}

function Resolve-ContainedPath {
    param([string]$Path, [string]$Description)
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if (-not $resolved.StartsWith($repoRoot.TrimEnd('\') + '\',
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay inside the repository: '$resolved'."
    }
    return $resolved
}

function Get-TreeSnapshot {
    param([string]$Root)
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
        $hash = -join ($sha.ComputeHash($utf8.GetBytes($serialized)) |
            ForEach-Object { $_.ToString('X2') })
    } finally { $sha.Dispose() }
    $bytes = 0L
    foreach ($file in $files) { $bytes += [long]$file.Length }
    [pscustomobject]@{ Hash = $hash; FileCount = $files.Count; Bytes = $bytes }
}

function Get-GameIdentity {
    param([string]$Root)
    $tree = Get-TreeSnapshot $Root
    $verified = & $gameVerifier -GamePath $Root -VerifyHashes
    [ordered]@{
        file_count = $verified.FileCount
        payload_bytes = $verified.PayloadBytes
        hashes_verified = $verified.HashesVerified
        manifest_sha256 = (Get-FileHash -LiteralPath $verified.ManifestPath -Algorithm SHA256).Hash
        tree_sha256 = $tree.Hash
        tree_file_count = $tree.FileCount
        tree_directory_count = @(Get-ChildItem -LiteralPath $Root -Recurse -Directory -Force).Count
        tree_bytes = $tree.Bytes
    }
}

function Get-ArtifactSnapshot {
    param([string]$Root)
    @(@('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll') |
        ForEach-Object {
            $path = Join-Path $Root $_
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
                throw "Required render-path artifact was not found: '$path'."
            }
            [ordered]@{ name = $_; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
        })
}

function Get-TargetProcesses {
    param([string]$ExecutablePath)
    @((Get-Process -Name ([System.IO.Path]::GetFileNameWithoutExtension($ExecutablePath)) `
                -ErrorAction SilentlyContinue) | Where-Object {
            try {
                [string]::Equals($_.Path, $ExecutablePath,
                    [System.StringComparison]::OrdinalIgnoreCase)
            } catch { $false }
        })
}

function Assert-NoTargetProcess {
    param([string]$ExecutablePath)
    if (@(Get-TargetProcesses $ExecutablePath).Count -ne 0) {
        throw "The exact MCLA executable still has a live process: '$ExecutablePath'."
    }
}

function Send-WmCloseToExactGameWindow {
    param([System.Diagnostics.Process]$Process)
    $matches = @()
    foreach ($handle in [MclaRenderPathNativeWindow]::GetVisibleWindowHandlesForProcess($Process.Id)) {
        $title = [MclaRenderPathNativeWindow]::GetTitle($handle)
        if ([regex]::IsMatch($title, $gameWindowTitlePattern,
                [System.Text.RegularExpressions.RegexOptions]::CultureInvariant)) {
            $matches += [pscustomobject]@{ Handle = $handle; Title = $title }
        }
    }
    if ($matches.Count -ne 1) {
        throw "Expected exactly one visible exact-PID MCLA game window, found $($matches.Count)."
    }
    if (-not [MclaRenderPathNativeWindow]::PostClose($matches[0].Handle)) {
        throw 'PostMessage(WM_CLOSE) failed for the exact MCLA game window.'
    }
}

function Complete-OwnedProcessCleanupAfterFailure {
    param([System.Diagnostics.Process]$Process)
    if (-not $Process) { return [pscustomobject]@{ ForceIssued = $false; Cleaned = $true } }
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
    param([System.Management.Automation.ErrorRecord]$Failure,
        [System.Diagnostics.Process]$Process, [string]$RunRoot)
    $cleanup = Complete-OwnedProcessCleanupAfterFailure $Process
    if (-not $cleanup.Cleaned) {
        throw "Render-path failure left its owned PID alive. Original: $($Failure.Exception.Message) Private run: '$RunRoot'."
    }
    if ($cleanup.ForceIssued) {
        throw "Render-path failure required force cleanup. Original: $($Failure.Exception.Message) Private run: '$RunRoot'."
    }
    throw $Failure
}

function Assert-PriorTreesImmutable {
    param([System.Collections.ArrayList]$Snapshots)
    foreach ($snapshot in $Snapshots) {
        if ((Get-TreeSnapshot $snapshot.Root).Hash -ne $snapshot.Hash) {
            throw "A completed render-path cycle changed later: '$($snapshot.Label)'."
        }
    }
}

function Get-RuntimeLogTextForWait {
    param([string]$CurrentLogPath)
    $directory = Split-Path -Parent $CurrentLogPath
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) { return '' }
    $files = @()
    foreach ($item in @(Get-ChildItem -LiteralPath $directory -File -Force -Filter 'mcla*.log')) {
        if ($item.Name -ceq 'mcla.log') {
            $files += [pscustomobject]@{ Order = -1; Item = $item }
            continue
        }
        $match = [regex]::Match($item.Name, '^mcla\.(?<index>[1-9][0-9]*)\.log$')
        if ($match.Success) {
            $files += [pscustomobject]@{ Order = [int]$match.Groups['index'].Value; Item = $item }
        }
    }
    $parts = @($files | Sort-Object Order -Descending | ForEach-Object {
            try { [System.IO.File]::ReadAllText($_.Item.FullName) } catch { '' }
        })
    return ($parts -join [Environment]::NewLine)
}

$buildCandidate = Assert-LexicalContainedPathWithoutReparse $BuildRoot 'Build root'
$gameCandidate = Assert-LexicalContainedPathWithoutReparse $GameRoot 'Game root'
$evidenceParent = Assert-LexicalContainedPathWithoutReparse `
    'private/evidence/M4-002' 'M4-002 evidence root'
foreach ($path in @($verifier, $gameVerifier, $toolchainResolver, $referencePath,
        (Join-Path $gameCandidate 'default.xex'),
        (Join-Path $repoRoot 'private/game-manifest.json'))) {
    [void](Assert-LexicalContainedPathWithoutReparse $path 'Required input')
}
Assert-ExistingTreeWithoutReparse $buildCandidate 'Build root'
Assert-ExistingTreeWithoutReparse $gameCandidate 'Game root'
Assert-ExistingTreeWithoutReparse (Split-Path -Parent $evidenceParent) 'Evidence parent'
[System.IO.Directory]::CreateDirectory($evidenceParent) | Out-Null
Assert-ExistingTreeWithoutReparse $evidenceParent 'M4-002 evidence root'

$buildRootPath = Resolve-ContainedPath $buildCandidate 'Build root'
$gameRootPath = Resolve-ContainedPath $gameCandidate 'Game root'
$finalConfigurationRequested = $CycleCount -eq 10 -and -not $SkipCleanBuild -and
    $CaptureTimeoutSeconds -eq 60 -and $PostMarkerDwellMilliseconds -eq 2000 -and
    $ExitTimeoutSeconds -eq 10
if ($finalConfigurationRequested) {
    $canonicalGamePath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot 'private/game'))
    if (-not [string]::Equals($gameRootPath, $canonicalGamePath,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'Final render-path gate requires the canonical private/game tree.'
    }
}
foreach ($path in @($verifier, $gameVerifier, $toolchainResolver, $referencePath,
        (Join-Path $gameRootPath 'default.xex'))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required render-path input was not found: '$path'."
    }
}
$referenceHashBefore = (Get-FileHash -LiteralPath $referencePath -Algorithm SHA256).Hash
if ($referenceHashBefore -ne $expectedReferenceSha256) {
    throw 'Pinned private Xenia title reference SHA-256 mismatch.'
}
$referenceItem = Get-Item -LiteralPath $referencePath
$executablePath = Join-Path $buildRootPath 'mcla.exe'
Assert-NoTargetProcess $executablePath
$gameBefore = Get-GameIdentity $gameRootPath

$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $evidenceParent $runId
$runsRoot = Join-Path $runRoot 'runs'
[System.IO.Directory]::CreateDirectory($runsRoot) | Out-Null
$resultPath = Join-Path $runRoot 'result.json'
$buildLogPath = Join-Path $runRoot 'relwithdebinfo-clean-build.log'
$cleanBuild = [ordered]@{
    performed = $false; success = $false; exit_code = -1; duration_milliseconds = 0
    build_log_sha256 = ('0' * 64); executable_sha256 = ('0' * 64)
}

if (-not $SkipCleanBuild) {
    $canonical = [System.IO.Path]::GetFullPath(
        (Join-Path $repoRoot 'out/build/win-amd64-relwithdebinfo'))
    if (-not [string]::Equals($buildRootPath, $canonical,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'Final render-path gate requires the canonical RelWithDebInfo build root.'
    }
    $toolchain = & $toolchainResolver -ExportPath
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    Push-Location $repoRoot
    try {
        & $toolchain.CMakePath --preset win-amd64-relwithdebinfo *>&1 |
            Tee-Object -FilePath $buildLogPath | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "RelWithDebInfo configure failed with exit $LASTEXITCODE."
        }
        & $toolchain.CMakePath --build --preset win-amd64-relwithdebinfo --target mcla `
            --clean-first --parallel *>&1 | Tee-Object -FilePath $buildLogPath -Append | Out-Null
        $exitCode = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    $timer.Stop()
    if ($exitCode -ne 0) {
        throw "RelWithDebInfo clean-first build failed with exit $exitCode. Private run: '$runRoot'."
    }
    $cleanBuild = [ordered]@{
        performed = $true; success = $true; exit_code = 0
        duration_milliseconds = $timer.ElapsedMilliseconds
        build_log_sha256 = (Get-FileHash -LiteralPath $buildLogPath -Algorithm SHA256).Hash
        executable_sha256 = (Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash
    }
} else {
    [System.IO.File]::WriteAllText($buildLogPath, 'development-only: clean build skipped', $utf8)
}

$artifactsBefore = Get-ArtifactSnapshot $buildRootPath
$records = @()
$completedTrees = [System.Collections.ArrayList]::new()
for ($cycle = 1; $cycle -le $CycleCount; $cycle++) {
    Assert-PriorTreesImmutable $completedTrees
    Assert-NoTargetProcess $executablePath
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
        $captureTimer = [System.Diagnostics.Stopwatch]::StartNew()
        $process = Start-Process -FilePath $executablePath -ArgumentList @(
            '--gpu_render_audit=true', '--async_shader_compilation=false',
            '--mcla_first_frame_probe=true', '--mcla_first_frame_settle_seconds=35',
            '--render_target_path_d3d12=rtv',
            "--game_data_root=`"$gameRootPath`"", "--user_data_root=`"$userRoot`"",
            "--cache_root=`"$cacheRoot`"", "--log_file=`"$logPath`"",
            '--log_level=trace', '--fullscreen=false'
        ) -WorkingDirectory $buildRootPath -PassThru
        $captured = $false
        while ($captureTimer.Elapsed.TotalSeconds -lt $CaptureTimeoutSeconds) {
            if ($process.HasExited) {
                throw "Render-path cycle $cycle exited before capture. Private run: '$runRoot'."
            }
            if ((Test-Path -LiteralPath $logPath -PathType Leaf) -and
                (Test-Path -LiteralPath $bmpPath -PathType Leaf)) {
                $text = Get-RuntimeLogTextForWait -CurrentLogPath $logPath
                if ($text.IndexOf($completionMarker, [System.StringComparison]::Ordinal) -ge 0) {
                    $captured = $true
                    break
                }
            }
            Start-Sleep -Milliseconds 250
        }
        $captureTimer.Stop()
        if (-not $captured -or $captureTimer.ElapsedMilliseconds -gt
            ($CaptureTimeoutSeconds * 1000)) {
            throw "Render-path cycle $cycle missed its capture deadline. Private run: '$runRoot'."
        }
        $dwellTimer = [System.Diagnostics.Stopwatch]::StartNew()
        while ($dwellTimer.ElapsedMilliseconds -lt $PostMarkerDwellMilliseconds) {
            $remaining = $PostMarkerDwellMilliseconds - $dwellTimer.ElapsedMilliseconds
            Start-Sleep -Milliseconds ([Math]::Max(1, [int]$remaining))
        }
        $dwellTimer.Stop()
        $process.Refresh()
        if ($process.HasExited) { throw "Render-path cycle $cycle exited during dwell." }
        Send-WmCloseToExactGameWindow $process
        $exitTimer = [System.Diagnostics.Stopwatch]::StartNew()
        $signaled = $process.WaitForExit($ExitTimeoutSeconds * 1000)
        $exitTimer.Stop()
        if (-not $signaled) { throw "Render-path cycle $cycle did not signal controlled exit." }
        if ($process.ExitCode -ne 0) {
            throw "Render-path cycle $cycle exited with code $($process.ExitCode)."
        }
        Assert-NoTargetProcess $executablePath

        $probe = & $verifier -ProbeOnly -RuntimeLogPath $logPath -BmpPath $bmpPath `
            -ReferencePngPath $referencePath
        $userTree = Get-TreeSnapshot $userRoot
        $cacheTree = Get-TreeSnapshot $cacheRoot
        $metrics = [ordered]@{
            width = $probe.Bmp.Width; height = $probe.Bmp.Height; stride = $probe.Bmp.Stride
            pixel_count = $probe.Bmp.PixelCount
            occupied_rgb555_bins = $probe.Bmp.OccupiedRgb555Bins
            luma_p05 = $probe.Bmp.LumaP05; luma_p95 = $probe.Bmp.LumaP95
            luma_spread = $probe.Bmp.LumaSpread; modal_pixels = $probe.Bmp.ModalPixels
            modal_per_mille = $probe.Bmp.ModalPermille
            nonmodal_grid_cells = $probe.Bmp.NonmodalGridCells
        }
        $audit = [ordered]@{
            config_count = $probe.Audit.ConfigCount; rt_records = $probe.Audit.RtRecords
            bind_records = $probe.Audit.BindRecords
            ownership_modes = $probe.Audit.OwnershipModes
            resolve_records = $probe.Audit.ResolveRecords; resolve_calls = $probe.Audit.ResolveCalls
            shader_records = $probe.Audit.ShaderRecords; pso_records = $probe.Audit.PsoRecords
            pso_ok = $probe.Audit.PsoOk; draw_issued = $probe.Audit.DrawIssued
            depth_test = $probe.Audit.DepthTest; depth_write = $probe.Audit.DepthWrite
            msaa1 = $probe.Audit.Msaa1; msaa2 = $probe.Audit.Msaa2; msaa4 = $probe.Audit.Msaa4
            gamma_nonidentity = $probe.Audit.GammaNonidentity
            gamma_uploads = $probe.Audit.GammaUploads; summary_count = $probe.Audit.SummaryCount
        }
        $records += [ordered]@{
            index = $cycle; capture_elapsed_milliseconds = $captureTimer.ElapsedMilliseconds
            dwell_elapsed_milliseconds = $dwellTimer.ElapsedMilliseconds
            exit_elapsed_milliseconds = $exitTimer.ElapsedMilliseconds; exit_code = 0
            startup_marker_count = $probe.Log.StartupMarkerCount
            present_count_1_sequence = $probe.Log.PresentOneSequence
            present_count_3_sequence = $probe.Log.PresentThreeSequence
            present_count_1_hresult = $probe.Log.PresentOneHresult
            present_count_3_hresult = $probe.Log.PresentThreeHresult
            capture_sequence = $probe.Log.CaptureSequence
            capture_last_presented_sequence = $probe.Log.CaptureLastPresentedSequence
            capture_success_marker_count = $probe.Log.CaptureSuccessMarkerCount
            window_close_marker_occurrences = $probe.Log.WindowCloseMarkers
            execution_complete_marker_occurrences = $probe.Log.ExecutionCompleteMarkers
            hard_exit_marker_occurrences = $probe.Log.HardExitMarkers
            post_hard_exit_execution_complete_occurrences =
                $probe.Log.PostHardExitExecutionCompleteMarkers
            close_requested = $true; harness_force_cleanup = $false
            process_signal_confirmed = $true; process_cleanup_confirmed = $true
            prior_cycles_immutable = $true
            runtime_logs = @($probe.Log.RuntimeLogs)
            runtime_log_file_count = $probe.Log.RuntimeLogFileCount
            runtime_log_bytes = $probe.Log.RuntimeLogBytes
            runtime_log_set_sha256 = $probe.Log.RuntimeLogSetSha256
            capture_relative_path = "runs/$cycleName/user/mcla-first-frame.bmp"
            capture_sha256 = $probe.Bmp.Sha256; capture_bytes = $probe.Bmp.Bytes
            capture_metrics = $metrics
            logo_edge_correlation_ppm = $probe.Roi.LogoCorrelationPpm
            press_edge_correlation_ppm = $probe.Roi.PressCorrelationPpm
            title_route_verified = $true; audit = $audit
            user_tree_sha256 = $userTree.Hash; cache_tree_sha256 = $cacheTree.Hash
            user_file_count = $userTree.FileCount; cache_file_count = $cacheTree.FileCount
            user_bytes = $userTree.Bytes; cache_bytes = $cacheTree.Bytes
            cycle_tree_sha256 = ('0' * 64)
        }
    } catch {
        Rethrow-WithOwnedCleanup -Failure $_ -Process $process -RunRoot $runRoot
    }
    $cycleTree = Get-TreeSnapshot $cycleRoot
    $records[$records.Count - 1].cycle_tree_sha256 = $cycleTree.Hash
    [void]$completedTrees.Add([pscustomobject]@{
        Label = "render-path-$cycleName"; Root = $cycleRoot; Hash = $cycleTree.Hash
    })
}

Assert-PriorTreesImmutable $completedTrees
Assert-NoTargetProcess $executablePath
$gameAfter = Get-GameIdentity $gameRootPath
$artifactsAfter = Get-ArtifactSnapshot $buildRootPath
$referenceHashAfter = (Get-FileHash -LiteralPath $referencePath -Algorithm SHA256).Hash
if ($referenceHashAfter -ne $referenceHashBefore) {
    throw 'Pinned private Xenia title reference changed during the render-path gate.'
}
$isFinal = $finalConfigurationRequested
$result = [ordered]@{
    schema = 1; task = 'M4-002'; cycle_count = $records.Count
    execution_order = if ($isFinal) {
        'clean_build_then_10_serial_render_path_cycles'
    } else { 'development_render_path_cycles' }
    development_only = (-not $isFinal)
    capture_timeout_seconds = $CaptureTimeoutSeconds
    first_frame_settle_seconds = 35
    post_marker_dwell_milliseconds = $PostMarkerDwellMilliseconds
    exit_timeout_seconds = $ExitTimeoutSeconds; failure_cleanup_timeout_seconds = 5
    clean_build = $cleanBuild; first_cycle_post_clean_build = (-not $SkipCleanBuild)
    reference = [ordered]@{
        name = 'xenia-title-reference.png'; sha256 = $referenceHashBefore
        bytes = $referenceItem.Length; width = 1280; height = 720
    }
    game_identity = [ordered]@{ before = $gameBefore; after = $gameAfter }
    artifacts = [ordered]@{ before = $artifactsBefore; after = $artifactsAfter }
    cycles = $records
    all_write_roots_contained = $true; all_prior_cycles_immutable = $true
    no_surviving_processes = $true; data_integrity_preserved = $true
    all_captures_bound = $true; all_title_rois_match = $true
    all_render_audits_passed = $true
}
[System.IO.File]::WriteAllText($resultPath,
    (($result | ConvertTo-Json -Depth 10) + [Environment]::NewLine), $utf8)

if ($isFinal) {
    $verified = & $verifier -ResultPath $resultPath
    [pscustomobject]@{
        Passed = $verified.Passed; Cycles = $verified.Cycles
        PhysicalCapturesVerified = $verified.PhysicalCapturesVerified
        RenderAuditsVerified = $verified.RenderAuditsVerified
        ProcessCleanupVerified = $verified.ProcessCleanupVerified
        DataIntegrityVerified = $verified.DataIntegrityVerified
        PrivateRunRoot = $runRoot; ResultPath = $resultPath
    }
} else {
    [pscustomobject]@{
        Passed = $false; DevelopmentOnly = $true; Cycles = $records.Count
        PrivateRunRoot = $runRoot; ResultPath = $resultPath
    }
}
