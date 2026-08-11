[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifierPath = Join-Path $PSScriptRoot 'verify-xboxkrnl-startup-import-trace.ps1'
$fixtureRoot = Join-Path $repoRoot 'private/evidence/M3-005/verifier-fixtures'
[System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
$positiveTranscript = Join-Path $fixtureRoot 'positive-cdb.txt'
$positiveRuntime = Join-Path $fixtureRoot 'positive-runtime.log'

$reached = @(
    'ExGetXConfigSetting', 'KeGetCurrentProcessType', 'KeTlsAlloc', 'KeTlsSetValue',
    'NtAllocateVirtualMemory', 'RtlEnterCriticalSection', 'RtlImageXexHeaderField',
    'RtlInitializeCriticalSection', 'RtlLeaveCriticalSection', 'XexCheckExecutablePrivilege'
)
$variables = @(
    'ExThreadObjectType', 'KeDebugMonitorData', 'KeTimeStampBundle', 'XboxHardwareInfo',
    'XboxKrnlVersion', 'XexExecutableModuleHandle', 'ExLoadedCommandLine', 'VdGlobalDevice',
    'VdGpuClockInMHz', 'VdHSIOCalibrationLock', 'KeCertMonitorData'
)
[System.IO.File]::WriteAllLines(
    $positiveTranscript,
    @(($reached | ForEach-Object { "MCLA_XBOXKRNL_IMPORT $_" }) + 'MCLA_BOUNDARY title-main'),
    [System.Text.UTF8Encoding]::new($false)
)
[System.IO.File]::WriteAllLines(
    $positiveRuntime,
    @($variables | ForEach-Object { 'Patched variable import xboxkrnl:0x1 ({0}) -> 0x30000000' -f $_ }),
    [System.Text.UTF8Encoding]::new($false)
)

$positive = & $verifierPath -TranscriptPath $positiveTranscript -RuntimeLogPath $positiveRuntime
if (-not $positive.Passed -or $positive.FunctionImportsReached -ne 10 -or
    $positive.VariableImportsPatched -ne 11) {
    throw 'Positive startup-import verifier fixture did not pass with the expected counts.'
}

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Mutation)
    $transcript = Join-Path $fixtureRoot "$Name-cdb.txt"
    $runtime = Join-Path $fixtureRoot "$Name-runtime.log"
    [System.IO.File]::Copy($positiveTranscript, $transcript, $true)
    [System.IO.File]::Copy($positiveRuntime, $runtime, $true)
    & $Mutation $transcript $runtime
    try {
        & $verifierPath -TranscriptPath $transcript -RuntimeLogPath $runtime | Out-Null
    } catch {
        return
    }
    throw "Negative verifier fixture '$Name' was accepted."
}

Assert-Rejected -Name 'missing-boundary' -Mutation {
    param($transcript, $runtime)
    (Get-Content -LiteralPath $transcript) |
        Where-Object { $_ -ne 'MCLA_BOUNDARY title-main' } |
        Set-Content -LiteralPath $transcript -Encoding utf8
}
Assert-Rejected -Name 'missing-function' -Mutation {
    param($transcript, $runtime)
    (Get-Content -LiteralPath $transcript) |
        Where-Object { $_ -ne 'MCLA_XBOXKRNL_IMPORT KeTlsAlloc' } |
        Set-Content -LiteralPath $transcript -Encoding utf8
}
Assert-Rejected -Name 'missing-variable' -Mutation {
    param($transcript, $runtime)
    (Get-Content -LiteralPath $runtime) |
        Where-Object { $_ -notmatch '\(KeCertMonitorData\)' } |
        Set-Content -LiteralPath $runtime -Encoding utf8
}
Assert-Rejected -Name 'fatal-runtime' -Mutation {
    param($transcript, $runtime)
    Add-Content -LiteralPath $runtime -Value '[FATAL] invalid or unregistered function'
}

[pscustomobject]@{ Passed = $true; Cases = 5 }
