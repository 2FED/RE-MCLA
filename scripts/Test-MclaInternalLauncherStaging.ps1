[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [Parameter(Mandatory=$true)][string]$CrtDirectory
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'Portable-CppRuntime.ps1')
$stageScript = Join-Path $PSScriptRoot 'Stage-MclaInternalLauncher.ps1'
$script:checks = 0
$script:failures = 0
$script:skips = 0
function Check([bool]$Value, [string]$Label) {
    $script:checks++
    if (-not $Value) { $script:failures++; Write-Warning ('FAIL ' + $Label) }
}
function Expect-Failure([scriptblock]$Action, [string]$Label, [string]$Pattern = '.') {
    $failed = $false
    try { & $Action | Out-Null }
    catch { $failed = $_.Exception.Message -match $Pattern }
    Check $failed $Label
}
function Copy-FixtureFile([string]$From, [string]$To) {
    $inputStream = [IO.File]::Open($From, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    try {
        $outputStream = [IO.File]::Open($To, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        try { $inputStream.CopyTo($outputStream); $outputStream.Flush($true) }
        finally { $outputStream.Dispose() }
    } finally { $inputStream.Dispose() }
}
function New-FixtureText([string]$Path, [string]$Text) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        $bytes = [Text.UTF8Encoding]::new($false).GetBytes($Text)
        $stream.Write($bytes, 0, $bytes.Length)
    } finally { $stream.Dispose() }
}

# Only compiled allowlisted artifacts are read from supplied directories. No
# game data, real profile, environment secret, or diagnostic package is read.
# All writes are inside a NEW uniquely named temporary fixture, preserved on
# success/failure. No executable in the fixture is ever launched.
$source = (Resolve-Path -LiteralPath $BuildDirectory).Path
$crtSource = (Resolve-Path -LiteralPath $CrtDirectory).Path
$policy = Get-PortableCppRuntimePolicy
foreach ($entry in $policy.files) {
    Assert-PortableCppRuntimeFile -Path (Join-Path $crtSource $entry.name) -Sha256 $entry.sha256 -Version $policy.version -CheckSignature
}
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('MclaStagingTest-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture | Out-Null
Write-Output ('Preserved synthetic fixture: ' + $fixture)
$build = Join-Path $fixture 'source'
$crt = Join-Path $fixture 'crt'
New-Item -ItemType Directory -Path $build | Out-Null
New-Item -ItemType Directory -Path $crt | Out-Null
$runtime = @('mcla.exe','mcla_crash_handler.exe','mcla_diagnostic_image_helper.exe','rexruntime.dll','rexgpu-xenos.dll','TracyClient.dll','mcla-test-build.ticket')
foreach ($name in @($runtime) + @('MCLA-Launcher.exe')) {
    Copy-FixtureFile (Join-Path $source $name) (Join-Path $build $name)
}
foreach ($entry in $policy.files) { Copy-FixtureFile (Join-Path $crtSource $entry.name) (Join-Path $crt $entry.name) }
$actualSourceHashes = @{}
foreach ($name in @($runtime) + @('MCLA-Launcher.exe')) {
    $actualSourceHashes[$name] = (Get-FileHash -LiteralPath (Join-Path $source $name) -Algorithm SHA256).Hash
}
foreach ($name in @('mcla.toml','preferences.dat','private.env','default.xex','source.pdb','extra.dll','owner-save.bin','anything.iso')) {
    New-FixtureText (Join-Path $build $name) 'SYNTHETIC-FORBIDDEN-NOT-OWNER-DATA'
}
foreach ($name in @('user','cache','logs','diagnostics','private','game')) {
    $directory = Join-Path $build $name
    New-Item -ItemType Directory -Path $directory | Out-Null
    New-FixtureText (Join-Path $directory 'nested-fixture.txt') 'SYNTHETIC-NESTED-FORBIDDEN'
}

$candidate = Join-Path $fixture 'candidate'
$result = & $stageScript -BuildDirectory $build -Destination $candidate -CrtDirectory $crt
Check ($result -ceq $candidate) 'stager returns only exact completed candidate path'
$receiptPath = Join-Path $candidate 'candidate-receipt.json'
$receipt = [IO.File]::ReadAllText($receiptPath) | ConvertFrom-Json
Check ($receipt.schema -ceq 'mcla-internal-launcher-staging-v1') 'receipt schema'
Check ($receipt.external_delivery_approved -eq $false -and $receipt.game_assets_included -eq $false -and
       $receipt.personal_data_included -eq $false) 'private-only receipt does not approve redistribution'
Check ($receipt.crt_policy -ceq $policy.policy -and $receipt.crt_version -ceq $policy.version) 'receipt identifies pinned CRT policy'
Check (@($receipt.files).Count -eq 17) 'receipt covers all 17 nonreceipt artifacts including signed ticket'
$expected = [Collections.Generic.List[string]]::new()
$expected.Add('MCLA-Launcher.exe')
$expected.Add('candidate-receipt.json')
$expected.Add('build/mcla-build.manifest')
foreach ($name in $runtime) { $expected.Add('build/' + $name) }
foreach ($entry in $policy.files) { $expected.Add($entry.name); $expected.Add('build/' + $entry.name) }
$observed = @(
    Get-ChildItem -LiteralPath $candidate -File -Force | ForEach-Object { $_.Name }
    Get-ChildItem -LiteralPath (Join-Path $candidate 'build') -File -Force | ForEach-Object { 'build/' + $_.Name }
)
[string[]]$sortedExpected = $expected.ToArray()
[string[]]$sortedObserved = $observed
[Array]::Sort($sortedExpected, [StringComparer]::Ordinal)
[Array]::Sort($sortedObserved, [StringComparer]::Ordinal)
Check (($sortedExpected -join '|') -ceq ($sortedObserved -join '|')) 'exact 18-file output excludes planted private/game/extra files'
Check (@(Get-ChildItem -LiteralPath $candidate -Directory -Force).Count -eq 1 -and
       @(Get-ChildItem -LiteralPath (Join-Path $candidate 'build') -Directory -Force).Count -eq 0) 'only one build directory, no nested data'
foreach ($file in $receipt.files) {
    $path = Join-Path $candidate $file.path.Replace('/', '\')
    Check ((Get-Item -LiteralPath $path).Length -eq $file.bytes -and
        (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ceq $file.sha256) 'receipted artifact size/hash exact'
    $streams = @(Get-Item -LiteralPath $path -Stream '*')
    Check ($streams.Count -eq 1 -and $streams[0].Stream -ceq ':$DATA') 'candidate has no named stream'
}
$manifest = [IO.File]::ReadAllText((Join-Path $candidate 'build/mcla-build.manifest'))
Check ($manifest.StartsWith("MCLA-DEV-BUILD-V1`n") -and -not $manifest.Contains("`r") -and
       $manifest.EndsWith("`n") -and $manifest.TrimEnd("`n").Split("`n").Count -eq 12) 'manifest exact LF header and eleven entries'
Check (-not (Test-Path -LiteralPath (Join-Path $candidate 'candidate-receipt.pending.json'))) 'completion published without remaining pending receipt'
$receiptHash = (Get-FileHash -LiteralPath $receiptPath -Algorithm SHA256).Hash
Expect-Failure { & $stageScript -BuildDirectory $build -Destination $candidate -CrtDirectory $crt } 'existing destination never overwritten' 'must be NEW'
Check ((Get-FileHash -LiteralPath $receiptPath -Algorithm SHA256).Hash -ceq $receiptHash) 'rejected restage preserves completed receipt'
$inside = Join-Path $build 'nested-candidate'
Expect-Failure { & $stageScript -BuildDirectory $build -Destination $inside -CrtDirectory $crt } 'build-nested destination rejected' 'must not be inside'
Check (-not (Test-Path -LiteralPath $inside)) 'nested rejection occurs before writes'
$lockedTarget = Join-Path $fixture 'locked-input-rejected'
$locked = [IO.File]::Open((Join-Path $build 'MCLA-Launcher.exe'), [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::None)
try {
    Expect-Failure { & $stageScript -BuildDirectory $build -Destination $lockedTarget -CrtDirectory $crt } 'unreadable locked source fails copy admission'
} finally { $locked.Dispose() }
Check (-not (Test-Path -LiteralPath $lockedTarget)) 'all-source pinning completes before candidate creation'
$badCrt = Join-Path $fixture 'bad-crt'
New-Item -ItemType Directory -Path $badCrt | Out-Null
foreach ($entry in $policy.files) {
    if ($entry.name -ceq 'msvcp140.dll') { New-FixtureText (Join-Path $badCrt $entry.name) 'NOT-A-PINNED-CRT' }
    else { Copy-FixtureFile (Join-Path $crt $entry.name) (Join-Path $badCrt $entry.name) }
}
$badTarget = Join-Path $fixture 'bad-crt-rejected'
Expect-Failure { & $stageScript -BuildDirectory $build -Destination $badTarget -CrtDirectory $badCrt } 'wrong CRT content rejected' 'runtime'
Check (-not (Test-Path -LiteralPath $badTarget)) 'CRT validation fails before candidate writes'
$junction = Join-Path $fixture 'source-junction'
try {
    New-Item -ItemType Junction -Path $junction -Target $build -ErrorAction Stop | Out-Null
} catch {
    $script:skips++
    Write-Output 'SKIP junction fixture unavailable; no reparse PASS claimed.'
}
if (Test-Path -LiteralPath $junction) {
    $linkTarget = Join-Path $fixture 'junction-rejected'
    Expect-Failure { & $stageScript -BuildDirectory $junction -Destination $linkTarget -CrtDirectory $crt } 'reparse input root rejected' 'Redirected'
    Check (-not (Test-Path -LiteralPath $linkTarget)) 'reparse rejection before candidate writes'
}
# The named stream is planted only on the fixture copy, after all other tests.
# It is retained with the fixture; actual compiler outputs are never modified.
Set-Content -LiteralPath (Join-Path $build 'MCLA-Launcher.exe') -Stream 'mcla-private-fixture' -Value 'SYNTHETIC-STREAM-NOT-A-SECRET'
$adsTarget = Join-Path $fixture 'ads-rejected'
Expect-Failure { & $stageScript -BuildDirectory $build -Destination $adsTarget -CrtDirectory $crt } 'alternate stream source rejected' 'alternate streams'
Check (-not (Test-Path -LiteralPath $adsTarget)) 'ADS admission fails before output publication'
foreach ($name in $actualSourceHashes.Keys) {
    Check ((Get-FileHash -LiteralPath (Join-Path $source $name) -Algorithm SHA256).Hash -ceq $actualSourceHashes[$name]) 'actual compiled input remains unchanged'
}
[pscustomobject]@{checks=$script:checks; failures=$script:failures; skips=$script:skips; fixture=$fixture}
if ($script:failures) { throw 'Internal launcher staging fixtures failed; preserve evidence.' }
