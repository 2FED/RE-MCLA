[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$verify = Join-Path $PSScriptRoot 'verify-constructor-registry-coverage.ps1'
$root = Join-Path ([IO.Path]::GetTempPath()) ('mcla-constructor-registry-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($root) | Out-Null
$utf8 = [Text.UTF8Encoding]::new($false)

function Write-Fixture([string]$Name, [string]$Text) {
    $path = Join-Path $root $Name
    [IO.File]::WriteAllText($path, $Text, $utf8)
    $path
}
function Expect-Rejected([string]$Name, [scriptblock]$Action) {
    try { & $Action; throw "Negative fixture '$Name' was accepted." } catch {
        if ($_.Exception.Message -ceq "Negative fixture '$Name' was accepted.") { throw }
    }
}

try {
    $audit = Write-Fixture 'audit.txt' @'
Function_82554798(0xffffffff82000000,0xffffffff8220b7d0);
Function_82554798(0xffffffff82000004,0xffffffff8220da40);
Function_82554798(0xffffffff82000008,0xffffffff8220daa0);
'@
    $complete = Write-Fixture 'complete.cpp' @'
registrar->SetFunction(0x8220B7D0, sub_8220B7D0);
registrar->SetFunction(0x8220DA40, sub_8220DA40);
registrar->SetFunction(0x8220DAA0, sub_8220DAA0);
'@
    $missing = Write-Fixture 'missing.cpp' @'
registrar->SetFunction(0x8220B7D0, sub_8220B7D0);
registrar->SetFunction(0x8220DAA0, sub_8220DAA0);
'@
    $result = & $verify -RegistryAuditPath $audit -GeneratedRegisterPath $complete -ExpectedTargetCount 3
    if (-not $result.Verified -or $result.ConstructorTargets -ne 3 -or $result.MissingTargets -ne 0) {
        throw 'Positive constructor-registry fixture returned the wrong result.'
    }
    Expect-Rejected 'missing-target' { & $verify -RegistryAuditPath $audit -GeneratedRegisterPath $missing -ExpectedTargetCount 3 | Out-Null }
    Expect-Rejected 'wrong-target-count' { & $verify -RegistryAuditPath $audit -GeneratedRegisterPath $complete -ExpectedTargetCount 4 | Out-Null }
    [pscustomobject]@{ Passed = $true; PositiveCases = 1; NegativeCases = 2 }
}
finally {
    [IO.Directory]::Delete($root, $true)
}
