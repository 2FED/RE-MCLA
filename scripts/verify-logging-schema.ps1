[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$LogPath,
    [Parameter(Mandatory)]
    [ValidateSet('app', 'ppc', 'kernel', 'xam', 'vfs', 'gpu', 'audio', 'input', 'patches')]
    [string]$ExpectedCategory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
    throw "Structured log was not found: '$LogPath'."
}
$logItem = Get-Item -LiteralPath $LogPath
if ($logItem.Length -gt 65536) {
    throw 'Structured logging probe exceeded the reviewed 64-KiB bound.'
}

$log = Get-Content -LiteralPath $LogPath -Raw
$schemaMatches = [regex]::Matches(
    $log,
    'MCLA_LOG_SCHEMA schema=1 category=([a-z]+) event=probe'
)
if ($schemaMatches.Count -ne 1) {
    throw "Expected exactly one structured schema marker, found $($schemaMatches.Count)."
}
$actualCategory = $schemaMatches[0].Groups[1].Value
if ($actualCategory -ne $ExpectedCategory) {
    throw "Expected category '$ExpectedCategory', got '$actualCategory'."
}
$categoryPattern = "(?m)^.*\[info\] \[$([regex]::Escape($ExpectedCategory))\] \[t[0-9]+\] " +
    "MCLA_LOG_SCHEMA schema=1 category=$([regex]::Escape($ExpectedCategory)) event=probe\r?$"
if ([regex]::Matches($log, $categoryPattern).Count -ne 1) {
    throw "Structured marker was not written through the '$ExpectedCategory' logger."
}

$bannedPatterns = @(
    '(?i)\[(error|critical)\]',
    '(?i)\[FATAL\]',
    '(?i)[A-Z]:\\',
    '(?i)\\Users\\',
    '(?i)\\private\\',
    '(?i)default[.]xex'
)
foreach ($pattern in $bannedPatterns) {
    if ($log -match $pattern) {
        throw "Structured log contains banned probe pattern '$pattern'."
    }
}

[pscustomobject]@{
    Passed = $true
    Schema = 1
    Category = $actualCategory
    SchemaMarkers = $schemaMatches.Count
    LogBytes = $logItem.Length
}
