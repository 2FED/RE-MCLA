[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sdk = Join-Path $repo 'third_party/rexglue-sdk'
$verify = Join-Path $PSScriptRoot 'verify-save-content-contract.ps1'
$sdkVersion = '0.9.0.21'
$sdkCommit = '3ef5b4f143d56b57e3c0e539cb0009ffe3a67e05'
& $verify -SourceOnly | Out-Null
$tag = (& git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim(); $head = (& git -C $sdk rev-parse HEAD).Trim()
if ($tag -cne "v$sdkVersion" -or $head -cne $sdkCommit -or (git -C $sdk status --porcelain)) { throw 'M5-011 requires the clean exact SDK release.' }
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$runRoot = Join-Path $repo ('private/evidence/M5-011/' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
[IO.Directory]::CreateDirectory($runRoot) | Out-Null
$buildLog = Join-Path $runRoot 'sdk-build.log'; $testLog = Join-Path $runRoot 'sdk-content-test.log'
Write-Host 'M5-011 [1/3]: clean-building/installing the exact SDK and focused tests...' -ForegroundColor Cyan
Push-Location $sdk
try {
    & $toolchain.CMakePath --preset win-amd64 -DREXGLUE_BUILD_TESTS=ON *>&1 | Tee-Object -FilePath $buildLog | Out-Null
    if ($LASTEXITCODE) { throw "SDK configure failed. Private run: '$runRoot'." }
    & $toolchain.CMakePath --build out/build/win-amd64 --config RelWithDebInfo --target install unit_tests --clean-first --parallel 8 *>&1 | Tee-Object -FilePath $buildLog -Append | Out-Null
    if ($LASTEXITCODE) { throw "SDK build/install failed. Private run: '$runRoot'." }
} finally { Pop-Location }
Write-Host 'M5-011 [2/3]: verifying durable metadata across manager restart...' -ForegroundColor Cyan
$unit = Join-Path $sdk 'out/win-amd64/RelWithDebInfo/unit_tests.exe'
& $unit '[system][xam][content]' *>&1 | Tee-Object -FilePath $testLog | Out-Null
if ($LASTEXITCODE -or [IO.File]::ReadAllText($testLog) -notmatch 'All tests passed \(16 assertions in 2 test cases\)') { throw "Focused persistence tests failed. Private run: '$runRoot'." }
$sources = @('third_party/rexglue-sdk/src/kernel/xam/xam_content.cpp','third_party/rexglue-sdk/src/system/xam/content_manager.cpp','third_party/rexglue-sdk/tests/unit/system/xam_content_persistence_test.cpp') | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash (Join-Path $repo $_) -Algorithm SHA256).Hash} }
$result = [ordered]@{
    schema=1; task='M5-011'; decision='save-content-prerequisite-pass'; sdk_version=$sdkVersion; sdk_commit=$sdkCommit; build_configuration='RelWithDebInfo'
    prior_evidence=[ordered]@{task='M5-002';run_id='20260814-093131-ddca5b9d';result_sha256='A582A41BB254D9187BC6F8147E89E2FF2E92D0EA4B79945760BA6FBCF334BC28';existing_save_mount_read_verified=$true}
    focused_test=[ordered]@{cases=2;assertions=16;restart_roundtrip=$true;truncated_header_rejected=$true;build_log_sha256=(Get-FileHash $buildLog -Algorithm SHA256).Hash;test_log_sha256=(Get-FileHash $testLog -Algorithm SHA256).Hash;unit_executable_sha256=(Get-FileHash $unit -Algorithm SHA256).Hash}
    source_files=@($sources)
    scope=[ordered]@{create_header_failure_rolls_back=$true;race_result_write_physically_reached=$false;race_result_persistence_claimed=$false;next_task='M5-012'}
}
$resultPath = Join-Path $runRoot 'result.json'
[IO.File]::WriteAllText($resultPath,(($result|ConvertTo-Json -Depth 7)+[Environment]::NewLine),[Text.UTF8Encoding]::new($false))
Write-Host 'M5-011 [3/3]: revalidating physical result and prior save-read evidence...' -ForegroundColor Cyan
$final = & $verify -ResultPath $resultPath
[pscustomobject]@{Passed=$final.Passed;Decision=$final.Decision;ExistingSaveReadVerified=$final.ExistingSaveReadVerified;RestartRoundtripVerified=$final.RestartRoundtripVerified;RaceResultWritePhysicallyReached=$final.RaceResultWritePhysicallyReached;ResultPath=$resultPath}
