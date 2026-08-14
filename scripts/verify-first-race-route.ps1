[CmdletBinding()]
param(
    [string]$ConfigPath,
    [string]$SeedRoot,
    [string]$ScreenshotRoot,
    [switch]$FixtureMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$utf8 = [Text.UTF8Encoding]::new($false)
if (-not $ConfigPath) { $ConfigPath = Join-Path $repo 'config\first-race-route.json' }
if (-not $SeedRoot) { $SeedRoot = Join-Path $repo 'private\baseline\M4-011\post-oobe-profile' }
if (-not $ScreenshotRoot) { $ScreenshotRoot = Join-Path $repo 'private\tools\xenia-canary\artifacts\screenshots\545407F8' }

function Resolve-Safe([string]$Path, [string]$Description, [bool]$Directory) {
    $full = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repo $Path }))
    $prefix = $repo.TrimEnd('\') + '\'
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes the repository." }
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

function Assert-Properties([object]$Object, [string[]]$Names, [string]$Description) {
    if ($null -eq $Object -or ($Object.PSObject.Properties.Name -join ',') -cne ($Names -join ',')) {
        throw "$Description properties are not exact or ordered."
    }
}

function Assert-JsonString([object]$Value, [string]$Description) { if ($Value -isnot [string]) { throw "$Description must be a JSON string." } }
function Assert-JsonInt([object]$Value, [string]$Description) { if ($Value -isnot [int] -and $Value -isnot [long]) { throw "$Description must be a JSON integer." } }
function Assert-JsonBool([object]$Value, [string]$Description) { if ($Value -isnot [bool]) { throw "$Description must be a JSON boolean." } }

function Assert-TreeWithoutReparse([string]$Root) {
    $pending = [Collections.Generic.Stack[string]]::new()
    $pending.Push($Root)
    while ($pending.Count) {
        foreach ($item in @(Get-ChildItem -LiteralPath $pending.Pop() -Force)) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Seed tree contains a reparse point.' }
            if ($item.PSIsContainer) { $pending.Push($item.FullName) }
        }
    }
}

function Get-Tree([string]$Root) {
    Assert-TreeWithoutReparse $Root
    $items = @(Get-ChildItem -LiteralPath $Root -Recurse -Force)
    $files = @($items | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)
    $entries = @()
    foreach ($directory in @($items | Where-Object PSIsContainer | Sort-Object FullName)) {
        $entries += [ordered]@{ kind = 'directory'; path = $directory.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/') }
    }
    foreach ($file in $files) {
        $entries += [ordered]@{ kind = 'file'; path = $file.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/'); length = $file.Length; sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
    }
    $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
    $bytes = 0L
    foreach ($file in $files) { $bytes += $file.Length }
    [pscustomobject]@{ Hash = $hash; FileCount = $files.Count; DirectoryCount = @($items | Where-Object PSIsContainer).Count; Bytes = $bytes }
}

function Assert-Png([string]$Path, [long]$Bytes, [string]$Sha256, [string]$Description) {
    $path = Resolve-Safe $Path $Description $false
    if ((Get-Item -LiteralPath $path).Length -ne $Bytes -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $Sha256) {
        throw "$Description identity mismatch."
    }
    Add-Type -AssemblyName System.Drawing
    $image = [Drawing.Image]::FromFile($path)
    try {
        if ($image.Width -ne 1280 -or $image.Height -ne 720 -or $image.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Png.Guid) {
            throw "$Description is not the canonical 1280x720 PNG."
        }
    } finally { $image.Dispose() }
}

$config = Resolve-Safe $ConfigPath 'First-race route config' $false
$raw = [IO.File]::ReadAllText($config)
if ($raw -match '(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])') { throw 'Route config contains a private or absolute path.' }
$route = $raw | ConvertFrom-Json

Assert-Properties $route @('schema', 'task', 'route_id', 'status', 'supported_image', 'runtime', 'seed', 'xenia_baseline', 'start_state', 'controller', 'event', 'steps', 'timeouts_seconds', 'acceptance', 'exclusions') 'Route'
Assert-Properties $route.supported_image @('edition', 'title_id', 'media_id', 'entry', 'iso_sha256', 'default_xex_bytes', 'default_xex_sha256') 'Supported image'
Assert-Properties $route.runtime @('sdk_version', 'backend', 'draw_resolution_scale', 'target_fps', 'patches_enabled', 'title_updates_enabled', 'language', 'country') 'Runtime'
Assert-Properties $route.seed @('classification', 'tree_sha256', 'file_count', 'directory_count', 'bytes', 'save_sha256', 'header_sha256') 'Seed'
Assert-Properties $route.xenia_baseline @('free_roam_sha256', 'race_sha256', 'race_pause_sha256', 'width', 'height') 'Xenia baseline'
Assert-Properties $route.start_state @('vehicle', 'vehicle_color', 'location', 'time_of_day', 'weather', 'camera', 'hud') 'Start state'
Assert-Properties $route.controller @('backend', 'physical_controller_count', 'guest_slot', 'layout', 'gps', 'pause', 'headlights', 'steering', 'accelerate', 'brake_reverse') 'Controller'
Assert-Properties $route.event @('name', 'opponent', 'opponent_vehicle', 'field_size', 'selection', 'native_identity_proven') 'Event'
Assert-Properties $route.timeouts_seconds @('title', 'saved_free_roam_after_start', 'event_selection', 'opponent_navigation', 'race_start_after_challenge', 'race_completion', 'results_transition', 'return_to_free_roam', 'external_close') 'Timeouts'
Assert-Properties $route.acceptance @('required_finish_position', 'required_field_size', 'results_required', 'return_to_free_roam_required', 'controlled_external_close_required', 'save_persistence_claimed', 'whole_frame_parity_claimed', 'event_identity_must_be_calibrated_before_runtime_acceptance') 'Acceptance'

Assert-JsonInt $route.schema 'schema'
foreach ($entry in @(
        @($route.task, 'task'), @($route.route_id, 'route_id'), @($route.status, 'status'),
        @($route.supported_image.edition, 'supported_image.edition'), @($route.supported_image.title_id, 'supported_image.title_id'), @($route.supported_image.media_id, 'supported_image.media_id'), @($route.supported_image.entry, 'supported_image.entry'), @($route.supported_image.iso_sha256, 'supported_image.iso_sha256'), @($route.supported_image.default_xex_sha256, 'supported_image.default_xex_sha256'),
        @($route.runtime.sdk_version, 'runtime.sdk_version'), @($route.runtime.backend, 'runtime.backend'), @($route.runtime.language, 'runtime.language'), @($route.runtime.country, 'runtime.country'),
        @($route.seed.classification, 'seed.classification'), @($route.seed.tree_sha256, 'seed.tree_sha256'), @($route.seed.save_sha256, 'seed.save_sha256'), @($route.seed.header_sha256, 'seed.header_sha256'),
        @($route.xenia_baseline.free_roam_sha256, 'xenia_baseline.free_roam_sha256'), @($route.xenia_baseline.race_sha256, 'xenia_baseline.race_sha256'), @($route.xenia_baseline.race_pause_sha256, 'xenia_baseline.race_pause_sha256'),
        @($route.start_state.vehicle, 'start_state.vehicle'), @($route.start_state.vehicle_color, 'start_state.vehicle_color'), @($route.start_state.location, 'start_state.location'), @($route.start_state.time_of_day, 'start_state.time_of_day'), @($route.start_state.weather, 'start_state.weather'), @($route.start_state.camera, 'start_state.camera'), @($route.start_state.hud, 'start_state.hud'),
        @($route.controller.backend, 'controller.backend'), @($route.controller.layout, 'controller.layout'), @($route.controller.gps, 'controller.gps'), @($route.controller.pause, 'controller.pause'), @($route.controller.headlights, 'controller.headlights'), @($route.controller.steering, 'controller.steering'), @($route.controller.accelerate, 'controller.accelerate'), @($route.controller.brake_reverse, 'controller.brake_reverse'),
        @($route.event.name, 'event.name'), @($route.event.opponent, 'event.opponent'), @($route.event.opponent_vehicle, 'event.opponent_vehicle'), @($route.event.selection, 'event.selection')
    )) { Assert-JsonString $entry[0] $entry[1] }
foreach ($entry in @(
        @($route.supported_image.default_xex_bytes, 'supported_image.default_xex_bytes'), @($route.runtime.draw_resolution_scale, 'runtime.draw_resolution_scale'), @($route.runtime.target_fps, 'runtime.target_fps'),
        @($route.seed.file_count, 'seed.file_count'), @($route.seed.directory_count, 'seed.directory_count'), @($route.seed.bytes, 'seed.bytes'), @($route.xenia_baseline.width, 'xenia_baseline.width'), @($route.xenia_baseline.height, 'xenia_baseline.height'),
        @($route.controller.physical_controller_count, 'controller.physical_controller_count'), @($route.controller.guest_slot, 'controller.guest_slot'), @($route.event.field_size, 'event.field_size'),
        @($route.timeouts_seconds.title, 'timeouts_seconds.title'), @($route.timeouts_seconds.saved_free_roam_after_start, 'timeouts_seconds.saved_free_roam_after_start'), @($route.timeouts_seconds.event_selection, 'timeouts_seconds.event_selection'), @($route.timeouts_seconds.opponent_navigation, 'timeouts_seconds.opponent_navigation'), @($route.timeouts_seconds.race_start_after_challenge, 'timeouts_seconds.race_start_after_challenge'), @($route.timeouts_seconds.race_completion, 'timeouts_seconds.race_completion'), @($route.timeouts_seconds.results_transition, 'timeouts_seconds.results_transition'), @($route.timeouts_seconds.return_to_free_roam, 'timeouts_seconds.return_to_free_roam'), @($route.timeouts_seconds.external_close, 'timeouts_seconds.external_close'),
        @($route.acceptance.required_finish_position, 'acceptance.required_finish_position'), @($route.acceptance.required_field_size, 'acceptance.required_field_size')
    )) { Assert-JsonInt $entry[0] $entry[1] }
foreach ($entry in @(
        @($route.runtime.patches_enabled, 'runtime.patches_enabled'), @($route.runtime.title_updates_enabled, 'runtime.title_updates_enabled'), @($route.event.native_identity_proven, 'event.native_identity_proven'),
        @($route.acceptance.results_required, 'acceptance.results_required'), @($route.acceptance.return_to_free_roam_required, 'acceptance.return_to_free_roam_required'), @($route.acceptance.controlled_external_close_required, 'acceptance.controlled_external_close_required'), @($route.acceptance.save_persistence_claimed, 'acceptance.save_persistence_claimed'), @($route.acceptance.whole_frame_parity_claimed, 'acceptance.whole_frame_parity_claimed'), @($route.acceptance.event_identity_must_be_calibrated_before_runtime_acceptance, 'acceptance.event_identity_must_be_calibrated_before_runtime_acceptance')
    )) { Assert-JsonBool $entry[0] $entry[1] }
if ($route.steps -isnot [array] -or $route.exclusions -isnot [array]) { throw 'steps and exclusions must be JSON arrays.' }

if ($route.schema -ne 1 -or $route.task -cne 'M5-001' -or $route.route_id -cne 'pinned-save-sunset-strip-race-v1' -or $route.status -cne 'normative-route-requires-native-calibration') { throw 'Route identity mismatch.' }
if ($route.supported_image.edition -cne 'Complete Edition' -or $route.supported_image.title_id -cne '545407F8' -or $route.supported_image.media_id -cne '5940C9DB' -or $route.supported_image.entry -cne '821322B8' -or $route.supported_image.iso_sha256 -cne 'AFCAAE68593246B1F83328681B690E254A13C37DD2E8DC34AB0DEB6C0F471FDB' -or $route.supported_image.default_xex_bytes -ne 9252864 -or $route.supported_image.default_xex_sha256 -cne 'C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432') { throw 'Supported-image contract mismatch.' }
if ($route.runtime.sdk_version -cne '0.9.0.20' -or $route.runtime.backend -cne 'd3d12-host-rtv' -or $route.runtime.draw_resolution_scale -ne 1 -or $route.runtime.target_fps -ne 30 -or $route.runtime.patches_enabled -ne $false -or $route.runtime.title_updates_enabled -ne $false -or $route.runtime.language -cne 'en' -or $route.runtime.country -cne 'us') { throw 'Runtime contract mismatch.' }
if ($route.seed.classification -cne 'pinned-post-oobe-load-only' -or $route.seed.tree_sha256 -cne '5F3B045D690CFACE51F43AF504EF6006B7C5B0A913BF99C719F7EE179ABBC471' -or $route.seed.file_count -ne 2 -or $route.seed.directory_count -ne 6 -or $route.seed.bytes -ne 537756 -or $route.seed.save_sha256 -cne 'E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68' -or $route.seed.header_sha256 -cne '1DAEF55FA78BDD3F1AA75A36772B801C6C714B6D1A115A97904F3D1C957BC4C9') { throw 'Seed contract mismatch.' }
if ($route.xenia_baseline.free_roam_sha256 -cne 'A490553067A8F375191F618C34556B82C9FD244FF295519AB4383AAB40434F8B' -or $route.xenia_baseline.race_sha256 -cne 'C5EC83D8A7DFB1AFD6E64CECAD3CA785E339CD9A0FF7ABC1978D189CDE7E67CB' -or $route.xenia_baseline.race_pause_sha256 -cne '6C7BE43B4A6C5C49B7542DC848EF7EF49E0BC8102588A0BF3D9FD8A25C31ADA5' -or $route.xenia_baseline.width -ne 1280 -or $route.xenia_baseline.height -ne 720) { throw 'Xenia baseline contract mismatch.' }
if ($route.start_state.vehicle -cne '1998 Nissan 240SX' -or $route.start_state.vehicle_color -cne 'white' -or $route.start_state.location -cne 'Sunset Blvd free roam' -or $route.start_state.time_of_day -cne 'night' -or $route.start_state.weather -cne 'dry' -or $route.start_state.camera -cne 'default chase' -or $route.start_state.hud -cne 'enabled') { throw 'Start-state contract mismatch.' }
if ($route.controller.backend -cne 'sdl' -or $route.controller.physical_controller_count -ne 1 -or $route.controller.guest_slot -ne 0 -or $route.controller.layout -cne 'default-button-automatic' -or $route.controller.gps -cne 'BACK' -or $route.controller.pause -cne 'START' -or $route.controller.headlights -cne 'Y' -or $route.controller.steering -cne 'LEFT_STICK' -or $route.controller.accelerate -cne 'RIGHT_TRIGGER' -or $route.controller.brake_reverse -cne 'LEFT_TRIGGER') { throw 'Controller contract mismatch.' }
if ($route.event.name -cne 'Sunset Strip Race' -or $route.event.opponent -cne 'Trevor' -or $route.event.opponent_vehicle -cne '1975 Datsun 280Z' -or $route.event.field_size -ne 2 -or $route.event.selection -cne 'GPS-select-event-then-challenge-opponent-with-headlights' -or $route.event.native_identity_proven -ne $false) { throw 'Event contract mismatch.' }

$expectedSteps = @(
    @('boot-title', 'boot exact image to verified Complete Edition title'),
    @('load-free-roam', 'press START and reach pinned nighttime free roam'),
    @('open-gps', 'press BACK and open the GPS map'),
    @('select-event', 'select Sunset Strip Race and reject any different event'),
    @('navigate-opponent', 'follow GPS to Trevor cruising Sunset Blvd'),
    @('challenge', 'press Y to flash headlights at Trevor'),
    @('race-start', 'reach a two-car race start with position HUD 2/2'),
    @('race-finish', 'finish the race in position 1/2'),
    @('results', 'observe the completed-race results transition'),
    @('return', 'return to controllable free roam'),
    @('external-close', 'close the exact game window with WM_CLOSE')
)
if (@($route.steps).Count -ne $expectedSteps.Count) { throw 'Route step count mismatch.' }
for ($index = 0; $index -lt $expectedSteps.Count; $index++) {
    Assert-Properties $route.steps[$index] @('ordinal', 'id', 'action') "Route step $($index + 1)"
    Assert-JsonInt $route.steps[$index].ordinal "Route step $($index + 1) ordinal"
    Assert-JsonString $route.steps[$index].id "Route step $($index + 1) id"
    Assert-JsonString $route.steps[$index].action "Route step $($index + 1) action"
    if ($route.steps[$index].ordinal -ne $index + 1 -or $route.steps[$index].id -cne $expectedSteps[$index][0] -or $route.steps[$index].action -cne $expectedSteps[$index][1]) { throw "Route step $($index + 1) mismatch." }
}

$expectedTimeouts = @(60, 45, 120, 300, 60, 300, 30, 45, 10)
$actualTimeouts = @($route.timeouts_seconds.title, $route.timeouts_seconds.saved_free_roam_after_start, $route.timeouts_seconds.event_selection, $route.timeouts_seconds.opponent_navigation, $route.timeouts_seconds.race_start_after_challenge, $route.timeouts_seconds.race_completion, $route.timeouts_seconds.results_transition, $route.timeouts_seconds.return_to_free_roam, $route.timeouts_seconds.external_close)
for ($index = 0; $index -lt $expectedTimeouts.Count; $index++) { if ($actualTimeouts[$index] -ne $expectedTimeouts[$index]) { throw "Route timeout $index mismatch." } }
if ($route.acceptance.required_finish_position -ne 1 -or $route.acceptance.required_field_size -ne 2 -or $route.acceptance.results_required -ne $true -or $route.acceptance.return_to_free_roam_required -ne $true -or $route.acceptance.controlled_external_close_required -ne $true -or $route.acceptance.save_persistence_claimed -ne $false -or $route.acceptance.whole_frame_parity_claimed -ne $false -or $route.acceptance.event_identity_must_be_calibrated_before_runtime_acceptance -ne $true) { throw 'Acceptance contract mismatch.' }
$expectedExclusions = @('first-run-oobe', 'intro-bink-playback', 'alternate-races', 'save-persistence-after-race', 'multiple-controllers', 'title-driven-force-feedback', 'whole-frame-xenia-parity', 'non-d3d12-backends', 'non-stock-timing')
foreach ($exclusion in $route.exclusions) { Assert-JsonString $exclusion 'Route exclusion' }
if ((@($route.exclusions) -join ',') -cne ($expectedExclusions -join ',')) { throw 'Exclusion contract mismatch.' }

$manifestPath = Resolve-Safe (Join-Path $repo 'private\game-manifest.json') 'Game manifest' $false
$manifest = [IO.File]::ReadAllText($manifestPath) | ConvertFrom-Json
$xexRecord = @($manifest.Files | Where-Object { $_.Path -ceq 'default.xex' })
if ($manifest.SchemaVersion -ne 1 -or $manifest.SourceIsoSha256 -cne $route.supported_image.iso_sha256 -or $manifest.FileCount -ne 15 -or $manifest.PayloadBytes -ne 6569586392 -or $xexRecord.Count -ne 1 -or $xexRecord[0].Size -ne $route.supported_image.default_xex_bytes -or $xexRecord[0].Sha256 -cne $route.supported_image.default_xex_sha256) { throw 'Physical game manifest mismatch.' }
$xexPath = Resolve-Safe (Join-Path $repo 'private\game\default.xex') 'Default XEX' $false
if ((Get-Item -LiteralPath $xexPath).Length -ne $route.supported_image.default_xex_bytes -or (Get-FileHash -LiteralPath $xexPath -Algorithm SHA256).Hash -cne $route.supported_image.default_xex_sha256) { throw 'Physical default XEX mismatch.' }

$seedRoot = Resolve-Safe $SeedRoot 'Pinned seed root' $true
$tree = Get-Tree $seedRoot
if ($tree.Hash -cne $route.seed.tree_sha256 -or $tree.FileCount -ne $route.seed.file_count -or $tree.DirectoryCount -ne $route.seed.directory_count -or $tree.Bytes -ne $route.seed.bytes) { throw 'Physical seed tree mismatch.' }
$seedFiles = @(Get-ChildItem -LiteralPath $seedRoot -File -Recurse)
if (@($seedFiles | Where-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash -ceq $route.seed.save_sha256 }).Count -ne 1 -or @($seedFiles | Where-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash -ceq $route.seed.header_sha256 }).Count -ne 1) { throw 'Physical seed file identity mismatch.' }

$screenshotRoot = Resolve-Safe $ScreenshotRoot 'Xenia screenshot root' $true
Assert-Png (Join-Path $screenshotRoot '545407F8 - 2026-08-11T00-53-02.png') 1049452 $route.xenia_baseline.free_roam_sha256 'Xenia free-roam baseline'
Assert-Png (Join-Path $screenshotRoot '545407F8 - 2026-08-11T01-02-28.png') 1238428 $route.xenia_baseline.race_sha256 'Xenia race baseline'
Assert-Png (Join-Path $screenshotRoot '545407F8 - 2026-08-11T01-04-21.png') 1194956 $route.xenia_baseline.race_pause_sha256 'Xenia race-pause baseline'

if (-not $FixtureMode) {
    $evidence = [IO.File]::ReadAllText((Resolve-Safe (Join-Path $repo 'docs\evidence\M5-001-first-race-route.md') 'M5-001 evidence document' $false))
    foreach ($required in @($route.route_id, $route.seed.tree_sha256, $route.xenia_baseline.free_roam_sha256, $route.xenia_baseline.race_sha256, $route.xenia_baseline.race_pause_sha256, 'Sunset Strip Race', 'Trevor', '1975 Datsun 280Z', 'normative, not yet native-calibrated')) {
        if (-not $evidence.Contains($required)) { throw "M5-001 evidence document is missing '$required'." }
    }
    $testing = [IO.File]::ReadAllText((Resolve-Safe (Join-Path $repo 'docs\testing.md') 'Testing documentation' $false))
    foreach ($required in @('scripts/verify-first-race-route.ps1', 'pinned-save-sunset-strip-race-v1', 'finish in position `1/2`')) { if (-not $testing.Contains($required)) { throw "Testing documentation is missing '$required'." } }
}

[pscustomobject]@{
    Passed = $true
    Task = 'M5-001'
    RouteId = $route.route_id
    Steps = $route.steps.Count
    SeedTreeVerified = $true
    XeniaBaselinesVerified = 3
    NativeEventIdentityProven = $false
    Decision = 'canonical-first-race-route-defined-runtime-calibration-required'
}
