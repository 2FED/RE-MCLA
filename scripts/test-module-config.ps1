[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$validator = Join-Path $PSScriptRoot 'verify-module-config.ps1'
$testRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("mcla-module-config-" + [guid]::NewGuid())
[System.IO.Directory]::CreateDirectory($testRoot) | Out-Null

function Assert-Rejected {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][string]$Header,
        [Parameter(Mandatory)][string]$Implementation
    )
    $headerPath = Join-Path $testRoot "$Name.h"
    $implementationPath = Join-Path $testRoot "$Name.cpp"
    [System.IO.File]::WriteAllText($headerPath, $Header, [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllText($implementationPath, $Implementation, [System.Text.UTF8Encoding]::new($false))
    try {
        & $validator -HeaderPath $headerPath -ImplementationPath $implementationPath | Out-Null
    } catch {
        return
    }
    throw "Negative module-config fixture '$Name' was accepted."
}

$validHeader = @'
#define REX_IMAGE_BASE 0x82000000ull
#define REX_IMAGE_SIZE 0x9E0000ull
#define REX_CODE_BASE 0x82130000ull
#define REX_CODE_SIZE 0x69D054ull
'@
$validImplementation = @'
PPCFuncMapping PPCFuncMappings[] = {
  { 0x82130000, first },
  { 0x821322B8, xstart },
  { 0x827CD050, last },
  { 0, nullptr }
};
'@

try {
    $positiveHeader = Join-Path $testRoot 'positive.h'
    $positiveImplementation = Join-Path $testRoot 'positive.cpp'
    [System.IO.File]::WriteAllText($positiveHeader, $validHeader, [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllText(
        $positiveImplementation,
        $validImplementation,
        [System.Text.UTF8Encoding]::new($false)
    )
    & $validator -HeaderPath $positiveHeader -ImplementationPath $positiveImplementation | Out-Null

    Assert-Rejected -Name 'wrong-image-base' `
        -Header ($validHeader -replace '0x82000000', '0x81000000') `
        -Implementation $validImplementation
    Assert-Rejected -Name 'missing-entry' `
        -Header $validHeader `
        -Implementation ($validImplementation -replace '  \{ 0x821322B8, xstart \},\r?\n', '')
    Assert-Rejected -Name 'outside-code' `
        -Header $validHeader `
        -Implementation ($validImplementation -replace '0x827CD050', '0x827CD054')
    Assert-Rejected -Name 'unordered-duplicate' `
        -Header $validHeader `
        -Implementation ($validImplementation -replace '0x827CD050', '0x821322B8')
    Assert-Rejected -Name 'missing-sentinel' `
        -Header $validHeader `
        -Implementation ($validImplementation -replace '  \{ 0, nullptr \}\r?\n', '')

    [pscustomobject]@{
        Passed = $true
        PositiveCases = 1
        NegativeCases = 5
    }
} finally {
    [System.IO.Directory]::Delete($testRoot, $true)
}
