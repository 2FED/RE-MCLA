[CmdletBinding()]
param(
    [string]$ManifestPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $ManifestPath) {
    $ManifestPath = Join-Path $repoRoot 'mcla_manifest.toml'
}
if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Manifest was not found: '$ManifestPath'."
}
$resolvedManifest = (Resolve-Path -LiteralPath $ManifestPath).Path
if ((Get-Item -LiteralPath $resolvedManifest -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
    throw "Manifest must not be a reparse point: '$resolvedManifest'."
}

function Assert-SafeRelativePath {
    param(
        [Parameter(Mandatory)][string]$Value,
        [Parameter(Mandatory)][string]$Label
    )

    if ([System.IO.Path]::IsPathRooted($Value) -or $Value.Contains('\')) {
        throw "$Label must be a portable forward-slash relative path. Got '$Value'."
    }
    $segments = @($Value.Split('/') | Where-Object { $_ })
    if ($segments.Count -eq 0 -or $segments -contains '.' -or $segments -contains '..') {
        throw "$Label contains an empty/current/parent traversal segment: '$Value'."
    }
}

function Assert-ContainedPath {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Container,
        [Parameter(Mandatory)][string]$Label
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
    $fullContainer = [System.IO.Path]::GetFullPath($Container).TrimEnd('\')
    if (-not $fullPath.StartsWith("$fullContainer\", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Label escapes '$fullContainer': '$fullPath'."
    }

    $cursor = $fullPath
    while ($cursor -and $cursor.StartsWith($fullContainer, [System.StringComparison]::OrdinalIgnoreCase)) {
        if (Test-Path -LiteralPath $cursor) {
            if ((Get-Item -LiteralPath $cursor -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
                throw "$Label traverses a reparse point: '$cursor'."
            }
        }
        if ($cursor -eq $fullContainer) {
            break
        }
        $cursor = Split-Path -Parent $cursor
    }

    return $fullPath
}

$allowedKeys = @{
    project    = @('name', 'sdk_version', 'game_root')
    entrypoint = @(
        'file_path', 'out_directory_path', 'includes',
        'skip_lr', 'skip_msr', 'ctr_as_local', 'xer_as_local', 'reserved_as_local',
        'cr_as_local', 'non_argument_as_local', 'non_volatile_as_local',
        'generate_exception_handlers'
    )
}
$values = @{}
$section = $null
$seenSections = @{}
$lineNumber = 0

foreach ($rawLine in Get-Content -LiteralPath $resolvedManifest) {
    $lineNumber++
    $line = $rawLine.Trim()
    if (-not $line -or $line.StartsWith('#')) {
        continue
    }
    if ($line -match '^\[([a-z_]+)\]$') {
        $section = $Matches[1]
        if (-not $allowedKeys.ContainsKey($section)) {
            throw "Unknown manifest section [$section] at line $lineNumber."
        }
        if ($seenSections.ContainsKey($section)) {
            throw "Duplicate manifest section [$section] at line $lineNumber."
        }
        $seenSections[$section] = $true
        continue
    }
    if (-not $section) {
        throw "Manifest value appears before a section at line $lineNumber."
    }
    if ($line -notmatch '^([a-z_]+)\s*=\s*(?:"([^"]*)"|(\[\])|(true|false))$') {
        throw "Unsupported TOML syntax at line $lineNumber."
    }
    $key = $Matches[1]
    if ($key -notin $allowedKeys[$section]) {
        throw "Unknown key '$key' in [$section] at line $lineNumber."
    }
    $qualifiedKey = "$section.$key"
    if ($values.ContainsKey($qualifiedKey)) {
        throw "Duplicate key '$qualifiedKey' at line $lineNumber."
    }
    $values[$qualifiedKey] = if ($Matches[3]) { '[]' } elseif ($Matches[4]) { $Matches[4] } else { $Matches[2] }
}

$expected = [ordered]@{
    'project.name'                 = 'mcla'
    'project.sdk_version'          = '0.9.0'
    'project.game_root'            = 'private/game'
    'entrypoint.file_path'         = 'private/game/default.xex'
    'entrypoint.out_directory_path'= 'generated/default'
    'entrypoint.includes'          = '[]'
    'entrypoint.skip_lr'           = 'false'
    'entrypoint.skip_msr'          = 'false'
    'entrypoint.ctr_as_local'      = 'false'
    'entrypoint.xer_as_local'      = 'false'
    'entrypoint.reserved_as_local' = 'false'
    'entrypoint.cr_as_local'       = 'false'
    'entrypoint.non_argument_as_local' = 'false'
    'entrypoint.non_volatile_as_local' = 'false'
    'entrypoint.generate_exception_handlers' = 'false'
}
foreach ($entry in $expected.GetEnumerator()) {
    if (-not $values.ContainsKey($entry.Key)) {
        throw "Required manifest key is missing: '$($entry.Key)'."
    }
}
if ($values.Count -ne $expected.Count) {
    throw "Manifest contains unexpected values: parsed $($values.Count), expected $($expected.Count)."
}

Assert-SafeRelativePath -Value $values['project.game_root'] -Label 'project.game_root'
Assert-SafeRelativePath -Value $values['entrypoint.file_path'] -Label 'entrypoint.file_path'
Assert-SafeRelativePath -Value $values['entrypoint.out_directory_path'] -Label 'entrypoint.out_directory_path'

$privateRoot = Join-Path $repoRoot 'private'
$generatedRoot = Join-Path $repoRoot 'generated'
$gameRoot = Assert-ContainedPath -Path (Join-Path $repoRoot $values['project.game_root']) -Container $privateRoot -Label 'Game root'
$xexPath = Assert-ContainedPath -Path (Join-Path $repoRoot $values['entrypoint.file_path']) -Container $privateRoot -Label 'Entrypoint XEX'
$outputPath = Assert-ContainedPath -Path (Join-Path $repoRoot $values['entrypoint.out_directory_path']) -Container $generatedRoot -Label 'Generated output'

if (-not (Test-Path -LiteralPath $gameRoot -PathType Container)) {
    throw "Configured game root does not exist: '$gameRoot'."
}
if (-not (Test-Path -LiteralPath $xexPath -PathType Leaf)) {
    throw "Configured entrypoint XEX does not exist: '$xexPath'."
}
$xexItem = Get-Item -LiteralPath $xexPath
$xexHash = (Get-FileHash -LiteralPath $xexPath -Algorithm SHA256).Hash
if ($xexItem.Length -ne 9252864) {
    throw "Configured XEX size mismatch. Expected 9252864, got $($xexItem.Length)."
}
if ($xexHash -ne 'C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432') {
    throw "Configured XEX SHA-256 mismatch."
}
foreach ($entry in $expected.GetEnumerator()) {
    if ($values[$entry.Key] -ne $entry.Value) {
        throw "Manifest value mismatch for '$($entry.Key)'. Expected '$($entry.Value)', got '$($values[$entry.Key])'."
    }
}

[pscustomobject]@{
    Validated          = $true
    ProjectName        = $values['project.name']
    SdkVersion         = $values['project.sdk_version']
    GameRoot           = $values['project.game_root']
    Entrypoint         = $values['entrypoint.file_path']
    OutputDirectory    = $values['entrypoint.out_directory_path']
    IncludesCount      = 0
    OptimizationFlagsDisabled = 9
    EntrypointSize     = $xexItem.Length
    EntrypointSha256   = $xexHash
}
