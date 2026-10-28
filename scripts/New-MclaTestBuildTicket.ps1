#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$HostPath,
    [Parameter(Mandatory)][string]$OutputPath,
    [string]$CreatedUtc = ([DateTimeOffset]::UtcNow.ToUnixTimeSeconds().ToString([Globalization.CultureInfo]::InvariantCulture)),
    # Volunteer builds: 3-day offline start, online list authoritative (M9-005).
    [switch]$KillSwitch
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Mcla-TestBuildSigning.ps1')
$hostStream = Open-MclaTestBuildPinnedInput $HostPath
$rsa = $null
try {
    $hash = Get-MclaTestBuildPinnedHash $hostStream
    $rsa = Open-MclaTestBuildSigner
    $envelope = New-MclaTestBuildTicketEnvelope $rsa $hash $CreatedUtc -KillSwitch:$KillSwitch
    if ((Get-MclaTestBuildPinnedHash $hostStream) -cne $hash) { throw 'Pinned host changed during signing; no ticket published.' }
    Write-MclaTestBuildNewEnvelope $OutputPath $envelope
    [pscustomobject]@{ kind = $(if ($KillSwitch) { 'MCLA-TEST-BUILD-V2' } else { 'MCLA-TEST-BUILD-V1' }); host_sha256 = $hash; created_utc = $CreatedUtc; written = $true }
} finally { if ($null -ne $rsa) { $rsa.Dispose() }; $hostStream.Dispose() }
