[CmdletBinding(DefaultParameterSetName = 'Evidence')]
param(
  [Parameter(ParameterSetName = 'Evidence')]
  [string]$AcceptedResultPath = 'private/evidence/M5-003/20260814-104624-fde51a30/result.json',

  [Parameter(Mandatory, ParameterSetName = 'Probe')]
  [string]$RuntimeLogPath,

  [Parameter(ParameterSetName = 'Probe')]
  [switch]$FixtureMode,

  [Parameter(Mandatory, ParameterSetName = 'Result')]
  [string]$ResultPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$acceptedRun = '20260814-104624-fde51a30'
$acceptedHash = '299392E59CB38AFB44256E773884856FF0C869A96D2045086A65396C1ED4EFCA'
$evidenceSdkCommit = '923c92d1d1cb721cb704ac603fba263a01ba06aa'
$currentSdkCommit = '576b34fd233acf4579dd2375691dbe86fb4bf8e1'
$mainEvidenceCommit = 'c7ec3b672ff339228c5e53a805d8a92657642951'
$requiredFormats = @(
  'k_1_5_5_5',
  'k_16_16_16_16_FLOAT',
  'k_24_8_FLOAT',
  'k_32_FLOAT',
  'k_8',
  'k_8_8_8_8',
  'k_DXT1',
  'k_DXT2_3',
  'k_DXT4_5'
)

function Resolve-SafePath {
  param(
    [Parameter(Mandatory)]
    [string]$Path,

    [Parameter(Mandatory)]
    [string]$Description,

    [switch]$Directory
  )

  if ([IO.Path]::IsPathRooted($Path)) {
    $fullPath = [IO.Path]::GetFullPath($Path)
  } else {
    $fullPath = [IO.Path]::GetFullPath((Join-Path $repo $Path))
  }

  $repoPrefix = $repo.TrimEnd('\') + '\'
  if (-not $fullPath.StartsWith($repoPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "$Description escapes the repository."
  }

  $current = $repo
  foreach ($part in @($fullPath.Substring($repoPrefix.Length).Split('\') | Where-Object { $_.Length -gt 0 })) {
    $current = Join-Path $current $part
    if (-not (Test-Path -LiteralPath $current)) {
      throw "$Description is missing."
    }
    if ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
      throw "$Description traverses a reparse point."
    }
  }

  if ($Directory) {
    if (-not (Test-Path -LiteralPath $fullPath -PathType Container)) {
      throw "$Description is not a directory."
    }
  } elseif (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
    throw "$Description is not a file."
  }

  return $fullPath
}

function Get-OrderedRuntimeLogLines {
  param([Parameter(Mandatory)][string]$CurrentLogPath)

  $currentLog = Resolve-SafePath -Path $CurrentLogPath -Description 'Runtime log'
  if ((Split-Path $currentLog -Leaf) -cne 'mcla.log') {
    throw 'Current runtime log must be named mcla.log.'
  }

  $files = @(Get-ChildItem -LiteralPath (Split-Path $currentLog -Parent) -File -Filter 'mcla*.log')
  if ($files.Count -lt 1 -or $files.Count -gt 24) {
    throw 'Runtime-log topology is invalid.'
  }

  $rotations = @()
  $current = $null
  foreach ($file in $files) {
    if ($file.Name -ceq 'mcla.log') {
      if ($null -ne $current) {
        throw 'Duplicate current runtime log.'
      }
      $current = $file
      continue
    }

    $match = [regex]::Match($file.Name, '^mcla\.([1-9][0-9]*)\.log$')
    if (-not $match.Success) {
      throw 'Malformed runtime-log rotation.'
    }
    $rotations += [pscustomobject]@{
      Index = [int]$match.Groups[1].Value
      File = $file
    }
  }

  if ($null -eq $current) {
    throw 'Current runtime log is missing.'
  }

  $indices = @($rotations | ForEach-Object { $_.Index } | Sort-Object)
  for ($index = 0; $index -lt $indices.Count; $index++) {
    if ($indices[$index] -ne ($index + 1)) {
      throw 'Runtime-log rotations are not contiguous.'
    }
  }

  $ordered = @($rotations | Sort-Object Index -Descending | ForEach-Object { $_.File }) + $current
  $lines = [Collections.Generic.List[string]]::new()
  $totalBytes = 0L
  foreach ($file in $ordered) {
    $totalBytes += $file.Length
    if ($totalBytes -gt 268435456) {
      throw 'Runtime logs exceed 256 MiB.'
    }
    foreach ($line in [IO.File]::ReadLines($file.FullName)) {
      $lines.Add($line)
    }
  }

  return ,$lines
}

function Get-KeyValueFields {
  param([Parameter(Mandatory)][string]$Line)

  $fields = [ordered]@{}
  foreach ($match in [regex]::Matches($Line, '(?<key>[a-z0-9_]+)=(?<value>[^\s]+)')) {
    $key = $match.Groups['key'].Value
    if ($fields.Contains($key)) {
      throw "Duplicate field '$key'."
    }
    $fields[$key] = $match.Groups['value'].Value
  }
  return $fields
}

function Get-OnlyLine {
  param(
    [Parameter(Mandatory)]$Lines,
    [Parameter(Mandatory)][string]$Pattern,
    [Parameter(Mandatory)][string]$Description
  )

  $matches = @($Lines | Where-Object { $_ -match $Pattern })
  if ($matches.Count -ne 1) {
    throw "$Description count is $($matches.Count), expected 1."
  }
  return $matches[0]
}

function Assert-SourceContract {
  $sdkRoot = Join-Path $repo 'third_party/rexglue-sdk'
  $actualSdkCommit = (& git -C $sdkRoot rev-parse HEAD).Trim()
  if ($actualSdkCommit -cne $currentSdkCommit) {
    throw "SDK commit changed: $actualSdkCommit."
  }
  & git -C $sdkRoot diff --quiet v0.9.0.18 v0.9.0.22 -- src/graphics/pipeline/texture/cache.cpp src/graphics/d3d12/texture_cache.cpp src/graphics/d3d12/pipeline_cache.cpp
  if ($LASTEXITCODE -ne 0) { throw 'Material-pipeline source changed after accepted evidence.' }

  $commonPath = Resolve-SafePath -Path 'third_party/rexglue-sdk/src/graphics/pipeline/texture/cache.cpp' -Description 'Texture cache source'
  $d3dPath = Resolve-SafePath -Path 'third_party/rexglue-sdk/src/graphics/d3d12/texture_cache.cpp' -Description 'D3D12 texture cache source'
  $common = [IO.File]::ReadAllText($commonPath)
  $d3d = [IO.File]::ReadAllText($d3dPath)

  $loadIndex = $common.IndexOf('LoadTextureDataFromResidentMemoryImpl(texture', [StringComparison]::Ordinal)
  $failureReturnIndex = $common.IndexOf('return false;', $loadIndex, [StringComparison]::Ordinal)
  $upToDateIndex = $common.IndexOf('texture.MakeUpToDateAndWatch', $failureReturnIndex, [StringComparison]::Ordinal)
  $loadedIndex = $common.IndexOf('texture.LogAction("Loaded")', $loadIndex, [StringComparison]::Ordinal)
  if (
    $loadIndex -lt 0 -or
    $failureReturnIndex -lt $loadIndex -or
    $upToDateIndex -lt $failureReturnIndex -or
    $loadedIndex -lt $upToDateIndex
  ) {
    throw 'Loaded marker no longer follows successful texture loading.'
  }

  $tiledIndex = $d3d.IndexOf('uint32_t(texture_key.tiled)', [StringComparison]::Ordinal)
  $dispatchIndex = $d3d.IndexOf('command_list.D3DDispatch', $tiledIndex, [StringComparison]::Ordinal)
  $copyIndex = $d3d.IndexOf('command_list.D3DCopyTextureRegion', $dispatchIndex, [StringComparison]::Ordinal)
  $successIndex = $d3d.IndexOf('return true;', $copyIndex, [StringComparison]::Ordinal)
  if ($tiledIndex -lt 0 -or $dispatchIndex -lt $tiledIndex -or $copyIndex -lt $dispatchIndex -or $successIndex -lt $copyIndex) {
    throw 'Tiled texture load dispatch/copy/success ordering changed.'
  }
}

function Measure-MaterialPipeline {
  param(
    [Parameter(Mandatory)][string]$LogPath,
    [switch]$IsFixture
  )

  $lines = Get-OrderedRuntimeLogLines -CurrentLogPath $LogPath
  $allText = $lines -join "`n"
  Get-OnlyLine -Lines $lines -Pattern 'XENOS_AUDIT_CONFIG v=1 backend=d3d12 rt_path=host ' -Description 'Xenos config' | Out-Null

  $summaryLine = Get-OnlyLine -Lines $lines -Pattern 'XENOS_AUDIT_PIPELINE_SUMMARY v=1 phase=checkpoint ' -Description 'Pipeline summary'
  $summary = Get-KeyValueFields -Line $summaryLine
  if (
    [int64]$summary.translate_vs_ok -lt 150 -or
    [int64]$summary.translate_ps_ok -lt 190 -or
    [int64]$summary.translate_fail -ne 0 -or
    [int64]$summary.pso_attempt -ne [int64]$summary.pso_ok -or
    [int64]$summary.pso_fail -ne 0 -or
    [int64]$summary.pso_ok -lt 300 -or
    [int64]$summary.shader_records -ne 256 -or
    [int64]$summary.shader_overflow -lt 1 -or
    [int64]$summary.pso_records -ne [int64]$summary.pso_ok -or
    [int64]$summary.pso_overflow -ne 0
  ) {
    throw 'Shader/PSO aggregate coverage failed.'
  }

  $shaderLines = @($lines | Where-Object { $_ -match 'XENOS_AUDIT_SHADER v=1 ' })
  $psoLines = @($lines | Where-Object { $_ -match 'XENOS_AUDIT_PSO v=1 ' })
  if ($shaderLines.Count -ne 256 -or $psoLines.Count -ne [int]$summary.pso_records) {
    throw 'Shader/PSO record cardinality failed.'
  }

  $shaderIds = @($shaderLines | ForEach-Object { [int](Get-KeyValueFields -Line $_).id } | Sort-Object -Unique)
  if ($shaderIds.Count -ne 256 -or $shaderIds[0] -ne 0 -or $shaderIds[-1] -ne 255) {
    throw 'Shader record IDs are not unique and contiguous.'
  }

  $vertexRecords = 0
  $pixelRecords = 0
  foreach ($line in $shaderLines) {
    $fields = Get-KeyValueFields -Line $line
    if (
      $fields.result -cne 'ok' -or
      $fields.ucode -notmatch '^[0-9A-F]{16}$' -or
      $fields.modification -notmatch '^[0-9A-F]{16}$'
    ) {
      throw 'Shader detail record failed.'
    }
    if ($fields.stage -ceq 'vs') {
      $vertexRecords++
    } elseif ($fields.stage -ceq 'ps') {
      $pixelRecords++
    } else {
      throw 'Unknown shader stage.'
    }
  }

  $psoDescriptions = @{}
  foreach ($line in $psoLines) {
    $fields = Get-KeyValueFields -Line $line
    if (
      $fields.result -cne 'ok' -or
      $fields.hresult -cne '00000000' -or
      $fields.desc -notmatch '^[0-9A-F]{16}$' -or
      $psoDescriptions.ContainsKey($fields.desc)
    ) {
      throw 'PSO detail record failed.'
    }
    $psoDescriptions[$fields.desc] = $true
  }

  $texturePattern = '(?<verb>Loaded|Created) (?<layout>tiled|linear) (?<w>[0-9]+)x(?<h>[0-9]+)x(?<d>[0-9]+) (?<dim>[A-Za-z0-9]+) (?<fmt>k_[A-Za-z0-9_]+) texture with (?<mips>[0-9]+) (?<packing>packed|unpacked) mip'
  $successfulLoads = 0L
  $tiledLoads = 0L
  $linearLoads = 0L
  $packedMipLoads = 0L
  $unpackedMipLoads = 0L
  $formatCounts = @{}
  $dimensionClasses = @{}

  foreach ($line in $lines) {
    $match = [regex]::Match($line, $texturePattern)
    if (-not $match.Success -or $match.Groups['verb'].Value -cne 'Loaded') {
      continue
    }

    $successfulLoads++
    if ($match.Groups['layout'].Value -ceq 'tiled') {
      $tiledLoads++
    } else {
      $linearLoads++
    }
    if ($match.Groups['packing'].Value -ceq 'packed') {
      $packedMipLoads++
    } else {
      $unpackedMipLoads++
    }

    $format = $match.Groups['fmt'].Value
    if (-not $formatCounts.ContainsKey($format)) {
      $formatCounts[$format] = 0L
    }
    $formatCounts[$format]++

    $dimension = '{0}x{1}x{2} {3}' -f $match.Groups['w'].Value, $match.Groups['h'].Value, $match.Groups['d'].Value, $match.Groups['dim'].Value
    if (-not $dimensionClasses.ContainsKey($dimension)) {
      $dimensionClasses[$dimension] = 0L
    }
    $dimensionClasses[$dimension]++
  }

  if ($IsFixture) {
    $minimumLoads = 900
    $minimumTiled = 850
    $minimumPacked = 300
    $minimumUnpacked = 500
    $minimumDimensions = 30
  } else {
    $minimumLoads = 100000
    $minimumTiled = 100000
    $minimumPacked = 30000
    $minimumUnpacked = 80000
    $minimumDimensions = 40
  }

  if (
    $successfulLoads -lt $minimumLoads -or
    $tiledLoads -lt $minimumTiled -or
    $linearLoads -lt 1 -or
    $packedMipLoads -lt $minimumPacked -or
    $unpackedMipLoads -lt $minimumUnpacked -or
    $formatCounts.Count -ne 9 -or
    $dimensionClasses.Count -lt $minimumDimensions
  ) {
    throw 'Texture load coverage floor failed.'
  }

  foreach ($format in $requiredFormats) {
    if (-not $formatCounts.ContainsKey($format) -or $formatCounts[$format] -lt 1) {
      throw "Representative texture format '$format' is missing."
    }
  }

  if ($allText -match '(?i)(Failed to (?:create|load).*texture|Texture fetch constant .*invalid|texture is too (?:wide|tall)|XENOS_AUDIT_FAILURE|\[fatal\]|D3D12.*device (?:lost|removed))') {
    throw 'Texture/shader failure marker found.'
  }

  return [pscustomobject]@{
    Passed = $true
    Decision = 'representative-material-pipeline-pass'
    VsTranslations = [int64]$summary.translate_vs_ok
    PsTranslations = [int64]$summary.translate_ps_ok
    ShaderRecords = $shaderLines.Count
    VsRecords = $vertexRecords
    PsRecords = $pixelRecords
    ShaderOverflow = [int64]$summary.shader_overflow
    PsoOk = [int64]$summary.pso_ok
    TextureLoads = $successfulLoads
    TiledLoads = $tiledLoads
    LinearLoads = $linearLoads
    PackedMipLoads = $packedMipLoads
    UnpackedMipLoads = $unpackedMipLoads
    FormatCounts = $formatCounts
    DimensionClasses = $dimensionClasses.Count
  }
}

function Test-AcceptedEvidence {
  param([Parameter(Mandatory)][string]$Path)

  Assert-SourceContract
  $acceptedResult = Resolve-SafePath -Path $Path -Description 'Accepted M5-003 result'
  $runDirectory = Split-Path $acceptedResult -Parent
  if (
    (Split-Path $runDirectory -Leaf) -cne $acceptedRun -or
    (Get-FileHash -LiteralPath $acceptedResult -Algorithm SHA256).Hash -cne $acceptedHash
  ) {
    throw 'Accepted M5-003 identity changed.'
  }

  $renderingVerifier = Join-Path $PSScriptRoot 'verify-rendering-smoke.ps1'
  $rendering = & $renderingVerifier -ResultPath $acceptedResult
  if (-not $rendering.Passed -or -not $rendering.OwnerVisualPass) {
    throw 'M5-003 physical rendering evidence failed.'
  }

  return Measure-MaterialPipeline -LogPath (Join-Path $runDirectory 'runs/01/mcla.log')
}

function Assert-ExactPropertyOrder {
  param(
    [Parameter(Mandatory)]$Object,
    [Parameter(Mandatory)][string[]]$Expected,
    [Parameter(Mandatory)][string]$Description
  )

  if (($Object.PSObject.Properties.Name -join ',') -cne ($Expected -join ',')) {
    throw "$Description schema changed."
  }
}

if ($PSCmdlet.ParameterSetName -eq 'Probe') {
  Measure-MaterialPipeline -LogPath $RuntimeLogPath -IsFixture:$FixtureMode
  return
}

if ($PSCmdlet.ParameterSetName -eq 'Evidence') {
  Test-AcceptedEvidence -Path $AcceptedResultPath
  return
}

$reportPath = Resolve-SafePath -Path $ResultPath -Description 'M5-005 report'
$rawReport = [IO.File]::ReadAllText($reportPath)
if ($rawReport -match '(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/]|base_page|mip_page|guest_address)') {
  throw 'Report contains a private path or address.'
}

$report = $rawReport | ConvertFrom-Json
Assert-ExactPropertyOrder -Object $report -Expected @(
  'schema',
  'task',
  'decision',
  'sdk_version',
  'sdk_commit',
  'main_evidence_commit',
  'accepted_m5_003',
  'shader',
  'texture',
  'visual',
  'scope',
  'data_integrity_verified'
) -Description 'M5-005 report'

if (
  $report.schema -isnot [int] -or $report.schema -ne 1 -or
  $report.task -cne 'M5-005' -or
  $report.decision -cne 'representative-material-pipeline-pass' -or
  $report.sdk_version -cne '0.9.0.18' -or
  $report.sdk_commit -cne $evidenceSdkCommit -or
  $report.main_evidence_commit -cne $mainEvidenceCommit -or
  $report.data_integrity_verified -isnot [bool] -or -not $report.data_integrity_verified
) {
  throw 'M5-005 report identity failed.'
}

$reportRoot = Split-Path $reportPath -Parent
if (
  (Split-Path $reportPath -Leaf) -cne 'result.json' -or
  (Split-Path (Split-Path $reportRoot -Parent) -Leaf) -cne 'M5-005' -or
  (Split-Path $reportRoot -Leaf) -notmatch '^[0-9]{8}-[0-9]{6}-[0-9a-f]{8}$' -or
  (@(Get-ChildItem -LiteralPath $reportRoot -Force | ForEach-Object { $_.Name }) -join ',') -cne 'result.json'
) {
  throw 'M5-005 report topology failed.'
}

Assert-ExactPropertyOrder -Object $report.accepted_m5_003 -Expected @('run_id', 'result_sha256') -Description 'M5-003 binding'
if (
  $report.accepted_m5_003.run_id -cne $acceptedRun -or
  $report.accepted_m5_003.result_sha256 -cne $acceptedHash
) {
  throw 'M5-003 binding failed.'
}

$physical = Test-AcceptedEvidence -Path "private/evidence/M5-003/$acceptedRun/result.json"
Assert-ExactPropertyOrder -Object $report.shader -Expected @(
  'vs_translations',
  'ps_translations',
  'bounded_shader_records',
  'bounded_shader_overflow',
  'pso_ok'
) -Description 'Shader result'
foreach ($property in $report.shader.PSObject.Properties) {
  if ($property.Value -isnot [int]) {
    throw "Shader field '$($property.Name)' must be an integer."
  }
}
if (
  $report.shader.vs_translations -ne $physical.VsTranslations -or
  $report.shader.ps_translations -ne $physical.PsTranslations -or
  $report.shader.bounded_shader_records -ne $physical.ShaderRecords -or
  $report.shader.bounded_shader_overflow -ne $physical.ShaderOverflow -or
  $report.shader.pso_ok -ne $physical.PsoOk
) {
  throw 'Shader result mismatch.'
}

Assert-ExactPropertyOrder -Object $report.texture -Expected @(
  'successful_loads',
  'tiled_loads',
  'linear_loads',
  'packed_mip_loads',
  'unpacked_mip_loads',
  'format_count',
  'dimension_class_count',
  'formats'
) -Description 'Texture result'
foreach ($propertyName in @(
  'successful_loads',
  'tiled_loads',
  'linear_loads',
  'packed_mip_loads',
  'unpacked_mip_loads',
  'format_count',
  'dimension_class_count'
)) {
  if ($report.texture.$propertyName -isnot [int]) {
    throw "Texture field '$propertyName' must be an integer."
  }
}
if (
  $report.texture.successful_loads -ne $physical.TextureLoads -or
  $report.texture.tiled_loads -ne $physical.TiledLoads -or
  $report.texture.linear_loads -ne $physical.LinearLoads -or
  $report.texture.packed_mip_loads -ne $physical.PackedMipLoads -or
  $report.texture.unpacked_mip_loads -ne $physical.UnpackedMipLoads -or
  $report.texture.format_count -ne 9 -or
  $report.texture.dimension_class_count -ne $physical.DimensionClasses
) {
  throw 'Texture result mismatch.'
}

$formatNames = @($report.texture.formats.PSObject.Properties.Name | Sort-Object)
if (($formatNames -join ',') -cne (($requiredFormats | Sort-Object) -join ',')) {
  throw 'Texture format schema changed.'
}
foreach ($format in $requiredFormats) {
  if ($report.texture.formats.$format -isnot [int]) {
    throw "Texture count for '$format' must be an integer."
  }
  if ($report.texture.formats.$format -ne $physical.FormatCounts[$format]) {
    throw "Texture count mismatch for '$format'."
  }
}

Assert-ExactPropertyOrder -Object $report.visual -Expected @('owner_categories_pass', 'baseline_contact_sheet_bound') -Description 'Visual evidence'
if (
  $report.visual.owner_categories_pass -isnot [bool] -or
  $report.visual.baseline_contact_sheet_bound -isnot [bool] -or
  -not $report.visual.owner_categories_pass -or
  -not $report.visual.baseline_contact_sheet_bound
) {
  throw 'Visual evidence is missing.'
}

Assert-ExactPropertyOrder -Object $report.scope -Expected @(
  'representative_materials_only',
  'whole_frame_equivalence_claimed',
  'all_formats_claimed',
  'raw_texture_data_published'
) -Description 'M5-005 scope'
if (
  $report.scope.representative_materials_only -isnot [bool] -or
  $report.scope.whole_frame_equivalence_claimed -isnot [bool] -or
  $report.scope.all_formats_claimed -isnot [bool] -or
  $report.scope.raw_texture_data_published -isnot [bool] -or
  -not $report.scope.representative_materials_only -or
  $report.scope.whole_frame_equivalence_claimed -or
  $report.scope.all_formats_claimed -or
  $report.scope.raw_texture_data_published
) {
  throw 'M5-005 scope overclaims coverage.'
}

[pscustomobject]@{
  Passed = $true
  Decision = $report.decision
  ShaderTranslations = $physical.VsTranslations + $physical.PsTranslations
  PsoOk = $physical.PsoOk
  TextureLoads = $physical.TextureLoads
  TiledLoads = $physical.TiledLoads
  Formats = $physical.FormatCounts.Count
  DataIntegrityVerified = $true
}
