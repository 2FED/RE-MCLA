[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-route-failure-report.ps1'
$root = Join-Path $repo ('private/evidence/M5-010/test-' + [guid]::NewGuid().ToString('N'))
$utf8 = [Text.UTF8Encoding]::new($false)
[IO.Directory]::CreateDirectory($root) | Out-Null

function New-Lines {
    $lines = [Collections.Generic.List[string]]::new()
    foreach ($line in @(
            '[trace] [krnl] [NtCreateFile] -> 0x0 handle=0x1',
            '[trace] [krnl] [NtReadFile] -> 0x0 (sync=true, iosb_status=0x0, iosb_info=4096)',
            '[trace] [krnl] [NtReadFile] -> 0x103 (sync=false, iosb_status=0x0, iosb_info=4096)',
            '[trace] [krnl] [NtAllocateVirtualMemory] -> 0x0 addr=0x10000 size=0x1000',
            '[trace] [krnl] [NtFreeVirtualMemory] -> 0x0',
            '[trace] [krnl] [MmAllocatePhysicalMemoryEx] -> addr=0x10000')) { $lines.Add($line) }
    ,$lines
}
function Invoke-Probe([Collections.Generic.List[string]]$Lines, [string]$Name) {
    $dir=Join-Path $root $Name;[IO.Directory]::CreateDirectory($dir)|Out-Null
    $path=Join-Path $dir 'mcla.log';[IO.File]::WriteAllLines($path,$Lines,$utf8)
    & $verify -RuntimeLogPath $path -FixtureMode
}
function Expect-Failure([scriptblock]$Action,[string]$Name){try{&$Action|Out-Null;throw "Negative '$Name' was accepted."}catch{if($_.Exception.Message-eq"Negative '$Name' was accepted."){throw}}}
function Add-Line([Collections.Generic.List[string]]$Lines,[string]$Line){$Lines.Add($Line)}
function Replace-First([Collections.Generic.List[string]]$Lines,[string]$Old,[string]$New){for($i=0;$i-lt$Lines.Count;$i++){if($Lines[$i].Contains($Old)){$Lines[$i]=$Lines[$i].Replace($Old,$New);return}};throw "Fixture token '$Old' missing."}

try {
    $positive=Invoke-Probe (New-Lines) 'positive';if(-not$positive.Passed){throw 'Positive probe failed.'}
    $cases=[ordered]@{
        'fatal'={param($x)Add-Line $x '[fatal] fixture'}
        'ppc-unimplemented'={param($x)Add-Line $x 'PPC_UNIMPLEMENTED fixture'}
        'unregistered'={param($x)Add-Line $x 'invalid/unregistered guest target'}
        'guest-crash'={param($x)Add-Line $x 'Guest crash at test'}
        'assertion'={param($x)Add-Line $x 'Assertion failed!'}
        'access-violation'={param($x)Add-Line $x 'Access violation'}
        'out-of-memory'={param($x)Add-Line $x 'Out of memory'}
        'bad-alloc'={param($x)Add-Line $x 'std::bad_alloc'}
        'heap-corruption'={param($x)Add-Line $x 'heap corruption'}
        'allocation-failed'={param($x)Add-Line $x 'allocation failed'}
        'failed-to-allocate'={param($x)Add-Line $x 'Failed to allocate function table'}
        'svod-read'={param($x)Add-Line $x 'ReadSVOD failed to read magic'}
        'stfs-read'={param($x)Add-Line $x 'ReadSTFS failed to read block'}
        'archive-failed'={param($x)Add-Line $x 'archive open failed'}
        'read-failed-marker'={param($x)Add-Line $x '[NtReadFile] FAILED fixture'}
        'write-failed-marker'={param($x)Add-Line $x '[NtWriteFile] FAILED fixture'}
        'stream-thread-failed'={param($x)Add-Line $x 'Streaming thread creation failed'}
        'device-lost'={param($x)Add-Line $x 'D3D12 device lost'}
        'allocate-status'={param($x)Replace-First $x '[NtAllocateVirtualMemory] -> 0x0' '[NtAllocateVirtualMemory] -> 0xc0000017'}
        'free-status'={param($x)Replace-First $x '[NtFreeVirtualMemory] -> 0x0' '[NtFreeVirtualMemory] -> 0xc000000d'}
        'physical-null'={param($x)Replace-First $x '-> addr=0x10000' '-> addr=0x0'}
        'read-status'={param($x)Replace-First $x '[NtReadFile] -> 0x0' '[NtReadFile] -> 0xc0000011'}
        'pending-bad-iosb'={param($x)Replace-First $x '[NtReadFile] -> 0x103 (sync=false, iosb_status=0x0, iosb_info=4096)' '[NtReadFile] -> 0x103 (sync=false, iosb_status=0xc0000001, iosb_info=0)'}
        'create-status'={param($x)Replace-First $x '[NtCreateFile] -> 0x0' '[NtCreateFile] -> 0xc000000f'}
        'unexpected-create-failure'={param($x)Add-Line $x "[NtCreateFile] FAILED: path='game:\\missing.bin' -> 0xc000000f"}
        'unexpected-development-miss'={param($x)Add-Line $x "[NtCreateFile] FAILED: path='t:\\mc4\\art\\city\\test_dt_railyard.loc' -> 0xc000000f"}
    }
    $i=0;foreach($entry in $cases.GetEnumerator()){$lines=New-Lines;&$entry.Value -x $lines;Expect-Failure {Invoke-Probe $lines ('negative-{0:D2}'-f$i)} $entry.Key;$i++}
    $source=[IO.File]::ReadAllText($verify)
    foreach($needle in @('canonical-route-streaming-io-allocator-bounded-pass','bounded-five-process-no-defect-observed','long_session_leak_check_claimed','Read(?:SVOD|STFS|EntrySVOD) failed','nt_read_pending_success','physical_allocate_results','behavior_patch_required')){if(-not$source.Contains($needle)){throw "Source contract missing '$needle'."}}
    [pscustomobject]@{Passed=$true;PositiveFixtures=1;FailClosedNegatives=$cases.Count;SourceChecks=7}
} finally { if(Test-Path -LiteralPath $root){Remove-Item -LiteralPath $root -Recurse -Force} }
