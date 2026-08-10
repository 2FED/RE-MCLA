[CmdletBinding()]
param(
    [switch]$ExportPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-VersionOutput {
    param(
        [Parameter(Mandatory)]
        [string]$Executable
    )

    $output = & $Executable --version 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to query version from '$Executable' (exit code $LASTEXITCODE)."
    }

    return $output.Trim()
}

function Get-ParsedVersion {
    param(
        [Parameter(Mandatory)]
        [string]$Text,

        [Parameter(Mandatory)]
        [string]$ToolName
    )

    if ($Text -notmatch '(?m)(?<version>\d+\.\d+(?:\.\d+)?)') {
        throw "Could not parse the $ToolName version from: $Text"
    }

    return [version]$Matches.version
}

function Assert-MinimumVersion {
    param(
        [Parameter(Mandatory)]
        [version]$Actual,

        [Parameter(Mandatory)]
        [version]$Minimum,

        [Parameter(Mandatory)]
        [string]$ToolName
    )

    if ($Actual -lt $Minimum) {
        throw "$ToolName $Actual is unsupported; version $Minimum or newer is required."
    }
}

function Resolve-FirstExistingPath {
    param(
        [Parameter(Mandatory)]
        [string[]]$Candidates,

        [Parameter(Mandatory)]
        [string]$ToolName
    )

    foreach ($candidate in $Candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "Could not locate $ToolName. Checked: $($Candidates -join ', ')"
}

$programFilesX86 = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)
$programFiles = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFiles)
$vswhere = Resolve-FirstExistingPath -ToolName 'vswhere.exe' -Candidates @(
    (Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe')
)

$visualStudioRoot = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1).Trim()
if ($LASTEXITCODE -ne 0 -or -not $visualStudioRoot) {
    throw 'vswhere.exe did not find Visual Studio with the C++ x64 build tools component.'
}

$cmake = Resolve-FirstExistingPath -ToolName 'CMake' -Candidates @(
    (Join-Path $visualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'),
    (Join-Path $programFiles 'CMake\bin\cmake.exe')
)
$ninja = Resolve-FirstExistingPath -ToolName 'Ninja' -Candidates @(
    (Join-Path $visualStudioRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'),
    (Join-Path $programFiles 'Ninja\ninja.exe')
)
$clangCl = Resolve-FirstExistingPath -ToolName 'clang-cl' -Candidates @(
    (Join-Path $programFiles 'LLVM\bin\clang-cl.exe')
)

$cmakeVersion = Get-ParsedVersion -ToolName 'CMake' -Text (Get-VersionOutput -Executable $cmake)
$ninjaVersion = Get-ParsedVersion -ToolName 'Ninja' -Text (Get-VersionOutput -Executable $ninja)
$clangVersion = Get-ParsedVersion -ToolName 'clang-cl' -Text (Get-VersionOutput -Executable $clangCl)

Assert-MinimumVersion -ToolName 'CMake' -Actual $cmakeVersion -Minimum ([version]'3.25')
Assert-MinimumVersion -ToolName 'Ninja' -Actual $ninjaVersion -Minimum ([version]'1.10')
Assert-MinimumVersion -ToolName 'clang-cl' -Actual $clangVersion -Minimum ([version]'20.0')

$toolDirectories = @($cmake, $ninja, $clangCl) |
    ForEach-Object { Split-Path -Parent $_ } |
    Select-Object -Unique

if ($ExportPath) {
    $currentPathEntries = @($env:Path -split ';' | Where-Object { $_ })
    $newPathEntries = @($toolDirectories | Where-Object { $_ -notin $currentPathEntries })
    if ($newPathEntries.Count -gt 0) {
        $env:Path = (@($newPathEntries) + $currentPathEntries) -join ';'
    }
}

[pscustomobject]@{
    VisualStudioRoot = $visualStudioRoot
    CMakePath       = $cmake
    CMakeVersion    = $cmakeVersion.ToString()
    NinjaPath       = $ninja
    NinjaVersion    = $ninjaVersion.ToString()
    ClangClPath     = $clangCl
    ClangClVersion  = $clangVersion.ToString()
    PathExported    = [bool]$ExportPath
}
