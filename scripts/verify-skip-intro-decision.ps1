[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sourceFiles = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'src') -File -Recurse)
$sourceText = ($sourceFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join "`n"
foreach ($prohibited in @('mcla_skip_intro', '0x821F7F64', '0x48000048')) {
    if ($sourceText.Contains($prohibited)) {
        throw "Skip-intro code '$prohibited' exists without a demonstrated Bink blocker."
    }
}

$auditPath = Join-Path $repoRoot 'docs/evidence/M2-015-xenia-patch-audit.md'
if (-not (Test-Path -LiteralPath $auditPath -PathType Leaf)) {
    throw 'The prior byte audit required for any future skip-intro decision is missing.'
}
$audit = Get-Content -LiteralPath $auditPath -Raw
foreach ($required in @(
        '| Skip Intro | requested enhancement | `0x821F7F64` / be32 | `419A0048` | `48000048` |',
        'Every group is explicitly `is_enabled = false`',
        'Enabled patches: **0**'
    )) {
    if (-not $audit.Contains($required)) {
        throw "The prior skip-intro byte-audit contract is missing '$required'."
    }
}

[pscustomobject]@{
    Passed = $true
    PatchImplemented = $false
    PatchEnabled = $false
    PriorAddressAudit = $true
    Decision = 'not-justified-before-gpu-prerequisite'
}
