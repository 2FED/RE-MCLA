[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LivePackage,
    [Parameter(Mandatory)][string]$CrashPackage,
    [switch]$Fixture
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-Directory([string]$Path, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) { throw "$Label directory is missing: '$Path'." }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Read-Json([string]$Root, [string]$Name) {
    $path = Join-Path $Root $Name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required diagnostic artifact is missing: '$Name'." }
    return Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
}

function Hash([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return -join ($sha.ComputeHash($stream) | ForEach-Object { $_.ToString('X2') }) }
    finally { $sha.Dispose(); $stream.Dispose() }
}

function Assert-Dump([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Minidump is missing: '$Path'." }
    $item=Get-Item -LiteralPath $Path
    $header=[byte[]]::new(32)
    $stream=[IO.File]::OpenRead($Path)
    try{$read=$stream.Read($header,0,$header.Length)}finally{$stream.Dispose()}
    if ($read -ne 32 -or [Text.Encoding]::ASCII.GetString($header, 0, 4) -cne 'MDMP') { throw 'Diagnostic dump has no MDMP header.' }
    $flags = [BitConverter]::ToUInt64($header, 24)
    $required = [uint64]0x1020
    $allowed = [uint64]0x201020
    $unexpected = $flags -band ([uint64]::MaxValue -bxor $allowed)
    if (($flags -band $required) -ne $required -or $unexpected -ne 0) { throw "Diagnostic dump flags exceed the reviewed normal/thread/unloaded/AVX-context set: 0x$($flags.ToString('X'))." }
    if(-not$Fixture){
        $streamCount=[BitConverter]::ToUInt32($header,8)
        $directoryRva=[BitConverter]::ToUInt32($header,12)
        if($item.Length-lt4096-or$streamCount-lt1-or$streamCount-gt1024-or$directoryRva-lt32-or([uint64]$directoryRva+([uint64]$streamCount*12))-gt[uint64]$item.Length){throw 'Diagnostic dump stream directory is invalid.'}
    }
    return $flags
}

function Assert-SaveInventory([string]$PackageRoot, $Inventory, [uint32]$ExpectedCount, [string]$Label) {
    if ($Inventory.schema -cne 'mcla-private-save-inventory-v1' -or [bool]$Inventory.safe_to_share) { throw "$Label save inventory privacy contract is invalid." }
    $saveRoot = [IO.Path]::GetFullPath((Join-Path $PackageRoot 'save-private')).TrimEnd('\')
    $files = @($Inventory.files)
    if ($files.Count -ne $ExpectedCount) { throw "$Label save inventory count does not match the capture record." }
    if (Test-Path -LiteralPath $saveRoot -PathType Container) {
        $tree = @(Get-ChildItem -LiteralPath $saveRoot -Force -Recurse)
        if (@($tree | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) { throw "$Label private save snapshot contains a reparse point." }
        if (@($tree | Where-Object { -not $_.PSIsContainer }).Count -ne $ExpectedCount) { throw "$Label private save snapshot contains an unlisted file." }
    }
    elseif ($ExpectedCount) { throw "$Label private save snapshot directory is missing." }
    foreach ($file in $files) {
        $relative = [string]$file.path
        if (-not $relative -or [IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[\\/])\.\.([\\/]|$)') { throw "$Label save inventory contains an unsafe relative path." }
        $path = [IO.Path]::GetFullPath((Join-Path $saveRoot $relative))
        if (-not $path.StartsWith($saveRoot + '\',[StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "$Label save inventory points outside the private snapshot or to a missing file." }
        if ([uint64]$file.bytes -ne [uint64](Get-Item -LiteralPath $path).Length -or ([string]$file.sha256).ToUpperInvariant() -cne (Hash $path)) { throw "$Label save inventory integrity record is invalid." }
    }
}

function Assert-TopLevel([string]$PackageRoot, [string[]]$ArtifactNames, [string]$Label) {
    $expectedFiles = @('manifest.json') + @($ArtifactNames)
    if (@($expectedFiles | Group-Object | Where-Object Count -ne 1).Count) { throw "$Label package contains duplicate artifact names." }
    $children = @(Get-ChildItem -LiteralPath $PackageRoot -Force)
    if (@($children | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) { throw "$Label package contains a top-level reparse point." }
    $actualFiles = @($children | Where-Object { -not $_.PSIsContainer } | ForEach-Object Name | Sort-Object)
    $expectedFiles = @($expectedFiles | Sort-Object)
    if (($actualFiles -join "`n") -cne ($expectedFiles -join "`n")) { throw "$Label package top-level file topology is invalid." }
    $directories = @($children | Where-Object PSIsContainer | ForEach-Object Name)
    if (@($directories | Where-Object { $_ -cne 'save-private' }).Count) { throw "$Label package contains an unexpected top-level directory." }
}

$live = Resolve-Directory $LivePackage 'Live package'
$crash = Resolve-Directory $CrashPackage 'Crash package'
$liveManifest = Read-Json $live 'manifest.json'
$crashManifest = Read-Json $crash 'manifest.json'
$state = Read-Json $live 'state.json'
$save = Read-Json $live 'save-metadata.json'
$liveSaveInventory = Read-Json $live 'save-files-private.json'
$crashSaveInventory = Read-Json $crash 'save-files-private.json'

if ($liveManifest.schema -cne 'mcla-diagnostic-package-v1' -or $liveManifest.kind -cne 'live') { throw 'Live package identity is invalid.' }
if ($crashManifest.schema -cne 'mcla-native-crash-package-v1' -or $crashManifest.kind -cne 'native-crash') { throw 'Crash package identity is invalid.' }
foreach ($manifest in @($liveManifest, $crashManifest)) {
    if ([string]$manifest.mcla_version -notmatch '^\d+\.\d+\.\d+\.\d+$') { throw 'Diagnostic package has an invalid MCLA-R version.' }
    if ([bool]$manifest.privacy.automatic_upload -or [bool]$manifest.privacy.package_safe_to_share) { throw 'Diagnostic privacy contract drifted.' }
}
if ($state.schema -cne 'mcla-diagnostic-state-v1' -or -not $state.PSObject.Properties['process'] -or -not $state.PSObject.Properties['window'] -or -not $state.PSObject.Properties['runtime']) { throw 'Live state payload is incomplete.' }
if ($save.schema -cne 'mcla-save-snapshot-metadata-v1' -or [bool]$save.safe_to_share) { throw 'Save snapshot privacy metadata is invalid.' }
Assert-SaveInventory $live $liveSaveInventory ([uint32]$liveManifest.capture.save_files) 'Live'
Assert-SaveInventory $crash $crashSaveInventory ([uint32]$crashManifest.capture.save_files) 'Crash'
if (-not (Test-Path -LiteralPath (Join-Path $live 'log-tail.txt') -PathType Leaf)) { throw 'Sanitized live log tail is missing.' }
if (-not (Test-Path -LiteralPath (Join-Path $crash 'runtime-journal-private.log') -PathType Leaf)) { throw 'Crash runtime journal is missing.' }
if (-not (Test-Path -LiteralPath (Join-Path $crash 'README.txt') -PathType Leaf)) { throw 'Crash privacy/readme notice is missing.' }
if (-not [bool]$liveManifest.capture.minidump -or -not [bool]$crashManifest.capture.minidump) { throw 'A required minidump capture was not successful.' }

$liveDump = Join-Path $live 'process-private.dmp'
$crashDump = Join-Path $crash 'crash-private.dmp'
$liveFlags = Assert-Dump $liveDump
$crashFlags = Assert-Dump $crashDump
$liveAllowed=@('state.json','log-tail.txt','save-metadata.json','save-files-private.json','frame.bmp','process-private.dmp')
$liveArtifacts = @($liveManifest.artifacts)
foreach($artifact in $liveArtifacts){
    $name=[string]$artifact.name
    if($liveAllowed-notcontains$name){throw "Unexpected live diagnostic artifact: '$name'."}
    $path=Join-Path $live $name
    if(-not(Test-Path -LiteralPath $path -PathType Leaf)-or([string]$artifact.sha256).ToUpperInvariant()-cne(Hash $path)-or[uint64]$artifact.bytes-ne[uint64](Get-Item -LiteralPath $path).Length){throw "Live artifact record is invalid: '$name'."}
}
Assert-TopLevel $live @($liveArtifacts | ForEach-Object { [string]$_.name }) 'Live'
$crashAllowed=@('crash-private.dmp','runtime-journal-private.log','README.txt','save-files-private.json')
foreach($property in $crashManifest.artifacts.PSObject.Properties){
    if($crashAllowed-notcontains$property.Name){throw "Unexpected crash diagnostic artifact: '$($property.Name)'."}
    $path=Join-Path $crash $property.Name
    if(-not(Test-Path -LiteralPath $path -PathType Leaf)-or([string]$property.Value.sha256).ToUpperInvariant()-cne(Hash $path)-or[uint64]$property.Value.bytes-ne[uint64](Get-Item -LiteralPath $path).Length){throw "Crash artifact record is invalid: '$($property.Name)'."}
}
Assert-TopLevel $crash @($crashManifest.artifacts.PSObject.Properties | ForEach-Object Name) 'Crash'
$liveArtifact = @($liveManifest.artifacts | Where-Object name -CEQ 'process-private.dmp')
if ($liveArtifact.Count -ne 1 -or [bool]$liveArtifact[0].safe_to_share -or ([string]$liveArtifact[0].sha256).ToUpperInvariant() -cne (Hash $liveDump)) { throw 'Live minidump artifact record is invalid.' }
$stateArtifact = @($liveManifest.artifacts | Where-Object name -CEQ 'state.json')
$saveArtifact = @($liveManifest.artifacts | Where-Object name -CEQ 'save-metadata.json')
if($stateArtifact.Count-ne1-or-not[bool]$stateArtifact[0].safe_to_share-or$saveArtifact.Count-ne1-or-not[bool]$saveArtifact[0].safe_to_share){throw 'Required live metadata artifact records are invalid.'}
$logArtifact = @($liveManifest.artifacts | Where-Object name -CEQ 'log-tail.txt')
if ($logArtifact.Count -ne 1 -or [bool]$logArtifact[0].safe_to_share) { throw 'Live log-tail privacy record is invalid.' }
$liveSaveInventoryArtifact = @($liveManifest.artifacts | Where-Object name -CEQ 'save-files-private.json')
if ($liveSaveInventoryArtifact.Count -ne 1 -or [bool]$liveSaveInventoryArtifact[0].safe_to_share) { throw 'Live save inventory privacy record is invalid.' }
$frameArtifact = @($liveManifest.artifacts | Where-Object name -CEQ 'frame.bmp')
if ($frameArtifact.Count -gt 1 -or ($frameArtifact.Count -eq 1 -and [bool]$frameArtifact[0].safe_to_share)) { throw 'Live frame privacy record is invalid.' }
if ([bool]$liveManifest.capture.frame -ne ($frameArtifact.Count -eq 1)) { throw 'Live frame capture record does not match its artifact.' }
$crashArtifact = $crashManifest.artifacts.'crash-private.dmp'
if (-not $crashArtifact -or [bool]$crashArtifact.safe_to_share -or ([string]$crashArtifact.sha256).ToUpperInvariant() -cne (Hash $crashDump)) { throw 'Crash minidump artifact record is invalid.' }
$crashJournalArtifact=$crashManifest.artifacts.'runtime-journal-private.log'
if(-not$crashJournalArtifact-or[bool]$crashJournalArtifact.safe_to_share){throw 'Crash journal artifact record is invalid.'}
foreach($name in @('README.txt','save-files-private.json')){if(-not$crashManifest.artifacts.$name-or[bool]$crashManifest.artifacts.$name.safe_to_share){throw "Crash private artifact record is invalid: '$name'."}}
if ([string]$crashManifest.exception_code -notmatch '^0x[0-9A-F]{8}$' -or [string]$crashManifest.exception_address -notmatch '^0x[0-9A-F]{16}$') { throw 'Crash exception identity is malformed.' }

[pscustomobject][ordered]@{
    Passed = $true
    Decision = 'live-snapshot-and-native-crash-package-pass'
    LivePackage = $live
    CrashPackage = $crash
    LiveDumpBytes = (Get-Item -LiteralPath $liveDump).Length
    CrashDumpBytes = (Get-Item -LiteralPath $crashDump).Length
    LiveDumpFlags = "0x$($liveFlags.ToString('X'))"
    CrashDumpFlags = "0x$($crashFlags.ToString('X'))"
    Fixture = [bool]$Fixture
}
