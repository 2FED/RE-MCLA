[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [Parameter(Mandatory=$true)][string]$Destination,
    [Parameter(Mandatory=$true)][string]$CrtDirectory,
    # Optional plain-text guide copied to the root as TESTER-GUIDE.txt (M9-005).
    [string]$TesterGuide
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'Portable-CppRuntime.ps1')

# Private development candidate, NOT redistribution authorization. The fixed
# file inventory excludes game data, user folders, PDBs, TOML, logs and settings.
# Trusted local/cooperative inputs only, not a hostile-filesystem security boundary.
function Assert-LocalPath([string]$Path) {
    if ($Path -notmatch '^[A-Za-z]:[\\/]') { throw 'Staging requires absolute local-drive paths.' }
    $full = [IO.Path]::GetFullPath($Path)
    $drive = [IO.DriveInfo]::new($full.Substring(0, 3))
    if ($drive.DriveType -ne [IO.DriveType]::Fixed) { throw 'Staging requires a local fixed drive.' }
    return $full
}
function Get-CheckedDirectory([string]$Path) {
    $full = Assert-LocalPath $Path
    $item = Get-Item -LiteralPath $full -Force
    if (-not $item.PSIsContainer) { throw 'Staging input/parent must be an existing directory.' }
    for ($cursor = $item; $null -ne $cursor; $cursor = $cursor.Parent) {
        if (($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Redirected staging paths are unsupported.'
        }
    }
    return $item.FullName
}
function Assert-UnnamedStreamOnly([string]$Path) {
    $streams = @(Get-Item -LiteralPath $Path -Stream '*' -ErrorAction Stop)
    if ($streams.Count -ne 1 -or $streams[0].Stream -cne ':$DATA') {
        throw 'Staged files must have exactly one unnamed data stream; alternate streams are refused.'
    }
}
function Assert-RegularInput([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force
    if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 -or
        $item.Length -eq 0 -or $item.Length -gt 2147483648) { throw 'Invalid staged file type or size.' }
    Assert-UnnamedStreamOnly $Path
}
function Get-StreamHash([IO.Stream]$Stream) {
    $Stream.Position = 0
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($Stream))).Replace('-', '') }
    finally { $sha.Dispose(); $Stream.Position = 0 }
}
function Write-NewBytes([string]$Path, [byte[]]$Bytes) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $stream.Write($Bytes, 0, $Bytes.Length); $stream.Flush($true) }
    finally { $stream.Dispose() }
}
function Copy-PinnedInput($InputRecord, [string]$To) {
    Assert-UnnamedStreamOnly $InputRecord.Source
    $before = Get-StreamHash $InputRecord.Stream
    if ($before -cne $InputRecord.Sha256 -or $InputRecord.Stream.Length -ne $InputRecord.Bytes) {
        throw 'Pinned input changed before copy; incomplete candidate preserved.'
    }
    $output = [IO.File]::Open($To, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $InputRecord.Stream.Position = 0
        $InputRecord.Stream.CopyTo($output)
        $output.Flush($true)
    } finally { $output.Dispose() }
    $after = Get-StreamHash $InputRecord.Stream
    $copied = (Get-FileHash -LiteralPath $To -Algorithm SHA256).Hash
    Assert-UnnamedStreamOnly $InputRecord.Source
    Assert-RegularInput $To
    if ($before -cne $after -or $before -cne $copied -or
        (Get-Item -LiteralPath $To -Force).Length -ne $InputRecord.Bytes) {
        throw 'Input changed or copy mismatched; incomplete candidate preserved.'
    }
}
function Assert-ExactInventory([string]$Root, [string[]]$Expected) {
    $null = Get-CheckedDirectory $Root
    $actual = [Collections.Generic.List[string]]::new()
    foreach ($item in @(Get-ChildItem -LiteralPath $Root -Force)) {
        if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Redirected candidate entry.' }
        if ($item.PSIsContainer) {
            if ($item.Name -cne 'build') { throw 'Unexpected candidate directory.' }
            foreach ($child in @(Get-ChildItem -LiteralPath $item.FullName -Force)) {
                if ($child.PSIsContainer -or ($child.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                    throw 'Nested or redirected build entry.'
                }
                Assert-UnnamedStreamOnly $child.FullName
                $actual.Add('build/' + $child.Name)
            }
        } else {
            Assert-UnnamedStreamOnly $item.FullName
            $actual.Add($item.Name)
        }
    }
    [string[]]$observed = $actual.ToArray()
    [string[]]$wanted = @($Expected)
    [Array]::Sort($observed, [StringComparer]::Ordinal)
    [Array]::Sort($wanted, [StringComparer]::Ordinal)
    if (($observed -join "`n") -cne ($wanted -join "`n")) { throw 'Candidate final inventory is not the exact allowlist.' }
}

$source = Get-CheckedDirectory $BuildDirectory
$crt = Get-CheckedDirectory $CrtDirectory
$target = Assert-LocalPath $Destination
if (Test-Path -LiteralPath $target) { throw 'Destination must be NEW; no overwrite or merge.' }
$parent = Get-CheckedDirectory (Split-Path -Parent $target)
foreach ($inputRoot in @($source, $crt)) {
    if ($target.StartsWith($inputRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Destination must not be inside build or CRT inputs.'
    }
}
$policy = Get-PortableCppRuntimePolicy
foreach ($entry in $policy.files) {
    Assert-PortableCppRuntimeFile -Path (Join-Path $crt $entry.name) -Sha256 $entry.sha256 -Version $policy.version -CheckSignature
}
$runtime = @('mcla.exe','mcla_crash_handler.exe','mcla_diagnostic_image_helper.exe','rexruntime.dll','rexgpu-xenos.dll','TracyClient.dll','mcla-test-build.ticket')
$paths = @{}
foreach ($name in $runtime) { $paths[$name] = Join-Path $source $name }
foreach ($entry in $policy.files) { $paths[$entry.name] = Join-Path $crt $entry.name }
$paths['MCLA-Launcher.exe'] = Join-Path $source 'MCLA-Launcher.exe'
if ($TesterGuide) { $paths['TESTER-GUIDE.txt'] = Assert-LocalPath $TesterGuide }
$pinned = @{}
try {
    # Freeze every source before creating output. Competing rebuilds must wait
    # or fail; no file is silently taken from a different in-progress build.
    foreach ($name in $paths.Keys) {
        Assert-RegularInput $paths[$name]
        $stream = [IO.File]::Open($paths[$name], [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        $pinned[$name] = [pscustomobject]@{Source=$paths[$name]; Stream=$stream; Bytes=$stream.Length; Sha256=''}
        $pinned[$name].Sha256 = Get-StreamHash $stream
    }
    foreach ($entry in $policy.files) {
        if ($pinned[$entry.name].Sha256 -cne $entry.sha256) { throw 'Pinned CRT changed before staging.' }
    }
    # Cheap binding gate here; native RSA verification and real host admission
    # are mandatory after staging. Re-staging copies the existing signed date,
    # never resets its fourteen-day lifetime or generates a new signing key.
    if ($pinned['mcla-test-build.ticket'].Bytes -gt 65536) { throw 'Test-build ticket is oversized.' }
    $ticketBytes = New-Object byte[] ([int]$pinned['mcla-test-build.ticket'].Bytes)
    $ticketStream=$pinned['mcla-test-build.ticket'].Stream
    if ($ticketStream.Read($ticketBytes,0,$ticketBytes.Length) -ne $ticketBytes.Length) { throw 'Ticket truncated.' }
    $ticketStream.Position=0
    $ticketText=[Text.Encoding]::ASCII.GetString($ticketBytes)
    if ($ticketText -cnotmatch '\AMCLA-TEST-BUILD-V[12]\n([0-9A-F]{64}) ([1-9][0-9]{9})\n--SIGNATURE--\n[0-9A-F]{512}\n\z' -or
        $Matches[1] -cne $pinned['mcla.exe'].Sha256) { throw 'Test-build ticket does not match the staged host.' }
    New-Item -ItemType Directory -Path $target -ErrorAction Stop | Out-Null
    $bin = Join-Path $target 'build'
    New-Item -ItemType Directory -Path $bin -ErrorAction Stop | Out-Null
    [string[]]$names = @($runtime) + @($policy.files | ForEach-Object { [string]$_.name })
    [Array]::Sort($names, [StringComparer]::Ordinal)
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add('MCLA-DEV-BUILD-V1')
    $inventory = [Collections.Generic.List[object]]::new()
    foreach ($name in $names) {
        $record = $pinned[$name]
        Copy-PinnedInput $record (Join-Path $bin $name)
        $lines.Add($record.Sha256 + ' ' + $record.Bytes + ' ' + $name)
        $inventory.Add([ordered]@{path=('build/' + $name); bytes=$record.Bytes; sha256=$record.Sha256})
    }
    Copy-PinnedInput $pinned['MCLA-Launcher.exe'] (Join-Path $target 'MCLA-Launcher.exe')
    $inventory.Add([ordered]@{path='MCLA-Launcher.exe'; bytes=$pinned['MCLA-Launcher.exe'].Bytes; sha256=$pinned['MCLA-Launcher.exe'].Sha256})
    if ($pinned.ContainsKey('TESTER-GUIDE.txt')) {
        Copy-PinnedInput $pinned['TESTER-GUIDE.txt'] (Join-Path $target 'TESTER-GUIDE.txt')
        $inventory.Add([ordered]@{path='TESTER-GUIDE.txt'; bytes=$pinned['TESTER-GUIDE.txt'].Bytes; sha256=$pinned['TESTER-GUIDE.txt'].Sha256})
    }
    foreach ($entry in $policy.files) {
        $record = $pinned[$entry.name]
        Copy-PinnedInput $record (Join-Path $target $entry.name)
        $inventory.Add([ordered]@{path=$entry.name; bytes=$record.Bytes; sha256=$record.Sha256})
    }
    $manifestPath = Join-Path $bin 'mcla-build.manifest'
    $utf8 = [Text.UTF8Encoding]::new($false)
    Write-NewBytes $manifestPath ($utf8.GetBytes(($lines -join "`n") + "`n"))
    $manifestHash = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
    $inventory.Add([ordered]@{path='build/mcla-build.manifest'; bytes=(Get-Item -LiteralPath $manifestPath).Length; sha256=$manifestHash})
    [string[]]$relative = @($inventory | ForEach-Object { [string]$_.path })
    [Array]::Sort($relative, [StringComparer]::Ordinal)
    $orderedFiles = @($relative | ForEach-Object { $wanted = $_; $inventory | Where-Object { $_.path -ceq $wanted } })
    Assert-ExactInventory $target $relative
    foreach ($file in $orderedFiles) {
        $path = Join-Path $target $file.path.Replace('/', '\')
        if ((Get-Item -LiteralPath $path -Force).Length -ne $file.bytes -or
            (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $file.sha256) {
            throw 'Final candidate content differs from checked copy; incomplete candidate preserved.'
        }
    }
    $receipt = [ordered]@{
        schema='mcla-internal-launcher-staging-v1'
        external_delivery_approved=$false
        game_assets_included=$false
        personal_data_included=$false
        crt_policy=$policy.policy
        crt_version=$policy.version
        build_manifest_sha256=$manifestHash
        launcher_sha256=$pinned['MCLA-Launcher.exe'].Sha256
        test_build_ticket_sha256=$pinned['mcla-test-build.ticket'].Sha256
        files=$orderedFiles
    }
    $pending = Join-Path $target 'candidate-receipt.pending.json'
    Write-NewBytes $pending ($utf8.GetBytes(($receipt | ConvertTo-Json -Depth 6) + "`n"))
    Assert-ExactInventory $target (@($relative) + @('candidate-receipt.pending.json'))
    # Completion receipt is the final publication operation. Interrupted output
    # is preserved without a final receipt; no auto-retry, delete or merge.
    [IO.File]::Move($pending, (Join-Path $target 'candidate-receipt.json'))
    Write-Output $target
} finally {
    foreach ($record in $pinned.Values) { $record.Stream.Dispose() }
}
