[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'High')]
param(
    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$IsoPath,

    [Parameter(Mandatory)]
    [ValidateNotNullOrEmpty()]
    [string]$DestinationPath,

    [ValidateNotNullOrEmpty()]
    [string]$ExtractorPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$privateRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot 'private'))
$separator = [System.IO.Path]::DirectorySeparatorChar
$privatePrefix = $privateRoot.TrimEnd($separator) + $separator
$expectedExtractorSha256 = '7C7AF9C17E095C3C1E78E644DF5F0E72F01C4690B3117F038AAFE26EB5A8A2F4'

function Test-IsContainedPath {
    param(
        [Parameter(Mandatory)][string]$Candidate,
        [Parameter(Mandatory)][string]$RootPrefix
    )

    return $Candidate.StartsWith($RootPrefix, [System.StringComparison]::OrdinalIgnoreCase)
}

function Assert-NoReparseTraversal {
    param(
        [Parameter(Mandatory)][string]$ExistingPath,
        [Parameter(Mandatory)][string]$ContainmentRoot
    )

    $current = [System.IO.Path]::GetFullPath($ExistingPath)
    $root = [System.IO.Path]::GetFullPath($ContainmentRoot).TrimEnd($separator)
    while ($true) {
        $item = Get-Item -LiteralPath $current -Force -ErrorAction Stop
        if (($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Reparse points are not allowed in the extraction path: $current"
        }
        if ($current.TrimEnd($separator).Equals($root, [System.StringComparison]::OrdinalIgnoreCase)) {
            break
        }
        $parent = Split-Path -Parent $current
        if (-not $parent -or $parent.Equals($current, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Extraction path ancestor escaped private root: $ExistingPath"
        }
        $current = $parent
    }
}

if (-not (Test-Path -LiteralPath $privateRoot -PathType Container)) {
    New-Item -ItemType Directory -Path $privateRoot | Out-Null
}

$resolvedIso = (Resolve-Path -LiteralPath $IsoPath -ErrorAction Stop).Path
if (-not (Test-Path -LiteralPath $resolvedIso -PathType Leaf)) {
    throw "ISO path is not a regular file: $resolvedIso"
}

if ([System.IO.Path]::IsPathRooted($DestinationPath)) {
    $destination = [System.IO.Path]::GetFullPath($DestinationPath)
} else {
    $destination = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $DestinationPath))
}
if (-not (Test-IsContainedPath -Candidate $destination -RootPrefix $privatePrefix)) {
    throw "Destination must be a child of '$privateRoot': $destination"
}
if (Test-Path -LiteralPath $destination) {
    throw "Destination already exists; refusing to overwrite it: $destination"
}

$destinationParent = Split-Path -Parent $destination
if (-not (Test-Path -LiteralPath $destinationParent -PathType Container)) {
    throw "Destination parent must already exist: $destinationParent"
}
if (-not (Test-IsContainedPath -Candidate ($destinationParent.TrimEnd($separator) + $separator) -RootPrefix $privatePrefix) -and
    -not $destinationParent.Equals($privateRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Destination parent escaped private root: $destinationParent"
}
Assert-NoReparseTraversal -ExistingPath $destinationParent -ContainmentRoot $privateRoot

if ([string]::IsNullOrWhiteSpace($ExtractorPath)) {
    $ExtractorPath = Join-Path $privateRoot 'tools\extract-xiso\artifacts\extract-xiso.exe'
}
$resolvedExtractor = (Resolve-Path -LiteralPath $ExtractorPath -ErrorAction Stop).Path
if (-not (Test-Path -LiteralPath $resolvedExtractor -PathType Leaf)) {
    throw "Extractor path is not a regular file: $resolvedExtractor"
}
$extractorHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedExtractor).Hash.ToUpperInvariant()
if ($extractorHash -ne $expectedExtractorSha256) {
    throw "Extractor SHA-256 mismatch. Expected '$expectedExtractorSha256', got '$extractorHash'."
}

$verification = & (Join-Path $PSScriptRoot 'verify-source.ps1') -IsoPath $resolvedIso
if (-not $verification.Valid) {
    throw 'Source verifier did not return a valid result.'
}

if (-not $PSCmdlet.ShouldProcess($destination, "Extract verified MCLA source with $resolvedExtractor")) {
    return [pscustomobject]@{
        Extracted       = $false
        PreflightPassed = $true
        IsoPath         = $resolvedIso
        DestinationPath = $destination
        ExtractorPath   = $resolvedExtractor
        SourceSha256    = $verification.IsoSha256
    }
}

$staging = Join-Path $privateRoot ('.extracting-' + [Guid]::NewGuid().ToString('N'))
if (-not (Test-IsContainedPath -Candidate $staging -RootPrefix $privatePrefix)) {
    throw "Generated staging path escaped private root: $staging"
}

try {
    New-Item -ItemType Directory -Path $staging | Out-Null
    # extract-xiso's Windows getopt stops parsing options at the first ISO
    # argument, so every option must precede the source path.
    & $resolvedExtractor -q -x -d $staging $resolvedIso
    if ($LASTEXITCODE -ne 0) {
        throw "extract-xiso failed with exit code $LASTEXITCODE."
    }

    $extractedFiles = @(Get-ChildItem -LiteralPath $staging -Recurse -File -Force)
    if ($extractedFiles.Count -eq 0) {
        throw 'Extractor completed without producing files.'
    }
    if (Test-Path -LiteralPath $destination) {
        throw "Destination appeared during extraction; refusing to overwrite it: $destination"
    }

    Move-Item -LiteralPath $staging -Destination $destination
    [pscustomobject]@{
        Extracted       = $true
        PreflightPassed = $true
        IsoPath         = $resolvedIso
        DestinationPath = $destination
        ExtractorPath   = $resolvedExtractor
        SourceSha256    = $verification.IsoSha256
        FileCount       = $extractedFiles.Count
        PayloadBytes    = [long](($extractedFiles | Measure-Object -Property Length -Sum).Sum)
    }
} finally {
    if (Test-Path -LiteralPath $staging -PathType Container) {
        $resolvedStaging = [System.IO.Path]::GetFullPath($staging)
        if (-not (Test-IsContainedPath -Candidate $resolvedStaging -RootPrefix $privatePrefix) -or
            -not ([System.IO.Path]::GetFileName($resolvedStaging)).StartsWith('.extracting-', [System.StringComparison]::Ordinal)) {
            throw "Refusing to clean unexpected staging path: $resolvedStaging"
        }
        Remove-Item -LiteralPath $resolvedStaging -Recurse -Force
    }
}
