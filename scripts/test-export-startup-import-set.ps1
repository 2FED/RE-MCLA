[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$generated = Join-Path $repoRoot 'private/evidence/M2-016/pre-release-tag/generated'
$manifest = Join-Path $repoRoot 'private/evidence/M2-012/10-final-clean-b/generated-manifest.json'
$coverage = Join-Path $repoRoot 'docs/evidence/M2-013-import-coverage.md'
$sdk = Join-Path $repoRoot 'third_party/rexglue-sdk'
$output = Join-Path $repoRoot 'docs/evidence/M2-014-startup-import-set.md'
$exporter = Join-Path $PSScriptRoot 'export-startup-import-set.ps1'
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('mcla-startup-set-' + [guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($testRoot) | Out-Null

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Action)
    try { & $Action } catch { return }
    throw "Negative startup-set fixture '$Name' was accepted."
}

try {
    & $exporter -GeneratedRoot $generated -GeneratedManifestPath $manifest -ImportCoveragePath $coverage -SdkRoot $sdk -OutputPath $output | Out-Null
    $first = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
    & $exporter -GeneratedRoot $generated -GeneratedManifestPath $manifest -ImportCoveragePath $coverage -SdkRoot $sdk -OutputPath $output | Out-Null
    $second = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
    if ($first -ne $second) { throw 'Startup import export is not deterministic.' }

    $changedCoverage = Join-Path $testRoot 'changed-coverage.md'
    [System.IO.File]::WriteAllText($changedCoverage, (Get-Content -LiteralPath $coverage -Raw).Replace('XGetLanguage', 'XGetLanguages'))
    Assert-Rejected 'changed-coverage' { & $exporter -GeneratedRoot $generated -GeneratedManifestPath $manifest -ImportCoveragePath $changedCoverage -SdkRoot $sdk -OutputPath $output | Out-Null }

    $emptyGenerated = Join-Path $testRoot 'empty-generated'
    [System.IO.Directory]::CreateDirectory($emptyGenerated) | Out-Null
    Assert-Rejected 'missing-generated' { & $exporter -GeneratedRoot $emptyGenerated -GeneratedManifestPath $manifest -ImportCoveragePath $coverage -SdkRoot $sdk -OutputPath $output | Out-Null }

    Assert-Rejected 'outside-output' { & $exporter -GeneratedRoot $generated -GeneratedManifestPath $manifest -ImportCoveragePath $coverage -SdkRoot $sdk -OutputPath (Join-Path $testRoot 'outside.md') | Out-Null }

    [pscustomobject]@{ Passed=$true; PositiveCases=2; NegativeCases=3; OutputSha256=$second }
} finally {
    [System.IO.Directory]::Delete($testRoot, $true)
}
