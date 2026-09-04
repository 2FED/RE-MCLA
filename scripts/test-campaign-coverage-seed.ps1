[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$source=Join-Path $repo 'config/campaign-coverage-seed.json'
$verify=Join-Path $PSScriptRoot 'verify-campaign-coverage-seed.ps1'
$root=Join-Path $repo ('private/evidence/M7-001/fixture-'+[guid]::NewGuid().ToString('N'))
$utf8=[Text.UTF8Encoding]::new($false)
$script:negative=0
function Reject([scriptblock]$Mutation){
    $value=Get-Content -LiteralPath $source -Raw|ConvertFrom-Json;& $Mutation $value
    $path=Join-Path $root ($script:negative.ToString()+'.json');[IO.File]::WriteAllText($path,($value|ConvertTo-Json -Depth 15),$utf8)
    $rejected=$false;try{& $verify -MatrixPath $path|Out-Null}catch{$rejected=$true};if(-not $rejected){throw 'Invalid coverage claim accepted.'};$script:negative++
}
try{
    [IO.Directory]::CreateDirectory($root)|Out-Null
    $positive=& $verify -MatrixPath $source
    Reject {param($m)$m.catalog_complete=$true}
    Reject {param($m)$m.full_campaign_pass=$true}
    Reject {param($m)$m.mandatory_node_count=11}
    Reject {param($m)$m.dependency_graph_complete=$true}
    Reject {param($m)$m.entries[0].counts_toward_full_campaign=$true}
    Reject {param($m)$m.entries[1].id=$m.entries[0].id}
    Reject {param($m)$m.entries[0].evidence=@('docs/missing.md')}
    Reject {param($m)$m.entries[0].evidence=@('../private/save.json')}
    Reject {param($m)$m.entries[0].known_issues=@('KI-999')}
    Reject {param($m)$m.entries[0].unknown=@()}
    Reject {param($m)$m.entries[-1].evidence_level='historical-accepted'}
    Reject {param($m)$m.entries[-1].kind='campaign-route';$m.entries[-1].evidence_level='historical-accepted'}
    Reject {param($m)$m.entries[-1].id='renamed-first-race';$m.entries[-1].kind='campaign-route';$m.entries[-1].evidence_level='historical-accepted'}
    [pscustomobject]@{Passed=$true;Entries=$positive.Entries;Positive=1;Negatives=$script:negative;GameplayCredit=$false}
}finally{
    $parent=[IO.Path]::GetFullPath((Join-Path $repo 'private/evidence/M7-001')).TrimEnd('\')
    $target=[IO.Path]::GetFullPath($root)
    if((Split-Path -Parent $target) -cne $parent -or (Split-Path -Leaf $target) -cnotmatch '^fixture-[a-f0-9]{32}$'){throw 'Unsafe fixture cleanup path.'}
    if(Test-Path -LiteralPath $target){Remove-Item -LiteralPath $target -Recurse -Force}
}
