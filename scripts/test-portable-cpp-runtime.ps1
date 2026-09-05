[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
. (Join-Path $PSScriptRoot 'Portable-CppRuntime.ps1')
$privateRoot = [IO.Path]::GetFullPath((Join-Path $repo 'private')).TrimEnd('\')
$fixture = [IO.Path]::GetFullPath((Join-Path $privateRoot ('evidence\M7-016\cpp-runtime-test-' + [guid]::NewGuid().ToString('N'))))
if (-not $fixture.StartsWith($privateRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'C++ runtime fixture escaped private/.' }
$negative = 0

function Test-Hash([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') }
        finally { $sha.Dispose() }
    } finally { $stream.Dispose() }
}
function Must-Fail([scriptblock]$Action, [string]$Label) {
    $failed = $false
    try { & $Action | Out-Null } catch { $failed = $true }
    if (-not $failed) { throw "Invalid C++ runtime case was accepted: $Label" }
    $script:negative++
}

try {
    $toolchain = & (Join-Path $PSScriptRoot 'Resolve-Toolchain.ps1')
    $source = Resolve-PortableCppRuntimeSource -VisualStudioRoot $toolchain.VisualStudioRoot
    if ($source.Files.Count -ne 7) { throw 'C++ runtime closure must have exactly seven placements.' }
    [IO.Directory]::CreateDirectory((Join-Path $fixture 'bin')) | Out-Null
    foreach ($entry in $source.Files) {
        Copy-Item -LiteralPath (Join-Path $source.SourceDirectory $entry.Name) -Destination (Join-Path $fixture $entry.RelativePath.Replace('/','\'))
    }
    Assert-PortableCppRuntimeBundle -Root $fixture -Policy $source.Policy
    $entry = $source.Policy.files[0]
    $dll = Join-Path $fixture 'bin\msvcp140.dll'
    $original = [IO.File]::ReadAllBytes($dll)

    Must-Fail { Assert-PortableCppRuntimeFile -Path $dll -Sha256 ('0' * 64) -Version $source.Policy.version } 'incorrect pinned hash'
    Must-Fail { Assert-PortableCppRuntimeFile -Path $dll -Sha256 $entry.sha256 -Version '14.44.1.0' } 'incorrect version'
    # Rehash only inside this low-level fixture so PE/signature validation is
    # exercised independently; production always supplies tracked exact pins.
    $bytes = [byte[]]$original.Clone()
    $peOffset = [BitConverter]::ToUInt32($bytes, 0x3C)
    $bytes[$peOffset + 4] = 0x4C; $bytes[$peOffset + 5] = 0x01
    [IO.File]::WriteAllBytes($dll, $bytes)
    Must-Fail { Assert-PortableCppRuntimeFile -Path $dll -Sha256 (Test-Hash $dll) -Version $source.Policy.version } 'x86 machine'
    $bytes = [byte[]]$original.Clone(); $bytes[$peOffset + 24] = 0x0B; $bytes[$peOffset + 25] = 0x01
    [IO.File]::WriteAllBytes($dll, $bytes)
    Must-Fail { Assert-PortableCppRuntimeFile -Path $dll -Sha256 (Test-Hash $dll) -Version $source.Policy.version } 'PE32 optional header'
    $bytes = [byte[]]$original.Clone(); $bytes[0x3C] = 0xFF; $bytes[0x3D] = 0xFF; $bytes[0x3E] = 0xFF; $bytes[0x3F] = 0x7F
    [IO.File]::WriteAllBytes($dll, $bytes)
    Must-Fail { Assert-PortableCppRuntimeFile -Path $dll -Sha256 (Test-Hash $dll) -Version $source.Policy.version } 'out-of-bounds header'
    $bytes = [byte[]]$original.Clone(); $bytes[0x40] = $bytes[0x40] -bxor 1
    [IO.File]::WriteAllBytes($dll, $bytes)
    Must-Fail { Assert-PortableCppRuntimeFile -Path $dll -Sha256 (Test-Hash $dll) -Version $source.Policy.version -CheckSignature } 'altered Microsoft signature'
    Must-Fail { Assert-PortableCppRuntimeBundle -Root $fixture -Policy $source.Policy } 'altered DLL with original policy'
    [IO.File]::WriteAllBytes($dll, $original)
    Assert-PortableCppRuntimeBundle -Root $fixture -Policy $source.Policy
    Remove-Item -LiteralPath (Join-Path $fixture 'vcruntime140_1.dll')
    Must-Fail { Assert-PortableCppRuntimeBundle -Root $fixture -Policy $source.Policy } 'missing launcher transitive runtime'
    [pscustomobject]@{Passed=$true;Positive=3;FailClosedNegatives=$negative;RuntimeVersion=$source.Policy.version;Placements=$source.Files.Count;MicrosoftSignaturesVerified=$true}
} finally {
    if (Test-Path -LiteralPath $fixture) {
        $resolved = (Resolve-Path -LiteralPath $fixture).Path
        if ($resolved -cne $fixture -or -not $resolved.StartsWith($privateRoot + '\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Refusing to clean an unexpected C++ runtime fixture path.' }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
