[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$snapshot = Join-Path $PSScriptRoot 'snapshot-generated-integration.ps1'
$testRoot = Join-Path $repoRoot ('private/test-snapshot-generated-' + [guid]::NewGuid().ToString('N'))
$generatedRoot = Join-Path $testRoot 'generated'
$manifestPath = Join-Path $testRoot 'generated-manifest.json'
[System.IO.Directory]::CreateDirectory($generatedRoot) | Out-Null
[System.IO.File]::WriteAllText(
    (Join-Path $generatedRoot 'one.cpp'),
    "void one() {}`n",
    [System.Text.UTF8Encoding]::new($false)
)

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Action)
    try {
        & $Action | Out-Null
    } catch {
        return
    }
    throw "Negative generated-snapshot fixture '$Name' was accepted."
}

try {
    $result = & $snapshot -GeneratedRoot $generatedRoot -OutputPath $manifestPath -Confirm:$false
    if (-not $result.Written -or $result.GeneratedFiles -ne 1 -or
        $result.GeneratedCppSources -ne 1 -or -not (Test-Path -LiteralPath $manifestPath)) {
        throw 'Positive generated-snapshot fixture did not return the reviewed result.'
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.schema -ne 1 -or $manifest.file_count -ne 1 -or
        $manifest.files[0].path -ne 'one.cpp') {
        throw 'Positive generated-snapshot manifest has unexpected contents.'
    }

    Assert-Rejected 'overwrite' {
        & $snapshot -GeneratedRoot $generatedRoot -OutputPath $manifestPath -Confirm:$false
    }
    Assert-Rejected 'outside-private' {
        & $snapshot -GeneratedRoot $generatedRoot -OutputPath 'generated-snapshot.json' -Confirm:$false
    }
    $emptyRoot = Join-Path $testRoot 'empty'
    [System.IO.Directory]::CreateDirectory($emptyRoot) | Out-Null
    Assert-Rejected 'empty-root' {
        & $snapshot -GeneratedRoot $emptyRoot -OutputPath (Join-Path $testRoot 'empty.json') -Confirm:$false
    }

    [pscustomobject]@{ Passed = $true; PositiveCases = 1; NegativeCases = 3 }
} finally {
    if (Test-Path -LiteralPath $testRoot -PathType Container) {
        [System.IO.Directory]::Delete($testRoot, $true)
    }
}
