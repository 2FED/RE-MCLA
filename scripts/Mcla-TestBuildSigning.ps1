#requires -Version 7.0
# Offline owner-side signing only. No network and no private-key serialization.
# Trusted local filesystem; path checks/pinned reads are not a hostile-user sandbox.
Set-StrictMode -Version Latest

$script:MclaTestBuildKeyName = 'MCLA-R-TestBuild-Leases-v1'
$script:MclaTestBuildMinimumUtc = [long]1577836800
$script:MclaTestBuildMaximumUtc = [long]4102444800
# Launcher capability tokens a ticket may grant (M9-005), in canonical order.
# 720p and stock 30 FPS are always available and have no token.
$script:MclaTestBuildCapabilityTokens = @('1080p', '1440p', '2160p', 'unlocked')

function Assert-MclaTestBuildWindows {
    if (-not $IsWindows) { throw 'Test-build signing requires PowerShell 7 on Windows.' }
    if ([Environment]::Version.Major -lt 5) { throw 'Test-build signing requires .NET5+ (PowerShell7.1 or newer).' }
}

function ConvertTo-MclaTestBuildUtc([string]$Value) {
    if ($Value -cnotmatch '\A[1-9][0-9]{9}\z') { throw 'UTC must be canonical decimal Unix seconds.' }
    $parsed = [long]::Parse($Value, [Globalization.CultureInfo]::InvariantCulture)
    if ($parsed -lt $script:MclaTestBuildMinimumUtc -or $parsed -gt $script:MclaTestBuildMaximumUtc) {
        throw 'UTC is outside the inclusive 2020-01-01 to 2100-01-01 range.'
    }
    return $parsed.ToString([Globalization.CultureInfo]::InvariantCulture)
}

# Returns the canonical comma-separated token list (fixed order) or '' for none.
# Unknown, empty or repeated tokens are refused rather than dropped.
function ConvertTo-MclaTestBuildCapabilities([string[]]$Capabilities) {
    if ($null -eq $Capabilities -or $Capabilities.Count -eq 0) { return '' }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($token in $Capabilities) {
        if ($token -cnotin $script:MclaTestBuildCapabilityTokens) {
            throw "Unknown capability token '$token'; allowed: 1080p, 1440p, 2160p, unlocked."
        }
        if (-not $seen.Add($token)) { throw "Duplicate capability token '$token'." }
    }
    return (@($script:MclaTestBuildCapabilityTokens | Where-Object { $seen.Contains($_) }) -join ',')
}

function Assert-MclaTestBuildHash([string]$Hash) {
    if ($Hash -cnotmatch '\A[0-9A-F]{64}\z') { throw 'Host SHA256 must be exactly 64 uppercase hexadecimal characters.' }
}

function Open-MclaTestBuildSigner {
    Assert-MclaTestBuildWindows
    $provider = [Security.Cryptography.CngProvider]::MicrosoftSoftwareKeyStorageProvider
    # Open never creates a missing key. No MachineKey option: current-user scope.
    $key = [Security.Cryptography.CngKey]::Open($script:MclaTestBuildKeyName, $provider,
        [Security.Cryptography.CngKeyOpenOptions]::None)
    try {
        if ($key.Algorithm -ne [Security.Cryptography.CngAlgorithm]::Rsa -or
            $key.ExportPolicy -ne [Security.Cryptography.CngExportPolicies]::None -or
            $key.KeyUsage -ne [Security.Cryptography.CngKeyUsages]::Signing -or $key.IsMachineKey) {
            throw 'Existing signing key does not have the required nonexportable current-user RSA signing policy.'
        }
        $rsa = [Security.Cryptography.RSACng]::new($key)
        if ($rsa.KeySize -ne 2048) { $rsa.Dispose(); throw 'Signing key must be RSA2048.' }
        return $rsa
    } finally { $key.Dispose() }
}

function Get-MclaTestBuildPublicBlobHex([Security.Cryptography.RSA]$Signer) {
    if ($Signer.KeySize -ne 2048) { throw 'Public key must be RSA2048.' }
    $public = $Signer.ExportParameters($false)
    if ($public.Modulus.Length -ne 256 -or $public.Exponent.Length -ne 3 -or
        [Convert]::ToHexString($public.Exponent) -cne '010001') {
        throw 'Expected RSA2048 with exponent 65537.'
    }
    $memory = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($memory)
    try {
        # BCRYPT_RSAKEY_BLOB header is six little-endian ULONGs; integer bodies
        # are unsigned big-endian. Public parts only; p/q lengths remain zero.
        foreach ($value in @([uint32]0x31415352, [uint32]2048, [uint32]3, [uint32]256, [uint32]0, [uint32]0)) {
            $writer.Write([uint32]$value)
        }
        $writer.Write([byte[]]$public.Exponent)
        $writer.Write([byte[]]$public.Modulus)
        $writer.Flush()
        return [Convert]::ToHexString($memory.ToArray())
    } finally { $writer.Dispose(); $memory.Dispose() }
}

function New-MclaTestBuildSignedEnvelope([Security.Cryptography.RSA]$Signer, [string]$Payload) {
    if ($Signer.KeySize -ne 2048) { throw 'Signing requires RSA2048.' }
    # Revalidate at the signing boundary, even if a caller bypasses builders.
    if ($Payload.Contains("`r") -or $Payload -cmatch '[^\x00-\x7F]' -or -not $Payload.EndsWith("`n")) {
        throw 'Signing payload must be LF-terminated ASCII without carriage returns.'
    }
    $lines = $Payload.Split([char]10)
    if ($lines[0] -ceq 'MCLA-TEST-BUILD-V1' -or $lines[0] -ceq 'MCLA-TEST-BUILD-V2') {
        # Optional fourth line: the signed launcher capabilities grant (M9-005).
        if (($lines.Count -ne 3 -and $lines.Count -ne 4) -or $lines[1] -cnotmatch '\A([0-9A-F]{64}) ([1-9][0-9]{9})\z') {
            throw 'Noncanonical ticket signing payload.'
        }
        $null = ConvertTo-MclaTestBuildUtc $Matches[2]
        if ($lines.Count -eq 4) {
            if ($lines[2] -cnotmatch '\Acapabilities ([0-9a-z,]+)\z') { throw 'Noncanonical ticket capabilities line.' }
            $tokens = $Matches[1]
            if ((ConvertTo-MclaTestBuildCapabilities $tokens.Split(',')) -cne $tokens) {
                throw 'Ticket capabilities must be known, unique and in canonical order.'
            }
        }
    } elseif ($lines[0] -ceq 'MCLA-TEST-LEASES-V1') {
        if ($lines.Count -lt 3 -or $lines.Count -gt 259) { throw 'Noncanonical lease payload entry count.' }
        $null = ConvertTo-MclaTestBuildUtc $lines[1]
        $previous = $null
        for ($i = 2; $i -lt $lines.Count - 1; ++$i) {
            if ($lines[$i] -cnotmatch '\A([0-9A-F]{64}) ([1-9][0-9]{9})\z') { throw 'Noncanonical lease row.' }
            $hash = $Matches[1]
            $null = ConvertTo-MclaTestBuildUtc $Matches[2]
            if ($null -ne $previous -and [StringComparer]::Ordinal.Compare($previous, $hash) -ge 0) {
                throw 'Lease payload hashes must be strictly sorted and unique.'
            }
            $previous = $hash
        }
    } else { throw 'Unknown signing payload type.' }
    $signature = $Signer.SignData([Text.Encoding]::ASCII.GetBytes($Payload),
        [Security.Cryptography.HashAlgorithmName]::SHA256, [Security.Cryptography.RSASignaturePadding]::Pkcs1)
    if ($signature.Length -ne 256) { throw 'Expected exactly 256 signature bytes.' }
    $envelope = $Payload + "--SIGNATURE--`n" + [Convert]::ToHexString($signature) + "`n"
    if ([Text.Encoding]::ASCII.GetByteCount($envelope) -gt 65536) { throw 'Signed envelope exceeds 64 KiB.' }
    return $envelope
}

# -KillSwitch issues a V2 ticket: 3-day offline start, online list authoritative (M9-005).
# -Capabilities adds the signed `capabilities` line that admits the listed
# gated launcher options for this exact host; omitted, no line is written.
function New-MclaTestBuildTicketEnvelope([Security.Cryptography.RSA]$Signer, [string]$HostSha256, [string]$CreatedUtc,
        [switch]$KillSwitch, [string[]]$Capabilities = @()) {
    Assert-MclaTestBuildHash $HostSha256
    $created = ConvertTo-MclaTestBuildUtc $CreatedUtc
    $schema = if ($KillSwitch) { 'MCLA-TEST-BUILD-V2' } else { 'MCLA-TEST-BUILD-V1' }
    $grant = ConvertTo-MclaTestBuildCapabilities $Capabilities
    $payload = $schema + "`n" + $HostSha256 + ' ' + $created + "`n"
    if ($grant) { $payload += 'capabilities ' + $grant + "`n" }
    return New-MclaTestBuildSignedEnvelope $Signer $payload
}

function New-MclaTestBuildLeaseEnvelope([Security.Cryptography.RSA]$Signer, [string]$EntriesJson, [string]$IssuedUtc) {
    $issued = ConvertTo-MclaTestBuildUtc $IssuedUtc
    $document = [Text.Json.JsonDocument]::Parse($EntriesJson)
    try {
        if ($document.RootElement.ValueKind -ne [Text.Json.JsonValueKind]::Array -or
            $document.RootElement.GetArrayLength() -gt 256) { throw 'Lease input must be a JSON array with 0 to 256 entries.' }
        $entries = [Collections.Generic.SortedDictionary[string,string]]::new([StringComparer]::Ordinal)
        foreach ($entry in $document.RootElement.EnumerateArray()) {
            if ($entry.ValueKind -ne [Text.Json.JsonValueKind]::Object) { throw 'Every lease must be an object.' }
            $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
            foreach ($property in $entry.EnumerateObject()) {
                if (-not $names.Add($property.Name) -or $property.Name -cnotin @('host_sha256', 'valid_until')) {
                    throw 'Lease fields must be unique and exactly host_sha256/valid_until.'
                }
            }
            if ($names.Count -ne 2) { throw 'Lease requires host_sha256 and valid_until.' }
            $hashValue = $entry.GetProperty('host_sha256')
            $dateValue = $entry.GetProperty('valid_until')
            if ($hashValue.ValueKind -ne [Text.Json.JsonValueKind]::String -or
                $dateValue.ValueKind -ne [Text.Json.JsonValueKind]::Number) { throw 'Lease hash must be a string and valid_until an integer.' }
            $hash = $hashValue.GetString()
            Assert-MclaTestBuildHash $hash
            $date = ConvertTo-MclaTestBuildUtc $dateValue.GetRawText()
            if ($entries.ContainsKey($hash)) { throw 'Duplicate lease host SHA256 is forbidden.' }
            $entries.Add($hash, $date)
        }
        $payload = "MCLA-TEST-LEASES-V1`n" + $issued + "`n"
        foreach ($pair in $entries.GetEnumerator()) { $payload += $pair.Key + ' ' + $pair.Value + "`n" }
        return New-MclaTestBuildSignedEnvelope $Signer $payload
    } finally { $document.Dispose() }
}

function Get-MclaTestBuildLocalPath([string]$Path) {
    Assert-MclaTestBuildWindows
    if ($Path -notmatch '^[A-Za-z]:[\\/]' -or $Path.Substring(2).Contains(':')) {
        throw 'Signing inputs/outputs require explicit absolute local paths without alternate streams.'
    }
    $full = [IO.Path]::GetFullPath($Path)
    if ([IO.DriveInfo]::new([IO.Path]::GetPathRoot($full)).DriveType -ne [IO.DriveType]::Fixed) {
        throw 'Signing inputs/outputs require a local fixed drive.'
    }
    $parent = [IO.DirectoryInfo]::new([IO.Path]::GetDirectoryName($full))
    for ($cursor = $parent; $null -ne $cursor; $cursor = $cursor.Parent) {
        if (-not $cursor.Exists -or ($cursor.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Signing path ancestors must exist and must not be redirected.'
        }
    }
    return $full
}

function Open-MclaTestBuildPinnedInput([string]$Path, [long]$MaximumBytes = 2147483648) {
    $full = Get-MclaTestBuildLocalPath $Path
    $item = Get-Item -LiteralPath $full -Force -ErrorAction Stop
    if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Signing input must be a regular nonredirected file.'
    }
    $streams = @(Get-Item -LiteralPath $full -Stream '*' -ErrorAction Stop)
    if ($streams.Count -ne 1 -or $streams[0].Stream -cne ':$DATA') { throw 'Signing input must not contain alternate streams.' }
    $stream = [IO.File]::Open($full, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    if ($stream.Length -le 0 -or $stream.Length -gt $MaximumBytes) { $stream.Dispose(); throw 'Signing input has invalid size.' }
    return $stream
}

function Get-MclaTestBuildPinnedHash([IO.Stream]$Stream) {
    $Stream.Position = 0
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [Convert]::ToHexString($sha.ComputeHash($Stream)) }
    finally { $sha.Dispose(); $Stream.Position = 0 }
}

function Write-MclaTestBuildNewEnvelope([string]$OutputPath, [string]$Envelope) {
    if ($Envelope -cmatch '[^\x00-\x7F]' -or [Text.Encoding]::ASCII.GetByteCount($Envelope) -gt 65536) {
        throw 'Output must be ASCII and no larger than 64 KiB.'
    }
    $full = Get-MclaTestBuildLocalPath $OutputPath
    # CREATE_NEW refuses an existing file, directory, hardlink or reparse entry.
    # On disk failure preserve the partial new artifact; never overwrite/retry.
    $stream = [IO.File]::Open($full, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $bytes = [Text.Encoding]::ASCII.GetBytes($Envelope)
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.Flush($true)
    } finally { $stream.Dispose() }
}
