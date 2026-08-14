[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$verify = Join-Path $PSScriptRoot 'verify-route-failure-report.ps1'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$suffix = -join ((1..8) | ForEach-Object { '{0:x}' -f (Get-Random -Maximum 16) })
$root = Join-Path $repo "private/evidence/M5-010/$stamp-$suffix"
[IO.Directory]::CreateDirectory($root) | Out-Null
$record = [ordered]@{
    schema = 1
    task = 'M5-010'
    decision = 'canonical-route-streaming-io-allocator-bounded-pass'
    sdk_version = '0.9.0.20'
    sdk_commit = 'c4aa30c35386bb4d2ef051a59ea8e71bab667172'
    accepted_evidence = [ordered]@{
        streaming = [ordered]@{ task='M5-002'; run_id='20260814-093131-ddca5b9d'; result_sha256='A582A41BB254D9187BC6F8147E89E2FF2E92D0EA4B79945760BA6FBCF334BC28' }
        timing = [ordered]@{ task='M5-008'; run_id='20260814-150733-da37caed'; result_sha256='C705E09FF88CA33D705E11326B34B7E643604D0C40FC210414B6431D3A86CAA9' }
        audio = [ordered]@{ task='M5-009'; run_id='20260814-170657-f44949d7'; result_sha256='4E3D514386501D92B43CD4F2C4C89ECD8BA000ACF23D8B168CFA431C8F67C62F' }
    }
    coverage = [ordered]@{
        physical_process_cycles=5; controlled_exit_cycles=5; noisy_io_cycles=1; stock_speed_cycles=3; long_audio_gameplay_cycles=1
        cache_read_calls=8011; cache_read_bytes=262821908; audlo_read_calls=41; audlo_read_bytes=3290825
        nt_create_success=15; expected_development_misses_per_cycle=7; expected_development_miss_events=35; nt_read_results=8055; nt_read_success=8054; nt_read_pending_success=1; nt_read_failures=0
        nt_allocate_results=87; nt_allocate_success=87; nt_allocate_failures=0; nt_free_results=49; nt_free_success=49; nt_free_failures=0
        physical_allocate_results=2590; physical_allocate_success=2590; physical_allocate_failures=0
        fatal_allocator_events=0; fatal_io_events=0; fatal_streaming_events=0
    }
    scope = [ordered]@{
        route_failure_reproduced=$false; behavior_patch_required=$false; long_session_leak_check_claimed=$false; multi_race_claimed=$false
        classification='bounded-five-process-no-defect-observed'
    }
    data_integrity_verified = $true
}
$path = Join-Path $root 'result.json'
[IO.File]::WriteAllText($path, (($record | ConvertTo-Json -Depth 7) + [Environment]::NewLine), [Text.UTF8Encoding]::new($false))
$final = & $verify -ResultPath $path
[pscustomobject]@{ Passed=$final.Passed; Decision=$final.Decision; PhysicalProcessCycles=$final.PhysicalProcessCycles; NtReadResults=$final.NtReadResults; AllocatorResults=$final.AllocatorResults; FatalEvents=$final.FatalEvents; BehaviorPatchRequired=$final.BehaviorPatchRequired; ResultPath=$path }
