[CmdletBinding(DefaultParameterSetName = 'Probe')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$RuntimeLogPath,
    [Parameter(ParameterSetName = 'Probe')][string]$UserRoot,
    [Parameter(ParameterSetName = 'Probe')][switch]$ProbeOnly,
    [Parameter(ParameterSetName = 'Probe')][switch]$FixtureMode,
    [Parameter(Mandatory, ParameterSetName = 'Result')][string]$ResultPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$utf8 = [Text.UTF8Encoding]::new($false)
$cacheSize = 2130739200L
$audloSize = 1463189504L
$gameVerify = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'

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

function Assert-NoReparse([string]$Root) {
    $pending = [Collections.Generic.Stack[string]]::new(); $pending.Push($Root)
    while ($pending.Count) {
        foreach ($item in @(Get-ChildItem -LiteralPath $pending.Pop() -Force)) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Tree contains a reparse point.' }
            if ($item.PSIsContainer) { $pending.Push($item.FullName) }
        }
    }
}

function Get-Tree([string]$Root) {
    $root = Resolve-Safe $Root 'Tree' $true
    Assert-NoReparse $root
    $items = @(Get-ChildItem -LiteralPath $root -Recurse -Force)
    $files = @($items | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)
    $entries = @()
    foreach ($directory in @($items | Where-Object PSIsContainer | Sort-Object FullName)) { $entries += [ordered]@{ kind='directory'; path=$directory.FullName.Substring($root.Length).TrimStart('\').Replace('\','/') } }
    foreach ($file in $files) { $entries += [ordered]@{ kind='file'; path=$file.FullName.Substring($root.Length).TrimStart('\').Replace('\','/'); length=$file.Length; sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash } }
    $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4
    $sha = [Security.Cryptography.SHA256]::Create(); try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
    $bytes = 0L; foreach ($file in $files) { $bytes += $file.Length }
    [pscustomobject]@{ Hash=$hash; FileCount=$files.Count; DirectoryCount=@($items | Where-Object PSIsContainer).Count; Bytes=$bytes }
}

function Get-GameIdentity {
    $root = Resolve-Safe 'private/game' 'Canonical game' $true
    $tree = Get-Tree $root
    $verified = & $gameVerify -GamePath $root -VerifyHashes
    [ordered]@{ file_count=$verified.FileCount; payload_bytes=$verified.PayloadBytes; manifest_sha256=(Get-FileHash -LiteralPath $verified.ManifestPath -Algorithm SHA256).Hash; tree_sha256=$tree.Hash; tree_file_count=$tree.FileCount; tree_directory_count=$tree.DirectoryCount; tree_bytes=$tree.Bytes }
}

function Get-Artifacts {
    $build = Resolve-Safe 'out/build/win-amd64-relwithdebinfo' 'Canonical build' $true
    @('mcla.exe','rexruntimerd.dll','TracyClientrd.dll','rexgpu-xenosrd.dll') | ForEach-Object { [ordered]@{ name=$_; sha256=(Get-FileHash -LiteralPath (Resolve-Safe (Join-Path $build $_) "Artifact $_") -Algorithm SHA256).Hash } }
}

function Get-LogSet([string]$Current) {
    $current = Resolve-Safe $Current 'Runtime log'
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
    for ($index = 0; $index -lt $indices.Count; $index++) { if ($indices[$index] -ne $index + 1) { throw 'Runtime-log rotations are not contiguous.' } }
    $ordered = @($rotated | Sort-Object Index -Descending | ForEach-Object File) + $now
    $lines = [Collections.Generic.List[string]]::new(); $manifest = @(); $bytes = 0L
    foreach ($file in $ordered) {
        $bytes += $file.Length
        if ($bytes -gt 268435456) { throw 'Runtime logs exceed 256 MiB.' }
        foreach ($line in [IO.File]::ReadLines($file.FullName)) { $lines.Add($line) }
        $manifest += [ordered]@{ name = $file.Name; bytes = $file.Length; sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
    }
    $json = ConvertTo-Json -InputObject @($manifest) -Compress -Depth 3
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
    [pscustomobject]@{ Lines = $lines; Text = $lines -join "`n"; Files = @($manifest); Count = $manifest.Count; Bytes = $bytes; Hash = $hash }
}

function New-ArchiveMetrics([string]$Name, [long]$Size) {
    [pscustomobject]@{ Name = $Name; Size = $Size; OpenAttempts = 0; OpenSuccess = 0; OpenFailure = 0; ReadCalls = 0; ReadSuccess = 0; ReadFailure = 0; ReadBytes = 0L; HighestEnd = 0L; ZeroByteReads = 0; FirstOpenIndex = -1; FirstReadIndex = -1; LastReadIndex = -1 }
}

function Get-Probe([string]$Log, [string]$User, [bool]$Fixture) {
    $logSet = Get-LogSet $Log
    $text = $logSet.Text
    $caseMatches = [regex]::Matches($text, '(?m)^.*MCLA VFS: mixed-case RPF path resolution verified\s*$')
    $launchMatches = [regex]::Matches($text, '(?m)^.*KernelState: Preparing module launch\.\.\.\s*$')
    $gameplayMatches = [regex]::Matches($text, '(?m)^.*MCLA_FRONTEND_SMOKE_FRAME v=1 phase=gameplay width=1280 height=720 status=PASS\s*$')
    if ($caseMatches.Count -ne 1 -or $launchMatches.Count -ne 1 -or $gameplayMatches.Count -ne 1 -or -not ($caseMatches[0].Index -lt $launchMatches[0].Index -and $launchMatches[0].Index -lt $gameplayMatches[0].Index)) { throw 'Case/launch/gameplay marker topology failed.' }
    $caseLine = -1; $launchLine = -1; $gameplayLine = -1
    for ($lineIndex = 0; $lineIndex -lt $logSet.Lines.Count; $lineIndex++) {
        $line = $logSet.Lines[$lineIndex]
        if ($line -match 'MCLA VFS: mixed-case RPF path resolution verified') { $caseLine = $lineIndex }
        if ($line -match 'KernelState: Preparing module launch\.\.\.') { $launchLine = $lineIndex }
        if ($line -match 'MCLA_FRONTEND_SMOKE_FRAME v=1 phase=gameplay width=1280 height=720 status=PASS') { $gameplayLine = $lineIndex }
    }

    $metrics = @{ audlo = New-ArchiveMetrics 'audlo' $audloSize; cache = New-ArchiveMetrics 'cache' $cacheSize }
    $pendingCreate = @{}; $pendingRead = @{}; $handles = @{}
    for ($index = 0; $index -lt $logSet.Lines.Count; $index++) {
        $line = $logSet.Lines[$index]
        $match = [regex]::Match($line, '\[t(?<thread>[0-9]+)\].*\[NtCreateFile\] path=(?<path>\S+) ')
        if ($match.Success) {
            $thread = $match.Groups['thread'].Value
            $path = $match.Groups['path'].Value.ToLowerInvariant()
            $asset = if ($path.EndsWith('xarchive_audlo.rpf')) { 'audlo' } elseif ($path.EndsWith('xarchive_cache.rpf')) { 'cache' } else { $null }
            if ($asset) {
                if ($pendingCreate.ContainsKey($thread)) { throw 'Overlapping archive create requests on one thread.' }
                $metrics[$asset].OpenAttempts++
                $pendingCreate[$thread] = [pscustomobject]@{ Asset = $asset; Index = $index }
            }
            continue
        }
        $match = [regex]::Match($line, '\[t(?<thread>[0-9]+)\].*\[NtCreateFile\] -> 0x(?<status>[0-9a-fA-F]+) handle=0x(?<handle>[0-9a-fA-F]+)')
        if ($match.Success) {
            $thread = $match.Groups['thread'].Value
            if ($pendingCreate.ContainsKey($thread)) {
                $pending = $pendingCreate[$thread]; $metric = $metrics[$pending.Asset]
                if ([Convert]::ToUInt32($match.Groups['status'].Value, 16) -eq 0) {
                    $metric.OpenSuccess++
                    if ($metric.FirstOpenIndex -lt 0) { $metric.FirstOpenIndex = $pending.Index }
                    $handles[$match.Groups['handle'].Value.ToLowerInvariant()] = $pending.Asset
                } else { $metric.OpenFailure++ }
                $null = $pendingCreate.Remove($thread)
            }
            continue
        }
        $match = [regex]::Match($line, '\[NtClose\] handle=0x(?<handle>[0-9a-fA-F]+)')
        if ($match.Success) { $null = $handles.Remove($match.Groups['handle'].Value.ToLowerInvariant()); continue }
        $match = [regex]::Match($line, '\[t(?<thread>[0-9]+)\].*\[NtReadFile\] handle=0x(?<handle>[0-9a-fA-F]+).* len=0x(?<length>[0-9a-fA-F]+) offset=(?<offset>-?[0-9]+)')
        if ($match.Success) {
            $thread = $match.Groups['thread'].Value; $handle = $match.Groups['handle'].Value.ToLowerInvariant()
            if ($handles.ContainsKey($handle)) {
                if ($pendingRead.ContainsKey($thread)) { throw 'Overlapping archive read requests on one thread.' }
                $pendingRead[$thread] = [pscustomobject]@{ Asset = $handles[$handle]; Offset = [long]$match.Groups['offset'].Value; Requested = [long][Convert]::ToUInt32($match.Groups['length'].Value, 16); Index = $index }
            }
            continue
        }
        $match = [regex]::Match($line, '\[t(?<thread>[0-9]+)\].*\[NtReadFile\] -> 0x(?<status>[0-9a-fA-F]+) \(sync=(?:true|false), iosb_status=0x(?<iosb>[0-9a-fA-F]+), iosb_info=(?<bytes>[0-9]+),')
        if ($match.Success) {
            $thread = $match.Groups['thread'].Value
            if ($pendingRead.ContainsKey($thread)) {
                $pending = $pendingRead[$thread]; $metric = $metrics[$pending.Asset]; $actual = [long]$match.Groups['bytes'].Value
                $metric.ReadCalls++
                if ([Convert]::ToUInt32($match.Groups['status'].Value, 16) -eq 0 -and [Convert]::ToUInt32($match.Groups['iosb'].Value, 16) -eq 0 -and $actual -le $pending.Requested) {
                    $metric.ReadSuccess++; $metric.ReadBytes += $actual
                    if ($actual -eq 0 -and $pending.Requested -gt 0) { $metric.ZeroByteReads++ }
                    if ($pending.Offset -lt 0) { throw 'Archive read used an implicit offset.' }
                    $end = $pending.Offset + $actual
                    if ($end -gt $metric.Size) { throw 'Archive read extended beyond the physical RPF size.' }
                    if ($actual -gt 0 -and $end -gt $metric.HighestEnd) { $metric.HighestEnd = $end }
                    if ($metric.FirstReadIndex -lt 0) { $metric.FirstReadIndex = $pending.Index }
                    $metric.LastReadIndex = $index
                } else { $metric.ReadFailure++ }
                $null = $pendingRead.Remove($thread)
            }
            continue
        }
    }
    if ($pendingCreate.Count -or $pendingRead.Count) { throw 'Archive I/O request is missing its completion record.' }
    foreach ($asset in @('audlo', 'cache')) {
        $metric = $metrics[$asset]
        if ($metric.OpenAttempts -ne 2 -or $metric.OpenSuccess -ne 2 -or $metric.OpenFailure -ne 0 -or $metric.ReadCalls -ne $metric.ReadSuccess -or $metric.ReadFailure -ne 0 -or $metric.ZeroByteReads -ne 0 -or -not ($launchLine -lt $metric.FirstOpenIndex -and $metric.FirstOpenIndex -lt $metric.FirstReadIndex -and $metric.FirstReadIndex -lt $gameplayLine)) { throw "Archive $asset open/read accounting failed (opens=$($metric.OpenAttempts)/$($metric.OpenSuccess)/$($metric.OpenFailure), reads=$($metric.ReadCalls)/$($metric.ReadSuccess)/$($metric.ReadFailure), zero=$($metric.ZeroByteReads), order=$launchLine<$($metric.FirstOpenIndex)<$($metric.FirstReadIndex)<$gameplayLine)." }
    }
    if ($metrics.cache.ReadCalls -lt 4000 -or $metrics.cache.ReadBytes -lt 100000000 -or $metrics.cache.HighestEnd -lt [Math]::Floor($cacheSize * 0.95)) { throw 'World/cache RPF coverage is below the canonical floor.' }
    if ($metrics.audlo.ReadCalls -lt 10 -or $metrics.audlo.ReadBytes -lt 50000 -or $metrics.audlo.HighestEnd -lt [Math]::Floor($audloSize * 0.95)) { throw 'Audio-low RPF positive-control coverage is below the canonical floor.' }

    $failedCreates = [regex]::Matches($text, "(?m)^.*\[NtCreateFile\] FAILED: path='(?<path>[^']+)' -> 0x(?<status>[0-9a-fA-F]+)\s*$")
    if ($failedCreates.Count -ne 7) { throw 'Expected retail-safe development-path miss count changed.' }
    $railyard = 0; $park = 0
    foreach ($failure in $failedCreates) {
        if ($failure.Groups['status'].Value.ToLowerInvariant() -cne 'c000000f') { throw 'Unexpected NtCreateFile failure status.' }
        switch ($failure.Groups['path'].Value.ToLowerInvariant()) {
            't:\mc4\art\city\test_dt_railyard.loc' { $railyard++ }
            't:\mc4\art\city\test_sc_exposition_park.loc' { $park++ }
            default { throw 'Unexpected guest file-open failure.' }
        }
    }
    if ($railyard -ne 5 -or $park -ne 2) { throw 'Expected development-path miss distribution changed.' }
    if ([regex]::Matches($text, '(?m)^.*ResolvePath\(t:\\mc4\\art\\city\) failed - device not found\s*$').Count -ne 7) { throw 'Expected missing-development-device resolution count changed.' }
    if ($text -match '(?i)(REX_GUEST_CRASH|GUEST_CRASH_REPORT|PPC_UNIMPLEMENTED|\[fatal\]|assertion failed|D3D12.*device (?:lost|removed))') { throw 'Fatal or unsupported marker found.' }

    $frontend = $null
    if (-not $Fixture) {
        if (-not $User) { throw 'Production probe requires UserRoot.' }
        $frontend = & (Join-Path $PSScriptRoot 'verify-frontend-smoke.ps1') -ProbeOnly -RuntimeLogPath $Log -UserRoot (Resolve-Safe $User 'User root' $true)
    }
    [pscustomobject]@{ Passed = $true; LogSet = $logSet; Audlo = $metrics.audlo; Cache = $metrics.cache; ExpectedDevelopmentMisses = 7; MixedCaseVerified = $true; GameplayVerified = $true; Frontend = $frontend }
}

if ($PSCmdlet.ParameterSetName -eq 'Probe') {
    if (-not $ProbeOnly) { throw 'Probe mode requires -ProbeOnly.' }
    Get-Probe $RuntimeLogPath $UserRoot $FixtureMode.IsPresent
    return
}

$result = Resolve-Safe $ResultPath 'M5-002 result'
$raw = [IO.File]::ReadAllText($result)
if ($raw -match '(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])') { throw 'Result contains a private or absolute path.' }
$record = $raw | ConvertFrom-Json
$expectedProperties = @('schema','task','decision','sdk_version','route_id','cycle_count','build','seed','game_identity','artifacts','cycle','no_surviving_processes','data_integrity_preserved')
if (($record.PSObject.Properties.Name -join ',') -cne ($expectedProperties -join ',') -or -not ($record.schema -is [int] -or $record.schema -is [long]) -or $record.schema -ne 1 -or $record.task -cne 'M5-002' -or $record.decision -cne 'world-rpf-streaming-pass' -or $record.sdk_version -cne '0.9.0.18' -or $record.route_id -cne 'pinned-save-sunset-strip-race-v1:free-roam-prerequisite' -or -not ($record.cycle_count -is [int] -or $record.cycle_count -is [long]) -or $record.cycle_count -ne 1 -or $record.no_surviving_processes -ne $true -or $record.data_integrity_preserved -ne $true) { throw 'M5-002 result identity/scope failed.' }
$buildProperties = @('focused_test_cases','focused_test_assertions','focused_test_log_sha256','app_build_log_sha256','executable_sha256')
if (($record.build.PSObject.Properties.Name -join ',') -cne ($buildProperties -join ',') -or $record.build.focused_test_cases -ne 2 -or $record.build.focused_test_assertions -ne 33) { throw 'M5-002 build contract failed.' }
$resultRoot = Split-Path -Parent $result
$testLog = Resolve-Safe (Join-Path $resultRoot 'sdk-vfs-test.log') 'Focused test log'
$buildLog = Resolve-Safe (Join-Path $resultRoot 'relwithdebinfo-clean-build.log') 'App build log'
if ((Get-FileHash -LiteralPath $testLog -Algorithm SHA256).Hash -cne $record.build.focused_test_log_sha256 -or (Get-FileHash -LiteralPath $buildLog -Algorithm SHA256).Hash -cne $record.build.app_build_log_sha256 -or [IO.File]::ReadAllText($testLog) -notmatch 'All tests passed \(33 assertions in 2 test cases\)') { throw 'M5-002 build evidence mismatch.' }
$exe = Resolve-Safe 'out/build/win-amd64-relwithdebinfo/mcla.exe' 'Canonical executable'
if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -cne $record.build.executable_sha256) { throw 'M5-002 executable hash mismatch.' }
$seedNow = Get-Tree (Resolve-Safe 'private/baseline/M4-011/post-oobe-profile' 'Pinned seed' $true)
if (($record.seed.PSObject.Properties.Name -join ',') -cne 'before_sha256,after_sha256,file_count' -or $record.seed.before_sha256 -cne $seedNow.Hash -or $record.seed.after_sha256 -cne $seedNow.Hash -or $record.seed.file_count -ne 2) { throw 'M5-002 pinned seed changed.' }
$gameNow = Get-GameIdentity
if (($record.game_identity.before | ConvertTo-Json -Compress) -cne ($gameNow | ConvertTo-Json -Compress) -or ($record.game_identity.after | ConvertTo-Json -Compress) -cne ($gameNow | ConvertTo-Json -Compress)) { throw 'M5-002 source-game identity mismatch.' }
$artifactsNow = @(Get-Artifacts)
if (($record.artifacts.before | ConvertTo-Json -Compress) -cne ($artifactsNow | ConvertTo-Json -Compress) -or ($record.artifacts.after | ConvertTo-Json -Compress) -cne ($artifactsNow | ConvertTo-Json -Compress)) { throw 'M5-002 runtime artifacts changed.' }
$resultRoot = Split-Path -Parent $result
if ($record.cycle.relative_root -cne 'runs/01') { throw 'M5-002 cycle relative root changed.' }
$cycleRoot = Resolve-Safe (Join-Path $resultRoot $record.cycle.relative_root) 'M5-002 cycle root' $true
Assert-NoReparse $cycleRoot
$probe = Get-Probe (Join-Path $cycleRoot 'mcla.log') (Join-Path $cycleRoot 'user') $false
$cycleProperties = @('relative_root','exit_code','close_requested','harness_force_cleanup','runtime_logs','runtime_log_set_sha256','runtime_log_file_count','runtime_log_bytes','cache_read_calls','cache_read_bytes','cache_highest_end','audlo_read_calls','audlo_read_bytes','audlo_highest_end','expected_development_misses','captures')
if (($record.cycle.PSObject.Properties.Name -join ',') -cne ($cycleProperties -join ',') -or $record.cycle.runtime_log_set_sha256 -cne $probe.LogSet.Hash -or $record.cycle.runtime_log_file_count -ne $probe.LogSet.Count -or $record.cycle.runtime_log_bytes -ne $probe.LogSet.Bytes -or ($record.cycle.runtime_logs | ConvertTo-Json -Compress) -cne ($probe.LogSet.Files | ConvertTo-Json -Compress) -or $record.cycle.cache_read_calls -ne $probe.Cache.ReadCalls -or $record.cycle.cache_read_bytes -ne $probe.Cache.ReadBytes -or $record.cycle.cache_highest_end -ne $probe.Cache.HighestEnd -or $record.cycle.audlo_read_calls -ne $probe.Audlo.ReadCalls -or $record.cycle.audlo_read_bytes -ne $probe.Audlo.ReadBytes -or $record.cycle.audlo_highest_end -ne $probe.Audlo.HighestEnd -or $record.cycle.expected_development_misses -ne 7 -or $record.cycle.exit_code -ne 0 -or $record.cycle.close_requested -ne $true -or $record.cycle.harness_force_cleanup -ne $false) { throw 'M5-002 result metrics do not match physical evidence.' }
foreach ($phase in @('title','gameplay','pause','options')) { if ($record.cycle.captures.$phase.sha256 -cne $probe.Frontend.Bmps[$phase].Sha256 -or $record.cycle.captures.$phase.bytes -ne $probe.Frontend.Bmps[$phase].Bytes) { throw 'M5-002 capture/result mismatch.' } }
if (@((Get-Process mcla -ErrorAction SilentlyContinue) | Where-Object { try { [string]::Equals($_.Path,$exe,[StringComparison]::OrdinalIgnoreCase) } catch { $false } }).Count) { throw 'Canonical MCLA process still exists.' }
[pscustomobject]@{ Passed = $true; Decision = $record.decision; CacheReadCalls = $probe.Cache.ReadCalls; CacheReadBytes = $probe.Cache.ReadBytes; AudloReadCalls = $probe.Audlo.ReadCalls; ExpectedDevelopmentMisses = 7; MixedCaseVerified = $true; GameplayVerified = $true; DataIntegrityVerified = $true }
