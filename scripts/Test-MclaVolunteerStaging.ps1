[CmdletBinding()]
param(
    # A staged folder from Stage-MclaInternalLauncher.ps1 (launcher root with build\).
    [Parameter(Mandatory=$true)][string]$StagedDirectory,
    # Extra literal strings that must not appear anywhere (owner names, paths).
    [string[]]$Sentinel = @(),
    # Volunteer artifacts must carry a V2 (kill-switch) ticket and an expiry host.
    [switch]$RequireVolunteer
)
# M9-005 volunteer boundary audit. Read-only: reports findings, changes nothing.
# Checks the exact file allowlist, reparse points/streams, forbidden file types,
# and ASCII/UTF-16 occurrences of owner-identifying strings in every file.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = (Resolve-Path -LiteralPath $StagedDirectory).Path
$allowed = @(
    'MCLA-Launcher.exe', 'candidate-receipt.json',
    'msvcp140.dll', 'msvcp140_atomic_wait.dll', 'vcruntime140.dll', 'vcruntime140_1.dll',
    'build\mcla.exe', 'build\mcla_crash_handler.exe', 'build\mcla_diagnostic_image_helper.exe',
    'build\rexruntime.dll', 'build\rexgpu-xenos.dll', 'build\TracyClient.dll',
    'build\mcla-build.manifest', 'build\mcla-test-build.ticket',
    'build\msvcp140.dll', 'build\msvcp140_atomic_wait.dll', 'build\vcruntime140.dll', 'build\vcruntime140_1.dll',
    'TESTER-GUIDE.txt')
$findings = [Collections.Generic.List[string]]::new()
$checks = 0

$items = @(Get-ChildItem -LiteralPath $root -Recurse -Force)
foreach ($item in $items) {
    $relative = $item.FullName.Substring($root.Length).TrimStart('\')
    $checks++
    if (($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { $findings.Add("reparse point: $relative"); continue }
    if ($item.PSIsContainer) {
        if ($relative -ne 'build') { $findings.Add("unexpected directory: $relative") }
        continue
    }
    if ($allowed -notcontains $relative) { $findings.Add("file not on the allowlist: $relative") }
    $streams = @(Get-Item -LiteralPath $item.FullName -Stream '*')
    if ($streams.Count -ne 1) { $findings.Add("alternate data stream: $relative") }
    if ($relative -match '\.(xex|rpf|bik|iso|sav|pdb|toml|log|dmp|bmp|jsonl)$') { $findings.Add("forbidden file type: $relative") }
}

# Owner-identifying strings. The Windows user name and profile path come from
# the machine running the audit; callers add more with -Sentinel.
$patterns = [Collections.Generic.List[string]]::new()
# A bare user name can be an ordinary word (colour names, controller names), so
# it is matched only in path form.
foreach ($value in @('C:\BDU', 'C:/BDU', 'MCLA-Saves', 'MCLA-Recomp\private', 'MCLA-Recomp/private',
                     "\Users\$env:USERNAME\", "/Users/$env:USERNAME/", $env:USERPROFILE,
                     $env:COMPUTERNAME) + $Sentinel) {
    if ($value -and $value.Length -ge 4 -and -not $patterns.Contains($value)) { $patterns.Add($value) }
}
foreach ($item in $items | Where-Object { -not $_.PSIsContainer }) {
    $relative = $item.FullName.Substring($root.Length).TrimStart('\')
    $bytes = [IO.File]::ReadAllBytes($item.FullName)
    $ascii = [Text.Encoding]::GetEncoding(28591).GetString($bytes)
    $wide = [Text.Encoding]::Unicode.GetString($bytes)
    $wideShifted = if ($bytes.Length -gt 1) { [Text.Encoding]::Unicode.GetString($bytes, 1, $bytes.Length - 1) } else { '' }
    foreach ($pattern in $patterns) {
        $checks++
        foreach ($text in @($ascii, $wide, $wideShifted)) {
            if ($text.IndexOf($pattern, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
                $findings.Add("owner string '$pattern' in $relative"); break
            }
        }
    }
}

if ($RequireVolunteer) {
    $checks++
    $ticket = Join-Path $root 'build\mcla-test-build.ticket'
    if (-not (Test-Path -LiteralPath $ticket) -or
        -not ([IO.File]::ReadAllText($ticket)).StartsWith("MCLA-TEST-BUILD-V2`n")) {
        $findings.Add('ticket is not a V2 (kill-switch) ticket')
    }
    $checks++
    $hostBytes = [IO.File]::ReadAllBytes((Join-Path $root 'build\mcla.exe'))
    $hostText = [Text.Encoding]::GetEncoding(28591).GetString($hostBytes)
    if ($hostText.IndexOf('MCLA test-build admission', [StringComparison]::Ordinal) -lt 0) {
        $findings.Add('mcla.exe was not built with MCLA_TEST_BUILD_EXPIRY=ON')
    }
}

foreach ($finding in $findings) { "FINDING $finding" }
[pscustomobject]@{ Checks = $checks; Findings = $findings.Count; Directory = $root }
if ($findings.Count) { exit 1 }
