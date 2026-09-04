[CmdletBinding()]
param(
    [string]$OutputRoot,
    [switch]$MaterializeGameFiles,
    [switch]$SkipCleanBuild,
    [switch]$SkipRelocationProof,
    [switch]$PruneOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $OutputRoot) { $OutputRoot = Join-Path $repo 'private\bundles\M7-016' }
$output = [IO.Path]::GetFullPath($OutputRoot)
$privateRoot = [IO.Path]::GetFullPath((Join-Path $repo 'private'))
if (-not $output.StartsWith($privateRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Portable campaign bundles must remain below ignored private/ output.'
}
$build = Join-Path $repo 'out\build\win-amd64-release'
$game = Join-Path $repo 'private\game'
$gameManifestPath = Join-Path $repo 'private\game-manifest.json'
$version = ([IO.File]::ReadAllText((Join-Path $repo 'VERSION'))).Trim()
$utf8 = [Text.UTF8Encoding]::new($false)
$runId = (Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8)
$evidence = Join-Path $repo "private\evidence\M7-016\bundle-$runId"
[IO.Directory]::CreateDirectory($evidence) | Out-Null
$buildLog = Join-Path $evidence 'release-clean-build.log'
$saveRelative = 'B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav'
$headerRelative = 'B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header'

function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') }
        finally { $sha.Dispose() }
    } finally { $stream.Dispose() }
}

function Write-AtomicText([string]$Path, [string]$Text) {
    $temporary = $Path + '.tmp-' + [guid]::NewGuid().ToString('N')
    [IO.File]::WriteAllText($temporary, $Text, $utf8)
    Move-Item -LiteralPath $temporary -Destination $Path -Force
}

function Resolve-LatestSave {
    $candidates = @()
    foreach ($pointer in @(Get-ChildItem -LiteralPath (Join-Path $repo 'private\save-archive\M6-014') -Recurse -File -Filter 'latest.json')) {
        try {
            $value = Get-Content -LiteralPath $pointer.FullName -Raw | ConvertFrom-Json
            if ($value.schema -cne 'mcla-soak-save-snapshot-v1' -or $value.complete_profile_tree -ne $true -or
                [string]$value.snapshot_directory -cnotmatch '^\d{8}-\d{6}Z-[0-9A-F]{16}-[0-9A-F]{16}$') { continue }
            $captured = [DateTimeOffset]::Parse([string]$value.captured_utc, [Globalization.CultureInfo]::InvariantCulture)
            $snapshot = [IO.Path]::GetFullPath((Join-Path $pointer.DirectoryName ([string]$value.snapshot_directory)))
            if (-not $snapshot.StartsWith($pointer.DirectoryName.TrimEnd('\') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { continue }
            $save = Join-Path $snapshot $saveRelative.Replace('/','\')
            $header = Join-Path $snapshot $headerRelative.Replace('/','\')
            if (-not (Test-Path -LiteralPath $save -PathType Leaf) -or -not (Test-Path -LiteralPath $header -PathType Leaf)) { continue }
            if ((Get-Sha256 $save) -cne [string]$value.save_sha256 -or (Get-Sha256 $header) -cne [string]$value.header_sha256) { continue }
            $candidates += [pscustomobject]@{ Captured=$captured; Pointer=$pointer.FullName; Snapshot=$snapshot; Value=$value }
        } catch { continue }
    }
    if (-not $candidates.Count) { throw 'No complete, physically verified M6-014 progressed-save snapshot exists.' }
    return $candidates | Sort-Object Captured -Descending | Select-Object -First 1
}

function Copy-OrLink([string]$Source, [string]$Destination, [switch]$Materialize) {
    [IO.Directory]::CreateDirectory((Split-Path -Parent $Destination)) | Out-Null
    if (-not $Materialize) {
        try {
            New-Item -ItemType HardLink -Path $Destination -Target $Source -ErrorAction Stop | Out-Null
            return 'ntfs-hardlink'
        } catch {}
    }
    Copy-Item -LiteralPath $Source -Destination $Destination
    return 'materialized-copy'
}

function Clone-Relocated([string]$SourceRoot, [string]$DestinationRoot) {
    [IO.Directory]::CreateDirectory($DestinationRoot) | Out-Null
    foreach ($item in @(Get-ChildItem -LiteralPath $SourceRoot -Recurse -Force)) {
        $relative = $item.FullName.Substring($SourceRoot.Length).TrimStart('\')
        $target = Join-Path $DestinationRoot $relative
        if ($item.PSIsContainer) { [IO.Directory]::CreateDirectory($target) | Out-Null }
        else {
            [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
            try { New-Item -ItemType HardLink -Path $target -Target $item.FullName -ErrorAction Stop | Out-Null }
            catch { Copy-Item -LiteralPath $item.FullName -Destination $target }
        }
    }
}

function Remove-ObsoleteBundles([string]$BundleParent, [string]$CurrentBundle) {
    $parent = [IO.Path]::GetFullPath($BundleParent).TrimEnd('\')
    $current = [IO.Path]::GetFullPath($CurrentBundle).TrimEnd('\')
    if (-not $parent.StartsWith($privateRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
        -not $current.StartsWith($parent + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing to prune portable bundles outside the validated private output root.'
    }
    $removed = 0
    foreach ($candidate in @(Get-ChildItem -LiteralPath $parent -Directory -Force)) {
        $resolved = [IO.Path]::GetFullPath($candidate.FullName).TrimEnd('\')
        # Accept the early two-fingerprint prototype as well as the current
        # three-fingerprint identity so successful publications prune both.
        if ($resolved -ceq $current -or $candidate.Name -cnotmatch '^mcla-\d+\.\d+\.\d+\.\d+-win64-(?:[0-9A-F]{16}-){1,3}[0-9A-F]{16}$') { continue }
        if ((Split-Path -Parent $resolved).TrimEnd('\') -cne $parent) { throw 'Portable retention candidate escaped its parent.' }
        if (Test-Path -LiteralPath (Join-Path $resolved 'bundle.lock') -PathType Leaf) {
            Write-Warning "Retaining obsolete bundle with an active/stale writer lock: '$resolved'."
            continue
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
        $removed++
    }
    return $removed
}

if ($PruneOnly) {
    if (-not (Test-Path -LiteralPath $output -PathType Container)) { throw 'Portable bundle output root is missing.' }
    $pointer = Join-Path $output 'current.txt'
    if (-not (Test-Path -LiteralPath $pointer -PathType Leaf)) { throw 'Portable current.txt is missing.' }
    $currentId = [IO.File]::ReadAllText($pointer).Trim()
    if ($currentId -cnotmatch '^mcla-\d+\.\d+\.\d+\.\d+-win64-(?:[0-9A-F]{16}-){2}[0-9A-F]{16}$') { throw 'Portable current.txt identity is invalid.' }
    $currentBundle = [IO.Path]::GetFullPath((Join-Path $output $currentId))
    if ((Split-Path -Parent $currentBundle).TrimEnd('\') -cne $output.TrimEnd('\') -or
        -not (Test-Path -LiteralPath $currentBundle -PathType Container)) { throw 'Portable current bundle is missing or escaped its parent.' }
    $removedBundles = Remove-ObsoleteBundles $output $currentBundle
    Write-Host "M7-016 RETENTION: removed $removedBundles obsolete bundle(s); current plus current.txt retained." -ForegroundColor DarkCyan
    [pscustomobject]@{ Passed=$true; CurrentBundle=$currentBundle; RemovedBundles=$removedBundles }
    return
}

Write-Host 'M7-016 [1/6]: validating the exact private game and latest complete progressed save...' -ForegroundColor Cyan
$gameIdentity = & (Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath $game -ManifestPath $gameManifestPath -VerifyHashes
$selected = Resolve-LatestSave
$selectedSave = Join-Path $selected.Snapshot $saveRelative.Replace('/','\')
$selectedHeader = Join-Path $selected.Snapshot $headerRelative.Replace('/','\')
$selectedSaveHash = Get-Sha256 $selectedSave
$selectedHeaderHash = Get-Sha256 $selectedHeader

if (-not $SkipCleanBuild) {
    Write-Host 'M7-016 [2/6]: clean-building the toolchain-free Release runtime and native launcher...' -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath | Out-Null
    & cmake --preset win-amd64-release *>&1 | Tee-Object -FilePath $buildLog | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Release configure failed. Private evidence: '$evidence'." }
    & cmake --build --preset win-amd64-release --target mcla --clean-first --parallel 8 *>&1 | Tee-Object -FilePath $buildLog -Append | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Release build failed. Private evidence: '$evidence'." }
} else {
    [IO.File]::WriteAllText($buildLog, "clean build deliberately skipped`r`n", $utf8)
}

$launcherSource = Join-Path $build 'Launch-MCLA.exe'
$mclaSource = Join-Path $build 'mcla.exe'
foreach ($required in @($launcherSource,$mclaSource,(Join-Path $build 'mcla_crash_handler.exe'),(Join-Path $build 'rexruntime.dll'),(Join-Path $build 'TracyClient.dll'),(Join-Path $build 'rexgpu-xenos.dll'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required Release artifact is missing: '$required'." }
}
$mclaHash = Get-Sha256 $mclaSource
$launcherHash = Get-Sha256 $launcherSource
$bundleId = 'mcla-{0}-win64-{1}-{2}-{3}' -f $version,$mclaHash.Substring(0,16),$launcherHash.Substring(0,16),$selectedSaveHash.Substring(0,16)
$final = Join-Path $output $bundleId
$partial = Join-Path $output ('.' + $bundleId + '.' + [guid]::NewGuid().ToString('N').Substring(0,8) + '.partial')
if (Test-Path -LiteralPath $final) { throw "The exact bundle already exists and will not be overwritten: '$final'." }
[IO.Directory]::CreateDirectory($output) | Out-Null
if (Test-Path -LiteralPath $partial) { throw "Unexpected bundle staging collision: '$partial'." }

Write-Host 'M7-016 [3/6]: staging one self-contained Syncthing folder with the selected save...' -ForegroundColor Cyan
$copyModes = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
try {
    foreach ($directory in @('bin','game','user','cache','logs','diagnostics','results','update','metadata')) {
        [IO.Directory]::CreateDirectory((Join-Path $partial $directory)) | Out-Null
    }
    $null = $copyModes.Add((Copy-OrLink $launcherSource (Join-Path $partial 'Launch-MCLA.exe')))
    foreach ($name in @('mcla.exe','mcla_crash_handler.exe','rexruntime.dll','TracyClient.dll','rexgpu-xenos.dll')) {
        $null = $copyModes.Add((Copy-OrLink (Join-Path $build $name) (Join-Path $partial ('bin\' + $name))))
    }
    $portableConfig = [IO.File]::ReadAllText((Join-Path $repo 'config\mcla.toml.example'))
    $portableConfig = $portableConfig.Replace('mcla_diagnostics_root = ""','mcla_diagnostics_root = "diagnostics"')
    $portableConfig = $portableConfig.Replace('log_file = ""','log_file = "logs/mcla.log"')
    if (-not $portableConfig.Contains('mcla_diagnostics_root = "diagnostics"') -or
        -not $portableConfig.Contains('log_file = "logs/mcla.log"') -or
        -not $portableConfig.Contains('fullscreen = true')) { throw 'Portable config specialization failed.' }
    [IO.File]::WriteAllText((Join-Path $partial 'bin\mcla.toml'), $portableConfig, $utf8)
    Copy-Item -LiteralPath $gameManifestPath -Destination (Join-Path $partial 'game-manifest.json')

    $gameManifest = Get-Content -LiteralPath $gameManifestPath -Raw | ConvertFrom-Json
    foreach ($entry in @($gameManifest.Files)) {
        $source = Join-Path $game ([string]$entry.Path).Replace('/','\')
        $destination = Join-Path (Join-Path $partial 'game') ([string]$entry.Path).Replace('/','\')
        $null = $copyModes.Add((Copy-OrLink $source $destination -Materialize:$MaterializeGameFiles))
    }
    $sourceProfile = Join-Path $selected.Snapshot 'B13EBABEBABEBABE'
    Copy-Item -LiteralPath $sourceProfile -Destination (Join-Path $partial 'user') -Recurse

    [IO.File]::WriteAllText((Join-Path $partial 'bundle-id.txt'), $bundleId + "`n", $utf8)
    $sourceSnapshotRelative = $selected.Snapshot.Substring($repo.TrimEnd('\').Length + 1).Replace('\','/')
    $immutableCount = 4 + 6 + [int]$gameIdentity.FileCount
    $manifest = [ordered]@{
        schema = 'mcla-portable-campaign-bundle-v1'
        task = 'M7-016'
        bundle_id = $bundleId
        project_version = $version
        sdk_version = '0.10.0.2'
        sdk_commit = '492614eec92c31f11d75dd8fa0f09785cbae4a66'
        created_utc = [DateTime]::UtcNow.ToString('O')
        platform = 'windows-proton'
        architecture = 'x86_64'
        launcher = 'Launch-MCLA.exe'
        window_mode = [ordered]@{ startup_fullscreen=$true; alt_enter_toggle=$true; left_double_click_toggle=$true }
        immutable_file_count = $immutableCount
        game = [ordered]@{ manifest_sha256=(Get-Sha256 (Join-Path $partial 'game-manifest.json')); file_count=[int]$gameIdentity.FileCount; payload_bytes=[long]$gameIdentity.PayloadBytes }
        selected_save = [ordered]@{ source_snapshot=$sourceSnapshotRelative; save_relative=$saveRelative; header_relative=$headerRelative; save_sha256=$selectedSaveHash; header_sha256=$selectedHeaderHash }
        writable_roots = @('user','cache','logs','diagnostics','results','update','metadata')
        retention = [ordered]@{ completed_sessions=32; save_snapshots_per_session=32 }
        private_content = [ordered]@{ game_and_save_private=$true; source_iso_included=$false }
    }
    [IO.File]::WriteAllText((Join-Path $partial 'bundle-manifest.json'), (($manifest | ConvertTo-Json -Depth 8) + "`n"), $utf8)

    $immutable = @('Launch-MCLA.exe','bundle-id.txt','bundle-manifest.json','game-manifest.json')
    $immutable += @('mcla.exe','mcla_crash_handler.exe','mcla.toml','rexgpu-xenos.dll','rexruntime.dll','TracyClient.dll' | ForEach-Object { 'bin/' + $_ })
    $immutable += @($gameManifest.Files | ForEach-Object { 'game/' + ([string]$_.Path).Replace('\','/') })
    if ($immutable.Count -ne $immutableCount) { throw 'Immutable bundle inventory cardinality drifted.' }
    $rows = foreach ($relative in @($immutable | Sort-Object)) {
        $path = Join-Path $partial $relative.Replace('/','\')
        '{0} *{1}' -f (Get-Sha256 $path),$relative
    }
    [IO.File]::WriteAllText((Join-Path $partial 'bundle-files.sha256'), (($rows -join "`n") + "`n"), $utf8)

    Write-Host 'M7-016 [4/6]: verifying staged hashes, privacy boundaries, roots, and native launcher contract...' -ForegroundColor Cyan
    $staged = & (Join-Path $PSScriptRoot 'verify-portable-campaign-bundle.ps1') -BundleRoot $partial
    [IO.Directory]::Move($partial, $final)
    Write-AtomicText (Join-Path $output 'current.txt') ($bundleId + "`n")

    $relocation = $null
    $relocatedVerification = $null
    $relocatedSession = $null
    $relocatedSessionVerification = $null
    if (-not $SkipRelocationProof) {
        Write-Host 'M7-016 [5/6]: proving native launch and diagnostics from a clean relocated Windows path...' -ForegroundColor Cyan
        $relocation = Join-Path ([IO.Path]::GetTempPath()) ('MCLA Relocation ' + [guid]::NewGuid().ToString('N').Substring(0,8))
        if (Test-Path -LiteralPath $relocation) { throw 'Relocation proof path unexpectedly exists.' }
        Clone-Relocated $final $relocation
        try {
            $relocatedVerification = & (Join-Path $PSScriptRoot 'verify-portable-campaign-bundle.ps1') -BundleRoot $relocation -RunLauncherVerify
            $probe = Start-Process -FilePath (Join-Path $relocation 'Launch-MCLA.exe') -ArgumentList '--diagnostics-probe' -WorkingDirectory $relocation -Wait -PassThru
            if ($probe.ExitCode -ne 0) { throw "Relocated diagnostics probe exited $($probe.ExitCode)." }
            $sessions = @(Get-ChildItem -LiteralPath (Join-Path $relocation 'results') -Directory | Sort-Object LastWriteTimeUtc -Descending)
            if ($sessions.Count -ne 1 -or $sessions[0].Name.EndsWith('.partial')) { throw 'Relocated probe did not atomically publish exactly one completed session.' }
            $relocatedSessionVerification = & (Join-Path $PSScriptRoot 'verify-portable-campaign-session.ps1') -BundleRoot $relocation -SessionPath $sessions[0].FullName -RequireDiagnosticsProbe
            $relocatedSession = Get-Content -LiteralPath (Join-Path $sessions[0].FullName 'result.json') -Raw | ConvertFrom-Json
            $latestLive = Join-Path $relocation 'diagnostics\latest-live.txt'
            if (-not (Test-Path -LiteralPath $latestLive -PathType Leaf)) { throw 'Relocated diagnostics probe did not publish the sibling diagnostics root.' }
            Copy-Item -LiteralPath (Join-Path $relocation 'results') -Destination (Join-Path $evidence 'relocated-results') -Recurse
            Copy-Item -LiteralPath (Join-Path $relocation 'diagnostics') -Destination (Join-Path $evidence 'relocated-diagnostics') -Recurse
            Copy-Item -LiteralPath (Join-Path $relocation 'logs') -Destination (Join-Path $evidence 'relocated-logs') -Recurse
        } finally {
            $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + [IO.Path]::DirectorySeparatorChar
            $resolvedRelocation = [IO.Path]::GetFullPath($relocation)
            if ($resolvedRelocation.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase) -and (Split-Path -Leaf $resolvedRelocation).StartsWith('MCLA Relocation ')) {
                if (Test-Path -LiteralPath $resolvedRelocation) { Remove-Item -LiteralPath $resolvedRelocation -Recurse -Force }
            } else { throw 'Refusing to clean an unsafe relocation proof path.' }
        }
    }

    Write-Host 'M7-016 [6/6]: persisting the private bundle result...' -ForegroundColor Cyan
    $result = [ordered]@{
        schema = 'mcla-portable-campaign-bundle-result-v1'
        task = 'M7-016'
        decision = if ($SkipRelocationProof) { 'portable-bundle-built-relocation-pending' } else { 'portable-windows-bundle-and-relocation-pass' }
        run_id = $runId
        bundle_id = $bundleId
        bundle_path = $final.Substring($repo.TrimEnd('\').Length + 1).Replace('\','/')
        project_version = $version
        sdk_version = '0.10.0.2'
        sdk_commit = '492614eec92c31f11d75dd8fa0f09785cbae4a66'
        bundle_manifest_sha256 = Get-Sha256 (Join-Path $final 'bundle-manifest.json')
        bundle_lock_sha256 = Get-Sha256 (Join-Path $final 'bundle-files.sha256')
        immutable_file_count = $staged.ImmutableFiles
        game_file_count = $staged.GameFiles
        game_payload_bytes = $staged.GameBytes
        selected_save_sha256 = $selectedSaveHash
        selected_header_sha256 = $selectedHeaderHash
        selected_snapshot = $sourceSnapshotRelative
        publication_atomic = $true
        writable_roots_local = $true
        source_iso_included = $false
        copy_modes = @($copyModes | Sort-Object)
        clean_build = [bool](-not $SkipCleanBuild)
        relocation_proof = [bool](-not $SkipRelocationProof)
        relocated_launcher_verified = if ($relocatedVerification) { [bool]$relocatedVerification.LauncherVerified } else { $false }
        relocated_session_verified = if ($relocatedSessionVerification) { [bool]$relocatedSessionVerification.Passed } else { $false }
        relocated_diagnostics_exit_code = if ($relocatedSession) { [int]$relocatedSession.exit_code } else { $null }
    }
    $resultPath = Join-Path $evidence 'result.json'
    [IO.File]::WriteAllText($resultPath, (($result | ConvertTo-Json -Depth 8) + "`n"), $utf8)
    $removedBundles = Remove-ObsoleteBundles $output $final
    Write-Host "M7-016 RETENTION: removed $removedBundles obsolete bundle(s); current plus current.txt retained." -ForegroundColor DarkCyan
    Write-Host "M7-016 WINDOWS BUNDLE PASS: '$final'." -ForegroundColor Green
    [pscustomobject]@{ Passed=$true; Decision=$result.decision; BundleId=$bundleId; BundlePath=$final; ResultPath=$resultPath }
} catch {
    if (Test-Path -LiteralPath $partial -PathType Container) {
        Write-Warning "Incomplete staging retained for diagnosis: '$partial'."
    }
    throw
}
