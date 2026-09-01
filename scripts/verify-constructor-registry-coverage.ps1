[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RegistryAuditPath,
    [string]$GeneratedRegisterPath,
    [int]$ExpectedTargetCount = 102
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $GeneratedRegisterPath) {
    $GeneratedRegisterPath = Join-Path $repoRoot 'generated/default/mcla_register.cpp'
}

function Resolve-RegularFile([string]$Path, [string]$Role) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "$Role was not found: '$Path'." }
    $resolved = (Resolve-Path -LiteralPath $Path).Path
    if ((Get-Item -LiteralPath $resolved -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
        throw "$Role must not be a reparse point."
    }
    $resolved
}

$audit = [IO.File]::ReadAllText((Resolve-RegularFile $RegistryAuditPath 'Constructor-registry audit'))
$generated = [IO.File]::ReadAllText((Resolve-RegularFile $GeneratedRegisterPath 'Generated function registry'))
$targets = @([regex]::Matches($audit, 'Function_[0-9A-Fa-f]{8}\([^\r\n]*?0xffffffff([0-9A-Fa-f]{8})\)') |
    ForEach-Object { $_.Groups[1].Value.ToUpperInvariant() } | Sort-Object -Unique)
$registrations = @([regex]::Matches($generated, 'SetFunction\(0x([0-9A-Fa-f]{8}),') |
    ForEach-Object { $_.Groups[1].Value.ToUpperInvariant() } | Sort-Object -Unique)

if ($ExpectedTargetCount -lt 1 -or $targets.Count -ne $ExpectedTargetCount) {
    throw "Expected $ExpectedTargetCount unique constructor-registry targets, found $($targets.Count)."
}
if (-not $registrations.Count) { throw 'Generated function registry contains no registrations.' }
$missing = @($targets | Where-Object { $_ -notin $registrations })
if ($missing.Count) { throw "Generated dispatcher is missing constructor-registry target(s): $($missing -join ', ')." }

[pscustomobject][ordered]@{
    Verified = $true
    ConstructorTargets = $targets.Count
    GeneratedRegistrations = $registrations.Count
    MissingTargets = 0
    AuditSha256 = (Get-FileHash -LiteralPath $RegistryAuditPath -Algorithm SHA256).Hash
    GeneratedRegistrySha256 = (Get-FileHash -LiteralPath $GeneratedRegisterPath -Algorithm SHA256).Hash
}
