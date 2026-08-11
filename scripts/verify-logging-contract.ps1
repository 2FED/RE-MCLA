[CmdletBinding()]
param([string]$ProjectRoot = '.')

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$candidate = if ([System.IO.Path]::IsPathRooted($ProjectRoot)) {
    $ProjectRoot
} else {
    Join-Path $repoRoot $ProjectRoot
}
$projectRootPath = (Resolve-Path -LiteralPath $candidate).Path
$repoPrefix = $repoRoot.TrimEnd('\') + '\'
if ($projectRootPath -ne $repoRoot -and
    -not $projectRootPath.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Project root must stay inside the repository: '$projectRootPath'."
}

function Read-ContractFile {
    param([Parameter(Mandatory)][string]$RelativePath)
    $path = Join-Path $projectRootPath $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Logging contract file was not found: '$path'."
    }
    return Get-Content -LiteralPath $path -Raw
}

$header = Read-ContractFile 'src/mcla_logging.h'
$implementation = Read-ContractFile 'src/mcla_logging.cpp'
$app = Read-ContractFile 'src/mcla_app.cpp'
$cmake = Read-ContractFile 'CMakeLists.txt'
$categories = @('app', 'ppc', 'kernel', 'xam', 'vfs', 'gpu', 'audio', 'input', 'patches')

foreach ($category in $categories) {
    $registration = 'Register("' + $category + '")'
    if ([regex]::Matches($implementation, [regex]::Escape($registration)).Count -ne 1) {
        throw "Logging contract does not register category '$category' exactly once."
    }
    if (-not $app.Contains("MCLA_DEFINE_LOG_LEVEL_CVAR($category);")) {
        throw "Logging contract is missing the '$category' level override."
    }
    if (-not $app.Contains("REXCVAR_GET(mcla_log_$category)")) {
        throw "Logging contract does not apply the '$category' level override."
    }
}
if (-not $header.Contains('std::span<const CategoryDescriptor> Categories();') -or
    -not $implementation.Contains('MCLA_LOG_SCHEMA schema=1 category={} event=probe')) {
    throw 'Logging contract is missing its bounded category registry or schema marker.'
}
$macroStart = $app.IndexOf('#define MCLA_DEFINE_LOG_LEVEL_CVAR(name)', [System.StringComparison]::Ordinal)
$macroEnd = $app.IndexOf('MCLA_DEFINE_LOG_LEVEL_CVAR(app);', [System.StringComparison]::Ordinal)
if ($macroStart -lt 0 -or $macroEnd -le $macroStart) {
    throw 'Logging level-override definition was not found.'
}
$macroText = $app.Substring($macroStart, $macroEnd - $macroStart)
if (-not $macroText.Contains('mcla_log_##name, "inherit"') -or
    -not $macroText.Contains('.allowed(') -or
    @('inherit', 'trace', 'debug', 'info', 'warn', 'error', 'critical', 'off').Where({
            -not $macroText.Contains('"' + $_ + '"')
        }).Count -ne 0) {
    throw 'Logging overrides must default to inherit and use the reviewed level allow-list.'
}
if (-not $app.Contains('REXCVAR_DEFINE_BOOL(mcla_logging_probe, false') -or
    -not $app.Contains('mcla::logging::EmitSchemaProbe();')) {
    throw 'Logging probe must exist and remain off by default.'
}
if ($app -match '(?m)\bREXLOG_(TRACE|DEBUG|INFO|WARN|ERROR|CRITICAL)\s*\(') {
    throw 'Project application contains an uncategorized generic logging call.'
}
foreach ($macro in @('MCLA_APP_INFO', 'MCLA_PPC_INFO', 'MCLA_VFS_INFO')) {
    if (-not $app.Contains("$macro(")) {
        throw "Project application does not route operational logs through '$macro'."
    }
}
if (-not $cmake.Contains('src/mcla_logging.cpp')) {
    throw 'MCLA logging implementation is not registered in CMake.'
}

[pscustomobject]@{
    Passed = $true
    Schema = 1
    Categories = $categories.Count
    DefaultOverride = 'inherit'
    ProbeDefault = $false
}
