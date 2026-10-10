# Materialize pinned libmspack file links in the disposable SDK build copy only.
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$module=Join-Path $repo 'private/sdk20s/thirdparty/libmspack'
$original=Join-Path $repo 'third_party/rexglue-sdk/thirdparty/libmspack'
function Read-Git([string]$Root,[string[]]$Arguments){
    $result=@(& git -C $Root @Arguments)
    if($LASTEXITCODE-ne0){throw 'Git metadata read failed.'}
    return $result
}
if((Get-Item -LiteralPath $module).Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Module root must not be a reparse point.'}
if(@(Read-Git $module @('rev-parse','HEAD'))[0]-cne@(Read-Git $original @('rev-parse','HEAD'))[0]){throw 'Dependency identity differs.'}
if(@(Read-Git $module @('status','--porcelain=v1')).Count){throw 'Isolated module must start clean; no automatic repeat.'}
$originalStatus=(Read-Git $original @('status','--porcelain=v1'))-join"`n"
$entries=@(Read-Git $module @('ls-files','-s') | Where-Object {$_-match'^120000 '})
if($entries.Count-ne15){throw 'Expected15pinned file links.'}
$records=@()
foreach($entry in $entries){
    $relative=($entry-split"`t",2)[1]
    if($relative-cnotmatch'^cabextract/mspack/[a-zA-Z0-9_.]+$'){throw 'Unexpected link path.'}
    $path=Join-Path $module $relative
    if((Get-Item -LiteralPath $path).Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Expected plain placeholder.'}
    $link=@(Read-Git $module @('show',"HEAD:$relative"))[0]
    if((Get-Content -LiteralPath $path -Raw).Trim()-cne$link){throw 'Placeholder differs from pinned blob.'}
    $target=[IO.Path]::GetFullPath((Join-Path (Split-Path $path) $link))
    $allowed=Join-Path $module 'libmspack'
    if(-not$target.StartsWith($allowed+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Link target outside dependency sources.'}
    if(-not(Test-Path -LiteralPath $target -PathType Leaf)){throw 'Missing source target.'}
    if((Get-Item -LiteralPath $target).Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Target is another link.'}
    foreach($leaf in @($path,$target)){
        $ancestor=Split-Path $leaf
        while($ancestor.StartsWith($repo+'\',[StringComparison]::OrdinalIgnoreCase)){
            if((Get-Item -LiteralPath $ancestor).Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Reparse ancestor rejected.'}
            $ancestor=Split-Path $ancestor
        }
    }
    $originalPath=Join-Path $original $relative
    $hash=(Get-FileHash -LiteralPath $target).Hash
    if((Get-FileHash -LiteralPath $originalPath).Hash-cne$hash){throw 'Original resolved bytes differ.'}
    $records+=[pscustomobject]@{relative=$relative;path=$path;target=$target;sha256=$hash;blob=($entry-split' ')[1]}
}
foreach($record in $records){
    Copy-Item -LiteralPath $record.target -Destination $record.path
    if((Get-FileHash -LiteralPath $record.path).Hash-cne$record.sha256){throw 'Materialized content mismatch.'}
}
$changed=@(Read-Git $module @('diff','--name-only'))
if((($changed|Sort-Object)-join'|')-cne(($records.relative|Sort-Object)-join'|')){throw 'Unexpected dependency delta.'}
if(((Read-Git $original @('status','--porcelain=v1'))-join"`n")-cne$originalStatus){throw 'Original dependency status changed.'}
[pscustomobject]@{Passed=$true;Materialized=$records.Count;OriginalPreserved=$true;DependencyDirtyByDesign=$true;Files=$records|Select-Object relative,blob,target,sha256}
