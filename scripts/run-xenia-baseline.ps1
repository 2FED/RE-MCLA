[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$XeniaPath,
    [string]$XexPath,
    [string]$BaselinePath,
    [switch]$Resume,
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
if ($Resume -and -not $PSBoundParameters.ContainsKey('BaselinePath')) {
    throw 'Resume requires an explicit -BaselinePath for the isolated profile to reuse.'
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

function Assert-ContainedBaselineDirectory {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Container,
        [Parameter(Mandatory)][bool]$AllowExisting
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
    $fullContainer = [System.IO.Path]::GetFullPath($Container).TrimEnd('\')
    $prefix = "$fullContainer\"
    if (-not $fullPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Baseline path must be a child of '$fullContainer'. Got '$fullPath'."
    }
    if (Test-Path -LiteralPath $fullPath) {
        $baselineItem = Get-Item -LiteralPath $fullPath -Force
        if (-not $AllowExisting) {
            throw "Baseline path already exists and will not be overwritten: '$fullPath'."
        }
        if (-not $baselineItem.PSIsContainer) {
            throw "Resume baseline path is not a directory: '$fullPath'."
        }
        if ($baselineItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "Resume baseline path must not be a reparse point: '$fullPath'."
        }
    }
    elseif ($AllowExisting) {
        throw "Resume baseline path does not exist: '$fullPath'."
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
if ($Resume) {
    $xeniaProcessName = [System.IO.Path]::GetFileNameWithoutExtension($resolvedXenia)
    $activeXenia = @(
        Get-Process -Name $xeniaProcessName -ErrorAction SilentlyContinue |
            Where-Object { $_.Path -eq $resolvedXenia }
    )
    if ($activeXenia.Count -gt 0) {
        throw "Resume refused because the verified Xenia executable is already running (PID $($activeXenia.Id -join ', ')). Exit it normally before reusing the isolated profile."
    }
}
$resolvedBaseline = Assert-ContainedBaselineDirectory -Path $BaselinePath -Container $privateRoot -AllowExisting ([bool]$Resume)

$storagePath = Join-Path $resolvedBaseline 'storage'
$contentPath = Join-Path $resolvedBaseline 'content'
$cachePath = Join-Path $resolvedBaseline 'cache'
if ($Resume) {
    foreach ($requiredDirectory in @($storagePath, $contentPath, $cachePath)) {
        if (-not (Test-Path -LiteralPath $requiredDirectory -PathType Container)) {
            throw "Resume baseline is missing required directory: '$requiredDirectory'."
        }
        if ((Get-Item -LiteralPath $requiredDirectory -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "Resume baseline directory must not be a reparse point: '$requiredDirectory'."
        }
    }
    $resumeTimestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $logPath = Join-Path $resolvedBaseline "xenia-resume-$resumeTimestamp.log"
}
else {
    $logPath = Join-Path $resolvedBaseline 'xenia-stock.log'
}
if (Test-Path -LiteralPath $logPath) {
    throw "Session log already exists and will not be overwritten: '$logPath'."
}

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
    Resume         = [bool]$Resume
    ProcessId      = $null
    Started        = $false
    ExitCode       = $null
}

if ($Resume) {
    $operation = 'Resume verified XEX with an existing isolated profile and a new session log'
}
else {
    $operation = 'Create isolated stock Xenia baseline and launch the verified XEX'
}

if ($PSCmdlet.ShouldProcess($resolvedBaseline, $operation)) {
    if (-not $Resume) {
        New-Item -ItemType Directory -Path $storagePath, $contentPath, $cachePath -Force | Out-Null
    }
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
