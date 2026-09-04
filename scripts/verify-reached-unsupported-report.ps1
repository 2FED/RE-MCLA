[CmdletBinding()]
param(
    [string]$LogPath,
    [string]$ResultPath,
    [switch]$SourceOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sdkRoot = Join-Path $repoRoot 'third_party/rexglue-sdk'

function Resolve-Contained([string]$Path, [string]$Root, [string]$Label) {
    $full = [IO.Path]::GetFullPath($Path)
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    if (-not $full.StartsWith($rootFull, [StringComparison]::OrdinalIgnoreCase)) { throw "$Label escapes its root." }
    $item = Get-Item -LiteralPath $full -Force
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "$Label is a reparse point." }
    return $full
}

function Read-OrderedLogs([string]$CurrentLog) {
    $current = Get-Item -LiteralPath $CurrentLog
    $escaped = [regex]::Escape($current.BaseName)
    $rotated = @(Get-ChildItem -LiteralPath $current.DirectoryName -File | Where-Object {
        $_.Name -match "^$escaped\.(?<n>\d+)$([regex]::Escape($current.Extension))$"
    } | ForEach-Object { [pscustomobject]@{ Item = $_; N = [int]$Matches.n } } | Sort-Object N -Descending)
    if ($rotated.Count) {
        $numbers = @($rotated.N | Sort-Object)
        for ($i = 0; $i -lt $numbers.Count; $i++) { if ($numbers[$i] -ne $i + 1) { throw 'Rotated log set is not contiguous.' } }
    }
    $files = @($rotated | ForEach-Object { $_.Item }) + @($current)
    $text = [Text.StringBuilder]::new(); $manifest = @()
    foreach ($file in $files) {
        $stream = [IO.File]::Open($file.FullName, [IO.FileMode]::Open, [IO.FileAccess]::Read,
            [IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
        try { $reader = [IO.StreamReader]::new($stream); try { $content = $reader.ReadToEnd() } finally { $reader.Dispose() } } finally { $stream.Dispose() }
        [void]$text.Append($content).Append("`n")
        $manifest += [ordered]@{ name = $file.Name; bytes = $file.Length; sha256 = (Get-FileHash $file.FullName -Algorithm SHA256).Hash }
    }
    [pscustomobject]@{ Text = $text.ToString(); Manifest = $manifest }
}

function Assert-SourceContract {
    $io = Get-Content (Join-Path $sdkRoot 'src/kernel/xboxkrnl/xboxkrnl_io.cpp') -Raw
    $crypto = Get-Content (Join-Path $sdkRoot 'src/kernel/xboxkrnl/xboxkrnl_crypt.cpp') -Raw
    $ioHelper = Get-Content (Join-Path $sdkRoot 'src/kernel/xboxkrnl/io_compat.h') -Raw
    $cryptoHelper = Get-Content (Join-Path $sdkRoot 'src/kernel/xboxkrnl/crypto_compat.h') -Raw
    $tests = Get-Content (Join-Path $sdkRoot 'tests/unit/kernel/xboxkrnl_reached_compat_test.cpp') -Raw
    $testCmake = Get-Content (Join-Path $sdkRoot 'tests/unit/CMakeLists.txt') -Raw
    $contractText = [regex]::Replace(($io + $crypto + $ioHelper + $cryptoHelper + $tests + $testCmake), '\s+', ' ')
    foreach ($needle in @(
        'LookupObject<XFile>(handle)',
        '[COMPAT] IoDismountVolumeByFileHandle accepted;',
        'runtime-owned',
        'is_file_handle ? X_STATUS_SUCCESS : X_STATUS_INVALID_HANDLE',
        '[UNAVAILABLE] XeKeysConsolePrivateKeySign has no console private key;',
        'caller buffer preserved',
        'return X_STATUS_NOT_SUPPORTED',
        'return X_STATUS_INVALID_PARAMETER',
        '[kernel][xboxkrnl][reached-compat]',
        'xboxkrnl_reached_compat_test.cpp')) {
        if (-not $contractText.Contains($needle, [StringComparison]::Ordinal)) { throw "Missing source contract: $needle" }
    }
    if ($io -match 'IoDismountVolumeByFileHandle[^\r\n]*- stub' -or $crypto -match 'XeKeysConsolePrivateKeySign[^\r\n]*- stub') { throw 'Reached imports still contain legacy stub logging.' }

    $targets = [ordered]@{
        '8220B810'='8220B834'; '82262320'='8226233C'; '82264760'='82264770';
        '82264770'='82264780'; '822C9FE8'='822CA04C'; '82554080'='8255409C'
    }
    $config = Get-Content (Join-Path $repoRoot 'config/mcla_functions.toml') -Raw
    $init = Get-Content (Join-Path $repoRoot 'generated/default/mcla_init.cpp') -Raw
    $register = Get-Content (Join-Path $repoRoot 'generated/default/mcla_register.cpp') -Raw
    $generated = (Get-ChildItem (Join-Path $repoRoot 'generated/default') -Filter 'mcla_recomp.*.cpp' | ForEach-Object { Get-Content $_.FullName -Raw }) -join "`n"
    foreach ($pair in $targets.GetEnumerator()) {
        if ($config -notmatch ('"0x' + $pair.Key + '"\s*=\s*\{\s*end\s*=\s*0x' + $pair.Value + ',\s*name\s*=\s*"sub_' + $pair.Key + '"\s*\}')) { throw "Missing exact function boundary $($pair.Key)." }
        if ($init -notlike "*{ 0x$($pair.Key), sub_$($pair.Key) }*") { throw "Missing init mapping $($pair.Key)." }
        if ($register -notlike "*SetFunction(0x$($pair.Key), sub_$($pair.Key))*") { throw "Missing registration $($pair.Key)." }
        if ($generated -notmatch ('DEFINE_REX_FUNC\(sub_' + $pair.Key + '\)\s*\{\s*REX_FUNC_PROLOGUE\(0x' + $pair.Key + '\)')) { throw "Missing generated body $($pair.Key)." }
    }

    $hid = Get-Content (Join-Path $sdkRoot 'src/kernel/xboxkrnl/xboxkrnl_hid.cpp') -Raw
    if ([regex]::Matches($hid, 'REX_EXPORT_STUB\(__imp__XInputdFF').Count -ne 0) { throw 'An advanced FFB export regressed to a stub.' }
    if ([regex]::Matches($hid, 'REX_HOOK_RAW\(__imp__XInputdFF').Count -ne 8) { throw 'Implemented advanced FFB import inventory drifted.' }
    $sdl = Get-Content (Join-Path $sdkRoot 'src/input/sdl/sdl_input_driver.cpp') -Raw
    $sdlHeader = Get-Content (Join-Path $sdkRoot 'include/rex/input/sdl/sdl_input_driver.h') -Raw
    if ([regex]::Matches($sdl, 'cap_flags\s*\|=\s*X_INPUT_CAPS_FFB_SUPPORTED').Count -ne 1) { throw 'Advanced FFB capability advertisement drifted.' }
    if (-not $sdlHeader.Contains('kWheelForceFeedbackEffectSlots = 64')) { throw 'Advanced FFB effect-slot bound drifted.' }
    [pscustomobject]@{ Passed = $true; FixedTargetCount = $targets.Count; ImplementedFfbImportCount = 8; WithheldFfbImportCount = 0; SourceChecks = 20 }
}

function Parse-Log([string]$CurrentLog) {
    $ordered = Read-OrderedLogs $CurrentLog
    $text = $ordered.Text
    $launch = [regex]::Matches($text, 'KernelState: Preparing module launch').Count
    $privateKey = [regex]::Matches($text, '\[UNAVAILABLE\] XeKeysConsolePrivateKeySign has no console private key; caller buffer preserved').Count
    $dismount = [regex]::Matches($text, '\[COMPAT\] IoDismountVolumeByFileHandle accepted; host VFS mount is runtime-owned').Count
    $present = [regex]::Matches($text, 'D3D12 guest present: successful sequence count=3').Count
    if ($launch -ne 1 -or $privateKey -ne 1 -or $dismount -ne 1 -or $present -ne 1) { throw 'Expected exactly one launch, unavailable-signing, compatible-dismount, and present marker.' }
    $indices = @($text.IndexOf('KernelState: Preparing module launch'), $text.IndexOf('[UNAVAILABLE] XeKeysConsolePrivateKeySign'), $text.IndexOf('[COMPAT] IoDismountVolumeByFileHandle'), $text.IndexOf('D3D12 guest present: successful sequence count=3'))
    for ($i = 1; $i -lt $indices.Count; $i++) { if ($indices[$i] -le $indices[$i - 1]) { throw 'Runtime marker chronology is invalid.' } }
    foreach ($bad in @('XeKeysConsolePrivateKeySign - stub','IoDismountVolumeByFileHandle(HANDLE) - stub','PPC_UNIMPLEMENTED','invalid/unregistered guest','guest crash','[FATAL]','device removed','DRED')) { if ($text -match [regex]::Escape($bad)) { throw "Forbidden runtime marker: $bad" } }
    if ($text -match '(?i)\[stub\]| STUB - returning') { throw 'Generic reached stub marker found.' }
    foreach ($marker in @('Window closing, shutting down','Execution complete','Title terminated; hard-exiting process')) { if ([regex]::Matches($text, [regex]::Escape($marker)).Count -ne 1) { throw "Controlled lifecycle marker drifted: $marker" } }
    [pscustomobject]@{ Passed = $true; Manifest = $ordered.Manifest; ReachedImportCount = 2; PpcUnimplementedCount = 0; FixedTargetCount = 6 }
}

$source = Assert-SourceContract
if ($SourceOnly) { return $source }
if ($LogPath) { return Parse-Log (Resolve-Path -LiteralPath $LogPath).Path }
if (-not $ResultPath) { throw 'Specify -SourceOnly, -LogPath, or -ResultPath.' }

$privateRoot = Join-Path $repoRoot 'private/evidence/M6-013'
$resultFull = Resolve-Contained $ResultPath $privateRoot 'Result path'
$result = Get-Content $resultFull -Raw | ConvertFrom-Json
if ($result.schema -ne 1 -or $result.task -ne 'M6-013' -or $result.decision -ne 'reached-unsupported-surface-fixed-or-bounded-nonblocking' -or
    $result.sdk_version -ne '0.9.0.29' -or $result.sdk_commit -ne '5a7fc75713d1d43188b7574349f44a7e7923033d' -or $result.focused_test_cases -ne 2 -or $result.focused_test_assertions -ne 5 -or
    $result.reached_import_count -ne 2 -or $result.fixed_target_count -ne 6 -or $result.not_observed_import_count -ne 12 -or
    $result.withheld_ffb_import_count -ne 8 -or $result.all_sdk_stubs_claimed -ne $false -or $result.controlled_exit_verified -ne $true) { throw 'Result identity or scope is invalid.' }

$expectedReached = @('IoDismountVolumeByFileHandle','XeKeysConsolePrivateKeySign')
if (@($result.reached_imports).Count -ne 2) { throw 'Reached import inventory drifted.' }
for ($i = 0; $i -lt 2; $i++) { if ($result.reached_imports[$i].name -ne $expectedReached[$i]) { throw 'Reached import order or identity drifted.' } }
if ($result.reached_imports[0].disposition -ne 'validated-compat-success' -or $result.reached_imports[0].invalid_result -ne 'X_STATUS_INVALID_HANDLE' -or $result.reached_imports[0].host_vfs_mount -ne 'runtime-owned' -or
    $result.reached_imports[1].disposition -ne 'explicit-unavailable' -or $result.reached_imports[1].valid_result -ne 'X_STATUS_NOT_SUPPORTED' -or $result.reached_imports[1].caller_buffer_preserved -ne $true) { throw 'Reached import disposition drifted.' }
$expectedTargets = @('8220B810:8220B834','82262320:8226233C','82264760:82264770','82264770:82264780','822C9FE8:822CA04C','82554080:8255409C')
if (@($result.fixed_targets).Count -ne 6) { throw 'Fixed target inventory drifted.' }
for ($i = 0; $i -lt 6; $i++) { if (($result.fixed_targets[$i].start + ':' + $result.fixed_targets[$i].end) -ne $expectedTargets[$i]) { throw 'Fixed target identity drifted.' } }
$expectedNotObserved = @('__C_specific_handler','StfsControlDevice','StfsCreateDevice','XeKeysConsoleSignatureVerification','IoDismountVolume','IoInvalidDeviceRequest','IoCompleteRequest','ObIsTitleObject','IoCheckShareAccess','IoSetShareAccess','IoRemoveShareAccess','RtlUnwind')
if (@($result.not_observed_imports).Count -ne $expectedNotObserved.Count) { throw 'Not-observed inventory drifted.' }
for ($i = 0; $i -lt $expectedNotObserved.Count; $i++) { if ($result.not_observed_imports[$i] -ne $expectedNotObserved[$i]) { throw 'Not-observed import identity drifted.' } }

$expectedPrior = [ordered]@{
    'M5-007'='709609A904C3A49AD0C88E8CC88DFD794D849FB9A7B9B6B5F8AA0887BA9C1E18'
    'M5-013'='D890A903775A3B53262B9957E7B7DC1D4B76B49B8735360BECD8233842446298'
    'M6-001'='519B84FF456BDD3220BFC8BE3DD230CCB209A56CF2B203D51CFF5454729E178F'
    'M6-002'='21FCBE38695B45D22AE516E33E51AD0A292286985549673811E0FE3E33900644'
    'M6-003'='AD3573B56412D7DBC10650BE0D250AFD4AACA22EB8EEEB6F9816E94EC3007E3A'
    'M6-007'='CE4B4700CEA1108243989165211A8C70AC0C67F72130708A129C1BD834948B5B'
}
$prior = @($result.prior_evidence)
if ($prior.Count -ne $expectedPrior.Count) { throw 'Prior evidence cardinality drifted.' }
for ($i = 0; $i -lt $prior.Count; $i++) {
    $task = @($expectedPrior.Keys)[$i]
    if ($prior[$i].task -ne $task -or $prior[$i].sha256 -ne $expectedPrior[$task]) { throw 'Prior evidence identity drifted.' }
    $physical = Get-Item (Join-Path $repoRoot $prior[$i].relative_path)
    if ($physical.Attributes -band [IO.FileAttributes]::ReparsePoint -or (Get-FileHash $physical.FullName -Algorithm SHA256).Hash -ne $prior[$i].sha256) { throw 'Prior evidence physical binding failed.' }
}

$runRoot = Split-Path $resultFull -Parent
$probe = Parse-Log (Resolve-Contained (Join-Path $runRoot $result.runtime_log) $runRoot 'Runtime log')
$declared = @($result.runtime_manifest)
if ($declared.Count -ne $probe.Manifest.Count) { throw 'Runtime manifest cardinality drifted.' }
for ($i = 0; $i -lt $declared.Count; $i++) { if ($declared[$i].name -ne $probe.Manifest[$i].name -or [long]$declared[$i].bytes -ne $probe.Manifest[$i].bytes -or $declared[$i].sha256 -ne $probe.Manifest[$i].sha256) { throw 'Runtime manifest mismatch.' } }
foreach ($binding in @('sdk_install_log_sha256','focused_test_log_sha256','app_clean_build_log_sha256','executable_sha256')) {
    $path = if ($binding -eq 'executable_sha256') { Join-Path $repoRoot 'out/build/win-amd64-relwithdebinfo/mcla.exe' } else { Join-Path $runRoot (@{sdk_install_log_sha256='sdk-install.log';focused_test_log_sha256='focused-tests.log';app_clean_build_log_sha256='app-clean-build.log'}[$binding]) }
    if ($result.$binding -notmatch '^[A-F0-9]{64}$' -or (Get-FileHash $path -Algorithm SHA256).Hash -ne $result.$binding) { throw "Physical binding failed: $binding" }
}
if ((Get-Content (Join-Path $runRoot 'focused-tests.log') -Raw) -notmatch 'All tests passed \(5 assertions in 2 test cases\)') { throw 'Focused test total drifted.' }
return [pscustomobject]@{ Passed = $true; ReachedImports = 2; FixedTargets = 6; NotObservedImports = 12; WithheldFfbImports = 8 }
