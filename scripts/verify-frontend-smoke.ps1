[CmdletBinding(DefaultParameterSetName = 'Probe')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$RuntimeLogPath,
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$UserRoot,
    [Parameter(ParameterSetName = 'Probe')][switch]$ProbeOnly,
    [Parameter(ParameterSetName = 'Probe')][switch]$FixtureMode,
    [Parameter(Mandatory, ParameterSetName = 'Result')][string]$ResultPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$utf8 = [Text.UTF8Encoding]::new($false)
$seedRoot = Join-Path $repo 'private\baseline\M4-011\post-oobe-profile'
$referenceRoot = Join-Path $repo 'private\baseline\M4-011\frontend-reference'

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

function Get-LogSet([string]$Current) {
    $current = Resolve-Safe $Current 'Runtime log'
    if ((Split-Path $current -Leaf) -cne 'mcla.log') { throw 'Current log must be mcla.log.' }
    $all = @(Get-ChildItem -LiteralPath (Split-Path $current) -File -Filter 'mcla*.log')
    $rotated = @(); $now = $null
    foreach ($file in $all) {
        if ($file.Name -ceq 'mcla.log') { $now = $file; continue }
        $match = [regex]::Match($file.Name, '^mcla\.([1-9][0-9]*)\.log$')
        if (-not $match.Success) { throw 'Malformed runtime-log rotation.' }
        $rotated += [pscustomobject]@{ Index = [int]$match.Groups[1].Value; File = $file }
    }
    if (-not $now -or $all.Count -gt 16) { throw 'Runtime-log topology is invalid.' }
    $indices = @($rotated | ForEach-Object Index | Sort-Object)
    for ($index = 0; $index -lt $indices.Count; $index++) { if ($indices[$index] -ne $index + 1) { throw 'Runtime-log rotations are not contiguous.' } }
    $ordered = @($rotated | Sort-Object Index -Descending | ForEach-Object File) + $now
    $parts = @(); $manifest = @(); $bytes = 0L
    foreach ($file in $ordered) {
        $bytes += $file.Length; if ($bytes -gt 134217728) { throw 'Runtime logs exceed 128 MiB.' }
        $parts += [IO.File]::ReadAllText($file.FullName)
        $manifest += [ordered]@{ name = $file.Name; bytes = $file.Length; sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
    }
    $manifestJson = ConvertTo-Json -InputObject @($manifest) -Compress -Depth 3
    $sha = [Security.Cryptography.SHA256]::Create(); try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($manifestJson)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
    [pscustomobject]@{ Text = $parts -join "`n"; Files = @($manifest); Count = $manifest.Count; Bytes = $bytes; Hash = $hash }
}

function Get-Bmp([string]$Path) {
    $path = Resolve-Safe $Path 'Frontend BMP'; [byte[]]$bytes = [IO.File]::ReadAllBytes($path)
    if ($bytes.Length -ne 3686454 -or $bytes[0] -ne 0x42 -or $bytes[1] -ne 0x4D -or [BitConverter]::ToInt32($bytes, 18) -ne 1280 -or [Math]::Abs([BitConverter]::ToInt32($bytes, 22)) -ne 720 -or [BitConverter]::ToUInt16($bytes, 28) -ne 32) { throw 'Frontend capture is not the canonical 1280x720 BMP.' }
    $bins = [Collections.Generic.HashSet[int]]::new(); $lumaMin = 255; $lumaMax = 0
    for ($offset = 54; $offset -lt $bytes.Length; $offset += 4) {
        $b = [int]$bytes[$offset]; $g = [int]$bytes[$offset + 1]; $r = [int]$bytes[$offset + 2]
        $null = $bins.Add((($r -shr 3) -shl 10) -bor (($g -shr 3) -shl 5) -bor ($b -shr 3))
        $luma = (54 * $r + 183 * $g + 19 * $b + 128) -shr 8; if ($luma -lt $lumaMin) { $lumaMin = $luma }; if ($luma -gt $lumaMax) { $lumaMax = $luma }
    }
    if ($bins.Count -lt 64 -or ($lumaMax - $lumaMin) -lt 32) { throw 'Frontend capture is trivial.' }
    [pscustomobject]@{ Bytes = $bytes.Length; Sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash; OccupiedBins = $bins.Count; LumaRange = $lumaMax - $lumaMin }
}

function Get-Edge([string]$Path) {
    Add-Type -AssemblyName System.Drawing
    $roi = [Drawing.Rectangle]::new(210, 325, 330, 155)
    $bitmap = [Drawing.Bitmap]::new((Resolve-Safe $Path 'ROI image'))
    try {
        $values = [double[]]::new(($roi.Width - 2) * ($roi.Height - 2)); $index = 0
        for ($y = $roi.Top + 1; $y -lt $roi.Bottom - 1; $y++) { for ($x = $roi.Left + 1; $x -lt $roi.Right - 1; $x++) {
            $left = $bitmap.GetPixel($x - 1, $y); $right = $bitmap.GetPixel($x + 1, $y); $up = $bitmap.GetPixel($x, $y - 1); $down = $bitmap.GetPixel($x, $y + 1)
            $ll = (54 * $left.R + 183 * $left.G + 19 * $left.B + 128) -shr 8; $lr = (54 * $right.R + 183 * $right.G + 19 * $right.B + 128) -shr 8
            $lu = (54 * $up.R + 183 * $up.G + 19 * $up.B + 128) -shr 8; $ld = (54 * $down.R + 183 * $down.G + 19 * $down.B + 128) -shr 8
            $gx = $lr - $ll; $gy = $ld - $lu; $values[$index++] = [Math]::Sqrt($gx * $gx + $gy * $gy)
        } }
        return ,$values
    } finally { $bitmap.Dispose() }
}

function Get-CorrelationPpm([string]$Candidate, [string]$Reference) {
    $left = Get-Edge $Candidate; $right = Get-Edge $Reference
    $leftMean = ($left | Measure-Object -Average).Average; $rightMean = ($right | Measure-Object -Average).Average
    $numerator = 0.0; $leftDenominator = 0.0; $rightDenominator = 0.0
    for ($index = 0; $index -lt $left.Count; $index++) { $x = $left[$index] - $leftMean; $y = $right[$index] - $rightMean; $numerator += $x * $y; $leftDenominator += $x * $x; $rightDenominator += $y * $y }
    [int][Math]::Floor(1000000.0 * $numerator / [Math]::Sqrt($leftDenominator * $rightDenominator))
}

function One([regex]$Regex, [string]$Text, [string]$Name) { $matches = $Regex.Matches($Text); if ($matches.Count -ne 1) { throw "$Name must occur exactly once." }; $matches[0] }

function Get-Probe([string]$Log, [string]$User, [bool]$SkipTitleGate = $false) {
    $user = Resolve-Safe $User 'User root'; $logSet = Get-LogSet $Log; $text = $logSet.Text
    $config = One ([regex]'(?m)^.*MCLA_FRONTEND_SMOKE_CONFIG v=1 slot=0 hold_ms=200 gameplay_wait_seconds=30 intertab_wait_seconds=2\s*$') $text 'Frontend config'
    $inputs = [regex]::Matches($text, '(?m)^.*MCLA_FRONTEND_SMOKE_INPUT v=1 side=(?<side>source|guest) sequence=(?<seq>[1-4]) buttons=(?<buttons>[0-9A-F]{4})\s*$')
    $expected = @(); foreach ($entry in @(@(1, '0010'), @(2, '0010'), @(3, '0200'), @(4, '0200'))) { $expected += @(@('source', $entry[0], $entry[1]), @('guest', $entry[0], $entry[1]), @('source', $entry[0], '0000'), @('guest', $entry[0], '0000')) }
    if ($inputs.Count -ne 16) { throw 'Frontend input record count is not exactly 16.' }
    for ($index = 0; $index -lt $expected.Count; $index++) { if ($inputs[$index].Groups['side'].Value -cne $expected[$index][0] -or [int]$inputs[$index].Groups['seq'].Value -ne $expected[$index][1] -or $inputs[$index].Groups['buttons'].Value -cne $expected[$index][2]) { throw "Frontend input chronology failed at record $index." } }
    $frames = [regex]::Matches($text, '(?m)^.*MCLA_FRONTEND_SMOKE_FRAME v=1 phase=(?<phase>gameplay|pause|options) width=1280 height=720 status=PASS\s*$')
    if ($frames.Count -ne 3 -or ($frames | ForEach-Object { $_.Groups['phase'].Value }) -join ',' -cne 'gameplay,pause,options') { throw 'Frontend frame chronology failed.' }
    $summary = One ([regex]'(?m)^.*MCLA_FRONTEND_SMOKE_SUMMARY v=1 status=PASS pulses=4 source_records=8 guest_records=8 frames=3 gameplay=1 pause=1 options=1 external_close_required=1\s*$') $text 'Frontend summary'
    $title = One ([regex]'(?m)^.*MCLA graphics: nontrivial guest frame captured 1280x720,.*$') $text 'Title capture'
    $close = One ([regex]'(?m)^.*Window closing, shutting down\.\.\.\s*$') $text 'WM_CLOSE'; $complete = One ([regex]'(?m)^.*Execution complete\s*$') $text 'Execution complete'; $hard = One ([regex]'(?m)^.*Title terminated; hard-exiting process\.\s*$') $text 'Hard exit'
    if (-not ($config.Index -lt $title.Index -and $title.Index -lt $inputs[0].Index -and $inputs[3].Index -lt $frames[0].Index -and $frames[0].Index -lt $inputs[4].Index -and $inputs[7].Index -lt $frames[1].Index -and $frames[1].Index -lt $inputs[8].Index -and $inputs[15].Index -lt $frames[2].Index -and $frames[2].Index -lt $summary.Index -and $summary.Index -lt $close.Index -and $close.Index -lt $complete.Index -and $complete.Index -lt $hard.Index)) { throw 'Frontend/lifecycle chronology failed.' }
    if ($text -match '(?i)(REX_GUEST_CRASH|GUEST_CRASH_REPORT|PPC_UNIMPLEMENTED|\[fatal\]|assertion failed|D3D12.*device (?:lost|removed))') { throw 'Fatal or unsupported marker found.' }
    $paths = [ordered]@{ title = Join-Path $user 'mcla-first-frame.bmp'; gameplay = Join-Path $user 'mcla-frontend-gameplay.bmp'; pause = Join-Path $user 'mcla-frontend-pause.bmp'; options = Join-Path $user 'mcla-frontend-options.bmp' }
    $bmps = [ordered]@{}; foreach ($name in $paths.Keys) { $bmps[$name] = Get-Bmp $paths[$name] }
    if (@($bmps.Values | ForEach-Object Sha256 | Sort-Object -Unique).Count -ne 4) { throw 'Frontend captures are not four distinct frames.' }
    $pausePpm = Get-CorrelationPpm $paths.pause (Join-Path $referenceRoot 'pause.bmp'); $optionsPpm = Get-CorrelationPpm $paths.options (Join-Path $referenceRoot 'options.bmp')
    if ($pausePpm -lt 550000 -or $optionsPpm -lt 550000) { throw 'Pause/options menu ROI classification failed.' }
    if (-not $SkipTitleGate) { $null = & (Join-Path $PSScriptRoot 'verify-render-path-smoke.ps1') -ProbeOnly -RuntimeLogPath $Log -BmpPath $paths.title }
    [pscustomobject]@{ Passed = $true; LogSet = $logSet; Bmps = $bmps; PauseCorrelationPpm = $pausePpm; OptionsCorrelationPpm = $optionsPpm }
}

if ($PSCmdlet.ParameterSetName -eq 'Probe') { if (-not $ProbeOnly) { throw 'Probe mode requires -ProbeOnly.' }; Get-Probe $RuntimeLogPath $UserRoot $FixtureMode.IsPresent; return }

$result = Resolve-Safe $ResultPath 'Result'; $json = [IO.File]::ReadAllText($result)
if ($json -match '(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])') { throw 'Result contains a private or absolute path.' }
$record = $json | ConvertFrom-Json
if ($record.schema -ne 1 -or $record.task -cne 'M4-011' -or $record.decision -cne 'saved-gameplay-pause-options-external-exit-pass' -or $record.sdk_version -cne '0.9.0.18' -or $record.cycle_count -ne 3 -or $record.route -cne 'startup-title-START-saved-gameplay-START-pause-RB-modes-RB-settings-options-external-WM_CLOSE' -or $record.internal_exit_claimed -ne $false -or $record.external_wm_close_verified -ne $true -or $record.data_integrity_preserved -ne $true -or $record.no_surviving_processes -ne $true) { throw 'Result identity/scope failed.' }
$buildFields = @($record.build.focused_test_cases, $record.build.focused_test_assertions)
if ($buildFields[0] -ne 2 -or $buildFields[1] -ne 33) { throw 'Focused VFS test totals mismatch.' }
$root = Split-Path $result; $seed = Get-Tree $seedRoot
if ($record.seed.tree_sha256 -cne $seed.Hash -or $record.seed.file_count -ne 2 -or $seed.FileCount -ne 2) { throw 'Pinned post-OOBE seed mismatch.' }
if ((Get-FileHash (Join-Path $seedRoot 'B13EBABEBABEBABE\545407F8\00000001\mc4.sav\mc4.sav') -Algorithm SHA256).Hash -cne 'E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68' -or (Get-FileHash (Join-Path $seedRoot 'B13EBABEBABEBABE\545407F8\Headers\00000001\mc4.sav.header') -Algorithm SHA256).Hash -cne '1DAEF55FA78BDD3F1AA75A36772B801C6C714B6D1A115A97904F3D1C957BC4C9') { throw 'Pinned seed file hash mismatch.' }
if (@($record.cycles).Count -ne 3) { throw 'Cycle count mismatch.' }
for ($index = 0; $index -lt 3; $index++) {
    $name = '{0:D2}' -f ($index + 1); $cycleRoot = Resolve-Safe (Join-Path $root "runs\$name") 'Cycle'; $probe = Get-Probe (Join-Path $cycleRoot 'mcla.log') (Join-Path $cycleRoot 'user'); $cycle = $record.cycles[$index]
    if ($cycle.index -ne $index + 1 -or $cycle.exit_code -ne 0 -or $cycle.close_requested -ne $true -or $cycle.harness_force_cleanup -ne $false -or $cycle.runtime_log_set_sha256 -cne $probe.LogSet.Hash -or $cycle.pause_menu_correlation_ppm -ne $probe.PauseCorrelationPpm -or $cycle.options_menu_correlation_ppm -ne $probe.OptionsCorrelationPpm) { throw 'Cycle/result mismatch.' }
    foreach ($phase in @('title', 'gameplay', 'pause', 'options')) { if ($cycle.captures.$phase.sha256 -cne $probe.Bmps[$phase].Sha256 -or $cycle.captures.$phase.bytes -ne $probe.Bmps[$phase].Bytes) { throw 'Capture/result mismatch.' } }
    if (@($cycle.runtime_logs).Count -ne $probe.LogSet.Count) { throw 'Runtime-log manifest count mismatch.' }
    for ($logIndex = 0; $logIndex -lt $probe.LogSet.Count; $logIndex++) { foreach ($field in @('name', 'bytes', 'sha256')) { if ($cycle.runtime_logs[$logIndex].$field -cne $probe.LogSet.Files[$logIndex].$field) { throw 'Runtime-log manifest mismatch.' } } }
    $tree = Get-Tree $cycleRoot; if ($cycle.cycle_tree_sha256 -cne $tree.Hash -or $cycle.cycle_file_count -ne $tree.FileCount -or $cycle.cycle_bytes -ne $tree.Bytes) { throw 'Cycle tree mismatch.' }
}
if (($record.game_identity.before | ConvertTo-Json -Compress) -cne ($record.game_identity.after | ConvertTo-Json -Compress) -or ($record.artifacts.before | ConvertTo-Json -Compress) -cne ($record.artifacts.after | ConvertTo-Json -Compress)) { throw 'Game/runtime identity changed.' }
$canonicalGame = Get-Tree (Join-Path $repo 'private\game'); $manifest = & (Join-Path $PSScriptRoot 'verify-game-manifest.ps1') -GamePath (Join-Path $repo 'private\game') -VerifyHashes; $gameAfter = $record.game_identity.after
if ($gameAfter.tree_sha256 -cne $canonicalGame.Hash -or $gameAfter.tree_file_count -ne $canonicalGame.FileCount -or $gameAfter.tree_bytes -ne $canonicalGame.Bytes -or $gameAfter.file_count -ne $manifest.FileCount -or $gameAfter.payload_bytes -ne $manifest.PayloadBytes -or $gameAfter.manifest_sha256 -cne (Get-FileHash $manifest.ManifestPath -Algorithm SHA256).Hash) { throw 'Canonical source-game physical identity mismatch.' }
$canonicalBuild = Resolve-Safe (Join-Path $repo 'out\build\win-amd64-relwithdebinfo') 'Canonical build'
if (@($record.artifacts.after).Count -ne 4) { throw 'Runtime artifact count mismatch.' }
foreach ($artifact in $record.artifacts.after) { if ($artifact.name -notin @('mcla.exe', 'rexruntimerd.dll', 'TracyClientrd.dll', 'rexgpu-xenosrd.dll') -or $artifact.sha256 -cne (Get-FileHash (Resolve-Safe (Join-Path $canonicalBuild $artifact.name) 'Runtime artifact') -Algorithm SHA256).Hash) { throw 'Canonical runtime artifact mismatch.' } }
foreach ($pair in @(@('sdk-install.log', 'sdk_install_log_sha256'), @('sdk-vfs-test.log', 'focused_test_log_sha256'), @('relwithdebinfo-clean-build.log', 'app_build_log_sha256'))) { if ((Get-FileHash (Resolve-Safe (Join-Path $root $pair[0]) 'Build log') -Algorithm SHA256).Hash -cne $record.build.($pair[1])) { throw 'Build-log hash mismatch.' } }
$canonicalExe = (Resolve-Safe (Join-Path $canonicalBuild 'mcla.exe') 'Canonical executable').ToLowerInvariant()
$survivors = @(Get-Process -Name mcla -ErrorAction SilentlyContinue | Where-Object { try { $_.Path -and ([IO.Path]::GetFullPath($_.Path).ToLowerInvariant() -ceq $canonicalExe) } catch { $false } })
if ($survivors.Count -ne 0) { throw 'Canonical MCLA process is still running.' }
[pscustomobject]@{ Passed = $true; Decision = $record.decision; Cycles = 3; SavedGameplayVerified = $true; PauseVerified = $true; OptionsVerified = $true; ExternalWmCloseVerified = $true; DataIntegrityVerified = $true }
