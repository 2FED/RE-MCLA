[CmdletBinding(DefaultParameterSetName='Source')]
param(
  [Parameter(ParameterSetName='Source')][switch]$SourceOnly,
  [Parameter(Mandatory,ParameterSetName='Result')][string]$ResultPath,
  [Parameter(ParameterSetName='Result')][switch]$FixtureMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sdk=Join-Path $repo 'third_party/rexglue-sdk'
$sdkVersion='0.9.0.23'
$sdkCommit='2473e42d76f6aa0081c93e6745e91f4b35b393fa'
$priorRun='20260817-155005-1dd57bd3'
$priorResultSha='21FCBE38695B45D22AE516E33E51AD0A292286985549673811E0FE3E33900644'
$seedSaveSha='126F7482878C7AACB09AA6795331C906DFB9C4218BE94EDB1D8E51B27CA78AB2'
$seedHeaderSha='5827A913515AC0E5D55BB56AEC56DE99CACC0ABB7C8061F59336DF4CEA4A8731'
$sourcePaths=@(
  'third_party/rexglue-sdk/include/rex/system/xtypes.h',
  'third_party/rexglue-sdk/include/rex/system/xam/content_device.h',
  'third_party/rexglue-sdk/include/rex/system/xam/content_manager.h',
  'third_party/rexglue-sdk/src/kernel/xam/xam_content.cpp',
  'third_party/rexglue-sdk/src/kernel/xam/xam_content_device.cpp',
  'third_party/rexglue-sdk/src/kernel/xam/xam_ui.cpp',
  'third_party/rexglue-sdk/src/system/xam/content_manager.cpp',
  'third_party/rexglue-sdk/tests/unit/system/xam_content_persistence_test.cpp',
  'third_party/rexglue-sdk/tests/unit/CMakeLists.txt'
)

function Resolve-Safe([string]$Path,[string]$Description,[switch]$Exists){
  $full=if([IO.Path]::IsPathRooted($Path)){[IO.Path]::GetFullPath($Path)}else{[IO.Path]::GetFullPath((Join-Path $repo $Path))}
  $prefix=$repo.TrimEnd('\')+'\';if(-not$full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw "$Description escapes repository."}
  $current=$repo;foreach($part in @($full.Substring($prefix.Length).Split('\')|Where-Object{$_.Length})){$current=Join-Path $current $part;if((Test-Path -LiteralPath $current)-and((Get-Item -LiteralPath $current -Force).Attributes-band[IO.FileAttributes]::ReparsePoint)){throw "$Description traverses a reparse point."}}
  if($Exists-and-not(Test-Path -LiteralPath $full)){throw "$Description is missing."};$full
}
function Exact($Object,[string[]]$Names,[string]$Description){if(($Object.PSObject.Properties.Name-join',')-cne($Names-join',')){throw "$Description schema is invalid."}}
function Bool($Value,[bool]$Expected,[string]$Description){if($Value-isnot[bool]-or$Value-ne$Expected){throw "$Description is invalid."}}
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}

function Assert-SourceContract{
  $xtypes=[IO.File]::ReadAllText((Join-Path $sdk 'include/rex/system/xtypes.h'))
  $device=[IO.File]::ReadAllText((Join-Path $sdk 'src/kernel/xam/xam_content_device.cpp'))
  $ui=[IO.File]::ReadAllText((Join-Path $sdk 'src/kernel/xam/xam_ui.cpp'))
  $content=[IO.File]::ReadAllText((Join-Path $sdk 'src/kernel/xam/xam_content.cpp'))
  $manager=[IO.File]::ReadAllText((Join-Path $sdk 'src/system/xam/content_manager.cpp'))
  $header=[IO.File]::ReadAllText((Join-Path $sdk 'include/rex/system/xam/content_manager.h'))
  $test=[IO.File]::ReadAllText((Join-Path $sdk 'tests/unit/system/xam_content_persistence_test.cpp'))
  $cmake=[IO.File]::ReadAllText((Join-Path $sdk 'tests/unit/CMakeLists.txt'))
  foreach($pair in @(
    @($xtypes,'#define X_ERROR_DISK_FULL X_RESULT_FROM_WIN32(0x00000070L)'),
    @($device,'REXCVAR_DEFINE_UINT32(xam_hdd_free_bytes, 3u * 1024u * 1024u * 1024u, "XAM"'),
    @($device,'.lifecycle(rex::cvar::Lifecycle::kInitOnly);'),
    @($device,'bool HasDummyHddCapacity(uint64_t requested_bytes, uint64_t free_bytes)'),
    @($ui,'return X_ERROR_DISK_FULL;'),
    @($content,'content_manager->BeginContentTransaction(xuid, content_data)'),
    @($content,'result == X_ERROR_SUCCESS'),
    @($content,'!HasDummyHddCapacity(content_size, GetDummyHddFreeBytes())'),
    @($manager,'kTransactionBackupSuffix = ".rexbackup"'),
    @($manager,'kTransactionMarkerSuffix = ".rexpending"'),
    @($manager,'active_content_transactions_.contains(marker_path)'),
    @($manager,'active_content_transactions_.insert(marker_path)'),
    @($manager,'active_content_transactions_.erase(marker_path)'),
    @($manager,'X_RESULT ContentManager::CommitContentTransaction'),
    @($header,'std::unordered_set<std::filesystem::path> active_content_transactions_'),
    @($cmake,'system/xam_content_persistence_test.cpp')
  )){if(-not$pair[0].Contains($pair[1])){throw "Save source contract is missing '$($pair[1])'."}}
  $capacity=$content.IndexOf('!HasDummyHddCapacity(content_size, GetDummyHddFreeBytes())',[StringComparison]::Ordinal)
  $begin=$content.IndexOf('content_manager->BeginContentTransaction(xuid, content_data)',[StringComparison]::Ordinal)
  $removeMarker=$manager.IndexOf('std::filesystem::remove(marker_path, error);',[StringComparison]::Ordinal)
  $removeBackup=$manager.IndexOf('std::filesystem::remove_all(package_backup_path, error);',$removeMarker,[StringComparison]::Ordinal)
  if($capacity-lt0-or$begin-le$capacity){throw 'Storage-full rejection is not before destructive transaction start.'}
  if($removeMarker-lt0-or$removeBackup-le$removeMarker){throw 'Transaction commit point is not before backup cleanup.'}
  $tests=@(
    'XAM saved-game metadata survives manager restart',
    'XAM saved-game metadata rejects truncated headers',
    'XAM saved-game metadata rejects mismatched full-size headers',
    'XAM interrupted saved-game overwrite restores the prior complete save',
    'XAM interrupted brand-new save is not exposed as loadable content',
    'XAM saved-game recovery remains restart-safe while restoring a backup',
    'XAM committed marker keeps replacement after stale-backup interruption',
    'XAM enumeration does not roll back an active saved-game transaction',
    'XAM committed saved-game overwrite keeps the replacement',
    'XAM dummy HDD capacity reports full storage fail closed'
  )
  foreach($name in $tests){if(-not$test.Contains($name)){throw "Focused test is missing '$name'."}}
  [pscustomobject][ordered]@{SourceContractVerified=$true;SourceChecks=28;FocusedCases=10;FocusedAssertions=117}
}

$source=Assert-SourceContract
if($PSCmdlet.ParameterSetName-ceq'Source'){return $source}
$full=Resolve-Safe $ResultPath 'M6-005 result' -Exists
$rootPrefix=(Join-Path $repo 'private/evidence/M6-005').TrimEnd('\')+'\';if(-not$full.StartsWith($rootPrefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Result is outside private M6-005 evidence.'}
$raw=[IO.File]::ReadAllText($full);if($raw-match'(?i)[A-Z]:\\|B13EBABEBABEBABE|controller|serial|xuid'){throw 'Result leaks a path or private identity.'};$r=$raw|ConvertFrom-Json
Exact $r @('schema','task','decision','sdk_version','sdk_commit','build_configuration','prior_native','focused_test','source_integrity','release_artifacts','source_files','scope') 'Result'
if($r.schema-cne'mcla-save-matrix-v1'-or$r.task-cne'M6-005'-or$r.decision-cne'native-autosave-load-overwrite-plus-isolated-creation-destructive-save-matrix-pass'-or$r.sdk_version-cne$sdkVersion-or$r.sdk_commit-cne$sdkCommit-or$r.build_configuration-cne'Release+RelWithDebInfo'){throw 'Result identity is invalid.'}
Exact $r.prior_native @('task','run_id','result_sha256','creation','autosave','load','overwrite','fresh_process','controlled_exit') 'Prior native evidence'
if($r.prior_native.task-cne'M6-002'-or$r.prior_native.run_id-cne$priorRun-or$r.prior_native.result_sha256-cne$priorResultSha){throw 'Prior native identity is invalid.'};Bool $r.prior_native.creation $false 'Prior native clean creation';foreach($name in @('autosave','load','overwrite','fresh_process','controlled_exit')){Bool $r.prior_native.$name $true "Prior native $name"}
Exact $r.focused_test @('cases','assertions','isolated_creation_roundtrip','restart_metadata','truncated_header_rejected','mismatched_header_rejected','interrupted_overwrite_restored','interrupted_new_removed','restart_recovery_idempotent','commit_point_preserved','live_enumeration_safe','committed_overwrite_preserved','storage_full_rejected','sdk_build_log_sha256','test_log_sha256','unit_executable_sha256') 'Focused test'
if($r.focused_test.cases-ne10-or$r.focused_test.assertions-ne117){throw 'Focused totals are invalid.'};foreach($name in @('isolated_creation_roundtrip','restart_metadata','truncated_header_rejected','mismatched_header_rejected','interrupted_overwrite_restored','interrupted_new_removed','restart_recovery_idempotent','commit_point_preserved','live_enumeration_safe','committed_overwrite_preserved','storage_full_rejected')){Bool $r.focused_test.$name $true "Focused $name"};foreach($name in @('sdk_build_log_sha256','test_log_sha256','unit_executable_sha256')){if($r.focused_test.$name-cnotmatch'^[A-F0-9]{64}$'){throw "Focused digest $name is invalid."}}
Exact $r.source_integrity @('seed_save_before_sha256','seed_save_after_sha256','seed_header_before_sha256','seed_header_after_sha256','source_save_mutated','release_build_log_sha256') 'Source integrity';if($r.source_integrity.seed_save_before_sha256-cne$seedSaveSha-or$r.source_integrity.seed_save_after_sha256-cne$seedSaveSha-or$r.source_integrity.seed_header_before_sha256-cne$seedHeaderSha-or$r.source_integrity.seed_header_after_sha256-cne$seedHeaderSha-or$r.source_integrity.release_build_log_sha256-cnotmatch'^[A-F0-9]{64}$'){throw 'Source integrity hashes are invalid.'};Bool $r.source_integrity.source_save_mutated $false 'Source save mutation'
if(@($r.release_artifacts).Count-ne4){throw 'Release artifact count is invalid.'};$artifactNames=@('mcla.exe','rexruntime.dll','TracyClient.dll','rexgpu-xenos.dll');for($i=0;$i-lt4;$i++){Exact $r.release_artifacts[$i] @('name','sha256') 'Release artifact';if($r.release_artifacts[$i].name-cne$artifactNames[$i]-or$r.release_artifacts[$i].sha256-cnotmatch'^[A-F0-9]{64}$'){throw 'Release artifact manifest is invalid.'}}
if(@($r.source_files).Count-ne$sourcePaths.Count){throw 'Source manifest count is invalid.'};for($i=0;$i-lt$sourcePaths.Count;$i++){Exact $r.source_files[$i] @('path','sha256') 'Source file';if($r.source_files[$i].path-cne$sourcePaths[$i]-or$r.source_files[$i].sha256-cnotmatch'^[A-F0-9]{64}$'){throw 'Source manifest is invalid.'}}
Exact $r.scope @('manual_save_applicable','manual_save_claimed','first_run_oobe_claimed','stock_creation_baseline','general_profile_persistence_claimed','native_gameplay_replayed','destructive_tests_isolated') 'Scope';Bool $r.scope.manual_save_applicable $false 'Manual save applicability';Bool $r.scope.manual_save_claimed $false 'Manual save claim';Bool $r.scope.first_run_oobe_claimed $false 'First-run claim';Bool $r.scope.stock_creation_baseline $true 'Stock creation baseline';Bool $r.scope.general_profile_persistence_claimed $false 'General profile claim';Bool $r.scope.native_gameplay_replayed $false 'Native gameplay replay';Bool $r.scope.destructive_tests_isolated $true 'Destructive isolation'
if(-not$FixtureMode){
  $runRoot=Split-Path $full;$children=@(Get-ChildItem -LiteralPath $runRoot -Force);if($children.Count-ne4-or(@($children.Name|Sort-Object)-join',')-cne'release-clean-build.log,result.json,sdk-build.log,sdk-content-test.log'){throw 'Evidence topology is invalid.'};foreach($child in $children){if($child.Attributes-band[IO.FileAttributes]::ReparsePoint){throw 'Evidence child is a reparse point.'}}
  $prior=Resolve-Safe "private/evidence/M6-002/$priorRun/result.json" 'Prior M6-002 result' -Exists;if((Hash $prior)-cne$priorResultSha){throw 'Prior M6-002 result drifted.'};& (Join-Path $PSScriptRoot 'verify-garage-lifecycle-smoke.ps1') -ResultPath $prior -Fixture|Out-Null
  $seed=Resolve-Safe 'private/evidence/M5-013/20260817-013319-c2e7223f/runs/01/user/B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav' 'Seed save' -Exists;$seedHeader=Resolve-Safe 'private/evidence/M5-013/20260817-013319-c2e7223f/runs/01/user/B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header' 'Seed header' -Exists;if((Hash $seed)-cne$seedSaveSha-or(Hash $seedHeader)-cne$seedHeaderSha){throw 'Seed save/header drifted.'}
  $tag=(&git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim();$head=(&git -C $sdk rev-parse HEAD).Trim();if($tag-cne"v$sdkVersion"-or$head-cne$sdkCommit-or(git -C $sdk status --porcelain)){throw 'SDK identity or cleanliness is invalid.'}
  $sdkLog=Join-Path $runRoot 'sdk-build.log';$testLog=Join-Path $runRoot 'sdk-content-test.log';$releaseLog=Join-Path $runRoot 'release-clean-build.log';$unit=Join-Path $sdk 'out/win-amd64/RelWithDebInfo/unit_tests.exe';if((Hash $sdkLog)-cne$r.focused_test.sdk_build_log_sha256-or(Hash $testLog)-cne$r.focused_test.test_log_sha256-or(Hash $unit)-cne$r.focused_test.unit_executable_sha256-or[IO.File]::ReadAllText($testLog)-notmatch'All tests passed \(117 assertions in 10 test cases\)'){throw 'Focused physical test evidence is invalid.'};if((Hash $releaseLog)-cne$r.source_integrity.release_build_log_sha256){throw 'Release build log drifted.'};$releaseText=[IO.File]::ReadAllText($releaseLog);foreach($needle in @('CMAKE_BUILD_TYPE="Release"','REXSDK_VERSION="0.9.0.23"','Cleaning all built files','Linking CXX executable mcla.exe')){if(-not$releaseText.Contains($needle)){throw "Release build log is missing '$needle'."}}
  foreach($entry in $r.source_files){if((Hash (Resolve-Safe $entry.path 'Source file' -Exists))-cne$entry.sha256){throw 'Source file drifted.'}};$build=Resolve-Safe 'out/build/win-amd64-release' 'Release build' -Exists;for($i=0;$i-lt4;$i++){if((Hash (Resolve-Safe (Join-Path $build $artifactNames[$i]) 'Release artifact' -Exists))-cne$r.release_artifacts[$i].sha256){throw 'Release artifact drifted.'}}
  if(@((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{$_.Path-ceq(Join-Path $build 'mcla.exe')}catch{$false}}).Count){throw 'Canonical MCLA process remains.'}
}
[pscustomobject][ordered]@{Decision=$r.decision;NativeAutosaveLoadOverwriteVerified=$true;IsolatedCreationRoundtripVerified=$true;InterruptedRecoveryVerified=$true;CorruptionHandlingVerified=$true;StorageFullVerified=$true;ManualSaveApplicable=$false;FirstRunOobeClaimed=$false;SourceSaveMutated=$false;DataIntegrityVerified=$true}
