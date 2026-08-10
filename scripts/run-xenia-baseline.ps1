[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$XeniaPath,
    [string]$XexPath,
    [string]$BaselinePath,
    [switch]$Wait
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$privateRoot = Join-Path $repoRoot 'private'

if (-not $XeniaPath) {
    $XeniaPath = Join-Path $privateRoot 'tools\xenia-canary\artifacts\xenia_canary.exe'
}
if (-not $XexPath) {
    $XexPath = Join-Path $privateRoot 'game\default.xex'
}
if (-not $BaselinePath) {
    $timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $BaselinePath = Join-Path $privateRoot "baseline\stock-$timestamp"
}

$expectedXeniaSha256 = 'C51D73364180D5F09B29BC348732A5B79D3959D5639321BDA58D490B4ABCF06A'
$expectedXexSha256 = 'C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432'
$expectedXexSize = [long]9252864

function Assert-FileIdentity {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][long]$ExpectedSize,
        [Parameter(Mandatory)][string]$ExpectedSha256,
        [Parameter(Mandatory)][string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label was not found at '$Path'."
    }

    $item = Get-Item -LiteralPath $Path
    if ($ExpectedSize -ge 0 -and $item.Length -ne $ExpectedSize) {
        throw "$Label size mismatch. Expected $ExpectedSize, got $($item.Length)."
    }

    $actualHash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    if ($actualHash -ne $ExpectedSha256) {
        throw "$Label SHA-256 mismatch. Expected '$ExpectedSha256', got '$actualHash'."
    }
}

function Assert-ContainedNewDirectory {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Container
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
    $fullContainer = [System.IO.Path]::GetFullPath($Container).TrimEnd('\')
    $prefix = "$fullContainer\"
    if (-not $fullPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Baseline path must be a child of '$fullContainer'. Got '$fullPath'."
    }
    if (Test-Path -LiteralPath $fullPath) {
        throw "Baseline path already exists and will not be overwritten: '$fullPath'."
    }

    $cursor = Split-Path -Parent $fullPath
    while ($cursor -and $cursor.StartsWith($fullContainer, [System.StringComparison]::OrdinalIgnoreCase)) {
        if (Test-Path -LiteralPath $cursor) {
            $item = Get-Item -LiteralPath $cursor -Force
            if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
                throw "Baseline path traverses a reparse point: '$cursor'."
            }
        }
        if ($cursor -eq $fullContainer) {
            break
        }
        $cursor = Split-Path -Parent $cursor
    }

    return $fullPath
}

$resolvedXenia = (Resolve-Path -LiteralPath $XeniaPath).Path
$resolvedXex = (Resolve-Path -LiteralPath $XexPath).Path
Assert-FileIdentity -Path $resolvedXenia -ExpectedSize -1 -ExpectedSha256 $expectedXeniaSha256 -Label 'Xenia Canary executable'
Assert-FileIdentity -Path $resolvedXex -ExpectedSize $expectedXexSize -ExpectedSha256 $expectedXexSha256 -Label 'default.xex'
$resolvedBaseline = Assert-ContainedNewDirectory -Path $BaselinePath -Container $privateRoot

$storagePath = Join-Path $resolvedBaseline 'storage'
$contentPath = Join-Path $resolvedBaseline 'content'
$cachePath = Join-Path $resolvedBaseline 'cache'
$logPath = Join-Path $resolvedBaseline 'xenia-stock.log'

$arguments = @(
    "`"$resolvedXex`""
    "--storage_root=`"$storagePath`""
    "--content_root=`"$contentPath`""
    "--cache_root=`"$cachePath`""
    "--log_file=`"$logPath`""
    '--gpu=d3d12'
    '--apu=xaudio2'
    '--hid=any'
    '--apply_patches=false'
    '--apply_title_update=false'
    '--time_scalar=1'
    '--vsync=true'
    '--discord=false'
    '--fullscreen=false'
    '--headless=false'
    '--window_size_x=1280'
    '--window_size_y=720'
    '--storage_selection_dialog=false'
    '--log_level=2'
)

$result = [ordered]@{
    Validated      = $true
    XeniaPath      = $resolvedXenia
    XexPath        = $resolvedXex
    BaselinePath   = $resolvedBaseline
    LogPath        = $logPath
    StockTimeScale = 1
    PatchesEnabled = $false
    TitleUpdates   = $false
    ProcessId      = $null
    Started        = $false
    ExitCode       = $null
}

if ($PSCmdlet.ShouldProcess($resolvedBaseline, 'Create isolated stock Xenia baseline and launch the verified XEX')) {
    New-Item -ItemType Directory -Path $storagePath, $contentPath, $cachePath -Force | Out-Null
    $process = Start-Process -FilePath $resolvedXenia -ArgumentList $arguments `
        -WorkingDirectory (Split-Path -Parent $resolvedXenia) -PassThru
    $result.ProcessId = $process.Id
    $result.Started = $true
    if ($Wait) {
        $process.WaitForExit()
        $result.ExitCode = $process.ExitCode
    }
}

[pscustomobject]$result
