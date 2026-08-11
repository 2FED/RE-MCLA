[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [ValidateRange(5, 60)][int]$TimeoutSeconds = 20
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifierPath = Join-Path $PSScriptRoot 'verify-logging-schema.ps1'
$categories = @('app', 'ppc', 'kernel', 'xam', 'vfs', 'gpu', 'audio', 'input', 'patches')
$candidate = if ([System.IO.Path]::IsPathRooted($BuildRoot)) {
    $BuildRoot
} else {
    Join-Path $repoRoot $BuildRoot
}
$buildRootPath = (Resolve-Path -LiteralPath $candidate).Path
$repoPrefix = $repoRoot.TrimEnd('\') + '\'
if (-not $buildRootPath.StartsWith($repoPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Build root must stay inside the repository: '$buildRootPath'."
}
$executablePath = Join-Path $buildRootPath 'mcla.exe'
foreach ($path in @($executablePath, $verifierPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required logging-probe input was not found: '$path'."
    }
}

$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $repoRoot "private/evidence/M3-010/$runId"
[System.IO.Directory]::CreateDirectory($runRoot) | Out-Null
$results = [System.Collections.Generic.List[object]]::new()

foreach ($category in $categories) {
    $categoryRoot = Join-Path $runRoot $category
    $userRoot = Join-Path $categoryRoot 'user'
    $cacheRoot = Join-Path $categoryRoot 'cache'
    [System.IO.Directory]::CreateDirectory($userRoot) | Out-Null
    [System.IO.Directory]::CreateDirectory($cacheRoot) | Out-Null
    $logPath = Join-Path $categoryRoot 'mcla.log'
    $override = "--mcla_log_$category=info"
    $process = Start-Process -FilePath $executablePath -ArgumentList @(
        '--mcla_lifecycle_probe',
        '--mcla_logging_probe',
        '--log_level=off',
        $override,
        "--user_data_root=$userRoot",
        "--cache_root=$cacheRoot",
        "--log_file=$logPath"
    ) -WorkingDirectory $buildRootPath -PassThru

    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force
        throw "Logging probe '$category' exceeded $TimeoutSeconds seconds. Private run: '$runRoot'."
    }
    if ($process.ExitCode -ne 0) {
        throw "Logging probe '$category' exited with code $($process.ExitCode). Private run: '$runRoot'."
    }

    $verified = & $verifierPath -LogPath $logPath -ExpectedCategory $category
    $results.Add([ordered]@{
        category = $category
        exit_code = $process.ExitCode
        schema_markers = $verified.SchemaMarkers
        log_bytes = $verified.LogBytes
        log_sha256 = (Get-FileHash -LiteralPath $logPath -Algorithm SHA256).Hash
    })
}

$resultPath = Join-Path $runRoot 'result.json'
$result = [ordered]@{
    schema = 1
    task = 'M3-010'
    required_categories = $categories
    runs = $results
}
[System.IO.File]::WriteAllText(
    $resultPath,
    (($result | ConvertTo-Json -Depth 5) + [Environment]::NewLine),
    [System.Text.UTF8Encoding]::new($false)
)

[pscustomobject]@{
    Passed = $true
    Categories = $categories.Count
    FilteredRuns = $results.Count
    PrivateRunRoot = $runRoot
    ResultPath = $resultPath
}
