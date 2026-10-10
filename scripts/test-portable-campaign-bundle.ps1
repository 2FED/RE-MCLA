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
. (Join-Path $PSScriptRoot 'Portable-SaveLineage.ps1')
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
    $captureManifest = Get-Content -LiteralPath (Join-Path $Root 'bundle-manifest.json') -Raw | ConvertFrom-Json
    if ($captureManifest.PSObject.Properties['image_capture']) { $immutable += 'bin/mcla_diagnostic_image_helper.exe' }
    $rows = foreach ($relative in $immutable | Sort-Object) { '{0} *{1}' -f (Get-Sha256 (Join-Path $Root $relative.Replace('/','\'))),$relative }
    [IO.File]::WriteAllText((Join-Path $Root 'bundle-files.sha256'), (($rows -join "`n") + "`n"), $utf8)
}

function Expect-Failure([scriptblock]$Mutation, [switch]$RewriteLock, [string]$SourceRoot = $fixture) {
    $script:index++
    $copy = Join-Path $testRoot ('bad-{0:D2}' -f $script:index)
    Copy-Item -LiteralPath $SourceRoot -Destination $copy -Recurse
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
        schema='mcla-portable-campaign-bundle-v2';task='M7-016';bundle_id=$id;project_version='0.9.0.0';sdk_version='0.20.0.0';sdk_commit='5dd58f894bf34b9ea85e59d3903c7f0e8044d853';created_utc=[DateTime]::UtcNow.ToString('O');platform='windows-proton';architecture='x86_64';launcher='Launch-MCLA.exe';window_mode=[ordered]@{startup_fullscreen=$true;alt_enter_toggle=$true;left_double_click_toggle=$true};immutable_file_count=18
        cpp_runtime=[ordered]@{policy=$runtimePolicy.policy;version=$runtimePolicy.version}
        game=[ordered]@{manifest_sha256=Get-Sha256 (Join-Path $fixture 'game-manifest.json');file_count=1;payload_bytes=[long](Get-Item $gameFile).Length}
        selected_save=[ordered]@{source_snapshot='private/save-archive/M6-014/fixture/20260904-000000Z-AAAAAAAAAAAAAAAA-BBBBBBBBBBBBBBBB';save_relative='B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav';header_relative='B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header';save_sha256=Get-Sha256 $save;header_sha256=Get-Sha256 $header}
        writable_roots=@('user','cache','logs','diagnostics','results','update','metadata');retention=[ordered]@{completed_sessions=32;save_snapshots_per_session=32};private_content=[ordered]@{game_and_save_private=$true;source_iso_included=$false}
    }
    [IO.File]::WriteAllText((Join-Path $fixture 'bundle-manifest.json'), (($manifest|ConvertTo-Json -Depth 8)+"`n"), $utf8)
    Write-Lock $fixture
    $positive = & $verify -BundleRoot $fixture -Fixture
    $returnedV2 = & $verify -BundleRoot $fixture -Fixture -ReturnedProfile

    # Previous v2 runtime remains admissible for private return preservation,
    # but cannot masquerade as the current SDK publication.
    $oldV2 = Join-Path $testRoot 'old-sdk-v2-return'
    Copy-Item -LiteralPath $fixture -Destination $oldV2 -Recurse
    $oldV2ManifestPath = Join-Path $oldV2 'bundle-manifest.json'
    $oldV2Manifest = Get-Content -LiteralPath $oldV2ManifestPath -Raw | ConvertFrom-Json
    $oldV2Manifest.sdk_version = '0.19.0.0'
    $oldV2Manifest.sdk_commit = 'c2e67161f1207ddcee609507c399efe7a1981b72'
    [IO.File]::WriteAllText($oldV2ManifestPath, (($oldV2Manifest | ConvertTo-Json -Depth 8) + "`n"), $utf8)
    Write-Lock $oldV2
    $oldV2Return = & $verify -BundleRoot $oldV2 -Fixture -ReturnedProfile
    $oldV2Rejected = $false
    try { & $verify -BundleRoot $oldV2 -Fixture | Out-Null } catch { $oldV2Rejected = $true }
    if (-not $oldV2Rejected) { throw 'Old SDK v2 return accepted as current publication.' }
    $negative++

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

    # The capability is opt-in: no unadvertised helper and no fixture exemption
    # from host/helper pin checking. Synthetic host bytes contain the actual pin.
    Expect-Failure { param($r) [IO.File]::WriteAllText((Join-Path $r 'bin/mcla_diagnostic_image_helper.exe'),'unadvertised',$utf8) }
    $wgc = Join-Path $testRoot 'wgc'
    Copy-Item -LiteralPath $fixture -Destination $wgc -Recurse
    $helperPath = Join-Path $wgc 'bin/mcla_diagnostic_image_helper.exe'
    [IO.File]::WriteAllText($helperPath,'synthetic-window-only-helper',$utf8)
    $helperHash = Get-Sha256 $helperPath
    [IO.File]::AppendAllText((Join-Path $wgc 'bin/mcla.exe'),$helperHash,$utf8)
    $wgcManifestPath = Join-Path $wgc 'bundle-manifest.json'
    $wgcManifest = Get-Content -LiteralPath $wgcManifestPath -Raw | ConvertFrom-Json
    $wgcManifest.immutable_file_count++
    $wgcManifest | Add-Member NoteProperty image_capture ([pscustomobject]@{mode='window-wgc-v1';helper_path='bin/mcla_diagnostic_image_helper.exe';helper_sha256=$helperHash;helper_bytes=[long](Get-Item $helperPath).Length;host_sha256=(Get-Sha256 (Join-Path $wgc 'bin/mcla.exe'))})
    [IO.File]::WriteAllText($wgcManifestPath,(($wgcManifest|ConvertTo-Json -Depth 8)+"`n"),$utf8)
    Write-Lock $wgc
    $wgcPositive = & $verify -BundleRoot $wgc -Fixture
    $wgcReturn = & $verify -BundleRoot $wgc -Fixture -ReturnedProfile
    if (-not $wgcPositive.F10Wgc -or -not $wgcReturn.F10Wgc -or $positive.F10Wgc) { throw 'Image capture capability reporting drifted.' }
    Expect-Failure -SourceRoot $wgc { param($r) Remove-Item -LiteralPath (Join-Path $r 'bin/mcla_diagnostic_image_helper.exe') }
    Expect-Failure -SourceRoot $wgc { param($r) [IO.File]::AppendAllText((Join-Path $r 'bin/mcla_diagnostic_image_helper.exe'),'drift') }
    Expect-Failure -SourceRoot $wgc -RewriteLock { param($r) [IO.File]::AppendAllText((Join-Path $r 'bin/mcla_diagnostic_image_helper.exe'),'drift') }
    Expect-Failure -SourceRoot $wgc { param($r) [IO.File]::WriteAllText((Join-Path $r 'bin/extra-helper.exe'),'extra',$utf8) }
    foreach ($captureMutation in @(
        { param($j) $j.image_capture.mode='desktop-wgc-v1' },
        { param($j) $j.image_capture.helper_path='../helper.exe' },
        { param($j) $j.image_capture.helper_sha256='A'*64 },
        { param($j) $j.image_capture.host_sha256='B'*64 },
        { param($j) $j.image_capture.helper_bytes++ },
        { param($j) $j.image_capture.helper_bytes='29' },
        { param($j) $j.image_capture.helper_bytes=0 },
        { param($j) $j.image_capture.helper_bytes=16777217 },
        { param($j) $j.image_capture.mode=@('window-wgc-v1') },
        { param($j) $j.image_capture|Add-Member NoteProperty fallback $true },
        { param($j) $j.image_capture.PSObject.Properties.Remove('host_sha256') },
        { param($j) $j.PSObject.Properties.Remove('image_capture'); $j.immutable_file_count-- }
    )) {
        Expect-Failure -SourceRoot $wgc -RewriteLock {
            param($r)
            $p=Join-Path $r 'bundle-manifest.json';$j=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json
            & $captureMutation $j
            [IO.File]::WriteAllText($p,(($j|ConvertTo-Json -Depth 8)+"`n"),$utf8)
        }
    }
    foreach ($pinCount in @(0,2)) {
        Expect-Failure -SourceRoot $wgc -RewriteLock {
            param($r)
            $hostPath=Join-Path $r 'bin/mcla.exe'
            [IO.File]::WriteAllText($hostPath,('synthetic-host'+($helperHash*$pinCount)),$utf8)
            $p=Join-Path $r 'bundle-manifest.json';$j=Get-Content -LiteralPath $p -Raw|ConvertFrom-Json
            $j.image_capture.host_sha256=Get-Sha256 $hostPath
            [IO.File]::WriteAllText($p,(($j|ConvertTo-Json -Depth 8)+"`n"),$utf8)
        }
    }

    $source = [IO.File]::ReadAllText((Join-Path $repo 'src\mcla_portable_launcher_win.cpp')) + [IO.File]::ReadAllText((Join-Path $repo 'scripts\build-portable-campaign-bundle.ps1')) + [IO.File]::ReadAllText((Join-Path $repo 'CMakeLists.txt'))
    $sourceChecks = 0
    foreach ($needle in @('bundle-files.sha256','bundle.lock','.sync-conflict-','.partial','JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE','--game_data_root','--user_data_root','--cache_root','--metadata_root','--update_data_root','--diagnostics-probe','--gpu-backend=','--gpu_backend=','mcla-portable-session-result-v1','mcla-portable-interrupted-session-result-v1','RecoverInterruptedSessions','Recovered stale bundle.lock','save_watcher_error_count','session_root / L"saves"','kMaximumCompletedSessions = 32','kMaximumSnapshotsPerSession = 32','WriteTextAtomic','MoveFileExW','CreateFileW(lock.path.c_str()','HasReparseAttribute(root)','add_executable(mcla_portable_launcher','OUTPUT_NAME "Launch-MCLA"','private\bundles\M7-016','verify-game-manifest.ps1','Resolve-LatestSave','Clone-Relocated','Remove-ObsoleteBundles','$PruneOnly','fullscreen = true','left_double_click_toggle','5dd58f894bf34b9ea85e59d3903c7f0e8044d853')) {
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
    foreach ($functionName in @('Assert-PortableBuildOptions','Resolve-PortableSdkInstall','Assert-PortableSdkCache','New-PortableConfig','Assert-PortableProfilePathBudget')) {
        $definition = @($ast.FindAll({param($node)
            $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq $functionName
        }, $false))
        if ($definition.Count -ne 1) { throw "Publisher helper was not uniquely resolved: $functionName" }
        . ([scriptblock]::Create($definition[0].Extent.Text))
    }
    # Pure publication-path admission, including the actual-root failure seam.
    # No long directory or launcher process is created by these checks.
    $pathEntries=@(
        [pscustomobject]@{path='B13EBABEBABEBABE';kind='directory'},
        [pscustomobject]@{path='B13EBABEBABEBABE/545407F8/00000001/mc4.sav/mc4.sav';kind='file'},
        [pscustomobject]@{path='B13EBABEBABEBABE/545407F8/Headers/00000001/mc4.sav.header';kind='file'}
    )
    $shortFinal='C:\'+('s'*97)
    $shortStaged=$shortFinal+'.12345678.partial'
    $pathBudget=Assert-PortableProfilePathBudget $shortStaged $shortFinal $pathEntries
    if($shortFinal.Length-ne100-or$pathBudget.MaximumFileCharacters-gt259-or$pathBudget.MaximumDirectoryCharacters-gt247){throw 'Short actual root path budget failed.'}
    $pathBudgetPositives=1
    $originalDeep='C:\'+('d'*133)
    foreach($badBudget in @(
        @{Staged=$originalDeep+'.12345678.partial';Final=$originalDeep;Entries=$pathEntries},
        @{Staged=('C:\'+('s'*220));Final=$shortFinal;Entries=$pathEntries},
        @{Staged=$shortStaged;Final=$shortFinal;Entries=@()},
        @{Staged=$shortStaged;Final=$shortFinal;Entries=@([pscustomobject]@{path='B13EBABEBABEBABE/../escape';kind='file'})},
        @{Staged=$shortStaged;Final=$shortFinal;Entries=@([pscustomobject]@{path='B13EBABEBABEBABE/file';kind='link'})},
        @{Staged=$shortStaged;Final='relative';Entries=$pathEntries}
    )) {
        $rejected=$false;try{$null=Assert-PortableProfilePathBudget $badBudget.Staged $badBudget.Final $badBudget.Entries}catch{$rejected=$true}
        if(-not$rejected){throw 'Unsafe or too-deep portable path budget accepted.'};$negative++
    }
    # Exact boundary for an ordinary file and its parent, using the longest
    # modeled diagnostic nesting rather than an arbitrary root-only threshold.
    $longestCopyPrefix='diagnostics/live/live-20000101T000000Z-4294967295-18446744073709551615.partial/save-private/'
    $boundaryRelative='B13EBABEBABEBABE/545407F8/'+('x'*45)
    $boundaryRootLength=259-1-$longestCopyPrefix.Length-$boundaryRelative.Length
    $boundaryRoot='C:\'+('b'*($boundaryRootLength-3))
    $boundaryEntries=@([pscustomobject]@{path=$boundaryRelative;kind='file'})
    $boundaryBudget=Assert-PortableProfilePathBudget ($boundaryRoot+'.12345678.partial') $boundaryRoot $boundaryEntries
    if($boundaryBudget.MaximumFileCharacters-ne259){throw 'File path boundary model drifted.'};$pathBudgetPositives++
    $rejected=$false;try{$null=Assert-PortableProfilePathBudget ($boundaryRoot+'.12345678.partial') ($boundaryRoot+'b') $boundaryEntries}catch{$rejected=$true}
    if(-not$rejected){throw '260-character file path was accepted.'};$negative++
    $directoryRelative='B13EBABEBABEBABE/545407F8/'+('x'*45)
    $directoryRootLength=247-1-$longestCopyPrefix.Length-$directoryRelative.Length
    $directoryRoot='C:\'+('b'*($directoryRootLength-3))
    $directoryEntries=@([pscustomobject]@{path=$directoryRelative;kind='directory'})
    $directoryBudget=Assert-PortableProfilePathBudget ($directoryRoot+'.12345678.partial') $directoryRoot $directoryEntries
    if($directoryBudget.MaximumDirectoryCharacters-ne247){throw 'Directory path boundary model drifted.'};$pathBudgetPositives++
    $rejected=$false;try{$null=Assert-PortableProfilePathBudget ($directoryRoot+'.12345678.partial') ($directoryRoot+'b') $directoryEntries}catch{$rejected=$true}
    if(-not$rejected){throw '248-character directory path was accepted.'};$negative++
    # Deep auxiliary files cannot disappear from the budget just because the
    # main save/header are short. Unicode counts UTF-16 units, as Windows does.
    foreach($extraEntry in @(
        [pscustomobject]@{path=('B13EBABEBABEBABE/545407F8/photo/'+('x'*90));kind='file'},
        [pscustomobject]@{path=('B13EBABEBABEBABE/545407F8/'+([string][char]0x0416*90));kind='directory'}
    )) {
        $rejected=$false;try{$null=Assert-PortableProfilePathBudget $shortStaged $shortFinal ($pathEntries+@($extraEntry))}catch{$rejected=$true}
        if(-not$rejected){throw 'A deep full-profile entry escaped the path budget.'};$negative++
    }
    $preflightCall=$publisher.IndexOf('$plannedPathBudget = Assert-PortableProfilePathBudget',[StringComparison]::Ordinal)
    $buildCall=$publisher.IndexOf('& cmake --preset',[StringComparison]::Ordinal)
    $actualCall=$publisher.IndexOf('$actualPathBudget = Assert-PortableProfilePathBudget',[StringComparison]::Ordinal)
    $stageCall=$publisher.IndexOf('[IO.Directory]::CreateDirectory((Join-Path $partial $directory))',[StringComparison]::Ordinal)
    if($preflightCall-lt0-or$buildCall-lt$preflightCall-or$actualCall-lt$buildCall-or$stageCall-lt$actualCall){throw 'Path guard is not before build and actual staging.'}
    $settingsPositives = 0
    $template = [IO.File]::ReadAllText((Join-Path $repo 'config/mcla.toml.example'))
    foreach ($scale in @(1,2)) {
        $specialized = New-PortableConfig $template $scale
        foreach ($expectedLine in @('mcla_diagnostics_root = "diagnostics"','log_file = "logs/mcla.log"','fullscreen = true',"resolution_scale = $scale",'vsync = true')) {
            if ([regex]::Matches($specialized, ('(?m)^'+[regex]::Escape($expectedLine)+'\r?$')).Count -ne 1) { throw 'Portable config specialization drifted.' }
        }
        if ($specialized -match '(?m)^\s*(gpu_backend|d3d12_adapter)\s*=') { throw 'Specialization overrides CLI GPU selection.' }
        $settingsPositives++
    }
    foreach ($badSettings in @(
        @{Text=$template;Scale=3},
        @{Text=$template+"`nresolution_scale = 1";Scale=2},
        @{Text=$template+"`nvsync = false";Scale=2},
        @{Text=$template+"`ngpu_backend = 'any'";Scale=2},
        @{Text=$template+"`nd3d12_adapter = 0";Scale=2},
        @{Text=$template+"`nlocal_user_display_name = 'Other'";Scale=2},
        @{Text=$template+"`n'gpu_backend' = 'any'";Scale=2},
        @{Text=$template+"`n[GPU]";Scale=2},
        @{Text=$template+"`nvalue = '''multiline'''";Scale=2},
        @{Text=$template+"`nvalue = [1,2]";Scale=2},
        @{Text=$template+"`nfullscreen = true";Scale=2},
        @{Text=$template.Replace('fullscreen = true','fullscreen = false');Scale=2},
        @{Text=$template.Replace('fullscreen = true','FULLSCREEN = true');Scale=2},
        @{Text=$template.Replace('mcla_diagnostics_root = ""','mcla_diagnostics_root = "elsewhere"');Scale=2},
        @{Text=$template.Replace('log_file = ""','# absent');Scale=2}
    )) {
        $rejected=$false;try { $null=New-PortableConfig $badSettings.Text $badSettings.Scale } catch { $rejected=$true }
        if (-not $rejected) { throw 'Invalid portable config specialization accepted.' };$negative++
    }
    Assert-PortableBuildOptions @{}
    function Test-BoundPublisherOptions {
        param([switch]$PruneOnly,[string]$RexglueInstallPrefix,[switch]$SkipCleanBuild,[int]$ResolutionScale)
        Assert-PortableBuildOptions $PSBoundParameters
    }
    Test-BoundPublisherOptions -ResolutionScale 2 -RexglueInstallPrefix 'C:\fixture'
    $rejected=$false;try { Test-BoundPublisherOptions -RexglueInstallPrefix 'C:\fixture' -SkipCleanBuild } catch { $rejected=$true }
    if (-not $rejected) { throw 'Bound publisher options allowed a skipped explicit SDK build.' };$negative++
    Assert-PortableBuildOptions @{PruneOnly=$true;OutputRoot='fixture';SaveSource='legacy'}
    Assert-PortableBuildOptions @{ResolutionScale=2;RexglueInstallPrefix='C:\fixture';SkipCleanBuild=$false}
    foreach ($optionName in @('EnableF10Wgc','CppWinRtIncludeDir','RexglueInstallPrefix','ResolutionScale','SkipCleanBuild','SkipRelocationProof','MaterializeGameFiles')) {
        $options=@{PruneOnly=$true};$options[$optionName]=$false
        $rejected=$false;try { Assert-PortableBuildOptions $options } catch { $rejected=$true }
        if (-not $rejected) { throw 'PruneOnly accepted explicit build/settings options.' };$negative++
    }
    $rejected=$false;try { Assert-PortableBuildOptions @{RexglueInstallPrefix='C:\fixture';SkipCleanBuild=$true} } catch { $rejected=$true }
    if (-not $rejected) { throw 'Explicit SDK selection accepted SkipCleanBuild.' };$negative++
    $sdkFixture=Join-Path $testRoot 'explicit-sdk'
    foreach ($relative in @('lib/cmake/rexglue/rexglueConfig.cmake','lib/cmake/rexglue/rexglueConfigVersion.cmake','lib/cmake/rexglue/rexglueTargets.cmake','lib/cmake/rexglue/rexglueTargets-release.cmake','bin/rexruntime.dll','bin/rexgpu-xenos.dll','lib/rexruntime.lib','lib/rexgpu-xenos.lib')) {
        $path=Join-Path $sdkFixture $relative
        [IO.Directory]::CreateDirectory((Split-Path -Parent $path))|Out-Null
        [IO.File]::WriteAllText($path,'fixture-sdk',$utf8)
    }
    $sdkVersion=Join-Path $sdkFixture 'lib/cmake/rexglue/rexglueConfigVersion.cmake'
    [IO.File]::WriteAllText($sdkVersion,'set(PACKAGE_VERSION "0.20.0.0")',$utf8)
    $sdkSelection=Resolve-PortableSdkInstall $sdkFixture
    if ($sdkSelection.Pins.Count -ne 8) { throw 'Explicit SDK pin inventory drifted.' }
    $sdkPrefix=$sdkFixture.Replace('\','/')
    $goodSdkCache="CMAKE_PREFIX_PATH:PATH=$sdkPrefix`nrexglue_DIR:PATH=$sdkPrefix/lib/cmake/rexglue`nREXSDK_DIR:PATH=`n"
    Assert-PortableSdkCache $goodSdkCache $sdkFixture
    foreach ($badSdkCache in @(
        $goodSdkCache.Replace($sdkPrefix,'C:/legacy-sdk'),
        $goodSdkCache.Replace("CMAKE_PREFIX_PATH:PATH=$sdkPrefix", "CMAKE_PREFIX_PATH:PATH=$sdkPrefix;C:/other"),
        $goodSdkCache.Replace("rexglue_DIR:PATH=$sdkPrefix/lib/cmake/rexglue",'rexglue_DIR:PATH=C:/legacy-sdk/lib/cmake/rexglue'),
        $goodSdkCache.Replace('REXSDK_DIR:PATH=','REXSDK_DIR:PATH=C:/source-sdk'),
        $goodSdkCache.Replace('REXSDK_DIR:PATH=','# absent'),
        ($goodSdkCache+"rexglue_DIR:PATH=$sdkPrefix/lib/cmake/rexglue`n")
    )) {
        $rejected=$false;try { Assert-PortableSdkCache $badSdkCache $sdkFixture } catch { $rejected=$true }
        if (-not $rejected) { throw 'Mismatched SDK cache accepted.' };$negative++
    }
    foreach ($badPrefix in @('relative-sdk','C:relative-sdk','\root-relative-sdk',($sdkFixture+';C:\other'),($sdkFixture+"`nother"),(Join-Path $testRoot 'absent-sdk'))) {
        $rejected=$false;try { $null=Resolve-PortableSdkInstall $badPrefix } catch { $rejected=$true }
        if (-not $rejected) { throw 'Invalid explicit SDK prefix accepted.' };$negative++
    }
    [IO.File]::WriteAllText($sdkVersion,'set(PACKAGE_VERSION "0.18.0.0")',$utf8)
    $rejected=$false;try { $null=Resolve-PortableSdkInstall $sdkFixture } catch { $rejected=$true }
    if (-not $rejected) { throw 'Wrong explicit SDK version accepted.' };$negative++
    $cacheHelper = @($ast.FindAll({param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -ceq 'Assert-PortableCaptureBuildCache'
    }, $false))
    if ($cacheHelper.Count -ne 1) { throw 'Capture cache helper was not uniquely resolved.' }
    . ([scriptblock]::Create($cacheHelper[0].Extent.Text))
    $cacheRoot=Join-Path $testRoot 'cache-contract';[IO.Directory]::CreateDirectory($cacheRoot)|Out-Null
    foreach ($mode in @('OFF','ON')) {
        [IO.File]::WriteAllText((Join-Path $cacheRoot 'CMakeCache.txt'),("CMAKE_BUILD_TYPE:STRING=Release`nMCLA_EXPERIMENTAL_F10_WGC:BOOL=$mode`n"),$utf8)
        Assert-PortableCaptureBuildCache $cacheRoot ($mode -ceq 'ON')
        $rejected=$false;try { Assert-PortableCaptureBuildCache $cacheRoot ($mode -cne 'ON') } catch { $rejected=$true }
        if (-not $rejected) { throw 'Mismatched capture build cache accepted.' };$negative++
    }
    foreach ($cacheText in @('CMAKE_BUILD_TYPE:STRING=Release',"CMAKE_BUILD_TYPE:STRING=Debug`nMCLA_EXPERIMENTAL_F10_WGC:BOOL=OFF", "CMAKE_BUILD_TYPE:STRING=Release`nMCLA_EXPERIMENTAL_F10_WGC:BOOL=OFF`nMCLA_EXPERIMENTAL_F10_WGC:BOOL=ON")) {
        [IO.File]::WriteAllText((Join-Path $cacheRoot 'CMakeCache.txt'),$cacheText,$utf8)
        $rejected=$false;try { Assert-PortableCaptureBuildCache $cacheRoot $false } catch { $rejected=$true }
        if (-not $rejected) { throw 'Malformed capture build cache accepted.' };$negative++
    }
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
    [pscustomobject]@{Passed=$true;Positive=5;FailClosedNegatives=$negative;SourceContractChecks=$sourceChecks;ImmutableFiles=$positive.ImmutableFiles;RuntimeFiles=$runtimeFiles.Count;V2Return=$returnedV2.Passed;LegacyReturn=$legacyReturn.Passed;WgcPublication=$wgcPositive.Passed;WgcReturn=$wgcReturn.Passed;CaptureCachePositives=2;SettingsPositives=$settingsPositives;PathBudgetPositives=$pathBudgetPositives;SdkSelectionVerified=$true;GitIgnored=$true}
} finally {
    if (Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}
