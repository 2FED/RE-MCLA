[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$source=Join-Path $repoRoot 'config/mcla_functions.toml'
$validator=Join-Path $PSScriptRoot 'verify-analysis-config.ps1'
$testRoot=Join-Path ([System.IO.Path]::GetTempPath()) ('mcla-analysis-config-'+[guid]::NewGuid().ToString('N'))
[System.IO.Directory]::CreateDirectory($testRoot)|Out-Null
function Assert-Rejected([string]$Name,[string]$Content){$path=Join-Path $testRoot "$Name.toml";[System.IO.File]::WriteAllText($path,$Content,[System.Text.UTF8Encoding]::new($false));try{& $validator -ConfigPath $path|Out-Null}catch{return};throw "Negative analysis-config fixture '$Name' was accepted."}
try {
    $content=[System.IO.File]::ReadAllText($source)
    & $validator -ConfigPath $source|Out-Null
    Assert-Rejected 'missing-function' ($content -replace '(?m)^"0x823FD718".*\r?\n','')
    Assert-Rejected 'missing-runtime-function' ($content -replace '(?m)^"0x827B4B78".*\r?\n','')
    Assert-Rejected 'missing-race-runtime-function' ($content -replace '(?m)^"0x82262320".*\r?\n','')
    Assert-Rejected 'wrong-runtime-discovered-boundary' ($content -replace '"0x82554080" = \{ end = 0x8255409C', '"0x82554080" = { end = 0x825540A0')
    Assert-Rejected 'wrong-race-series-boundary' ($content -replace '"0x8220B810" = \{ end = 0x8220B834', '"0x8220B810" = { end = 0x8220B838')
    Assert-Rejected 'wrong-parent' ($content -replace 'parent = 0x824B0CC0','parent = 0x824B0CC4')
    Assert-Rejected 'unreviewed-section' ($content+"`n[[switch_tables]]`naddress=1`n")
    [pscustomobject]@{Passed=$true;PositiveCases=1;NegativeCases=7}
} finally {[System.IO.Directory]::Delete($testRoot,$true)}
