[CmdletBinding()]
param(
    [string]$HeaderPath,
    [string]$ImplementationPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $HeaderPath) {
    $HeaderPath = Join-Path $repoRoot 'generated/default/mcla_init.h'
}
if (-not $ImplementationPath) {
    $ImplementationPath = Join-Path $repoRoot 'generated/default/mcla_init.cpp'
}
foreach ($path in @($HeaderPath, $ImplementationPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Generated module-config input was not found: '$path'."
    }
}

$header = Get-Content -LiteralPath $HeaderPath -Raw
$implementation = Get-Content -LiteralPath $ImplementationPath -Raw
$expected = [ordered]@{
    REX_IMAGE_BASE = [Convert]::ToUInt64('82000000', 16)
    REX_IMAGE_SIZE = [Convert]::ToUInt64('009E0000', 16)
    REX_CODE_BASE  = [Convert]::ToUInt64('82130000', 16)
    REX_CODE_SIZE  = [Convert]::ToUInt64('0069D054', 16)
}

$actual = @{}
foreach ($name in $expected.Keys) {
    $match = [regex]::Match(
        $header,
        "(?m)^#define\s+$name\s+0x(?<hex>[0-9A-Fa-f]+)(?:ull)?\s*$"
    )
    if (-not $match.Success) {
        throw "Generated header is missing exact macro '$name'."
    }
    $value = [Convert]::ToUInt64($match.Groups['hex'].Value, 16)
    if ($value -ne $expected[$name]) {
        throw "Generated macro '$name' is 0x$($value.ToString('X')), expected 0x$($expected[$name].ToString('X'))."
    }
    $actual[$name] = $value
}

$imageEnd = $actual.REX_IMAGE_BASE + $actual.REX_IMAGE_SIZE
$codeEnd = $actual.REX_CODE_BASE + $actual.REX_CODE_SIZE
$dispatchBase = $imageEnd
$dispatchEnd = $dispatchBase + ($actual.REX_CODE_SIZE + [uint64]0x10000) * 2
if ($actual.REX_CODE_BASE -lt $actual.REX_IMAGE_BASE -or $codeEnd -gt $imageEnd -or
    $dispatchEnd -gt ([uint64][uint32]::MaxValue + 1)) {
    throw 'Generated image, code, or dispatch-table ranges are invalid.'
}

$mappingPattern = '(?m)^\s*\{\s*(?<guest>0x[0-9A-Fa-f]+|0)\s*,\s*(?<host>[A-Za-z_][A-Za-z0-9_]*|nullptr)\s*\}\s*,?\s*$'
$mappingMatches = [regex]::Matches($implementation, $mappingPattern)
if ($mappingMatches.Count -lt 2) {
    throw 'Generated function mapping table is missing or empty.'
}

$entryPoint = [Convert]::ToUInt64('821322B8', 16)
$entryCount = 0
$previous = [uint64]0
$sentinelCount = 0
$functionCount = 0
for ($index = 0; $index -lt $mappingMatches.Count; $index++) {
    $match = $mappingMatches[$index]
    $guestText = $match.Groups['guest'].Value
    $hostSymbol = $match.Groups['host'].Value
    $guest = if ($guestText -eq '0') { [uint64]0 } else {
        [Convert]::ToUInt64($guestText.Substring(2), 16)
    }
    if ($guest -eq 0) {
        if ($hostSymbol -ne 'nullptr' -or $index -ne $mappingMatches.Count - 1) {
            throw 'Function mapping sentinel must be the final { 0, nullptr } entry.'
        }
        $sentinelCount++
        continue
    }
    if ($hostSymbol -eq 'nullptr' -or $guest -lt $actual.REX_CODE_BASE -or $guest -ge $codeEnd -or
        ($guest % 4) -ne 0 -or ($functionCount -gt 0 -and $guest -le $previous)) {
        throw "Invalid or unordered function mapping at sanitized index $index."
    }
    $previous = $guest
    $functionCount++
    if ($guest -eq $entryPoint) { $entryCount++ }
}

if ($sentinelCount -ne 1 -or $entryCount -ne 1) {
    throw "Expected one mapping sentinel and one executable-entry mapping; got $sentinelCount and $entryCount."
}

[pscustomobject]@{
    Validated = $true
    ImageRange = '82000000-829E0000'
    CodeRange = '82130000-827CD054'
    DispatchRange = '{0:X8}-{1:X8}' -f $dispatchBase, $dispatchEnd
    EntryPoint = '821322B8'
    FunctionMappings = $functionCount
}
