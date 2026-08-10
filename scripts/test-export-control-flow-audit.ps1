[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$audit = Join-Path $repoRoot 'private/evidence/M2-011/control-flow-audit.tsv'
$forceRoot = Join-Path $repoRoot 'private/evidence/M2-009/force-first-run'
$output = Join-Path $repoRoot 'docs/evidence/M2-011-control-flow-metadata.md'
$exporter = Join-Path $PSScriptRoot 'export-control-flow-audit.ps1'
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("mcla-flow-audit-" + [guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($testRoot) | Out-Null

function Assert-Rejected {
    param([string]$Name, [scriptblock]$Action)
    try { & $Action } catch { return }
    throw "Negative control-flow fixture '$Name' was accepted."
}

try {
    & $exporter -AuditPath $audit -ForceRoot $forceRoot -OutputPath $output | Out-Null
    $first = (Get-FileHash $output -Algorithm SHA256).Hash
    & $exporter -AuditPath $audit -ForceRoot $forceRoot -OutputPath $output | Out-Null
    $second = (Get-FileHash $output -Algorithm SHA256).Hash
    if ($first -ne $second) { throw 'Control-flow export is not deterministic.' }

    $missing = Join-Path $testRoot 'missing.tsv'
    Get-Content $audit | Select-Object -SkipLast 1 | Set-Content $missing
    Assert-Rejected 'missing-record' { & $exporter -AuditPath $missing -ForceRoot $forceRoot -OutputPath $output | Out-Null }

    $changed = Join-Path $testRoot 'changed.tsv'
    (Get-Content $audit).Replace("target_flow`t823f32e8`t823f32fc`tblr`t6", "target_flow`t823f32e8`t823f32f8`tblr`t5") | Set-Content $changed
    Assert-Rejected 'changed-terminal' { & $exporter -AuditPath $changed -ForceRoot $forceRoot -OutputPath $output | Out-Null }

    Assert-Rejected 'outside-output' { & $exporter -AuditPath $audit -ForceRoot $forceRoot -OutputPath (Join-Path $testRoot 'outside.md') | Out-Null }
    [pscustomobject]@{ Passed=$true; PositiveCases=2; NegativeCases=3; OutputSha256=$second }
} finally {
    [System.IO.Directory]::Delete($testRoot, $true)
}
