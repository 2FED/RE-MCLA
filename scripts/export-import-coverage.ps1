[CmdletBinding()]
param(
    [string]$XeniaLogPath,
    [string]$GeneratedRoot,
    [string]$GeneratedManifestPath,
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$evidenceRoot = Join-Path $repoRoot 'docs/evidence'
if (-not $XeniaLogPath) { $XeniaLogPath = Join-Path $repoRoot 'private/baseline/M2-002/xenia-stock.snapshot.log' }
if (-not $GeneratedRoot) { $GeneratedRoot = Join-Path $repoRoot 'generated/default' }
if (-not $GeneratedManifestPath) { $GeneratedManifestPath = Join-Path $repoRoot 'private/evidence/M2-012/10-final-clean-b/generated-manifest.json' }
if (-not $OutputPath) { $OutputPath = Join-Path $evidenceRoot 'M2-013-import-coverage.md' }

function Resolve-RegularInput {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Required input was not found: '$Path'." }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if ((Get-Item -LiteralPath $resolved -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Input must not be a reparse point: '$resolved'."
    }
    return $resolved
}

$resolvedLog = Resolve-RegularInput $XeniaLogPath
$resolvedGeneratedManifest = Resolve-RegularInput $GeneratedManifestPath
if (-not (Test-Path -LiteralPath $GeneratedRoot -PathType Container)) {
    throw "Generated root was not found: '$GeneratedRoot'."
}
$resolvedGeneratedRoot = (Resolve-Path -LiteralPath $GeneratedRoot).Path
if ((Get-Item -LiteralPath $resolvedGeneratedRoot -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
    throw 'Generated root must not be a reparse point.'
}

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
$resolvedEvidenceRoot = [System.IO.Path]::GetFullPath($evidenceRoot).TrimEnd('\')
if (-not $resolvedOutput.StartsWith("$resolvedEvidenceRoot\", [System.StringComparison]::OrdinalIgnoreCase) -or
    [System.IO.Path]::GetExtension($resolvedOutput) -ne '.md') {
    throw "Output must be a Markdown file below '$resolvedEvidenceRoot'."
}
$outputParent = Split-Path -Parent $resolvedOutput
if (-not (Test-Path -LiteralPath $outputParent -PathType Container) -or
    ((Get-Item -LiteralPath $outputParent -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
    throw 'Output parent must be an existing non-reparse directory.'
}

$logHash = (Get-FileHash -LiteralPath $resolvedLog -Algorithm SHA256).Hash
$generatedManifestHash = (Get-FileHash -LiteralPath $resolvedGeneratedManifest -Algorithm SHA256).Hash
if ($logHash -ne '95573DE737058A8E9A71B776A6E0A3851379FB26AF1E28A160FFA0B037EE3DE0') {
    throw 'Xenia import audit is not the immutable M2-002 snapshot.'
}
if ($generatedManifestHash -ne 'F8A97618215848CA3B2279725DE77D8AF556E6B2E02AF1CA66D51A812E60F933') {
    throw 'Generated manifest is not the accepted M2-012 clean snapshot.'
}

$generatedManifest = Get-Content -LiteralPath $resolvedGeneratedManifest -Raw | ConvertFrom-Json
if ($generatedManifest.schema -ne 1 -or $generatedManifest.file_count -ne 64 -or
    $generatedManifest.total_bytes -ne 128031984 -or @($generatedManifest.files).Count -ne 64) {
    throw 'Generated manifest aggregate changed.'
}
foreach ($entry in @($generatedManifest.files)) {
    $segments = @($entry.path.Split('/'))
    if ([string]::IsNullOrWhiteSpace($entry.path) -or $entry.path.Contains('\') -or
        [System.IO.Path]::IsPathRooted($entry.path) -or $segments -contains '' -or
        $segments -contains '.' -or $segments -contains '..') {
        throw "Unsafe generated-manifest path: '$($entry.path)'."
    }
    $path = Join-Path $resolvedGeneratedRoot $entry.path
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
        (Get-Item -LiteralPath $path).Length -ne $entry.bytes -or
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw "Generated file does not match the accepted M2-012 snapshot: '$($entry.path)'."
    }
}

$logText = Get-Content -LiteralPath $resolvedLog -Raw
$recordPattern = '(?m)^\s{3}([FV])\s+([0-9A-F]{8})(?:\s+([0-9A-F]{8}))?\s+([0-9A-F]{3})\s+\(\s*(\d+)\)\s+(!!\s+)?([A-Za-z0-9_]+)\s*$'
$matches = @([regex]::Matches($logText, $recordPattern))
if ($matches.Count -ne 257) { throw "Expected 257 unique import rows, found $($matches.Count)." }
$xamStart = [Convert]::ToUInt32('82000600', 16)
$xamEnd = [Convert]::ToUInt32('82000778', 16)
$kernelStart = [Convert]::ToUInt32('82000780', 16)
$kernelEnd = [Convert]::ToUInt32('82000A04', 16)

$records = foreach ($match in $matches) {
    $iat = [Convert]::ToUInt32($match.Groups[2].Value, 16)
    $library = if ($iat -ge $xamStart -and $iat -le $xamEnd) { 'xam.xex' }
        elseif ($iat -ge $kernelStart -and $iat -le $kernelEnd) { 'xboxkrnl.exe' }
        else { throw "Import IAT address is outside both reviewed ranges: 0x$($match.Groups[2].Value)." }
    $ordinal = [Convert]::ToUInt32($match.Groups[4].Value, 16)
    if ($ordinal -ne [uint32]$match.Groups[5].Value) { throw "Ordinal formats disagree for $($match.Groups[7].Value)." }
    [pscustomobject]@{
        Library = $library
        Type = $match.Groups[1].Value
        Iat = $match.Groups[2].Value
        Thunk = $match.Groups[3].Value
        OrdinalHex = $match.Groups[4].Value
        Ordinal = $ordinal
        Implemented = -not $match.Groups[6].Success
        Name = $match.Groups[7].Value
    }
}

$duplicates = @($records | Group-Object Library,Ordinal | Where-Object Count -ne 1)
$duplicateIat = @($records | Group-Object Iat | Where-Object Count -ne 1)
$duplicateThunks = @($records | Where-Object Type -eq 'F' | Group-Object Thunk | Where-Object Count -ne 1)
$duplicateFunctionNames = @($records | Where-Object Type -eq 'F' | Group-Object Name | Where-Object Count -ne 1)
if ($duplicates.Count -or $duplicateIat.Count -or $duplicateThunks.Count -or $duplicateFunctionNames.Count) {
    throw 'Duplicate import identity, IAT slot, function thunk, or function name found.'
}
$xam = @($records | Where-Object Library -eq 'xam.xex')
$kernel = @($records | Where-Object Library -eq 'xboxkrnl.exe')
$functions = @($records | Where-Object Type -eq 'F')
$variables = @($records | Where-Object Type -eq 'V')
if ($xam.Count -ne 95 -or $kernel.Count -ne 162 -or $functions.Count -ne 246 -or $variables.Count -ne 11 -or
    @($xam | Where-Object { -not $_.Implemented }).Count -ne 9 -or
    @($kernel | Where-Object { -not $_.Implemented }).Count -ne 13 -or
    (2 * @($xam | Where-Object Type -eq 'F').Count + @($xam | Where-Object Type -eq 'V').Count) -ne 190 -or
    (2 * @($kernel | Where-Object Type -eq 'F').Count + @($kernel | Where-Object Type -eq 'V').Count) -ne 313) {
    throw 'Import class, implementation, or XEX-record counts changed.'
}

$initPath = Join-Path $resolvedGeneratedRoot 'mcla_init.cpp'
$initRegistrations = @{}
foreach ($line in [System.IO.File]::ReadLines($initPath)) {
    if ($line -match '^\s*\{ 0x([0-9A-F]{8}), __imp__([A-Za-z0-9_]+) \},$') {
        if ($initRegistrations.ContainsKey($Matches[1])) { throw "Duplicate import thunk registration: 0x$($Matches[1])." }
        $initRegistrations[$Matches[1]] = $Matches[2]
    }
}
if ($initRegistrations.Count -ne 246) { throw "Expected 246 generated function imports, found $($initRegistrations.Count)." }
foreach ($record in $functions) {
    if (-not $initRegistrations.ContainsKey($record.Thunk) -or $initRegistrations[$record.Thunk] -cne $record.Name) {
        throw "Generated thunk mapping changed for $($record.Library) ordinal $($record.Ordinal)."
    }
}

$callCounts = @{}
foreach ($file in Get-ChildItem -LiteralPath $resolvedGeneratedRoot -Filter 'mcla_recomp.*.cpp' -File) {
    foreach ($line in [System.IO.File]::ReadLines($file.FullName)) {
        if ($line -match '^\s*__imp__([A-Za-z0-9_]+)\(ctx, base\);$') {
            $name = $Matches[1]
            $callCounts[$name] = 1 + [int]$callCounts[$name]
        }
    }
}
$knownNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($record in $functions) { $knownNames.Add($record.Name) | Out-Null }
foreach ($name in $callCounts.Keys) {
    if (-not $knownNames.Contains($name)) { throw "Generated call references unknown import '$name'." }
}
$directFunctions = @($functions | Where-Object { $callCounts.ContainsKey($_.Name) })
$indirectFunctions = @($functions | Where-Object { -not $callCounts.ContainsKey($_.Name) })
$totalCallSites = [int](($callCounts.Values | Measure-Object -Sum).Sum)
$expectedIndirect = @('__C_specific_handler','IoInvalidDeviceRequest','NtQueryDirectoryFile','NtReadFileScatter','StfsControlDevice','StfsCreateDevice')
if ($directFunctions.Count -ne 240 -or $indirectFunctions.Count -ne 6 -or $totalCallSites -ne 1517 -or
    @(Compare-Object ($indirectFunctions.Name | Sort-Object) $expectedIndirect).Count -ne 0) {
    throw 'Static import call-site coverage changed.'
}

$rows = foreach ($record in $records) {
    $status = if ($record.Implemented) { 'implemented' } else { 'unimplemented' }
    $thunk = if ($record.Type -eq 'F') { "``0x$($record.Thunk)``" } else { '-' }
    $reachability = if ($record.Type -eq 'V') { 'data import (no call site)' }
        elseif ($callCounts.ContainsKey($record.Name)) { "direct ($($callCounts[$record.Name]) sites)" }
        else { 'indirect-only (0 direct sites)' }
    "| ``$($record.Library)`` | $($record.Type) | ``$($record.Ordinal) / 0x$($record.OrdinalHex)`` | ``$($record.Name)`` | ``0x$($record.Iat)`` | $thunk | $status | $reachability |"
}

$report = @(
    '# M2-013 import symbol and coverage matrix', '', 'Date: 2026-08-11',
    'Result: ALL XAM/XBOXKRNL IMPORT RECORDS SYMBOLICALLY MAPPED', '',
    '## Evidence identity', '',
    "- Private Xenia audit SHA-256: ``$logHash``",
    "- Accepted generated-manifest SHA-256: ``$generatedManifestHash``",
    '- Source XEX: verified Complete Edition dump; requested/minimum import version `2.0.7371.0`', '',
    '## Coverage summary', '',
    '| Library | XEX records | Unique symbols | Functions | Variables | Symbol known | Xenia implemented | Direct generated calls |',
    '| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |',
    "| ``xam.xex`` | 190 | 95 | 95 | 0 | 95/95 | $(@($xam | Where-Object Implemented).Count)/95 | $(@($directFunctions | Where-Object Library -eq 'xam.xex').Count)/95 |",
    "| ``xboxkrnl.exe`` | 313 | 162 | 151 | 11 | 162/162 | $(@($kernel | Where-Object Implemented).Count)/162 | $(@($directFunctions | Where-Object Library -eq 'xboxkrnl.exe').Count)/151 |",
    "| **Total** | **503** | **257** | **246** | **11** | **257/257** | **235/257** | **240/246** |", '',
    "The generated corpus contains **$totalCallSites** direct import call sites. Six function imports have zero direct generated calls and are explicitly classified as indirect-only; eleven variable imports have no callable thunk by design. This is static coverage, not an assertion that every direct caller executes during startup. M2-014 owns entry-point startup reachability and the minimum stub set.", '',
    '## Full matrix', '',
    '| Library | Type | Ordinal dec/hex | Symbol | IAT slot | Function thunk | Xenia status | Static reachability |',
    '| --- | :---: | --- | --- | --- | --- | --- | --- |'
) + $rows + @('',
    '## Gate result', '',
    '- Unknown library/ordinal symbols: 0',
    '- Unmapped XEX import records: 0',
    '- Generated function thunk mismatches: 0',
    '- Unclassified static reachability entries: 0',
    '- Xenia-unimplemented symbols retained for M2-014/M3 ownership: 22', '',
    'M2-013 acceptance: PASS. Every unique import maps to a symbolic export and an explicit static reachability class.'
)

[System.IO.File]::WriteAllText($resolvedOutput, (($report -join [Environment]::NewLine) + [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))
[pscustomobject]@{
    Exported=$true
    XexRecords=503
    UniqueImports=$records.Count
    Functions=$functions.Count
    Variables=$variables.Count
    UnknownSymbols=0
    DirectFunctions=$directFunctions.Count
    IndirectFunctions=$indirectFunctions.Count
    DirectCallSites=$totalCallSites
    XeniaUnimplemented=@($records | Where-Object { -not $_.Implemented }).Count
    OutputPath=$resolvedOutput
    OutputSha256=(Get-FileHash -LiteralPath $resolvedOutput -Algorithm SHA256).Hash
}
