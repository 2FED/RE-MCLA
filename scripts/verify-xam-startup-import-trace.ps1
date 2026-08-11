[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$TranscriptPath,
    [Parameter(Mandatory)][string]$RuntimeLogPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$startupImports = @(
    'XamLoaderTerminateTitle',
    'XamShowMessageBoxUIEx',
    'XGetAVPack',
    'XGetLanguage'
)
$expectedResults = [ordered]@{
    XGetAVPack = 6
}

foreach ($path in @($TranscriptPath, $RuntimeLogPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required trace input was not found: '$path'."
    }
}

$transcript = Get-Content -LiteralPath $TranscriptPath -Raw
$runtimeLog = Get-Content -LiteralPath $RuntimeLogPath -Raw
if ($transcript -match "(?m)^Couldn't resolve error at" -or
    $transcript -match '(?m)^Syntax error at') {
    throw 'CDB could not resolve every requested startup symbol or command.'
}

$boundaryCount = [regex]::Matches($transcript, '(?m)^MCLA_BOUNDARY title-main\r?$').Count
if ($boundaryCount -ne 1) {
    throw "Expected exactly one title-main boundary marker; found $boundaryCount."
}

$hitMatches = [regex]::Matches($transcript, '(?m)^MCLA_XAM_IMPORT ([A-Za-z0-9_]+)\r?$')
$hits = @($hitMatches | ForEach-Object { $_.Groups[1].Value })
$uniqueHits = @($hits | Sort-Object -Unique)
$unknownHits = @($uniqueHits | Where-Object { $_ -notin $startupImports })
if ($unknownHits.Count -ne 0) {
    throw "Trace contains imports outside the reviewed startup set: $($unknownHits -join ', ')."
}

$expectedReached = @($expectedResults.Keys)
$missingReached = @($expectedReached | Where-Object { $_ -notin $uniqueHits })
if ($missingReached.Count -ne 0) {
    throw "Required stock-path imports were not reached: $($missingReached -join ', ')."
}
$unexpectedReached = @($uniqueHits | Where-Object { $_ -notin $expectedReached })
if ($unexpectedReached.Count -ne 0) {
    throw "The pre-main XAM import path changed: $($unexpectedReached -join ', ')."
}

$observedResults = @{}
$resultMatches = [regex]::Matches(
    $transcript,
    '(?m)^MCLA_XAM_RESULT ([A-Za-z0-9_]+) 0x([0-9a-fA-F]+)\r?$'
)
foreach ($match in $resultMatches) {
    $name = $match.Groups[1].Value
    if ($name -notin $expectedReached) {
        throw "Trace contains a result for an unreviewed XAM import: $name."
    }
    if ($observedResults.ContainsKey($name)) {
        throw "Trace contains more than one result for '$name'."
    }
    $observedResults[$name] = [Convert]::ToUInt64($match.Groups[2].Value, 16)
}
foreach ($entry in $expectedResults.GetEnumerator()) {
    if (-not $observedResults.ContainsKey($entry.Key)) {
        throw "Trace is missing the guest return value for '$($entry.Key)'."
    }
    if ($observedResults[$entry.Key] -ne [uint64]$entry.Value) {
        throw "Unexpected '$($entry.Key)' result. Expected $($entry.Value), got $($observedResults[$entry.Key])."
    }
}

if ($runtimeLog -match '(?i)\[FATAL\]|invalid or unregistered function|PPC_UNIMPLEMENTED' -or
    $runtimeLog -match '\[STUB\] XamShowMessageBoxUIEx') {
    throw 'Runtime log contains a fatal startup failure or reached the XAM message-box stub.'
}

$matrix = foreach ($import in $startupImports) {
    $count = @($hits | Where-Object { $_ -eq $import }).Count
    [pscustomobject]@{
        Import = $import
        Classification = if ($import -in $expectedReached) {
            'stock-path reached'
        } else {
            'conditional/post-main not reached'
        }
        Hits = $count
        Result = if ($observedResults.ContainsKey($import)) { $observedResults[$import] } else { $null }
    }
}

[pscustomobject]@{
    Passed = $true
    BoundaryCount = $boundaryCount
    FunctionImportsReviewed = $startupImports.Count
    FunctionImportsReached = $expectedReached.Count
    TotalFunctionHits = $hits.Count
    Matrix = $matrix
}
