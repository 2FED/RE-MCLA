[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'High')]
param(
    [Parameter(Mandatory)][ValidatePattern('^[0-9]{2}-[a-z0-9-]+$')][string]$Iteration,
    [Parameter(Mandatory)][ValidateRange(0, 7)][int]$ExpectedUnresolvedCount,
    [Parameter(Mandatory)][ValidateSet(0, 1)][int]$ExpectedExitCode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$manifestPath = Join-Path $repoRoot 'mcla_manifest.toml'
$configPath = Join-Path $repoRoot 'config/mcla_functions.toml'
$rexgluePath = Join-Path $repoRoot 'third_party/rexglue-sdk/out/install/win-amd64/bin/rexglue.exe'
$generatedRoot = Join-Path $repoRoot 'generated/default'
$evidenceRoot = Join-Path $repoRoot "private/evidence/M2-012/$Iteration"

foreach ($path in @($manifestPath, $configPath, $rexgluePath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required input was not found: '$path'." }
    if ((Get-Item -LiteralPath $path -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Required input must not be a reparse point: '$path'."
    }
}
if (Test-Path -LiteralPath $evidenceRoot) {
    throw "Iteration evidence already exists and will not be overwritten: '$evidenceRoot'."
}

$relativeCommand = 'rexglue codegen mcla_manifest.toml'
if (-not $PSCmdlet.ShouldProcess($manifestPath, "Run M2-012 config iteration '$Iteration'")) {
    [pscustomobject]@{ Ready=$true; Iteration=$Iteration; Command=$relativeCommand; EvidenceCreated=$false }
    return
}

[System.IO.Directory]::CreateDirectory($evidenceRoot) | Out-Null
$stdoutPath = Join-Path $evidenceRoot 'stdout.log'
$stderrPath = Join-Path $evidenceRoot 'stderr.log'
$metadataPath = Join-Path $evidenceRoot 'run.json'
$manifestSnapshot = Join-Path $evidenceRoot 'mcla_manifest.toml'
$configSnapshot = Join-Path $evidenceRoot 'mcla_functions.toml'
[System.IO.File]::Copy($manifestPath, $manifestSnapshot)
[System.IO.File]::Copy($configPath, $configSnapshot)

$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $rexgluePath
$startInfo.Arguments = "codegen `"$manifestPath`""
$startInfo.WorkingDirectory = $repoRoot
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$process = [System.Diagnostics.Process]::new()
$process.StartInfo = $startInfo
$started = [datetime]::UtcNow
if (-not $process.Start()) { throw 'ReXGlue process did not start.' }
$stdoutTask = $process.StandardOutput.ReadToEndAsync()
$stderrTask = $process.StandardError.ReadToEndAsync()
$process.WaitForExit()
$stdout = $stdoutTask.GetAwaiter().GetResult()
$stderr = $stderrTask.GetAwaiter().GetResult()
$exitCode = $process.ExitCode
$process.Dispose()
$finished = [datetime]::UtcNow
[System.IO.File]::WriteAllText($stdoutPath, $stdout, [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText($stderrPath, $stderr, [System.Text.UTF8Encoding]::new($false))

$unresolved = @($stderr -split "`r?`n" | Where-Object { $_ -match '^  0x[0-9A-F]{8} from 0x[0-9A-F]{8}:' })
$generatedManifestPath = Join-Path $evidenceRoot 'generated-manifest.json'
$generatedFiles = if ($exitCode -eq 0 -and (Test-Path -LiteralPath $generatedRoot -PathType Container)) {
    @(Get-ChildItem -LiteralPath $generatedRoot -File -Recurse -Force | Sort-Object FullName)
} else { @() }
$generatedEntries = @($generatedFiles | ForEach-Object {
    [ordered]@{
        path = $_.FullName.Substring($generatedRoot.Length + 1).Replace('\', '/')
        bytes = $_.Length
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
})
$generatedBytes = if ($generatedFiles.Count) { [long](($generatedFiles | Measure-Object Length -Sum).Sum) } else { 0 }
if ($exitCode -eq 0) {
    $generatedManifest = [ordered]@{ schema=1; file_count=$generatedEntries.Count; total_bytes=$generatedBytes; files=$generatedEntries }
    [System.IO.File]::WriteAllText($generatedManifestPath, (($generatedManifest | ConvertTo-Json -Depth 5) + [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))
}
$metadata = [ordered]@{
    schema = 1
    task = 'M2-012'
    iteration = $Iteration
    command = $relativeCommand
    expected_exit_code = $ExpectedExitCode
    expected_unresolved_count = $ExpectedUnresolvedCount
    exit_code = $exitCode
    unresolved_count = $unresolved.Count
    started_utc = $started.ToString('o')
    finished_utc = $finished.ToString('o')
    duration_seconds = [math]::Round(($finished - $started).TotalSeconds, 3)
    manifest_sha256 = (Get-FileHash $manifestSnapshot -Algorithm SHA256).Hash
    config_sha256 = (Get-FileHash $configSnapshot -Algorithm SHA256).Hash
    stdout_sha256 = (Get-FileHash $stdoutPath -Algorithm SHA256).Hash
    stderr_sha256 = (Get-FileHash $stderrPath -Algorithm SHA256).Hash
    generated_file_count = $generatedFiles.Count
    generated_total_bytes = $generatedBytes
    generated_manifest_sha256 = if ($exitCode -eq 0) { (Get-FileHash $generatedManifestPath -Algorithm SHA256).Hash } else { $null }
}
[System.IO.File]::WriteAllText($metadataPath, (($metadata | ConvertTo-Json) + [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))

if ($exitCode -ne $ExpectedExitCode -or $unresolved.Count -ne $ExpectedUnresolvedCount) {
    throw "Iteration '$Iteration' produced exit $exitCode and $($unresolved.Count) unresolved calls; expected exit $ExpectedExitCode and $ExpectedUnresolvedCount. Evidence was retained."
}
[pscustomobject]@{ Completed=$true; Iteration=$Iteration; ExitCode=$exitCode; UnresolvedCount=$unresolved.Count; DurationSeconds=$metadata.duration_seconds; EvidenceRoot=$evidenceRoot }
