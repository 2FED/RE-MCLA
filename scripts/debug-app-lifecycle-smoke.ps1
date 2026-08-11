[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$debuggerPath = 'C:\Program Files\Windows Kits\10\Debuggers\x64\cdb.exe'
if (-not (Test-Path -LiteralPath $debuggerPath -PathType Leaf)) {
    throw "CDB was not found: '$debuggerPath'."
}
$buildRootPath = if ([System.IO.Path]::IsPathRooted($BuildRoot)) {
    (Resolve-Path -LiteralPath $BuildRoot).Path
} else {
    (Resolve-Path -LiteralPath (Join-Path $repoRoot $BuildRoot)).Path
}
$repoPrefix = $repoRoot.TrimEnd('\') + '\'
if (-not $buildRootPath.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Build root must stay inside the repository: '$buildRootPath'."
}
$executablePath = Join-Path $buildRootPath 'mcla.exe'
$symbolsPath = Join-Path $buildRootPath 'mcla.pdb'
foreach ($path in @($executablePath, $symbolsPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required diagnostic build artifact was not found: '$path'."
    }
}

$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $repoRoot "private/evidence/M3-002/$runId-cdb"
[System.IO.Directory]::CreateDirectory($runRoot) | Out-Null
$logPath = Join-Path $runRoot 'mcla-lifecycle.log'
$transcriptPath = Join-Path $runRoot 'cdb.txt'

$debuggerArguments = @(
    '-lines',
    '-logo', $transcriptPath,
    '-c', 'g; .echo === MCLA EXCEPTION STACKS ===; ~* kp; q',
    $executablePath,
    '--mcla_lifecycle_probe',
    "--log_file=$logPath",
    '--log_level=trace'
)
$debuggerOutput = @(& $debuggerPath @debuggerArguments 2>&1 |
    ForEach-Object { $_.ToString() })
$debuggerExitCode = $LASTEXITCODE

$debuggerOutput
[pscustomobject]@{
    DebuggerExitCode = $debuggerExitCode
    PrivateRunRoot = $runRoot
    DebuggerTranscript = $transcriptPath
    LifecycleLog = $logPath
}
