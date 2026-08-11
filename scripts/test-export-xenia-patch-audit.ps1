[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$patch = Join-Path $repoRoot 'private/evidence/M2-015/complete-edition.patch.toml'
$audit = Join-Path $repoRoot 'private/evidence/M2-015/patch-audit.tsv'
$output = Join-Path $repoRoot 'docs/evidence/M2-015-xenia-patch-audit.md'
$exporter = Join-Path $PSScriptRoot 'export-xenia-patch-audit.ps1'
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('mcla-patch-audit-' + [guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($testRoot) | Out-Null

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Action)
    try { & $Action } catch { return }
    throw "Negative Xenia-patch fixture '$Name' was accepted."
}

try {
    & $exporter -PatchPath $patch -AuditPath $audit -OutputPath $output | Out-Null
    $first = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
    & $exporter -PatchPath $patch -AuditPath $audit -OutputPath $output | Out-Null
    $second = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
    if ($first -ne $second) { throw 'Xenia patch audit export is not deterministic.' }

    $changedPatch = Join-Path $testRoot 'changed.patch.toml'
    [System.IO.File]::WriteAllText($changedPatch, (Get-Content -LiteralPath $patch -Raw).Replace('is_enabled = false', 'is_enabled = true'))
    Assert-Rejected 'enabled-patch' { & $exporter -PatchPath $changedPatch -AuditPath $audit -OutputPath $output | Out-Null }

    $changedAudit = Join-Path $testRoot 'changed.tsv'
    [System.IO.File]::WriteAllText($changedAudit, (Get-Content -LiteralPath $audit -Raw).Replace('409A00C0', '409A00C1'))
    Assert-Rejected 'changed-original' { & $exporter -PatchPath $patch -AuditPath $changedAudit -OutputPath $output | Out-Null }

    Assert-Rejected 'outside-output' { & $exporter -PatchPath $patch -AuditPath $audit -OutputPath (Join-Path $testRoot 'outside.md') | Out-Null }
    [pscustomobject]@{ Passed=$true; PositiveCases=2; NegativeCases=3; OutputSha256=$second }
} finally {
    [System.IO.Directory]::Delete($testRoot, $true)
}
