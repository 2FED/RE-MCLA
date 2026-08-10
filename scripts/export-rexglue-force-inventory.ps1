[CmdletBinding()]
param(
    [string]$RawRoot,
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$evidenceRoot = Join-Path $repoRoot 'docs/evidence'
$generatedRoot = Join-Path $repoRoot 'generated/default'
if (-not $RawRoot) {
    $RawRoot = Join-Path $repoRoot 'private/evidence/M2-009/force-first-run'
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $evidenceRoot 'M2-009-force-codegen-inventory.md'
}

function Resolve-RegularInput {
    param([Parameter(Mandatory)][string]$Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Required input was not found: '$Path'."
    }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if ((Get-Item -LiteralPath $resolved -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Input file must not be a reparse point: '$resolved'."
    }
    return $resolved
}

$resolvedRawRoot = (Resolve-Path -LiteralPath $RawRoot).Path
if ((Get-Item -LiteralPath $resolvedRawRoot -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
    throw "Raw evidence root must not be a reparse point: '$resolvedRawRoot'."
}
$metadataPath = Resolve-RegularInput (Join-Path $resolvedRawRoot 'run.json')
$stdoutPath = Resolve-RegularInput (Join-Path $resolvedRawRoot 'stdout.log')
$stderrPath = Resolve-RegularInput (Join-Path $resolvedRawRoot 'stderr.log')
$generatedManifestPath = Resolve-RegularInput (Join-Path $resolvedRawRoot 'generated-manifest.json')

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
$resolvedEvidenceRoot = [System.IO.Path]::GetFullPath($evidenceRoot).TrimEnd('\')
if (-not $resolvedOutput.StartsWith("$resolvedEvidenceRoot\", [System.StringComparison]::OrdinalIgnoreCase) -or
    [System.IO.Path]::GetExtension($resolvedOutput) -ne '.md') {
    throw "Output must be a Markdown file below '$resolvedEvidenceRoot'."
}
$outputParent = Split-Path -Parent $resolvedOutput
if ((Get-Item -LiteralPath $outputParent -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
    throw 'Output directory must not be a reparse point.'
}
if (Test-Path -LiteralPath $resolvedOutput) {
    $outputItem = Get-Item -LiteralPath $resolvedOutput -Force
    if ($outputItem.PSIsContainer -or ($outputItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
        throw 'Output must be a regular file, not a directory or reparse point.'
    }
}

$metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
$stdoutHash = (Get-FileHash -LiteralPath $stdoutPath -Algorithm SHA256).Hash
$stderrHash = (Get-FileHash -LiteralPath $stderrPath -Algorithm SHA256).Hash
$generatedManifestHash = (Get-FileHash -LiteralPath $generatedManifestPath -Algorithm SHA256).Hash
if ($metadata.schema -ne 1 -or $metadata.task -ne 'M2-009' -or
    $metadata.command -ne 'rexglue --force codegen mcla_manifest.toml' -or
    $metadata.force_enabled -ne $true -or $metadata.exit_code -ne 0 -or
    $metadata.stdout_sha256 -ne $stdoutHash -or $metadata.stderr_sha256 -ne $stderrHash -or
    $metadata.generated_manifest_sha256 -ne $generatedManifestHash -or
    $metadata.prerequisite_stderr_sha256 -ne '1D0E23191068D2B92DCC76D24D9D80B0B699111A1C9231494D0EF60ECB5B65BB') {
    throw 'Run metadata does not describe the expected immutable M2-009 force inventory.'
}
if ((Get-Item -LiteralPath $stdoutPath).Length -ne $metadata.stdout_bytes -or
    (Get-Item -LiteralPath $stderrPath).Length -ne $metadata.stderr_bytes -or
    $metadata.stdout_bytes -ne 0) {
    throw 'The captured force run unexpectedly contains stdout data.'
}

$generatedManifest = Get-Content -LiteralPath $generatedManifestPath -Raw | ConvertFrom-Json
$entries = @($generatedManifest.files)
if ($generatedManifest.schema -ne 1 -or $generatedManifest.task -ne 'M2-009' -or
    $generatedManifest.file_count -ne 64 -or $generatedManifest.total_bytes -ne 128010691 -or
    $metadata.generated_target_exists -ne $true -or $metadata.generated_file_count -ne 64 -or
    $metadata.generated_total_bytes -ne 128010691 -or $entries.Count -ne 64) {
    throw 'Generated snapshot aggregate does not match the first force run.'
}
$seenPaths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($entry in $entries) {
    $segments = @($entry.path.Split('/'))
    if ([string]::IsNullOrWhiteSpace($entry.path) -or $entry.path.Contains('\') -or
        [System.IO.Path]::IsPathRooted($entry.path) -or $segments -contains '' -or
        $segments -contains '.' -or $segments -contains '..' -or
        -not $seenPaths.Add($entry.path)) {
        throw "Unsafe or duplicate generated-manifest path: '$($entry.path)'."
    }
    $actualPath = Join-Path $generatedRoot $entry.path
    $actualItem = if (Test-Path -LiteralPath $actualPath -PathType Leaf) { Get-Item -LiteralPath $actualPath -Force } else { $null }
    if (-not $actualItem -or ($actualItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -or
        $actualItem.Length -ne $entry.bytes -or
        (Get-FileHash -LiteralPath $actualPath -Algorithm SHA256).Hash -ne $entry.sha256) {
        throw "Generated file no longer matches the immutable first-run manifest: '$($entry.path)'."
    }
}
$extensionGroups = $entries | Group-Object { [System.IO.Path]::GetExtension($_.path).ToLowerInvariant() }
$extensionCounts = @{}
foreach ($group in $extensionGroups) {
    $extensionCounts[$group.Name] = @($group.Group).Count
}
if ($extensionCounts['.cpp'] -ne 62 -or $extensionCounts['.h'] -ne 1 -or
    $extensionCounts['.cmake'] -ne 1) {
    throw 'Generated extension inventory changed from the first force run.'
}
$largestGeneratedFile = ($entries | Sort-Object {[long]$_.bytes} -Descending | Select-Object -First 1)

$rawLines = @(Get-Content -LiteralPath $stderrPath)
if ($rawLines.Count -ne 76) {
    throw "Expected the immutable 76-line force transcript, got $($rawLines.Count) lines."
}
$expectedManifestLine = '  Manifest: ' + $repoRoot.Replace('\', '/') + '/mcla_manifest.toml'
$expectedMountLine = '  Mounted ' + (Join-Path $repoRoot 'private/game') + ' at \Device\Harddisk0\Partition1'
if (@($rawLines | Where-Object { $_ -ceq $expectedManifestLine }).Count -ne 1 -or
    @($rawLines | Where-Object { $_ -ceq $expectedMountLine }).Count -ne 1) {
    throw 'Raw log does not contain the exact expected manifest and private mount paths.'
}

$allowedPatterns = @(
    '^$',
    '^ReXGlue v0\.9\.0 - Xbox 360 Recompilation Toolkit$',
    '^  Manifest: .+/mcla_manifest\.toml$', '^  Project:  mcla$',
    '^  SDK:      0\.9\.0 \(project last generated by 0\.9\.0\)$',
    '^Recompiling mcla$', '^FunctionDispatcher initialized$',
    '^  Mounted .+\\private\\game at \\Device\\Harddisk0\\Partition1$',
    '^Runtime initialized in tool mode \(no GPU\)$', '^Loading XEX image: game:\\default\.xex$',
    "^XThread::Execute thid 1 \(handle=F8000014, 'Kernel Dispatch \(F8000014\)', native=[0-9A-F]+, <host>\)$",
    '^Achievement store: loaded 55 entries$', '^default\.xex$',
    '^  Title ID: 545407F8$', '^  Media ID: 5940C9DB$', '^  Version:  0\.0\.0\.8$',
    '^  Filetime: 2009-07-17 22:23:19 UTC$', '^  start  default\.xex$',
    '^  phase  default\.xex: (Register|Scan|Discover|GapFill|Merge|Validate|Write)$',
    '^Analyze: found 7 errors$', '^=== ANALYSIS ERRORS ===$', '^UnresolvedCall \(7\):$',
    '^  0x[0-9A-F]{8} from 0x[0-9A-F]{8}: b 0x[0-9A-F]{8} from 0x[0-9A-F]{8} - target not in any function$',
    '^Total: 7 errors$',
    "^Analysis errors for 'default' \(continuing due to --force\): Validation failed: 7 unresolved calls$",
    '^Unresolved b target 0x[0-9A-F]{8} from 0x[0-9A-F]{8}$',
    '^Unresolved function 0x[0-9A-F]{8} from 0x[0-9A-F]{8} \(no CallTarget in FunctionNode\)$',
    '^Unexpected float16_4 pack instruction at [0-9A-F]{8}$',
    '^  done   default\.xex \([0-9]+\.[0-9]+s\)$', '^Done in [0-9]+\.[0-9]+s\.$'
)
for ($index = 0; $index -lt $rawLines.Count; $index++) {
    $accepted = $false
    foreach ($pattern in $allowedPatterns) {
        if ($rawLines[$index] -match $pattern) { $accepted = $true; break }
    }
    if (-not $accepted) {
        throw "Unrecognized raw log line $($index + 1); inventory export is fail-closed."
    }
}

$analysisDetails = @($rawLines | Where-Object { $_ -match '^  0x[0-9A-F]{8} from ' })
$unresolvedTargets = @($rawLines | Where-Object { $_ -match '^Unresolved b target ' })
$unresolvedFunctions = @($rawLines | Where-Object { $_ -match '^Unresolved function ' })
$floatPackWarnings = @($rawLines | Where-Object { $_ -match '^Unexpected float16_4 pack instruction at ' })
$phases = @($rawLines | Where-Object { $_ -match '^  phase  default\.xex:' })
if ($analysisDetails.Count -ne 7 -or $unresolvedTargets.Count -ne 7 -or
    $unresolvedFunctions.Count -ne 7 -or $floatPackWarnings.Count -ne 20 -or $phases.Count -ne 7) {
    throw 'Force log category counts do not match the immutable first run.'
}

$repoForward = $repoRoot.Replace('\', '/')
$privateGame = Join-Path $repoRoot 'private/game'
$sanitizedLines = foreach ($line in $rawLines) {
    $line.Replace($repoForward, '[REPO]').Replace($privateGame, '[PRIVATE_GAME_ROOT]') `
        -replace 'native=[0-9A-F]+', 'native=[HOST_THREAD]'
}
$sanitizedText = $sanitizedLines -join "`n"
if ($sanitizedText -match '(?i)(^|[^A-Za-z])[A-Z]:[\\/]|(^|\s)\\\\|\\Users\\|/Users/|native=[0-9A-F]+') {
    throw 'Sanitized force transcript still contains host-specific data.'
}

$report = @(
    '# M2-009 force codegen inventory', '', 'Date: 2026-08-11', 'Result: FORCE EMISSION COMPLETED', '',
    '## Immutable run identity', '',
    '- Command: `rexglue --force codegen mcla_manifest.toml`', '- `--force`: present (`true`)',
    '- ReXGlue: `0.9.0`', "- Exit code: ``$($metadata.exit_code)``",
    "- Duration: ``$($metadata.duration_seconds) s``",
    "- Private stderr SHA-256: ``$stderrHash``", "- Private generated manifest SHA-256: ``$generatedManifestHash``",
    '- M2-008 prerequisite stderr SHA-256: `1D0E23191068D2B92DCC76D24D9D80B0B699111A1C9231494D0EF60ECB5B65BB`', '',
    'The separate M2-008 non-force evidence remains unchanged. Force mode continued after the same seven validation findings and emitted a private generated snapshot.', '',
    '## Complete finding inventory', '',
    '| Requested class | Count | Evidence and interpretation |', '| --- | ---: | --- |',
    '| Analyzer `UnresolvedCall` | 7 | Seven unique direct branch target/source pairs; validation blocker without force. |',
    '| Writer unresolved branch target | 7 | The same seven findings recur during emission with no `CallTarget` in the function graph. |',
    '| Unexpected `float16_4` pack operand shape | 20 | Writer emitted conversion code but warned that operands differ from its expected shape; correctness requires M2-010 triage. |',
    '| Missing jump table | 0 | No `MissingJumpTable` analysis finding or writer diagnostic. |',
    '| Jump target out of bounds | 0 | No `JumpTargetOutOfBounds` analysis finding. |',
    '| Discontinuous function / invalid region | 0 | No `DiscontinuousFunction` finding or invalid-region diagnostic. |',
    '| Unimplemented PPC instruction | 0 | No `UnimplementedInsn` analysis finding; the 20 pack warnings are emitted-but-anomalous, not reported as unimplemented. |',
    '| Exception-handler failure | 0 | No exception-handler diagnostic. Generation of handler wrappers remained disabled by M2-007 policy. |',
    '| Oversized function/file warning | 0 | No large-function or max-file warning. |', '',
    'Counts distinguish seven unique unresolved control-flow findings from their repeated write-stage diagnostics; they are not summed as fourteen defects.', '',
    '## Private generated snapshot', '',
    "- Files: **$($entries.Count)** (62 C++, 1 header, 1 CMake source list)",
    "- Total bytes: **$($generatedManifest.total_bytes)**", "- Largest file: **$($largestGeneratedFile.bytes)** bytes",
    '- Storage: ignored `generated/default/`; no generated file or per-file manifest is tracked', '',
    '## Full sanitized force transcript', '',
    'Host paths and the host-native thread identifier are replaced; complete guest-address diagnostics are retained.', '', '```text'
) + $sanitizedLines + @(
    '```', '', '## Export verification', '',
    '- stream and generated-manifest hashes match private metadata: PASS',
    '- all 64 current generated files match private size/SHA-256 entries: PASS',
    '- exact force command, successful exit, and M2-008 prerequisite: PASS',
    '- fail-closed 76-line grammar and all finding counts: PASS',
    '- host path/native-thread scan: PASS'
)

[System.IO.File]::WriteAllText($resolvedOutput, (($report -join [Environment]::NewLine) + [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))

[pscustomobject]@{
    Exported                  = $true
    UnresolvedCallCount       = $analysisDetails.Count
    FloatPackWarningCount     = $floatPackWarnings.Count
    GeneratedFileCount        = $entries.Count
    GeneratedTotalBytes       = $generatedManifest.total_bytes
    OutputPath                = $resolvedOutput
    OutputSha256              = (Get-FileHash -LiteralPath $resolvedOutput -Algorithm SHA256).Hash
}
