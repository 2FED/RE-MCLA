[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-save-content-contract.ps1'
$source = & $verify -SourceOnly
if (-not $source.SourceContractVerified -or $source.SourceChecks -ne 15) { throw 'Source contract failed.' }
$sdkVersion='0.9.0.21';$sdkCommit='3ef5b4f143d56b57e3c0e539cb0009ffe3a67e05'
function New-Good {
    [ordered]@{schema=1;task='M5-011';decision='save-content-prerequisite-pass';sdk_version=$sdkVersion;sdk_commit=$sdkCommit;build_configuration='RelWithDebInfo';prior_evidence=[ordered]@{task='M5-002';run_id='20260814-093131-ddca5b9d';result_sha256='A582A41BB254D9187BC6F8147E89E2FF2E92D0EA4B79945760BA6FBCF334BC28';existing_save_mount_read_verified=$true};focused_test=[ordered]@{cases=2;assertions=16;restart_roundtrip=$true;truncated_header_rejected=$true;build_log_sha256=('A'*64);test_log_sha256=('B'*64);unit_executable_sha256=('C'*64)};source_files=@([ordered]@{path='third_party/rexglue-sdk/src/kernel/xam/xam_content.cpp';sha256=('D'*64)},[ordered]@{path='third_party/rexglue-sdk/src/system/xam/content_manager.cpp';sha256=('E'*64)},[ordered]@{path='third_party/rexglue-sdk/tests/unit/system/xam_content_persistence_test.cpp';sha256=('F'*64)});scope=[ordered]@{create_header_failure_rolls_back=$true;race_result_write_physically_reached=$false;race_result_persistence_claimed=$false;next_task='M5-012'}}
}
$root=Join-Path $repo ('private/evidence/M5-011/test-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root)|Out-Null
try {
    $path=Join-Path $root 'result.json';$good=New-Good;[IO.File]::WriteAllText($path,($good|ConvertTo-Json -Depth 7));&$verify -ResultPath $path -FixtureMode|Out-Null
    $cases=@(
        {param($x)$x.schema=2}, {param($x)$x.task='M5-012'}, {param($x)$x.decision='pass'}, {param($x)$x.sdk_version='0'},
        {param($x)$x.prior_evidence.result_sha256='0'}, {param($x)$x.prior_evidence.existing_save_mount_read_verified=$false},
        {param($x)$x.focused_test.cases=1}, {param($x)$x.focused_test.restart_roundtrip=$false},
        {param($x)$x.scope.race_result_write_physically_reached=$true}, {param($x)$x.scope.race_result_persistence_claimed=$true},
        {param($x)$x.scope.next_task='M6-005'}, {param($x)$x.source_files=@()}
    )
    $failed=0
    foreach($mutate in $cases){$bad=New-Good;&$mutate $bad;[IO.File]::WriteAllText($path,($bad|ConvertTo-Json -Depth 7));try{&$verify -ResultPath $path -FixtureMode|Out-Null}catch{$failed++}}
    if($failed-ne$cases.Count){throw "Only $failed/$($cases.Count) negative fixtures failed closed."}
    [pscustomobject]@{Passed=$true;Positives=1;FailClosedNegatives=$failed;SourceChecks=$source.SourceChecks}
} finally { Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue }
