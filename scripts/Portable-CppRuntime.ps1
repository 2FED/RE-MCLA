# Fixed, app-local Microsoft C++ runtime closure. No installation, prefix,
# registry, environment or loader override changes belong in this helper.
function Get-PortableCppRuntimePolicy {
    $path = Join-Path (Split-Path -Parent $PSScriptRoot) 'config\portable-cpp-runtime.json'
    if ((Get-Item -LiteralPath $path -ErrorAction Stop).Length -gt 16384) { throw 'C++ runtime policy is too large.' }
    $value = [IO.File]::ReadAllText($path) | ConvertFrom-Json
    $properties = @($value.PSObject.Properties.Name | Sort-Object)
    if (($properties -join '|') -cne 'files|policy|redist_relative_path|schema|version' -or
        $value.schema -cne 'mcla-portable-cpp-runtime-v1' -or
        $value.policy -cne 'msvc-14.44-app-local-v1' -or $value.version -cne '14.44.35211.0' -or
        $value.redist_relative_path -cne 'VC/Redist/MSVC/14.44.35112/x64/Microsoft.VC143.CRT' -or
        $value.files -isnot [array] -or $value.files.Count -ne 4) { throw 'Unsupported C++ runtime policy.' }
    $names = @('msvcp140.dll','msvcp140_atomic_wait.dll','vcruntime140.dll','vcruntime140_1.dll')
    for ($i = 0; $i -lt $names.Count; $i++) {
        $entry = $value.files[$i]
        if ((@($entry.PSObject.Properties.Name | Sort-Object) -join '|') -cne 'name|sha256' -or
            $entry.name -cne $names[$i] -or $entry.sha256 -isnot [string] -or
            $entry.sha256 -cnotmatch '^[0-9A-F]{64}$') { throw 'C++ runtime file policy is invalid.' }
    }
    return $value
}

function Get-PortableCppRuntimeFiles($Policy) {
    # Launcher imports MSVCP140, which itself imports VCRUNTIME140_1. The host
    # and rexruntime also import MSVCP140_ATOMIC_WAIT. Each EXE needs its own
    # directory-local closure; root DLLs are not a substitute for bin DLLs.
    foreach ($entry in $Policy.files) {
        if ($entry.name -cne 'msvcp140_atomic_wait.dll') {
            [pscustomobject]@{ RelativePath=[string]$entry.name; Name=[string]$entry.name; Sha256=[string]$entry.sha256 }
        }
        [pscustomobject]@{ RelativePath=('bin/' + $entry.name); Name=[string]$entry.name; Sha256=[string]$entry.sha256 }
    }
}

function Assert-PortableCppRuntimeFile([string]$Path, [string]$Sha256, [string]$Version, [switch]$CheckSignature) {
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if ($item.PSIsContainer -or $item.Length -lt 256 -or $item.Length -gt 16777216) { throw 'C++ runtime file size/type is invalid.' }
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'C++ runtime source/file is a reparse point.' }
    for ($parent = $item.Directory; $null -ne $parent; $parent = $parent.Parent) {
        if ($parent.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'C++ runtime source/file has a reparse ancestor.' }
    }
    $stream = [IO.File]::OpenRead($item.FullName)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $actual = ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') }
        finally { $sha.Dispose() }
        if ($actual -cne $Sha256) { throw "Pinned C++ runtime SHA-256 drifted: '$($item.Name)'." }
        $reader = [IO.BinaryReader]::new($stream, [Text.Encoding]::UTF8, $true)
        try {
            $stream.Position = 0
            if ($reader.ReadUInt16() -ne 0x5A4D) { throw 'C++ runtime is not a DOS/PE image.' }
            $stream.Position = 0x3C
            $peOffset = $reader.ReadUInt32()
            if ($peOffset -lt 0x40 -or $peOffset -gt ($stream.Length - 26)) { throw 'C++ runtime PE header is out of bounds.' }
            $stream.Position = $peOffset
            if ($reader.ReadUInt32() -ne 0x00004550 -or $reader.ReadUInt16() -ne 0x8664) { throw 'C++ runtime must be an AMD64 PE.' }
            $stream.Position = $peOffset + 24
            if ($reader.ReadUInt16() -ne 0x20B) { throw 'C++ runtime must be PE32+.' }
        } finally { $reader.Dispose() }
    } finally { $stream.Dispose() }
    if ([Diagnostics.FileVersionInfo]::GetVersionInfo($item.FullName).FileVersion -cne $Version) { throw 'C++ runtime file version drifted.' }
    if ($CheckSignature) {
        $signature = Get-AuthenticodeSignature -LiteralPath $item.FullName
        if ($signature.Status -ne [Management.Automation.SignatureStatus]::Valid -or
            $null -eq $signature.SignerCertificate -or
            $signature.SignerCertificate.Subject -notmatch '(?:^|,\s*)O=Microsoft Corporation(?:,|$)') { throw 'C++ runtime source must have a valid Microsoft signature.' }
    }
}

function Resolve-PortableCppRuntimeSource([Parameter(Mandatory=$true)][string]$VisualStudioRoot) {
    $policy = Get-PortableCppRuntimePolicy
    $installation = (Resolve-Path -LiteralPath $VisualStudioRoot -ErrorAction Stop).Path
    # VisualStudioRoot comes from Resolve-Toolchain, never a system DLL search.
    $directory = Join-Path $installation $policy.redist_relative_path.Replace('/','\')
    foreach ($entry in $policy.files) {
        Assert-PortableCppRuntimeFile -Path (Join-Path $directory $entry.name) -Sha256 $entry.sha256 -Version $policy.version -CheckSignature
    }
    [pscustomobject]@{ Policy=$policy; SourceDirectory=$directory; Files=@(Get-PortableCppRuntimeFiles $policy) }
}

function Assert-PortableCppRuntimeBundle([string]$Root, $Policy) {
    foreach ($entry in @(Get-PortableCppRuntimeFiles $Policy)) {
        Assert-PortableCppRuntimeFile -Path (Join-Path $Root $entry.RelativePath.Replace('/','\')) -Sha256 $entry.Sha256 -Version $Policy.version
    }
}
