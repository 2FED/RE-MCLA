[CmdletBinding()]
param(
    [Parameter(ParameterSetName='Log', Mandatory)] [string]$LogPath,
    [Parameter(ParameterSetName='Result', Mandatory)] [string]$ResultPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path

function Resolve-Contained([string]$Path, [string]$Root, [string]$Label) {
    $full = [IO.Path]::GetFullPath($Path)
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($rootFull, [StringComparison]::OrdinalIgnoreCase)) { throw "$Label escapes its root." }
    $item = Get-Item -LiteralPath $full -Force
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "$Label is a reparse point." }
    return $full
}

function Read-OrderedLogs([string]$CurrentLog) {
    $current = Get-Item -LiteralPath $CurrentLog
    $escaped = [regex]::Escape($current.BaseName)
    $rotated = @(Get-ChildItem -LiteralPath $current.DirectoryName -File | Where-Object {
        $_.Name -match "^$escaped\.(?<n>\d+)$([regex]::Escape($current.Extension))$"
    } | ForEach-Object { [pscustomobject]@{ Item=$_; N=[int]$Matches.n } } | Sort-Object N -Descending)
    if ($rotated.Count) {
        $numbers = @($rotated.N | Sort-Object)
        for ($i=0; $i -lt $numbers.Count; $i++) { if ($numbers[$i] -ne $i + 1) { throw 'Rotated log set is not contiguous.' } }
    }
    $files = @($rotated | ForEach-Object { $_.Item }) + @($current)
    $builder = [Text.StringBuilder]::new()
    $manifest = @()
    foreach ($file in $files) {
        $stream = [IO.File]::Open($file.FullName, [IO.FileMode]::Open, [IO.FileAccess]::Read,
            [IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
        try { $reader=[IO.StreamReader]::new($stream,[Text.Encoding]::UTF8,$true); $text=$reader.ReadToEnd(); $reader.Dispose() }
        finally { $stream.Dispose() }
        [void]$builder.Append($text).Append("`n")
        $manifest += [ordered]@{ name=$file.Name; bytes=$file.Length; sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
    }
    [pscustomobject]@{ Text=$builder.ToString(); Manifest=$manifest }
}

function Parse-Probe([string]$CurrentLog) {
    $ordered = Read-OrderedLogs $CurrentLog
    $lines = @($ordered.Text -split "`r?`n")
    $config = @($lines | Where-Object { $_ -match 'REX_PERF_AUDIT_CONFIG' })
    $samples = @($lines | Where-Object { $_ -match 'REX_PERF_AUDIT_SAMPLE' })
    $summary = @($lines | Where-Object { $_ -match 'REX_PERF_AUDIT_SUMMARY' })
    if ($config.Count -ne 1) { throw 'Expected exactly one performance CONFIG.' }
    if ($samples.Count -ne 300) { throw 'Expected exactly 300 performance SAMPLE records.' }
    if ($summary.Count -ne 1) { throw 'Expected exactly one performance SUMMARY.' }
    if ($config[0] -notmatch 'REX_PERF_AUDIT_CONFIG v=1 enabled=1 backend=d3d12 sample_limit=300 streaming_stall_us=5000$') { throw 'Malformed performance CONFIG.' }
    $samplePattern = 'REX_PERF_AUDIT_SAMPLE v=1 sample=(?<sample>\d+) host_us=(?<host>\d+) guest_frame=(?<frame>\d+) cpu_us=(?<cpu>\d+) gpu_us=(?<gpu>\d+) stream_reads=(?<reads>\d+) stream_bytes=(?<bytes>\d+) stream_stalls=(?<stalls>\d+) stream_max_us=(?<maxread>\d+) audio_underruns=(?<audio>\d+) shader_count=(?<shader>\d+) shader_fail=(?<shaderfail>\d+) shader_us=(?<shaderus>\d+) pso_count=(?<pso>\d+) pso_fail=(?<psofail>\d+) pso_us=(?<psous>\d+)$'
    $parsed = @(); $lastHost=-1L; $lastFrame=0L
    $totals = [ordered]@{ reads=0L; bytes=0L; stalls=0L; audio=0L; shader=0L; shaderfail=0L; shaderus=0L; pso=0L; psofail=0L; psous=0L }
    for ($i=0; $i -lt $samples.Count; $i++) {
        if ($samples[$i] -notmatch $samplePattern) { throw "Malformed SAMPLE at index $i." }
        $row = [ordered]@{}
        foreach ($name in @('sample','host','frame','cpu','gpu','reads','bytes','stalls','maxread','audio','shader','shaderfail','shaderus','pso','psofail','psous')) { $row[$name]=[long]$Matches[$name] }
        if ($row.sample -ne $i -or $row.host -le $lastHost -or $row.frame -le $lastFrame -or $row.cpu -le 0 -or $row.gpu -le 0) { throw "Non-monotonic or zero timing SAMPLE at index $i." }
        if ($row.shaderfail -gt $row.shader -or $row.psofail -gt $row.pso -or ($row.stalls -gt 0 -and $row.maxread -lt 5000)) { throw "Impossible SAMPLE accounting at index $i." }
        $lastHost=$row.host; $lastFrame=$row.frame
        foreach ($pair in @(@('reads','reads'),@('bytes','bytes'),@('stalls','stalls'),@('audio','audio'),@('shader','shader'),@('shaderfail','shaderfail'),@('shaderus','shaderus'),@('pso','pso'),@('psofail','psofail'),@('psous','psous'))) { $totals[$pair[0]] += $row[$pair[1]] }
        $parsed += [pscustomobject]$row
    }
    $summaryPattern = 'REX_PERF_AUDIT_SUMMARY v=1 status=PASS samples=300 stream_reads=(?<reads>\d+) stream_bytes=(?<bytes>\d+) stream_stalls=(?<stalls>\d+) audio_underruns=(?<audio>\d+) shader_count=(?<shader>\d+) shader_fail=(?<shaderfail>\d+) shader_us=(?<shaderus>\d+) pso_count=(?<pso>\d+) pso_fail=(?<psofail>\d+) pso_us=(?<psous>\d+) marker_overflow=(?<overflow>\d+)$'
    if ($summary[0] -notmatch $summaryPattern) { throw 'Malformed performance SUMMARY.' }
    foreach ($name in $totals.Keys) { if ([long]$Matches[$name] -ne $totals[$name]) { throw "SUMMARY $name does not balance SAMPLE records." } }
    if ([long]$Matches.overflow -ne 0 -or $totals.shaderfail -ne 0 -or $totals.psofail -ne 0) { throw 'Performance telemetry reports overflow or compilation failure.' }
    if ($totals.reads -le 0 -or $totals.bytes -le 0 -or $totals.shader -le 0 -or $totals.pso -le 0) { throw 'Trace did not exercise streaming and cold shader/PSO work.' }
    $summaryIndex=$ordered.Text.IndexOf('REX_PERF_AUDIT_SUMMARY',[StringComparison]::Ordinal)
    $tail=$ordered.Text.Substring($summaryIndex + 'REX_PERF_AUDIT_SUMMARY'.Length)
    if ($tail -match 'REX_PERF_AUDIT_(CONFIG|SAMPLE|SUMMARY)') { throw 'Performance markers were emitted after the frozen summary.' }
    if ($ordered.Text -match '(?i)\[FATAL\]|device removed|DRED|guest crash') { throw 'Fatal or device-loss marker found.' }
    function Percentile([long[]]$Values,[double]$P) { $sorted=@($Values|Sort-Object); return $sorted[[Math]::Min($sorted.Count-1,[Math]::Floor(($sorted.Count-1)*$P))] }
    [pscustomobject]@{
        Passed=$true; Samples=300; FirstGuestFrame=$parsed[0].frame; LastGuestFrame=$parsed[-1].frame
        CpuP50Us=(Percentile @($parsed.cpu) .50); CpuP95Us=(Percentile @($parsed.cpu) .95); CpuMaxUs=($parsed.cpu|Measure-Object -Maximum).Maximum
        GpuP50Us=(Percentile @($parsed.gpu) .50); GpuP95Us=(Percentile @($parsed.gpu) .95); GpuMaxUs=($parsed.gpu|Measure-Object -Maximum).Maximum
        StreamReads=$totals.reads; StreamBytes=$totals.bytes; StreamStalls=$totals.stalls; AudioUnderruns=$totals.audio
        ShaderCount=$totals.shader; PsoCount=$totals.pso; Manifest=$ordered.Manifest
    }
}

if ($PSCmdlet.ParameterSetName -eq 'Log') { return Parse-Probe (Resolve-Path -LiteralPath $LogPath).Path }

$privateRoot = Join-Path $repoRoot 'private/evidence/M6-012'
$resultFull = Resolve-Contained $ResultPath $privateRoot 'Result path'
$result = Get-Content -LiteralPath $resultFull -Raw | ConvertFrom-Json
if ($result.schema -ne 1 -or $result.task -ne 'M6-012' -or $result.decision -ne 'timestamped-performance-telemetry-pass' -or
    $result.sdk_version -ne '0.9.0.28' -or $result.sdk_commit -ne '6354bbe2150c7ce06bee5ffe399f17a94c948616' -or
    $result.focused_test_cases -ne 2 -or $result.focused_test_assertions -ne 7) { throw 'Result identity is invalid.' }
$runRoot = Split-Path $resultFull -Parent
$logFull = Resolve-Contained (Join-Path $runRoot $result.runtime_log) $runRoot 'Runtime log'
$probe = Parse-Probe $logFull
foreach ($field in @('samples','first_guest_frame','last_guest_frame','cpu_p50_us','cpu_p95_us','cpu_max_us','gpu_p50_us','gpu_p95_us','gpu_max_us','stream_reads','stream_bytes','stream_stalls','audio_underruns','shader_count','pso_count')) {
    $probeName = @{samples='Samples';first_guest_frame='FirstGuestFrame';last_guest_frame='LastGuestFrame';cpu_p50_us='CpuP50Us';cpu_p95_us='CpuP95Us';cpu_max_us='CpuMaxUs';gpu_p50_us='GpuP50Us';gpu_p95_us='GpuP95Us';gpu_max_us='GpuMaxUs';stream_reads='StreamReads';stream_bytes='StreamBytes';stream_stalls='StreamStalls';audio_underruns='AudioUnderruns';shader_count='ShaderCount';pso_count='PsoCount'}[$field]
    if ([long]$result.$field -ne [long]$probe.$probeName) { throw "Result field $field does not match physical logs." }
}
$declaredManifest=@($result.runtime_manifest)
if($declaredManifest.Count-ne$probe.Manifest.Count){throw 'Runtime log manifest cardinality drifted.'}
for($i=0;$i-lt$declaredManifest.Count;$i++){
    if($declaredManifest[$i].name-ne$probe.Manifest[$i].name -or [long]$declaredManifest[$i].bytes-ne[long]$probe.Manifest[$i].bytes -or $declaredManifest[$i].sha256-ne$probe.Manifest[$i].sha256){throw 'Runtime log manifest does not match physical files.'}
}
$exe=Resolve-Contained (Join-Path $repoRoot 'out/build/win-amd64-relwithdebinfo/mcla.exe') $repoRoot 'Canonical executable'
foreach($binding in @(
    @('executable_sha256',$exe),
    @('sdk_install_log_sha256',(Join-Path $runRoot 'sdk-install.log')),
    @('focused_test_log_sha256',(Join-Path $runRoot 'focused-tests.log')),
    @('app_clean_build_log_sha256',(Join-Path $runRoot 'app-clean-build.log'))
)){
    $path=if($binding[0]-eq'executable_sha256'){$exe}else{Resolve-Contained $binding[1] $runRoot "Binding $($binding[0])"}
    if($result.($binding[0])-notmatch '^[A-F0-9]{64}$' -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash-ne$result.($binding[0])){throw "Physical hash binding failed for $($binding[0])."}
}
$focusedText=Get-Content -LiteralPath (Join-Path $runRoot 'focused-tests.log') -Raw
if($focusedText-notmatch'All tests passed \(7 assertions in 2 test cases\)'){throw 'Focused test log does not contain the exact passing total.'}
if ($result.controlled_exit_verified -ne $true) { throw 'Lifecycle binding is invalid.' }
return $probe
