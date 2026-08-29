[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Write-Host 'M6-016 [1/3]: validating the exact alpha build, host, route, and limitation inventory...'
$result = & (Join-Path $PSScriptRoot 'verify-alpha-compatibility-matrix.ps1')

Write-Host 'M6-016 [2/3]: running fail-closed matrix fixtures...'
$fixtures = & (Join-Path $PSScriptRoot 'test-alpha-compatibility-matrix.ps1')

if (-not $result.Passed -or -not $fixtures.Passed) { throw 'Alpha compatibility matrix verification failed.' }

Write-Host 'M6-016 [3/3]: alpha scope is internally consistent.'
[pscustomobject][ordered]@{
    Decision = $result.Decision
    CompatibilityRows = $result.CompatibilityRows
    KnownIssues = $result.KnownIssues
    ReferenceHosts = $result.ReferenceHosts
    FailClosedNegatives = $fixtures.FailClosedNegatives
    BroadClaimsRejected = $result.BroadClaimsRejected
}
