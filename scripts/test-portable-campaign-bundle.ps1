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
. (Join-Path $PSScriptRoot 'Portable-CppRuntime.ps1')
$runtimePolicy = Get-PortableCppRuntimePolicy
$runtimeFiles = @(Get-PortableCppRuntimeFiles $runtimePolicy)

function Get-Sha256([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $sha = [Security.Cryptography.SHA256]::Create()
        try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-', '') }
        finally { $sha.Dispose() }
    } finally { $stream.Dispose() }
}

function Write-Lock([string]$Root, [switch]$Legacy) {
    $immutable = @('Launch-MCLA.exe','bundle-id.txt','bundle-manifest.json','game-manifest.json','bin/mcla.exe','bin/mcla_crash_handler.exe','bin/mcla.toml','bin/rexgpu-xenos.dll','bin/rexruntime.dll','bin/TracyClient.dll','game/default.xex')
    if (-not $Legacy) { $immutable += @($runtimeFiles | ForEach-Object { $_.RelativePath }) }
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
    foreach ($entry in $runtimeFiles) {
        [IO.File]::WriteAllText((Join-Path $fixture $entry.RelativePath.Replace('/','\')), "fixture:$($entry.RelativePath)`n", $utf8)
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
        schema='mcla-portable-campaign-bundle-v2';task='M7-016';bundle_id=$id;project_version='0.9.0.0';sdk_version='0.19.0.0';sdk_commit='c2e67161f1207ddcee609507c399efe7a1981b72';created_utc=[DateTime]::UtcNow.ToString('O');platform='windows-proton';architecture='x86_64';launcher='Launch-MCLA.exe';window_mode=[ordered]@{startup_fullscreen=$true;alt_enter_toggle=$true;left_double_click_toggle=$true};immutable_file_count=18
        cpp_runtime=[ordered]@{policy=$runtimePolicy.policy;version=$runtimePolicy.version}
        game=[ordered]@{manifest_sha256=Get-Sha256 (Join-Path $fixture 'game-manifest.json');file_count=1;payload_bytes=[long](Get-Item $gameFile).Length}
        selected_save=[ordered]@{source_snapshot='private/save-archive/M6-014/fixture/20260904-000000Z-AAAAAAAAAAAAAAAA-BBBBBBBBBBBBBBBB';save_relative='B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav';header_relative='B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header';save_sha256=Get-Sha256 $save;header_sha256=Get-Sha256 $header}
        writable_roots=@('user','cache','logs','diagnostics','results','update','metadata');retention=[ordered]@{completed_sessions=32;save_snapshots_per_session=32};private_content=[ordered]@{game_and_save_private=$true;source_iso_included=$false}
    }
    [IO.File]::WriteAllText((Join-Path $fixture 'bundle-manifest.json'), (($manifest|ConvertTo-Json -Depth 8)+"`n"), $utf8)
    Write-Lock $fixture
    $positive = & $verify -BundleRoot $fixture -Fixture
    $returnedV2 = & $verify -BundleRoot $fixture -Fixture -ReturnedProfile

    # Historical children remain readable for save-lineage preservation, never
    # acceptable as a newly published toolchain-free runtime.
    $legacy = Join-Path $testRoot 'legacy-return'
    Copy-Item -LiteralPath $fixture -Destination $legacy -Recurse
    foreach ($entry in $runtimeFiles) { Remove-Item -LiteralPath (Join-Path $legacy $entry.RelativePath.Replace('/','\')) }
    $legacyManifestPath = Join-Path $legacy 'bundle-manifest.json'
    $legacyManifest = Get-Content -LiteralPath $legacyManifestPath -Raw | ConvertFrom-Json
    $legacyManifest.schema = 'mcla-portable-campaign-bundle-v1'
    $legacyManifest.sdk_version = '0.18.0.1'
    $legacyManifest.sdk_commit = '9098e122afa116f4316d6cec47e561ebc5186974'
    $legacyManifest.immutable_file_count = 11
    $legacyManifest.PSObject.Properties.Remove('cpp_runtime')
    [IO.File]::WriteAllText($legacyManifestPath, (($legacyManifest | ConvertTo-Json -Depth 8) + "`n"), $utf8)
    Write-Lock $legacy -Legacy
    $legacyReturn = & $verify -BundleRoot $legacy -Fixture -ReturnedProfile
    $rejectedLegacyPublication = $false
    try { & $verify -BundleRoot $legacy -Fixture | Out-Null } catch { $rejectedLegacyPublication = $true }
    if (-not $rejectedLegacyPublication) { throw 'Legacy v1 bundle was accepted as a new publication.' }
    $negative++

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
    foreach ($entry in $runtimeFiles) {
        $missingRelative = $entry.RelativePath
        Expect-Failure { param($r) Remove-Item -LiteralPath (Join-Path $r $missingRelative.Replace('/','\')) }
    }
    Expect-Failure { param($r) [IO.File]::AppendAllText((Join-Path $r 'bin\msvcp140.dll'),'drift') }
    Expect-Failure { param($r) [IO.File]::WriteAllText((Join-Path $r 'bin\msvcp140d.dll'),'debug-runtime') }
    Expect-Failure { param($r) [IO.File]::WriteAllText((Join-Path $r 'msvcp140_atomic_wait.dll'),'wrong-placement') }
    Expect-Failure { param($r) [IO.File]::WriteAllText((Join-Path $r 'bin\concrt140.dll'),'unrequired-runtime') }
    Expect-Failure { param($r) [IO.Directory]::CreateDirectory((Join-Path $r 'bin\unexpected-runtime')) | Out-Null }
    Expect-Failure { param($r) $p=Join-Path $r 'bundle-manifest.json';$j=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json;$j.cpp_runtime.policy='msvc-any';[IO.File]::WriteAllText($p,(($j|ConvertTo-Json -Depth 8)+"`n"),$utf8) } -RewriteLock
    Expect-Failure { param($r) $p=Join-Path $r 'bundle-manifest.json';$j=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json;$j.cpp_runtime.version='14.44.1.0';[IO.File]::WriteAllText($p,(($j|ConvertTo-Json -Depth 8)+"`n"),$utf8) } -RewriteLock
    Expect-Failure { param($r) $p=Join-Path $r 'bundle-manifest.json';$j=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json;$j.cpp_runtime.policy=@('msvc-14.44-app-local-v1');[IO.File]::WriteAllText($p,(($j|ConvertTo-Json -Depth 8)+"`n"),$utf8) } -RewriteLock
    Expect-Failure { param($r) $p=Join-Path $r 'bundle-manifest.json';$j=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json;$j.cpp_runtime.version=@('14.44.35211.0');[IO.File]::WriteAllText($p,(($j|ConvertTo-Json -Depth 8)+"`n"),$utf8) } -RewriteLock
    Expect-Failure { param($r) $p=Join-Path $r 'bundle-manifest.json';$j=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json;$j.cpp_runtime|Add-Member NoteProperty fallback $true;[IO.File]::WriteAllText($p,(($j|ConvertTo-Json -Depth 8)+"`n"),$utf8) } -RewriteLock
    Expect-Failure { param($r) $p=Join-Path $r 'bundle-manifest.json';$j=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json;$j.PSObject.Properties.Remove('cpp_runtime');[IO.File]::WriteAllText($p,(($j|ConvertTo-Json -Depth 8)+"`n"),$utf8) } -RewriteLock
    Expect-Failure { param($r) $p=Join-Path $r 'bundle-manifest.json';$j=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json;$j.schema='mcla-portable-campaign-bundle-v3';[IO.File]::WriteAllText($p,(($j|ConvertTo-Json -Depth 8)+"`n"),$utf8) } -RewriteLock
    # A structurally self-consistent v2 missing its whole runtime is rejected,
    # even if the attacker rewrites both the manifest count and hash lock.
    Expect-Failure {
        param($r)
        foreach ($entry in $runtimeFiles) { Remove-Item -LiteralPath (Join-Path $r $entry.RelativePath.Replace('/','\')) }
        $p=Join-Path $r 'bundle-manifest.json';$j=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json;$j.immutable_file_count=11
        [IO.File]::WriteAllText($p,(($j|ConvertTo-Json -Depth 8)+"`n"),$utf8)
        Write-Lock $r -Legacy
    }

    $source = [IO.File]::ReadAllText((Join-Path $repo 'src\mcla_portable_launcher_win.cpp')) + [IO.File]::ReadAllText((Join-Path $repo 'scripts\build-portable-campaign-bundle.ps1')) + [IO.File]::ReadAllText((Join-Path $repo 'CMakeLists.txt'))
    $sourceChecks = 0
    foreach ($needle in @('bundle-files.sha256','bundle.lock','.sync-conflict-','.partial','JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE','--game_data_root','--user_data_root','--cache_root','--metadata_root','--update_data_root','--diagnostics-probe','--gpu-backend=','--gpu_backend=','mcla-portable-session-result-v1','mcla-portable-interrupted-session-result-v1','RecoverInterruptedSessions','Recovered stale bundle.lock','save_watcher_error_count','session_root / L"saves"','kMaximumCompletedSessions = 32','kMaximumSnapshotsPerSession = 32','WriteTextAtomic','MoveFileExW','CreateFileW(lock.path.c_str()','HasReparseAttribute(root)','add_executable(mcla_portable_launcher','OUTPUT_NAME "Launch-MCLA"','private\bundles\M7-016','verify-game-manifest.ps1','Resolve-LatestSave','Clone-Relocated','Remove-ObsoleteBundles','$PruneOnly','fullscreen = true','left_double_click_toggle','c2e67161f1207ddcee609507c399efe7a1981b72')) {
        if (-not $source.Contains($needle)) { throw "Portable bundle source contract is missing '$needle'." }
        $sourceChecks++
    }
    $publisherPath = Join-Path $repo 'scripts/build-portable-campaign-bundle.ps1'
    $publisher = [IO.File]::ReadAllText($publisherPath)
    foreach ($copyCall in @(
        "`$build = Join-Path `$repo 'out\build\win-amd64-portable-release'",
        "cmake --preset win-amd64-release -B `$build",
        "cmake --build `$build --target mcla --clean-first",
        "Copy-OrLink `$launcherSource (Join-Path `$partial 'Launch-MCLA.exe') -Materialize",
        "Copy-OrLink (Join-Path `$build `$name) (Join-Path `$partial ('bin\' + `$name)) -Materialize",
        "Copy-OrLink (Join-Path `$cppRuntime.SourceDirectory `$runtimeFile.Name) (Join-Path `$partial `$runtimeFile.RelativePath.Replace('/','\')) -Materialize",
        'Resolve-PortableCppRuntimeSource -VisualStudioRoot $toolchain.VisualStudioRoot',
        "schema = 'mcla-portable-campaign-bundle-v2'",
        'Assert-PortableCppRuntimeBundle -Root $partial -Policy $cppRuntime.Policy'
    )) {
        if (-not $publisher.Contains($copyCall)) { throw 'Published runtime is not independent of mutable build output.' }
        $sourceChecks++
    }
    # Evaluate only the actual pure copy helper, never the publishing script.
    $tokens=$null; $errors=$null
    $ast = [Management.Automation.Language.Parser]::ParseFile($publisherPath, [ref]$tokens, [ref]$errors)
    if ($errors.Count) { throw 'Publisher parse failed.' }
    $helper = @($ast.FindAll({param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Copy-OrLink'
    }, $false))
    if ($helper.Count -ne 1) { throw 'Copy helper was not uniquely resolved.' }
    . ([scriptblock]::Create($helper[0].Extent.Text))
    $mutable = Join-Path $testRoot 'mutable-build.exe'
    $published = Join-Path $testRoot 'published-build.exe'
    [IO.File]::WriteAllText($mutable, 'before', $utf8)
    if ((Copy-OrLink $mutable $published -Materialize) -cne 'materialized-copy') { throw 'Runtime was not copied.' }
    [IO.File]::WriteAllText($mutable, 'after', $utf8)
    if ([IO.File]::ReadAllText($published) -cne 'before') { throw 'Rebuilding mutated a published binary.' }
    & git -C $repo check-ignore -q -- 'private/bundles/M7-016/probe/game/default.xex'
    if ($LASTEXITCODE -ne 0) { throw 'Private portable bundle output is not ignored by Git.' }
    [pscustomobject]@{Passed=$true;Positive=3;FailClosedNegatives=$negative;SourceContractChecks=$sourceChecks;ImmutableFiles=$positive.ImmutableFiles;RuntimeFiles=$runtimeFiles.Count;V2Return=$returnedV2.Passed;LegacyReturn=$legacyReturn.Passed;GitIgnored=$true}
} finally {
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}
