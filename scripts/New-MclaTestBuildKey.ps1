#requires -Version 7.0
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Mcla-TestBuildSigning.ps1')
Assert-MclaTestBuildWindows
$provider = [Security.Cryptography.CngProvider]::MicrosoftSoftwareKeyStorageProvider
if ([Security.Cryptography.CngKey]::Exists($script:MclaTestBuildKeyName, $provider,
        [Security.Cryptography.CngKeyOpenOptions]::None)) {
    throw 'Signing key already exists; refusing replacement. Existing-key public data can be read with the common helper.'
}
$parameters = [Security.Cryptography.CngKeyCreationParameters]::new()
$parameters.Provider = $provider
$parameters.KeyUsage = [Security.Cryptography.CngKeyUsages]::Signing
$parameters.ExportPolicy = [Security.Cryptography.CngExportPolicies]::None
$parameters.KeyCreationOptions = [Security.Cryptography.CngKeyCreationOptions]::None
$parameters.Parameters.Add([Security.Cryptography.CngProperty]::new('Length', [BitConverter]::GetBytes([int]2048),
    [Security.Cryptography.CngPropertyOptions]::None))
# No overwrite or MachineKey flags. CNG refuses a racing existing named key.
$key = [Security.Cryptography.CngKey]::Create([Security.Cryptography.CngAlgorithm]::Rsa,
    $script:MclaTestBuildKeyName, $parameters)
$rsa = $null
try {
    if ($key.IsEphemeral -or $key.IsMachineKey -or
        $key.ExportPolicy -ne [Security.Cryptography.CngExportPolicies]::None -or
        $key.KeyUsage -ne [Security.Cryptography.CngKeyUsages]::Signing) {
        throw 'Created key policy differs from requested policy; preserve key and investigate, do not replace it.'
    }
    $rsa = [Security.Cryptography.RSACng]::new($key)
    [pscustomobject]@{
        key_name = $script:MclaTestBuildKeyName
        scope = 'current-user'
        private_export = 'forbidden'
        public_bcrypt_rsapublic_blob_hex = Get-MclaTestBuildPublicBlobHex $rsa
    }
} finally { if ($null -ne $rsa) { $rsa.Dispose() }; $key.Dispose() }
