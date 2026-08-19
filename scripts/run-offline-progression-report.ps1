[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sdk=Join-Path $repo 'third_party/rexglue-sdk'
$verify=Join-Path $PSScriptRoot 'verify-offline-progression-report.ps1'
$sdkVersion='0.9.0.26';$sdkCommit='51f18fab1a5c11d50a380a30fbe592b93fd98248'
$prior=@(
  [ordered]@{task='M4-004';run_id='20260812-123316-db0f1cf4';sha256='388F2FC350C43149CA6E5AD4E24A1B743F18CFE3A450C6D80962099F73C89AE0';decision='single-local-offline-profile'},
  [ordered]@{task='M4-009';run_id='20260813-193514-2bb7e7ca';sha256='52AC3A713038DC4EF9A6236CFB76462006B5AAF8D3856D2422A5410AA9847539';decision='offline-service-title-route-pass'},
  [ordered]@{task='M5-012';run_id='20260817-001225-ade395f8';sha256='D993E2612D1AC769D88264C83FD9C9186BC761E2067317F5A7EB66038C250E58';decision='first-series-results-return-and-release-restart-pass'},
  [ordered]@{task='M6-003';run_id='20260818-180605-e3b74fb5';sha256='AD3573B56412D7DBC10650BE0D250AFD4AACA22EB8EEEB6F9816E94EC3007E3A';decision='representative-race-system-matrix-pass'},
  [ordered]@{task='M6-005';run_id='20260818-201702-5d089d9c';sha256='5F46FA6657F39EE5AF990BD505B67D5EDBE6A781E8283D1D51BE07F3543F169E';decision='native-autosave-load-overwrite-plus-isolated-creation-destructive-save-matrix-pass'}
)
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
function Logged([scriptblock]$Command,[string]$Log,[switch]$Append){$old=$ErrorActionPreference;$ErrorActionPreference='Continue';try{if($Append){&$Command *>&1|Tee-Object -FilePath $Log -Append|Out-Null}else{&$Command *>&1|Tee-Object -FilePath $Log|Out-Null};$LASTEXITCODE}finally{$ErrorActionPreference=$old}}
function Artifacts([string]$Build){@('mcla.exe','rexruntime.dll','TracyClient.dll','rexgpu-xenos.dll')|ForEach-Object{$p=Join-Path $Build $_;if(-not(Test-Path -LiteralPath $p -PathType Leaf)){throw "Release artifact $_ is missing."};[ordered]@{name=$_;sha256=Hash $p}}}

&$verify -SourceOnly|Out-Null
if((&git -C $sdk rev-parse HEAD).Trim()-cne$sdkCommit-or(&git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim()-cne"v$sdkVersion"-or(git -C $sdk status --porcelain)){throw 'M6-008 requires the clean exact SDK release.'}
foreach($entry in $prior){$path=Join-Path $repo "private/evidence/$($entry.task)/$($entry.run_id)/result.json";if(-not(Test-Path -LiteralPath $path)-or(Hash $path)-cne$entry.sha256){throw "Accepted $($entry.task) result drifted."};$json=[IO.File]::ReadAllText($path)|ConvertFrom-Json;if($json.task-cne$entry.task-or$json.decision-cne$entry.decision){throw "Accepted $($entry.task) identity drifted."}}
$tool=& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$runRoot=Join-Path $repo ('private/evidence/M6-008/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8));[IO.Directory]::CreateDirectory($runRoot)|Out-Null
$sdkLog=Join-Path $runRoot 'sdk-build.log';$xamLog=Join-Path $runRoot 'offline-xam-test.log';$serviceLog=Join-Path $runRoot 'offline-service-test.log';$achievementLog=Join-Path $runRoot 'achievement-test.log';$releaseLog=Join-Path $runRoot 'release-clean-build.log'
Write-Host 'M6-008 [1/4]: validating immutable offline-profile, service, progression, race, and save evidence...' -ForegroundColor Cyan
Write-Host 'M6-008 [2/4]: clean-building the exact SDK and offline progression suites...' -ForegroundColor Cyan
Push-Location $sdk;try{if(Logged {&$tool.CMakePath --preset win-amd64 -DREXGLUE_BUILD_TESTS=ON} $sdkLog){throw "SDK configure failed. Private run: '$runRoot'."};if(Logged {&$tool.CMakePath --build out/build/win-amd64 --config RelWithDebInfo --target install unit_tests --clean-first --parallel 8} $sdkLog -Append){throw "SDK RelWithDebInfo build/install failed. Private run: '$runRoot'."};if(Logged {&$tool.CMakePath --build out/build/win-amd64 --config Release --target install --parallel 8} $sdkLog -Append){throw "SDK Release build/install failed. Private run: '$runRoot'."}}finally{Pop-Location}
$unit=Join-Path $sdk 'out/win-amd64/RelWithDebInfo/unit_tests.exe'
&$unit '[kernel][xam][offline]' *>&1|Tee-Object -FilePath $xamLog|Out-Null;if($LASTEXITCODE-or[IO.File]::ReadAllText($xamLog)-notmatch'All tests passed \(25 assertions in 6 test cases\)'){throw "Focused offline XAM suite failed. Private run: '$runRoot'."}
&$unit '[kernel][offline-service]' *>&1|Tee-Object -FilePath $serviceLog|Out-Null;if($LASTEXITCODE-or[IO.File]::ReadAllText($serviceLog)-notmatch'All tests passed \(32 assertions in 3 test cases\)'){throw "Focused service matrix failed. Private run: '$runRoot'."}
&$unit '[achievements]' *>&1|Tee-Object -FilePath $achievementLog|Out-Null;if($LASTEXITCODE-or[IO.File]::ReadAllText($achievementLog)-notmatch'All tests passed \(50 assertions in 6 test cases\)'){throw "Focused achievement suite failed. Private run: '$runRoot'."}
Write-Host 'M6-008 [3/4]: clean-building the Release host against the offline-safe SDK...' -ForegroundColor Cyan
if(Logged {&$tool.CMakePath --preset win-amd64-release} $releaseLog){throw "Release configure failed. Private run: '$runRoot'."};if(Logged {&$tool.CMakePath --build --preset win-amd64-release --target mcla --clean-first --parallel 8} $releaseLog -Append){throw "Release clean build failed. Private run: '$runRoot'."}
$build=Join-Path $repo 'out/build/win-amd64-release'
$matrix=@(
  [ordered]@{feature='achievements';behavior='local';progression_source='achievement-manager';retired_service_result='000004DD';fabricated_rows_or_unlocks=$false},
  [ordered]@{feature='presence';behavior='unavailable';progression_source='none';retired_service_result='explicit-offline';fabricated_rows_or_unlocks=$false},
  [ordered]@{feature='leaderboards';behavior='empty-enumeration';progression_source='none';retired_service_result='00000000';fabricated_rows_or_unlocks=$false},
  [ordered]@{feature='rate-my-ride';behavior='unavailable';progression_source='none';retired_service_result='explicit-offline';fabricated_rows_or_unlocks=$false},
  [ordered]@{feature='driving-test';behavior='local-save';progression_source='title-save';retired_service_result='not-applicable';fabricated_rows_or_unlocks=$false},
  [ordered]@{feature='voice';behavior='unavailable';progression_source='none';retired_service_result='800700AA';fabricated_rows_or_unlocks=$false}
)
$sourcePaths=@('third_party/rexglue-sdk/include/rex/kernel/xam/offline_progression.h','third_party/rexglue-sdk/src/kernel/xam/xam_ui.cpp','third_party/rexglue-sdk/src/kernel/xam/xam_user.cpp','third_party/rexglue-sdk/src/kernel/xam/xam_voice.cpp','third_party/rexglue-sdk/tests/unit/kernel/offline_service_test.cpp','third_party/rexglue-sdk/tests/unit/kernel/offline_service_route_test.cpp','third_party/rexglue-sdk/tests/unit/system/achievement_manager_test.cpp','generated/default/mcla_init.cpp','generated/default/mcla_recomp.0.cpp','generated/default/mcla_recomp.29.cpp','generated/default/mcla_recomp.58.cpp')
$result=[ordered]@{schema='mcla-offline-progression-v1';task='M6-008';decision='explicit-offline-progression-and-retired-service-matrix-pass';sdk_version=$sdkVersion;sdk_commit=$sdkCommit;build_configuration='Release+RelWithDebInfo';prior_evidence=@($prior);service_matrix=@($matrix);focused_tests=[ordered]@{offline_xam_cases=6;offline_xam_assertions=25;offline_service_cases=3;offline_service_assertions=32;achievement_cases=6;achievement_assertions=50;sdk_build_log_sha256=Hash $sdkLog;offline_xam_test_log_sha256=Hash $xamLog;offline_service_test_log_sha256=Hash $serviceLog;achievement_test_log_sha256=Hash $achievementLog;unit_executable_sha256=Hash $unit};release=[ordered]@{build_log_sha256=Hash $releaseLog;artifacts=@(Artifacts $build)};source_files=@($sourcePaths|ForEach-Object{[ordered]@{path=$_;sha256=Hash (Join-Path $repo $_)}});scope=[ordered]@{prior_offline_frontend_route_bound=$true;prior_local_profile_route_bound=$true;prior_race_progression_bound=$true;prior_save_restart_bound=$true;m6_009_unlock_policy_unchanged=$true;native_current_service_calls_claimed=$false;achievement_guide_ui_claimed=$false;xbox_live_presence_claimed=$false;leaderboard_rows_claimed=$false;rate_my_ride_backend_claimed=$false;voice_transport_claimed=$false;driving_test_exact_unlock_physically_claimed=$false;service_vehicle_unlock_claimed=$false;fake_online_identity_used=$false;fabricated_progression_used=$false}}
$resultPath=Join-Path $runRoot 'result.json';[IO.File]::WriteAllText($resultPath,(($result|ConvertTo-Json -Depth 10)+[Environment]::NewLine),[Text.UTF8Encoding]::new($false))
Write-Host 'M6-008 [4/4]: revalidating source policy, tests, immutable progression evidence, artifacts, and scope...' -ForegroundColor Cyan
$final=&$verify -ResultPath $resultPath
Write-Host "M6-008 PASS: '$resultPath'." -ForegroundColor Green
$final
