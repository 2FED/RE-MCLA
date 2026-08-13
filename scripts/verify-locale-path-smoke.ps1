[CmdletBinding(DefaultParameterSetName = 'Probe')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$RuntimeLogPath,
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$BmpPath,
    [Parameter(Mandatory, ParameterSetName = 'Probe')][ValidateSet(1, 4, 12)][uint32]$Language,
    [Parameter(Mandatory, ParameterSetName = 'Probe')][ValidateSet(34, 88, 103)][uint32]$Country,
    [Parameter(ParameterSetName = 'Probe')][switch]$ProbeOnly,
    [Parameter(ParameterSetName = 'Probe')][switch]$LocaleOnly,
    [Parameter(Mandatory, ParameterSetName = 'Result')][string]$ResultPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$utf8 = [Text.UTF8Encoding]::new($false)

function Resolve-Safe([string]$Path, [string]$Description) {
    $full = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($Path)) { $Path } else { Join-Path $repo $Path }))
    $prefix = $repo.TrimEnd('\') + '\'
    if (-not $full.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw "$Description escapes repository." }
    $current = $repo
    foreach ($part in @($full.Substring($prefix.Length).Split('\') | Where-Object Length)) {
        $current = Join-Path $current $part
        if (-not (Test-Path -LiteralPath $current)) { throw "$Description is missing." }
        if ((Get-Item -LiteralPath $current -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "$Description traverses a reparse point." }
    }
    $full
}

function Assert-Tree([string]$Root) {
    $pending = [Collections.Generic.Stack[string]]::new(); $pending.Push($Root)
    while ($pending.Count) {
        foreach ($item in @(Get-ChildItem -LiteralPath $pending.Pop() -Force)) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Tree contains a reparse point.' }
            if ($item.PSIsContainer) { $pending.Push($item.FullName) }
        }
    }
}

function Get-Tree([string]$Root) {
    $root = Resolve-Safe $Root 'Tree'; Assert-Tree $root
    $items = @(Get-ChildItem -LiteralPath $root -Recurse -Force)
    $files = @($items | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)
    $entries = @()
    foreach ($directory in @($items | Where-Object PSIsContainer | Sort-Object FullName)) { $entries += [ordered]@{ kind = 'directory'; path = $directory.FullName.Substring($root.Length).TrimStart('\').Replace('\', '/') } }
    foreach ($file in $files) { $entries += [ordered]@{ kind = 'file'; path = $file.FullName.Substring($root.Length).TrimStart('\').Replace('\', '/'); length = $file.Length; sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash } }
    $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4
    $sha = [Security.Cryptography.SHA256]::Create(); try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
    $bytes = 0L; foreach ($file in $files) { $bytes += $file.Length }
    [pscustomobject]@{ Hash = $hash; FileCount = $files.Count; DirectoryCount = @($items | Where-Object PSIsContainer).Count; Bytes = $bytes }
}

function Read-Utf8Shared([string]$Path) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try { $reader = [IO.StreamReader]::new($stream, $utf8, $true, 65536, $false); try { $reader.ReadToEnd() } finally { $reader.Dispose() } } finally { $stream.Dispose() }
}

function Get-LogSet([string]$Current) {
    $current = Resolve-Safe $Current 'Runtime log'
    if ((Split-Path $current -Leaf) -cne 'mcla.log') { throw 'Current log must be mcla.log.' }
    $directory = Split-Path $current
    $files = @(Get-ChildItem -LiteralPath $directory -File -Filter 'mcla*.log')
    $rotated = @(); $now = $null
    foreach ($file in $files) {
        if ($file.Name -ceq 'mcla.log') { $now = $file; continue }
        $match = [regex]::Match($file.Name, '^mcla\.([1-9][0-9]*)\.log$')
        if (-not $match.Success) { throw 'Malformed log rotation.' }
        $rotated += [pscustomobject]@{ Number = [int]$match.Groups[1].Value; File = $file }
    }
    if (-not $now -or $files.Count -gt 16) { throw 'Runtime log topology is invalid.' }
    $ids = @($rotated | ForEach-Object Number | Sort-Object)
    for ($i = 0; $i -lt $ids.Count; $i++) { if ($ids[$i] -ne $i + 1) { throw 'Log rotations are not contiguous.' } }
    $ordered = @($rotated | Sort-Object Number -Descending | ForEach-Object File) + $now
    $parts = @(); $manifest = @(); $bytes = 0L
    foreach ($file in $ordered) {
        $bytes += $file.Length; if ($bytes -gt 134217728) { throw 'Log set exceeds 128 MiB.' }
        $parts += Read-Utf8Shared $file.FullName
        $manifest += [ordered]@{ name = $file.Name; bytes = $file.Length; sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
    }
    $json = ConvertTo-Json -InputObject @($manifest) -Compress -Depth 3
    $sha = [Security.Cryptography.SHA256]::Create(); try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
    [pscustomobject]@{ Text = $parts -join "`n"; Files = @($manifest); Count = $manifest.Count; Bytes = $bytes; Hash = $hash }
}

function One([regex]$Regex, [string]$Text, [string]$Name) {
    $matches = $Regex.Matches($Text); if ($matches.Count -ne 1) { throw "$Name must occur exactly once." }; $matches[0]
}

function Get-Bmp([string]$Path) {
    $path = Resolve-Safe $Path 'BMP'; $bytes = [IO.File]::ReadAllBytes($path)
    if ($bytes.Length -ne 3686454 -or $bytes[0] -ne 0x42 -or $bytes[1] -ne 0x4D -or [BitConverter]::ToInt32($bytes, 18) -ne 1280 -or [BitConverter]::ToInt32($bytes, 22) -ne 720 -or [BitConverter]::ToUInt16($bytes, 28) -ne 32) { throw 'Capture is not canonical BMP.' }
    [pscustomobject]@{ Sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash; Bytes = $bytes.Length }
}

function From-CodePoints([int[]]$CodePoints) { -join @($CodePoints | ForEach-Object { [char]$_ }) }

function Get-LocalizedPromptReference([uint32]$ExpectedLanguage) {
    $reference = switch ($ExpectedLanguage) {
        4 { [pscustomobject]@{ Path = 'private/baseline/M4-010/fr-title-reference.bmp'; Sha256 = 'EF1EC173D0810A145273006D9E68967781A43DAA4A66E679FC9580612AFA9C7D' } }
        default { return $null }
    }
    [pscustomobject]@{
        Path = Resolve-Safe $reference.Path 'Localized title reference'
        Sha256 = $reference.Sha256
    }
}

function Get-Probe([string]$Log, [string]$Bmp, [uint32]$ExpectedLanguage, [uint32]$ExpectedCountry, [bool]$SkipTitle) {
    $logSet = Get-LogSet $Log; $text = $logSet.Text
    $config = One ([regex]"(?m)^.*LOCALE_AUDIT_CONFIG v=1 enabled=1 language=$ExpectedLanguage country=$ExpectedCountry language_valid=1 country_valid=1 record_limit=3\s*$") $text 'Config'
    $xconfig = One ([regex]"(?m)^.*LOCALE_AUDIT_XCONFIG v=1 setting=language value=$ExpectedLanguage result=00000000 match=1\s*$") $text 'Language XConfig record'
    $xget = [regex]::Matches($text, '(?m)^.*LOCALE_AUDIT_XGET v=1 .*$')
    $country = [regex]::Matches($text, '(?m)^.*LOCALE_AUDIT_XCONFIG v=1 setting=country .*$')
    $summary = One ([regex]"(?m)^.*LOCALE_AUDIT_SUMMARY v=1 phase=title status=PASS language=$ExpectedLanguage country=$ExpectedCountry xget_reach=unreached xget_calls=0 xget_matches=0 xconfig_language_calls=(?<language_calls>[1-9][0-9]*) xconfig_language_value_calls=(?<language_values>[1-9][0-9]*) xconfig_country_reach=unreached xconfig_country_calls=0 xconfig_country_value_calls=0 xconfig_failures=0 mismatches=0 records=1 overflow=0 dropped_records=0\s*$") $text 'Summary'
    $capture = One ([regex]'(?m)^.*MCLA graphics: nontrivial guest frame captured 1280x720,.*$') $text 'Capture marker'
    $project = One ([regex]'(?m)^.*MCLA locale: title route summarized\s*$') $text 'Project marker'
    $close = One ([regex]'(?m)^.*Window closing, shutting down\.\.\.\s*$') $text 'Close'
    $complete = One ([regex]'(?m)^.*Execution complete\s*$') $text 'Complete'
    $hard = One ([regex]'(?m)^.*Title terminated; hard-exiting process\.\s*$') $text 'Hard exit'
    foreach ($marker in @('MCLA VFS: game: and d: resolve 3/3 expected disc files on \Device\Harddisk0\Partition1', 'MCLA VFS: root-escape paths rejected', 'MCLA VFS: write, create, delete, and writable-map requests denied')) { $null = One ([regex]("(?m)^.*" + [regex]::Escape($marker) + "\s*$")) $text $marker }
    if ($xget.Count -or $country.Count) { throw 'Title route unexpectedly reached an unclaimed locale API.' }
    $calls = [uint64]$summary.Groups['language_calls'].Value; $values = [uint64]$summary.Groups['language_values'].Value
    if ($calls -ne $values -or -not ($config.Index -lt $xconfig.Index -and $xconfig.Index -lt $capture.Index -and $capture.Index -lt $summary.Index -and $summary.Index -lt $project.Index -and $project.Index -lt $close.Index -and $close.Index -lt $complete.Index -and $complete.Index -lt $hard.Index)) { throw 'Locale accounting or chronology failed.' }
    if ($text -match '(?i)(PPC_UNIMPLEMENTED|GUEST_CRASH_REPORT|\[fatal\]|D3D12.*device (?:lost|removed)|LOCALE_AUDIT_.*status=FAIL)') { throw 'Fatal, device-loss, or failed locale marker found.' }
    $visual = $null
    if (-not $SkipTitle) {
        $renderArguments = @{
            ProbeOnly = $true
            RuntimeLogPath = $Log
            BmpPath = $Bmp
        }
        $localizedReference = Get-LocalizedPromptReference $ExpectedLanguage
        if ($localizedReference) {
            $renderArguments.LocalizedPrompt = $true
            $renderArguments.LocalizedPromptReferencePath = $localizedReference.Path
            $renderArguments.LocalizedPromptReferenceSha256 = $localizedReference.Sha256
        }
        $renderProbe = & (Join-Path $PSScriptRoot 'verify-render-path-smoke.ps1') @renderArguments
        $visual = [pscustomobject]@{
            Mode = $(if ($localizedReference) { 'localized-prompt' } else { 'english-reference' })
            LogoCorrelationPpm = $renderProbe.Roi.LogoCorrelationPpm
            PressCorrelationPpm = $renderProbe.Roi.PressCorrelationPpm
            PromptReferenceSha256 = $(if ($localizedReference) { $localizedReference.Sha256 } else { '' })
            PromptCorrelationPpm = $renderProbe.Roi.LocalizedPromptCorrelationPpm
        }
    }
    [pscustomobject]@{ Passed = $true; Language = $ExpectedLanguage; Country = $ExpectedCountry; LanguageCalls = $calls; XGetLanguageReached = $false; CountryReached = $false; LogSet = $logSet; Bmp = Get-Bmp $Bmp; Visual = $visual }
}

if ($PSCmdlet.ParameterSetName -eq 'Probe') {
    if (-not $ProbeOnly) { throw 'Probe requires -ProbeOnly.' }
    Get-Probe $RuntimeLogPath $BmpPath $Language $Country $LocaleOnly.IsPresent
    return
}

$result = Resolve-Safe $ResultPath 'Result'
$json = [IO.File]::ReadAllText($result, $utf8)
if ($json -match '(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])') { throw 'Result contains a private or absolute path.' }
$record = $json | ConvertFrom-Json
if ($record.schema -ne 1 -or $record.task -cne 'M4-010' -or $record.decision -cne 'locale-selection-unicode-path-title-matrix-pass' -or $record.sdk_version -cne '0.9.0.18' -or $record.frontend_title_reached -ne $true -or $record.xget_language_title_reached -ne $false -or $record.country_title_reached -ne $false -or $record.data_integrity_preserved -ne $true -or $record.no_surviving_processes -ne $true) { throw 'Result identity or scope failed.' }
if (@($record.cycles).Count -ne 3) { throw 'Locale matrix must contain exactly three cycles.' }
$expected = @(@(1, 103, '01-en-us'), @(4, 34, '02-fr-fr'), @(12, 88, '03-ru-ru'))
$root = Split-Path $result
$localWord = From-CodePoints @(0x041B, 0x043E, 0x043A, 0x0430, 0x043B, 0x044C); $cacheWord = From-CodePoints @(0x043A, 0x0438, 0x0457, 0x0432); $accent = [char]0x00E9
for ($i = 0; $i -lt 3; $i++) {
    $cycleRecord = $record.cycles[$i]; $tuple = $expected[$i]
    if ([uint32]$cycleRecord.language -ne $tuple[0] -or [uint32]$cycleRecord.country -ne $tuple[1] -or $cycleRecord.name -cne $tuple[2] -or $cycleRecord.exit_code -ne 0 -or $cycleRecord.harness_force_cleanup -ne $false -or $cycleRecord.unicode_user_path_exact -ne $true -or $cycleRecord.unicode_cache_path_exact -ne $true -or $cycleRecord.unicode_log_path_exact -ne $true) { throw 'Locale cycle identity failed.' }
    $cycle = Resolve-Safe (Join-Path $root ('runs\' + $tuple[2])) 'Cycle'
    $expectedNames = @(('user-' + $localWord + '-' + $accent), ('cache-' + $cacheWord), ('logs-' + $localWord))
    $directories = @(Get-ChildItem -LiteralPath $cycle -Directory)
    $actualDirectoryNames = [string]::Join("`n", @($directories.Name | Sort-Object))
    $expectedDirectoryNames = [string]::Join("`n", @($expectedNames | Sort-Object))
    if ($directories.Count -ne 3 -or $actualDirectoryNames -cne $expectedDirectoryNames) { throw 'Exact Unicode cycle topology failed.' }
    $user = Join-Path $cycle $expectedNames[0]; $logs = Join-Path $cycle $expectedNames[2]
    $probe = Get-Probe (Join-Path $logs 'mcla.log') (Join-Path $user 'mcla-first-frame.bmp') $tuple[0] $tuple[1] $false
    if ($cycleRecord.runtime_log_set_sha256 -cne $probe.LogSet.Hash -or $cycleRecord.capture_sha256 -cne $probe.Bmp.Sha256 -or [uint64]$cycleRecord.language_calls -ne $probe.LanguageCalls -or @($cycleRecord.runtime_logs).Count -ne $probe.LogSet.Count -or $cycleRecord.title_visual_mode -cne $probe.Visual.Mode -or [int]$cycleRecord.logo_edge_correlation_ppm -ne $probe.Visual.LogoCorrelationPpm -or $cycleRecord.prompt_reference_sha256 -cne $probe.Visual.PromptReferenceSha256 -or [int]$cycleRecord.prompt_edge_correlation_ppm -ne $probe.Visual.PromptCorrelationPpm) { throw 'Locale result/physical probe mismatch.' }
    for ($j = 0; $j -lt $probe.LogSet.Count; $j++) { foreach ($field in @('name', 'bytes', 'sha256')) { if ($cycleRecord.runtime_logs[$j].$field -cne $probe.LogSet.Files[$j].$field) { throw 'Runtime log manifest mismatch.' } } }
    $tree = Get-Tree $cycle
    if ($cycleRecord.cycle_tree_sha256 -cne $tree.Hash -or [uint64]$cycleRecord.cycle_file_count -ne $tree.FileCount -or [uint64]$cycleRecord.cycle_bytes -ne $tree.Bytes) { throw 'Cycle tree mismatch.' }
}
if ($record.build.filesystem_test_cases -ne 3 -or $record.build.filesystem_test_assertions -ne 32 -or $record.build.locale_test_cases -ne 3 -or $record.build.locale_test_assertions -ne 22) { throw 'Focused test totals changed.' }
foreach ($pair in @(@('sdk-install.log', 'sdk_install_log_sha256'), @('sdk-locale-path-test.log', 'focused_test_log_sha256'), @('relwithdebinfo-clean-build.log', 'app_build_log_sha256'))) { $path = Resolve-Safe (Join-Path $root $pair[0]) 'Build log'; if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $record.build.($pair[1])) { throw 'Build log mismatch.' } }
if (($record.game_identity.before | ConvertTo-Json -Compress) -cne ($record.game_identity.after | ConvertTo-Json -Compress)) { throw 'Source game changed.' }
$game = Resolve-Safe 'private\game' 'Canonical game'; $gameTree = Get-Tree $game; $gameVerified = & (Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath $game -VerifyHashes; $gameExpected = $record.game_identity.after
if ($gameExpected.manifest_sha256 -cne (Get-FileHash -LiteralPath $gameVerified.ManifestPath -Algorithm SHA256).Hash -or $gameExpected.tree_sha256 -cne $gameTree.Hash -or [uint64]$gameExpected.file_count -ne $gameVerified.FileCount -or [uint64]$gameExpected.payload_bytes -ne $gameVerified.PayloadBytes) { throw 'Physical source-game identity mismatch.' }
if (($record.artifacts.before | ConvertTo-Json -Compress) -cne ($record.artifacts.after | ConvertTo-Json -Compress) -or @($record.artifacts.after).Count -ne 4) { throw 'Runtime artifact set changed.' }
$build = Resolve-Safe 'out\build\win-amd64-relwithdebinfo' 'Build'; foreach ($artifact in $record.artifacts.after) { if ($artifact.name -notin @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll') -or (Get-FileHash -LiteralPath (Resolve-Safe (Join-Path $build $artifact.name) 'Artifact') -Algorithm SHA256).Hash -cne $artifact.sha256) { throw 'Runtime artifact mismatch.' } }
$executable = Join-Path $build 'mcla.exe'; if (@((Get-Process mcla -ErrorAction SilentlyContinue) | Where-Object { try { [string]::Equals($_.Path, $executable, [StringComparison]::OrdinalIgnoreCase) } catch { $false } }).Count) { throw 'Canonical MCLA process still exists.' }
[pscustomobject]@{ Passed = $true; Decision = $record.decision; Cycles = 3; Languages = '1,4,12'; UnicodePathsVerified = $true; XGetLanguageTitleReached = $false; CountryTitleReached = $false; DataIntegrityVerified = $true }
