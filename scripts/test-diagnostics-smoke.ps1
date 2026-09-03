[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-diagnostics-smoke.ps1'
$root = Join-Path ([IO.Path]::GetTempPath()) ('mcla-diagnostics-test-' + [guid]::NewGuid().ToString('N'))
$utf8 = [Text.UTF8Encoding]::new($false)
$positive = 0
$negative = 0

function Write-Json([string]$Path, $Value) { [IO.File]::WriteAllText($Path, (($Value | ConvertTo-Json -Depth 12) + [Environment]::NewLine), $utf8) }
function Hash([string]$Path) { $s=[IO.File]::OpenRead($Path);$h=[Security.Cryptography.SHA256]::Create();try{return -join($h.ComputeHash($s)|ForEach-Object{$_.ToString('X2')})}finally{$h.Dispose();$s.Dispose()} }
function Expect-Failure([scriptblock]$Action, [string]$Label) { $failed=$false;try{&$Action|Out-Null}catch{$failed=$true};if(-not$failed){throw "Negative diagnostic fixture passed: $Label"};$script:negative++ }

try {
    $diagnostics = Join-Path $root 'diagnostics'
    $live = Join-Path $diagnostics 'live\live-20260903T000000Z-1-1'
    $crash = Join-Path $diagnostics 'crash\crash-20260903T000001Z-1'
    New-Item -ItemType Directory -Force -Path $live,$crash | Out-Null
    $dump = [byte[]]::new(64);[Text.Encoding]::ASCII.GetBytes('MDMP').CopyTo($dump,0);[BitConverter]::GetBytes([uint64]0x1020).CopyTo($dump,24)
    [IO.File]::WriteAllBytes((Join-Path $live 'process-private.dmp'),$dump)
    [IO.File]::WriteAllBytes((Join-Path $crash 'crash-private.dmp'),$dump)
    [IO.File]::WriteAllText((Join-Path $live 'log-tail.txt'),'safe',$utf8)
    [IO.File]::WriteAllText((Join-Path $crash 'runtime-journal-private.log'),'private',$utf8)
    [IO.File]::WriteAllText((Join-Path $crash 'README.txt'),'private',$utf8)
    $saveRelative='profile/545407F8/save.bin'
    $liveSave=Join-Path (Join-Path $live 'save-private') $saveRelative
    $crashSave=Join-Path (Join-Path $crash 'save-private') $saveRelative
    New-Item -ItemType Directory -Force -Path (Split-Path $liveSave),(Split-Path $crashSave) | Out-Null
    [IO.File]::WriteAllBytes($liveSave,[byte[]](1,2,3,4));[IO.File]::WriteAllBytes($crashSave,[byte[]](1,2,3,4))
    Write-Json (Join-Path $live 'save-files-private.json') ([ordered]@{schema='mcla-private-save-inventory-v1';safe_to_share=$false;files=@([ordered]@{path=$saveRelative;bytes=4;sha256=(Hash $liveSave)})})
    Write-Json (Join-Path $crash 'save-files-private.json') ([ordered]@{schema='mcla-private-save-inventory-v1';safe_to_share=$false;files=@([ordered]@{path=$saveRelative;bytes=4;sha256=(Hash $crashSave)})})
    Write-Json (Join-Path $live 'state.json') ([ordered]@{schema='mcla-diagnostic-state-v1';process=@{};window=@{};runtime=@{}})
    Write-Json (Join-Path $live 'save-metadata.json') ([ordered]@{schema='mcla-save-snapshot-metadata-v1';safe_to_share=$false})
    $liveManifest=[ordered]@{schema='mcla-diagnostic-package-v1';kind='live';mcla_version='0.8.0.0';privacy=[ordered]@{automatic_upload=$false;package_safe_to_share=$false};capture=[ordered]@{frame=$false;minidump=$true;save_files=1};artifacts=@(
        [ordered]@{name='state.json';bytes=(Get-Item (Join-Path $live 'state.json')).Length;sha256=(Hash (Join-Path $live 'state.json'));safe_to_share=$true},
        [ordered]@{name='log-tail.txt';bytes=4;sha256=(Hash (Join-Path $live 'log-tail.txt'));safe_to_share=$false},
        [ordered]@{name='save-metadata.json';bytes=(Get-Item (Join-Path $live 'save-metadata.json')).Length;sha256=(Hash (Join-Path $live 'save-metadata.json'));safe_to_share=$true},
        [ordered]@{name='save-files-private.json';bytes=(Get-Item (Join-Path $live 'save-files-private.json')).Length;sha256=(Hash (Join-Path $live 'save-files-private.json'));safe_to_share=$false},
        [ordered]@{name='process-private.dmp';bytes=64;sha256=(Hash (Join-Path $live 'process-private.dmp'));safe_to_share=$false})}
    $crashManifest=[ordered]@{schema='mcla-native-crash-package-v1';kind='native-crash';mcla_version='0.8.0.0';exception_code='0xE0434D44';exception_address='0x0000000000000001';privacy=[ordered]@{automatic_upload=$false;package_safe_to_share=$false};capture=[ordered]@{minidump=$true;save_files=1};artifacts=[ordered]@{
        'crash-private.dmp'=[ordered]@{bytes=64;sha256=(Hash (Join-Path $crash 'crash-private.dmp'));safe_to_share=$false}
        'runtime-journal-private.log'=[ordered]@{bytes=7;sha256=(Hash (Join-Path $crash 'runtime-journal-private.log'));safe_to_share=$false}
        'README.txt'=[ordered]@{bytes=7;sha256=(Hash (Join-Path $crash 'README.txt'));safe_to_share=$false}
        'save-files-private.json'=[ordered]@{bytes=(Get-Item (Join-Path $crash 'save-files-private.json')).Length;sha256=(Hash (Join-Path $crash 'save-files-private.json'));safe_to_share=$false}}}
    Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Write-Json (Join-Path $crash 'manifest.json') $crashManifest
    [IO.File]::WriteAllText((Join-Path $diagnostics 'latest-live.txt'),"live-20260903T000000Z-1-1`n",$utf8)
    [IO.File]::WriteAllText((Join-Path $diagnostics 'latest-crash.txt'),"crash-20260903T000001Z-1`n",$utf8)
    & $verify -LivePackage $live -CrashPackage $crash -Fixture | Out-Null;$positive++
    $located=& (Join-Path $PSScriptRoot 'show-latest-diagnostic.ps1') -UserDataRoot $root
    if($located.Path-notin@((Resolve-Path $live).Path,(Resolve-Path $crash).Path)){throw 'Latest diagnostic locator returned an unexpected package.'}

    [IO.File]::WriteAllText((Join-Path $diagnostics 'latest-live.txt'),"..\escape`n",$utf8)
    Expect-Failure { & (Join-Path $PSScriptRoot 'show-latest-diagnostic.ps1') -UserDataRoot $root -Kind Live } 'unsafe latest pointer'
    [IO.File]::WriteAllText((Join-Path $diagnostics 'latest-live.txt'),"live-20260903T000000Z-1-1`n",$utf8)

    $liveManifest.privacy.package_safe_to_share=$true;Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'unsafe live manifest'
    $liveManifest.privacy.package_safe_to_share=$false;Write-Json (Join-Path $live 'manifest.json') $liveManifest
    $bad=[byte[]]$dump.Clone();[BitConverter]::GetBytes([uint64]0x1022).CopyTo($bad,24);[IO.File]::WriteAllBytes((Join-Path $live 'process-private.dmp'),$bad);($liveManifest.artifacts|Where-Object name -CEQ 'process-private.dmp').sha256=Hash(Join-Path $live 'process-private.dmp');Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'full-memory dump flag'
    [IO.File]::WriteAllBytes((Join-Path $live 'process-private.dmp'),$dump);($liveManifest.artifacts|Where-Object name -CEQ 'process-private.dmp').sha256=Hash(Join-Path $live 'process-private.dmp');Write-Json (Join-Path $live 'manifest.json') $liveManifest
    $bad=[byte[]]$dump.Clone();[BitConverter]::GetBytes([uint64]0x11020).CopyTo($bad,24);[IO.File]::WriteAllBytes((Join-Path $live 'process-private.dmp'),$bad);($liveManifest.artifacts|Where-Object name -CEQ 'process-private.dmp').sha256=Hash(Join-Path $live 'process-private.dmp');Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'private-write-copy dump flag'
    [IO.File]::WriteAllBytes((Join-Path $live 'process-private.dmp'),$dump);($liveManifest.artifacts|Where-Object name -CEQ 'process-private.dmp').sha256=Hash(Join-Path $live 'process-private.dmp');Write-Json (Join-Path $live 'manifest.json') $liveManifest
    $crashManifest.privacy.automatic_upload=$true;Write-Json (Join-Path $crash 'manifest.json') $crashManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'automatic upload enabled'
    $crashManifest.privacy.automatic_upload=$false;Write-Json (Join-Path $crash 'manifest.json') $crashManifest
    [IO.File]::WriteAllText((Join-Path $crash 'unlisted-private.txt'),'private',$utf8)
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'unlisted crash artifact'
    Remove-Item -LiteralPath (Join-Path $crash 'unlisted-private.txt')
    Remove-Item -LiteralPath (Join-Path $crash 'runtime-journal-private.log')
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'missing crash journal'

    $source = [IO.File]::ReadAllText((Join-Path $repo 'src/mcla_app.cpp')) + [IO.File]::ReadAllText((Join-Path $repo 'src/mcla_diagnostics.cpp')) + [IO.File]::ReadAllText((Join-Path $repo 'src/mcla_native_crash_win.cpp')) + [IO.File]::ReadAllText((Join-Path $repo 'src/mcla_crash_handler_win.cpp')) + [IO.File]::ReadAllText((Join-Path $repo 'CMakeLists.txt'))
    foreach($needle in @('bind_mcla_debug_snapshot','"F10"','QueueDiagnosticSnapshot("f10")','mcla_diagnostics_snapshot_probe','mcla_native_crash_probe','mcla_native_crash_post_setup_probe','mcla_crash_reporter_dialog','MessageBoxW','RefreshCrashHandlers','live_completed_sequence','PROC_THREAD_ATTRIBUTE_HANDLE_LIST','EXTENDED_STARTUPINFO_PRESENT','rotating_file_sink_mt','kMaxSaveEntries','kMaxLiveDumpBytes','kMaxCrashDumpBytes','WriteTextAtomic','snapshot worker failed','CaptureWindowFrame','GPU readback/fence wait','MiniDumpWriteDump','SetUnhandledExceptionFilter','std::set_terminate','SIGABRT','CREATE_NO_WINDOW','MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules','automatic_upload','package_safe_to_share','save-private','save-files-private.json','runtime-journal-private.log','"frame.bmp", false','FILE_ATTRIBUTE_REPARSE_POINT','if (!WriteText(partial / "manifest.json", manifest.str()))','add_executable(mcla_crash_handler WIN32')) { if(-not$source.Contains($needle)){throw "Diagnostic source contract missing: $needle"} }
    foreach($forbidden in @('MiniDumpWithFullMemory |','MiniDumpWithHandleData |','MiniDumpWithPrivateReadWriteMemory |','basic_file_sink_mt')) { if($source.Contains($forbidden)){throw "Forbidden diagnostic mode found: $forbidden"} }
    [pscustomobject]@{Passed=$true;PositiveFixtures=$positive;NegativeFixtures=$negative;SourceChecks=35}
}
finally {
    if(Test-Path -LiteralPath $root){
        $resolved=(Resolve-Path -LiteralPath $root).Path
        $temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
        if(-not$resolved.StartsWith($temp+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Refusing to remove a diagnostics fixture outside the temp root.'}
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
