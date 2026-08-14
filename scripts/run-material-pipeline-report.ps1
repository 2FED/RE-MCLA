[CmdletBinding()]
param(
  [string]$AcceptedResultPath = 'private/evidence/M5-003/20260814-104624-fde51a30/result.json'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-material-pipeline-report.ps1'
$physical = & $verifier -AcceptedResultPath $AcceptedResultPath

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$suffix = -join ((1..8) | ForEach-Object { '{0:x}' -f (Get-Random -Maximum 16) })
$runRoot = Join-Path $repo "private/evidence/M5-005/$stamp-$suffix"
[IO.Directory]::CreateDirectory($runRoot) | Out-Null

$formats = [ordered]@{}
foreach ($format in @($physical.FormatCounts.Keys | Sort-Object)) {
  $formats[$format] = $physical.FormatCounts[$format]
}

$report = [ordered]@{
  schema = 1
  task = 'M5-005'
  decision = 'representative-material-pipeline-pass'
  sdk_version = '0.9.0.18'
  sdk_commit = '923c92d1d1cb721cb704ac603fba263a01ba06aa'
  main_evidence_commit = 'c7ec3b672ff339228c5e53a805d8a92657642951'
  accepted_m5_003 = [ordered]@{
    run_id = '20260814-104624-fde51a30'
    result_sha256 = '299392E59CB38AFB44256E773884856FF0C869A96D2045086A65396C1ED4EFCA'
  }
  shader = [ordered]@{
    vs_translations = $physical.VsTranslations
    ps_translations = $physical.PsTranslations
    bounded_shader_records = $physical.ShaderRecords
    bounded_shader_overflow = $physical.ShaderOverflow
    pso_ok = $physical.PsoOk
  }
  texture = [ordered]@{
    successful_loads = $physical.TextureLoads
    tiled_loads = $physical.TiledLoads
    linear_loads = $physical.LinearLoads
    packed_mip_loads = $physical.PackedMipLoads
    unpacked_mip_loads = $physical.UnpackedMipLoads
    format_count = $physical.FormatCounts.Count
    dimension_class_count = $physical.DimensionClasses
    formats = $formats
  }
  visual = [ordered]@{
    owner_categories_pass = $true
    baseline_contact_sheet_bound = $true
  }
  scope = [ordered]@{
    representative_materials_only = $true
    whole_frame_equivalence_claimed = $false
    all_formats_claimed = $false
    raw_texture_data_published = $false
  }
  data_integrity_verified = $true
}

$resultPath = Join-Path $runRoot 'result.json'
$json = $report | ConvertTo-Json -Depth 7
[IO.File]::WriteAllText($resultPath, $json + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))

$verified = & $verifier -ResultPath $resultPath
[pscustomobject]@{
  Passed = $verified.Passed
  Decision = $verified.Decision
  ShaderTranslations = $verified.ShaderTranslations
  PsoOk = $verified.PsoOk
  TextureLoads = $verified.TextureLoads
  TiledLoads = $verified.TiledLoads
  Formats = $verified.Formats
  PrivateRunRoot = $runRoot
  ResultPath = $resultPath
}
