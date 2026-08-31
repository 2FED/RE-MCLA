Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-photo-mode-readback.ps1'
$root = Join-Path $repo ('private/test-photo-mode-readback-' + [guid]::NewGuid().ToString('N'))
Add-Type -AssemblyName System.Drawing

function Write-AlbumFixture([string]$Path, [bool]$Black) {
  $bitmap = [Drawing.Bitmap]::new(640, 360)
  try {
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    try {
      $background = if ($Black) { [Drawing.Color]::Black } else { [Drawing.Color]::CornflowerBlue }
      $graphics.Clear($background)
      if (-not $Black) {
        $graphics.FillRectangle([Drawing.Brushes]::OrangeRed, 80, 60, 240, 160)
        $graphics.FillEllipse([Drawing.Brushes]::White, 360, 100, 180, 180)
      }
    } finally { $graphics.Dispose() }
    $jpeg = [IO.MemoryStream]::new()
    try {
      $bitmap.Save($jpeg, [Drawing.Imaging.ImageFormat]::Jpeg)
      $prefix = [byte[]]::new(204)
      $suffix = [byte[]]::new(32)
      $payload = $jpeg.ToArray()
      $container = [byte[]]::new($prefix.Length + $payload.Length + $suffix.Length)
      [Array]::Copy($prefix, 0, $container, 0, $prefix.Length)
      [Array]::Copy($payload, 0, $container, $prefix.Length, $payload.Length)
      [Array]::Copy($suffix, 0, $container, $prefix.Length + $payload.Length, $suffix.Length)
      [IO.File]::WriteAllBytes($Path, $container)
    } finally { $jpeg.Dispose() }
  } finally { $bitmap.Dispose() }
}

try {
  [IO.Directory]::CreateDirectory($root) | Out-Null
  $positive = Join-Path $root 'positive.album'
  $negative = Join-Path $root 'black.album'
  Write-AlbumFixture $positive $false
  Write-AlbumFixture $negative $true
  $result = & $verify -AlbumPath $positive
  if ($result.Decision -cne 'photo-mode-cpu-readback-nonblack-pass' -or
      $result.JpegPayloads -ne 1 -or $result.NonblackPayloads -ne 1) {
    throw 'Positive Photo Mode fixture did not pass exactly.'
  }
  $failed = $false
  try { & $verify -AlbumPath $negative | Out-Null } catch {
    $failed = $_.Exception.Message -ceq 'Every decoded Photo Mode JPEG is globally black or visually empty.'
  }
  if (-not $failed) { throw 'Black Photo Mode fixture did not fail closed.' }
  [pscustomobject][ordered]@{ PositiveFixtures = 1; FailClosedBlackFixtures = 1; Passed = $true }
} finally {
  if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
