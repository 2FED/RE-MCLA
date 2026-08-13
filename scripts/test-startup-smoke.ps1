[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verifier = Join-Path $PSScriptRoot 'verify-startup-smoke.ps1'
$resultVerifier = Join-Path $PSScriptRoot 'verify-startup-smoke-result.ps1'
$runner = Join-Path $PSScriptRoot 'run-startup-smoke.ps1'
$sourcePath = Join-Path $repoRoot 'src/mcla_app.cpp'
$fixtureRoot = Join-Path $repoRoot ('private/test-startup-smoke-' + [guid]::NewGuid().ToString('N'))
$fixturePath = Join-Path $fixtureRoot 'mcla.log'
$utf8 = [System.Text.UTF8Encoding]::new($false)
$positive = @'
[info] [app] MCLA lifecycle: logging ready
[info] [gpu] MCLA graphics: selected GPU plugin 'xenos'
[info] [sys] GPU plugin 'xenos' loaded (rexgpu-xenosrd.dll)
[info] [ppc] MCLA module config: static image 82000000-829E0000, code 82130000-827CD054, 30026 mappings
[info] [sys] Runtime initialized successfully
[info] [sys] Loading XEX image: game:\default.xex
[info] [ppc] MCLA module identity: title 545407F8, media 5940C9DB, image 82000000-829E0000, entry 821322B8
[info] [ppc] MCLA module config: entry 821322B8 registered in dispatch range 82130000-827CD054
[info] [vfs] MCLA VFS: game: and d: resolve 3/3 expected disc files on \Device\Harddisk0\Partition1
[info] [vfs] MCLA VFS: write, create, delete, and writable-map requests denied
[info] [sys] KernelState: Preparing module launch...
[info] [core] Initializing shader storage for title 545407F8...
[info] [gpu] SetInterruptCallback(82411478, 40002080)
[debug] [gpu] Creating graphics pipeline with VS A, PS B
[debug] [apu] AudioWorker: dispatching callback 823F56D0 with arg 1 for client 0
'@

function Write-Fixture {
    param([Parameter(Mandatory)][string]$Text)
    [System.IO.Directory]::CreateDirectory($fixtureRoot) | Out-Null
    [System.IO.File]::WriteAllText($fixturePath, $Text, $utf8)
}

function Assert-Rejected {
    param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Text)
    Write-Fixture $Text
    try {
        & $verifier -RuntimeLogPath $fixturePath | Out-Null
    } catch {
        return
    }
    throw "Negative startup-smoke fixture '$Name' was accepted."
}

try {
    $source = Get-Content -LiteralPath $sourcePath -Raw
    $sourceRequirements = @(
        'constexpr uint32_t kExpectedTitleId = 0x545407F8;',
        'constexpr uint32_t kExpectedMediaId = 0x5940C9DB;',
        'const auto *execution_info = xex ? xex->opt_execution_info() : nullptr;',
        'execution_info->title_id != kExpectedTitleId',
        'execution_info->media_id != kExpectedMediaId',
        'xex->image_size() != kExpectedImageSize',
        'MCLA module identity: title {:08X}, media {:08X}, image '
    )
    foreach ($requirement in $sourceRequirements) {
        if ($source.IndexOf($requirement, [System.StringComparison]::Ordinal) -lt 0) {
            throw "Startup-smoke source contract is missing '$requirement'."
        }
    }

    Write-Fixture $positive
    $verified = & $verifier -RuntimeLogPath $fixturePath
    if (-not $verified.Passed -or $verified.MarkerCount -ne 15 -or
        $verified.TitleId -ne '545407F8' -or $verified.MediaId -ne '5940C9DB' -or
        -not $verified.AudioCallbackReached) {
        throw 'Positive startup-smoke fixture returned an unexpected result.'
    }

    Assert-Rejected missing-identity ($positive -replace '(?m)^.*MCLA module identity.*\r?\n?', '')
    Assert-Rejected wrong-media $positive.Replace('media 5940C9DB', 'media DEADBEEF')
    Assert-Rejected wrong-image $positive.Replace('image 82000000-829E0000, entry', 'image 81000000-819E0000, entry')
    Assert-Rejected wrong-order ($positive.Replace(
        "[info] [sys] KernelState: Preparing module launch...`n[info] [core] Initializing shader storage for title 545407F8...",
        "[info] [core] Initializing shader storage for title 545407F8...`n[info] [sys] KernelState: Preparing module launch..."))
    Assert-Rejected missing-vfs ($positive -replace '(?m)^.*MCLA VFS: game:.*\r?\n?', '')
    Assert-Rejected missing-read-only-vfs ($positive -replace '(?m)^.*MCLA VFS: write, create.*\r?\n?', '')
    Assert-Rejected no-gpu ($positive + '[warning] gpu_plugin not set')
    Assert-Rejected fatal ($positive + '[critical] startup failed')
    Assert-Rejected invalid-function ($positive + 'Call to invalid or unregistered function')
    Assert-Rejected ppc-unimplemented ($positive + 'PPC_UNIMPLEMENTED')
    Assert-Rejected guest-crash ($positive + 'REX_GUEST_CRASH schema=1')
    Assert-Rejected gpu-load-failure ($positive + "GPU plugin 'xenos' failed to load")
    Assert-Rejected post-launch-bink ($positive + 'game:\intro720.bik')

    Write-Fixture $positive
    $logItem = Get-Item -LiteralPath $fixturePath
    $logHash = (Get-FileHash -LiteralPath $fixturePath -Algorithm SHA256).Hash
    $resultPath = Join-Path $fixtureRoot 'result.json'
    $validResult = [ordered]@{
        schema = 1
        task = 'M3-014'
        startup_timeout_seconds = 20
        cleanup_timeout_seconds = 5
        startup_elapsed_milliseconds = 7000
        cleanup_elapsed_milliseconds = 100
        total_elapsed_milliseconds = 7100
        termination_reason = 'expected_markers_reached'
        process_exited_early = $false
        harness_stop_issued = $true
        process_signal_confirmed = $true
        process_cleanup_confirmed = $true
        expected_marker_count = 15
        title_id = '545407F8'
        media_id = '5940C9DB'
        image_range = '82000000-829E0000'
        entry_point = '821322B8'
        gpu_plugin_loaded = $true
        vfs_verified = $true
        vfs_read_only_verified = $true
        module_launch_reached = $true
        graphics_pipeline_reached = $true
        audio_callback_reached = $true
        fatal_markers = 0
        post_launch_bink_evidence = 0
        executable_sha256 = ('A' * 64)
        default_xex_size = 9252864
        default_xex_sha256 = 'C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432'
        runtime_log_sha256 = $logHash
        runtime_log_bytes = $logItem.Length
    }
    function Write-ResultFixture {
        param([Parameter(Mandatory)][object]$Value)
        [System.IO.File]::WriteAllText(
            $resultPath,
            (($Value | ConvertTo-Json -Depth 4) + [Environment]::NewLine),
            $utf8
        )
    }
    function Assert-ResultRejected {
        param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][scriptblock]$Mutate)
        $candidate = $validResult | ConvertTo-Json -Depth 4 | ConvertFrom-Json
        & $Mutate $candidate
        Write-ResultFixture $candidate
        try {
            & $resultVerifier -ResultPath $resultPath -RuntimeLogPath $fixturePath | Out-Null
        } catch {
            return
        }
        throw "Negative startup-smoke result fixture '$Name' was accepted."
    }
    Write-ResultFixture $validResult
    $verifiedResult = & $resultVerifier -ResultPath $resultPath -RuntimeLogPath $fixturePath
    if (-not $verifiedResult.Passed -or -not $verifiedResult.ControlledTerminationVerified) {
        throw 'Positive startup-smoke result fixture returned an unexpected result.'
    }
    Assert-ResultRejected wrong-log-bytes { param($value) $value.runtime_log_bytes++ }
    Assert-ResultRejected wrong-log-hash { param($value) $value.runtime_log_sha256 = ('B' * 64) }
    Assert-ResultRejected early-exit { param($value) $value.process_exited_early = $true }
    Assert-ResultRejected wrong-termination { param($value) $value.termination_reason = 'early_exit' }
    Assert-ResultRejected missing-harness-stop { param($value) $value.harness_stop_issued = $false }
    Assert-ResultRejected missing-process-signal { param($value) $value.process_signal_confirmed = $false }
    Assert-ResultRejected missing-cleanup { param($value) $value.process_cleanup_confirmed = $false }
    Assert-ResultRejected excessive-cleanup-time { param($value) $value.cleanup_elapsed_milliseconds = 5001 }

    $preflightBuild = Join-Path $fixtureRoot 'bad-build'
    $preflightGame = Join-Path $fixtureRoot 'bad-game'
    [System.IO.Directory]::CreateDirectory($preflightBuild) | Out-Null
    [System.IO.Directory]::CreateDirectory($preflightGame) | Out-Null
    [System.IO.File]::WriteAllBytes((Join-Path $preflightBuild 'mcla.exe'), [byte[]](1, 2, 3))
    [System.IO.File]::WriteAllBytes((Join-Path $preflightBuild 'rexgpu-xenosrd.dll'), [byte[]](4, 5, 6))
    [System.IO.File]::WriteAllBytes((Join-Path $preflightGame 'default.xex'), [byte[]](7, 8, 9))
    $preflightRejected = $false
    try {
        & $runner -BuildRoot $preflightBuild -GameRoot $preflightGame `
            -StartupTimeoutSeconds 10 | Out-Null
    } catch {
        $preflightRejected = $_.Exception.Message -match 'size/hash mismatch'
    }
    if (-not $preflightRejected) {
        throw 'Corrupt default.xex preflight fixture was not rejected before launch.'
    }

    [pscustomobject]@{
        Passed = $true
        SourceContractVerified = $true
        PositiveCases = 2
        NegativeCases = 22
    }
} finally {
    if (Test-Path -LiteralPath $fixtureRoot -PathType Container) {
        [System.IO.Directory]::Delete($fixtureRoot, $true)
    }
}
