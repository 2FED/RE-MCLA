[CmdletBinding()]param()
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
$verifier=Join-Path $PSScriptRoot 'verify-performance-telemetry-smoke.ps1';$root=Join-Path $env:TEMP ('m6-012-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory $root|Out-Null
try {
  $lines=@('REX_PERF_AUDIT_CONFIG v=1 enabled=1 backend=d3d12 sample_limit=300 streaming_stall_us=5000');$reads=0L;$bytes=0L;$audio=0L;$shader=0L;$shaderus=0L;$pso=0L;$psous=0L
  for($i=0;$i-lt300;$i++){ $r=if($i-eq0){3}else{0};$b=$r*32768;$a=if($i%30-eq0){1}else{0};$sh=if($i-eq0){2}else{0};$su=$sh*100;$ps=if($i-eq0){1}else{0};$pu=$ps*200;$reads+=$r;$bytes+=$b;$audio+=$a;$shader+=$sh;$shaderus+=$su;$pso+=$ps;$psous+=$pu;$lines+="REX_PERF_AUDIT_SAMPLE v=1 sample=$i host_us=$([long]($i+1)*40000) guest_frame=$($i+1) cpu_us=33000 gpu_us=1000 stream_reads=$r stream_bytes=$b stream_stalls=0 stream_max_us=100 audio_underruns=$a shader_count=$sh shader_fail=0 shader_us=$su pso_count=$ps pso_fail=0 pso_us=$pu" }
  $lines+="REX_PERF_AUDIT_SUMMARY v=1 status=PASS samples=300 stream_reads=$reads stream_bytes=$bytes stream_stalls=0 audio_underruns=$audio shader_count=$shader shader_fail=0 shader_us=$shaderus pso_count=$pso pso_fail=0 pso_us=$psous marker_overflow=0"
  $base=Join-Path $root 'mcla.log';[IO.File]::WriteAllLines($base,$lines,[Text.UTF8Encoding]::new($false));$positive=& $verifier -LogPath $base;if(-not$positive.Passed){throw 'Positive fixture failed.'}
  $cases=@(
    @{n='missing-config';f={param($x)@($x|?{$_-notmatch 'CONFIG'})}},@{n='duplicate-config';f={param($x)@($x[0])+$x}},
    @{n='missing-sample';f={param($x)@($x|?{$_-notmatch 'sample=12 '})}},@{n='duplicate-summary';f={param($x)$x+@($x[-1])}},
    @{n='bad-schema';f={param($x)@($x|%{$_-replace 'v=1','v=2'})}},@{n='zero-cpu';f={param($x)@($x|%{$_-replace 'sample=4 host_us=200000 guest_frame=5 cpu_us=33000','sample=4 host_us=200000 guest_frame=5 cpu_us=0'})}},
    @{n='zero-gpu';f={param($x)@($x|%{$_-replace 'sample=5 host_us=240000 guest_frame=6 cpu_us=33000 gpu_us=1000','sample=5 host_us=240000 guest_frame=6 cpu_us=33000 gpu_us=0'})}},
    @{n='nonmonotonic-frame';f={param($x)@($x|%{$_-replace 'sample=6 host_us=280000 guest_frame=7','sample=6 host_us=280000 guest_frame=6'})}},
    @{n='summary-drift';f={param($x)$y=@($x);$y[-1]=$y[-1]-replace "stream_reads=$reads","stream_reads=999";$y}},@{n='overflow';f={param($x)@($x|%{$_-replace 'marker_overflow=0','marker_overflow=1'})}},
    @{n='shader-fail';f={param($x)@($x|%{$_-replace 'shader_count=2 shader_fail=0','shader_count=2 shader_fail=1'})}},@{n='post-summary';f={param($x)$x+'REX_PERF_AUDIT_SAMPLE v=1 bad'}},
    @{n='fatal';f={param($x)$x+'[FATAL] fixture'}},@{n='no-streaming';f={param($x)@($x|%{$_-replace 'stream_reads=3 stream_bytes=98304','stream_reads=0 stream_bytes=0' -replace "stream_reads=$reads stream_bytes=$bytes","stream_reads=0 stream_bytes=0"})}}
  )
  $failed=0;foreach($case in $cases){$path=Join-Path $root ($case.n+'.log');[IO.File]::WriteAllLines($path,(& $case.f $lines),[Text.UTF8Encoding]::new($false));try{&$verifier -LogPath $path|Out-Null;throw "Negative accepted: $($case.n)"}catch{if($_.Exception.Message -like 'Negative accepted:*'){throw};$failed++}}
  $sdk=Join-Path (Resolve-Path (Join-Path $PSScriptRoot '..')) 'third_party/rexglue-sdk'
  $contracts=@(
    @{p='src/core/perf/frame_audit.cpp';n='REXCVAR_DEFINE_BOOL(performance_telemetry_audit, false'},
    @{p='src/core/perf/frame_audit.cpp';n='.lifecycle(rex::cvar::Lifecycle::kInitOnly)'},
    @{p='src/core/perf/frame_audit.cpp';n='constexpr uint64_t kSampleLimit = 300'},
    @{p='src/core/perf/frame_audit.cpp';n='audit_enabled.store(false, std::memory_order_release)'},
    @{p='src/core/perf/frame_audit.cpp';n='return audit_enabled.load(std::memory_order_acquire);'},
    @{p='src/core/CMakeLists.txt';n='perf/frame_audit.cpp'},
    @{p='src/filesystem/devices/host_path_file.cpp';n='RecordStreamingRead'},
    @{p='src/audio/sdl/sdl_audio_driver.cpp';n='RecordAudioUnderrun'},
    @{p='src/graphics/d3d12/pipeline_cache.cpp';n='RecordShaderTranslation'},
    @{p='src/graphics/d3d12/pipeline_cache.cpp';n='RecordPipelineCreation'},
    @{p='src/graphics/d3d12/command_processor.cpp';n='D3D12_QUERY_HEAP_TYPE_TIMESTAMP'},
    @{p='src/graphics/d3d12/command_processor.cpp';n='ResolveQueryData'},
    @{p='src/graphics/d3d12/command_processor.cpp';n='RecordCpuFrame'},
    @{p='src/graphics/d3d12/command_processor.cpp';n='RecordGpuFrame'},
    @{p='tests/unit/CMakeLists.txt';n='core/frame_audit_test.cpp'}
  );foreach($c in $contracts){$text=Get-Content (Join-Path $sdk $c.p) -Raw;if(-not$text.Contains($c.n)){throw "Source contract missing: $($c.p) :: $($c.n)"}}
  [pscustomobject]@{Passed=$true;PositiveFixtures=1;FailClosedNegatives=$failed;SourceContractChecks=$contracts.Count}
}finally{Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue}
