[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$AlbumPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Resolve-RepoFile([string]$Path, [string]$Description) {
  $full = if ([IO.Path]::IsPathRooted($Path)) {
    [IO.Path]::GetFullPath($Path)
  } else {
    [IO.Path]::GetFullPath((Join-Path $repo $Path))
  }
  $prefix = $repo.TrimEnd('\') + '\'
  if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or
      -not (Test-Path -LiteralPath $full -PathType Leaf)) {
    throw "$Description is missing or escapes the repository."
  }
  $cursor = $repo
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object { $_ })) {
    $cursor = Join-Path $cursor $part
    if ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
      throw "$Description traverses a reparse point."
    }
  }
  $full
}

function Get-Sha256([string]$Path) {
  $stream = [IO.File]::OpenRead($Path)
  try {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') }
    finally { $sha.Dispose() }
  } finally { $stream.Dispose() }
}

Add-Type -AssemblyName System.Drawing
$album = Resolve-RepoFile $AlbumPath 'Photo Album container'
$bytes = [IO.File]::ReadAllBytes($album)
$records = @()
$cursor = 0
while ($cursor -lt $bytes.Length - 1) {
  $start = -1
  for ($i = $cursor; $i -lt $bytes.Length - 1; $i++) {
    if ($bytes[$i] -eq 0xFF -and $bytes[$i + 1] -eq 0xD8) { $start = $i; break }
  }
  if ($start -lt 0) { break }
  $end = -1
  for ($i = $start + 2; $i -lt $bytes.Length - 1; $i++) {
    if ($bytes[$i] -eq 0xFF -and $bytes[$i + 1] -eq 0xD9) { $end = $i + 2; break }
  }
  if ($end -lt 0) { throw 'Photo Album contains a truncated JPEG payload.' }
  $length = $end - $start
  $stream = [IO.MemoryStream]::new($bytes, $start, $length, $false)
  try {
    $image = [Drawing.Bitmap]::FromStream($stream)
    try {
      $bins = [Collections.Generic.HashSet[int]]::new()
      $minimumLuma = 255
      $maximumLuma = 0
      $stepX = [Math]::Max(1, [int]($image.Width / 80))
      $stepY = [Math]::Max(1, [int]($image.Height / 45))
      for ($y = 0; $y -lt $image.Height; $y += $stepY) {
        for ($x = 0; $x -lt $image.Width; $x += $stepX) {
          $color = $image.GetPixel($x, $y)
          $null = $bins.Add((($color.R -shr 3) -shl 10) -bor (($color.G -shr 3) -shl 5) -bor ($color.B -shr 3))
          $luma = [int](0.2126 * $color.R + 0.7152 * $color.G + 0.0722 * $color.B)
          $minimumLuma = [Math]::Min($minimumLuma, $luma)
          $maximumLuma = [Math]::Max($maximumLuma, $luma)
        }
      }
      $records += [ordered]@{
        index = $records.Count
        offset = $start
        bytes = $length
        width = $image.Width
        height = $image.Height
        color_bins = $bins.Count
        minimum_luma = $minimumLuma
        maximum_luma = $maximumLuma
        nonblack = $image.Width -ge 320 -and $image.Height -ge 180 -and
          $bins.Count -ge 16 -and $maximumLuma -ge 8
      }
    } finally { $image.Dispose() }
  } catch {
    throw "Photo Album JPEG at offset $start could not be decoded: $($_.Exception.Message)"
  } finally { $stream.Dispose() }
  $cursor = $end
}

if (-not $records.Count) { throw 'Photo Album contains no JPEG payload.' }
if (-not @($records | Where-Object nonblack).Count) {
  throw 'Every decoded Photo Mode JPEG is globally black or visually empty.'
}

[pscustomobject][ordered]@{
  Decision = 'photo-mode-cpu-readback-nonblack-pass'
  AlbumSha256 = Get-Sha256 $album
  JpegPayloads = $records.Count
  NonblackPayloads = @($records | Where-Object nonblack).Count
  Frames = @($records)
}
