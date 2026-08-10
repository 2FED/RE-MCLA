[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'High')]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$manifestPath = Join-Path $repoRoot 'mcla_manifest.toml'
$generatedTarget = Join-Path $repoRoot 'generated/default'
$m2008Root = Join-Path $repoRoot 'private/evidence/M2-008/non-force-first-run'
$evidenceRoot = Join-Path $repoRoot 'private/evidence/M2-009/force-first-run'
$rexgluePath = Join-Path $repoRoot 'third_party/rexglue-sdk/out/install/win-amd64/bin/rexglue.exe'

if (-not (Test-Path -LiteralPath $rexgluePath -PathType Leaf)) {
    throw "Pinned ReXGlue CLI was not found: '$rexgluePath'. Run scripts/bootstrap.ps1."
}
if (Test-Path -LiteralPath $evidenceRoot) {
    throw "M2-009 first-run evidence already exists and will not be overwritten: '$evidenceRoot'."
}
if (Test-Path -LiteralPath $generatedTarget) {
    $existing = @(Get-ChildItem -LiteralPath $generatedTarget -Force)
    if ($existing.Count -ne 0) {
        throw "Force-inventory generated target is not empty: '$generatedTarget'."
    }
}

$m2008MetadataPath = Join-Path $m2008Root 'run.json'
$m2008StderrPath = Join-Path $m2008Root 'stderr.log'
if (-not (Test-Path -LiteralPath $m2008MetadataPath -PathType Leaf) -or
    -not (Test-Path -LiteralPath $m2008StderrPath -PathType Leaf)) {
    throw 'The immutable M2-008 non-force evidence must exist before M2-009.'
}
$m2008 = Get-Content -LiteralPath $m2008MetadataPath -Raw | ConvertFrom-Json
$m2008StderrHash = (Get-FileHash -LiteralPath $m2008StderrPath -Algorithm SHA256).Hash
if ($m2008.command -ne 'rexglue codegen mcla_manifest.toml' -or
    $m2008.force_enabled -ne $false -or $m2008.exit_code -ne 1 -or
    $m2008.stderr_sha256 -ne $m2008StderrHash) {
    throw 'The M2-008 prerequisite evidence is missing, changed, or not the expected non-force rejection.'
}

& (Join-Path $PSScriptRoot 'verify-rexglue-manifest.ps1') -ManifestPath $manifestPath | Out-Null
& (Join-Path $PSScriptRoot 'verify-first-analysis-policy.ps1') -ManifestPath $manifestPath | Out-Null

$relativeCommand = 'rexglue --force codegen mcla_manifest.toml'
if (-not $PSCmdlet.ShouldProcess($manifestPath, "Run first force inventory and retain private evidence at '$evidenceRoot'")) {
    [pscustomobject]@{
        Ready           = $true
        Command         = $relativeCommand
        ForceEnabled    = $true
        EvidenceCreated = $false
    }
    return
}

[System.IO.Directory]::CreateDirectory($evidenceRoot) | Out-Null
$stdoutPath = Join-Path $evidenceRoot 'stdout.log'
$stderrPath = Join-Path $evidenceRoot 'stderr.log'
$metadataPath = Join-Path $evidenceRoot 'run.json'
$generatedManifestPath = Join-Path $evidenceRoot 'generated-manifest.json'
$startedUtc = [datetime]::UtcNow

$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $rexgluePath
$startInfo.Arguments = "--force codegen `"$manifestPath`""
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
$generatedEntries = @($generatedFiles | Sort-Object FullName | ForEach-Object {
    [ordered]@{
        path   = $_.FullName.Substring($generatedTarget.Length + 1).Replace('\', '/')
        bytes  = $_.Length
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
})
$generatedManifest = [ordered]@{
    schema      = 1
    task        = 'M2-009'
    file_count  = $generatedEntries.Count
    total_bytes = $generatedBytes
    files       = $generatedEntries
}
[System.IO.File]::WriteAllText(
    $generatedManifestPath,
    (($generatedManifest | ConvertTo-Json -Depth 5) + [Environment]::NewLine),
    [System.Text.UTF8Encoding]::new($false)
)

$metadata = [ordered]@{
    schema                  = 1
    task                    = 'M2-009'
    command                 = $relativeCommand
    force_enabled           = $true
    prerequisite_stderr_sha256 = $m2008StderrHash
    started_utc             = $startedUtc.ToString('o')
    finished_utc            = $finishedUtc.ToString('o')
    duration_seconds        = [math]::Round(($finishedUtc - $startedUtc).TotalSeconds, 3)
    exit_code               = $exitCode
    stdout_bytes            = (Get-Item -LiteralPath $stdoutPath).Length
    stdout_sha256           = (Get-FileHash -LiteralPath $stdoutPath -Algorithm SHA256).Hash
    stderr_bytes            = (Get-Item -LiteralPath $stderrPath).Length
    stderr_sha256           = (Get-FileHash -LiteralPath $stderrPath -Algorithm SHA256).Hash
    generated_target_exists = (Test-Path -LiteralPath $generatedTarget -PathType Container)
    generated_file_count    = $generatedFiles.Count
    generated_total_bytes   = $generatedBytes
    generated_manifest_sha256 = (Get-FileHash -LiteralPath $generatedManifestPath -Algorithm SHA256).Hash
}
[System.IO.File]::WriteAllText(
    $metadataPath,
    (($metadata | ConvertTo-Json -Depth 3) + [Environment]::NewLine),
    [System.Text.UTF8Encoding]::new($false)
)

[pscustomobject]@{
    Completed           = $true
    ExitCode            = $exitCode
    DurationSeconds     = $metadata.duration_seconds
    StdoutBytes         = $metadata.stdout_bytes
    StderrBytes         = $metadata.stderr_bytes
    GeneratedFileCount  = $generatedFiles.Count
    GeneratedTotalBytes = $generatedBytes
    EvidenceRoot        = $evidenceRoot
}
