[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path;$verify=Join-Path $PSScriptRoot 'verify-xmp-route-smoke.ps1';$run=Join-Path $PSScriptRoot 'run-xmp-route-smoke.ps1';$root=Join-Path $repo ('private/evidence/M4-008/test-'+[guid]::NewGuid().ToString('N').Substring(0,8));New-Item -ItemType Directory -Force $root|Out-Null
function Line([string]$Text){"[2026-08-13 18:00:00.000] [info] [xam] [t1] $Text"}
try{
  $bmp=Join-Path $root 'mcla-first-frame.bmp';$bytes=[byte[]]::new(3686454);$bytes[0]=0x42;$bytes[1]=0x4D;[BitConverter]::GetBytes([int]1280).CopyTo($bytes,18);[BitConverter]::GetBytes([int]720).CopyTo($bytes,22);[BitConverter]::GetBytes([uint16]32).CopyTo($bytes,28);[IO.File]::WriteAllBytes($bmp,$bytes)
  $base=@(
    (Line 'XMP_AUDIT_CONFIG v=1 enabled=1 policy=metadata-only-fallback decoder=0 record_limit=128'),
    (Line 'XMP_AUDIT_MESSAGE v=1 id=0 msg=00070009 class=query result=00000000 before=0 after=0 client=1 playlists=0 active_songs=0 decoder=0 consistent=1'),
    (Line 'MCLA graphics: nontrivial guest frame captured 1280x720, rgb555 bins 1, luma p05 1, luma p95 2, modal permille 1, nonmodal grid cells 1'),
    (Line 'XMP_AUDIT_SUMMARY v=1 phase=title status=PASS calls=1200 known_calls=1200 query_calls=1200 playback_calls=0 unsupported_calls=0 state_changes=0 unexpected_calls=0 inconsistent_calls=0 records=1 overflow=0 dropped_records=0'),
    (Line 'MCLA audio: XMP title route summarized'),
    (Line 'Window closing, shutting down...'),
    (Line 'Execution complete'),
    (Line 'Title terminated; hard-exiting process.')
  )
  $log=Join-Path $root 'mcla.log';[IO.File]::WriteAllLines($log,$base);$positive=&$verify -ProbeOnly -XmpOnly -RuntimeLogPath $log -BmpPath $bmp;if(-not$positive.Passed){throw 'Positive XMP fixture failed.'}
  $cases=[ordered]@{
    'missing-config'=@($base|Where-Object{$_-notmatch'AUDIT_CONFIG'});
    'summary-fail'=@($base-replace'phase=title status=PASS','phase=title status=FAIL');
    'wrong-message'=@($base-replace'msg=00070009','msg=00070008');
    'state-change'=@($base-replace'before=0 after=0','before=0 after=1');
    'playback-call'=@($base-replace'playback_calls=0','playback_calls=1');
    'query-drift'=@($base-replace'query_calls=1200','query_calls=1199');
    'known-drift'=@($base-replace'known_calls=1200','known_calls=1199');
    'overflow'=@($base-replace'overflow=0','overflow=1');
    'inconsistent'=@($base-replace'consistent=1','consistent=0');
    'record-drift'=@($base-replace'records=1','records=2');
    'decoder-present'=@($base-replace'decoder=0 record_limit','decoder=1 record_limit');
    'duplicate-summary'=@($base[0..3]+$base[3]+$base[4..7]);
    'summary-before-capture'=@($base[0..1]+$base[3]+$base[2]+$base[4..7]);
    'missing-complete'=@($base|Where-Object{$_-notmatch'Execution complete'});
    'hard-exit-before-complete'=@($base[0..5]+$base[7]+$base[6]);
    'fatal-tail'=@($base+(Line '[fatal] fixture'))
  }
  $failed=0;foreach($case in $cases.GetEnumerator()){[IO.File]::WriteAllLines($log,$case.Value);try{$null=&$verify -ProbeOnly -XmpOnly -RuntimeLogPath $log -BmpPath $bmp;throw "Negative '$($case.Key)' was accepted."}catch{if($_.Exception.Message-like"Negative '* was accepted."){throw};$failed++}}
  $sourceFiles=@(
    $run,$verify,(Join-Path $repo 'src/mcla_app.cpp'),
    (Join-Path $repo 'third_party/rexglue-sdk/include/rex/kernel/xam/xmp_audit.h'),
    (Join-Path $repo 'third_party/rexglue-sdk/src/kernel/xam/xmp_audit.cpp'),
    (Join-Path $repo 'third_party/rexglue-sdk/src/kernel/xam/apps/xmp_app.cpp'),
    (Join-Path $repo 'third_party/rexglue-sdk/tests/unit/kernel/xmp_fallback_test.cpp'),
    (Join-Path $repo 'third_party/rexglue-sdk/tests/unit/CMakeLists.txt')
  );$source=($sourceFiles|ForEach-Object{[IO.File]::ReadAllText($_)})-join"`n"
  $needles=@(
    'REXCVAR_DEFINE_BOOL(xmp_route_audit, false','Lifecycle::kInitOnly','BeginXmpRouteMessage()','condition_.wait(lock','active_calls_ == 0','accepting_ = false',
    'Title playlist playback is unsupported; remaining idle','state_ = State::kIdle','return X_E_FAIL','!active_playlist_ || active_playlist_->songs.empty()',
    'case 0x00070025','case 0x0007003B','XMP output capture is unsupported','EmitXmpRouteAuditSummary("title")','--xmp_route_audit=true',
    'Exact PID/window WM_CLOSE failed.','Source-game identity changed.','Runtime artifacts changed during the cycle.','--log_level=trace','NativeLogged','safe.directory','xmp_fallback_test.cpp','All tests passed \(20 assertions in 4 test cases\)'
  );foreach($needle in $needles){if(-not$source.Contains($needle)){throw "Source contract missing '$needle'."}}
  $begin=$source.IndexOf('accepting_ = false');$wait=$source.IndexOf('condition_.wait(lock');$snapshot=$source.IndexOf('summary_ = true',$wait);if($begin-lt0-or$wait-le$begin-or$snapshot-le$wait){throw 'XMP audit freeze/drain source order is invalid.'}
  [pscustomobject]@{Passed=$true;Positives=1;FailClosedNegatives=$failed;SourceContractChecks=$needles.Count+1;FocusedSdkTestCases=4;FocusedSdkAssertions=20}
}finally{Remove-Item $root -Recurse -Force -ErrorAction SilentlyContinue}
