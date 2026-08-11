[CmdletBinding()]
param([string]$ConfigPath)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $ConfigPath) { $ConfigPath = Join-Path $repoRoot 'config/mcla_functions.toml' }
if (-not (Test-Path -LiteralPath $ConfigPath -PathType Leaf)) { throw "Analysis config was not found: '$ConfigPath'." }
$resolved = (Resolve-Path -LiteralPath $ConfigPath).Path
if ((Get-Item -LiteralPath $resolved -Force).Attributes -band [System.IO.FileAttributes]::ReparsePoint) { throw 'Analysis config must not be a reparse point.' }

$expected = [ordered]@{
    '8220BF08' = @{ end='8220C018'; parent=''; name='sub_8220BF08' }
    '8220C018' = @{ end='8220C0D0'; parent=''; name='sub_8220C018' }
    '822C98B8' = @{ end='822C9948'; parent=''; name='sub_822C98B8' }
    '822C9948' = @{ end='822C9A2C'; parent=''; name='sub_822C9948' }
    '823F32E8' = @{ end='823F3300'; parent=''; name='sub_823F32E8' }
    '823FD718' = @{ end='823FD720'; parent=''; name='sub_823FD718' }
    '822B88C8' = @{ end='822B88DC'; parent=''; name='sub_822B88C8' }
    '824B0DE8' = @{ end='824B0DF8'; parent='824B0CC0'; name='sub_824B0DE8' }
    '827A7FD0' = @{ end='827A7FF0'; parent=''; name='sub_827A7FD0' }
    '827A8220' = @{ end='827A8240'; parent=''; name='sub_827A8220' }
    '827AD168' = @{ end='827AD178'; parent=''; name='sub_827AD168' }
    '827AFC78' = @{ end='827AFC88'; parent=''; name='sub_827AFC78' }
    '827B0538' = @{ end='827B0558'; parent=''; name='sub_827B0538' }
    '827B0558' = @{ end='827B0578'; parent=''; name='sub_827B0558' }
    '827B0578' = @{ end='827B0598'; parent=''; name='sub_827B0578' }
    '827B0598' = @{ end='827B05B8'; parent=''; name='sub_827B0598' }
    '827B1048' = @{ end='827B1068'; parent=''; name='sub_827B1048' }
    '827B1068' = @{ end='827B1088'; parent=''; name='sub_827B1068' }
    '827B4B58' = @{ end='827B4B78'; parent=''; name='sub_827B4B58' }
    '827B4B78' = @{ end='827B4B98'; parent=''; name='sub_827B4B78' }
}
$seen = @{}
$inFunctions = $false
foreach ($raw in Get-Content -LiteralPath $resolved) {
    $line = $raw.Trim()
    if (-not $line -or $line.StartsWith('#')) { continue }
    if ($line -ceq '[functions]') {
        if ($inFunctions) { throw 'Duplicate [functions] section.' }
        $inFunctions = $true
        continue
    }
    if (-not $inFunctions -or $line -notmatch '^"0x([0-9A-F]{8})"\s*=\s*\{\s*(?:parent\s*=\s*0x([0-9A-F]{8}),\s*)?end\s*=\s*0x([0-9A-F]{8}),\s*name\s*=\s*"([A-Za-z0-9_]+)"\s*\}$') {
        throw "Unsupported analysis-config line: '$line'."
    }
    $address=[string]$Matches[1]; $parent=[string]$Matches[2]; $end=[string]$Matches[3]; $name=[string]$Matches[4]
    if ($seen.ContainsKey($address)) { throw "Duplicate function address: 0x$address." }
    if (-not $expected.Contains($address)) { throw "Unreviewed function address: 0x$address." }
    $want=$expected[$address]
    if ($end -cne $want.end -or $parent -cne $want.parent -or $name -cne $want.name) { throw "Function rationale fields changed at 0x$address." }
    $seen[$address]=$true
}
if (-not $inFunctions -or $seen.Count -ne $expected.Count) { throw "Expected $($expected.Count) reviewed functions, found $($seen.Count)." }

$text=Get-Content -LiteralPath $resolved -Raw
if ($text -match '(?m)^\s*(\[\[?(switch_tables|invalid_instructions|analysis)|setjmp_address|longjmp_address)') {
    throw 'No switch table, invalid region, analysis hint, or jump override is justified.'
}
[pscustomobject]@{ Validated=$true; Functions=$seen.Count; Chunks=1; SwitchTables=0; InvalidRegions=0; ExceptionHints=0; JumpOverrides=0; Sha256=(Get-FileHash $resolved -Algorithm SHA256).Hash }
