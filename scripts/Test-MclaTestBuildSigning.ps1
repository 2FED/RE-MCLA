#requires -Version 7.0
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Mcla-TestBuildSigning.ps1')
Assert-MclaTestBuildWindows
$script:checks = 0
function Assert-Test([bool]$Condition, [string]$Label) {
    ++$script:checks
    if (-not $Condition) { throw "FAIL: $Label" }
}
function Assert-Rejected([scriptblock]$Action, [string]$Label) {
    $rejected = $false
    try { $null = & $Action } catch { $rejected = $true }
    Assert-Test $rejected $Label
}
function Assert-Envelope([Security.Cryptography.RSA]$Verifier, [string]$Envelope, [string]$ExpectedPayload) {
    $pieces = $Envelope.Split('--SIGNATURE--' + "`n", [StringSplitOptions]::None)
    Assert-Test ($pieces.Count -eq 2 -and $pieces[0] -ceq $ExpectedPayload) 'exact canonical signed payload'
    Assert-Test ($pieces[1] -cmatch '\A[0-9A-F]{512}\n\z') 'exact uppercase signature and terminal LF'
    $signature = [Convert]::FromHexString($pieces[1].TrimEnd([char]10))
    Assert-Test ($Verifier.VerifyData([Text.Encoding]::ASCII.GetBytes($pieces[0]), $signature,
        [Security.Cryptography.HashAlgorithmName]::SHA256, [Security.Cryptography.RSASignaturePadding]::Pkcs1)) 'RSA2048 PKCS1 SHA256 verifies'
    Assert-Test (-not $Verifier.VerifyData([Text.Encoding]::ASCII.GetBytes($pieces[0] + 'x'), $signature,
        [Security.Cryptography.HashAlgorithmName]::SHA256, [Security.Cryptography.RSASignaturePadding]::Pkcs1)) 'payload mutation rejected'
    Assert-Test (-not $Envelope.Contains("`r") -and [Text.Encoding]::ASCII.GetByteCount($Envelope) -le 65536) 'LF ASCII envelope bounded64KiB'
}

# This constructor creates an UNNAMED ephemeral key. Never run the durable key
# creation script or Open-MclaTestBuildSigner here, even if a named key exists.
$rsa = [Security.Cryptography.RSACng]::new(2048)
$publicKey = $null
$verifier = $null
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('mcla-test-build-signing-' + [Guid]::NewGuid().ToString('N'))
$null = [IO.Directory]::CreateDirectory($fixture)
try {
    Assert-Test $rsa.Key.IsEphemeral 'test RSA key is unnamed/ephemeral'
    $hex = Get-MclaTestBuildPublicBlobHex $rsa
    $blob = [Convert]::FromHexString($hex)
    Assert-Test ($blob.Length -eq 283 -and $hex -cmatch '^[0-9A-F]{566}$') 'public BCRYPT blob exact size/encoding'
    Assert-Test ([BitConverter]::ToUInt32($blob, 0) -eq 0x31415352 -and
        [BitConverter]::ToUInt32($blob, 4) -eq 2048 -and [BitConverter]::ToUInt32($blob, 8) -eq 3 -and
        [BitConverter]::ToUInt32($blob, 12) -eq 256 -and [BitConverter]::ToUInt32($blob, 16) -eq 0 -and
        [BitConverter]::ToUInt32($blob, 20) -eq 0) 'public header RSA1/2048/exponent/modulus/no private primes'
    Assert-Test ([Convert]::ToHexString($blob[24..26]) -ceq '010001') 'public exponent65537 big-endian'
    $publicKey = [Security.Cryptography.CngKey]::Import($blob, [Security.Cryptography.CngKeyBlobFormat]::GenericPublicBlob)
    $verifier = [Security.Cryptography.RSACng]::new($publicKey)
    $hashA = 'A' * 64
    $hashB = 'B' * 64
    $ticket = New-MclaTestBuildTicketEnvelope $rsa $hashA '1577836800'
    Assert-Envelope $verifier $ticket ("MCLA-TEST-BUILD-V1`n" + $hashA + " 1577836800`n")
    Assert-Test ((New-MclaTestBuildTicketEnvelope $rsa $hashA '1577836800') -ceq $ticket) 'PKCS1 signing deterministic for canonical input'
    $null = New-MclaTestBuildTicketEnvelope $rsa $hashA '4102444800'
    Assert-Test $true 'inclusive upper date accepted'
    foreach ($invalid in @('1577836799', '4102444801', '01577836800', '+1577836800', '1577836800.0', '1.5778368e9', ' 1577836800', "1577836800`n", '')) {
        Assert-Rejected { New-MclaTestBuildTicketEnvelope $rsa $hashA $invalid } "noncanonical/out-of-bounds date rejected [$invalid]"
    }
    foreach ($invalid in @(('a' * 64), ('A' * 63), ('G' * 64), (($hashA) + "`n"))) {
        Assert-Rejected { New-MclaTestBuildTicketEnvelope $rsa $invalid '1577836800' } 'invalid hash rejected'
    }
    $entries = '[{"host_sha256":"' + $hashB + '","valid_until":4102444800},{"host_sha256":"' + $hashA + '","valid_until":1577836800}]'
    $leases = New-MclaTestBuildLeaseEnvelope $rsa $entries '1800000000'
    Assert-Envelope $verifier $leases ("MCLA-TEST-LEASES-V1`n1800000000`n" + $hashA + " 1577836800`n" + $hashB + " 4102444800`n")
    Assert-Envelope $verifier (New-MclaTestBuildLeaseEnvelope $rsa '[]' '1800000000') "MCLA-TEST-LEASES-V1`n1800000000`n"
    $one = '{"host_sha256":"' + $hashA + '","valid_until":1800000000}'
    foreach ($invalid in @('{}', 'null', '[null]', '[1]', ('[' + $one + ',' + $one + ']'),
        ('[{"host_sha256":"' + $hashA + '","valid_until":1800000000,"extra":0}]'),
        ('[{"host_sha256":"' + $hashA + '","valid_until":1800000000,"valid_until":1800000001}]'),
        ('[{"host_sha256":"' + $hashA + '"}]'),
        ('[{"host_sha256":"' + $hashA + '","valid_until":"1800000000"}]'),
        ('[{"host_sha256":"' + $hashA + '","valid_until":1.8e9}]'),
        ('[{"host_sha256":"' + $hashA + '","valid_until":1800000000.0}]'),
        ('[' + $one + ',]'), ('[/*comment*/' + $one + ']'))) {
        Assert-Rejected { New-MclaTestBuildLeaseEnvelope $rsa $invalid '1800000000' } 'malformed/duplicate/noncanonical JSON lease rejected'
    }
    $rows = [Collections.Generic.List[string]]::new()
    for ($i = 0; $i -lt 257; ++$i) {
        $rows.Add('{"host_sha256":"' + $i.ToString('X64') + '","valid_until":1800000000}')
    }
    $maxLeases = New-MclaTestBuildLeaseEnvelope $rsa ('[' + ($rows[0..255] -join ',') + ']') '1800000000'
    Assert-Test ($maxLeases.Length -lt 65536) '256 leases accepted within64KiB'
    Assert-Rejected { New-MclaTestBuildLeaseEnvelope $rsa ('[' + ($rows -join ',') + ']') '1800000000' } '257 leases rejected'
    foreach ($invalid in @("UNKNOWN`n", ("MCLA-TEST-BUILD-V1`r`n" + $hashA + " 1800000000`n"),
        ("MCLA-TEST-LEASES-V1`n1800000000`n" + $hashB + " 1800000000`n" + $hashA + " 1800000000`n"),
        ("MCLA-TEST-BUILD-V1`n" + $hashA + " 1800000000`n`n"))) {
        Assert-Rejected { New-MclaTestBuildSignedEnvelope $rsa $invalid } 'direct signing primitive rejects malformed payload'
    }
    $output = Join-Path $fixture 'ticket.dat'
    Write-MclaTestBuildNewEnvelope $output $ticket
    Assert-Test ([IO.File]::ReadAllText($output) -ceq $ticket) 'ticket roundtrip without BOM/line conversion'
    Assert-Rejected { Write-MclaTestBuildNewEnvelope $output $leases } 'existing output not overwritten'
    Assert-Test ([IO.File]::ReadAllText($output) -ceq $ticket) 'existing ticket retained byte for byte'
    Assert-Rejected { Write-MclaTestBuildNewEnvelope (Join-Path $fixture 'missing/lease.dat') $leases } 'missing output parent not auto-created'
    Assert-Test (-not [IO.Directory]::Exists((Join-Path $fixture 'missing'))) 'missing parent remains absent'
    $hostPath = Join-Path $fixture 'synthetic-host.bin'
    [IO.File]::WriteAllBytes($hostPath, [byte[]](0,1,2,3,4,5,6,7,8,9))
    $pin = Open-MclaTestBuildPinnedInput $hostPath
    try {
        $before = Get-MclaTestBuildPinnedHash $pin
        Assert-Rejected {
            $writer = [IO.File]::Open($hostPath, [IO.FileMode]::Open, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite)
            $writer.Dispose()
        } 'read pin denies concurrent source writer'
        Assert-Rejected { [IO.File]::Move($hostPath, (Join-Path $fixture 'moved-host.bin')) } 'read pin denies source rename'
        $fromSource = New-MclaTestBuildTicketEnvelope $rsa $before '1800000000'
        Assert-Test ($fromSource.StartsWith("MCLA-TEST-BUILD-V1`n" + $before + ' ')) 'ticket binds actual pinned source SHA256'
        Assert-Test ((Get-MclaTestBuildPinnedHash $pin) -ceq $before) 'source remains unchanged across signing'
    } finally { $pin.Dispose() }
    Assert-Rejected { Open-MclaTestBuildPinnedInput $hostPath 1 } 'input size bound enforced'
    [IO.File]::WriteAllText($hostPath + ':fixture', 'synthetic ADS')
    Assert-Rejected { Open-MclaTestBuildPinnedInput $hostPath } 'alternate-stream input rejected'
    Assert-Rejected { Write-MclaTestBuildNewEnvelope ($output + ':fixture') $ticket } 'alternate-stream output path rejected'
    $small = [Security.Cryptography.RSACng]::new(1024)
    try { Assert-Rejected { New-MclaTestBuildTicketEnvelope $small $hashA '1800000000' } 'wrong RSA size rejected' }
    finally { $small.Dispose() }
    [pscustomobject]@{ result = 'PASS'; checks = $script:checks; fixture = $fixture; durable_key_created = $false }
} finally {
    if ($null -ne $verifier) { $verifier.Dispose() }
    if ($null -ne $publicKey) { $publicKey.Dispose() }
    $rsa.Dispose()
    # Synthetic evidence is intentionally retained; no recursive deletion.
}
