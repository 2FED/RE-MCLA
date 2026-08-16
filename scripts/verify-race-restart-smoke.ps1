[CmdletBinding(DefaultParameterSetName = 'Run')]
param(
  [Parameter(Mandatory, ParameterSetName = 'Run')][string]$RunPath,
  [Parameter(Mandatory, ParameterSetName = 'Result')][string]$ResultPath,
  [switch]$Fixture
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$completedRoute = Join-Path $repo 'private/evidence/M5-012/20260816-132209-a316f851'
$completedSaveSha256 = '711B94303445034A3B127F83E8143FF6DBA3FC96C2B6176F8720E49829AD5021'
$saveRelative = 'user/B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
$headerRelative = 'user/B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'
$utf8 = [Text.UTF8Encoding]::new($false)

function Resolve-Safe([string]$Path, [string]$Description, [switch]$Exists) {
  $full = if ([IO.Path]::IsPathRooted($Path)) { [IO.Path]::GetFullPath($Path) } else { [IO.Path]::GetFullPath((Join-Path $repo $Path)) }
  $prefix = $repo.TrimEnd('\') + '\'
  if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes the repository." }
  $current = $repo
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object { $_.Length })) {
    $current = Join-Path $current $part
    if ((Test-Path -LiteralPath $current) -and ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "$Description traverses a reparse point." }
  }
  if ($Exists -and -not (Test-Path -LiteralPath $full)) { throw "$Description is missing." }
  $full
}

function Assert-NoReparse([string]$Root) {
  $pending = [Collections.Generic.Stack[string]]::new(); $pending.Push($Root)
  while ($pending.Count) {
    foreach ($item in @(Get-ChildItem -LiteralPath $pending.Pop() -Force)) {
      if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Evidence contains a reparse point.' }
      if ($item.PSIsContainer) { $pending.Push($item.FullName) }
    }
  }
}

function Get-TreeIdentity([string]$Root) {
  $rootPath = Resolve-Safe $Root 'Evidence tree' -Exists
  Assert-NoReparse $rootPath
  $entries = @(); $bytes = 0L
  foreach ($item in @(Get-ChildItem -LiteralPath $rootPath -Recurse -Force | Sort-Object FullName)) {
    $relative = $item.FullName.Substring($rootPath.Length).TrimStart('\').Replace('\', '/')
    if ($relative -ceq 'result.json') { continue }
    if ($item.PSIsContainer) { $entries += [ordered]@{ kind = 'directory'; path = $relative }; continue }
    $bytes += $item.Length
    $entries += [ordered]@{ kind = 'file'; path = $relative; bytes = $item.Length; sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash }
  }
  $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4
  $sha = [Security.Cryptography.SHA256]::Create()
  try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
  [pscustomobject]@{ sha256 = $hash; files = @($entries | Where-Object { $_.kind -ceq 'file' }).Count; directories = @($entries | Where-Object { $_.kind -ceq 'directory' }).Count; bytes = $bytes }
}

function Get-Logs([string]$Root) {
  $files = @(Get-ChildItem -LiteralPath $Root -File -Filter 'mcla*.log')
  if ($files.Count -lt 1 -or $files.Count -gt 16) { throw 'Runtime-log count is invalid.' }
  $current = @($files | Where-Object { $_.Name -ceq 'mcla.log' }); if ($current.Count -ne 1) { throw 'Current runtime log is missing or duplicated.' }
  $rotated = @()
  foreach ($file in @($files | Where-Object { $_.Name -cne 'mcla.log' })) {
    $match = [regex]::Match($file.Name, '^mcla\.([1-9][0-9]*)\.log$'); if (-not $match.Success) { throw 'Malformed log rotation.' }
    $rotated += [pscustomobject]@{ Index = [int]$match.Groups[1].Value; File = $file }
  }
  $indices = @($rotated | ForEach-Object { $_.Index } | Sort-Object)
  for ($i = 0; $i -lt $indices.Count; $i++) { if ($indices[$i] -ne $i + 1) { throw 'Log rotations are not contiguous.' } }
  $ordered = @($rotated | Sort-Object Index -Descending | ForEach-Object { $_.File }) + $current[0]
  $text = ''; $manifest = @(); $bytes = 0L
  foreach ($file in $ordered) {
    $bytes += $file.Length; if ($bytes -gt 134217728) { throw 'Runtime logs exceed 128 MiB.' }
    $text += [IO.File]::ReadAllText($file.FullName) + "`n"
    $manifest += [ordered]@{ name = $file.Name; bytes = $file.Length; sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
  }
  [pscustomobject]@{ Text = $text; Manifest = @($manifest); Bytes = $bytes }
}

function Get-One([string]$Text, [string]$Pattern, [string]$Description) {
  $matches = [regex]::Matches($Text, $Pattern)
  if ($matches.Count -ne 1) { throw "$Description count is $($matches.Count), expected 1." }
  $matches[0]
}

function Get-Bmp([string]$Path) {
  $resolved = Resolve-Safe $Path 'Restart BMP' -Exists
  $bytes = [IO.File]::ReadAllBytes($resolved)
  if ($bytes.Length -ne 3686454 -or $bytes[0] -ne 0x42 -or $bytes[1] -ne 0x4D -or [BitConverter]::ToInt32($bytes, 18) -ne 1280 -or [Math]::Abs([BitConverter]::ToInt32($bytes, 22)) -ne 720 -or [BitConverter]::ToUInt16($bytes, 28) -ne 32) { throw 'Restart capture is not canonical 1280x720 BGRA.' }
  [pscustomobject]@{ Bytes = $bytes; sha256 = (Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash }
}

function Get-SampledDifference($Left, $Right) {
  $count = 0
  for ($offset = 54; $offset -lt $Left.Bytes.Length; $offset += 16) {
    $delta = [Math]::Abs([int]$Left.Bytes[$offset] - [int]$Right.Bytes[$offset]) + [Math]::Abs([int]$Left.Bytes[$offset + 1] - [int]$Right.Bytes[$offset + 1]) + [Math]::Abs([int]$Left.Bytes[$offset + 2] - [int]$Right.Bytes[$offset + 2])
    if ($delta -gt 36) { $count++ }
  }
  $count
}

function Get-RestartProbe([string]$Path) {
  $root = Resolve-Safe $Path 'M5-012 restart cycle' -Exists; Assert-NoReparse $root
  $logs = Get-Logs $root; $text = $logs.Text
  foreach ($bad in @('[FATAL]', 'Assertion failed', 'PPC_UNIMPLEMENTED', 'Guest crash', 'DXGI_ERROR_DEVICE_REMOVED', 'MCLA physics timing: final sample failed')) { if ($text.Contains($bad)) { throw "Restart contains banned marker '$bad'." } }
  $config = Get-One $text 'MCLA_PHYSICS_TIMING_CONFIG v=1 slot=0 gameplay_wait_seconds=45 dismiss_pulses=6 dismiss_interval_ms=5000 sample_seconds=10 guest_tick_frequency=50000000 expected_vblank_millihz=60000 expected_present_millihz=30000' 'Timing config'
  $start = Get-One $text 'MCLA_PHYSICS_TIMING_FRAME v=1 phase=start width=1280 height=720 status=PASS' 'Start frame'
  $end = Get-One $text 'MCLA_PHYSICS_TIMING_FRAME v=1 phase=end width=1280 height=720 status=PASS' 'End frame'
  $timer = Get-One $text 'MCLA_PHYSICS_TIMER_SUMMARY v=1 calls=(\d+) records=(\d+) invalid_values=(\d+) effective_us_min=(\d+) effective_us_max=(\d+) clamped_us_min=(\d+) clamped_us_max=(\d+) raw_us_min=(\d+) raw_us_max=(\d+)' 'Timer summary'
  $sample = Get-One $text 'MCLA_PHYSICS_TIMING_SAMPLE v=1 host_us=(\d+) guest_ticks=(\d+) guest_host_ratio_ppm=(\d+) vblank_delta=(\d+) vblank_millihz=(\d+) present_delta=(\d+) present_millihz=(\d+) present_to_vblank_ppm=(\d+) simulated_time_to_wall_ppm=(\d+)' 'Timing sample'
  $summary = Get-One $text 'MCLA_PHYSICS_TIMING_SUMMARY v=1 status=COMPLETE samples=1 frames=2 gameplay_input_records=8 external_close_required=1' 'Timing summary'
  $closing = Get-One $text 'Window closing, shutting down\.\.\.' 'Window close'; $complete = Get-One $text 'Execution complete' 'Execution complete'; $hard = Get-One $text 'Title terminated; hard-exiting process\.' 'Hard exit'
  if (-not ($config.Index -lt $start.Index -and $start.Index -lt $end.Index -and $end.Index -lt $timer.Index -and $timer.Index -lt $sample.Index -and $sample.Index -lt $summary.Index -and $summary.Index -lt $closing.Index -and $closing.Index -lt $complete.Index -and $complete.Index -lt $hard.Index)) { throw 'Restart chronology is invalid.' }
  $records = [regex]::Matches($text, 'MCLA_PHYSICS_TIMER_RECORD v=1 id=(\d+) effective_bits=([0-9A-F]{8}) clamped_bits=([0-9A-F]{8}) raw_bits=([0-9A-F]{8})')
  if ($records.Count -ne 16 -or [regex]::Matches($text, 'MCLA_GAMEPLAY_INPUT v=1').Count -ne 8) { throw 'Restart record cardinality is invalid.' }
  for ($i = 0; $i -lt 16; $i++) { if ([int]$records[$i].Groups[1].Value -ne $i -or $records[$i].Groups[2].Value -cne '3D088889' -or $records[$i].Groups[3].Value -cne '3D088889') { throw 'Fixed-step record is invalid.' } }
  $calls = [int64]$timer.Groups[1].Value; $invalid = [int64]$timer.Groups[3].Value
  $hostUs = [int64]$sample.Groups[1].Value; $guestRatio = [int]$sample.Groups[3].Value; $vblankDelta = [int]$sample.Groups[4].Value; $vblankRate = [int]$sample.Groups[5].Value; $presentDelta = [int]$sample.Groups[6].Value; $presentRate = [int]$sample.Groups[7].Value; $simulatedRatio = [int]$sample.Groups[9].Value
  if ($calls -lt 294 -or $calls -gt 306 -or $invalid -ne 0 -or [int]$timer.Groups[4].Value -ne 33333 -or [int]$timer.Groups[5].Value -ne 33333 -or [int]$timer.Groups[6].Value -ne 33333 -or [int]$timer.Groups[7].Value -ne 33333) { throw 'Restart fixed-step measurements are invalid.' }
  if ($hostUs -lt 9900000 -or $hostUs -gt 10100000 -or $guestRatio -lt 999000 -or $guestRatio -gt 1001000 -or $vblankDelta -lt 594 -or $vblankDelta -gt 606 -or $vblankRate -lt 59400 -or $vblankRate -gt 60600 -or $presentDelta -lt 294 -or $presentDelta -gt 306 -or $presentRate -lt 29400 -or $presentRate -gt 30600 -or $simulatedRatio -lt 980000 -or $simulatedRatio -gt 1020000) { throw 'Restart Release timing is outside stock bounds.' }
  $save = Resolve-Safe (Join-Path $root $saveRelative) 'Completed save' -Exists; Resolve-Safe (Join-Path $root $headerRelative) 'Completed save header' -Exists | Out-Null
  if ((Get-FileHash -LiteralPath $save -Algorithm SHA256).Hash -cne $completedSaveSha256) { throw 'Completed save changed during restart verification.' }
  $startBmp = Get-Bmp (Join-Path $root 'user/mcla-physics-start.bmp'); $endBmp = Get-Bmp (Join-Path $root 'user/mcla-physics-end.bmp'); $difference = Get-SampledDifference $startBmp $endBmp
  if ($difference -lt 20000) { throw 'Restart gameplay frames are insufficiently distinct.' }
  [pscustomobject][ordered]@{ decision = 'completed-save-release-restart-pass'; present_millihz = $presentRate; present_delta = $presentDelta; vblank_millihz = $vblankRate; timer_calls = $calls; simulated_time_to_wall_ppm = $simulatedRatio; sampled_frame_difference = $difference; start_sha256 = $startBmp.sha256; end_sha256 = $endBmp.sha256; save_sha256 = $completedSaveSha256; runtime_logs = @($logs.Manifest); runtime_log_bytes = $logs.Bytes; controlled_exit = $true }
}

if ($PSCmdlet.ParameterSetName -ceq 'Run') {
  $probe = Get-RestartProbe $RunPath
  if (-not $Fixture) {
    $root = Resolve-Safe $RunPath 'Restart cycle' -Exists
    $dirs = @(Get-ChildItem -LiteralPath $root -Directory -Force | ForEach-Object { $_.Name } | Sort-Object)
    $extra = @(Get-ChildItem -LiteralPath $root -File -Force | Where-Object { $_.Name -notmatch '^mcla(?:\.[1-9][0-9]*)?\.log$' })
    if (($dirs -join ',') -cne 'cache,user' -or $extra.Count) { throw 'Restart cycle topology is invalid.' }
  }
  $probe; return
}

$result = Resolve-Safe $ResultPath 'M5-012 restart result' -Exists
$record = Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
$properties = @('schema', 'task', 'decision', 'sdk_version', 'sdk_commit', 'build_configuration', 'completed_route_run_id', 'completed_route_tree', 'completed_save_sha256', 'restart_probe', 'release_artifacts', 'restart_tree', 'scope')
if (($record.PSObject.Properties.Name -join ',') -cne ($properties -join ',') -or $record.schema -cne 'mcla-race-restart-v1' -or $record.task -cne 'M5-012' -or $record.decision -cne 'first-series-results-return-and-release-restart-pass' -or $record.sdk_version -cne '0.9.0.21' -or $record.sdk_commit -cne '3ef5b4f143d56b57e3c0e539cb0009ffe3a67e05' -or $record.build_configuration -cne 'Release' -or $record.completed_route_run_id -cne '20260816-132209-a316f851' -or $record.completed_save_sha256 -cne $completedSaveSha256) { throw 'Restart result identity is invalid.' }
$routeProbe = & (Join-Path $PSScriptRoot 'verify-race-results-smoke.ps1') -RunPath (Join-Path $completedRoute 'runs/01')
if (-not $routeProbe.controlled_exit -or @($routeProbe.checkpoint_captures).Count -ne 3) { throw 'Completed route no longer verifies.' }
$routeTree = Get-TreeIdentity $completedRoute
if ((ConvertTo-Json $record.completed_route_tree -Compress) -cne (ConvertTo-Json $routeTree -Compress)) { throw 'Completed route identity drifted.' }
$root = Split-Path $result -Parent; $probe = Get-RestartProbe (Join-Path $root 'runs/01')
if ((ConvertTo-Json $record.restart_probe -Compress -Depth 7) -cne (ConvertTo-Json $probe -Compress -Depth 7)) { throw 'Restart probe does not match physical evidence.' }
$tree = Get-TreeIdentity $root
if ((ConvertTo-Json $record.restart_tree -Compress) -cne (ConvertTo-Json $tree -Compress)) { throw 'Restart tree identity drifted.' }
$artifacts = @('mcla.exe', 'rexruntime.dll', 'TracyClient.dll', 'rexgpu-xenos.dll') | ForEach-Object { $path = Resolve-Safe (Join-Path 'out/build/win-amd64-release' $_) "Release artifact $_" -Exists; [pscustomobject][ordered]@{ name = $_; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash } }
if ((ConvertTo-Json $record.release_artifacts -Compress) -cne (ConvertTo-Json $artifacts -Compress)) { throw 'Current Release artifacts drifted.' }
if ($record.scope -cne 'one operator-confirmed Ian event series reached final rewards/results and controllable free roam; the changed completed save then loaded in a fresh optimized Release process that sustained the stock 30-FPS timing sample; five repeated race/resource checks remain M5-013') { throw 'Restart result scope is invalid.' }
$sdk = Join-Path $repo 'third_party/rexglue-sdk'; $sdkHead = (& git -C $sdk rev-parse HEAD).Trim(); $sdkTag = (& git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim()
if ($LASTEXITCODE -ne 0 -or $sdkHead -cne $record.sdk_commit -or $sdkTag -cne 'v0.9.0.21' -or (git -C $sdk status --porcelain)) { throw 'Current SDK identity drifted.' }
$releaseExe = Resolve-Safe 'out/build/win-amd64-release/mcla.exe' 'Release executable' -Exists
if (@((Get-Process mcla -ErrorAction SilentlyContinue) | Where-Object { try { [string]::Equals($_.Path, $releaseExe, [StringComparison]::OrdinalIgnoreCase) } catch { $false } }).Count) { throw 'Canonical Release MCLA process is still running.' }
[pscustomobject][ordered]@{ Decision = $record.decision; FullRouteVerified = $true; ResultSaveChanged = $true; RestartVerified = $true; Release30FpsVerified = $true; ControlledExitVerified = $true; DataIntegrityVerified = $true }
