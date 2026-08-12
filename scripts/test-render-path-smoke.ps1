[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-render-path-smoke.ps1'
$runner = Join-Path $PSScriptRoot 'run-render-path-smoke.ps1'
$gameVerifier = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$reference = Join-Path $repoRoot (
    'private/tools/xenia-canary/artifacts/screenshots/545407F8/' +
    '545407F8 - 2026-08-11T00-59-52.png')
$fixtureRoot = Join-Path $repoRoot (
    'private/evidence/M4-002/test-render-path-smoke-' + [guid]::NewGuid().ToString('N'))
$resultPath = Join-Path $fixtureRoot 'result.json'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$hashB = 'B' * 64
$rejections = 0

$startupPrefix = @'
[info] [app] MCLA lifecycle: logging ready
[info] [gpu] MCLA graphics: selected GPU plugin 'xenos'
[info] [sys] GPU plugin 'xenos' loaded (rexgpu-xenosrd.dll)
[info] [ppc] MCLA module config: static image 82000000-829E0000, code 82130000-827CD054, 30025 mappings
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
    param([string]$Root)
    if (-not (Test-Path -LiteralPath $Root -PathType Container)) { return }
    $reparseItems = @(Get-ChildItem -LiteralPath $Root -Recurse -Force |
        Where-Object { $_.Attributes -band [System.IO.FileAttributes]::ReparsePoint } |
        Sort-Object { $_.FullName.Length } -Descending)
    foreach ($item in $reparseItems) {
        if ($item.PSIsContainer) { [System.IO.Directory]::Delete($item.FullName, $false) }
        else { [System.IO.File]::Delete($item.FullName) }
    }
    [System.IO.Directory]::Delete($Root, $true)
}

function Get-TestTreeSnapshot {
    param([string]$Root)
    $items = @(Get-ChildItem -LiteralPath $Root -Recurse -Force)
    $entries = @()
    foreach ($directory in @($items | Where-Object { $_.PSIsContainer } | Sort-Object FullName)) {
        $entries += [ordered]@{
            kind = 'directory'
            path = $directory.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/')
        }
    }
    $files = @($items | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)
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
    [pscustomobject]@{
        Hash = $hash
        FileCount = $files.Count
        DirectoryCount = @($items | Where-Object { $_.PSIsContainer }).Count
        Bytes = $bytes
    }
}

function Get-TestBmpMetrics {
    param([string]$Path)
    [byte[]]$bytes = [System.IO.File]::ReadAllBytes($Path)
    $width = [BitConverter]::ToInt32($bytes, 18)
    $signedHeight = [BitConverter]::ToInt32($bytes, 22)
    $height = [Math]::Abs($signedHeight)
    $offset = [BitConverter]::ToUInt32($bytes, 10)
    $stride = $width * 4
    $bins = [int[]]::new(32768)
    $lumas = [int[]]::new(256)
    $quantized = [int[]]::new($width * $height)
    $pixelIndex = 0
    for ($row = 0; $row -lt $height; $row++) {
        $sourceRow = if ($signedHeight -gt 0) { $height - 1 - $row } else { $row }
        for ($x = 0; $x -lt $width; $x++) {
            $position = $offset + $sourceRow * $stride + $x * 4
            $b = [int]$bytes[$position]
            $g = [int]$bytes[$position + 1]
            $r = [int]$bytes[$position + 2]
            $bin = (($r -shr 3) -shl 10) -bor (($g -shr 3) -shl 5) -bor ($b -shr 3)
            $quantized[$pixelIndex++] = $bin
            $bins[$bin]++
            $lumas[((54 * $r + 183 * $g + 19 * $b + 128) -shr 8)]++
        }
    }
    $occupied = 0
    $modalBin = 0
    $modalPixels = 0
    for ($index = 0; $index -lt $bins.Count; $index++) {
        if ($bins[$index] -gt 0) {
            $occupied++
            if ($bins[$index] -gt $modalPixels) { $modalPixels = $bins[$index]; $modalBin = $index }
        }
    }
    $targets = @(46080, 875520)
    $values = @(-1, -1)
    $total = 0L
    for ($luma = 0; $luma -lt 256; $luma++) {
        $total += $lumas[$luma]
        for ($p = 0; $p -lt 2; $p++) {
            if ($values[$p] -lt 0 -and $total -ge $targets[$p]) { $values[$p] = $luma }
        }
    }
    $cells = [bool[]]::new(144)
    for ($y = 0; $y -lt $height; $y++) {
        for ($x = 0; $x -lt $width; $x++) {
            if ($quantized[$y * $width + $x] -ne $modalBin) {
                $gx = [Math]::Min(15, [int][Math]::Floor(
                        ([double]([long]$x * 16)) / ([double]$width)))
                $gy = [Math]::Min(8, [int][Math]::Floor(
                        ([double]([long]$y * 9)) / ([double]$height)))
                $cells[$gy * 16 + $gx] = $true
            }
        }
    }
    [pscustomobject]@{
        Width = $width; Height = $height; Stride = $stride; PixelCount = $width * $height
        Occupied = $occupied; P05 = $values[0]; P95 = $values[1]
        Spread = $values[1] - $values[0]; ModalPixels = $modalPixels; ModalBin = $modalBin
        ModalPermille = [int][Math]::Floor(
            ([double]([long]$modalPixels * 1000)) / ([double]($width * $height)))
        Cells = @($cells | Where-Object { $_ }).Count
    }
}

function Set-TestCaptureModalFloorDiscriminator {
    param([string]$Path)
    $before = Get-TestBmpMetrics $Path
    $removeCount = -1
    for ($candidate = 0; $candidate -le 2048 -and $candidate -lt $before.ModalPixels; $candidate++) {
        $candidatePixels = $before.ModalPixels - $candidate
        $ratio = ([double]([long]$candidatePixels * 1000)) / ([double]$before.PixelCount)
        if ($ratio -ne [Math]::Floor($ratio) -and [int]$ratio -ne [int][Math]::Floor($ratio)) {
            $removeCount = $candidate
            break
        }
    }
    if ($removeCount -lt 0) {
        throw 'Could not construct a bounded modal-floor discriminator.'
    }
    if ($removeCount -eq 0) { return }

    [byte[]]$bytes = [System.IO.File]::ReadAllBytes($Path)
    $width = [BitConverter]::ToInt32($bytes, 18)
    $signedHeight = [BitConverter]::ToInt32($bytes, 22)
    $height = [Math]::Abs($signedHeight)
    $offset = [BitConverter]::ToUInt32($bytes, 10)
    $stride = $width * 4
    $changed = 0
    for ($y = 0; $y -lt $height -and $changed -lt $removeCount; $y++) {
        $insideLogoY = $y -ge 230 -and $y -lt 430
        $insidePressY = $y -ge 630 -and $y -lt 675
        $sourceRow = if ($signedHeight -gt 0) { $height - 1 - $y } else { $y }
        for ($x = 0; $x -lt $width -and $changed -lt $removeCount; $x++) {
            if (($insideLogoY -and $x -ge 280 -and $x -lt 1010) -or
                ($insidePressY -and $x -ge 1080 -and $x -lt 1185)) {
                continue
            }
            $position = $offset + $sourceRow * $stride + $x * 4
            $bin = (([int]$bytes[$position + 2] -shr 3) -shl 10) -bor
                (([int]$bytes[$position + 1] -shr 3) -shl 5) -bor
                ([int]$bytes[$position] -shr 3)
            if ($bin -eq $before.ModalBin) {
                $bytes[$position] = $bytes[$position] -bxor 0xF8
                $changed++
            }
        }
    }
    if ($changed -ne $removeCount) {
        throw 'Could not mutate enough modal pixels outside the fixed ROIs.'
    }
    [System.IO.File]::WriteAllBytes($Path, $bytes)
}

function New-TestCapture {
    param([string]$Path)
    Add-Type -AssemblyName System.Drawing
    $source = [System.Drawing.Image]::FromFile($reference)
    $bitmap = [System.Drawing.Bitmap]::new(1280, 720,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.DrawImageUnscaled($source, 0, 0)
        # Deliberately change animated-background pixels outside both fixed ROIs.
        $graphics.FillRectangle([System.Drawing.Brushes]::Magenta, 10, 500, 180, 180)
        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Bmp)
    } finally {
        $graphics.Dispose(); $bitmap.Dispose(); $source.Dispose()
    }
}

function New-AuditLines {
    $detail = [System.Collections.Generic.List[string]]::new()
    $detail.Add('XENOS_AUDIT_CONFIG v=1 backend=d3d12 rt_path=host bindless=1 scale_x=1 scale_y=1 native_2x_supported=1 gamma_rt_unorm16=1 depth_f24_ps=0 depth_f24_round=0 direct_host_resolve=0')
    $colorFormats = @(0, 1, 2, 3, 4, 5, 6, 7, 10, 12)
    $samples = @(1, 2, 4)
    for ($id = 0; $id -lt 20; $id++) {
        $sample = $samples[$id % 3]
        if (($id % 2) -eq 0) {
            $format = $colorFormats[($id -shr 1) % $colorFormats.Count]
            $detail.Add("XENOS_AUDIT_RT v=1 id=$id kind=color storage_fmt=$format guest_msaa=$sample host_samples=$sample resource_dxgi=28 draw_dxgi=28 ownership_dxgi=28 depth_srv_dxgi=-1 stencil_srv_dxgi=-1 emulated_2x=0")
        } else {
            $format = ($id -shr 1) % 2
            $detail.Add("XENOS_AUDIT_RT v=1 id=$id kind=depth storage_fmt=$format guest_msaa=$sample host_samples=$sample resource_dxgi=45 draw_dxgi=-1 ownership_dxgi=-1 depth_srv_dxgi=46 stencil_srv_dxgi=47 emulated_2x=0")
        }
    }
    $detail.Add('XENOS_AUDIT_BIND v=1 id=0 slot=color0 guest_fmt=0 storage_fmt=0 guest_msaa=1 depth_test=0 depth_write=0 stencil=0 host_bound=1')
    $detail.Add('XENOS_AUDIT_BIND v=1 id=1 slot=depth guest_fmt=0 storage_fmt=0 guest_msaa=1 depth_test=1 depth_write=1 stencil=0 host_bound=1')
    $detail.Add('XENOS_AUDIT_BIND v=1 id=2 slot=color0 guest_fmt=3 storage_fmt=2 guest_msaa=2 depth_test=0 depth_write=0 stencil=0 host_bound=1')
    $detail.Add('XENOS_AUDIT_BIND v=1 id=3 slot=depth guest_fmt=1 storage_fmt=1 guest_msaa=2 depth_test=1 depth_write=1 stencil=1 host_bound=1')
    $detail.Add('XENOS_AUDIT_BIND v=1 id=4 slot=color0 guest_fmt=7 storage_fmt=7 guest_msaa=4 depth_test=0 depth_write=0 stencil=0 host_bound=1')
    $detail.Add('XENOS_AUDIT_BIND v=1 id=5 slot=depth guest_fmt=0 storage_fmt=0 guest_msaa=4 depth_test=1 depth_write=1 stencil=0 host_bound=1')
    $detail.Add('XENOS_AUDIT_OWNERSHIP v=1 id=0 mode=7 src_guest_msaa=2 dst_guest_msaa=2 src_host_samples=2 dst_host_samples=2')
    $detail.Add('XENOS_AUDIT_RESOLVE v=1 id=0 src_kind=color src_fmt=7 guest_msaa=4 dest_fmt=6 shader=7 scaled=0')
    $detail.Add('XENOS_AUDIT_RESOLVE v=1 id=1 src_kind=depth src_fmt=0 guest_msaa=2 dest_fmt=6 shader=6 scaled=0')
    $detail.Add('XENOS_AUDIT_RESOLVE v=1 id=2 src_kind=color src_fmt=0 guest_msaa=1 dest_fmt=6 shader=6 scaled=0')
    $detail.Add('XENOS_AUDIT_SHADER v=1 id=0 stage=vs ucode=1111111111111111 modification=0000000000000000 result=ok')
    $detail.Add('XENOS_AUDIT_SHADER v=1 id=1 stage=ps ucode=2222222222222222 modification=0000000000000001 result=ok')
    $detail.Add('XENOS_AUDIT_PSO v=1 id=0 desc=3333333333333333 vs=1111111111111111 ps=2222222222222222 host_msaa=4 depth_fmt=1 depth_func=3 depth_write=1 result=ok hresult=00000000')
    $detail.Add('XENOS_AUDIT_GAMMA v=1 id=0 mode=pwl mode_source=frontbuffer_format_heuristic fb_fmt=7 luta_control=00000001 identity=0 upload=1 dispatch=1')
    $summary = @(
        'XENOS_AUDIT_RT_SUMMARY v=1 phase=checkpoint create_attempt=20 create_ok=20 create_fail=0 records=20 overflow=0 host_depth_store=1 ownership_draws=10 ownership_modes=1 ownership_overflow=0',
        'XENOS_AUDIT_RESOLVE_SUMMARY v=1 phase=checkpoint calls=10000 info_ok=10000 info_fail=0 zero_area=0 shader_known=10000 shader_unknown=0 direct_preflight=0 direct_preflight_dump_ok=0 direct_reject=0 fallback_dump_ok=10000 fallback_dump_fail=0 copy_dispatch=10000 final_ok=10000 final_fail=0 modes=3 overflow=0 true_direct_dispatch=0',
        'XENOS_AUDIT_PIPELINE_SUMMARY v=1 phase=checkpoint shader_entries=2 translate_vs_ok=1 translate_ps_ok=1 translate_fail=0 pso_entries=100 pso_attempt=100 pso_ok=100 pso_fail=0 shader_records=2 shader_overflow=0 pso_records=1 pso_overflow=0',
        'XENOS_AUDIT_CP_SUMMARY v=1 phase=checkpoint draw_issued=1000 draw_indexed=700 draw_nonindexed=300 pso_pending_skip=0 pso_failed_skip=0 depth_test=600 depth_write=500 stencil=20 depth_bound=700 depth_without_bound=10 bind_records=6 bind_overflow=0 msaa1=100 msaa2=200 msaa4=700 gamma_table_dispatch=0 gamma_pwl_dispatch=20 gamma_identity_dispatch=0 gamma_nonidentity_dispatch=20 gamma_table_writes=0 gamma_pwl_writes=2 gamma_uploads=2 gamma_records=1 gamma_overflow=0 refresh_fail=0'
    )
    [pscustomobject]@{ Detail = @($detail); Summary = $summary }
}

function Assert-ProbeRejected {
    param([string]$Name, [scriptblock]$Mutator)
    $mutated = & $Mutator $script:validLog
    $negativeRoot = Join-Path $fixtureRoot "negative-$Name"
    [System.IO.Directory]::CreateDirectory($negativeRoot) | Out-Null
    $path = Join-Path $negativeRoot 'mcla.log'
    [System.IO.File]::WriteAllText($path, $mutated, $utf8)
    try {
        & $verifier -ProbeOnly -RuntimeLogPath $path -BmpPath $script:templateBmp `
            -ReferencePngPath $reference | Out-Null
        throw "Negative probe '$Name' was accepted."
    } catch {
        if ($_.Exception.Message -eq "Negative probe '$Name' was accepted.") { throw }
        $script:rejections++
    } finally { [System.IO.Directory]::Delete($negativeRoot, $true) }
}

function Assert-OutOfOrderConcurrentIdsAccepted {
    $first = 'XENOS_AUDIT_SHADER v=1 id=0 stage=vs ucode=1111111111111111 modification=0000000000000000 result=ok'
    $second = 'XENOS_AUDIT_SHADER v=1 id=1 stage=ps ucode=2222222222222222 modification=0000000000000001 result=ok'
    $mutated = $script:validLog.Replace($first, '__SHADER_ZERO__').Replace($second, $first).
        Replace('__SHADER_ZERO__', $second)
    $root = Join-Path $fixtureRoot 'concurrent-order-positive'
    [System.IO.Directory]::CreateDirectory($root) | Out-Null
    $path = Join-Path $root 'mcla.log'
    [System.IO.File]::WriteAllText($path, $mutated, $utf8)
    try {
        $value = & $verifier -ProbeOnly -RuntimeLogPath $path -BmpPath $script:templateBmp `
            -ReferencePngPath $reference
        if ($value.Audit.ShaderRecords -ne 2) {
            throw 'Out-of-order concurrent shader IDs did not retain their exact set.'
        }
    } finally { [System.IO.Directory]::Delete($root, $true) }
}

function Assert-BenignPostHardExitTraceAccepted {
    $mutated = $script:validLog + [Environment]::NewLine +
        '[trace] [gpu] ResolveCopy: benign in-flight trace after hard exit' +
        [Environment]::NewLine + '[trace] [gpu] MakeResident: benign completion' +
        [Environment]::NewLine + '[trace] [gpu] Loaded pipeline cache entry'
    $root = Join-Path $fixtureRoot 'post-hard-exit-trace-positive'
    [System.IO.Directory]::CreateDirectory($root) | Out-Null
    $path = Join-Path $root 'mcla.log'
    [System.IO.File]::WriteAllText($path, $mutated, $utf8)
    try {
        $value = & $verifier -ProbeOnly -RuntimeLogPath $path -BmpPath $script:templateBmp `
            -ReferencePngPath $reference
        if ($value.Log.ExecutionCompleteMarkers -ne 1 -or
            $value.Log.PostHardExitExecutionCompleteMarkers -ne 0) {
            throw 'Benign post-hard-exit trace changed lifecycle marker cardinality.'
        }
    } finally { [System.IO.Directory]::Delete($root, $true) }
}

function Get-Emulated2xFallbackLog {
    param([string]$LogText)
    $value = $LogText.Replace('native_2x_supported=1', 'native_2x_supported=0').Replace(
        'src_host_samples=2 dst_host_samples=2', 'src_host_samples=4 dst_host_samples=4')
    return [regex]::Replace($value,
        '(?m)^(XENOS_AUDIT_RT[^\r\n]* guest_msaa=2 )host_samples=2([^\r\n]* )emulated_2x=0(?=\r?$)',
        '${1}host_samples=4${2}emulated_2x=1')
}

function Assert-Emulated2xFallbackAccepted {
    $mutated = Get-Emulated2xFallbackLog $script:validLog
    $root = Join-Path $fixtureRoot 'emulated-2x-positive'
    [System.IO.Directory]::CreateDirectory($root) | Out-Null
    $path = Join-Path $root 'mcla.log'
    [System.IO.File]::WriteAllText($path, $mutated, $utf8)
    try {
        $value = & $verifier -ProbeOnly -RuntimeLogPath $path -BmpPath $script:templateBmp `
            -ReferencePngPath $reference
        if ($value.Audit.RtRecords -ne 20 -or $value.Audit.OwnershipModes -ne 1) {
            throw 'Emulated 2x fallback did not retain RT and ownership coverage.'
        }
    } finally { [System.IO.Directory]::Delete($root, $true) }
}

function Get-DirectResolveLog {
    param([string]$LogText, [int]$RejectedCalls = 0)
    $directOk = 10000 - $RejectedCalls
    $value = $LogText.Replace('direct_host_resolve=0', 'direct_host_resolve=1')
    return $value.Replace(
        'direct_preflight=0 direct_preflight_dump_ok=0 direct_reject=0 fallback_dump_ok=10000',
        "direct_preflight=10000 direct_preflight_dump_ok=$directOk direct_reject=$RejectedCalls fallback_dump_ok=$RejectedCalls")
}

function Assert-DirectResolveAccepted {
    param([string]$Name, [int]$RejectedCalls)
    $mutated = Get-DirectResolveLog -LogText $script:validLog -RejectedCalls $RejectedCalls
    $root = Join-Path $fixtureRoot "direct-resolve-$Name-positive"
    [System.IO.Directory]::CreateDirectory($root) | Out-Null
    $path = Join-Path $root 'mcla.log'
    [System.IO.File]::WriteAllText($path, $mutated, $utf8)
    try {
        $value = & $verifier -ProbeOnly -RuntimeLogPath $path -BmpPath $script:templateBmp `
            -ReferencePngPath $reference
        if ($value.Audit.ResolveCalls -ne 10000) {
            throw "Direct resolve fixture '$Name' lost its successful resolve coverage."
        }
    } finally { [System.IO.Directory]::Delete($root, $true) }
}

function Write-Result {
    param([object]$Value)
    [System.IO.File]::WriteAllText($resultPath,
        (($Value | ConvertTo-Json -Depth 10) + [Environment]::NewLine), $utf8)
}

function Assert-ResultRejected {
    param([string]$Name, [scriptblock]$Mutator)
    $validJson = Get-Content -LiteralPath $resultPath -Raw
    $value = $validJson | ConvertFrom-Json
    & $Mutator $value
    Write-Result $value
    try {
        & $verifier -ResultPath $resultPath | Out-Null
        throw "Negative result '$Name' was accepted."
    } catch {
        if ($_.Exception.Message -eq "Negative result '$Name' was accepted.") { throw }
        $script:rejections++
    } finally { [System.IO.File]::WriteAllText($resultPath, $validJson, $utf8) }
}

try {
    foreach ($path in @($verifier, $runner, $reference)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required test input missing: '$path'." }
    }
    $runnerText = Get-Content -LiteralPath $runner -Raw
    foreach ($required in @('--gpu_render_audit=true', '--async_shader_compilation=false',
            '--mcla_first_frame_probe=true', '--mcla_first_frame_settle_seconds=35',
            '--render_target_path_d3d12=rtv',
            'CMakePath --preset win-amd64-relwithdebinfo',
            '--clean-first', 'Tee-Object -FilePath $buildLogPath -Append',
            'EnumWindows', 'GetWindowThreadProcessId',
            'Send-WmCloseToExactGameWindow', 'WaitForExit($ExitTimeoutSeconds * 1000)',
            'CycleCount -eq 10', 'private/evidence/M4-002')) {
        if ($runnerText.IndexOf($required, [System.StringComparison]::Ordinal) -lt 0) {
            throw "Render-path runner source contract is missing '$required'."
        }
    }
    if ($runnerText.Contains('CloseMainWindow()') -or $runnerText.Contains('.MainWindowHandle')) {
        throw 'Render-path runner must not target a helper Process.MainWindowHandle.'
    }
    $verifierText = Get-Content -LiteralPath $verifier -Raw
    if (-not $verifierText.Contains(
            '$pressRoi = [System.Drawing.Rectangle]::new(1080, 630, 105, 45)') -or
        -not $verifierText.Contains('$pressPpm -lt 900000') -or
        -not $verifierText.Contains('$logoPpm -lt 900000')) {
        throw 'Render-path verifier source contract changed the reviewed title ROI thresholds.'
    }
    $appText = Get-Content -LiteralPath (Join-Path $repoRoot 'src/mcla_app.cpp') -Raw
    if ($appText -notmatch 'mcla_first_frame_settle_seconds,\s*3' -or
        $appText -notmatch '\.range\(1,\s*60\)' -or
        $appText -notmatch 'WriteFrameBmp[\s\S]{0,300}RequestRenderAuditCheckpoint\(\)') {
        throw 'Project source lacks the bounded settle delay or post-BMP render-audit checkpoint.'
    }
    $sdkFlags = Get-Content -LiteralPath (
        Join-Path $repoRoot 'third_party/rexglue-sdk/src/graphics/flags.cpp') -Raw
    if ($sdkFlags -notmatch 'REXCVAR_DEFINE_BOOL\(gpu_render_audit,\s*false' -or
        $sdkFlags -notmatch 'gpu_render_audit[\s\S]{0,200}Lifecycle::kInitOnly') {
        throw 'SDK render audit must remain disabled by default and init-only.'
    }
    $graphicsInterface = Get-Content -LiteralPath (Join-Path $repoRoot 'third_party/rexglue-sdk/include/rex/system/interfaces/graphics.h') -Raw
    $graphicsBase = Get-Content -LiteralPath (Join-Path $repoRoot 'third_party/rexglue-sdk/include/rex/graphics/graphics_system.h') -Raw
    $d3d12Graphics = Get-Content -LiteralPath (Join-Path $repoRoot 'third_party/rexglue-sdk/include/rex/graphics/d3d12/graphics_system.h') -Raw
    $gpuPlugin = Get-Content -LiteralPath (Join-Path $repoRoot 'third_party/rexglue-sdk/include/rex/system/gpu_plugin.h') -Raw
    if ($graphicsInterface -notmatch 'virtual\s+void\s+RequestRenderAuditCheckpoint\(\)\s*\{\s*\}' -or
        $graphicsBase -notmatch 'void\s+RequestRenderAuditCheckpoint\(\)\s+override\s*\{\s*\}' -or
        $d3d12Graphics -notmatch 'void\s+RequestRenderAuditCheckpoint\(\)\s+override\s*;' -or
        $gpuPlugin -notmatch 'kGpuPluginAbiVersion\s*=\s*2\s*;') {
        throw 'SDK render-audit checkpoint interface, backend contract, or ABI version is missing.'
    }

    [System.IO.Directory]::CreateDirectory((Join-Path $fixtureRoot 'runs')) | Out-Null
    $templateCapturePath = Join-Path $fixtureRoot 'template.bmp'
    $script:templateBmp = $templateCapturePath
    New-TestCapture $templateCapturePath
    Set-TestCaptureModalFloorDiscriminator $templateCapturePath
    $metric = Get-TestBmpMetrics $templateCapturePath
    $modalRatio = ([double]([long]$metric.ModalPixels * 1000)) /
        ([double]$metric.PixelCount)
    $modalFloor = [int][Math]::Floor($modalRatio)
    if ($modalRatio -eq $modalFloor -or [int]$modalRatio -eq $modalFloor -or
        $metric.ModalPermille -ne $modalFloor) {
        throw 'Modal-per-mille fixture must distinguish C++ truncation from PowerShell rounding.'
    }
    $auditLines = New-AuditLines
    $logLines = [System.Collections.Generic.List[string]]::new()
    $logLines.Add($startupPrefix.TrimEnd())
    $logLines.AddRange([string[]]$auditLines.Detail)
    $logLines.Add('[info] [gpu] D3D12 IssueSwap: first active guest output refresh succeeded source=1280x720')
    $logLines.Add('[info] [gpu] D3D12 guest present: successful sequence count=1 sequence=1 source=1280x720 swapchain=1280x720 HRESULT=0x00000000')
    $logLines.Add('[info] [gpu] D3D12 guest present: successful sequence count=3 sequence=3 source=1280x720 swapchain=1280x720 HRESULT=0x00000000')
    $logLines.Add('[info] [gpu] D3D12 guest capture: success sequence=3 last_presented_sequence=3 dimensions=1280x720')
    $logLines.Add('[info] [gpu] D3D12 guest capture: success sequence=100 last_presented_sequence=100 dimensions=1280x720')
    $logLines.Add("[info] [gpu] MCLA graphics: nontrivial guest frame captured 1280x720, rgb555 bins $($metric.Occupied), luma p05 $($metric.P05), luma p95 $($metric.P95), modal permille $($metric.ModalPermille), nonmodal grid cells $($metric.Cells)")
    $logLines.AddRange([string[]]$auditLines.Summary)
    $logLines.Add('[info] [app] Window closing, shutting down...')
    $logLines.Add('[00:00:42.000] [info] [core] [t1234] Execution complete')
    $logLines.Add('[info] [app] Title terminated; hard-exiting process.')
    $script:validLog = $logLines -join [Environment]::NewLine

    $firstCycleRoot = Join-Path $fixtureRoot 'runs/01'
    [System.IO.Directory]::CreateDirectory((Join-Path $firstCycleRoot 'user')) | Out-Null
    [System.IO.Directory]::CreateDirectory((Join-Path $firstCycleRoot 'cache')) | Out-Null
    $firstLog = Join-Path $firstCycleRoot 'mcla.log'
    $firstRotation = Join-Path $firstCycleRoot 'mcla.1.log'
    $firstBmp = Join-Path $firstCycleRoot 'user/mcla-first-frame.bmp'
    $splitAt = $validLog.IndexOf('XENOS_AUDIT_RT_SUMMARY', [System.StringComparison]::Ordinal)
    if ($splitAt -lt 1) { throw 'Fixture could not split its chronological rotated logs.' }
    [System.IO.File]::WriteAllText($firstRotation, $validLog.Substring(0, $splitAt), $utf8)
    [System.IO.File]::WriteAllText($firstLog, $validLog.Substring($splitAt), $utf8)
    (Get-Item -LiteralPath $firstRotation).LastWriteTimeUtc = [datetime]::UtcNow.AddMinutes(-1)
    (Get-Item -LiteralPath $firstLog).LastWriteTimeUtc = [datetime]::UtcNow
    [System.IO.File]::Copy($templateCapturePath, $firstBmp)
    $probe = & $verifier -ProbeOnly -RuntimeLogPath $firstLog -BmpPath $firstBmp `
        -ReferencePngPath $reference
    $script:templateBmp = $firstBmp
    Assert-OutOfOrderConcurrentIdsAccepted
    Assert-BenignPostHardExitTraceAccepted
    Assert-Emulated2xFallbackAccepted
    Assert-DirectResolveAccepted -Name all-direct -RejectedCalls 0
    Assert-DirectResolveAccepted -Name rejected-fallback -RejectedCalls 1000

    for ($cycle = 2; $cycle -le 10; $cycle++) {
        $name = '{0:D2}' -f $cycle
        $root = Join-Path $fixtureRoot "runs/$name"
        [System.IO.Directory]::CreateDirectory((Join-Path $root 'user')) | Out-Null
        [System.IO.Directory]::CreateDirectory((Join-Path $root 'cache')) | Out-Null
        [System.IO.File]::Copy($firstLog, (Join-Path $root 'mcla.log'))
        [System.IO.File]::Copy($firstRotation, (Join-Path $root 'mcla.1.log'))
        (Get-Item -LiteralPath (Join-Path $root 'mcla.1.log')).LastWriteTimeUtc =
            [datetime]::UtcNow.AddMinutes(-1)
        (Get-Item -LiteralPath (Join-Path $root 'mcla.log')).LastWriteTimeUtc = [datetime]::UtcNow
        [System.IO.File]::Copy($firstBmp, (Join-Path $root 'user/mcla-first-frame.bmp'))
    }
    Remove-Item -LiteralPath $templateCapturePath -Force
    $script:templateBmp = $firstBmp

    $buildLog = Join-Path $fixtureRoot 'relwithdebinfo-clean-build.log'
    [System.IO.File]::WriteAllText($buildLog, 'fixture clean build', $utf8)
    $cycles = @()
    for ($cycle = 1; $cycle -le 10; $cycle++) {
        $name = '{0:D2}' -f $cycle
        $root = Join-Path $fixtureRoot "runs/$name"
        $userTree = Get-TestTreeSnapshot (Join-Path $root 'user')
        $cacheTree = Get-TestTreeSnapshot (Join-Path $root 'cache')
        $cycleTree = Get-TestTreeSnapshot $root
        $cycles += [ordered]@{
            index = $cycle; capture_elapsed_milliseconds = 45000
            dwell_elapsed_milliseconds = 2000; exit_elapsed_milliseconds = 200; exit_code = 0
            startup_marker_count = 15; present_count_1_sequence = 1
            present_count_3_sequence = 3; present_count_1_hresult = '0x00000000'
            present_count_3_hresult = '0x00000000'; capture_sequence = 100
            capture_last_presented_sequence = 100; capture_success_marker_count = 2
            window_close_marker_occurrences = 1; execution_complete_marker_occurrences = 1
            hard_exit_marker_occurrences = 1
            post_hard_exit_execution_complete_occurrences = 0; close_requested = $true
            harness_force_cleanup = $false; process_signal_confirmed = $true
            process_cleanup_confirmed = $true; prior_cycles_immutable = $true
            runtime_logs = @($probe.Log.RuntimeLogs)
            runtime_log_file_count = $probe.Log.RuntimeLogFileCount
            runtime_log_bytes = $probe.Log.RuntimeLogBytes
            runtime_log_set_sha256 = $probe.Log.RuntimeLogSetSha256
            capture_relative_path = "runs/$name/user/mcla-first-frame.bmp"
            capture_sha256 = $probe.Bmp.Sha256; capture_bytes = $probe.Bmp.Bytes
            capture_metrics = [ordered]@{
                width = $probe.Bmp.Width; height = $probe.Bmp.Height; stride = $probe.Bmp.Stride
                pixel_count = $probe.Bmp.PixelCount
                occupied_rgb555_bins = $probe.Bmp.OccupiedRgb555Bins
                luma_p05 = $probe.Bmp.LumaP05; luma_p95 = $probe.Bmp.LumaP95
                luma_spread = $probe.Bmp.LumaSpread; modal_pixels = $probe.Bmp.ModalPixels
                modal_per_mille = $probe.Bmp.ModalPermille
                nonmodal_grid_cells = $probe.Bmp.NonmodalGridCells
            }
            logo_edge_correlation_ppm = $probe.Roi.LogoCorrelationPpm
            press_edge_correlation_ppm = $probe.Roi.PressCorrelationPpm
            title_route_verified = $true
            audit = [ordered]@{
                config_count = $probe.Audit.ConfigCount; rt_records = $probe.Audit.RtRecords
                bind_records = $probe.Audit.BindRecords
                ownership_modes = $probe.Audit.OwnershipModes
                resolve_records = $probe.Audit.ResolveRecords
                resolve_calls = $probe.Audit.ResolveCalls; shader_records = $probe.Audit.ShaderRecords
                pso_records = $probe.Audit.PsoRecords; pso_ok = $probe.Audit.PsoOk
                draw_issued = $probe.Audit.DrawIssued; depth_test = $probe.Audit.DepthTest
                depth_write = $probe.Audit.DepthWrite; msaa1 = $probe.Audit.Msaa1
                msaa2 = $probe.Audit.Msaa2; msaa4 = $probe.Audit.Msaa4
                gamma_nonidentity = $probe.Audit.GammaNonidentity
                gamma_uploads = $probe.Audit.GammaUploads; summary_count = $probe.Audit.SummaryCount
            }
            user_tree_sha256 = $userTree.Hash; cache_tree_sha256 = $cacheTree.Hash
            user_file_count = $userTree.FileCount; cache_file_count = $cacheTree.FileCount
            user_bytes = $userTree.Bytes; cache_bytes = $cacheTree.Bytes
            cycle_tree_sha256 = $cycleTree.Hash
        }
    }
    $gameRoot = Join-Path $repoRoot 'private/game'
    $manifestPath = Join-Path $repoRoot 'private/game-manifest.json'
    $verifiedGame = & $gameVerifier -GamePath $gameRoot -ManifestPath $manifestPath -VerifyHashes
    $gameTree = Get-TestTreeSnapshot $gameRoot
    $game = [ordered]@{
        file_count = $verifiedGame.FileCount; payload_bytes = $verifiedGame.PayloadBytes
        hashes_verified = $verifiedGame.HashesVerified
        manifest_sha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
        tree_sha256 = $gameTree.Hash; tree_file_count = $gameTree.FileCount
        tree_directory_count = $gameTree.DirectoryCount; tree_bytes = $gameTree.Bytes
    }
    $buildRoot = Join-Path $repoRoot 'out/build/win-amd64-relwithdebinfo'
    $artifacts = @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll') |
        ForEach-Object {
            [ordered]@{
                name = $_
                sha256 = (Get-FileHash -LiteralPath (Join-Path $buildRoot $_) -Algorithm SHA256).Hash
            }
        }
    $result = [ordered]@{
        schema = 1; task = 'M4-002'; cycle_count = 10
        execution_order = 'clean_build_then_10_serial_render_path_cycles'
        development_only = $false; capture_timeout_seconds = 60
        first_frame_settle_seconds = 35
        post_marker_dwell_milliseconds = 2000; exit_timeout_seconds = 10
        failure_cleanup_timeout_seconds = 5
        clean_build = [ordered]@{
            performed = $true; success = $true; exit_code = 0; duration_milliseconds = 1000
            build_log_sha256 = (Get-FileHash -LiteralPath $buildLog -Algorithm SHA256).Hash
            executable_sha256 = $artifacts[0].sha256
        }
        first_cycle_post_clean_build = $true
        reference = [ordered]@{
            name = 'xenia-title-reference.png'
            sha256 = (Get-FileHash -LiteralPath $reference -Algorithm SHA256).Hash
            bytes = (Get-Item -LiteralPath $reference).Length; width = 1280; height = 720
        }
        game_identity = [ordered]@{ before = $game; after = $game }
        artifacts = [ordered]@{ before = $artifacts; after = $artifacts }
        cycles = $cycles; all_write_roots_contained = $true
        all_prior_cycles_immutable = $true; no_surviving_processes = $true
        data_integrity_preserved = $true; all_captures_bound = $true
        all_title_rois_match = $true; all_render_audits_passed = $true
    }
    Write-Result $result
    $positive = & $verifier -ResultPath $resultPath
    if (-not $positive.Passed -or $positive.Cycles -ne 10 -or
        $positive.PhysicalCapturesVerified -ne 10) {
        throw 'Realistic physical render-path positive fixture did not pass.'
    }

    Assert-ProbeRejected malformed-marker { param($v) $v.Replace('backend=d3d12', 'backend=vulkan') }
    Assert-ProbeRejected rov-path { param($v) $v.Replace('rt_path=host', 'rt_path=rov') }
    Assert-ProbeRejected duplicate-config { param($v) $v.Replace('XENOS_AUDIT_CONFIG', "XENOS_AUDIT_CONFIG`nXENOS_AUDIT_CONFIG") }
    Assert-ProbeRejected unknown-marker { param($v) $v.Replace('XENOS_AUDIT_CONFIG', "XENOS_AUDIT_UNKNOWN v=1 value=0`nXENOS_AUDIT_CONFIG") }
    Assert-ProbeRejected rt-failure { param($v) $v.Replace('create_fail=0', 'create_fail=1') }
    Assert-ProbeRejected rt-overflow { param($v) $v.Replace('records=20 overflow=0', 'records=20 overflow=1') }
    Assert-ProbeRejected missing-depth { param($v) $v.Replace('kind=depth', 'kind=color') }
    Assert-ProbeRejected missing-depth-bind { param($v) $v.Replace('slot=depth', 'slot=color0') }
    Assert-ProbeRejected bind-unknown-map { param($v) $v.Replace('storage_fmt=0 guest_msaa=1 depth_test=0', 'storage_fmt=-1 guest_msaa=1 depth_test=0') }
    Assert-ProbeRejected bind-tuple-absent-from-rt {
        param($v)
        $v.Replace('slot=color0 guest_fmt=0 storage_fmt=0 guest_msaa=1',
            'slot=color0 guest_fmt=0 storage_fmt=15 guest_msaa=1')
    }
    Assert-ProbeRejected bind-overflow { param($v) $v.Replace('bind_overflow=0', 'bind_overflow=1') }
    Assert-ProbeRejected bind-order { param($v) $v.Replace('XENOS_AUDIT_BIND v=1 id=0', 'XENOS_AUDIT_BIND v=1 id=9') }
    Assert-ProbeRejected msaa-downgrade { param($v) $v.Replace('guest_msaa=2 host_samples=2', 'guest_msaa=2 host_samples=1') }
    Assert-ProbeRejected native-2x-with-emulated-flag {
        param($v)
        [regex]::Replace($v,
            '(?m)^(XENOS_AUDIT_RT[^\r\n]* guest_msaa=2 [^\r\n]* emulated_2x=)0(?=\r?$)',
            '${1}1')
    }
    Assert-ProbeRejected fallback-2x-without-emulated-flag {
        param($v)
        (Get-Emulated2xFallbackLog $v).Replace('emulated_2x=1', 'emulated_2x=0')
    }
    Assert-ProbeRejected ownership-2x-to1 {
        param($v)
        $v.Replace('src_host_samples=2 dst_host_samples=2',
            'src_host_samples=1 dst_host_samples=1')
    }
    Assert-ProbeRejected resolve-underflow { param($v) $v.Replace('10000', '699') }
    Assert-ProbeRejected resolve-failure { param($v) $v.Replace('final_fail=0', 'final_fail=1') }
    Assert-ProbeRejected resolve-accounting {
        param($v)
        $v.Replace('fallback_dump_ok=10000', 'fallback_dump_ok=9999')
    }
    Assert-ProbeRejected no-hdr { param($v) $v.Replace('src_fmt=7 guest_msaa=4', 'src_fmt=0 guest_msaa=4') }
    Assert-ProbeRejected shader-failure { param($v) $v.Replace('translate_fail=0', 'translate_fail=1') }
    Assert-ProbeRejected pso-failure { param($v) $v.Replace('pso_fail=0', 'pso_fail=1') }
    Assert-ProbeRejected pending-skip { param($v) $v.Replace('pso_pending_skip=0', 'pso_pending_skip=1') }
    Assert-ProbeRejected no-depth-write { param($v) $v.Replace('depth_write=500', 'depth_write=0') }
    Assert-ProbeRejected impossible-unbound-depth { param($v) $v.Replace('depth_without_bound=10', 'depth_without_bound=1001') }
    Assert-ProbeRejected no-msaa4 { param($v) $v.Replace('msaa4=700', 'msaa4=0') }
    Assert-ProbeRejected no-gamma-upload { param($v) $v.Replace('gamma_uploads=2', 'gamma_uploads=0') }
    Assert-ProbeRejected shutdown-summary { param($v) $v.Replace('phase=checkpoint', 'phase=shutdown') }
    Assert-ProbeRejected detail-after-summary {
        param($v)
        $line = 'XENOS_AUDIT_SHADER v=1 id=0 stage=vs ucode=1111111111111111 modification=0000000000000000 result=ok'
        $v.Replace($line + [Environment]::NewLine, '') + [Environment]::NewLine + $line
    }
    Assert-ProbeRejected capture-watermark { param($v) $v.Replace('sequence=100 last_presented_sequence=100', 'sequence=100 last_presented_sequence=99') }
    Assert-ProbeRejected post-hard-exit-execution-complete {
        param($v)
        $line = '[00:00:42.000] [info] [core] [t1234] Execution complete'
        $v.Replace($line + [Environment]::NewLine, '') + [Environment]::NewLine + $line
    }
    Assert-ProbeRejected fatal-tail { param($v) $v + "`n[00:00:43.000] [fatal] [gpu] synthetic fatal" }
    Assert-ProbeRejected device-loss { param($v) $v + "`nDXGI_ERROR_DEVICE_REMOVED" }

    Assert-ResultRejected development-only { param($v) $v.development_only = $true }
    Assert-ResultRejected cycle-count { param($v) $v.cycle_count = 9 }
    Assert-ResultRejected settle-seconds { param($v) $v.first_frame_settle_seconds = 34 }
    Assert-ResultRejected process-cleanup { param($v) $v.cycles[0].harness_force_cleanup = $true }
    Assert-ResultRejected orphan { param($v) $v.no_surviving_processes = $false }
    Assert-ResultRejected game-drift { param($v) $v.game_identity.after.tree_sha256 = $hashB }
    Assert-ResultRejected artifact-drift { param($v) $v.artifacts.after[3].sha256 = $hashB }
    Assert-ResultRejected prior-drift { param($v) $v.cycles[0].prior_cycles_immutable = $false }
    Assert-ResultRejected forged-metrics { param($v) $v.cycles[0].capture_metrics.luma_p95++ }
    Assert-ResultRejected forged-audit { param($v) $v.cycles[0].audit.resolve_calls++ }
    Assert-ResultRejected roi-threshold { param($v) $v.cycles[0].logo_edge_correlation_ppm = 899999 }
    Assert-ResultRejected privacy { param($v) $v.cycles[0].capture_relative_path = 'C:\private\frame.bmp' }

    $resultLink = Join-Path (Split-Path -Parent $fixtureRoot) (
        'test-render-path-result-link-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Junction -Path $resultLink -Target $fixtureRoot | Out-Null
    try {
        & $verifier -ResultPath (Join-Path $resultLink 'result.json') | Out-Null
        throw 'Reparse-point ResultPath was accepted.'
    } catch {
        if ($_.Exception.Message -eq 'Reparse-point ResultPath was accepted.') { throw }
        $rejections++
    } finally { [System.IO.Directory]::Delete($resultLink, $false) }

    $extra = Join-Path $fixtureRoot 'runs/01/extra.bin'
    [System.IO.File]::WriteAllBytes($extra, [byte[]](1, 2, 3))
    try { & $verifier -ResultPath $resultPath | Out-Null; throw 'Extra artifact was accepted.' }
    catch { if ($_.Exception.Message -eq 'Extra artifact was accepted.') { throw }; $rejections++ }
    finally { Remove-Item -LiteralPath $extra -Force }

    $rotationOne = Join-Path $fixtureRoot 'runs/01/mcla.1.log'
    $rotationTwo = Join-Path $fixtureRoot 'runs/01/mcla.2.log'
    [System.IO.File]::Move($rotationOne, $rotationTwo)
    try { & $verifier -ResultPath $resultPath | Out-Null; throw 'Rotation gap was accepted.' }
    catch { if ($_.Exception.Message -eq 'Rotation gap was accepted.') { throw }; $rejections++ }
    finally { [System.IO.File]::Move($rotationTwo, $rotationOne) }

    $malformedRotation = Join-Path $fixtureRoot 'runs/01/mcla.bad.log'
    [System.IO.File]::WriteAllText($malformedRotation, 'malformed rotation', $utf8)
    try { & $verifier -ResultPath $resultPath | Out-Null; throw 'Malformed rotation was accepted.' }
    catch { if ($_.Exception.Message -eq 'Malformed rotation was accepted.') { throw }; $rejections++ }
    finally { Remove-Item -LiteralPath $malformedRotation -Force }

    $currentLogItem = Get-Item -LiteralPath $firstLog
    $rotationItem = Get-Item -LiteralPath $rotationOne
    $savedCurrentTime = $currentLogItem.LastWriteTimeUtc
    $savedRotationTime = $rotationItem.LastWriteTimeUtc
    $rotationItem.LastWriteTimeUtc = [datetime]::UtcNow.AddMinutes(1)
    $currentLogItem.LastWriteTimeUtc = [datetime]::UtcNow
    try { & $verifier -ResultPath $resultPath | Out-Null; throw 'Contradictory rotation timestamps were accepted.' }
    catch {
        if ($_.Exception.Message -eq 'Contradictory rotation timestamps were accepted.') { throw }
        $rejections++
    } finally {
        $rotationItem.LastWriteTimeUtc = $savedRotationTime
        $currentLogItem.LastWriteTimeUtc = $savedCurrentTime
    }

    $savedCapture = Join-Path $fixtureRoot 'saved-capture.bmp'
    [System.IO.File]::Copy($firstBmp, $savedCapture)
    Remove-Item -LiteralPath $firstBmp -Force
    try { & $verifier -ResultPath $resultPath | Out-Null; throw 'Deleted capture was accepted.' }
    catch { if ($_.Exception.Message -eq 'Deleted capture was accepted.') { throw }; $rejections++ }
    finally { [System.IO.File]::Move($savedCapture, $firstBmp) }

    $roiBitmap = [System.Drawing.Bitmap]::FromFile($firstBmp)
    $roiCopy = [System.Drawing.Bitmap]::new($roiBitmap)
    $roiBitmap.Dispose()
    $roiGraphics = [System.Drawing.Graphics]::FromImage($roiCopy)
    try { $roiGraphics.FillRectangle([System.Drawing.Brushes]::Black, 1080, 630, 105, 45) }
    finally { $roiGraphics.Dispose() }
    $roiCopy.Save($savedCapture, [System.Drawing.Imaging.ImageFormat]::Bmp)
    $roiCopy.Dispose()
    [System.IO.File]::Copy($savedCapture, $firstBmp, $true)
    try { & $verifier -ResultPath $resultPath | Out-Null; throw 'PRESS ROI mismatch was accepted.' }
    catch { if ($_.Exception.Message -eq 'PRESS ROI mismatch was accepted.') { throw }; $rejections++ }
    finally {
        Remove-Item -LiteralPath $savedCapture -Force
        [System.IO.File]::Copy((Join-Path $fixtureRoot 'runs/02/user/mcla-first-frame.bmp'),
            $firstBmp, $true)
    }

    $junctionTarget = Join-Path $repoRoot ('private/test-render-path-target-' + [guid]::NewGuid().ToString('N'))
    [System.IO.Directory]::CreateDirectory($junctionTarget) | Out-Null
    $junction = Join-Path $fixtureRoot 'runs/01/cache/junction'
    New-Item -ItemType Junction -Path $junction -Target $junctionTarget | Out-Null
    try { & $verifier -ResultPath $resultPath | Out-Null; throw 'Reparse evidence was accepted.' }
    catch { if ($_.Exception.Message -eq 'Reparse evidence was accepted.') { throw }; $rejections++ }
    finally {
        [System.IO.Directory]::Delete($junction, $false)
        [System.IO.Directory]::Delete($junctionTarget, $true)
    }

    [pscustomobject]@{
        Passed = $true
        PositiveFixtures = 6
        ConcurrentRecordOrderingAccepted = $true
        AnimatedBackgroundDifferenceAccepted = $true
        BenignPostHardExitTraceAccepted = $true
        Emulated2xFallbackAccepted = $true
        DirectResolveAndRejectedFallbackAccepted = $true
        ModalFloorTruncationVerified = $true
        NegativeFixtures = $rejections
        SourceContractVerified = $true
        PhysicalEvidenceVerified = $true
    }
} finally {
    Remove-TestTreeSafely $fixtureRoot
}
