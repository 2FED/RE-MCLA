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

function Set-FrameFixture([string]$Classification, [byte[]]$Pixels) {
    if ($Pixels.Length -ne 8) { throw 'Frame fixture requires two BGRA pixels.' }
    $bmp = [byte[]]::new(62)
    $bmp[0] = 0x42; $bmp[1] = 0x4D
    [BitConverter]::GetBytes([uint32]62).CopyTo($bmp,2)
    [BitConverter]::GetBytes([uint32]54).CopyTo($bmp,10)
    [BitConverter]::GetBytes([uint32]40).CopyTo($bmp,14)
    [BitConverter]::GetBytes([int32]2).CopyTo($bmp,18)
    [BitConverter]::GetBytes([int32]-1).CopyTo($bmp,22)
    [BitConverter]::GetBytes([uint16]1).CopyTo($bmp,26)
    [BitConverter]::GetBytes([uint16]32).CopyTo($bmp,28)
    [BitConverter]::GetBytes([uint32]8).CopyTo($bmp,34)
    $Pixels.CopyTo($bmp,54)
    $path = Join-Path $live 'frame.bmp'
    [IO.File]::WriteAllBytes($path,$bmp)
    $liveManifest.capture.frame = $true
    $liveManifest.capture['frame_signal'] = [ordered]@{
        source='win32-client-bitblt';classification=$Classification;width=2;height=1
    }
    $liveManifest.artifacts = @($liveManifest.artifacts | Where-Object name -CNE 'frame.bmp') + @(
        [ordered]@{name='frame.bmp';bytes=$bmp.Length;sha256=(Hash $path);safe_to_share=$false})
    Write-Json (Join-Path $live 'manifest.json') $liveManifest
}

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
    Write-Json (Join-Path $live 'state.json') ([ordered]@{
        schema='mcla-diagnostic-state-v1'
        process=[ordered]@{native_thread_count=1}
        window=@{}
        runtime=@{}
        graphics=[ordered]@{
            backend=2;guest_frames_opened=10;guest_frames_closed=10;issue_swap_calls=10
            guest_output_refresh_successes=10;guest_output_refresh_failures=0
            async_placeholder_skips=0;swap_source_missing=0;submission_count=20
            frame_fence_wait_us=100;pipeline_end_submission_us=200
            deferred_execute_us=300;queue_submit_us=400;guest_frame_wall_us=5000
        }
        native_threads=@([ordered]@{id=1;base_priority=8;kernel_time_100ns=10;user_time_100ns=20;name='fixture'})
    })
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

    # The old fixture above has no frame_signal group, proving compatibility.
    $liveManifest.capture['frame_signal'] = [ordered]@{
        source='win32-client-bitblt';classification='unavailable';width=0;height=0
    }
    Write-Json (Join-Path $live 'manifest.json') $liveManifest
    & $verify -LivePackage $live -CrashPackage $crash -Fixture | Out-Null;$positive++
    Set-FrameFixture 'all-black' ([byte[]](0,0,0,0,0,0,0,255))
    & $verify -LivePackage $live -CrashPackage $crash -Fixture | Out-Null;$positive++
    Set-FrameFixture 'uniform-nonblack' ([byte[]](7,12,19,0,7,12,19,255))
    & $verify -LivePackage $live -CrashPackage $crash -Fixture | Out-Null;$positive++
    Set-FrameFixture 'nonuniform' ([byte[]](7,12,19,0,7,12,20,255))
    & $verify -LivePackage $live -CrashPackage $crash -Fixture | Out-Null;$positive++

    $liveManifest.capture.frame_signal.Remove('height');Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'partial frame signal group'
    $liveManifest.capture.frame_signal['height']=1
    $liveManifest.capture.frame_signal.source='vulkan-renderer';Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'false frame source'
    $liveManifest.capture.frame_signal.source='win32-client-bitblt'
    $liveManifest.capture.frame_signal.classification='healthy-gameplay';Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'unsupported frame quality claim'
    $liveManifest.capture.frame_signal.classification='nonuniform'
    $liveManifest.capture.frame_signal.width='2';Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'string frame dimension'
    $liveManifest.capture.frame_signal.width=-1;Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'negative frame dimension'
    $liveManifest.capture.frame_signal.width=8193;Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'unbounded frame dimension'
    $liveManifest.capture.frame_signal.width=0;Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'available frame without dimensions'
    $liveManifest.capture.frame_signal.width=1;Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'frame dimensions disagree with bitmap'
    Set-FrameFixture 'nonuniform' ([byte[]](0,0,0,0,0,0,0,255))
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'black pixels falsely called nonuniform'
    $liveManifest.capture.frame_signal.classification='uniform-nonblack';Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'black pixels falsely called nonblack'
    $liveManifest.capture.frame_signal=[ordered]@{source='win32-client-bitblt';classification='unavailable';width=0;height=0};Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'unavailable signal with written frame'
    $liveManifest.capture.frame_signal=$null;Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'null frame signal group'
    Set-FrameFixture 'nonuniform' ([byte[]](7,12,19,0,7,12,20,255))
    $framePath=Join-Path $live 'frame.bmp'
    $wrongHeader=[IO.File]::ReadAllBytes($framePath);$wrongHeader[28]=24
    [IO.File]::WriteAllBytes($framePath,$wrongHeader)
    ($liveManifest.artifacts | Where-Object name -CEQ 'frame.bmp').sha256=Hash $framePath
    Write-Json (Join-Path $live 'manifest.json') $liveManifest
    Expect-Failure { &$verify -LivePackage $live -CrashPackage $crash -Fixture } 'unsupported bitmap format despite matching artifact hash'

    Remove-Item -LiteralPath $framePath
    $liveManifest.artifacts=@($liveManifest.artifacts | Where-Object name -CNE 'frame.bmp')
    $liveManifest.capture.frame=$false
    $liveManifest.capture.Remove('frame_signal')
    Write-Json (Join-Path $live 'manifest.json') $liveManifest

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
    foreach($needle in @('bind_mcla_debug_snapshot','"F10"','QueueDiagnosticSnapshot("f10")','mcla_diagnostics_snapshot_probe','mcla_native_crash_probe','mcla_native_crash_post_setup_probe','mcla_crash_reporter_dialog','MessageBoxW','RefreshCrashHandlers','live_completed_sequence','PROC_THREAD_ATTRIBUTE_HANDLE_LIST','EXTENDED_STARTUPINFO_PRESENT','rotating_file_sink_mt','kMaxSaveEntries','kMaxLiveDumpBytes','kMaxCrashDumpBytes','WriteTextAtomic','snapshot worker failed','CaptureWindowFrame','GPU readback/fence wait','MiniDumpWriteDump','SetUnhandledExceptionFilter','std::set_terminate','SIGABRT','CREATE_NO_WINDOW','MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules','automatic_upload','package_safe_to_share','save-private','save-files-private.json','runtime-journal-private.log','"frame.bmp", false','FILE_ATTRIBUTE_REPARSE_POINT','if (!WriteText(partial / "manifest.json", manifest.str()))','add_executable(mcla_crash_handler WIN32','GetThreadTimes','GetThreadDescription','graphics_guest_frames_opened','NativeThreadSample')) { if(-not$source.Contains($needle)){throw "Diagnostic source contract missing: $needle"} }
    foreach($forbidden in @('MiniDumpWithFullMemory |','MiniDumpWithHandleData |','MiniDumpWithPrivateReadWriteMemory |','basic_file_sink_mt')) { if($source.Contains($forbidden)){throw "Forbidden diagnostic mode found: $forbidden"} }
    foreach($needle in @('F10: RECORDING...','F10: READY','F10: BUSY','F10: FAILED','snapshot_status() const','SnapshotStatus::kCapturing')) {
        if(-not$source.Contains($needle)){throw "Diagnostic status overlay contract missing: $needle"}
    }
    foreach($needle in @('void ShowCapture()', 'SetVisible(false)', 'SetVisible(true)',
                         'dialog->ShowCapture()', 'graphics_command_ring_idle_us',
                         'graphics_command_wait_reg_mem_us', 'graphics_memory_non_mmio_write_fault_count',
                         'graphics_memory_non_mmio_write_fault_us')) {
        if(-not$source.Contains($needle)){throw "Diagnostic wake/wait contract missing: $needle"}
    }
    $header = [IO.File]::ReadAllText((Join-Path $repo 'src/mcla_diagnostics.h'))
    $app = [IO.File]::ReadAllText((Join-Path $repo 'src/mcla_app.cpp'))
    $writer = [IO.File]::ReadAllText((Join-Path $repo 'src/mcla_diagnostics.cpp'))
    $details = @(
        'command_primary_buffer_count',
        'command_primary_buffer_us',
        'command_draw_packet_count',
        'command_draw_packet_us',
        'command_swap_packet_count',
        'command_swap_packet_us',
        'command_interrupt_packet_count',
        'command_interrupt_packet_us',
        'vulkan_fence_call_count',
        'vulkan_fence_call_us',
        'memory_write_fault_lock_wait_us',
        'memory_write_fault_query_us',
        'memory_write_fault_callback_us',
        'memory_write_fault_already_cleared_count',
        'memory_watch_arm_calls',
        'memory_watch_arm_us',
        'memory_watch_arm_lock_wait_us',
        'memory_watch_disarm_calls',
        'memory_watch_disarm_us',
        'memory_watch_protect_failures',
        'memory_watch_arm_gap_splits',
        'vulkan_resolve_count',
        'vulkan_resolve_us',
        'vulkan_resolve_readback_copy_count',
        'vulkan_resolve_readback_bytes',
        'vulkan_resolve_readback_copy_us',
        'vulkan_resolve_readback_wait_count',
        'vulkan_resolve_readback_wait_us',
        'vulkan_binding_update_count',
        'vulkan_binding_update_us',
        'vulkan_texture_descriptor_reuse_count',
        'vulkan_texture_descriptor_write_count',
        'vulkan_descriptor_update_count',
        'vulkan_descriptor_update_us'
    )
    foreach ($field in $details) {
        if (-not $header.Contains("uint64_t graphics_$field = 0;")) { throw "Missing initialized detail state: $field" }
        if (-not $app.Contains("state.graphics_$field = graphics.$field;")) { throw "Missing SDK detail mapping: $field" }
        if (-not $writer.Contains(('<< ", \"{0}\": " << runtime.graphics_{0}' -f $field))) { throw "Missing detail JSON field: $field" }
    }
    $frameGuards=@('FrameRgbClassName(frame.signal.classification)',
        'win32-client-bitblt','MeasureBgraFrame(pixels, width, height, width * 4)',
        'add_executable(mcla_frame_signal_test EXCLUDE_FROM_ALL','MCLA_BUILD_DIAGNOSTIC_TESTS')
    foreach($needle in $frameGuards){if(-not$source.Contains($needle)){throw "Missing frame source/signal contract: $needle"}}
    $processGuards=@('GetProcessTimes(GetCurrentProcess()',
        'creation_time_100ns = FileTimeTicks(process_creation);',
        '<< ", \"creation_time_100ns\": " << creation_time_100ns',
        'handles, creation_time_100ns, native_threads)')
    foreach($needle in $processGuards){if(-not$source.Contains($needle)){throw "Missing process lifetime contract: $needle"}}
    [pscustomobject]@{Passed=$true;PositiveFixtures=$positive;NegativeFixtures=$negative;SourceChecks=(53 + 3 * $details.Count + $frameGuards.Count + $processGuards.Count)}
}
finally {
    if(Test-Path -LiteralPath $root){
        $resolved=(Resolve-Path -LiteralPath $root).Path
        $temp=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')
        if(-not$resolved.StartsWith($temp+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Refusing to remove a diagnostics fixture outside the temp root.'}
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
