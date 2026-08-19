[CmdletBinding()]param()
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
$verifier=Join-Path $PSScriptRoot 'verify-reached-unsupported-report.ps1';$root=Join-Path $env:TEMP ('m6-013-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory $root|Out-Null
try{
  $base=@(
    'KernelState: Preparing module launch',
    '[UNAVAILABLE] XeKeysConsolePrivateKeySign has no console private key; caller buffer preserved',
    '[COMPAT] IoDismountVolumeByFileHandle accepted; host VFS mount is runtime-owned',
    'D3D12 guest present: successful sequence count=3',
    'Window closing, shutting down...',
    'Execution complete',
    'Title terminated; hard-exiting process.')
  $path=Join-Path $root 'mcla.log';[IO.File]::WriteAllLines($path,$base,[Text.UTF8Encoding]::new($false));$p=&$verifier -LogPath $path;if(-not$p.Passed){throw 'Positive fixture failed.'}
  $cases=@(
    @{n='missing-launch';f={param($x)@($x|?{$_-notmatch'Preparing module'})}},@{n='duplicate-sign';f={param($x)$x+@($x[1])}},
    @{n='missing-dismount';f={param($x)@($x|?{$_-notmatch'IoDismount'})}},@{n='wrong-order';f={param($x)@($x[0],$x[2],$x[1])+$x[3..6]}},
    @{n='legacy-sign';f={param($x)$x+'XeKeysConsolePrivateKeySign - stub'}},@{n='legacy-dismount';f={param($x)$x+'IoDismountVolumeByFileHandle(HANDLE) - stub'}},
    @{n='ppc';f={param($x)$x+'PPC_UNIMPLEMENTED'}},@{n='invalid-target';f={param($x)$x+'invalid/unregistered guest'}},
    @{n='generic-stub';f={param($x)$x+'[STUB] fixture'}},@{n='fatal';f={param($x)$x+'[FATAL] fixture'}},
    @{n='dred';f={param($x)$x+'DRED fixture'}},@{n='missing-close';f={param($x)@($x|?{$_-notmatch'Window closing'})}},
    @{n='missing-exec';f={param($x)@($x|?{$_-notmatch'Execution complete'})}},@{n='duplicate-hard-exit';f={param($x)$x+@($x[-1])}})
  $failed=0;foreach($c in $cases){$q=Join-Path $root ($c.n+'.log');[IO.File]::WriteAllLines($q,(&$c.f $base),[Text.UTF8Encoding]::new($false));try{&$verifier -LogPath $q|Out-Null;throw "Negative accepted: $($c.n)"}catch{if($_.Exception.Message-like'Negative accepted:*'){throw};$failed++}}
  $source=&$verifier -SourceOnly;if(-not$source.Passed){throw 'Source contract failed.'}
  [pscustomobject]@{Passed=$true;PositiveFixtures=1;FailClosedNegatives=$failed;SourceChecks=$source.SourceChecks}
}finally{Remove-Item $root -Recurse -Force -ErrorAction SilentlyContinue}
