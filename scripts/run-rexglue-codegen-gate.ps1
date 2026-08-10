[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'High')]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$manifestPath = Join-Path $repoRoot 'mcla_manifest.toml'
$generatedTarget = Join-Path $repoRoot 'generated/default'
$evidenceRoot = Join-Path $repoRoot 'private/evidence/M2-008/non-force-first-run'
$rexgluePath = Join-Path $repoRoot 'third_party/rexglue-sdk/out/install/win-amd64/bin/rexglue.exe'

if (-not (Test-Path -LiteralPath $rexgluePath -PathType Leaf)) {
    throw "Pinned ReXGlue CLI was not found: '$rexgluePath'. Run scripts/bootstrap.ps1."
}
if (Test-Path -LiteralPath $evidenceRoot) {
    throw "M2-008 first-run evidence already exists and will not be overwritten: '$evidenceRoot'."
}
if (Test-Path -LiteralPath $generatedTarget) {
    $existing = @(Get-ChildItem -LiteralPath $generatedTarget -Force)
    if ($existing.Count -ne 0) {
        throw "First-run generated target is not empty: '$generatedTarget'."
    }
}

& (Join-Path $PSScriptRoot 'verify-rexglue-manifest.ps1') -ManifestPath $manifestPath | Out-Null
& (Join-Path $PSScriptRoot 'verify-first-analysis-policy.ps1') -ManifestPath $manifestPath | Out-Null

$relativeCommand = 'rexglue codegen mcla_manifest.toml'
if (-not $PSCmdlet.ShouldProcess($manifestPath, "Run first non-force codegen and retain private evidence at '$evidenceRoot'")) {
    [pscustomobject]@{
        Ready           = $true
        Command         = $relativeCommand
        ForceEnabled    = $false
        EvidenceCreated = $false
    }
    return
}

[System.IO.Directory]::CreateDirectory($evidenceRoot) | Out-Null
$stdoutPath = Join-Path $evidenceRoot 'stdout.log'
$stderrPath = Join-Path $evidenceRoot 'stderr.log'
$metadataPath = Join-Path $evidenceRoot 'run.json'
$startedUtc = [datetime]::UtcNow

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
if (-not $process.Start()) {
    throw 'ReXGlue process did not start.'
}
$stdoutTask = $process.StandardOutput.ReadToEndAsync()
$stderrTask = $process.StandardError.ReadToEndAsync()
$process.WaitForExit()
$stdout = $stdoutTask.GetAwaiter().GetResult()
$stderr = $stderrTask.GetAwaiter().GetResult()
$exitCode = $process.ExitCode
$process.Dispose()
$finishedUtc = [datetime]::UtcNow

[System.IO.File]::WriteAllText($stdoutPath, $stdout, [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText($stderrPath, $stderr, [System.Text.UTF8Encoding]::new($false))

$generatedFiles = if (Test-Path -LiteralPath $generatedTarget -PathType Container) {
    @(Get-ChildItem -LiteralPath $generatedTarget -File -Recurse -Force)
} else {
    @()
}
$generatedBytes = if ($generatedFiles.Count -eq 0) {
    0
} else {
    [long](($generatedFiles | Measure-Object -Property Length -Sum).Sum)
}

$metadata = [ordered]@{
    schema             = 1
    task               = 'M2-008'
    command            = $relativeCommand
    force_enabled      = $false
    started_utc        = $startedUtc.ToString('o')
    finished_utc       = $finishedUtc.ToString('o')
    duration_seconds   = [math]::Round(($finishedUtc - $startedUtc).TotalSeconds, 3)
    exit_code          = $exitCode
    stdout_bytes       = (Get-Item -LiteralPath $stdoutPath).Length
    stdout_sha256      = (Get-FileHash -LiteralPath $stdoutPath -Algorithm SHA256).Hash
    stderr_bytes       = (Get-Item -LiteralPath $stderrPath).Length
    stderr_sha256      = (Get-FileHash -LiteralPath $stderrPath -Algorithm SHA256).Hash
    generated_target_exists = (Test-Path -LiteralPath $generatedTarget -PathType Container)
    generated_file_count = $generatedFiles.Count
    generated_total_bytes = $generatedBytes
}
[System.IO.File]::WriteAllText(
    $metadataPath,
    (($metadata | ConvertTo-Json -Depth 3) + [Environment]::NewLine),
    [System.Text.UTF8Encoding]::new($false)
)

[pscustomobject]@{
    Completed       = $true
    ExitCode        = $exitCode
    DurationSeconds = $metadata.duration_seconds
    StdoutBytes     = $metadata.stdout_bytes
    StderrBytes     = $metadata.stderr_bytes
    EvidenceRoot    = $evidenceRoot
}
