[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-relwithdebinfo',
    [string]$GameRoot = 'private/game'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sdk = Join-Path $repo 'third_party\rexglue-sdk'
$verify = Join-Path $PSScriptRoot 'verify-locale-path-smoke.ps1'
$gameVerify = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$cmake = $toolchain.CMakePath
$utf8 = [Text.UTF8Encoding]::new($false)

if (-not ('MclaLocalePathWindow' -as [type])) {
    Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Runtime.InteropServices;using System.Text;
public static class MclaLocalePathWindow{delegate bool E(IntPtr h,IntPtr p);[DllImport("user32.dll")]static extern bool EnumWindows(E c,IntPtr p);[DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);[DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr h);[DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetWindowText(IntPtr h,StringBuilder s,int n);[DllImport("user32.dll")]static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);public static IntPtr[] Handles(int pid){var a=new List<IntPtr>();EnumWindows(delegate(IntPtr h,IntPtr x){uint p;GetWindowThreadProcessId(h,out p);if(p==(uint)pid&&IsWindowVisible(h))a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}public static string Title(IntPtr h){var s=new StringBuilder(1024);int n=GetWindowText(h,s,s.Capacity);return n>0?s.ToString(0,n):String.Empty;}public static bool Close(IntPtr h){return PostMessage(h,0x10,IntPtr.Zero,IntPtr.Zero);}}
'@
}

function Resolve-Safe([string]$Path, [string]$Description, [switch]$MustExist) {
    $full = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repo $Path }))
    $prefix = $repo.TrimEnd('\') + '\'
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes repository." }
    $current = $repo
    foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object Length)) {
        $current = Join-Path $current $part
        if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "$Description traverses a reparse point." }
    }
    if ($MustExist -and -not (Test-Path -LiteralPath $full)) { throw "$Description is missing." }
    $full
}

function Assert-Tree([string]$Root) {
    $pending = [Collections.Generic.Stack[string]]::new(); $pending.Push($Root)
    while ($pending.Count) { foreach ($item in @(Get-ChildItem -LiteralPath $pending.Pop() -Force)) { if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Tree contains a reparse point.' }; if ($item.PSIsContainer) { $pending.Push($item.FullName) } } }
}

function Get-Tree([string]$Root) {
    Assert-Tree $Root; $items = @(Get-ChildItem -LiteralPath $Root -Recurse -Force); $files = @($items | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName); $entries = @()
    foreach ($directory in @($items | Where-Object PSIsContainer | Sort-Object FullName)) { $entries += [ordered]@{ kind = 'directory'; path = $directory.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/') } }
    foreach ($file in $files) { $entries += [ordered]@{ kind = 'file'; path = $file.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/'); length = $file.Length; sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash } }
    $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4; $sha = [Security.Cryptography.SHA256]::Create(); try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
    $bytes = 0L; foreach ($file in $files) { $bytes += $file.Length }; [pscustomobject]@{ Hash = $hash; FileCount = $files.Count; DirectoryCount = @($items | Where-Object PSIsContainer).Count; Bytes = $bytes }
}

function Get-Game([string]$Root) {
    $tree = Get-Tree $Root; $checked = & $gameVerify -GamePath $Root -VerifyHashes
    [ordered]@{ file_count = $checked.FileCount; payload_bytes = $checked.PayloadBytes; hashes_verified = $checked.HashesVerified; manifest_sha256 = (Get-FileHash -LiteralPath $checked.ManifestPath -Algorithm SHA256).Hash; tree_sha256 = $tree.Hash; tree_file_count = $tree.FileCount; tree_directory_count = $tree.DirectoryCount; tree_bytes = $tree.Bytes }
}

function Get-Artifacts([string]$Root) {
    @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll') | ForEach-Object { $path = Resolve-Safe (Join-Path $Root $_) "Artifact $_" -MustExist; [ordered]@{ name = $_; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash } }
}

function Get-ExactProcesses([string]$Exe) { @((Get-Process mcla -ErrorAction SilentlyContinue) | Where-Object { try { [string]::Equals($_.Path, $Exe, [StringComparison]::OrdinalIgnoreCase) } catch { $false } }) }

function Test-Log([string]$Directory, [string]$Needle) {
    foreach ($file in @(Get-ChildItem -LiteralPath $Directory -File -Filter 'mcla*.log' -ErrorAction SilentlyContinue)) {
        try { $stream = [IO.File]::Open($file.FullName, 'Open', 'Read', 'ReadWrite'); try { $reader = [IO.StreamReader]::new($stream, $utf8, $true, 65536, $false); $text = $reader.ReadToEnd(); $reader.Dispose() } finally { $stream.Dispose() }; if ($text.Contains($Needle)) { return $true } } catch {}
    }
    $false
}

function Close-Exact([Diagnostics.Process]$Process) {
    $matches = @(); foreach ($handle in [MclaLocalePathWindow]::Handles($Process.Id)) { if ([regex]::IsMatch([MclaLocalePathWindow]::Title($handle), '^mcla \[rexglue-v[^\]]+\]$')) { $matches += $handle } }
    if ($matches.Count -ne 1 -or -not [MclaLocalePathWindow]::Close($matches[0])) { throw 'Exact PID/window WM_CLOSE failed.' }
}

function Invoke-Logged([scriptblock]$Command, [string]$Log, [switch]$Append) {
    $prior = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
    try { if ($Append) { & $Command *>&1 | Tee-Object $Log -Append | Out-Null } else { & $Command *>&1 | Tee-Object $Log | Out-Null }; $code = $LASTEXITCODE } finally { $ErrorActionPreference = $prior }
    $code
}

function From-CodePoints([int[]]$CodePoints) { -join @($CodePoints | ForEach-Object { [char]$_ }) }

$build = Resolve-Safe $BuildRoot 'Build root' -MustExist; $game = Resolve-Safe $GameRoot 'Game root' -MustExist
if ($build -cne (Resolve-Safe 'out/build/win-amd64-relwithdebinfo' 'Canonical build') -or $game -cne (Resolve-Safe 'private/game' 'Canonical game')) { throw 'M4-010 requires canonical inputs.' }
$evidence = Resolve-Safe 'private/evidence/M4-010' 'Evidence root'; [IO.Directory]::CreateDirectory($evidence) | Out-Null
$root = Join-Path $evidence ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)); $runs = Join-Path $root 'runs'; [IO.Directory]::CreateDirectory($runs) | Out-Null

Write-Host 'M4-010 [1/7]: validating canonical inputs and source-game integrity...' -ForegroundColor Cyan
$gameBefore = Get-Game $game; $sdkLog = Join-Path $root 'sdk-install.log'; $testLog = Join-Path $root 'sdk-locale-path-test.log'; $buildLog = Join-Path $root 'relwithdebinfo-clean-build.log'
Write-Host 'M4-010 [2/7]: clean-building and installing ReXGlue SDK...' -ForegroundColor Cyan
Push-Location $sdk; $oldCount = $env:GIT_CONFIG_COUNT; $oldKey = $env:GIT_CONFIG_KEY_0; $oldValue = $env:GIT_CONFIG_VALUE_0
try { $env:GIT_CONFIG_COUNT = '1'; $env:GIT_CONFIG_KEY_0 = 'safe.directory'; $env:GIT_CONFIG_VALUE_0 = $sdk.Replace('\', '/'); if (Invoke-Logged { & $cmake --preset win-amd64 -DREXGLUE_BUILD_TESTS=ON } $sdkLog) { throw 'SDK configure failed.' }; if (Invoke-Logged { & $cmake --build out/build/win-amd64 --config RelWithDebInfo --target install unit_tests --clean-first --parallel 8 } $sdkLog -Append) { throw 'SDK install failed.' } } finally { $env:GIT_CONFIG_COUNT = $oldCount; $env:GIT_CONFIG_KEY_0 = $oldKey; $env:GIT_CONFIG_VALUE_0 = $oldValue; Pop-Location }

Write-Host 'M4-010 [3/7]: running focused Unicode-path and locale tests...' -ForegroundColor Cyan
$unit = Resolve-Safe (Join-Path $sdk 'out/win-amd64/RelWithDebInfo/unit_tests.exe') 'Unit tests' -MustExist
if (Invoke-Logged { & $unit '[filesystem]' --order declared } $testLog) { throw 'Filesystem tests failed.' }; if (Invoke-Logged { & $unit '[locale]' --order declared } $testLog -Append) { throw 'Locale tests failed.' }
$focusedText = [IO.File]::ReadAllText($testLog, $utf8); if ($focusedText -notmatch 'All tests passed \(32 assertions in 3 test cases\)' -or $focusedText -notmatch 'All tests passed \(22 assertions in 3 test cases\)') { throw 'Focused test totals changed.' }

Write-Host 'M4-010 [4/7]: clean-building the MCLA host...' -ForegroundColor Cyan
if (Invoke-Logged { & $cmake --preset win-amd64-relwithdebinfo } $buildLog) { throw 'App configure failed.' }; if (Invoke-Logged { & $cmake --build --preset win-amd64-relwithdebinfo --target mcla --clean-first --parallel 8 } $buildLog -Append) { throw 'App clean build failed.' }
$exe = Resolve-Safe (Join-Path $build 'mcla.exe') 'MCLA executable' -MustExist; if (@(Get-ExactProcesses $exe).Count) { throw 'Canonical MCLA is already running.' }
$artifactsBefore = @(Get-Artifacts $build)
$localWord = From-CodePoints @(0x041B, 0x043E, 0x043A, 0x0430, 0x043B, 0x044C); $cacheWord = From-CodePoints @(0x043A, 0x0438, 0x0457, 0x0432); $accent = [char]0x00E9
$matrix = @([pscustomobject]@{ Name = '01-en-us'; Language = 1; Country = 103 }, [pscustomobject]@{ Name = '02-fr-fr'; Language = 4; Country = 34 }, [pscustomobject]@{ Name = '03-ru-ru'; Language = 12; Country = 88 })
$records = @()
Write-Host 'M4-010 [5/7]: running three isolated EN/FR/RU Unicode title cycles...' -ForegroundColor Cyan
foreach ($entry in $matrix) {
    Write-Host ("  {0}: language={1}, country={2}" -f $entry.Name, $entry.Language, $entry.Country) -ForegroundColor DarkCyan
    $cycle = Join-Path $runs $entry.Name; $user = Join-Path $cycle ('user-' + $localWord + '-' + $accent); $cache = Join-Path $cycle ('cache-' + $cacheWord); $logs = Join-Path $cycle ('logs-' + $localWord); [IO.Directory]::CreateDirectory($user) | Out-Null; [IO.Directory]::CreateDirectory($cache) | Out-Null; [IO.Directory]::CreateDirectory($logs) | Out-Null
    $log = Join-Path $logs 'mcla.log'; $bmp = Join-Path $user 'mcla-first-frame.bmp'; $process = $null; $forced = $false
    try {
        $process = Start-Process -FilePath $exe -ArgumentList @('--xam_locale_audit=true', ("--user_language=$($entry.Language)"), ("--user_country=$($entry.Country)"), '--mcla_first_frame_probe=true', '--mcla_first_frame_settle_seconds=35', '--gpu_render_audit=true', '--async_shader_compilation=false', '--render_target_path_d3d12=rtv', '--log_max_file_size_mb=5', '--log_max_files=15', '--log_level=trace', '--fullscreen=false', ("--game_data_root=$game"), ("--user_data_root=$user"), ("--cache_root=$cache"), ("--log_file=$log")) -WorkingDirectory $build -PassThru
        $deadline = [DateTime]::UtcNow.AddSeconds(100); while ([DateTime]::UtcNow -lt $deadline) { if ($process.HasExited) { throw 'Process exited before locale summary.' }; if ((Test-Path -LiteralPath $bmp) -and (Test-Log $logs 'LOCALE_AUDIT_SUMMARY v=1 phase=title status=PASS') -and (Test-Log $logs 'MCLA locale: title route summarized') -and (Test-Log $logs 'XENOS_AUDIT_PIPELINE_SUMMARY v=1 phase=checkpoint') -and (Test-Log $logs 'XENOS_AUDIT_CP_SUMMARY v=1 phase=checkpoint') -and (Test-Log $logs 'XENOS_AUDIT_RT_SUMMARY v=1 phase=checkpoint') -and (Test-Log $logs 'XENOS_AUDIT_RESOLVE_SUMMARY v=1 phase=checkpoint')) { break }; Start-Sleep -Milliseconds 500 }
        if (-not (Test-Path -LiteralPath $bmp) -or -not (Test-Log $logs 'LOCALE_AUDIT_SUMMARY v=1 phase=title status=PASS') -or -not (Test-Log $logs 'XENOS_AUDIT_RESOLVE_SUMMARY v=1 phase=checkpoint')) { throw 'Locale title and render-audit checkpoint deadline expired.' }
        Close-Exact $process; $exit = [Diagnostics.Stopwatch]::StartNew(); if (-not $process.WaitForExit(10000) -or $process.ExitCode -ne 0) { throw 'Controlled exit failed.' }; $exit.Stop(); if (@(Get-ExactProcesses $exe).Count) { throw 'MCLA process survived controlled exit.' }
    } catch { $errorRecord = $_; if ($process -and -not $process.HasExited) { $forced = $true; Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue; $null = $process.WaitForExit(5000) }; if ($forced) { throw "M4-010 failure required force cleanup. $($errorRecord.Exception.Message) Private run: '$root'." }; throw }
    if (-not (Test-Path -LiteralPath $user) -or -not (Test-Path -LiteralPath $cache) -or -not (Test-Path -LiteralPath $logs)) { throw 'Exact Unicode roots were not preserved.' }
    $probe = & $verify -ProbeOnly -RuntimeLogPath $log -BmpPath $bmp -Language $entry.Language -Country $entry.Country; $tree = Get-Tree $cycle
    $records += [ordered]@{ name = $entry.Name; language = $entry.Language; country = $entry.Country; language_calls = $probe.LanguageCalls; xget_language_reached = $false; country_reached = $false; title_visual_mode = $probe.Visual.Mode; logo_edge_correlation_ppm = $probe.Visual.LogoCorrelationPpm; prompt_reference_sha256 = $probe.Visual.PromptReferenceSha256; prompt_edge_correlation_ppm = $probe.Visual.PromptCorrelationPpm; unicode_user_path_exact = $true; unicode_cache_path_exact = $true; unicode_log_path_exact = $true; runtime_logs = @($probe.LogSet.Files); runtime_log_set_sha256 = $probe.LogSet.Hash; capture_sha256 = $probe.Bmp.Sha256; exit_elapsed_milliseconds = $exit.ElapsedMilliseconds; exit_code = 0; harness_force_cleanup = $false; cycle_tree_sha256 = $tree.Hash; cycle_file_count = $tree.FileCount; cycle_bytes = $tree.Bytes }
}

Write-Host 'M4-010 [6/7]: rehashing source game and runtime artifacts...' -ForegroundColor Cyan
$gameAfter = Get-Game $game; $artifactsAfter = @(Get-Artifacts $build); if (($gameBefore | ConvertTo-Json -Compress) -cne ($gameAfter | ConvertTo-Json -Compress)) { throw 'Source-game identity changed.' }; if (($artifactsBefore | ConvertTo-Json -Compress) -cne ($artifactsAfter | ConvertTo-Json -Compress)) { throw 'Runtime artifacts changed.' }
$result = [ordered]@{ schema = 1; task = 'M4-010'; decision = 'locale-selection-unicode-path-title-matrix-pass'; sdk_version = '0.9.0.17'; frontend_title_reached = $true; xget_language_title_reached = $false; country_title_reached = $false; scope = 'XConfig-language-title-route-plus-static-XGetLanguage-contract'; build = [ordered]@{ filesystem_test_cases = 3; filesystem_test_assertions = 32; locale_test_cases = 3; locale_test_assertions = 22; sdk_install_log_sha256 = (Get-FileHash -LiteralPath $sdkLog -Algorithm SHA256).Hash; focused_test_log_sha256 = (Get-FileHash -LiteralPath $testLog -Algorithm SHA256).Hash; app_build_log_sha256 = (Get-FileHash -LiteralPath $buildLog -Algorithm SHA256).Hash }; game_identity = [ordered]@{ before = $gameBefore; after = $gameAfter }; artifacts = [ordered]@{ before = $artifactsBefore; after = $artifactsAfter }; cycles = @($records); no_surviving_processes = $true; data_integrity_preserved = $true }
$resultPath = Join-Path $root 'result.json'; [IO.File]::WriteAllText($resultPath, (ConvertTo-Json $result -Depth 10) + [Environment]::NewLine, $utf8)
Write-Host 'M4-010 [7/7]: final physical/result verification...' -ForegroundColor Cyan
$final = & $verify -ResultPath $resultPath
[pscustomobject]@{ Passed = $final.Passed; Decision = $final.Decision; Cycles = $final.Cycles; Languages = $final.Languages; UnicodePathsVerified = $final.UnicodePathsVerified; XGetLanguageTitleReached = $final.XGetLanguageTitleReached; CountryTitleReached = $final.CountryTitleReached; DataIntegrityVerified = $final.DataIntegrityVerified; PrivateRunRoot = $root; ResultPath = $resultPath }
