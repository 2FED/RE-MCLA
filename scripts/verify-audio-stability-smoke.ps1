[CmdletBinding(DefaultParameterSetName = 'Run')]
param(
  [Parameter(Mandatory, ParameterSetName = 'Run')][string]$RunPath,
  [Parameter(Mandatory, ParameterSetName = 'Soak')][string]$LongSoakPath,
  [Parameter(Mandatory, ParameterSetName = 'Result')][string]$ResultPath,
  [switch]$Fixture
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$utf8 = [Text.UTF8Encoding]::new($false)

function Resolve-Safe([string]$Path, [string]$Description) {
  $full = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repo $Path }))
  $prefix = $repo.TrimEnd('\') + '\'
  if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes the repository." }
  $current = $repo
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object Length)) {
    $current = Join-Path $current $part
    if (-not (Test-Path -LiteralPath $current)) { throw "$Description is missing." }
    if ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "$Description traverses a reparse point." }
  }
  $full
}

function Get-LogSet([string]$Directory) {
  $directory = Resolve-Safe $Directory 'Audio run directory'
  $files = @(Get-ChildItem -LiteralPath $directory -File -Filter 'mcla*.log')
  $current = $files | Where-Object Name -CEQ 'mcla.log'
  $rotations = @()
  foreach ($file in @($files | Where-Object Name -CNE 'mcla.log')) {
    $match = [regex]::Match($file.Name, '^mcla\.([1-9][0-9]*)\.log$')
    if (-not $match.Success) { throw 'Malformed runtime-log rotation.' }
    $rotations += [pscustomobject]@{ Number = [int]$match.Groups[1].Value; File = $file }
  }
  if (@($current).Count -ne 1 -or $files.Count -gt 16) { throw 'Runtime-log topology is invalid.' }
  $ids = @($rotations | ForEach-Object Number | Sort-Object)
  for ($i = 0; $i -lt $ids.Count; ++$i) { if ($ids[$i] -ne $i + 1) { throw 'Runtime-log rotations are not contiguous.' } }
  $ordered = @($rotations | Sort-Object Number -Descending | ForEach-Object File) + @($current)
  $text = ''; $manifest = @(); $bytes = 0L
  foreach ($file in $ordered) {
    if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Runtime log is a reparse point.' }
    $bytes += $file.Length
    if ($bytes -gt 268435456) { throw 'Runtime-log set exceeds 256 MiB.' }
    $text += [IO.File]::ReadAllText($file.FullName) + "`n"
    $manifest += [ordered]@{ name = $file.Name; bytes = [long]$file.Length; sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
  }
  $manifestJson = ConvertTo-Json -InputObject @($manifest) -Compress -Depth 4
  $sha = [Security.Cryptography.SHA256]::Create()
  try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($manifestJson)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
  [pscustomobject]@{ Text = $text; Files = @($manifest); Count = $manifest.Count; Bytes = $bytes; Hash = $hash }
}

function One([string]$Pattern, [string]$Text, [string]$Name) {
  $Pattern = $Pattern.Replace('\s*$', '[ \t\r]*$')
  $matches = [regex]::Matches($Text, $Pattern, [Text.RegularExpressions.RegexOptions]::Multiline)
  if ($matches.Count -ne 1) { throw "$Name must occur exactly once." }
  $matches[0]
}

function Number($Match, [string]$Name) { [uint64]$Match.Groups[$Name].Value }
function Growth([long]$Start, [long]$End) { [Math]::Max(0L, $End - $Start) }
function Peak-Growth($Samples, [string]$Property, [long]$Start) { Growth $Start ([long](($Samples | Measure-Object -Property $Property -Maximum).Maximum)) }

function Game-Record($Probe) {
  [ordered]@{
    valid = [bool]$Probe.Valid; file_count = [int]$Probe.FileCount; payload_bytes = [long]$Probe.PayloadBytes
    rpf_count = [int]$Probe.RpfCount; rpf_bytes = [long]$Probe.RpfBytes; bik_count = [int]$Probe.BikCount
    bik_bytes = [long]$Probe.BikBytes; hashes_verified = [int]$Probe.HashesVerified; source_iso_sha256 = [string]$Probe.SourceIsoSha256
  }
}

function Assert-Lifecycle([string]$Text) {
  $close = One '^.*Window closing, shutting down\.\.\.[ \t\r]*$' $Text 'Window-close marker'
  $complete = One '^.*Execution complete[ \t\r]*$' $Text 'Execution-complete marker'
  $hard = One '^.*Title terminated; hard-exiting process\.[ \t\r]*$' $Text 'Hard-exit marker'
  if ($complete.Index -le $close.Index -or $hard.Index -le $complete.Index) { throw 'Controlled external-close lifecycle is reordered.' }
  if ($Text -match '(?i)(PPC_UNIMPLEMENTED|Guest crash|DRED|device removed|\bFATAL\b)') { throw 'Runtime log contains a fatal/device-loss marker.' }
}

function Get-Bmp([string]$Path) {
  $path = Resolve-Safe $Path 'Audio title frame'
  $bytes = [IO.File]::ReadAllBytes($path)
  if ($bytes.Length -ne 3686454 -or $bytes[0] -ne 0x42 -or $bytes[1] -ne 0x4D -or [BitConverter]::ToInt32($bytes, 18) -ne 1280 -or [BitConverter]::ToInt32($bytes, 22) -ne 720 -or [BitConverter]::ToUInt16($bytes, 28) -ne 32) { throw 'Audio title frame format is invalid.' }
  [pscustomobject]@{ Sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash; Bytes = $bytes.Length }
}

function Get-Tree([string]$Root) {
  $items = @(Get-ChildItem -LiteralPath $Root -Recurse -Force)
  $entries = @(); $bytes = 0L
  foreach ($directory in @($items | Where-Object PSIsContainer | Sort-Object FullName)) { if ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Evidence tree contains a reparse point.' }; $entries += [ordered]@{ kind = 'directory'; path = $directory.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/') } }
  foreach ($file in @($items | Where-Object { -not $_.PSIsContainer -and $_.Name -cne 'result.json' } | Sort-Object FullName)) { if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Evidence tree contains a reparse point.' }; $bytes += $file.Length; $entries += [ordered]@{ kind = 'file'; path = $file.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/'); bytes = [long]$file.Length; sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash } }
  $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4; $sha = [Security.Cryptography.SHA256]::Create(); try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
  [pscustomobject]@{ Hash = $hash; FileCount = @($entries | Where-Object kind -CEQ 'file').Count; DirectoryCount = @($entries | Where-Object kind -CEQ 'directory').Count; Bytes = $bytes }
}

function Get-RouteSummary([string]$Text) {
  $config = One '^.*SDL_AUDIO_AUDIT_CONFIG v=1 enabled=1 backend=sdl sample_rate=48000 source_channels=6\s*$' $Text 'SDL audio-route config'
  $client = One '^.*SDL_AUDIO_AUDIT_CLIENT v=1 event=register result=00000000 index_class=bounded\s*$' $Text 'SDL audio client'
  $frames = [regex]::Matches($Text, '^.*SDL_AUDIO_AUDIT_FRAME v=1 layer=(?<layer>submit|device|xma) class=(?:silence|nonzero) finite=1 channels=(?<channels>\d+) peak_ppm=\d+[ \t\r]*$', [Text.RegularExpressions.RegexOptions]::Multiline)
  if ($frames.Count -ne 3 -or @($frames | ForEach-Object { $_.Groups['layer'].Value } | Sort-Object -Unique).Count -ne 3) { throw 'Audio route must contain one bounded record per layer.' }
  $pattern = '^.*SDL_AUDIO_AUDIT_SUMMARY v=1 phase=title status=PASS client_calls=(?<cc>\d+) client_success=(?<cs>\d+) submit_frames=(?<sf>\d+) submit_nonzero=(?<sn>\d+) submit_invalid=(?<si>\d+) submit_peak_ppm=(?<sp>\d+) device_frames=(?<df>\d+) device_nonzero=(?<dn>\d+) device_invalid=(?<di>\d+) device_submit_fail=(?<fail>\d+) device_peak_ppm=(?<dp>\d+) xma_frames=(?<xf>\d+) xma_nonzero=(?<xn>\d+) xma_invalid=(?<xi>\d+) xma_peak_ppm=(?<xp>\d+) max_queue_depth=(?<qd>\d+) starvation_fills=(?<st>\d+) max_consecutive_starvation_fills=(?<mc>\d+) dropped_records=(?<drop>\d+)\s*$'
  $summary = One $pattern $Text 'SDL audio-route summary'
  if (-not ($config.Index -lt $client.Index -and $client.Index -lt $summary.Index)) { throw 'Audio-route config/client/summary chronology is invalid.' }
  foreach ($name in @('si', 'di', 'fail', 'xi', 'drop')) { if ((Number $summary $name) -ne 0) { throw "Audio-route failure counter '$name' is nonzero." } }
  foreach ($name in @('cs', 'sf', 'sn', 'df', 'dn', 'xf', 'xn')) { if ((Number $summary $name) -lt 1) { throw "Audio-route counter '$name' is empty." } }
  if ((Number $summary qd) -gt 64 -or (Number $summary mc) -gt 2) { throw 'Audio queue/starvation bound failed.' }
  [ordered]@{ submit_frames = Number $summary sf; submit_nonzero = Number $summary sn; device_frames = Number $summary df; device_nonzero = Number $summary dn; xma_frames = Number $summary xf; xma_nonzero = Number $summary xn; max_queue_depth = Number $summary qd; starvation_fills = Number $summary st; max_consecutive_starvation_fills = Number $summary mc }
}

function Get-LongSoak([string]$Path) {
  $root = Resolve-Safe $Path 'Long-soak run'
  $logs = Get-LogSet (Join-Path $root 'runs\soak')
  $text = $logs.Text
  $start = One '^.*MCLA audio: title soak started seconds 7200\s*$' $text 'Two-hour soak start'
  $summaryMarker = One '^.*SDL_AUDIO_AUDIT_SUMMARY v=1 phase=title status=PASS .*$' $text 'Two-hour audio summary'
  $done = One '^.*MCLA audio: title soak completed seconds 7200\s*$' $text 'Two-hour soak completion'
  $capture = One '^.*MCLA graphics: nontrivial guest frame captured .*$' $text 'Two-hour title capture'
  if (-not ($capture.Index -lt $start.Index -and $start.Index -lt $summaryMarker.Index -and $summaryMarker.Index -lt $done.Index)) { throw 'Two-hour soak chronology is invalid.' }
  Assert-Lifecycle $text
  $route = Get-RouteSummary $text
  $bmp = Get-Bmp (Join-Path $root 'runs\soak\user\mcla-first-frame.bmp')
  $samplePath = Resolve-Safe (Join-Path $root 'resource-samples.json') 'Two-hour resource samples'
  $samples = @([IO.File]::ReadAllText($samplePath) | ConvertFrom-Json)
  $properties = @('checkpoint', 'elapsed_seconds', 'private_bytes', 'working_set_bytes', 'handle_count', 'thread_count')
  if ($samples.Count -ne 13) { throw 'Two-hour resource evidence must contain baseline plus twelve ten-minute samples.' }
  for ($i = 0; $i -lt 13; ++$i) {
    $sample = $samples[$i]
    if (($sample.PSObject.Properties.Name -join ',') -cne ($properties -join ',') -or $sample.checkpoint -ne $i) { throw 'Two-hour resource sample schema or checkpoint order is invalid.' }
    foreach ($property in $properties[1..5]) { if ($sample.$property -isnot [int] -and $sample.$property -isnot [long]) { throw "Two-hour resource sample '$property' is not integral." }; if ([long]$sample.$property -lt 0) { throw "Two-hour resource sample '$property' is negative." } }
    $expectedElapsed = $i * 600
    if (($i -eq 0 -and $sample.elapsed_seconds -gt 5) -or ($i -gt 0 -and ($sample.elapsed_seconds -lt $expectedElapsed -or $sample.elapsed_seconds -gt $expectedElapsed + 5))) { throw 'Two-hour resource sample cadence is invalid.' }
  }
  $baseline = $samples[0]; $last = $samples[12]; $MiB = 1048576L
  $finalGrowth = [ordered]@{ private_bytes = Growth $baseline.private_bytes $last.private_bytes; working_set_bytes = Growth $baseline.working_set_bytes $last.working_set_bytes; handle_count = Growth $baseline.handle_count $last.handle_count; thread_count = Growth $baseline.thread_count $last.thread_count }
  $peakGrowth = [ordered]@{ private_bytes = Peak-Growth $samples private_bytes $baseline.private_bytes; working_set_bytes = Peak-Growth $samples working_set_bytes $baseline.working_set_bytes; handle_count = Peak-Growth $samples handle_count $baseline.handle_count; thread_count = Peak-Growth $samples thread_count $baseline.thread_count }
  # The animated title route warms shader/pipeline caches after the baseline;
  # the accepted two-hour calibration peaks below 1 GiB private growth while
  # resident working-set growth remains below 512 MiB.
  $thresholds = [ordered]@{ private_bytes = 1024 * $MiB; working_set_bytes = 512 * $MiB; handle_count = 128; thread_count = 16 }
  foreach ($property in @('private_bytes', 'working_set_bytes', 'handle_count', 'thread_count')) { if ($peakGrowth[$property] -gt $thresholds[$property]) { throw "Two-hour peak resource growth exceeds the '$property' bound." } }
  $resources = [ordered]@{ sample_count = 13; interval_seconds = 600; samples_sha256 = (Get-FileHash -LiteralPath $samplePath -Algorithm SHA256).Hash; final_growth = $finalGrowth; peak_growth = $peakGrowth; thresholds = $thresholds }
  [pscustomobject]@{ Passed = $true; RunId = Split-Path $root -Leaf; LogSet = $logs; CaptureSha256 = $bmp.Sha256; Route = $route; Resources = $resources; SoakSeconds = 7200; ControlledExit = $true }
}

function Get-StabilityRun([string]$Path) {
  $root = Resolve-Safe $Path 'Audio-stability run'
  $logs = Get-LogSet (Join-Path $root 'runs\stability')
  $text = $logs.Text
  $sdkConfig = One '^.*SDL_AUDIO_STABILITY_CONFIG v=1 enabled=1 backend=sdl default_device=1 pause_cycles=2 device_identity=redacted\s*$' $text 'Stability config'
  $driver = One '^.*SDL_AUDIO_STABILITY_DRIVER v=1 event=open result=00000000 source_channels=(?:2|6)\s*$' $text 'SDL driver open'
  $appConfig = One '^.*MCLA_AUDIO_STABILITY_CONFIG v=1 pause_cycles=2 pause_ms=2000 recovery_ms=5000 device_switch=external identity=redacted\s*$' $text 'App stability config'
  $appProbes = [regex]::Matches($text, '^.*MCLA_AUDIO_STABILITY_PROBE v=1 cycle=(?<cycle>[12]) event=(?<event>PAUSE|RESUME)[ \t\r]*$', [Text.RegularExpressions.RegexOptions]::Multiline)
  $states = [regex]::Matches($text, '^.*SDL_AUDIO_STABILITY_STATE v=1 event=(?<event>pause|resume) result=00000000 order_valid=1 cycle=(?<cycle>[12])[ \t\r]*$', [Text.RegularExpressions.RegexOptions]::Multiline)
  $recoveries = [regex]::Matches($text, '^.*SDL_AUDIO_STABILITY_RECOVERY v=1 source=pause cycle=(?<cycle>[12]) nonzero=1 result=00000000[ \t\r]*$', [Text.RegularExpressions.RegexOptions]::Multiline)
  if ($appProbes.Count -ne 4 -or $states.Count -ne 4 -or $recoveries.Count -ne 2) { throw 'Exact two-cycle pause/resume evidence is missing.' }
  for ($i = 0; $i -lt 4; ++$i) {
    $expectedEvent = if (($i % 2) -eq 0) { 'pause' } else { 'resume' }
    $expectedAppEvent = $expectedEvent.ToUpperInvariant()
    $expectedCycle = [string]([Math]::Floor($i / 2) + 1)
    if ($appProbes[$i].Groups['event'].Value -cne $expectedAppEvent -or $appProbes[$i].Groups['cycle'].Value -cne $expectedCycle -or $states[$i].Groups['event'].Value -cne $expectedEvent -or $states[$i].Groups['cycle'].Value -cne $expectedCycle -or $appProbes[$i].Index -ge $states[$i].Index) { throw 'Pause/resume state order is invalid.' }
  }
  for ($cycle = 0; $cycle -lt 2; ++$cycle) {
    $pauseIndex = $cycle * 2
    if ($recoveries[$cycle].Groups['cycle'].Value -cne [string]($cycle + 1) -or -not ($appProbes[$pauseIndex].Index -lt $states[$pauseIndex].Index -and $states[$pauseIndex].Index -lt $appProbes[$pauseIndex + 1].Index -and $appProbes[$pauseIndex + 1].Index -lt $states[$pauseIndex + 1].Index -and $states[$pauseIndex + 1].Index -lt $recoveries[$cycle].Index)) { throw 'Pause/resume recovery causality is invalid.' }
  }
  $sdkReady = One '^.*SDL_AUDIO_STABILITY_READY v=1 phase=device-switch pause_recoveries=2 output_nonzero=(?<output>\d+) status=READY\s*$' $text 'SDK device-switch readiness'
  $appReady = One '^.*MCLA_AUDIO_STABILITY_READY v=1 phase=device-switch status=READY external_confirmation_required=1\s*$' $text 'App device-switch readiness'
  $devices = [regex]::Matches($text, '^.*SDL_AUDIO_STABILITY_DEVICE v=1 event=(?<event>added|removed|format-changed|migrated) playback=1 sequence=(?<seq>\d+)[ \t\r]*$', [Text.RegularExpressions.RegexOptions]::Multiline)
  $deviceRecoveries = [regex]::Matches($text, '^.*SDL_AUDIO_STABILITY_RECOVERY v=1 source=device cycle=(?<cycle>\d+) nonzero=1 result=00000000[ \t\r]*$', [Text.RegularExpressions.RegexOptions]::Multiline)
  if ($devices.Count -lt 1 -or $devices.Count -gt 4 -or @($devices | ForEach-Object { $_.Groups['event'].Value } | Sort-Object -Unique).Count -ne $devices.Count -or $deviceRecoveries.Count -lt 1 -or $deviceRecoveries.Count -gt 16) { throw 'Post-arm playback-device evidence is unbounded, duplicated, or missing.' }
  if ('migrated' -cnotin @($devices | ForEach-Object { $_.Groups['event'].Value })) { throw 'Default playback endpoint migration was not machine-observed.' }
  $previousSequence = 0
  foreach ($device in $devices) { $sequence = Number $device seq; if ($sequence -le $previousSequence) { throw 'Playback-device event sequence is not increasing.' }; $previousSequence = $sequence }
  for ($i = 0; $i -lt $deviceRecoveries.Count; ++$i) { if ((Number $deviceRecoveries[$i] cycle) -ne $i + 1) { throw 'Playback-device recovery sequence is not contiguous.' } }
  $confirm = One '^.*MCLA_AUDIO_STABILITY_CONFIRM v=1 machine_recovered=1 operator_heard=1 identity=redacted\s*$' $text 'Owner recovery confirmation'
  $summaryPattern = '^.*SDL_AUDIO_STABILITY_SUMMARY v=1 phase=title status=PASS driver_calls=(?<dc>\d+) driver_success=(?<ds>\d+) output_nonzero=(?<on>\d+) pause_calls=2 pause_success=2 resume_calls=2 resume_success=2 resume_recoveries=2 device_events=(?<de>\d+) device_recoveries=(?<dr>\d+) failures=0 dropped_records=0\s*$'
  $summary = One $summaryPattern $text 'SDL stability summary'
  $xmpSummary = One '^.*XMP_AUDIT_SUMMARY v=1 phase=title status=PASS calls=(?<calls>\d+) known_calls=(?<known>\d+) query_calls=(?<query>\d+) playback_calls=0 unsupported_calls=0 state_changes=0 unexpected_calls=0 inconsistent_calls=0 records=1 overflow=0 dropped_records=0\s*$' $text 'XMP title summary'
  $appSummary = One '^.*MCLA_AUDIO_STABILITY_SUMMARY v=1 status=COMPLETE pause_cycles=2 device_switch=external prior_routes_bound=1\s*$' $text 'App stability summary'
  $summaryDeviceEvents = Number $summary de; $summaryDeviceRecoveries = Number $summary dr
  if ((Number $sdkReady output) -lt 1 -or (Number $summary dc) -ne 1 -or (Number $summary ds) -ne 1 -or $summaryDeviceEvents -lt $devices.Count -or $summaryDeviceEvents -gt 16 -or $summaryDeviceRecoveries -ne $deviceRecoveries.Count -or (Number $xmpSummary calls) -lt 1 -or (Number $xmpSummary calls) -ne (Number $xmpSummary known) -or (Number $xmpSummary calls) -ne (Number $xmpSummary query)) { throw 'Stability summary lacks bounded driver/output/device/XMP recovery.' }
  $lastDeviceIndex = $devices[$devices.Count - 1].Index; $lastDeviceRecoveryIndex = $deviceRecoveries[$deviceRecoveries.Count - 1].Index
  if (-not ($sdkConfig.Index -lt $driver.Index -and $driver.Index -lt $appConfig.Index -and $appConfig.Index -lt $states[0].Index -and $recoveries[1].Index -lt $sdkReady.Index -and $sdkReady.Index -lt $appReady.Index -and $appReady.Index -lt $devices[0].Index -and $lastDeviceIndex -lt $lastDeviceRecoveryIndex -and $lastDeviceRecoveryIndex -lt $confirm.Index -and $confirm.Index -lt $summary.Index -and $summary.Index -lt $appSummary.Index)) { throw 'Audio-stability chronology is invalid.' }
  if ([regex]::IsMatch($text, '^.*SDL_AUDIO_STABILITY_(?:STATE|DEVICE|RECOVERY|READY|SUMMARY).*status=FAIL.*$', [Text.RegularExpressions.RegexOptions]::Multiline)) { throw 'Stability audit contains a failure marker.' }
  $allSdkMarkers = [regex]::Matches($text, '^.*SDL_AUDIO_STABILITY_[A-Z_]+.*[ \t\r]*$', [Text.RegularExpressions.RegexOptions]::Multiline)
  if ($allSdkMarkers.Count -ne 10 + $devices.Count + $deviceRecoveries.Count) { throw 'Unknown or duplicate SDL audio-stability marker is present.' }
  $allAppMarkers = [regex]::Matches($text, '^.*MCLA_AUDIO_STABILITY_[A-Z_]+.*[ \t\r]*$', [Text.RegularExpressions.RegexOptions]::Multiline)
  if ($allAppMarkers.Count -ne 8) { throw 'Unknown or duplicate app audio-stability marker is present.' }
  Assert-Lifecycle $text
  $route = Get-RouteSummary $text
  $routeSummaryIndex = (One '^.*SDL_AUDIO_AUDIT_SUMMARY v=1 phase=title status=PASS .*$' $text 'Ordered SDL audio-route summary').Index
  if (-not ($summary.Index -lt $routeSummaryIndex -and $routeSummaryIndex -lt $xmpSummary.Index -and $xmpSummary.Index -lt $appSummary.Index)) { throw 'Final stability/route/XMP/app summary order is invalid.' }
  [pscustomobject]@{ Passed = $true; RunId = Split-Path $root -Leaf; LogSet = $logs; Route = $route; PauseCycles = 2; DeviceEvents = $summaryDeviceEvents; DeviceRecoveries = $summaryDeviceRecoveries; OperatorHeard = $true; ControlledExit = $true }
}

if ($PSCmdlet.ParameterSetName -ceq 'Soak') { Get-LongSoak $LongSoakPath; return }
if ($PSCmdlet.ParameterSetName -ceq 'Run') { Get-StabilityRun $RunPath; return }

$result = Resolve-Safe $ResultPath 'Audio-stability result'
$raw = [IO.File]::ReadAllText($result)
if ($raw -match '(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])') { throw 'Result contains a private or absolute path.' }
$record = $raw | ConvertFrom-Json
if ($record.schema -cne 'mcla-audio-stability-v1' -or $record.task -cne 'M6-007' -or $record.decision -cne 'two-hour-audio-and-device-recovery-pass') { throw 'Audio-stability result identity is invalid.' }
$root = Split-Path $result
if (-not $Fixture -and ($record.run_id -cne (Split-Path $root -Leaf) -or $record.run_id -notmatch '^20[0-9]{6}-[0-9]{6}-[0-9a-f]{8}$' -or $record.sdk_version -cne '0.9.0.25' -or $record.sdk_commit -cne 'f28ddabbae3bca56ddf5ffea067982c49c9549b7' -or $record.build_configuration -cne 'RelWithDebInfo')) { throw 'Audio-stability release identity is invalid.' }
$soak = Get-LongSoak (Join-Path $root 'long-soak')
$stability = Get-StabilityRun (Join-Path $root 'device-switch')
if ($record.long_soak.run_id -cne $soak.RunId -or $record.long_soak.log_set_sha256 -cne $soak.LogSet.Hash -or $record.long_soak.soak_seconds -ne 7200 -or $record.long_soak.controlled_exit -ne $true -or (ConvertTo-Json $record.long_soak.route -Compress -Depth 6) -cne (ConvertTo-Json $soak.Route -Compress -Depth 6) -or (ConvertTo-Json $record.long_soak.resources -Compress -Depth 6) -cne (ConvertTo-Json $soak.Resources -Compress -Depth 6) -or $record.device_switch.run_id -cne $stability.RunId -or $record.device_switch.log_set_sha256 -cne $stability.LogSet.Hash -or $record.device_switch.pause_cycles -ne 2 -or $record.device_switch.device_events -ne $stability.DeviceEvents -or $record.device_switch.device_recoveries -ne $stability.DeviceRecoveries -or $record.device_switch.operator_heard -ne $true -or $record.device_switch.controlled_exit -ne $true -or $record.scope.monolithic_run_claimed -ne $false -or $record.scope.two_hour_soak -ne $true -or $record.scope.current_pause_resume -ne $true -or $record.scope.current_default_device_recovery -ne $true -or $record.scope.operator_heard_recovery -ne $true -or $record.scope.prior_stream_transition_bound -ne $true -or $record.scope.prior_xmp_metadata_fallback_bound -ne $true -or $record.scope.device_identity_recorded -ne $false -or $record.scope.audio_mix_fidelity_claimed -ne $false) { throw 'Audio-stability result is not bound to both physical stages and its claim boundary.' }
$expectedPriors = @('M4-007', 'M4-008', 'M5-009', 'M5-013')
if (@($record.prior_evidence).Count -ne 4) { throw 'Prior audio evidence count is invalid.' }
for ($i = 0; $i -lt $expectedPriors.Count; ++$i) {
  $prior = $record.prior_evidence[$i]
  if ($prior.task -cne $expectedPriors[$i]) { throw 'Prior audio evidence order is invalid.' }
  if ($prior.task -notin @('M4-007', 'M4-008', 'M5-009', 'M5-013') -or $prior.run_id -notmatch '^20[0-9]{6}-[0-9]{6}-[0-9a-f]{8}$') { throw 'Prior audio evidence identity is invalid.' }
  $path = Resolve-Safe ("private/evidence/$($prior.task)/$($prior.run_id)/result.json") 'Prior audio result'
  if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $prior.sha256) { throw 'Prior audio evidence drifted.' }
}
if (-not $Fixture) {
  $sdk = Resolve-Safe 'third_party/rexglue-sdk' 'ReXGlue SDK'
  if ((& git -C $sdk rev-parse HEAD).Trim() -cne $record.sdk_commit -or (& git -C $sdk describe --tags --exact-match HEAD 2>$null).Trim() -cne "v$($record.sdk_version)" -or (git -C $sdk status --porcelain)) { throw 'Current ReXGlue identity or cleanliness does not match the result.' }
  $build = Resolve-Safe 'out/build/win-amd64-relwithdebinfo' 'Canonical RelWithDebInfo build'
  if (@($record.artifacts).Count -ne 4) { throw 'Runtime artifact count is invalid.' }
  $artifactNames = @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll')
  for ($i = 0; $i -lt $artifactNames.Count; ++$i) { $artifact = $record.artifacts[$i]; $path = Resolve-Safe (Join-Path $build $artifactNames[$i]) 'Runtime artifact'; if ($artifact.name -cne $artifactNames[$i] -or $artifact.bytes -ne (Get-Item -LiteralPath $path).Length -or $artifact.sha256 -cne (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash) { throw 'Runtime artifact binding failed.' } }
  if ($record.focused_tests.cases -ne 10 -or $record.focused_tests.assertions -ne 42) { throw 'Focused audio-test totals changed.' }
  foreach ($pair in @(@('sdk-install.log', 'sdk_log_sha256'), @('sdk-audio-test.log', 'test_log_sha256'), @('app-clean-build.log', 'app_log_sha256'))) { $path = Resolve-Safe (Join-Path $root ('build\' + $pair[0])) 'Build evidence'; if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $record.focused_tests.($pair[1])) { throw 'Build evidence hash mismatch.' } }
  $unit = Resolve-Safe 'third_party/rexglue-sdk/out/win-amd64/RelWithDebInfo/unit_tests.exe' 'Focused unit executable'
  if ((Get-FileHash -LiteralPath $unit -Algorithm SHA256).Hash -cne $record.focused_tests.unit_executable_sha256) { throw 'Focused unit executable drifted.' }
  if (($record.game_before | ConvertTo-Json -Compress -Depth 5) -cne ($record.game_after | ConvertTo-Json -Compress -Depth 5)) { throw 'Source-game identity changed in the result.' }
  $gameVerifier = Resolve-Safe 'scripts/verify-game-manifest.ps1' 'Game-manifest verifier'
  $currentGame = Game-Record (& $gameVerifier -GamePath (Resolve-Safe 'private/game' 'Canonical game') -VerifyHashes)
  if (($record.game_after | ConvertTo-Json -Compress -Depth 5) -cne ($currentGame | ConvertTo-Json -Compress -Depth 5)) { throw 'Current source-game identity drifted from the physical result.' }
  $tree = Get-Tree $root
  if ($record.evidence_tree_sha256 -cne $tree.Hash -or $record.evidence_tree_file_count -ne $tree.FileCount -or $record.evidence_tree_directory_count -ne $tree.DirectoryCount -or $record.evidence_tree_bytes -ne $tree.Bytes) { throw 'Evidence tree binding failed.' }
  if (@((Get-Process mcla -ErrorAction SilentlyContinue) | Where-Object { try { [string]::Equals($_.Path, (Join-Path $build 'mcla.exe'), [StringComparison]::OrdinalIgnoreCase) } catch { $false } }).Count) { throw 'Canonical MCLA process is still running.' }
}
[pscustomobject]@{ Passed = $true; Decision = $record.decision; SoakSeconds = 7200; PauseCycles = 2; DeviceRecoveryVerified = $true; OperatorHeard = $true; ControlledExitVerified = $true; MonolithicRunClaimed = $false }
