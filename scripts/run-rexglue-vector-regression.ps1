[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'High')]
param(
    [string]$ReXGluePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $ReXGluePath) {
    $ReXGluePath = Join-Path $repoRoot 'third_party/rexglue-sdk/out/win-amd64/Release/rexglue.exe'
}
$manifestPath = Join-Path $repoRoot 'mcla_manifest.toml'
$expectedManifestPath = Join-Path $repoRoot 'private/evidence/M2-012/09-final-clean-a/generated-manifest.json'
$evidenceRoot = Join-Path $repoRoot 'private/evidence/M2-016/codegen-regression'
$generatedRoot = Join-Path $evidenceRoot 'generated'
$temporaryManifest = Join-Path $repoRoot '.m2-016-vector-regression.toml'

foreach ($path in @($ReXGluePath, $manifestPath, $expectedManifestPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required input was not found: '$path'."
    }
}
if (Test-Path -LiteralPath $evidenceRoot) {
    throw "Regression evidence already exists and will not be overwritten: '$evidenceRoot'."
}
if (Test-Path -LiteralPath $temporaryManifest) {
    throw "Temporary manifest already exists: '$temporaryManifest'."
}

$manifestText = Get-Content -LiteralPath $manifestPath -Raw
$outputLine = 'out_directory_path = "generated/default"'
$privateOutputLine = 'out_directory_path = "private/evidence/M2-016/codegen-regression/generated"'
if (@([regex]::Matches($manifestText, [regex]::Escape($outputLine))).Count -ne 1) {
    throw 'The reviewed output path was not found exactly once in mcla_manifest.toml.'
}
$regressionManifest = $manifestText.Replace($outputLine, $privateOutputLine)

if (-not $PSCmdlet.ShouldProcess($evidenceRoot, 'Run patched ReXGlue codegen regression and retain private evidence')) {
    [pscustomobject]@{ Ready = $true; EvidenceCreated = $false; ReXGluePath = $ReXGluePath }
    return
}

[System.IO.Directory]::CreateDirectory($evidenceRoot) | Out-Null
[System.IO.File]::WriteAllText($temporaryManifest, $regressionManifest, [System.Text.UTF8Encoding]::new($false))
try {
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $ReXGluePath
    $startInfo.Arguments = "codegen `"$temporaryManifest`""
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

    $stdoutPath = Join-Path $evidenceRoot 'stdout.log'
    $stderrPath = Join-Path $evidenceRoot 'stderr.log'
    [System.IO.File]::WriteAllText($stdoutPath, $stdout, [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllText($stderrPath, $stderr, [System.Text.UTF8Encoding]::new($false))

    $vectorWarnings = @($stderr -split "`r?`n" | Where-Object { $_ -match 'Unexpected float16_4 pack instruction' })
    if ($exitCode -ne 0) { throw "ReXGlue codegen failed with exit code $exitCode." }
    if ($vectorWarnings.Count -ne 0) { throw "Codegen retained $($vectorWarnings.Count) FLOAT16_4 warnings." }

    $expected = Get-Content -LiteralPath $expectedManifestPath -Raw | ConvertFrom-Json
    $actualFiles = @(Get-ChildItem -LiteralPath $generatedRoot -File -Recurse -Force | Sort-Object FullName)
    if ($actualFiles.Count -ne $expected.file_count) {
        throw "Generated file count $($actualFiles.Count) does not match accepted count $($expected.file_count)."
    }

    $entries = [System.Collections.Generic.List[object]]::new()
    foreach ($entry in @($expected.files)) {
        $path = Join-Path $generatedRoot $entry.path
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Generated output is missing '$($entry.path)'."
        }
        $item = Get-Item -LiteralPath $path
        $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if ($item.Length -ne $entry.bytes -or $hash -ne $entry.sha256) {
            throw "Generated output changed for '$($entry.path)'."
        }
        $entries.Add([ordered]@{ path = $entry.path; bytes = $item.Length; sha256 = $hash })
    }

    $actualManifestPath = Join-Path $evidenceRoot 'generated-manifest.json'
    $actualManifest = [ordered]@{
        schema = 1
        file_count = $entries.Count
        total_bytes = [long](($actualFiles | Measure-Object Length -Sum).Sum)
        files = $entries
    }
    [System.IO.File]::WriteAllText(
        $actualManifestPath,
        (($actualManifest | ConvertTo-Json -Depth 5) + [Environment]::NewLine),
        [System.Text.UTF8Encoding]::new($false)
    )

    $metadata = [ordered]@{
        schema = 1
        task = 'M2-016'
        command = 'rexglue codegen .m2-016-vector-regression.toml'
        exit_code = $exitCode
        float16_4_warning_count = $vectorWarnings.Count
        generated_file_count = $entries.Count
        generated_total_bytes = $actualManifest.total_bytes
        generated_manifest_sha256 = (Get-FileHash -LiteralPath $actualManifestPath -Algorithm SHA256).Hash
        accepted_manifest_sha256 = (Get-FileHash -LiteralPath $expectedManifestPath -Algorithm SHA256).Hash
        rexglue_sha256 = (Get-FileHash -LiteralPath $ReXGluePath -Algorithm SHA256).Hash
        started_utc = $started.ToString('o')
        finished_utc = $finished.ToString('o')
        duration_seconds = [math]::Round(($finished - $started).TotalSeconds, 3)
    }
    $metadataPath = Join-Path $evidenceRoot 'run.json'
    [System.IO.File]::WriteAllText(
        $metadataPath,
        (($metadata | ConvertTo-Json) + [Environment]::NewLine),
        [System.Text.UTF8Encoding]::new($false)
    )

    [pscustomobject]@{
        Completed = $true
        ExitCode = $exitCode
        Float16_4Warnings = $vectorWarnings.Count
        GeneratedFiles = $entries.Count
        GeneratedBytes = $actualManifest.total_bytes
        GeneratedManifestSha256 = $metadata.generated_manifest_sha256
        DurationSeconds = $metadata.duration_seconds
        EvidenceRoot = $evidenceRoot
    }
} finally {
    if (Test-Path -LiteralPath $temporaryManifest -PathType Leaf) {
        [System.IO.File]::Delete($temporaryManifest)
    }
}
