[CmdletBinding()]
param(
    [string]$ManifestPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $ManifestPath) {
    $ManifestPath = Join-Path $repoRoot 'mcla_manifest.toml'
}
if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Manifest was not found: '$ManifestPath'."
}
$manifestPath = (Resolve-Path -LiteralPath $ManifestPath).Path
$sdkRoot = Join-Path $repoRoot 'third_party/rexglue-sdk'
$configHeader = Join-Path $sdkRoot 'include/rex/codegen/config.h'
$configSource = Join-Path $sdkRoot 'src/codegen/config.cpp'
$expectedSdkCommit = '3eb9b511b4140d2769e27be63eae57d41bfa2afa'

$actualSdkCommit = (& git -c "safe.directory=$($sdkRoot.Replace('\', '/'))" -C $sdkRoot rev-parse HEAD)
if ($LASTEXITCODE -ne 0 -or -not $actualSdkCommit) {
    throw "Could not resolve the pinned ReXGlue SDK commit."
}
$actualSdkCommit = $actualSdkCommit.Trim()
if ($actualSdkCommit -ne $expectedSdkCommit) {
    throw "ReXGlue SDK pin mismatch. Expected $expectedSdkCommit, got '$actualSdkCommit'."
}

$manifest = Get-Content -LiteralPath $manifestPath -Raw
$header = Get-Content -LiteralPath $configHeader -Raw
$source = Get-Content -LiteralPath $configSource -Raw
$flags = [ordered]@{
    skip_lr                     = 'skipLr'
    skip_msr                    = 'skipMsr'
    ctr_as_local                = 'ctrAsLocalVariable'
    xer_as_local                = 'xerAsLocalVariable'
    reserved_as_local           = 'reservedRegisterAsLocalVariable'
    cr_as_local                 = 'crRegistersAsLocalVariables'
    non_argument_as_local       = 'nonArgumentRegistersAsLocalVariables'
    non_volatile_as_local       = 'nonVolatileRegistersAsLocalVariables'
    generate_exception_handlers = 'generateExceptionHandlers'
}

foreach ($entry in $flags.GetEnumerator()) {
    $manifestPattern = "(?m)^$([regex]::Escape($entry.Key))\s*=\s*false\s*$"
    if ($manifest -notmatch $manifestPattern) {
        throw "First-analysis flag '$($entry.Key)' must be explicitly false in mcla_manifest.toml."
    }
    $defaultPattern = "bool\s+$([regex]::Escape($entry.Value))\s*=\s*false\s*;"
    if ($header -notmatch $defaultPattern) {
        throw "Pinned SDK no longer declares '$($entry.Value)' with a false default."
    }
    $parserPattern = 'toml\["{0}"\]' -f [regex]::Escape($entry.Key)
    if ($source -notmatch $parserPattern) {
        throw "Pinned SDK parser does not consume manifest key '$($entry.Key)'."
    }
}

$forbiddenSections = @('analysis', 'functions', 'switch_tables', 'mid_asm_hooks', 'rexcrt')
foreach ($section in $forbiddenSections) {
    if ($manifest -match "(?m)^\[\[?$([regex]::Escape($section))\]?\]\s*$") {
        throw "First analysis must not contain manual [$section] hints or tuning."
    }
}

[pscustomobject]@{
    Validated                 = $true
    SdkCommit                = $actualSdkCommit
    ExplicitDisabledFlags    = $flags.Count
    ManualOrAnalysisSections = 0
}
