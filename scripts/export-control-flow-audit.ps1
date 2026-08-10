[CmdletBinding()]
param(
    [string]$AuditPath,
    [string]$ForceRoot,
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$evidenceRoot = Join-Path $repoRoot 'docs/evidence'
$generatedRoot = Join-Path $repoRoot 'generated/default'
if (-not $AuditPath) { $AuditPath = Join-Path $repoRoot 'private/evidence/M2-011/control-flow-audit.tsv' }
if (-not $ForceRoot) { $ForceRoot = Join-Path $repoRoot 'private/evidence/M2-009/force-first-run' }
if (-not $OutputPath) { $OutputPath = Join-Path $evidenceRoot 'M2-011-control-flow-metadata.md' }

function Resolve-RegularInput {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Required input was not found: '$Path'." }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if ((Get-Item -LiteralPath $resolved -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Input must not be a reparse point: '$resolved'."
    }
    return $resolved
}

$resolvedAudit = Resolve-RegularInput $AuditPath
$manifestPath = Resolve-RegularInput (Join-Path $ForceRoot 'generated-manifest.json')
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
$resolvedEvidenceRoot = [System.IO.Path]::GetFullPath($evidenceRoot).TrimEnd('\')
if (-not $resolvedOutput.StartsWith("$resolvedEvidenceRoot\", [System.StringComparison]::OrdinalIgnoreCase) -or
    [System.IO.Path]::GetExtension($resolvedOutput) -ne '.md') {
    throw "Output must be a Markdown file below '$resolvedEvidenceRoot'."
}
$outputParent = Split-Path -Parent $resolvedOutput
if (-not (Test-Path -LiteralPath $outputParent -PathType Container) -or
    ((Get-Item -LiteralPath $outputParent -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
    throw 'Output parent must be an existing non-reparse directory.'
}
if ((Get-Item -LiteralPath $generatedRoot -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
    throw 'Generated analysis root must not be a reparse point.'
}

$auditHash = (Get-FileHash -LiteralPath $resolvedAudit -Algorithm SHA256).Hash
if ($auditHash -ne 'FB13A297983F3A756B0314ABFB1A7E9162E672555D6F6D829155D32F145D0BA2') {
    throw 'Ghidra control-flow audit does not match the reviewed M2-011 evidence.'
}
$auditLines = @(Get-Content -LiteralPath $resolvedAudit)
if ($auditLines.Count -ne 68 -or $auditLines[0] -cne "program`tdefault.xex" -or
    $auditLines[1] -cne "pdata`t82102a00`t82129d37`t160568`t20071`t34") {
    throw 'Ghidra audit header, PDATA identity, or record count changed.'
}

$expectedSignatures = @(
    "signature`tsavegprlr_14`t823d91c0", "signature`trestgprlr_14`t823d9210",
    "signature`tsavefpr_14`t823db9a0", "signature`trestfpr_14`t823db9ec",
    "signature`tsavevmx_14`t823dd2c0", "signature`trestvmx_14`t823dd558",
    "signature`tsavevmx_64`t823dd354", "signature`trestvmx_64`t823dd5ec",
    "signature`tsetjmp_fpr_store_r3`tNONE", "signature`tlongjmp_fpr_load_r3`tNONE",
    "signature`tlongjmp_fpr_load_r7`tNONE"
)
foreach ($line in $expectedSignatures) {
    if (@($auditLines | Where-Object { $_ -ceq $line }).Count -ne 1) { throw "Missing signature result: $line" }
}

$pairs = @(
    [pscustomobject]@{ Id='CF-01'; Source='82203F90'; Target='822B88C8'; SourceFunction='sub_82203F90'; SourceGap='82203F7C-82203F98'; TargetGap='822B88A4-822B88E0'; Terminal='822B88D8 b'; Kind='tail-branch thunk' },
    [pscustomobject]@{ Id='CF-02'; Source='824AF4D0'; Target='824B0DE8'; SourceFunction='sub_824AF4D0'; SourceGap='824AF4CC-824AF4D8'; TargetGap='824B0DE8-824B0DF8'; Terminal='824B0DF0 b'; Kind='function-chunk candidate' },
    [pscustomobject]@{ Id='CF-03'; Source='8220DA7C'; Target='8220C018'; SourceFunction='sub_8220DA70'; SourceGap='8220DA30-8220DB60'; TargetGap='8220BDEC-8220C0D0'; Terminal='8220C0CC bctr'; Kind='computed-dispatch entry' },
    [pscustomobject]@{ Id='CF-04'; Source='8220DAF4'; Target='8220BF08'; SourceFunction='sub_8220DAE8'; SourceGap='8220DA30-8220DB60'; TargetGap='8220BDEC-8220C0D0'; Terminal='8220C014 bctr'; Kind='computed-dispatch entry' },
    [pscustomobject]@{ Id='CF-05'; Source='822C9E04'; Target='822C98B8'; SourceFunction='sub_822C9DF8'; SourceGap='822C9D6C-822C9E28'; TargetGap='822C97B0-822C9B88'; Terminal='822C9944 bctr'; Kind='computed-dispatch entry' },
    [pscustomobject]@{ Id='CF-06'; Source='823FB7F4'; Target='823F32E8'; SourceFunction='sub_823FB7F0'; SourceGap='823FB7E0-823FB848'; TargetGap='823F32DC-823F3300'; Terminal='823F32FC blr'; Kind='leaf helper' },
    [pscustomobject]@{ Id='CF-07'; Source='823FDB24'; Target='823FD718'; SourceFunction='sub_823FDB20'; SourceGap='823FDABC-823FDB30'; TargetGap='823FD6FC-823FD720'; Terminal='823FD71C b'; Kind='tail-branch thunk' }
)

$expectedSiteLines = @(
    "site`t82203f90`tGAP`t82203F50`t82203F7C`t82203F98", "site`t822b88c8`tGAP`t822B8820`t822B88A4`t822B88E0",
    "site`t824af4d0`tGAP`t824AF398`t824AF4CC`t824AF4D8", "site`t824b0de8`tGAP`t824B0CC0`t824B0DE8`t824B0DF8",
    "site`t8220da7c`tGAP`t8220D9E8`t8220DA30`t8220DB60", "site`t8220c018`tGAP`t8220BD90`t8220BDEC`t8220C0D0",
    "site`t8220daf4`tGAP`t8220D9E8`t8220DA30`t8220DB60", "site`t8220bf08`tGAP`t8220BD90`t8220BDEC`t8220C0D0",
    "site`t822c9e04`tGAP`t822C9D28`t822C9D6C`t822C9E28", "site`t822c98b8`tGAP`t822C96C8`t822C97B0`t822C9B88",
    "site`t823fb7f4`tGAP`t823FB708`t823FB7E0`t823FB848", "site`t823f32e8`tGAP`t823F3268`t823F32DC`t823F3300",
    "site`t823fdb24`tGAP`t823FD9B8`t823FDABC`t823FDB30", "site`t823fd718`tGAP`t823FD6A8`t823FD6FC`t823FD720"
)
$expectedFlowLines = @(
    "target_flow`t822b88c8`t822b88d8`tb`t5", "target_flow`t824b0de8`t824b0df0`tb`t3",
    "target_flow`t8220c018`t8220c0cc`tbctr`t46", "target_flow`t8220bf08`t8220c014`tbctr`t68",
    "target_flow`t822c98b8`t822c9944`tbctr`t36", "target_flow`t823f32e8`t823f32fc`tblr`t6",
    "target_flow`t823fd718`t823fd71c`tb`t2"
)
foreach ($line in @($expectedSiteLines + $expectedFlowLines)) {
    if (@($auditLines | Where-Object { $_ -ceq $line }).Count -ne 1) { throw "Missing control-flow result: $line" }
}
$exceptionLines = @($auditLines | Where-Object { $_ -match '^exception\t[0-9A-F]{8}\t[0-9A-F]{8}\t\d+\t(?:true|false)$' })
if ($exceptionLines.Count -ne 34) { throw 'Expected exactly 34 exception-marked PDATA records.' }

$generatedManifestHash = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
if ($generatedManifestHash -ne 'F0A16B36EECD0EAAF49B2272B334C39FB487E58DDA08276E6A6D85CBAB5B5BDC') {
    throw 'Generated manifest is not the immutable M2-009 snapshot.'
}
$generatedManifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
foreach ($entry in @($generatedManifest.files)) {
    $path = Join-Path $generatedRoot $entry.path
    if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
        (Get-Item -LiteralPath $path).Length -ne $entry.bytes -or
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw "Generated analysis snapshot changed: '$($entry.path)'."
    }
}

$branchOwners = @{}
$unwindCallOwner = $null
$unwindCallCount = 0
foreach ($file in Get-ChildItem -LiteralPath $generatedRoot -Filter 'mcla_recomp.*.cpp' -File) {
    $currentFunction = ''
    foreach ($line in [System.IO.File]::ReadLines($file.FullName)) {
        if ($line -match '^DEFINE_REX_FUNC\(([^)]+)\)') { $currentFunction = $Matches[1] }
        if ($line -match '^\s*// b 0x([0-9a-f]{8})$') {
            $target = $Matches[1].ToUpperInvariant()
            if (@($pairs.Target) -contains $target) {
                if ($branchOwners.ContainsKey($target)) { throw "Duplicate unresolved branch body for $target." }
                $branchOwners[$target] = $currentFunction
            }
        }
        if ($line -ceq "`t// bl 0x827bdca4") {
            $unwindCallCount++
            $unwindCallOwner = $currentFunction
        }
    }
}
foreach ($pair in $pairs) {
    if (-not $branchOwners.ContainsKey($pair.Target) -or $branchOwners[$pair.Target] -cne $pair.SourceFunction) {
        throw "Generated source-function ownership changed for $($pair.Id)."
    }
}
if ($unwindCallCount -ne 1 -or $unwindCallOwner -cne 'sub_8274ABB0') {
    throw 'Expected one RtlUnwind caller owned by sub_8274ABB0.'
}

$initText = Get-Content -LiteralPath (Join-Path $generatedRoot 'mcla_init.cpp') -Raw
foreach ($pair in $pairs) {
    if ($initText -match "\{ 0x$($pair.Target),") { throw "Previously unresolved target is unexpectedly registered: $($pair.Target)." }
}
$manifestText = Get-Content -LiteralPath (Join-Path $repoRoot 'mcla_manifest.toml') -Raw
if ($manifestText -match '(?m)^\s*(setjmp_address|longjmp_address)\s*=') {
    throw 'setjmp/longjmp addresses must remain unset until contrary evidence exists.'
}

$pairRows = foreach ($pair in $pairs) {
    "| $($pair.Id) | ``0x$($pair.Source)`` / ``$($pair.SourceFunction)`` | ``0x$($pair.Target)`` | ``$($pair.SourceGap)`` | ``$($pair.TargetGap)`` | $($pair.Terminal) | $($pair.Kind) |"
}
$exceptionRows = foreach ($line in $exceptionLines) {
    $fields = $line.Split("`t")
    "| ``0x$($fields[1])`` | ``0x$($fields[2])`` | $($fields[3]) | $($fields[4]) |"
}

$report = @(
    '# M2-011 control-flow and exception metadata audit', '', 'Date: 2026-08-11',
    'Result: HELPERS, PDATA, JUMP CONTEXT, AND GAP ENTRIES IDENTIFIED', '',
    '## Evidence identity', '', "- Private Ghidra audit SHA-256: ``$auditHash``",
    "- Private generated-manifest SHA-256: ``$generatedManifestHash``",
    '- Ghidra: 12.0.4 with XEXLoaderWV 13.0.0, PowerPC big-endian Xenon language',
    '- ReXGlue: pinned v0.9.0 M2-009 force snapshot', '',
    '## Save/restore helpers', '',
    '| Family base | Save | Restore | ReXGlue expansion |', '| --- | --- | --- | --- |',
    '| GPR 14 | `0x823D91C0` | `0x823D9210` | registers 14-31, stride 4 |',
    '| FPR 14 | `0x823DB9A0` | `0x823DB9EC` | registers 14-31, stride 4 |',
    '| VMX 14 | `0x823DD2C0` | `0x823DD558` | registers 14-31, stride 8 |',
    '| VMX 64 | `0x823DD354` | `0x823DD5EC` | registers 64-127, stride 8 |', '',
    'All eight base signatures are unique in executable memory and agree with the helper registrations in the immutable generated dispatcher.', '',
    '## setjmp / longjmp decision', '',
    '- No standard 18-register `stfd f14..f31` setjmp body targeting `r3` exists.',
    '- No standard 18-register `lfd f14..f31` longjmp body targeting `r3` or copied buffer register `r7` exists.',
    '- The image imports `RtlUnwind`; its only direct generated caller is `sub_8274ABB0`, a small unwind wrapper that does not contain jump-buffer FPR restore semantics.',
    '- Therefore `setjmp_address` and `longjmp_address` remain intentionally unset. Revisit only if runtime evidence exposes a nonstandard indirect implementation.', '',
    '## Seven unresolved cross-gap branches', '',
    '| ID | Source / generated owner | Target | Source PDATA gap | Target PDATA gap | First terminal | Classification |',
    '| --- | --- | --- | --- | --- | --- | --- |'
) + $pairRows + @(
    '', 'All fourteen source/target sites are executable but outside every PDATA range. ReXGlue recovered each source during gap fill but did not register the target as a call target. CF-02 is the sole parent-chunk candidate: `0x824B0DE8` begins exactly at the end of PDATA function `0x824B0CC0-0x824B0DE8` and also has a local incoming branch. CF-03 and CF-04 are separate entries in one shared dispatch gap. The remaining targets have standalone terminal behavior. M2-012 must add the minimum explicit entries and verify the classification by a non-force rerun.', '',
    'The M2-009 analyzer reported zero `DiscontinuousFunction` diagnostics. That zero does not erase the cross-gap evidence above; it means there is no additional analyzer-reported discontinuity outside this finite seven-pair set.', '',
    '## Exception directory', '',
    '- `.pdata`: `0x82102A00-0x82129D38` (160,568 bytes)',
    '- Runtime-function records: 20,071 (8 bytes each)',
    '- Records with `ExceptionFlag`: 34', '',
    '| Function begin | Exclusive end | Prolog instructions | 32-bit flag |', '| --- | --- | ---: | --- |'
) + $exceptionRows + @(
    '', 'ReXGlue consumed this exception directory without an exception-parser diagnostic. Exception wrapper generation remains disabled by the conservative M2-007 policy; M2-012 must not add exception-handler hints unless a rerun or runtime route demonstrates a missing handler.', '',
    '## M2-012 handoff', '',
    '- Add the seven target entries incrementally, not as a bulk speculative range.',
    '- Test CF-02 first as a chunk of `0x824B0CC0`; fall back to a standalone function only if the analyzer rejects or misroutes the parent relation.',
    '- Treat CF-03/CF-04 as explicit multi-entry dispatch functions in the shared PDATA gap.',
    '- Do not add switch tables, invalid regions, exception hints, or setjmp/longjmp overrides without new address-level evidence.',
    '- A successful non-force rerun with zero uncatalogued findings is the configuration acceptance gate.'
)

[System.IO.File]::WriteAllText($resolvedOutput, (($report -join [Environment]::NewLine) + [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))
[pscustomobject]@{ Exported=$true; PdataRecords=20071; ExceptionRecords=34; HelperBases=8; GapPairs=7; OutputPath=$resolvedOutput; OutputSha256=(Get-FileHash $resolvedOutput -Algorithm SHA256).Hash }
