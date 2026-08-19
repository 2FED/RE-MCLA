[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$toolchain=& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$cmake=$toolchain.CMakePath
$sdk=Join-Path $repoRoot 'third_party/rexglue-sdk'
$runId=(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)
$runRoot=Join-Path $repoRoot "private/evidence/M6-012/$runId"
$user=Join-Path $runRoot 'user'; $cache=Join-Path $runRoot 'cache'
[IO.Directory]::CreateDirectory($user)|Out-Null; [IO.Directory]::CreateDirectory($cache)|Out-Null
$sdkLog=Join-Path $runRoot 'sdk-install.log'; $testLog=Join-Path $runRoot 'focused-tests.log'; $appLog=Join-Path $runRoot 'app-clean-build.log'
$sdkCommit='6354bbe2150c7ce06bee5ffe399f17a94c948616'
$sdkVersion='0.9.0.28'
if ((& git -C $sdk rev-parse HEAD).Trim() -ne $sdkCommit -or (& git -C $sdk describe --tags --exact-match HEAD).Trim() -ne "v$sdkVersion") { throw 'The checked-out ReXGlue SDK does not match the canonical M6-012 pin.' }

Write-Host 'M6-012 [1/5]: clean-building/installing ReXGlue SDK...'
Push-Location $sdk
try { & $cmake --preset win-amd64 *>&1|Tee-Object $sdkLog|Out-Null; if($LASTEXITCODE-ne0){throw 'SDK configure failed.'}; & $cmake --build --preset win-amd64-relwithdebinfo --target install --clean-first --parallel 8 *>&1|Tee-Object $sdkLog -Append|Out-Null; if($LASTEXITCODE-ne0){throw 'SDK install failed.'} } finally { Pop-Location }
Write-Host 'M6-012 [2/5]: running focused telemetry tests...'
Push-Location $sdk
try { & $cmake --build --preset win-amd64-relwithdebinfo --target unit_tests --parallel 8 *>&1|Tee-Object $testLog|Out-Null; if($LASTEXITCODE-ne0){throw 'Focused test build failed.'}; & .\out\win-amd64\RelWithDebInfo\unit_tests.exe '[perf][frame-audit]' *>&1|Tee-Object $testLog -Append|Out-Null; if($LASTEXITCODE-ne0){throw 'Focused tests failed.'} } finally { Pop-Location }
$testText=Get-Content $testLog -Raw; if($testText -notmatch 'All tests passed \(7 assertions in 2 test cases\)'){throw 'Focused test totals drifted.'}
Write-Host 'M6-012 [3/5]: clean-building the MCLA host...'
Push-Location $repoRoot
try { & $cmake --preset win-amd64-relwithdebinfo *>&1|Tee-Object $appLog|Out-Null; if($LASTEXITCODE-ne0){throw 'App configure failed.'}; & $cmake --build --preset win-amd64-relwithdebinfo --target mcla --clean-first --parallel 8 *>&1|Tee-Object $appLog -Append|Out-Null; if($LASTEXITCODE-ne0){throw 'App build failed.'} } finally { Pop-Location }

Write-Host 'M6-012 [4/5]: collecting 300 correlated guest-frame samples (no operator input)...'
$build=Join-Path $repoRoot 'out/build/win-amd64-relwithdebinfo'; $exe=Join-Path $build 'mcla.exe'; $log=Join-Path $runRoot 'mcla.log'; $game=Join-Path $repoRoot 'private/game'
$args=@('--performance_telemetry_audit=true','--async_shader_compilation=false','--d3d12_pipeline_creation_threads=0','--render_target_path_d3d12=rtv','--log_max_file_size_mb=8','--log_max_files=10','--log_level=info','--fullscreen=false',"--game_data_root=`"$game`"","--user_data_root=`"$user`"","--cache_root=`"$cache`"","--log_file=`"$log`"")
$process=$null
function Read-Shared([string]$Path){
    if(-not(Test-Path -LiteralPath $Path)){return ''}
    $stream=[IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
    try{$reader=[IO.StreamReader]::new($stream);try{return $reader.ReadToEnd()}finally{$reader.Dispose()}}finally{$stream.Dispose()}
}
try {
    $process=Start-Process $exe -ArgumentList $args -WorkingDirectory $build -PassThru
    $deadline=(Get-Date).AddSeconds(90);$complete=$false
    while((Get-Date)-lt$deadline -and -not $process.HasExited){Start-Sleep -Milliseconds 250;$text=Read-Shared $log;if($text -match 'REX_PERF_AUDIT_SUMMARY v=1 status=PASS samples=300'){$complete=$true;break}}
    if(-not $complete){throw "Telemetry summary deadline missed. Private run: '$runRoot'."}
    Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public static class M6Close{[DllImport("user32.dll")]public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);}'
    $process.Refresh(); if($process.MainWindowHandle-eq0){throw 'MCLA window was not found for controlled close.'}; [M6Close]::PostMessage($process.MainWindowHandle,0x10,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null
    if(-not $process.WaitForExit(10000)){throw 'Controlled close timed out.'}; if($process.ExitCode-ne0){throw "MCLA exited with $($process.ExitCode)."}
} finally {
    if($null-ne$process -and -not$process.HasExited){$process.Kill($true);$process.WaitForExit()}
}
$probe=& (Join-Path $PSScriptRoot 'verify-performance-telemetry-smoke.ps1') -LogPath $log
$result=[ordered]@{schema=1;task='M6-012';decision='timestamped-performance-telemetry-pass';sdk_version=$sdkVersion;sdk_commit=$sdkCommit;focused_test_cases=2;focused_test_assertions=7;runtime_log='mcla.log';runtime_manifest=$probe.Manifest;samples=$probe.Samples;first_guest_frame=$probe.FirstGuestFrame;last_guest_frame=$probe.LastGuestFrame;cpu_p50_us=$probe.CpuP50Us;cpu_p95_us=$probe.CpuP95Us;cpu_max_us=$probe.CpuMaxUs;gpu_p50_us=$probe.GpuP50Us;gpu_p95_us=$probe.GpuP95Us;gpu_max_us=$probe.GpuMaxUs;stream_reads=$probe.StreamReads;stream_bytes=$probe.StreamBytes;stream_stalls=$probe.StreamStalls;audio_underruns=$probe.AudioUnderruns;shader_count=$probe.ShaderCount;pso_count=$probe.PsoCount;controlled_exit_verified=$true;executable_sha256=(Get-FileHash $exe -Algorithm SHA256).Hash;sdk_install_log_sha256=(Get-FileHash $sdkLog -Algorithm SHA256).Hash;focused_test_log_sha256=(Get-FileHash $testLog -Algorithm SHA256).Hash;app_clean_build_log_sha256=(Get-FileHash $appLog -Algorithm SHA256).Hash}
$resultPath=Join-Path $runRoot 'result.json';[IO.File]::WriteAllText($resultPath,(($result|ConvertTo-Json -Depth 5)+[Environment]::NewLine),[Text.UTF8Encoding]::new($false))
Write-Host 'M6-012 [5/5]: revalidating persisted physical evidence...'
& (Join-Path $PSScriptRoot 'verify-performance-telemetry-smoke.ps1') -ResultPath $resultPath
