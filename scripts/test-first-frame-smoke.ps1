[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-first-frame-smoke.ps1'
$runner = Join-Path $PSScriptRoot 'run-first-frame-smoke.ps1'
$fixtureRoot = Join-Path $repoRoot (
    'private/evidence/M4-001/test-first-frame-smoke-' + [guid]::NewGuid().ToString('N'))
$resultPath = Join-Path $fixtureRoot 'result.json'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$hashA = 'A' * 64
$hashB = 'B' * 64
$junctionTarget = Join-Path $repoRoot ('private/test-first-frame-target-' + [guid]::NewGuid().ToString('N'))

$startupPrefix = @'
[info] [app] MCLA lifecycle: logging ready
[info] [gpu] MCLA graphics: selected GPU plugin 'xenos'
[info] [sys] GPU plugin 'xenos' loaded (rexgpu-xenosrd.dll)
[info] [ppc] MCLA module config: static image 82000000-829E0000, code 82130000-827CD054, 30034 mappings
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
'@

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

function Set-UInt16LE {
    param([byte[]]$Bytes, [int]$Offset, [uint16]$Value)
    $Bytes[$Offset] = [byte]($Value -band 0xFF)
    $Bytes[$Offset + 1] = [byte](($Value -shr 8) -band 0xFF)
}

function Set-UInt32LE {
    param([byte[]]$Bytes, [int]$Offset, [uint32]$Value)
    for ($index = 0; $index -lt 4; $index++) {
        $Bytes[$Offset + $index] = [byte](($Value -shr (8 * $index)) -band 0xFF)
    }
}

function Write-TestBmp {
    param([Parameter(Mandatory)][string]$Path, [switch]$Solid)
    $width = 64
    $height = 64
    $payload = $width * $height * 4
    [byte[]]$bytes = [byte[]]::new(54 + $payload)
    $bytes[0] = 0x42
    $bytes[1] = 0x4D
    Set-UInt32LE -Bytes $bytes -Offset 2 -Value ([uint32]$bytes.Length)
    Set-UInt32LE -Bytes $bytes -Offset 10 -Value 54
    Set-UInt32LE -Bytes $bytes -Offset 14 -Value 40
    Set-UInt32LE -Bytes $bytes -Offset 18 -Value $width
    Set-UInt32LE -Bytes $bytes -Offset 22 -Value $height
    Set-UInt16LE -Bytes $bytes -Offset 26 -Value 1
    Set-UInt16LE -Bytes $bytes -Offset 28 -Value 32
    Set-UInt32LE -Bytes $bytes -Offset 34 -Value $payload
    for ($y = 0; $y -lt $height; $y++) {
        for ($x = 0; $x -lt $width; $x++) {
            $offset = 54 + (($y * $width + $x) * 4)
            if ($Solid) {
                $red = 32; $green = 32; $blue = 32
            } else {
                $red = $x * 4
                $green = $y * 4
                $blue = ($x + $y) * 2
            }
            $bytes[$offset] = [byte]$blue
            $bytes[$offset + 1] = [byte]$green
            $bytes[$offset + 2] = [byte]$red
            $bytes[$offset + 3] = 255
        }
    }
    [System.IO.Directory]::CreateDirectory((Split-Path -Parent $Path)) | Out-Null
    [System.IO.File]::WriteAllBytes($Path, $bytes)
}

function Get-GradientMetrics {
    $binCounts = [int[]]::new(32768)
    $lumaCounts = [int[]]::new(256)
    for ($y = 0; $y -lt 64; $y++) {
        for ($x = 0; $x -lt 64; $x++) {
            $red = $x * 4
            $green = $y * 4
            $blue = ($x + $y) * 2
            $bin = (($red -shr 3) -shl 10) -bor (($green -shr 3) -shl 5) -bor ($blue -shr 3)
            $binCounts[$bin]++
            $lumaCounts[(54 * $red + 183 * $green + 19 * $blue + 128) -shr 8]++
        }
    }
    $occupied = 0; $modal = 0
    foreach ($count in $binCounts) {
        if ($count -gt 0) { $occupied++ }
        if ($count -gt $modal) { $modal = $count }
    }
    $cumulative = 0; $p05 = -1; $p95 = -1
    for ($value = 0; $value -lt 256; $value++) {
        $cumulative += $lumaCounts[$value]
        if ($p05 -lt 0 -and $cumulative -ge 205) { $p05 = $value }
        if ($p95 -lt 0 -and $cumulative -ge 3892) { $p95 = $value; break }
    }
    [pscustomobject]@{
        Bins = $occupied
        P05 = $p05
        P95 = $p95
        ModalPermille = [int](($modal * 1000) / 4096)
        Cells = 144
    }
}

$gradient = Get-GradientMetrics

function New-PositiveLog {
    @"
$startupPrefix[info] [gpu] D3D12 guest present: successful sequence count=1 sequence=1 source=64x64 swapchain=1280x720 HRESULT=0x00000000
[info] [gpu] D3D12 IssueSwap: first active guest output refresh succeeded source=64x64
[info] [gpu] D3D12 guest capture: success sequence=1 last_presented_sequence=1 dimensions=64x64
[info] [gpu] D3D12 guest present: successful sequence count=3 sequence=3 source=64x64 swapchain=1280x720 HRESULT=0x087A0001
[info] [gpu] D3D12 guest capture: success sequence=3 last_presented_sequence=3 dimensions=64x64
[info] [gpu] MCLA graphics: nontrivial guest frame captured 64x64, rgb555 bins $($gradient.Bins), luma p05 $($gradient.P05), luma p95 $($gradient.P95), modal permille $($gradient.ModalPermille), nonmodal grid cells $($gradient.Cells)
[info] Window closing, shutting down...
[info] Title terminated; hard-exiting process.
[2026-08-11 18:00:00.000] [info] [core] [t1234] Execution complete
"@
}

function Get-TestTreeSnapshot {
    param([Parameter(Mandatory)][string]$Root)
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
    $bytes = [long]0
    foreach ($file in $files) { $bytes += [long]$file.Length }
    [pscustomobject]@{ Hash = $hash; FileCount = $files.Count; Bytes = $bytes }
}

function Write-Result {
    param([Parameter(Mandatory)][object]$Value)
    [System.IO.File]::WriteAllText(
        $resultPath, (($Value | ConvertTo-Json -Depth 10) + [Environment]::NewLine), $utf8)
}

function New-ValidFixture {
    Remove-TestTreeSafely -Root $fixtureRoot
    [System.IO.Directory]::CreateDirectory((Join-Path $fixtureRoot 'runs')) | Out-Null
    $buildLog = Join-Path $fixtureRoot 'relwithdebinfo-clean-build.log'
    [System.IO.File]::WriteAllText($buildLog, 'clean build passed', $utf8)
    $cycles = @()
    for ($cycle = 1; $cycle -le 20; $cycle++) {
        $name = '{0:D2}' -f $cycle
        $cycleRoot = Join-Path $fixtureRoot "runs/$name"
        $userRoot = Join-Path $cycleRoot 'user'
        $cacheRoot = Join-Path $cycleRoot 'cache'
        [System.IO.Directory]::CreateDirectory($userRoot) | Out-Null
        [System.IO.Directory]::CreateDirectory($cacheRoot) | Out-Null
        $logPath = Join-Path $cycleRoot 'mcla.log'
        $bmpPath = Join-Path $userRoot 'mcla-first-frame.bmp'
        Write-TestBmp -Path $bmpPath
        [System.IO.File]::WriteAllText($logPath, (New-PositiveLog), $utf8)
        $probe = & $verifier -ProbeOnly -RuntimeLogPath $logPath -BmpPath $bmpPath
        $userTree = Get-TestTreeSnapshot -Root $userRoot
        $cacheTree = Get-TestTreeSnapshot -Root $cacheRoot
        $cycleTree = Get-TestTreeSnapshot -Root $cycleRoot
        $cycles += [ordered]@{
            index = $cycle
            first_frame_elapsed_milliseconds = 9000
            dwell_elapsed_milliseconds = 2000
            exit_elapsed_milliseconds = 100
            exit_code = 0
            startup_marker_count = $probe.Log.StartupMarkerCount
            first_active_refresh_count = 1
            present_count_1_sequence = 1
            present_count_3_sequence = 3
            present_count_1_hresult = '0x00000000'
            present_count_3_hresult = '0x087A0001'
            minimum_successful_guest_present_count = 3
            source_width = 64
            source_height = 64
            swapchain_width = 1280
            swapchain_height = 720
            capture_sequence = 3
            capture_last_presented_sequence = 3
            capture_success_marker_count = 2
            capture_width = 64
            capture_height = 64
            present_result_class = 'SUCCEEDED'
            close_requested = $true
            window_close_marker_occurrences = 1
            hard_exit_marker_occurrences = 1
            post_hard_exit_execution_complete_occurrences = 1
            harness_force_cleanup = $false
            process_signal_confirmed = $true
            process_cleanup_confirmed = $true
            prior_cycles_immutable = $true
            runtime_log_sha256 = $probe.Log.Sha256
            capture_relative_path = "runs/$name/user/mcla-first-frame.bmp"
            capture_sha256 = $probe.Bmp.Sha256
            capture_bytes = $probe.Bmp.Bytes
            capture_metrics = [ordered]@{
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
            user_tree_sha256 = $userTree.Hash
            cache_tree_sha256 = $cacheTree.Hash
            user_file_count = $userTree.FileCount
            cache_file_count = $cacheTree.FileCount
            user_bytes = $userTree.Bytes
            cache_bytes = $cacheTree.Bytes
            cycle_tree_sha256 = $cycleTree.Hash
        }
    }
    $artifacts = @(
        [ordered]@{ name = 'mcla.exe'; sha256 = $hashA },
        [ordered]@{ name = 'rexruntimerd.dll'; sha256 = $hashA },
        [ordered]@{ name = 'TracyClientrd.dll'; sha256 = $hashA },
        [ordered]@{ name = 'rexgpu-xenosrd.dll'; sha256 = $hashA }
    )
    $value = [ordered]@{
        schema = 1
        task = 'M4-001'
        cycle_count = 20
        execution_order = 'clean_build_then_20_serial_first_frame_cycles'
        development_only = $false
        first_frame_timeout_seconds = 60
        post_marker_dwell_milliseconds = 2000
        exit_timeout_seconds = 10
        failure_cleanup_timeout_seconds = 5
        clean_build = [ordered]@{
            performed = $true
            success = $true
            exit_code = 0
            duration_milliseconds = 1000
            build_log_sha256 = (Get-FileHash -LiteralPath $buildLog -Algorithm SHA256).Hash
            executable_sha256 = $hashA
        }
        first_cycle_post_clean_build = $true
        game_identity = [ordered]@{
            before = [ordered]@{
                file_count = 15
                payload_bytes = 6569586392
                hashes_verified = 15
                manifest_sha256 = $hashA
                tree_sha256 = $hashA
                tree_file_count = 15
                tree_directory_count = 1
                tree_bytes = 6569586392
            }
            after = [ordered]@{
                file_count = 15
                payload_bytes = 6569586392
                hashes_verified = 15
                manifest_sha256 = $hashA
                tree_sha256 = $hashA
                tree_file_count = 15
                tree_directory_count = 1
                tree_bytes = 6569586392
            }
        }
        artifacts = [ordered]@{ before = $artifacts; after = $artifacts }
        cycles = $cycles
        all_write_roots_contained = $true
        all_prior_cycles_immutable = $true
        no_surviving_processes = $true
        data_integrity_preserved = $true
        all_captures_nontrivial = $true
    }
    Write-Result -Value $value
    return $value
}

function Assert-ResultRejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Mutate)
    $candidate = New-ValidFixture | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    & $Mutate $candidate
    Write-Result -Value $candidate
    try { & $verifier -ResultPath $resultPath | Out-Null } catch { return }
    throw "Negative first-frame result fixture '$Name' was accepted."
}

function Assert-PrivacyRejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Mutate)
    $candidate = New-ValidFixture | ConvertTo-Json -Depth 10 | ConvertFrom-Json
    & $Mutate $candidate
    Write-Result -Value $candidate
    try {
        & $verifier -ResultPath $resultPath | Out-Null
    } catch {
        if ($_.Exception.Message -match 'prohibited path pattern') { return }
        throw "Privacy fixture '$Name' was rejected for the wrong reason: $($_.Exception.Message)"
    }
    throw "Negative first-frame privacy fixture '$Name' was accepted."
}

function Assert-EvidenceRejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Mutate)
    $candidate = New-ValidFixture
    & $Mutate $candidate
    Write-Result -Value $candidate
    try { & $verifier -ResultPath $resultPath | Out-Null } catch { return }
    throw "Negative first-frame evidence fixture '$Name' was accepted."
}

function Assert-ProbeRejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Mutate)
    $null = New-ValidFixture
    $bmp = Join-Path $fixtureRoot 'runs/01/user/mcla-first-frame.bmp'
    & $Mutate $bmp
    try {
        & $verifier -ProbeOnly -RuntimeLogPath (Join-Path $fixtureRoot 'runs/01/mcla.log') `
            -BmpPath $bmp | Out-Null
    } catch { return }
    throw "Negative first-frame probe fixture '$Name' was accepted."
}

function Mutate-FirstLog {
    param([Parameter(Mandatory)][scriptblock]$Transform)
    $path = Join-Path $fixtureRoot 'runs/01/mcla.log'
    $text = Get-Content -LiteralPath $path -Raw
    [System.IO.File]::WriteAllText($path, (& $Transform $text), $utf8)
}

try {
    $runnerText = Get-Content -LiteralPath $runner -Raw
    foreach ($required in @(
            '--clean-first', '--mcla_first_frame_probe=true', 'EnumWindows',
            'GetWindowThreadProcessId', 'IsWindowVisible', 'GetWindowText', 'PostMessage',
            '^mcla \[rexglue-v[^\]]+\]$', 'Send-WmCloseToExactGameWindow',
            'WaitForExit($ExitTimeoutSeconds * 1000)', 'Assert-PriorTreesImmutable',
            'Complete-OwnedProcessCleanupAfterFailure', 'CycleCount -eq 20',
            'private/evidence/M4-001', 'mcla-first-frame.bmp'
        )) {
        if ($runnerText.IndexOf($required, [System.StringComparison]::Ordinal) -lt 0) {
            throw "First-frame runner contract is missing '$required'."
        }
    }
    if ($runnerText.IndexOf('CloseMainWindow()', [System.StringComparison]::Ordinal) -ge 0 -or
        $runnerText.IndexOf('.MainWindowHandle', [System.StringComparison]::Ordinal) -ge 0) {
        throw 'First-frame runner must not target the Process.MainWindowHandle helper window.'
    }

    $valid = New-ValidFixture
    $positive = & $verifier -ResultPath $resultPath
    if (-not $positive.Passed -or $positive.Cycles -ne 20 -or
        $positive.PhysicalCapturesVerified -ne 20) {
        throw 'Positive first-frame fixture returned an unexpected result.'
    }

    Assert-ResultRejected nineteen-runs { param($value) $value.cycle_count = 19 }
    Assert-ResultRejected development-only { param($value) $value.development_only = $true }
    Assert-ResultRejected boolean-type-coercion { param($value) $value.development_only = 0 }
    Assert-ResultRejected integer-type-coercion { param($value) $value.cycle_count = '20' }
    Assert-ResultRejected noncanonical-order { param($value) $value.execution_order = 'development_first_frame_cycles' }
    Assert-ResultRejected noncanonical-dwell {
        param($value)
        $value.post_marker_dwell_milliseconds = 5000
        foreach ($cycle in $value.cycles) { $cycle.dwell_elapsed_milliseconds = 5000 }
    }
    Assert-ResultRejected nonzero-exit { param($value) $value.cycles[0].exit_code = 1 }
    Assert-ResultRejected force-cleanup { param($value) $value.cycles[0].harness_force_cleanup = $true }
    Assert-ResultRejected surviving-process { param($value) $value.no_surviving_processes = $false }
    Assert-ResultRejected binary-drift { param($value) $value.artifacts.after[3].sha256 = $hashB }
    Assert-ResultRejected game-tree-drift { param($value) $value.game_identity.after.tree_sha256 = $hashB }
    Assert-ResultRejected capture-hash { param($value) $value.cycles[0].capture_sha256 = $hashB }
    Assert-ResultRejected forged-metrics { param($value) $value.cycles[0].capture_metrics.luma_p95++ }
    Assert-PrivacyRejected drive-private-path {
        param($value) $value.cycles[0].capture_relative_path = 'C:\private\M4-001.bmp'
    }
    Assert-PrivacyRejected relative-private-path {
        param($value) $value.cycles[0].capture_relative_path = 'private\evidence\M4-001.bmp'
    }
    Assert-PrivacyRejected unc-path {
        param($value) $value.cycles[0].capture_relative_path = '\\server\share\frame.bmp'
    }

    Assert-EvidenceRejected present-sequence-order {
        param($value)
        Mutate-FirstLog { param($text) $text.Replace('count=1 sequence=1', 'count=1 sequence=4') }
    }
    Assert-EvidenceRejected present-marker-order {
        param($value)
        Mutate-FirstLog {
            param($text)
            $text.Replace('count=1 sequence=1', 'count=3 sequence=1').Replace(
                'count=3 sequence=3', 'count=1 sequence=3')
        }
    }
    Assert-EvidenceRejected duplicate-present {
        param($value)
        Mutate-FirstLog { param($text) $text.Replace(
            '[info] [gpu] D3D12 guest present: successful sequence count=3',
            "[info] [gpu] D3D12 guest present: successful sequence count=1 sequence=1 source=64x64 swapchain=1280x720`r`n[info] [gpu] D3D12 guest present: successful sequence count=3") }
    }
    Assert-EvidenceRejected ui-only-present {
        param($value)
        Mutate-FirstLog { param($text) $text.Replace(
            '[info] [gpu] D3D12 guest capture:',
            "[warning] D3D12 guest present: UI-only`r`n[info] [gpu] D3D12 guest capture:") }
    }
    Assert-EvidenceRejected failed-present {
        param($value)
        Mutate-FirstLog { param($text) $text.Replace(
            '[info] [gpu] D3D12 guest capture:',
            "[error] D3D12 guest present: failed sequence=2 source=64x64 swapchain=1280x720 HRESULT=0x80004005`r`n[info] [gpu] D3D12 guest capture:") }
    }
    Assert-EvidenceRejected failed-success-hresult {
        param($value)
        Mutate-FirstLog { param($text) $text.Replace(
            'count=3 sequence=3 source=64x64 swapchain=1280x720 HRESULT=0x087A0001',
            'count=3 sequence=3 source=64x64 swapchain=1280x720 HRESULT=0x80004005') }
    }
    Assert-EvidenceRejected failed-refresh {
        param($value)
        Mutate-FirstLog { param($text) $text.Replace(
            '[info] [gpu] D3D12 guest present: successful sequence count=1',
            "[error] IssueSwap: RefreshGuestOutput failed for active 64x64 guest output`r`n[info] [gpu] D3D12 guest present: successful sequence count=1") }
    }
    Assert-EvidenceRejected sequence-overflow {
        param($value)
        Mutate-FirstLog { param($text) $text.Replace(
            '[info] [gpu] D3D12 guest capture:',
            "[critical] Presenter: Active guest output sequence exhausted`r`n[info] [gpu] D3D12 guest capture:") }
    }
    Assert-EvidenceRejected device-loss {
        param($value)
        Mutate-FirstLog { param($text) $text.Replace(
            '[info] [gpu] D3D12 guest capture:',
            "[error] D3D12 guest present: device removed`r`n[info] [gpu] D3D12 guest capture:") }
    }
    Assert-EvidenceRejected unexpected-post-hard-exit-line {
        param($value)
        Mutate-FirstLog { param($text) $text + "[error] unexpected late failure`r`n" }
    }
    Assert-EvidenceRejected capture-sequence-mismatch {
        param($value)
        Mutate-FirstLog { param($text) $text.Replace('capture: success sequence=3', 'capture: success sequence=2') }
    }
    Assert-EvidenceRejected capture-not-presented {
        param($value)
        Mutate-FirstLog { param($text) $text.Replace(
            'sequence=3 last_presented_sequence=3 dimensions=64x64',
            'sequence=3 last_presented_sequence=2 dimensions=64x64') }
    }
    Assert-EvidenceRejected corrupt-bmp {
        param($value)
        $path = Join-Path $fixtureRoot 'runs/01/user/mcla-first-frame.bmp'
        [byte[]]$bytes = [System.IO.File]::ReadAllBytes($path)
        [System.IO.File]::WriteAllBytes($path, ($bytes + [byte]0))
    }
    Assert-ProbeRejected non-32bpp-bmp {
        param($path)
        [byte[]]$bytes = [System.IO.File]::ReadAllBytes($path)
        Set-UInt16LE -Bytes $bytes -Offset 28 -Value 24
        [System.IO.File]::WriteAllBytes($path, $bytes)
    }
    Assert-EvidenceRejected missing-bmp {
        param($value)
        [System.IO.File]::Delete(
            (Join-Path $fixtureRoot 'runs/01/user/mcla-first-frame.bmp'))
    }
    Assert-EvidenceRejected solid-bmp {
        param($value)
        Write-TestBmp -Path (Join-Path $fixtureRoot 'runs/01/user/mcla-first-frame.bmp') -Solid
    }
    Assert-EvidenceRejected prior-tree-mutation {
        param($value)
        [System.IO.File]::WriteAllText(
            (Join-Path $fixtureRoot 'runs/03/user/unexpected.bin'), 'mutation', $utf8)
    }
    Assert-EvidenceRejected empty-directory-mutation {
        param($value)
        [System.IO.Directory]::CreateDirectory(
            (Join-Path $fixtureRoot 'runs/03/user/unexpected-empty-directory')) | Out-Null
    }
    Assert-EvidenceRejected extra-artifact {
        param($value)
        [System.IO.File]::WriteAllText((Join-Path $fixtureRoot 'unexpected.txt'), 'extra', $utf8)
    }
    Assert-EvidenceRejected extra-bmp {
        param($value)
        Write-TestBmp -Path (Join-Path $fixtureRoot 'runs/04/user/duplicate.bmp')
    }

    $reparseCases = 0
    try {
        $candidate = New-ValidFixture
        [System.IO.Directory]::CreateDirectory($junctionTarget) | Out-Null
        $cache = Join-Path $fixtureRoot 'runs/05/cache'
        [System.IO.Directory]::Delete($cache, $true)
        New-Item -ItemType Junction -Path $cache -Target $junctionTarget -ErrorAction Stop | Out-Null
        try {
            & $verifier -ResultPath $resultPath | Out-Null
            throw 'Negative first-frame reparse fixture was accepted.'
        } catch {
            if ($_.Exception.Message -eq 'Negative first-frame reparse fixture was accepted.') { throw }
            $reparseCases++
        }
    } catch [System.UnauthorizedAccessException] {
        if ($runnerText.IndexOf('[System.IO.FileAttributes]::ReparsePoint',
                [System.StringComparison]::Ordinal) -lt 0) {
            throw 'Junction fixtures are unavailable and the runner reparse guard is missing.'
        }
    }

    [pscustomobject]@{
        Passed = $true
        RunnerContractVerified = $true
        PositiveCases = 1
        NegativeCases = 36 + $reparseCases
        ReparseCases = $reparseCases
    }
} finally {
    Remove-TestTreeSafely -Root $fixtureRoot
    Remove-TestTreeSafely -Root $junctionTarget
}
