[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-locale-path-smoke.ps1'
$runner = Join-Path $PSScriptRoot 'run-locale-path-smoke.ps1'
$root = Join-Path $repo ('private\evidence\M4-010\test-' + [guid]::NewGuid().ToString('N'))
$user = Join-Path $root 'user-fixture'; $utf8 = [Text.UTF8Encoding]::new($false)
[IO.Directory]::CreateDirectory($user) | Out-Null

function Write-Bmp([string]$Path) {
    $bytes = [byte[]]::new(3686454); $bytes[0] = 0x42; $bytes[1] = 0x4D; [BitConverter]::GetBytes([int]1280).CopyTo($bytes, 18); [BitConverter]::GetBytes([int]720).CopyTo($bytes, 22); [BitConverter]::GetBytes([uint16]32).CopyTo($bytes, 28); [IO.File]::WriteAllBytes($Path, $bytes)
}

$config = 'LOCALE_AUDIT_CONFIG v=1 enabled=1 language=4 country=34 language_valid=1 country_valid=1 record_limit=3'
$xconfig = 'LOCALE_AUDIT_XCONFIG v=1 setting=language value=4 result=00000000 match=1'
$capture = 'MCLA graphics: nontrivial guest frame captured 1280x720, rgb555 bins 10, luma p05 1, luma p95 2, modal permille 10, nonmodal grid cells 4'
$summary = 'LOCALE_AUDIT_SUMMARY v=1 phase=title status=PASS language=4 country=34 xget_reach=unreached xget_calls=0 xget_matches=0 xconfig_language_calls=25 xconfig_language_value_calls=25 xconfig_country_reach=unreached xconfig_country_calls=0 xconfig_country_value_calls=0 xconfig_failures=0 mismatches=0 records=1 overflow=0 dropped_records=0'
$project = 'MCLA locale: title route summarized'
$vfs = "MCLA VFS: game: and d: resolve 3/3 expected disc files on \Device\Harddisk0\Partition1`nMCLA VFS: root-escape paths rejected`nMCLA VFS: write, create, delete, and writable-map requests denied"
$lifecycle = "Window closing, shutting down...`nExecution complete`nTitle terminated; hard-exiting process."
$log = Join-Path $root 'mcla.log'; $bmp = Join-Path $user 'mcla-first-frame.bmp'; Write-Bmp $bmp

function Accept([string]$Text) { [IO.File]::WriteAllText($log, $Text, $utf8); $null = & $verify -ProbeOnly -LocaleOnly -RuntimeLogPath $log -BmpPath $bmp -Language 4 -Country 34 }
function Reject([string]$Name, [string]$Text) { [IO.File]::WriteAllText($log, $Text, $utf8); $failed = $false; try { $null = & $verify -ProbeOnly -LocaleOnly -RuntimeLogPath $log -BmpPath $bmp -Language 4 -Country 34 } catch { $failed = $true }; if (-not $failed) { throw "Negative '$Name' was accepted." } }

try {
    $positive = "$vfs`n$config`n$xconfig`n$capture`n$summary`n$project`n$lifecycle"
    Accept $positive
    $cases = [ordered]@{
        missing_config = $positive.Replace("$config`n", '')
        duplicate_config = $positive.Replace($config, "$config`n$config")
        wrong_language = $positive.Replace('language=4', 'language=5')
        wrong_country = $positive.Replace('country=34', 'country=35')
        invalid_language = $positive.Replace('language_valid=1', 'language_valid=0')
        missing_xconfig = $positive.Replace("$xconfig`n", '')
        xconfig_wrong_value = $positive.Replace('setting=language value=4', 'setting=language value=5')
        xconfig_failure = $positive.Replace('result=00000000 match=1', 'result=C000000D match=0')
        duplicate_xconfig = $positive.Replace($xconfig, "$xconfig`n$xconfig")
        reached_xget = $positive.Replace($summary, "LOCALE_AUDIT_XGET v=1 language=4 result=4 match=1`n$summary")
        reached_country = $positive.Replace($summary, "LOCALE_AUDIT_XCONFIG v=1 setting=country value=34 result=00000000 match=1`n$summary")
        fail_summary = $positive.Replace('status=PASS', 'status=FAIL')
        missing_language_calls = $positive.Replace('xconfig_language_calls=25', 'xconfig_language_calls=0')
        mismatched_value_calls = $positive.Replace('xconfig_language_value_calls=25', 'xconfig_language_value_calls=24')
        summary_failure = $positive.Replace('xconfig_failures=0', 'xconfig_failures=1')
        summary_mismatch = $positive.Replace('mismatches=0', 'mismatches=1')
        summary_records = $positive.Replace('records=1', 'records=2')
        overflow = $positive.Replace('overflow=0', 'overflow=1')
        missing_vfs_resolve = $positive.Replace("MCLA VFS: game: and d: resolve 3/3 expected disc files on \Device\Harddisk0\Partition1`n", '')
        missing_vfs_escape = $positive.Replace("MCLA VFS: root-escape paths rejected`n", '')
        missing_vfs_readonly = $positive.Replace("MCLA VFS: write, create, delete, and writable-map requests denied`n", '')
        bad_order = "$vfs`n$config`n$capture`n$xconfig`n$summary`n$project`n$lifecycle"
        missing_capture = $positive.Replace("$capture`n", '')
        missing_project = $positive.Replace("$project`n", '')
        missing_close = $positive.Replace("Window closing, shutting down...`n", '')
        fatal = $positive.Replace($project, "[fatal] locale crash`n$project")
        ppc_unimplemented = $positive.Replace($project, "PPC_UNIMPLEMENTED`n$project")
        device_lost = $positive.Replace($project, "D3D12 device lost`n$project")
    }
    foreach ($case in $cases.GetEnumerator()) { Reject $case.Key $case.Value }

    $runnerText = [IO.File]::ReadAllText($runner, $utf8); $verifierText = [IO.File]::ReadAllText($verify, $utf8)
    $sourceNeedles = @('From-CodePoints', '0x041B', '0x0457', "Name = '01-en-us'; Language = 1; Country = 103", "Name = '02-fr-fr'; Language = 4; Country = 34", "Name = '03-ru-ru'; Language = 12; Country = 88", '--xam_locale_audit=true', '--mcla_first_frame_settle_seconds=35', 'Close-Exact', 'Get-ExactProcesses', 'verify-game-manifest.ps1', 'xget_language_title_reached = $false', 'country_title_reached = $false', 'filesystem_test_assertions = 32', 'locale_test_assertions = 22', 'XENOS_AUDIT_PIPELINE_SUMMARY v=1 phase=checkpoint', 'XENOS_AUDIT_CP_SUMMARY v=1 phase=checkpoint', 'XENOS_AUDIT_RT_SUMMARY v=1 phase=checkpoint', 'XENOS_AUDIT_RESOLVE_SUMMARY v=1 phase=checkpoint')
    foreach ($needle in $sourceNeedles) { if (-not $runnerText.Contains($needle)) { throw "Runner source contract is missing '$needle'." } }
    foreach ($needle in @('xget_reach=unreached', 'xconfig_country_reach=unreached', 'Read-Utf8Shared', 'Exact Unicode cycle topology failed.', 'Canonical MCLA process still exists.', 'Result contains a private or absolute path', 'verify-render-path-smoke.ps1', 'LocalizedPrompt', "'english-reference'", "'localized-prompt'", 'prompt_reference_sha256', 'prompt_edge_correlation_ppm', 'EF1EC173D0810A145273006D9E68967781A43DAA4A66E679FC9580612AFA9C7D')) { if (-not $verifierText.Contains($needle)) { throw "Verifier source contract is missing '$needle'." } }
    $renderVerifierText = [IO.File]::ReadAllText((Join-Path $repo 'scripts\verify-render-path-smoke.ps1'), $utf8)
    foreach ($needle in @('Localized title-prompt ROI edge correlation is below 0.90', '$localizedPromptPpm -lt 900000', 'Localized prompt reference SHA-256 does not match its pinned value.')) { if (-not $renderVerifierText.Contains($needle)) { throw "Render verifier source contract is missing '$needle'." } }
    [pscustomobject]@{ Passed = $true; PositiveFixtures = 1; FailClosedNegatives = $cases.Count; SourceContractChecks = $sourceNeedles.Count + 15 }
} finally {
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
