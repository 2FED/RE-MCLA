[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$rawRoot = Join-Path $repoRoot 'private/evidence/M2-008/non-force-first-run'
$outputPath = Join-Path $repoRoot 'docs/evidence/M2-008-non-force-codegen.md'
$exporter = Join-Path $PSScriptRoot 'export-rexglue-codegen-gate.ps1'
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("mcla-codegen-export-" + [guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($testRoot) | Out-Null

function Copy-RawFixture {
    param([Parameter(Mandatory)][string]$Name)
    $destination = Join-Path $testRoot $Name
    [System.IO.Directory]::CreateDirectory($destination) | Out-Null
    foreach ($file in @('run.json', 'stdout.log', 'stderr.log')) {
        [System.IO.File]::Copy((Join-Path $rawRoot $file), (Join-Path $destination $file))
    }
    return $destination
}

function Assert-Rejected {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][scriptblock]$Action
    )
    try {
        & $Action
    } catch {
        return
    }
    throw "Negative exporter fixture '$Name' was accepted."
}

try {
    & $exporter -RawRoot $rawRoot -OutputPath $outputPath | Out-Null
    $firstHash = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash
    & $exporter -RawRoot $rawRoot -OutputPath $outputPath | Out-Null
    $secondHash = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash
    if ($firstHash -ne $secondHash) {
        throw 'Two positive exports were not deterministic.'
    }

    $forceFixture = Copy-RawFixture -Name 'force-enabled'
    $forceMetadataPath = Join-Path $forceFixture 'run.json'
    $forceMetadata = Get-Content -LiteralPath $forceMetadataPath -Raw | ConvertFrom-Json
    $forceMetadata.force_enabled = $true
    [System.IO.File]::WriteAllText($forceMetadataPath, (($forceMetadata | ConvertTo-Json) + "`n"))
    Assert-Rejected -Name 'force-enabled' -Action {
        & $exporter -RawRoot $forceFixture -OutputPath $outputPath | Out-Null
    }

    $unknownFixture = Copy-RawFixture -Name 'unknown-log-line'
    $unknownLogPath = Join-Path $unknownFixture 'stderr.log'
    [System.IO.File]::AppendAllText($unknownLogPath, "UNREVIEWED OUTPUT`n")
    $unknownMetadataPath = Join-Path $unknownFixture 'run.json'
    $unknownMetadata = Get-Content -LiteralPath $unknownMetadataPath -Raw | ConvertFrom-Json
    $unknownMetadata.stderr_bytes = (Get-Item -LiteralPath $unknownLogPath).Length
    $unknownMetadata.stderr_sha256 = (Get-FileHash -LiteralPath $unknownLogPath -Algorithm SHA256).Hash
    [System.IO.File]::WriteAllText($unknownMetadataPath, (($unknownMetadata | ConvertTo-Json) + "`n"))
    Assert-Rejected -Name 'unknown-log-line' -Action {
        & $exporter -RawRoot $unknownFixture -OutputPath $outputPath | Out-Null
    }

    Assert-Rejected -Name 'outside-output' -Action {
        & $exporter -RawRoot $rawRoot -OutputPath (Join-Path $testRoot 'outside.md') | Out-Null
    }

    [pscustomobject]@{
        Passed        = $true
        PositiveCases = 2
        NegativeCases = 3
        OutputSha256  = $secondHash
    }
} finally {
    [System.IO.Directory]::Delete($testRoot, $true)
}
