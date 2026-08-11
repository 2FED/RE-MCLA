[CmdletBinding()]
param([Parameter(Mandatory)][string]$ResultPath)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $ResultPath -PathType Leaf)) {
    throw "Launch/exit cycle result was not found: '$ResultPath'."
}
$item = Get-Item -LiteralPath $ResultPath
if ($item.Length -gt 1048576) {
    throw 'Launch/exit cycle result exceeded the reviewed 1-MiB bound.'
}
$json = Get-Content -LiteralPath $ResultPath -Raw
foreach ($pattern in @(
        '(?i)[A-Z]:[\\/]',
        '(?i)\\\\[^"\s]+[\\/]',
        '(?i)(?:^|["\\/])private[\\/]'
    )) {
    if ($json -match $pattern) {
        throw "Launch/exit aggregate contains a prohibited absolute/private path pattern '$pattern'."
    }
}
$result = $json | ConvertFrom-Json
$hashPattern = '^[0-9A-F]{64}$'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$utf8 = [System.Text.UTF8Encoding]::new($false)

function Assert-ContainedNonReparsePath {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Description)
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $fullPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay inside the repository: '$fullPath'."
    }
    $relative = $fullPath.Substring($prefix.Length)
    $current = $repoRoot
    foreach ($component in @($relative.Split('\') | Where-Object { $_.Length -ne 0 })) {
        $current = Join-Path $current $component
        if (-not (Test-Path -LiteralPath $current)) {
            throw "$Description component is missing: '$current'."
        }
        $pathItem = Get-Item -LiteralPath $current -Force
        if ($pathItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "$Description traverses a reparse point: '$current'."
        }
    }
    return $fullPath
}

function Assert-ExactChildren {
    param(
        [Parameter(Mandatory)][string]$Root,
        [Parameter(Mandatory)][string[]]$ExpectedNames,
        [Parameter(Mandatory)][string]$Description
    )
    $actual = @((Get-ChildItem -LiteralPath $Root -Force | Sort-Object Name).Name)
    $expected = @($ExpectedNames | Sort-Object)
    if ($actual.Count -ne $expected.Count) {
        throw "$Description has missing or extra artifacts."
    }
    for ($index = 0; $index -lt $expected.Count; $index++) {
        if ($actual[$index] -cne $expected[$index]) {
            throw "$Description has missing, extra, or incorrectly named artifacts."
        }
    }
}

function Get-EvidenceTreeSnapshot {
    param([Parameter(Mandatory)][string]$Root)
    $allItems = @(Get-ChildItem -LiteralPath $Root -Recurse -Force)
    foreach ($child in $allItems) {
        if ($child.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "Evidence tree contains a reparse point: '$($child.FullName)'."
        }
    }
    $entries = @()
    foreach ($file in @($allItems | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)) {
        $entries += [ordered]@{
            path = $file.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/')
            length = $file.Length
            sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        }
    }
    $serialized = ConvertTo-Json -InputObject @($entries) -Depth 4 -Compress
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $treeHash = -join ($sha.ComputeHash($utf8.GetBytes($serialized)) |
            ForEach-Object { $_.ToString('X2') })
    } finally {
        $sha.Dispose()
    }
    $totalBytes = [long]0
    foreach ($entry in $entries) { $totalBytes += [long]$entry.length }
    [pscustomobject]@{
        Hash = $treeHash
        FileCount = $entries.Count
        Bytes = $totalBytes
    }
}

if ($result.schema -ne 1 -or $result.task -ne 'M3-015' -or
    $result.normal_cycle_count -ne 10 -or $result.crash_probe_cycle_count -ne 10 -or
    $result.execution_order -ne
        'clean_build_then_10_crash_probes_then_10_normal_cycles') {
    throw 'Launch/exit result header does not prove exactly 10/10 cycles.'
}
if ($result.startup_timeout_seconds -lt 10 -or $result.startup_timeout_seconds -gt 60 -or
    $result.exit_timeout_seconds -lt 1 -or $result.exit_timeout_seconds -gt 30 -or
    $result.crash_probe_timeout_seconds -lt 5 -or
    $result.crash_probe_timeout_seconds -gt 60) {
    throw 'Launch/exit result contains invalid timeout bounds.'
}

$build = $result.clean_build
if ($build.performed -ne $true -or $build.success -ne $true -or
    $build.exit_code -ne 0 -or $build.duration_milliseconds -le 0 -or
    [string]$build.build_log_sha256 -notmatch $hashPattern -or
    [string]$build.executable_sha256 -notmatch $hashPattern -or
    $result.first_crash_probe_post_clean_build -ne $true) {
    throw 'The result does not prove a successful clean-first build immediately before cycle 1.'
}

foreach ($phase in @('before', 'after')) {
    $game = $result.game_identity.$phase
    if ($game.file_count -ne 15 -or $game.payload_bytes -ne 6569586392 -or
        $game.hashes_verified -ne 15 -or
        [string]$game.manifest_sha256 -notmatch $hashPattern) {
        throw "Supported game identity '$phase' is incomplete."
    }
}
if ($result.game_identity.before.manifest_sha256 -ne
    $result.game_identity.after.manifest_sha256) {
    throw 'Supported game manifest identity changed across the cycle gate.'
}

$expectedArtifacts = @(
    'mcla.exe',
    'rexruntimerd.dll',
    'TracyClientrd.dll',
    'rexgpu-xenosrd.dll'
)
foreach ($phase in @('before', 'after')) {
    $artifacts = @($result.artifacts.$phase)
    if ($artifacts.Count -ne $expectedArtifacts.Count) {
        throw "Artifact snapshot '$phase' does not contain exactly four reviewed binaries."
    }
    for ($index = 0; $index -lt $expectedArtifacts.Count; $index++) {
        if ($artifacts[$index].name -ne $expectedArtifacts[$index] -or
            [string]$artifacts[$index].sha256 -notmatch $hashPattern) {
            throw "Artifact snapshot '$phase' is invalid or out of order."
        }
    }
}
for ($index = 0; $index -lt $expectedArtifacts.Count; $index++) {
    if ($result.artifacts.before[$index].sha256 -ne $result.artifacts.after[$index].sha256) {
        throw "Artifact '$($expectedArtifacts[$index])' changed during repeated execution."
    }
}
if ($build.executable_sha256 -ne $result.artifacts.before[0].sha256) {
    throw 'Clean-build executable identity does not match the pre-cycle artifact snapshot.'
}

$normalCycles = @($result.normal_cycles)
$crashCycles = @($result.crash_probe_cycles)
if ($normalCycles.Count -ne 10 -or $crashCycles.Count -ne 10) {
    throw 'Launch/exit arrays do not contain exactly ten records each.'
}

$resolvedResult = Assert-ContainedNonReparsePath -Path $ResultPath -Description 'Result path'
if ((Split-Path -Leaf $resolvedResult) -cne 'result.json') {
    throw 'Final cycle evidence must use the exact result.json artifact name.'
}
$runRoot = Split-Path -Parent $resolvedResult
[void](Assert-ContainedNonReparsePath -Path $runRoot -Description 'Run root')
foreach ($child in @(Get-ChildItem -LiteralPath $runRoot -Recurse -Force)) {
    if ($child.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Run evidence contains a reparse point: '$($child.FullName)'."
    }
}
Assert-ExactChildren -Root $runRoot -ExpectedNames @(
    'crash-probes', 'normal-cycles', 'relwithdebinfo-clean-build.log', 'result.json'
) -Description 'Run root'
$buildLogPath = Join-Path $runRoot 'relwithdebinfo-clean-build.log'
if ((Get-FileHash -LiteralPath $buildLogPath -Algorithm SHA256).Hash -ne
    $build.build_log_sha256) {
    throw 'Clean-build log hash does not match the sibling evidence artifact.'
}

$crashPhaseRoot = Join-Path $runRoot 'crash-probes'
$normalPhaseRoot = Join-Path $runRoot 'normal-cycles'
$expectedCycleNames = @(1..10 | ForEach-Object { '{0:D2}' -f $_ })
Assert-ExactChildren -Root $crashPhaseRoot -ExpectedNames $expectedCycleNames `
    -Description 'Crash-probe phase'
Assert-ExactChildren -Root $normalPhaseRoot -ExpectedNames $expectedCycleNames `
    -Description 'Normal-cycle phase'

for ($index = 0; $index -lt 10; $index++) {
    $cycleName = '{0:D2}' -f ($index + 1)
    $crash = $crashCycles[$index]
    $crashRoot = Join-Path $crashPhaseRoot $cycleName
    Assert-ExactChildren -Root $crashRoot -ExpectedNames @('cache', 'mcla.log', 'user') `
        -Description "Crash-probe cycle $cycleName"
    $crashLog = Join-Path $crashRoot 'mcla.log'
    $crashUser = Join-Path $crashRoot 'user'
    $crashCache = Join-Path $crashRoot 'cache'
    $crashReport = Join-Path $crashUser 'mcla-crash-report.txt'
    foreach ($path in @($crashLog, $crashReport)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Crash-probe cycle $cycleName is missing required evidence '$path'."
        }
    }
    $verifiedCrash = & (Join-Path $PSScriptRoot 'verify-crash-report.ps1') `
        -ReportPath $crashReport -RuntimeLogPath $crashLog
    $crashText = Get-Content -LiteralPath $crashLog -Raw
    $hardExitMarker = 'MCLA crash probe: controlled hard exit'
    $hardExitMatches = [regex]::Matches($crashText, [regex]::Escape($hardExitMarker))
    $shutdownOffset = $crashText.IndexOf('MCLA lifecycle: shutdown',
        [System.StringComparison]::Ordinal)
    $hardExitOffset = $crashText.IndexOf($hardExitMarker,
        [System.StringComparison]::Ordinal)
    $afterHardExit = if ($hardExitOffset -ge 0) {
        $crashText.Substring($hardExitOffset + $hardExitMarker.Length).Trim()
    } else { 'missing' }
    if ($hardExitMatches.Count -ne 1 -or $hardExitOffset -le $shutdownOffset -or
        $afterHardExit.Length -ne 0 -or $crash.hard_exit_marker_occurrences -ne 1) {
        throw "Crash-probe cycle $cycleName lacks its exact final hard-exit evidence."
    }
    $crashUserTree = Get-EvidenceTreeSnapshot -Root $crashUser
    $crashCacheTree = Get-EvidenceTreeSnapshot -Root $crashCache
    if ((Get-FileHash -LiteralPath $crashLog -Algorithm SHA256).Hash -ne
            $crash.runtime_log_sha256 -or
        (Get-FileHash -LiteralPath $crashReport -Algorithm SHA256).Hash -ne
            $crash.report_sha256 -or
        $crashUserTree.Hash -ne $crash.user_tree_sha256 -or
        $crashCacheTree.Hash -ne $crash.cache_tree_sha256 -or
        -not $verifiedCrash.Passed -or
        $crash.required_fields -ne $verifiedCrash.RequiredFields -or
        $crash.host_stack_frames -ne $verifiedCrash.HostStackFrames -or
        $crash.guest_memory_included -ne $verifiedCrash.GuestMemoryIncluded -or
        $crash.ordered_markers -ne $verifiedCrash.OrderedMarkers) {
        throw "Crash-probe cycle $cycleName does not match its sibling evidence."
    }

    $normal = $normalCycles[$index]
    $normalRoot = Join-Path $normalPhaseRoot $cycleName
    Assert-ExactChildren -Root $normalRoot -ExpectedNames @('cache', 'mcla.log', 'user') `
        -Description "Normal cycle $cycleName"
    $normalLog = Join-Path $normalRoot 'mcla.log'
    $normalUser = Join-Path $normalRoot 'user'
    $normalCache = Join-Path $normalRoot 'cache'
    if (-not (Test-Path -LiteralPath $normalLog -PathType Leaf)) {
        throw "Normal cycle $cycleName is missing its runtime log."
    }
    $verifiedStartup = & (Join-Path $PSScriptRoot 'verify-startup-smoke.ps1') `
        -RuntimeLogPath $normalLog
    $normalText = Get-Content -LiteralPath $normalLog -Raw
    $windowMarker = 'Window closing, shutting down...'
    $exitMarker = 'Title terminated; hard-exiting process.'
    $windowMatches = [regex]::Matches($normalText, [regex]::Escape($windowMarker))
    $exitMatches = [regex]::Matches($normalText, [regex]::Escape($exitMarker))
    if ($windowMatches.Count -ne 1 -or $exitMatches.Count -ne 1 -or
        $normalText.IndexOf($exitMarker, [System.StringComparison]::Ordinal) -le
            $normalText.IndexOf($windowMarker, [System.StringComparison]::Ordinal)) {
        throw "Normal cycle $cycleName lacks exact ordered close evidence."
    }
    $normalUserTree = Get-EvidenceTreeSnapshot -Root $normalUser
    $normalCacheTree = Get-EvidenceTreeSnapshot -Root $normalCache
    if ((Get-FileHash -LiteralPath $normalLog -Algorithm SHA256).Hash -ne
            $normal.runtime_log_sha256 -or
        $normalUserTree.Hash -ne $normal.user_tree_sha256 -or
        $normalCacheTree.Hash -ne $normal.cache_tree_sha256 -or
        $normalUserTree.FileCount -ne $normal.user_file_count -or
        $normalCacheTree.FileCount -ne $normal.cache_file_count -or
        $normalUserTree.Bytes -ne $normal.user_bytes -or
        $normalCacheTree.Bytes -ne $normal.cache_bytes -or
        -not $verifiedStartup.Passed) {
        throw "Normal cycle $cycleName does not match its sibling evidence."
    }
}

for ($index = 0; $index -lt 10; $index++) {
    $normal = $normalCycles[$index]
    if ($normal.index -ne ($index + 1) -or
        $normal.startup_elapsed_milliseconds -lt 0 -or
        $normal.startup_elapsed_milliseconds -gt ($result.startup_timeout_seconds * 1000) -or
        $normal.exit_elapsed_milliseconds -lt 0 -or
        $normal.exit_elapsed_milliseconds -gt ($result.exit_timeout_seconds * 1000) -or
        $normal.exit_code -ne 0 -or $normal.startup_marker_count -ne 15 -or
        $normal.close_requested -ne $true -or $normal.close_marker_count -ne 2 -or
        $normal.window_close_marker_occurrences -ne 1 -or
        $normal.hard_exit_marker_occurrences -ne 1 -or
        $normal.harness_force_cleanup -ne $false -or
        $normal.process_signal_confirmed -ne $true -or
        $normal.process_cleanup_confirmed -ne $true -or
        $normal.prior_cycles_immutable -ne $true -or
        [string]$normal.runtime_log_sha256 -notmatch $hashPattern -or
        [string]$normal.user_tree_sha256 -notmatch $hashPattern -or
        [string]$normal.cache_tree_sha256 -notmatch $hashPattern -or
        $normal.user_file_count -lt 0 -or $normal.cache_file_count -lt 0 -or
        $normal.user_bytes -lt 0 -or $normal.cache_bytes -lt 0) {
        throw "Normal launch cycle $($index + 1) is incomplete or outside its bounds."
    }

    $crash = $crashCycles[$index]
    if ($crash.index -ne ($index + 1) -or
        $crash.elapsed_milliseconds -lt 0 -or
        $crash.elapsed_milliseconds -gt ($result.crash_probe_timeout_seconds * 1000) -or
        $crash.exit_code -ne 0 -or $crash.report_verified -ne $true -or
        $crash.required_fields -ne 7 -or
        $crash.host_stack_frames -lt 1 -or $crash.host_stack_frames -gt 16 -or
        $crash.guest_memory_included -ne $false -or $crash.ordered_markers -ne 4 -or
        $crash.hard_exit_marker_occurrences -ne 1 -or
        $crash.harness_force_cleanup -ne $false -or
        $crash.process_signal_confirmed -ne $true -or
        $crash.process_cleanup_confirmed -ne $true -or
        $crash.prior_cycles_immutable -ne $true -or
        [string]$crash.runtime_log_sha256 -notmatch $hashPattern -or
        [string]$crash.report_sha256 -notmatch $hashPattern -or
        [string]$crash.user_tree_sha256 -notmatch $hashPattern -or
        [string]$crash.cache_tree_sha256 -notmatch $hashPattern) {
        throw "Crash-probe teardown cycle $($index + 1) is incomplete or outside its bounds."
    }
}

if ($result.all_write_roots_contained -ne $true -or
    $result.all_prior_cycles_immutable -ne $true -or
    $result.no_surviving_processes -ne $true -or
    $result.data_integrity_preserved -ne $true) {
    throw 'Launch/exit aggregate does not prove containment, cleanup, and data integrity.'
}

[pscustomobject]@{
    Passed = $true
    NormalCycles = 10
    CrashProbeCycles = 10
    CleanBuildVerified = $true
    ProcessCleanupVerified = $true
    DataIntegrityVerified = $true
}
