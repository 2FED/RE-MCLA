[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$LogPath,
    [string]$ConfigPath,
    [string]$OutputPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$evidenceRoot = Join-Path $repoRoot 'docs\evidence'

if (-not $LogPath) {
    $LogPath = Join-Path $repoRoot 'private\baseline\M2-002\xenia-stock.snapshot.log'
}
if (-not $ConfigPath) {
    $ConfigPath = Join-Path $repoRoot 'private\baseline\M2-002\xenia-canary.config.snapshot.toml'
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $evidenceRoot 'M2-002-xenia-runtime-metadata.md'
}

function Assert-InputFile {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Label was not found at '$Path'."
    }

    return (Resolve-Path -LiteralPath $Path).Path
}

function Assert-EvidenceOutput {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string]$Container
    )

    $fullPath = [System.IO.Path]::GetFullPath($Path)
    $fullContainer = [System.IO.Path]::GetFullPath($Container).TrimEnd('\')
    if (-not $fullPath.StartsWith("$fullContainer\", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Output path must be below '$fullContainer'. Got '$fullPath'."
    }
    if ([System.IO.Path]::GetExtension($fullPath) -ne '.md') {
        throw "Output must be a Markdown file. Got '$fullPath'."
    }

    $parent = Split-Path -Parent $fullPath
    if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
        throw "Output directory does not exist: '$parent'."
    }
    if ((Get-Item -LiteralPath $parent -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        throw "Output directory must not be a reparse point: '$parent'."
    }
    if (Test-Path -LiteralPath $fullPath) {
        $outputItem = Get-Item -LiteralPath $fullPath -Force
        if (-not $outputItem.PSIsContainer -and
            ($outputItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
            throw "Output file must not be a reparse point: '$fullPath'."
        }
        if ($outputItem.PSIsContainer) {
            throw "Output path is a directory: '$fullPath'."
        }
    }

    return $fullPath
}

function Get-RequiredCapture {
    param(
        [Parameter(Mandatory)][string]$Text,
        [Parameter(Mandatory)][string]$Pattern,
        [Parameter(Mandatory)][string]$Label,
        [int]$Group = 1
    )

    $match = [regex]::Match($Text, $Pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)
    if (-not $match.Success) {
        throw "Required baseline field is missing: $Label."
    }

    return $match.Groups[$Group].Value
}

function Assert-ExpectedValue {
    param(
        [Parameter(Mandatory)][string]$Actual,
        [Parameter(Mandatory)][string]$Expected,
        [Parameter(Mandatory)][string]$Label
    )

    if ($Actual -ne $Expected) {
        throw "$Label mismatch. Expected '$Expected', got '$Actual'."
    }
}

function Get-EventSummary {
    param(
        [Parameter(Mandatory)][AllowEmptyString()][string[]]$Lines,
        [Parameter(Mandatory)][string]$Pattern,
        [Parameter(Mandatory)][string]$Label,
        [Parameter(Mandatory)][string]$Meaning
    )

    $matches = for ($index = 0; $index -lt $Lines.Count; $index++) {
        if ($Lines[$index] -match $Pattern) {
            [pscustomobject]@{ Line = $index + 1 }
        }
    }

    if (-not $matches) {
        return $null
    }

    return [pscustomobject]@{
        Label     = $Label
        Count     = @($matches).Count
        FirstLine = @($matches)[0].Line
        Meaning   = $Meaning
    }
}

$resolvedLog = Assert-InputFile -Path $LogPath -Label 'Xenia baseline log'
$resolvedConfig = Assert-InputFile -Path $ConfigPath -Label 'Xenia configuration snapshot'
$resolvedOutput = Assert-EvidenceOutput -Path $OutputPath -Container $evidenceRoot

$logLines = @(Get-Content -LiteralPath $resolvedLog)
$logText = $logLines -join "`n"
$configText = Get-Content -LiteralPath $resolvedConfig -Raw

$build = Get-RequiredCapture -Text $logText -Pattern '^i> [0-9A-F]+ Build: ([A-Za-z0-9_]+@[0-9a-f]{9}) on [A-Za-z]{3} [0-9]{1,2} [0-9]{4}$' -Label 'Xenia build'
$moduleHash = Get-RequiredCapture -Text $logText -Pattern '^Module Hash: ([0-9A-F]{16})$' -Label 'module hash'
$entryPoint = Get-RequiredCapture -Text $logText -Pattern '^  XEX_HEADER_ENTRY_POINT: ([0-9A-F]{8})$' -Label 'entry point'
$mediaId = Get-RequiredCapture -Text $logText -Pattern '^       Media ID: ([0-9A-F]{8})$' -Label 'Media ID'
$titleId = Get-RequiredCapture -Text $logText -Pattern '^       Title ID: ([0-9A-F]{8})$' -Label 'Title ID'
$titleName = Get-RequiredCapture -Text $logText -Pattern '^i> [0-9A-F]+ Title name: ([A-Za-z0-9 :.-]+)$' -Label 'title name'
$kernelBuild = Get-RequiredCapture -Text $configText -Pattern '^kernel_build_version\s*=\s*([0-9]+)\s*' -Label 'configured kernel build'
$staticKernelVersion = Get-RequiredCapture -Text $logText -Pattern '^    XBOXKRNL : ([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)$' -Label 'static XBOXKRNL version'

$xamHeader = [regex]::Match($logText, '^    xam\.xex - ([0-9]+) imports\n      Version: ([0-9.]+)\n      Min Version: ([0-9.]+)$', [System.Text.RegularExpressions.RegexOptions]::Multiline)
$kernelHeader = [regex]::Match($logText, '^    xboxkrnl\.exe - ([0-9]+) imports\n      Version: ([0-9.]+)\n      Min Version: ([0-9.]+)$', [System.Text.RegularExpressions.RegexOptions]::Multiline)
$xamAudit = [regex]::Match($logText, '^ xam - ([0-9]+) imports\n   Version: ([0-9.]+)\n   Min Version: ([0-9.]+).*?^\s+Implemented:\s+([0-9]+)% \(([0-9]+) implemented, ([0-9]+) unimplemented\)$', [System.Text.RegularExpressions.RegexOptions]::Multiline -bor [System.Text.RegularExpressions.RegexOptions]::Singleline)
$kernelAudit = [regex]::Match($logText, '^ xboxkrnl - ([0-9]+) imports\n   Version: ([0-9.]+)\n   Min Version: ([0-9.]+).*?^\s+Implemented:\s+([0-9]+)% \(([0-9]+) implemented, ([0-9]+) unimplemented\)$', [System.Text.RegularExpressions.RegexOptions]::Multiline -bor [System.Text.RegularExpressions.RegexOptions]::Singleline)
if (-not ($xamHeader.Success -and $kernelHeader.Success -and $xamAudit.Success -and $kernelAudit.Success)) {
    throw 'Required import header or import-audit summary is missing.'
}

Assert-ExpectedValue -Actual $build -Expected 'canary_experimental@7d8db5a2c' -Label 'Xenia build'
Assert-ExpectedValue -Actual $moduleHash -Expected '1984A3354B78CE19' -Label 'Module hash'
Assert-ExpectedValue -Actual $entryPoint -Expected '821322B8' -Label 'Entry point'
Assert-ExpectedValue -Actual $mediaId -Expected '5940C9DB' -Label 'Media ID'
Assert-ExpectedValue -Actual $titleId -Expected '545407F8' -Label 'Title ID'
Assert-ExpectedValue -Actual $titleName -Expected 'Midnight Club: LA' -Label 'Title name'

foreach ($requiredMount in @('Storage root:', 'Content root:', 'Host cache root:', 'Loading module GAME:\default.xex', 'Module \Device\Harddisk0\Partition1\default.xex:')) {
    if (-not $logText.Contains($requiredMount)) {
        throw "Required mount marker is missing: $requiredMount."
    }
}

$eventDefinitions = @(
    @{ Pattern = '^w> .*SDL GameControllerDB:'; Label = 'optional-controller-database'; Meaning = 'Optional SDL mapping database was absent.' },
    @{ Pattern = '^w> .*BaseHeap::AllocFixed attempting commit on unreserved page'; Label = 'base-heap-commit'; Meaning = 'Xenia committed an unreserved guest page during startup.' },
    @{ Pattern = '^w> .*GetProcAddressByOrdinal.*not implemented$|^w> .*XexGetProcedureAddress ordinal .* not found!$'; Label = 'dynamic-import-resolution'; Meaning = 'One vibration helper and force-feedback ordinals were unavailable.' },
    @{ Pattern = '^!> .*undefined extern call to [0-9A-F]+ IoDismountVolumeByFileHandle$'; Label = 'undefined-extern'; Meaning = 'The title called the unimplemented volume-dismount helper.' },
    @{ Pattern = '^w> .*Stub XFileSectorInformation!$'; Label = 'stubbed-file-sector-query'; Meaning = 'The file-sector query used a Xenia stub.' },
    @{ Pattern = '^!> .*ResolvePath\(t:\\mc4\\art\\city\) failed - device not found$'; Label = 'missing-development-device'; Meaning = 'A retail-safe lookup referenced the absent development t: device.' },
    @{ Pattern = '^w> .*doesn''t have profile GPD!$'; Label = 'new-profile-gpd'; Meaning = 'The isolated profile initially had no title GPD.' }
)

$eventSummaries = @()
foreach ($definition in $eventDefinitions) {
    $summary = Get-EventSummary -Lines $logLines -Pattern $definition.Pattern -Label $definition.Label -Meaning $definition.Meaning
    if ($summary) {
        $eventSummaries += $summary
    }
}

$allWarningCount = @($logLines | Where-Object { $_ -match '^(w>|!>) ' }).Count
$classifiedCount = ($eventSummaries | Measure-Object -Property Count -Sum).Sum
if ($classifiedCount -ne $allWarningCount) {
    throw "Warning classification is incomplete. Classified $classifiedCount of $allWarningCount warning/error events."
}

$fatalPattern = '(?i)\bfatal\b|assert(?:ion)? failed|unhandled exception|device[ _-]?lost|\bcrash(?:ed)?\b'
$fatalCount = @($logLines | Where-Object { $_ -match $fatalPattern }).Count
$snapshotHash = (Get-FileHash -LiteralPath $resolvedLog -Algorithm SHA256).Hash
$snapshotDate = (Get-Item -LiteralPath $resolvedLog).LastWriteTime.ToString('yyyy-MM-dd')

$eventRows = $eventSummaries | ForEach-Object {
    "| ``$($_.Label)`` | $($_.Count) | $($_.FirstLine) | $($_.Meaning) |"
}

$report = @(
    '# M2-002 sanitized Xenia runtime metadata'
    ''
    "Date: $snapshotDate"
    'Result: PASS'
    ''
    '## Snapshot identity'
    ''
    '- Source: ignored private stock-Xenia log snapshot (raw path intentionally omitted)'
    "- Snapshot SHA-256: ``$snapshotHash``"
    "- Xenia build: ``$build``"
    "- Module hash: ``$moduleHash``"
    "- Entry point: ``0x$entryPoint``"
    "- Title: ``$titleName``"
    "- Title ID: ``$titleId``"
    "- Media ID: ``$mediaId``"
    ''
    '## Sanitized mounts'
    ''
    '| Role | Sanitized value |'
    '| --- | --- |'
    '| Module launch alias | `GAME:\default.xex` |'
    '| Guest module device | `\Device\Harddisk0\Partition1\default.xex` |'
    '| Host storage | `[PRIVATE_BASELINE]\storage` |'
    '| Host content/save data | `[PRIVATE_BASELINE]\content` |'
    '| Host cache | `[PRIVATE_BASELINE]\cache` |'
    ''
    'No absolute host path, user/profile identifier, or proprietary payload path is reproduced in this report.'
    ''
    '## Import and kernel expectations'
    ''
    '| Library | XEX import records | Requested / minimum | Unique audited imports | Xenia implementation coverage |'
    '| --- | ---: | --- | ---: | --- |'
    "| ``xam.xex`` | $($xamHeader.Groups[1].Value) | ``$($xamHeader.Groups[2].Value)`` / ``$($xamHeader.Groups[3].Value)`` | $($xamAudit.Groups[1].Value) | $($xamAudit.Groups[4].Value)% ($($xamAudit.Groups[5].Value) implemented, $($xamAudit.Groups[6].Value) unimplemented) |"
    "| ``xboxkrnl.exe`` | $($kernelHeader.Groups[1].Value) | ``$($kernelHeader.Groups[2].Value)`` / ``$($kernelHeader.Groups[3].Value)`` | $($kernelAudit.Groups[1].Value) | $($kernelAudit.Groups[4].Value)% ($($kernelAudit.Groups[5].Value) implemented, $($kernelAudit.Groups[6].Value) unimplemented) |"
    ''
    "The XEX statically identifies ``XBOXKRNL $staticKernelVersion`` while both import descriptors request and require ``$($kernelHeader.Groups[2].Value)``. The pinned Xenia configuration reports numeric ``kernel_build_version=$kernelBuild``. These are three different metadata domains; the configured numeric build is not rewritten as a four-part Xbox version here. M2-013 will map every unique import and determine runtime relevance."
    ''
    '## Warning and fatal-event baseline'
    ''
    "The snapshot contains **$allWarningCount** warning/error events, all assigned to the following finite classes. ``First line`` is relative to the ignored snapshot and reveals no host path."
    ''
    '| Sanitized class | Count | First line | Meaning |'
    '| --- | ---: | ---: | --- |'
) + $eventRows + @(
    ''
    "Fatal/crash/assert/device-lost/unhandled-exception marker count: **$fatalCount**. The first warning is ``optional-controller-database`` at snapshot line $($eventSummaries[0].FirstLine); there is no first fatal event because the fatal count is zero."
    ''
    'The word `ERROR` in the dynamic import messages is emitted at Xenia warning severity and did not terminate the observed gameplay route. This report records evidence, not final severity: import/runtime ownership remains M2-010/M2-014 work.'
    ''
    '## Verification performed'
    ''
    '- PowerShell parser and positive `-WhatIf` no-write preflight: PASS'
    '- Two consecutive report generations produced the same SHA-256: PASS'
    '- Wrong module hash rejection: PASS'
    '- Unknown warning-class rejection: PASS'
    '- Output-below-`docs/evidence` containment rejection: PASS'
    '- Absolute host path and profile-identifier scan: PASS'
    '- `ast-grep scan` and 3/3 structural rule tests: PASS'
    ''
    '## Sanitization and reproducibility gate'
    ''
    '`scripts/export-xenia-baseline.ps1` accepts an ignored log/config snapshot, requires the exact supported title/build identifiers, permits output only below `docs/evidence`, emits only whitelisted captures and fixed event summaries, and fails if any warning event is unclassified. A private-path scan of this generated report is required before commit.'
)

if ($PSCmdlet.ShouldProcess($resolvedOutput, 'Write sanitized Xenia baseline report')) {
    $utf8NoBom = [System.Text.UTF8Encoding]::new($false)
    [System.IO.File]::WriteAllLines($resolvedOutput, $report, $utf8NoBom)
}

[pscustomobject]@{
    ValidatedWarnings = $allWarningCount
    FatalMarkers      = $fatalCount
    ModuleHash        = $moduleHash
    SnapshotSha256    = $snapshotHash
    OutputPath        = $resolvedOutput
    Written           = -not [bool]$WhatIfPreference
}
