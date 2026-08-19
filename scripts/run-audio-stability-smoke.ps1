[CmdletBinding()]
param(
  [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
  [string]$GameRoot = 'private/game',
  [switch]$LongSoakOnly,
  [string]$ResumeRun,
  [switch]$ResourceSelfTest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdk = Join-Path $repo 'third_party/rexglue-sdk'
$verify = Join-Path $PSScriptRoot 'verify-audio-stability-smoke.ps1'
$gameVerifier = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$cmake = $toolchain.CMakePath
$utf8 = [Text.UTF8Encoding]::new($false)
$sdkVersion = '0.9.0.25'
$sdkCommit = 'f28ddabbae3bca56ddf5ffea067982c49c9549b7'
$priorEvidence = @(
  [ordered]@{ task = 'M4-007'; run_id = '20260813-170202-44d2c7d8'; sha256 = 'A55CD1CAED7063CC811BB5F45EAD52B6DB971F8A80BF94C61F534B6BCA9F0A7A' },
  [ordered]@{ task = 'M4-008'; run_id = '20260813-182745-5b65003b'; sha256 = '578B0F7CA1E531A9F56E172A9625E37D549E69FDD23D7D5D77BBF0C33B85A1EB' },
  [ordered]@{ task = 'M5-009'; run_id = '20260814-170657-f44949d7'; sha256 = '4E3D514386501D92B43CD4F2C4C89ECD8BA000ACF23D8B168CFA431C8F67C62F' },
  [ordered]@{ task = 'M5-013'; run_id = '20260817-015958-36eec226'; sha256 = 'D890A903775A3B53262B9957E7B7DC1D4B76B49B8735360BECD8233842446298' }
)

if (-not ('MclaAudioStabilityWindow' -as [type])) {
  Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;
public static class MclaAudioStabilityWindow{delegate bool E(IntPtr h,IntPtr p);[DllImport("user32.dll")]static extern bool EnumWindows(E c,IntPtr p);[DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);[DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr h);[DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetWindowText(IntPtr h,StringBuilder s,int n);[DllImport("user32.dll")]static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);public static IntPtr[] Handles(int pid){var a=new List<IntPtr>();EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}public static string Title(IntPtr h){var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty;}public static bool Close(IntPtr h){return PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);}}
'@
}

function Resolve-Safe([string]$Path, [string]$Description, [switch]$Exists) {
  $full = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repo $Path }))
  $prefix = $repo.TrimEnd('\') + '\'
  if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes the repository." }
  $current = $repo
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object Length)) {
    $current = Join-Path $current $part
    if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "$Description traverses a reparse point." }
  }
  if ($Exists -and -not (Test-Path -LiteralPath $full)) { throw "$Description is missing." }
  $full
}
function Hash([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }
function Game-Record($Probe) {
  [ordered]@{
    valid = [bool]$Probe.Valid; file_count = [int]$Probe.FileCount; payload_bytes = [long]$Probe.PayloadBytes
    rpf_count = [int]$Probe.RpfCount; rpf_bytes = [long]$Probe.RpfBytes; bik_count = [int]$Probe.BikCount
    bik_bytes = [long]$Probe.BikBytes; hashes_verified = [int]$Probe.HashesVerified; source_iso_sha256 = [string]$Probe.SourceIsoSha256
  }
}
function Tree([string]$Root) {
  $items = @(Get-ChildItem -LiteralPath $Root -Recurse -Force)
  $entries = @(); $bytes = 0L
  foreach ($directory in @($items | Where-Object PSIsContainer | Sort-Object FullName)) { if ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Evidence tree contains a reparse point.' }; $entries += [ordered]@{ kind = 'directory'; path = $directory.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/') } }
  foreach ($file in @($items | Where-Object { -not $_.PSIsContainer -and $_.Name -cne 'result.json' } | Sort-Object FullName)) { if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Evidence tree contains a reparse point.' }; $bytes += $file.Length; $entries += [ordered]@{ kind = 'file'; path = $file.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/'); bytes = [long]$file.Length; sha256 = Hash $file.FullName } }
  $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4; $sha = [Security.Cryptography.SHA256]::Create(); try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
  [pscustomobject]@{ Hash = $hash; FileCount = @($entries | Where-Object kind -CEQ 'file').Count; DirectoryCount = @($entries | Where-Object kind -CEQ 'directory').Count; Bytes = $bytes }
}
function Invoke-Logged([scriptblock]$Command, [string]$Log, [switch]$Append) { $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'; try { if ($Append) { & $Command *>&1 | Tee-Object -FilePath $Log -Append | Out-Null } else { & $Command *>&1 | Tee-Object -FilePath $Log | Out-Null }; $LASTEXITCODE } finally { $ErrorActionPreference = $old } }
function Read-LiveLogs([string]$Directory) { $text = ''; foreach ($file in @(Get-ChildItem -LiteralPath $Directory -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue)) { try { $stream = [IO.File]::Open($file.FullName, 'Open', 'Read', 'ReadWrite'); try { $reader = [IO.StreamReader]::new($stream); $text += $reader.ReadToEnd(); $reader.Dispose() } finally { $stream.Dispose() } } catch {} }; $text }
function Median([long[]]$Values) { $sorted = @($Values | Sort-Object); [long]$sorted[[Math]::Floor($sorted.Count / 2)] }
function Resource-Sample([Diagnostics.Process]$Process, [int]$Checkpoint, [long]$ElapsedSeconds) {
  $observations = @()
  for ($attempt = 0; $attempt -lt 3; ++$attempt) {
    if ($Process.HasExited) { throw 'Process exited during two-hour resource sampling.' }
    $Process.Refresh()
    $observations += [pscustomobject]@{ private = [long]$Process.PrivateMemorySize64; working = [long]$Process.WorkingSet64; handles = [long]$Process.HandleCount; threads = [long]$Process.Threads.Count }
    if ($attempt -lt 2) { Start-Sleep -Milliseconds 250 }
  }
  [ordered]@{ checkpoint = $Checkpoint; elapsed_seconds = $ElapsedSeconds; private_bytes = Median @($observations.private); working_set_bytes = Median @($observations.working); handle_count = Median @($observations.handles); thread_count = Median @($observations.threads) }
}
function Write-Samples([string]$Path, [object[]]$Samples) { [IO.File]::WriteAllText($Path, ((ConvertTo-Json -InputObject @($Samples) -Depth 4) + [Environment]::NewLine), $utf8) }
function Close-Exact([Diagnostics.Process]$Process) { $matches = @(); foreach ($handle in [MclaAudioStabilityWindow]::Handles($Process.Id)) { if ([regex]::IsMatch([MclaAudioStabilityWindow]::Title($handle), '^mcla \[rexglue-v[^\]]+\]$')) { $matches += $handle } }; if ($matches.Count -ne 1 -or -not [MclaAudioStabilityWindow]::Close($matches[0])) { throw 'Exact PID/window WM_CLOSE failed.' } }
function Exact-Processes([string]$Executable) { @((Get-Process mcla -ErrorAction SilentlyContinue) | Where-Object { try { [string]::Equals($_.Path, $Executable, [StringComparison]::OrdinalIgnoreCase) } catch { $false } }) }
function Artifacts([string]$Build) { @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll') | ForEach-Object { $path = Resolve-Safe (Join-Path $Build $_) "Artifact $_" -Exists; [ordered]@{ name = $_; bytes = [long](Get-Item -LiteralPath $path).Length; sha256 = Hash $path } } }
function Run-ProcessStage([string]$Executable, [string]$Build, [string]$Cycle, [string[]]$Arguments, [scriptblock]$WaitForPass, [int]$DeadlineSeconds, [string]$FailureLabel) {
  $process = $null; $forced = $false
  try {
    $process = Start-Process $Executable -ArgumentList $Arguments -WorkingDirectory $Build -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds($DeadlineSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
      if ($process.HasExited) { throw "Process exited during $FailureLabel with $($process.ExitCode)." }
      if (& $WaitForPass (Read-LiveLogs $Cycle) $process) { break }
      Start-Sleep -Milliseconds 500
    }
    if (-not (& $WaitForPass (Read-LiveLogs $Cycle) $process)) { throw "$FailureLabel deadline expired." }
    Close-Exact $process
    if (-not $process.WaitForExit(10000) -or $process.ExitCode -ne 0) { throw "$FailureLabel controlled external close failed." }
    if (@(Exact-Processes $Executable).Count) { throw "$FailureLabel process survived close." }
  } catch {
    $failure = $_
    if ($null -ne $process -and -not $process.HasExited) { $forced = $true; Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue; $null = $process.WaitForExit(5000) }
    if ($forced) { throw "$FailureLabel required force cleanup. $($failure.Exception.Message)" }
    throw
  }
}

if ($ResourceSelfTest) {
  $sample = Resource-Sample ([Diagnostics.Process]::GetCurrentProcess()) 0 0
  if ($sample.checkpoint -ne 0 -or $sample.elapsed_seconds -ne 0 -or $sample.private_bytes -le 0 -or $sample.working_set_bytes -le 0 -or $sample.handle_count -le 0 -or $sample.thread_count -le 0) { throw 'Resource sampler self-test failed.' }
  return [pscustomobject]@{ ProcessResourceSamplingVerified = $true; MedianObservations = 3; GpuCounterDependency = $false }
}

$build = Resolve-Safe $BuildRoot 'Build root' -Exists
$game = Resolve-Safe $GameRoot 'Game root' -Exists
if ($build -cne (Resolve-Safe 'out/build/win-amd64-relwithdebinfo' 'Canonical build' -Exists) -or $game -cne (Resolve-Safe 'private/game' 'Canonical game' -Exists)) { throw 'M6-007 requires canonical build and game roots.' }
foreach ($prior in $priorEvidence) { $path = Resolve-Safe ("private/evidence/$($prior.task)/$($prior.run_id)/result.json") "Prior $($prior.task) result" -Exists; if ((Hash $path) -cne $prior.sha256) { throw "Prior $($prior.task) result drifted." } }
if ((& git -C $sdk rev-parse HEAD).Trim() -cne $sdkCommit -or (& git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim() -cne "v$sdkVersion" -or (git -C $sdk status --porcelain)) { throw "M6-007 requires clean exact ReXGlue v$sdkVersion." }
if ($ResumeRun) {
  if ($ResumeRun -notmatch '^20[0-9]{6}-[0-9]{6}-[0-9a-f]{8}$') { throw 'ResumeRun format is invalid.' }
  $runRoot = Resolve-Safe ("private/evidence/M6-007/" + $ResumeRun) 'M6-007 resume root' -Exists
  if ((Split-Path $runRoot -Parent) -cne (Resolve-Safe 'private/evidence/M6-007' 'M6-007 evidence root' -Exists)) { throw 'ResumeRun must be one direct evidence child.' }
  $null = & $verify -LongSoakPath (Join-Path $runRoot 'long-soak')
} else {
  $evidence = Resolve-Safe 'private/evidence/M6-007' 'M6-007 evidence root'
  [IO.Directory]::CreateDirectory($evidence) | Out-Null
  $runRoot = Join-Path $evidence ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
  [IO.Directory]::CreateDirectory($runRoot) | Out-Null
}
$buildEvidence = Join-Path $runRoot 'build'
[IO.Directory]::CreateDirectory($buildEvidence) | Out-Null
$sdkLog = Join-Path $buildEvidence 'sdk-install.log'
$testLog = Join-Path $buildEvidence 'sdk-audio-test.log'
$appLog = Join-Path $buildEvidence 'app-clean-build.log'

Write-Host 'M6-007 [1/6]: validating prior audio routes, source game, and exact SDK identity...' -ForegroundColor Cyan
$gameBefore = Game-Record (& $gameVerifier -GamePath $game -VerifyHashes)
Write-Host 'M6-007 [2/6]: clean-building SDK, focused audio tests, and the host...' -ForegroundColor Cyan
Push-Location $sdk
try {
  if ((Invoke-Logged { & $cmake --preset win-amd64 -DREXGLUE_BUILD_TESTS=ON } $sdkLog) -ne 0 -or (Invoke-Logged { & $cmake --build out/build/win-amd64 --config RelWithDebInfo --target install unit_tests --clean-first --parallel 8 } $sdkLog -Append) -ne 0) { throw "SDK build/install failed. Private run: '$runRoot'." }
} finally { Pop-Location }
$unit = Resolve-Safe 'third_party/rexglue-sdk/out/win-amd64/RelWithDebInfo/unit_tests.exe' 'Audio unit executable' -Exists
if ((Invoke-Logged { & $unit '[audio]' } $testLog) -ne 0 -or [IO.File]::ReadAllText($testLog) -notmatch 'All tests passed \(42 assertions in 10 test cases\)') { throw "Focused audio tests failed or totals changed. Private run: '$runRoot'." }
if ((Invoke-Logged { & $cmake --preset win-amd64-relwithdebinfo } $appLog) -ne 0 -or (Invoke-Logged { & $cmake --build --preset win-amd64-relwithdebinfo --target mcla --clean-first --parallel 8 } $appLog -Append) -ne 0) { throw "Host clean build failed. Private run: '$runRoot'." }
$exe = Resolve-Safe (Join-Path $build 'mcla.exe') 'MCLA executable' -Exists
if (@(Exact-Processes $exe).Count) { throw 'Canonical MCLA is already running.' }
$artifacts = @(Artifacts $build)

if (-not $ResumeRun) {
  Write-Host 'M6-007 [3/6]: starting the autonomous two-hour title-audio soak...' -ForegroundColor Cyan
  $soakRoot = Join-Path $runRoot 'long-soak'; $soakCycle = Join-Path $soakRoot 'runs\soak'; $soakUser = Join-Path $soakCycle 'user'; $soakCache = Join-Path $soakCycle 'cache'
  [IO.Directory]::CreateDirectory($soakUser) | Out-Null; [IO.Directory]::CreateDirectory($soakCache) | Out-Null
  $soakLog = Join-Path $soakCycle 'mcla.log'
  $samplePath = Join-Path $soakRoot 'resource-samples.json'
  $arguments = @('--mcla_first_frame_probe=true', '--sdl_audio_route_audit=true', '--mcla_audio_route_soak_seconds=7200', '--mcla_first_frame_settle_seconds=35', '--async_shader_compilation=false', '--d3d12_pipeline_creation_threads=0', '--log_level=info', '--log_max_file_size_mb=8', '--log_max_files=15', '--fullscreen=false', "--game_data_root=`"$game`"", "--user_data_root=`"$soakUser`"", "--cache_root=`"$soakCache`"", "--log_file=`"$soakLog`"")
  $timer = [Diagnostics.Stopwatch]::StartNew()
  $resource = [pscustomobject]@{ Timer = $null; Samples = @(); NextCheckpoint = 0 }
  Run-ProcessStage $exe $build $soakCycle $arguments {
    param($text, $process)
    $started = $text.Contains('MCLA audio: title soak started seconds 7200')
    $completed = $text.Contains('MCLA audio: title soak completed seconds 7200') -and $text.Contains('SDL_AUDIO_AUDIT_SUMMARY v=1 phase=title status=PASS')
    if ($started -and $null -eq $resource.Timer) { $resource.Timer = [Diagnostics.Stopwatch]::StartNew() }
    if ($null -ne $resource.Timer) {
      while ($resource.NextCheckpoint -le 12 -and ($resource.Timer.Elapsed.TotalSeconds -ge ($resource.NextCheckpoint * 600) -or ($completed -and $resource.NextCheckpoint -eq 12))) {
        $elapsed = if ($resource.NextCheckpoint -eq 12) { 7200L } else { [long][Math]::Floor($resource.Timer.Elapsed.TotalSeconds) }
        $resource.Samples += Resource-Sample $process $resource.NextCheckpoint $elapsed
        Write-Samples $samplePath @($resource.Samples)
        if ($resource.NextCheckpoint -gt 0) { Write-Host ("Two-hour audio soak: {0}/7200 seconds; resource checkpoint PASS." -f ($resource.NextCheckpoint * 600)) -ForegroundColor DarkCyan }
        ++$resource.NextCheckpoint
      }
    }
    $completed
  } 7500 'Two-hour audio soak'
  $timer.Stop()
  if (@($resource.Samples).Count -ne 13) { throw 'Two-hour soak did not produce baseline plus twelve resource samples.' }
  $soakProbe = & $verify -LongSoakPath $soakRoot
  [IO.File]::WriteAllText((Join-Path $soakRoot 'stage.json'), ((ConvertTo-Json ([ordered]@{ schema = 'mcla-audio-long-soak-v1'; run_id = Split-Path $runRoot -Leaf; elapsed_milliseconds = [long]$timer.ElapsedMilliseconds; log_set_sha256 = $soakProbe.LogSet.Hash; capture_sha256 = $soakProbe.CaptureSha256; route = $soakProbe.Route; resources = $soakProbe.Resources; controlled_exit = $true }) -Depth 8) + [Environment]::NewLine), $utf8)
  if ($LongSoakOnly) { Write-Host "M6-007 long soak PASS. Resume later with -ResumeRun '$(Split-Path $runRoot -Leaf)'." -ForegroundColor Green; return }
}

Write-Host 'M6-007 [4/6]: launching the short pause/resume and playback-device recovery stage...' -ForegroundColor Cyan
$switchRoot = Join-Path $runRoot 'device-switch'; $switchCycle = Join-Path $switchRoot 'runs\stability'; $switchUser = Join-Path $switchCycle 'user'; $switchCache = Join-Path $switchCycle 'cache'
[IO.Directory]::CreateDirectory($switchUser) | Out-Null; [IO.Directory]::CreateDirectory($switchCache) | Out-Null
$switchLog = Join-Path $switchCycle 'mcla.log'; $request = Join-Path $switchUser '.mcla-audio-device-confirm.request'
$arguments = @('--mcla_audio_stability_probe=true', '--sdl_audio_stability_audit=true', '--sdl_audio_route_audit=true', '--xmp_route_audit=true', '--mcla_first_frame_settle_seconds=35', '--async_shader_compilation=false', '--d3d12_pipeline_creation_threads=0', '--log_level=info', '--log_max_file_size_mb=8', '--log_max_files=15', '--fullscreen=false', "--game_data_root=`"$game`"", "--user_data_root=`"$switchUser`"", "--cache_root=`"$switchCache`"", "--log_file=`"$switchLog`"")
$process = $null; $forced = $false
try {
  $process = Start-Process $exe -ArgumentList $arguments -WorkingDirectory $build -PassThru
  $readyDeadline = [DateTime]::UtcNow.AddSeconds(120)
  while ([DateTime]::UtcNow -lt $readyDeadline) { if ($process.HasExited) { throw "Process exited before device-switch READY with $($process.ExitCode)." }; if ((Read-LiveLogs $switchCycle).Contains('MCLA_AUDIO_STABILITY_READY v=1 phase=device-switch status=READY')) { break }; Start-Sleep -Milliseconds 250 }
  if (-not (Read-LiveLogs $switchCycle).Contains('MCLA_AUDIO_STABILITY_READY v=1 phase=device-switch status=READY')) { throw 'Pause/resume recovery did not reach device-switch READY.' }
  Write-Host 'AUDIO READY | Switch the Windows default playback device (or disconnect/reconnect the current output).' -ForegroundColor Yellow
  Write-Host 'AUDIO READY | Keep the game open; the script is waiting for a real SDL playback-device event and recovered nonzero output.' -ForegroundColor Yellow
  # Keep this operator window generous: the endpoint may be controlled from a
  # remote session, and a timeout must not invalidate the completed two-hour
  # stage merely because the owner was briefly away from the console.
  $deviceDeadline = [DateTime]::UtcNow.AddMinutes(15)
  while ([DateTime]::UtcNow -lt $deviceDeadline) { if ($process.HasExited) { throw "Process exited during playback-device switch with $($process.ExitCode)." }; $text = Read-LiveLogs $switchCycle; if ($text.Contains('SDL_AUDIO_STABILITY_DEVICE v=1 event=migrated') -and $text.Contains('SDL_AUDIO_STABILITY_RECOVERY v=1 source=device')) { break }; Start-Sleep -Milliseconds 250 }
  $text = Read-LiveLogs $switchCycle
  if (-not ($text.Contains('SDL_AUDIO_STABILITY_DEVICE v=1 event=migrated') -and $text.Contains('SDL_AUDIO_STABILITY_RECOVERY v=1 source=device'))) { throw 'No post-arm default-endpoint migration with recovered audio output was observed.' }
  Write-Host 'Machine recovery PASS. Confirm that title music is audible on the selected output.' -ForegroundColor Green
  Write-Host 'Type exactly: AUDIO DEVICE RECOVERED' -ForegroundColor Yellow
  $confirmation = Read-Host
  if ($confirmation -cne 'AUDIO DEVICE RECOVERED') { throw 'Owner audio recovery confirmation was not accepted.' }
  [IO.File]::WriteAllText($request, 'AUDIO DEVICE RECOVERED', $utf8)
  $summaryDeadline = [DateTime]::UtcNow.AddSeconds(20)
  while ([DateTime]::UtcNow -lt $summaryDeadline) { if ($process.HasExited) { throw "Process exited before stability summary with $($process.ExitCode)." }; $text = Read-LiveLogs $switchCycle; if ($text.Contains('SDL_AUDIO_STABILITY_SUMMARY v=1 phase=title status=PASS') -and $text.Contains('MCLA_AUDIO_STABILITY_SUMMARY v=1 status=COMPLETE')) { break }; Start-Sleep -Milliseconds 250 }
  if (-not (Read-LiveLogs $switchCycle).Contains('MCLA_AUDIO_STABILITY_SUMMARY v=1 status=COMPLETE')) { throw 'Audio-stability summary deadline expired.' }
  Close-Exact $process
  if (-not $process.WaitForExit(10000) -or $process.ExitCode -ne 0) { throw 'Controlled external WM_CLOSE failed.' }
  if (@(Exact-Processes $exe).Count) { throw 'MCLA process survived controlled close.' }
} catch {
  $failure = $_
  if ($null -ne $process -and -not $process.HasExited) { $forced = $true; Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue; $null = $process.WaitForExit(5000) }
  if ($forced) { throw "M6-007 failure required force cleanup. $($failure.Exception.Message) Private run: '$runRoot'." }
  throw
}

Write-Host 'M6-007 [5/6]: verifying both physical stages and immutable prior audio routes...' -ForegroundColor Cyan
$soakProbe = & $verify -LongSoakPath (Join-Path $runRoot 'long-soak')
$stabilityProbe = & $verify -RunPath $switchRoot
$gameAfter = Game-Record (& $gameVerifier -GamePath $game -VerifyHashes)
if (($gameBefore | ConvertTo-Json -Compress -Depth 5) -cne ($gameAfter | ConvertTo-Json -Compress -Depth 5)) { throw 'Source-game identity changed.' }
$artifactsAfter = @(Artifacts $build)
if (($artifacts | ConvertTo-Json -Compress -Depth 4) -cne ($artifactsAfter | ConvertTo-Json -Compress -Depth 4)) { throw 'Runtime artifacts changed during M6-007.' }
$record = [ordered]@{
  schema = 'mcla-audio-stability-v1'; task = 'M6-007'; decision = 'two-hour-audio-and-device-recovery-pass'; run_id = Split-Path $runRoot -Leaf
  sdk_version = $sdkVersion; sdk_commit = $sdkCommit; build_configuration = 'RelWithDebInfo'; prior_evidence = @($priorEvidence)
  long_soak = [ordered]@{ run_id = $soakProbe.RunId; log_set_sha256 = $soakProbe.LogSet.Hash; soak_seconds = 7200; controlled_exit = $true; route = $soakProbe.Route; resources = $soakProbe.Resources }
  device_switch = [ordered]@{ run_id = $stabilityProbe.RunId; log_set_sha256 = $stabilityProbe.LogSet.Hash; pause_cycles = 2; device_events = $stabilityProbe.DeviceEvents; device_recoveries = $stabilityProbe.DeviceRecoveries; operator_heard = $true; controlled_exit = $true }
  focused_tests = [ordered]@{ cases = 10; assertions = 42; sdk_log_sha256 = Hash $sdkLog; test_log_sha256 = Hash $testLog; app_log_sha256 = Hash $appLog; unit_executable_sha256 = Hash $unit }
  artifacts = @($artifactsAfter); game_before = $gameBefore; game_after = $gameAfter
  scope = [ordered]@{ two_hour_soak = $true; current_pause_resume = $true; current_default_device_recovery = $true; operator_heard_recovery = $true; prior_stream_transition_bound = $true; prior_xmp_metadata_fallback_bound = $true; monolithic_run_claimed = $false; device_identity_recorded = $false; audio_mix_fidelity_claimed = $false }
}
$evidenceTree = Tree $runRoot
$record.evidence_tree_sha256 = $evidenceTree.Hash; $record.evidence_tree_file_count = $evidenceTree.FileCount; $record.evidence_tree_directory_count = $evidenceTree.DirectoryCount; $record.evidence_tree_bytes = $evidenceTree.Bytes
$result = Join-Path $runRoot 'result.json'
[IO.File]::WriteAllText($result, ((ConvertTo-Json $record -Depth 12) + [Environment]::NewLine), $utf8)
Write-Host 'M6-007 [6/6]: revalidating persisted result and current physical hashes...' -ForegroundColor Cyan
$final = & $verify -ResultPath $result
Write-Host "M6-007 PASS: '$result'." -ForegroundColor Green
$final
