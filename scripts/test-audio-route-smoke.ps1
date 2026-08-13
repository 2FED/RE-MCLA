[CmdletBinding()]param()
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path;$verify=Join-Path $PSScriptRoot 'verify-audio-route-smoke.ps1';$run=Join-Path $PSScriptRoot 'run-audio-route-smoke.ps1';$root=Join-Path $repo ('private/evidence/M4-007/test-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory $root|Out-Null
function Line([string]$s){"[2026-08-13 16:00:00.000] [info] [apu] [t1] $s"}
$lines=@(
  (Line 'SDL_AUDIO_AUDIT_CONFIG v=1 enabled=1 backend=sdl sample_rate=48000 source_channels=6'),
  (Line 'SDL_AUDIO_AUDIT_CLIENT v=1 event=register result=00000000 index_class=bounded'),
  (Line 'SDL_AUDIO_AUDIT_FRAME v=1 layer=submit class=silence finite=1 channels=6 peak_ppm=0'),
  (Line 'SDL_AUDIO_AUDIT_FRAME v=1 layer=device class=silence finite=1 channels=2 peak_ppm=0'),
  (Line 'SDL_AUDIO_AUDIT_FRAME v=1 layer=xma class=nonzero finite=1 channels=0 peak_ppm=1617'),
  (Line 'MCLA graphics: nontrivial guest frame captured 1280x720, rgb555 bins 1, luma p05 1, luma p95 2, modal permille 1, nonmodal grid cells 1'),
  (Line 'MCLA audio: title soak started seconds 300'),
  (Line 'SDL_AUDIO_AUDIT_SUMMARY v=1 phase=title status=PASS client_calls=1 client_success=1 submit_frames=60000 submit_nonzero=50000 submit_invalid=0 submit_peak_ppm=7000 device_frames=59998 device_nonzero=49998 device_invalid=0 device_submit_fail=0 device_peak_ppm=6000 xma_frames=90000 xma_nonzero=90000 xma_invalid=0 xma_peak_ppm=900000 max_queue_depth=4 starvation_fills=2 max_consecutive_starvation_fills=1 dropped_records=0'),
  (Line 'MCLA audio: title soak completed seconds 300')
)
$log=Join-Path $root 'mcla.log';$bmp=Join-Path $root 'unused.bmp';[IO.File]::WriteAllLines($log,$lines);$bmpBytes=[byte[]]::new(3686454);$bmpBytes[0]=0x42;$bmpBytes[1]=0x4D;[BitConverter]::GetBytes([int]1280).CopyTo($bmpBytes,18);[BitConverter]::GetBytes([int]720).CopyTo($bmpBytes,22);[BitConverter]::GetBytes([uint16]32).CopyTo($bmpBytes,28);[IO.File]::WriteAllBytes($bmp,$bmpBytes)
$positive=&$verify -ProbeOnly -AudioOnly -RuntimeLogPath $log -BmpPath $bmp;if(-not$positive.Passed){throw 'Positive audio fixture failed.'}
$cases=[ordered]@{
 'missing-config'=@($lines|Where-Object{$_-notmatch'AUDIT_CONFIG'});
 'duplicate-client'=@($lines[0..1]+$lines[1]+$lines[2..($lines.Count-1)]);
 'missing-xma'=@($lines|Where-Object{$_-notmatch'layer=xma'});
 'nonfinite'=@($lines-replace'layer=device class=silence finite=1','layer=device class=silence finite=0');
 'summary-fail'=@($lines-replace'phase=title status=PASS','phase=title status=FAIL');
 'submit-zero'=@($lines-replace'submit_nonzero=50000','submit_nonzero=0');
 'device-failure'=@($lines-replace'device_submit_fail=0','device_submit_fail=1');
 'xma-zero'=@($lines-replace'xma_nonzero=90000','xma_nonzero=0');
 'queue-runaway'=@($lines-replace'max_queue_depth=4','max_queue_depth=65');
 'starvation-run'=@($lines-replace'max_consecutive_starvation_fills=1','max_consecutive_starvation_fills=3');
 'dropped'=@($lines-replace'dropped_records=0','dropped_records=1');
 'wrong-order'=@($lines[0..4]+$lines[6]+$lines[5]+$lines[7..8]);
 'banned-sdl-failure'=@($lines+(Line 'SDL_PutAudioStreamData() failed: fixture'))
}
$failed=0;foreach($case in $cases.GetEnumerator()){[IO.File]::WriteAllLines($log,$case.Value);try{$null=&$verify -ProbeOnly -AudioOnly -RuntimeLogPath $log -BmpPath $bmp;throw "Negative '$($case.Key)' was accepted."}catch{if($_.Exception.Message-like"Negative '* was accepted."){throw};$failed++}}
$source=[IO.File]::ReadAllText($run)+[IO.File]::ReadAllText($verify)+[IO.File]::ReadAllText((Join-Path $repo 'src/mcla_app.cpp'))+[IO.File]::ReadAllText((Join-Path $repo 'third_party/rexglue-sdk/src/audio/audio_route_audit.cpp'))
foreach($needle in @('--sdl_audio_route_audit=true','--mcla_audio_route_soak_seconds=300','title soak started seconds 300','title soak completed seconds 300','max_queue_depth','max_consecutive_starvation_fills','EmitAudioRouteAuditSummary("title")')){if(-not$source.Contains($needle)){throw "Source contract missing '$needle'."}}
Remove-Item $root -Recurse -Force
[pscustomobject]@{Passed=$true;Positives=1;FailClosedNegatives=$failed;SourceContractChecks=7;FocusedSdkTestCases=5;FocusedSdkAssertions=16}
