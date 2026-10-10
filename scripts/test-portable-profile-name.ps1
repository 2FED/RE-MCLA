[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$LauncherPath,
    [Parameter(Mandatory=$true)][string]$ChildFixturePath,
    [ValidateRange(0,180)][int]$RootLength=0,
    [switch]$LaunchOnly,
    [switch]$HistoryBoundary
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$private=[IO.Path]::GetFullPath((Join-Path $repo 'private'))
$testRoot=[IO.Path]::GetFullPath((Join-Path $private ('evidence\M7-002\profile-name-test-'+[guid]::NewGuid().ToString('N'))))
if(-not $testRoot.StartsWith($private+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Fixture escaped private/.'}
$root=Join-Path $testRoot 'bundle'
if($RootLength){
    if($RootLength-lt$root.Length){throw 'Requested fixture root is shorter than its contained base.'}
    $root+='x'*($RootLength-$root.Length)
}
$utf8=[Text.UTF8Encoding]::new($false)
$calls=0;$negatives=0
. (Join-Path $PSScriptRoot 'Portable-CppRuntime.ps1')
. (Join-Path $PSScriptRoot 'Portable-SaveLineage.ps1')
$runtimeFiles=@(Get-PortableCppRuntimeFiles (Get-PortableCppRuntimePolicy))
function Write-Text([string]$Path,[string]$Text){[IO.File]::WriteAllText($Path,$Text,$utf8)}
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
function Tree([string]$Path){
    $prefix=[IO.Path]::GetFullPath($Path).TrimEnd('\')+'\'
    (@(Get-ChildItem -LiteralPath $Path -Recurse -Force|Sort-Object FullName|ForEach-Object{
        if($_.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Fixture contains redirected path.'}
        $relative=$_.FullName.Substring($prefix.Length).Replace('\','/')
        if($_.PSIsContainer){"D $relative"}else{"F $relative $($_.Length) $(Hash $_.FullName)"}
    })-join"`n")
}
function Quote-Argument([string]$Value){
    # CommandLineToArgvW/MSVC escaping, including empty values and spaces.
    '"'+([regex]::Replace([regex]::Replace($Value,'(\\*)"','$1$1\"'),'(\\+)$','$1$1'))+'"'
}
function Invoke-Fixture([string[]]$Arguments=@(),[hashtable]$Environment=@{}){
    $script:calls++
    $info=[Diagnostics.ProcessStartInfo]::new()
    $info.FileName=Join-Path $root 'Launch-MCLA.exe'
    $info.WorkingDirectory=$root;$info.UseShellExecute=$false;$info.CreateNoWindow=$true
    $info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
    $info.Arguments=(@($Arguments|ForEach-Object{Quote-Argument $_})-join' ')
    foreach($key in @($info.EnvironmentVariables.Keys)){
        if([string]$key -imatch '^REX_(GPU_BACKEND|D3D12_ADAPTER|LOCAL_USER_DISPLAY_NAME)$'){$info.EnvironmentVariables.Remove($key)}
    }
    foreach($key in $Environment.Keys){$info.EnvironmentVariables[$key]=[string]$Environment[$key]}
    $process=[Diagnostics.Process]::new();$process.StartInfo=$info
    try{
        if(-not $process.Start()){throw 'Fixture process did not start.'}
        $stdout=$process.StandardOutput.ReadToEndAsync();$stderr=$process.StandardError.ReadToEndAsync()
        if(-not $process.WaitForExit(20000)){$process.Kill();$process.WaitForExit();throw ('Owned fixture launcher timed out; possible modal error. '+$stderr.Result+' '+$stdout.Result)}
        [pscustomobject]@{ExitCode=$process.ExitCode;Stdout=$stdout.Result;Stderr=$stderr.Result}
    }finally{$process.Dispose()}
}
function Children{@(Get-ChildItem -LiteralPath (Join-Path $root 'results') -File -Filter 'child-argv-*.json')}
function Assert-Exit($Result,[bool]$Success){if(($Result.ExitCode-eq0)-ne$Success){throw "Unexpected fixture exit $($Result.ExitCode): $($Result.Stderr) $($Result.Stdout)"}}
function Reject([string[]]$Arguments,[hashtable]$Environment=@{}){
    $before=Tree (Join-Path $root 'user');$childCount=@(Children).Count
    $journalsBefore=@(Get-ChildItem -LiteralPath (Join-Path $root 'results') -Directory -Filter 'profile-rename-*').Count
    $result=Invoke-Fixture -Arguments $Arguments -Environment $Environment
    Assert-Exit $result $false
    if(-not $result.Stderr){throw 'Rejection did not provide stderr explanation.'}
    if((Tree (Join-Path $root 'user'))-cne$before-or@(Children).Count-ne$childCount){throw 'Rejected operation mutated user tree or launched a child.'}
    if(@(Get-ChildItem -LiteralPath (Join-Path $root 'results') -Directory -Filter 'profile-rename-*').Count-ne$journalsBefore){throw 'Rejected operation created a rename journal.'}
    $script:negatives++
}
function Assert-Journal([string]$ExpectedBefore,[string]$ExpectedAfter,[int]$Count){
    $all=@(Get-ChildItem -LiteralPath (Join-Path $root 'results') -Directory -Filter 'profile-rename-*')
    if($all.Count-ne$Count-or@($all|Where-Object Name -Like '*.partial').Count){throw 'Unexpected rename journal count/partial.'}
    $match=@($all|Where-Object{(Tree (Join-Path $_.FullName 'before\B13EBABEBABEBABE'))-ceq$ExpectedBefore-and(Tree (Join-Path $_.FullName 'after\B13EBABEBABEBABE'))-ceq$ExpectedAfter})
    if($match.Count-ne1){throw 'Rename before/after full profile snapshots differ.'}
    $journal=Get-Content -LiteralPath (Join-Path $match[0].FullName 'rename.json') -Raw|ConvertFrom-Json
    if($journal.schema-cne'mcla-portable-profile-rename-v1'-or$journal.bundle_id-cne$id-or$journal.transition_id-cne$match[0].Name-or
       [string]$journal.completed_utc-cnotmatch '^\d{8}T\d{6}Z$'-or$journal.verified_file_count-ne$immutable.Count){throw 'Rename journal identity/schema invalid.'}
}
try{
    foreach($inputPath in @($LauncherPath,$ChildFixturePath)){if(-not(Test-Path -LiteralPath $inputPath -PathType Leaf)){throw 'Build both supplied fixture executables first.'}}
    if((Get-Item -LiteralPath $ChildFixturePath).Length-gt16MB-or
       [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($ChildFixturePath)).IndexOf('mcla-portable-child-fixture-v1',[StringComparison]::Ordinal)-lt0){throw 'Child must be the bounded synthetic argv fixture, never a real game executable.'}
    foreach($directory in @('bin','game','user\B13EBABEBABEBABE\545407F8\00000001\mc4.sav','user\B13EBABEBABEBABE\545407F8\Headers\00000001','user\profile\User','user\545407F8\profile\User','cache','logs','diagnostics','results','update','metadata')){[IO.Directory]::CreateDirectory((Join-Path $root $directory))|Out-Null}
    $toolchain=& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1')
    $runtimeSource=Resolve-PortableCppRuntimeSource -VisualStudioRoot $toolchain.VisualStudioRoot
    Copy-Item -LiteralPath $LauncherPath -Destination (Join-Path $root 'Launch-MCLA.exe')
    Copy-Item -LiteralPath $ChildFixturePath -Destination (Join-Path $root 'bin\mcla.exe')
    foreach($entry in $runtimeFiles){Copy-Item -LiteralPath (Join-Path $runtimeSource.SourceDirectory $entry.Name) -Destination (Join-Path $root $entry.RelativePath.Replace('/','\'))}
    foreach($relative in @('bin/mcla_crash_handler.exe','bin/rexgpu-xenos.dll','bin/rexruntime.dll','bin/TracyClient.dll','game/default.xex')){Write-Text (Join-Path $root $relative) "synthetic:$relative"}
    Write-Text (Join-Path $root 'bin/mcla.toml') "fullscreen = true`n"
    Write-Text (Join-Path $root 'results/child-fixture.enabled') 'mcla-portable-child-fixture-v1'
    $save=Join-Path $root 'user/B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
    $header=Join-Path $root 'user/B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'
    Write-Text $save 'synthetic-save';Write-Text $header 'synthetic-header'
    Write-Text (Join-Path $root 'user/profile/User/10040002.bin') 'synthetic-global-setting'
    Write-Text (Join-Path $root 'user/545407F8/profile/User/63E83FFF') 'synthetic-title-setting'
    $legacyGlobal=Tree (Join-Path $root 'user/profile');$legacyTitle=Tree (Join-Path $root 'user/545407F8')
    $saveHash=Hash $save;$headerHash=Hash $header
    $id='mcla-0.19.0.0-win64-1111111111111111-2222222222222222-3333333333333333'
    Write-Text (Join-Path $root 'bundle-id.txt') ($id+"`n")
    Write-Text (Join-Path $root 'bundle-manifest.json') '{}';Write-Text (Join-Path $root 'game-manifest.json') '{}'
    $immutable=@('Launch-MCLA.exe','bundle-id.txt','bundle-manifest.json','game-manifest.json','bin/mcla.exe','bin/mcla_crash_handler.exe','bin/mcla.toml','bin/rexgpu-xenos.dll','bin/rexruntime.dll','bin/TracyClient.dll','game/default.xex')+@($runtimeFiles|ForEach-Object{$_.RelativePath})
    Write-Text (Join-Path $root 'bundle-files.sha256') ((@($immutable|Sort-Object|ForEach-Object{"$(Hash (Join-Path $root $_)) *$_"})-join"`n")+"`n")
    $profile=Join-Path $root 'user/B13EBABEBABEBABE';$preference=Join-Path $profile 'local-display-name.txt'
    $before=Tree $profile
    if($HistoryBoundary){
        if($LaunchOnly){throw 'HistoryBoundary and LaunchOnly are exclusive.'}
        Assert-Exit (Invoke-Fixture -Arguments @('--set-profile-name=History_1')) $true
        for($index=0;$index-lt31;$index++) { Assert-Exit (Invoke-Fixture) $true }
        $history=@(Get-ChildItem -LiteralPath (Join-Path $root 'results') -Directory)
        if($history.Count-ne32-or@(Children).Count-ne31){throw 'Did not reach exact combined history boundary.'}
        $historyBefore=Tree (Join-Path $root 'results')
        Reject -Arguments @('--set-profile-name=History_2')
        Reject -Arguments @()
        Assert-Exit (Invoke-Fixture -Arguments @('--set-profile-name=History_1')) $true
        if((Tree (Join-Path $root 'results'))-cne$historyBefore){throw 'Full-history rejection/idempotent rename altered provenance.'}
        $profileBefore=Tree $profile
        $historiesBefore=@($history|Sort-Object Name|ForEach-Object{$_.Name+':'+(Tree $_.FullName)})-join"`n"
        Assert-Exit (Invoke-Fixture -Arguments @('--verify-only')) $true
        $historiesAfter=@($history|Sort-Object Name|ForEach-Object{$_.Name+':'+(Tree $_.FullName)})-join"`n"
        if((Tree $profile)-cne$profileBefore-or$historiesAfter-cne$historiesBefore){throw 'Verification at capacity changed profile/history.'}
        # Reconstruct an interrupted last session using only this owned fixture.
        $last=@($history|Where-Object Name -Like 'session-*'|Sort-Object Name)[-1].FullName
        if(-not$last.StartsWith($testRoot+'\',[StringComparison]::OrdinalIgnoreCase)-or(Test-Path -LiteralPath ($last+'.partial'))){throw 'Unsafe synthetic interruption target.'}
        Move-Item -LiteralPath $last -Destination ($last+'.partial')
        Reject -Arguments @()
        $recovered=Get-Content -LiteralPath (Join-Path $last 'result.json') -Raw|ConvertFrom-Json
        if($recovered.state-cne'interrupted'-or(Test-Path -LiteralPath ($last+'.partial'))-or@(Get-ChildItem -LiteralPath (Join-Path $root 'results') -Directory).Count-ne32){throw 'Capacity recovery lost history or started a new session.'}
        [pscustomobject]@{Passed=$true;Calls=$calls;Rejected=$negatives;ChildLaunches=31;CompletedRecords=32;HistoryPreserved=$true;VerifyAtCapacity=$true;RecoveryAtCapacity=$true;ProprietaryFilesUsed=$false}
        return
    }
    if($LaunchOnly){
        Assert-Exit (Invoke-Fixture) $true
        if(@(Children).Count-ne1-or(Tree $profile)-cne$before){throw 'Launch smoke lost child or changed synthetic profile.'}
        $sessions=@(Get-ChildItem -LiteralPath (Join-Path $root 'results') -Directory -Filter 'session-*')
        if($sessions.Count-ne1-or$sessions[0].Name.EndsWith('.partial')){throw 'Launch did not publish one complete session.'}
        $receipt=Get-Content -LiteralPath (Join-Path $sessions[0].FullName 'result.json') -Raw|ConvertFrom-Json
        if($receipt.save_snapshot_count-ne1-or$receipt.save_watcher_error_count-ne0){throw 'Launch did not preserve a recoverable snapshot.'}
        [pscustomobject]@{Passed=$true;RootLength=$root.Length;ChildLaunches=1;SaveSnapshots=1;ProfileUnchanged=$true;ProprietaryFilesUsed=$false}
        return
    }
    Assert-Exit (Invoke-Fixture -Arguments @('--set-profile-name=Orange Driver')) $true
    if(@(Children).Count-ne0-or[IO.File]::ReadAllText($preference)-cne'Orange Driver'){throw 'Standalone rename launched child or failed preference.'}
    $after=Tree $profile;Assert-Journal $before $after 1
    foreach($expectedCount in @(1,2)){
        Assert-Exit (Invoke-Fixture) $true
        $children=@(Children);if($children.Count-ne$expectedCount){throw 'Expected exactly one child per launch.'}
        foreach($child in $children){
            $record=Get-Content -LiteralPath $child.FullName -Raw|ConvertFrom-Json
            $names=@($record.argv|Where-Object{$_-clike'--local_user_display_name=*'})
            if($record.schema-cne'mcla-portable-child-argv-v1'-or$names.Count-ne1-or$names[0]-cne'--local_user_display_name=Orange Driver'-or@($record.argv|Where-Object{$_-ceq'Driver'}).Count){throw 'Display name was not forwarded as one complete argv entry.'}
        }
    }
    $before=Tree $profile
    Assert-Exit (Invoke-Fixture -Arguments @('--set-profile-name=Orange_2')) $true
    if(@(Children).Count-ne2-or[IO.File]::ReadAllText($preference)-cne'Orange_2'){throw 'Second standalone rename failed.'}
    Assert-Journal $before (Tree $profile) 2
    Assert-Exit (Invoke-Fixture -Arguments @('--set-profile-name=Orange_2')) $true
    if(@(Get-ChildItem -LiteralPath (Join-Path $root 'results') -Directory -Filter 'profile-rename-*').Count-ne2){throw 'Idempotent rename created extra journal.'}
    foreach($name in @('',' leading','trailing ','abcdefghijklmnop','a/b','a\b','a"b',"a`tb",([string][char]0x00E9))){Reject -Arguments @(('--set-profile-name='+$name))}
    Reject -Arguments @('--set-profile-name=A','--set-profile-name=B')
    Reject -Arguments @('--set-profile-name=A','--verify-only')
    Reject -Arguments @('--set-profile-name=A') -Environment @{rex_local_user_display_name='override'}
    foreach($bad in @('',"bad`n",'abcdefghijklmnop')){
        Write-Text $preference $bad
        Reject -Arguments @('--set-profile-name=Valid')
    }
    Write-Text $preference 'Orange_2'
    foreach($incomplete in @($save,$header)) {
        $preserved=[IO.File]::ReadAllBytes($incomplete)
        try {
            [IO.File]::WriteAllBytes($incomplete,[byte[]]@())
            Reject -Arguments @('--set-profile-name=Valid')
        } finally { [IO.File]::WriteAllBytes($incomplete,$preserved) }
    }
    Reject -Arguments @('--verify-adapter-only')
    Reject -Arguments @('--verify-adapter-only','--d3d12-adapter-name=fixture','--gpu-backend=vulkan')
    Reject -Arguments @('--verify-adapter-only','--d3d12-adapter-name=fixture') -Environment @{rex_d3d12_adapter='0'}
    Reject -Arguments @('--verify-adapter-only','--d3d12-adapter-name=fixture') -Environment @{rex_gpu_backend='d3d12'}
    if((Hash $save)-cne$saveHash-or(Hash $header)-cne$headerHash-or(Tree (Join-Path $root 'user/profile'))-cne$legacyGlobal-or(Tree (Join-Path $root 'user/545407F8'))-cne$legacyTitle){throw 'Rename changed save/header/legacy settings.'}
    if(Test-Path -LiteralPath (Join-Path $root 'bundle.lock')){throw 'Fixture left launcher lock.'}
    # Consume actual native receipts with the production lineage interpreter,
    # not just a synthetic approximation of the native schema.
    $nativeManifest=[pscustomobject]@{bundle_id=$id;immutable_file_count=$immutable.Count}
    $edges=@(Get-ChildItem -LiteralPath (Join-Path $root 'results') -Directory -Filter 'profile-rename-*' | ForEach-Object {
        Get-PortableReturnedProfileRename $_.FullName $nativeManifest
    })
    $seed=@($edges | Where-Object {(Get-PortableProfileDisplayName $_.StartProfile)-ceq'User'})[0].StartProfile
    $edges+=@(Get-ChildItem -LiteralPath (Join-Path $root 'results') -Directory -Filter 'session-*' | ForEach-Object {
        Get-PortableReturnedSession $_.FullName $nativeManifest
    })
    $live=Get-PortableProfile (Join-Path $root 'user')
    $tip=Resolve-PortableRenamedSessionTip $edges $live $seed
    if($tip.RootProfile.Sha256-cne$seed.Sha256){throw 'Actual native rename lineage lost its seed.'}
    $archived=Save-PortableProfileArchive $testRoot $live ([pscustomobject]@{fixture='native-profile-name'})
    if((Get-PortableProfileDisplayName (Get-PortableProfileArchive $testRoot $archived))-cne'Orange_2'){throw 'Actual native name did not survive archive.'}
    [pscustomobject]@{Passed=$true;Calls=$calls;Rejected=$negatives;ChildLaunches=@(Children).Count;RenameJournals=2;FullSnapshots=$true;NativeLineageAndArchive=$true;LegacySettingsPreserved=$true;SpaceArgumentPreserved=$true;ProprietaryFilesUsed=$false}
}finally{
    # Exact newly-created fixture tree, already validated under private/evidence.
    if(Test-Path -LiteralPath $testRoot){Remove-Item -LiteralPath $testRoot -Recurse -Force}
}
