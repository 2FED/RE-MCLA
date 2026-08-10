[CmdletBinding()]param()
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path;$raw=Join-Path $repoRoot 'private/evidence/M2-012';$output=Join-Path $repoRoot 'docs/evidence/M2-012-manual-analysis-config.md';$exporter=Join-Path $PSScriptRoot 'export-manual-analysis-config.ps1';$temp=Join-Path ([System.IO.Path]::GetTempPath()) ('mcla-m2012-'+[guid]::NewGuid().ToString('N'));[System.IO.Directory]::CreateDirectory($temp)|Out-Null
function Assert-Rejected([string]$Name,[scriptblock]$Action){try{&$Action}catch{return};throw "Negative M2-012 fixture '$Name' was accepted."}
try{
 &$exporter -RawRoot $raw -OutputPath $output|Out-Null;$first=(Get-FileHash $output -Algorithm SHA256).Hash;&$exporter -RawRoot $raw -OutputPath $output|Out-Null;$second=(Get-FileHash $output -Algorithm SHA256).Hash;if($first-ne$second){throw'Export is not deterministic.'}
 $fixture=Join-Path $temp 'raw';[System.IO.Directory]::CreateDirectory($fixture)|Out-Null
 foreach($dir in Get-ChildItem $raw -Directory|Where-Object{$_.Name-match'^\d{2}-'}){$to=Join-Path $fixture $dir.Name;[System.IO.Directory]::CreateDirectory($to)|Out-Null;foreach($name in @('run.json','stdout.log','stderr.log','mcla_manifest.toml','mcla_functions.toml','generated-manifest.json')){$from=Join-Path $dir.FullName $name;if(Test-Path $from -PathType Leaf){[System.IO.File]::Copy($from,(Join-Path $to $name))}}}
 Remove-Item -LiteralPath (Join-Path $fixture '07-cf06-leaf/stderr.log')
 Assert-Rejected 'missing-stream' {&$exporter -RawRoot $fixture -OutputPath $output|Out-Null}
 Assert-Rejected 'outside-output' {&$exporter -RawRoot $raw -OutputPath (Join-Path $temp 'outside.md')|Out-Null}
 [pscustomobject]@{Passed=$true;PositiveCases=2;NegativeCases=2;OutputSha256=$second}
}finally{[System.IO.Directory]::Delete($temp,$true)}
