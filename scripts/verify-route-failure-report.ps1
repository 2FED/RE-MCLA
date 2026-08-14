[CmdletBinding(DefaultParameterSetName = 'Evidence')]
param(
    [Parameter(ParameterSetName = 'Evidence')][switch]$EvidenceOnly,
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$RuntimeLogPath,
    [Parameter(ParameterSetName = 'Probe')][int]$ExpectedDevelopmentMisses = 0,
    [Parameter(ParameterSetName = 'Probe')][switch]$FixtureMode,
    [Parameter(Mandatory, ParameterSetName = 'Result')][string]$ResultPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$accepted = [ordered]@{
    streaming = [ordered]@{ task = 'M5-002'; run = '20260814-093131-ddca5b9d'; hash = 'A582A41BB254D9187BC6F8147E89E2FF2E92D0EA4B79945760BA6FBCF334BC28' }
    timing = [ordered]@{ task = 'M5-008'; run = '20260814-150733-da37caed'; hash = 'C705E09FF88CA33D705E11326B34B7E643604D0C40FC210414B6431D3A86CAA9' }
    audio = [ordered]@{ task = 'M5-009'; run = '20260814-170657-f44949d7'; hash = '4E3D514386501D92B43CD4F2C4C89ECD8BA000ACF23D8B168CFA431C8F67C62F' }
}

function Resolve-Safe([string]$Path, [string]$Description, [bool]$Directory = $false) {
    $full = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repo $Path }))
    $prefix = $repo.TrimEnd('\') + '\'
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes repository." }
    $current = $repo
    foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object Length)) {
        $current = Join-Path $current $part
        if (-not (Test-Path -LiteralPath $current)) { throw "$Description is missing." }
        if ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "$Description traverses a reparse point." }
    }
    if ($Directory -and -not (Test-Path -LiteralPath $full -PathType Container)) { throw "$Description is not a directory." }
    if (-not $Directory -and -not (Test-Path -LiteralPath $full -PathType Leaf)) { throw "$Description is not a file." }
    $full
}

function Get-LogSet([string]$CurrentPath) {
    $current = Resolve-Safe $CurrentPath 'Runtime log'
    if ((Split-Path $current -Leaf) -cne 'mcla.log') { throw 'Current runtime log must be mcla.log.' }
    $files = @(Get-ChildItem -LiteralPath (Split-Path $current) -File -Filter 'mcla*.log')
    $rotated = @(); $now = $null
    foreach ($file in $files) {
        if ($file.Name -ceq 'mcla.log') { $now = $file; continue }
        $match = [regex]::Match($file.Name, '^mcla\.([1-9][0-9]*)\.log$')
        if (-not $match.Success) { throw 'Malformed runtime-log rotation.' }
        $rotated += [pscustomobject]@{ Index = [int]$match.Groups[1].Value; File = $file }
    }
    if (-not $now -or $files.Count -gt 24) { throw 'Runtime-log topology is invalid.' }
    $indices = @($rotated | ForEach-Object Index | Sort-Object)
    for ($i = 0; $i -lt $indices.Count; $i++) { if ($indices[$i] -ne $i + 1) { throw 'Runtime-log rotations are not contiguous.' } }
    $ordered = @($rotated | Sort-Object Index -Descending | ForEach-Object File) + $now
    $lines = [Collections.Generic.List[string]]::new(); $bytes = 0L
    foreach ($file in $ordered) {
        $bytes += $file.Length
        if ($bytes -gt 268435456) { throw 'Runtime logs exceed 256 MiB.' }
        foreach ($line in [IO.File]::ReadLines($file.FullName)) { $lines.Add($line) }
    }
    [pscustomobject]@{ Lines = $lines; FileCount = $ordered.Count; Bytes = $bytes }
}

function Get-Probe([string]$Log, [int]$DevelopmentMisses) {
    $set = Get-LogSet $Log
    $text = $set.Lines -join "`n"
    $banned = @(
        '(?i)\[fatal\]', 'PPC_UNIMPLEMENTED', '(?i)invalid/unregistered guest', '(?i)guest crash',
        '(?i)assertion failed', '(?i)access violation', '(?i)out of memory', '(?i)bad_alloc',
        '(?i)heap corruption', '(?i)(?:allocation failed|failed to allocate)',
        '(?i)Read(?:SVOD|STFS|EntrySVOD) failed', '(?i)archive[^\r\n]*failed',
        '(?i)\[Nt(?:ReadFile|WriteFile|AllocateVirtualMemory|FreeVirtualMemory)\][^\r\n]*FAILED',
        '(?i)(?:Thread creation|CreateThread|streaming thread)[^\r\n]*failed',
        '(?i)D3D12[^\r\n]*device (?:lost|removed)'
    )
    foreach ($pattern in $banned) { if ($text -match $pattern) { throw "Fatal allocator, I/O, or streaming pattern '$pattern' was found." } }

    $metrics = [ordered]@{
        nt_allocate_results = 0; nt_allocate_success = 0; nt_allocate_failures = 0
        nt_free_results = 0; nt_free_success = 0; nt_free_failures = 0
        physical_allocate_results = 0; physical_allocate_success = 0; physical_allocate_failures = 0
        nt_read_results = 0; nt_read_success = 0; nt_read_pending = 0; nt_read_failures = 0
        nt_create_success = 0; development_misses = 0
    }
    foreach ($line in $set.Lines) {
        if ($line -match '\[NtAllocateVirtualMemory\] -> (0x[0-9a-fA-F]+)') {
            $metrics.nt_allocate_results++; if ($matches[1] -ceq '0x0') { $metrics.nt_allocate_success++ } else { $metrics.nt_allocate_failures++ }
        }
        if ($line -match '\[NtFreeVirtualMemory\] -> (0x[0-9a-fA-F]+)') {
            $metrics.nt_free_results++; if ($matches[1] -ceq '0x0') { $metrics.nt_free_success++ } else { $metrics.nt_free_failures++ }
        }
        if ($line -match '\[MmAllocatePhysicalMemoryEx\] -> addr=(0x[0-9a-fA-F]+)') {
            $metrics.physical_allocate_results++; if ($matches[1] -cne '0x0') { $metrics.physical_allocate_success++ } else { $metrics.physical_allocate_failures++ }
        }
        if ($line -match '\[NtReadFile\] -> (0x[0-9a-fA-F]+)(.*)$') {
            $metrics.nt_read_results++
            if ($matches[1] -ceq '0x0') { $metrics.nt_read_success++ }
            elseif ($matches[1] -ceq '0x103' -and $matches[2] -match 'sync=false, iosb_status=0x0') { $metrics.nt_read_pending++ }
            else { $metrics.nt_read_failures++ }
        }
        if ($line -match '\[NtCreateFile\] -> (0x[0-9a-fA-F]+)') {
            if ($matches[1] -ceq '0x0') { $metrics.nt_create_success++ } else { throw 'Unexpected NtCreateFile result was found.' }
        }
        if ($line -match "\[NtCreateFile\] FAILED: path='t:\\mc4\\art\\city\\test_(?:dt_railyard|sc_exposition_park)\.loc' -> 0xc000000f$") { $metrics.development_misses++ }
        elseif ($line -match '\[NtCreateFile\] FAILED:') { throw 'Unexpected NtCreateFile failure was found.' }
    }
    if ($metrics.development_misses -ne $DevelopmentMisses) { throw 'Development-path miss count changed.' }
    foreach ($name in @('nt_allocate_failures','nt_free_failures','physical_allocate_failures','nt_read_failures')) {
        if ($metrics[$name] -ne 0) { throw "Failure counter '$name' is nonzero." }
    }
    [pscustomobject]@{ Passed = $true; Metrics = [pscustomobject]$metrics; LogFiles = $set.FileCount; LogBytes = $set.Bytes }
}

function Get-AcceptedPath([string]$Name) {
    $item = $accepted[$Name]
    $path = Resolve-Safe "private/evidence/$($item.task)/$($item.run)/result.json" "Accepted $Name result"
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $item.hash) { throw "Accepted $Name result hash changed." }
    $path
}

function Assert-NoReparseTree([string]$Root) {
    foreach ($item in @(Get-Item -LiteralPath $Root -Force) + @(Get-ChildItem -LiteralPath $Root -Recurse -Force)) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Accepted evidence contains a reparse point.' }
    }
}

function Get-TreeSnapshot([string]$Root) {
    $rootPath = Resolve-Safe $Root 'Accepted evidence root' $true
    Assert-NoReparseTree $rootPath
    $items = @(Get-ChildItem -LiteralPath $rootPath -Recurse -Force)
    $files = @($items | Where-Object { -not $_.PSIsContainer -and -not ($_.FullName -ceq (Join-Path $rootPath 'result.json')) } | Sort-Object FullName)
    $entries = @(); $bytes = 0L
    foreach ($directory in @($items | Where-Object { $_.PSIsContainer } | Sort-Object FullName)) {
        $entries += [ordered]@{ kind='directory'; path=$directory.FullName.Substring($rootPath.Length).TrimStart('\').Replace('\','/') }
    }
    foreach ($file in $files) {
        $bytes += $file.Length
        $entries += [ordered]@{ kind='file'; path=$file.FullName.Substring($rootPath.Length).TrimStart('\').Replace('\','/'); bytes=$file.Length; sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
    }
    $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = -join ($sha.ComputeHash([Text.UTF8Encoding]::new($false).GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
    [pscustomobject]@{ Hash=$hash; FileCount=$files.Count; DirectoryCount=@($items | Where-Object { $_.PSIsContainer }).Count; Bytes=$bytes }
}

function Assert-M5-002Physical([string]$ResultPath) {
    $record = [IO.File]::ReadAllText($ResultPath) | ConvertFrom-Json
    if ($record.schema -ne 1 -or $record.task -cne 'M5-002' -or $record.decision -cne 'world-rpf-streaming-pass' -or
        $record.cycle.exit_code -ne 0 -or $record.cycle.close_requested -ne $true -or $record.cycle.harness_force_cleanup -ne $false -or
        $record.no_surviving_processes -ne $true -or $record.data_integrity_preserved -ne $true) { throw 'Accepted M5-002 result semantics changed.' }
    $root = Split-Path $ResultPath -Parent
    Assert-NoReparseTree $root
    $tree = Get-TreeSnapshot $root
    if ($tree.Hash -cne '2643FAC63954C6AA1559670648E0EC060D3AF3854CB5B4F600AF2610ED80E5AB' -or
        $tree.FileCount -ne 17 -or $tree.DirectoryCount -ne 12 -or $tree.Bytes -ne 66240992) { throw 'Accepted M5-002 evidence tree changed.' }
    if ((Get-FileHash -LiteralPath (Join-Path $root 'sdk-vfs-test.log') -Algorithm SHA256).Hash -cne $record.build.focused_test_log_sha256 -or
        (Get-FileHash -LiteralPath (Join-Path $root 'relwithdebinfo-clean-build.log') -Algorithm SHA256).Hash -cne $record.build.app_build_log_sha256) { throw 'Accepted M5-002 build-log identity changed.' }
    foreach ($entry in @($record.cycle.runtime_logs)) {
        $path = Join-Path $root ('runs/01/' + $entry.name)
        if ((Get-Item -LiteralPath $path).Length -ne $entry.bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $entry.sha256) { throw "Accepted M5-002 log '$($entry.name)' changed." }
    }
    $captures = [ordered]@{ title='mcla-first-frame.bmp'; gameplay='mcla-frontend-gameplay.bmp'; pause='mcla-frontend-pause.bmp'; options='mcla-frontend-options.bmp' }
    foreach ($entry in $captures.GetEnumerator()) {
        $expected = $record.cycle.captures.($entry.Key); $path = Join-Path $root ('runs/01/user/' + $entry.Value)
        if ((Get-Item -LiteralPath $path).Length -ne $expected.bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $expected.sha256) { throw "Accepted M5-002 capture '$($entry.Key)' changed." }
    }
    $record
}

function Assert-TreeBackedResult([string]$ResultPath, [string]$Task, [string]$Decision) {
    $record = [IO.File]::ReadAllText($ResultPath) | ConvertFrom-Json
    if ($record.task -cne $Task -or $record.decision -cne $Decision -or $record.evidence_tree_sha256 -cnotmatch '^[A-F0-9]{64}$') { throw "Accepted $Task result semantics changed." }
    $tree = Get-TreeSnapshot (Split-Path $ResultPath -Parent)
    if ($record.evidence_tree_sha256 -cne $tree.Hash -or $record.evidence_tree_file_count -ne $tree.FileCount -or
        $record.evidence_tree_directory_count -ne $tree.DirectoryCount -or $record.evidence_tree_bytes -ne $tree.Bytes) { throw "Accepted $Task evidence tree changed." }
    $record
}

function Get-Evidence {
    $streamPath = Get-AcceptedPath 'streaming'
    $timingPath = Get-AcceptedPath 'timing'
    $audioPath = Get-AcceptedPath 'audio'
    $stream = Assert-M5-002Physical $streamPath
    $timing = Assert-TreeBackedResult $timingPath 'M5-008' 'stock-30-fixed-step-and-real-time-throughput-pass'
    $audio = & (Join-Path $PSScriptRoot 'verify-audio-event-smoke.ps1') -ResultPath $audioPath
    if ($audio.Decision -cne 'six-class-audio-stream-presence-pass' -or $audio.ControlledExitVerified -ne $true -or
        @($timing.cycles).Count -ne 3 -or @($timing.cycles | Where-Object { $_.controlled_exit -ne $true }).Count -ne 0) { throw 'An accepted prerequisite failed physical re-verification.' }

    $streamProbe = Get-Probe (Join-Path (Split-Path $streamPath -Parent) 'runs/01/mcla.log') 7
    $timingBytes = 0L
    foreach ($cycle in 1..3) {
        $timingProbe = Get-Probe (Join-Path (Split-Path $timingPath -Parent) ('runs/{0:D2}/mcla.log' -f $cycle)) 7
        $timingBytes += $timingProbe.LogBytes
    }
    $audioProbe = Get-Probe (Join-Path (Split-Path $audioPath -Parent) 'runs/01/mcla.log') 7
    $m = $streamProbe.Metrics
    if ($m.nt_allocate_results -ne 87 -or $m.nt_allocate_success -ne 87 -or $m.nt_free_results -ne 49 -or $m.nt_free_success -ne 49 -or
        $m.physical_allocate_results -ne 2590 -or $m.physical_allocate_success -ne 2590 -or $m.nt_read_results -ne 8055 -or
        $m.nt_read_success -ne 8054 -or $m.nt_read_pending -ne 1 -or $m.nt_create_success -ne 15) { throw 'Noisy allocator/I/O calibration changed.' }
    [pscustomobject]@{
        Passed = $true; Decision = 'canonical-route-streaming-io-allocator-bounded-pass'; ProcessCycles = 5
        Stream = $stream; Timing = $timing; Audio = $audio; Metrics = $m
        RuntimeLogFiles = $streamProbe.LogFiles + 3 + $audioProbe.LogFiles
        RuntimeLogBytes = $streamProbe.LogBytes + $timingBytes + $audioProbe.LogBytes
    }
}

if ($PSCmdlet.ParameterSetName -eq 'Probe') { Get-Probe $RuntimeLogPath $ExpectedDevelopmentMisses; return }
if ($PSCmdlet.ParameterSetName -eq 'Evidence') { Get-Evidence; return }

$result = Resolve-Safe $ResultPath 'M5-010 result'
$raw = [IO.File]::ReadAllText($result)
if ($raw -match '(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])') { throw 'Result contains a private or absolute path.' }
$record = $raw | ConvertFrom-Json
$expectedProperties = @('schema','task','decision','sdk_version','sdk_commit','accepted_evidence','coverage','scope','data_integrity_verified')
if (($record.PSObject.Properties.Name -join ',') -cne ($expectedProperties -join ',') -or $record.schema -ne 1 -or
    $record.task -cne 'M5-010' -or $record.decision -cne 'canonical-route-streaming-io-allocator-bounded-pass' -or
    $record.sdk_version -cne '0.9.0.20' -or $record.sdk_commit -cne 'c4aa30c35386bb4d2ef051a59ea8e71bab667172' -or
    $record.data_integrity_verified -ne $true) { throw 'M5-010 result identity changed.' }
$root = Split-Path $result -Parent
if ((Split-Path $result -Leaf) -cne 'result.json' -or (Split-Path (Split-Path $root -Parent) -Leaf) -cne 'M5-010' -or
    (Split-Path $root -Leaf) -notmatch '^[0-9]{8}-[0-9]{6}-[0-9a-f]{8}$' -or
    (@(Get-ChildItem -LiteralPath $root -Force | ForEach-Object Name) -join ',') -cne 'result.json') { throw 'M5-010 result topology changed.' }
$acceptedExpected = @('streaming','timing','audio')
if (($record.accepted_evidence.PSObject.Properties.Name -join ',') -cne ($acceptedExpected -join ',')) { throw 'Accepted-evidence schema changed.' }
foreach ($name in $acceptedExpected) {
    $actual = $record.accepted_evidence.$name; $item = $accepted[$name]
    if (($actual.PSObject.Properties.Name -join ',') -cne 'task,run_id,result_sha256' -or $actual.task -cne $item.task -or
        $actual.run_id -cne $item.run -or $actual.result_sha256 -cne $item.hash) { throw "Accepted $name binding changed." }
}
$probe = Get-Evidence
$c = $record.coverage
$coverageExpected = @('physical_process_cycles','controlled_exit_cycles','noisy_io_cycles','stock_speed_cycles','long_audio_gameplay_cycles','cache_read_calls','cache_read_bytes','audlo_read_calls','audlo_read_bytes','nt_create_success','expected_development_misses_per_cycle','expected_development_miss_events','nt_read_results','nt_read_success','nt_read_pending_success','nt_read_failures','nt_allocate_results','nt_allocate_success','nt_allocate_failures','nt_free_results','nt_free_success','nt_free_failures','physical_allocate_results','physical_allocate_success','physical_allocate_failures','fatal_allocator_events','fatal_io_events','fatal_streaming_events')
if (($c.PSObject.Properties.Name -join ',') -cne ($coverageExpected -join ',') -or
    $c.physical_process_cycles -ne 5 -or $c.controlled_exit_cycles -ne 5 -or $c.noisy_io_cycles -ne 1 -or $c.stock_speed_cycles -ne 3 -or $c.long_audio_gameplay_cycles -ne 1 -or
    $c.cache_read_calls -ne 8011 -or $c.cache_read_bytes -ne 262821908 -or $c.audlo_read_calls -ne 41 -or $c.audlo_read_bytes -ne 3290825 -or
    $c.nt_create_success -ne 15 -or $c.expected_development_misses_per_cycle -ne 7 -or $c.expected_development_miss_events -ne 35 -or $c.nt_read_results -ne 8055 -or $c.nt_read_success -ne 8054 -or
    $c.nt_read_pending_success -ne 1 -or $c.nt_read_failures -ne 0 -or $c.nt_allocate_results -ne 87 -or $c.nt_allocate_success -ne 87 -or
    $c.nt_allocate_failures -ne 0 -or $c.nt_free_results -ne 49 -or $c.nt_free_success -ne 49 -or $c.nt_free_failures -ne 0 -or
    $c.physical_allocate_results -ne 2590 -or $c.physical_allocate_success -ne 2590 -or $c.physical_allocate_failures -ne 0 -or
    $c.fatal_allocator_events -ne 0 -or $c.fatal_io_events -ne 0 -or $c.fatal_streaming_events -ne 0) { throw 'M5-010 coverage changed.' }
$scope = $record.scope
if (($scope.PSObject.Properties.Name -join ',') -cne 'route_failure_reproduced,behavior_patch_required,long_session_leak_check_claimed,multi_race_claimed,classification' -or
    $scope.route_failure_reproduced -ne $false -or $scope.behavior_patch_required -ne $false -or $scope.long_session_leak_check_claimed -ne $false -or
    $scope.multi_race_claimed -ne $false -or $scope.classification -cne 'bounded-five-process-no-defect-observed') { throw 'M5-010 scope overclaims coverage.' }
[pscustomobject]@{ Passed=$true; Decision=$record.decision; PhysicalProcessCycles=5; NtReadResults=8055; AllocatorResults=2726; FatalEvents=0; BehaviorPatchRequired=$false; DataIntegrityVerified=$true }
