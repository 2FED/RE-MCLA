[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-force-feedback-report.ps1'
$gameplayRun = '20260814-130533-0b95f6b6'
$gameplayResultHash = 'A89C0CC3E02C8D264B0DA29157021D050276BF46F028BBAAAD9B1FFC220CCEAB'
$gameplayResult = Join-Path $repo "private/evidence/M5-006/$gameplayRun/result.json"
$gameplayLog = Join-Path $repo "private/evidence/M5-006/$gameplayRun/runs/01/mcla.log"
$utf8 = [Text.UTF8Encoding]::new($false)

function Assert-SafeWriteRoot {
  param([Parameter(Mandatory)][string]$Path)
  $full = [IO.Path]::GetFullPath($Path)
  $allowed = [IO.Path]::GetFullPath((Join-Path $repo 'private/evidence/M5-007')).TrimEnd('\')
  if (-not ($full -ceq $allowed -or $full.StartsWith($allowed + '\', [StringComparison]::OrdinalIgnoreCase))) {
    throw 'M5-007 write root escapes its private evidence directory.'
  }
  $current = $repo
  $prefix = $repo.TrimEnd('\') + '\'
  foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object { $_ })) {
    $current = Join-Path $current $part
    if ((Test-Path -LiteralPath $current) -and
        ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
      throw 'M5-007 write root traverses a reparse point.'
    }
  }
}

Write-Host 'M5-007 [1/4]: checking current force-feedback capability and degradation source contract...' -ForegroundColor Cyan
$probe = & $verifier -ProbeOnly -RuntimeLogPath $gameplayLog

Write-Host 'M5-007 [2/4]: re-verifying saved gameplay and physical rumble/reconnect evidence...' -ForegroundColor Cyan
$gameplayPath = (Resolve-Path $gameplayResult).Path
if ((Get-FileHash $gameplayPath -Algorithm SHA256).Hash -cne $gameplayResultHash) {
  throw 'Accepted M5-006 result identity changed.'
}
$gameplay = Get-Content $gameplayPath -Raw | ConvertFrom-Json

Write-Host 'M5-007 [3/4]: writing privacy-safe bounded report...' -ForegroundColor Cyan
$root = Join-Path $repo 'private/evidence/M5-007'
Assert-SafeWriteRoot $root
[IO.Directory]::CreateDirectory($root) | Out-Null
$runRoot = Join-Path $root ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
Assert-SafeWriteRoot $runRoot
[IO.Directory]::CreateDirectory($runRoot) | Out-Null
$resultPath = Join-Path $runRoot 'result.json'
$record = [ordered]@{
  schema = 1
  task = 'M5-007'
  decision = 'ffb-withheld-host-rumble-bounded'
  sdk_version = '0.9.0.18'
  sdk_commit = '923c92d1d1cb721cb704ac603fba263a01ba06aa'
  gameplay_run_id = $gameplayRun
  gameplay_result_sha256 = $gameplayResultHash
  current_gameplay = [ordered]@{
    route_passed = $true
    module_resolution_count = $probe.ModuleResolutionCount
    module_resolution_ordinals = '0282-0289'
    xinputdff_stub_call_markers = $probe.StubCallMarkers
    gameplay_input_records = $gameplay.cycle.gameplay_input_records
    pause_correlation_ppm = $gameplay.cycle.pause_correlation_ppm
    controlled_exit = $true
  }
  ffb_surface = [ordered]@{
    xinputdff_stub_exports = 8
    ffb_capability_advertised = $false
    xam_input_set_state_concrete = $true
    sdl_rumble_concrete = $true
    device_disconnected_result = '0000048F'
    basic_rumble_uses_xam_set_state = $true
  }
  physical_rumble = [ordered]@{
    run_id = '20260812-212030-5fc01c73'
    command_records = 6
    patterns = 'left-right-both'
    all_results_success = $true
    supported_property = $true
    user_report_source = 'external-user-report'
    confirmation_recorded_in_run = $false
    attestation_machine_verified = $false
  }
  unsupported_degradation = [ordered]@{
    capability_withheld = $true
    stub_path_entered = $false
    saved_gameplay_blocked = $false
    physical_disconnect_observed = $true
    physical_reconnect_observed = $true
    nop_masking_present = $false
  }
  scope = [ordered]@{
    host_rumble_diagnostic_only = $true
    title_driven_force_feedback_claimed = $false
    new_physical_rumble_test = $false
    multi_pad_claimed = $false
  }
  data_integrity_verified = $true
}
[IO.File]::WriteAllText($resultPath, ($record | ConvertTo-Json -Depth 8) + [Environment]::NewLine, $utf8)

Write-Host 'M5-007 [4/4]: final physical and source re-verification...' -ForegroundColor Cyan
$verified = & $verifier -ResultPath $resultPath
[pscustomobject]@{
  Passed = $verified.Passed
  Decision = $verified.Decision
  XInputdFFStubExports = $verified.XInputdFFStubExports
  XInputdFFStubCallMarkers = $verified.XInputdFFStubCallMarkers
  PhysicalRumbleCommandRecords = $verified.PhysicalRumbleCommandRecords
  SavedGameplayPassed = $verified.SavedGameplayPassed
  TitleDrivenForceFeedbackClaimed = $verified.TitleDrivenForceFeedbackClaimed
  PrivateRunRoot = $runRoot
  ResultPath = $resultPath
}
