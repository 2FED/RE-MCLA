[CmdletBinding(DefaultParameterSetName = 'Result')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Result')][string]$ResultPath,
    [Parameter(ParameterSetName = 'Result')][switch]$HistoricalEvidenceOnly,
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$RuntimeLogPath,
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$BmpPath,
    [Parameter(Mandatory, ParameterSetName = 'Probe')][switch]$ProbeOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$m4Verifier = Join-Path $PSScriptRoot 'verify-render-path-smoke.ps1'
$gameVerifier = Join-Path $PSScriptRoot 'verify-game-manifest.ps1'
$skipVerifier = Join-Path $PSScriptRoot 'verify-skip-intro-decision.ps1'
$acceptedM4Result = Join-Path $repoRoot 'private/evidence/M4-002/20260812-085022-cc01a857/result.json'
$canonicalGame = Join-Path $repoRoot 'private/game'
$canonicalBuild = Join-Path $repoRoot 'out/build/win-amd64-relwithdebinfo'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$hashPattern = '^[0-9A-F]{64}$'
$artifactNames = @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll')

function Assert-ExactProperties {
    param([object]$Value, [string[]]$Expected, [string]$Description)
    $actual = @($Value.PSObject.Properties.Name | Sort-Object)
    $wanted = @($Expected | Sort-Object)
    if ($actual.Count -ne $wanted.Count) { throw "$Description has missing or unknown properties." }
    for ($index = 0; $index -lt $wanted.Count; $index++) {
        if ($actual[$index] -cne $wanted[$index]) {
            throw "$Description has missing or unknown properties."
        }
    }
}

function Assert-JsonTypes {
    param([object]$Value, [string[]]$BooleanNames = @(), [string[]]$IntegerNames = @(),
        [string[]]$StringNames = @(), [string]$Description)
    foreach ($name in $BooleanNames) {
        if ($Value.PSObject.Properties[$name].Value -isnot [bool]) {
            throw "$Description '$name' must be a JSON boolean."
        }
    }
    foreach ($name in $IntegerNames) {
        $candidate = $Value.PSObject.Properties[$name].Value
        if ($candidate -isnot [byte] -and $candidate -isnot [sbyte] -and
            $candidate -isnot [int16] -and $candidate -isnot [uint16] -and
            $candidate -isnot [int32] -and $candidate -isnot [uint32] -and
            $candidate -isnot [int64] -and $candidate -isnot [uint64]) {
            throw "$Description '$name' must be a JSON integer."
        }
    }
    foreach ($name in $StringNames) {
        if ($Value.PSObject.Properties[$name].Value -isnot [string]) {
            throw "$Description '$name' must be a JSON string."
        }
    }
}

function Assert-ContainedNonReparsePath {
    param([string]$Path, [string]$Description)
    $full = [System.IO.Path]::GetFullPath($Path)
    $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $full.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay inside the repository."
    }
    $current = $repoRoot
    foreach ($component in @($full.Substring($prefix.Length).Split('\') |
            Where-Object { $_.Length -ne 0 })) {
        $current = Join-Path $current $component
        if (-not (Test-Path -LiteralPath $current)) {
            throw "$Description component is missing: '$current'."
        }
        if ((Get-Item -LiteralPath $current -Force).Attributes -band
            [System.IO.FileAttributes]::ReparsePoint) {
            throw "$Description traverses a reparse point."
        }
    }
    return $full
}

function Assert-ExactChildren {
    param([string]$Root, [string[]]$Expected, [string]$Description)
    $actual = @((Get-ChildItem -LiteralPath $Root -Force | Sort-Object Name).Name)
    $wanted = @($Expected | Sort-Object)
    if ($actual.Count -ne $wanted.Count) { throw "$Description has missing or extra children." }
    for ($index = 0; $index -lt $wanted.Count; $index++) {
        if ($actual[$index] -cne $wanted[$index]) {
            throw "$Description has incorrectly named children."
        }
    }
}

function Get-TreeSnapshot {
    param([string]$Root)
    $items = @(Get-ChildItem -LiteralPath $Root -Recurse -Force)
    foreach ($item in @((Get-Item -LiteralPath $Root -Force)) + $items) {
        if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw 'Evidence or source tree contains a reparse point.'
        }
    }
    $entries = @()
    foreach ($directory in @($items | Where-Object { $_.PSIsContainer } | Sort-Object FullName)) {
        $entries += [ordered]@{
            kind = 'directory'
            path = $directory.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/')
        }
    }
    $files = @($items | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)
    foreach ($file in $files) {
        $entries += [ordered]@{
            kind = 'file'; path = $file.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/')
            length = $file.Length; sha256 = (Get-FileHash $file.FullName -Algorithm SHA256).Hash
        }
    }
    $json = ConvertTo-Json -InputObject @($entries) -Depth 4 -Compress
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') })
    } finally { $sha.Dispose() }
    $bytes = 0L
    foreach ($file in $files) { $bytes += [long]$file.Length }
    [pscustomobject]@{
        Hash = $hash; FileCount = $files.Count
        DirectoryCount = @($items | Where-Object { $_.PSIsContainer }).Count; Bytes = $bytes
    }
}

function Get-GameIdentity {
    $root = Assert-ContainedNonReparsePath $canonicalGame 'Canonical game tree'
    $manifest = Assert-ContainedNonReparsePath (Join-Path $repoRoot 'private/game-manifest.json') `
        'Canonical game manifest'
    $tree = Get-TreeSnapshot $root
    $verified = & $gameVerifier -GamePath $root -ManifestPath $manifest -VerifyHashes
    [pscustomobject]@{
        file_count = $verified.FileCount; payload_bytes = $verified.PayloadBytes
        hashes_verified = $verified.HashesVerified
        manifest_sha256 = (Get-FileHash $manifest -Algorithm SHA256).Hash
        tree_sha256 = $tree.Hash; tree_file_count = $tree.FileCount
        tree_directory_count = $tree.DirectoryCount; tree_bytes = $tree.Bytes
    }
}

function Get-ArtifactSnapshot {
    @($artifactNames | ForEach-Object {
        $path = Assert-ContainedNonReparsePath (Join-Path $canonicalBuild $_) "Artifact $_"
        [pscustomobject]@{ name = $_; sha256 = (Get-FileHash $path -Algorithm SHA256).Hash }
    })
}

function Get-ExactProcesses {
    $executable = [System.IO.Path]::GetFullPath((Join-Path $canonicalBuild 'mcla.exe'))
    @((Get-Process -Name 'mcla' -ErrorAction SilentlyContinue) | Where-Object {
        try { [string]::Equals($_.Path, $executable, [System.StringComparison]::OrdinalIgnoreCase) }
        catch { $false }
    })
}

function Get-RuntimeLogSet {
    param([string]$CurrentLogPath)
    if ((Split-Path -Leaf $CurrentLogPath) -cne 'mcla.log') {
        throw 'Current runtime log must be named mcla.log.'
    }
    $directory = Split-Path -Parent $CurrentLogPath
    $candidates = @(Get-ChildItem -LiteralPath $directory -File -Force -Filter 'mcla*.log')
    if ($candidates.Count -lt 1 -or $candidates.Count -gt 16) {
        throw 'Runtime log set must contain 1-16 files.'
    }
    $current = @($candidates | Where-Object { $_.Name -ceq 'mcla.log' })
    if ($current.Count -ne 1) { throw 'Runtime log set requires exactly one mcla.log.' }
    $rotations = @()
    foreach ($candidate in $candidates) {
        if ($candidate.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw 'Runtime log is a reparse point.'
        }
        if ($candidate.Name -ceq 'mcla.log') { continue }
        $match = [regex]::Match($candidate.Name, '^mcla\.(?<id>[1-9][0-9]*)\.log$')
        if (-not $match.Success) { throw 'Malformed runtime log rotation name.' }
        $rotations += [pscustomobject]@{ Index = [int]$match.Groups['id'].Value; Item = $candidate }
    }
    $indices = @($rotations.Index | Sort-Object)
    for ($index = 0; $index -lt $indices.Count; $index++) {
        if ($indices[$index] -ne $index + 1 -or $indices[$index] -gt 15) {
            throw 'Runtime log rotations are not bounded and contiguous.'
        }
    }
    $ordered = @($rotations | Sort-Object Index -Descending | ForEach-Object { $_.Item }) + $current
    $manifest = @(); $parts = @(); $total = 0L; $previous = [datetime]::MinValue
    foreach ($item in $ordered) {
        if ($item.Length -gt 8388608 -or $item.LastWriteTimeUtc -lt $previous) {
            throw 'Runtime log size or chronological timestamp contract failed.'
        }
        $previous = $item.LastWriteTimeUtc; $total += [long]$item.Length
        if ($total -gt 100663296) { throw 'Runtime log set exceeds 96 MiB.' }
        $manifest += [ordered]@{
            name = $item.Name; bytes = $item.Length
            sha256 = (Get-FileHash $item.FullName -Algorithm SHA256).Hash
        }
        $parts += [System.IO.File]::ReadAllText($item.FullName)
    }
    $manifestJson = ConvertTo-Json -InputObject @($manifest) -Depth 3 -Compress
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $setHash = -join ($sha.ComputeHash($utf8.GetBytes($manifestJson)) |
            ForEach-Object { $_.ToString('X2') })
    } finally { $sha.Dispose() }
    [pscustomobject]@{
        Files = @($manifest); Count = $manifest.Count; Bytes = $total; Hash = $setHash
        Text = ($parts -join [Environment]::NewLine)
    }
}

function Assert-PatchAbsent {
    $decision = & $skipVerifier
    if (-not $decision.Passed -or $decision.PatchImplemented -or $decision.PatchEnabled -or
        -not $decision.PriorAddressAudit) {
        throw 'Prior guarded no-patch decision is no longer intact.'
    }
}

function Get-RouteProbe {
    param([string]$LogPath, [string]$FramePath)
    $resolvedLog = Assert-ContainedNonReparsePath $LogPath 'Runtime log'
    $resolvedFrame = Assert-ContainedNonReparsePath $FramePath 'Capture BMP'
    $m4 = & $m4Verifier -ProbeOnly -RuntimeLogPath $resolvedLog -BmpPath $resolvedFrame
    $set = Get-RuntimeLogSet $resolvedLog
    $log = $set.Text
    $launchMatches = [regex]::Matches($log, [regex]::Escape('KernelState: Preparing module launch...'))
    if ($launchMatches.Count -ne 1) { throw 'Route requires exactly one module-launch marker.' }
    $launch = $launchMatches[0].Index
    $preflightPatterns = @(
        "VFS resolved 'game:\intro720.bik'",
        "VFS resolved 'd:\intro720.bik'",
        "VFS resolved '\Device\Harddisk0\Partition1\intro720.bik'"
    )
    foreach ($pattern in $preflightPatterns) {
        $matches = [regex]::Matches($log, [regex]::Escape($pattern))
        if ($matches.Count -ne 1 -or $matches[0].Index -ge $launch) {
            throw 'Exact intro720 VFS preflight resolutions are missing, duplicated, or misplaced.'
        }
    }
    $allBinkLines = @($log -split "`r?`n" | Where-Object { $_ -match '(?i)\bBink\b|[.]bik\b' })
    if ($allBinkLines.Count -ne 3) {
        throw 'The complete route must contain only the three pre-launch Bink/BIK preflight records.'
    }
    $post = $log.Substring($launch)
    if ($post -match '(?i)\bBink\b|[.]bik\b') {
        throw 'Post-launch Bink/BIK evidence contradicts guest bypass.'
    }
    $createCount = [regex]::Matches($post, '(?m)\[NtCreateFile\] path=').Count
    $readCount = [regex]::Matches($post, '(?m)\[NtReadFile\] handle=').Count
    $successfulReads = [regex]::Matches($post, '(?m)\[NtReadFile\] -> (?:0x)?0\b')
    $capture = $log.IndexOf('MCLA graphics: nontrivial guest frame captured ',
        [System.StringComparison]::Ordinal)
    $coverageFloor = [Math]::Max($launch, $capture - 2097152)
    $coverage = @($successfulReads | Where-Object { ($launch + $_.Index) -ge $coverageFloor }).Count
    if ($createCount -lt 10 -or $readCount -lt 100 -or $successfulReads.Count -lt 100 -or
        $capture -lt $launch -or $coverage -lt 1) {
        throw 'Noisy post-launch guest I/O telemetry is not meaningfully populated.'
    }
    [pscustomobject]@{
        LogSet = $set; M4 = $m4; PreflightCount = 3
        PostLaunchBinkCount = 0; NtCreateFileCount = $createCount; NtReadFileCount = $readCount
        NtReadFileSuccessCount = $successfulReads.Count; TitleCoverageSuccessCount = $coverage
    }
}

if ($PSCmdlet.ParameterSetName -eq 'Probe') {
    if (-not $ProbeOnly) { throw 'Probe inputs require -ProbeOnly.' }
    Assert-PatchAbsent
    Get-RouteProbe -LogPath $RuntimeLogPath -FramePath $BmpPath
    return
}

$resolvedResult = Assert-ContainedNonReparsePath $ResultPath 'Result path'
if ((Split-Path -Leaf $resolvedResult) -cne 'result.json') { throw 'Result must be result.json.' }
$resultItem = Get-Item -LiteralPath $resolvedResult
if ($resultItem.Length -gt 1048576) { throw 'Result JSON exceeds 1 MiB.' }
$json = Get-Content -LiteralPath $resolvedResult -Raw
foreach ($pattern in @('(?i)[A-Z]:[\\/]', '(?i)\\\\[^"\s]+[\\/]', '(?i)(?:^|["\\/])private[\\/]')) {
    if ($json -match $pattern) { throw 'Sanitized result contains a private or absolute path.' }
}
$result = $json | ConvertFrom-Json
$topFields = @('schema','task','decision','claim','cycle_count','capture_timeout_seconds',
    'first_frame_settle_seconds','post_marker_dwell_milliseconds','exit_timeout_seconds',
    'accepted_m4_002','game_identity','artifacts','cycles','patch_implemented','patch_enabled',
    'prior_word_audit','all_write_roots_contained','all_prior_cycles_immutable',
    'no_surviving_processes','data_integrity_preserved','all_title_probes_passed',
    'zero_post_launch_bink')
Assert-ExactProperties $result $topFields 'M4-003 result'
Assert-JsonTypes $result -BooleanNames @('patch_implemented','patch_enabled','prior_word_audit',
    'all_write_roots_contained','all_prior_cycles_immutable','no_surviving_processes',
    'data_integrity_preserved','all_title_probes_passed','zero_post_launch_bink') `
    -IntegerNames @('schema','cycle_count','capture_timeout_seconds','first_frame_settle_seconds',
        'post_marker_dwell_milliseconds','exit_timeout_seconds') `
    -StringNames @('task','decision','claim') -Description 'M4-003 result'
if ($result.schema -ne 1 -or $result.task -ne 'M4-003' -or
    $result.decision -cne 'guest-bypass-no-patch' -or
    $result.claim -cne 'title-reached-with-zero-post-launch-bink-not-playback-proof' -or
    $result.cycle_count -ne 3 -or $result.capture_timeout_seconds -ne 60 -or
    $result.first_frame_settle_seconds -ne 35 -or
    $result.post_marker_dwell_milliseconds -ne 2000 -or $result.exit_timeout_seconds -ne 10 -or
    $result.patch_implemented -or $result.patch_enabled -or -not $result.prior_word_audit) {
    throw 'M4-003 decision header is not the canonical guest-bypass contract.'
}

Assert-ExactProperties $result.accepted_m4_002 @('name','sha256','cycles','verified') `
    'Accepted M4-002 reference'
Assert-JsonTypes $result.accepted_m4_002 -BooleanNames @('verified') -IntegerNames @('cycles') `
    -StringNames @('name','sha256') -Description 'Accepted M4-002 reference'
$acceptedPath = Assert-ContainedNonReparsePath $acceptedM4Result 'Accepted M4-002 result'
$acceptedHash = (Get-FileHash $acceptedPath -Algorithm SHA256).Hash
if ($result.accepted_m4_002.name -cne 'M4-002-result.json' -or
    $result.accepted_m4_002.sha256 -ne $acceptedHash -or
    $result.accepted_m4_002.cycles -ne 10 -or -not $result.accepted_m4_002.verified) {
    throw 'Accepted M4-002 reference is not physically bound.'
}

$runRoot = Split-Path -Parent $resolvedResult
$expectedParent = [System.IO.Path]::GetFullPath((Join-Path $repoRoot 'private/evidence/M4-003'))
if (-not [string]::Equals((Split-Path -Parent $runRoot), $expectedParent,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'M4-003 run must be an immediate child of its private evidence root.'
}
Assert-ExactChildren $runRoot @('result.json','runs') 'M4-003 run root'
$runsRoot = Join-Path $runRoot 'runs'
Assert-ExactChildren $runsRoot @('01','02','03') 'M4-003 runs root'

$identityFields = @('file_count','payload_bytes','hashes_verified','manifest_sha256','tree_sha256',
    'tree_file_count','tree_directory_count','tree_bytes')
Assert-ExactProperties $result.game_identity @('before','after') 'Game identity wrapper'
foreach ($phase in @('before','after')) {
    Assert-ExactProperties $result.game_identity.$phase $identityFields "Game identity $phase"
    Assert-JsonTypes $result.game_identity.$phase `
        -IntegerNames @('file_count','payload_bytes','hashes_verified','tree_file_count','tree_directory_count','tree_bytes') `
        -StringNames @('manifest_sha256','tree_sha256') -Description "Game identity $phase"
    if ($result.game_identity.$phase.hashes_verified -ne $result.game_identity.$phase.file_count -or
        $result.game_identity.$phase.manifest_sha256 -notmatch $hashPattern -or
        $result.game_identity.$phase.tree_sha256 -notmatch $hashPattern) {
        throw "Game identity $phase is not hash-verified."
    }
}
foreach ($field in $identityFields) {
    if ($result.game_identity.before.$field -ne $result.game_identity.after.$field) {
        throw "Game identity '$field' drifted."
    }
}
Assert-ExactProperties $result.artifacts @('before','after') 'Artifact identity wrapper'
foreach ($phase in @('before','after')) {
    $items = @($result.artifacts.$phase)
    if ($items.Count -ne 4) { throw 'Artifact identity must contain four files.' }
    for ($index = 0; $index -lt 4; $index++) {
        Assert-ExactProperties $items[$index] @('name','sha256') "Artifact $phase/$index"
        Assert-JsonTypes $items[$index] -StringNames @('name','sha256') `
            -Description "Artifact $phase/$index"
        if ($items[$index].name -cne $artifactNames[$index] -or
            [string]$items[$index].sha256 -notmatch $hashPattern -or
            $items[$index].sha256 -ne $result.artifacts.before[$index].sha256) {
            throw 'Artifact identity is invalid or drifted.'
        }
    }
}

$cycleFields = @('index','capture_elapsed_milliseconds','dwell_elapsed_milliseconds',
    'exit_elapsed_milliseconds','exit_code','close_requested','harness_force_cleanup',
    'process_signal_confirmed','process_cleanup_confirmed','prior_cycles_immutable',
    'runtime_logs','runtime_log_file_count','runtime_log_bytes','runtime_log_set_sha256',
    'capture_relative_path','capture_sha256','capture_bytes','preflight_resolution_count',
    'post_launch_bink_count','nt_create_file_count','nt_read_file_count',
    'nt_read_file_success_count','title_coverage_success_count',
    'logo_edge_correlation_ppm','press_edge_correlation_ppm','resolve_calls','draw_issued',
    'user_tree_sha256','cache_tree_sha256','cycle_tree_sha256','user_file_count',
    'cache_file_count','user_bytes','cache_bytes')
$cycles = @($result.cycles)
if ($cycles.Count -ne 3) { throw 'Result must contain exactly three cycles.' }
for ($index = 0; $index -lt 3; $index++) {
    $name = '{0:D2}' -f ($index + 1); $cycle = $cycles[$index]
    Assert-ExactProperties $cycle $cycleFields "Cycle $name"
    Assert-JsonTypes $cycle -BooleanNames @('close_requested','harness_force_cleanup',
        'process_signal_confirmed','process_cleanup_confirmed','prior_cycles_immutable') `
        -IntegerNames @('index','capture_elapsed_milliseconds','dwell_elapsed_milliseconds',
            'exit_elapsed_milliseconds','exit_code','runtime_log_file_count','runtime_log_bytes',
            'capture_bytes','preflight_resolution_count','post_launch_bink_count',
            'nt_create_file_count','nt_read_file_count','nt_read_file_success_count',
            'title_coverage_success_count','logo_edge_correlation_ppm',
            'press_edge_correlation_ppm','resolve_calls','draw_issued','user_file_count',
            'cache_file_count','user_bytes','cache_bytes') `
        -StringNames @('runtime_log_set_sha256','capture_relative_path','capture_sha256',
            'user_tree_sha256','cache_tree_sha256','cycle_tree_sha256') -Description "Cycle $name"
    if ($cycle.index -ne $index + 1 -or $cycle.capture_elapsed_milliseconds -lt 0 -or
        $cycle.capture_elapsed_milliseconds -gt 60000 -or $cycle.dwell_elapsed_milliseconds -lt 2000 -or
        $cycle.exit_elapsed_milliseconds -lt 0 -or $cycle.exit_elapsed_milliseconds -gt 10000 -or
        $cycle.exit_code -ne 0 -or -not $cycle.close_requested -or $cycle.harness_force_cleanup -or
        -not $cycle.process_signal_confirmed -or -not $cycle.process_cleanup_confirmed -or
        -not $cycle.prior_cycles_immutable -or $cycle.preflight_resolution_count -ne 3 -or
        $cycle.post_launch_bink_count -ne 0 -or $cycle.nt_create_file_count -lt 10 -or
        $cycle.nt_read_file_count -lt 100 -or $cycle.nt_read_file_success_count -lt 100 -or
        $cycle.title_coverage_success_count -lt 1 -or $cycle.logo_edge_correlation_ppm -lt 900000 -or
        $cycle.press_edge_correlation_ppm -lt 900000 -or $cycle.resolve_calls -lt 700 -or
        $cycle.draw_issued -lt 1) {
        throw "Cycle $name does not satisfy the bounded bypass route."
    }
    $declaredLogs = @($cycle.runtime_logs)
    if ($declaredLogs.Count -lt 1 -or $declaredLogs.Count -gt 16 -or
        $cycle.runtime_log_file_count -ne $declaredLogs.Count) {
        throw "Cycle $name runtime log manifest cardinality is invalid."
    }
    foreach ($declaredLog in $declaredLogs) {
        Assert-ExactProperties $declaredLog @('name','bytes','sha256') "Cycle $name runtime log"
        Assert-JsonTypes $declaredLog -IntegerNames @('bytes') -StringNames @('name','sha256') `
            -Description "Cycle $name runtime log"
        if ($declaredLog.bytes -lt 0 -or $declaredLog.sha256 -notmatch $hashPattern) {
            throw "Cycle $name runtime log manifest is invalid."
        }
    }
    $cycleRoot = Join-Path $runsRoot $name
    Assert-ExactChildren $cycleRoot (@('cache','user') + @($declaredLogs.name)) "Cycle $name root"
    $framePath = Join-Path $cycleRoot 'user/mcla-first-frame.bmp'
    if (@(Get-ChildItem -LiteralPath $cycleRoot -Recurse -File -Force -Filter '*.bmp').Count -ne 1) {
        throw "Cycle $name must contain exactly one physical BMP."
    }
    if ($cycle.capture_relative_path -cne "runs/$name/user/mcla-first-frame.bmp") {
        throw 'Capture relative path is not canonical.'
    }
    $probe = Get-RouteProbe -LogPath (Join-Path $cycleRoot 'mcla.log') -FramePath $framePath
    if ($declaredLogs.Count -ne $probe.LogSet.Files.Count -or
        $cycle.runtime_log_file_count -ne $probe.LogSet.Count -or
        $cycle.runtime_log_bytes -ne $probe.LogSet.Bytes -or
        $cycle.runtime_log_set_sha256 -ne $probe.LogSet.Hash -or
        $cycle.capture_sha256 -ne $probe.M4.Bmp.Sha256 -or
        $cycle.capture_bytes -ne $probe.M4.Bmp.Bytes -or
        $cycle.preflight_resolution_count -ne $probe.PreflightCount -or
        $cycle.post_launch_bink_count -ne $probe.PostLaunchBinkCount -or
        $cycle.nt_create_file_count -ne $probe.NtCreateFileCount -or
        $cycle.nt_read_file_count -ne $probe.NtReadFileCount -or
        $cycle.nt_read_file_success_count -ne $probe.NtReadFileSuccessCount -or
        $cycle.title_coverage_success_count -ne $probe.TitleCoverageSuccessCount -or
        $cycle.logo_edge_correlation_ppm -ne $probe.M4.Roi.LogoCorrelationPpm -or
        $cycle.press_edge_correlation_ppm -ne $probe.M4.Roi.PressCorrelationPpm -or
        $cycle.resolve_calls -ne $probe.M4.Audit.ResolveCalls -or
        $cycle.draw_issued -ne $probe.M4.Audit.DrawIssued) {
        throw "Cycle $name aggregate does not match physical route evidence."
    }
    for ($logIndex = 0; $logIndex -lt $declaredLogs.Count; $logIndex++) {
        foreach ($field in @('name','bytes','sha256')) {
            if ($declaredLogs[$logIndex].$field -ne $probe.LogSet.Files[$logIndex].$field) {
                throw "Cycle $name runtime log manifest is physically inconsistent."
            }
        }
    }
    $user = Get-TreeSnapshot (Join-Path $cycleRoot 'user')
    $cache = Get-TreeSnapshot (Join-Path $cycleRoot 'cache')
    $tree = Get-TreeSnapshot $cycleRoot
    if ($cycle.user_tree_sha256 -ne $user.Hash -or $cycle.cache_tree_sha256 -ne $cache.Hash -or
        $cycle.cycle_tree_sha256 -ne $tree.Hash -or $cycle.user_file_count -ne $user.FileCount -or
        $cycle.cache_file_count -ne $cache.FileCount -or $cycle.user_bytes -ne $user.Bytes -or
        $cycle.cache_bytes -ne $cache.Bytes) {
        throw "Cycle $name physical tree differs from its aggregate."
    }
}

foreach ($name in @('all_write_roots_contained','all_prior_cycles_immutable',
        'no_surviving_processes','data_integrity_preserved','all_title_probes_passed',
        'zero_post_launch_bink')) {
    if (-not $result.$name) { throw "Aggregate flag '$name' is false." }
}
Assert-PatchAbsent
$m4Arguments = @{ ResultPath = $acceptedPath }
if ($HistoricalEvidenceOnly) { $m4Arguments.HistoricalEvidenceOnly = $true }
$m4Result = & $m4Verifier @m4Arguments
if (-not $m4Result.Passed -or $m4Result.Cycles -ne 10) {
    throw 'Accepted M4-002 physical result no longer verifies.'
}
$physicalGame = Get-GameIdentity
foreach ($field in $identityFields) {
    if ($result.game_identity.after.$field -ne $physicalGame.$field) {
        throw "Canonical game identity '$field' differs physically."
    }
}
$acceptedObject = Get-Content -LiteralPath $acceptedPath -Raw | ConvertFrom-Json
for ($index = 0; $index -lt 4; $index++) {
    if ($result.artifacts.after[$index].sha256 -ne $acceptedObject.artifacts.after[$index].sha256) {
        throw "Canonical artifact '$($artifactNames[$index])' differs physically."
    }
}
if (-not $HistoricalEvidenceOnly) {
    $physicalArtifacts = @(Get-ArtifactSnapshot)
    for ($index = 0; $index -lt 4; $index++) {
        if ($result.artifacts.after[$index].sha256 -ne $physicalArtifacts[$index].sha256) {
            throw "Canonical artifact '$($artifactNames[$index])' changed after the gate."
        }
    }
}
if (@(Get-ExactProcesses).Count -ne 0) { throw 'Exact canonical MCLA process survived.' }

[pscustomobject]@{
    Passed = $true; Decision = 'guest-bypass-no-patch'; PlaybackProven = $false
    Cycles = 3; AcceptedM4CyclesReverified = 10; PhysicalTitleProbes = 3
    ProcessCleanupVerified = $true; DataIntegrityVerified = $true
}
