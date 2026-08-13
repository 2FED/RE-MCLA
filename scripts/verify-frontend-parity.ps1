[CmdletBinding(DefaultParameterSetName = 'Result')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$RuntimeLogPath,
    [Parameter(Mandatory, ParameterSetName = 'Probe')][string]$UserRoot,
    [Parameter(Mandatory, ParameterSetName = 'Probe')][switch]$ProbeOnly,
    [Parameter(Mandatory, ParameterSetName = 'Result')][string]$ResultPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$utf8 = [Text.UTF8Encoding]::new($false)
$m4011Root = Join-Path $repo 'private\evidence\M4-011\20260813-230502-1f0de720'
$m4007Root = Join-Path $repo 'private\evidence\M4-007\20260813-170202-44d2c7d8'
$xeniaRoot = Join-Path $repo 'private\tools\xenia-canary\artifacts\screenshots\545407F8'
$xeniaLog = Join-Path $repo 'private\baseline\M2-001\xenia-resume-20260811-005919.log'
$expected = [ordered]@{
    m4011 = 'FAD092EAF957BA727053859792040943D93FDFA54CCA5990F6E612F855FAA131'
    m4007 = 'A55CD1CAED7063CC811BB5F45EAD52B6DB971F8A80BF94C61F534B6BCA9F0A7A'
    xenia_log = '0A7E5418E3722192E1878C01E671E8559351304EB192A70D88028755D24C4D0D'
    title = '7F0293842A6AA30EF0B0EA7C7954FF5130A03ECF6E3A112EEFCAA4A6B11C613E'
    pause = 'EA85BD3AECFAD647819682D19E1951097A22F59360500ACD70A0DAF4DC80BFE2'
    gameplay = 'A490553067A8F375191F618C34556B82C9FD244FF295519AB4383AAB40434F8B'
}

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

function Get-Tree([string]$Root) {
    $root = Resolve-Safe $Root 'Tree'; $items = @(Get-ChildItem -LiteralPath $root -Recurse -Force)
    foreach ($item in @((Get-Item -LiteralPath $root -Force)) + $items) { if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Tree contains a reparse point.' } }
    $files = @($items | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName); $entries = @(); $bytes = 0L
    foreach ($directory in @($items | Where-Object PSIsContainer | Sort-Object FullName)) { $entries += [ordered]@{ kind = 'directory'; path = $directory.FullName.Substring($root.Length).TrimStart('\').Replace('\', '/') } }
    foreach ($file in $files) { $bytes += $file.Length; $entries += [ordered]@{ kind = 'file'; path = $file.FullName.Substring($root.Length).TrimStart('\').Replace('\', '/'); length = $file.Length; sha256 = (Get-FileHash $file.FullName -Algorithm SHA256).Hash } }
    $json = ConvertTo-Json -InputObject @($entries) -Compress -Depth 4; $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
    [pscustomobject]@{ Hash = $hash; FileCount = $files.Count; DirectoryCount = @($items | Where-Object PSIsContainer).Count; Bytes = $bytes }
}

function Get-LogSet([string]$Current) {
    $current = Resolve-Safe $Current 'Runtime log'; if ((Split-Path $current -Leaf) -cne 'mcla.log') { throw 'Current runtime log must be mcla.log.' }
    $all = @(Get-ChildItem -LiteralPath (Split-Path $current) -File -Filter 'mcla*.log'); $rotated = @(); $now = $null
    foreach ($file in $all) { if ($file.Name -ceq 'mcla.log') { $now = $file; continue }; $m = [regex]::Match($file.Name, '^mcla\.([1-9][0-9]*)\.log$'); if (-not $m.Success) { throw 'Malformed log rotation.' }; $rotated += [pscustomobject]@{ Index = [int]$m.Groups[1].Value; File = $file } }
    if (-not $now -or $all.Count -gt 16) { throw 'Runtime-log topology is invalid.' }; $ids = @($rotated | ForEach-Object Index | Sort-Object)
    for ($i = 0; $i -lt $ids.Count; $i++) { if ($ids[$i] -ne $i + 1) { throw 'Runtime-log rotations are not contiguous.' } }
    $ordered = @($rotated | Sort-Object Index -Descending | ForEach-Object File) + $now; $parts = @(); $manifest = @(); $bytes = 0L
    foreach ($file in $ordered) { $bytes += $file.Length; if ($bytes -gt 134217728) { throw 'Runtime logs exceed 128 MiB.' }; $parts += [IO.File]::ReadAllText($file.FullName); $manifest += [ordered]@{ name = $file.Name; bytes = $file.Length; sha256 = (Get-FileHash $file.FullName -Algorithm SHA256).Hash } }
    $json = ConvertTo-Json -InputObject @($manifest) -Compress -Depth 3; $sha = [Security.Cryptography.SHA256]::Create()
    try { $hash = -join ($sha.ComputeHash($utf8.GetBytes($json)) | ForEach-Object { $_.ToString('X2') }) } finally { $sha.Dispose() }
    [pscustomobject]@{ Text = $parts -join "`n"; Files = @($manifest); Count = $manifest.Count; Bytes = $bytes; Hash = $hash }
}

function Get-Bitmap([string]$Path, [int]$Width, [int]$Height) {
    $path = Resolve-Safe $Path 'Comparison image'; $image = [Drawing.Image]::FromFile($path)
    try {
        if ($image.Width -ne $Width -or $image.Height -ne $Height) { throw "Comparison image must be ${Width}x${Height}." }
        if ($image.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Bmp.Guid) { throw 'Comparison image must be BMP.' }
    } finally { $image.Dispose() }
    [pscustomobject]@{ Path = $path; Sha256 = (Get-FileHash $path -Algorithm SHA256).Hash; Bytes = (Get-Item $path).Length; Width = $Width; Height = $Height }
}

function Get-Edge([string]$Path, [Drawing.Rectangle]$Roi) {
    $source = [Drawing.Bitmap]::new($Path); $bitmap = $source
    try {
        if ($source.Width -ne 1280 -or $source.Height -ne 720) { $bitmap = [Drawing.Bitmap]::new(1280, 720); $graphics = [Drawing.Graphics]::FromImage($bitmap); try { $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic; $graphics.DrawImage($source, 0, 0, 1280, 720) } finally { $graphics.Dispose() } }
        $values = [double[]]::new(($Roi.Width - 2) * ($Roi.Height - 2)); $index = 0
        for ($y = $Roi.Top + 1; $y -lt $Roi.Bottom - 1; $y++) { for ($x = $Roi.Left + 1; $x -lt $Roi.Right - 1; $x++) {
            $left = $bitmap.GetPixel($x - 1, $y); $right = $bitmap.GetPixel($x + 1, $y); $up = $bitmap.GetPixel($x, $y - 1); $down = $bitmap.GetPixel($x, $y + 1)
            $ll = (54 * $left.R + 183 * $left.G + 19 * $left.B + 128) -shr 8; $lr = (54 * $right.R + 183 * $right.G + 19 * $right.B + 128) -shr 8; $lu = (54 * $up.R + 183 * $up.G + 19 * $up.B + 128) -shr 8; $ld = (54 * $down.R + 183 * $down.G + 19 * $down.B + 128) -shr 8
            $gx = $lr - $ll; $gy = $ld - $lu; $values[$index++] = [Math]::Sqrt($gx * $gx + $gy * $gy)
        } }
        return ,$values
    } finally { if ($bitmap -ne $source) { $bitmap.Dispose() }; $source.Dispose() }
}

function Get-Correlation(
    [string]$Candidate,
    [string]$Reference,
    [Drawing.Rectangle]$Roi,
    $ReferenceRoi = $null
) {
    $rightRoi = if ($null -eq $ReferenceRoi) { $Roi } else { [Drawing.Rectangle]$ReferenceRoi }
    $left = Get-Edge $Candidate $Roi; $right = Get-Edge $Reference $rightRoi; $lm = ($left | Measure-Object -Average).Average; $rm = ($right | Measure-Object -Average).Average
    $numerator = 0.0; $ld = 0.0; $rd = 0.0
    for ($i = 0; $i -lt $left.Count; $i++) { $x = $left[$i] - $lm; $y = $right[$i] - $rm; $numerator += $x * $y; $ld += $x * $x; $rd += $y * $y }
    if ($ld -le 0 -or $rd -le 0) { throw 'Comparison ROI has zero variance.' }
    [int][Math]::Round(1000000.0 * $numerator / [Math]::Sqrt($ld * $rd))
}

function Get-RegisteredCorrelation(
    [string]$Candidate,
    [string]$Reference,
    [Drawing.Rectangle]$ReferenceRoi,
    [int]$MaxX,
    [int]$MaxY
) {
    $candidateBounds = [Drawing.Rectangle]::new(
        $ReferenceRoi.X - $MaxX, $ReferenceRoi.Y - $MaxY,
        $ReferenceRoi.Width + 2 * $MaxX,
        $ReferenceRoi.Height + 2 * $MaxY)
    $candidateEdges = Get-Edge $Candidate $candidateBounds
    $referenceEdges = Get-Edge $Reference $ReferenceRoi
    $candidateStride = $candidateBounds.Width - 2
    $roiWidth = $ReferenceRoi.Width - 2
    $roiHeight = $ReferenceRoi.Height - 2
    $referenceMean = ($referenceEdges | Measure-Object -Average).Average
    $best = $null
    for ($dy = -$MaxY; $dy -le $MaxY; $dy++) {
        for ($dx = -$MaxX; $dx -le $MaxX; $dx++) {
            $candidateValues = [double[]]::new($roiWidth * $roiHeight)
            $target = 0
            $sourceX = $dx + $MaxX
            $sourceY = $dy + $MaxY
            for ($y = 0; $y -lt $roiHeight; $y++) {
                $source = ($sourceY + $y) * $candidateStride + $sourceX
                [Array]::Copy($candidateEdges, $source, $candidateValues,
                    $target, $roiWidth)
                $target += $roiWidth
            }
            $candidateMean = ($candidateValues | Measure-Object -Average).Average
            $numerator = 0.0
            $candidateDeviation = 0.0
            $referenceDeviation = 0.0
            for ($i = 0; $i -lt $candidateValues.Count; $i++) {
                $left = $candidateValues[$i] - $candidateMean
                $right = $referenceEdges[$i] - $referenceMean
                $numerator += $left * $right
                $candidateDeviation += $left * $left
                $referenceDeviation += $right * $right
            }
            if ($candidateDeviation -le 0 -or $referenceDeviation -le 0) {
                throw 'Registered comparison ROI has zero variance.'
            }
            $ppm = [int][Math]::Round(
                1000000.0 * $numerator /
                    [Math]::Sqrt($candidateDeviation * $referenceDeviation))
            if ($null -eq $best -or $ppm -gt $best.Ppm) {
                $best = [pscustomobject]@{ Ppm = $ppm; Dx = $dx; Dy = $dy }
            }
        }
    }
    $best
}

function Get-Metrics([string]$User) {
    $user = Resolve-Safe $User 'User root'; $title = Get-Bitmap (Join-Path $user 'mcla-first-frame.bmp') 2560 1440; $gameplay = Get-Bitmap (Join-Path $user 'mcla-frontend-gameplay.bmp') 2560 1440; $pause = Get-Bitmap (Join-Path $user 'mcla-frontend-pause.bmp') 2560 1440; $options = Get-Bitmap (Join-Path $user 'mcla-frontend-options.bmp') 2560 1440
    $xTitle = Resolve-Safe (Join-Path $xeniaRoot '545407F8 - 2026-08-11T00-59-52.png') 'Xenia title'; $xPause = Resolve-Safe (Join-Path $xeniaRoot '545407F8 - 2026-08-11T00-52-04.png') 'Xenia pause'; $xGameplay = Resolve-Safe (Join-Path $xeniaRoot '545407F8 - 2026-08-11T00-53-02.png') 'Xenia gameplay'; $nativeOptions = Resolve-Safe (Join-Path $m4011Root 'runs\02\user\mcla-frontend-options.bmp') 'Native options reference'
    # The pause panel is anchored to the viewport with a small layout offset
    # between the stock-Xenia capture and the native saved-profile route.
    # Register only the stable footer within a tightly bounded translation;
    # this does not compensate for scale, rotation, content, or deformation.
    $pauseComparison = Get-RegisteredCorrelation $pause.Path $xPause ([Drawing.Rectangle]::new(300, 470, 240, 50)) 8 2
    $metrics = [ordered]@{
        title_logo_ppm = Get-Correlation $title.Path $xTitle ([Drawing.Rectangle]::new(265, 235, 745, 195))
        title_press_ppm = Get-Correlation $title.Path $xTitle ([Drawing.Rectangle]::new(1080, 630, 105, 45))
        pause_footer_ppm = $pauseComparison.Ppm
        pause_registration_dx = $pauseComparison.Dx
        pause_registration_dy = $pauseComparison.Dy
        gameplay_hud_ppm = Get-Correlation $gameplay.Path $xGameplay ([Drawing.Rectangle]::new(930, 500, 275, 175))
        options_native_ppm = Get-Correlation $options.Path $nativeOptions ([Drawing.Rectangle]::new(210, 325, 330, 155))
    }
    if ($metrics.title_logo_ppm -lt 900000 -or $metrics.title_press_ppm -lt 900000 -or $metrics.pause_footer_ppm -lt 500000 -or $metrics.gameplay_hud_ppm -lt 650000 -or $metrics.options_native_ppm -lt 550000) { throw 'Two-resolution UI comparison is below its fail-closed threshold.' }
    [pscustomobject]@{ Captures = [ordered]@{ title = $title; gameplay = $gameplay; pause = $pause; options = $options }; Metrics = $metrics }
}

function Get-Scale1Metrics([string]$User) {
    $user = Resolve-Safe $User 'Scale-1 user root'; $title = Resolve-Safe (Join-Path $user 'mcla-first-frame.bmp') 'Scale-1 title'; $gameplay = Resolve-Safe (Join-Path $user 'mcla-frontend-gameplay.bmp') 'Scale-1 gameplay'; $pause = Resolve-Safe (Join-Path $user 'mcla-frontend-pause.bmp') 'Scale-1 pause'
    $metrics = [ordered]@{
        title_logo_ppm = Get-Correlation $title (Resolve-Safe (Join-Path $xeniaRoot '545407F8 - 2026-08-11T00-59-52.png') 'Xenia title') ([Drawing.Rectangle]::new(265, 235, 745, 195))
        title_press_ppm = Get-Correlation $title (Resolve-Safe (Join-Path $xeniaRoot '545407F8 - 2026-08-11T00-59-52.png') 'Xenia title') ([Drawing.Rectangle]::new(1080, 630, 105, 45))
        pause_footer_ppm = Get-Correlation $pause (Resolve-Safe (Join-Path $xeniaRoot '545407F8 - 2026-08-11T00-52-04.png') 'Xenia pause') ([Drawing.Rectangle]::new(300, 470, 240, 50))
        gameplay_hud_ppm = Get-Correlation $gameplay (Resolve-Safe (Join-Path $xeniaRoot '545407F8 - 2026-08-11T00-53-02.png') 'Xenia gameplay') ([Drawing.Rectangle]::new(930, 500, 275, 175))
    }
    if ($metrics.title_logo_ppm -lt 900000 -or $metrics.title_press_ppm -lt 900000 -or $metrics.pause_footer_ppm -lt 500000 -or $metrics.gameplay_hud_ppm -lt 650000) { throw 'Pinned scale-1 Xenia comparison is below its fail-closed threshold.' }
    [pscustomobject]$metrics
}

function Get-Probe([string]$Log, [string]$User) {
    $logSet = Get-LogSet $Log; $text = $logSet.Text
    if ([regex]::Matches($text, '(?m)^.*MCLA_FRONTEND_SMOKE_CONFIG v=1 slot=0 hold_ms=200 gameplay_wait_seconds=30 intertab_wait_seconds=2\s*$').Count -ne 1 -or [regex]::Matches($text, '(?m)^.*MCLA_FRONTEND_SMOKE_TIMING v=1 first_frame_settle_seconds=45 gameplay_wait_seconds=45\s*$').Count -ne 1) { throw 'Frontend parity config/timing markers are invalid.' }
    $frames = [regex]::Matches($text, '(?m)^.*MCLA_FRONTEND_SMOKE_FRAME v=1 phase=(?<phase>gameplay|pause|options) width=2560 height=1440 status=PASS\s*$')
    if ($frames.Count -ne 3 -or ($frames | ForEach-Object { $_.Groups['phase'].Value }) -join ',' -cne 'gameplay,pause,options') { throw 'Two-resolution frame markers are invalid.' }
    $summary = [regex]::Matches($text, '(?m)^.*MCLA_FRONTEND_SMOKE_SUMMARY v=1 status=PASS pulses=4 source_records=8 guest_records=8 frames=3 gameplay=1 pause=1 options=1 external_close_required=1\s*$'); if ($summary.Count -ne 1) { throw 'Frontend parity summary is invalid.' }
    $close = $text.IndexOf('Window closing, shutting down...'); $complete = $text.IndexOf('Execution complete'); $hard = $text.IndexOf('Title terminated; hard-exiting process.')
    if ($close -lt 0 -or $complete -le $close -or $hard -le $complete -or $summary[0].Index -ge $close) { throw 'Frontend parity lifecycle is invalid.' }
    if ($text -match '(?i)(REX_GUEST_CRASH|GUEST_CRASH_REPORT|PPC_UNIMPLEMENTED|\[fatal\]|assertion failed|D3D12.*device (?:lost|removed))') { throw 'Fatal/unsupported marker found.' }
    $visual = Get-Metrics $User
    [pscustomobject]@{ Passed = $true; LogSet = $logSet; Captures = $visual.Captures; Metrics = $visual.Metrics }
}

Add-Type -AssemblyName System.Drawing
if ($PSCmdlet.ParameterSetName -eq 'Probe') { if (-not $ProbeOnly) { throw 'Probe mode requires -ProbeOnly.' }; Get-Probe $RuntimeLogPath $UserRoot; return }

$result = Resolve-Safe $ResultPath 'Result'; $json = [IO.File]::ReadAllText($result)
if ($json -match '(?i)([A-Z]:[\\/]|\\\\[^"\s]+[\\/]|(?:^|["\\/])private[\\/])') { throw 'Result contains a private or absolute path.' }
$record = $json | ConvertFrom-Json
$exactTopLevel = @('schema', 'task', 'decision', 'resolution_count', 'whole_frame_equivalence_claimed', 'audio_event_identity_claimed', 'xenia_options_reference_available', 'prior_frontend_result_sha256', 'prior_audio_result_sha256', 'xenia_log_sha256', 'xenia_frames', 'scale1_cycles', 'scale2', 'audio', 'contact_sheet_sha256', 'build_log_sha256', 'game_identity', 'artifacts', 'no_surviving_processes', 'data_integrity_preserved')
if (($record.PSObject.Properties.Name -join ',') -cne ($exactTopLevel -join ',') -or $record.schema -isnot [int] -or $record.schema -ne 1 -or $record.task -cne 'M4-012' -or $record.decision -cne 'two-resolution-frontend-and-audio-lifecycle-comparison-pass' -or $record.resolution_count -isnot [int] -or $record.resolution_count -ne 2 -or $record.whole_frame_equivalence_claimed -isnot [bool] -or $record.whole_frame_equivalence_claimed -ne $false -or $record.audio_event_identity_claimed -isnot [bool] -or $record.audio_event_identity_claimed -ne $false -or $record.xenia_options_reference_available -isnot [bool] -or $record.xenia_options_reference_available -ne $false -or $record.no_surviving_processes -ne $true -or $record.data_integrity_preserved -ne $true) { throw 'Comparison result identity/scope is invalid.' }

$m4011Result = Resolve-Safe (Join-Path $m4011Root 'result.json') 'M4-011 result'; if ((Get-FileHash $m4011Result -Algorithm SHA256).Hash -cne $expected.m4011 -or $record.prior_frontend_result_sha256 -cne $expected.m4011) { throw 'Pinned M4-011 evidence changed.' }
for ($i = 1; $i -le 3; $i++) {
    $name = '{0:D2}' -f $i; $user = Join-Path $m4011Root "runs\$name\user"; $null = & (Join-Path $PSScriptRoot 'verify-frontend-smoke.ps1') -ProbeOnly -RuntimeLogPath (Join-Path $m4011Root "runs\$name\mcla.log") -UserRoot $user; $scale1 = Get-Scale1Metrics $user
    foreach ($metric in @('title_logo_ppm', 'title_press_ppm', 'pause_footer_ppm', 'gameplay_hud_ppm')) { if ($record.scale1_cycles[$i - 1].$metric -ne $scale1.$metric) { throw 'Scale-1 metric binding failed.' } }
}
$m4007Result = Resolve-Safe (Join-Path $m4007Root 'result.json') 'M4-007 result'; if ((Get-FileHash $m4007Result -Algorithm SHA256).Hash -cne $expected.m4007 -or $record.prior_audio_result_sha256 -cne $expected.m4007) { throw 'Pinned M4-007 evidence changed.' }
$audio = & (Join-Path $PSScriptRoot 'verify-audio-route-smoke.ps1') -ProbeOnly -AudioOnly -RuntimeLogPath (Join-Path $m4007Root 'runs\01\mcla.log') -BmpPath (Join-Path $m4007Root 'runs\01\user\mcla-first-frame.bmp')
if ($record.audio.native_submit_nonzero -ne $audio.SubmitNonzero -or $record.audio.native_device_nonzero -ne $audio.DeviceNonzero -or $record.audio.native_xma_nonzero -ne $audio.XmaNonzero -or $record.audio.native_max_queue_depth -ne $audio.MaxQueueDepth -or $record.audio.xenia_event_level_telemetry_available -ne $false -or $record.audio.shared_lifecycle_events -ne 3) { throw 'Native audio evidence/result binding failed.' }

if ((Get-FileHash (Resolve-Safe $xeniaLog 'Xenia log') -Algorithm SHA256).Hash -cne $expected.xenia_log -or $record.xenia_log_sha256 -cne $expected.xenia_log) { throw 'Pinned Xenia log changed.' }
$xtext = [IO.File]::ReadAllText($xeniaLog); foreach ($pattern in @('XMA Decoder \(', 'Audio Worker \(', 'AudioSystem::RegisterClient: client 0 registered successfully')) { if ([regex]::Matches($xtext, $pattern).Count -ne 1) { throw 'Pinned Xenia audio lifecycle event set changed.' } }
$xeniaFiles = [ordered]@{ title = '545407F8 - 2026-08-11T00-59-52.png'; pause = '545407F8 - 2026-08-11T00-52-04.png'; gameplay = '545407F8 - 2026-08-11T00-53-02.png' }
foreach ($key in $xeniaFiles.Keys) { $path = Resolve-Safe (Join-Path $xeniaRoot $xeniaFiles[$key]) "Xenia $key"; if ((Get-FileHash $path -Algorithm SHA256).Hash -cne $expected[$key] -or $record.xenia_frames.$key.sha256 -cne $expected[$key]) { throw "Pinned Xenia $key frame changed." } }

$root = Split-Path $result; $probe = Get-Probe (Join-Path $root 'runs\scale2\mcla.log') (Join-Path $root 'runs\scale2\user')
foreach ($name in @('title', 'gameplay', 'pause', 'options')) { if ($record.scale2.captures.$name.sha256 -cne $probe.Captures[$name].Sha256 -or $record.scale2.captures.$name.bytes -ne $probe.Captures[$name].Bytes) { throw 'Scale-2 capture binding failed.' } }
foreach ($name in @('title_logo_ppm', 'title_press_ppm', 'pause_footer_ppm', 'pause_registration_dx', 'pause_registration_dy', 'gameplay_hud_ppm', 'options_native_ppm')) { if ($record.scale2.metrics.$name -ne $probe.Metrics[$name]) { throw 'Scale-2 metric binding failed.' } }
if ($record.scale2.runtime_log_set_sha256 -cne $probe.LogSet.Hash -or $record.scale2.exit_code -ne 0 -or $record.scale2.harness_force_cleanup -ne $false -or $record.scale2.width -ne 2560 -or $record.scale2.height -ne 1440 -or $record.scale2.first_frame_settle_seconds -ne 45 -or $record.scale2.gameplay_wait_seconds -ne 45) { throw 'Scale-2 runtime binding failed.' }
$cycleTree = Get-Tree (Join-Path $root 'runs\scale2'); if ($record.scale2.cycle_tree_sha256 -cne $cycleTree.Hash -or $record.scale2.cycle_file_count -ne $cycleTree.FileCount -or $record.scale2.cycle_bytes -ne $cycleTree.Bytes) { throw 'Scale-2 cycle tree binding failed.' }
$contact = Resolve-Safe (Join-Path $root 'contact-sheet.png') 'Contact sheet'; if ($record.contact_sheet_sha256 -cne (Get-FileHash $contact -Algorithm SHA256).Hash) { throw 'Contact-sheet binding failed.' }; $contactImage = [Drawing.Image]::FromFile($contact); try { if ($contactImage.Width -ne 1280 -or $contactImage.Height -ne 800 -or $contactImage.RawFormat.Guid -ne [Drawing.Imaging.ImageFormat]::Png.Guid) { throw 'Contact-sheet format is invalid.' } } finally { $contactImage.Dispose() }
$buildLog = Resolve-Safe (Join-Path $root 'relwithdebinfo-clean-build.log') 'Build log'; $buildText = [IO.File]::ReadAllText($buildLog); if ($record.build_log_sha256 -cne (Get-FileHash $buildLog -Algorithm SHA256).Hash -or -not $buildText.Contains('[71/71] Linking CXX executable mcla.exe') -or $buildText -match '(?im)(^|\s)(FAILED:|error:|ninja: build stopped)') { throw 'Clean-build log binding failed.' }
if (($record.artifacts.before | ConvertTo-Json -Compress) -cne ($record.artifacts.after | ConvertTo-Json -Compress) -or ($record.game_identity.before | ConvertTo-Json -Compress) -cne ($record.game_identity.after | ConvertTo-Json -Compress)) { throw 'Source-game/runtime identity changed.' }
$canonicalBuild = Resolve-Safe (Join-Path $repo 'out\build\win-amd64-relwithdebinfo') 'Canonical build'; foreach ($artifact in $record.artifacts.after) { if ((Get-FileHash (Resolve-Safe (Join-Path $canonicalBuild $artifact.name) 'Runtime artifact') -Algorithm SHA256).Hash -cne $artifact.sha256) { throw 'Current runtime artifact binding failed.' } }
if (@((Get-Process mcla -ErrorAction SilentlyContinue) | Where-Object { try { [string]::Equals($_.Path, (Join-Path $canonicalBuild 'mcla.exe'), [StringComparison]::OrdinalIgnoreCase) } catch { $false } }).Count) { throw 'Canonical MCLA process still survives.' }
[pscustomobject]@{ Passed = $true; Decision = $record.decision; ResolutionCount = 2; XeniaFramesCompared = 3; SharedAudioLifecycleEvents = 3; AudioEventIdentityClaimed = $false; WholeFrameEquivalenceClaimed = $false; DataIntegrityVerified = $true }
