[CmdletBinding()]
param([switch]$NativeLauncher)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Portable-SaveLineage.ps1')
. (Join-Path $PSScriptRoot 'Portable-CppRuntime.ps1')
$cppPolicy=Get-PortableCppRuntimePolicy
$cppFiles=@(Get-PortableCppRuntimeFiles $cppPolicy)
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$testRoot=Join-Path $repo ('private\evidence\M7-009\fixture-'+[guid]::NewGuid().ToString('N'))
$utf8=[Text.UTF8Encoding]::new($false)
$script:checks=0
function Put([string]$Path,[string]$Value) { [IO.Directory]::CreateDirectory((Split-Path -Parent $Path))|Out-Null; [IO.File]::WriteAllText($Path,$Value,$utf8) }
function Json([string]$Path,$Value) { Put $Path ($Value|ConvertTo-Json -Depth 20) }
function Check([bool]$Value,[string]$Label) { if(-not $Value){throw "FAILED: $Label"};$script:checks++ }
function Reject([scriptblock]$Action,[string]$Pattern) { $caught=$null;try{& $Action|Out-Null}catch{$caught=$_.Exception.Message};if(-not $caught -or $caught -notmatch $Pattern){throw "Expected rejection /$Pattern/; got '$caught'"};$script:checks++ }
function Profile([string]$Root,[string]$Value) { Put (Join-Path $Root $script:PortableSavePath) $Value;Put (Join-Path $Root $script:PortableHeaderPath) 'header';Put (Join-Path $Root ($script:PortableProfileName+'/photo.jpg')) ('photo-'+$Value);Get-PortableProfile $Root }
function Lock-Manifest([string]$Root) {
    $paths=@('Launch-MCLA.exe','bundle-id.txt','bundle-manifest.json','game-manifest.json','bin/mcla.exe','bin/mcla_crash_handler.exe','bin/mcla.toml','bin/rexgpu-xenos.dll','bin/rexruntime.dll','bin/TracyClient.dll','game/default.xex')
    $manifest=Get-Content -LiteralPath (Join-Path $Root 'bundle-manifest.json') -Raw|ConvertFrom-Json
    if($manifest.schema -ceq 'mcla-portable-campaign-bundle-v2'){$paths+=@($cppFiles|ForEach-Object{$_.RelativePath})}
    Put (Join-Path $Root 'bundle-files.sha256') ((@($paths|Sort-Object|ForEach-Object { '{0} *{1}' -f (Get-PortableSha256 (Join-Path $Root $_)),$_ }) -join "`n")+"`n")
}
function Bundle([string]$Root,$Seed,[string]$Id,[string]$SeedRelative,[string[]]$Ancestors=@(),[switch]$CppRuntime) {
    foreach($name in @('bin','game','user','cache','logs','diagnostics','results','update','metadata')){[IO.Directory]::CreateDirectory((Join-Path $Root $name))|Out-Null}
    Copy-Item -LiteralPath (Join-Path $Seed.Root $script:PortableProfileName) -Destination (Join-Path $Root 'user') -Recurse
    foreach($file in @('Launch-MCLA.exe','bin/mcla.exe','bin/mcla_crash_handler.exe','bin/rexgpu-xenos.dll','bin/rexruntime.dll','bin/TracyClient.dll','game/default.xex')){Put (Join-Path $Root $file) ('fixture-'+$file)}
    Put (Join-Path $Root 'bin/mcla.toml') ('mcla_diagnostics_root = "diagnostics"','log_file = "logs/mcla.log"','fullscreen = true','game_data_root = ""','user_data_root = ""','update_data_root = ""','cache_root = ""','metadata_root = ""' -join "`n")
    $g=Join-Path $Root 'game/default.xex'
    Json (Join-Path $Root 'game-manifest.json') ([ordered]@{SchemaVersion=1;SourceIsoSha256=('0'*64);FileCount=1;PayloadBytes=[long](Get-Item $g).Length;Files=@([ordered]@{Path='default.xex';Size=[long](Get-Item $g).Length;Sha256=Get-PortableSha256 $g})})
    Put (Join-Path $Root 'bundle-id.txt') $Id
    $manifest=[ordered]@{
        schema='mcla-portable-campaign-bundle-v1';task='M7-016';bundle_id=$Id;project_version='0.14.0.0';sdk_version='0.18.0.1';sdk_commit='9098e122afa116f4316d6cec47e561ebc5186974';created_utc=[DateTime]::UtcNow.ToString('O');platform='windows-proton';architecture='x86_64';launcher='Launch-MCLA.exe';window_mode=[ordered]@{startup_fullscreen=$true;alt_enter_toggle=$true;left_double_click_toggle=$true};immutable_file_count=11
        game=[ordered]@{manifest_sha256=Get-PortableSha256 (Join-Path $Root 'game-manifest.json');file_count=1;payload_bytes=[long](Get-Item $g).Length}
        selected_save=[ordered]@{source_snapshot=$SeedRelative;save_relative=$script:PortableSavePath;header_relative=$script:PortableHeaderPath;save_sha256=$Seed.SaveSha256;header_sha256=$Seed.HeaderSha256;profile_sha256=$Seed.Sha256;ancestor_profile_sha256=@($Ancestors)}
        writable_roots=@('user','cache','logs','diagnostics','results','update','metadata');retention=[ordered]@{completed_sessions=32;save_snapshots_per_session=32};private_content=[ordered]@{game_and_save_private=$true;source_iso_included=$false}
    }
    if($CppRuntime){
        $manifest.schema='mcla-portable-campaign-bundle-v2';$manifest.immutable_file_count=18
        $manifest.sdk_version='0.20.0.0';$manifest.sdk_commit='5dd58f894bf34b9ea85e59d3903c7f0e8044d853'
        $manifest.cpp_runtime=[ordered]@{policy=$cppPolicy.policy;version=$cppPolicy.version}
        foreach($entry in $cppFiles){Put (Join-Path $Root $entry.RelativePath) ('fixture-'+$entry.RelativePath)}
    }
    Json (Join-Path $Root 'bundle-manifest.json') $manifest;Lock-Manifest $Root
}
function NamedProfile([string]$Root,$Source,[string]$Name) {
    $null=Copy-PortableTreeVerified $Source $Root @($script:PortableProfileName)
    Put (Join-Path $Root $script:PortableDisplayNamePath) $Name
    Get-PortableProfile $Root
}
function RenameTransition([string]$Root,[string]$Stamp,$From,$To,[switch]$KeepLive) {
    $transitionId='profile-rename-'+$Stamp+'-340';$dir=Join-Path $Root ('results/'+$transitionId)
    $null=Copy-PortableTreeVerified $From (Join-Path $dir 'before') @($script:PortableProfileName)
    $null=Copy-PortableTreeVerified $To (Join-Path $dir 'after') @($script:PortableProfileName)
    $manifest=Get-Content -LiteralPath (Join-Path $Root 'bundle-manifest.json') -Raw|ConvertFrom-Json
    Json (Join-Path $dir 'rename.json') ([ordered]@{schema='mcla-portable-profile-rename-v1';bundle_id=$manifest.bundle_id;transition_id=$transitionId;completed_utc=$Stamp;verified_file_count=$manifest.immutable_file_count})
    if(-not $KeepLive){
        $user=Assert-PortablePath $testRoot (Join-Path $Root 'user');Remove-Item -LiteralPath $user -Recurse -Force
        $null=Copy-PortableTreeVerified $To $user @($script:PortableProfileName)
    }
    return $dir
}
function Session([string]$Root,[string]$Stamp,$From,$To,[string]$Kind='complete',[long]$ExitCode=0) {
    $id=[IO.File]::ReadAllText((Join-Path $Root 'bundle-id.txt'));$sessionId='session-'+$Stamp+'-340';$dir=Join-Path $Root ('results/'+$sessionId)
    Json (Join-Path $dir 'session.json') ([ordered]@{schema='mcla-portable-session-v1';bundle_id=$id;session_id=$sessionId;started_utc=$Stamp;mode='gameplay';state='running'})
    $profiles=@($From);if($From.Sha256 -cne $To.Sha256){$profiles+= $To}
    foreach($profile in $profiles){$dst=Join-Path $dir ('saves/'+$profile.SaveSha256.Substring(0,16));$null=Copy-PortableTreeVerified $profile $dst @($script:PortableProfileName);Json (Join-Path $dst 'snapshot.json') ([ordered]@{schema='mcla-portable-save-snapshot-v1';captured_utc=$Stamp;reason=$(if($profile.Sha256 -ceq $From.Sha256){'launcher-start'}else{'process-exit'});save_sha256=$profile.SaveSha256;header_sha256=$profile.HeaderSha256;complete_profile_tree=$true})}
    $bundleManifest=Get-Content -LiteralPath (Join-Path $Root 'bundle-manifest.json') -Raw|ConvertFrom-Json
    $result=[ordered]@{schema='mcla-portable-session-result-v1';bundle_id=$id;session_id=$sessionId;started_utc=$Stamp;completed_utc=$Stamp;mode='gameplay';exit_code=$ExitCode;verified_file_count=$bundleManifest.immutable_file_count;save_snapshot_count=$profiles.Count;save_watcher_error_count=0;final_save_sha256=$To.SaveSha256;final_header_sha256=$To.HeaderSha256;state='complete'}
    if($Kind -eq 'interrupted'){$result.schema='mcla-portable-interrupted-session-result-v1';$result.state='interrupted';$result.exit_code=$null;$result.Remove('completed_utc');$result.Remove('save_watcher_error_count');$result.recovered_utc=$Stamp;$result.interruption_reason='launcher-terminated-before-session-finalization'}
    Json (Join-Path $dir 'result.json') $result
    # These are isolated fixture paths, never owner profiles.
    $user=Assert-PortablePath $testRoot (Join-Path $Root 'user');Remove-Item -LiteralPath $user -Recurse -Force
    $null=Copy-PortableTreeVerified $To $user @($script:PortableProfileName)
}
try {
    [IO.Directory]::CreateDirectory($testRoot)|Out-Null
    $a=Profile (Join-Path $testRoot 'a') 'a';$b=Profile (Join-Path $testRoot 'b') 'b';$c=Profile (Join-Path $testRoot 'c') 'c'
    $ca=[pscustomobject]@{Source='A';Profile=$a};$cb=[pscustomobject]@{Source='B';Profile=$b};$cc=[pscustomobject]@{Source='C';Profile=$c}
    $ab=[pscustomobject]@{From=$a.Sha256;To=$b.Sha256};$bc=[pscustomobject]@{From=$b.Sha256;To=$c.Sha256};$ba=[pscustomobject]@{From=$b.Sha256;To=$a.Sha256}
    Check ((Resolve-PortableLineage @($ca,$cb,$cc) @($bc,$ab) '').Source -ceq 'C') 'descendant graph'
    Check ((Resolve-PortableLineage @($ca,$ca) @() '').Profile.Sha256 -ceq $a.Sha256) 'identical trees'
    Reject {Resolve-PortableLineage @($ca,$cb) @() ''} 'ambiguous'
    Check ((Resolve-PortableLineage @($ca,$cb) @() 'A').Source -ceq 'A') 'explicit preserved branch'
    Reject {Resolve-PortableLineage @($ca,$cb) @($ab,$ba) ''} 'cycle'
    Reject {Resolve-PortableLineage @($cc) @($ab,$ba,$bc) ''} 'cycle'
    Reject {Resolve-PortableLineage @($ca,$cb) @($ab,$ba) 'A'} 'cycle'
    Reject {Get-PortableTree $a.Root @($script:PortableProfileName) -MaximumBytes 1} 'byte bound'
    Put (Join-Path $a.Root ($script:PortableProfileName+'/bad.sync-conflict-1')) 'x'
    Reject {Get-PortableProfile $a.Root} 'conflicted'
    Remove-Item -LiteralPath (Join-Path $a.Root ($script:PortableProfileName+'/bad.sync-conflict-1'))
    $drift=Profile (Join-Path $testRoot 'drift') 'drift';Put (Join-Path $drift.Root ($script:PortableProfileName+'/photo.jpg')) 'changed'
    Reject {Copy-PortableTreeVerified $drift (Join-Path $testRoot 'copy.partial') @($script:PortableProfileName)} 'changed during copy'
    $fakeRepo=Join-Path $testRoot 'repo';[IO.Directory]::CreateDirectory($fakeRepo)|Out-Null
    $archive=Save-PortableProfileArchive $fakeRepo $a @{source='fixture'}
    Check ((Get-PortableProfile $archive).Sha256 -ceq $a.Sha256) 'complete archive'
    Check ((Save-PortableProfileArchive $fakeRepo $a @{source='fixture'}) -ceq $archive) 'verified archive reuse'
    $seedRelative='private/save-archive/M7-009/'+$a.Sha256
    $root=Join-Path $fakeRepo 'private/bundles/test';$id='mcla-0.14.0.0-win64-AAAAAAAAAAAAAAAA-BBBBBBBBBBBBBBBB-CCCCCCCCCCCCCCCC';$bundle=Join-Path $root $id
    Bundle $bundle $a $id $seedRelative;Put (Join-Path $root 'current.txt') ($id+"`n")
    if ($NativeLauncher) {
        $launcher=Join-Path $repo 'out/build/win-amd64-release/Launch-MCLA.exe'
        if(-not (Test-Path -LiteralPath $launcher -PathType Leaf)){throw 'Build the native Release launcher before this optional fixture.'}
        # A newly built launcher intentionally requires v2 CRT placements. Do
        # not overwrite the historical v1 parent used by the return tests.
        $nativeBundle=Join-Path $testRoot 'native-successor'
        Bundle $nativeBundle $a $id $seedRelative -CppRuntime
        $toolchain=& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1')
        $cppSource=Resolve-PortableCppRuntimeSource -VisualStudioRoot $toolchain.VisualStudioRoot
        foreach($entry in $cppFiles){Copy-Item -LiteralPath (Join-Path $cppSource.SourceDirectory $entry.Name) -Destination (Join-Path $nativeBundle $entry.RelativePath) -Force}
        Copy-Item -LiteralPath $launcher -Destination (Join-Path $nativeBundle 'Launch-MCLA.exe') -Force;Lock-Manifest $nativeBundle
        $process=Start-Process -FilePath (Join-Path $nativeBundle 'Launch-MCLA.exe') -ArgumentList '--verify-only' -WorkingDirectory $nativeBundle -WindowStyle Hidden -PassThru
        $null=$process.Handle
        if(-not $process.WaitForExit(15000)){$process.Kill();$process.WaitForExit();throw 'Native fixture verification timed out.'}
        Check ($process.ExitCode -eq 0) 'native current launcher accepts v2 successor manifest fields'
    }
    Check ((Get-PortableBundleReturn $fakeRepo $bundle -Fixture).Profile.Sha256 -ceq $a.Sha256) 'pristine return'
    Json (Join-Path $bundle 'results/portable-verification-20260904T195959Z.json') ([ordered]@{schema='mcla-portable-verification-v1';bundle_id=$id;verified_utc='20260904T200000Z';verified_file_count=11;relocatable_root_verified=$true;decision='portable-bundle-integrity-pass'})
    Check ((Get-PortableBundleReturn $fakeRepo $bundle -Fixture).Profile.Sha256 -ceq $a.Sha256) 'native verify-only record without gameplay'
    Session $bundle '20260904T200000Z' $a $b
    Check ((Get-PortableBundleReturn $fakeRepo $bundle -Fixture).Profile.Sha256 -ceq $b.Sha256) 'progressed return'
    foreach($recordName in @('session.json','result.json')) {
        $recordPath=Join-Path $bundle ('results/session-20260904T200000Z-340/'+$recordName)
        $record=Get-Content -LiteralPath $recordPath -Raw|ConvertFrom-Json;$record.started_utc='20260904T200001Z';Json $recordPath $record
    }
    Check ((Get-PortableBundleReturn $fakeRepo $bundle -Fixture).Profile.Sha256 -ceq $b.Sha256) 'native separate clock readings cross a second boundary'
    $publicationCandidate=Join-Path $fakeRepo 'private/publication-fixture'
    Bundle $publicationCandidate $a $id $seedRelative -CppRuntime
    Check ((& (Join-Path $PSScriptRoot 'verify-portable-campaign-bundle.ps1') -BundleRoot $publicationCandidate -Fixture).Passed) 'v2 pristine successor is a valid publication'
    Session $publicationCandidate '20260904T200000Z' $a $b
    Reject {& (Join-Path $PSScriptRoot 'verify-portable-campaign-bundle.ps1') -BundleRoot $publicationCandidate -Fixture} 'physical identity drifted'
    Session $bundle '20260903T200000Z' $b $c 'interrupted'
    Check ((Get-PortableBundleReturn $fakeRepo $bundle -Fixture).Profile.Sha256 -ceq $c.Sha256) 'clock rollback interrupted descendant'
    Put (Join-Path $bundle $script:PortableSavePath.Insert(0,'user/')) 'b'
    Put (Join-Path $bundle ('user/'+$script:PortableProfileName+'/photo.jpg')) 'photo-b'
    Reject {Get-PortableBundleReturn $fakeRepo $bundle -Fixture} 'profile|terminal|tip|snapshot'
    Put (Join-Path $bundle $script:PortableSavePath.Insert(0,'user/')) 'c'
    Put (Join-Path $bundle ('user/'+$script:PortableProfileName+'/photo.jpg')) 'photo-c'
    $startRecord=Join-Path $bundle ('results/session-20260904T200000Z-340/saves/'+$a.SaveSha256.Substring(0,16)+'/snapshot.json')
    $startText=[IO.File]::ReadAllText($startRecord);Put $startRecord $startText.Replace('launcher-start','process-exit')
    Reject {Get-PortableBundleReturn $fakeRepo $bundle -Fixture} 'start and final'
    Put $startRecord $startText
    $seal=Get-PortableBundleReturn $fakeRepo $bundle -Fixture
    Put (Join-Path $bundle 'logs/late.log') 'late'
    Reject {Save-PortableReturnArchive $fakeRepo $seal} 'changed after interpretation'
    Remove-Item -LiteralPath (Join-Path $bundle 'logs/late.log')
    $state=Enter-PortablePublication $fakeRepo $root
    try {
        Reject {Enter-PortablePublication $fakeRepo $root} 'file|process|exists'
        $plan=New-PortableSavePlan $fakeRepo $state $a.Root '' -Fixture
        Check ($plan.Profile.Sha256 -ceq $c.Sha256) 'returned progress supersedes seed'
        Check ((Get-PortableProfile $plan.Snapshot).Sha256 -ceq $c.Sha256) 'successor gets entire profile'
        Check ($plan.Receipts.Count -eq 1) 'return archive receipt'
        $id2=$id.Replace('AAAAAAAAAAAAAAAA','DDDDDDDDDDDDDDDD');$next=Join-Path $root $id2
        Bundle $next (Get-PortableProfile $plan.Snapshot) $id2 ('private/save-archive/M7-009/'+$c.Sha256) $plan.Ancestors -CppRuntime
        Add-PortableWriterLock $state (Join-Path $next 'bundle.lock')
        Check ((Get-PortableBundleReturn $fakeRepo $next -Fixture).Profile.Sha256 -ceq $c.Sha256) 'successor return provenance'
        Reject {Remove-PortableArchivedBundles $state $next @()} 'current.txt'
        $ptr=Join-Path $root 'current.txt';$old=[IO.File]::ReadAllText($ptr);Put $ptr 'changed'
        Reject {Assert-PortablePublicationUnchanged $state $plan.Receipts} 'current.txt changed'
        Put $ptr $old
        Put (Join-Path $bundle 'logs/new.log') 'new runtime writer'
        Reject {Assert-PortablePublicationUnchanged $state $plan.Receipts} 'changed'
        Remove-Item -LiteralPath (Join-Path $bundle 'logs/new.log')
        Assert-PortablePublicationUnchanged $state $plan.Receipts
        # Force an OS-level atomic-replace failure; the old advertised child survives.
        $handle=[IO.File]::Open($ptr,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
        try {Reject {Publish-PortablePointer $state $id2} 'process|file|access'} finally {$handle.Dispose()}
        Check ([IO.File]::ReadAllText($ptr) -ceq $old) 'failed publication preserves old pointer'
        Check (Test-Path -LiteralPath $bundle) 'failed publication preserves old child'
        foreach($incomplete in @(Get-ChildItem -LiteralPath $root -File -Filter '.current.*.partial')){$safe=Assert-PortablePath $testRoot $incomplete.FullName;Remove-Item -LiteralPath $safe}
        Publish-PortablePointer $state $id2
        Reject {Remove-PortableArchivedBundles $state $next @()} 'verified archive'
        $external=Join-Path $testRoot 'external';[IO.Directory]::CreateDirectory($external)|Out-Null;Put (Join-Path $external 'sentinel') 'keep'
        $junction=Join-Path $bundle 'cache/junction';New-Item -ItemType Junction -Path $junction -Value $external|Out-Null
        try {Reject {Remove-PortableArchivedBundles $state $next $plan.Receipts} 'reparse';Check ([IO.File]::ReadAllText((Join-Path $external 'sentinel')) -ceq 'keep') 'retention never traverses junction'} finally {[IO.Directory]::Delete($junction)}
        Check ((Remove-PortableArchivedBundles $state $next $plan.Receipts) -eq 1) 'archive-gated pruning'
        Check (Test-Path -LiteralPath $plan.Receipts[0].Archive) 'archive survives pruning'
    } finally {Exit-PortablePublication $state}
    Check (-not (Test-Path -LiteralPath (Join-Path $root '.publication.lock'))) 'owned locks released'
    $d=Profile (Join-Path $testRoot 'd') 'd'
    Session $next '20260902T200000Z' $c $d 'complete' 3221225477
    $nextState=Enter-PortablePublication $fakeRepo $root
    try {
        $nextPlan=New-PortableSavePlan $fakeRepo $nextState $a.Root '' -Fixture
        Check ($nextPlan.Profile.Sha256 -ceq $d.Sha256) 'ancestry survives predecessor pruning with stale desktop baseline'
        Check ($nextPlan.Ancestors -ccontains $a.Sha256) 'original ancestry carried forward'
        Check ((Get-PortableBundleReturn $fakeRepo $next -Fixture).Profile.Sha256 -ceq $d.Sha256) 'crash exit preserves independently verified save'
    } finally {Exit-PortablePublication $nextState}
    # Explicit import is an archived profile, not a fabricated portable session
    # or a claim that an unobserved local pilot descends from this publication.
    $imported=Profile (Join-Path $testRoot 'imported') 'local-pilot-progress'
    $importArchive=Save-PortableProfileArchive $fakeRepo $imported @{source='explicit-local-pilot-fixture'}
    $importRecord=Join-Path $importArchive 'profile.json'
    $importRecordText=[IO.File]::ReadAllText($importRecord)
    $importState=Enter-PortablePublication $fakeRepo $root
    try {
        Reject {New-PortableSavePlan $fakeRepo $importState $a.Root 'legacy' -SaveArchive $importArchive -Fixture} 'mutually exclusive'
        foreach ($relativeArchive in @('private/save-archive/M7-009/fixture', 'C:private/save-archive/M7-009/fixture', '\private\save-archive\M7-009\fixture', '/private/save-archive/M7-009/fixture')) {
            Reject {New-PortableSavePlan $fakeRepo $importState $a.Root '' -SaveArchive $relativeArchive -Fixture} 'absolute'
        }
        Reject {New-PortableSavePlan $fakeRepo $importState $a.Root '' -SaveArchive $imported.Root -Fixture} 'escaped'
        Reject {New-PortableSavePlan $fakeRepo $importState $a.Root '' -SaveArchive (Join-Path (Split-Path -Parent $importArchive) ('F'*64)) -Fixture} 'exist|find'
        Reject {New-PortableSavePlan $fakeRepo $importState $a.Root '' -SaveArchive (Split-Path -Parent $importArchive) -Fixture} 'identity'
        $importPlan=New-PortableSavePlan $fakeRepo $importState $a.Root '' -SaveArchive $importArchive -Fixture
        Check ($importPlan.Source -ceq ('archive:'+$imported.Sha256)) 'explicit import is visibly distinct from legacy/native return'
        Check ($importPlan.Profile.Sha256 -ceq $imported.Sha256 -and $importPlan.Snapshot -ceq $importArchive) 'exact full archived profile selected'
        Check (@($importPlan.Ancestors).Count -eq 0) 'unobserved imported ancestry never invented'
        Check ($importPlan.Receipts.Count -eq 1 -and (Test-Path -LiteralPath $importPlan.Receipts[0].Archive)) 'unselected portable branch preserved'
        Check ((Get-PortableProfileArchive $fakeRepo (Join-Path (Split-Path -Parent $importArchive) $d.Sha256)).Sha256 -ceq $d.Sha256) 'unselected progressed profile archived'
        Check ((Get-PortableProfileArchive $fakeRepo $archive).Sha256 -ceq $a.Sha256) 'unselected legacy profile preserved'
        Assert-PortableSavePlanUnchanged $fakeRepo $importPlan
        Check ([IO.File]::ReadAllText($importRecord) -ceq $importRecordText) 'explicit import does not rewrite archive provenance'
        $samePlan=New-PortableSavePlan $fakeRepo $importState $a.Root '' -SaveArchive (Join-Path (Split-Path -Parent $importArchive) $d.Sha256) -Fixture
        Check (@($samePlan.Ancestors).Count -eq 0) 'explicit root does not adopt edges merely because content matches'
        $defaultPlan=New-PortableSavePlan $fakeRepo $importState $a.Root '' -Fixture
        Check ($defaultPlan.Profile.Sha256 -ceq $d.Sha256) 'unselected archives never change default lineage selection'
        Check ([IO.File]::ReadAllText((Join-Path $root 'current.txt')).Trim() -ceq $id2) 'archive admission never publishes a pointer'
        $importSuccessor=Join-Path $testRoot 'explicit-import-successor'
        Bundle $importSuccessor $importPlan.Profile $id ('private/save-archive/M7-009/'+$imported.Sha256) $importPlan.Ancestors -CppRuntime
        Check ((& (Join-Path $PSScriptRoot 'verify-portable-campaign-bundle.ps1') -BundleRoot $importSuccessor -Fixture).Passed) 'explicit-root successor passes unchanged publication verifier'
        Check (@(Get-ChildItem -LiteralPath (Join-Path $importSuccessor 'results') -Force).Count -eq 0) 'explicit successor has no synthetic sessions'
        # Get-PortableBundleReturn requires a repository-contained root.
        $importReturnRoot=Join-Path $fakeRepo 'private/import-return-fixture'
        Bundle $importReturnRoot $importPlan.Profile $id ('private/save-archive/M7-009/'+$imported.Sha256) $importPlan.Ancestors -CppRuntime
        Check ((Get-PortableBundleReturn $fakeRepo $importReturnRoot -Fixture).Profile.Sha256 -ceq $imported.Sha256) 'explicit root is a valid pristine successor return'
        Put $importRecord ($importRecordText+"`n")
        Reject {Assert-PortableSavePlanUnchanged $fakeRepo $importPlan} 'changed'
        Put $importRecord $importRecordText
        $photo=Join-Path $importArchive ($script:PortableProfileName+'/photo.jpg')
        $photoText=[IO.File]::ReadAllText($photo);Put $photo 'tampered'
        Reject {New-PortableSavePlan $fakeRepo $importState $a.Root '' -SaveArchive $importArchive -Fixture} 'drifted'
        Reject {Assert-PortableSavePlanUnchanged $fakeRepo $importPlan} 'drifted|changed'
        Put $photo $photoText
        Put $importRecord '{}'
        Reject {New-PortableSavePlan $fakeRepo $importState $a.Root '' -SaveArchive $importArchive -Fixture} 'property|schema'
        Put $importRecord $importRecordText
        $importJunction=Join-Path (Split-Path -Parent $importArchive) ('E'*64)
        New-Item -ItemType Junction -Path $importJunction -Value $importArchive|Out-Null
        try {Reject {New-PortableSavePlan $fakeRepo $importState $a.Root '' -SaveArchive $importJunction -Fixture} 'reparse'} finally {[IO.Directory]::Delete($importJunction)}
        # Even an explicitly valid root must not bypass an invalid native return.
        $badResult=Join-Path $next 'results/unknown.json';Put $badResult '{}'
        try {Reject {New-PortableSavePlan $fakeRepo $importState $a.Root '' -SaveArchive $importArchive -Fixture} 'unrecognized|incomplete'} finally {Remove-Item -LiteralPath $badResult}
        Assert-PortableSavePlanUnchanged $fakeRepo $importPlan
        Assert-PortablePublicationUnchanged $importState $importPlan.Receipts
    } finally {Exit-PortablePublication $importState}
    Put (Join-Path $next 'bundle.lock') 'stale-remote-lock'
    Reject {Enter-PortablePublication $fakeRepo $root} 'file|exists|process'
    Check ([IO.File]::ReadAllText((Join-Path $next 'bundle.lock')) -ceq 'stale-remote-lock') 'stale lock is not removed'
    Remove-Item -LiteralPath (Join-Path $next 'bundle.lock')
    $legacyRoot=Join-Path $fakeRepo 'private/save-archive/M6-014/fixture/seed';$null=Copy-PortableTreeVerified $a $legacyRoot @($script:PortableProfileName)
    $legacyBundle=Join-Path $fakeRepo 'private/legacy-fixture'
    Bundle $legacyBundle $a $id 'private/save-archive/M6-014/fixture/seed'
    $legacyManifest=Get-Content -LiteralPath (Join-Path $legacyBundle 'bundle-manifest.json') -Raw|ConvertFrom-Json
    $legacyManifest.selected_save.PSObject.Properties.Remove('profile_sha256');$legacyManifest.selected_save.PSObject.Properties.Remove('ancestor_profile_sha256');$legacyManifest.sdk_version='0.9.0.0';$legacyManifest.sdk_commit='a'*40
    Json (Join-Path $legacyBundle 'bundle-manifest.json') $legacyManifest;Lock-Manifest $legacyBundle
    Session $legacyBundle '20260801T100000Z' $a $b
    Check (-not (Get-PortableBundleReturn $fakeRepo $legacyBundle -Fixture).ProvenSeed) 'historical two-file seed never invents whole-profile ancestry'
    Reject {& (Join-Path $PSScriptRoot 'verify-portable-campaign-bundle.ps1') -BundleRoot $legacyBundle -Fixture} 'Legacy v1 bundles are accepted only for returned-profile preservation'
    # Preserve the separate SDK-identity negative on the accepted publication
    # schema, so the new v1 gate cannot mask the intended rejection.
    $oldSdkManifest=Get-Content -LiteralPath (Join-Path $publicationCandidate 'bundle-manifest.json') -Raw|ConvertFrom-Json
    $oldSdkManifest.sdk_version='0.9.0.0';$oldSdkManifest.sdk_commit='a'*40
    Json (Join-Path $publicationCandidate 'bundle-manifest.json') $oldSdkManifest;Lock-Manifest $publicationCandidate
    Reject {& (Join-Path $PSScriptRoot 'verify-portable-campaign-bundle.ps1') -BundleRoot $publicationCandidate -Fixture} 'manifest identity'
    $m=Get-Content -LiteralPath (Join-Path $next 'bundle-manifest.json') -Raw|ConvertFrom-Json
    $m.selected_save.ancestor_profile_sha256=@($c.Sha256);Json (Join-Path $next 'bundle-manifest.json') $m;Lock-Manifest $next
    Reject {Get-PortableBundleReturn $fakeRepo $next -Fixture} 'self identity'
    # Name preferences are explicit administrative transitions, never invented
    # gameplay sessions. All copies and destructive mutations below are fixtures.
    $namedA=NamedProfile (Join-Path $testRoot 'named-a') $a 'Orange'
    $namedB=NamedProfile (Join-Path $testRoot 'named-b') $b 'Orange'
    $renamedB=NamedProfile (Join-Path $testRoot 'renamed-b') $namedB 'Race Driver'
    $renameRoot=Join-Path $fakeRepo 'private/bundles/rename-fixture'
    $renameBundle=Join-Path $renameRoot $id
    Bundle $renameBundle $a $id $seedRelative
    Put (Join-Path $renameRoot 'current.txt') ($id+"`n")
    $firstRename=RenameTransition $renameBundle '20261010T100000Z' $a $namedA
    $renameReturn=Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture
    Check ($renameReturn.Profile.Sha256 -ceq $namedA.Sha256 -and $renameReturn.Profile.SaveSha256 -ceq $a.SaveSha256) 'rename before first gameplay preserves seeded save'
    Check ($renameReturn.SessionId -ceq 'profile-rename-20261010T100000Z-340') 'rename tip explicitly identified without a gameplay claim'
    Session $renameBundle '20261010T100001Z' $namedA $namedB
    $lastRename=RenameTransition $renameBundle '20261009T100000Z' $namedB $renamedB
    $renameReturn=Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture
    Check ($renameReturn.Profile.Sha256 -ceq $renamedB.Sha256) 'rename gameplay rename follows content not clock order'
    $renameManifest=Get-Content -LiteralPath (Join-Path $renameBundle 'bundle-manifest.json') -Raw|ConvertFrom-Json
    $transition=Get-PortableReturnedProfileRename $lastRename $renameManifest
    Check ($transition.State -ceq 'profile-rename' -and $null -eq $transition.ExitCode) 'administrative transition has no fabricated game exit'
    $initialTransition=Get-PortableReturnedProfileRename $firstRename $renameManifest
    # Test the graph boundary directly: the native session reader separately
    # rejects same-save snapshot collisions, but quotient code must not rely on
    # that implementation detail to hide a different-hash gameplay transition.
    $collapsedGameplay=[pscustomobject]@{SessionId='session-20261010T100002Z-340';StartProfile=$a;FinalProfile=$namedA;State='complete';ExitCode=0}
    Reject {Resolve-PortableRenamedSessionTip @($initialTransition,$collapsedGameplay) $namedA $a} 'without an administrative transition'
    $mergeLeft=[pscustomobject]@{SessionId='session-20261010T100003Z-340';StartProfile=$namedA;FinalProfile=$b;State='complete';ExitCode=0}
    $mergeRight=[pscustomobject]@{SessionId='session-20261010T100004Z-340';StartProfile=$c;FinalProfile=$b;State='complete';ExitCode=0}
    Reject {Resolve-PortableRenamedSessionTip @($initialTransition,$mergeLeft,$mergeRight) $b $a} 'branches or merges'
    $renameState=Enter-PortablePublication $fakeRepo $renameRoot
    try {
        $renamePlan=New-PortableSavePlan $fakeRepo $renameState $a.Root '' -Fixture
        Check ($renamePlan.Profile.Sha256 -ceq $renamedB.Sha256) 'rename return selected as seed descendant'
        Check ((Get-PortableProfile $renamePlan.Snapshot).Sha256 -ceq $renamedB.Sha256) 'renamed full tree independently archived'
        $renameSuccessor=Join-Path $fakeRepo 'private/rename-successor'
        Bundle $renameSuccessor $renamePlan.Profile $id2 ('private/save-archive/M7-009/'+$renamedB.Sha256) $renamePlan.Ancestors -CppRuntime
        Check ((Get-PortableBundleReturn $fakeRepo $renameSuccessor -Fixture).Profile.Sha256 -ceq $renamedB.Sha256) 'successor retains exact renamed profile'
        Check (@(Get-ChildItem -LiteralPath (Join-Path $renameSuccessor 'results') -Force).Count -eq 0) 'successor does not fabricate rename or gameplay records'
    } finally {Exit-PortablePublication $renameState}
    $renameRecordPath=Join-Path $lastRename 'rename.json';$renameRecordText=[IO.File]::ReadAllText($renameRecordPath)
    foreach($renameMutation in @(
        {param($r) $r.schema='wrong'}, {param($r) $r.bundle_id='wrong'},
        {param($r) $r.transition_id='profile-rename-20261010T100000Z-340'},
        {param($r) $r.completed_utc='20261301T100000Z'}, {param($r) $r.verified_file_count=12},
        {param($r) $r.verified_file_count='11'}, {param($r) $r|Add-Member NoteProperty extra 'unrecognized'}
    )) {
        $renameRecord=$renameRecordText|ConvertFrom-Json;& $renameMutation $renameRecord;Json $renameRecordPath $renameRecord
        Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'identity|timestamp|integer|topology'
        Put $renameRecordPath $renameRecordText
    }
    foreach($renameRelative in @($script:PortableSavePath,$script:PortableHeaderPath,($script:PortableProfileName+'/photo.jpg'))) {
        $renameFile=Join-Path $lastRename ('after/'+$renameRelative);$original=[IO.File]::ReadAllText($renameFile)
        Put $renameFile 'unauthorized mutation'
        Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'outside its display-name'
        Put $renameFile $original
    }
    $preference=Join-Path $lastRename ('after/'+$script:PortableDisplayNamePath)
    foreach($badName in @('', ' Orange', 'Orange ', ('X'*16), "Bad`nName", 'Bad/Name', ([string][char]0xE9), ([string][char]0xFEFF+'Orange'))) {
        Put $preference $badName
        Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'preference is invalid'
    }
    Put $preference 'Orange'
    Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'no-op'
    Remove-Item -LiteralPath $preference
    Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'lacks its new'
    Put $preference 'Race Driver'
    $extraDirectory=Join-Path $lastRename ('after/'+$script:PortableProfileName+'/extra-empty')
    [IO.Directory]::CreateDirectory($extraDirectory)|Out-Null
    Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'outside its display-name'
    [IO.Directory]::Delete($extraDirectory)
    foreach($extraRelative in @('unexpected.json','before/unexpected.json')) {
        $extra=Join-Path $lastRename $extraRelative;Put $extra '{}'
        Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'topology'
        Remove-Item -LiteralPath $extra
    }
    $missingHeader=Join-Path $lastRename ('before/'+$script:PortableHeaderPath)
    $original=[IO.File]::ReadAllText($missingHeader);Remove-Item -LiteralPath $missingHeader
    Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'complete save/header'
    Put $missingHeader $original
    $partialRename=$lastRename+'.partial'
    [IO.Directory]::Move($lastRename,$partialRename)
    Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'incomplete'
    [IO.Directory]::Move($partialRename,$lastRename)
    $renameSeal=Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture
    Put $preference 'Another Driver'
    Reject {Save-PortableReturnArchive $fakeRepo $renameSeal} 'changed after interpretation'
    Put $preference 'Race Driver'
    $namedC=NamedProfile (Join-Path $testRoot 'named-c') $c 'Disconnected'
    $disconnectedRename=RenameTransition $renameBundle '20261010T110000Z' $c $namedC -KeepLive
    Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'disconnected'
    $safeRename=Assert-PortablePath $testRoot $disconnectedRename;Remove-Item -LiteralPath $safeRename -Recurse -Force
    # A copied directory is not a new record: its internal identity still binds
    # the original transition ID and must fail before graph interpretation.
    $forgedRename=Join-Path $renameBundle 'results/profile-rename-20261010T120000Z-340'
    Copy-Item -LiteralPath $lastRename -Destination $forgedRename -Recurse
    Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'identity'
    $safeRename=Assert-PortablePath $testRoot $forgedRename;Remove-Item -LiteralPath $safeRename -Recurse -Force
    $unrecordedPreference=Join-Path $renameBundle ('user/'+$script:PortableDisplayNamePath)
    Put $unrecordedPreference 'Unrecorded'
    Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'recorded session or rename tip'
    Put $unrecordedPreference 'Race Driver'
    $unreachableB=NamedProfile (Join-Path $testRoot 'unreachable-b') $b 'Unreachable'
    $backwardRename=RenameTransition $renameBundle '20261010T130000Z' $unreachableB $renamedB -KeepLive
    Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'directed-reachable'
    $safeRename=Assert-PortablePath $testRoot $backwardRename;Remove-Item -LiteralPath $safeRename -Recurse -Force
    # Administrative equivalence must not hide a branch or cycle in gameplay.
    Session $renameBundle '20261010T140000Z' $namedA $namedC
    Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'branches or merges'
    $badSession=Assert-PortablePath $testRoot (Join-Path $renameBundle 'results/session-20261010T140000Z-340');Remove-Item -LiteralPath $badSession -Recurse -Force
    Session $renameBundle '20261010T150000Z' $renamedB $namedA
    Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'cyclic'
    $badSession=Assert-PortablePath $testRoot (Join-Path $renameBundle 'results/session-20261010T150000Z-340');Remove-Item -LiteralPath $badSession -Recurse -Force
    $renameUser=Assert-PortablePath $testRoot (Join-Path $renameBundle 'user');Remove-Item -LiteralPath $renameUser -Recurse -Force
    $null=Copy-PortableTreeVerified $renamedB $renameUser @($script:PortableProfileName)
    # Count sessions and administrative records together before interpreting
    # any excess entry (these empty complete-looking directories cannot pass).
    $overflow=@()
    for($i=0;$i -lt 30;$i++) {
        $extra=Join-Path $renameBundle ('results/profile-rename-20261010T160000Z-'+(1000+$i))
        [IO.Directory]::CreateDirectory($extra)|Out-Null;$overflow+=$extra
    }
    Reject {Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture} 'combined retention bound'
    foreach($extra in $overflow){[IO.Directory]::Delete($extra)}
    Check ((Get-PortableBundleReturn $fakeRepo $renameBundle -Fixture).Profile.Sha256 -ceq $renamedB.Sha256) 'negative fixtures restore accepted rename chain'
    $renameBackBundle=Join-Path $fakeRepo 'private/rename-back-fixture'
    Bundle $renameBackBundle $a $id $seedRelative
    $namedA2=NamedProfile (Join-Path $testRoot 'named-a2') $a 'Second Name'
    $namedUser=NamedProfile (Join-Path $testRoot 'named-user') $a 'User'
    $null=RenameTransition $renameBackBundle '20261010T170000Z' $a $namedA
    $null=RenameTransition $renameBackBundle '20261010T170001Z' $namedA $namedA2
    $null=RenameTransition $renameBackBundle '20261010T170002Z' $namedA2 $namedA
    Check ((Get-PortableBundleReturn $fakeRepo $renameBackBundle -Fixture).Profile.Sha256 -ceq $namedA.Sha256) 'name A to B to A does not invent a gameplay cycle'
    $null=RenameTransition $renameBackBundle '20261010T170003Z' $namedA $namedA2
    $null=RenameTransition $renameBackBundle '20261010T170004Z' $namedA2 $namedUser
    $null=RenameTransition $renameBackBundle '20261010T170005Z' $namedUser $namedA
    Check ((Get-PortableBundleReturn $fakeRepo $renameBackBundle -Fixture).Profile.Sha256 -ceq $namedA.Sha256) 'name A to B to User to A is a reachable administrative history'
    $noopBundle=Join-Path $fakeRepo 'private/rename-noop-fixture'
    Bundle $noopBundle $a $id $seedRelative
    $null=RenameTransition $noopBundle '20261010T180000Z' $a $namedUser
    Reject {Get-PortableBundleReturn $fakeRepo $noopBundle -Fixture} 'no-op'
    $record=Join-Path $archive 'profile.json';Put $record '{}'
    Reject {Save-PortableProfileArchive $fakeRepo $a @{source='fixture'}} 'archive|property|schema|profile'
    [pscustomobject]@{Passed=$true;Checks=$script:checks;Scope='isolated fixtures; no owner save/runtime touched'}
} finally {
    $target=Assert-PortablePath (Join-Path $repo 'private/evidence/M7-009') $testRoot
    if((Split-Path -Leaf $target) -cmatch '^fixture-[0-9a-f]{32}$' -and (Test-Path -LiteralPath $target)){Remove-Item -LiteralPath $target -Recurse -Force}
}
