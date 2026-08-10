[CmdletBinding()]
param(
    [string]$GamePath,
    [string]$ManifestPath,
    [switch]$VerifyHashes
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
if ([string]::IsNullOrWhiteSpace($GamePath)) {
    $GamePath = Join-Path $repoRoot 'private\game'
}
if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $ManifestPath = Join-Path $repoRoot 'private\game-manifest.json'
}

$gameRoot = (Resolve-Path -LiteralPath $GamePath -ErrorAction Stop).Path.TrimEnd(
    [System.IO.Path]::DirectorySeparatorChar)
$resolvedManifest = (Resolve-Path -LiteralPath $ManifestPath -ErrorAction Stop).Path
if (-not (Test-Path -LiteralPath $gameRoot -PathType Container)) {
    throw "Game path is not a directory: $gameRoot"
}
if (-not (Test-Path -LiteralPath $resolvedManifest -PathType Leaf)) {
    throw "Manifest path is not a file: $resolvedManifest"
}

$manifest = Get-Content -Raw -LiteralPath $resolvedManifest | ConvertFrom-Json
if ([int]$manifest.SchemaVersion -ne 1) {
    throw "Unsupported manifest schema version: $($manifest.SchemaVersion)"
}
if ($manifest.SourceIsoSha256 -ne 'AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB') {
    throw "Manifest source ISO hash is unsupported: $($manifest.SourceIsoSha256)"
}
if ([int]$manifest.FileCount -ne 15 -or @($manifest.Files).Count -ne 15) {
    throw "Manifest must contain exactly 15 files; header=$($manifest.FileCount), entries=$(@($manifest.Files).Count)."
}

$comparison = [System.Collections.Generic.Dictionary[string, object]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
$gamePrefix = $gameRoot + [System.IO.Path]::DirectorySeparatorChar
foreach ($entry in $manifest.Files) {
    $relativePath = [string]$entry.Path
    if ([string]::IsNullOrWhiteSpace($relativePath) -or
        [System.IO.Path]::IsPathRooted($relativePath) -or
        @($relativePath.Replace('\', '/').Split('/') | Where-Object { $_ -eq '..' }).Count -gt 0) {
        throw "Manifest contains an unsafe relative path: '$relativePath'"
    }
    if ($comparison.ContainsKey($relativePath)) {
        throw "Manifest contains a duplicate path: '$relativePath'"
    }

    $candidate = [System.IO.Path]::GetFullPath((Join-Path $gameRoot $relativePath.Replace('/', '\')))
    if (-not $candidate.StartsWith($gamePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Manifest path escaped the game directory: '$relativePath'"
    }
    $comparison.Add($relativePath, [pscustomobject]@{
        FullPath       = $candidate
        ExpectedSize   = [long]$entry.Size
        ExpectedSha256 = ([string]$entry.Sha256).ToUpperInvariant()
    })
}

$actualFiles = @(Get-ChildItem -LiteralPath $gameRoot -Recurse -File -Force)
if ($actualFiles.Count -ne $comparison.Count) {
    throw "Actual file count $($actualFiles.Count) differs from manifest count $($comparison.Count)."
}

$payloadBytes = [long]0
$verifiedHashes = 0
foreach ($file in $actualFiles) {
    $relativePath = $file.FullName.Substring($gameRoot.Length).TrimStart('\').Replace('\', '/')
    if (-not $comparison.ContainsKey($relativePath)) {
        throw "Unexpected extracted file: '$relativePath'"
    }
    $expectedEntry = $comparison[$relativePath]
    if ([long]$file.Length -ne $expectedEntry.ExpectedSize) {
        throw "Size mismatch for '$relativePath'. Expected $($expectedEntry.ExpectedSize), got $($file.Length)."
    }
    $payloadBytes += [long]$file.Length

    if ($VerifyHashes) {
        $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToUpperInvariant()
        if ($actualHash -ne $expectedEntry.ExpectedSha256) {
            throw "SHA-256 mismatch for '$relativePath'. Expected '$($expectedEntry.ExpectedSha256)', got '$actualHash'."
        }
        $verifiedHashes++
    }
}

if ($payloadBytes -ne [long]$manifest.PayloadBytes) {
    throw "Payload byte count mismatch. Expected $($manifest.PayloadBytes), got $payloadBytes."
}

$rpfEntries = @($manifest.Files | Where-Object { ([string]$_.Path).EndsWith('.rpf', [System.StringComparison]::OrdinalIgnoreCase) })
$bikEntries = @($manifest.Files | Where-Object { ([string]$_.Path).EndsWith('.bik', [System.StringComparison]::OrdinalIgnoreCase) })
if ($rpfEntries.Count -ne 4) {
    throw "Expected exactly 4 RPF entries, got $($rpfEntries.Count)."
}
if ($bikEntries.Count -ne 6) {
    throw "Expected exactly 6 BIK entries, got $($bikEntries.Count)."
}

[pscustomobject]@{
    Valid              = $true
    GamePath           = $gameRoot
    ManifestPath       = $resolvedManifest
    FileCount          = $actualFiles.Count
    PayloadBytes       = $payloadBytes
    RpfCount           = $rpfEntries.Count
    RpfBytes           = [long](($rpfEntries | Measure-Object -Property Size -Sum).Sum)
    BikCount           = $bikEntries.Count
    BikBytes           = [long](($bikEntries | Measure-Object -Property Size -Sum).Sum)
    HashesVerified     = $verifiedHashes
    SourceIsoSha256    = [string]$manifest.SourceIsoSha256
}
