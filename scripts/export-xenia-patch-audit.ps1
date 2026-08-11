[CmdletBinding()]
param(
    [string]$PatchPath,
    [string]$AuditPath,
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$evidenceRoot = Join-Path $repoRoot 'docs/evidence'
if (-not $PatchPath) { $PatchPath = Join-Path $repoRoot 'private/evidence/M2-015/complete-edition.patch.toml' }
if (-not $AuditPath) { $AuditPath = Join-Path $repoRoot 'private/evidence/M2-015/patch-audit.tsv' }
if (-not $OutputPath) { $OutputPath = Join-Path $evidenceRoot 'M2-015-xenia-patch-audit.md' }

function Resolve-RegularInput {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Required input was not found: '$Path'." }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if ((Get-Item -LiteralPath $resolved -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Input must not be a reparse point: '$resolved'."
    }
    return $resolved
}

$resolvedPatch = Resolve-RegularInput $PatchPath
$resolvedAudit = Resolve-RegularInput $AuditPath
$xexPath = Resolve-RegularInput (Join-Path $repoRoot 'private/game/default.xex')
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
$safeOutputRoot = [System.IO.Path]::GetFullPath($evidenceRoot).TrimEnd('\')
if (-not $resolvedOutput.StartsWith("$safeOutputRoot\", [System.StringComparison]::OrdinalIgnoreCase) -or
    [System.IO.Path]::GetExtension($resolvedOutput) -ne '.md') {
    throw "Output must be a Markdown file below '$safeOutputRoot'."
}

$patchHash = (Get-FileHash -LiteralPath $resolvedPatch -Algorithm SHA256).Hash
$auditHash = (Get-FileHash -LiteralPath $resolvedAudit -Algorithm SHA256).Hash
$xexHash = (Get-FileHash -LiteralPath $xexPath -Algorithm SHA256).Hash
if ($patchHash -ne 'AA9873984BEAE91FD68152CBFC34A07A17D1E4C6231B88F749048489728B06B6' -or
    $auditHash -ne 'CF2EC8252E3B670998AB2DF32C0FC05FD2306E5922BC595F01B40EB53E56FDDD' -or
    $xexHash -ne 'C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432') {
    throw 'Patch, loaded-byte audit, or target XEX differs from the reviewed M2-015 inputs.'
}

$patchText = Get-Content -LiteralPath $resolvedPatch -Raw
if ($patchText -notmatch '(?m)^title_id = "545407F8"' -or
    $patchText -notmatch '(?m)^hash = "1984A3354B78CE19"' -or
    $patchText -notmatch '(?m)^#media_id = "5940C9DB"' -or
    @([regex]::Matches($patchText, '(?m)^\s*is_enabled = false\s*$')).Count -ne 6 -or
    $patchText -match '(?m)^\s*is_enabled = true\s*$') {
    throw 'Upstream patch identity or disabled-by-default policy changed.'
}

$expected = @(
    [pscustomobject]@{Group='60 FPS - Game Speed Fix';Type='be32';Address='821BDB08';Value='4800012C';Original='409A00C0';Word='409A00C0';Instruction='bne cr6,0x821bdbc8';Result='b 0x821bdc34';Requested=$true},
    [pscustomobject]@{Group='60 FPS - Game Speed Fix';Type='be8';Address='82419AA3';Value='01';Original='02';Word='39600002';Instruction='li r11,0x2';Result='li r11,0x1';Requested=$true},
    [pscustomobject]@{Group='Skip Intro';Type='be32';Address='821F7F64';Value='48000048';Original='419A0048';Word='419A0048';Instruction='beq cr6,0x821f7fac';Result='b 0x821f7fac';Requested=$true},
    [pscustomobject]@{Group='Disable Motion Blur';Type='be32';Address='8260D0B8';Value='38600000';Original='4BB7D4B1';Word='4BB7D4B1';Instruction='bl 0x8218a568';Result='li r3,0x0';Requested=$true},
    [pscustomobject]@{Group='Disable Motion Blur';Type='be32';Address='8260D0D4';Value='38600000';Original='4BB7D495';Word='4BB7D495';Instruction='bl 0x8218a568';Result='li r3,0x0';Requested=$true},
    [pscustomobject]@{Group='Disable Imposter Shadows - Performance Mode';Type='be16';Address='8230C87C';Value='4800';Original='419A';Word='419A01C0';Instruction='beq cr6,0x8230ca3c';Result='b 0x8230ca3c';Requested=$true},
    [pscustomobject]@{Group='Disable MSAA';Type='be32';Address='822E4B80';Value='39600001';Original='816A0004';Word='816A0004';Instruction='lwz r11,0x4(r10)';Result='li r11,0x1';Requested=$true},
    [pscustomobject]@{Group='DbgPrint';Type='be32';Address='821BD618';Value='485FF8DC';Original='7D8802A6';Word='7D8802A6';Instruction='mfspr r12,LR';Result='b 0x827bcef4';Requested=$false}
)

$parsedWrites = @()
$currentGroup = ''
$currentType = ''
$pendingAddress = ''
foreach ($rawLine in Get-Content -LiteralPath $resolvedPatch) {
    $line = $rawLine.Trim()
    if ($line -match '^name = "(.+)"$') { $currentGroup = $Matches[1]; continue }
    if ($line -match '^\[\[patch\.(be8|be16|be32)\]\]$') { $currentType = $Matches[1]; $pendingAddress = ''; continue }
    if ($currentType -and $line -match '^address = 0x([0-9a-f]{8})$') { $pendingAddress = $Matches[1].ToUpperInvariant(); continue }
    if ($currentType -and $pendingAddress -and $line -match '^value = 0x([0-9a-f]+)(?:\s+#.*)?$') {
        $parsedWrites += [pscustomobject]@{Group=$currentGroup;Type=$currentType;Address=$pendingAddress;Value=$Matches[1].ToUpperInvariant()}
        $currentType = ''; $pendingAddress = ''
    }
}
if ($parsedWrites.Count -ne $expected.Count) { throw "Expected 8 upstream writes, found $($parsedWrites.Count)." }
for ($index = 0; $index -lt $expected.Count; $index++) {
    $actual = $parsedWrites[$index]
    $wanted = $expected[$index]
    if ($actual.Group -cne $wanted.Group -or $actual.Type -cne $wanted.Type -or
        $actual.Address -cne $wanted.Address -or $actual.Value -cne $wanted.Value) {
        throw "Upstream patch write $($index + 1) changed."
    }
}

$auditLines = @(Get-Content -LiteralPath $resolvedAudit)
if ($auditLines.Count -ne 11 -or $auditLines[0] -cne "program`tdefault.xex" -or
    $auditLines[1] -cne "image_base`t00000000") {
    throw 'Private loaded-image audit header or record count changed.'
}
$auditRecords = @{}
foreach ($line in $auditLines | Select-Object -Skip 3) {
    $fields = $line.Split("`t")
    if ($fields.Count -ne 8 -or $auditRecords.ContainsKey($fields[0].ToUpperInvariant())) {
        throw 'Malformed or duplicate loaded-image audit record.'
    }
    $auditRecords[$fields[0].ToUpperInvariant()] = [pscustomobject]@{
        Length=[int]$fields[1]; Bytes=$fields[2]; WordAddress=$fields[3]; Word=$fields[4]
        Instruction=$fields[5]; FunctionEntry=$fields[6]; FunctionName=$fields[7]
    }
}
foreach ($site in $expected) {
    if (-not $auditRecords.ContainsKey($site.Address)) { throw "Missing loaded-image bytes at 0x$($site.Address)." }
    $record = $auditRecords[$site.Address]
    $wantedLength = switch ($site.Type) { 'be8' { 1 } 'be16' { 2 } 'be32' { 4 } }
    if ($record.Length -ne $wantedLength -or $record.Bytes -cne $site.Original -or
        $record.Word -cne $site.Word -or $record.Instruction -cne $site.Instruction) {
        throw "Original byte/instruction context changed at 0x$($site.Address)."
    }
}

$rows = foreach ($site in $expected) {
    $scope = if ($site.Requested) { 'requested enhancement' } else { 'diagnostic extra' }
    "| $($site.Group) | $scope | ``0x$($site.Address)`` / $($site.Type) | ``$($site.Original)`` | ``$($site.Value)`` | $($site.Instruction) | $($site.Result) | VERIFIED, disabled |"
}
$report = @(
    '# M2-015 Complete Edition Xenia patch byte audit', '', 'Date: 2026-08-11',
    'Result: ALL PUBLIC ADDRESSES MATCH; NO PATCH ENABLED', '',
    '## Source identity', '',
    '- Upstream: `xenia-canary/game-patches` commit `84d6682caf1b75b2fdb7adcd197c6559c09b2ed4`',
    '- File: `545407F8 - Midnight Club Los Angeles (Complete Edition).patch.toml`',
    "- Upstream file SHA-256: ``$patchHash``",
    '- Declared module hash: `1984A3354B78CE19` (exact pinned Xenia baseline match)',
    '- Declared Media ID: `5940C9DB` (exact local dump match)',
    "- Local XEX SHA-256: ``$xexHash``",
    "- Private Ghidra loaded-byte audit SHA-256: ``$auditHash``", '',
    'The upstream file contains six patch groups and eight writes. Every group is explicitly `is_enabled = false`; this task validates provenance and bytes only.', '',
    '## Address audit', '',
    '| Patch | Scope | Address/type | Original bytes | Replacement | Original instruction | Patched meaning | Decision |',
    '| --- | --- | --- | --- | --- | --- | --- | --- |'
) + $rows + @('',
    'Partial writes were validated in their containing words: `0x82419AA3` changes `39600002` to `39600001`; `0x8230C87C` changes `419A01C0` to `480001C0`. The DbgPrint branch target resolves to the existing import thunk at `0x827BCEF4`.', '',
    '## Decision', '',
    '- Requested enhancement groups byte-verified: **5/5** (7 writes).',
    '- Additional upstream diagnostic group byte-verified: **1/1** (1 write).',
    '- Rejected addresses: **0**.',
    '- Enabled patches: **0**.',
    '- M8 owns any future opt-in 60 FPS, visual, or skip-intro enablement and behavioral testing.', '',
    'M2-015 acceptance: PASS. Every current Complete Edition upstream write is byte-verified against the loaded local image and remains disabled.'
)
[System.IO.File]::WriteAllText($resolvedOutput, (($report -join [Environment]::NewLine) + [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))
[pscustomobject]@{
    Exported=$true
    PatchGroups=6
    RequestedGroups=5
    Writes=$expected.Count
    VerifiedWrites=$expected.Count
    RejectedWrites=0
    EnabledPatches=0
    OutputPath=$resolvedOutput
    OutputSha256=(Get-FileHash -LiteralPath $resolvedOutput -Algorithm SHA256).Hash
}
