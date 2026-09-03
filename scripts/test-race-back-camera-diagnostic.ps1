[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$runner = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'run-race-back-camera-diagnostic.ps1'))
$app = [IO.File]::ReadAllText((Join-Path $repo 'src/mcla_app.cpp'))
$runnerNeedles = @(
  'mcla-race-back-camera-diagnostic-v1','KI-026','RACE BACK RETURNED','RACE BACK STUCK',
  'ReadOutcome','[Console]::KeyAvailable','Process exited while waiting','MCLA_RACE_BACK_CONFIG v=1',
  'MCLA_RACE_BACK_SELECT v=1','MCLA_RACE_BACK_COMMAND_RETURN v=1',
  'MCLA_RACE_BACK_CAMERA_HANDLER v=1','MCLA_RACE_BACK_CAMERA_APPLY_EDGE v=1',
  'watch-soak-save.ps1','complete_profile_tree','--mcla_race_back_probe=true',
  'controlled_external_close = $true','force_cleanup = $false','--clean-first'
)
$appNeedles = @(
  'kRaceBackCommandAddress = 0x82666C50','kApplyGameCameraHandlerAddress = 0x822AD640',
  'kApplyGameCameraEdgeAddress = 0x822AD698','RaceBackCommandProbe','ApplyGameCameraHandlerProbe',
  'MclaRaceBackCameraApplyEdge','mcla_race_back_probe','MCLA_RACE_BACK_CONFIG v=1'
)
$config = [IO.File]::ReadAllText((Join-Path $repo 'config/mcla_functions.toml'))
$checks = 0
foreach ($needle in $runnerNeedles) { if (-not $runner.Contains($needle)) { throw "Runner source contract missing: $needle" }; $checks++ }
foreach ($needle in $appNeedles) { if (-not $app.Contains($needle)) { throw "Application source contract missing: $needle" }; $checks++ }
foreach ($needle in @('address = 0x822AD698','name = "MclaRaceBackCameraApplyEdge"','registers = ["r3", "r4", "f1"]')) { if (-not $config.Contains($needle)) { throw "Codegen source contract missing: $needle" }; $checks++ }
foreach ($forbidden in @('Read-Host','Get-FileHash','Stop-Process -Name','Kill()')) { if ($runner.Contains($forbidden)) { throw "Runner contains forbidden construct: $forbidden" }; $checks++ }
[pscustomobject]@{ Passed = $true; SourceChecks = $checks }
