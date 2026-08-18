[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sdk=Join-Path $repo 'third_party/rexglue-sdk'
$verify=Join-Path $PSScriptRoot 'verify-save-matrix-smoke.ps1'
$sdkVersion='0.9.0.23';$sdkCommit='2473e42d76f6aa0081c93e6745e91f4b35b393fa';$priorRun='20260817-155005-1dd57bd3';$priorSha='21FCBE38695B45D22AE516E33E51AD0A292286985549673811E0FE3E33900644'
$seed='private/evidence/M5-013/20260817-013319-c2e7223f/runs/01/user/B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav';$header='private/evidence/M5-013/20260817-013319-c2e7223f/runs/01/user/B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash}
function Logged([scriptblock]$Command,[string]$Log,[switch]$Append){$prior=$ErrorActionPreference;$ErrorActionPreference='Continue';try{if($Append){&$Command *>&1|Tee-Object -FilePath $Log -Append|Out-Null}else{&$Command *>&1|Tee-Object -FilePath $Log|Out-Null};$LASTEXITCODE}finally{$ErrorActionPreference=$prior}}
function Artifacts([string]$Build){@('mcla.exe','rexruntime.dll','TracyClient.dll','rexgpu-xenos.dll')|ForEach-Object{$path=Join-Path $Build $_;if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Release artifact $_ is missing."};[ordered]@{name=$_;sha256=Hash $path}}}
&$verify -SourceOnly|Out-Null
if((&git -C $sdk rev-parse HEAD).Trim()-cne$sdkCommit-or(&git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim()-cne"v$sdkVersion"-or(git -C $sdk status --porcelain)){throw 'M6-005 requires the clean exact SDK release.'}
$prior=Join-Path $repo "private/evidence/M6-002/$priorRun/result.json";if(-not(Test-Path $prior)-or(Hash $prior)-cne$priorSha){throw 'Accepted M6-002 result drifted.'};& (Join-Path $PSScriptRoot 'verify-garage-lifecycle-smoke.ps1') -ResultPath $prior -Fixture|Out-Null
$seed=Join-Path $repo $seed;$header=Join-Path $repo $header;$seedBefore=Hash $seed;$headerBefore=Hash $header
$tool=& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$runRoot=Join-Path $repo ('private/evidence/M6-005/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8));[IO.Directory]::CreateDirectory($runRoot)|Out-Null
$sdkLog=Join-Path $runRoot 'sdk-build.log';$testLog=Join-Path $runRoot 'sdk-content-test.log';$releaseLog=Join-Path $runRoot 'release-clean-build.log'
Write-Host 'M6-005 [1/4]: validating prior native autosave/load/overwrite evidence and immutable HANGOUT save...' -ForegroundColor Cyan
Write-Host 'M6-005 [2/4]: clean-building the exact SDK and isolated destructive save tests...' -ForegroundColor Cyan
Push-Location $sdk;try{if(Logged {&$tool.CMakePath --preset win-amd64 -DREXGLUE_BUILD_TESTS=ON} $sdkLog){throw "SDK configure failed. Private run: '$runRoot'."};if(Logged {&$tool.CMakePath --build out/build/win-amd64 --config RelWithDebInfo --target install unit_tests --clean-first --parallel 8} $sdkLog -Append){throw "SDK build/install failed. Private run: '$runRoot'."}}finally{Pop-Location}
$unit=Join-Path $sdk 'out/win-amd64/RelWithDebInfo/unit_tests.exe';&$unit '[system][xam][content]' *>&1|Tee-Object -FilePath $testLog|Out-Null;if($LASTEXITCODE-or[IO.File]::ReadAllText($testLog)-notmatch'All tests passed \(117 assertions in 10 test cases\)'){throw "Focused save matrix failed. Private run: '$runRoot'."}
Write-Host 'M6-005 [3/4]: clean-building the Release host against the new SDK pin...' -ForegroundColor Cyan
if(Logged {&$tool.CMakePath --preset win-amd64-release} $releaseLog){throw "Release configure failed. Private run: '$runRoot'."};if(Logged {&$tool.CMakePath --build --preset win-amd64-release --target mcla --clean-first --parallel 8} $releaseLog -Append){throw "Release clean build failed. Private run: '$runRoot'."}
$build=Join-Path $repo 'out/build/win-amd64-release';$seedAfter=Hash $seed;$headerAfter=Hash $header;if($seedAfter-cne$seedBefore-or$headerAfter-cne$headerBefore){throw 'Canonical HANGOUT save/header changed during isolated tests.'}
$sourcePaths=@('third_party/rexglue-sdk/include/rex/system/xtypes.h','third_party/rexglue-sdk/include/rex/system/xam/content_device.h','third_party/rexglue-sdk/include/rex/system/xam/content_manager.h','third_party/rexglue-sdk/src/kernel/xam/xam_content.cpp','third_party/rexglue-sdk/src/kernel/xam/xam_content_device.cpp','third_party/rexglue-sdk/src/kernel/xam/xam_ui.cpp','third_party/rexglue-sdk/src/system/xam/content_manager.cpp','third_party/rexglue-sdk/tests/unit/system/xam_content_persistence_test.cpp','third_party/rexglue-sdk/tests/unit/CMakeLists.txt')
$result=[ordered]@{schema='mcla-save-matrix-v1';task='M6-005';decision='native-autosave-load-overwrite-plus-isolated-creation-destructive-save-matrix-pass';sdk_version=$sdkVersion;sdk_commit=$sdkCommit;build_configuration='Release+RelWithDebInfo';prior_native=[ordered]@{task='M6-002';run_id=$priorRun;result_sha256=$priorSha;creation=$false;autosave=$true;load=$true;overwrite=$true;fresh_process=$true;controlled_exit=$true};focused_test=[ordered]@{cases=10;assertions=117;isolated_creation_roundtrip=$true;restart_metadata=$true;truncated_header_rejected=$true;mismatched_header_rejected=$true;interrupted_overwrite_restored=$true;interrupted_new_removed=$true;restart_recovery_idempotent=$true;commit_point_preserved=$true;live_enumeration_safe=$true;committed_overwrite_preserved=$true;storage_full_rejected=$true;sdk_build_log_sha256=Hash $sdkLog;test_log_sha256=Hash $testLog;unit_executable_sha256=Hash $unit};source_integrity=[ordered]@{seed_save_before_sha256=$seedBefore;seed_save_after_sha256=$seedAfter;seed_header_before_sha256=$headerBefore;seed_header_after_sha256=$headerAfter;source_save_mutated=$false;release_build_log_sha256=Hash $releaseLog};release_artifacts=@(Artifacts $build);source_files=@($sourcePaths|ForEach-Object{[ordered]@{path=$_;sha256=Hash (Join-Path $repo $_)}});scope=[ordered]@{manual_save_applicable=$false;manual_save_claimed=$false;first_run_oobe_claimed=$false;stock_creation_baseline=$true;general_profile_persistence_claimed=$false;native_gameplay_replayed=$false;destructive_tests_isolated=$true}}
$resultPath=Join-Path $runRoot 'result.json';[IO.File]::WriteAllText($resultPath,(($result|ConvertTo-Json -Depth 9)+[Environment]::NewLine),[Text.UTF8Encoding]::new($false))
Write-Host 'M6-005 [4/4]: physically revalidating prior evidence, isolated failures, artifacts, and source integrity...' -ForegroundColor Cyan
$final=&$verify -ResultPath $resultPath
Write-Host "M6-005 PASS: '$resultPath'." -ForegroundColor Green
$final
