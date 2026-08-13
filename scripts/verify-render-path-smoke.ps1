[CmdletBinding(DefaultParameterSetName = 'Result')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Result')]
    [string]$ResultPath,

    [Parameter(ParameterSetName = 'Result')]
    [switch]$HistoricalEvidenceOnly,

    [Parameter(Mandatory, ParameterSetName = 'Probe')]
    [string]$RuntimeLogPath,

    [Parameter(Mandatory, ParameterSetName = 'Probe')]
    [string]$BmpPath,

    [Parameter(ParameterSetName = 'Probe')]
    [string]$ReferencePngPath,

    [Parameter(ParameterSetName = 'Probe')]
    [switch]$LocalizedPrompt,

    [Parameter(ParameterSetName = 'Probe')]
    [string]$LocalizedPromptReferencePath,

    [Parameter(ParameterSetName = 'Probe')]
    [ValidatePattern('^[0-9A-F]{64}$')]
    [string]$LocalizedPromptReferenceSha256,

    [Parameter(Mandatory, ParameterSetName = 'Probe')]
    [switch]$ProbeOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$utf8 = [System.Text.UTF8Encoding]::new($false)
$hashPattern = '^[0-9A-F]{64}$'
$expectedReferenceSha256 = '7F0293842A6AA30EF0B0EA7C7954FF5130A03ECF6E3A112EEFCAA4A6B11C613E'
$defaultReferencePath = Join-Path $repoRoot (
    'private/tools/xenia-canary/artifacts/screenshots/545407F8/' +
    '545407F8 - 2026-08-11T00-59-52.png')
$gameVerifierPath = Join-Path $repoRoot 'scripts/verify-game-manifest.ps1'
$canonicalGamePath = Join-Path $repoRoot 'private/game'
$canonicalBuildPath = Join-Path $repoRoot 'out/build/win-amd64-relwithdebinfo'
Add-Type -AssemblyName System.Drawing
$logoRoi = [System.Drawing.Rectangle]::new(280, 230, 730, 200)
$pressRoi = [System.Drawing.Rectangle]::new(1080, 630, 105, 45)

$firstFrameMarkers = [ordered]@{
    Refresh = '(?m)^.*D3D12 IssueSwap: first active guest output refresh succeeded source=(?<width>[0-9]+)x(?<height>[0-9]+)\s*$'
    Present = '(?m)^.*D3D12 guest present: successful sequence count=(?<count>1|3) sequence=(?<sequence>[0-9]+) source=(?<source_width>[0-9]+)x(?<source_height>[0-9]+) swapchain=(?<swap_width>[0-9]+)x(?<swap_height>[0-9]+) HRESULT=0x(?<hresult>[0-9A-Fa-f]{8})\s*$'
    Capture = '(?m)^.*D3D12 guest capture: success sequence=(?<sequence>[0-9]+) last_presented_sequence=(?<last_presented_sequence>[0-9]+) dimensions=(?<width>[0-9]+)x(?<height>[0-9]+)\s*$'
    Project = '(?m)^.*MCLA graphics: nontrivial guest frame captured (?<width>[0-9]+)x(?<height>[0-9]+), rgb555 bins (?<bins>[0-9]+), luma p05 (?<p05>[0-9]+), luma p95 (?<p95>[0-9]+), modal permille (?<modal>[0-9]+), nonmodal grid cells (?<cells>[0-9]+)\s*$'
    WindowClose = 'Window closing, shutting down...'
    ExecutionComplete = '(?m)^\[[^\r\n]+\] \[info\] \[core\] \[t[0-9]+\] Execution complete\s*$'
    HardExit = 'Title terminated; hard-exiting process.'
}

# This is the sole grammar definition for SDK render-audit markers consumed by
# the project gate. Values are decimal except explicitly hexadecimal hashes.
$auditGrammar = [ordered]@{
    Config = '^XENOS_AUDIT_CONFIG v=1 backend=(?<backend>d3d12) rt_path=(?<rt_path>host|rov) bindless=(?<bindless>0|1) scale_x=(?<scale_x>[0-9]+) scale_y=(?<scale_y>[0-9]+) native_2x_supported=(?<native_2x_supported>0|1) gamma_rt_unorm16=(?<gamma_rt_unorm16>0|1) depth_f24_ps=(?<depth_f24_ps>0|1) depth_f24_round=(?<depth_f24_round>0|1) direct_host_resolve=(?<direct_host_resolve>0|1)$'
    Rt = '^XENOS_AUDIT_RT v=1 id=(?<id>[0-9]+) kind=(?<kind>color|depth) storage_fmt=(?<storage_fmt>[0-9]+) guest_msaa=(?<guest_msaa>1|2|4) host_samples=(?<host_samples>1|2|4) resource_dxgi=(?<resource_dxgi>-1|[0-9]+) draw_dxgi=(?<draw_dxgi>-1|[0-9]+) ownership_dxgi=(?<ownership_dxgi>-1|[0-9]+) depth_srv_dxgi=(?<depth_srv_dxgi>-1|[0-9]+) stencil_srv_dxgi=(?<stencil_srv_dxgi>-1|[0-9]+) emulated_2x=(?<emulated_2x>0|1)$'
    Bind = '^XENOS_AUDIT_BIND v=1 id=(?<id>[0-9]+) slot=(?<slot>depth|color0|color1|color2|color3) guest_fmt=(?<guest_fmt>[0-9]+) storage_fmt=(?<storage_fmt>-1|[0-9]+) guest_msaa=(?<guest_msaa>1|2|4) depth_test=(?<depth_test>0|1) depth_write=(?<depth_write>0|1) stencil=(?<stencil>0|1) host_bound=(?<host_bound>0|1)$'
    Ownership = '^XENOS_AUDIT_OWNERSHIP v=1 id=(?<id>[0-9]+) mode=(?<mode>[0-9]+) src_guest_msaa=(?<src_guest_msaa>1|2|4) dst_guest_msaa=(?<dst_guest_msaa>1|2|4) src_host_samples=(?<src_host_samples>1|2|4) dst_host_samples=(?<dst_host_samples>1|2|4)$'
    Resolve = '^XENOS_AUDIT_RESOLVE v=1 id=(?<id>[0-9]+) src_kind=(?<src_kind>color|depth) src_fmt=(?<src_fmt>[0-9]+) guest_msaa=(?<guest_msaa>1|2|4) dest_fmt=(?<dest_fmt>[0-9]+) shader=(?<shader>[0-9]+) scaled=(?<scaled>0|1)$'
    Shader = '^XENOS_AUDIT_SHADER v=1 id=(?<id>[0-9]+) stage=(?<stage>vs|ps) ucode=(?<ucode>[0-9A-F]{16}) modification=(?<modification>[0-9A-F]{16}) result=(?<result>ok|fail)$'
    Pso = '^XENOS_AUDIT_PSO v=1 id=(?<id>[0-9]+) desc=(?<desc>[0-9A-F]{16}) vs=(?<vs>[0-9A-F]{16}) ps=(?<ps>[0-9A-F]{16}) host_msaa=(?<host_msaa>1|2|4) depth_fmt=(?<depth_fmt>[0-9]+) depth_func=(?<depth_func>[0-9]+) depth_write=(?<depth_write>0|1) result=(?<result>ok|fail) hresult=(?<hresult>[0-9A-F]{8})$'
    Gamma = '^XENOS_AUDIT_GAMMA v=1 id=(?<id>[0-9]+) mode=(?<mode>table|pwl) mode_source=frontbuffer_format_heuristic fb_fmt=(?<fb_fmt>[0-9]+) luta_control=(?<luta_control>[0-9A-F]{8}) identity=(?<identity>0|1) upload=(?<upload>0|1) dispatch=1$'
    RtSummary = '^XENOS_AUDIT_RT_SUMMARY v=1 phase=(?<phase>checkpoint|shutdown) create_attempt=(?<create_attempt>[0-9]+) create_ok=(?<create_ok>[0-9]+) create_fail=(?<create_fail>[0-9]+) records=(?<records>[0-9]+) overflow=(?<overflow>[0-9]+) host_depth_store=(?<host_depth_store>[0-9]+) ownership_draws=(?<ownership_draws>[0-9]+) ownership_modes=(?<ownership_modes>[0-9]+) ownership_overflow=(?<ownership_overflow>[0-9]+)$'
    ResolveSummary = '^XENOS_AUDIT_RESOLVE_SUMMARY v=1 phase=(?<phase>checkpoint|shutdown) calls=(?<calls>[0-9]+) info_ok=(?<info_ok>[0-9]+) info_fail=(?<info_fail>[0-9]+) zero_area=(?<zero_area>[0-9]+) shader_known=(?<shader_known>[0-9]+) shader_unknown=(?<shader_unknown>[0-9]+) direct_preflight=(?<direct_preflight>[0-9]+) direct_preflight_dump_ok=(?<direct_preflight_dump_ok>[0-9]+) direct_reject=(?<direct_reject>[0-9]+) fallback_dump_ok=(?<fallback_dump_ok>[0-9]+) fallback_dump_fail=(?<fallback_dump_fail>[0-9]+) copy_dispatch=(?<copy_dispatch>[0-9]+) final_ok=(?<final_ok>[0-9]+) final_fail=(?<final_fail>[0-9]+) modes=(?<modes>[0-9]+) overflow=(?<overflow>[0-9]+) true_direct_dispatch=(?<true_direct_dispatch>0)$'
    PipelineSummary = '^XENOS_AUDIT_PIPELINE_SUMMARY v=1 phase=(?<phase>checkpoint|shutdown) shader_entries=(?<shader_entries>[0-9]+) translate_vs_ok=(?<translate_vs_ok>[0-9]+) translate_ps_ok=(?<translate_ps_ok>[0-9]+) translate_fail=(?<translate_fail>[0-9]+) pso_entries=(?<pso_entries>[0-9]+) pso_attempt=(?<pso_attempt>[0-9]+) pso_ok=(?<pso_ok>[0-9]+) pso_fail=(?<pso_fail>[0-9]+) shader_records=(?<shader_records>[0-9]+) shader_overflow=(?<shader_overflow>[0-9]+) pso_records=(?<pso_records>[0-9]+) pso_overflow=(?<pso_overflow>[0-9]+)$'
    CpSummary = '^XENOS_AUDIT_CP_SUMMARY v=1 phase=(?<phase>checkpoint|shutdown) draw_issued=(?<draw_issued>[0-9]+) draw_indexed=(?<draw_indexed>[0-9]+) draw_nonindexed=(?<draw_nonindexed>[0-9]+) pso_pending_skip=(?<pso_pending_skip>[0-9]+) pso_failed_skip=(?<pso_failed_skip>[0-9]+) depth_test=(?<depth_test>[0-9]+) depth_write=(?<depth_write>[0-9]+) stencil=(?<stencil>[0-9]+) depth_bound=(?<depth_bound>[0-9]+) depth_without_bound=(?<depth_without_bound>[0-9]+) bind_records=(?<bind_records>[0-9]+) bind_overflow=(?<bind_overflow>[0-9]+) msaa1=(?<msaa1>[0-9]+) msaa2=(?<msaa2>[0-9]+) msaa4=(?<msaa4>[0-9]+) gamma_table_dispatch=(?<gamma_table_dispatch>[0-9]+) gamma_pwl_dispatch=(?<gamma_pwl_dispatch>[0-9]+) gamma_identity_dispatch=(?<gamma_identity_dispatch>[0-9]+) gamma_nonidentity_dispatch=(?<gamma_nonidentity_dispatch>[0-9]+) gamma_table_writes=(?<gamma_table_writes>[0-9]+) gamma_pwl_writes=(?<gamma_pwl_writes>[0-9]+) gamma_uploads=(?<gamma_uploads>[0-9]+) gamma_records=(?<gamma_records>[0-9]+) gamma_overflow=(?<gamma_overflow>[0-9]+) refresh_fail=(?<refresh_fail>[0-9]+)$'
}

$bannedPatterns = @(
    '(?i)D3D12 guest present: failed sequence=',
    '(?i)D3D12 guest present: device (?:removed|reset)',
    '(?i)DXGI_ERROR_DEVICE_(?:REMOVED|RESET|HUNG)',
    '(?i)D3D12 device removed',
    '(?i)GPU (?:device )?lost',
    '(?i)D3D12 guest capture: failed',
    '(?i)MCLA graphics: failed to write private first-frame capture',
    '(?i)XENOS_AUDIT_.*(?:unknown|overflow|fail)=[1-9][0-9]*'
)

$startupMarkers = [ordered]@{
    lifecycle = 'MCLA lifecycle: logging ready'
    gpu_selection = "MCLA graphics: selected GPU plugin 'xenos'"
    gpu_loaded = "GPU plugin 'xenos' loaded (rexgpu-xenosrd.dll)"
    static_image = 'MCLA module config: static image 82000000-829E0000, code 82130000-827CD054, 30026 mappings'
    runtime = 'Runtime initialized successfully'
    xex_load = 'Loading XEX image: game:\default.xex'
    identity = 'MCLA module identity: title 545407F8, media 5940C9DB, image 82000000-829E0000, entry 821322B8'
    dispatch = 'MCLA module config: entry 821322B8 registered in dispatch range 82130000-827CD054'
    vfs = 'MCLA VFS: game: and d: resolve 3/3 expected disc files'
    vfs_read_only = 'MCLA VFS: write, create, delete, and writable-map requests denied'
    launch = 'KernelState: Preparing module launch...'
    title = 'Initializing shader storage for title 545407F8...'
    interrupt = 'SetInterruptCallback('
    pipeline = 'Creating graphics pipeline with VS '
    audio = 'AudioWorker: dispatching callback '
}

function Assert-ExactProperties {
    param([object]$Value, [string[]]$Expected, [string]$Description)
    $actual = @($Value.PSObject.Properties.Name | Sort-Object)
    $wanted = @($Expected | Sort-Object)
    if ($actual.Count -ne $wanted.Count) { throw "$Description has missing or unknown properties." }
    for ($index = 0; $index -lt $wanted.Count; $index++) {
        if ($actual[$index] -cne $wanted[$index]) {
            throw "$Description has missing or unknown properties."
        }
    }
}

function Assert-JsonTypes {
    param(
        [object]$Value, [string[]]$BooleanNames = @(), [string[]]$IntegerNames = @(),
        [string[]]$StringNames = @(), [string]$Description
    )
    foreach ($name in $BooleanNames) {
        if ($Value.PSObject.Properties[$name].Value -isnot [bool]) {
            throw "$Description property '$name' must be a JSON boolean."
        }
    }
    foreach ($name in $IntegerNames) {
        $candidate = $Value.PSObject.Properties[$name].Value
        if ($candidate -isnot [byte] -and $candidate -isnot [sbyte] -and
            $candidate -isnot [int16] -and $candidate -isnot [uint16] -and
            $candidate -isnot [int32] -and $candidate -isnot [uint32] -and
            $candidate -isnot [int64] -and $candidate -isnot [uint64]) {
            throw "$Description property '$name' must be a JSON integer."
        }
    }
    foreach ($name in $StringNames) {
        if ($Value.PSObject.Properties[$name].Value -isnot [string]) {
            throw "$Description property '$name' must be a JSON string."
        }
    }
}

function Assert-ContainedNonReparsePath {
    param([string]$Path, [string]$Description)
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $fullPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay inside the repository: '$fullPath'."
    }
    $relative = $fullPath.Substring($prefix.Length)
    $current = $repoRoot
    foreach ($component in @($relative.Split('\') | Where-Object { $_.Length -ne 0 })) {
        $current = Join-Path $current $component
        if (-not (Test-Path -LiteralPath $current)) {
            throw "$Description component is missing: '$current'."
        }
        if ((Get-Item -LiteralPath $current -Force).Attributes -band
            [System.IO.FileAttributes]::ReparsePoint) {
            throw "$Description traverses a reparse point: '$current'."
        }
    }
    return $fullPath
}

function Assert-ExactChildren {
    param([string]$Root, [string[]]$ExpectedNames, [string]$Description)
    $actual = @((Get-ChildItem -LiteralPath $Root -Force | Sort-Object Name).Name)
    $expected = @($ExpectedNames | Sort-Object)
    if ($actual.Count -ne $expected.Count) { throw "$Description has missing or extra artifacts." }
    for ($index = 0; $index -lt $expected.Count; $index++) {
        if ($actual[$index] -cne $expected[$index]) {
            throw "$Description has missing, extra, or incorrectly named artifacts."
        }
    }
}

function Get-TreeSnapshot {
    param([string]$Root)
    $allItems = @(Get-ChildItem -LiteralPath $Root -Recurse -Force)
    foreach ($item in @((Get-Item -LiteralPath $Root -Force)) + $allItems) {
        if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "Evidence tree contains a reparse point: '$($item.FullName)'."
        }
    }
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
    } finally { $sha.Dispose() }
    $bytes = [long]0
    foreach ($file in $files) { $bytes += [long]$file.Length }
    [pscustomobject]@{
        Hash = $treeHash
        FileCount = $files.Count
        DirectoryCount = @($allItems | Where-Object { $_.PSIsContainer }).Count
        Bytes = $bytes
    }
}

function Get-CanonicalGameIdentity {
    $gameRoot = Assert-ContainedNonReparsePath -Path $canonicalGamePath `
        -Description 'Canonical game tree'
    $manifestPath = Assert-ContainedNonReparsePath `
        -Path (Join-Path $repoRoot 'private/game-manifest.json') `
        -Description 'Canonical game manifest'
    $verifierPath = Assert-ContainedNonReparsePath -Path $gameVerifierPath `
        -Description 'Game-manifest verifier'
    $verified = & $verifierPath -GamePath $gameRoot -ManifestPath $manifestPath -VerifyHashes
    $tree = Get-TreeSnapshot $gameRoot
    [pscustomobject]@{
        file_count = $verified.FileCount
        payload_bytes = $verified.PayloadBytes
        hashes_verified = $verified.HashesVerified
        manifest_sha256 = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
        tree_sha256 = $tree.Hash
        tree_file_count = $tree.FileCount
        tree_directory_count = $tree.DirectoryCount
        tree_bytes = $tree.Bytes
    }
}

function Get-CanonicalArtifactSnapshot {
    $buildRoot = Assert-ContainedNonReparsePath -Path $canonicalBuildPath `
        -Description 'Canonical build tree'
    @(@('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll') |
        ForEach-Object {
            $path = Assert-ContainedNonReparsePath -Path (Join-Path $buildRoot $_) `
                -Description "Canonical runtime artifact $_"
            [pscustomobject]@{
                name = $_
                sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
            }
        })
}

function Get-ExactCanonicalProcesses {
    $executable = [System.IO.Path]::GetFullPath((Join-Path $canonicalBuildPath 'mcla.exe'))
    @((Get-Process -Name 'mcla' -ErrorAction SilentlyContinue) | Where-Object {
            try {
                [string]::Equals($_.Path, $executable,
                    [System.StringComparison]::OrdinalIgnoreCase)
            } catch { $false }
        })
}

function Get-UInt16LE {
    param([byte[]]$Bytes, [int]$Offset)
    [uint16]([uint16]$Bytes[$Offset] -bor ([uint16]$Bytes[$Offset + 1] -shl 8))
}

function Get-UInt32LE {
    param([byte[]]$Bytes, [int]$Offset)
    [uint32]([uint32]$Bytes[$Offset] -bor ([uint32]$Bytes[$Offset + 1] -shl 8) -bor
        ([uint32]$Bytes[$Offset + 2] -shl 16) -bor ([uint32]$Bytes[$Offset + 3] -shl 24))
}

function Get-BmpEvidence {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Render-path BMP was not found: '$Path'."
    }
    $item = Get-Item -LiteralPath $Path
    if ($item.Length -lt 54 -or $item.Length -gt 268435510) {
        throw 'Render-path BMP size is outside the reviewed bound.'
    }
    [byte[]]$bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes[0] -ne 0x42 -or $bytes[1] -ne 0x4D) { throw 'Capture is not a BMP file.' }
    $declaredSize = Get-UInt32LE $bytes 2
    $pixelOffset = Get-UInt32LE $bytes 10
    $dibSize = Get-UInt32LE $bytes 14
    $width = [System.BitConverter]::ToInt32($bytes, 18)
    $signedHeight = [System.BitConverter]::ToInt32($bytes, 22)
    $planes = Get-UInt16LE $bytes 26
    $bitsPerPixel = Get-UInt16LE $bytes 28
    $compression = Get-UInt32LE $bytes 30
    if ($declaredSize -ne $bytes.Length -or $pixelOffset -ne 54 -or $dibSize -ne 40 -or
        $planes -ne 1 -or $bitsPerPixel -ne 32 -or $compression -ne 0 -or
        $width -ne 1280 -or [Math]::Abs([long]$signedHeight) -ne 720) {
        throw 'Capture must be an exact 1280x720 32-bpp BI_RGB BMP.'
    }
    $height = 720
    $stride = 5120
    if ([long]$pixelOffset + ([long]$stride * $height) -ne $bytes.Length) {
        throw 'Capture BMP payload length is inconsistent.'
    }
    $binCounts = [int[]]::new(32768)
    $lumaCounts = [int[]]::new(256)
    $quantized = [int[]]::new($width * $height)
    $pixelIndex = 0
    for ($row = 0; $row -lt $height; $row++) {
        $sourceRow = if ($signedHeight -gt 0) { $height - 1 - $row } else { $row }
        $rowOffset = $pixelOffset + $sourceRow * $stride
        for ($x = 0; $x -lt $width; $x++) {
            $offset = $rowOffset + $x * 4
            $blue = [int]$bytes[$offset]
            $green = [int]$bytes[$offset + 1]
            $red = [int]$bytes[$offset + 2]
            $bin = (($red -shr 3) -shl 10) -bor (($green -shr 3) -shl 5) -bor ($blue -shr 3)
            $quantized[$pixelIndex++] = $bin
            $binCounts[$bin]++
            $lumaCounts[((54 * $red + 183 * $green + 19 * $blue + 128) -shr 8)]++
        }
    }
    $occupied = 0
    $modalBin = 0
    $modalPixels = 0
    for ($index = 0; $index -lt $binCounts.Length; $index++) {
        if ($binCounts[$index] -gt 0) {
            $occupied++
            if ($binCounts[$index] -gt $modalPixels) {
                $modalPixels = $binCounts[$index]
                $modalBin = $index
            }
        }
    }
    $targets = @(46080, 875520)
    $percentiles = @(-1, -1)
    $cumulative = 0L
    for ($value = 0; $value -lt 256; $value++) {
        $cumulative += $lumaCounts[$value]
        for ($p = 0; $p -lt 2; $p++) {
            if ($percentiles[$p] -lt 0 -and $cumulative -ge $targets[$p]) {
                $percentiles[$p] = $value
            }
        }
    }
    $cells = [bool[]]::new(144)
    for ($y = 0; $y -lt $height; $y++) {
        $gridY = [Math]::Min(8, [int][Math]::Floor(
                ([double]([long]$y * 9)) / ([double]$height)))
        for ($x = 0; $x -lt $width; $x++) {
            if ($quantized[$y * $width + $x] -ne $modalBin) {
                $gridX = [Math]::Min(15, [int][Math]::Floor(
                        ([double]([long]$x * 16)) / ([double]$width)))
                $cells[$gridY * 16 + $gridX] = $true
            }
        }
    }
    $cellCount = @($cells | Where-Object { $_ }).Count
    $modalPermille = [int][Math]::Floor(
        ([double]([long]$modalPixels * 1000)) / ([double]($width * $height)))
    if ($occupied -lt 16 -or ($percentiles[1] - $percentiles[0]) -lt 8 -or
        $modalPermille -gt 995 -or $cellCount -lt 4) {
        throw 'Capture is uniform or below the physical nontrivial-image thresholds.'
    }
    [pscustomobject]@{
        Width = $width; Height = $height; Stride = $stride; PixelCount = $width * $height
        OccupiedRgb555Bins = $occupied; LumaP05 = $percentiles[0]; LumaP95 = $percentiles[1]
        LumaSpread = $percentiles[1] - $percentiles[0]; ModalPixels = $modalPixels
        ModalPermille = $modalPermille; NonmodalGridCells = $cellCount
        Bytes = $item.Length; Sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    }
}

function Get-EdgeVector {
    param([System.Drawing.Bitmap]$Bitmap, [System.Drawing.Rectangle]$Roi)
    if ($Roi.Left -lt 1 -or $Roi.Top -lt 1 -or $Roi.Right -ge $Bitmap.Width -or
        $Roi.Bottom -ge $Bitmap.Height) {
        throw 'Edge-correlation ROI is outside the reviewed image bounds.'
    }
    $values = [double[]]::new(($Roi.Width - 2) * ($Roi.Height - 2))
    $index = 0
    for ($y = $Roi.Top + 1; $y -lt $Roi.Bottom - 1; $y++) {
        for ($x = $Roi.Left + 1; $x -lt $Roi.Right - 1; $x++) {
            $left = $Bitmap.GetPixel($x - 1, $y)
            $right = $Bitmap.GetPixel($x + 1, $y)
            $up = $Bitmap.GetPixel($x, $y - 1)
            $down = $Bitmap.GetPixel($x, $y + 1)
            $lumaLeft = (54 * $left.R + 183 * $left.G + 19 * $left.B + 128) -shr 8
            $lumaRight = (54 * $right.R + 183 * $right.G + 19 * $right.B + 128) -shr 8
            $lumaUp = (54 * $up.R + 183 * $up.G + 19 * $up.B + 128) -shr 8
            $lumaDown = (54 * $down.R + 183 * $down.G + 19 * $down.B + 128) -shr 8
            $gx = $lumaRight - $lumaLeft
            $gy = $lumaDown - $lumaUp
            $values[$index++] = [Math]::Sqrt([double]($gx * $gx + $gy * $gy))
        }
    }
    return ,$values
}

function Get-NormalizedCorrelation {
    param([double[]]$Left, [double[]]$Right)
    if ($Left.Count -ne $Right.Count -or $Left.Count -lt 2) {
        throw 'Edge-correlation vectors have inconsistent lengths.'
    }
    $sumLeft = 0.0
    $sumRight = 0.0
    $sumLeftSquare = 0.0
    $sumRightSquare = 0.0
    $sumProduct = 0.0
    for ($index = 0; $index -lt $Left.Count; $index++) {
        $a = $Left[$index]
        $b = $Right[$index]
        $sumLeft += $a
        $sumRight += $b
        $sumLeftSquare += $a * $a
        $sumRightSquare += $b * $b
        $sumProduct += $a * $b
    }
    $count = [double]$Left.Count
    $numerator = $count * $sumProduct - $sumLeft * $sumRight
    $leftVariance = $count * $sumLeftSquare - $sumLeft * $sumLeft
    $rightVariance = $count * $sumRightSquare - $sumRight * $sumRight
    $denominator = [Math]::Sqrt([Math]::Max(0.0, $leftVariance * $rightVariance))
    if ($denominator -le 0.0) { throw 'Edge-correlation ROI has zero variance.' }
    return [Math]::Max(-1.0, [Math]::Min(1.0, $numerator / $denominator))
}

function Get-ReferenceEvidence {
    param([string]$Path)
    $resolved = Assert-ContainedNonReparsePath -Path $Path -Description 'Stock title reference'
    if ((Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash -ne
        $expectedReferenceSha256) {
        throw 'Stock title reference SHA-256 does not match the pinned private PNG.'
    }
    $image = [System.Drawing.Image]::FromFile($resolved)
    try {
        if ($image.Width -ne 1280 -or $image.Height -ne 720 -or
            $image.RawFormat.Guid -ne [System.Drawing.Imaging.ImageFormat]::Png.Guid) {
            throw 'Stock title reference must be the exact 1280x720 PNG.'
        }
    } finally { $image.Dispose() }
    [pscustomobject]@{
        Path = $resolved
        Sha256 = $expectedReferenceSha256
        Bytes = (Get-Item -LiteralPath $resolved).Length
        Width = 1280
        Height = 720
    }
}

function Get-RoiEvidence {
    param([string]$CandidatePath, [string]$ReferencePath, [bool]$AllowLocalizedPrompt,
        [string]$PromptReferencePath)
    $candidateImage = [System.Drawing.Image]::FromFile($CandidatePath)
    $referenceImage = [System.Drawing.Image]::FromFile($ReferencePath)
    $candidate = [System.Drawing.Bitmap]::new($candidateImage)
    $reference = [System.Drawing.Bitmap]::new($referenceImage)
    try {
        if ($candidate.Width -ne 1280 -or $candidate.Height -ne 720 -or
            $reference.Width -ne 1280 -or $reference.Height -ne 720) {
            throw 'ROI comparison requires exact 1280x720 images.'
        }
        $logo = Get-NormalizedCorrelation `
            -Left (Get-EdgeVector -Bitmap $candidate -Roi $logoRoi) `
            -Right (Get-EdgeVector -Bitmap $reference -Roi $logoRoi)
        $press = Get-NormalizedCorrelation `
            -Left (Get-EdgeVector -Bitmap $candidate -Roi $pressRoi) `
            -Right (Get-EdgeVector -Bitmap $reference -Roi $pressRoi)
        $logoPpm = [int][Math]::Round($logo * 1000000.0)
        $pressPpm = [int][Math]::Round($press * 1000000.0)
        if ($logoPpm -lt 900000) {
            throw "Title-logo ROI edge correlation is below 0.90 ($logo)."
        }
        $localizedPromptPpm = 0
        if ($AllowLocalizedPrompt) {
            if (-not $PromptReferencePath) {
                throw 'Localized prompt comparison requires a pinned reference image.'
            }
            $localizedPromptRoi = [System.Drawing.Rectangle]::new(1000, 620, 250, 65)
            $promptReferenceImage = [System.Drawing.Image]::FromFile($PromptReferencePath)
            $promptReference = [System.Drawing.Bitmap]::new($promptReferenceImage)
            try {
                if ($promptReference.Width -ne 1280 -or $promptReference.Height -ne 720) {
                    throw 'Localized prompt reference must be exactly 1280x720.'
                }
                $localizedPrompt = Get-NormalizedCorrelation `
                    -Left (Get-EdgeVector -Bitmap $candidate -Roi $localizedPromptRoi) `
                    -Right (Get-EdgeVector -Bitmap $promptReference -Roi $localizedPromptRoi)
                $localizedPromptPpm = [int][Math]::Round($localizedPrompt * 1000000.0)
            } finally {
                $promptReference.Dispose()
                $promptReferenceImage.Dispose()
            }
            if ($localizedPromptPpm -lt 900000) {
                throw "Localized title-prompt ROI edge correlation is below 0.90 ($localizedPrompt)."
            }
        } elseif ($pressPpm -lt 900000) {
            throw "PRESS ROI edge correlation is below 0.90 ($press)."
        }
        [pscustomobject]@{
            LogoCorrelationPpm = $logoPpm
            PressCorrelationPpm = $pressPpm
            LocalizedPrompt = $AllowLocalizedPrompt
            LocalizedPromptCorrelationPpm = $localizedPromptPpm
        }
    } finally {
        $candidate.Dispose()
        $reference.Dispose()
        $candidateImage.Dispose()
        $referenceImage.Dispose()
    }
}

function Get-SingleRegexMatch {
    param([string]$Text, [string]$Pattern, [string]$Description)
    $matches = [regex]::Matches($Text, $Pattern)
    if ($matches.Count -ne 1) { throw "Render-path log requires exactly one $Description marker." }
    return $matches[0]
}

function Get-RuntimeLogSet {
    param([string]$CurrentLogPath)
    if ((Split-Path -Leaf $CurrentLogPath) -cne 'mcla.log') {
        throw 'Render-path current runtime log must use the exact mcla.log name.'
    }
    $directory = Split-Path -Parent $CurrentLogPath
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        throw 'Render-path runtime-log directory is missing.'
    }
    $candidates = @(Get-ChildItem -LiteralPath $directory -File -Force -Filter 'mcla*.log')
    if ($candidates.Count -lt 1 -or $candidates.Count -gt 16) {
        throw 'Runtime log set must contain 1-16 bounded files.'
    }
    $current = @($candidates | Where-Object { $_.Name -ceq 'mcla.log' })
    if ($current.Count -ne 1) { throw 'Runtime log set requires exactly one mcla.log.' }
    $rotations = @()
    foreach ($candidate in $candidates) {
        if ($candidate.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "Runtime log set contains a reparse point: '$($candidate.Name)'."
        }
        if ($candidate.Name -ceq 'mcla.log') { continue }
        $match = [regex]::Match($candidate.Name, '^mcla\.(?<index>[1-9][0-9]*)\.log$')
        if (-not $match.Success) { throw "Malformed or extra runtime log '$($candidate.Name)'." }
        $index = [int]$match.Groups['index'].Value
        if ($index -gt 15) { throw 'Runtime log rotation index exceeds the reviewed bound.' }
        $rotations += [pscustomobject]@{ Index = $index; Item = $candidate }
    }
    if ($rotations.Count -gt 0) {
        $indices = @($rotations.Index | Sort-Object)
        for ($index = 0; $index -lt $indices.Count; $index++) {
            if ($indices[$index] -ne ($index + 1)) {
                throw 'Runtime log rotation set has a gap or duplicate index.'
            }
        }
    }
    # Rotating loggers keep .1 as the newest rotation; highest index is oldest.
    $orderedItems = @($rotations | Sort-Object Index -Descending | ForEach-Object { $_.Item }) +
        @($current[0])
    $manifest = @()
    $parts = @()
    $totalBytes = 0L
    $previousWrite = [datetime]::MinValue
    foreach ($item in $orderedItems) {
        if ($item.Length -gt 8388608) { throw "Runtime log '$($item.Name)' exceeds 8 MiB." }
        $totalBytes += [long]$item.Length
        if ($totalBytes -gt 100663296) { throw 'Runtime log set exceeds 96 MiB.' }
        if ($item.LastWriteTimeUtc -lt $previousWrite) {
            throw 'Runtime log file timestamps contradict chronological rotation order.'
        }
        $previousWrite = $item.LastWriteTimeUtc
        $hash = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash
        $manifest += [ordered]@{ name = $item.Name; bytes = $item.Length; sha256 = $hash }
        $parts += [System.IO.File]::ReadAllText($item.FullName)
    }
    $manifestJson = ConvertTo-Json -InputObject @($manifest) -Depth 3 -Compress
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $setHash = -join ($sha.ComputeHash($utf8.GetBytes($manifestJson)) |
            ForEach-Object { $_.ToString('X2') })
    } finally { $sha.Dispose() }
    [pscustomobject]@{
        Files = @($manifest)
        FileCount = $manifest.Count
        TotalBytes = $totalBytes
        SetSha256 = $setHash
        Text = ($parts -join [Environment]::NewLine)
    }
}

function Get-FirstFrameLogEvidence {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Render-path runtime log was not found: '$Path'."
    }
    $logSet = Get-RuntimeLogSet -CurrentLogPath $Path
    $log = $logSet.Text
    foreach ($pattern in $bannedPatterns) {
        if ($log -match $pattern) { throw "Render-path log contains banned pattern '$pattern'." }
    }
    $refresh = Get-SingleRegexMatch $log $firstFrameMarkers.Refresh 'active guest-output refresh'
    $presentMatches = [regex]::Matches($log, $firstFrameMarkers.Present)
    $presentOne = @($presentMatches | Where-Object { $_.Groups['count'].Value -eq '1' })
    $presentThree = @($presentMatches | Where-Object { $_.Groups['count'].Value -eq '3' })
    if ($presentMatches.Count -ne 2 -or $presentOne.Count -ne 1 -or $presentThree.Count -ne 1) {
        throw 'Render-path log requires exact distinct count-1/count-3 successful presents.'
    }
    $presentOne = $presentOne[0]
    $presentThree = $presentThree[0]
    $project = Get-SingleRegexMatch $log $firstFrameMarkers.Project 'settled project capture'
    $captureMatches = @([regex]::Matches($log, $firstFrameMarkers.Capture) |
        Where-Object { $_.Index -lt $project.Index })
    if ($captureMatches.Count -lt 2 -or $captureMatches.Count -gt 4096) {
        throw 'Render-path log requires 2-4096 pre-project capture successes.'
    }
    $previousSequence = 0L
    $previousPresented = 0L
    foreach ($candidate in $captureMatches) {
        $sequence = [long]$candidate.Groups['sequence'].Value
        $presented = [long]$candidate.Groups['last_presented_sequence'].Value
        if ($sequence -lt 1 -or $sequence -lt $previousSequence -or
            $presented -lt $sequence -or $presented -lt $previousPresented) {
            throw 'Render-path capture is not monotonically bound to successful presentation.'
        }
        $previousSequence = $sequence
        $previousPresented = $presented
    }
    $capture = $captureMatches[$captureMatches.Count - 1]
    $windowMatches = [regex]::Matches($log, [regex]::Escape($firstFrameMarkers.WindowClose))
    $executionMatches = [regex]::Matches($log, $firstFrameMarkers.ExecutionComplete)
    $exitMatches = [regex]::Matches($log, [regex]::Escape($firstFrameMarkers.HardExit))
    if ($windowMatches.Count -ne 1 -or $executionMatches.Count -ne 1 -or
        $exitMatches.Count -ne 1) {
        throw 'Render-path log lacks the exact WM_CLOSE/execution-complete/hard-exit lifecycle.'
    }
    if ($refresh.Index -ge $presentThree.Index -or $presentOne.Index -ge $presentThree.Index -or
        $presentThree.Index -ge $capture.Index -or $capture.Index -ge $project.Index -or
        $project.Index -ge $windowMatches[0].Index -or
        $windowMatches[0].Index -ge $executionMatches[0].Index -or
        $executionMatches[0].Index -ge $exitMatches[0].Index) {
        throw 'Render-path lifecycle/capture markers are out of order.'
    }
    foreach ($summaryPrefix in @('XENOS_AUDIT_RT_SUMMARY v=1 phase=checkpoint',
            'XENOS_AUDIT_RESOLVE_SUMMARY v=1 phase=checkpoint',
            'XENOS_AUDIT_PIPELINE_SUMMARY v=1 phase=checkpoint',
            'XENOS_AUDIT_CP_SUMMARY v=1 phase=checkpoint')) {
        $summaryPosition = $log.IndexOf($summaryPrefix, [System.StringComparison]::Ordinal)
        if ($summaryPosition -le $project.Index -or $summaryPosition -ge $windowMatches[0].Index) {
            throw 'Render-audit checkpoint summary is not bound after capture and before WM_CLOSE.'
        }
    }
    $presentOneSequence = [long]$presentOne.Groups['sequence'].Value
    $presentThreeSequence = [long]$presentThree.Groups['sequence'].Value
    $captureSequence = [long]$capture.Groups['sequence'].Value
    $capturePresented = [long]$capture.Groups['last_presented_sequence'].Value
    $hresultOne = [Convert]::ToUInt32($presentOne.Groups['hresult'].Value, 16)
    $hresultThree = [Convert]::ToUInt32($presentThree.Groups['hresult'].Value, 16)
    if ($presentOneSequence -lt 1 -or $presentThreeSequence -le $presentOneSequence -or
        $captureSequence -lt $presentThreeSequence -or $capturePresented -lt $captureSequence -or
        ($hresultOne -band 0x80000000) -ne 0 -or ($hresultThree -band 0x80000000) -ne 0) {
        throw 'Render-path presentation/capture watermark contract failed.'
    }
    $dimensions = @(
        [int]$refresh.Groups['width'].Value, [int]$refresh.Groups['height'].Value,
        [int]$presentThree.Groups['source_width'].Value,
        [int]$presentThree.Groups['source_height'].Value,
        [int]$presentThree.Groups['swap_width'].Value,
        [int]$presentThree.Groups['swap_height'].Value,
        [int]$capture.Groups['width'].Value, [int]$capture.Groups['height'].Value,
        [int]$project.Groups['width'].Value, [int]$project.Groups['height'].Value)
    if (@($dimensions | Where-Object { $_ -ne 1280 -and $_ -ne 720 }).Count -ne 0 -or
        $dimensions[0] -ne 1280 -or $dimensions[1] -ne 720 -or
        $dimensions[2] -ne 1280 -or $dimensions[3] -ne 720 -or
        $dimensions[4] -ne 1280 -or $dimensions[5] -ne 720 -or
        $dimensions[6] -ne 1280 -or $dimensions[7] -ne 720 -or
        $dimensions[8] -ne 1280 -or $dimensions[9] -ne 720) {
        throw 'Render-path source/swap/capture/project dimensions are not exact 1280x720.'
    }
    $startupPositions = @()
    foreach ($entry in $startupMarkers.GetEnumerator()) {
        $position = $log.IndexOf($entry.Value, [System.StringComparison]::Ordinal)
        if ($position -lt 0) { throw "Required startup marker '$($entry.Key)' is missing." }
        $startupPositions += $position
    }
    for ($index = 1; $index -lt $startupPositions.Count; $index++) {
        if ($startupPositions[$index] -le $startupPositions[$index - 1]) {
            throw 'Startup markers are out of order across the rotated log set.'
        }
    }
    foreach ($pattern in @('(?i)gpu_plugin not set', '(?i)\[(?:fatal|critical)\]',
            '(?i)invalid or unregistered function', '(?i)PPC_UNIMPLEMENTED',
            '(?i)REX_GUEST_CRASH', '(?i)GPU plugin .* failed to load',
            '(?i)loaded image contract rejected', '(?i)disc-root contract rejected')) {
        if ($log -match $pattern) { throw "Startup route contains banned pattern '$pattern'." }
    }
    [pscustomobject]@{
        Text = $log
        StartupMarkerCount = $startupMarkers.Count
        PresentOneSequence = $presentOneSequence
        PresentThreeSequence = $presentThreeSequence
        PresentOneHresult = ('0x{0:X8}' -f $hresultOne)
        PresentThreeHresult = ('0x{0:X8}' -f $hresultThree)
        CaptureSequence = $captureSequence
        CaptureLastPresentedSequence = $capturePresented
        CaptureSuccessMarkerCount = $captureMatches.Count
        ProjectOccupiedRgb555Bins = [int]$project.Groups['bins'].Value
        ProjectLumaP05 = [int]$project.Groups['p05'].Value
        ProjectLumaP95 = [int]$project.Groups['p95'].Value
        ProjectModalPermille = [int]$project.Groups['modal'].Value
        ProjectNonmodalGridCells = [int]$project.Groups['cells'].Value
        WindowCloseMarkers = 1
        ExecutionCompleteMarkers = 1
        HardExitMarkers = 1
        PostHardExitExecutionCompleteMarkers = 0
        RuntimeLogs = $logSet.Files
        RuntimeLogFileCount = $logSet.FileCount
        RuntimeLogBytes = $logSet.TotalBytes
        RuntimeLogSetSha256 = $logSet.SetSha256
    }
}

function Get-AuditInteger {
    param([System.Text.RegularExpressions.Match]$Match, [string]$Name)
    return [long]$Match.Groups[$Name].Value
}

function Assert-ContiguousIdSet {
    param([object[]]$Matches, [string]$Description)
    $ids = @($Matches | ForEach-Object { Get-AuditInteger $_ 'id' } | Sort-Object -Unique)
    if ($ids.Count -ne $Matches.Count) { throw "$Description IDs are duplicated." }
    for ($index = 0; $index -lt $ids.Count; $index++) {
        if ($ids[$index] -ne $index) {
            throw "$Description IDs do not form the exact contiguous zero-based set."
        }
    }
}

function Get-AuditEvidence {
    param([string]$LogText)
    $payloads = @()
    foreach ($line in @($LogText -split "`r?`n")) {
        $offset = $line.IndexOf('XENOS_AUDIT_', [System.StringComparison]::Ordinal)
        if ($offset -ge 0) { $payloads += $line.Substring($offset).Trim() }
    }
    if ($payloads.Count -lt 13 -or $payloads.Count -gt 1240) {
        throw 'Render audit marker count is outside the reviewed bound.'
    }
    $matchesByType = [ordered]@{}
    foreach ($name in $auditGrammar.Keys) { $matchesByType[$name] = @() }
    $summaryStarted = $false
    $summaryNames = @('RtSummary', 'ResolveSummary', 'PipelineSummary', 'CpSummary')
    foreach ($payload in $payloads) {
        $matchedName = $null
        $matchedValue = $null
        foreach ($name in $auditGrammar.Keys) {
            $candidate = [regex]::Match($payload, $auditGrammar[$name])
            if ($candidate.Success) {
                if ($null -ne $matchedName) { throw "Audit marker matched multiple grammars: '$payload'." }
                $matchedName = $name
                $matchedValue = $candidate
            }
        }
        if ($null -eq $matchedName) { throw "Unknown or malformed render-audit marker: '$payload'." }
        if ($matchedName -in $summaryNames) {
            $summaryStarted = $true
        } elseif ($summaryStarted) {
            throw 'Render-audit detail or config marker appeared after the frozen summary boundary.'
        }
        $matchesByType[$matchedName] += $matchedValue
    }

    foreach ($name in @('Config', 'RtSummary', 'ResolveSummary', 'PipelineSummary', 'CpSummary')) {
        if ($matchesByType[$name].Count -ne 1) {
            throw "Render audit requires exactly one $name marker."
        }
    }
    foreach ($name in $summaryNames) {
        if ($matchesByType[$name][0].Groups['phase'].Value -ne 'checkpoint') {
            throw 'Canonical render audit requires the explicit successful-swap checkpoint phase.'
        }
    }
    $limits = [ordered]@{
        Rt = 128; Bind = 128; Ownership = 64; Resolve = 128
        Shader = 256; Pso = 512; Gamma = 16
    }
    foreach ($name in $limits.Keys) {
        if ($matchesByType[$name].Count -lt 1 -or $matchesByType[$name].Count -gt $limits[$name]) {
            throw "Render audit $name marker cardinality is outside 1-$($limits[$name])."
        }
        Assert-ContiguousIdSet -Matches $matchesByType[$name] -Description "Render audit $name"
    }

    $config = $matchesByType.Config[0]
    $native2xSupported = Get-AuditInteger $config 'native_2x_supported'
    if ($config.Groups['rt_path'].Value -ne 'host' -or
        (Get-AuditInteger $config 'scale_x') -ne 1 -or
        (Get-AuditInteger $config 'scale_y') -ne 1) {
        throw 'Render audit final gate requires the D3D12 host RTV/DSV path at native 1x scale.'
    }

    $rtKinds = @{}
    $rtMsaa = @{}
    $rtTuples = @{}
    foreach ($record in $matchesByType.Rt) {
        $kind = $record.Groups['kind'].Value
        $guestFormat = Get-AuditInteger $record 'storage_fmt'
        $guestSamples = Get-AuditInteger $record 'guest_msaa'
        $hostSamples = Get-AuditInteger $record 'host_samples'
        $emulated2x = Get-AuditInteger $record 'emulated_2x'
        $resourceDxgi = Get-AuditInteger $record 'resource_dxgi'
        $drawDxgi = Get-AuditInteger $record 'draw_dxgi'
        $depthSrvDxgi = Get-AuditInteger $record 'depth_srv_dxgi'
        if ($resourceDxgi -lt 0 -or ($kind -eq 'color' -and $drawDxgi -lt 0) -or
            ($kind -eq 'depth' -and $depthSrvDxgi -lt 0)) {
            throw 'Render audit contains an unknown required RT/DXGI mapping.'
        }
        if ($kind -eq 'color' -and $guestFormat -notin @(0, 1, 2, 3, 4, 5, 6, 7, 10, 12, 14, 15)) {
            throw 'Render audit contains an unknown color render-target format.'
        }
        if ($kind -eq 'depth' -and $guestFormat -notin @(0, 1)) {
            throw 'Render audit contains an unknown depth render-target format.'
        }
        if ($guestSamples -eq 2) {
            $expectedHostSamples = if ($native2xSupported -eq 1) { 2 } else { 4 }
            $expectedEmulated2x = if ($native2xSupported -eq 1) { 0 } else { 1 }
            if ($hostSamples -ne $expectedHostSamples -or
                $emulated2x -ne $expectedEmulated2x) {
                throw '2x RT mapping contradicts native support or its emulation flag.'
            }
        } elseif ($guestSamples -ne $hostSamples -or $emulated2x -ne 0) {
            throw 'Render audit silently changed RT sample count.'
        }
        $rtKinds[$kind] = $true
        $rtMsaa[[string]$guestSamples] = $true
        $rtTuples["$kind|$guestFormat|$guestSamples"] = $true
    }
    foreach ($required in @('color', 'depth')) {
        if (-not $rtKinds.ContainsKey($required)) { throw "Render audit lacks $required RT coverage." }
    }
    foreach ($required in @('1', '2', '4')) {
        if (-not $rtMsaa.ContainsKey($required)) { throw "Render audit lacks ${required}x RT coverage." }
    }

    $bindKinds = @{}
    $bindMsaa = @{}
    $bindMatches = $matchesByType.Bind
    for ($bindIndex = 0; $bindIndex -lt $bindMatches.Count; $bindIndex++) {
        $record = $bindMatches[$bindIndex]
        if ((Get-AuditInteger $record 'id') -ne $bindIndex) {
            throw 'Single-threaded BIND records are not in contiguous textual order.'
        }
        $slot = $record.Groups['slot'].Value
        $guestFormat = Get-AuditInteger $record 'guest_fmt'
        $storageFormat = Get-AuditInteger $record 'storage_fmt'
        $hostBound = Get-AuditInteger $record 'host_bound'
        if (($hostBound -eq 0 -and $storageFormat -ne -1) -or
            ($hostBound -eq 1 -and $storageFormat -lt 0)) {
            throw 'BIND record host-bound and storage-format fields are inconsistent.'
        }
        $bindKind = if ($slot -eq 'depth') { 'depth' } else { 'color' }
        $bindSamples = Get-AuditInteger $record 'guest_msaa'
        if ($hostBound -eq 1 -and
            -not $rtTuples.ContainsKey("$bindKind|$storageFormat|$bindSamples")) {
            throw 'Host-bound BIND tuple is absent from the emitted RT inventory.'
        }
        if ($slot -eq 'depth') {
            if ($guestFormat -notin @(0, 1)) { throw 'BIND uses an unknown guest depth format.' }
            $bindKinds['depth'] = $true
            if ($hostBound -eq 1) { $bindKinds['bound-depth'] = $true }
        } else {
            if ($guestFormat -notin @(0, 1, 2, 3, 4, 5, 6, 7, 10, 12, 14, 15)) {
                throw 'BIND uses an unknown guest color format.'
            }
            $bindKinds['color'] = $true
            if ($hostBound -eq 1) { $bindKinds['bound-color'] = $true }
        }
        $bindMsaa[[string]$bindSamples] = $true
    }
    foreach ($required in @('color', 'depth', 'bound-color', 'bound-depth')) {
        if (-not $bindKinds.ContainsKey($required)) { throw "BIND inventory lacks $required coverage." }
    }
    foreach ($required in @('1', '2', '4')) {
        if (-not $bindMsaa.ContainsKey($required)) { throw "BIND inventory lacks ${required}x coverage." }
    }

    $ownership = $matchesByType.Ownership
    foreach ($record in $ownership) {
        foreach ($pair in @(@('src_guest_msaa', 'src_host_samples'),
                @('dst_guest_msaa', 'dst_host_samples'))) {
            $guest = Get-AuditInteger $record $pair[0]
            $hostSampleCount = Get-AuditInteger $record $pair[1]
            $expectedHostSampleCount = if ($guest -eq 2 -and $native2xSupported -eq 0) {
                4
            } else {
                $guest
            }
            if ($hostSampleCount -ne $expectedHostSampleCount) {
                throw 'Ownership conversion has an unsupported sample mapping.'
            }
        }
    }

    $resolveKinds = @{}
    $hasHdrColorResolve = $false
    foreach ($record in $matchesByType.Resolve) {
        $kind = $record.Groups['src_kind'].Value
        $sourceFormat = Get-AuditInteger $record 'src_fmt'
        $shader = Get-AuditInteger $record 'shader'
        if ($shader -lt 0 -or $shader -gt 8) {
            throw 'Resolve uses an unknown common-copy shader.'
        }
        if ($kind -eq 'color') {
            if ($sourceFormat -notin @(0, 1, 2, 3, 4, 5, 6, 7, 10, 12, 14, 15)) {
                throw 'Resolve uses an unknown color RT source format.'
            }
            if ($sourceFormat -in @(3, 5, 6, 7, 10, 12, 14, 15)) { $hasHdrColorResolve = $true }
        } elseif ($sourceFormat -notin @(0, 1)) {
            throw 'Resolve uses an unknown depth RT source format.'
        }
        $resolveKinds[$kind] = $true
    }
    if (-not $resolveKinds.ContainsKey('color') -or -not $resolveKinds.ContainsKey('depth') -or
        -not $hasHdrColorResolve) {
        throw 'Resolve inventory lacks color, depth, or HDR-source common-copy coverage.'
    }

    foreach ($record in $matchesByType.Shader) {
        if ($record.Groups['result'].Value -ne 'ok') { throw 'Shader translation record failed.' }
    }
    if (@($matchesByType.Shader | Where-Object { $_.Groups['stage'].Value -eq 'vs' }).Count -lt 1 -or
        @($matchesByType.Shader | Where-Object { $_.Groups['stage'].Value -eq 'ps' }).Count -lt 1) {
        throw 'Render audit lacks successful VS or PS translation coverage.'
    }
    foreach ($record in $matchesByType.Pso) {
        $hresult = [Convert]::ToUInt32($record.Groups['hresult'].Value, 16)
        if ($record.Groups['result'].Value -ne 'ok' -or ($hresult -band 0x80000000) -ne 0) {
            throw 'PSO record contains a failed result or HRESULT.'
        }
    }
    $hasNonidentityGammaUpload = $false
    foreach ($record in $matchesByType.Gamma) {
        if ((Get-AuditInteger $record 'identity') -eq 0 -and
            (Get-AuditInteger $record 'upload') -eq 1) {
            $hasNonidentityGammaUpload = $true
        }
    }
    if (-not $hasNonidentityGammaUpload) {
        throw 'Gamma inventory lacks a nonidentity uploaded dispatch.'
    }

    $rtSummary = $matchesByType.RtSummary[0]
    if ((Get-AuditInteger $rtSummary 'records') -ne $matchesByType.Rt.Count -or
        (Get-AuditInteger $rtSummary 'records') -lt 20 -or
        (Get-AuditInteger $rtSummary 'create_attempt') -ne
            (Get-AuditInteger $rtSummary 'create_ok') -or
        (Get-AuditInteger $rtSummary 'create_fail') -ne 0 -or
        (Get-AuditInteger $rtSummary 'overflow') -ne 0 -or
        (Get-AuditInteger $rtSummary 'host_depth_store') -lt 1 -or
        (Get-AuditInteger $rtSummary 'ownership_draws') -lt 1 -or
        (Get-AuditInteger $rtSummary 'ownership_modes') -ne $ownership.Count -or
        (Get-AuditInteger $rtSummary 'ownership_overflow') -ne 0) {
        throw 'Render-target summary does not prove complete successful coverage.'
    }

    $resolveSummary = $matchesByType.ResolveSummary[0]
    $resolveCalls = Get-AuditInteger $resolveSummary 'calls'
    $directPreflight = Get-AuditInteger $resolveSummary 'direct_preflight'
    $directDumpOk = Get-AuditInteger $resolveSummary 'direct_preflight_dump_ok'
    $directReject = Get-AuditInteger $resolveSummary 'direct_reject'
    $fallbackDumpOk = Get-AuditInteger $resolveSummary 'fallback_dump_ok'
    $fallbackDumpFail = Get-AuditInteger $resolveSummary 'fallback_dump_fail'
    $expectedDirectPreflight = if ((Get-AuditInteger $config 'direct_host_resolve') -eq 1) {
        $resolveCalls
    } else {
        0
    }
    $expectedFallbackAttempts = ($resolveCalls - $directPreflight) + $directReject
    if ($resolveCalls -lt 700 -or
        (Get-AuditInteger $resolveSummary 'info_ok') -ne $resolveCalls -or
        (Get-AuditInteger $resolveSummary 'info_fail') -ne 0 -or
        (Get-AuditInteger $resolveSummary 'zero_area') -ne 0 -or
        (Get-AuditInteger $resolveSummary 'shader_known') -ne $resolveCalls -or
        (Get-AuditInteger $resolveSummary 'shader_unknown') -ne 0 -or
        $directPreflight -ne $expectedDirectPreflight -or
        $directPreflight -ne ($directDumpOk + $directReject) -or
        ($fallbackDumpOk + $fallbackDumpFail) -ne $expectedFallbackAttempts -or
        $fallbackDumpFail -ne 0 -or
        (Get-AuditInteger $resolveSummary 'copy_dispatch') -ne $resolveCalls -or
        (Get-AuditInteger $resolveSummary 'final_ok') -ne $resolveCalls -or
        (Get-AuditInteger $resolveSummary 'final_fail') -ne 0 -or
        (Get-AuditInteger $resolveSummary 'modes') -ne $matchesByType.Resolve.Count -or
        (Get-AuditInteger $resolveSummary 'overflow') -ne 0 -or
        (Get-AuditInteger $resolveSummary 'true_direct_dispatch') -ne 0) {
        throw 'Resolve summary does not prove 700 successful common-copy resolves.'
    }

    $pipelineSummary = $matchesByType.PipelineSummary[0]
    if ((Get-AuditInteger $pipelineSummary 'translate_fail') -ne 0 -or
        (Get-AuditInteger $pipelineSummary 'translate_vs_ok') -lt 1 -or
        (Get-AuditInteger $pipelineSummary 'translate_ps_ok') -lt 1 -or
        (Get-AuditInteger $pipelineSummary 'pso_attempt') -ne
            (Get-AuditInteger $pipelineSummary 'pso_ok') -or
        (Get-AuditInteger $pipelineSummary 'pso_ok') -lt 100 -or
        (Get-AuditInteger $pipelineSummary 'pso_fail') -ne 0 -or
        (Get-AuditInteger $pipelineSummary 'shader_records') -ne $matchesByType.Shader.Count -or
        (Get-AuditInteger $pipelineSummary 'shader_overflow') -ne 0 -or
        (Get-AuditInteger $pipelineSummary 'pso_records') -ne $matchesByType.Pso.Count -or
        (Get-AuditInteger $pipelineSummary 'pso_overflow') -ne 0) {
        throw 'Pipeline summary does not prove successful shader/PSO coverage.'
    }

    $cpSummary = $matchesByType.CpSummary[0]
    $drawIssued = Get-AuditInteger $cpSummary 'draw_issued'
    if ($drawIssued -lt 1 -or $drawIssued -ne
        ((Get-AuditInteger $cpSummary 'draw_indexed') +
        (Get-AuditInteger $cpSummary 'draw_nonindexed')) -or
        (Get-AuditInteger $cpSummary 'pso_pending_skip') -ne 0 -or
        (Get-AuditInteger $cpSummary 'pso_failed_skip') -ne 0 -or
        (Get-AuditInteger $cpSummary 'depth_test') -lt 1 -or
        (Get-AuditInteger $cpSummary 'depth_write') -lt 1 -or
        (Get-AuditInteger $cpSummary 'depth_bound') -lt 1 -or
        (Get-AuditInteger $cpSummary 'depth_without_bound') -gt $drawIssued -or
        (Get-AuditInteger $cpSummary 'bind_records') -ne $matchesByType.Bind.Count -or
        (Get-AuditInteger $cpSummary 'bind_overflow') -ne 0 -or
        (Get-AuditInteger $cpSummary 'msaa1') -lt 1 -or
        (Get-AuditInteger $cpSummary 'msaa2') -lt 1 -or
        (Get-AuditInteger $cpSummary 'msaa4') -lt 1 -or
        ((Get-AuditInteger $cpSummary 'gamma_table_dispatch') +
        (Get-AuditInteger $cpSummary 'gamma_pwl_dispatch')) -lt 1 -or
        (Get-AuditInteger $cpSummary 'gamma_nonidentity_dispatch') -lt 1 -or
        (Get-AuditInteger $cpSummary 'gamma_uploads') -lt 1 -or
        (Get-AuditInteger $cpSummary 'gamma_records') -ne $matchesByType.Gamma.Count -or
        (Get-AuditInteger $cpSummary 'gamma_overflow') -ne 0 -or
        (Get-AuditInteger $cpSummary 'refresh_fail') -ne 0) {
        throw 'Command-processor summary lacks required draw/depth/MSAA/gamma coverage.'
    }

    [pscustomobject]@{
        ConfigCount = 1
        RtRecords = $matchesByType.Rt.Count
        BindRecords = $matchesByType.Bind.Count
        OwnershipModes = $matchesByType.Ownership.Count
        ResolveRecords = $matchesByType.Resolve.Count
        ResolveCalls = $resolveCalls
        ShaderRecords = $matchesByType.Shader.Count
        PsoRecords = $matchesByType.Pso.Count
        PsoOk = Get-AuditInteger $pipelineSummary 'pso_ok'
        DrawIssued = $drawIssued
        DepthTest = Get-AuditInteger $cpSummary 'depth_test'
        DepthWrite = Get-AuditInteger $cpSummary 'depth_write'
        Msaa1 = Get-AuditInteger $cpSummary 'msaa1'
        Msaa2 = Get-AuditInteger $cpSummary 'msaa2'
        Msaa4 = Get-AuditInteger $cpSummary 'msaa4'
        GammaNonidentity = Get-AuditInteger $cpSummary 'gamma_nonidentity_dispatch'
        GammaUploads = Get-AuditInteger $cpSummary 'gamma_uploads'
        SummaryCount = 4
    }
}

function Get-ProbeEvidence {
    param([string]$LogPath, [string]$FramePath, [string]$ReferencePath,
        [bool]$AllowLocalizedPrompt, [string]$PromptReferencePath,
        [string]$PromptReferenceSha256)
    $resolvedLog = Assert-ContainedNonReparsePath -Path $LogPath -Description 'Runtime log'
    $resolvedFrame = Assert-ContainedNonReparsePath -Path $FramePath -Description 'Capture BMP'
    $resolvedReference = Assert-ContainedNonReparsePath -Path $ReferencePath `
        -Description 'Stock title reference'
    $log = Get-FirstFrameLogEvidence -Path $resolvedLog
    $audit = Get-AuditEvidence -LogText $log.Text
    $bmp = Get-BmpEvidence -Path $resolvedFrame
    if ($bmp.OccupiedRgb555Bins -ne $log.ProjectOccupiedRgb555Bins -or
        $bmp.LumaP05 -ne $log.ProjectLumaP05 -or $bmp.LumaP95 -ne $log.ProjectLumaP95 -or
        $bmp.ModalPermille -ne $log.ProjectModalPermille -or
        $bmp.NonmodalGridCells -ne $log.ProjectNonmodalGridCells) {
        throw 'Physical BMP metrics do not match the bound project capture marker.'
    }
    $reference = Get-ReferenceEvidence -Path $resolvedReference
    $resolvedPromptReference = $null
    if ($AllowLocalizedPrompt) {
        $resolvedPromptReference = Assert-ContainedNonReparsePath -Path $PromptReferencePath `
            -Description 'Localized prompt reference'
        if ((Get-FileHash -LiteralPath $resolvedPromptReference -Algorithm SHA256).Hash -cne
            $PromptReferenceSha256) {
            throw 'Localized prompt reference SHA-256 does not match its pinned value.'
        }
    }
    $roi = Get-RoiEvidence -CandidatePath $resolvedFrame -ReferencePath $reference.Path `
        -AllowLocalizedPrompt $AllowLocalizedPrompt -PromptReferencePath $resolvedPromptReference
    [pscustomobject]@{ Log = $log; Bmp = $bmp; Reference = $reference; Roi = $roi; Audit = $audit }
}

if ($PSCmdlet.ParameterSetName -eq 'Probe') {
    if (-not $ProbeOnly) { throw 'Probe inputs require -ProbeOnly.' }
    if (-not $ReferencePngPath) { $ReferencePngPath = $defaultReferencePath }
    if ($LocalizedPrompt.IsPresent -and
        (-not $LocalizedPromptReferencePath -or -not $LocalizedPromptReferenceSha256)) {
        throw 'Localized prompt mode requires its reference path and SHA-256.'
    }
    Get-ProbeEvidence -LogPath $RuntimeLogPath -FramePath $BmpPath `
        -ReferencePath $ReferencePngPath -AllowLocalizedPrompt $LocalizedPrompt.IsPresent `
        -PromptReferencePath $LocalizedPromptReferencePath `
        -PromptReferenceSha256 $LocalizedPromptReferenceSha256
    return
}

$resolvedResult = Assert-ContainedNonReparsePath -Path $ResultPath -Description 'Result path'
if (-not (Test-Path -LiteralPath $resolvedResult -PathType Leaf)) {
    throw "Render-path aggregate result was not found: '$resolvedResult'."
}
$resultItem = Get-Item -LiteralPath $resolvedResult
if ($resultItem.Length -gt 1048576) { throw 'Render-path aggregate exceeded the 1-MiB bound.' }
$json = Get-Content -LiteralPath $resolvedResult -Raw
foreach ($pattern in @('(?i)[A-Z]:[\\/]', '(?i)\\\\[^"\s]+[\\/]',
        '(?i)(?:^|["\\/])private[\\/]')) {
    if ($json -match $pattern) { throw "Render-path aggregate contains prohibited path '$pattern'." }
}
$result = $json | ConvertFrom-Json
Assert-ExactProperties $result @(
    'schema', 'task', 'cycle_count', 'execution_order', 'development_only',
    'capture_timeout_seconds', 'first_frame_settle_seconds',
    'post_marker_dwell_milliseconds', 'exit_timeout_seconds',
    'failure_cleanup_timeout_seconds', 'clean_build', 'first_cycle_post_clean_build',
    'reference', 'game_identity', 'artifacts', 'cycles', 'all_write_roots_contained',
    'all_prior_cycles_immutable', 'no_surviving_processes', 'data_integrity_preserved',
    'all_captures_bound', 'all_title_rois_match', 'all_render_audits_passed') 'Render-path result'
Assert-JsonTypes $result `
    -BooleanNames @('development_only', 'first_cycle_post_clean_build',
        'all_write_roots_contained', 'all_prior_cycles_immutable', 'no_surviving_processes',
        'data_integrity_preserved', 'all_captures_bound', 'all_title_rois_match',
        'all_render_audits_passed') `
    -IntegerNames @('schema', 'cycle_count', 'capture_timeout_seconds',
        'first_frame_settle_seconds',
        'post_marker_dwell_milliseconds', 'exit_timeout_seconds',
        'failure_cleanup_timeout_seconds') `
    -StringNames @('task', 'execution_order') -Description 'Render-path result'
if ($result.schema -ne 1 -or $result.task -ne 'M4-002' -or $result.cycle_count -ne 10 -or
    $result.execution_order -ne 'clean_build_then_10_serial_render_path_cycles' -or
    $result.development_only -ne $false) {
    throw 'Render-path result header does not prove the final ten-run gate.'
}
if ($result.capture_timeout_seconds -ne 60 -or
    $result.first_frame_settle_seconds -ne 35 -or
    $result.post_marker_dwell_milliseconds -ne 2000 -or
    $result.exit_timeout_seconds -ne 10 -or
    $result.failure_cleanup_timeout_seconds -ne 5) {
    throw 'Render-path result does not use canonical bounded deadlines.'
}

$build = $result.clean_build
Assert-ExactProperties $build @('performed', 'success', 'exit_code',
    'duration_milliseconds', 'build_log_sha256', 'executable_sha256') 'Clean build'
Assert-JsonTypes $build -BooleanNames @('performed', 'success') `
    -IntegerNames @('exit_code', 'duration_milliseconds') `
    -StringNames @('build_log_sha256', 'executable_sha256') -Description 'Clean build'
if ($build.performed -ne $true -or $build.success -ne $true -or $build.exit_code -ne 0 -or
    $build.duration_milliseconds -le 0 -or [string]$build.build_log_sha256 -notmatch $hashPattern -or
    [string]$build.executable_sha256 -notmatch $hashPattern -or
    $result.first_cycle_post_clean_build -ne $true) {
    throw 'Render-path result lacks its canonical clean-first build.'
}

$reference = $result.reference
Assert-ExactProperties $reference @('name', 'sha256', 'bytes', 'width', 'height') `
    'Stock title reference'
Assert-JsonTypes $reference -IntegerNames @('bytes', 'width', 'height') `
    -StringNames @('name', 'sha256') -Description 'Stock title reference'
$physicalReference = Get-ReferenceEvidence -Path $defaultReferencePath
if ($reference.name -cne 'xenia-title-reference.png' -or
    $reference.sha256 -ne $physicalReference.Sha256 -or
    $reference.bytes -ne $physicalReference.Bytes -or $reference.width -ne 1280 -or
    $reference.height -ne 720) {
    throw 'Aggregate stock title reference does not match its pinned physical PNG.'
}

Assert-ExactProperties $result.game_identity @('before', 'after') 'Game identity wrapper'
foreach ($phase in @('before', 'after')) {
    $game = $result.game_identity.$phase
    Assert-ExactProperties $game @('file_count', 'payload_bytes', 'hashes_verified',
        'manifest_sha256', 'tree_sha256', 'tree_file_count', 'tree_directory_count',
        'tree_bytes') "Game identity $phase"
    Assert-JsonTypes $game -IntegerNames @('file_count', 'payload_bytes', 'hashes_verified',
        'tree_file_count', 'tree_directory_count', 'tree_bytes') `
        -StringNames @('manifest_sha256', 'tree_sha256') -Description "Game identity $phase"
    if ($game.file_count -ne 15 -or $game.payload_bytes -ne 6569586392 -or
        $game.hashes_verified -ne 15 -or [string]$game.manifest_sha256 -notmatch $hashPattern -or
        [string]$game.tree_sha256 -notmatch $hashPattern -or
        $game.tree_file_count -ne 15 -or $game.tree_directory_count -lt 1 -or
        $game.tree_bytes -ne 6569586392) {
        throw "Supported game identity '$phase' is incomplete."
    }
}
foreach ($field in @('manifest_sha256', 'tree_sha256', 'tree_file_count',
        'tree_directory_count', 'tree_bytes')) {
    if ($result.game_identity.before.$field -ne $result.game_identity.after.$field) {
        throw "Supported game identity '$field' changed during render-path execution."
    }
}

Assert-ExactProperties $result.artifacts @('before', 'after') 'Artifact wrapper'
$artifactNames = @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll')
foreach ($phase in @('before', 'after')) {
    $items = @($result.artifacts.$phase)
    if ($items.Count -ne 4) { throw "Artifact snapshot '$phase' is incomplete." }
    for ($index = 0; $index -lt 4; $index++) {
        Assert-ExactProperties $items[$index] @('name', 'sha256') "Artifact $phase/$index"
        if ($items[$index].name -cne $artifactNames[$index] -or
            [string]$items[$index].sha256 -notmatch $hashPattern) {
            throw "Artifact snapshot '$phase' is invalid or out of order."
        }
    }
}
for ($index = 0; $index -lt 4; $index++) {
    if ($result.artifacts.before[$index].sha256 -ne $result.artifacts.after[$index].sha256) {
        throw "Runtime artifact '$($artifactNames[$index])' changed during the gate."
    }
}
if ($build.executable_sha256 -ne $result.artifacts.before[0].sha256) {
    throw 'Clean-build executable does not match the executed artifact.'
}

if ((Split-Path -Leaf $resolvedResult) -cne 'result.json') {
    throw 'Render-path aggregate must use the exact result.json name.'
}
$runRoot = Split-Path -Parent $resolvedResult
$expectedParent = [System.IO.Path]::GetFullPath((Join-Path $repoRoot 'private/evidence/M4-002'))
if (-not [string]::Equals((Split-Path -Parent $runRoot), $expectedParent,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Render-path run must be an immediate child of private/evidence/M4-002.'
}
Assert-ExactChildren $runRoot @('relwithdebinfo-clean-build.log', 'result.json', 'runs') 'Run root'
$buildLogPath = Assert-ContainedNonReparsePath `
    -Path (Join-Path $runRoot 'relwithdebinfo-clean-build.log') `
    -Description 'Clean-build log'
if ((Get-FileHash -LiteralPath $buildLogPath -Algorithm SHA256).Hash -ne
    $build.build_log_sha256) {
    throw 'Clean-build log hash does not match its physical sibling.'
}
$runsRoot = Join-Path $runRoot 'runs'
$cycleNames = @(1..10 | ForEach-Object { '{0:D2}' -f $_ })
Assert-ExactChildren $runsRoot $cycleNames 'Cycle root'

$cycles = @($result.cycles)
if ($cycles.Count -ne 10) { throw 'Render-path result must contain exactly ten cycles.' }
$metricsFields = @('width', 'height', 'stride', 'pixel_count', 'occupied_rgb555_bins',
    'luma_p05', 'luma_p95', 'luma_spread', 'modal_pixels', 'modal_per_mille',
    'nonmodal_grid_cells')
$auditFields = @('config_count', 'rt_records', 'bind_records', 'ownership_modes', 'resolve_records',
    'resolve_calls', 'shader_records', 'pso_records', 'pso_ok', 'draw_issued', 'depth_test',
    'depth_write', 'msaa1', 'msaa2', 'msaa4', 'gamma_nonidentity', 'gamma_uploads',
    'summary_count')
$cycleFields = @('index', 'capture_elapsed_milliseconds', 'dwell_elapsed_milliseconds',
    'exit_elapsed_milliseconds', 'exit_code', 'startup_marker_count',
    'present_count_1_sequence', 'present_count_3_sequence', 'present_count_1_hresult',
    'present_count_3_hresult', 'capture_sequence', 'capture_last_presented_sequence',
    'capture_success_marker_count', 'window_close_marker_occurrences',
    'execution_complete_marker_occurrences', 'hard_exit_marker_occurrences',
    'post_hard_exit_execution_complete_occurrences',
    'close_requested', 'harness_force_cleanup', 'process_signal_confirmed',
    'process_cleanup_confirmed', 'prior_cycles_immutable', 'runtime_logs',
    'runtime_log_file_count', 'runtime_log_bytes', 'runtime_log_set_sha256',
    'capture_relative_path', 'capture_sha256', 'capture_bytes', 'capture_metrics',
    'logo_edge_correlation_ppm', 'press_edge_correlation_ppm', 'title_route_verified',
    'audit', 'user_tree_sha256', 'cache_tree_sha256', 'user_file_count', 'cache_file_count',
    'user_bytes', 'cache_bytes', 'cycle_tree_sha256')

for ($index = 0; $index -lt 10; $index++) {
    $number = $index + 1
    $name = '{0:D2}' -f $number
    $cycle = $cycles[$index]
    Assert-ExactProperties $cycle $cycleFields "Cycle $name"
    Assert-JsonTypes $cycle `
        -BooleanNames @('close_requested', 'harness_force_cleanup', 'process_signal_confirmed',
            'process_cleanup_confirmed', 'prior_cycles_immutable', 'title_route_verified') `
        -IntegerNames @('index', 'capture_elapsed_milliseconds', 'dwell_elapsed_milliseconds',
            'exit_elapsed_milliseconds', 'exit_code', 'startup_marker_count',
            'present_count_1_sequence', 'present_count_3_sequence', 'capture_sequence',
            'capture_last_presented_sequence', 'capture_success_marker_count',
            'window_close_marker_occurrences', 'execution_complete_marker_occurrences',
            'hard_exit_marker_occurrences',
            'post_hard_exit_execution_complete_occurrences', 'capture_bytes',
            'runtime_log_file_count', 'runtime_log_bytes',
            'logo_edge_correlation_ppm', 'press_edge_correlation_ppm', 'user_file_count',
            'cache_file_count', 'user_bytes', 'cache_bytes') `
        -StringNames @('present_count_1_hresult', 'present_count_3_hresult',
            'runtime_log_set_sha256', 'capture_relative_path', 'capture_sha256',
            'user_tree_sha256', 'cache_tree_sha256', 'cycle_tree_sha256') `
        -Description "Cycle $name"
    if ($cycle.index -ne $number -or $cycle.capture_elapsed_milliseconds -lt 0 -or
        $cycle.capture_elapsed_milliseconds -gt 60000 -or
        $cycle.dwell_elapsed_milliseconds -lt 2000 -or
        $cycle.dwell_elapsed_milliseconds -gt 10000 -or
        $cycle.exit_elapsed_milliseconds -lt 0 -or $cycle.exit_elapsed_milliseconds -gt 10000 -or
        $cycle.exit_code -ne 0 -or $cycle.startup_marker_count -ne 15 -or
        $cycle.present_count_1_hresult -notmatch '^0x[0-9A-F]{8}$' -or
        $cycle.present_count_3_hresult -notmatch '^0x[0-9A-F]{8}$' -or
        $cycle.window_close_marker_occurrences -ne 1 -or
        $cycle.execution_complete_marker_occurrences -ne 1 -or
        $cycle.hard_exit_marker_occurrences -ne 1 -or
        $cycle.post_hard_exit_execution_complete_occurrences -ne 0 -or
        $cycle.close_requested -ne $true -or
        $cycle.harness_force_cleanup -ne $false -or
        $cycle.process_signal_confirmed -ne $true -or $cycle.process_cleanup_confirmed -ne $true -or
        $cycle.prior_cycles_immutable -ne $true -or $cycle.title_route_verified -ne $true -or
        $cycle.logo_edge_correlation_ppm -lt 900000 -or
        $cycle.press_edge_correlation_ppm -lt 900000) {
        throw "Render-path cycle $name is incomplete or outside canonical bounds."
    }
    if ($cycle.capture_relative_path -cne "runs/$name/user/mcla-first-frame.bmp") {
        throw "Render-path cycle $name has an unsafe capture path."
    }
    foreach ($value in @($cycle.runtime_log_set_sha256, $cycle.capture_sha256,
            $cycle.user_tree_sha256, $cycle.cache_tree_sha256, $cycle.cycle_tree_sha256)) {
        if ([string]$value -notmatch $hashPattern) { throw "Cycle $name contains an invalid hash." }
    }
    $declaredLogs = @($cycle.runtime_logs)
    if ($declaredLogs.Count -ne $cycle.runtime_log_file_count -or
        $declaredLogs.Count -lt 1 -or $declaredLogs.Count -gt 16) {
        throw "Cycle $name runtime-log manifest has invalid cardinality."
    }
    foreach ($declaredLog in $declaredLogs) {
        Assert-ExactProperties $declaredLog @('name', 'bytes', 'sha256') `
            "Cycle $name runtime log"
        Assert-JsonTypes $declaredLog -IntegerNames @('bytes') -StringNames @('name', 'sha256') `
            -Description "Cycle $name runtime log"
        if ([string]$declaredLog.sha256 -notmatch $hashPattern -or $declaredLog.bytes -lt 0 -or
            $declaredLog.bytes -gt 8388608) {
            throw "Cycle $name runtime-log manifest entry is invalid."
        }
    }
    Assert-ExactProperties $cycle.capture_metrics $metricsFields "Cycle $name metrics"
    Assert-JsonTypes $cycle.capture_metrics -IntegerNames $metricsFields `
        -Description "Cycle $name metrics"
    Assert-ExactProperties $cycle.audit $auditFields "Cycle $name audit"
    Assert-JsonTypes $cycle.audit -IntegerNames $auditFields -Description "Cycle $name audit"

    $cycleRoot = Join-Path $runsRoot $name
    Assert-ExactChildren $cycleRoot (@('cache', 'user') + @($declaredLogs.name)) "Cycle $name"
    $logPath = Join-Path $cycleRoot 'mcla.log'
    $userRoot = Join-Path $cycleRoot 'user'
    $cacheRoot = Join-Path $cycleRoot 'cache'
    $framePath = Join-Path $userRoot 'mcla-first-frame.bmp'
    $bmps = @(Get-ChildItem -LiteralPath $cycleRoot -Recurse -File -Force -Filter '*.bmp')
    if ($bmps.Count -ne 1 -or -not [string]::Equals($bmps[0].FullName, $framePath,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Cycle $name must contain exactly its expected BMP."
    }
    $probe = Get-ProbeEvidence -LogPath $logPath -FramePath $framePath `
        -ReferencePath $defaultReferencePath
    $userTree = Get-TreeSnapshot $userRoot
    $cacheTree = Get-TreeSnapshot $cacheRoot
    $cycleTree = Get-TreeSnapshot $cycleRoot
    $expectedMetrics = @($probe.Bmp.Width, $probe.Bmp.Height, $probe.Bmp.Stride,
        $probe.Bmp.PixelCount, $probe.Bmp.OccupiedRgb555Bins, $probe.Bmp.LumaP05,
        $probe.Bmp.LumaP95, $probe.Bmp.LumaSpread, $probe.Bmp.ModalPixels,
        $probe.Bmp.ModalPermille, $probe.Bmp.NonmodalGridCells)
    for ($metricIndex = 0; $metricIndex -lt $metricsFields.Count; $metricIndex++) {
        if ($cycle.capture_metrics.($metricsFields[$metricIndex]) -ne $expectedMetrics[$metricIndex]) {
            throw "Cycle $name capture metrics do not match its physical BMP."
        }
    }
    $expectedAudit = @($probe.Audit.ConfigCount, $probe.Audit.RtRecords,
        $probe.Audit.BindRecords, $probe.Audit.OwnershipModes,
        $probe.Audit.ResolveRecords, $probe.Audit.ResolveCalls,
        $probe.Audit.ShaderRecords, $probe.Audit.PsoRecords, $probe.Audit.PsoOk,
        $probe.Audit.DrawIssued, $probe.Audit.DepthTest, $probe.Audit.DepthWrite,
        $probe.Audit.Msaa1, $probe.Audit.Msaa2, $probe.Audit.Msaa4,
        $probe.Audit.GammaNonidentity, $probe.Audit.GammaUploads, $probe.Audit.SummaryCount)
    for ($auditIndex = 0; $auditIndex -lt $auditFields.Count; $auditIndex++) {
        if ($cycle.audit.($auditFields[$auditIndex]) -ne $expectedAudit[$auditIndex]) {
            throw "Cycle $name audit aggregate does not match physical markers."
        }
    }
    if ($cycle.runtime_log_file_count -ne $probe.Log.RuntimeLogFileCount -or
        $cycle.runtime_log_bytes -ne $probe.Log.RuntimeLogBytes -or
        $cycle.runtime_log_set_sha256 -ne $probe.Log.RuntimeLogSetSha256 -or
        $cycle.capture_sha256 -ne $probe.Bmp.Sha256 -or
        $cycle.capture_bytes -ne $probe.Bmp.Bytes -or
        $cycle.startup_marker_count -ne $probe.Log.StartupMarkerCount -or
        $cycle.present_count_1_sequence -ne $probe.Log.PresentOneSequence -or
        $cycle.present_count_3_sequence -ne $probe.Log.PresentThreeSequence -or
        $cycle.present_count_1_hresult -ne $probe.Log.PresentOneHresult -or
        $cycle.present_count_3_hresult -ne $probe.Log.PresentThreeHresult -or
        $cycle.capture_sequence -ne $probe.Log.CaptureSequence -or
        $cycle.capture_last_presented_sequence -ne $probe.Log.CaptureLastPresentedSequence -or
        $cycle.capture_success_marker_count -ne $probe.Log.CaptureSuccessMarkerCount -or
        $cycle.window_close_marker_occurrences -ne $probe.Log.WindowCloseMarkers -or
        $cycle.execution_complete_marker_occurrences -ne
            $probe.Log.ExecutionCompleteMarkers -or
        $cycle.hard_exit_marker_occurrences -ne $probe.Log.HardExitMarkers -or
        $cycle.post_hard_exit_execution_complete_occurrences -ne
            $probe.Log.PostHardExitExecutionCompleteMarkers -or
        $cycle.logo_edge_correlation_ppm -ne $probe.Roi.LogoCorrelationPpm -or
        $cycle.press_edge_correlation_ppm -ne $probe.Roi.PressCorrelationPpm -or
        $cycle.user_tree_sha256 -ne $userTree.Hash -or
        $cycle.cache_tree_sha256 -ne $cacheTree.Hash -or
        $cycle.cycle_tree_sha256 -ne $cycleTree.Hash -or
        $cycle.user_file_count -ne $userTree.FileCount -or
        $cycle.cache_file_count -ne $cacheTree.FileCount -or
        $cycle.user_bytes -ne $userTree.Bytes -or $cycle.cache_bytes -ne $cacheTree.Bytes) {
        throw "Cycle $name aggregate does not match physical evidence."
    }
    for ($logIndex = 0; $logIndex -lt $declaredLogs.Count; $logIndex++) {
        foreach ($field in @('name', 'bytes', 'sha256')) {
            if ($declaredLogs[$logIndex].$field -ne $probe.Log.RuntimeLogs[$logIndex].$field) {
                throw "Cycle $name runtime-log manifest does not match physical files."
            }
        }
    }
}

$physicalGame = Get-CanonicalGameIdentity
foreach ($field in @('file_count', 'payload_bytes', 'hashes_verified', 'manifest_sha256',
        'tree_sha256', 'tree_file_count', 'tree_directory_count', 'tree_bytes')) {
    if ($result.game_identity.after.$field -ne $physicalGame.$field) {
        throw "Canonical game tree no longer matches aggregate field '$field'."
    }
}
$physicalArtifacts = @()
if (-not $HistoricalEvidenceOnly) {
    $physicalArtifacts = @(Get-CanonicalArtifactSnapshot)
    for ($index = 0; $index -lt $physicalArtifacts.Count; $index++) {
        if ($result.artifacts.after[$index].name -cne $physicalArtifacts[$index].name -or
            $result.artifacts.after[$index].sha256 -ne $physicalArtifacts[$index].sha256) {
            throw "Canonical runtime artifact '$($physicalArtifacts[$index].name)' changed after the gate."
        }
    }
}
if (@(Get-ExactCanonicalProcesses).Count -ne 0) {
    throw 'The exact canonical MCLA executable still has a surviving process.'
}

foreach ($name in @('all_write_roots_contained', 'all_prior_cycles_immutable',
        'no_surviving_processes', 'data_integrity_preserved', 'all_captures_bound',
        'all_title_rois_match', 'all_render_audits_passed')) {
    if ($result.$name -ne $true) { throw "Render-path aggregate flag '$name' is not true." }
}

[pscustomobject]@{
    Passed = $true
    Cycles = 10
    PhysicalCapturesVerified = 10
    LogoCorrelationThresholdPpm = 900000
    PressCorrelationThresholdPpm = 900000
    RenderAuditsVerified = 10
    ProcessCleanupVerified = $true
    DataIntegrityVerified = $true
}
