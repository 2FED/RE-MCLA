[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$runner = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'run-race-back-camera-diagnostic.ps1'))
$app = [IO.File]::ReadAllText((Join-Path $repo 'src/mcla_app.cpp'))
$runnerNeedles = @(
  'mcla-race-back-camera-diagnostic-v2','KI-026','RACE BACK RETURNED','RACE BACK STUCK',
  'ReadOutcome','[Console]::KeyAvailable','Process exited while waiting','MCLA_RACE_BACK_CONFIG v=2',
  'MCLA_RACE_BACK_SELECT v=1','MCLA_RACE_BACK_COMMAND_RETURN v=1',
  'MCLA_RACE_BACK_CAMERA_HANDLER v=1','MCLA_RACE_BACK_CAMERA_APPLY_EDGE v=2','camera_apply_sites',
  'watch-soak-save.ps1','complete_profile_tree','--mcla_race_back_probe=true',
  'controlled_external_close = $true','force_cleanup = $false','--clean-first'
)
$appNeedles = @(
  'kRaceBackCommandAddress = 0x82666C50','kApplyGameCameraHandlerAddress = 0x822AD640',
  'kApplyGameCameraEdgeAddresses','RaceBackCommandProbe','ApplyGameCameraHandlerProbe',
  'MclaRaceBackCameraApplyEdge','mcla_race_back_probe','MCLA_RACE_BACK_CONFIG v=2'
)
$config = [IO.File]::ReadAllText((Join-Path $repo 'config/mcla_functions.toml'))
$checks = 0
foreach ($needle in $runnerNeedles) { if (-not $runner.Contains($needle)) { throw "Runner source contract missing: $needle" }; $checks++ }
foreach ($needle in $appNeedles) { if (-not $app.Contains($needle)) { throw "Application source contract missing: $needle" }; $checks++ }
foreach ($needle in @('address = 0x822A5990','address = 0x822AD698','address = 0x822B1258','address = 0x822B1464','address = 0x822B3460','address = 0x822B359C','name = "MclaRaceBackCameraApply822A5990"','name = "MclaRaceBackCameraApply822B359C"','registers = ["r3", "r4", "f1"]')) { if (-not $config.Contains($needle)) { throw "Codegen source contract missing: $needle" }; $checks++ }
foreach ($forbidden in @('Read-Host','Get-FileHash','Stop-Process -Name','Kill()')) { if ($runner.Contains($forbidden)) { throw "Runner contains forbidden construct: $forbidden" }; $checks++ }
[pscustomobject]@{ Passed = $true; SourceChecks = $checks }
