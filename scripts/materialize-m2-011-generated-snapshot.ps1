[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'High')]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$evidenceRoot = Join-Path $repoRoot 'private/evidence/M2-011'
$snapshotRoot = Join-Path $evidenceRoot 'generated-snapshot'
$snapshotManifest = Join-Path $repoRoot '.m2-011-historical-manifest.toml'
$expectedManifestPath = Join-Path $repoRoot 'private/evidence/M2-009/force-first-run/generated-manifest.json'
$currentManifestPath = Join-Path $repoRoot 'mcla_manifest.toml'
$rexgluePath = Join-Path $repoRoot 'third_party/rexglue-sdk/out/install/win-amd64/bin/rexglue.exe'

foreach ($path in @($expectedManifestPath, $currentManifestPath, $rexgluePath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required input was not found: '$path'." }
}
if (Test-Path -LiteralPath $snapshotRoot) {
    throw "Historical snapshot already exists and will not be overwritten: '$snapshotRoot'."
}

$manifestText = Get-Content -LiteralPath $currentManifestPath -Raw
if (@([regex]::Matches($manifestText, '(?m)^out_directory_path = "generated/default"$')).Count -ne 1 -or
    @([regex]::Matches($manifestText, '(?m)^includes = \["config/mcla_functions.toml"\]$')).Count -ne 1) {
    throw 'Current manifest does not have the reviewed M2-012 output/include lines.'
}
$historicalText = $manifestText.Replace(
    'out_directory_path = "generated/default"',
    'out_directory_path = "private/evidence/M2-011/generated-snapshot"'
).Replace(
    'includes = ["config/mcla_functions.toml"]',
    'includes = []'
)

if (-not $PSCmdlet.ShouldProcess($snapshotRoot, 'Regenerate and validate the immutable M2-009 generated snapshot used by M2-011')) {
    [pscustomobject]@{ Ready=$true; SnapshotCreated=$false; SnapshotRoot=$snapshotRoot }
    return
}

[System.IO.Directory]::CreateDirectory($evidenceRoot) | Out-Null
[System.IO.File]::WriteAllText($snapshotManifest, $historicalText, [System.Text.UTF8Encoding]::new($false))
try {
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $rexgluePath
    $startInfo.Arguments = "--force codegen `"$snapshotManifest`""
    $startInfo.WorkingDirectory = $repoRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) { throw 'ReXGlue process did not start.' }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    $exitCode = $process.ExitCode
    $process.Dispose()
    if ($exitCode -ne 0) { throw "Historical force codegen failed with exit code $exitCode.`n$stderr" }

    $expected = Get-Content -LiteralPath $expectedManifestPath -Raw | ConvertFrom-Json
    $actualFiles = @(Get-ChildItem -LiteralPath $snapshotRoot -File -Recurse -Force | Sort-Object FullName)
    if ($actualFiles.Count -ne $expected.file_count) {
        throw "Historical snapshot produced $($actualFiles.Count) files; expected $($expected.file_count)."
    }
    foreach ($entry in @($expected.files)) {
        $path = Join-Path $snapshotRoot $entry.path
        if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
            (Get-Item -LiteralPath $path).Length -ne $entry.bytes -or
            (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.sha256) {
            throw "Historical generated snapshot does not match M2-009: '$($entry.path)'."
        }
    }
    [pscustomobject]@{
        Completed=$true
        SnapshotCreated=$true
        FileCount=$actualFiles.Count
        ExpectedManifestSha256=(Get-FileHash -LiteralPath $expectedManifestPath -Algorithm SHA256).Hash
        SnapshotRoot=$snapshotRoot
        StdoutBytes=[System.Text.Encoding]::UTF8.GetByteCount($stdout)
        StderrBytes=[System.Text.Encoding]::UTF8.GetByteCount($stderr)
    }
} finally {
    if (Test-Path -LiteralPath $snapshotManifest -PathType Leaf) {
        [System.IO.File]::Delete($snapshotManifest)
    }
}
