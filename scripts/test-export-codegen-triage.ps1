[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$rawRoot = Join-Path $repoRoot 'private/evidence/M2-009/force-first-run'
$auditPath = Join-Path $repoRoot 'private/evidence/M2-010/address-audit.tsv'
$outputPath = Join-Path $repoRoot 'docs/evidence/M2-010-codegen-triage.md'
$exporter = Join-Path $PSScriptRoot 'export-codegen-triage.ps1'
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("mcla-triage-export-" + [guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($testRoot) | Out-Null

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Action)
    try { & $Action } catch { return }
    throw "Negative triage fixture '$Name' was accepted."
}

try {
    & $exporter -RawRoot $rawRoot -AuditPath $auditPath -OutputPath $outputPath | Out-Null
    $firstHash = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash
    & $exporter -RawRoot $rawRoot -AuditPath $auditPath -OutputPath $outputPath | Out-Null
    $secondHash = (Get-FileHash -LiteralPath $outputPath -Algorithm SHA256).Hash
    if ($firstHash -ne $secondHash) { throw 'Codegen triage export is not deterministic.' }

    $missingAudit = Join-Path $testRoot 'missing-row.tsv'
    Get-Content -LiteralPath $auditPath | Select-Object -SkipLast 1 | Set-Content -LiteralPath $missingAudit
    Assert-Rejected 'missing-audit-row' { & $exporter -RawRoot $rawRoot -AuditPath $missingAudit -OutputPath $outputPath | Out-Null }

    $wrongPack = Join-Path $testRoot 'wrong-pack.tsv'
    (Get-Content -LiteralPath $auditPath) -replace '1BD7EE15','1BD6EE15' | Set-Content -LiteralPath $wrongPack
    Assert-Rejected 'wrong-pack-fields' { & $exporter -RawRoot $rawRoot -AuditPath $wrongPack -OutputPath $outputPath | Out-Null }

    $rawFixture = Join-Path $testRoot 'raw'
    [System.IO.Directory]::CreateDirectory($rawFixture) | Out-Null
    [System.IO.File]::Copy((Join-Path $rawRoot 'run.json'), (Join-Path $rawFixture 'run.json'))
    [System.IO.File]::Copy((Join-Path $rawRoot 'stderr.log'), (Join-Path $rawFixture 'stderr.log'))
    [System.IO.File]::AppendAllText((Join-Path $rawFixture 'stderr.log'), "UNKNOWN FINDING`n")
    Assert-Rejected 'mutated-force-log' { & $exporter -RawRoot $rawFixture -AuditPath $auditPath -OutputPath $outputPath | Out-Null }

    Assert-Rejected 'outside-output' { & $exporter -RawRoot $rawRoot -AuditPath $auditPath -OutputPath (Join-Path $testRoot 'outside.md') | Out-Null }

    [pscustomobject]@{ Passed=$true; PositiveCases=2; NegativeCases=4; OutputSha256=$secondHash }
} finally {
    [System.IO.Directory]::Delete($testRoot, $true)
}
