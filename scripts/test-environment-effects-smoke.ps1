[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify=Join-Path $PSScriptRoot 'verify-environment-effects-smoke.ps1'
$physical=Join-Path $repo 'private/evidence/M6-004/diagnostic-environment-probe-20260818-03'
$root=Join-Path $repo ('private/evidence/M6-004/test-'+[guid]::NewGuid().ToString('N').Substring(0,8))
$phaseFiles=[ordered]@{'dry-night-baseline'='mcla-environment-dry-night.bmp';'rain-dawn-options'='mcla-environment-options.bmp';'rain-dawn-stationary'='mcla-environment-stationary.bmp';'rain-dawn-moving'='mcla-environment-moving.bmp';'rain-dawn-stopped'='mcla-environment-stopped.bmp';'rain-dawn-particle'='mcla-environment-particle.bmp'}
$positives=0;$negatives=0;$sourceChecks=0

function Write-Fixture([string]$Path){
  [IO.Directory]::CreateDirectory((Join-Path $Path 'user'))|Out-Null;Add-Type -AssemblyName System.Drawing;$colors=@([Drawing.Color]::FromArgb(10,10,20),[Drawing.Color]::FromArgb(30,80,120),[Drawing.Color]::FromArgb(50,100,150),[Drawing.Color]::FromArgb(100,40,160),[Drawing.Color]::FromArgb(160,80,30),[Drawing.Color]::FromArgb(220,180,60));$i=0;foreach($file in $phaseFiles.Values){$bmp=[Drawing.Bitmap]::new(1280,720,[Drawing.Imaging.PixelFormat]::Format24bppRgb);$g=[Drawing.Graphics]::FromImage($bmp);try{$g.Clear($colors[$i]);$g.FillRectangle([Drawing.Brushes]::White,($i*97)%1000,100,180,300);$bmp.Save((Join-Path $Path "user\$file"),[Drawing.Imaging.ImageFormat]::Bmp)}finally{$g.Dispose();$bmp.Dispose()};$i++}
  $lines=[Collections.Generic.List[string]]::new();$lines.Add('MCLA_ENVIRONMENT_EFFECTS_CONFIG v=1 route=arcade-ordered-sunset-and-vine weather=rain time=dawn frames=6 external_close_required=1')
  $buttons=@('0010','0010','0200','1000','1000','1000','0002','0002','0002','0002','0008','0008','0008','0008','0002','0008','0008','1000');$addInput={param([int]$sequence);foreach($entry in @(@('source',$buttons[$sequence-1]),@('guest',$buttons[$sequence-1]),@('source','0000'),@('guest','0000'))){$lines.Add("MCLA_FRONTEND_SMOKE_INPUT v=1 side=$($entry[0]) sequence=$sequence buttons=$($entry[1])")}};$addFrame={param([string]$phase,[int]$present);$lines.Add("MCLA_ENVIRONMENT_EFFECTS_FRAME v=1 phase=$phase width=1280 height=720 present_seq=$present status=PASS")};$addRender={param([string]$side,[int]$sequence,[int]$lt,[int]$rt);$lines.Add("MCLA_RENDER_SMOKE_INPUT v=1 side=$side sequence=$sequence buttons=0000 lt=$lt rt=$rt lx=0 ly=0 rx=0 ry=0")}
  &$addInput 1;&$addFrame 'dry-night-baseline' 100;for($sequence=2;$sequence-le17;$sequence++){&$addInput $sequence};&$addFrame 'rain-dawn-options' 200;&$addInput 18;&$addFrame 'rain-dawn-stationary' 300;&$addRender 'source' 1 0 192;&$addRender 'guest' 1 0 192;&$addFrame 'rain-dawn-moving' 400;&$addRender 'source' 1 0 0;&$addRender 'guest' 1 0 0;&$addFrame 'rain-dawn-stopped' 500;&$addRender 'source' 2 255 255;&$addRender 'guest' 2 255 255;&$addFrame 'rain-dawn-particle' 600;&$addRender 'source' 2 0 0;&$addRender 'guest' 2 0 0
  $lines.Add('MCLA_ENVIRONMENT_EFFECTS_SUMMARY v=1 status=PASS frames=6 frontend_input_records=72 render_input_records=8 weather=rain time=dawn external_close_required=1');$lines.Add('Window closing, shutting down...');$lines.Add('Execution complete');$lines.Add('Title terminated; hard-exiting process.');[IO.File]::WriteAllLines((Join-Path $Path 'mcla.log'),$lines,[Text.UTF8Encoding]::new($false))
}
function Expect-Fail([string]$Name,[scriptblock]$Mutate){$case=Join-Path $root $Name;Copy-Item -LiteralPath (Join-Path $root 'base') -Destination $case -Recurse;&$Mutate $case;$failed=$false;try{$null=&$verify -ProbeOnly -RuntimeLogPath (Join-Path $case 'mcla.log') -UserRoot (Join-Path $case 'user')}catch{$failed=$true};if(-not$failed){throw "Negative fixture '$Name' was accepted."};$script:negatives++}

try{
  if(-not(Test-Path -LiteralPath (Join-Path $physical 'mcla.log'))){throw 'Pinned physical calibration probe is missing.'};$null=&$verify -ProbeOnly -RuntimeLogPath (Join-Path $physical 'mcla.log') -UserRoot (Join-Path $physical 'user');$positives++
  $base=Join-Path $root 'base';Write-Fixture $base;$null=&$verify -ProbeOnly -RuntimeLogPath (Join-Path $base 'mcla.log') -UserRoot (Join-Path $base 'user');$positives++
  $replace={param($case,$from,$to);$p=Join-Path $case 'mcla.log';[IO.File]::WriteAllText($p,([IO.File]::ReadAllText($p).Replace($from,$to)),[Text.UTF8Encoding]::new($false))}
  Expect-Fail 'missing-config' {param($c);&$replace $c 'MCLA_ENVIRONMENT_EFFECTS_CONFIG' 'REMOVED_CONFIG'}
  Expect-Fail 'wrong-route' {param($c);&$replace $c 'route=arcade-ordered-sunset-and-vine' 'route=free-roam'}
  Expect-Fail 'wrong-weather' {param($c);&$replace $c 'weather=rain time=dawn' 'weather=clear time=dawn'}
  Expect-Fail 'wrong-time' {param($c);&$replace $c 'weather=rain time=dawn' 'weather=rain time=midnight'}
  Expect-Fail 'duplicate-config' {param($c);$p=Join-Path $c 'mcla.log';$first=[IO.File]::ReadAllLines($p)[0];[IO.File]::AppendAllText($p,[Environment]::NewLine+$first,[Text.UTF8Encoding]::new($false))}
  Expect-Fail 'missing-frame' {param($c);&$replace $c 'MCLA_ENVIRONMENT_EFFECTS_FRAME v=1 phase=rain-dawn-particle' 'REMOVED_FRAME v=1 phase=rain-dawn-particle'}
  Expect-Fail 'wrong-frame-order' {param($c);&$replace $c 'phase=rain-dawn-moving' 'phase=rain-dawn-stopped'}
  Expect-Fail 'nonmonotonic-present' {param($c);&$replace $c 'present_seq=600' 'present_seq=100'}
  Expect-Fail 'wrong-dimensions' {param($c);&$replace $c 'width=1280 height=720 present_seq=300' 'width=1920 height=1080 present_seq=300'}
  Expect-Fail 'missing-frontend-record' {param($c);&$replace $c 'MCLA_FRONTEND_SMOKE_INPUT v=1 side=source sequence=18 buttons=1000' 'REMOVED_INPUT v=1 side=source sequence=18 buttons=1000'}
  Expect-Fail 'wrong-frontend-button' {param($c);&$replace $c 'side=source sequence=11 buttons=0008' 'side=source sequence=11 buttons=0004'}
  Expect-Fail 'wrong-frontend-side' {param($c);&$replace $c 'side=guest sequence=7 buttons=0002' 'side=source sequence=7 buttons=0002'}
  Expect-Fail 'wrong-frontend-sequence' {param($c);&$replace $c 'side=source sequence=15 buttons=0002' 'side=source sequence=14 buttons=0002'}
  Expect-Fail 'missing-render-record' {param($c);&$replace $c 'side=source sequence=2 buttons=0000 lt=255 rt=255' 'side=source sequence=2 buttons=0000 lt=254 rt=255'}
  Expect-Fail 'wrong-throttle' {param($c);&$replace $c 'side=guest sequence=1 buttons=0000 lt=0 rt=192' 'side=guest sequence=1 buttons=0000 lt=0 rt=191'}
  Expect-Fail 'wrong-particle-input' {param($c);&$replace $c 'side=guest sequence=2 buttons=0000 lt=255 rt=255' 'side=guest sequence=2 buttons=0000 lt=255 rt=0'}
  Expect-Fail 'missing-summary' {param($c);&$replace $c 'MCLA_ENVIRONMENT_EFFECTS_SUMMARY' 'REMOVED_SUMMARY'}
  Expect-Fail 'wrong-summary-count' {param($c);&$replace $c 'frontend_input_records=72' 'frontend_input_records=68'}
  Expect-Fail 'missing-close' {param($c);&$replace $c 'Window closing, shutting down...' 'NO CLOSE'}
  Expect-Fail 'missing-complete' {param($c);&$replace $c 'Execution complete' 'NO COMPLETE'}
  Expect-Fail 'missing-hard-exit' {param($c);&$replace $c 'Title terminated; hard-exiting process.' 'NO HARD EXIT'}
  Expect-Fail 'fatal-marker' {param($c);Add-Content (Join-Path $c 'mcla.log') '[fatal] device failure'}
  Expect-Fail 'missing-bmp' {param($c);Remove-Item -LiteralPath (Join-Path $c 'user\mcla-environment-particle.bmp')}
  Expect-Fail 'identical-motion-frame' {param($c);Copy-Item -LiteralPath (Join-Path $c 'user\mcla-environment-stationary.bmp') -Destination (Join-Path $c 'user\mcla-environment-moving.bmp') -Force}
  Expect-Fail 'rotation-gap' {param($c);Copy-Item -LiteralPath (Join-Path $c 'mcla.log') -Destination (Join-Path $c 'mcla.2.log')}
  $app=[IO.File]::ReadAllText((Join-Path $repo 'src/mcla_app.cpp'));$runner=[IO.File]::ReadAllText((Join-Path $repo 'scripts/run-environment-effects-smoke.ps1'));$verifier=[IO.File]::ReadAllText($verify)
  $needles=@('mcla_environment_effects_probe, false','Capture a bounded Rain and Dawn Arcade rendering matrix','MCLA_ENVIRONMENT_EFFECTS_CONFIG v=1','weather=rain time=dawn frames=6','sequence <= 10','X_INPUT_GAMEPAD_DPAD_RIGHT, 14','X_INPUT_GAMEPAD_DPAD_RIGHT, 17','frontend_input_records=72 render_input_records=8','mcla-environment-options.bmp','mcla-environment-particle.bmp','--mcla_environment_effects_probe=true','--xam_user_signin_state=2','--async_shader_compilation=false','M6-004 failure required force cleanup','representative-environment-effects-pass-open-s2-defects','open-s2-preserved','open-s2-intermittent-preserved','whole_frame_console_parity_claimed=$false','Expected exactly 72 frontend input records.','Expected exactly eight render input records.','Environment comparison is insufficiently distinct.','Controlled lifecycle chronology is invalid.')
  foreach($needle in $needles){if(-not($app.Contains($needle)-or$runner.Contains($needle)-or$verifier.Contains($needle))){throw "Source contract needle is missing: $needle"};$sourceChecks++}
  [pscustomobject]@{Passed=$true;PhysicalPositives=1;SyntheticPositives=1;FailClosedNegatives=$negatives;SourceContractChecks=$sourceChecks}
}finally{if(Test-Path -LiteralPath $root){[GC]::Collect();[GC]::WaitForPendingFinalizers();for($attempt=0;$attempt-lt5;$attempt++){try{Remove-Item -LiteralPath $root -Recurse -Force;break}catch{if($attempt-eq4){throw};Start-Sleep -Milliseconds 200}}}}
