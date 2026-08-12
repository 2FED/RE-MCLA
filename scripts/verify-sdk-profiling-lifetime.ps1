[CmdletBinding()]
param(
    [string]$SdkRoot = 'third_party/rexglue-sdk'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$candidate = if ([System.IO.Path]::IsPathRooted($SdkRoot)) {
    $SdkRoot
} else {
    Join-Path $repoRoot $SdkRoot
}
$sdkRootPath = (Resolve-Path -LiteralPath $candidate).Path
$debugHeader = Get-Content -LiteralPath (Join-Path $sdkRootPath 'include/rex/dbg.h') -Raw
$hookHeader = Get-Content -LiteralPath (Join-Path $sdkRootPath 'include/rex/hook.h') -Raw
$normalizedDebugHeader = $debugHeader -replace '\\\r?\n', ''
$normalizedHookHeader = $hookHeader -replace '\\\r?\n', ''

if ($debugHeader -match 'ZoneNamedN\s*\([^\r\n]*,\s*true\s*\)' -or
    $hookHeader -match 'ZoneNamedN\s*\([^\r\n]*,\s*true\s*\)') {
    throw 'Profiled CPU or hook zones must not activate before Tracy manual lifetime starts.'
}

$requiredDebugPatterns = @(
    'ZoneNamedN\s*\(\s*___tracy_cpu_zone\s*,\s*name\s*,\s*TracyIsStarted\s*\)',
    'ZoneNamedN\s*\(\s*___tracy_cpu_zone_i\s*,\s*name\s*,\s*TracyIsStarted\s*\)',
    '(?s)if\s*\(\s*TracyIsStarted\s*\)\s*\{\s*tracy::SetThreadName\s*\(\s*name\s*\)',
    '(?s)if\s*\(\s*TracyIsStarted\s*\)\s*\{\s*TracyFiberEnter\s*\(\s*name\s*\)',
    '(?s)if\s*\(\s*TracyIsStarted\s*\)\s*\{\s*TracyFiberLeave',
    '(?s)if\s*\(\s*TracyIsStarted\s*\)\s*\{\s*TracyPlot\s*\('
)
foreach ($pattern in $requiredDebugPatterns) {
    if ($normalizedDebugHeader -notmatch $pattern) {
        throw "Tracy manual-lifetime guard is missing from dbg.h: $pattern"
    }
}
if ($normalizedHookHeader -notmatch
    'ZoneNamedN\s*\(\s*___tracy_hook_zone\s*,\s*#subroutine\s*,\s*TracyIsStarted\s*\)') {
    throw 'REX_HOOK does not guard its Tracy zone with TracyIsStarted.'
}

[pscustomobject]@{
    Passed = $true
    GuardedCpuZones = 2
    GuardedHookZones = 1
    GuardedAuxiliaryMacros = 4
}
