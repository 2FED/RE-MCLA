[CmdletBinding()]
param(
    [string]$GeneratedRoot,
    [string]$GeneratedManifestPath,
    [string]$ImportCoveragePath,
    [string]$SdkRoot,
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$evidenceRoot = Join-Path $repoRoot 'docs/evidence'
if (-not $GeneratedRoot) { $GeneratedRoot = Join-Path $repoRoot 'private/evidence/M2-016/pre-release-tag/generated' }
if (-not $GeneratedManifestPath) { $GeneratedManifestPath = Join-Path $repoRoot 'private/evidence/M2-012/10-final-clean-b/generated-manifest.json' }
if (-not $ImportCoveragePath) { $ImportCoveragePath = Join-Path $evidenceRoot 'M2-013-import-coverage.md' }
if (-not $SdkRoot) { $SdkRoot = Join-Path $repoRoot 'third_party/rexglue-sdk' }
if (-not $OutputPath) { $OutputPath = Join-Path $evidenceRoot 'M2-014-startup-import-set.md' }

function Resolve-RegularInput {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Required input was not found: '$Path'." }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if ((Get-Item -LiteralPath $resolved -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Input must not be a reparse point: '$resolved'."
    }
    return $resolved
}

$resolvedManifest = Resolve-RegularInput $GeneratedManifestPath
$resolvedCoverage = Resolve-RegularInput $ImportCoveragePath
foreach ($root in @($GeneratedRoot, $SdkRoot)) {
    if (-not (Test-Path -LiteralPath $root -PathType Container)) { throw "Required directory was not found: '$root'." }
    if ((Get-Item -LiteralPath $root -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Required directory must not be a reparse point: '$root'."
    }
}
$resolvedGeneratedRoot = (Resolve-Path -LiteralPath $GeneratedRoot).Path
$resolvedSdkRoot = (Resolve-Path -LiteralPath $SdkRoot).Path
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
$safeOutputRoot = [System.IO.Path]::GetFullPath($evidenceRoot).TrimEnd('\')
if (-not $resolvedOutput.StartsWith("$safeOutputRoot\", [System.StringComparison]::OrdinalIgnoreCase) -or
    [System.IO.Path]::GetExtension($resolvedOutput) -ne '.md') {
    throw "Output must be a Markdown file below '$safeOutputRoot'."
}

$manifestHash = (Get-FileHash -LiteralPath $resolvedManifest -Algorithm SHA256).Hash
$coverageHash = (Get-FileHash -LiteralPath $resolvedCoverage -Algorithm SHA256).Hash
if ($manifestHash -ne 'F8A97618215848CA3B2279725DE77D8AF556E6B2E02AF1CA66D51A812E60F933' -or
    $coverageHash -ne '7E761FAF92A6559B83C166ED1493E601BF588D3F97812F1D3536EFCD535BC96D') {
    throw 'Startup audit inputs are not the accepted M2-012/M2-013 evidence.'
}
$manifest = Get-Content -LiteralPath $resolvedManifest -Raw | ConvertFrom-Json
foreach ($entry in @($manifest.files)) {
    $path = Join-Path $resolvedGeneratedRoot $entry.path
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
        (Get-Item -LiteralPath $path).Length -ne $entry.bytes -or
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw "Generated startup input changed: '$($entry.path)'."
    }
}

$importLookup = @{}
$matrixPattern = '^\| `(xam\.xex|xboxkrnl\.exe)` \| ([FV]) \| `(\d+) / 0x([0-9A-F]{3})` \| `([A-Za-z0-9_]+)` \| `0x([0-9A-F]{8})` \| (?:`0x([0-9A-F]{8})`|-) \| (implemented|unimplemented) \| (.+) \|$'
foreach ($line in Get-Content -LiteralPath $resolvedCoverage) {
    if ($line -match $matrixPattern) {
        $importLookup[$Matches[5]] = [pscustomobject]@{
            Library=$Matches[1]; Type=$Matches[2]; Ordinal=[int]$Matches[3]; OrdinalHex=$Matches[4]
            Name=$Matches[5]; Iat=$Matches[6]; Thunk=$Matches[7]; XeniaStatus=$Matches[8]; StaticClass=$Matches[9]
        }
    }
}
if ($importLookup.Count -ne 257) { throw "Expected 257 imports in M2-013 matrix, found $($importLookup.Count)." }

$graph = @{}
$importsByFunction = @{}
$callOrder = @{}
$current = ''
foreach ($file in Get-ChildItem -LiteralPath $resolvedGeneratedRoot -Filter 'mcla_recomp.*.cpp' -File) {
    foreach ($line in [System.IO.File]::ReadLines($file.FullName)) {
        if ($line -match '^DEFINE_REX_FUNC\(([^)]+)\)') {
            $current = $Matches[1]
            if ($graph.ContainsKey($current)) { throw "Duplicate generated function body: '$current'." }
            $graph[$current] = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
            $importsByFunction[$current] = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
            $callOrder[$current] = [System.Collections.Generic.List[string]]::new()
        } elseif ($current -and $line -match '^\s*(sub_[0-9A-F]+)\(ctx, base\);$') {
            $graph[$current].Add($Matches[1]) | Out-Null
            $callOrder[$current].Add($Matches[1])
        } elseif ($current -and $line -match '^\s*__imp__([A-Za-z0-9_]+)\(ctx, base\);$') {
            $importsByFunction[$current].Add($Matches[1]) | Out-Null
            $callOrder[$current].Add('__imp__' + $Matches[1])
        }
    }
}

$expectedXstartOrder = @(
    'sub_82132A48','sub_82132898','sub_821320D0','__imp__XamLoaderTerminateTitle',
    'sub_823DB480','sub_82132820','sub_82132740','sub_82131C08','sub_821305E8',
    'sub_823DB0C0','__imp__DbgPrint','__imp__XamLoaderTerminateTitle'
)
if (-not $callOrder.ContainsKey('xstart') -or
    @(Compare-Object @($callOrder['xstart']) $expectedXstartOrder -SyncWindow 0).Count -ne 0) {
    throw 'xstart call order or title-main boundary changed.'
}

$roots = @('sub_82132A48','sub_82132898','sub_821320D0','sub_823DB480','sub_82132820','sub_82132740','sub_82131C08')
$queue = [System.Collections.Generic.Queue[object]]::new()
foreach ($root in $roots) { $queue.Enqueue([pscustomobject]@{ Name=$root; Depth=1 }) }
$depths = @{}
$importOwners = @{}
while ($queue.Count) {
    $item = $queue.Dequeue()
    if ($depths.ContainsKey($item.Name)) { continue }
    if (-not $graph.ContainsKey($item.Name)) { throw "Startup graph references missing function '$($item.Name)'." }
    $depths[$item.Name] = $item.Depth
    foreach ($name in $importsByFunction[$item.Name]) {
        if (-not $importOwners.ContainsKey($name)) { $importOwners[$name] = [System.Collections.Generic.List[string]]::new() }
        $importOwners[$name].Add("$($item.Name)@$($item.Depth)")
    }
    foreach ($callee in $graph[$item.Name]) {
        $queue.Enqueue([pscustomobject]@{ Name=$callee; Depth=$item.Depth + 1 })
    }
}
$importOwners['XamLoaderTerminateTitle'] = [System.Collections.Generic.List[string]]::new()
$importOwners['XamLoaderTerminateTitle'].Add('xstart@0')

$expectedStartupImports = @(
    'ExGetXConfigSetting','HalReturnToFirmware','KeBugCheckEx','KeGetCurrentProcessType','KeTlsAlloc','KeTlsFree',
    'KeTlsGetValue','KeTlsSetValue','NtAllocateVirtualMemory','NtClose','NtCreateEvent','NtFreeVirtualMemory',
    'NtQueryVirtualMemory','NtWaitForSingleObjectEx','RtlCompareMemoryUlong','RtlEnterCriticalSection',
    'RtlImageXexHeaderField','RtlInitializeCriticalSection','RtlLeaveCriticalSection','RtlNtStatusToDosError',
    'RtlRaiseException','XamLoaderTerminateTitle','XamShowMessageBoxUIEx','XexCheckExecutablePrivilege','XGetAVPack','XGetLanguage'
)
if ($depths.Count -ne 59 -or ($depths.Values | Measure-Object -Maximum).Maximum -ne 6 -or
    @(Compare-Object ($importOwners.Keys | Sort-Object) $expectedStartupImports).Count -ne 0) {
    throw 'Bounded pre-main function/import closure changed.'
}

$sdkCppFiles = @(Get-ChildItem -LiteralPath (Join-Path $resolvedSdkRoot 'src') -Recurse -Filter '*.cpp' -File)
$sdkText = ($sdkCppFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join "`n"
$registered = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($match in [regex]::Matches($sdkText, 'REX_EXPORT\(\s*__imp__([A-Za-z0-9_]+)')) { $registered.Add($match.Groups[1].Value) | Out-Null }
$macroStubs = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($match in [regex]::Matches($sdkText, 'REX_EXPORT_STUB(?:_RETURN)?\(\s*__imp__([A-Za-z0-9_]+)')) { $macroStubs.Add($match.Groups[1].Value) | Out-Null }
foreach ($name in $expectedStartupImports) {
    if (-not $importLookup.ContainsKey($name) -or -not $registered.Contains($name) -or $macroStubs.Contains($name)) {
        throw "Startup import lacks a concrete SDK registration: '$name'."
    }
}
$xamUiPath = Join-Path $resolvedSdkRoot 'src/kernel/xam/xam_ui.cpp'
$xamUiText = Get-Content -LiteralPath $xamUiPath -Raw
if ($xamUiText -notmatch '\[STUB\] XamShowMessageBoxUIEx - not implemented' -or
    $xamUiText -notmatch 'uint32_t XamShowMessageBoxUIEx_entry\(\)') {
    throw 'Expected explicit XamShowMessageBoxUIEx semantic stub changed.'
}

$variables = @($importLookup.Values | Where-Object Type -eq 'V' | Sort-Object Ordinal)
$mappedOrdinals = [System.Collections.Generic.HashSet[int]]::new()
foreach ($match in [regex]::Matches($sdkText, 'SetVariableMapping\(\s*"xboxkrnl\.exe",\s*0x([0-9A-Fa-f]{4})')) {
    $mappedOrdinals.Add([Convert]::ToInt32($match.Groups[1].Value, 16)) | Out-Null
}
foreach ($variable in $variables) {
    if (-not $mappedOrdinals.Contains($variable.Ordinal)) { throw "Load-time variable mapping is missing: '$($variable.Name)'." }
}
if ($variables.Count -ne 11) { throw "Expected 11 load-time variables, found $($variables.Count)." }

$functionRows = foreach ($name in $expectedStartupImports | Sort-Object) {
    $import = $importLookup[$name]
    $owners = @($importOwners[$name] | Sort-Object -Unique) -join ', '
    $sdkStatus = if ($name -eq 'XamShowMessageBoxUIEx') { 'semantic stub (error path)' } else { 'concrete registration' }
    "| ``$($import.Library)`` | ``$($import.Ordinal) / 0x$($import.OrdinalHex)`` | ``$name`` | $owners | $($import.XeniaStatus) | $sdkStatus |"
}
$variableRows = foreach ($variable in $variables) {
    "| ``$($variable.Ordinal) / 0x$($variable.OrdinalHex)`` | ``$($variable.Name)`` | ``0x$($variable.Iat)`` | mapped before guest entry |"
}

$report = @(
    '# M2-014 startup import and stub set', '', 'Date: 2026-08-11',
    'Result: FINITE STARTUP SET; NO MISSING SDK REGISTRATION', '',
    '## Boundary and evidence', '',
    '- Module entry: `xstart` at `0x821322B8`',
    '- Title-main boundary: first `xstart` call to `sub_821305E8`',
    "- Accepted generated-manifest SHA-256: ``$manifestHash``",
    "- M2-013 import matrix SHA-256: ``$coverageHash``",
    '- SDK: ReXGlue v0.9.0 at pinned commit `3eb9b511b4140d2769e27be63eae57d41bfa2afa`', '',
    'The host runtime resolves variable imports before invoking `xstart`. The early guest envelope is then bounded at the first transition into title main, so post-main shutdown calls such as `DbgPrint` are not mislabeled startup requirements.', '',
    '## Load-time minimum', '',
    'All eleven variable imports must be mapped before guest entry. The pinned SDK contains all eleven mappings.', '',
    '| Ordinal dec/hex | Variable | IAT slot | SDK state |', '| --- | --- | --- | --- |'
) + $variableRows + @('',
    '## Pre-main guest envelope', '',
    '- Internal functions in bounded transitive closure: **59**',
    '- Maximum static call depth from the seven direct initializer roots: **6**',
    '- Function imports in conservative pre-main set: **26**',
    '- Missing ReXGlue registrations: **0**',
    '- Explicit semantic stubs in this set: **1** (`XamShowMessageBoxUIEx`, error path)', '',
    '| Library | Ordinal dec/hex | Import | Static owners (`function@depth`) | Xenia status | ReXGlue status |',
    '| --- | --- | --- | --- | --- | --- |'
) + $functionRows + @('',
    '## Minimum project work before title main', '',
    '- New project-owned function stubs required: **0**.',
    '- New project-owned variable mappings required: **0**.',
    '- Preserve the SDK warning/zero-return behavior of `XamShowMessageBoxUIEx` for now; it is reachable only through the bounded error-dialog branch and the pinned stock route reaches gameplay without taking it.',
    '- Treat `HalReturnToFirmware`, `KeBugCheck*`, `RtlRaiseException`, and `XamLoaderTerminateTitle` as terminating/error paths, not successful startup markers.',
    '- M3 must still build and smoke-test the real native runtime; static availability does not prove behavior parity.', '',
    'M2-014 acceptance: PASS. Load-time mappings and the finite pre-main import envelope are explicit, and no missing SDK registration blocks entry into title main.'
)

[System.IO.File]::WriteAllText($resolvedOutput, (($report -join [Environment]::NewLine) + [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))
[pscustomobject]@{
    Exported=$true
    LoadTimeVariables=$variables.Count
    PremainFunctions=$depths.Count
    MaximumDepth=($depths.Values | Measure-Object -Maximum).Maximum
    StartupImports=$expectedStartupImports.Count
    MissingRegistrations=0
    SemanticStubs=1
    OutputPath=$resolvedOutput
    OutputSha256=(Get-FileHash -LiteralPath $resolvedOutput -Algorithm SHA256).Hash
}
