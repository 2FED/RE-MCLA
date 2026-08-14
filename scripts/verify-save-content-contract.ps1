[CmdletBinding(DefaultParameterSetName = 'Source')]
param(
    [Parameter(ParameterSetName = 'Source')][switch]$SourceOnly,
    [Parameter(Mandatory, ParameterSetName = 'Result')][string]$ResultPath,
    [Parameter(ParameterSetName = 'Result')][switch]$FixtureMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sdk = Join-Path $repo 'third_party/rexglue-sdk'
$sdkVersion = '0.9.0.21'
$sdkCommit = '3ef5b4f143d56b57e3c0e539cb0009ffe3a67e05'
$priorRun = '20260814-093131-ddca5b9d'
$priorHash = 'A582A41BB254D9187BC6F8147E89E2FF2E92D0EA4B79945760BA6FBCF334BC28'

function Assert-SourceContract {
    $content = [IO.File]::ReadAllText((Join-Path $sdk 'src/kernel/xam/xam_content.cpp'))
    $manager = [IO.File]::ReadAllText((Join-Path $sdk 'src/system/xam/content_manager.cpp'))
    $test = [IO.File]::ReadAllText((Join-Path $sdk 'tests/unit/system/xam_content_persistence_test.cpp'))
    $cmake = [IO.File]::ReadAllText((Join-Path $sdk 'tests/unit/CMakeLists.txt'))
    $imports = [IO.File]::ReadAllText((Join-Path $repo 'docs/evidence/M2-013-import-coverage.md'))
    foreach ($needle in @('XamContentCreateEx','XamContentClose','XamContentCreateEnumerator','XamContentGetDeviceData','XamContentGetDeviceState','NtWriteFile')) {
        if (-not $imports.Contains($needle)) { throw "Import evidence is missing '$needle'." }
    }
    $writeIndex = $content.IndexOf('result = content_manager->WriteContentHeaderFile(xuid, content_data);',[StringComparison]::Ordinal)
    $closeIndex = $content.IndexOf('content_manager->CloseContent(root_name);',$writeIndex,[StringComparison]::Ordinal)
    $deleteIndex = $content.IndexOf('content_manager->DeleteContent(xuid, content_data);',$closeIndex,[StringComparison]::Ordinal)
    if ($writeIndex -lt 0 -or $closeIndex -le $writeIndex -or $deleteIndex -le $closeIndex -or -not $content.Contains('disposition = kDispositionState::Unknown;')) { throw 'CreateEx does not fail closed and roll back after header-write failure.' }
    foreach ($needle in @('fwrite(&data, 1, sizeof(XCONTENT_AGGREGATE_DATA), file) ==','fclose(file) == 0','std::filesystem::remove(header_path, error);','return X_ERROR_ACCESS_DENIED;')) {
        if (-not $manager.Contains($needle)) { throw "Durable header contract is missing '$needle'." }
    }
    foreach ($needle in @('XAM saved-game metadata survives manager restart','XAM saved-game metadata rejects truncated headers','ContentManager restarted(nullptr, temp.path())','listed.size() == 1')) {
        if (-not $test.Contains($needle)) { throw "Focused persistence test is missing '$needle'." }
    }
    if (-not $cmake.Contains('system/xam_content_persistence_test.cpp')) { throw 'Focused persistence test is not registered.' }
    [pscustomobject]@{ SourceContractVerified = $true; SourceChecks = 15 }
}

function Assert-ExactProperties($Object, [string[]]$Names, [string]$Description) {
    if (($Object.PSObject.Properties.Name -join ',') -cne ($Names -join ',')) { throw "$Description schema is invalid." }
}

$source = Assert-SourceContract
if ($PSCmdlet.ParameterSetName -eq 'Source') { return $source }

$full = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($ResultPath)) { $ResultPath } else { Join-Path $repo $ResultPath }))
$rootPrefix = (Join-Path $repo 'private/evidence/M5-011').TrimEnd('\') + '\'
if (-not $full.StartsWith($rootPrefix,[StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $full -PathType Leaf)) { throw 'Result path is outside private M5-011 evidence.' }
$walk = Split-Path $full
while ($walk.StartsWith($repo,[StringComparison]::OrdinalIgnoreCase)) {
    if ((Get-Item -LiteralPath $walk -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Result path traverses a reparse point.' }
    if ([string]::Equals($walk,$repo,[StringComparison]::OrdinalIgnoreCase)) { break }
    $walk = Split-Path $walk
}
$raw = [IO.File]::ReadAllText($full)
if ($raw -match '(?i)[A-Z]:\\|controller|serial|xuid') { throw 'Result leaks a path or private identity.' }
$r = $raw | ConvertFrom-Json
$properties = @('schema','task','decision','sdk_version','sdk_commit','build_configuration','prior_evidence','focused_test','source_files','scope')
if (($r.PSObject.Properties.Name -join ',') -cne ($properties -join ',') -or $r.schema -ne 1 -or $r.task -cne 'M5-011' -or $r.decision -cne 'save-content-prerequisite-pass' -or $r.sdk_version -cne $sdkVersion -or $r.sdk_commit -cne $sdkCommit -or $r.build_configuration -cne 'RelWithDebInfo') { throw 'Result identity is invalid.' }
Assert-ExactProperties $r.prior_evidence @('task','run_id','result_sha256','existing_save_mount_read_verified') 'Prior evidence'
Assert-ExactProperties $r.focused_test @('cases','assertions','restart_roundtrip','truncated_header_rejected','build_log_sha256','test_log_sha256','unit_executable_sha256') 'Focused test'
Assert-ExactProperties $r.scope @('create_header_failure_rolls_back','race_result_write_physically_reached','race_result_persistence_claimed','next_task') 'Scope'
if ($r.prior_evidence.task -cne 'M5-002' -or $r.prior_evidence.run_id -cne $priorRun -or $r.prior_evidence.result_sha256 -cne $priorHash -or $r.prior_evidence.existing_save_mount_read_verified -ne $true) { throw 'Prior physical save-read evidence is invalid.' }
if ($r.focused_test.cases -ne 2 -or $r.focused_test.assertions -ne 16 -or $r.focused_test.restart_roundtrip -ne $true -or $r.focused_test.truncated_header_rejected -ne $true -or $r.focused_test.build_log_sha256 -notmatch '^[0-9A-F]{64}$' -or $r.focused_test.test_log_sha256 -notmatch '^[0-9A-F]{64}$' -or $r.focused_test.unit_executable_sha256 -notmatch '^[0-9A-F]{64}$') { throw 'Focused persistence result is invalid.' }
if ($r.scope.create_header_failure_rolls_back -ne $true -or $r.scope.race_result_write_physically_reached -ne $false -or $r.scope.race_result_persistence_claimed -ne $false -or $r.scope.next_task -cne 'M5-012') { throw 'Result scope overclaims persistence.' }
$expectedSources = @('third_party/rexglue-sdk/src/kernel/xam/xam_content.cpp','third_party/rexglue-sdk/src/system/xam/content_manager.cpp','third_party/rexglue-sdk/tests/unit/system/xam_content_persistence_test.cpp')
if (@($r.source_files).Count -ne $expectedSources.Count) { throw 'Source manifest count is invalid.' }
for ($i=0; $i -lt $expectedSources.Count; $i++) {
    Assert-ExactProperties $r.source_files[$i] @('path','sha256') 'Source entry'
    if ($r.source_files[$i].path -cne $expectedSources[$i] -or $r.source_files[$i].sha256 -notmatch '^[0-9A-F]{64}$') { throw 'Source manifest is invalid.' }
}
if (-not $FixtureMode) {
    $prior = Join-Path $repo "private/evidence/M5-002/$priorRun/result.json"
    if (-not (Test-Path -LiteralPath $prior -PathType Leaf) -or (Get-FileHash $prior -Algorithm SHA256).Hash -cne $priorHash) { throw 'Prior physical evidence drifted.' }
    $tag = (& git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim(); $head = (& git -C $sdk rev-parse HEAD).Trim()
    if ($tag -cne "v$sdkVersion" -or $head -cne $sdkCommit -or (git -C $sdk status --porcelain)) { throw 'SDK identity or cleanliness is invalid.' }
    $runRoot = Split-Path $full
    $children = @(Get-ChildItem -LiteralPath $runRoot -Force)
    if ($children.Count -ne 3 -or (@($children.Name | Sort-Object) -join ',') -cne 'result.json,sdk-build.log,sdk-content-test.log') { throw 'Evidence topology is invalid.' }
    foreach ($name in @('sdk-build.log','sdk-content-test.log')) {
        $file = Join-Path $runRoot $name
        if (-not (Test-Path -LiteralPath $file -PathType Leaf) -or ((Get-Item $file).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Evidence file '$name' is invalid." }
    }
    $testLog = Join-Path $runRoot 'sdk-content-test.log'
    $unit = Join-Path $sdk 'out/win-amd64/RelWithDebInfo/unit_tests.exe'
    if ((Get-FileHash (Join-Path $runRoot 'sdk-build.log') -Algorithm SHA256).Hash -cne $r.focused_test.build_log_sha256 -or (Get-FileHash $testLog -Algorithm SHA256).Hash -cne $r.focused_test.test_log_sha256 -or (Get-FileHash $unit -Algorithm SHA256).Hash -cne $r.focused_test.unit_executable_sha256 -or [IO.File]::ReadAllText($testLog) -notmatch 'All tests passed \(16 assertions in 2 test cases\)') { throw 'Focused test evidence is not physically bound.' }
    foreach ($entry in $r.source_files) {
        $path = Join-Path $repo $entry.path
        if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-FileHash $path -Algorithm SHA256).Hash -cne $entry.sha256) { throw 'Source identity drifted.' }
    }
}
[pscustomobject]@{ Passed=$true; Decision=$r.decision; ExistingSaveReadVerified=$true; RestartRoundtripVerified=$true; RaceResultWritePhysicallyReached=$false; SourceContractVerified=$source.SourceContractVerified }
