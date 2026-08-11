[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'High')]
param(
    [string]$GeneratedRoot = 'generated/default',
    [string]$OutputPath = 'private/evidence/M3-013/generated-manifest-v0.9.0.7.json'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path

function Resolve-ContainedPath {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Description)
    $candidate = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repoRoot $Path }
    $full = [System.IO.Path]::GetFullPath($candidate)
    $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $full.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay inside the repository: '$full'."
    }
    return $full
}

$generatedRootPath = Resolve-ContainedPath -Path $GeneratedRoot -Description 'Generated root'
$outputPathValue = Resolve-ContainedPath -Path $OutputPath -Description 'Output path'
if (-not (Test-Path -LiteralPath $generatedRootPath -PathType Container)) {
    throw "Generated root was not found: '$generatedRootPath'."
}
if ((Get-Item -LiteralPath $generatedRootPath -Force).Attributes -band
    [System.IO.FileAttributes]::ReparsePoint) {
    throw "Generated root must not be a reparse point: '$generatedRootPath'."
}
if (-not $outputPathValue.StartsWith(
        (Join-Path $repoRoot 'private').TrimEnd('\') + '\',
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Generated manifest must stay under the ignored private root: '$outputPathValue'."
}
if (Test-Path -LiteralPath $outputPathValue) {
    throw "Generated manifest already exists and will not be overwritten: '$outputPathValue'."
}

$files = @(Get-ChildItem -LiteralPath $generatedRootPath -File -Recurse -Force | Sort-Object FullName)
if ($files.Count -eq 0) {
    throw 'Generated root is empty.'
}
$rootPrefix = $generatedRootPath.TrimEnd('\') + '\'
$entries = @($files | ForEach-Object {
    if ($_.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Generated file must not be a reparse point: '$($_.FullName)'."
    }
    [ordered]@{
        path = $_.FullName.Substring($rootPrefix.Length).Replace('\', '/')
        bytes = $_.Length
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
})
$manifest = [ordered]@{
    schema = 1
    file_count = $entries.Count
    total_bytes = [long](($files | Measure-Object Length -Sum).Sum)
    files = $entries
}

if (-not $PSCmdlet.ShouldProcess($outputPathValue, 'Write private generated-integration manifest')) {
    [pscustomobject]@{
        Ready = $true
        GeneratedFiles = $manifest.file_count
        GeneratedBytes = $manifest.total_bytes
        OutputPath = $outputPathValue
    }
    return
}

[System.IO.Directory]::CreateDirectory((Split-Path -Parent $outputPathValue)) | Out-Null
[System.IO.File]::WriteAllText(
    $outputPathValue,
    (($manifest | ConvertTo-Json -Depth 5) + [Environment]::NewLine),
    [System.Text.UTF8Encoding]::new($false)
)

[pscustomobject]@{
    Written = $true
    GeneratedFiles = $manifest.file_count
    GeneratedCppSources = @($entries | Where-Object { $_.path -match '[.]cpp$' }).Count
    GeneratedBytes = $manifest.total_bytes
    ManifestSha256 = (Get-FileHash -LiteralPath $outputPathValue -Algorithm SHA256).Hash
    OutputPath = $outputPathValue
}
