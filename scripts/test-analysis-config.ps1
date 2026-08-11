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
    Assert-Rejected 'wrong-parent' ($content -replace 'parent = 0x824B0CC0','parent = 0x824B0CC4')
    Assert-Rejected 'unreviewed-section' ($content+"`n[[switch_tables]]`naddress=1`n")
    [pscustomobject]@{Passed=$true;PositiveCases=1;NegativeCases=4}
} finally {[System.IO.Directory]::Delete($testRoot,$true)}
