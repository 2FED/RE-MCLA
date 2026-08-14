[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-world-streaming-smoke.ps1'
$utf8 = [Text.UTF8Encoding]::new($false)
$root = Join-Path $repo ('private\evidence\M5-002\test-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
[IO.Directory]::CreateDirectory($root) | Out-Null

function L([string]$Body) { "[2026-08-14 10:00:00.000] [trace] [krnl] [t1] $Body" }

function New-PositiveLines {
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add((L 'MCLA VFS: mixed-case RPF path resolution verified'))
    $lines.Add((L 'KernelState: Preparing module launch...'))
    foreach ($asset in @(
        [pscustomobject]@{ Name='audlo'; File='xarchive_audlo.rpf'; Main='a100'; Temp='a101'; Size=1463189504L; Calls=10; Bytes=6000 },
        [pscustomobject]@{ Name='cache'; File='xarchive_cache.rpf'; Main='c100'; Temp='c101'; Size=2130739200L; Calls=4000; Bytes=25000 }
    )) {
        $lines.Add((L "[NtCreateFile] path=game:\$($asset.File) access=0x1 attrs=0x0 share=0x1 disp=0x1 options=0x60"))
        $lines.Add((L "[NtCreateFile] -> 0x0 handle=0x$($asset.Main)"))
        $lines.Add((L "[NtCreateFile] path=game:\$($asset.File) access=0x1 attrs=0x0 share=0x3 disp=0x1 options=0x60"))
        $lines.Add((L "[NtCreateFile] -> 0x0 handle=0x$($asset.Temp)"))
        $lines.Add((L "[NtClose] handle=0x$($asset.Temp)"))
        for ($i=0; $i -lt $asset.Calls; $i++) {
            $offset = if ($i -eq $asset.Calls - 1) { [long][Math]::Floor($asset.Size * 0.96) } else { [long]($i * $asset.Bytes) }
            $lines.Add((L ("[NtReadFile] handle=0x{0} event=0x0 apc=0x0 apc_ctx=0x0 iosb=0x0 buf=0x0 len=0x{1:X} offset={2}" -f $asset.Main,$asset.Bytes,$offset)))
            $lines.Add((L "[NtReadFile] -> 0x0 (sync=true, iosb_status=0x0, iosb_info=$($asset.Bytes), ev_signaled=false, apc_requested=false, apc_queued=false)"))
        }
    }
    1..5 | ForEach-Object { $lines.Add((L "[NtCreateFile] FAILED: path='t:\mc4\art\city\test_dt_railyard.loc' -> 0xC000000F")); $lines.Add((L 'ResolvePath(t:\mc4\art\city) failed - device not found')) }
    1..2 | ForEach-Object { $lines.Add((L "[NtCreateFile] FAILED: path='t:\mc4\art\city\test_sc_exposition_park.loc' -> 0xC000000F")); $lines.Add((L 'ResolvePath(t:\mc4\art\city) failed - device not found')) }
    $lines.Add((L 'MCLA_FRONTEND_SMOKE_FRAME v=1 phase=gameplay width=1280 height=720 status=PASS'))
    @($lines)
}

function Invoke-Case([string]$Name, [scriptblock]$Transform, [bool]$ShouldPass) {
    $case = Join-Path $root $Name; [IO.Directory]::CreateDirectory($case) | Out-Null
    $lines = @(New-PositiveLines)
    if ($Transform) { $lines = @(& $Transform $lines) }
    [IO.File]::WriteAllLines((Join-Path $case 'mcla.log'), $lines, $utf8)
    $passed = $false
    try { $null = & $verify -ProbeOnly -FixtureMode -RuntimeLogPath (Join-Path $case 'mcla.log'); $passed = $true } catch { if ($ShouldPass) { throw "Positive '$Name' failed: $($_.Exception.Message)" } }
    if ($passed -ne $ShouldPass) { throw "Fixture '$Name' acceptance mismatch." }
}

try {
    Invoke-Case 'positive' $null $true
    $negatives = [ordered]@{
        'missing-case'={param($x) $x | Where-Object {$_ -notmatch 'mixed-case RPF'}}
        'duplicate-case'={param($x) @($x[0]) + $x}
        'missing-launch'={param($x) $x | Where-Object {$_ -notmatch 'Preparing module launch'}}
        'duplicate-launch'={param($x) @($x[0],$x[1]) + $x[1..($x.Count-1)]}
        'missing-gameplay'={param($x) $x | Where-Object {$_ -notmatch 'phase=gameplay'}}
        'fatal'={param($x) @($x[0..($x.Count-2)]) + (L '[fatal] crash') + $x[-1]}
        'guest-crash'={param($x) @($x[0..($x.Count-2)]) + (L 'REX_GUEST_CRASH') + $x[-1]}
        'ppc-unimplemented'={param($x) @($x[0..($x.Count-2)]) + (L 'PPC_UNIMPLEMENTED') + $x[-1]}
        'assertion'={param($x) @($x[0..($x.Count-2)]) + (L 'Assertion failed') + $x[-1]}
        'device-removed'={param($x) @($x[0..($x.Count-2)]) + (L 'D3D12 device removed') + $x[-1]}
        'unexpected-miss'={param($x) @($x[0..($x.Count-2)]) + (L "[NtCreateFile] FAILED: path='game:\missing.bin' -> 0xC000000F") + $x[-1]}
        'duplicate-expected-miss'={param($x) $needle=@($x|Where-Object{$_ -match 'FAILED:.*railyard'})[0]; @($x[0..($x.Count-2)])+$needle+$x[-1]}
        'wrong-miss-status'={param($x) $done=$false; $x | ForEach-Object {if(-not $done -and $_ -match 'FAILED:'){ $done=$true; $_ -replace 'C000000F','C0000001'}else{$_}}}
        'missing-railyard'={param($x) $done=$false; $x | Where-Object {if(-not $done -and $_ -match 'test_dt_railyard'){ $done=$true;$false}else{$true}}}
        'wrong-miss-distribution'={param($x) $done=$false; $x|ForEach-Object{if(-not$done-and$_-match'test_sc_exposition_park'){$done=$true;$_-replace'test_sc_exposition_park','test_dt_railyard'}else{$_}}}
        'missing-resolve'={param($x) $done=$false; $x | Where-Object {if(-not $done -and $_ -match 'ResolvePath'){ $done=$true;$false}else{$true}}}
        'duplicate-resolve'={param($x) $needle=@($x|Where-Object{$_ -match 'ResolvePath'})[0]; @($x[0..($x.Count-2)])+$needle+$x[-1]}
        'duplicate-gameplay'={param($x) $x + $x[-1]}
        'implicit-offset'={param($x) $done=$false; $x | ForEach-Object {if(-not $done -and $_ -match 'NtReadFile.*offset='){ $done=$true; $_ -replace 'offset=[0-9]+','offset=-1'}else{$_}}}
        'read-failure'={param($x) $done=$false; $x | ForEach-Object {if(-not $done -and $_ -match 'NtReadFile.*-> 0x0'){ $done=$true; $_ -replace '-> 0x0','-> 0xC0000001'}else{$_}}}
        'iosb-failure'={param($x) $done=$false; $x | ForEach-Object {if(-not $done -and $_ -match 'iosb_status=0x0'){ $done=$true; $_ -replace 'iosb_status=0x0','iosb_status=0xC0000001'}else{$_}}}
        'zero-read'={param($x) $done=$false; $x | ForEach-Object {if(-not $done -and $_ -match 'iosb_info=6000'){ $done=$true; $_ -replace 'iosb_info=6000','iosb_info=0'}else{$_}}}
        'too-many-bytes'={param($x) $done=$false; $x | ForEach-Object {if(-not $done -and $_ -match 'iosb_info=6000'){ $done=$true; $_ -replace 'iosb_info=6000','iosb_info=7000'}else{$_}}}
        'missing-create-completion'={param($x) $done=$false; $x | Where-Object {if(-not $done -and $_ -match 'NtCreateFile.*-> 0x0'){ $done=$true;$false}else{$true}}}
        'archive-open-failure'={param($x) $done=$false;$x|ForEach-Object{if(-not$done-and$_-match'NtCreateFile.*-> 0x0'){$done=$true;$_-replace'-> 0x0','-> 0xC0000001'}else{$_}}}
        'missing-read-completion'={param($x) $done=$false; $x | Where-Object {if(-not $done -and $_ -match 'NtReadFile.*-> 0x0'){ $done=$true;$false}else{$true}}}
        'cache-read-floor'={param($x) $removed=0; $x | Where-Object {if($removed -lt 2 -and $_ -match 'handle=0xc100|NtReadFile.*-> 0x0'){if($_ -match 'handle=0xc100'){$removed=1;$false}elseif($removed -eq 1){$removed=2;$false}else{$true}}else{$true}}}
        'case-after-launch'={param($x) @($x[1],$x[0]) + $x[2..($x.Count-1)]}
        'gameplay-before-reads'={param($x) @($x[0],$x[1],$x[-1]) + $x[2..($x.Count-2)]}
        'wrong-case-marker'={param($x) $x|ForEach-Object{$_-replace'mixed-case RPF path resolution verified','mixed-case path check skipped'}}
    }
    foreach ($item in $negatives.GetEnumerator()) { Invoke-Case $item.Key $item.Value $false }

    $source = [IO.File]::ReadAllText((Join-Path $repo 'src\mcla_app.cpp'))
    foreach ($needle in @('GAME:\\XARCHIVE_CACHE.RPF','D:\\XaRcHiVe_CaChE.RpF','mixed-case RPF path resolution verified')) { if (-not $source.Contains($needle)) { throw "Source contract missing '$needle'." } }
    [pscustomobject]@{ Passed=$true; Positives=1; FailClosedNegatives=$negatives.Count; SourceChecks=3 }
} finally {
    if (Test-Path $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
