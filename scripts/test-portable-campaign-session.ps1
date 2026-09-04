[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-portable-campaign-session.ps1'
$testRoot = Join-Path $repo ('private\evidence\M7-016\session-test-' + [guid]::NewGuid().ToString('N').Substring(0,8))
$fixture = Join-Path $testRoot 'fixture'
$sessionName = 'session-20260904T120000Z-1234'
$bundleId = 'mcla-0.9.0.0-win64-AAAAAAAAAAAAAAAA-BBBBBBBBBBBBBBBB-CCCCCCCCCCCCCCCC'
$utf8 = [Text.UTF8Encoding]::new($false)
$negative = 0

function Hash([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') }
        finally { $sha.Dispose() }
    } finally { $stream.Dispose() }
}

function Json([string]$Path, $Value) {
    [IO.Directory]::CreateDirectory((Split-Path -Parent $Path)) | Out-Null
    [IO.File]::WriteAllText($Path, (($Value | ConvertTo-Json -Depth 8 -Compress) + "`n"), $utf8)
}

function Expect-Failure([scriptblock]$Mutation) {
    $script:index++
    $copy = Join-Path $testRoot ('bad-{0:D2}' -f $script:index)
    Copy-Item -LiteralPath $fixture -Destination $copy -Recurse
    & $Mutation $copy
    $failed = $false
    try { & $verify -BundleRoot $copy -SessionPath (Join-Path $copy ('results\' + $sessionName)) -RequireDiagnosticsProbe | Out-Null }
    catch { $failed = $true }
    if (-not $failed) { throw "Invalid portable session fixture $script:index was accepted." }
    $script:negative++
}

try {
    $saveRelative = 'B13EBABEBABEBABE\545407F8\00000001\mc4.sav\mc4.sav'
    $headerRelative = 'B13EBABEBABEBABE\545407F8\Headers\00000001\mc4.sav.header'
    $session = Join-Path $fixture ('results\' + $sessionName)
    $snapshot = Join-Path $session 'saves\AAAAAAAAAAAAAAAA'
    foreach ($directory in @('user','logs','diagnostics\live\live-20260904T120000Z-4321-1')) {
        [IO.Directory]::CreateDirectory((Join-Path $fixture $directory)) | Out-Null
    }
    [IO.Directory]::CreateDirectory($snapshot) | Out-Null
    foreach ($root in @((Join-Path $fixture 'user'),$snapshot)) {
        $save = Join-Path $root $saveRelative
        $header = Join-Path $root $headerRelative
        [IO.Directory]::CreateDirectory((Split-Path -Parent $save)) | Out-Null
        [IO.Directory]::CreateDirectory((Split-Path -Parent $header)) | Out-Null
        [IO.File]::WriteAllText($save, "fixture-save`n", $utf8)
        [IO.File]::WriteAllText($header, "fixture-header`n", $utf8)
    }
    $saveHash = Hash (Join-Path $fixture ('user\' + $saveRelative))
    $headerHash = Hash (Join-Path $fixture ('user\' + $headerRelative))
    [IO.File]::WriteAllText((Join-Path $fixture 'bundle-id.txt'), $bundleId + "`n", $utf8)
    [IO.File]::WriteAllText((Join-Path $fixture 'bundle-files.sha256'), (('A' * 64) + " *bin/mcla.exe`n"), $utf8)
    Json (Join-Path $session 'session.json') ([ordered]@{schema='mcla-portable-session-v1';bundle_id=$bundleId;session_id=$sessionName;started_utc='20260904T120000Z';mode='diagnostics-probe';state='running'})
    Json (Join-Path $session 'result.json') ([ordered]@{schema='mcla-portable-session-result-v1';bundle_id=$bundleId;session_id=$sessionName;started_utc='20260904T120000Z';completed_utc='20260904T120005Z';mode='diagnostics-probe';exit_code=0;verified_file_count=1;save_snapshot_count=1;save_watcher_error_count=0;final_save_sha256=$saveHash;final_header_sha256=$headerHash;state='complete'})
    Json (Join-Path $snapshot 'snapshot.json') ([ordered]@{schema='mcla-portable-save-snapshot-v1';captured_utc='20260904T120000Z';reason='launcher-start';save_sha256=$saveHash;header_sha256=$headerHash;complete_profile_tree=$true})
    $diagnostic = Join-Path $fixture 'diagnostics\live\live-20260904T120000Z-4321-1'
    [IO.File]::WriteAllText((Join-Path $diagnostic 'state.json'), "fixture-state`n", $utf8)
    Json (Join-Path $diagnostic 'manifest.json') ([ordered]@{schema='mcla-diagnostic-package-v1';kind='live';reason='probe';created_utc='20260904T120000Z';mcla_version='0.9.0.0';platform='windows';privacy=[ordered]@{automatic_upload=$false;package_safe_to_share=$false;private_artifacts=@()};capture=[ordered]@{};artifacts=@([ordered]@{name='state.json';bytes=[long](Get-Item (Join-Path $diagnostic 'state.json')).Length;sha256=Hash (Join-Path $diagnostic 'state.json');safe_to_share=$true})})
    [IO.File]::WriteAllText((Join-Path $fixture 'diagnostics\latest-live.txt'), "live-20260904T120000Z-4321-1`n", $utf8)
    [IO.File]::WriteAllText((Join-Path $fixture 'logs\mcla.log'), "MCLA_DIAGNOSTIC_SNAPSHOT_PROBE v=1 queued=1 completed=1 status=PASS`n", $utf8)

    $positive = & $verify -BundleRoot $fixture -SessionPath $session -RequireDiagnosticsProbe

    $script:index = 0
    Expect-Failure { param($r) $p=Join-Path $r ('results\'+$sessionName+'\result.json');$j=Get-Content $p -Raw|ConvertFrom-Json;$j.save_watcher_error_count=1;Json $p $j }
    Expect-Failure { param($r) $p=Join-Path $r ('results\'+$sessionName+'\saves');Remove-Item -LiteralPath $p -Recurse;$q=Join-Path $r ('results\'+$sessionName+'\result.json');$j=Get-Content $q -Raw|ConvertFrom-Json;$j.save_snapshot_count=0;Json $q $j }
    Expect-Failure { param($r) Add-Content -LiteralPath (Join-Path $r ('results\'+$sessionName+'\saves\AAAAAAAAAAAAAAAA\'+$saveRelative)) -Value 'drift' }
    Expect-Failure { param($r) [IO.File]::WriteAllText((Join-Path $r ('results\'+$sessionName+'\stale.partial')),'x') }
    Expect-Failure { param($r) [IO.File]::WriteAllText((Join-Path $r 'bundle.lock'),'x') }
    Expect-Failure { param($r) $p=Join-Path $r 'diagnostics\live\live-20260904T120000Z-4321-1\manifest.json';$j=Get-Content $p -Raw|ConvertFrom-Json;$j.privacy.automatic_upload=$true;Json $p $j }
    Expect-Failure { param($r) Add-Content -LiteralPath (Join-Path $r 'logs\mcla.log') -Value '[FATAL] fixture' }
    Expect-Failure { param($r) $p=Join-Path $r ('results\'+$sessionName+'\result.json');$j=Get-Content $p -Raw|ConvertFrom-Json;$j.bundle_id='wrong';Json $p $j }
    Expect-Failure { param($r) $p=Join-Path $r ('results\'+$sessionName+'\result.json');$j=Get-Content $p -Raw|ConvertFrom-Json;$j.completed_utc='20260904T115959Z';Json $p $j }
    Expect-Failure { param($r) $p=Join-Path $r ('results\'+$sessionName);$t=Join-Path $r 'session-target';Move-Item -LiteralPath $p -Destination $t;New-Item -ItemType Junction -Path $p -Target $t | Out-Null }

    [pscustomobject]@{Passed=[bool]$positive.Passed;Positive=1;FailClosedNegatives=$negative;SaveSnapshots=$positive.SaveSnapshots;DiagnosticsVerified=$positive.DiagnosticsVerified}
} finally {
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}
