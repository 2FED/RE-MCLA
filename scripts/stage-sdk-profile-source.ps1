# One-shot local source isolation. No network, build, install, tag or publication.
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$source=Join-Path $repo 'third_party/rexglue-sdk'
$target=Join-Path $repo 'private/sdk20s'
$baseline='c2e67161f1207ddcee609507c399efe7a1981b72'
$files=@('CMakeLists.txt','include/rex/system/xam/user_profile.h','src/system/kernel_state.cpp','src/system/runtime.cpp','src/system/xam/content_manager.cpp','src/system/xam/user_profile.cpp','tests/unit/kernel/xam_user_profile_test.cpp')
function Git-Checked([string[]]$Arguments){
    & git @Arguments
    if($LASTEXITCODE-ne0){throw 'Git isolation command failed; retain partial destination.'}
}
if(Test-Path -LiteralPath $target){throw 'Isolation destination already exists; never overwrite/retry automatically.'}
if((git -C $source rev-parse HEAD).Trim()-cne$baseline){throw 'Source baseline changed.'}
if((Get-PSDrive C).Free-lt15GB){throw 'Need15GB free for isolated sources/build.'}
$statusBefore=@(git -C $source status --porcelain=v1 --untracked-files=all)
$before=@{};foreach($file in $files){$before[$file]=(Get-FileHash -LiteralPath (Join-Path $source $file)).Hash}
$modules=@(git -C $source submodule status --recursive)
if($LASTEXITCODE-ne0-or$modules.Count-ne25){throw 'Expected25pinned dependencies.'}
$records=@()
foreach($line in $modules){
    if($line-cnotmatch '^ ([0-9a-f]{40}) ([a-zA-Z0-9_./-]+)(?: |$)'){throw 'Dependency not at pinned commit.'}
    $sha=$Matches[1];$relative=$Matches[2]
    if($relative-match '(^|/)\.\.(/|$)'-or-not$relative.StartsWith('thirdparty/')){throw 'Unsafe dependency path.'}
    if(@(git -C (Join-Path $source $relative) status --porcelain=v1 --untracked-files=no).Count){throw 'Dependency tracked changes present.'}
    $records+= [pscustomobject]@{path=$relative;commit=$sha}
}
Git-Checked @('-c','protocol.file.allow=always','clone','--local','--no-checkout',$source,$target)
Git-Checked @('-C',$target,'checkout','--detach',$baseline)
Git-Checked @('-C',$target,'submodule','init')
foreach($module in $records){
    $from=Join-Path $source $module.path;$to=Join-Path $target $module.path
    Git-Checked @('-c','protocol.file.allow=always','clone','--local','--no-checkout',$from,$to)
    Git-Checked @('-C',$to,'checkout','--detach',$module.commit)
    Git-Checked @('-C',$to,'submodule','init')
}
foreach($file in $files){
    Copy-Item -LiteralPath (Join-Path $source $file) -Destination (Join-Path $target $file)
    if((Get-FileHash -LiteralPath (Join-Path $target $file)).Hash-cne$before[$file]){throw 'Profile source copy mismatch.'}
}
$changed=@(git -C $target diff --name-only)
if((($changed|Sort-Object)-join'|')-cne(($files|Sort-Object)-join'|')){throw 'Unexpected isolated source delta.'}
foreach($file in $files){if((Get-FileHash -LiteralPath (Join-Path $source $file)).Hash-cne$before[$file]){throw 'Original source changed during isolation.'}}
if(((@(git -C $source status --porcelain=v1 --untracked-files=all))-join"`n")-cne($statusBefore-join"`n")){throw 'Original SDK status changed.'}
$result=@(git -C $target submodule status --recursive)
if($LASTEXITCODE-ne0-or$result.Count-ne$records.Count){throw 'Isolated dependency count differs.'}
$actual=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach($line in $result){
    if($line-cnotmatch '^ ([0-9a-f]{40}) ([a-zA-Z0-9_./-]+)(?: |$)'){throw 'Isolated dependency is not pinned.'}
    if(-not$actual.Add($Matches[2]+'|'+$Matches[1])){throw 'Duplicate isolated dependency.'}
}
foreach($module in $records){if(-not$actual.Contains($module.path+'|'+$module.commit)){throw 'Isolated dependency identity differs.'}}
[pscustomobject]@{Passed=$true;Source=$target;Baseline=$baseline;ProfileFiles=$files.Count;Dependencies=$records.Count;OriginalPreserved=$true;BuildRun=$false}
