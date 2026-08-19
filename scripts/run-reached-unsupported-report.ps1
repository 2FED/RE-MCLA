[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdk=Join-Path $repoRoot 'third_party/rexglue-sdk'
$cmake=(& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath).CMakePath
$sdkVersion='0.9.0.29'; $sdkCommit='5a7fc75713d1d43188b7574349f44a7e7923033d'
if ((& git -C $sdk rev-parse HEAD).Trim() -ne $sdkCommit -or (& git -C $sdk describe --tags --exact-match HEAD).Trim() -ne "v$sdkVersion") { throw 'The checked-out SDK is not the canonical M6-013 pin.' }
& (Join-Path $PSScriptRoot 'verify-reached-unsupported-report.ps1') -SourceOnly | Out-Null
$runId=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)
$runRoot=Join-Path $repoRoot "private/evidence/M6-013/$runId"; $user=Join-Path $runRoot 'user'; $cache=Join-Path $runRoot 'cache'
[IO.Directory]::CreateDirectory($user)|Out-Null;[IO.Directory]::CreateDirectory($cache)|Out-Null
$sdkLog=Join-Path $runRoot 'sdk-install.log';$testLog=Join-Path $runRoot 'focused-tests.log';$appLog=Join-Path $runRoot 'app-clean-build.log'
Write-Host 'M6-013 [1/5]: clean-building/installing the exact ReXGlue SDK...'
Push-Location $sdk;try{&$cmake --preset win-amd64 *>&1|Tee-Object $sdkLog|Out-Null;if($LASTEXITCODE){throw 'SDK configure failed.'};&$cmake --build --preset win-amd64-relwithdebinfo --target install --clean-first --parallel 8 *>&1|Tee-Object $sdkLog -Append|Out-Null;if($LASTEXITCODE){throw 'SDK install failed.'}}finally{Pop-Location}
Write-Host 'M6-013 [2/5]: running reached-compatibility tests...'
Push-Location $sdk;try{&$cmake --build --preset win-amd64-relwithdebinfo --target unit_tests --parallel 8 *>&1|Tee-Object $testLog|Out-Null;if($LASTEXITCODE){throw 'Focused test build failed.'};& .\out\win-amd64\RelWithDebInfo\unit_tests.exe '[kernel][xboxkrnl][reached-compat]' *>&1|Tee-Object $testLog -Append|Out-Null;if($LASTEXITCODE){throw 'Focused tests failed.'}}finally{Pop-Location}
if((Get-Content $testLog -Raw)-notmatch'All tests passed \(5 assertions in 2 test cases\)'){throw 'Focused test totals drifted.'}
Write-Host 'M6-013 [3/5]: clean-building the MCLA host...'
Push-Location $repoRoot;try{&$cmake --preset win-amd64-relwithdebinfo *>&1|Tee-Object $appLog|Out-Null;if($LASTEXITCODE){throw 'App configure failed.'};&$cmake --build --preset win-amd64-relwithdebinfo --target mcla --clean-first --parallel 8 *>&1|Tee-Object $appLog -Append|Out-Null;if($LASTEXITCODE){throw 'App build failed.'}}finally{Pop-Location}
Write-Host 'M6-013 [4/5]: verifying the reached import path through guest presentation...'
$build=Join-Path $repoRoot 'out/build/win-amd64-relwithdebinfo';$exe=Join-Path $build 'mcla.exe';$log=Join-Path $runRoot 'mcla.log';$game=Join-Path $repoRoot 'private/game'
$args=@('--log_max_file_size_mb=8','--log_max_files=8','--log_level=info','--fullscreen=false',"--game_data_root=`"$game`"","--user_data_root=`"$user`"","--cache_root=`"$cache`"","--log_file=`"$log`"")
$process=$null
function Read-Shared($p){if(-not(Test-Path $p)){return ''};$s=[IO.File]::Open($p,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete);try{$r=[IO.StreamReader]::new($s);try{$r.ReadToEnd()}finally{$r.Dispose()}}finally{$s.Dispose()}}
try{$process=Start-Process $exe -ArgumentList $args -WorkingDirectory $build -PassThru;$deadline=(Get-Date).AddSeconds(90);$ready=$false;while((Get-Date)-lt$deadline-and-not$process.HasExited){Start-Sleep -Milliseconds 250;$t=Read-Shared $log;if($t-match'\[UNAVAILABLE\] XeKeysConsolePrivateKeySign'-and$t-match'\[COMPAT\] IoDismountVolumeByFileHandle'-and$t-match'D3D12 guest present: successful sequence count=3'){$ready=$true;break}};if(-not$ready){throw "Reached-import deadline missed. Private run: '$runRoot'."};Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public static class M613Close{[DllImport("user32.dll")]public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);}';$process.Refresh();if($process.MainWindowHandle-eq0){throw 'Window not found.'};[M613Close]::PostMessage($process.MainWindowHandle,0x10,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null;if(-not$process.WaitForExit(10000)-or$process.ExitCode-ne0){throw 'Controlled close failed.'}}finally{if($process-and-not$process.HasExited){$process.Kill($true);$process.WaitForExit()}}
$probe=& (Join-Path $PSScriptRoot 'verify-reached-unsupported-report.ps1') -LogPath $log
$prior=@(
@{task='M5-007';relative_path='private/evidence/M5-007/20260814-132533-40ee5698/result.json';sha256='709609A904C3A49AD0C88E8CC88DFD794D849FB9A7B9B6B5F8AA0887BA9C1E18'},
@{task='M5-013';relative_path='private/evidence/M5-013/20260817-015958-36eec226/result.json';sha256='D890A903775A3B53262B9957E7B7DC1D4B76B49B8735360BECD8233842446298'},
@{task='M6-001';relative_path='private/evidence/M6-001/20260817-115619-d269e2a9/result.json';sha256='519B84FF456BDD3220BFC8BE3DD230CCB209A56CF2B203D51CFF5454729E178F'},
@{task='M6-002';relative_path='private/evidence/M6-002/20260817-155005-1dd57bd3/result.json';sha256='21FCBE38695B45D22AE516E33E51AD0A292286985549673811E0FE3E33900644'},
@{task='M6-003';relative_path='private/evidence/M6-003/20260818-180605-e3b74fb5/result.json';sha256='AD3573B56412D7DBC10650BE0D250AFD4AACA22EB8EEEB6F9816E94EC3007E3A'},
@{task='M6-007';relative_path='private/evidence/M6-007/20260819-122150-2ad3b961/result.json';sha256='CE4B4700CEA1108243989165211A8C70AC0C67F72130708A129C1BD834948B5B'})
$reached=@(
  [ordered]@{name='IoDismountVolumeByFileHandle';disposition='validated-compat-success';invalid_result='X_STATUS_INVALID_HANDLE';host_vfs_mount='runtime-owned'},
  [ordered]@{name='XeKeysConsolePrivateKeySign';disposition='explicit-unavailable';valid_result='X_STATUS_NOT_SUPPORTED';caller_buffer_preserved=$true})
$fixed=@(
  [ordered]@{start='8220B810';end='8220B834'},[ordered]@{start='82262320';end='8226233C'},
  [ordered]@{start='82264760';end='82264770'},[ordered]@{start='82264770';end='82264780'},
  [ordered]@{start='822C9FE8';end='822CA04C'},[ordered]@{start='82554080';end='8255409C'})
$notObserved=@('__C_specific_handler','StfsControlDevice','StfsCreateDevice','XeKeysConsoleSignatureVerification','IoDismountVolume','IoInvalidDeviceRequest','IoCompleteRequest','ObIsTitleObject','IoCheckShareAccess','IoSetShareAccess','IoRemoveShareAccess','RtlUnwind')
$result=[ordered]@{schema=1;task='M6-013';decision='reached-unsupported-surface-fixed-or-bounded-nonblocking';sdk_version=$sdkVersion;sdk_commit=$sdkCommit;prior_evidence=$prior;reached_import_count=2;reached_imports=$reached;fixed_target_count=6;fixed_targets=$fixed;not_observed_import_count=12;not_observed_imports=$notObserved;withheld_ffb_import_count=8;all_sdk_stubs_claimed=$false;focused_test_cases=2;focused_test_assertions=5;runtime_log='mcla.log';runtime_manifest=$probe.Manifest;controlled_exit_verified=$true;executable_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash;sdk_install_log_sha256=(Get-FileHash $sdkLog -Algorithm SHA256).Hash;focused_test_log_sha256=(Get-FileHash $testLog -Algorithm SHA256).Hash;app_clean_build_log_sha256=(Get-FileHash $appLog -Algorithm SHA256).Hash}
$resultPath=Join-Path $runRoot 'result.json';[IO.File]::WriteAllText($resultPath,(($result|ConvertTo-Json -Depth 8)+[Environment]::NewLine),[Text.UTF8Encoding]::new($false))
Write-Host 'M6-013 [5/5]: revalidating persisted coverage evidence...'
& (Join-Path $PSScriptRoot 'verify-reached-unsupported-report.ps1') -ResultPath $resultPath
