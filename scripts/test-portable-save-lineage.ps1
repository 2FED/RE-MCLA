[CmdletBinding()]
param([switch]$NativeLauncher)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Portable-SaveLineage.ps1')
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
    Put (Join-Path $Root 'bundle-files.sha256') ((@($paths|Sort-Object|ForEach-Object { '{0} *{1}' -f (Get-PortableSha256 (Join-Path $Root $_)),$_ }) -join "`n")+"`n")
}
function Bundle([string]$Root,$Seed,[string]$Id,[string]$SeedRelative,[string[]]$Ancestors=@()) {
    foreach($name in @('bin','game','user','cache','logs','diagnostics','results','update','metadata')){[IO.Directory]::CreateDirectory((Join-Path $Root $name))|Out-Null}
    Copy-Item -LiteralPath (Join-Path $Seed.Root $script:PortableProfileName) -Destination (Join-Path $Root 'user') -Recurse
    foreach($file in @('Launch-MCLA.exe','bin/mcla.exe','bin/mcla_crash_handler.exe','bin/rexgpu-xenos.dll','bin/rexruntime.dll','bin/TracyClient.dll','game/default.xex')){Put (Join-Path $Root $file) ('fixture-'+$file)}
    Put (Join-Path $Root 'bin/mcla.toml') ('mcla_diagnostics_root = "diagnostics"','log_file = "logs/mcla.log"','fullscreen = true','game_data_root = ""','user_data_root = ""','update_data_root = ""','cache_root = ""','metadata_root = ""' -join "`n")
    $g=Join-Path $Root 'game/default.xex'
    Json (Join-Path $Root 'game-manifest.json') ([ordered]@{SchemaVersion=1;SourceIsoSha256=('0'*64);FileCount=1;PayloadBytes=[long](Get-Item $g).Length;Files=@([ordered]@{Path='default.xex';Size=[long](Get-Item $g).Length;Sha256=Get-PortableSha256 $g})})
    Put (Join-Path $Root 'bundle-id.txt') $Id
    $manifest=[ordered]@{
        schema='mcla-portable-campaign-bundle-v1';task='M7-016';bundle_id=$Id;project_version='0.14.0.0';sdk_version='0.14.0.0';sdk_commit='5cc7134845504dd60ba66090880be766a58ab2bb';created_utc=[DateTime]::UtcNow.ToString('O');platform='windows-proton';architecture='x86_64';launcher='Launch-MCLA.exe';window_mode=[ordered]@{startup_fullscreen=$true;alt_enter_toggle=$true;left_double_click_toggle=$true};immutable_file_count=11
        game=[ordered]@{manifest_sha256=Get-PortableSha256 (Join-Path $Root 'game-manifest.json');file_count=1;payload_bytes=[long](Get-Item $g).Length}
        selected_save=[ordered]@{source_snapshot=$SeedRelative;save_relative=$script:PortableSavePath;header_relative=$script:PortableHeaderPath;save_sha256=$Seed.SaveSha256;header_sha256=$Seed.HeaderSha256;profile_sha256=$Seed.Sha256;ancestor_profile_sha256=@($Ancestors)}
        writable_roots=@('user','cache','logs','diagnostics','results','update','metadata');retention=[ordered]@{completed_sessions=32;save_snapshots_per_session=32};private_content=[ordered]@{game_and_save_private=$true;source_iso_included=$false}
    }
    Json (Join-Path $Root 'bundle-manifest.json') $manifest;Lock-Manifest $Root
}
function Session([string]$Root,[string]$Stamp,$From,$To,[string]$Kind='complete',[long]$ExitCode=0) {
    $id=[IO.File]::ReadAllText((Join-Path $Root 'bundle-id.txt'));$sessionId='session-'+$Stamp+'-340';$dir=Join-Path $Root ('results/'+$sessionId)
    Json (Join-Path $dir 'session.json') ([ordered]@{schema='mcla-portable-session-v1';bundle_id=$id;session_id=$sessionId;started_utc=$Stamp;mode='gameplay';state='running'})
    $profiles=@($From);if($From.Sha256 -cne $To.Sha256){$profiles+= $To}
    foreach($profile in $profiles){$dst=Join-Path $dir ('saves/'+$profile.SaveSha256.Substring(0,16));$null=Copy-PortableTreeVerified $profile $dst @($script:PortableProfileName);Json (Join-Path $dst 'snapshot.json') ([ordered]@{schema='mcla-portable-save-snapshot-v1';captured_utc=$Stamp;reason=$(if($profile.Sha256 -ceq $From.Sha256){'launcher-start'}else{'process-exit'});save_sha256=$profile.SaveSha256;header_sha256=$profile.HeaderSha256;complete_profile_tree=$true})}
    $result=[ordered]@{schema='mcla-portable-session-result-v1';bundle_id=$id;session_id=$sessionId;started_utc=$Stamp;completed_utc=$Stamp;mode='gameplay';exit_code=$ExitCode;verified_file_count=11;save_snapshot_count=$profiles.Count;save_watcher_error_count=0;final_save_sha256=$To.SaveSha256;final_header_sha256=$To.HeaderSha256;state='complete'}
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
        Copy-Item -LiteralPath $launcher -Destination (Join-Path $bundle 'Launch-MCLA.exe') -Force;Lock-Manifest $bundle
        $process=Start-Process -FilePath (Join-Path $bundle 'Launch-MCLA.exe') -ArgumentList '--verify-only' -WorkingDirectory $bundle -WindowStyle Hidden -PassThru
        if(-not $process.WaitForExit(15000)){$process.Kill();$process.WaitForExit();throw 'Native fixture verification timed out.'}
        Check ($process.ExitCode -eq 0) 'native existing launcher accepts successor manifest fields'
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
    Reject {& (Join-Path $PSScriptRoot 'verify-portable-campaign-bundle.ps1') -BundleRoot $bundle -Fixture} 'physical identity drifted'
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
        Bundle $next (Get-PortableProfile $plan.Snapshot) $id2 ('private/save-archive/M7-009/'+$c.Sha256) $plan.Ancestors
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
    Reject {& (Join-Path $PSScriptRoot 'verify-portable-campaign-bundle.ps1') -BundleRoot $legacyBundle -Fixture} 'manifest identity'
    $m=Get-Content -LiteralPath (Join-Path $next 'bundle-manifest.json') -Raw|ConvertFrom-Json
    $m.selected_save.ancestor_profile_sha256=@($c.Sha256);Json (Join-Path $next 'bundle-manifest.json') $m;Lock-Manifest $next
    Reject {Get-PortableBundleReturn $fakeRepo $next -Fixture} 'self identity'
    $record=Join-Path $archive 'profile.json';Put $record '{}'
    Reject {Save-PortableProfileArchive $fakeRepo $a @{source='fixture'}} 'archive|property|schema|profile'
    [pscustomobject]@{Passed=$true;Checks=$script:checks;Scope='isolated fixtures; no owner save/runtime touched'}
} finally {
    $target=Assert-PortablePath (Join-Path $repo 'private/evidence/M7-009') $testRoot
    if((Split-Path -Leaf $target) -cmatch '^fixture-[0-9a-f]{32}$' -and (Test-Path -LiteralPath $target)){Remove-Item -LiteralPath $target -Recurse -Force}
}
