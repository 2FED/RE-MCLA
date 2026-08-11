[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$log = Join-Path $repoRoot 'private/baseline/M2-002/xenia-stock.snapshot.log'
$generated = Join-Path $repoRoot 'private/evidence/M2-016/pre-release-tag/generated'
$manifest = Join-Path $repoRoot 'private/evidence/M2-012/10-final-clean-b/generated-manifest.json'
$output = Join-Path $repoRoot 'docs/evidence/M2-013-import-coverage.md'
$exporter = Join-Path $PSScriptRoot 'export-import-coverage.ps1'
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('mcla-import-coverage-' + [guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($testRoot) | Out-Null

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Action)
    try { & $Action } catch { return }
    throw "Negative import-coverage fixture '$Name' was accepted."
}

try {
    & $exporter -XeniaLogPath $log -GeneratedRoot $generated -GeneratedManifestPath $manifest -OutputPath $output | Out-Null
    $first = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
    & $exporter -XeniaLogPath $log -GeneratedRoot $generated -GeneratedManifestPath $manifest -OutputPath $output | Out-Null
    $second = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
    if ($first -ne $second) { throw 'Import coverage export is not deterministic.' }

    $changedLog = Join-Path $testRoot 'changed.log'
    [System.IO.File]::WriteAllText($changedLog, (Get-Content -LiteralPath $log -Raw).Replace('XGetVideoMode', 'XGetVideoMood'))
    Assert-Rejected 'changed-log' { & $exporter -XeniaLogPath $changedLog -GeneratedRoot $generated -GeneratedManifestPath $manifest -OutputPath $output | Out-Null }

    $changedManifest = Join-Path $testRoot 'changed-manifest.json'
    $manifestObject = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    $manifestObject.file_count = 63
    [System.IO.File]::WriteAllText($changedManifest, (($manifestObject | ConvertTo-Json -Depth 5) + [Environment]::NewLine))
    Assert-Rejected 'changed-manifest' { & $exporter -XeniaLogPath $log -GeneratedRoot $generated -GeneratedManifestPath $changedManifest -OutputPath $output | Out-Null }

    Assert-Rejected 'outside-output' { & $exporter -XeniaLogPath $log -GeneratedRoot $generated -GeneratedManifestPath $manifest -OutputPath (Join-Path $testRoot 'outside.md') | Out-Null }

    [pscustomobject]@{ Passed=$true; PositiveCases=2; NegativeCases=3; OutputSha256=$second }
} finally {
    [System.IO.Directory]::Delete($testRoot, $true)
}
