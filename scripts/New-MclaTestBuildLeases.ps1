#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$EntriesPath,
    [Parameter(Mandatory)][string]$OutputPath,
    [string]$IssuedUtc = ([DateTimeOffset]::UtcNow.ToUnixTimeSeconds().ToString([Globalization.CultureInfo]::InvariantCulture))
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Mcla-TestBuildSigning.ps1')
$inputStream = Open-MclaTestBuildPinnedInput $EntriesPath 65536
$rsa = $null
try {
    $originalHash = Get-MclaTestBuildPinnedHash $inputStream
    $reader = [IO.StreamReader]::new($inputStream, [Text.UTF8Encoding]::new($false, $true), $false, 4096, $true)
    try { $json = $reader.ReadToEnd() } finally { $reader.Dispose() }
    $rsa = Open-MclaTestBuildSigner
    $envelope = New-MclaTestBuildLeaseEnvelope $rsa $json $IssuedUtc
    if ((Get-MclaTestBuildPinnedHash $inputStream) -cne $originalHash) { throw 'Pinned lease source changed; no publication.' }
    Write-MclaTestBuildNewEnvelope $OutputPath $envelope
    [pscustomobject]@{ kind = 'MCLA-TEST-LEASES-V1'; issued_utc = $IssuedUtc; written = $true }
} finally { if ($null -ne $rsa) { $rsa.Dispose() }; $inputStream.Dispose() }
