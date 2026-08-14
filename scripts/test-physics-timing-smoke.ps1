[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verifier=Join-Path $PSScriptRoot 'verify-physics-timing-smoke.ps1'
$diagnostic=Join-Path $repo 'private/evidence/M5-008/diagnostic-release-20260814-144839/runs/01'
if(-not(Test-Path -LiteralPath $diagnostic)){throw 'Pinned private M5-008 timer diagnostic is missing.'}
$testRoot=Join-Path $repo ('private/evidence/M5-008/test-'+[guid]::NewGuid().ToString('N'))
$utf8=[Text.UTF8Encoding]::new($false)
$positive=0;$negative=0;$sourceChecks=0

function New-Probe([string]$Name){$root=Join-Path $testRoot $Name;[IO.Directory]::CreateDirectory((Join-Path $root 'runs'))|Out-Null;1..3|ForEach-Object{Copy-Item -LiteralPath $diagnostic -Destination (Join-Path (Join-Path $root 'runs') ('{0:D2}'-f$_)) -Recurse};$root}
function Log-Path([string]$Root,[int]$Cycle=1){Join-Path $Root ('runs/{0:D2}/mcla.log'-f$Cycle)}
function Rewrite([string]$Path,[string]$Old,[string]$New){$text=[IO.File]::ReadAllText($Path);if(-not$text.Contains($Old)){throw "Fixture needle missing: $Old"};[IO.File]::WriteAllText($Path,$text.Replace($Old,$New),$utf8)}
function Expect-Failure([string]$Name,[scriptblock]$Mutation){$root=New-Probe $Name;&$Mutation $root;$failed=$false;try{&$verifier -RunPath $root -Fixture|Out-Null}catch{$failed=$true};if(-not$failed){throw "Negative fixture '$Name' was accepted."};$script:negative++}
function Assert-Source([bool]$Condition,[string]$Description){if(-not$Condition){throw "Source contract failed: $Description"};$script:sourceChecks++}

try{
  $root=New-Probe 'positive';$probe=&$verifier -RunPath $root -Fixture;if($probe.cycle_count-ne3-or$probe.decision-cne'stock-30-fixed-step-and-real-time-throughput-pass'){throw 'Positive fixture returned wrong decision.'};$positive++
  Expect-Failure 'wrong-hook' {param($r)Rewrite (Log-Path $r) 'address=821BDA90' 'address=821BDA94'}
  Expect-Failure 'wrong-frequency' {param($r)Rewrite (Log-Path $r) 'guest_tick_frequency=50000000' 'guest_tick_frequency=49999999'}
  Expect-Failure 'wrong-fixed-record' {param($r)Rewrite (Log-Path $r) 'effective_bits=3D088889' 'effective_bits=3D088888'}
  Expect-Failure 'duplicate-record' {param($r)$p=Log-Path $r;$text=[IO.File]::ReadAllText($p);$line=($text-split"`r?`n"|Where-Object{$_-match'MCLA_PHYSICS_TIMER_RECORD v=1 id=0 '}|Select-Object -First 1);[IO.File]::WriteAllText($p,$text+"`r`n"+$line,$utf8)}
  Expect-Failure 'invalid-timer' {param($r)Rewrite (Log-Path $r) 'invalid_values=0' 'invalid_values=1'}
  Expect-Failure 'wrong-effective-summary' {param($r)Rewrite (Log-Path $r) 'effective_us_max=33333' 'effective_us_max=50000'}
  Expect-Failure 'raw-too-large' {param($r)Rewrite (Log-Path $r) 'raw_us_max=51744' 'raw_us_max=75001'}
  Expect-Failure 'guest-clock-drift' {param($r)Rewrite (Log-Path $r) 'guest_host_ratio_ppm=1000003' 'guest_host_ratio_ppm=990000'}
  Expect-Failure 'vblank-drift' {param($r)Rewrite (Log-Path $r) 'vblank_delta=600' 'vblank_delta=500'}
  Expect-Failure 'vblank-rate-drift' {param($r)Rewrite (Log-Path $r) 'vblank_millihz=60000' 'vblank_millihz=50000'}
  Expect-Failure 'present-too-slow' {param($r)Rewrite (Log-Path $r) 'present_delta=299' 'present_delta=200'}
  Expect-Failure 'present-rate-too-slow' {param($r)Rewrite (Log-Path $r) 'present_millihz=29900' 'present_millihz=20000'}
  Expect-Failure 'present-ratio-wrong' {param($r)Rewrite (Log-Path $r) 'present_to_vblank_ppm=498333' 'present_to_vblank_ppm=330000'}
  Expect-Failure 'slow-simulation' {param($r)Rewrite (Log-Path $r) 'simulated_time_to_wall_ppm=999990' 'simulated_time_to_wall_ppm=666660'}
  Expect-Failure 'false-pass-summary' {param($r)Rewrite (Log-Path $r) 'status=COMPLETE samples=1' 'status=PASS samples=1'}
  Expect-Failure 'missing-close' {param($r)Rewrite (Log-Path $r) 'Window closing, shutting down...' 'Window close omitted'}
  Expect-Failure 'missing-hard-exit' {param($r)Rewrite (Log-Path $r) 'Title terminated; hard-exiting process.' 'Hard exit omitted'}
  Expect-Failure 'static-vehicle-frame' {param($r)Copy-Item -LiteralPath (Join-Path $r 'runs/01/user/mcla-physics-start.bmp') -Destination (Join-Path $r 'runs/01/user/mcla-physics-end.bmp') -Force}
  Expect-Failure 'mutated-cycle-save' {param($r)Add-Content -LiteralPath (Join-Path $r 'runs/01/user/B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav') -Value 'drift' -NoNewline}
  Expect-Failure 'malformed-rotation' {param($r)Move-Item -LiteralPath (Log-Path $r) -Destination (Join-Path $r 'runs/01/mcla.bad.log')}

  $app=[IO.File]::ReadAllText((Join-Path $repo 'src/mcla_app.cpp'));$presenterH=[IO.File]::ReadAllText((Join-Path $repo 'third_party/rexglue-sdk/include/rex/ui/presenter.h'));$presenterCpp=[IO.File]::ReadAllText((Join-Path $repo 'third_party/rexglue-sdk/src/ui/presenter.cpp'));$graphics=[IO.File]::ReadAllText((Join-Path $repo 'third_party/rexglue-sdk/src/graphics/graphics_system.cpp'));$runner=[IO.File]::ReadAllText((Join-Path $repo 'scripts/run-physics-timing-smoke.ps1'))
  Assert-Source ($app.Contains('mcla_physics_timing_probe, false')) 'timing probe defaults off'
  Assert-Source ($app.Contains('.lifecycle(rex::cvar::Lifecycle::kInitOnly)')) 'probe is InitOnly'
  Assert-Source ($app.Contains('kPhysicsTimerAddress = 0x821BDA90')) 'stock timer address is exact'
  Assert-Source ($app.Contains('dispatcher->SetFunction(kPhysicsTimerAddress, PhysicsTimerProbe)')) 'dispatcher installs bounded wrapper'
  Assert-Source ($app.Contains('physics_timer_original(ctx, base);')) 'wrapper calls original timer'
  Assert-Source ($app.Contains('effective_us_min')-and$app.Contains('raw_us_max')) 'timer summary is bounded'
  Assert-Source ($presenterH.Contains('GetGuestOutputSequence()')-and$presenterCpp.Contains('Presenter::GetGuestOutputSequence()')) 'output sequence getter is concrete'
  Assert-Source ($presenterH.Contains('NotifyGuestVblank()')-and$presenterH.Contains('GetGuestVblankSequence()')) 'vblank diagnostic is nonvirtual presenter state'
  Assert-Source ($graphics.Contains('presenter_->NotifyGuestVblank();')) 'vblank producer updates presenter diagnostic'
  Assert-Source (-not$presenterH.Contains('virtual uint64_t GetGuestOutputSequence')) 'diagnostic getter does not change vtable'
  Assert-Source ($runner.Contains("--log_level=info")) 'canonical gate excludes trace overhead'
  Assert-Source ($runner.Contains("v0.9.0.19")) 'runner pins exact SDK release'
  Assert-Source ($runner.Contains("BuildRoot='out/build/win-amd64-release'")) 'canonical timing gate uses optimized Release build'
  Assert-Source ($runner.Contains("'rexruntime.dll','TracyClient.dll','rexgpu-xenos.dll'")) 'runtime manifest uses Release artifacts'
  Assert-Source ($runner.Contains("'--async_shader_compilation=false'")-and$runner.Contains("'--d3d12_pipeline_creation_threads=0'")) 'canonical timing gate excludes asynchronous pipeline noise'
  Assert-Source ($runner.Contains('actual_output_30fps_sustained=$true')) 'result records rendered 30 FPS only after physical proof'
  Assert-Source ($runner.Contains('stock_speed_sustained=$true')) 'result records real-time stock speed only after physical proof'
  Assert-Source ([IO.File]::ReadAllText($verifier).Contains('Cycle $Cycle save identity mismatch.')) 'each evidence cycle preserves the pinned save identity'
  [pscustomobject][ordered]@{Status='PASS';PhysicalPositives=$positive;FailClosedNegatives=$negative;SourceContractChecks=$sourceChecks;Decision='stock-30-fixed-step-and-real-time-throughput-pass'}
}finally{
  $full=[IO.Path]::GetFullPath($testRoot);$allowed=(Join-Path $repo 'private/evidence/M5-008').TrimEnd('\')+'\';if((Test-Path -LiteralPath $full)-and$full.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){Remove-Item -LiteralPath $full -Recurse -Force}
}
