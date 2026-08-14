[CmdletBinding()]
param(
    [string]$IsoPath,
    [string]$GamePath,
    [string]$ManifestPath,
    [string]$AstGrepPath,
    [string]$ReXGluePath,
    [string]$ExtractorPath,
    [string]$XeniaPath,
    [string]$GhidraRoot,
    [string]$JavaPath,
    [string]$RenderDocPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path

if (-not $IsoPath) {
    $IsoPath = Join-Path $repoRoot 'Midnight.Club.Los.Angeles.The.Complete.Edition.XBOX360\midmets4.iso'
}
if (-not $GamePath) {
    $GamePath = Join-Path $repoRoot 'private\game'
}
if (-not $ManifestPath) {
    $ManifestPath = Join-Path $repoRoot 'private\game-manifest.json'
}
if (-not $ReXGluePath) {
    $ReXGluePath = Join-Path $repoRoot 'third_party\rexglue-sdk\out\install\win-amd64\bin\rexglue.exe'
}
if (-not $ExtractorPath) {
    $ExtractorPath = Join-Path $repoRoot 'private\tools\extract-xiso\artifacts\extract-xiso.exe'
}
if (-not $XeniaPath) {
    $XeniaPath = Join-Path $repoRoot 'private\tools\xenia-canary\artifacts\xenia_canary.exe'
}
if (-not $GhidraRoot) {
    $GhidraRoot = Join-Path $repoRoot 'private\tools\ghidra\install\ghidra_12.0.4_PUBLIC'
}
if (-not $JavaPath) {
    $JavaPath = 'C:\Program Files\Eclipse Adoptium\jdk-21.0.12.8-hotspot\bin\java.exe'
}
if (-not $RenderDocPath) {
    $RenderDocPath = 'C:\Program Files\RenderDoc\renderdoccmd.exe'
}

$expected = [ordered]@{
    AstGrepVersion     = '0.45.0'
    ReXGlueCommit      = 'c4aa30c35386bb4d2ef051a59ea8e71bab667172'
    ReXGlueTag         = 'v0.9.0.20'
    ReXGlueVersion     = '0.9.0.20'
    ExtractorSha256    = '7C7AF9C17E095C3C1E78E644DF5F0E72F01C4690B3117F038AAFE26EB5A8A2F4'
    XeniaSha256        = 'C51D73364180D5F09B29BC348732A5B79D3959D5639321BDA58D490B4ABCF06A'
    GhidraVersion      = '12.0.4'
    XexLoaderVersion   = '13.0.0'
    XexLoaderJarSha256 = '6B0B2B470DF64300A0AE6E421A2593EA421FDB2795C018C8A21EEF92C9F3D339'
    JavaVersion        = '21.0.12'
    JavaSha256         = 'A38D821EFB69EF99C55D315B00D9E8B88F126743B8773E44154E1A3D193EFD41'
    RenderDocVersion   = '1.45'
    RenderDocSha256    = '273352017E23E890FE9134DE0157D1FE556676A4C6004BFE3265DB1A4648ED07'
}

$results = [System.Collections.Generic.List[object]]::new()

function Assert-Equal {
    param(
        [Parameter(Mandatory)]$Actual,
        [Parameter(Mandatory)]$Expected,
        [Parameter(Mandatory)][string]$Label
    )

    if ($Actual -ne $Expected) {
        throw "$Label mismatch. Expected '$Expected', got '$Actual'."
    }
}

function Assert-Leaf {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label was not found at '$Path'."
    }
}

function Assert-FileHash {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$ExpectedSha256,
        [Parameter(Mandatory)][string]$Label
    )

    Assert-Leaf -Path $Path -Label $Label
    $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
    Assert-Equal -Actual $actual -Expected $ExpectedSha256 -Label "$Label SHA-256"
}

function Invoke-NativeText {
    param(
        [Parameter(Mandatory)][string]$Executable,
        [Parameter(Mandatory)][string[]]$Arguments
    )

    Assert-Leaf -Path $Executable -Label 'Executable'
    $previousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        $output = & $Executable @Arguments 2>&1 | Out-String
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorAction
    }
    if ($exitCode -ne 0) {
        throw "'$Executable' exited with code $exitCode. $($output.Trim())"
    }
    return $output.Trim()
}

function Invoke-PrerequisiteCheck {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][scriptblock]$Action
    )

    try {
        $detail = (& $Action | Out-String).Trim()
        if (-not $detail) {
            $detail = 'validated'
        }
        $results.Add([pscustomobject]@{ Name = $Name; Status = 'PASS'; Detail = $detail })
        Write-Host "[PASS] $Name - $detail"
    }
    catch {
        $detail = $_.Exception.Message
        $results.Add([pscustomobject]@{ Name = $Name; Status = 'FAIL'; Detail = $detail })
        Write-Host "[FAIL] $Name - $detail"
    }
}

Invoke-PrerequisiteCheck -Name 'Repository and public version' -Action {
    $versionPath = Join-Path $repoRoot 'VERSION'
    Assert-Leaf -Path $versionPath -Label 'VERSION'
    $version = (Get-Content -LiteralPath $versionPath -Raw).Trim()
    if ($version -notmatch '^\d+\.\d+\.\d+\.\d+$') {
        throw "VERSION '$version' is not a four-part numeric version."
    }
    "VERSION $version"
}

Invoke-PrerequisiteCheck -Name 'Visual Studio, CMake, Ninja, and LLVM' -Action {
    $toolchain = & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1')
    "CMake $($toolchain.CMakeVersion), Ninja $($toolchain.NinjaVersion), clang-cl $($toolchain.ClangClVersion)"
}

Invoke-PrerequisiteCheck -Name 'ast-grep' -Action {
    if (-not $AstGrepPath) {
        $command = Get-Command 'ast-grep' -CommandType Application -ErrorAction Stop
        $resolvedAstGrep = $command.Source
    }
    else {
        $resolvedAstGrep = $AstGrepPath
    }
    $output = Invoke-NativeText -Executable $resolvedAstGrep -Arguments @('--version')
    if ($output -notmatch "(?m)ast-grep\s+$([regex]::Escape($expected.AstGrepVersion))\b") {
        throw "Unexpected ast-grep version output: $output"
    }
    "ast-grep $($expected.AstGrepVersion)"
}

Invoke-PrerequisiteCheck -Name 'ReXGlue recursive source pin' -Action {
    $sdkRoot = Join-Path $repoRoot 'third_party\rexglue-sdk'
    if (-not (Test-Path -LiteralPath $sdkRoot -PathType Container)) {
        throw "ReXGlue source directory was not found at '$sdkRoot'."
    }

    $gitCommand = Get-Command 'git' -CommandType Application -ErrorAction Stop | Select-Object -First 1
    $gitExe = $gitCommand.Source
    $gitRoot = Split-Path -Parent (Split-Path -Parent $gitExe)
    $gitUnixTools = Join-Path $gitRoot 'usr\bin'
    if (Test-Path -LiteralPath $gitUnixTools -PathType Container) {
        $env:Path = "$gitUnixTools;$env:Path"
    }

    $commit = Invoke-NativeText -Executable $gitExe -Arguments @('-c', 'safe.directory=*', '-C', $sdkRoot, 'rev-parse', 'HEAD')
    Assert-Equal -Actual $commit -Expected $expected.ReXGlueCommit -Label 'ReXGlue commit'

    $tagCommit = Invoke-NativeText -Executable $gitExe -Arguments @('-c', 'safe.directory=*', '-C', $sdkRoot, 'rev-parse', "$($expected.ReXGlueTag)^{commit}")
    Assert-Equal -Actual $tagCommit -Expected $expected.ReXGlueCommit -Label "ReXGlue $($expected.ReXGlueTag) target"

    $dirty = Invoke-NativeText -Executable $gitExe -Arguments @('-c', 'safe.directory=*', '-C', $sdkRoot, 'status', '--porcelain', '--untracked-files=no')
    if ($dirty) {
        throw "ReXGlue tracked worktree is dirty: $dirty"
    }

    $recursive = Invoke-NativeText -Executable $gitExe -Arguments @('-c', 'safe.directory=*', '-C', $sdkRoot, 'submodule', 'status', '--recursive')
    $badLines = @($recursive -split "`r?`n" | Where-Object { $_ -match '^[-+U]' })
    if ($badLines.Count -gt 0) {
        throw "Incomplete or mismatched recursive submodule state: $($badLines -join '; ')"
    }
    $nodeCount = @($recursive -split "`r?`n" | Where-Object { $_ }).Count
    if ($nodeCount -lt 2) {
        throw 'Recursive ReXGlue dependency status did not contain nested submodules.'
    }
    "$($expected.ReXGlueTag) at $($expected.ReXGlueCommit.Substring(0, 12)); $nodeCount recursive nodes"
}

Invoke-PrerequisiteCheck -Name 'ReXGlue installed CLI' -Action {
    $output = Invoke-NativeText -Executable $ReXGluePath -Arguments @('--version')
    Assert-Equal -Actual $output -Expected $expected.ReXGlueVersion -Label 'ReXGlue CLI version'
    "ReXGlue $output"
}

Invoke-PrerequisiteCheck -Name 'Pinned XDVDFS extractor' -Action {
    Assert-FileHash -Path $ExtractorPath -ExpectedSha256 $expected.ExtractorSha256 -Label 'extract-xiso executable'
    "extract-xiso SHA-256 $($expected.ExtractorSha256.Substring(0, 12))..."
}

Invoke-PrerequisiteCheck -Name 'Supported source ISO and XEX' -Action {
    $verified = & (Join-Path $PSScriptRoot 'verify-source.ps1') -IsoPath $IsoPath
    if (-not $verified.Valid) {
        throw 'Source verifier returned Valid=False.'
    }
    "Title ID $($verified.TitleId), Media ID $($verified.MediaId), source hash verified"
}

Invoke-PrerequisiteCheck -Name 'Extracted game manifest' -Action {
    $verified = & (Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath $GamePath -ManifestPath $ManifestPath -VerifyHashes
    if (-not $verified.Valid) {
        throw 'Game manifest verifier returned Valid=False.'
    }
    "$($verified.FileCount) files, $($verified.PayloadBytes) bytes, all hashes verified"
}

Invoke-PrerequisiteCheck -Name 'Pinned Xenia Canary' -Action {
    Assert-FileHash -Path $XeniaPath -ExpectedSha256 $expected.XeniaSha256 -Label 'Xenia Canary executable'
    "Xenia Canary SHA-256 $($expected.XeniaSha256.Substring(0, 12))..."
}

Invoke-PrerequisiteCheck -Name 'Temurin JDK 21' -Action {
    Assert-FileHash -Path $JavaPath -ExpectedSha256 $expected.JavaSha256 -Label 'Temurin java.exe'
    $output = Invoke-NativeText -Executable $JavaPath -Arguments @('-version')
    if ($output -notmatch "(?m)openjdk version `"$([regex]::Escape($expected.JavaVersion))`"") {
        throw "Unexpected Java version output: $output"
    }
    "Temurin $($expected.JavaVersion)"
}

Invoke-PrerequisiteCheck -Name 'Ghidra and XEXLoaderWV' -Action {
    $applicationProperties = Join-Path $GhidraRoot 'Ghidra\application.properties'
    $extensionProperties = Join-Path $GhidraRoot 'Ghidra\Extensions\XEXLoaderWV\extension.properties'
    $loaderJar = Join-Path $GhidraRoot 'Ghidra\Extensions\XEXLoaderWV\lib\XEXLoaderWV.jar'
    $headless = Join-Path $GhidraRoot 'support\analyzeHeadless.bat'
    Assert-Leaf -Path $applicationProperties -Label 'Ghidra application.properties'
    Assert-Leaf -Path $extensionProperties -Label 'XEXLoaderWV extension.properties'
    Assert-Leaf -Path $headless -Label 'Ghidra headless launcher'

    $applicationText = Get-Content -LiteralPath $applicationProperties -Raw
    $extensionText = Get-Content -LiteralPath $extensionProperties -Raw
    if ($applicationText -notmatch "(?m)^application\.version=$([regex]::Escape($expected.GhidraVersion))$") {
        throw 'Ghidra version pin was not found in application.properties.'
    }
    if ($extensionText -notmatch "(?m)^version=$([regex]::Escape($expected.XexLoaderVersion))$") {
        throw 'XEXLoaderWV version pin was not found in extension.properties.'
    }
    Assert-FileHash -Path $loaderJar -ExpectedSha256 $expected.XexLoaderJarSha256 -Label 'XEXLoaderWV.jar'
    "Ghidra $($expected.GhidraVersion), XEXLoaderWV $($expected.XexLoaderVersion)"
}

Invoke-PrerequisiteCheck -Name 'RenderDoc' -Action {
    Assert-FileHash -Path $RenderDocPath -ExpectedSha256 $expected.RenderDocSha256 -Label 'renderdoccmd.exe'
    $output = Invoke-NativeText -Executable $RenderDocPath -Arguments @('--version')
    if ($output -notmatch "(?m)renderdoccmd x64 v$([regex]::Escape($expected.RenderDocVersion))\b") {
        throw "Unexpected RenderDoc version output: $output"
    }
    "RenderDoc $($expected.RenderDocVersion)"
}

$passed = @($results | Where-Object Status -eq 'PASS').Count
$failed = @($results | Where-Object Status -eq 'FAIL').Count
Write-Host ''
Write-Host "Bootstrap summary: $passed passed, $failed failed, $($results.Count) total."

if ($failed -gt 0) {
    Write-Error 'Required MCLA-R prerequisites are missing or invalid. No installation or repair was attempted.'
    exit 1
}

Write-Host 'MCLA-R prerequisite validation passed. No installation or host mutation was performed.'
