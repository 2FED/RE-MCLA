[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game',
    [ValidateRange(5, 60)][int]$TimeoutSeconds = 15
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$debuggerPath = 'C:\Program Files\Windows Kits\10\Debuggers\x64\cdb.exe'
$verifierPath = Join-Path $PSScriptRoot 'verify-xam-startup-import-trace.ps1'

function Resolve-ContainedPath {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Description)
    $candidate = if ([System.IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repoRoot $Path }
    $resolved = (Resolve-Path -LiteralPath $candidate).Path
    $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay inside the repository: '$resolved'."
    }
    return $resolved
}

foreach ($path in @($debuggerPath, $verifierPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required startup-trace tool was not found: '$path'."
    }
}
$buildRootPath = Resolve-ContainedPath -Path $BuildRoot -Description 'Build root'
$gameRootPath = Resolve-ContainedPath -Path $GameRoot -Description 'Game root'
$executablePath = Join-Path $buildRootPath 'mcla.exe'
foreach ($path in @($executablePath, (Join-Path $buildRootPath 'mcla.pdb'), (Join-Path $gameRootPath 'default.xex'))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required startup-trace input was not found: '$path'."
    }
}

$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $repoRoot "private/evidence/M3-006/$runId-cdb"
$userRoot = Join-Path $runRoot 'user'
$cacheRoot = Join-Path $runRoot 'cache'
[System.IO.Directory]::CreateDirectory($userRoot) | Out-Null
[System.IO.Directory]::CreateDirectory($cacheRoot) | Out-Null
$transcriptPath = Join-Path $runRoot 'cdb.txt'
$runtimeLogPath = Join-Path $runRoot 'mcla.log'
$commandPath = Join-Path $runRoot 'commands.txt'
$resultPath = Join-Path $runRoot 'result.json'

$commands = @(
    'bp mcla!_imp__XamLoaderTerminateTitle ".echo MCLA_XAM_IMPORT XamLoaderTerminateTitle; gc"',
    'bp mcla!_imp__XamShowMessageBoxUIEx ".echo MCLA_XAM_IMPORT XamShowMessageBoxUIEx; gc"',
    'bp mcla!_imp__XGetAVPack ".echo MCLA_XAM_IMPORT XGetAVPack; r @$t0=@rcx; bp /1 poi(@rsp) \".printf \\\"MCLA_XAM_RESULT XGetAVPack 0x%I64x\\\\n\\\", poi(@$t0); gc\"; gc"',
    'bp mcla!_imp__XGetLanguage ".echo MCLA_XAM_IMPORT XGetLanguage; gc"',
    'bp mcla!__imp__sub_821305E8 ".echo MCLA_BOUNDARY title-main; q"',
    'g'
)
[System.IO.File]::WriteAllLines($commandPath, $commands, [System.Text.UTF8Encoding]::new($false))

$arguments = '-logo "' + $transcriptPath + '" -cf "' + $commandPath + '" "' +
    $executablePath + '" --game_data_root="' + $gameRootPath + '" --user_data_root="' +
    $userRoot + '" --cache_root="' + $cacheRoot + '" --log_file="' + $runtimeLogPath +
    '" --log_level=trace'
$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $debuggerPath
$startInfo.Arguments = $arguments
$startInfo.WorkingDirectory = $buildRootPath
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$process = [System.Diagnostics.Process]::Start($startInfo)
$completed = $process.WaitForExit($TimeoutSeconds * 1000)
if (-not $completed) {
    $process.Kill()
    $process.WaitForExit()
    throw "Startup import trace exceeded $TimeoutSeconds seconds. Evidence: '$runRoot'."
}
if ($process.ExitCode -ne 0) {
    throw "CDB startup import trace exited with code $($process.ExitCode). Evidence: '$runRoot'."
}

$verification = & $verifierPath -TranscriptPath $transcriptPath -RuntimeLogPath $runtimeLogPath
$result = [ordered]@{
    schema = 1
    task = 'M3-006'
    debugger_exit_code = $process.ExitCode
    boundary_count = $verification.BoundaryCount
    function_imports_reviewed = $verification.FunctionImportsReviewed
    function_imports_reached = $verification.FunctionImportsReached
    total_function_hits = $verification.TotalFunctionHits
    executable_sha256 = (Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash
    transcript_sha256 = (Get-FileHash -LiteralPath $transcriptPath -Algorithm SHA256).Hash
    runtime_log_sha256 = (Get-FileHash -LiteralPath $runtimeLogPath -Algorithm SHA256).Hash
    matrix = @($verification.Matrix)
}
[System.IO.File]::WriteAllText(
    $resultPath,
    (($result | ConvertTo-Json -Depth 5) + [Environment]::NewLine),
    [System.Text.UTF8Encoding]::new($false)
)

[pscustomobject]@{
    Passed = $true
    PrivateRunRoot = $runRoot
    FunctionImportsReached = $verification.FunctionImportsReached
    TotalFunctionHits = $verification.TotalFunctionHits
    ResultPath = $resultPath
}
