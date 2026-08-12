[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game',
    [ValidateRange(2, 10)][int]$RunCount = 3,
    [ValidateRange(10, 60)][int]$TimeoutSeconds = 30
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$debuggerPath = 'C:\Program Files\Windows Kits\10\Debuggers\x64\cdb.exe'
$verifierPath = Join-Path $PSScriptRoot 'verify-early-init-trace.ps1'

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
        throw "Required early-init trace tool was not found: '$path'."
    }
}
$buildRootPath = Resolve-ContainedPath -Path $BuildRoot -Description 'Build root'
$gameRootPath = Resolve-ContainedPath -Path $GameRoot -Description 'Game root'
$executablePath = Join-Path $buildRootPath 'mcla.exe'
foreach ($path in @($executablePath, (Join-Path $buildRootPath 'mcla.pdb'),
        (Join-Path $gameRootPath 'default.xex'))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required early-init trace input was not found: '$path'."
    }
}

$batchId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$batchRoot = Join-Path $repoRoot "private/evidence/M3-008/$batchId-cdb"
[System.IO.Directory]::CreateDirectory($batchRoot) | Out-Null
$results = @()

for ($run = 1; $run -le $RunCount; $run++) {
    $runRoot = Join-Path $batchRoot ("run-{0:D2}" -f $run)
    $userRoot = Join-Path $runRoot 'user'
    $cacheRoot = Join-Path $runRoot 'cache'
    [System.IO.Directory]::CreateDirectory($userRoot) | Out-Null
    [System.IO.Directory]::CreateDirectory($cacheRoot) | Out-Null
    $transcriptPath = Join-Path $runRoot 'cdb.txt'
    $runtimeLogPath = Join-Path $runRoot 'mcla.log'
    $commandPath = Join-Path $runRoot 'commands.txt'

    $commands = @(
        'r @$t0=0',
        'r @$t1=0',
        'r @$t2=0',
        'bp mcla!_imp__KeQueryPerformanceFrequency ".echo MCLA_EARLY_INIT time-frequency; r @$t2=1; gc"',
        'bp mcla!_imp__KeQuerySystemTime ".echo MCLA_EARLY_INIT system-time; r @$t1=1; gc"',
        'bp mcla!_imp__ExCreateThread ".echo MCLA_EARLY_INIT create-thread; r @$t0=1; gc"',
        'bm rexruntimerd!*XThread*BuildStartPlan* ".if ((@$t0 == 1) & (@$t1 == 1) & (@$t2 == 1)) {.echo MCLA_EARLY_INIT start-plan; q} .else {gc}"',
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
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill()
        $process.WaitForExit()
        throw "Early-init trace run $run exceeded $TimeoutSeconds seconds. Evidence: '$runRoot'."
    }
    if ($process.ExitCode -ne 0) {
        throw "CDB early-init trace run $run exited with code $($process.ExitCode). Evidence: '$runRoot'."
    }

    $verified = & $verifierPath -TranscriptPath $transcriptPath -RuntimeLogPath $runtimeLogPath
    $results += [pscustomobject]@{
        Run = $run
        Signature = $verified.FirstOccurrenceSignature
        EventCount = $verified.EventCount
        CreateThreadHits = $verified.CreateThreadHits
        SystemTimeHits = $verified.SystemTimeHits
        PerformanceFrequencyHits = $verified.PerformanceFrequencyHits
        TranscriptSha256 = (Get-FileHash -LiteralPath $transcriptPath -Algorithm SHA256).Hash
        RuntimeLogSha256 = (Get-FileHash -LiteralPath $runtimeLogPath -Algorithm SHA256).Hash
    }
}

$signatures = @($results.Signature | Sort-Object -Unique)
if ($signatures.Count -ne 1) {
    throw "Repeated early-init ordering diverged: $($signatures -join ' | '). Evidence: '$batchRoot'."
}
$resultPath = Join-Path $batchRoot 'result.json'
$result = [ordered]@{
    schema = 1
    task = 'M3-008'
    runs = $RunCount
    timeout_seconds_per_run = $TimeoutSeconds
    first_occurrence_signature = $signatures[0]
    sdk_version = '0.9.0.12'
    executable_sha256 = (Get-FileHash -LiteralPath $executablePath -Algorithm SHA256).Hash
    results = $results
}
[System.IO.File]::WriteAllText(
    $resultPath,
    (($result | ConvertTo-Json -Depth 5) + [Environment]::NewLine),
    [System.Text.UTF8Encoding]::new($false)
)

[pscustomobject]@{
    Passed = $true
    Runs = $RunCount
    FirstOccurrenceSignature = $signatures[0]
    PrivateRunRoot = $batchRoot
    ResultPath = $resultPath
}
