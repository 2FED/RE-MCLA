[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$IsoPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$expected = [ordered]@{
    IsoSize      = [long]7838695424
    IsoSha256    = 'AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB'
    TitleId      = '545407F8'
    MediaId      = '5940C9DB'
    XexSize      = [long]9252864
    XexSha256    = 'C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432'
}

function Assert-Equal {
    param(
        [Parameter(Mandatory)]$Actual,
        [Parameter(Mandatory)]$Expected,
        [Parameter(Mandatory)][string]$Label
    )

    if ($Actual -ne $Expected) {
        throw "$Label mismatch. Expected '$Expected', got '$Actual'."
    }
}

function Read-Exact {
    param(
        [Parameter(Mandatory)][System.IO.Stream]$Stream,
        [Parameter(Mandatory)][byte[]]$Buffer,
        [int]$Offset = 0,
        [int]$Count = $Buffer.Length
    )

    $total = 0
    while ($total -lt $Count) {
        $read = $Stream.Read($Buffer, $Offset + $total, $Count - $total)
        if ($read -eq 0) {
            throw "Unexpected end of file after $total of $Count requested bytes."
        }
        $total += $read
    }
}

function Read-UInt32BigEndian {
    param(
        [Parameter(Mandatory)][byte[]]$Buffer,
        [Parameter(Mandatory)][int]$Offset
    )

    return [uint32]((([uint32]$Buffer[$Offset]) -shl 24) -bor
        (([uint32]$Buffer[$Offset + 1]) -shl 16) -bor
        (([uint32]$Buffer[$Offset + 2]) -shl 8) -bor
        ([uint32]$Buffer[$Offset + 3]))
}

function Find-XdvdfsMediaHeader {
    param(
        [Parameter(Mandatory)][System.IO.Stream]$Stream
    )

    $magic = 'MICROSOFT*XBOX*MEDIA'
    $encoding = [System.Text.Encoding]::ASCII
    $buffer = [byte[]]::new(4MB)
    $overlap = $magic.Length - 1
    $position = [long]0
    $scanLimit = [Math]::Min($Stream.Length, [long]1GB)

    while ($position -lt $scanLimit) {
        $Stream.Position = $position
        $wanted = [int][Math]::Min($buffer.Length, $scanLimit - $position)
        $read = $Stream.Read($buffer, 0, $wanted)
        if ($read -eq 0) {
            break
        }

        $text = $encoding.GetString($buffer, 0, $read)
        $index = $text.IndexOf($magic, [System.StringComparison]::Ordinal)
        if ($index -ge 0) {
            return $position + $index
        }

        if ($read -le $overlap) {
            break
        }
        $position += $read - $overlap
    }

    throw 'XDVDFS media header was not found within the first 1 GiB of the image.'
}

function Get-RootFileEntry {
    param(
        [Parameter(Mandatory)][byte[]]$Directory,
        [Parameter(Mandatory)][string]$FileName
    )

    $pending = [System.Collections.Generic.Stack[int]]::new()
    $visited = [System.Collections.Generic.HashSet[int]]::new()
    $pending.Push(0)

    while ($pending.Count -gt 0) {
        $offset = $pending.Pop()
        if (-not $visited.Add($offset)) {
            continue
        }
        if ($offset -lt 0 -or $offset + 14 -gt $Directory.Length) {
            throw "Invalid XDVDFS directory node offset $offset."
        }

        $leftOffset = [int]([BitConverter]::ToUInt16($Directory, $offset) * 4)
        $rightOffset = [int]([BitConverter]::ToUInt16($Directory, $offset + 2) * 4)
        $startSector = [uint32][BitConverter]::ToUInt32($Directory, $offset + 4)
        $size = [uint32][BitConverter]::ToUInt32($Directory, $offset + 8)
        $attributes = $Directory[$offset + 12]
        $nameLength = [int]$Directory[$offset + 13]
        if ($nameLength -le 0 -or $offset + 14 + $nameLength -gt $Directory.Length) {
            throw "Invalid XDVDFS filename length $nameLength at directory offset $offset."
        }

        $name = [System.Text.Encoding]::ASCII.GetString($Directory, $offset + 14, $nameLength)
        if ($name.Equals($FileName, [System.StringComparison]::OrdinalIgnoreCase)) {
            return [pscustomobject]@{
                Name        = $name
                StartSector = $startSector
                Size        = [long]$size
                Attributes  = $attributes
            }
        }

        if ($rightOffset -ne 0) { $pending.Push($rightOffset) }
        if ($leftOffset -ne 0) { $pending.Push($leftOffset) }
    }

    throw "'$FileName' was not found in the XDVDFS root directory."
}

function Get-StreamRangeSha256 {
    param(
        [Parameter(Mandatory)][System.IO.Stream]$Stream,
        [Parameter(Mandatory)][long]$Offset,
        [Parameter(Mandatory)][long]$Length
    )

    $hash = [System.Security.Cryptography.IncrementalHash]::CreateHash(
        [System.Security.Cryptography.HashAlgorithmName]::SHA256)
    try {
        $buffer = [byte[]]::new(4MB)
        $remaining = $Length
        $Stream.Position = $Offset
        while ($remaining -gt 0) {
            $wanted = [int][Math]::Min($buffer.Length, $remaining)
            $read = $Stream.Read($buffer, 0, $wanted)
            if ($read -eq 0) {
                throw "Unexpected end of file while hashing range at offset $Offset."
            }
            $hash.AppendData($buffer, 0, $read)
            $remaining -= $read
        }
        return ([BitConverter]::ToString($hash.GetHashAndReset())).Replace('-', '')
    } finally {
        $hash.Dispose()
    }
}

$resolvedIso = (Resolve-Path -LiteralPath $IsoPath -ErrorAction Stop).Path
$isoInfo = Get-Item -LiteralPath $resolvedIso -ErrorAction Stop
if (-not $isoInfo.PSIsContainer) {
    Assert-Equal -Label 'ISO size' -Actual ([long]$isoInfo.Length) -Expected $expected.IsoSize
} else {
    throw "ISO path points to a directory: $resolvedIso"
}

$isoHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedIso).Hash.ToUpperInvariant()
Assert-Equal -Label 'ISO SHA-256' -Actual $isoHash -Expected $expected.IsoSha256

$stream = [System.IO.File]::Open($resolvedIso, [System.IO.FileMode]::Open,
    [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
try {
    $mediaHeaderOffset = Find-XdvdfsMediaHeader -Stream $stream
    $partitionOffset = $mediaHeaderOffset - 0x10000
    if ($partitionOffset -lt 0) {
        throw "Invalid XDVDFS partition offset $partitionOffset."
    }

    $mediaHeader = [byte[]]::new(28)
    $stream.Position = $mediaHeaderOffset
    Read-Exact -Stream $stream -Buffer $mediaHeader
    $rootSector = [uint32][BitConverter]::ToUInt32($mediaHeader, 20)
    $rootSize = [uint32][BitConverter]::ToUInt32($mediaHeader, 24)
    if ($rootSize -eq 0 -or $rootSize -gt 16MB) {
        throw "Invalid XDVDFS root directory size $rootSize."
    }

    $rootDirectory = [byte[]]::new([int]$rootSize)
    $stream.Position = $partitionOffset + ([long]$rootSector * 2048)
    Read-Exact -Stream $stream -Buffer $rootDirectory
    $xexEntry = Get-RootFileEntry -Directory $rootDirectory -FileName 'default.xex'
    Assert-Equal -Label 'default.xex size' -Actual $xexEntry.Size -Expected $expected.XexSize

    $xexOffset = $partitionOffset + ([long]$xexEntry.StartSector * 2048)
    if ($xexOffset -lt 0 -or $xexOffset + $xexEntry.Size -gt $stream.Length) {
        throw "default.xex range is outside the ISO: offset $xexOffset, size $($xexEntry.Size)."
    }

    $xexHeader = [byte[]]::new(24)
    $stream.Position = $xexOffset
    Read-Exact -Stream $stream -Buffer $xexHeader
    $xexMagic = [System.Text.Encoding]::ASCII.GetString($xexHeader, 0, 4)
    Assert-Equal -Label 'XEX magic' -Actual $xexMagic -Expected 'XEX2'
    $headerCount = Read-UInt32BigEndian -Buffer $xexHeader -Offset 20
    if ($headerCount -eq 0 -or $headerCount -gt 1024) {
        throw "Invalid XEX optional header count $headerCount."
    }

    $optionalHeaders = [byte[]]::new([int]$headerCount * 8)
    Read-Exact -Stream $stream -Buffer $optionalHeaders
    $executionInfoOffset = $null
    for ($i = 0; $i -lt $headerCount; $i++) {
        $entryOffset = $i * 8
        $key = Read-UInt32BigEndian -Buffer $optionalHeaders -Offset $entryOffset
        if ($key -eq 0x00040006) {
            $executionInfoOffset = Read-UInt32BigEndian -Buffer $optionalHeaders -Offset ($entryOffset + 4)
            break
        }
    }
    if ($null -eq $executionInfoOffset) {
        throw 'XEX execution-info optional header 0x00040006 was not found.'
    }

    $executionInfo = [byte[]]::new(24)
    $stream.Position = $xexOffset + $executionInfoOffset
    Read-Exact -Stream $stream -Buffer $executionInfo
    $mediaId = '{0:X8}' -f (Read-UInt32BigEndian -Buffer $executionInfo -Offset 0)
    $titleId = '{0:X8}' -f (Read-UInt32BigEndian -Buffer $executionInfo -Offset 12)
    Assert-Equal -Label 'Media ID' -Actual $mediaId -Expected $expected.MediaId
    Assert-Equal -Label 'Title ID' -Actual $titleId -Expected $expected.TitleId

    $xexHash = Get-StreamRangeSha256 -Stream $stream -Offset $xexOffset -Length $xexEntry.Size
    Assert-Equal -Label 'default.xex SHA-256' -Actual $xexHash -Expected $expected.XexSha256

    [pscustomobject]@{
        Valid             = $true
        IsoPath           = $resolvedIso
        IsoSize           = [long]$isoInfo.Length
        IsoSha256         = $isoHash
        PartitionOffset   = ('0x{0:X8}' -f $partitionOffset)
        MediaHeaderOffset = ('0x{0:X8}' -f $mediaHeaderOffset)
        DefaultXexOffset  = ('0x{0:X8}' -f $xexOffset)
        DefaultXexSize    = $xexEntry.Size
        DefaultXexSha256  = $xexHash
        TitleId           = $titleId
        MediaId           = $mediaId
    }
} finally {
    $stream.Dispose()
}
