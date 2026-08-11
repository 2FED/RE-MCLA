[CmdletBinding()]
param(
    [string]$BuildRoot = 'out/build/win-amd64-release',
    [string]$GameRoot = 'private/game',
    [ValidateRange(1, 120)][int]$TimeoutSeconds = 30
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
function Resolve-ContainedPath {
    param([Parameter(Mandatory)][string]$Path, [Parameter(Mandatory)][string]$Description)
    $resolved = if ([System.IO.Path]::IsPathRooted($Path)) {
        (Resolve-Path -LiteralPath $Path).Path
    } else {
        (Resolve-Path -LiteralPath (Join-Path $repoRoot $Path)).Path
    }
    $prefix = $repoRoot.TrimEnd('\') + '\'
    if (-not $resolved.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "$Description must stay inside the repository: '$resolved'."
    }
    return $resolved
}

function Get-GameSnapshot {
    param([Parameter(Mandatory)][string]$Root)
    $entries = @(
        Get-ChildItem -LiteralPath $Root -File -Recurse -Force |
            ForEach-Object {
                [pscustomobject][ordered]@{
                    Path = $_.FullName.Substring($Root.Length).TrimStart('\').Replace('\', '/')
                    Length = $_.Length
                    LastWriteUtcTicks = $_.LastWriteTimeUtc.Ticks
                    Attributes = [int]$_.Attributes
                }
            } |
            Sort-Object Path
    )
    $json = $entries | ConvertTo-Json -Compress
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $snapshotHash = -join ($sha.ComputeHash($bytes) | ForEach-Object { $_.ToString('X2') })
    } finally {
        $sha.Dispose()
    }
    return [pscustomobject]@{
        FileCount = $entries.Count
        PayloadBytes = [uint64](($entries | Measure-Object -Property Length -Sum).Sum)
        MetadataSha256 = $snapshotHash
    }
}

$buildRootPath = Resolve-ContainedPath -Path $BuildRoot -Description 'Build root'
$gameRootPath = Resolve-ContainedPath -Path $GameRoot -Description 'Game root'
$executablePath = Join-Path $buildRootPath 'mcla.exe'
$xexPath = Join-Path $gameRootPath 'default.xex'
foreach ($path in @($executablePath, $xexPath)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required VFS probe input was not found: '$path'."
    }
}

$before = Get-GameSnapshot -Root $gameRootPath
$xexHashBefore = (Get-FileHash -LiteralPath $xexPath -Algorithm SHA256).Hash
$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 8)
$runRoot = Join-Path $repoRoot "private/evidence/M3-004/$runId"
$userRoot = Join-Path $runRoot 'user'
$cacheRoot = Join-Path $runRoot 'cache'
[System.IO.Directory]::CreateDirectory($userRoot) | Out-Null
[System.IO.Directory]::CreateDirectory($cacheRoot) | Out-Null
$logPath = Join-Path $runRoot 'mcla-vfs.log'

$process = Start-Process -FilePath $executablePath -ArgumentList @(
    '--mcla_vfs_probe',
    "--game_data_root=$gameRootPath",
    "--user_data_root=$userRoot",
    "--cache_root=$cacheRoot",
    "--log_file=$logPath",
    '--log_level=trace'
) -WorkingDirectory $buildRootPath -PassThru

if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
    Stop-Process -Id $process.Id -Force
    throw "VFS probe exceeded $TimeoutSeconds seconds. Private run: '$runRoot'."
}
if ($process.ExitCode -ne 0) {
    throw "VFS probe exited with code $($process.ExitCode). Private run: '$runRoot'."
}

$after = Get-GameSnapshot -Root $gameRootPath
$xexHashAfter = (Get-FileHash -LiteralPath $xexPath -Algorithm SHA256).Hash
if ($before.FileCount -ne 15 -or $before.PayloadBytes -ne 6569586392 -or
    $before.MetadataSha256 -ne $after.MetadataSha256 -or
    $xexHashBefore -ne $xexHashAfter -or
    (Test-Path -LiteralPath (Join-Path $gameRootPath '__mcla_vfs_write_probe.tmp'))) {
    throw "Game-root containment check failed. Private run: '$runRoot'."
}

$markers = @(
    'MCLA VFS: game: and d: resolve 3/3 expected disc files',
    'MCLA VFS: root-escape paths rejected',
    'MCLA VFS: write, create, delete, and writable-map requests denied',
    'MCLA VFS: probe complete; guest launch skipped',
    'MCLA lifecycle: shutdown'
)
$logText = Get-Content -LiteralPath $logPath -Raw
$offset = -1
foreach ($marker in $markers) {
    $next = $logText.IndexOf($marker, $offset + 1, [System.StringComparison]::Ordinal)
    if ($next -lt 0) {
        throw "Missing or out-of-order VFS marker: '$marker'. Private run: '$runRoot'."
    }
    $offset = $next
}
if ($logText.IndexOf('Execution complete', [System.StringComparison]::Ordinal) -ge 0) {
    throw "VFS probe unexpectedly entered guest execution. Private run: '$runRoot'."
}

[pscustomobject]@{
    Passed = $true
    ExitCode = $process.ExitCode
    OrderedMarkers = $markers.Count
    GameFileCount = $after.FileCount
    PayloadBytes = $after.PayloadBytes
    GameMetadataSha256 = $after.MetadataSha256
    XexSha256 = $xexHashAfter
    PrivateRunRoot = $runRoot
    LogSha256 = (Get-FileHash -LiteralPath $logPath -Algorithm SHA256).Hash
}
