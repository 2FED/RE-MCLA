[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$root = Join-Path $repo ('private\evidence\M7-016\deck-perf-test-' + [guid]::NewGuid().ToString('N').Substring(0,8))
$utf8 = [Text.UTF8Encoding]::new($false)

function Write-Utf8([string]$Path, [string]$Text) {
    $parent = Split-Path $Path -Parent
    [IO.Directory]::CreateDirectory($parent) | Out-Null
    [IO.File]::WriteAllText($Path, $Text, $utf8)
}

function Write-Package([string]$Name, [int]$ProcessId, [long]$Outputs, [long]$Vblanks, [bool]$WithAudit) {
    $dir = Join-Path $root "diagnostics\live\$Name"
    [IO.Directory]::CreateDirectory($dir) | Out-Null
    $stamp = [regex]::Match($Name, 'live-(\d{8}T\d{6}Z)-').Groups[1].Value
    $state = [ordered]@{schema='mcla-diagnostic-state-v1';reason='f10';process=[ordered]@{pid=$ProcessId};runtime=[ordered]@{available=$true;guest_output_sequence=$Outputs;guest_vblank_sequence=$Vblanks}}
    Write-Utf8 (Join-Path $dir 'state.json') ($state | ConvertTo-Json -Depth 5)
    $lines = @('[core] ordinary log')
    if ($WithAudit) {
        $lines += '[core] REX_PERF_AUDIT_CONFIG v=1 enabled=1 backend=d3d12 sample_limit=300 streaming_stall_us=5000'
        $lines += '[core] REX_PERF_AUDIT_TRIGGER v=1 source=application-diagnostics'
        for ($i=0; $i -lt 300; $i++) {
            $hostUs = 1000000 + ($i * 50000)
            $lines += "[core] REX_PERF_AUDIT_SAMPLE v=1 sample=$i host_us=$hostUs guest_frame=$($i+10) cpu_us=12000 gpu_us=18000 stream_reads=1 stream_bytes=4096 stream_stalls=0 stream_max_us=100 audio_underruns=0 shader_count=0 shader_fail=0 shader_us=0 pso_count=0 pso_fail=0 pso_us=0"
        }
        $lines += '[core] REX_PERF_AUDIT_SUMMARY v=1 status=PASS samples=300 stream_reads=300 stream_bytes=1228800 stream_stalls=0 audio_underruns=0 shader_count=0 shader_fail=0 shader_us=0 pso_count=0 pso_fail=0 pso_us=0 marker_overflow=0'
    }
    Write-Utf8 (Join-Path $dir 'log-tail.txt') (($lines -join "`n") + "`n")
    $artifacts = @()
    foreach ($file in @('state.json','log-tail.txt')) {
        $item = Get-Item -LiteralPath (Join-Path $dir $file)
        $artifacts += [ordered]@{name=$file;bytes=$item.Length;sha256=(Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash;safe_to_share=$false}
    }
    $manifest = [ordered]@{schema='mcla-diagnostic-package-v1';kind='live';reason='f10';created_utc=$stamp;artifacts=$artifacts}
    Write-Utf8 (Join-Path $dir 'manifest.json') ($manifest | ConvertTo-Json -Depth 5)
    return $dir
}

try {
    [IO.Directory]::CreateDirectory($root) | Out-Null
    Write-Utf8 (Join-Path $root 'bundle-manifest.json') (([ordered]@{schema='mcla-portable-campaign-bundle-v1';task='M7-016';bundle_id='fixture';project_version='0.9.1.0';sdk_version='0.10.1.0'} | ConvertTo-Json))
    $first = Write-Package 'live-20260904T120000Z-10-1' 10 100 600 $false
    $second = Write-Package 'live-20260904T120030Z-10-2' 10 700 2400 $true
    $result = & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root
    if (-not $result.Passed -or $result.GuestOutputRateHz -ne 20 -or $result.GuestVblankRateHz -ne 60 -or
        $result.Audit.Samples -ne 300 -or $result.Audit.SampleRateHz -ne 20 -or
        $result.Audit.Classification -cne 'outside-render-work-or-frame-pacing') { throw 'Positive Deck performance analysis fixture did not produce the expected result.' }

    $negative = 0
    Add-Content -LiteralPath (Join-Path $first 'state.json') -Value 'tamper'
    try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root | Out-Null; throw 'Tampered state fixture unexpectedly passed.' }
    catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    Remove-Item -LiteralPath $first -Recurse -Force
    [void](Write-Package 'live-20260904T120000Z-11-1' 11 100 600 $false)
    try { & (Join-Path $PSScriptRoot 'analyze-portable-deck-performance.ps1') -BundlePath $root | Out-Null; throw 'Mixed-process fixture unexpectedly passed.' }
    catch { if ($_.Exception.Message -like '*unexpectedly passed*') { throw }; $negative++ }
    [pscustomobject]@{Passed=$true;PositiveFixtures=1;FailClosedNegatives=$negative}
} finally {
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
