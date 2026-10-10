[CmdletBinding()]
param(
    [string]$OutputRoot,
    [switch]$MaterializeGameFiles,
    [switch]$SkipCleanBuild,
    [switch]$SkipRelocationProof,
    [switch]$PruneOnly,
    [switch]$EnableF10Wgc,
    [string]$CppWinRtIncludeDir,
    [ValidateNotNullOrEmpty()][string]$RexglueInstallPrefix,
    [ValidateSet(1,2)][int]$ResolutionScale = 1,
    [string]$SaveSource,
    [ValidateNotNullOrEmpty()][string]$SaveArchive
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
. (Join-Path $PSScriptRoot 'Portable-SaveLineage.ps1')
. (Join-Path $PSScriptRoot 'Portable-CppRuntime.ps1')

function Assert-PortableBuildOptions([Collections.IDictionary]$Options) {
    if (@($Options.Keys) -contains 'PruneOnly' -and [bool]$Options['PruneOnly']) {
        foreach ($name in @('EnableF10Wgc','CppWinRtIncludeDir','RexglueInstallPrefix','ResolutionScale','SkipCleanBuild','SkipRelocationProof','MaterializeGameFiles')) {
            if (@($Options.Keys) -contains $name) { throw "PruneOnly cannot be combined with explicit build/settings option '$name'." }
        }
    }
    if (@($Options.Keys) -contains 'RexglueInstallPrefix' -and @($Options.Keys) -contains 'SkipCleanBuild' -and [bool]$Options['SkipCleanBuild']) {
        throw 'An explicit SDK install prefix requires a clean build.'
    }
}

function Resolve-PortableSdkInstall([string]$Prefix) {
    if ($Prefix -notmatch '^(?:[A-Za-z]:[\\/]|\\\\[^\\]+\\[^\\]+\\)' -or $Prefix -match '[;\r\n]') {
        throw 'SDK install prefix must be one absolute directory, not a CMake path list.'
    }
    $full = [IO.Path]::GetFullPath($Prefix).TrimEnd('\','/')
    $null = Assert-PortablePath $full $full
    if (-not (Test-Path -LiteralPath $full -PathType Container)) { throw 'SDK install prefix directory is missing.' }
    $package = Join-Path $full 'lib/cmake/rexglue'
    $pins = @()
    foreach ($relative in @('lib/cmake/rexglue/rexglueConfig.cmake','lib/cmake/rexglue/rexglueConfigVersion.cmake','lib/cmake/rexglue/rexglueTargets.cmake','lib/cmake/rexglue/rexglueTargets-release.cmake','bin/rexruntime.dll','bin/rexgpu-xenos.dll','lib/rexruntime.lib','lib/rexgpu-xenos.lib')) {
        $path = Assert-PortablePath $full (Join-Path $full $relative)
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "SDK install input missing: $relative" }
        $pins += [pscustomobject]@{ RelativePath=$relative; Sha256=(Get-PortableSha256 $path) }
    }
    $versionText = [IO.File]::ReadAllText((Join-Path $package 'rexglueConfigVersion.cmake'))
    if ([regex]::Matches($versionText, '(?m)^set\(PACKAGE_VERSION "0\.20\.0\.0"\)\r?$').Count -ne 1) { throw 'Explicit SDK package version must be exactly 0.20.0.0.' }
    return [pscustomobject]@{ Prefix=$full; PackageDirectory=$package; Pins=$pins }
}

function Assert-PortableSdkCache([string]$CacheText, [string]$Prefix) {
    $expectedPrefix = [IO.Path]::GetFullPath($Prefix).Replace('\','/').TrimEnd('/')
    foreach ($key in @('CMAKE_PREFIX_PATH','rexglue_DIR','REXSDK_DIR')) {
        $rows = [regex]::Matches($CacheText, ('(?m)^' + [regex]::Escape($key) + ':(?:PATH|STRING|UNINITIALIZED)=([^\r\n]*)\r?$'))
        if ($rows.Count -ne 1) { throw "SDK cache must contain exactly one '$key' entry." }
        $value = $rows[0].Groups[1].Value.Replace('\','/').TrimEnd('/')
        $expected = switch ($key) { 'CMAKE_PREFIX_PATH' { $expectedPrefix }; 'rexglue_DIR' { $expectedPrefix + '/lib/cmake/rexglue' }; 'REXSDK_DIR' { '' } }
        if (-not [string]::Equals($value, $expected, [StringComparison]::OrdinalIgnoreCase)) { throw "Explicit SDK cache mismatch: $key" }
    }
}

function Assert-PortableProfilePathBudget([string]$StagedRoot, [string]$FinalRoot, [object[]]$ProfileEntries) {
    # Pure admission policy for the current ordinary Win32 filesystem calls,
    # not long-path support. Count UTF-16 characters excluding the terminator:
    # MAX_PATH permits 259 for files; reserve MAX_PATH-12 for directory creation
    # conservatively (247 characters plus NUL). Never modify registry/policy.
    $roots = @{}
    foreach ($pair in @(@('staged',$StagedRoot),@('final',$FinalRoot))) {
        if ($pair[1] -cnotmatch '^[A-Za-z]:[\\/]' -or $pair[1] -match '[\x00-\x1f"<>|?*]') { throw 'Portable path budget requires ordinary absolute local roots.' }
        $roots[$pair[0]] = [IO.Path]::GetFullPath($pair[1]).TrimEnd('\','/')
    }
    if (-not $ProfileEntries.Count) { throw 'Portable path budget requires the complete selected profile inventory.' }
    $budget = [pscustomobject]@{MaximumFileCharacters=0;MaximumDirectoryCharacters=0;FileLimit=259;DirectoryLimit=247;Scope='selected full-profile entries and known native save/rename/diagnostic nesting; not a long-path guarantee'}
    $measure = {
        param([string]$Base,[string]$Relative,[string]$Kind,[string]$Context)
        $path = $Base + '\' + $Relative.Replace('/','\')
        $directoryLength = if ($Kind -ceq 'directory') { $path.Length } else { $path.LastIndexOf('\') }
        if ($directoryLength -gt $budget.MaximumDirectoryCharacters) { $budget.MaximumDirectoryCharacters = $directoryLength }
        if ($Kind -ceq 'file' -and $path.Length -gt $budget.MaximumFileCharacters) { $budget.MaximumFileCharacters = $path.Length }
        if ($directoryLength -gt $budget.DirectoryLimit -or ($Kind -ceq 'file' -and $path.Length -gt $budget.FileLimit)) {
            throw "Portable path budget exceeded for $Context (file $($path.Length), directory $directoryLength characters). Choose a shorter OutputRoot; the short TEMP relocation proof does not validate this destination."
        }
    }
    $pidText = '4294967295' # uint32 process ID, never an observed short PID.
    $stamp = '20000101T000000Z'
    $session = 'results/session-' + $stamp + '-' + $pidText + '.partial/'
    $snapshot = $session + 'saves/0000000000000000.partial/'
    $rename = 'results/profile-rename-' + $stamp + '-' + $pidText + '.partial/'
    # mcla_diagnostics.cpp Capture uses a uint64 sequence; the crash helper has
    # no sequence. Both CopySaveTrees implementations retain title-relative paths.
    $live = 'diagnostics/live/live-' + $stamp + '-' + $pidText + '-18446744073709551615.partial/'
    $crash = 'diagnostics/crash/crash-' + $stamp + '-' + $pidText + '.partial/'
    $entries = @($ProfileEntries) + @([pscustomobject]@{path='B13EBABEBABEBABE/local-display-name.txt';kind='file'})
    foreach ($entry in $entries) {
        if ($null -eq $entry -or $entry.path -isnot [string] -or $entry.kind -cnotin @('file','directory') -or
            $entry.path -cnotmatch '^B13EBABEBABEBABE(?:/[^/]+)*$' -or $entry.path -match '[\\\x00-\x1f:"<>|?*]' -or
            @($entry.path.Split('/') | Where-Object { $_ -cin @('.','..') }).Count) {
            throw 'Portable path budget contains an unsafe profile entry.'
        }
        & $measure $roots.staged ('user/' + $entry.path) $entry.kind 'staged user tree'
        foreach ($prefix in @('user/',$snapshot,($rename+'before/'),($rename+'after/'))) {
            & $measure $roots.final ($prefix + $entry.path) $entry.kind 'final native profile/snapshot tree'
        }
        if ($entry.path.StartsWith('B13EBABEBABEBABE/545407F8/',[StringComparison]::Ordinal)) {
            foreach ($prefix in @($live,$crash)) { & $measure $roots.final ($prefix+'save-private/'+$entry.path) $entry.kind 'final diagnostic copied-save tree' }
        }
    }
    foreach ($relative in @(
        ($session+'session.json.tmp-'+$pidText),($session+'result.json.tmp-'+$pidText),($snapshot+'snapshot.json'),
        ($rename+'rename.json.tmp-'+$pidText),('results/portable-verification-'+$stamp+'.json.tmp-'+$pidText),
        'user/B13EBABEBABEBABE/local-display-name.partial',
        ($live+'save-files-private.json'),($crash+'runtime-journal-private.log')
    )) { & $measure $roots.final $relative 'file' 'final native metadata' }
    return $budget
}

function New-PortableConfig([string]$Template, [int]$Scale) {
    if ($Scale -notin @(1,2) -or $Template.Length -gt 1MB -or $Template.Contains([string][char]0)) { throw 'Invalid portable settings input.' }
    $keys = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $lines = [Collections.Generic.List[string]]::new()
    foreach ($line in ($Template -split '\r?\n')) {
        if ($line -match '^\s*(#.*)?$') { $lines.Add($line); continue }
        # Exactly the shipped flat single-line scalar subset. Quoted keys,
        # tables, multiline strings and compound values cannot hide overrides.
        if ($line -cnotmatch '^\s*([A-Za-z0-9_-]+)\s*=\s*("(?:[^"\\\r\n]|\\[^\r\n])*"|''[^''\r\n]*''|[-+A-Za-z0-9_.]+)\s*(?:#.*)?$') { throw 'Portable config template must contain flat scalar assignments.' }
        $key=$Matches[1];$value=$Matches[2]
        if ($key -in @('mcla_diagnostics_root','log_file','fullscreen') -and $key -cne $key.ToLowerInvariant()) { throw 'Required portable key spelling changed.' }
        if (-not $keys.Add($key)) { throw "Duplicate portable config key: $key" }
        if ($key -in @('resolution_scale','vsync','gpu_backend','d3d12_adapter','local_user_display_name')) { throw 'Portable video/profile defaults conflict with launcher specialization.' }
        switch -CaseSensitive ($key) {
            'mcla_diagnostics_root' { if ($value -cne '""') { throw 'Diagnostic root template changed.' };$lines.Add('mcla_diagnostics_root = "diagnostics"') }
            'log_file' { if ($value -cne '""') { throw 'Log root template changed.' };$lines.Add('log_file = "logs/mcla.log"') }
            'fullscreen' { if ($value -cne 'true') { throw 'Fullscreen template changed.' };$lines.Add('fullscreen = true') }
            default { $lines.Add($line) }
        }
    }
    foreach ($required in @('mcla_diagnostics_root','log_file','fullscreen')) { if (-not $keys.Contains($required)) { throw "Portable config missing required key: $required" } }
    return ($lines -join "`n") + "`nresolution_scale = $Scale`nvsync = true`n"
}

# Fail before creating evidence/build/publication state for invalid admission.
Assert-PortableBuildOptions $PSBoundParameters
$sdkInstall = $null
if ($RexglueInstallPrefix) { $sdkInstall = Resolve-PortableSdkInstall $RexglueInstallPrefix }
if ($EnableF10Wgc) {
    if ($SkipCleanBuild) { throw 'WGC publication requires a clean build; SkipCleanBuild is not supported.' }
    if (-not $CppWinRtIncludeDir -or $CppWinRtIncludeDir -notmatch '^(?:[A-Za-z]:[\\/]|\\\\[^\\]+\\[^\\]+\\)' -or
        -not (Test-Path -LiteralPath (Join-Path $CppWinRtIncludeDir 'winrt/Windows.Graphics.Capture.h') -PathType Leaf)) {
        throw 'EnableF10Wgc requires the absolute installed Windows SDK cppwinrt include directory.'
    }
    $CppWinRtIncludeDir = (Resolve-Path -LiteralPath $CppWinRtIncludeDir).Path
} elseif ($CppWinRtIncludeDir) { throw 'CppWinRtIncludeDir requires EnableF10Wgc.' }
if ($SaveArchive -and $SaveSource) { throw 'SaveArchive and SaveSource are mutually exclusive.' }
if ($SaveArchive) {
    if ($SaveArchive -notmatch '^(?:[A-Za-z]:[\\/]|\\\\[^\\]+\\[^\\]+\\)') { throw 'Explicit save archive must be an absolute content-addressed archive path.' }
    $null = Get-PortableProfileArchive $repo $SaveArchive
}
if (-not $OutputRoot) { $OutputRoot = Join-Path $repo 'private\bundles\M7-016' }
$output = [IO.Path]::GetFullPath($OutputRoot)
$privateRoot = [IO.Path]::GetFullPath((Join-Path $repo 'private'))
if (-not $output.StartsWith($privateRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Portable campaign bundles must remain below ignored private/ output.'
}
# Keep publication builds separate from legacy hardlinked developer outputs.
$build = Join-Path $repo 'out\build\win-amd64-portable-release'
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

function Assert-PortableCaptureBuildCache([string]$BuildRoot, [bool]$Wgc) {
    $cache = [IO.File]::ReadAllText((Join-Path $BuildRoot 'CMakeCache.txt'))
    $feature = [regex]::Matches($cache, '(?m)^MCLA_EXPERIMENTAL_F10_WGC:BOOL=(ON|OFF)\r?$')
    $expected = if ($Wgc) { 'ON' } else { 'OFF' }
    if ($feature.Count -ne 1 -or $feature[0].Groups[1].Value -cne $expected -or
        $cache -cnotmatch '(?m)^CMAKE_BUILD_TYPE:STRING=Release\r?$') {
        throw 'Portable capture feature/build cache mismatch; configure and clean-build the requested Release variant.'
    }
}

function Resolve-LatestSave {
    $candidates = @()
    $archiveRoot = Assert-PortablePath $repo (Join-Path $repo 'private\save-archive\M6-014')
    $pending = [Collections.Generic.Queue[string]]::new(); $pending.Enqueue($archiveRoot)
    $pointers = [Collections.Generic.List[object]]::new()
    while ($pending.Count) {
        foreach ($item in @(Get-ChildItem -LiteralPath $pending.Dequeue() -Force -ErrorAction Stop)) {
            $null = Assert-PortablePath $archiveRoot $item.FullName
            if ($item.PSIsContainer) { $pending.Enqueue($item.FullName) }
            elseif ($item.Name -ceq 'latest.json') { $pointers.Add($item) }
        }
    }
    foreach ($pointer in $pointers) {
        try {
            $value = Get-Content -LiteralPath $pointer.FullName -Raw | ConvertFrom-Json
            if ($value.schema -cne 'mcla-soak-save-snapshot-v1' -or $value.complete_profile_tree -ne $true -or
                [string]$value.snapshot_directory -cnotmatch '^\d{8}-\d{6}Z-[0-9A-F]{16}-[0-9A-F]{16}$') { throw 'Malformed legacy save pointer.' }
            $captured = [DateTimeOffset]::Parse([string]$value.captured_utc, [Globalization.CultureInfo]::InvariantCulture)
            $snapshot = [IO.Path]::GetFullPath((Join-Path $pointer.DirectoryName ([string]$value.snapshot_directory)))
            $null = Assert-PortablePath $pointer.DirectoryName $snapshot
            $save = Join-Path $snapshot $saveRelative.Replace('/','\')
            $header = Join-Path $snapshot $headerRelative.Replace('/','\')
            if (-not (Test-Path -LiteralPath $save -PathType Leaf) -or -not (Test-Path -LiteralPath $header -PathType Leaf)) { throw 'Legacy save/header missing.' }
            if ((Get-Sha256 $save) -cne [string]$value.save_sha256 -or (Get-Sha256 $header) -cne [string]$value.header_sha256) { throw 'Legacy save/header hash drifted.' }
            $candidates += [pscustomobject]@{ Captured=$captured; Pointer=$pointer.FullName; Snapshot=$snapshot; Value=$value }
        } catch { throw "Legacy snapshot needs recovery review; refusing to fall back to older progress: '$($pointer.FullName)'. $($_.Exception.Message)" }
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
        if ($relative -ceq 'bundle.lock') { continue }
        $target = Join-Path $DestinationRoot $relative
        if ($item.PSIsContainer) { [IO.Directory]::CreateDirectory($target) | Out-Null }
        else {
            [IO.Directory]::CreateDirectory((Split-Path -Parent $target)) | Out-Null
            if ($relative -cmatch '^(user|logs|diagnostics|results|metadata|update|cache)\\') {
                Copy-Item -LiteralPath $item.FullName -Destination $target
            } else {
                try { New-Item -ItemType HardLink -Path $target -Target $item.FullName -ErrorAction Stop | Out-Null }
                catch { Copy-Item -LiteralPath $item.FullName -Destination $target }
            }
        }
    }
}

function Remove-ObsoleteBundles([string]$BundleParent, [string]$CurrentBundle) {
    if ($BundleParent.TrimEnd('\') -cne $publication.Root) { throw 'Retention parent does not match the held publication lock.' }
    Assert-PortableSavePlanUnchanged $repo $selected
    Remove-PortableArchivedBundles $publication $CurrentBundle $selected.Receipts
}

$publication = $null
try {
$publication = Enter-PortablePublication $repo $output
$legacy = Resolve-LatestSave
$planArguments = @{RepoRoot=$repo;State=$publication;LegacySnapshot=$legacy.Snapshot;SaveSource=$SaveSource}
if ($SaveArchive) { $planArguments.SaveArchive = $SaveArchive }
$selected = New-PortableSavePlan @planArguments
if ($PruneOnly) {
    if (-not (Test-Path -LiteralPath $output -PathType Container)) { throw 'Portable bundle output root is missing.' }
    $pointer = Join-Path $output 'current.txt'
    if (-not (Test-Path -LiteralPath $pointer -PathType Leaf)) { throw 'Portable current.txt is missing.' }
    $currentId = [IO.File]::ReadAllText($pointer).Trim()
    if ($currentId -cnotmatch '^mcla-\d+\.\d+\.\d+\.\d+-win64-(?:[0-9A-F]{16}-){2}[0-9A-F]{16}$') { throw 'Portable current.txt identity is invalid.' }
    $currentBundle = [IO.Path]::GetFullPath((Join-Path $output $currentId))
    if ((Split-Path -Parent $currentBundle).TrimEnd('\') -cne $output.TrimEnd('\') -or
        -not (Test-Path -LiteralPath $currentBundle -PathType Container)) { throw 'Portable current bundle is missing or escaped its parent.' }
    # Pruning must not discard a branch that should seed the next child.
    if ((Get-PortableProfile (Join-Path $currentBundle 'user')).Sha256 -cne $selected.Profile.Sha256) { throw 'Current child does not carry the selected profile; publish its successor before pruning.' }
    Assert-PortablePublicationUnchanged $publication $selected.Receipts
    $removedBundles = Remove-ObsoleteBundles $output $currentBundle
    Write-Host "M7-016 RETENTION: removed $removedBundles obsolete bundle(s); current plus current.txt retained." -ForegroundColor DarkCyan
    [pscustomobject]@{ Passed=$true; CurrentBundle=$currentBundle; RemovedBundles=$removedBundles }
    return
}

# Hash segments have fixed widths, so actual destination lengths can be checked
# before expensive builds. A later check binds the exact generated roots too.
$plannedBundleId = 'mcla-{0}-win64-{1}-{1}-{1}' -f $version,('0'*16)
$plannedFinal = Join-Path $output $plannedBundleId
$plannedPartial = Join-Path $output ('.'+$plannedBundleId+'.00000000.partial')
$plannedPathBudget = Assert-PortableProfilePathBudget $plannedPartial $plannedFinal $selected.Profile.Entries

Write-Host 'M7-016 [1/6]: validating the exact private game and selected complete progressed save...' -ForegroundColor Cyan
$gameIdentity = & (Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath $game -ManifestPath $gameManifestPath -VerifyHashes
$selectedSave = Join-Path $selected.Snapshot $saveRelative.Replace('/','\')
$selectedHeader = Join-Path $selected.Snapshot $headerRelative.Replace('/','\')
$selectedSaveHash = Get-Sha256 $selectedSave
$selectedHeaderHash = Get-Sha256 $selectedHeader

$toolchain = & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1') -ExportPath
$cppRuntime = Resolve-PortableCppRuntimeSource -VisualStudioRoot $toolchain.VisualStudioRoot
if (-not $SkipCleanBuild) {
    Write-Host 'M7-016 [2/6]: clean-building the toolchain-free Release runtime and native launcher...' -ForegroundColor Cyan
    $captureArguments = @('-DMCLA_EXPERIMENTAL_F10_WGC=OFF','-DMCLA_BUILD_IMAGE_CAPTURE_TESTS=OFF')
    if ($EnableF10Wgc) { $captureArguments = @('-DMCLA_EXPERIMENTAL_F10_WGC=ON','-DMCLA_BUILD_IMAGE_CAPTURE_TESTS=OFF', ('-DMCLA_CPPWINRT_INCLUDE_DIR=' + $CppWinRtIncludeDir)) }
    if ($sdkInstall) { $captureArguments += @(('-DCMAKE_PREFIX_PATH:PATH=' + $sdkInstall.Prefix), ('-Drexglue_DIR:PATH=' + $sdkInstall.PackageDirectory), '-DREXSDK_DIR:PATH=') }
    & cmake --preset win-amd64-release -B $build @captureArguments *>&1 | Tee-Object -FilePath $buildLog | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Release configure failed. Private evidence: '$evidence'." }
    if ($sdkInstall) { Assert-PortableSdkCache ([IO.File]::ReadAllText((Join-Path $build 'CMakeCache.txt'))) $sdkInstall.Prefix }
    & cmake --build $build --target mcla --clean-first --parallel 8 *>&1 | Tee-Object -FilePath $buildLog -Append | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Release build failed. Private evidence: '$evidence'." }
} else {
    [IO.File]::WriteAllText($buildLog, "clean build deliberately skipped`r`n", $utf8)
}
Assert-PortableCaptureBuildCache $build ([bool]$EnableF10Wgc)
if ($sdkInstall) {
    Assert-PortableSdkCache ([IO.File]::ReadAllText((Join-Path $build 'CMakeCache.txt'))) $sdkInstall.Prefix
    foreach ($pin in $sdkInstall.Pins) {
        if ((Get-PortableSha256 (Join-Path $sdkInstall.Prefix $pin.RelativePath)) -cne $pin.Sha256) { throw 'Explicit SDK inputs changed during the build.' }
    }
    foreach ($name in @('rexruntime.dll','rexgpu-xenos.dll')) {
        if ((Get-PortableSha256 (Join-Path $build $name)) -cne (Get-PortableSha256 (Join-Path $sdkInstall.Prefix ('bin/' + $name)))) { throw "Built runtime did not come from selected SDK: $name" }
    }
}

$launcherSource = Join-Path $build 'Launch-MCLA.exe'
$mclaSource = Join-Path $build 'mcla.exe'
$imageCapture = $null
if ($EnableF10Wgc) {
    $helperSource = Join-Path $build 'mcla_diagnostic_image_helper.exe'
    $helperItem = Get-Item -LiteralPath $helperSource
    if ($helperItem.Length -le 0 -or $helperItem.Length -gt 16MB) { throw 'Image helper size is outside the publication bound.' }
    $helperHash = Get-Sha256 $helperSource
    $pinText = [IO.File]::ReadAllText((Join-Path $build 'mcla_image_helper_pin.h'))
    $pins = [regex]::Matches($pinText, '(?m)^#define MCLA_IMAGE_HELPER_SHA256 "([0-9A-F]{64})"\r?$')
    if ($pins.Count -ne 1 -or $pins[0].Groups[1].Value -cne $helperHash) { throw 'Built helper does not match the generated host pin.' }
    if ((Get-Item -LiteralPath $mclaSource).Length -gt 256MB) { throw 'Host exceeds the bounded capture-pin scan.' }
    $hostBytes = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($mclaSource))
    if ([regex]::Matches($hostBytes, [regex]::Escape($helperHash)).Count -ne 1) { throw 'Built host does not contain exactly one matching image-helper pin.' }
    $imageCapture = [ordered]@{ mode='window-wgc-v1'; helper_path='bin/mcla_diagnostic_image_helper.exe'; helper_sha256=$helperHash; helper_bytes=[long]$helperItem.Length; host_sha256=(Get-Sha256 $mclaSource) }
}
foreach ($required in @($launcherSource,$mclaSource,(Join-Path $build 'mcla_crash_handler.exe'),(Join-Path $build 'rexruntime.dll'),(Join-Path $build 'TracyClient.dll'),(Join-Path $build 'rexgpu-xenos.dll'))) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required Release artifact is missing: '$required'." }
}
$mclaHash = Get-Sha256 $mclaSource
$launcherHash = Get-Sha256 $launcherSource
$bundleId = 'mcla-{0}-win64-{1}-{2}-{3}' -f $version,$mclaHash.Substring(0,16),$launcherHash.Substring(0,16),$selectedSaveHash.Substring(0,16)
$final = Join-Path $output $bundleId
$partial = Join-Path $output ('.' + $bundleId + '.' + [guid]::NewGuid().ToString('N').Substring(0,8) + '.partial')
$actualPathBudget = Assert-PortableProfilePathBudget $partial $final $selected.Profile.Entries
if (Test-Path -LiteralPath $final) { throw "The exact bundle already exists and will not be overwritten: '$final'." }
[IO.Directory]::CreateDirectory($output) | Out-Null
if (Test-Path -LiteralPath $partial) { throw "Unexpected bundle staging collision: '$partial'." }

Write-Host 'M7-016 [3/6]: staging one self-contained Syncthing folder with the selected save...' -ForegroundColor Cyan
$copyModes = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
try {
    foreach ($directory in @('bin','game','user','cache','logs','diagnostics','results','update','metadata')) {
        [IO.Directory]::CreateDirectory((Join-Path $partial $directory)) | Out-Null
    }
    # Build outputs may be overwritten in place by a later linker/staging copy.
    # Never hardlink them into an already published immutable bundle.
    $null = $copyModes.Add((Copy-OrLink $launcherSource (Join-Path $partial 'Launch-MCLA.exe') -Materialize))
    foreach ($name in @('mcla.exe','mcla_crash_handler.exe','rexruntime.dll','TracyClient.dll','rexgpu-xenos.dll')) {
        $null = $copyModes.Add((Copy-OrLink (Join-Path $build $name) (Join-Path $partial ('bin\' + $name)) -Materialize))
    }
    if ($EnableF10Wgc) { $null = $copyModes.Add((Copy-OrLink $helperSource (Join-Path $partial 'bin/mcla_diagnostic_image_helper.exe') -Materialize)) }
    foreach ($runtimeFile in $cppRuntime.Files) {
        $null = $copyModes.Add((Copy-OrLink (Join-Path $cppRuntime.SourceDirectory $runtimeFile.Name) (Join-Path $partial $runtimeFile.RelativePath.Replace('/','\')) -Materialize))
    }
    # Bind staged bytes again: a source update during copy must not publish a
    # mixed runtime. The publisher never changes loader overrides or a prefix.
    Assert-PortableCppRuntimeBundle -Root $partial -Policy $cppRuntime.Policy
    $portableConfig = New-PortableConfig ([IO.File]::ReadAllText((Join-Path $repo 'config\mcla.toml.example'))) $ResolutionScale
    [IO.File]::WriteAllText((Join-Path $partial 'bin\mcla.toml'), $portableConfig, $utf8)
    Copy-Item -LiteralPath $gameManifestPath -Destination (Join-Path $partial 'game-manifest.json')

    $gameManifest = Get-Content -LiteralPath $gameManifestPath -Raw | ConvertFrom-Json
    foreach ($entry in @($gameManifest.Files)) {
        $source = Join-Path $game ([string]$entry.Path).Replace('/','\')
        $destination = Join-Path (Join-Path $partial 'game') ([string]$entry.Path).Replace('/','\')
        $null = $copyModes.Add((Copy-OrLink $source $destination -Materialize:$MaterializeGameFiles))
    }
    Assert-PortableSavePlanUnchanged $repo $selected
    $sourceProfile = Get-PortableProfileArchive $repo $selected.Snapshot
    foreach ($entry in $sourceProfile.Entries) {
        $destination = Join-Path (Join-Path $partial 'user') $entry.path.Replace('/','\')
        if ($entry.kind -eq 'directory') { [IO.Directory]::CreateDirectory($destination) | Out-Null }
        else { Copy-Item -LiteralPath (Join-Path $selected.Snapshot $entry.path.Replace('/','\')) -Destination $destination }
    }
    if ((Get-PortableProfile (Join-Path $partial 'user')).Sha256 -cne $selected.Profile.Sha256) { throw 'Staged full profile differs from the chosen branch.' }
    Assert-PortableSavePlanUnchanged $repo $selected

    [IO.File]::WriteAllText((Join-Path $partial 'bundle-id.txt'), $bundleId + "`n", $utf8)
    $sourceSnapshotRelative = $selected.Snapshot.Substring($repo.TrimEnd('\').Length + 1).Replace('\','/')
    $immutableCount = 4 + 6 + $cppRuntime.Files.Count + [int]$gameIdentity.FileCount
    if ($EnableF10Wgc) { $immutableCount++ }
    $manifest = [ordered]@{
        schema = 'mcla-portable-campaign-bundle-v2'
        task = 'M7-016'
        bundle_id = $bundleId
        project_version = $version
        sdk_version = '0.20.0.0'
        sdk_commit = '5dd58f894bf34b9ea85e59d3903c7f0e8044d853'
        created_utc = [DateTime]::UtcNow.ToString('O')
        platform = 'windows-proton'
        architecture = 'x86_64'
        launcher = 'Launch-MCLA.exe'
        cpp_runtime = [ordered]@{ policy=$cppRuntime.Policy.policy; version=$cppRuntime.Policy.version }
        window_mode = [ordered]@{ startup_fullscreen=$true; alt_enter_toggle=$true; left_double_click_toggle=$true }
        immutable_file_count = $immutableCount
        game = [ordered]@{ manifest_sha256=(Get-Sha256 (Join-Path $partial 'game-manifest.json')); file_count=[int]$gameIdentity.FileCount; payload_bytes=[long]$gameIdentity.PayloadBytes }
        selected_save = [ordered]@{ source_snapshot=$sourceSnapshotRelative; save_relative=$saveRelative; header_relative=$headerRelative; save_sha256=$selectedSaveHash; header_sha256=$selectedHeaderHash; profile_sha256=$selected.Profile.Sha256; ancestor_profile_sha256=@($selected.Ancestors) }
        writable_roots = @('user','cache','logs','diagnostics','results','update','metadata')
        retention = [ordered]@{ completed_sessions=32; save_snapshots_per_session=32 }
        private_content = [ordered]@{ game_and_save_private=$true; source_iso_included=$false }
    }
    if ($EnableF10Wgc) { $manifest.image_capture = $imageCapture }
    [IO.File]::WriteAllText((Join-Path $partial 'bundle-manifest.json'), (($manifest | ConvertTo-Json -Depth 8) + "`n"), $utf8)

    $immutable = @('Launch-MCLA.exe','bundle-id.txt','bundle-manifest.json','game-manifest.json')
    $immutable += @('mcla.exe','mcla_crash_handler.exe','mcla.toml','rexgpu-xenos.dll','rexruntime.dll','TracyClient.dll' | ForEach-Object { 'bin/' + $_ })
    if ($EnableF10Wgc) { $immutable += 'bin/mcla_diagnostic_image_helper.exe' }
    $immutable += @($cppRuntime.Files | ForEach-Object { $_.RelativePath })
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
    Add-PortableWriterLock $publication (Join-Path $final 'bundle.lock')

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
            $probe = Start-Process -FilePath (Join-Path $relocation 'Launch-MCLA.exe') -ArgumentList '--diagnostics-probe' -WorkingDirectory $relocation -WindowStyle Hidden -Wait -PassThru
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
    Assert-PortableSavePlanUnchanged $repo $selected
    Assert-PortablePublicationUnchanged $publication $selected.Receipts
    $result = [ordered]@{
        schema = 'mcla-portable-campaign-bundle-result-v1'
        task = 'M7-016'
        decision = if ($SkipRelocationProof) { 'portable-bundle-built-relocation-pending' } else { 'portable-windows-bundle-and-relocation-pass' }
        run_id = $runId
        bundle_id = $bundleId
        bundle_path = $final.Substring($repo.TrimEnd('\').Length + 1).Replace('\','/')
        project_version = $version
        sdk_version = '0.20.0.0'
        sdk_commit = '5dd58f894bf34b9ea85e59d3903c7f0e8044d853'
        bundle_manifest_sha256 = Get-Sha256 (Join-Path $final 'bundle-manifest.json')
        bundle_lock_sha256 = Get-Sha256 (Join-Path $final 'bundle-files.sha256')
        immutable_file_count = $staged.ImmutableFiles
        game_file_count = $staged.GameFiles
        game_payload_bytes = $staged.GameBytes
        selected_save_sha256 = $selectedSaveHash
        selected_header_sha256 = $selectedHeaderHash
        selected_snapshot = $sourceSnapshotRelative
        selected_profile_sha256 = $selected.Profile.Sha256
        selected_source = $selected.Source
        profile_path_budget = $actualPathBudget
        returned_state_archives = @($selected.Receipts)
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
    if ($sdkInstall) { $result.explicit_sdk_install = $sdkInstall }
    $resultPath = Join-Path $evidence 'result.json'
    [IO.File]::WriteAllText($resultPath, (($result | ConvertTo-Json -Depth 8) + "`n"), $utf8)
    # Only advertise the successor after relocation and return preservation.
    Assert-PortableSavePlanUnchanged $repo $selected
    Publish-PortablePointer $publication $bundleId
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
} finally { Exit-PortablePublication $publication }
