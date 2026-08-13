[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdk = Join-Path $repo 'third_party\rexglue-sdk'
$verify = Join-Path $PSScriptRoot 'verify-offline-service-smoke.ps1'
$gameVerify = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$cmake = $toolchain.CMakePath
$utf8 = [Text.UTF8Encoding]::new($false)

if (-not ('MclaOfflineServiceWindow' -as [type])) {
    Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;
public static class MclaOfflineServiceWindow{delegate bool E(IntPtr h,IntPtr p);[DllImport("user32.dll")]static extern bool EnumWindows(E c,IntPtr p);[DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);[DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr h);[DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetWindowText(IntPtr h,StringBuilder s,int n);[DllImport("user32.dll")]static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);public static IntPtr[] Handles(int pid){var a=new List<IntPtr>();EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}public static string Title(IntPtr h){var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty;}public static bool Close(IntPtr h){return PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);}}
'@
}

function Resolve-SafePath([string]$Path, [string]$What, [switch]$MustExist) {
    $full = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repo $Path }))
    $prefix = $repo.TrimEnd('\') + '\'
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$What escapes the repository."
    }
    $current = $repo
    foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object Length)) {
        $current = Join-Path $current $part
        if ((Test-Path $current) -and ((Get-Item $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "$What traverses a reparse point."
        }
    }
    if ($MustExist -and -not (Test-Path $full)) {
        throw "$What is missing."
    }
    $full
}

function Assert-SafeTree([string]$Root) {
    $pending = [Collections.Generic.Stack[string]]::new()
    $pending.Push($Root)
    while ($pending.Count) {
        foreach ($item in @(Get-ChildItem -LiteralPath $pending.Pop() -Force)) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw 'Tree contains a reparse point.'
            }
            if ($item.PSIsContainer) { $pending.Push($item.FullName) }
        }
    }
}

function Get-TreeIdentity([string]$Root) {
    Assert-SafeTree $Root
    $items = @(Get-ChildItem -LiteralPath $Root -Recurse -Force)
    $files = @($items | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)
    $entries = @()
    foreach ($directory in @($items | Where-Object PSIsContainer | Sort-Object FullName)) {
        $entries += [ordered]@{ kind = 'directory'; path = $directory.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/') }
    }
    foreach ($file in $files) {
        $entries += [ordered]@{ kind = 'file'; path = $file.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/'); length = $file.Length; sha256 = (Get-FileHash $file.FullName -Algorithm SHA256).Hash }
    }
    $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
    $bytes = 0L
    foreach ($file in $files) { $bytes += $file.Length }
    [pscustomobject]@{ Hash = $hash; FileCount = $files.Count; DirectoryCount = @($items | Where-Object PSIsContainer).Count; Bytes = $bytes }
}

function Get-GameIdentity([string]$Root) {
    $tree = Get-TreeIdentity $Root
    $verified = & $gameVerify -GamePath $Root -VerifyHashes
    [ordered]@{ file_count = $verified.FileCount; payload_bytes = $verified.PayloadBytes; hashes_verified = $verified.HashesVerified; manifest_sha256 = (Get-FileHash $verified.ManifestPath -Algorithm SHA256).Hash; tree_sha256 = $tree.Hash; tree_file_count = $tree.FileCount; tree_directory_count = $tree.DirectoryCount; tree_bytes = $tree.Bytes }
}

function Get-Artifacts([string]$Root) {
    @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll') | ForEach-Object {
        $path = Resolve-SafePath (Join-Path $Root $_) "Artifact $_" -MustExist
        [ordered]@{ name = $_; sha256 = (Get-FileHash $path -Algorithm SHA256).Hash }
    }
}

function Get-ExactProcesses([string]$Exe) {
    @((Get-Process mcla -ErrorAction SilentlyContinue) | Where-Object {
        try { [string]::Equals($_.Path, $Exe, [StringComparison]::OrdinalIgnoreCase) } catch { $false }
    })
}

function Test-LogContains([string]$Directory, [string]$Needle) {
    foreach ($file in @(Get-ChildItem -LiteralPath $Directory -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue)) {
        try {
            $stream = [IO.File]::Open($file.FullName, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
            try { $reader = [IO.StreamReader]::new($stream, $utf8, $true, 65536, $false); $text = $reader.ReadToEnd(); $reader.Dispose() } finally { $stream.Dispose() }
            if ($text.Contains($Needle)) { return $true }
        } catch {}
    }
    $false
}

function Close-ExactWindow([Diagnostics.Process]$Process) {
    $matches = @()
    foreach ($handle in [MclaOfflineServiceWindow]::Handles($Process.Id)) {
        if ([regex]::IsMatch([MclaOfflineServiceWindow]::Title($handle), '^mcla \[rexglue-v[^\]]+\]$')) { $matches += $handle }
    }
    if ($matches.Count -ne 1 -or -not [MclaOfflineServiceWindow]::Close($matches[0])) {
        throw 'Exact PID/window WM_CLOSE failed.'
    }
}

function Invoke-NativeLogged([scriptblock]$Command, [string]$Log, [switch]$Append) {
    $prior = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        if ($Append) { & $Command *>&1 | Tee-Object $Log -Append | Out-Null } else { & $Command *>&1 | Tee-Object $Log | Out-Null }
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $prior }
    $code
}

$build = Resolve-SafePath $BuildRoot 'Build root' -MustExist
$game = Resolve-SafePath $GameRoot 'Game root' -MustExist
if ($build -cne (Resolve-SafePath 'out/build/win-amd64-relwithdebinfo' 'Canonical build') -or $game -cne (Resolve-SafePath 'private/game' 'Canonical game')) {
    throw 'M4-009 requires canonical inputs.'
}
$evidence = Resolve-SafePath 'private/evidence/M4-009' 'Evidence root'
[IO.Directory]::CreateDirectory($evidence) | Out-Null
$root = Join-Path $evidence ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
$cycle = Join-Path $root 'runs\01'
$user = Join-Path $cycle 'user'
$cache = Join-Path $cycle 'cache'
[IO.Directory]::CreateDirectory($user) | Out-Null
[IO.Directory]::CreateDirectory($cache) | Out-Null

Write-Host 'M4-009 [1/7]: validating canonical inputs and source-game integrity...' -ForegroundColor Cyan
$gameBefore = Get-GameIdentity $game
$sdkLog = Join-Path $root 'sdk-install.log'
$unitLog = Join-Path $root 'sdk-offline-service-test.log'
$buildLog = Join-Path $root 'relwithdebinfo-clean-build.log'

Write-Host 'M4-009 [2/7]: clean-building and installing ReXGlue SDK...' -ForegroundColor Cyan
Push-Location $sdk
$oldCount = $env:GIT_CONFIG_COUNT; $oldKey = $env:GIT_CONFIG_KEY_0; $oldValue = $env:GIT_CONFIG_VALUE_0
try {
    $env:GIT_CONFIG_COUNT = '1'; $env:GIT_CONFIG_KEY_0 = 'safe.directory'; $env:GIT_CONFIG_VALUE_0 = $sdk.Replace('\', '/')
    if (Invoke-NativeLogged { & $cmake --preset win-amd64 -DREXGLUE_BUILD_TESTS=ON } $sdkLog) { throw 'SDK configure failed.' }
    if (Invoke-NativeLogged { & $cmake --build out/build/win-amd64 --config RelWithDebInfo --target install unit_tests --clean-first --parallel 8 } $sdkLog -Append) { throw 'SDK install failed.' }
} finally {
    $env:GIT_CONFIG_COUNT = $oldCount; $env:GIT_CONFIG_KEY_0 = $oldKey; $env:GIT_CONFIG_VALUE_0 = $oldValue
    Pop-Location
}

Write-Host 'M4-009 [3/7]: running focused offline-service tests...' -ForegroundColor Cyan
$unit = Resolve-SafePath (Join-Path $sdk 'out/win-amd64/RelWithDebInfo/unit_tests.exe') 'Unit tests' -MustExist
if ((Invoke-NativeLogged { & $unit '[kernel][offline-service]' --order declared } $unitLog) -or ([IO.File]::ReadAllText($unitLog) -notmatch 'All tests passed \(32 assertions in 3 test cases\)')) {
    throw 'Focused offline-service tests failed or totals changed.'
}

Write-Host 'M4-009 [4/7]: clean-building the MCLA host...' -ForegroundColor Cyan
if (Invoke-NativeLogged { & $cmake --preset win-amd64-relwithdebinfo } $buildLog) { throw 'App configure failed.' }
if (Invoke-NativeLogged { & $cmake --build --preset win-amd64-relwithdebinfo --target mcla --clean-first --parallel 8 } $buildLog -Append) { throw 'App clean build failed.' }
$exe = Resolve-SafePath (Join-Path $build 'mcla.exe') 'MCLA executable' -MustExist
if (@(Get-ExactProcesses $exe).Count) { throw 'Canonical MCLA is already running.' }
$artifactsBefore = @(Get-Artifacts $build)
$log = Join-Path $cycle 'mcla.log'
$bmp = Join-Path $user 'mcla-first-frame.bmp'
$process = $null
$forced = $false

Write-Host 'M4-009 [5/7]: launching the network-blocked title route (~45 seconds)...' -ForegroundColor Cyan
try {
    $process = Start-Process $exe -ArgumentList @('--xam_offline_service_audit=true', '--xam_offline_network_block=true', '--mcla_first_frame_probe=true', '--mcla_first_frame_settle_seconds=35', '--gpu_render_audit=true', '--async_shader_compilation=false', '--render_target_path_d3d12=rtv', '--log_max_file_size_mb=5', '--log_max_files=15', '--log_level=trace', '--fullscreen=false', "--game_data_root=`"$game`"", "--user_data_root=`"$user`"", "--cache_root=`"$cache`"", "--log_file=`"$log`"") -WorkingDirectory $build -PassThru
    $deadline = [datetime]::UtcNow.AddSeconds(100)
    while ([datetime]::UtcNow -lt $deadline) {
        if ($process.HasExited) { throw 'Process exited before offline-service summary.' }
        if ((Test-Path $bmp) -and (Test-LogContains $cycle 'OFFLINE_SERVICE_AUDIT_SUMMARY v=1 phase=title status=PASS') -and (Test-LogContains $cycle 'MCLA offline services: title route summarized')) { break }
        Start-Sleep -Milliseconds 500
    }
    if (-not (Test-Path $bmp) -or -not (Test-LogContains $cycle 'OFFLINE_SERVICE_AUDIT_SUMMARY v=1 phase=title status=PASS')) { throw 'Offline title deadline expired.' }
    Write-Host 'M4-009 [6/7]: offline-service boundary stayed explicit; closing normally...' -ForegroundColor Cyan
    Close-ExactWindow $process
    $exit = [Diagnostics.Stopwatch]::StartNew()
    if (-not $process.WaitForExit(10000) -or $process.ExitCode -ne 0) { throw 'Controlled exit failed.' }
    $exit.Stop()
    if (@(Get-ExactProcesses $exe).Count) { throw 'MCLA process survived controlled exit.' }
} catch {
    $errorRecord = $_
    if ($process -and -not $process.HasExited) {
        $forced = $true
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $null = $process.WaitForExit(5000)
    }
    if ($forced) { throw "M4-009 failure required force cleanup. $($errorRecord.Exception.Message) Private run: '$root'." }
    throw
}

$probe = & $verify -ProbeOnly -RuntimeLogPath $log -BmpPath $bmp
$gameAfter = Get-GameIdentity $game
$artifactsAfter = @(Get-Artifacts $build)
if (($gameBefore | ConvertTo-Json -Compress) -cne ($gameAfter | ConvertTo-Json -Compress)) { throw 'Source-game identity changed.' }
if (($artifactsBefore | ConvertTo-Json -Compress) -cne ($artifactsAfter | ConvertTo-Json -Compress)) { throw 'Runtime artifacts changed during the cycle.' }
$cycleTree = Get-TreeIdentity $cycle
$result = [ordered]@{
    schema = 1; task = 'M4-009'; decision = 'offline-service-title-route-pass'; sdk_version = '0.9.0.16'; network_block_enabled = $true; frontend_title_reached = $true
    build = [ordered]@{ focused_test_cases = 3; focused_test_assertions = 32; sdk_install_log_sha256 = (Get-FileHash $sdkLog -Algorithm SHA256).Hash; focused_test_log_sha256 = (Get-FileHash $unitLog -Algorithm SHA256).Hash; app_build_log_sha256 = (Get-FileHash $buildLog -Algorithm SHA256).Hash }
    game_identity = [ordered]@{ before = $gameBefore; after = $gameAfter }
    artifacts = [ordered]@{ before = $artifactsBefore; after = $artifactsAfter }
    cycle = [ordered]@{ runtime_logs = @($probe.LogSet.Files); runtime_log_set_sha256 = $probe.LogSet.Hash; capture_sha256 = $probe.Bmp.Sha256; message_calls = $probe.Calls; socket_attempts = $probe.SocketAttempts; blocked_socket_attempts = $probe.BlockedSocketAttempts; exit_elapsed_milliseconds = $exit.ElapsedMilliseconds; exit_code = 0; harness_force_cleanup = $false; cycle_tree_sha256 = $cycleTree.Hash; cycle_file_count = $cycleTree.FileCount; cycle_bytes = $cycleTree.Bytes }
    no_surviving_processes = $true; data_integrity_preserved = $true
}
$resultPath = Join-Path $root 'result.json'
[IO.File]::WriteAllText($resultPath, (ConvertTo-Json $result -Depth 10) + [Environment]::NewLine, $utf8)

Write-Host 'M4-009 [7/7]: final physical/result verification...' -ForegroundColor Cyan
$final = & $verify -ResultPath $resultPath
[pscustomobject]@{ Passed = $final.Passed; Decision = $final.Decision; MessageCalls = $final.MessageCalls; SocketAttempts = $final.SocketAttempts; FrontendTitleReached = $final.FrontendTitleReached; NetworkBlockVerified = $final.NetworkBlockVerified; DataIntegrityVerified = $final.DataIntegrityVerified; PrivateRunRoot = $root; ResultPath = $resultPath }
