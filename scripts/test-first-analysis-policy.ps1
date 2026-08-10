[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sourceManifest = Join-Path $repoRoot 'mcla_manifest.toml'
$validator = Join-Path $PSScriptRoot 'verify-first-analysis-policy.ps1'
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("mcla-policy-" + [guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($testRoot) | Out-Null

function Assert-Rejected {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Content
    )

    $path = Join-Path $testRoot "$Name.toml"
    [System.IO.File]::WriteAllText($path, $Content, [System.Text.UTF8Encoding]::new($false))
    try {
        & $validator -ManifestPath $path | Out-Null
    } catch {
        return
    }
    throw "Negative fixture '$Name' was accepted."
}

try {
    $source = [System.IO.File]::ReadAllText($sourceManifest)
    $firstAnalysis = $source -replace '(?m)^includes\s*=.*$', 'includes = []'
    $positivePath = Join-Path $testRoot 'first-analysis.toml'
    [System.IO.File]::WriteAllText($positivePath, $firstAnalysis, [System.Text.UTF8Encoding]::new($false))
    & $validator -ManifestPath $positivePath | Out-Null
    Assert-Rejected -Name 'enabled-flag' -Content ($firstAnalysis -replace '(?m)^skip_lr\s*=\s*false\s*$', 'skip_lr = true')
    Assert-Rejected -Name 'analysis-override' -Content ($firstAnalysis + "`n[analysis]`nmax_jump_extension = 1`n")
    Assert-Rejected -Name 'manual-include' -Content $source
    [pscustomobject]@{
        Passed           = $true
        PositiveCases    = 1
        NegativeCases    = 3
    }
} finally {
    [System.IO.Directory]::Delete($testRoot, $true)
}
