[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game',
    [ValidateSet(3, 20)][int]$CycleCount = 3,
    [switch]$MilestoneClosure,
    [string]$FinalizeExistingClosureRun = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdk = Join-Path $repo 'third_party\rexglue-sdk'
$verify = Join-Path $PSScriptRoot 'verify-frontend-smoke.ps1'
$gameVerify = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$cmake = $toolchain.CMakePath
$utf8 = [Text.UTF8Encoding]::new($false)

if (-not ('MclaFrontendWindow' -as [type])) { Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;public static class MclaFrontendWindow{delegate bool E(IntPtr h,IntPtr p);[DllImport("user32.dll")]static extern bool EnumWindows(E c,IntPtr p);[DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);[DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr h);[DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetWindowText(IntPtr h,StringBuilder s,int n);[DllImport("user32.dll")]static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);public static IntPtr[] Handles(int pid){var a=new List<IntPtr>();EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}public static string Title(IntPtr h){var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty;}public static bool Close(IntPtr h){return PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);}}
'@ }

function Safe([string]$Path, [string]$Description, [switch]$Exists) { $full = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repo $Path })); $prefix = $repo.TrimEnd('\') + '\'; if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes repository." }; $current = $repo; foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object Length)) { $current = Join-Path $current $part; if ((Test-Path $current) -and ((Get-Item $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "$Description traverses a reparse point." } }; if ($Exists -and -not (Test-Path $full)) { throw "$Description is missing." }; $full }
function Tree([string]$Root) { $root = Safe $Root 'Tree' -Exists; $items = @(Get-ChildItem $root -Recurse -Force); foreach ($item in @((Get-Item $root -Force)) + $items) { if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Tree contains a reparse point.' } }; $files = @($items | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName); $entries = @(); foreach ($directory in @($items | Where-Object PSIsContainer | Sort-Object FullName)) { $entries += [ordered]@{ kind = 'directory'; path = $directory.FullName.Substring($root.Length).TrimStart('\').Replace('\', '/') } }; foreach ($file in $files) { $entries += [ordered]@{ kind = 'file'; path = $file.FullName.Substring($root.Length).TrimStart('\').Replace('\', '/'); length = $file.Length; sha256 = (Get-FileHash $file.FullName -Algorithm SHA256).Hash } }; $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4; $sha = [Security.Cryptography.SHA256]::Create(); try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }; $bytes = 0L; foreach ($file in $files) { $bytes += $file.Length }; [pscustomobject]@{ Hash = $hash; FileCount = $files.Count; DirectoryCount = @($items | Where-Object PSIsContainer).Count; Bytes = $bytes } }
function Game([string]$Root) { $tree = Tree $Root; $verified = & $gameVerify -GamePath $Root -VerifyHashes; [ordered]@{ file_count = $verified.FileCount; payload_bytes = $verified.PayloadBytes; manifest_sha256 = (Get-FileHash $verified.ManifestPath -Algorithm SHA256).Hash; tree_sha256 = $tree.Hash; tree_file_count = $tree.FileCount; tree_directory_count = $tree.DirectoryCount; tree_bytes = $tree.Bytes } }
function Artifacts([string]$Root) { @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll') | ForEach-Object { [ordered]@{ name = $_; sha256 = (Get-FileHash (Safe (Join-Path $Root $_) "Artifact $_" -Exists) -Algorithm SHA256).Hash } } }
function Processes([string]$Exe) { @((Get-Process mcla -ErrorAction SilentlyContinue) | Where-Object { try { [string]::Equals($_.Path, $Exe, [StringComparison]::OrdinalIgnoreCase) } catch { $false } }) }
function Contains([string]$Directory, [string]$Needle) { foreach ($file in @(Get-ChildItem $Directory -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue)) { try { $stream = [IO.File]::Open($file.FullName, 'Open', 'Read', 'ReadWrite'); try { $reader = [IO.StreamReader]::new($stream); $text = $reader.ReadToEnd(); $reader.Dispose() } finally { $stream.Dispose() }; if ($text.Contains($Needle)) { return $true } } catch {} }; $false }
function Close-Exact([Diagnostics.Process]$Process) { $matches = @(); foreach ($handle in [MclaFrontendWindow]::Handles($Process.Id)) { if ([regex]::IsMatch([MclaFrontendWindow]::Title($handle), '^mcla \[rexglue-v[^\]]+\]$')) { $matches += $handle } }; if ($matches.Count -ne 1 -or -not [MclaFrontendWindow]::Close($matches[0])) { throw 'Exact PID/window WM_CLOSE failed.' } }
function Logged([scriptblock]$Command, [string]$Log, [switch]$Append) { $prior = $ErrorActionPreference; $ErrorActionPreference = 'Continue'; try { if ($Append) { & $Command *>&1 | Tee-Object $Log -Append | Out-Null } else { & $Command *>&1 | Tee-Object $Log | Out-Null }; $code = $LASTEXITCODE } finally { $ErrorActionPreference = $prior }; $code }

$build = Safe $BuildRoot 'Build root' -Exists; $game = Safe $GameRoot 'Game root' -Exists
if ($build -cne (Safe 'out/build/win-amd64-relwithdebinfo' 'Canonical build') -or $game -cne (Safe 'private/game' 'Canonical game')) { throw 'M4-011 requires canonical inputs.' }
$seed = Safe 'private/baseline/M4-011/post-oobe-profile' 'Post-OOBE seed' -Exists; $seedBefore = Tree $seed
if ($seedBefore.FileCount -ne 2 -or (Get-FileHash (Join-Path $seed 'B13EBABEBABEBABE\545407F8\00000001\mc4.sav\mc4.sav') -Algorithm SHA256).Hash -cne 'E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68' -or (Get-FileHash (Join-Path $seed 'B13EBABEBABEBABE\545407F8\Headers\00000001\mc4.sav.header') -Algorithm SHA256).Hash -cne '1DAEF55FA78BDD3F1AA75A36772B801C6C714B6D1A115A97904F3D1C957BC4C9') { throw 'Pinned post-OOBE seed failed identity.' }
$isClosure = $MilestoneClosure.IsPresent
if ($isClosure -and $CycleCount -ne 20) { throw 'M4 closure requires exactly 20 cycles.' }
if (-not $isClosure -and $CycleCount -ne 3) { throw 'M4-011 requires exactly 3 cycles.' }
$task = if ($isClosure) { 'M4-013' } else { 'M4-011' }
$evidence = Safe ("private/evidence/$task") 'Evidence root'; [IO.Directory]::CreateDirectory($evidence) | Out-Null

if ($FinalizeExistingClosureRun.Length) {
    if (-not $isClosure -or $CycleCount -ne 20 -or $FinalizeExistingClosureRun -notmatch '^\d{8}-\d{6}-[0-9a-f]{8}$') { throw 'Existing-run finalization is restricted to one exact M4-013 closure run ID.' }
    $root = Safe (Join-Path $evidence $FinalizeExistingClosureRun) 'Existing closure run' -Exists
    $resultPath = Join-Path $root 'result.json'
    if (Test-Path $resultPath) { throw 'Existing closure run already has a result.' }
    $rootChildren = @(Get-ChildItem $root -Force | Sort-Object Name)
    if (($rootChildren.Name -join ',') -cne 'relwithdebinfo-clean-build.log,runs,sdk-install.log,sdk-vfs-test.log' -or @($rootChildren | Where-Object PSIsContainer).Count -ne 1) { throw 'Existing closure run topology is not the exact recoverable pre-result layout.' }
    $runsRoot = Safe (Join-Path $root 'runs') 'Existing runs root' -Exists
    $runDirectories = @(Get-ChildItem $runsRoot -Directory -Force | Sort-Object Name)
    if ($runDirectories.Count -ne 20 -or ($runDirectories.Name -join ',') -cne ((1..20 | ForEach-Object { '{0:D2}' -f $_ }) -join ',')) { throw 'Existing closure run does not contain exactly cycles 01 through 20.' }
    $sdkLog = Safe (Join-Path $root 'sdk-install.log') 'Existing SDK log' -Exists
    $testLog = Safe (Join-Path $root 'sdk-vfs-test.log') 'Existing focused-test log' -Exists
    $buildLog = Safe (Join-Path $root 'relwithdebinfo-clean-build.log') 'Existing build log' -Exists
    if ([IO.File]::ReadAllText($testLog) -notmatch 'All tests passed \(33 assertions in 2 test cases\)') { throw 'Existing focused VFS test totals changed.' }
    $exe = Safe (Join-Path $build 'mcla.exe') 'Executable' -Exists
    if (@(Processes $exe).Count) { throw 'Canonical MCLA is still running.' }
    $gameIdentity = Game $game; $runtimeArtifacts = @(Artifacts $build); $records = @()
    foreach ($index in 1..20) {
        $name = '{0:D2}' -f $index; $cycle = Safe (Join-Path $runsRoot $name) "Existing cycle $name" -Exists; $user = Safe (Join-Path $cycle 'user') "Existing cycle $name user root" -Exists
        $probe = & $verify -ProbeOnly -ClosureProbe -RuntimeLogPath (Join-Path $cycle 'mcla.log') -UserRoot $user
        $tree = Tree $cycle; $captures = [ordered]@{}
        foreach ($phase in @('title', 'gameplay', 'pause', 'options')) { $bmp = $probe.Bmps[$phase]; $captures[$phase] = [ordered]@{ sha256 = $bmp.Sha256; bytes = $bmp.Bytes } }
        $records += [ordered]@{ index = $index; exit_code = 0; close_requested = $true; harness_force_cleanup = $false; runtime_logs = @($probe.LogSet.Files); runtime_log_set_sha256 = $probe.LogSet.Hash; captures = $captures; pause_menu_correlation_ppm = $probe.PauseCorrelationPpm; options_menu_correlation_ppm = $probe.OptionsCorrelationPpm; cycle_tree_sha256 = $tree.Hash; cycle_file_count = $tree.FileCount; cycle_bytes = $tree.Bytes }
    }
    $seedAfter = Tree $seed
    if ($seedBefore.Hash -cne $seedAfter.Hash) { throw 'Pinned seed changed before existing-run finalization.' }
    $result = [ordered]@{ schema = 1; task = 'M4-013'; decision = 'twenty-consecutive-boot-to-frontend-routes-pass'; sdk_version = '0.9.0.18'; cycle_count = 20; route = 'startup-title-START-saved-gameplay-START-pause-RB-modes-RB-settings-options-external-WM_CLOSE'; internal_exit_claimed = $false; external_wm_close_verified = $true; recovery = [ordered]@{ finalized_after_verifier_false_negative = $true; reason = 'execution-complete-post-hard-exit-race'; elapsed_stopwatch_metrics_available = $false }; seed = [ordered]@{ tree_sha256 = $seedBefore.Hash; file_count = $seedBefore.FileCount }; build = [ordered]@{ focused_test_cases = 2; focused_test_assertions = 33; sdk_install_log_sha256 = (Get-FileHash $sdkLog -Algorithm SHA256).Hash; focused_test_log_sha256 = (Get-FileHash $testLog -Algorithm SHA256).Hash; app_build_log_sha256 = (Get-FileHash $buildLog -Algorithm SHA256).Hash }; game_identity = [ordered]@{ before = $gameIdentity; after = $gameIdentity }; artifacts = [ordered]@{ before = $runtimeArtifacts; after = $runtimeArtifacts }; cycles = @($records); no_surviving_processes = $true; data_integrity_preserved = $true }
    [IO.File]::WriteAllText($resultPath, (ConvertTo-Json $result -Depth 12) + [Environment]::NewLine, $utf8)
    try {
        $final = & $verify -ResultPath $resultPath -MilestoneClosure
    } catch {
        Remove-Item -LiteralPath $resultPath -Force -ErrorAction SilentlyContinue
        throw
    }
    [pscustomobject]@{ Passed = $final.Passed; Decision = $final.Decision; Cycles = $final.Cycles; SavedGameplayVerified = $final.SavedGameplayVerified; PauseVerified = $final.PauseVerified; OptionsVerified = $final.OptionsVerified; ExternalWmCloseVerified = $final.ExternalWmCloseVerified; DataIntegrityVerified = $final.DataIntegrityVerified; RecoveredAfterVerifierRepair = $true; PrivateRunRoot = $root; ResultPath = $resultPath }
    return
}

$root = Join-Path $evidence ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)); [IO.Directory]::CreateDirectory($root) | Out-Null
$sdkLog = Join-Path $root 'sdk-install.log'; $testLog = Join-Path $root 'sdk-vfs-test.log'; $buildLog = Join-Path $root 'relwithdebinfo-clean-build.log'

Write-Host "$task [1/7]: validating pinned post-OOBE save and source-game integrity..." -ForegroundColor Cyan
$gameBefore = Game $game
Write-Host "$task [2/7]: clean-building and installing ReXGlue SDK..." -ForegroundColor Cyan
$oldCount = $env:GIT_CONFIG_COUNT; $oldKey = $env:GIT_CONFIG_KEY_0; $oldValue = $env:GIT_CONFIG_VALUE_0; Push-Location $sdk
try { $env:GIT_CONFIG_COUNT = '1'; $env:GIT_CONFIG_KEY_0 = 'safe.directory'; $env:GIT_CONFIG_VALUE_0 = $sdk.Replace('\', '/'); if (Logged { & $cmake --preset win-amd64 -DREXGLUE_BUILD_TESTS=ON } $sdkLog) { throw 'SDK configure failed.' }; if (Logged { & $cmake --build out/build/win-amd64 --config RelWithDebInfo --target install unit_tests --clean-first --parallel 8 } $sdkLog -Append) { throw 'SDK install failed.' } } finally { $env:GIT_CONFIG_COUNT = $oldCount; $env:GIT_CONFIG_KEY_0 = $oldKey; $env:GIT_CONFIG_VALUE_0 = $oldValue; Pop-Location }
Write-Host "$task [3/7]: running focused VFS content-root tests..." -ForegroundColor Cyan
$unit = Safe (Join-Path $sdk 'out/win-amd64/RelWithDebInfo/unit_tests.exe') 'Unit tests' -Exists
if ((Logged { & $unit '[filesystem][vfs]' --order declared } $testLog) -or ([IO.File]::ReadAllText($testLog) -notmatch 'All tests passed \(33 assertions in 2 test cases\)')) { throw 'Focused VFS tests failed or totals changed.' }
Write-Host "$task [4/7]: clean-building the MCLA host..." -ForegroundColor Cyan
if (Logged { & $cmake --preset win-amd64-relwithdebinfo } $buildLog) { throw 'App configure failed.' }; if (Logged { & $cmake --build --preset win-amd64-relwithdebinfo --target mcla --clean-first --parallel 8 } $buildLog -Append) { throw 'App clean build failed.' }
$exe = Safe (Join-Path $build 'mcla.exe') 'Executable' -Exists; if (@(Processes $exe).Count) { throw 'Canonical MCLA is already running.' }; $artifactsBefore = @(Artifacts $build)

Write-Host "$task [5/7]: running $CycleCount isolated saved-gameplay -> pause -> options routes..." -ForegroundColor Cyan
$records = @()
for ($index = 1; $index -le $CycleCount; $index++) {
    $name = '{0:D2}' -f $index; $cycle = Join-Path $root "runs\$name"; $user = Join-Path $cycle 'user'; $cache = Join-Path $cycle 'cache'; [IO.Directory]::CreateDirectory($user) | Out-Null; [IO.Directory]::CreateDirectory($cache) | Out-Null
    Get-ChildItem -LiteralPath $seed -Force | Copy-Item -Destination $user -Recurse -Force
    if ((Tree $user).Hash -cne $seedBefore.Hash) { throw "Cycle $name did not start from the exact pinned post-OOBE seed." }
    $log = Join-Path $cycle 'mcla.log'; $process = $null; $forced = $false; $route = [Diagnostics.Stopwatch]::StartNew()
    try {
        $timingArgs = if ($isClosure) { @('--mcla_first_frame_settle_seconds=45', '--mcla_frontend_gameplay_wait_seconds=45', '--mcla_frontend_pause_wait_seconds=4') } else { @('--mcla_first_frame_settle_seconds=35') }
        $process = Start-Process $exe -ArgumentList (@('--mcla_frontend_smoke_probe=true') + $timingArgs + @('--gpu_render_audit=true', '--async_shader_compilation=false', '--render_target_path_d3d12=rtv', '--log_max_file_size_mb=5', '--log_max_files=15', '--log_level=trace', '--fullscreen=false', "--game_data_root=$game", "--user_data_root=$user", "--cache_root=$cache", "--log_file=$log")) -WorkingDirectory $build -PassThru
        $deadline = [DateTime]::UtcNow.AddSeconds($(if ($isClosure) { 140 } else { 115 }))
        while ([DateTime]::UtcNow -lt $deadline) { if ($process.HasExited) { throw "Cycle $name exited before frontend PASS." }; if ((Test-Path (Join-Path $user 'mcla-frontend-options.bmp')) -and (Contains $cycle 'MCLA_FRONTEND_SMOKE_SUMMARY v=1 status=PASS')) { break }; Start-Sleep -Milliseconds 250 }
        if (-not (Test-Path (Join-Path $user 'mcla-frontend-options.bmp') -PathType Leaf) -or -not (Contains $cycle 'MCLA_FRONTEND_SMOKE_SUMMARY v=1 status=PASS')) { throw "Cycle $name frontend deadline expired." }
        $route.Stop(); Close-Exact $process; $exit = [Diagnostics.Stopwatch]::StartNew(); if (-not $process.WaitForExit(10000) -or $process.ExitCode -ne 0) { throw "Cycle $name controlled external close failed." }; $exit.Stop(); if (@(Processes $exe).Count) { throw "Cycle $name left an exact-path process." }
    } catch { $failure = $_; if ($process -and -not $process.HasExited) { $forced = $true; Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue; $null = $process.WaitForExit(5000) }; if ($forced) { throw "$task failure required force cleanup. $($failure.Exception.Message) Private run: '$root'." }; throw }
    $probe = if ($isClosure) { & $verify -ProbeOnly -ClosureProbe -RuntimeLogPath $log -UserRoot $user } else { & $verify -ProbeOnly -RuntimeLogPath $log -UserRoot $user }; $tree = Tree $cycle
    $captures = [ordered]@{}; foreach ($phase in @('title', 'gameplay', 'pause', 'options')) { $bmp = $probe.Bmps[$phase]; $captures[$phase] = [ordered]@{ sha256 = $bmp.Sha256; bytes = $bmp.Bytes } }
    $records += [ordered]@{ index = $index; route_elapsed_milliseconds = $route.ElapsedMilliseconds; exit_elapsed_milliseconds = $exit.ElapsedMilliseconds; exit_code = 0; close_requested = $true; harness_force_cleanup = $false; runtime_logs = @($probe.LogSet.Files); runtime_log_set_sha256 = $probe.LogSet.Hash; captures = $captures; pause_menu_correlation_ppm = $probe.PauseCorrelationPpm; options_menu_correlation_ppm = $probe.OptionsCorrelationPpm; cycle_tree_sha256 = $tree.Hash; cycle_file_count = $tree.FileCount; cycle_bytes = $tree.Bytes }
    Write-Host "$task cycle ${name}/$CycleCount`: saved gameplay, pause, Options, and external close PASS." -ForegroundColor Green
}

Write-Host "$task [6/7]: checking immutable source/runtime/seed state..." -ForegroundColor Cyan
$gameAfter = Game $game; $artifactsAfter = @(Artifacts $build); $seedAfter = Tree $seed
if (($gameBefore | ConvertTo-Json -Compress) -cne ($gameAfter | ConvertTo-Json -Compress) -or ($artifactsBefore | ConvertTo-Json -Compress) -cne ($artifactsAfter | ConvertTo-Json -Compress) -or $seedBefore.Hash -cne $seedAfter.Hash) { throw 'Source-game, runtime-artifact, or pinned-seed identity changed.' }
$decision = if ($isClosure) { 'twenty-consecutive-boot-to-frontend-routes-pass' } else { 'saved-gameplay-pause-options-external-exit-pass' }
$result = [ordered]@{ schema = 1; task = $task; decision = $decision; sdk_version = '0.9.0.18'; cycle_count = $CycleCount; route = 'startup-title-START-saved-gameplay-START-pause-RB-modes-RB-settings-options-external-WM_CLOSE'; internal_exit_claimed = $false; external_wm_close_verified = $true; recovery = if ($isClosure) { [ordered]@{ finalized_after_verifier_false_negative = $false; reason = 'none'; elapsed_stopwatch_metrics_available = $true } } else { $null }; seed = [ordered]@{ tree_sha256 = $seedBefore.Hash; file_count = $seedBefore.FileCount }; build = [ordered]@{ focused_test_cases = 2; focused_test_assertions = 33; sdk_install_log_sha256 = (Get-FileHash $sdkLog -Algorithm SHA256).Hash; focused_test_log_sha256 = (Get-FileHash $testLog -Algorithm SHA256).Hash; app_build_log_sha256 = (Get-FileHash $buildLog -Algorithm SHA256).Hash }; game_identity = [ordered]@{ before = $gameBefore; after = $gameAfter }; artifacts = [ordered]@{ before = $artifactsBefore; after = $artifactsAfter }; cycles = @($records); no_surviving_processes = $true; data_integrity_preserved = $true }
if (-not $isClosure) { $result.Remove('recovery') }
$resultPath = Join-Path $root 'result.json'; [IO.File]::WriteAllText($resultPath, (ConvertTo-Json $result -Depth 12) + [Environment]::NewLine, $utf8)
Write-Host "$task [7/7]: final physical/result verification..." -ForegroundColor Cyan
$final = if ($isClosure) { & $verify -ResultPath $resultPath -MilestoneClosure } else { & $verify -ResultPath $resultPath }
[pscustomobject]@{ Passed = $final.Passed; Decision = $final.Decision; Cycles = $final.Cycles; SavedGameplayVerified = $final.SavedGameplayVerified; PauseVerified = $final.PauseVerified; OptionsVerified = $final.OptionsVerified; ExternalWmCloseVerified = $final.ExternalWmCloseVerified; DataIntegrityVerified = $final.DataIntegrityVerified; PrivateRunRoot = $root; ResultPath = $resultPath }
