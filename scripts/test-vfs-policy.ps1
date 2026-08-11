[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$app = Get-Content -LiteralPath (Join-Path $repoRoot 'src/mcla_app.cpp') -Raw
$vfs = Get-Content -LiteralPath (
    Join-Path $repoRoot 'third_party/rexglue-sdk/src/filesystem/virtual_file_system.cpp'
) -Raw
$entry = Get-Content -LiteralPath (
    Join-Path $repoRoot 'third_party/rexglue-sdk/src/filesystem/entry.cpp'
) -Raw
$hostEntry = Get-Content -LiteralPath (
    Join-Path $repoRoot 'third_party/rexglue-sdk/src/filesystem/devices/host_path_entry.cpp'
) -Raw
$sdkTests = Get-Content -LiteralPath (
    Join-Path $repoRoot 'third_party/rexglue-sdk/tests/unit/core/filesystem_test.cpp'
) -Raw

$requiredAppPatterns = @(
    'REXCVAR_DEFINE_BOOL\s*\(\s*mcla_vfs_probe',
    'FindSymbolicLink\("game:"',
    'FindSymbolicLink\("d:"',
    'FileDisposition::kCreate',
    'MappedMemory::Mode::kReadWrite',
    'MCLA VFS: disc-root contract rejected; guest launch blocked'
)
foreach ($pattern in $requiredAppPatterns) {
    if ($app -notmatch $pattern) {
        throw "Project VFS contract is missing required pattern '$pattern'."
    }
}
if (-not $app.Contains('vfs->ResolvePath("game:\\..\\default.xex")')) {
    throw 'Project VFS contract does not probe a guest root-escape path.'
}
if ($vfs -notmatch 'PathEscapesDeviceRoot' -or
    $vfs -notmatch 'return X_STATUS_ACCESS_DENIED;' -or
    $vfs -match 'downgrade to read access') {
    throw 'SDK VFS does not fail closed for traversal and write requests.'
}
if ($entry -notmatch 'X_STATUS Entry::Rename[\s\S]*?if \(is_read_only\(\)\)[\s\S]*?X_STATUS_ACCESS_DENIED' -or
    $hostEntry -notmatch 'is_read_only\(\).*Mode::kReadWrite' -or
    $sdkTests -notmatch 'read-only host VFS rejects traversal and every mutation path') {
    throw 'SDK rename/mapping guards or their regression test are missing.'
}

[pscustomobject]@{
    Passed = $true
    ProjectContractPatterns = $requiredAppPatterns.Count
    SdkWritePolicy = 'fail-closed'
    SdkRegression = 'present'
}
