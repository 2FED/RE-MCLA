[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$debuggerPath = 'C:\Program Files\Windows Kits\10\Debuggers\x64\cdb.exe'
if (-not (Test-Path -LiteralPath $debuggerPath -PathType Leaf)) {
    throw "CDB was not found: '$debuggerPath'."
}

function Resolve-ContainedPath {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Description)
    $resolved = if ([System.IO.Path]::IsPathRooted($Path)) {
        (Resolve-Path -LiteralPath $Path).Path
    } else {
        (Resolve-Path -LiteralPath (Join-Path $repoRoot $Path)).Path
    }
    $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay inside the repository: '$resolved'."
    }
    return $resolved
}

$buildRootPath = Resolve-ContainedPath -Path $BuildRoot -Description 'Build root'
$gameRootPath = Resolve-ContainedPath -Path $GameRoot -Description 'Game root'
$executablePath = Join-Path $buildRootPath 'mcla.exe'
$symbolsPath = Join-Path $buildRootPath 'mcla.pdb'
foreach ($path in @($executablePath, $symbolsPath, (Join-Path $gameRootPath 'default.xex'))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required module-config debugger input was not found: '$path'."
    }
}

$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $repoRoot "private/evidence/M3-003/$runId-cdb"
$userRoot = Join-Path $runRoot 'user'
$cacheRoot = Join-Path $runRoot 'cache'
[System.IO.Directory]::CreateDirectory($userRoot) | Out-Null
[System.IO.Directory]::CreateDirectory($cacheRoot) | Out-Null
$logPath = Join-Path $runRoot 'mcla-module-config.log'
$transcriptPath = Join-Path $runRoot 'cdb.txt'

$debuggerArguments = @(
    '-lines',
    '-logo', $transcriptPath,
    '-c', 'g; .echo === MCLA MODULE-CONFIG FAILURE ===; .exr -1; ~* kp; q',
    $executablePath,
    '--mcla_module_config_probe',
    "--game_data_root=$gameRootPath",
    "--user_data_root=$userRoot",
    "--cache_root=$cacheRoot",
    "--log_file=$logPath",
    '--log_level=trace'
)
$debuggerOutput = @(& $debuggerPath @debuggerArguments 2>&1 | ForEach-Object { $_.ToString() })
$debuggerExitCode = $LASTEXITCODE

$debuggerOutput
[pscustomobject]@{
    DebuggerExitCode = $debuggerExitCode
    PrivateRunRoot = $runRoot
    DebuggerTranscript = $transcriptPath
    ModuleConfigLog = $logPath
}
