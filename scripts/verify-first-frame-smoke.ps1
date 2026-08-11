[CmdletBinding(DefaultParameterSetName = 'Result')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Result')]
    [string]$ResultPath,

    [Parameter(Mandatory, ParameterSetName = 'Probe')]
    [string]$RuntimeLogPath,

    [Parameter(Mandatory, ParameterSetName = 'Probe')]
    [string]$BmpPath,

    [Parameter(Mandatory, ParameterSetName = 'Probe')]
    [switch]$ProbeOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$startupVerifier = Join-Path $PSScriptRoot 'verify-startup-smoke.ps1'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$hashPattern = '^[0-9A-F]{64}$'

# Keep the runtime-facing grammar in one block. The SDK/project handoff may
# change wording, but must preserve the named fields and ordering contract.
$firstFrameMarkers = [ordered]@{
    Refresh = '(?m)^.*D3D12 IssueSwap: first active guest output refresh succeeded source=(?<width>[0-9]+)x(?<height>[0-9]+)\s*$'
    Present = '(?m)^.*D3D12 guest present: successful sequence count=(?<count>1|3) sequence=(?<sequence>[0-9]+) source=(?<source_width>[0-9]+)x(?<source_height>[0-9]+) swapchain=(?<swap_width>[0-9]+)x(?<swap_height>[0-9]+) HRESULT=0x(?<hresult>[0-9A-Fa-f]{8})\s*$'
    Capture = '(?m)^.*D3D12 guest capture: success sequence=(?<sequence>[0-9]+) last_presented_sequence=(?<last_presented_sequence>[0-9]+) dimensions=(?<width>[0-9]+)x(?<height>[0-9]+)\s*$'
    Project = '(?m)^.*MCLA graphics: nontrivial guest frame captured (?<width>[0-9]+)x(?<height>[0-9]+), rgb555 bins (?<bins>[0-9]+), luma p05 (?<p05>[0-9]+), luma p95 (?<p95>[0-9]+), modal permille (?<modal>[0-9]+), nonmodal grid cells (?<cells>[0-9]+)\s*$'
    WindowClose = 'Window closing, shutting down...'
    HardExit = 'Title terminated; hard-exiting process.'
}

$bannedFirstFramePatterns = @(
    '(?i)IssueSwap: RefreshGuestOutput failed for active',
    '(?i)D3D12 guest present: failed sequence=',
    '(?i)D3D12 guest present: device (?:removed|reset)',
    '(?i)Presenter: Active guest output sequence exhausted',
    '(?i)guest present.*UI[- ]only',
    '(?i)DXGI_ERROR_DEVICE_(?:REMOVED|RESET|HUNG)',
    '(?i)D3D12 device removed',
    '(?i)GPU (?:device )?lost',
    '(?i)D3D12 guest capture: failed',
    '(?i)MCLA graphics: guest frame readback has invalid',
    '(?i)MCLA graphics: failed to write private first-frame capture',
    '(?i)IssueSwap: (?:presenter is null|BeginSubmission failed|RequestSwapTexture failed)'
)

function Assert-ExactProperties {
    param(
        [Parameter(Mandatory)][object]$Value,
        [Parameter(Mandatory)][string[]]$Expected,
        [Parameter(Mandatory)][string]$Description
    )
    $actual = @($Value.PSObject.Properties.Name | Sort-Object)
    $wanted = @($Expected | Sort-Object)
    if ($actual.Count -ne $wanted.Count) {
        throw "$Description has missing or unknown properties."
    }
    for ($index = 0; $index -lt $wanted.Count; $index++) {
        if ($actual[$index] -cne $wanted[$index]) {
            throw "$Description has missing or unknown properties."
        }
    }
}

function Assert-JsonTypes {
    param(
        [Parameter(Mandatory)][object]$Value,
        [string[]]$BooleanNames = @(),
        [string[]]$IntegerNames = @(),
        [string[]]$StringNames = @(),
        [Parameter(Mandatory)][string]$Description
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
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Description)
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $fullPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay inside the repository: '$fullPath'."
    }
    $repoItem = Get-Item -LiteralPath $repoRoot -Force
    if ($repoItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Repository root must not be a reparse point: '$repoRoot'."
    }
    $relative = $fullPath.Substring($prefix.Length)
    $current = $repoRoot
    foreach ($component in @($relative.Split('\') | Where-Object { $_.Length -ne 0 })) {
        $current = Join-Path $current $component
        if (-not (Test-Path -LiteralPath $current)) {
            throw "$Description component is missing: '$current'."
        }
        $item = Get-Item -LiteralPath $current -Force
        if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "$Description traverses a reparse point: '$current'."
        }
    }
    return $fullPath
}

function Assert-ExactChildren {
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string[]]$ExpectedNames,
        [Parameter(Mandatory)][string]$Description
    )
    $actual = @((Get-ChildItem -LiteralPath $Root -Force | Sort-Object Name).Name)
    $expected = @($ExpectedNames | Sort-Object)
    if ($actual.Count -ne $expected.Count) {
        throw "$Description has missing or extra artifacts."
    }
    for ($index = 0; $index -lt $expected.Count; $index++) {
        if ($actual[$index] -cne $expected[$index]) {
            throw "$Description has missing, extra, or incorrectly named artifacts."
        }
    }
}

function Get-TreeSnapshot {
    param([Parameter(Mandatory)][string]$Root)
    $rootItem = Get-Item -LiteralPath $Root -Force
    if ($rootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Evidence root is a reparse point: '$Root'."
    }
    $allItems = @(Get-ChildItem -LiteralPath $Root -Recurse -Force)
    foreach ($item in $allItems) {
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
    } finally {
        $sha.Dispose()
    }
    $totalBytes = [long]0
    foreach ($file in $files) { $totalBytes += [long]$file.Length }
    [pscustomobject]@{
        Hash = $treeHash
        FileCount = $files.Count
        Bytes = $totalBytes
    }
}

function Get-UInt16LE {
    param([byte[]]$Bytes, [int]$Offset)
    return [uint16]([uint16]$Bytes[$Offset] -bor ([uint16]$Bytes[$Offset + 1] -shl 8))
}

function Get-UInt32LE {
    param([byte[]]$Bytes, [int]$Offset)
    return [uint32]([uint32]$Bytes[$Offset] -bor
        ([uint32]$Bytes[$Offset + 1] -shl 8) -bor
        ([uint32]$Bytes[$Offset + 2] -shl 16) -bor
        ([uint32]$Bytes[$Offset + 3] -shl 24))
}

function Get-Int32LE {
    param([byte[]]$Bytes, [int]$Offset)
    return [System.BitConverter]::ToInt32($Bytes, $Offset)
}

function Get-BmpEvidence {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "First-frame BMP was not found: '$Path'."
    }
    $item = Get-Item -LiteralPath $Path
    if ($item.Length -lt 54 -or $item.Length -gt 268435510) {
        throw 'First-frame BMP size is outside the reviewed bound.'
    }
    [byte[]]$bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes[0] -ne 0x42 -or $bytes[1] -ne 0x4D) {
        throw 'First-frame artifact is not a BMP file.'
    }
    $declaredSize = Get-UInt32LE -Bytes $bytes -Offset 2
    $reservedOne = Get-UInt16LE -Bytes $bytes -Offset 6
    $reservedTwo = Get-UInt16LE -Bytes $bytes -Offset 8
    $pixelOffset = Get-UInt32LE -Bytes $bytes -Offset 10
    $dibSize = Get-UInt32LE -Bytes $bytes -Offset 14
    $width = Get-Int32LE -Bytes $bytes -Offset 18
    $signedHeight = Get-Int32LE -Bytes $bytes -Offset 22
    $planes = Get-UInt16LE -Bytes $bytes -Offset 26
    $bitsPerPixel = Get-UInt16LE -Bytes $bytes -Offset 28
    $compression = Get-UInt32LE -Bytes $bytes -Offset 30
    $declaredImageSize = Get-UInt32LE -Bytes $bytes -Offset 34
    $colorsUsed = Get-UInt32LE -Bytes $bytes -Offset 46
    $colorsImportant = Get-UInt32LE -Bytes $bytes -Offset 50
    if ($declaredSize -ne $bytes.Length -or $reservedOne -ne 0 -or $reservedTwo -ne 0 -or
        $pixelOffset -ne 54 -or $dibSize -ne 40 -or $planes -ne 1 -or
        $bitsPerPixel -ne 32 -or $compression -ne 0 -or $colorsUsed -ne 0 -or
        $colorsImportant -ne 0 -or $width -lt 64 -or $width -gt 8192 -or
        $signedHeight -eq 0 -or [Math]::Abs([long]$signedHeight) -lt 64 -or
        [Math]::Abs([long]$signedHeight) -gt 8192) {
        throw 'First-frame BMP header is not exact 32-bpp BI_RGB within safe dimensions.'
    }
    $height = [int][Math]::Abs([long]$signedHeight)
    $stride = [long]$width * 4
    $pixelCount = [long]$width * $height
    $payloadBytes = $stride * $height
    if ($pixelCount -gt 67108864 -or $payloadBytes -gt 268435456 -or
        [long]$pixelOffset + $payloadBytes -ne $bytes.Length -or
        ($declaredImageSize -ne 0 -and $declaredImageSize -ne $payloadBytes)) {
        throw 'First-frame BMP payload length is inconsistent or excessive.'
    }

    $quantized = [int[]]::new([int]$pixelCount)
    $binCounts = [int[]]::new(32768)
    $lumaCounts = [int[]]::new(256)
    $pixelIndex = 0
    for ($row = 0; $row -lt $height; $row++) {
        $sourceRow = if ($signedHeight -gt 0) { $height - 1 - $row } else { $row }
        $rowOffset = [int]($pixelOffset + ([long]$sourceRow * $stride))
        for ($x = 0; $x -lt $width; $x++) {
            $offset = $rowOffset + ($x * 4)
            $blue = [int]$bytes[$offset]
            $green = [int]$bytes[$offset + 1]
            $red = [int]$bytes[$offset + 2]
            $bin = (($red -shr 3) -shl 10) -bor (($green -shr 3) -shl 5) -bor ($blue -shr 3)
            $quantized[$pixelIndex] = $bin
            $binCounts[$bin]++
            $luma = (54 * $red + 183 * $green + 19 * $blue + 128) -shr 8
            $lumaCounts[$luma]++
            $pixelIndex++
        }
    }

    $occupiedBins = 0
    $modalBin = 0
    $modalPixels = 0
    for ($index = 0; $index -lt $binCounts.Length; $index++) {
        if ($binCounts[$index] -gt 0) {
            $occupiedBins++
            if ($binCounts[$index] -gt $modalPixels) {
                $modalPixels = $binCounts[$index]
                $modalBin = $index
            }
        }
    }
    $p05Target = [long][Math]::Floor((([long]$pixelCount * 5 + 99) / 100))
    $p95Target = [long][Math]::Floor((([long]$pixelCount * 95 + 99) / 100))
    $cumulative = [long]0
    $p05 = -1
    $p95 = -1
    for ($value = 0; $value -lt 256; $value++) {
        $cumulative += $lumaCounts[$value]
        if ($p05 -lt 0 -and $cumulative -ge $p05Target) { $p05 = $value }
        if ($p95 -lt 0 -and $cumulative -ge $p95Target) {
            $p95 = $value
            break
        }
    }

    $nonmodalCells = [bool[]]::new(16 * 9)
    for ($y = 0; $y -lt $height; $y++) {
        $gridY = [Math]::Min(8, [int](([long]$y * 9) / $height))
        for ($x = 0; $x -lt $width; $x++) {
            $index = $y * $width + $x
            if ($quantized[$index] -ne $modalBin) {
                $gridX = [Math]::Min(15, [int](([long]$x * 16) / $width))
                $nonmodalCells[$gridY * 16 + $gridX] = $true
            }
        }
    }
    $nonmodalCellCount = @($nonmodalCells | Where-Object { $_ }).Count
    $modalPermille = [int](([long]$modalPixels * 1000) / $pixelCount)
    $lumaSpread = $p95 - $p05
    if ($occupiedBins -lt 16 -or $lumaSpread -lt 8 -or $modalPermille -gt 995 -or
        $nonmodalCellCount -lt 4) {
        throw 'First-frame BMP is uniform or below the reviewed nontrivial-image thresholds.'
    }

    [pscustomobject]@{
        Width = $width
        Height = $height
        Stride = [int]$stride
        PixelCount = $pixelCount
        OccupiedRgb555Bins = $occupiedBins
        LumaP05 = $p05
        LumaP95 = $p95
        LumaSpread = $lumaSpread
        ModalPixels = $modalPixels
        ModalPermille = $modalPermille
        NonmodalGridCells = $nonmodalCellCount
        Bytes = $item.Length
        Sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    }
}

function Get-SingleRegexMatch {
    param([string]$Text, [string]$Pattern, [string]$Description)
    $matches = [regex]::Matches($Text, $Pattern)
    if ($matches.Count -ne 1) {
        throw "First-frame log requires exactly one $Description marker."
    }
    return $matches[0]
}

function Get-FirstFrameLogEvidence {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "First-frame runtime log was not found: '$Path'."
    }
    $item = Get-Item -LiteralPath $Path
    if ($item.Length -gt 1048576) {
        throw 'First-frame runtime log exceeded the reviewed 1-MiB bound.'
    }
    $log = Get-Content -LiteralPath $Path -Raw
    foreach ($pattern in $bannedFirstFramePatterns) {
        if ($log -match $pattern) {
            throw "First-frame log contains banned failure pattern '$pattern'."
        }
    }

    $refresh = Get-SingleRegexMatch -Text $log -Pattern $firstFrameMarkers.Refresh `
        -Description 'active guest-output refresh'
    $presentMatches = [regex]::Matches($log, $firstFrameMarkers.Present)
    if ($presentMatches.Count -ne 2) {
        throw 'First-frame log requires exactly the distinct count-1 and count-3 guest presents.'
    }
    $presentOne = @($presentMatches | Where-Object { $_.Groups['count'].Value -eq '1' })
    $presentThree = @($presentMatches | Where-Object { $_.Groups['count'].Value -eq '3' })
    if ($presentOne.Count -ne 1 -or $presentThree.Count -ne 1) {
        throw 'First-frame guest-present markers are duplicated or incomplete.'
    }
    $presentOne = $presentOne[0]
    $presentThree = $presentThree[0]
    $project = Get-SingleRegexMatch -Text $log -Pattern $firstFrameMarkers.Project `
        -Description 'project nontrivial-frame'
    $captureMatches = @([regex]::Matches($log, $firstFrameMarkers.Capture) |
        Where-Object { $_.Index -lt $project.Index })
    if ($captureMatches.Count -lt 2 -or $captureMatches.Count -gt 256) {
        throw 'First-frame log requires 2-256 pre-project CaptureGuestOutput successes.'
    }
    $previousCaptureSequence = [long]0
    $previousLastPresentedSequence = [long]0
    foreach ($candidate in $captureMatches) {
        $candidateSequence = [long]$candidate.Groups['sequence'].Value
        $candidateLastPresentedSequence = [long]$candidate.Groups['last_presented_sequence'].Value
        if ($candidateSequence -lt 1 -or $candidateSequence -lt $previousCaptureSequence) {
            throw 'First-frame capture sequences are zero or nonmonotonic.'
        }
        if ($candidateLastPresentedSequence -lt $candidateSequence -or
            $candidateLastPresentedSequence -lt $previousLastPresentedSequence) {
            throw 'First-frame capture sequence was not bound to a successful guest presentation.'
        }
        $previousCaptureSequence = $candidateSequence
        $previousLastPresentedSequence = $candidateLastPresentedSequence
    }
    $capture = $captureMatches[$captureMatches.Count - 1]

    $windowMatches = [regex]::Matches($log, [regex]::Escape($firstFrameMarkers.WindowClose))
    $exitMatches = [regex]::Matches($log, [regex]::Escape($firstFrameMarkers.HardExit))
    if ($windowMatches.Count -ne 1 -or $exitMatches.Count -ne 1) {
        throw 'First-frame log lacks the exact WM_CLOSE/hard-exit tail.'
    }
    $windowOffset = $windowMatches[0].Index
    $exitOffset = $exitMatches[0].Index
    $afterExit = $log.Substring($exitOffset + $firstFrameMarkers.HardExit.Length).Trim()
    $postHardExitExecutionCompleteMarkers = 0
    if ($afterExit.Length -ne 0) {
        if ($afterExit -notmatch '^\[[^\r\n]+\] \[info\] \[core\] \[t[0-9]+\] Execution complete$') {
            throw 'First-frame log contains an unexpected line after the hard-exit marker.'
        }
        $postHardExitExecutionCompleteMarkers = 1
    }
    # RefreshGuestOutput may present synchronously before IssueSwap logs its
    # successful return, or may defer presentation to the UI thread. Therefore
    # refresh and count-1 may occur in either order, but both must precede count-3.
    if ($refresh.Index -ge $presentThree.Index -or $presentOne.Index -ge $presentThree.Index -or
        $presentThree.Index -ge $capture.Index -or $capture.Index -ge $project.Index -or
        $project.Index -ge $windowOffset -or $windowOffset -ge $exitOffset) {
        throw 'First-frame markers are out of order.'
    }

    $presentOneSequence = [long]$presentOne.Groups['sequence'].Value
    $presentThreeSequence = [long]$presentThree.Groups['sequence'].Value
    $presentOneHresult = [Convert]::ToUInt32($presentOne.Groups['hresult'].Value, 16)
    $presentThreeHresult = [Convert]::ToUInt32($presentThree.Groups['hresult'].Value, 16)
    $captureSequence = [long]$capture.Groups['sequence'].Value
    $captureLastPresentedSequence = [long]$capture.Groups['last_presented_sequence'].Value
    if ($presentOneSequence -lt 1 -or $presentThreeSequence -le $presentOneSequence -or
        $captureSequence -lt $presentThreeSequence -or
        $captureLastPresentedSequence -lt $captureSequence -or
        ($presentOneHresult -band 0x80000000) -ne 0 -or
        ($presentThreeHresult -band 0x80000000) -ne 0) {
        throw 'First-frame guest-output sequences are invalid, nonmonotonic, or unbound.'
    }

    $refreshWidth = [int]$refresh.Groups['width'].Value
    $refreshHeight = [int]$refresh.Groups['height'].Value
    $sourceWidthOne = [int]$presentOne.Groups['source_width'].Value
    $sourceHeightOne = [int]$presentOne.Groups['source_height'].Value
    $sourceWidthThree = [int]$presentThree.Groups['source_width'].Value
    $sourceHeightThree = [int]$presentThree.Groups['source_height'].Value
    $swapWidthOne = [int]$presentOne.Groups['swap_width'].Value
    $swapHeightOne = [int]$presentOne.Groups['swap_height'].Value
    $swapWidthThree = [int]$presentThree.Groups['swap_width'].Value
    $swapHeightThree = [int]$presentThree.Groups['swap_height'].Value
    $captureWidth = [int]$capture.Groups['width'].Value
    $captureHeight = [int]$capture.Groups['height'].Value
    $projectWidth = [int]$project.Groups['width'].Value
    $projectHeight = [int]$project.Groups['height'].Value
    $invalidDimensions = @(
        $refreshWidth, $refreshHeight, $sourceWidthOne, $sourceHeightOne,
        $sourceWidthThree, $sourceHeightThree, $swapWidthOne, $swapHeightOne,
        $swapWidthThree, $swapHeightThree, $captureWidth, $captureHeight
    ) |
        Where-Object { $_ -lt 64 -or $_ -gt 8192 }
    if ($captureWidth -ne $sourceWidthThree -or $captureHeight -ne $sourceHeightThree -or
        $projectWidth -ne $captureWidth -or $projectHeight -ne $captureHeight -or
        @($invalidDimensions).Count -ne 0) {
        throw 'First-frame source, swap-chain, capture, or project dimensions are inconsistent.'
    }

    $startup = & $startupVerifier -RuntimeLogPath $Path
    [pscustomobject]@{
        StartupMarkerCount = $startup.MarkerCount
        FirstActiveRefreshCount = 1
        PresentOneSequence = $presentOneSequence
        PresentThreeSequence = $presentThreeSequence
        PresentOneHresult = ('0x{0:X8}' -f $presentOneHresult)
        PresentThreeHresult = ('0x{0:X8}' -f $presentThreeHresult)
        MinimumSuccessfulPresentCount = 3
        SourceWidth = $sourceWidthThree
        SourceHeight = $sourceHeightThree
        SwapchainWidth = $swapWidthThree
        SwapchainHeight = $swapHeightThree
        CaptureSequence = $captureSequence
        CaptureLastPresentedSequence = $captureLastPresentedSequence
        CaptureSuccessMarkerCount = $captureMatches.Count
        CaptureWidth = $captureWidth
        CaptureHeight = $captureHeight
        ProjectOccupiedRgb555Bins = [int]$project.Groups['bins'].Value
        ProjectLumaP05 = [int]$project.Groups['p05'].Value
        ProjectLumaP95 = [int]$project.Groups['p95'].Value
        ProjectModalPermille = [int]$project.Groups['modal'].Value
        ProjectNonmodalGridCells = [int]$project.Groups['cells'].Value
        PresentResultClass = 'SUCCEEDED'
        WindowCloseMarkers = $windowMatches.Count
        HardExitMarkers = $exitMatches.Count
        PostHardExitExecutionCompleteMarkers = $postHardExitExecutionCompleteMarkers
        Bytes = $item.Length
        Sha256 = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    }
}

function Get-ProbeEvidence {
    param([string]$LogPath, [string]$FramePath)
    $logEvidence = Get-FirstFrameLogEvidence -Path $LogPath
    $bmpEvidence = Get-BmpEvidence -Path $FramePath
    if ($bmpEvidence.Width -ne $logEvidence.CaptureWidth -or
        $bmpEvidence.Height -ne $logEvidence.CaptureHeight -or
        $bmpEvidence.OccupiedRgb555Bins -ne $logEvidence.ProjectOccupiedRgb555Bins -or
        $bmpEvidence.LumaP05 -ne $logEvidence.ProjectLumaP05 -or
        $bmpEvidence.LumaP95 -ne $logEvidence.ProjectLumaP95 -or
        $bmpEvidence.ModalPermille -ne $logEvidence.ProjectModalPermille -or
        $bmpEvidence.NonmodalGridCells -ne $logEvidence.ProjectNonmodalGridCells) {
        throw 'Physical BMP does not match the bound capture/project marker.'
    }
    [pscustomobject]@{ Log = $logEvidence; Bmp = $bmpEvidence }
}

if ($PSCmdlet.ParameterSetName -eq 'Probe') {
    if (-not $ProbeOnly) { throw 'Probe inputs require -ProbeOnly.' }
    Get-ProbeEvidence -LogPath $RuntimeLogPath -FramePath $BmpPath
    return
}

if (-not (Test-Path -LiteralPath $ResultPath -PathType Leaf)) {
    throw "First-frame aggregate result was not found: '$ResultPath'."
}
$resultItem = Get-Item -LiteralPath $ResultPath
if ($resultItem.Length -gt 1048576) {
    throw 'First-frame aggregate exceeded the reviewed 1-MiB bound.'
}
$json = Get-Content -LiteralPath $ResultPath -Raw
foreach ($pattern in @(
        '(?i)[A-Z]:[\\/]',
        '(?i)\\\\[^"\s]+[\\/]',
        '(?i)(?:^|["\\/])private[\\/]'
    )) {
    if ($json -match $pattern) {
        throw "First-frame aggregate contains prohibited path pattern '$pattern'."
    }
}
$result = $json | ConvertFrom-Json
Assert-ExactProperties -Value $result -Description 'First-frame result' -Expected @(
    'schema', 'task', 'cycle_count', 'execution_order', 'development_only',
    'first_frame_timeout_seconds', 'post_marker_dwell_milliseconds',
    'exit_timeout_seconds', 'failure_cleanup_timeout_seconds', 'clean_build',
    'first_cycle_post_clean_build', 'game_identity', 'artifacts', 'cycles',
    'all_write_roots_contained', 'all_prior_cycles_immutable',
    'no_surviving_processes', 'data_integrity_preserved', 'all_captures_nontrivial'
)
Assert-JsonTypes -Value $result -Description 'First-frame result' `
    -BooleanNames @(
        'development_only', 'first_cycle_post_clean_build', 'all_write_roots_contained',
        'all_prior_cycles_immutable', 'no_surviving_processes',
        'data_integrity_preserved', 'all_captures_nontrivial') `
    -IntegerNames @(
        'schema', 'cycle_count', 'first_frame_timeout_seconds',
        'post_marker_dwell_milliseconds', 'exit_timeout_seconds',
        'failure_cleanup_timeout_seconds') `
    -StringNames @('task', 'execution_order')
if ($result.schema -ne 1 -or $result.task -ne 'M4-001' -or $result.cycle_count -ne 20 -or
    $result.execution_order -ne 'clean_build_then_20_serial_first_frame_cycles' -or
    $result.development_only -ne $false) {
    throw 'First-frame result header does not prove the final 20-run gate.'
}
if ($result.first_frame_timeout_seconds -ne 60 -or
    $result.post_marker_dwell_milliseconds -ne 2000 -or
    $result.exit_timeout_seconds -ne 10 -or $result.failure_cleanup_timeout_seconds -ne 5) {
    throw 'First-frame result does not use the canonical bounded deadlines.'
}

$build = $result.clean_build
Assert-ExactProperties -Value $build -Description 'Clean build' -Expected @(
    'performed', 'success', 'exit_code', 'duration_milliseconds',
    'build_log_sha256', 'executable_sha256'
)
Assert-JsonTypes -Value $build -Description 'Clean build' `
    -BooleanNames @('performed', 'success') `
    -IntegerNames @('exit_code', 'duration_milliseconds') `
    -StringNames @('build_log_sha256', 'executable_sha256')
if ($build.performed -ne $true -or $build.success -ne $true -or $build.exit_code -ne 0 -or
    $build.duration_milliseconds -le 0 -or
    [string]$build.build_log_sha256 -notmatch $hashPattern -or
    [string]$build.executable_sha256 -notmatch $hashPattern -or
    $result.first_cycle_post_clean_build -ne $true) {
    throw 'First-frame result lacks its canonical clean-first build.'
}

Assert-ExactProperties -Value $result.game_identity -Description 'Game identity wrapper' `
    -Expected @('before', 'after')
foreach ($phase in @('before', 'after')) {
    $game = $result.game_identity.$phase
    Assert-ExactProperties -Value $game -Description "Game identity $phase" -Expected @(
        'file_count', 'payload_bytes', 'hashes_verified', 'manifest_sha256',
        'tree_sha256', 'tree_file_count', 'tree_directory_count', 'tree_bytes')
    Assert-JsonTypes -Value $game -Description "Game identity $phase" `
        -IntegerNames @('file_count', 'payload_bytes', 'hashes_verified', 'tree_file_count',
            'tree_directory_count', 'tree_bytes') `
        -StringNames @('manifest_sha256', 'tree_sha256')
    if ($game.file_count -ne 15 -or $game.payload_bytes -ne 6569586392 -or
        $game.hashes_verified -ne 15 -or [string]$game.manifest_sha256 -notmatch $hashPattern -or
        [string]$game.tree_sha256 -notmatch $hashPattern -or
        $game.tree_file_count -ne $game.file_count -or $game.tree_directory_count -lt 1 -or
        $game.tree_bytes -ne $game.payload_bytes) {
        throw "Supported game identity '$phase' is incomplete."
    }
}
foreach ($field in @('manifest_sha256', 'tree_sha256', 'tree_file_count',
        'tree_directory_count', 'tree_bytes')) {
    if ($result.game_identity.before.$field -ne $result.game_identity.after.$field) {
        throw "Supported game identity field '$field' changed across first-frame execution."
    }
}
Assert-ExactProperties -Value $result.artifacts -Description 'Artifact wrapper' `
    -Expected @('before', 'after')
$artifactNames = @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll')
foreach ($phase in @('before', 'after')) {
    $artifacts = @($result.artifacts.$phase)
    if ($artifacts.Count -ne 4) { throw "Artifact snapshot '$phase' is incomplete." }
    for ($index = 0; $index -lt 4; $index++) {
        Assert-ExactProperties -Value $artifacts[$index] `
            -Description "Artifact snapshot $phase/$index" -Expected @('name', 'sha256')
        Assert-JsonTypes -Value $artifacts[$index] -Description "Artifact snapshot $phase/$index" `
            -StringNames @('name', 'sha256')
        if ($artifacts[$index].name -ne $artifactNames[$index] -or
            [string]$artifacts[$index].sha256 -notmatch $hashPattern) {
            throw "Artifact snapshot '$phase' is invalid or out of order."
        }
    }
}
for ($index = 0; $index -lt 4; $index++) {
    if ($result.artifacts.before[$index].sha256 -ne $result.artifacts.after[$index].sha256) {
        throw "Artifact '$($artifactNames[$index])' changed during first-frame execution."
    }
}
if ($build.executable_sha256 -ne $result.artifacts.before[0].sha256) {
    throw 'Clean-build executable does not match the executed artifact snapshot.'
}

$cycles = @($result.cycles)
if ($cycles.Count -ne 20) { throw 'First-frame result must contain exactly twenty cycles.' }
$resolvedResult = Assert-ContainedNonReparsePath -Path $ResultPath -Description 'Result path'
if ((Split-Path -Leaf $resolvedResult) -cne 'result.json') {
    throw 'First-frame aggregate must use the exact result.json name.'
}
$runRoot = Split-Path -Parent $resolvedResult
[void](Assert-ContainedNonReparsePath -Path $runRoot -Description 'Run root')
$expectedEvidenceParent = [System.IO.Path]::GetFullPath(
    (Join-Path $repoRoot 'private/evidence/M4-001'))
$actualEvidenceParent = [System.IO.Path]::GetFullPath((Split-Path -Parent $runRoot))
if (-not [string]::Equals($actualEvidenceParent, $expectedEvidenceParent,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'First-frame run root must be an immediate child of private/evidence/M4-001.'
}
foreach ($child in @(Get-ChildItem -LiteralPath $runRoot -Recurse -Force)) {
    if ($child.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "First-frame evidence contains a reparse point: '$($child.FullName)'."
    }
}
Assert-ExactChildren -Root $runRoot -ExpectedNames @(
    'relwithdebinfo-clean-build.log', 'result.json', 'runs') -Description 'Run root'
$buildLogPath = Join-Path $runRoot 'relwithdebinfo-clean-build.log'
if ((Get-FileHash -LiteralPath $buildLogPath -Algorithm SHA256).Hash -ne $build.build_log_sha256) {
    throw 'Clean-build log hash does not match its physical sibling.'
}
$cyclesRoot = Join-Path $runRoot 'runs'
$cycleNames = @(1..20 | ForEach-Object { '{0:D2}' -f $_ })
Assert-ExactChildren -Root $cyclesRoot -ExpectedNames $cycleNames -Description 'Cycle root'
$stableDimensions = $null

for ($index = 0; $index -lt 20; $index++) {
    $cycleNumber = $index + 1
    $cycleName = '{0:D2}' -f $cycleNumber
    $cycle = $cycles[$index]
    Assert-ExactProperties -Value $cycle -Description "Cycle $cycleName" -Expected @(
        'index', 'first_frame_elapsed_milliseconds', 'dwell_elapsed_milliseconds',
        'exit_elapsed_milliseconds', 'exit_code', 'startup_marker_count',
        'first_active_refresh_count', 'present_count_1_sequence', 'present_count_3_sequence',
        'present_count_1_hresult', 'present_count_3_hresult',
        'minimum_successful_guest_present_count', 'source_width', 'source_height',
        'swapchain_width', 'swapchain_height', 'capture_sequence',
        'capture_last_presented_sequence',
        'capture_success_marker_count', 'capture_width',
        'capture_height', 'present_result_class', 'close_requested',
        'window_close_marker_occurrences', 'hard_exit_marker_occurrences',
        'post_hard_exit_execution_complete_occurrences',
        'harness_force_cleanup', 'process_signal_confirmed',
        'process_cleanup_confirmed', 'prior_cycles_immutable',
        'runtime_log_sha256', 'capture_relative_path', 'capture_sha256',
        'capture_bytes', 'capture_metrics', 'user_tree_sha256', 'cache_tree_sha256',
        'user_file_count', 'cache_file_count', 'user_bytes', 'cache_bytes',
        'cycle_tree_sha256'
    )
    Assert-JsonTypes -Value $cycle -Description "Cycle $cycleName" `
        -BooleanNames @(
            'close_requested', 'harness_force_cleanup', 'process_signal_confirmed',
            'process_cleanup_confirmed', 'prior_cycles_immutable') `
        -IntegerNames @(
            'index', 'first_frame_elapsed_milliseconds', 'dwell_elapsed_milliseconds',
            'exit_elapsed_milliseconds', 'exit_code', 'startup_marker_count',
            'first_active_refresh_count', 'present_count_1_sequence',
            'present_count_3_sequence', 'minimum_successful_guest_present_count',
            'source_width', 'source_height', 'swapchain_width', 'swapchain_height',
            'capture_sequence', 'capture_last_presented_sequence',
            'capture_success_marker_count', 'capture_width',
            'capture_height', 'window_close_marker_occurrences',
            'hard_exit_marker_occurrences',
            'post_hard_exit_execution_complete_occurrences', 'capture_bytes', 'user_file_count',
            'cache_file_count', 'user_bytes', 'cache_bytes') `
        -StringNames @(
            'present_count_1_hresult', 'present_count_3_hresult',
            'present_result_class', 'runtime_log_sha256', 'capture_relative_path',
            'capture_sha256', 'user_tree_sha256', 'cache_tree_sha256',
            'cycle_tree_sha256')
    if ($cycle.index -ne $cycleNumber -or $cycle.first_frame_elapsed_milliseconds -lt 0 -or
        $cycle.first_frame_elapsed_milliseconds -gt 60000 -or
        $cycle.dwell_elapsed_milliseconds -lt $result.post_marker_dwell_milliseconds -or
        $cycle.dwell_elapsed_milliseconds -gt 10000 -or
        $cycle.exit_elapsed_milliseconds -lt 0 -or $cycle.exit_elapsed_milliseconds -gt 10000 -or
        $cycle.exit_code -ne 0 -or $cycle.startup_marker_count -ne 15 -or
        $cycle.minimum_successful_guest_present_count -ne 3 -or
        $cycle.present_count_1_hresult -notmatch '^0x[0-9A-F]{8}$' -or
        $cycle.present_count_3_hresult -notmatch '^0x[0-9A-F]{8}$' -or
        $cycle.present_result_class -ne 'SUCCEEDED' -or
        $cycle.close_requested -ne $true -or $cycle.window_close_marker_occurrences -ne 1 -or
        $cycle.hard_exit_marker_occurrences -ne 1 -or $cycle.harness_force_cleanup -ne $false -or
        $cycle.post_hard_exit_execution_complete_occurrences -lt 0 -or
        $cycle.post_hard_exit_execution_complete_occurrences -gt 1 -or
        $cycle.process_signal_confirmed -ne $true -or
        $cycle.process_cleanup_confirmed -ne $true -or $cycle.prior_cycles_immutable -ne $true -or
        [string]$cycle.runtime_log_sha256 -notmatch $hashPattern -or
        [string]$cycle.capture_sha256 -notmatch $hashPattern -or
        [string]$cycle.user_tree_sha256 -notmatch $hashPattern -or
        [string]$cycle.cache_tree_sha256 -notmatch $hashPattern -or
        [string]$cycle.cycle_tree_sha256 -notmatch $hashPattern) {
        throw "First-frame cycle $cycleName is incomplete or outside canonical bounds."
    }
    $expectedRelative = "runs/$cycleName/user/mcla-first-frame.bmp"
    if ($cycle.capture_relative_path -cne $expectedRelative) {
        throw "First-frame cycle $cycleName has an unsafe or unexpected capture path."
    }

    $cycleRoot = Join-Path $cyclesRoot $cycleName
    Assert-ExactChildren -Root $cycleRoot -ExpectedNames @('cache', 'mcla.log', 'user') `
        -Description "Cycle $cycleName"
    $logPath = Join-Path $cycleRoot 'mcla.log'
    $userRoot = Join-Path $cycleRoot 'user'
    $framePath = Join-Path $userRoot 'mcla-first-frame.bmp'
    $bmpFiles = @(Get-ChildItem -LiteralPath $cycleRoot -Recurse -File -Force -Filter '*.bmp')
    if ($bmpFiles.Count -ne 1 -or -not [string]::Equals($bmpFiles[0].FullName, $framePath,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "First-frame cycle $cycleName must contain exactly its one expected BMP."
    }
    $cacheRoot = Join-Path $cycleRoot 'cache'
    $probe = Get-ProbeEvidence -LogPath $logPath -FramePath $framePath
    $currentDimensions = '{0}x{1}/{2}x{3}' -f $probe.Log.SourceWidth,
        $probe.Log.SourceHeight, $probe.Log.SwapchainWidth, $probe.Log.SwapchainHeight
    if ($null -eq $stableDimensions) {
        $stableDimensions = $currentDimensions
    } elseif ($currentDimensions -cne $stableDimensions) {
        throw "First-frame cycle $cycleName changed source/swap-chain dimensions across runs."
    }
    $userTree = Get-TreeSnapshot -Root $userRoot
    $cacheTree = Get-TreeSnapshot -Root $cacheRoot
    $cycleTree = Get-TreeSnapshot -Root $cycleRoot
    $metrics = $cycle.capture_metrics
    Assert-ExactProperties -Value $metrics -Description "Cycle $cycleName capture metrics" -Expected @(
        'width', 'height', 'stride', 'pixel_count', 'occupied_rgb555_bins',
        'luma_p05', 'luma_p95', 'luma_spread', 'modal_pixels',
        'modal_per_mille', 'nonmodal_grid_cells'
    )
    Assert-JsonTypes -Value $metrics -Description "Cycle $cycleName capture metrics" `
        -IntegerNames @(
            'width', 'height', 'stride', 'pixel_count', 'occupied_rgb555_bins',
            'luma_p05', 'luma_p95', 'luma_spread', 'modal_pixels',
            'modal_per_mille', 'nonmodal_grid_cells')
    if ($cycle.runtime_log_sha256 -ne $probe.Log.Sha256 -or
        $cycle.capture_sha256 -ne $probe.Bmp.Sha256 -or
        $cycle.capture_bytes -ne $probe.Bmp.Bytes -or
        $cycle.startup_marker_count -ne $probe.Log.StartupMarkerCount -or
        $cycle.first_active_refresh_count -ne $probe.Log.FirstActiveRefreshCount -or
        $cycle.present_count_1_sequence -ne $probe.Log.PresentOneSequence -or
        $cycle.present_count_3_sequence -ne $probe.Log.PresentThreeSequence -or
        $cycle.present_count_1_hresult -ne $probe.Log.PresentOneHresult -or
        $cycle.present_count_3_hresult -ne $probe.Log.PresentThreeHresult -or
        $cycle.minimum_successful_guest_present_count -ne
            $probe.Log.MinimumSuccessfulPresentCount -or
        $cycle.present_result_class -ne $probe.Log.PresentResultClass -or
        $cycle.source_width -ne $probe.Log.SourceWidth -or
        $cycle.source_height -ne $probe.Log.SourceHeight -or
        $cycle.swapchain_width -ne $probe.Log.SwapchainWidth -or
        $cycle.swapchain_height -ne $probe.Log.SwapchainHeight -or
        $cycle.capture_sequence -ne $probe.Log.CaptureSequence -or
        $cycle.capture_last_presented_sequence -ne $probe.Log.CaptureLastPresentedSequence -or
        $cycle.capture_success_marker_count -ne $probe.Log.CaptureSuccessMarkerCount -or
        $cycle.capture_width -ne $probe.Log.CaptureWidth -or
        $cycle.capture_height -ne $probe.Log.CaptureHeight -or
        $cycle.post_hard_exit_execution_complete_occurrences -ne
            $probe.Log.PostHardExitExecutionCompleteMarkers -or
        $metrics.width -ne $probe.Bmp.Width -or $metrics.height -ne $probe.Bmp.Height -or
        $metrics.stride -ne $probe.Bmp.Stride -or $metrics.pixel_count -ne $probe.Bmp.PixelCount -or
        $metrics.occupied_rgb555_bins -ne $probe.Bmp.OccupiedRgb555Bins -or
        $metrics.luma_p05 -ne $probe.Bmp.LumaP05 -or $metrics.luma_p95 -ne $probe.Bmp.LumaP95 -or
        $metrics.luma_spread -ne $probe.Bmp.LumaSpread -or
        $metrics.modal_pixels -ne $probe.Bmp.ModalPixels -or
        $metrics.modal_per_mille -ne $probe.Bmp.ModalPermille -or
        $metrics.nonmodal_grid_cells -ne $probe.Bmp.NonmodalGridCells -or
        $cycle.user_tree_sha256 -ne $userTree.Hash -or
        $cycle.cache_tree_sha256 -ne $cacheTree.Hash -or
        $cycle.user_file_count -ne $userTree.FileCount -or
        $cycle.cache_file_count -ne $cacheTree.FileCount -or
        $cycle.user_bytes -ne $userTree.Bytes -or $cycle.cache_bytes -ne $cacheTree.Bytes -or
        $cycle.cycle_tree_sha256 -ne $cycleTree.Hash) {
        throw "First-frame cycle $cycleName does not match its physical evidence."
    }
}

if ($result.all_write_roots_contained -ne $true -or
    $result.all_prior_cycles_immutable -ne $true -or
    $result.no_surviving_processes -ne $true -or
    $result.data_integrity_preserved -ne $true -or
    $result.all_captures_nontrivial -ne $true) {
    throw 'First-frame aggregate does not prove containment, cleanup, integrity, and captures.'
}

[pscustomobject]@{
    Passed = $true
    Cycles = 20
    MinimumSuccessfulGuestPresentsPerCycle = 3
    PhysicalCapturesVerified = 20
    ProcessCleanupVerified = $true
    DataIntegrityVerified = $true
}
