[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$UnitTestsPath)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$exe=(Resolve-Path -LiteralPath $UnitTestsPath).Path
if((Split-Path -Leaf $exe)-cne'unit_tests.exe'){throw 'Expected the SDK unit test executable.'}
$evidence=Join-Path $repo ('private/evidence/M7-002/sdk-unit-isolated-'+[guid]::NewGuid().ToString('N'))
$null=New-Item -ItemType Directory -Path $evidence
$utf8=[Text.UTF8Encoding]::new($false)
$hash=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash
[xml]$inventory=(@(& $exe '*' --list-tests --reporter xml)-join"`n")
if($LASTEXITCODE-ne0){throw 'SDK test discovery failed.'}
$cases=@($inventory.MatchingTests.TestCase)
if(-not$cases.Count){throw 'SDK test inventory is empty.'}
$groups=@($cases|Group-Object {$_.SourceInfo.File})
$seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$results=@();$total=0;$assertions=0;$skips=0
foreach($group in $groups){
    $stem=[IO.Path]::GetFileNameWithoutExtension($group.Name)
    if($stem-cnotmatch'^[a-z0-9_]+$'-or-not$seen.Add($stem)){throw 'SDK filename tag is unsafe or ambiguous.'}
    # Two existing suites keep process-lifetime fixed guest-memory mappings.
    # Isolate source suites rather than weaken Memory::Initialize or skip tests.
    $output=Join-Path $evidence ($stem+'.xml')
    $info=[Diagnostics.ProcessStartInfo]::new()
    $info.FileName=$exe;$info.WorkingDirectory=$evidence
    $info.UseShellExecute=$false;$info.CreateNoWindow=$true
    $info.Arguments='-# "[#'+$stem+']" --reporter xml --rng-seed 346143289 --out "'+$output+'"'
    $process=[Diagnostics.Process]::new();$process.StartInfo=$info
    try{
        if(-not$process.Start()){throw 'SDK suite process did not start.'}
        if(-not$process.WaitForExit(60000)){$process.Kill();$process.WaitForExit();throw "SDK suite timed out: $stem"}
        if($process.ExitCode-ne0){throw "SDK suite failed: $stem, exit $($process.ExitCode); evidence retained."}
    }finally{$process.Dispose()}
    [xml]$report=[IO.File]::ReadAllText($output)
    $summary=$report.Catch2TestRun.OverallResultsCases
    $count=[int]$summary.successes+[int]$summary.failures+[int]$summary.expectedFailures+[int]$summary.skips
    if($count-ne$group.Count-or[int]$summary.failures-ne0-or[int]$summary.expectedFailures-ne0){throw "SDK suite coverage/failure mismatch: $stem"}
    foreach($case in @($report.Catch2TestRun.TestCase)){
        if($case.filename-cne$group.Name){throw 'SDK suite ran a case from another source.'}
    }
    $expectedNames=@($group.Group | ForEach-Object { [string]$_.Name } | Sort-Object)
    $actualNames=@($report.Catch2TestRun.TestCase | ForEach-Object { [string]$_.name } | Sort-Object)
    if (@(Compare-Object $expectedNames $actualNames -CaseSensitive).Count) {
        throw "SDK suite case identities differ from discovery: $stem"
    }
    $total+=$count;$skips+=[int]$summary.skips
    $assertions+=[int]$report.Catch2TestRun.OverallResults.successes
    $results+= [pscustomobject]@{source=$group.Name;cases=$count;skipped=[int]$summary.skips;report_sha256=(Get-FileHash -LiteralPath $output).Hash}
}
if($total-ne$cases.Count-or(Get-FileHash -LiteralPath $exe).Hash-cne$hash){throw 'SDK complete-suite coverage or executable identity drifted.'}
$receipt=[ordered]@{schema='mcla-sdk-source-isolated-tests-v1';executable_sha256=$hash;cases=$total;assertions=$assertions;skipped=$skips;suites=$results;scope='Every discovered case, isolated by source file; not a monolithic-process PASS'}
[IO.File]::WriteAllText((Join-Path $evidence 'result.json'),($receipt|ConvertTo-Json -Depth 6),$utf8)
[pscustomobject]@{Passed=$true;Cases=$total;Assertions=$assertions;Skipped=$skips;Suites=$groups.Count;Evidence=$evidence}
