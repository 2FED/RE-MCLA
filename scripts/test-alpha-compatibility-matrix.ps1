[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-alpha-compatibility-matrix.ps1'
$source = Join-Path $repo 'config/alpha-compatibility-matrix.json'
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('mcla-alpha-matrix-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($tempRoot) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)

function Clone-Matrix { (Get-Content -LiteralPath $source -Raw | ConvertFrom-Json) }
function Write-Fixture([string]$Name, $Value) {
    $path = Join-Path $tempRoot ($Name + '.json')
    [IO.File]::WriteAllText($path, (($Value | ConvertTo-Json -Depth 20) + [Environment]::NewLine), $utf8)
    return $path
}
function Expect-Fail([string]$Name, [scriptblock]$Mutate) {
    $value = Clone-Matrix
    & $Mutate $value
    $path = Write-Fixture $Name $value
    $failed = $false
    try { & $verify -MatrixPath $path -Fixture | Out-Null } catch { $failed = $true }
    if (-not $failed) { throw "Negative fixture passed: $Name" }
    $script:negative++
}

try {
    $positive = & $verify
    if (-not $positive.Passed -or $positive.CompatibilityRows -ne 13 -or $positive.KnownIssues -ne 24) { throw 'Positive matrix verification failed.' }
    $negative = 0
    Expect-Fail bad-schema { param($m) $m.schema = 'bad' }
    Expect-Fail bad-task { param($m) $m.task = 'M6-015' }
    Expect-Fail bad-decision { param($m) $m.decision = 'unbounded' }
    Expect-Fail bad-project-version { param($m) $m.tested_build.project_version = '0.7.0.1' }
    Expect-Fail bad-project-commit { param($m) $m.tested_build.project_commit = '0000000' }
    Expect-Fail bad-sdk-version { param($m) $m.tested_build.sdk_version = '0.9.0.30' }
    Expect-Fail bad-sdk-commit { param($m) $m.tested_build.sdk_commit = ('0' * 40) }
    Expect-Fail bad-title { param($m) $m.tested_build.title_id = '00000000' }
    Expect-Fail bad-media { param($m) $m.tested_build.media_id = '00000000' }
    Expect-Fail bad-module { param($m) $m.tested_build.module_hash = '0000000000000000' }
    Expect-Fail bad-source-hash { param($m) $m.tested_build.source_iso_sha256 = ('0' * 64) }
    Expect-Fail bad-frame-rate { param($m) $m.tested_build.stock_frame_rate = 60 }
    Expect-Fail bad-os { param($m) $m.reference_host.operating_system = 'Windows 10' }
    Expect-Fail bad-gpu { param($m) $m.reference_host.gpu = 'Unknown GPU' }
    Expect-Fail bad-resolution { param($m) $m.reference_host.physically_verified_resolutions = @('1280x720') }
    Expect-Fail named-audio-device { param($m) $m.reference_host.audio_output = 'Private device name' }
    Expect-Fail missing-wheel { param($m) $m.reference_host.physically_verified_wheels = @() }
    Expect-Fail missing-row { param($m) $m.rows = @($m.rows | Select-Object -First 12) }
    Expect-Fail duplicate-row { param($m) $m.rows[1].id = $m.rows[0].id }
    Expect-Fail bad-row-status { param($m) $m.rows[4].status = 'perfect' }
    Expect-Fail missing-evidence { param($m) $m.rows[0].evidence = @() }
    Expect-Fail outside-evidence { param($m) $m.rows[0].evidence = @('../outside.md') }
    Expect-Fail missing-evidence-file { param($m) $m.rows[0].evidence = @('docs/evidence/does-not-exist.md') }
    Expect-Fail missing-limitation { param($m) $m.rows[0].limitations = @() }
    Expect-Fail missing-known-issue { param($m) $m.known_issue_inventory = @($m.known_issue_inventory | Select-Object -First 22) }
    Expect-Fail bad-known-severity { param($m) $m.known_issue_inventory[12].severity = 'S0' }
    Expect-Fail bad-known-status { param($m) $m.known_issue_inventory[17].status = 'Open' }
    Expect-Fail broad-host-claim { param($m) $m.claims.single_reference_host_only = $false }
    Expect-Fail alternate-gpu-claim { param($m) $m.claims.alternate_gpu_verified = $true }
    Expect-Fail campaign-claim { param($m) $m.claims.full_campaign_claim = $true }
    Expect-Fail soak-complete-claim { param($m) $m.claims.current_five_stage_soak_complete = $true }
    [pscustomobject]@{ PositiveFixtures = 1; FailClosedNegatives = $negative; CompatibilityRows = 13; KnownIssues = 24; Passed = $true }
}
finally {
    if (Test-Path -LiteralPath $tempRoot) { Remove-Item -LiteralPath $tempRoot -Recurse -Force }
}
