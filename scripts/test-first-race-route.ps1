[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$validator = Join-Path $PSScriptRoot 'verify-first-race-route.ps1'
$sourceConfig = Join-Path $repo 'config\first-race-route.json'
$fixtureRoot = Join-Path $repo ('private\evidence\M5-001-fixture-' + [guid]::NewGuid().ToString('N'))
$configRoot = Join-Path $fixtureRoot 'configs'
$seedRoot = Join-Path $fixtureRoot 'seed'
$screenshotRoot = Join-Path $fixtureRoot 'screenshots'
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.Directory]::CreateDirectory($configRoot) | Out-Null
[IO.Directory]::CreateDirectory($seedRoot) | Out-Null
[IO.Directory]::CreateDirectory($screenshotRoot) | Out-Null

function Copy-CanonicalFixtures {
    foreach ($item in @(Get-ChildItem -LiteralPath (Join-Path $repo 'private\baseline\M4-011\post-oobe-profile') -Force)) {
        Copy-Item -LiteralPath $item.FullName -Destination $seedRoot -Recurse -Force
    }
    foreach ($name in @('545407F8 - 2026-08-11T00-53-02.png', '545407F8 - 2026-08-11T01-02-28.png', '545407F8 - 2026-08-11T01-04-21.png')) {
        Copy-Item -LiteralPath (Join-Path $repo "private\tools\xenia-canary\artifacts\screenshots\545407F8\$name") -Destination (Join-Path $screenshotRoot $name) -Force
    }
}

function Write-Fixture([string]$Name, [scriptblock]$Mutation) {
    $record = [IO.File]::ReadAllText($sourceConfig) | ConvertFrom-Json
    & $Mutation $record
    $path = Join-Path $configRoot "$Name.json"
    [IO.File]::WriteAllText($path, (ConvertTo-Json $record -Depth 20), $utf8)
    $path
}

function Assert-Rejected([string]$Name, [scriptblock]$Mutation) {
    $path = Write-Fixture $Name $Mutation
    try {
        & $validator -ConfigPath $path -SeedRoot $seedRoot -ScreenshotRoot $screenshotRoot -FixtureMode | Out-Null
    } catch { return }
    throw "Negative first-race fixture '$Name' was accepted."
}

try {
    Copy-CanonicalFixtures
    $positive = & $validator -ConfigPath $sourceConfig -SeedRoot $seedRoot -ScreenshotRoot $screenshotRoot -FixtureMode
    if (-not $positive.Passed -or $positive.Steps -ne 11 -or $positive.XeniaBaselinesVerified -ne 3 -or $positive.NativeEventIdentityProven -ne $false) { throw 'Positive first-race fixture returned the wrong summary.' }

    $negativeCases = 0
    $cases = [ordered]@{
        'wrong-schema' = { param($r) $r.schema = 2 }
        'schema-string' = { param($r) $r.schema = '1' }
        'wrong-task' = { param($r) $r.task = 'M5-002' }
        'wrong-route' = { param($r) $r.route_id = 'some-race' }
        'premature-native-proof' = { param($r) $r.event.native_identity_proven = $true }
        'wrong-title' = { param($r) $r.supported_image.title_id = '00000000' }
        'wrong-media' = { param($r) $r.supported_image.media_id = '00000000' }
        'wrong-xex-hash' = { param($r) $r.supported_image.default_xex_sha256 = '0' * 64 }
        'wrong-sdk' = { param($r) $r.runtime.sdk_version = '0.9.0.17' }
        'wrong-backend' = { param($r) $r.runtime.backend = 'd3d12-rov' }
        'wrong-scale' = { param($r) $r.runtime.draw_resolution_scale = 2 }
        'wrong-fps' = { param($r) $r.runtime.target_fps = 60 }
        'fps-string' = { param($r) $r.runtime.target_fps = '30' }
        'patches-enabled' = { param($r) $r.runtime.patches_enabled = $true }
        'patches-string' = { param($r) $r.runtime.patches_enabled = 'false' }
        'wrong-seed-tree' = { param($r) $r.seed.tree_sha256 = '1' * 64 }
        'wrong-save-hash' = { param($r) $r.seed.save_sha256 = '2' * 64 }
        'wrong-baseline-hash' = { param($r) $r.xenia_baseline.race_sha256 = '3' * 64 }
        'wrong-vehicle' = { param($r) $r.start_state.vehicle = 'Volkswagen Golf GTI' }
        'wrong-time' = { param($r) $r.start_state.time_of_day = 'day' }
        'wrong-weather' = { param($r) $r.start_state.weather = 'wet' }
        'two-controllers' = { param($r) $r.controller.physical_controller_count = 2 }
        'wrong-slot' = { param($r) $r.controller.guest_slot = 1 }
        'wrong-layout' = { param($r) $r.controller.layout = 'custom' }
        'wrong-headlights' = { param($r) $r.controller.headlights = 'X' }
        'wrong-event' = { param($r) $r.event.name = 'First Impressions' }
        'wrong-opponent' = { param($r) $r.event.opponent = 'Henry' }
        'wrong-opponent-car' = { param($r) $r.event.opponent_vehicle = '1969 Camaro' }
        'wrong-field' = { param($r) $r.event.field_size = 3 }
        'missing-step' = { param($r) $r.steps = @($r.steps | Select-Object -First 10) }
        'reordered-step' = { param($r) $first = $r.steps[0]; $r.steps[0] = $r.steps[1]; $r.steps[1] = $first }
        'wrong-step-action' = { param($r) $r.steps[5].action = 'press X' }
        'wrong-race-timeout' = { param($r) $r.timeouts_seconds.race_completion = 301 }
        'accept-second-place' = { param($r) $r.acceptance.required_finish_position = 2 }
        'omit-results' = { param($r) $r.acceptance.results_required = $false }
        'claim-persistence' = { param($r) $r.acceptance.save_persistence_claimed = $true }
        'claim-whole-frame' = { param($r) $r.acceptance.whole_frame_parity_claimed = $true }
        'omit-calibration' = { param($r) $r.acceptance.event_identity_must_be_calibrated_before_runtime_acceptance = $false }
        'wrong-exclusion' = { param($r) $r.exclusions[0] = 'none' }
        'exclusions-scalar' = { param($r) $r.exclusions = 'first-run-oobe' }
        'private-path' = { param($r) $r.status = 'private/game' }
        'absolute-path' = { param($r) $r.status = 'C:\secret' }
        'extra-property' = { param($r) $r | Add-Member -NotePropertyName extra -NotePropertyValue 1 }
    }
    foreach ($case in $cases.GetEnumerator()) { Assert-Rejected $case.Key $case.Value; $negativeCases++ }

    $saveFile = Get-ChildItem -LiteralPath $seedRoot -File -Recurse | Where-Object Length -eq 537428 | Select-Object -First 1
    $stream = [IO.File]::Open($saveFile.FullName, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try { $value = $stream.ReadByte(); $stream.Position = 0; $stream.WriteByte($value -bxor 1) } finally { $stream.Dispose() }
    try { & $validator -ConfigPath $sourceConfig -SeedRoot $seedRoot -ScreenshotRoot $screenshotRoot -FixtureMode | Out-Null; throw 'Mutated physical seed was accepted.' } catch { if ($_.Exception.Message -eq 'Mutated physical seed was accepted.') { throw } }
    $negativeCases++

    Remove-Item -LiteralPath $seedRoot -Recurse -Force
    [IO.Directory]::CreateDirectory($seedRoot) | Out-Null
    foreach ($item in @(Get-ChildItem -LiteralPath (Join-Path $repo 'private\baseline\M4-011\post-oobe-profile') -Force)) {
        Copy-Item -LiteralPath $item.FullName -Destination $seedRoot -Recurse -Force
    }
    $raceImage = Join-Path $screenshotRoot '545407F8 - 2026-08-11T01-02-28.png'
    $stream = [IO.File]::Open($raceImage, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try { $stream.Position = 100; $value = $stream.ReadByte(); $stream.Position = 100; $stream.WriteByte($value -bxor 1) } finally { $stream.Dispose() }
    try { & $validator -ConfigPath $sourceConfig -SeedRoot $seedRoot -ScreenshotRoot $screenshotRoot -FixtureMode | Out-Null; throw 'Mutated physical Xenia frame was accepted.' } catch { if ($_.Exception.Message -eq 'Mutated physical Xenia frame was accepted.') { throw } }
    $negativeCases++

    $configText = [IO.File]::ReadAllText($sourceConfig)
    $validatorText = [IO.File]::ReadAllText($validator)
    $sourceChecks = 0
    foreach ($required in @('"route_id": "pinned-save-sunset-strip-race-v1"', '"headlights": "Y"', '"native_identity_proven": false', '"required_finish_position": 1', '"save_persistence_claimed": false')) { if (-not $configText.Contains($required)) { throw "Config source contract is missing '$required'." }; $sourceChecks++ }
    foreach ($required in @('Assert-TreeWithoutReparse', 'Physical default XEX mismatch.', 'Physical seed tree mismatch.', 'Xenia race baseline', 'event_identity_must_be_calibrated_before_runtime_acceptance')) { if (-not $validatorText.Contains($required)) { throw "Verifier source contract is missing '$required'." }; $sourceChecks++ }
    if ($configText -match '(?i)([A-Z]:[\\/]|(?:^|["\\/])private[\\/])') { throw 'Tracked route config contains a private or absolute path.' }

    [pscustomobject]@{
        Passed = $true
        PositiveCases = 1
        FailClosedNegatives = $negativeCases
        SourceContractChecks = $sourceChecks
        PhysicalSeedMutationRejected = $true
        PhysicalBaselineMutationRejected = $true
    }
} finally {
    $resolvedFixture = [IO.Path]::GetFullPath($fixtureRoot)
    $privatePrefix = [IO.Path]::GetFullPath((Join-Path $repo 'private\evidence')).TrimEnd('\') + '\'
    if ($resolvedFixture.StartsWith($privatePrefix, [StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $resolvedFixture)) {
        Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
    }
}
