[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$header = Get-Content -LiteralPath (Join-Path $repoRoot 'src/mcla_app.h') -Raw
$implementation = Get-Content -LiteralPath (Join-Path $repoRoot 'src/mcla_app.cpp') -Raw
$main = Get-Content -LiteralPath (Join-Path $repoRoot 'src/main.cpp') -Raw
$sdkApp = Get-Content -LiteralPath (Join-Path $repoRoot 'third_party/rexglue-sdk/src/ui/rex_app.cpp') -Raw

$requiredHeaderPatterns = @(
    'class\s+MclaApp\s*:\s*public\s+rex::ReXApp',
    'OnPostInitLogging\s*\(\s*\)\s*override',
    'OnFinalizePaths\s*\(',
    'OnShutdown\s*\(\s*\)\s*override'
)
$requiredImplementationPatterns = @(
    'REXCVAR_DEFINE_BOOL\s*\(\s*mcla_lifecycle_probe',
    'MCLA lifecycle: logging ready',
    'MCLA lifecycle: probe requested; guest runtime skipped',
    'MCLA lifecycle: probe complete',
    'MCLA lifecycle: shutdown',
    'CallInUIThreadDeferred',
    'return\s+std::nullopt'
)

foreach ($pattern in $requiredHeaderPatterns) {
    if ($header -notmatch $pattern) { throw "MclaApp lifecycle declaration is missing: $pattern" }
}
foreach ($pattern in $requiredImplementationPatterns) {
    if ($implementation -notmatch $pattern) { throw "MclaApp lifecycle implementation is missing: $pattern" }
}
if ($main -notmatch 'REX_DEFINE_APP\s*\(\s*mcla\s*,\s*MclaApp::Create\s*\)') {
    throw 'The native entry point is not registered with MclaApp::Create.'
}
if ($header -match 'generated/default' -or $main -match 'generated/default') {
    throw 'Generated image declarations must remain isolated in mcla_app.cpp.'
}
$runtimeReset = $sdkApp.LastIndexOf('runtime_.reset();', [System.StringComparison]::Ordinal)
$windowReset = $sdkApp.LastIndexOf('window_.reset();', [System.StringComparison]::Ordinal)
if ($runtimeReset -lt 0 -or $windowReset -lt 0 -or $runtimeReset -gt $windowReset) {
    throw 'ReXApp must destroy Runtime/input drivers before their attached Window.'
}

[pscustomobject]@{
    Passed = $true
    AppSubclass = 'MclaApp'
    LifecycleHooks = 3
    ProbeMarkers = 4
}
