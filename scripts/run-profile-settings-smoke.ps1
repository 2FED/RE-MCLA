[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sdk=Join-Path $repo 'third_party/rexglue-sdk'
$verify=Join-Path $PSScriptRoot 'verify-profile-settings-smoke.ps1'
$sdkVersion='0.9.0.24';$sdkCommit='1e4dbc0040c1eebbf78dca0b5679ac64f99b9f4d'
$seedRelative='private/evidence/M5-013/20260817-013319-c2e7223f/runs/01/user/B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
$headerRelative='private/evidence/M5-013/20260817-013319-c2e7223f/runs/01/user/B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'
$prior=@(
  [ordered]@{task='M4-004';run_id='20260812-123316-db0f1cf4';sha256='388F2FC350C43149CA6E5AD4E24A1B743F18CFE3A450C6D80962099F73C89AE0';decision='single-local-offline-profile'},
  [ordered]@{task='M4-010';run_id='20260813-213301-505ca1ca';sha256='458B742FC1112DDAF39BDC8B81935CC8F2F9C616477B258B1E6FBA10058E096A';decision='locale-selection-unicode-path-title-matrix-pass'},
  [ordered]@{task='M5-006';run_id='20260814-130533-0b95f6b6';sha256='A89C0CC3E02C8D264B0DA29157021D050276BF46F028BBAAAD9B1FFC220CCEAB';decision='saved-gameplay-input-pass'},
  [ordered]@{task='M6-005';run_id='20260818-201702-5d089d9c';sha256='5F46FA6657F39EE5AF990BD505B67D5EDBE6A781E8283D1D51BE07F3543F169E';decision='native-autosave-load-overwrite-plus-isolated-creation-destructive-save-matrix-pass'}
)
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
function Logged([scriptblock]$Command,[string]$Log,[switch]$Append){$old=$ErrorActionPreference;$ErrorActionPreference='Continue';try{if($Append){&$Command *>&1|Tee-Object -FilePath $Log -Append|Out-Null}else{&$Command *>&1|Tee-Object -FilePath $Log|Out-Null};$LASTEXITCODE}finally{$ErrorActionPreference=$old}}
function Artifacts([string]$Build){@('mcla.exe','rexruntime.dll','TracyClient.dll','rexgpu-xenos.dll')|ForEach-Object{$p=Join-Path $Build $_;if(-not(Test-Path -LiteralPath $p -PathType Leaf)){throw "Release artifact $_ is missing."};[ordered]@{name=$_;sha256=Hash $p}}}

&$verify -SourceOnly|Out-Null
if((&git -C $sdk rev-parse HEAD).Trim()-cne$sdkCommit-or(&git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim()-cne"v$sdkVersion"-or(git -C $sdk status --porcelain)){throw 'M6-006 requires the clean exact SDK release.'}
foreach($entry in $prior){$path=Join-Path $repo "private/evidence/$($entry.task)/$($entry.run_id)/result.json";if(-not(Test-Path -LiteralPath $path)-or(Hash $path)-cne$entry.sha256){throw "Accepted $($entry.task) result drifted."};$json=[IO.File]::ReadAllText($path)|ConvertFrom-Json;if($json.task-cne$entry.task-or$json.decision-cne$entry.decision){throw "Accepted $($entry.task) identity drifted."}}
$seed=Join-Path $repo $seedRelative;$header=Join-Path $repo $headerRelative;$seedBefore=Hash $seed;$headerBefore=Hash $header
$tool=& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$runRoot=Join-Path $repo ('private/evidence/M6-006/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8));[IO.Directory]::CreateDirectory($runRoot)|Out-Null
$sdkLog=Join-Path $runRoot 'sdk-build.log';$profileLog=Join-Path $runRoot 'profile-test.log';$cvarLog=Join-Path $runRoot 'cvar-test.log';$releaseLog=Join-Path $runRoot 'release-clean-build.log'
Write-Host 'M6-006 [1/4]: validating immutable profile, controller, locale, save, and SDK prerequisites...' -ForegroundColor Cyan
Write-Host 'M6-006 [2/4]: clean-building the exact SDK and isolated restart suites...' -ForegroundColor Cyan
Push-Location $sdk;try{if(Logged {&$tool.CMakePath --preset win-amd64 -DREXGLUE_BUILD_TESTS=ON} $sdkLog){throw "SDK configure failed. Private run: '$runRoot'."};if(Logged {&$tool.CMakePath --build out/build/win-amd64 --config RelWithDebInfo --target install unit_tests --clean-first --parallel 8} $sdkLog -Append){throw "SDK build/install failed. Private run: '$runRoot'."}}finally{Pop-Location}
$unit=Join-Path $sdk 'out/win-amd64/RelWithDebInfo/unit_tests.exe'
&$unit '[kernel][xam][profile]' *>&1|Tee-Object -FilePath $profileLog|Out-Null;if($LASTEXITCODE-or[IO.File]::ReadAllText($profileLog)-notmatch'All tests passed \(40 assertions in 7 test cases\)'){throw "Focused profile restart suite failed. Private run: '$runRoot'."}
&$unit '[cvar]' *>&1|Tee-Object -FilePath $cvarLog|Out-Null;if($LASTEXITCODE-or[IO.File]::ReadAllText($cvarLog)-notmatch'All tests passed \(128 assertions in 26 test cases\)'){throw "Focused preference restart suite failed. Private run: '$runRoot'."}
Write-Host 'M6-006 [3/4]: clean-building the Release host against the persisted-preference SDK...' -ForegroundColor Cyan
if(Logged {&$tool.CMakePath --preset win-amd64-release} $releaseLog){throw "Release configure failed. Private run: '$runRoot'."};if(Logged {&$tool.CMakePath --build --preset win-amd64-release --target mcla --clean-first --parallel 8} $releaseLog -Append){throw "Release clean build failed. Private run: '$runRoot'."}
$build=Join-Path $repo 'out/build/win-amd64-release';$seedAfter=Hash $seed;$headerAfter=Hash $header;if($seedAfter-cne$seedBefore-or$headerAfter-cne$headerBefore){throw 'Canonical HANGOUT save/header changed during isolated restart tests.'}
$sourcePaths=@('third_party/rexglue-sdk/include/rex/system/xam/content_manager.h','third_party/rexglue-sdk/include/rex/system/xam/user_profile.h','third_party/rexglue-sdk/src/kernel/xam/xam_user.cpp','third_party/rexglue-sdk/src/system/kernel_state.cpp','third_party/rexglue-sdk/src/system/xam/content_manager.cpp','third_party/rexglue-sdk/src/system/xam/user_profile.cpp','third_party/rexglue-sdk/tests/unit/kernel/xam_user_profile_test.cpp','third_party/rexglue-sdk/src/core/cvar.cpp','third_party/rexglue-sdk/src/input/input_system.cpp','third_party/rexglue-sdk/src/input/mnk/mnk_input_driver.cpp')
$result=[ordered]@{schema='mcla-profile-settings-v1';task='M6-006';decision='isolated-profile-controller-language-restart-matrix-pass';sdk_version=$sdkVersion;sdk_commit=$sdkCommit;build_configuration='Release+RelWithDebInfo';prior_evidence=@($prior);focused_tests=[ordered]@{profile_cases=7;profile_assertions=40;cvar_cases=26;cvar_assertions=128;scalar_restart=$true;unicode_restart=$true;metadata_rejected=$true;backup_recovered=$true;title_settings_isolated=$true;config_save_load=$true;sdk_build_log_sha256=Hash $sdkLog;profile_test_log_sha256=Hash $profileLog;cvar_test_log_sha256=Hash $cvarLog;unit_executable_sha256=Hash $unit};preferences=[ordered]@{language_flag='user_language';controller_backend_flag='input_backend';controller_mode_flag='mnk_mode';named_flags_source_bound=$true;restart_serialization_verified=$true};source_integrity=[ordered]@{seed_save_before_sha256=$seedBefore;seed_save_after_sha256=$seedAfter;seed_header_before_sha256=$headerBefore;seed_header_after_sha256=$headerAfter;source_save_mutated=$false;release_build_log_sha256=Hash $releaseLog};release_artifacts=@(Artifacts $build);source_files=@($sourcePaths|ForEach-Object{[ordered]@{path=$_;sha256=Hash (Join-Path $repo $_)}});scope=[ordered]@{title_write_imported=$false;native_profile_write_route_claimed=$false;isolated_platform_persistence=$true;prior_profile_identity_physical=$true;prior_language_selection_physical=$true;prior_controller_route_physical=$true;profile_read_runtime_claimed=$false;account_picker_claimed=$false;multiple_profiles_claimed=$false;first_run_oobe_claimed=$false;native_gameplay_replayed=$false;general_host_config_claimed=$false}}
$resultPath=Join-Path $runRoot 'result.json';[IO.File]::WriteAllText($resultPath,(($result|ConvertTo-Json -Depth 9)+[Environment]::NewLine),[Text.UTF8Encoding]::new($false))
Write-Host 'M6-006 [4/4]: revalidating restart semantics, immutable prerequisites, artifacts, and scope...' -ForegroundColor Cyan
$final=&$verify -ResultPath $resultPath
Write-Host "M6-006 PASS: '$resultPath'." -ForegroundColor Green
$final
