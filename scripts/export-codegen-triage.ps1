[CmdletBinding()]
param(
    [string]$RawRoot,
    [string]$AuditPath,
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$evidenceRoot = Join-Path $repoRoot 'docs/evidence'
if (-not $RawRoot) { $RawRoot = Join-Path $repoRoot 'private/evidence/M2-009/force-first-run' }
if (-not $AuditPath) { $AuditPath = Join-Path $repoRoot 'private/evidence/M2-010/address-audit.tsv' }
if (-not $OutputPath) { $OutputPath = Join-Path $evidenceRoot 'M2-010-codegen-triage.md' }

function Resolve-RegularInput {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Required input was not found: '$Path'." }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if ((Get-Item -LiteralPath $resolved -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Input file must not be a reparse point: '$resolved'."
    }
    return $resolved
}

$stderrPath = Resolve-RegularInput (Join-Path $RawRoot 'stderr.log')
$metadataPath = Resolve-RegularInput (Join-Path $RawRoot 'run.json')
$resolvedAudit = Resolve-RegularInput $AuditPath
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
$resolvedEvidenceRoot = [System.IO.Path]::GetFullPath($evidenceRoot).TrimEnd('\')
if (-not $resolvedOutput.StartsWith("$resolvedEvidenceRoot\", [System.StringComparison]::OrdinalIgnoreCase) -or
    [System.IO.Path]::GetExtension($resolvedOutput) -ne '.md') {
    throw "Output must be a Markdown file below '$resolvedEvidenceRoot'."
}

$metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
$stderrHash = (Get-FileHash -LiteralPath $stderrPath -Algorithm SHA256).Hash
if ($metadata.schema -ne 1 -or $metadata.task -ne 'M2-009' -or
    $metadata.command -ne 'rexglue --force codegen mcla_manifest.toml' -or
    $metadata.exit_code -ne 0 -or $metadata.stderr_sha256 -ne $stderrHash -or
    $stderrHash -ne '3679E164642BE88B29E549FBE16BEC2A95658F6408994FFEBF88610FF8207CFD') {
    throw 'Force-run identity does not match the immutable M2-009 evidence.'
}

$expectedCalls = @(
    [pscustomobject]@{ Target='822B88C8'; Source='82203F90' },
    [pscustomobject]@{ Target='824B0DE8'; Source='824AF4D0' },
    [pscustomobject]@{ Target='8220C018'; Source='8220DA7C' },
    [pscustomobject]@{ Target='8220BF08'; Source='8220DAF4' },
    [pscustomobject]@{ Target='822C98B8'; Source='822C9E04' },
    [pscustomobject]@{ Target='823F32E8'; Source='823FB7F4' },
    [pscustomobject]@{ Target='823FD718'; Source='823FDB24' }
)
$expectedPacks = @(
    '8243C67C','8243C6F4','8243C7FC','8243C808','8243C80C','8243C810','8243C814','8243C818','8243C828','8243C894',
    '8243CD48','8243D010','8243D040','8243D050','8243D064','8243D07C','8243D08C','8243D0B8','8243D0CC','8243D158'
)

$rawLines = @(Get-Content -LiteralPath $stderrPath)
$callLines = @($rawLines | Where-Object { $_ -match '^  0x([0-9A-F]{8}) from 0x([0-9A-F]{8}): b 0x\1 from 0x\2 - target not in any function$' })
$packLines = @($rawLines | Where-Object { $_ -match '^Unexpected float16_4 pack instruction at ([0-9A-F]{8})$' })
if ($callLines.Count -ne 7 -or $packLines.Count -ne 20) { throw 'Expected exactly seven call and twenty pack findings.' }
foreach ($call in $expectedCalls) {
    $expected = "  0x$($call.Target) from 0x$($call.Source): b 0x$($call.Target) from 0x$($call.Source) - target not in any function"
    if (@($callLines | Where-Object { $_ -ceq $expected }).Count -ne 1) { throw "Missing or duplicate call finding: $expected" }
}
foreach ($address in $expectedPacks) {
    if (@($packLines | Where-Object { $_ -ceq "Unexpected float16_4 pack instruction at $address" }).Count -ne 1) {
        throw "Missing or duplicate pack finding at $address."
    }
}

$auditLines = @(Get-Content -LiteralPath $resolvedAudit)
if ($auditLines.Count -ne 37 -or $auditLines[0] -cne "program`tdefault.xex" -or
    $auditLines[1] -cne "image_base`t00000000") { throw 'Address audit header or record count is unexpected.' }
$audit = @($auditLines | Select-Object -Skip 2 | ConvertFrom-Csv -Delimiter "`t")
if ($audit.Count -ne 34) { throw 'Address audit must contain exactly 34 address records.' }
$auditByAddress = @{}
foreach ($row in $audit) {
    $key = $row.address.ToUpperInvariant()
    if ($auditByAddress.ContainsKey($key)) { throw "Duplicate audit address: $key" }
    if ($row.block -cne '.text' -or $row.execute -cne 'true') { throw "Audit address is not executable .text: $key" }
    $auditByAddress[$key] = $row
}

foreach ($call in $expectedCalls) {
    if (-not $auditByAddress.ContainsKey($call.Source) -or -not $auditByAddress.ContainsKey($call.Target)) {
        throw "Audit is missing call pair $($call.Source) -> $($call.Target)."
    }
    $source = $auditByAddress[$call.Source]
    $target = $auditByAddress[$call.Target]
    if ($source.instruction -cne "b 0x$($call.Target.ToLowerInvariant())" -or
        ($target.previous -cne 'blr' -and $source.previous -cne 'blr') -or
        $target.instruction -ceq '<no-instruction>') {
        throw "Audit does not support the function-boundary classification for $($call.Source)."
    }
}

$packWords = @{}
foreach ($address in $expectedPacks) {
    if (-not $auditByAddress.ContainsKey($address)) { throw "Audit is missing pack address $address." }
    $wordText = $auditByAddress[$address].bytes.ToUpperInvariant()
    if ($wordText -notmatch '^[0-9A-F]{8}$') { throw "Invalid instruction word at $address." }
    $word = [Convert]::ToUInt32($wordText, 16)
    $format = ($word -shr 18) -band 7
    $mask = ($word -shr 16) -band 3
    $shift = ($word -shr 6) -band 3
    if ($format -ne 5 -or $mask -ne 3 -or $shift -ne 0) {
        throw "Pack fields changed at $address (format=$format mask=$mask shift=$shift)."
    }
    $packWords[$address] = $wordText
}

$auditHash = (Get-FileHash -LiteralPath $resolvedAudit -Algorithm SHA256).Hash
$rows = [System.Collections.Generic.List[string]]::new()
$index = 1
foreach ($call in $expectedCalls) {
    $rows.Add(("| CF-{0:D2} | ``0x{1}`` -> ``0x{2}`` | Config fix | MCLA-R analysis/config | S1 | Verify unwind/PData and helper boundaries in M2-011; add the target as a justified manual function in M2-012, then rerun non-force codegen. |" -f $index,$call.Source,$call.Target))
    $index++
}
$index = 1
foreach ($address in $expectedPacks) {
    $rows.Add(("| VP-{0:D2} | ``0x{1}`` / ``{2}`` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |" -f $index,$address,$packWords[$address]))
    $index++
}

$report = @(
    '# M2-010 codegen finding triage', '', 'Date: 2026-08-11', 'Result: ALL 27 UNIQUE FINDINGS CLASSIFIED', '',
    '## Evidence identity', '',
    "- M2-009 force stderr SHA-256: ``$stderrHash``",
    "- Private Ghidra address-audit SHA-256: ``$auditHash``",
    '- Audit scope: 34 unique guest addresses (7 source/target pairs plus 20 vector-pack sites)',
    '- XEX identity is inherited from the verified M2-009 run and remains private.', '',
    '## Classification summary', '',
    '| Category | Findings | Severity | Owner |', '| --- | ---: | --- | --- |',
    '| Config fix | 7 | S1 | MCLA-R analysis/config |',
    '| ReXGlue defect | 20 | S2 | Upstream ReXGlue; local SDK fork if required |',
    '| Project hook/stub | 0 | N/A | N/A |', '| False positive | 0 | N/A | N/A |', '| Unknown | 0 | N/A | N/A |', '',
    'The seven control-flow findings are S1 because normal codegen fails and force-generated call sites terminate with a fatal diagnostic if executed. The 20 pack findings are S2: generation completes, but unverified vector packing can produce a major rendering/data defect.', '',
    '## Per-finding register', '',
    '| ID | Guest site | Category | Owner | Severity | Next action |', '| --- | --- | --- | --- | --- | --- |'
) + $rows + @(
    '', '## Rationale', '',
    '### Seven unresolved direct branches', '',
    'Every source instruction is an unconditional direct branch to mapped executable `.text`, and every target contains an instruction plus an incoming reference. Six targets are immediately preceded by `blr`; in the remaining pair the source branch is immediately preceded by `blr` and the target has another local incoming branch. This is consistent with omitted function/chunk boundaries rather than an import. M2-011 must distinguish save/restore helpers, function chunks, and exception metadata before M2-012 adds manual boundaries.', '',
    '### Twenty FLOAT16_4 pack warnings', '',
    'All twenty instruction words decode as `vpkd3d128` format 5 (`FLOAT16_4`), mask 3, shift 0. ReXGlue v0.9.0 warns whenever FLOAT16_4 mask is not 2, while its writer still emits conversion code. Xenia''s independent Xenon implementation accepts masks 1 through 3 and treats mask 3 like mask 2 except for the shift-3 special case. Therefore mask 3/shift 0 is a valid operand shape and the ReXGlue diagnostic/coverage gap is an SDK defect, although emitted-value parity still needs M2-016 regression tests.', '',
    '- ReXGlue source: `third_party/rexglue-sdk/src/codegen/builders/vector.cpp` (`build_vpkd3d128`)',
    '- Independent reference: [Xenia `ppc_emit_altivec.cc` at audited revision](https://github.com/xenia-project/xenia/blob/95a5c3ee250f80c3b9d139658649d9ffb6db3eec/src/xenia/cpu/ppc/ppc_emit_altivec.cc)',
    '- Reference tests: [Xenia `instr_vpkd3d128.s` at audited revision](https://github.com/xenia-project/xenia/blob/95a5c3ee250f80c3b9d139658649d9ffb6db3eec/src/xenia/cpu/ppc/testing/instr_vpkd3d128.s)', '',
    '## Decision', '',
    '- No finding remains uncatalogued or unknown.',
    '- No project hook/stub is justified by this codegen evidence.',
    '- Proceed to M2-011 before changing the function list.',
    '- Preserve the 20 pack sites as the M2-016 SDK regression corpus.'
)

[System.IO.File]::WriteAllText($resolvedOutput, (($report -join [Environment]::NewLine) + [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))

[pscustomobject]@{
    Exported = $true
    FindingCount = $rows.Count
    ConfigFixCount = 7
    ReXGlueDefectCount = 20
    UnknownCount = 0
    OutputPath = $resolvedOutput
    OutputSha256 = (Get-FileHash -LiteralPath $resolvedOutput -Algorithm SHA256).Hash
}
