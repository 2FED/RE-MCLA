[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$TranscriptPath,
    [Parameter(Mandatory)][string]$RuntimeLogPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$startupImports = @(
    'ExGetXConfigSetting',
    'HalReturnToFirmware',
    'KeBugCheckEx',
    'KeGetCurrentProcessType',
    'KeTlsAlloc',
    'KeTlsFree',
    'KeTlsGetValue',
    'KeTlsSetValue',
    'NtAllocateVirtualMemory',
    'NtClose',
    'NtCreateEvent',
    'NtFreeVirtualMemory',
    'NtQueryVirtualMemory',
    'NtWaitForSingleObjectEx',
    'RtlCompareMemoryUlong',
    'RtlEnterCriticalSection',
    'RtlImageXexHeaderField',
    'RtlInitializeCriticalSection',
    'RtlLeaveCriticalSection',
    'RtlNtStatusToDosError',
    'RtlRaiseException',
    'XexCheckExecutablePrivilege'
)
$expectedReached = @(
    'ExGetXConfigSetting',
    'KeGetCurrentProcessType',
    'KeTlsAlloc',
    'KeTlsSetValue',
    'NtAllocateVirtualMemory',
    'RtlEnterCriticalSection',
    'RtlImageXexHeaderField',
    'RtlInitializeCriticalSection',
    'RtlLeaveCriticalSection',
    'XexCheckExecutablePrivilege'
)
$variableImports = @(
    'ExThreadObjectType',
    'KeDebugMonitorData',
    'KeTimeStampBundle',
    'XboxHardwareInfo',
    'XboxKrnlVersion',
    'XexExecutableModuleHandle',
    'ExLoadedCommandLine',
    'VdGlobalDevice',
    'VdGpuClockInMHz',
    'VdHSIOCalibrationLock',
    'KeCertMonitorData'
)

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

$hitMatches = [regex]::Matches($transcript, '(?m)^MCLA_XBOXKRNL_IMPORT ([A-Za-z0-9_]+)\r?$')
$hits = @($hitMatches | ForEach-Object { $_.Groups[1].Value })
$uniqueHits = @($hits | Sort-Object -Unique)
$unknownHits = @($uniqueHits | Where-Object { $_ -notin $startupImports })
if ($unknownHits.Count -ne 0) {
    throw "Trace contains imports outside the reviewed startup set: $($unknownHits -join ', ')."
}

$missingReached = @($expectedReached | Where-Object { $_ -notin $uniqueHits })
if ($missingReached.Count -ne 0) {
    throw "Required stock-path imports were not reached: $($missingReached -join ', ')."
}
$unexpectedReached = @($uniqueHits | Where-Object { $_ -notin $expectedReached })
if ($unexpectedReached.Count -ne 0) {
    throw "The pre-main import path changed: $($unexpectedReached -join ', ')."
}

foreach ($variableImport in $variableImports) {
    $escaped = [regex]::Escape($variableImport)
    $count = [regex]::Matches(
        $runtimeLog,
        "(?m)Patched variable import xboxkrnl:[^\r\n]+\($escaped\) -> 0x[0-9a-fA-F]+"
    ).Count
    if ($count -ne 1) {
        throw "Expected one load-time patch for '$variableImport'; found $count."
    }
}
if ($runtimeLog -match '(?i)\[FATAL\]|invalid or unregistered function|PPC_UNIMPLEMENTED') {
    throw 'Runtime log contains a fatal startup or dispatch failure before title main.'
}

$matrix = foreach ($import in $startupImports) {
    $count = @($hits | Where-Object { $_ -eq $import }).Count
    [pscustomobject]@{
        Import = $import
        Classification = if ($import -in $expectedReached) { 'stock-path reached' } else { 'conditional/error-path not reached' }
        Hits = $count
    }
}

[pscustomobject]@{
    Passed = $true
    BoundaryCount = $boundaryCount
    VariableImportsPatched = $variableImports.Count
    FunctionImportsReviewed = $startupImports.Count
    FunctionImportsReached = $expectedReached.Count
    TotalFunctionHits = $hits.Count
    Matrix = $matrix
}
