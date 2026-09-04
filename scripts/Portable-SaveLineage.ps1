# Shared, fail-closed developer-side portable save handoff. No runtime hooks.
Set-StrictMode -Version Latest
$script:PortableProfileName = 'B13EBABEBABEBABE'
$script:PortableSavePath = 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
$script:PortableHeaderPath = 'B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'

function Get-PortableSha256([string]$Path) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') }
        finally { $sha.Dispose() }
    } finally { $stream.Dispose() }
}

function Assert-PortablePath([string]$Root, [string]$Path) {
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    $full = [IO.Path]::GetFullPath($Path).TrimEnd('\')
    if ($full -cne $rootFull -and -not $full.StartsWith($rootFull + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Portable handoff path escaped its allowed root.'
    }
    # Check every existing ancestor, not only the final component.
    $cursor = $full
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            if ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw 'Portable handoff path contains a reparse point.'
            }
        }
        $parent = Split-Path -Parent $cursor
        if ($parent -eq $cursor) { break }
        $cursor = $parent
    }
    return $full
}

function Get-PortableTree([string]$Root, [string[]]$TopNames, [long]$MaximumBytes=536870912, [switch]$AllowDiagnosticPartial) {
    $rootFull = Assert-PortablePath $Root $Root
    $pending = [Collections.Generic.Queue[string]]::new()
    foreach ($name in $TopNames) {
        if ($name -notmatch '^[A-Za-z0-9_.-]+$' -or $name -in @('.','..')) { throw 'Invalid tree root name.' }
        $path = Assert-PortablePath $rootFull (Join-Path $rootFull $name)
        if (-not (Test-Path -LiteralPath $path)) { throw "Portable tree root missing: $name" }
        $pending.Enqueue($path)
    }
    $rows = [Collections.Generic.List[object]]::new()
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    [long]$bytes = 0
    while ($pending.Count) {
        $path = $pending.Dequeue()
        $item = Get-Item -LiteralPath $path -Force -ErrorAction Stop
        $relative = $item.FullName.Substring($rootFull.Length + 1).Replace('\','/')
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Portable tree contains a reparse point.' }
        if ($relative -match '(?i)\.sync-conflict-|(^|/)\.syncthing\.' -or
            ($item.Name.EndsWith('.partial',[StringComparison]::OrdinalIgnoreCase) -and -not ($AllowDiagnosticPartial -and $relative.StartsWith('diagnostics/',[StringComparison]::Ordinal)))) {
            throw 'Portable tree contains conflicted or incomplete state.'
        }
        if ($relative -match '[\r\n:|]' -or -not $seen.Add($relative)) { throw 'Portable tree has unsafe or case-colliding paths.' }
        if ($rows.Count -ge 16384) { throw 'Portable tree exceeds the entry bound.' }
        if ($item.PSIsContainer) {
            $rows.Add([pscustomobject][ordered]@{path=$relative;kind='directory';bytes=[long]0;sha256=''})
            foreach ($child in @(Get-ChildItem -LiteralPath $path -Force -ErrorAction Stop)) { $pending.Enqueue($child.FullName) }
        } else {
            $bytes += $item.Length
            if ($bytes -gt $MaximumBytes) { throw 'Portable tree exceeds the byte bound.' }
            $rows.Add([pscustomobject][ordered]@{path=$relative;kind='file';bytes=[long]$item.Length;sha256=(Get-PortableSha256 $path)})
        }
    }
    # Ordinal path ordering avoids locale-dependent archive identities.
    $names = [string[]]@($rows | ForEach-Object { $_.path })
    [Array]::Sort($names, [StringComparer]::Ordinal)
    $byName = @{}; foreach ($row in $rows) { $byName[$row.path] = $row }
    $sorted = @($names | ForEach-Object { $byName[$_] })
    $canonical = (@($sorted | ForEach-Object { '{0}|{1}|{2}|{3}' -f $_.kind,$_.path,$_.bytes,$_.sha256 }) -join "`n") + "`n"
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $digest = ([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($canonical)))).Replace('-','') }
    finally { $sha.Dispose() }
    return [pscustomobject]@{Root=$rootFull;Sha256=$digest;Bytes=$bytes;Entries=$sorted}
}

function Get-PortableProfile([string]$Root) {
    $tree = Get-PortableTree $Root @($script:PortableProfileName)
    foreach ($relative in @($script:PortableSavePath,$script:PortableHeaderPath)) {
        $entry = @($tree.Entries | Where-Object { $_.path -ceq $relative -and $_.kind -eq 'file' })
        if ($entry.Count -ne 1 -or $entry[0].bytes -le 0) { throw 'Portable profile lacks its complete save/header pair.' }
    }
    $tree | Add-Member NoteProperty SaveSha256 (@($tree.Entries | Where-Object path -CEQ $script:PortableSavePath)[0].sha256)
    $tree | Add-Member NoteProperty HeaderSha256 (@($tree.Entries | Where-Object path -CEQ $script:PortableHeaderPath)[0].sha256)
    return $tree
}

function Copy-PortableTreeVerified($Tree, [string]$Destination, [string[]]$TopNames, [long]$MaximumBytes=536870912, [switch]$AllowDiagnosticPartial) {
    if (Test-Path -LiteralPath $Destination) { throw 'Portable copy destination already exists.' }
    $null = Assert-PortablePath (Split-Path -Parent $Destination) $Destination
    [IO.Directory]::CreateDirectory($Destination) | Out-Null
    foreach ($entry in $Tree.Entries) {
        $target = Join-Path $Destination $entry.path.Replace('/','\')
        if ($entry.kind -eq 'directory') { [IO.Directory]::CreateDirectory($target) | Out-Null }
        else {
            [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
            Copy-Item -LiteralPath (Join-Path $Tree.Root $entry.path.Replace('/','\')) -Destination $target -ErrorAction Stop
        }
    }
    $copied = Get-PortableTree $Destination $TopNames -MaximumBytes $MaximumBytes -AllowDiagnosticPartial:$AllowDiagnosticPartial
    $after = Get-PortableTree $Tree.Root $TopNames -MaximumBytes $MaximumBytes -AllowDiagnosticPartial:$AllowDiagnosticPartial
    if ($copied.Sha256 -cne $Tree.Sha256 -or $after.Sha256 -cne $Tree.Sha256) { throw 'Portable tree changed during copy; incomplete copy retained.' }
    return $copied
}

function Assert-PortableRecord($Value, [string[]]$Names, [string]$Label) {
    if ($null -eq $Value -or $Value -isnot [pscustomobject]) { throw "$Label is not a JSON object." }
    $actual = @($Value.PSObject.Properties.Name | Sort-Object)
    if (($actual -join '|') -cne ((@($Names | Sort-Object)) -join '|')) { throw "$Label property topology drifted." }
}

function Read-PortableRecord([string]$Path, [long]$MaximumBytes=65536) {
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
        $item.Length -le 0 -or $item.Length -gt $MaximumBytes) { throw 'Portable JSON record is missing, unsafe or oversized.' }
    return Get-Content -LiteralPath $Path -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
}

function Assert-PortableInteger($Value, [long]$Minimum, [long]$Maximum, [string]$Label) {
    if (($Value -isnot [int] -and $Value -isnot [long]) -or $Value -lt $Minimum -or $Value -gt $Maximum) {
        throw "$Label is not a bounded JSON integer."
    }
}

function Assert-PortableUtcStamp($Value) {
    if ($Value -isnot [string] -or $Value -cnotmatch '^\d{8}T\d{6}Z$') { throw 'Portable session timestamp is malformed.' }
    try { $null = [DateTime]::ParseExact($Value, "yyyyMMdd'T'HHmmss'Z'", [Globalization.CultureInfo]::InvariantCulture) }
    catch { throw 'Portable session timestamp is invalid.' }
    # Clock ordering is deliberately not used to infer save ancestry.
}

function Get-PortableProfileArchive([string]$RepoRoot, [string]$ArchiveRoot) {
    $parent = Assert-PortablePath $RepoRoot (Join-Path $RepoRoot 'private\save-archive\M7-009')
    $root = Assert-PortablePath $parent $ArchiveRoot
    $name = Split-Path -Leaf $root
    if ((Split-Path -Parent $root) -ne $parent -or $name -cnotmatch '^[0-9A-F]{64}$') { throw 'Portable profile archive identity is invalid.' }
    $top = @((Get-ChildItem -LiteralPath $root -Force -ErrorAction Stop).Name | Sort-Object)
    if (($top -join '|') -cne ((@($script:PortableProfileName,'profile.json') | Sort-Object) -join '|')) { throw 'Portable profile archive topology drifted.' }
    $tree = Get-PortableProfile $root
    $record = Read-PortableRecord (Join-Path $root 'profile.json') -MaximumBytes 8388608
    Assert-PortableRecord $record @('schema','profile_sha256','save_sha256','header_sha256','files','provenance') 'Portable profile archive'
    if ($record.schema -cne 'mcla-portable-profile-archive-v1' -or $tree.Sha256 -cne $name -or
        $record.profile_sha256 -cne $tree.Sha256 -or $record.save_sha256 -cne $tree.SaveSha256 -or
        $record.header_sha256 -cne $tree.HeaderSha256 -or $record.provenance -isnot [pscustomobject] -or
        @($record.provenance.PSObject.Properties).Count -lt 1) { throw 'Portable profile archive metadata drifted.' }
    $entries = @($record.files)
    if ($entries.Count -ne $tree.Entries.Count) { throw 'Portable profile archive inventory drifted.' }
    for ($i=0; $i -lt $entries.Count; $i++) {
        $entry = $entries[$i]; $expected = $tree.Entries[$i]
        Assert-PortableRecord $entry @('path','kind','bytes','sha256') 'Portable profile archive entry'
        Assert-PortableInteger $entry.bytes 0 536870912 'Portable archive entry size'
        if ($entry.path -cne $expected.path -or $entry.kind -cne $expected.kind -or
            $entry.bytes -ne $expected.bytes -or $entry.sha256 -cne $expected.sha256) { throw 'Portable profile archive inventory drifted.' }
    }
    return $tree
}

function Save-PortableProfileArchive([string]$RepoRoot, $Tree, $Provenance) {
    $parent = Assert-PortablePath $RepoRoot (Join-Path $RepoRoot 'private\save-archive\M7-009')
    [IO.Directory]::CreateDirectory($parent) | Out-Null
    $destination = Join-Path $parent $Tree.Sha256
    if (Test-Path -LiteralPath $destination) {
        $existing = Get-PortableProfileArchive $RepoRoot $destination
        if ($existing.Sha256 -cne $Tree.Sha256) { throw 'Existing content-addressed profile archive drifted.' }
    } else {
        $partial = $destination + '.' + [guid]::NewGuid().ToString('N') + '.partial'
        $copied = Copy-PortableTreeVerified $Tree $partial @($script:PortableProfileName)
        $record = [ordered]@{schema='mcla-portable-profile-archive-v1';profile_sha256=$Tree.Sha256;save_sha256=$Tree.SaveSha256;header_sha256=$Tree.HeaderSha256;files=$Tree.Entries;provenance=$Provenance}
        [IO.File]::WriteAllText((Join-Path $partial 'profile.json'),($record|ConvertTo-Json -Depth 12),[Text.UTF8Encoding]::new($false))
        Move-PortableDirectory $partial $destination
    }
    $null = Get-PortableProfileArchive $RepoRoot $destination
    return $destination
}

function Resolve-PortableLineage($Candidates, $Edges, [string]$ExplicitSource) {
    $byTree = @{}; foreach ($candidate in $Candidates) { $byTree[$candidate.Profile.Sha256] = $candidate }
    if (-not $byTree.Count) { throw 'No validated profile candidates.' }
    $children = @{}
    foreach ($edge in $Edges) {
        if ($edge.From -ceq $edge.To) { continue }
        if (-not $children.ContainsKey($edge.From)) { $children[$edge.From] = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal) }
        $null = $children[$edge.From].Add($edge.To)
    }
    # Iterative path search catches cycles and only eliminates proven ancestors.
    $ancestors = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $vertices = @(@($byTree.Keys) + @($Edges | ForEach-Object { $_.From; $_.To }) | Sort-Object -Unique)
    if ($vertices.Count -gt 8192) { throw 'Portable lineage exceeds its node bound.' }
    foreach ($start in $vertices) {
        $pending = [Collections.Generic.Queue[string]]::new(); $pending.Enqueue($start)
        $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        $null = $seen.Add($start)
        while ($pending.Count) {
            $node = $pending.Dequeue()
            if (-not $children.ContainsKey($node)) { continue }
            foreach ($child in $children[$node]) {
                if ($child -ceq $start) { throw 'Portable save lineage has a cycle; explicit recovery review required.' }
                if ($byTree.ContainsKey($child)) { $null = $ancestors.Add($start) }
                if ($seen.Add($child)) { $pending.Enqueue($child) }
            }
        }
    }
    if ($ExplicitSource) {
        $chosen = @($Candidates | Where-Object { $_.Source -ceq $ExplicitSource })
        if ($chosen.Count -ne 1) { throw 'Explicit save source is not one validated candidate.' }
        return $chosen[0]
    }
    $tips = @($byTree.Keys | Where-Object { -not $ancestors.Contains($_) })
    if ($tips.Count -ne 1) {
        $names = @($Candidates | Where-Object { $tips -ccontains $_.Profile.Sha256 } | ForEach-Object Source) -join ', '
        throw "Portable save branches are ambiguous. Preserve all branches; owner must select -SaveSource explicitly: $names"
    }
    return $byTree[$tips[0]]
}

function Get-PortableReturnedSession([string]$SessionRoot, $BundleManifest) {
    $sessionId = Split-Path -Leaf $SessionRoot
    if ($sessionId -cnotmatch '^session-\d{8}T\d{6}Z-\d+$') { throw 'Portable session is incomplete or malformed.' }
    $top = @((Get-ChildItem -LiteralPath $SessionRoot -Force -ErrorAction Stop).Name | Sort-Object)
    if (($top -join '|') -cne 'result.json|saves|session.json') { throw 'Portable finalized session topology drifted.' }
    $start = Read-PortableRecord (Join-Path $SessionRoot 'session.json')
    $result = Read-PortableRecord (Join-Path $SessionRoot 'result.json')
    Assert-PortableRecord $start @('schema','bundle_id','session_id','started_utc','mode','state') 'Portable running-session manifest'
    $common = @('schema','bundle_id','session_id','started_utc','mode','exit_code','verified_file_count','save_snapshot_count','final_save_sha256','final_header_sha256','state')
    if ($result.schema -ceq 'mcla-portable-session-result-v1') {
        Assert-PortableRecord $result ($common + @('completed_utc','save_watcher_error_count')) 'Portable completed-session result'
        Assert-PortableUtcStamp $result.completed_utc
        Assert-PortableInteger $result.exit_code 0 4294967295 'Portable game exit code'
        Assert-PortableInteger $result.save_watcher_error_count 0 0 'Portable save watcher error count'
        if ($result.state -cne 'complete') { throw 'Portable completed-session state is invalid.' }
        # A nonzero game exit does not invalidate a separately verified persisted save.
    } elseif ($result.schema -ceq 'mcla-portable-interrupted-session-result-v1') {
        Assert-PortableRecord $result ($common + @('recovered_utc','interruption_reason')) 'Portable interrupted-session result'
        Assert-PortableUtcStamp $result.recovered_utc
        if ($result.state -cne 'interrupted' -or $null -ne $result.exit_code -or
            $result.interruption_reason -cne 'launcher-terminated-before-session-finalization') { throw 'Portable interrupted-session state is invalid.' }
    } else { throw 'Unsupported portable session result schema.' }
    Assert-PortableUtcStamp $start.started_utc
    Assert-PortableUtcStamp $result.started_utc
    # The launcher takes these two clock readings separately. A second boundary
    # may lie between the directory name and started_utc; only IDs must match.
    Assert-PortableUtcStamp $sessionId.Substring(8,16)
    Assert-PortableInteger $result.verified_file_count 1 16384 'Portable verified file count'
    Assert-PortableInteger $result.save_snapshot_count 1 32 'Portable retained snapshot count'
    if ($start.schema -cne 'mcla-portable-session-v1' -or $start.state -cne 'running' -or
        $start.bundle_id -cne $BundleManifest.bundle_id -or $result.bundle_id -cne $BundleManifest.bundle_id -or
        $start.session_id -cne $sessionId -or $result.session_id -cne $sessionId -or
        $start.started_utc -cne $result.started_utc -or $start.mode -cne $result.mode -or
        $start.mode -cnotin @('gameplay','diagnostics-probe') -or
        [string]$result.final_save_sha256 -cnotmatch '^[0-9A-F]{64}$' -or
        [string]$result.final_header_sha256 -cnotmatch '^[0-9A-F]{64}$' -or
        $result.verified_file_count -ne $BundleManifest.immutable_file_count) { throw 'Portable returned session identity drifted.' }
    $snapshots = @(Get-ChildItem -LiteralPath (Join-Path $SessionRoot 'saves') -Force -ErrorAction Stop)
    if ($snapshots.Count -ne $result.save_snapshot_count) { throw 'Portable snapshot inventory drifted.' }
    $starts = @(); $finals = @()
    foreach ($snapshot in $snapshots) {
        if (-not $snapshot.PSIsContainer -or $snapshot.Name -cnotmatch '^[0-9A-F]{16}$') { throw 'Portable snapshot identity is malformed.' }
        $snapshotTop = @((Get-ChildItem -LiteralPath $snapshot.FullName -Force -ErrorAction Stop).Name | Sort-Object)
        if (($snapshotTop -join '|') -cne ((@($script:PortableProfileName,'snapshot.json') | Sort-Object) -join '|')) { throw 'Portable snapshot topology drifted.' }
        $s = Read-PortableRecord (Join-Path $snapshot.FullName 'snapshot.json')
        Assert-PortableRecord $s @('schema','captured_utc','reason','save_sha256','header_sha256','complete_profile_tree') 'Portable save snapshot'
        Assert-PortableUtcStamp $s.captured_utc
        $tree = Get-PortableProfile $snapshot.FullName
        if ($s.schema -cne 'mcla-portable-save-snapshot-v1' -or $s.complete_profile_tree -isnot [bool] -or
            $s.complete_profile_tree -ne $true -or $s.reason -cnotin @('launcher-start','changed-during-session','process-exit','launcher-recovery') -or
            $s.save_sha256 -cne $tree.SaveSha256 -or $s.header_sha256 -cne $tree.HeaderSha256 -or
            $snapshot.Name -cne $tree.SaveSha256.Substring(0,16)) { throw 'Portable snapshot hashes or metadata drifted.' }
        if ($s.reason -ceq 'launcher-start') { $starts += $tree }
        if ($tree.SaveSha256 -ceq $result.final_save_sha256 -and $tree.HeaderSha256 -ceq $result.final_header_sha256) { $finals += $tree }
    }
    if ($starts.Count -ne 1 -or $finals.Count -ne 1) {
        throw 'Portable session lacks one retained full-profile start and final snapshot; explicit recovery review required.'
    }
    return [pscustomobject]@{SessionId=$sessionId;StartProfile=$starts[0];FinalProfile=$finals[0];State=[string]$result.state;ExitCode=$result.exit_code}
}

function Resolve-PortableSessionTip($Sessions, $LiveProfile) {
    $nodes = @{}; $children = @{}; $parents = @{}
    foreach ($session in $Sessions) {
        $from = $session.StartProfile.Sha256; $to = $session.FinalProfile.Sha256
        $nodes[$from] = $session.StartProfile; $nodes[$to] = $session.FinalProfile
        foreach ($key in @($from,$to)) {
            if (-not $children.ContainsKey($key)) {
                $children[$key] = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
                $parents[$key] = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
            }
        }
        if ($from -ceq $to) { continue }
        $null = $children[$from].Add($to); $null = $parents[$to].Add($from)
    }
    if (-not $nodes.Count) { throw 'Portable session lineage is empty.' }
    foreach ($key in @($nodes.Keys)) {
        if ($children[$key].Count -gt 1 -or $parents[$key].Count -gt 1) { throw 'Portable session lineage branches or merges; preserve every return.' }
    }
    $roots = @($nodes.Keys | Where-Object { $parents[$_].Count -eq 0 })
    $tips = @($nodes.Keys | Where-Object { $children[$_].Count -eq 0 })
    if ($roots.Count -ne 1 -or $tips.Count -ne 1) { throw 'Portable session lineage is disconnected or cyclic.' }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $node = $roots[0]
    while ($true) {
        if (-not $seen.Add($node)) { throw 'Portable session lineage is cyclic.' }
        if (-not $children[$node].Count) { break }
        $node = @($children[$node])[0]
    }
    if ($seen.Count -ne $nodes.Count) { throw 'Portable session lineage contains an unconnected cycle.' }
    if ($LiveProfile.Sha256 -cne $tips[0]) { throw 'Portable live profile is not the unique completed-session tip; synchronization or recovery is incomplete.' }
    $finalIds = @($Sessions | Where-Object { $_.FinalProfile.Sha256 -ceq $tips[0] } | ForEach-Object SessionId | Sort-Object)
    return [pscustomobject]@{RootProfile=$nodes[$roots[0]];SessionIds=$finalIds}
}

function Get-PortableBundleReturn([string]$RepoRoot, [string]$BundleRoot, [switch]$Fixture) {
    $null = Assert-PortablePath $RepoRoot $BundleRoot
    $stateBefore = Get-PortableReturnTree $BundleRoot
    $null = & (Join-Path $PSScriptRoot 'verify-portable-campaign-bundle.ps1') -BundleRoot $BundleRoot -ReturnedProfile -Fixture:$Fixture
    $manifest = Get-Content -LiteralPath (Join-Path $BundleRoot 'bundle-manifest.json') -Raw | ConvertFrom-Json
    $relative = [string]$manifest.selected_save.source_snapshot
    if ($relative -cnotmatch '^private/save-archive/(M6-014/.+|M7-009/[0-9A-F]{64})$' -or $relative -match '(^|/)\.\.?(/|$)|\\|:') { throw 'Portable seed provenance is unsafe.' }
    $seedRoot = Assert-PortablePath (Join-Path $RepoRoot 'private\save-archive') (Join-Path $RepoRoot $relative.Replace('/','\'))
    $hasProfileHash = $null -ne $manifest.selected_save.PSObject.Properties['profile_sha256']
    if ($relative.StartsWith('private/save-archive/M7-009/',[StringComparison]::Ordinal)) {
        if (-not $hasProfileHash) { throw 'M7 portable seed requires a bound full-profile hash.' }
        $seed = Get-PortableProfileArchive $RepoRoot $seedRoot
    } else {
        $seed = Get-PortableProfile $seedRoot
    }
    if ($seed.SaveSha256 -cne $manifest.selected_save.save_sha256 -or $seed.HeaderSha256 -cne $manifest.selected_save.header_sha256) { throw 'Portable seed archive identity drifted.' }
    if ($hasProfileHash -and $seed.Sha256 -cne $manifest.selected_save.profile_sha256) { throw 'Portable full-profile seed provenance drifted.' }
    # Legacy manifests bind only two files, not the complete seed tree. They may
    # be preserved, but cannot eliminate another candidate as a proven ancestor.
    $provenSeed = $hasProfileHash
    $profile = Get-PortableProfile (Join-Path $BundleRoot 'user')
    $sessions = @()
    foreach ($item in @(Get-ChildItem -LiteralPath (Join-Path $BundleRoot 'results') -Force -ErrorAction Stop)) {
        if ($item.PSIsContainer -and $item.Name -cmatch '^session-\d{8}T\d{6}Z-\d+$') { $sessions += $item; continue }
        if ($item.PSIsContainer -or $item.Name -cnotmatch '^portable-verification-(\d{8}T\d{6}Z)\.json$') { throw 'Portable results contain unrecognized or incomplete session state.' }
        Assert-PortableUtcStamp $Matches[1]
        $verification = Read-PortableRecord $item.FullName
        Assert-PortableRecord $verification @('schema','bundle_id','verified_utc','verified_file_count','relocatable_root_verified','decision') 'Portable verification result'
        Assert-PortableUtcStamp $verification.verified_utc
        Assert-PortableInteger $verification.verified_file_count 1 16384 'Portable verification count'
        if ($verification.schema -cne 'mcla-portable-verification-v1' -or $verification.bundle_id -cne $manifest.bundle_id -or
            $verification.verified_file_count -ne $manifest.immutable_file_count -or $verification.relocatable_root_verified -isnot [bool] -or
            $verification.relocatable_root_verified -ne $true -or $verification.decision -cne 'portable-bundle-integrity-pass') { throw 'Portable verification record is invalid.' }
    }
    $sessionId = $null
    $sessionIds = @()
    if (-not $sessions.Count) {
        if ($profile.Sha256 -cne $seed.Sha256) { throw 'Changed portable profile has no completed session provenance.' }
    } else {
        if ($sessions.Count -gt 32) { throw 'Portable completed-session inventory exceeds its retention bound.' }
        $validated = @($sessions | ForEach-Object { Get-PortableReturnedSession $_.FullName $manifest })
        $tip = Resolve-PortableSessionTip $validated $profile
        if ($tip.RootProfile.Sha256 -cne $seed.Sha256) { throw 'Portable retained session history does not start at the bundle seed; explicit recovery review required.' }
        $sessionIds = @($tip.SessionIds)
        if ($sessionIds.Count -eq 1) { $sessionId = $sessionIds[0] }
    }
    $stateAfter = Get-PortableReturnTree $BundleRoot
    if ($stateAfter.Sha256 -cne $stateBefore.Sha256) { throw 'Portable returned state changed during interpretation; preserve it and finish synchronization.' }
    $ancestors = @(); if ($manifest.selected_save.PSObject.Properties['ancestor_profile_sha256']) { $ancestors = @($manifest.selected_save.ancestor_profile_sha256) }
    return [pscustomobject]@{Source=[string]$manifest.bundle_id;Profile=$profile;Seed=$seed;ProvenSeed=$provenSeed;SeedAncestors=$ancestors;BundleRoot=$BundleRoot;SessionId=$sessionId;SessionIds=$sessionIds;StateSha256=$stateBefore.Sha256;ManifestSha256=(Get-PortableSha256 (Join-Path $BundleRoot 'bundle-manifest.json'))}
}

# Locks serialize local launchers/publishers, not another Syncthing host. The
# operator must finish synchronization and stop every writer before publishing.
function Enter-PortablePublication([string]$RepoRoot, [string]$OutputRoot) {
    $root = Assert-PortablePath (Join-Path $RepoRoot 'private') $OutputRoot
    if ($root -eq (Join-Path $RepoRoot 'private')) { throw 'Publication root must be a child of private/.' }
    [IO.Directory]::CreateDirectory($root) | Out-Null
    $state = [pscustomobject]@{Root=$root;Locks=[Collections.Generic.List[object]]::new();Bundles=@();Pointer=$null}
    try {
        Add-PortableWriterLock $state (Join-Path $root '.publication.lock')
        foreach ($item in @(Get-ChildItem -LiteralPath $root -Force)) {
            $null = Assert-PortablePath $root $item.FullName
            if ($item.Name -cin @('.stfolder','.stignore','.publication.lock','current.txt')) { continue }
            if (-not $item.PSIsContainer -or $item.Name -cnotmatch '^mcla-\d+\.\d+\.\d+\.\d+-win64-(?:[0-9A-F]{16}-){2}[0-9A-F]{16}$') {
                throw 'Unrecognized or incomplete bundle publication; retain it for recovery review.'
            }
            Add-PortableWriterLock $state (Join-Path $item.FullName 'bundle.lock')
            $state.Bundles += $item.FullName
        }
        $pointer = Join-Path $root 'current.txt'
        if (Test-Path -LiteralPath $pointer) {
            $state.Pointer = [IO.File]::ReadAllText($pointer)
            if (-not (@($state.Bundles | ForEach-Object { Split-Path -Leaf $_ }) -ccontains $state.Pointer.Trim())) { throw 'current.txt does not select one complete child.' }
        } elseif ($state.Bundles.Count) { throw 'Existing children have no current.txt; explicit recovery review required.' }
        return $state
    } catch { Exit-PortablePublication $state; throw }
}

function Add-PortableWriterLock($State, [string]$Path) {
    $null = Assert-PortablePath $State.Root $Path
    # Do not remove a stale lock automatically. It can be a delayed remote copy.
    $stream = [IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
    $State.Locks.Add([pscustomobject]@{Path=$Path;Stream=$stream})
}

function Move-PortableDirectory([string]$Source, [string]$Destination) {
    $parent = [IO.Path]::GetFullPath((Split-Path -Parent $Source)).TrimEnd('\')
    if ([IO.Path]::GetFullPath((Split-Path -Parent $Destination)).TrimEnd('\') -cne $parent -or
        (Split-Path -Leaf $Source) -cnotmatch '\.partial$' -or (Split-Path -Leaf $Destination) -cnotmatch '^[0-9A-F]{64}$') { throw 'Unsafe archive rename targets.' }
    $null = Assert-PortablePath $parent $Source
    $null = Assert-PortablePath $parent $Destination
    # Antivirus/indexers can briefly retain Windows directory handles after
    # hashing. Retry only rename failures; never replace an existing archive.
    for ($attempt=0; $attempt -lt 5; $attempt++) {
        try { [IO.Directory]::Move($Source,$Destination); return }
        catch {
            $cause = $_.Exception.GetBaseException()
            if ($cause -isnot [IO.IOException] -and $cause -isnot [UnauthorizedAccessException]) { throw }
            if ($attempt -eq 4 -or (Test-Path -LiteralPath $Destination) -or -not (Test-Path -LiteralPath $Source)) { throw }
            Start-Sleep -Milliseconds 100
        }
    }
}

function Exit-PortablePublication($State) {
    if (-not $State) { return }
    foreach ($lock in @($State.Locks.ToArray()) | Sort-Object { $_.Path.Length } -Descending) {
        if ($null -ne $lock.Stream) {
            $lock.Stream.Dispose(); $lock.Stream = $null
            Remove-Item -LiteralPath $lock.Path -Force -ErrorAction Stop
        }
    }
}

function Get-PortableReturnTree([string]$Root) {
    Get-PortableTree $Root @('bundle-manifest.json','bundle-files.sha256','bundle-id.txt','game-manifest.json','user','logs','diagnostics','results','metadata','update') -MaximumBytes 4294967296 -AllowDiagnosticPartial
}

function Save-PortableReturnArchive([string]$RepoRoot, $Return) {
    $tree = Get-PortableReturnTree $Return.BundleRoot
    if ($tree.Sha256 -cne $Return.StateSha256) { throw 'Returned state changed after interpretation; keep the current bundle.' }
    $parent = Assert-PortablePath $RepoRoot (Join-Path $RepoRoot ('private\evidence\M7-016\returned-bundles\' + $Return.Source))
    [IO.Directory]::CreateDirectory($parent) | Out-Null
    $destination = Join-Path $parent $tree.Sha256
    if (-not (Test-Path -LiteralPath $destination)) {
        $partial = $destination + '.' + [guid]::NewGuid().ToString('N') + '.partial'
        $null = Copy-PortableTreeVerified $tree $partial @('bundle-manifest.json','bundle-files.sha256','bundle-id.txt','game-manifest.json','user','logs','diagnostics','results','metadata','update') -MaximumBytes 4294967296 -AllowDiagnosticPartial
        Move-PortableDirectory $partial $destination
    }
    if ((Get-PortableReturnTree $destination).Sha256 -cne $tree.Sha256) { throw 'Reused portable return archive drifted.' }
    return [pscustomobject]@{Source=$Return.BundleRoot;Archive=$destination;Sha256=$tree.Sha256}
}

function Assert-PortablePublicationUnchanged($State, $Receipts) {
    $expectedChildren = @($State.Locks | Where-Object { (Split-Path -Leaf $_.Path) -ceq 'bundle.lock' } | ForEach-Object { Split-Path -Leaf (Split-Path -Parent $_.Path) })
    foreach ($item in @(Get-ChildItem -LiteralPath $State.Root -Force)) {
        $null = Assert-PortablePath $State.Root $item.FullName
        if ($item.Name -cin @('.stfolder','.stignore','.publication.lock','current.txt')) { continue }
        if (-not ($expectedChildren -ccontains $item.Name)) { throw 'Output inventory changed during publication.' }
    }
    $pointer = Join-Path $State.Root 'current.txt'
    $value = if (Test-Path -LiteralPath $pointer) { [IO.File]::ReadAllText($pointer) } else { $null }
    if ($value -cne $State.Pointer) { throw 'current.txt changed during publication; refusing to replace it.' }
    foreach ($receipt in $Receipts) {
        if ((Get-PortableReturnTree $receipt.Source).Sha256 -cne $receipt.Sha256 -or
            (Get-PortableReturnTree $receipt.Archive).Sha256 -cne $receipt.Sha256) { throw 'Returned state/archive changed; keep all bundles and the old pointer.' }
    }
}

function New-PortableSavePlan([string]$RepoRoot, $State, [string]$LegacySnapshot, [string]$SaveSource, [switch]$Fixture) {
    $legacy = Get-PortableProfile $LegacySnapshot
    $candidates = @([pscustomobject]@{Source='legacy';Profile=$legacy})
    $edges = @(); $receipts = @()
    foreach ($bundle in $State.Bundles) {
        $returned = Get-PortableBundleReturn $RepoRoot $bundle -Fixture:$Fixture
        $candidates += $returned
        if ($returned.ProvenSeed) {
            $edges += [pscustomobject]@{From=$returned.Seed.Sha256;To=$returned.Profile.Sha256}
            foreach ($ancestor in $returned.SeedAncestors) { $edges += [pscustomobject]@{From=$ancestor;To=$returned.Seed.Sha256} }
        }
        $receipts += Save-PortableReturnArchive $RepoRoot $returned
    }
    # Archive every branch before selection, including an ambiguous result.
    foreach ($candidate in $candidates) {
        $archive = Save-PortableProfileArchive $RepoRoot $candidate.Profile ([ordered]@{source=$candidate.Source})
        $candidate | Add-Member NoteProperty Archive $archive
    }
    $chosen = Resolve-PortableLineage $candidates $edges $SaveSource
    # Carry previously proven ancestry in the next immutable manifest so pruning
    # a predecessor never erases the connection to the desktop baseline.
    $ancestry = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $pending = [Collections.Generic.Queue[string]]::new(); $pending.Enqueue($chosen.Profile.Sha256)
    while ($pending.Count) {
        $node = $pending.Dequeue()
        foreach ($edge in @($edges | Where-Object { $_.To -ceq $node -and $_.From -cne $node })) {
            if ($ancestry.Add($edge.From)) { $pending.Enqueue($edge.From) }
        }
    }
    if ($ancestry.Count -gt 256 -or $ancestry.Contains($chosen.Profile.Sha256)) { throw 'Save ancestry is oversized or cyclic.' }
    Assert-PortablePublicationUnchanged $State $receipts
    return [pscustomobject]@{Snapshot=$chosen.Archive;Profile=$chosen.Profile;Source=$chosen.Source;Receipts=$receipts;Ancestors=@($ancestry | Sort-Object)}
}

function Publish-PortablePointer($State, [string]$BundleId) {
    Assert-PortablePublicationUnchanged $State @()
    $child = Assert-PortablePath $State.Root (Join-Path $State.Root $BundleId)
    if ($BundleId -cnotmatch '^mcla-\d+\.\d+\.\d+\.\d+-win64-(?:[0-9A-F]{16}-){2}[0-9A-F]{16}$' -or -not (Test-Path -LiteralPath $child -PathType Container)) { throw 'Cannot publish an invalid or missing successor.' }
    $path = Join-Path $State.Root 'current.txt'
    $temporary = Join-Path $State.Root ('.current.'+[guid]::NewGuid().ToString('N')+'.partial')
    [IO.File]::WriteAllText($temporary,($BundleId+"`n"),[Text.UTF8Encoding]::new($false))
    # Same-directory rename/replace: on failure the prior pointer remains intact.
    if (Test-Path -LiteralPath $path) { [IO.File]::Replace($temporary,$path,[NullString]::Value) }
    else { [IO.File]::Move($temporary,$path) }
}

function Remove-PortableArchivedBundles($State, [string]$CurrentBundle, $Receipts) {
    $current = Assert-PortablePath $State.Root $CurrentBundle
    if ((Split-Path -Parent $current) -cne $State.Root -or -not (Test-Path -LiteralPath $current -PathType Container)) { throw 'Retention current child is invalid.' }
    $removed = 0
    foreach ($bundle in $State.Bundles) {
        if ($bundle -ceq $current) { continue }
        $pointer = Join-Path $State.Root 'current.txt'
        if (-not (Test-Path -LiteralPath $pointer -PathType Leaf) -or [IO.File]::ReadAllText($pointer).Trim() -cne (Split-Path -Leaf $current)) { throw 'Refusing to prune: current.txt does not advertise the validated successor.' }
        $target = Assert-PortablePath $State.Root $bundle
        if ((Split-Path -Parent $target) -cne $State.Root) { throw 'Retention target escaped its exact parent.' }
        $receipt = @($Receipts | Where-Object { $_.Source -ceq $target })
        $owned = @($State.Locks | Where-Object { $_.Path -ceq (Join-Path $target 'bundle.lock') -and $null -ne $_.Stream })
        if ($receipt.Count -ne 1 -or $owned.Count -ne 1) { throw 'Refusing to prune without one verified archive and an owned writer lock.' }
        if ((Get-PortableReturnTree $target).Sha256 -cne $receipt[0].Sha256 -or (Get-PortableReturnTree $receipt[0].Archive).Sha256 -cne $receipt[0].Sha256) { throw 'Retention archive/source changed; nothing removed.' }
        # Audit every path before recursive deletion, including disposable cache.
        $pending = [Collections.Generic.Queue[string]]::new(); $pending.Enqueue($target)
        while ($pending.Count) {
            foreach ($item in @(Get-ChildItem -LiteralPath $pending.Dequeue() -Force -ErrorAction Stop)) {
                $null = Assert-PortablePath $target $item.FullName
                if ($item.PSIsContainer) { $pending.Enqueue($item.FullName) }
            }
        }
        foreach ($item in @(Get-ChildItem -LiteralPath $target -Force | Where-Object Name -CNE 'bundle.lock')) {
            $null = Assert-PortablePath $target $item.FullName
            Remove-Item -LiteralPath $item.FullName -Recurse -Force -ErrorAction Stop
        }
        # The executable is gone before releasing the lock. Never delete a
        # running launcher's lock, even when pruning encounters an error.
        $owned[0].Stream.Dispose(); $owned[0].Stream=$null
        Remove-Item -LiteralPath $owned[0].Path -Force
        [IO.Directory]::Delete($target,$false)
        $removed++
    }
    return $removed
}
