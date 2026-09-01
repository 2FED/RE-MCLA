[CmdletBinding()]
param(
  [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
  [string]$GameRoot = 'private/game',
  [ValidateRange(1,1)][int]$CycleCount = 1,
  [string]$InitialUserRoot,
  [switch]$ReuseCurrentBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdk = Join-Path $repo 'third_party/rexglue-sdk'
$seed = Join-Path $repo 'private/baseline/M4-011/post-oobe-profile'
$verifier = Join-Path $PSScriptRoot 'verify-race-results-smoke.ps1'
$cmake = (& (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath).CMakePath
$utf8 = [Text.UTF8Encoding]::new($false)
$sdkCommit = '3ef5b4f143d56b57e3c0e539cb0009ffe3a67e05'
$saveRelative = 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
$headerRelative = 'B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'

if ($CycleCount -ne 1) { throw 'The race-route capture is intentionally one physical series; repeated-race resource checks belong to M5-013.' }

if (-not ('MclaRaceWindow' -as [type])) {
  Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;
public static class MclaRaceWindow{delegate bool E(IntPtr h,IntPtr p);[DllImport("user32.dll")]static extern bool EnumWindows(E c,IntPtr p);[DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);[DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr h);[DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetWindowText(IntPtr h,StringBuilder s,int n);[DllImport("user32.dll")]static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);public static IntPtr[] Handles(int pid){var a=new List<IntPtr>();EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}public static string Title(IntPtr h){var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty;}public static bool Close(IntPtr h){return PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);}}
'@
}

function Resolve-SafePath([string]$Path,[string]$Description,[switch]$Exists) {
  $full = if ([IO.Path]::IsPathRooted($Path)) { [IO.Path]::GetFullPath($Path) } else { [IO.Path]::GetFullPath((Join-Path $repo $Path)) }
  $prefix = $repo.TrimEnd('\') + '\'
  if (-not $full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes repository." }
  $current = $repo
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object { $_.Length })) {
    $current = Join-Path $current $part
    if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "$Description traverses a reparse point." }
  }
  if ($Exists -and -not (Test-Path -LiteralPath $full)) { throw "$Description is missing." }
  $full
}
function Read-LiveLogs([string]$Root) { $text=''; foreach($file in @(Get-ChildItem -LiteralPath $Root -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue)){try{$stream=[IO.File]::Open($file.FullName,'Open','Read','ReadWrite');try{$reader=[IO.StreamReader]::new($stream);$text+=$reader.ReadToEnd();$reader.Dispose()}finally{$stream.Dispose()}}catch{}};$text }
function Wait-Marker([Diagnostics.Process]$Process,[string]$Root,[string]$Marker,[int]$Seconds,[string]$Step) { $deadline=[DateTime]::UtcNow.AddSeconds($Seconds);while([DateTime]::UtcNow-lt$deadline){if($Process.HasExited){throw "Process exited during '$Step'."};if((Read-LiveLogs $Root).Contains($Marker)){return};Start-Sleep -Milliseconds 200};throw "Timed out during '$Step'." }
function Close-ExactWindow([Diagnostics.Process]$Process) { $matches=@();foreach($handle in [MclaRaceWindow]::Handles($Process.Id)){if([regex]::IsMatch([MclaRaceWindow]::Title($handle),'^mcla \[rexglue-v[^\]]+\]$')){$matches+=$handle}};if($matches.Count-ne1-or-not[MclaRaceWindow]::Close($matches[0])){throw 'Exact PID/window WM_CLOSE failed.'} }
function Get-ExactProcesses([string]$Executable) { @((Get-Process mcla -ErrorAction SilentlyContinue)|Where-Object{try{[string]::Equals($_.Path,$Executable,[StringComparison]::OrdinalIgnoreCase)}catch{$false}}) }
function Invoke-Logged([scriptblock]$Command,[string]$Log,[switch]$Append) { $prior=$ErrorActionPreference;$ErrorActionPreference='Continue';try{if($Append){&$Command *>&1|Tee-Object -FilePath $Log -Append|Out-Null}else{&$Command *>&1|Tee-Object -FilePath $Log|Out-Null};$LASTEXITCODE}finally{$ErrorActionPreference=$prior} }
function Confirm-Checkpoint([Diagnostics.Process]$Process,[string]$CycleRoot,[string]$User,[string]$Phase,[string[]]$Exact,[string]$Instruction) {
  Write-Host ("{0,-16} | {1}" -f $Phase.ToUpperInvariant(),$Instruction) -ForegroundColor Yellow
  $confirmation = $null
  while ($Exact -cnotcontains $confirmation) {
    if ($Process.HasExited) { throw "Process exited before '$Phase' confirmation." }
    Write-Host ("Type exactly one of: " + ($Exact -join '  /  ')) -ForegroundColor Green
    $confirmation = Read-Host
    if ($Exact -cnotcontains $confirmation) {
      Write-Host ("{0,-16} | IGNORED - game remains running; enter the exact phrase when ready" -f $Phase.ToUpperInvariant()) -ForegroundColor DarkYellow
    }
  }
  if ($Process.HasExited) { throw "Process exited before '$Phase' capture." }
  $request = Join-Path $User ".mcla-race-$Phase.request"
  [IO.File]::WriteAllText($request,'1',$utf8)
  Wait-Marker $Process $CycleRoot "MCLA_RACE_ROUTE_FRAME v=1 phase=$Phase width=1280 height=720" 15 "$Phase capture"
  if (Test-Path -LiteralPath $request) { throw "The '$Phase' request was not consumed." }
  Write-Host ("{0,-16} | PASS" -f $Phase.ToUpperInvariant()) -ForegroundColor Cyan
  $confirmation
}

$build=Resolve-SafePath $BuildRoot 'Build root' -Exists
$game=Resolve-SafePath $GameRoot 'Game root' -Exists
if (-not [string]::Equals($build,(Resolve-SafePath 'out/build/win-amd64-relwithdebinfo' 'Canonical build' -Exists),[StringComparison]::OrdinalIgnoreCase) -or -not [string]::Equals($game,(Resolve-SafePath 'private/game' 'Canonical game' -Exists),[StringComparison]::OrdinalIgnoreCase)) { throw 'M5-012 requires canonical roots.' }
& (Join-Path $PSScriptRoot 'verify-first-race-route.ps1') | Out-Null
$tag=(&git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim();$head=(&git -C $sdk rev-parse HEAD).Trim()
if($LASTEXITCODE-ne0-or$tag-cne'v0.9.0.21'-or$head-cne$sdkCommit-or(git -C $sdk status --porcelain)){throw 'M5-012 requires clean exact ReXGlue v0.9.0.21.'}
$seedSave=(Get-FileHash -LiteralPath (Join-Path $seed $saveRelative) -Algorithm SHA256).Hash
$seedHeader=(Get-FileHash -LiteralPath (Join-Path $seed $headerRelative) -Algorithm SHA256).Hash
if($seedSave-cne'E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68'-or$seedHeader-cne'1DAEF55FA78BDD3F1AA75A36772B801C6C714B6D1A115A97904F3D1C957BC4C9'){throw 'Pinned save identity failed.'}
if($InitialUserRoot){if($CycleCount-ne1){throw 'InitialUserRoot is calibration-only and requires CycleCount 1.'};$initialUser=Resolve-SafePath $InitialUserRoot 'Calibration user root' -Exists;if(-not(Test-Path -LiteralPath (Join-Path $initialUser $saveRelative)-PathType Leaf)-or-not(Test-Path -LiteralPath (Join-Path $initialUser $headerRelative)-PathType Leaf)){throw 'Calibration user root lacks the exact save/header topology.'}}else{$initialUser=$seed}

$runRoot=Resolve-SafePath ('private/evidence/M5-012/'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)) 'Run root'
[IO.Directory]::CreateDirectory($runRoot)|Out-Null
$buildLog=Join-Path $runRoot 'app-clean-build.log'
Write-Host 'M5-012 route [1/3]: clean-building the physical race-route host...' -ForegroundColor Cyan
if($ReuseCurrentBuild){if($CycleCount-ne1-or-not$InitialUserRoot){throw 'ReuseCurrentBuild is calibration-only and requires CycleCount 1 plus InitialUserRoot.'};$register=[IO.File]::ReadAllText((Join-Path $repo 'generated/default/mcla_register.cpp'));foreach($entry in @('0x822C9FE8','0x82264760','0x82264770','0x82262320')){if(-not$register.Contains("registrar->SetFunction($entry")){throw "Current generated dispatcher lacks repaired entry $entry."}};[IO.File]::WriteAllText($buildLog,"Calibration reused the primary-agent-reviewed clean 30034-entry build.`r`n",$utf8);Write-Host 'Reusing the just-reviewed clean build for this final calibration attempt.' -ForegroundColor DarkCyan}elseif((Invoke-Logged { &$cmake --preset win-amd64-relwithdebinfo } $buildLog)-ne0-or(Invoke-Logged { &$cmake --build --preset win-amd64-relwithdebinfo --target mcla --clean-first --parallel 8 } $buildLog -Append)-ne0){throw "App clean build failed. Private run: '$runRoot'."}
$exe=Resolve-SafePath (Join-Path $build 'mcla.exe') 'Executable' -Exists
if(@(Get-ExactProcesses $exe).Count){throw 'Canonical MCLA is already running.'}

Write-Host 'M5-012 route [2/3]: one physical results-to-free-roam series.' -ForegroundColor Cyan
Write-Host 'Use the selected controller in the game. The console confirmations capture the last guest frame; they do not press game buttons.' -ForegroundColor DarkCyan
$cycles=@();$priorUser=$null;$persistedHash=$null
for($cycle=1;$cycle-le$CycleCount;$cycle++){
  $cycleRoot=Join-Path $runRoot ('runs/{0:D2}'-f$cycle);$user=Join-Path $cycleRoot 'user';$cache=Join-Path $cycleRoot 'cache'
  [IO.Directory]::CreateDirectory($user)|Out-Null;[IO.Directory]::CreateDirectory($cache)|Out-Null
  $source=if($null-eq$priorUser){$initialUser}else{$priorUser}
  Copy-Item -LiteralPath (Join-Path $source 'B13EBABEBABEBABE') -Destination $user -Recurse -Force
  $before=(Get-FileHash -LiteralPath (Join-Path $user $saveRelative) -Algorithm SHA256).Hash
  if($null-ne$persistedHash-and$before-cne$persistedHash){throw 'Race result did not survive into the next process.'}
  $runtimeLog=Join-Path $cycleRoot 'mcla.log'
  $arguments=@('--mcla_race_route_probe=true','--mcla_first_frame_settle_seconds=35','--input_backend=sdl','--mnk_mode=false','--async_shader_compilation=false','--d3d12_pipeline_creation_threads=0','--log_max_file_size_mb=8','--log_max_files=15','--log_level=info','--fullscreen=false',"--game_data_root=`"$game`"","--user_data_root=`"$user`"","--cache_root=`"$cache`"","--log_file=`"$runtimeLog`"")
  Write-Host "`nCYCLE $cycle/$CycleCount`: booting to title (normally about 45 seconds)..." -ForegroundColor Magenta
  $process=$null;$forced=$false
  try{
    $process=Start-Process $exe -ArgumentList $arguments -WorkingDirectory $build -PassThru
    Wait-Marker $process $cycleRoot 'MCLA_RACE_ROUTE_CONFIG v=1 phases=race-start,results,return' 70 'title route'
    Write-Host 'TITLE            | PASS - press START, open GPS with BACK, select available Ian or Martin, reach and challenge with Y.' -ForegroundColor Cyan
    $raceConfirmation = Confirm-Checkpoint $process $cycleRoot $user 'race-start' @('RACE START 2/2 IAN','RACE START 2/2 MARTIN') 'At the two-car start with HUD position 2/2, Alt-Tab here.'
    $opponent = if ($raceConfirmation.EndsWith('IAN',[StringComparison]::Ordinal)) { 'Ian' } else { 'Martin' }
    $raceStartHash=(Get-FileHash -LiteralPath (Join-Path $user $saveRelative) -Algorithm SHA256).Hash
    Confirm-Checkpoint $process $cycleRoot $user 'results' "FINAL SERIES RESULTS $($opponent.ToUpperInvariant())" 'Complete every NEXT RACE event. At the final series results/standings screen, Alt-Tab here.' | Out-Null
    Confirm-Checkpoint $process $cycleRoot $user 'return' 'RETURN FREE ROAM' 'Continue until the car is controllable in free roam, then Alt-Tab here.' | Out-Null
    Wait-Marker $process $cycleRoot 'MCLA_RACE_ROUTE_SUMMARY v=1 status=PASS frames=3 external_close_required=1' 10 'route summary'
    Write-Host 'Closing the console-style title externally with WM_CLOSE...' -ForegroundColor DarkCyan
    Close-ExactWindow $process
    if(-not$process.WaitForExit(10000)-or$process.ExitCode-ne0){throw 'Controlled external WM_CLOSE failed.'}
  }catch{$failure=$_;if($null-ne$process-and-not$process.HasExited){$forced=$true;Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue;$null=$process.WaitForExit(5000)};if($forced){throw "M5-012 failure required force cleanup. $($failure.Exception.Message) Private run: '$runRoot'."};throw}
  $after=(Get-FileHash -LiteralPath (Join-Path $user $saveRelative) -Algorithm SHA256).Hash
  if($cycle-eq1-and$after-ceq$raceStartHash){throw 'Cycle 1 save did not change between race start and post-results close.'}
  $probe=&$verifier -RunPath $cycleRoot -Fixture
  $cycles+=[ordered]@{index=$cycle;opponent=$opponent;save_before_sha256=$before;save_at_race_start_sha256=$raceStartHash;save_after_sha256=$after;probe=$probe}
  $priorUser=$user;$persistedHash=$after
}

Write-Host 'M5-012 route [3/3]: verifying the transition and observed save change...' -ForegroundColor Cyan
Write-Host "M5-012 ROUTE PASS: one complete series reached final results and controllable free roam. Evidence remains private at '$runRoot'. The separate Release restart gate closes persistence and stock-speed restart." -ForegroundColor Green
[pscustomobject][ordered]@{Decision='single-series-route-evidence-pass';RunRoot=$runRoot;Opponent=$cycles[0].opponent;SaveChanged=($cycles[0].save_after_sha256-cne$cycles[0].save_at_race_start_sha256);ControlledExit=$cycles[0].probe.controlled_exit}
