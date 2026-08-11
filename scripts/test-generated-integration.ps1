[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-generated-integration.ps1'
$testRoot = Join-Path $repoRoot ("private/test-generated-integration-" + [guid]::NewGuid().ToString('N'))
$generatedRoot = Join-Path $testRoot 'generated'
$manifestPath = Join-Path $testRoot 'manifest.json'
[System.IO.Directory]::CreateDirectory($generatedRoot) | Out-Null

function Write-Utf8 {
    param([string]$Path, [string]$Content)
    [System.IO.File]::WriteAllText($Path, $Content, [System.Text.UTF8Encoding]::new($false))
}

function Write-Manifest {
    $files = @(Get-ChildItem -LiteralPath $generatedRoot -File | Sort-Object Name)
    $entries = @($files | ForEach-Object {
        [ordered]@{ path = $_.Name; bytes = $_.Length; sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash }
    })
    $manifest = [ordered]@{
        schema = 1
        file_count = $entries.Count
        total_bytes = [long](($files | Measure-Object Length -Sum).Sum)
        files = $entries
    }
    Write-Utf8 -Path $manifestPath -Content (($manifest | ConvertTo-Json -Depth 4) + [Environment]::NewLine)
}

function Assert-Rejected {
    param([string]$Name, [scriptblock]$Mutation)
    & $Mutation
    try {
        & $verifier -GeneratedRoot $generatedRoot -ExpectedManifestPath $manifestPath -ExpectedCppCount 2 | Out-Null
    } catch {
        return
    }
    throw "Negative fixture '$Name' was accepted."
}

try {
    Write-Utf8 (Join-Path $generatedRoot 'a.cpp') "void a() {}`n"
    Write-Utf8 (Join-Path $generatedRoot 'b.cpp') "void b() {}`n"
    Write-Utf8 (Join-Path $generatedRoot 'mcla_init.h') "#pragma once`n"
    Write-Utf8 (Join-Path $generatedRoot 'sources.cmake') @'
set(GENERATED_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/a.cpp
    ${CMAKE_CURRENT_LIST_DIR}/b.cpp
)
'@
    Write-Manifest
    & $verifier -GeneratedRoot $generatedRoot -ExpectedManifestPath $manifestPath -ExpectedCppCount 2 | Out-Null

    Assert-Rejected -Name 'changed-hash' -Mutation {
        Write-Utf8 (Join-Path $generatedRoot 'a.cpp') "void changed() {}`n"
    }
    Write-Utf8 (Join-Path $generatedRoot 'a.cpp') "void a() {}`n"

    Assert-Rejected -Name 'unlisted-file' -Mutation {
        Write-Utf8 (Join-Path $generatedRoot 'extra.cpp') "void extra() {}`n"
    }
    [System.IO.File]::Delete((Join-Path $generatedRoot 'extra.cpp'))

    Assert-Rejected -Name 'wrong-total-bytes' -Mutation {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        $manifest.total_bytes++
        Write-Utf8 -Path $manifestPath -Content (($manifest | ConvertTo-Json -Depth 4) + [Environment]::NewLine)
    }
    Write-Manifest

    Assert-Rejected -Name 'duplicate-manifest-path' -Mutation {
        $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
        $manifest.files[1].path = $manifest.files[0].path
        Write-Utf8 -Path $manifestPath -Content (($manifest | ConvertTo-Json -Depth 4) + [Environment]::NewLine)
    }
    Write-Manifest

    Assert-Rejected -Name 'source-list-mismatch' -Mutation {
        Write-Utf8 (Join-Path $generatedRoot 'sources.cmake') @'
set(GENERATED_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/a.cpp
    ${CMAKE_CURRENT_LIST_DIR}/a.cpp
)
'@
        Write-Manifest
    }

    [pscustomobject]@{ Passed = $true; PositiveCases = 1; NegativeCases = 5 }
} finally {
    if (Test-Path -LiteralPath $testRoot -PathType Container) {
        [System.IO.Directory]::Delete($testRoot, $true)
    }
}
