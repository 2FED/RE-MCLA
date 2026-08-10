[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$rawRoot = Join-Path $repoRoot 'private/evidence/M2-009/force-first-run'
$generatedRoot = Join-Path $repoRoot 'private/evidence/M2-011/generated-snapshot'
$outputPath = Join-Path $repoRoot 'docs/evidence/M2-009-force-codegen-inventory.md'
$exporter = Join-Path $PSScriptRoot 'export-rexglue-force-inventory.ps1'
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("mcla-force-export-" + [guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($testRoot) | Out-Null

function Copy-RawFixture {
    param([Parameter(Mandatory)][string]$Name)
    $destination = Join-Path $testRoot $Name
    [System.IO.Directory]::CreateDirectory($destination) | Out-Null
    foreach ($file in @('run.json', 'stdout.log', 'stderr.log', 'generated-manifest.json')) {
        [System.IO.File]::Copy((Join-Path $rawRoot $file), (Join-Path $destination $file))
    }
    return $destination
}

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Action)
    try { & $Action } catch { return }
    throw "Negative force-inventory fixture '$Name' was accepted."
}

try {
    & $exporter -RawRoot $rawRoot -GeneratedRoot $generatedRoot -OutputPath $outputPath | Out-Null
    $firstHash = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash
    & $exporter -RawRoot $rawRoot -GeneratedRoot $generatedRoot -OutputPath $outputPath | Out-Null
    $secondHash = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash
    if ($firstHash -ne $secondHash) { throw 'Force inventory export is not deterministic.' }

    $forceFixture = Copy-RawFixture 'force-disabled'
    $forceMetadataPath = Join-Path $forceFixture 'run.json'
    $forceMetadata = Get-Content $forceMetadataPath -Raw | ConvertFrom-Json
    $forceMetadata.force_enabled = $false
    [System.IO.File]::WriteAllText($forceMetadataPath, (($forceMetadata | ConvertTo-Json) + "`n"))
    Assert-Rejected 'force-disabled' { & $exporter -RawRoot $forceFixture -GeneratedRoot $generatedRoot -OutputPath $outputPath | Out-Null }

    $unknownFixture = Copy-RawFixture 'unknown-log-line'
    $unknownLogPath = Join-Path $unknownFixture 'stderr.log'
    [System.IO.File]::AppendAllText($unknownLogPath, "UNREVIEWED FORCE OUTPUT`n")
    $unknownMetadataPath = Join-Path $unknownFixture 'run.json'
    $unknownMetadata = Get-Content $unknownMetadataPath -Raw | ConvertFrom-Json
    $unknownMetadata.stderr_bytes = (Get-Item $unknownLogPath).Length
    $unknownMetadata.stderr_sha256 = (Get-FileHash $unknownLogPath -Algorithm SHA256).Hash
    [System.IO.File]::WriteAllText($unknownMetadataPath, (($unknownMetadata | ConvertTo-Json) + "`n"))
    Assert-Rejected 'unknown-log-line' { & $exporter -RawRoot $unknownFixture -GeneratedRoot $generatedRoot -OutputPath $outputPath | Out-Null }

    $manifestFixture = Copy-RawFixture 'manifest-count'
    $manifestPath = Join-Path $manifestFixture 'generated-manifest.json'
    $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
    $manifest.file_count = 63
    [System.IO.File]::WriteAllText($manifestPath, (($manifest | ConvertTo-Json -Depth 5) + "`n"))
    $manifestMetadataPath = Join-Path $manifestFixture 'run.json'
    $manifestMetadata = Get-Content $manifestMetadataPath -Raw | ConvertFrom-Json
    $manifestMetadata.generated_manifest_sha256 = (Get-FileHash $manifestPath -Algorithm SHA256).Hash
    [System.IO.File]::WriteAllText($manifestMetadataPath, (($manifestMetadata | ConvertTo-Json) + "`n"))
    Assert-Rejected 'manifest-count' { & $exporter -RawRoot $manifestFixture -GeneratedRoot $generatedRoot -OutputPath $outputPath | Out-Null }

    Assert-Rejected 'outside-output' { & $exporter -RawRoot $rawRoot -GeneratedRoot $generatedRoot -OutputPath (Join-Path $testRoot 'outside.md') | Out-Null }

    [pscustomobject]@{
        Passed        = $true
        PositiveCases = 2
        NegativeCases = 4
        OutputSha256  = $secondHash
    }
} finally {
    [System.IO.Directory]::Delete($testRoot, $true)
}
