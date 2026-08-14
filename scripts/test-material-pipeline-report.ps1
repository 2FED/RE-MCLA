[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-material-pipeline-report.ps1'
$fixtureRoot = Join-Path $repo ('private/evidence/M5-005/test-' + [guid]::NewGuid().ToString('N'))
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null

function New-ProbeLines {
  $lines = [Collections.Generic.List[string]]::new()
  $lines.Add('XENOS_AUDIT_CONFIG v=1 backend=d3d12 rt_path=host bindless=1 scale_x=1 scale_y=1 native_2x_supported=1 gamma_rt_unorm16=1 depth_f24_ps=0 depth_f24_round=0 direct_host_resolve=1')

  foreach ($id in 0..255) {
    $stage = if ($id -lt 111) { 'vs' } else { 'ps' }
    $lines.Add(('XENOS_AUDIT_SHADER v=1 id={0} stage={1} ucode={0:X16} modification={2:X16} result=ok' -f $id, $stage, ($id % 41)))
  }

  foreach ($id in 0..331) {
    $lines.Add(('XENOS_AUDIT_PSO v=1 id={0} desc={0:X16} vs={1:X16} ps={2:X16} host_msaa=1 depth_fmt=1 depth_func=6 depth_write=1 result=ok hresult=00000000' -f $id, ($id + 1), ($id + 2)))
  }

  $formats = @(
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
  foreach ($id in 0..999) {
    $format = $formats[$id % $formats.Count]
    $layout = if ($id -lt 900) { 'tiled' } else { 'linear' }
    $packing = if ($id -lt 350) { 'packed' } else { 'unpacked' }
    $width = 1 -shl ($id % 11)
    $heightShift = ([int][Math]::Floor($id / 11)) % 10
    $height = 1 -shl $heightShift
    $lines.Add("Loaded $layout ${width}x${height}x1 2D $format texture with 1 $packing mip level, base at 0x00000000")
  }

  $lines.Add('XENOS_AUDIT_PIPELINE_SUMMARY v=1 phase=checkpoint shader_entries=312 translate_vs_ok=163 translate_ps_ok=197 translate_fail=0 pso_entries=332 pso_attempt=332 pso_ok=332 pso_fail=0 shader_records=256 shader_overflow=104 pso_records=332 pso_overflow=0')
  return ,$lines
}

function Invoke-Probe {
  param(
    [Parameter(Mandatory)]$Lines,
    [Parameter(Mandatory)][string]$Name
  )

  $directory = Join-Path $fixtureRoot $Name
  [IO.Directory]::CreateDirectory($directory) | Out-Null
  $logPath = Join-Path $directory 'mcla.log'
  [IO.File]::WriteAllLines($logPath, $Lines, $utf8)
  return & $verifier -RuntimeLogPath $logPath -FixtureMode
}

function Assert-Rejected {
  param(
    [Parameter(Mandatory)][scriptblock]$Action,
    [Parameter(Mandatory)][string]$Name
  )

  try {
    & $Action | Out-Null
    throw "Negative '$Name' was accepted."
  } catch {
    if ($_.Exception.Message -ceq "Negative '$Name' was accepted.") {
      throw
    }
  }
}

function Replace-First {
  param(
    [Parameter(Mandatory)]$Lines,
    [Parameter(Mandatory)][string]$Old,
    [Parameter(Mandatory)][string]$New
  )

  for ($index = 0; $index -lt $Lines.Count; $index++) {
    if ($Lines[$index].Contains($Old)) {
      $Lines[$index] = $Lines[$index].Replace($Old, $New)
      return
    }
  }
  throw "Fixture token '$Old' is missing."
}

try {
  $positive = Invoke-Probe -Lines (New-ProbeLines) -Name 'positive'
  if (-not $positive.Passed) {
    throw 'Positive fixture failed.'
  }

  $cases = [ordered]@{
    'shader-fail' = {
      param($lines)
      Replace-First -Lines $lines -Old 'result=ok' -New 'result=fail'
    }
    'shader-id-duplicate' = {
      param($lines)
      Replace-First -Lines $lines -Old 'id=255 stage=ps' -New 'id=254 stage=ps'
    }
    'shader-summary-fail' = {
      param($lines)
      Replace-First -Lines $lines -Old 'translate_fail=0' -New 'translate_fail=1'
    }
    'pso-fail' = {
      param($lines)
      Replace-First -Lines $lines -Old 'hresult=00000000' -New 'hresult=80004005'
    }
    'pso-description-duplicate' = {
      param($lines)
      Replace-First -Lines $lines -Old 'id=331 desc=000000000000014B' -New 'id=331 desc=000000000000014A'
    }
    'texture-load-floor' = {
      param($lines)
      $changed = 0
      for ($index = 589; $index -lt $lines.Count -and $changed -lt 101; $index++) {
        if ($lines[$index].StartsWith('Loaded ', [StringComparison]::Ordinal)) {
          $lines[$index] = 'Created ' + $lines[$index].Substring(7)
          $changed++
        }
      }
    }
    'tiled-floor' = {
      param($lines)
      for ($index = 589; $index -lt 689; $index++) {
        $lines[$index] = $lines[$index].Replace('Loaded tiled ', 'Loaded linear ')
      }
    }
    'packed-floor' = {
      param($lines)
      for ($index = 589; $index -lt 640; $index++) {
        $lines[$index] = $lines[$index].Replace(' packed ', ' unpacked ')
      }
    }
    'unpacked-floor' = {
      param($lines)
      for ($index = 939; $index -lt 1090; $index++) {
        $lines[$index] = $lines[$index].Replace(' unpacked ', ' packed ')
      }
    }
    'format-missing' = {
      param($lines)
      for ($index = 589; $index -lt $lines.Count - 1; $index++) {
        $lines[$index] = $lines[$index].Replace('k_DXT2_3', 'k_DXT1')
      }
    }
    'texture-failure-marker' = {
      param($lines)
      $lines.Add('Failed to load texture')
    }
    'invalid-fetch-marker' = {
      param($lines)
      $lines.Add('Texture fetch constant 7 is invalid')
    }
    'device-loss-marker' = {
      param($lines)
      $lines.Add('D3D12 device removed')
    }
    'duplicate-summary' = {
      param($lines)
      $lines.Add($lines[-1])
    }
  }

  $caseIndex = 0
  foreach ($case in $cases.GetEnumerator()) {
    $lines = New-ProbeLines
    & $case.Value $lines
    $name = 'negative-{0:D2}-{1}' -f $caseIndex, $case.Key
    Assert-Rejected -Name $case.Key -Action { Invoke-Probe -Lines $lines -Name $name }
    $caseIndex++
  }

  $source = [IO.File]::ReadAllText($verifier)
  $sourceNeedles = @(
    'representative-material-pipeline-pass',
    'Loaded marker no longer follows successful texture loading.',
    'Tiled texture load dispatch/copy/success ordering changed.',
    'raw_texture_data_published',
    '$requiredFormats',
    '100000',
    '80000'
  )
  foreach ($needle in $sourceNeedles) {
    if (-not $source.Contains($needle)) {
      throw "Source contract is missing '$needle'."
    }
  }

  [pscustomobject]@{
    Passed = $true
    PositiveFixtures = 1
    FailClosedNegatives = $cases.Count
    SourceChecks = $sourceNeedles.Count
  }
} finally {
  if (Test-Path -LiteralPath $fixtureRoot) {
    Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
  }
}
