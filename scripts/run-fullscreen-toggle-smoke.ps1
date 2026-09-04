[CmdletBinding()]
param([switch]$SkipBuild)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sdk = Join-Path $repo 'third_party\rexglue-sdk'
$build = Join-Path $repo 'out\build\win-amd64-release'
$exe = Join-Path $build 'mcla.exe'
$game = Join-Path $repo 'private\game'
$runId = (Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8)
$root = Join-Path $repo "private\evidence\M7-016\fullscreen-$runId"
$user = Join-Path $root 'user'
$cache = Join-Path $root 'cache'
$log = Join-Path $root 'mcla.log'
$resultPath = Join-Path $root 'result.json'
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.Directory]::CreateDirectory($user) | Out-Null
[IO.Directory]::CreateDirectory($cache) | Out-Null

function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try { $sha = [Security.Cryptography.SHA256]::Create(); try { ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','') } finally { $sha.Dispose() } }
    finally { $stream.Dispose() }
}
function Read-Shared([string]$Path) {
    $stream = [IO.File]::Open($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
    try { $reader = [IO.StreamReader]::new($stream,[Text.Encoding]::UTF8,$true); try { $reader.ReadToEnd() } finally { $reader.Dispose() } }
    finally { $stream.Dispose() }
}
function Get-LogText { if (Test-Path -LiteralPath $log) { try { Read-Shared $log } catch [IO.IOException] { '' } } else { '' } }
function Wait-Log([Diagnostics.Process]$Process,[string]$Needle,[int]$Seconds) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($Process.HasExited) { throw "MCLA exited while waiting for '$Needle'." }
        if ((Get-LogText).Contains($Needle)) { return }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for '$Needle'."
}

if (-not ('MclaFullscreenNative' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
using System.Threading;
public static class MclaFullscreenNative {
  delegate bool EnumProc(IntPtr hwnd, IntPtr value);
  [StructLayout(LayoutKind.Sequential)] struct Rect { public int left, top, right, bottom; }
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Auto)] struct MonitorInfo { public int size; public Rect monitor; public Rect work; public uint flags; }
  [StructLayout(LayoutKind.Sequential)] struct Point { public int x, y; }
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc callback, IntPtr value);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
  [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr hwnd, out Rect rect);
  [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr hwnd, ref Point point);
  [DllImport("user32.dll")] static extern IntPtr MonitorFromWindow(IntPtr hwnd, uint flags);
  [DllImport("user32.dll", CharSet=CharSet.Auto)] static extern bool GetMonitorInfo(IntPtr monitor, ref MonitorInfo info);
  [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr hwnd);
  [DllImport("user32.dll")] static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] static extern void mouse_event(uint flags, int dx, int dy, uint data, UIntPtr extra);
  [DllImport("user32.dll")] static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
  public static IntPtr Find(int processId) {
    IntPtr found=IntPtr.Zero;
    EnumWindows(delegate(IntPtr hwnd, IntPtr value) { uint pid; GetWindowThreadProcessId(hwnd,out pid); if(pid==(uint)processId && IsWindowVisible(hwnd) && found==IntPtr.Zero) found=hwnd; return true; }, IntPtr.Zero);
    return found;
  }
  public static bool IsFullscreen(IntPtr hwnd) {
    Rect window; if(!GetWindowRect(hwnd,out window)) throw new Exception("GetWindowRect failed");
    IntPtr monitor=MonitorFromWindow(hwnd,2); MonitorInfo info=new MonitorInfo(); info.size=Marshal.SizeOf(typeof(MonitorInfo));
    if(monitor==IntPtr.Zero || !GetMonitorInfo(monitor,ref info)) throw new Exception("GetMonitorInfo failed");
    const int tolerance=2;
    return Math.Abs(window.left-info.monitor.left)<=tolerance && Math.Abs(window.top-info.monitor.top)<=tolerance && Math.Abs(window.right-info.monitor.right)<=tolerance && Math.Abs(window.bottom-info.monitor.bottom)<=tolerance;
  }
  public static void AltEnter(IntPtr hwnd) {
    SetForegroundWindow(hwnd); Thread.Sleep(250);
    keybd_event(0x12,0,0,UIntPtr.Zero); keybd_event(0x0D,0,0,UIntPtr.Zero);
    keybd_event(0x0D,0,2,UIntPtr.Zero); keybd_event(0x12,0,2,UIntPtr.Zero);
  }
  public static void LeftDoubleClick(IntPtr hwnd) {
    SetForegroundWindow(hwnd); Thread.Sleep(250);
    Rect client; if(!GetClientRect(hwnd,out client)) throw new Exception("GetClientRect failed");
    Point origin=new Point(); if(!ClientToScreen(hwnd,ref origin)) throw new Exception("ClientToScreen failed");
    SetCursorPos(origin.x+(client.right-client.left)/2,origin.y+(client.bottom-client.top)/2); Thread.Sleep(100);
    mouse_event(0x0002,0,0,0,UIntPtr.Zero); mouse_event(0x0004,0,0,0,UIntPtr.Zero); Thread.Sleep(100);
    mouse_event(0x0002,0,0,0,UIntPtr.Zero); mouse_event(0x0004,0,0,0,UIntPtr.Zero);
  }
  public static bool Close(IntPtr hwnd) { return PostMessage(hwnd,0x0010,IntPtr.Zero,IntPtr.Zero); }
}
'@
}
function Wait-FullscreenState([Diagnostics.Process]$Process,[IntPtr]$Handle,[bool]$Expected,[int]$Seconds) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($Process.HasExited) { throw 'MCLA exited during fullscreen transition.' }
        if ([MclaFullscreenNative]::IsFullscreen($Handle) -eq $Expected) { return }
        Start-Sleep -Milliseconds 100
    }
    throw "MCLA window did not reach fullscreen=$Expected."
}

& (Join-Path $PSScriptRoot 'verify-fullscreen-toggle-smoke.ps1') -SourceOnly | Out-Null
if (-not $SkipBuild) {
    Write-Host 'M7-016 FULLSCREEN [1/4]: clean-building the Release title...' -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath | Out-Null
    & cmake --preset win-amd64-release | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Release configure failed.' }
    & cmake --build --preset win-amd64-release --target mcla --clean-first --parallel 8 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Release build failed.' }
}
foreach ($required in @($exe,(Join-Path $game 'default.xex'))) { if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required fullscreen input is missing: '$required'." } }

$process = $null
$forced = $false
Write-Host 'M7-016 FULLSCREEN [2/4]: proving Alt+Enter windowed-to-fullscreen...' -ForegroundColor Cyan
try {
    $arguments = @(
        '--mcla_first_frame_probe=true','--mcla_first_frame_settle_seconds=35','--gpu_render_audit=false',
        '--async_shader_compilation=false','--d3d12_pipeline_creation_threads=0','--fullscreen=false',
        '--window_width=960','--window_height=540','--log_level=info','--log_max_file_size_mb=8','--log_max_files=4',
        "--game_data_root=`"$game`"","--user_data_root=`"$user`"","--cache_root=`"$cache`"","--log_file=`"$log`""
    )
    $process = Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $build -PassThru
    Wait-Log $process 'MCLA graphics: nontrivial guest frame captured' 90
    $handle = [MclaFullscreenNative]::Find($process.Id)
    if ($handle -eq [IntPtr]::Zero) { throw 'Exact visible MCLA window was not found.' }
    Wait-FullscreenState $process $handle $false 5
    [MclaFullscreenNative]::AltEnter($handle)
    Wait-Log $process 'source=alt-enter fullscreen=true' 15
    Wait-FullscreenState $process $handle $true 15

    Write-Host 'M7-016 FULLSCREEN [3/4]: proving LMB double-click fullscreen-to-windowed...' -ForegroundColor Cyan
    [MclaFullscreenNative]::LeftDoubleClick($handle)
    Wait-Log $process 'source=left-double-click fullscreen=false' 15
    Wait-FullscreenState $process $handle $false 15
    $text = Get-LogText
    $altCount = [regex]::Matches($text,'REX_WINDOW_FULLSCREEN_TOGGLE v=1 source=alt-enter fullscreen=true').Count
    $mouseCount = [regex]::Matches($text,'REX_WINDOW_FULLSCREEN_TOGGLE v=1 source=left-double-click fullscreen=false').Count
    if ($altCount -ne 1 -or $mouseCount -ne 1) { throw 'Fullscreen transition marker cardinality is invalid.' }
    if (-not [MclaFullscreenNative]::Close($handle)) { throw 'WM_CLOSE could not be posted.' }
    if (-not $process.WaitForExit(10000) -or $process.ExitCode -ne 0) { throw 'Controlled external close failed.' }
} catch {
    $failure = $_
    if ($process -and -not $process.HasExited) { $forced=$true; Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue; $null=$process.WaitForExit(5000) }
    if ($forced) { throw "Fullscreen failure required force cleanup. $($failure.Exception.Message) Private run: '$root'." }
    throw
}

$sdkVersion = (& git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim().TrimStart('v')
$sdkCommit = (& git -C $sdk rev-parse HEAD).Trim()
$result = [ordered]@{
    schema='mcla-runtime-fullscreen-toggle-v1'; task='M7-016'; decision='runtime-fullscreen-two-shortcuts-pass'; run_id=$runId
    sdk_version=$sdkVersion; sdk_commit=$sdkCommit; build_configuration='Release'; executable_sha256=(Get-Sha256 $exe)
    initial_windowed=$true; alt_enter_fullscreen=$true; left_double_click_windowed=$true
    alt_enter_marker_count=$altCount; left_double_click_marker_count=$mouseCount
    controlled_external_close=$true; exit_code=0; force_cleanup=$false; source_contract=$true
}
[IO.File]::WriteAllText($resultPath,(($result | ConvertTo-Json -Depth 5) + "`n"),$utf8)
Write-Host 'M7-016 FULLSCREEN [4/4]: revalidating the persisted machine result...' -ForegroundColor Cyan
$verified = & (Join-Path $PSScriptRoot 'verify-fullscreen-toggle-smoke.ps1') -ResultPath $resultPath
Write-Host "M7-016 FULLSCREEN PASS: Alt+Enter and LMB double-click both changed native window state. Result: '$resultPath'." -ForegroundColor Green
$verified
