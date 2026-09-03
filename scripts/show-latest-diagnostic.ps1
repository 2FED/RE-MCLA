[CmdletBinding()]
param(
    [ValidateSet('Any','Live','Crash')][string]$Kind = 'Any',
    [string]$UserDataRoot = '',
    [switch]$Open
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $UserDataRoot) {
    $documents = [Environment]::GetFolderPath([Environment+SpecialFolder]::MyDocuments)
    if (-not $documents) { throw 'The Windows Documents folder could not be resolved. Pass -UserDataRoot explicitly.' }
    $UserDataRoot = Join-Path $documents 'mcla'
}
$userRoot = [IO.Path]::GetFullPath($UserDataRoot)
$diagnosticsRoot = Join-Path $userRoot 'diagnostics'

function Read-Latest([string]$PackageKind) {
    $slug = $PackageKind.ToLowerInvariant()
    $pointer = Join-Path $diagnosticsRoot ("latest-{0}.txt" -f $slug)
    if (-not (Test-Path -LiteralPath $pointer -PathType Leaf)) { return $null }
    $name = (Get-Content -LiteralPath $pointer -Raw).Trim()
    if ($name -notmatch ('^{0}-[0-9TZ-]+$' -f [Regex]::Escape($slug))) { throw "The $PackageKind diagnostic pointer is malformed." }
    $path = Join-Path (Join-Path $diagnosticsRoot $slug) $name
    if (-not (Test-Path -LiteralPath $path -PathType Container)) { throw "The $PackageKind diagnostic pointer targets a missing package." }
    $manifest = Join-Path $path 'manifest.json'
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) { throw "The $PackageKind diagnostic package has no completed manifest." }
    [pscustomobject]@{ Kind=$PackageKind; Path=(Resolve-Path -LiteralPath $path).Path; WrittenUtc=(Get-Item -LiteralPath $manifest).LastWriteTimeUtc }
}

$candidates = @()
if ($Kind -in @('Any','Live')) { $candidate=Read-Latest 'Live'; if($candidate){$candidates+=$candidate} }
if ($Kind -in @('Any','Crash')) { $candidate=Read-Latest 'Crash'; if($candidate){$candidates+=$candidate} }
if (-not $candidates.Count) { throw "No completed $Kind diagnostic package exists below '$diagnosticsRoot'." }
$latest = $candidates | Sort-Object WrittenUtc -Descending | Select-Object -First 1
if ($Open) { Invoke-Item -LiteralPath $latest.Path }
$latest
