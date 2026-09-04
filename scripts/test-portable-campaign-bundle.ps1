[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-portable-campaign-bundle.ps1'
$testRoot = Join-Path $repo ('private\evidence\M7-016\test-' + [guid]::NewGuid().ToString('N').Substring(0,8))
$fixture = Join-Path $testRoot 'fixture'
$utf8 = [Text.UTF8Encoding]::new($false)
$negative = 0

function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') }
        finally { $sha.Dispose() }
    } finally { $stream.Dispose() }
}

function Write-Lock([string]$Root) {
    $immutable = @('Launch-MCLA.exe','bundle-id.txt','bundle-manifest.json','game-manifest.json','bin/mcla.exe','bin/mcla_crash_handler.exe','bin/mcla.toml','bin/rexgpu-xenos.dll','bin/rexruntime.dll','bin/TracyClient.dll','game/default.xex')
    $rows = foreach ($relative in $immutable | Sort-Object) { '{0} *{1}' -f (Get-Sha256 (Join-Path $Root $relative.Replace('/','\'))),$relative }
    [IO.File]::WriteAllText((Join-Path $Root 'bundle-files.sha256'), (($rows -join "`n") + "`n"), $utf8)
}

function Expect-Failure([scriptblock]$Mutation, [switch]$RewriteLock) {
    $script:index++
    $copy = Join-Path $testRoot ('bad-{0:D2}' -f $script:index)
    Copy-Item -LiteralPath $fixture -Destination $copy -Recurse
    & $Mutation $copy
    if ($RewriteLock) { Write-Lock $copy }
    $failed = $false
    try { & $verify -BundleRoot $copy -Fixture | Out-Null } catch { $failed = $true }
    if (-not $failed) { throw "Invalid portable bundle fixture $script:index was accepted." }
    $script:negative++
}

try {
    foreach ($directory in @('bin','game','user\B13EBABEBABEBABE\545407F8\00000001\mc4.sav','user\B13EBABEBABEBABE\545407F8\Headers\00000001','cache','logs','diagnostics','results','update','metadata')) {
        [IO.Directory]::CreateDirectory((Join-Path $fixture $directory)) | Out-Null
    }
    foreach ($relative in @('Launch-MCLA.exe','bin/mcla.exe','bin/mcla_crash_handler.exe','bin/rexgpu-xenos.dll','bin/rexruntime.dll','bin/TracyClient.dll','game/default.xex')) {
        $path = Join-Path $fixture $relative.Replace('/','\')
        [IO.File]::WriteAllText($path, "fixture:$relative`n", $utf8)
    }
$config = @'
mcla_diagnostics_root = "diagnostics"
log_file = "logs/mcla.log"
fullscreen = true
game_data_root = ""
user_data_root = ""
update_data_root = ""
cache_root = ""
metadata_root = ""
'@
    [IO.File]::WriteAllText((Join-Path $fixture 'bin\mcla.toml'), $config, $utf8)
    $save = Join-Path $fixture 'user\B13EBABEBABEBABE\545407F8\00000001\mc4.sav\mc4.sav'
    $header = Join-Path $fixture 'user\B13EBABEBABEBABE\545407F8\Headers\00000001\mc4.sav.header'
    [IO.File]::WriteAllText($save, "fixture-save`n", $utf8)
    [IO.File]::WriteAllText($header, "fixture-header`n", $utf8)
    $gameFile = Join-Path $fixture 'game\default.xex'
    $gameManifest = [ordered]@{ SchemaVersion=1; SourceIsoSha256=('0' * 64); FileCount=1; PayloadBytes=[long](Get-Item $gameFile).Length; Files=@([ordered]@{Path='default.xex';Size=[long](Get-Item $gameFile).Length;Sha256=Get-Sha256 $gameFile}) }
    [IO.File]::WriteAllText((Join-Path $fixture 'game-manifest.json'), (($gameManifest|ConvertTo-Json -Depth 5)+"`n"), $utf8)
    $id = 'mcla-0.9.0.0-win64-AAAAAAAAAAAAAAAA-BBBBBBBBBBBBBBBB-CCCCCCCCCCCCCCCC'
    [IO.File]::WriteAllText((Join-Path $fixture 'bundle-id.txt'), $id + "`n", $utf8)
    $manifest = [ordered]@{
        schema='mcla-portable-campaign-bundle-v1';task='M7-016';bundle_id=$id;project_version='0.9.0.0';sdk_version='0.10.0.2';sdk_commit='492614eec92c31f11d75dd8fa0f09785cbae4a66';created_utc=[DateTime]::UtcNow.ToString('O');platform='windows-proton';architecture='x86_64';launcher='Launch-MCLA.exe';window_mode=[ordered]@{startup_fullscreen=$true;alt_enter_toggle=$true;left_double_click_toggle=$true};immutable_file_count=11
        game=[ordered]@{manifest_sha256=Get-Sha256 (Join-Path $fixture 'game-manifest.json');file_count=1;payload_bytes=[long](Get-Item $gameFile).Length}
        selected_save=[ordered]@{source_snapshot='private/save-archive/M6-014/fixture/20260904-000000Z-AAAAAAAAAAAAAAAA-BBBBBBBBBBBBBBBB';save_relative='B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav';header_relative='B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header';save_sha256=Get-Sha256 $save;header_sha256=Get-Sha256 $header}
        writable_roots=@('user','cache','logs','diagnostics','results','update','metadata');retention=[ordered]@{completed_sessions=32;save_snapshots_per_session=32};private_content=[ordered]@{game_and_save_private=$true;source_iso_included=$false}
    }
    [IO.File]::WriteAllText((Join-Path $fixture 'bundle-manifest.json'), (($manifest|ConvertTo-Json -Depth 8)+"`n"), $utf8)
    Write-Lock $fixture
    $positive = & $verify -BundleRoot $fixture -Fixture

    $script:index = 0
    Expect-Failure { param($r) [IO.File]::AppendAllText((Join-Path $r 'bin\mcla.exe'),'drift') }
    Expect-Failure { param($r) Remove-Item -LiteralPath (Join-Path $r 'bin\rexruntime.dll') }
    Expect-Failure { param($r) [IO.File]::WriteAllText((Join-Path $r 'thing.sync-conflict-copy'),'x') }
    Expect-Failure { param($r) [IO.File]::WriteAllText((Join-Path $r 'stale.partial'),'x') }
    Expect-Failure { param($r) [IO.File]::WriteAllText((Join-Path $r 'source.iso'),'x') }
    Expect-Failure { param($r) [IO.File]::WriteAllText((Join-Path $r 'unexpected.txt'),'x') }
    Expect-Failure { param($r) $p=Join-Path $r 'bin\mcla.toml';$s=[IO.File]::ReadAllText($p).Replace('log_file = "logs/mcla.log"','log_file = "C:\\private\\mcla.log"');[IO.File]::WriteAllText($p,$s,$utf8) } -RewriteLock
    Expect-Failure { param($r) $p=Join-Path $r 'bin\mcla.toml';$s=[IO.File]::ReadAllText($p).Replace('fullscreen = true','fullscreen = false');[IO.File]::WriteAllText($p,$s,$utf8) } -RewriteLock
    Expect-Failure { param($r) $p=Join-Path $r 'bundle-manifest.json';$j=Get-Content $p -Raw|ConvertFrom-Json;$j.selected_save.save_sha256='C' * 64;[IO.File]::WriteAllText($p,(($j|ConvertTo-Json -Depth 8)+"`n"),$utf8) } -RewriteLock
    Expect-Failure { param($r) Add-Content -LiteralPath (Join-Path $r 'bundle-files.sha256') -Value ((Get-Content (Join-Path $r 'bundle-files.sha256')|Select-Object -First 1)) }
    Expect-Failure { param($r) Add-Content -LiteralPath (Join-Path $r 'bundle-files.sha256') -Value (('D' * 64) + ' *../escape') }
    Expect-Failure { param($r) Remove-Item -LiteralPath (Join-Path $r 'user') -Recurse }
    Expect-Failure { param($r) [IO.File]::WriteAllText((Join-Path $r 'game\extra.rpf'),'x') }

    $source = [IO.File]::ReadAllText((Join-Path $repo 'src\mcla_portable_launcher_win.cpp')) + [IO.File]::ReadAllText((Join-Path $repo 'scripts\build-portable-campaign-bundle.ps1')) + [IO.File]::ReadAllText((Join-Path $repo 'CMakeLists.txt'))
    $sourceChecks = 0
    foreach ($needle in @('bundle-files.sha256','bundle.lock','.sync-conflict-','.partial','JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE','--game_data_root','--user_data_root','--cache_root','--metadata_root','--update_data_root','--diagnostics-probe','mcla-portable-session-result-v1','save_watcher_error_count','session_root / L"saves"','kMaximumCompletedSessions = 32','kMaximumSnapshotsPerSession = 32','WriteTextAtomic','MoveFileExW','CreateFileW(lock.path.c_str()','HasReparseAttribute(root)','add_executable(mcla_portable_launcher','OUTPUT_NAME "Launch-MCLA"','private\bundles\M7-016','verify-game-manifest.ps1','Resolve-LatestSave','Clone-Relocated','Remove-ObsoleteBundles','$PruneOnly','fullscreen = true','left_double_click_toggle','492614eec92c31f11d75dd8fa0f09785cbae4a66')) {
        if (-not $source.Contains($needle)) { throw "Portable bundle source contract is missing '$needle'." }
        $sourceChecks++
    }
    & git -C $repo check-ignore -q -- 'private/bundles/M7-016/probe/game/default.xex'
    if ($LASTEXITCODE -ne 0) { throw 'Private portable bundle output is not ignored by Git.' }
    [pscustomobject]@{Passed=$true;Positive=1;FailClosedNegatives=$negative;SourceContractChecks=$sourceChecks;ImmutableFiles=$positive.ImmutableFiles;GitIgnored=$true}
} finally {
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}
