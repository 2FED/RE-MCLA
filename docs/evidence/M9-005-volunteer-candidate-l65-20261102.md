# M9-005 volunteer candidate l65 (2026-11-02)

Build/packaging subagent report, for review by the primary agent. I built the l65 volunteer
candidate from a clean clone of `main` after the SDK v0.20.0.7 pin, including a fresh build of
the SDK from that clone (D3D12 only, the volunteer configuration), with the l56 options plus
`MCLA_M10_SPEEDUP_DEFAULTS=ON` and `MCLA_PREWARM_SEED_DEFAULT=ON` (`MCLA_DEV_OVERRIDES` OFF).
The candidate is ready for the owner's pre-publication session and signing step.

Nothing was signed, staged, published or launched. No `mcla.exe`, `MCLA-Launcher.exe`,
`Launch-MCLA.exe` or game process was started. The only programs run were:

- `rexglue.exe --version` and `rexglue.exe codegen`;
- CMake/Ninja and SDK `ctest`;
- the 45 host unit test executables (the image client test starts the fake helper);
- ast-grep;
- Python for the byte scans, section mapping and the pins file;
- PowerShell 7 to parse the scripts and to dry-run the entries merge.

No key, save, profile or LocalAppData file was opened, the HKCU CMake package registry was not
written, and no git write command was run in the project repository. `private/l34-src`, the
l56 and the l65 dev directories were only read. Spark was not used: every step is a
deterministic clone, build or test gate, or a byte comparison.

**l56 status:** l56 (`M9-005-volunteer-candidate-l56-20261101.md`, host `BAED27A7...63D1`) was
signed and published (list issued 1793549989: l48, l49, l52, l56; `DebugBundle` `0a96366`).
l65 supersedes l56.

## What l65 adds over l56

From SDK v0.20.0.7 (`43577c8`), switched on by the host at start-up / graphics-plugin load
(`MCLA_M10_SPEEDUP_DEFAULTS`; each value only where the cvar exists, still holds its SDK default
and was not set by the user; one `MCLA m10 default: name=value` log line per value written and
one summary line per phase):

- **KI-047 fix with the targeted wait** (`d3d12_readback_sync_targeted=true`, early submission
  every 512 draws). The composite read-back sync stays ON (`MCLA_READBACK_SYNC_COMPOSITE_DEFAULT`)
  but waits only for the submission that holds the composite instead of draining the whole GPU
  queue. The full drain in l56 cost about **22% at 720p and 15% at 1440p** on the owner PC
  (l63 base, `M10-l63-pc-ab-20261102.md` section 2; 1080p estimated 25-30%).
- **Read-back elision** (`d3d12_readback_elide_unread=true`): GPU copies of read-back slots the
  CPU never reads are skipped (73-74% elided in l63; CPU bytes copied identical, guard counters 0).
- **CP event wake** (`d3d12_wrm_event_wake=true`): WAIT_REG_MEM waits on an event instead of
  sleeping.
- **Replay batched wake** (`d3d12_replay_batched_wake=true`): the replay worker is woken in
  batches (about 0.4 core freed; mainly for the Deck).
- `memory_watch_census=false` and `d3d12_readback_gpu_timing=false` (diagnostics not useful in
  volunteer logs). Bridging and upload-ahead are **not** set (net negative on PC in l63).
- **Native refcount** (host, default on since `mcla_native_refcount.cpp`): native XDK D3D
  AddRef/Release and XAPI Interlocked helpers without the emulated interrupt lock.
- **KI-055 fix:** a normal window close no longer leaves an `unexpected-guest-exit` `.partial`
  F10 folder.
- **KI-058 fix:** the diagnostics export reports the recorded guest vblank rate (240 for
  Unlocked).
- **Local shader store pre-warm** (`MCLA_PREWARM_SEED_DEFAULT`): before the first launch of a new
  build the launcher seeds the build's shader store from the player's other local stores
  (`store-seed.txt` per session). First launch of a new build about 1 s longer (l63: 544 ms
  seeding plus about 1.2 s pipeline creation once), then no stutters from shader compilation
  (l63: 0 pipeline waits in every minute of six sessions, against 2-448 per minute in l53).
- **Startup pipeline thread fix** (SDK `9d8ede4`): storage pipeline creation uses the needed
  thread count and waits for all creation threads.
- Read-back slot diagnostics fix (SDK `43577c8`): slots elided by elision count as unwritten
  (`prev_unwritten`), not unfenced.
- Everything in l56 (perf split and livery diag v2/v3 ON, always-on bounded pipeline wait,
  KI-022 wheel resync, KI-050/051 Deck fixes, D3D12 everywhere) is carried unchanged.

**Measured gains (owner, l63 "all" vs "base", BENCH-A, `M10-l63-pc-ab-20261102.md`):**
1440p ABAB stationary 32.5 -> 40.3 fps (**+24%**), lap 1 +15%, lap 2 +21%, p99 frame time and
>34 ms frames far lower; 720p stationary 43.1 -> 47.3 fps (**+9.7%**), lap 2 +11%. Forced KI-047
wait 5.1 -> 1.7 ms/frame (720p), 14.9 -> 7.7-9.0 ms/frame (1440p); GPU read-back 4.0 -> 1.0
ms/frame; `forced_wait_failed=0`, `fallbacks_to_full=0` everywhere. The owner reported the
police livery correct in all l63 arms. The l63 "all" arm also had bridging and upload-ahead
(net negative) and census/GPU timing on, so l65 is expected to be at least as fast; **the exact
l65 combination has not run.**

## Clone identity

| Item | Value |
| --- | --- |
| Project | Branch `main` cloned from the main checkout into `private/l65-src`. Clone HEAD = checkout HEAD = `1ff00dcea2f5a596401c444eb0bdaaa7d2e27c78` (task(M10): pin ReXGlue SDK v0.20.0.7 (43577c8)). |
| SDK pin | Gitlink `43577c89c7f5409ce5b2501d5d183b0f00bcbe04`, origin `https://github.com/2FED/rexglue-sdk.git`; `describe --exact-match` gives `v0.20.0.7` (annotated, tag object `4d41f2f1`). |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b`, origin `https://github.com/2FED/SDL.git`. |
| Submodules | 26 recursive, none `-`/`+`/`U`; the 25 nested SHAs equal l56's. 15 libmspack links materialized in the clone. |
| SDK install | `private/l65v-sdk-install`, built from the clone (`l65v-sdk-b`, output `l65v-sdk-o`) with the l56 flags (D3D12 only). `rexglue --version` and `rexglueConfigVersion` = `0.20.0.7`. SDK ctest 2006/2006 (one rerun for the manifest temp-dir race), 4 known skips. |
| Host build dir | `private/l65-build`. |
| Script | `private/l65-clean-clone.ps1`, generated from `l56-clean-clone.ps1` by `private/l65-work/mk_clone.py`. Full results in [M9-001 clean clone v0.20.0.7](M9-001-clean-clone-v0.20.0.7-20261102.md). |

## Build and test results

| # | Check | Result |
| --- | --- | --- |
| 1 | SDK build, version, ctest | **PASS.** 1442 actions; `0.20.0.7` from `rexglue --version` and the config file; ctest 2006/2006 on the rerun (first run 2005/2006: `Manifest: parses sdk_version when present`, `remove_all ... being used by another process`), 4 known BitStream skips. |
| 2 | Codegen | **PASS.** 67 files, 133,916,828 bytes; every SHA-256 equals the main checkout's `generated/default`. |
| 3 | Host build | **PASS.** l56 volunteer options plus `MCLA_M10_SPEEDUP_DEFAULTS=ON`, `MCLA_PREWARM_SEED_DEFAULT=ON`; `MCLA_DEV_OVERRIDES=OFF` (CMakeCache and `build.ninja` confirm). 0 compiler warnings, 0 errors. CMake defaults in `CMakeLists.txt` stay OFF. |
| 4 | Unit tests | **PASS, first run.** 45/45 executables return 0 (`private/l65-work/l65-host-tests.log`), including `mcla_m10_defaults_test` (891 checks), `mcla_vulkan_device_select_test`, `mcla_shader_store_seed_test`, `mcla_launcher_shader_seed_win_test`, `mcla_native_refcount_test`, `mcla_dev_overrides_test`; all summaries equal the l65 dev run. |
| 5 | ast-grep 0.50.0 in the clone | **PASS.** `scan` exit 0; `test` 3/3. |
| 6 | Default-on strings | **PASS.** `mcla.exe`: `MCLA m10 default: {}={}`, both `MCLA m10 defaults (...)` summary formats, `mcla_m10_speedup_defaults`; `MCLA readback sync: fix ON, build default {}=true` and the `mcla-dev-overrides.txt` single-line handling as in l56; no `MCLA dev overrides` reader strings. `MCLA-Launcher.exe`: `store-seed.txt`, `MCLA store seed: source=`; no override-file name. `rexgpu-xenos.dll`: the four speed-up cvars, `d3d12_readback_gpu_timing`, `prev_unwritten`. |
| 7 | Path strings | **PASS.** Seven input files, ASCII and UTF-16LE, case-insensitive. Only machine-code hits: six guest-address immediates and one `.pdata` RVA in `mcla.exe`, one `movdqu` encoding in `MCLA-Launcher.exe`, one call rel32 in `rexruntime.dll` (offsets and sections in the M9-001 report); not paths. |
| 8 | Expiry marker | **PASS.** UTF-16 `MCLA test-build admission` once in `mcla.exe`. |

## Artifacts (private/l65-build), copied to private/l65-vol-input

| File | Bytes | SHA-256 |
| --- | --- | --- |
| `mcla.exe` | 53254144 | `422A20A604ABF15986CCCCD13C027467EF8BFBF861519C6D3774BC420F02E499` |
| `MCLA-Launcher.exe` | 1442304 | `AA2A629EEC6583DDC44131CC0365D16D8ECC85148A354E9356DE91958B711C5B` |
| `mcla_crash_handler.exe` | 211968 | `D2A4E2EE219319A43ED0D4A8FCD155CA375DDBA96118AB87C7E1150D83D20A8F` |
| `mcla_diagnostic_image_helper.exe` | 60928 | `3712D81EB3938414DC64651F20160BE9FE3219CB63F67F56C816DC95E966E5F1` |
| `rexruntime.dll` | 11018240 | `991AEED22913B485FBB1641F2CF6820521712E79B9EE342833C09A685C5FA41D` |
| `rexgpu-xenos.dll` | 3210240 | `10153D6F91831C0DDB57D4897E1ED7DBE0EE41D7A7516A1D34FC6C997B828BC7` |
| `TracyClient.dll` | 246784 | `DC4AD050012AB61C92C452BDCA0EF3452730B67ADBA947251309EB91769AEB6E` |

`private/l65-vol-input` holds exactly these seven files; the prepare script adds
`mcla-test-build.ticket`. The three DLLs are byte-identical to `private/l65v-sdk-install/bin`.
Pins: `private/l65-pins.json`; hash list `private/l65-work/l65-vol-input.sha256`.
`Launch-MCLA.exe` (not packaged): `2DE732CD21C44425105E129467FF03CE1269691FCD9920BC7376950A93B091D1`.

## Prepare script (owner step)

`private/l65-volunteer-prepare.ps1` (SHA-256
`1D164F88A7ED284135ADC8599A36E99065C41E2AE3B78A7C7C17707F717E513C`) was generated from
`l56-volunteer-prepare.ps1` by `private/l65-work/mk_prepare.py` with `private/l65-pins.json`.
Code changes against l56 (diff of the script bodies): l65 paths for the input, staging, entries
and list files; the seven new pins; carried entries from `l56-entries.json` (was
`l52-entries.json`); updated messages. The V2 flow is otherwise identical
(`New-MclaTestBuildTicket.ps1 -KillSwitch -Capabilities unlocked,1080p,1440p`, grant line
`capabilities 1080p,1440p,unlocked`, CreateNew entries with read-back,
`New-MclaTestBuildLeases.ps1`, staging with the l12 CRT and the tester guide, lease probe,
`Test-MclaVolunteerStaging.ps1 -RequireVolunteer`, receipt grant check). PowerShell 7.4.22
parse: 0 errors for the prepare, clone and test scripts (`private/l65-work/l65-parsecheck.log`).
Run:

```
C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l65-volunteer-prepare.ps1
```

**Entries-merge dry run** (`-DryRunDirectory` on an empty temp directory, log
`private/l65-work/l65-dryrun.log`): **5 entries**: l48 `6FEC02D0...` (valid_until 1795957688),
l49 `0A46AE3E...` (1796023588), l52 `823BFBA4...` (1796045342), l56 `BAED27A7...`
(1796141989), all unchanged from `l56-entries.json`, then the l65 host `422A20A6...` with
valid_until 1796239898 (2026-12-02 21:31 Kyiv at dry-run time; the real run sets it at
signing). A second dry run into the now non-empty directory was refused ("dry-run directory is
not empty", `l65-dryrun-refuse.log`). The temp directory was removed. No `l65-entries.json`,
ticket, list or staging output exists. Note: l48 expires 2026-11-29 and l49 2026-11-30.

## Owner checks before publishing

1. **One PC session on the staged l65 at 720p and at 1440p**, with a police car in view in each.
   - Police livery correct (the targeted wait has the same KI-047 counters as l63 "all", but the
     visual is the acceptance). F10 once on a correct police car.
   - fps against l56 at the same spot: expected clearly higher (l63: about +10% at 720p,
     +15-24% at 1440p).
   - Log: `MCLA m10 default: memory_watch_census=false` (startup) and
     `MCLA m10 default: d3d12_readback_sync_targeted=true`, `d3d12_readback_elide_unread=true`,
     `d3d12_wrm_event_wake=true`, `d3d12_replay_batched_wake=true`,
     `d3d12_readback_gpu_timing=false`; summary `MCLA m10 defaults (graphics plugin): applied=5
     failed=0 ...`. Also `MCLA readback sync: fix ON, build default
     d3d12_readback_sync_composite=true`. `MCLA_READBACK_SYNC kind=minute`: targeted waits equal
     forced, `fallbacks_to_full=0`, `forced_wait_failed=0`.
   - `store-seed.txt` of the first session: `source=seeded ...` (or `source=existing` on later
     launches); first launch about 1 s longer; no pipeline waits in later minutes.
   - **If `%LocalAppData%\MCLA-R-Launcher-v1\mcla-dev-overrides.txt` is still present** from the
     l55/l57+ A/B runs, l65 reads it with the strict l55 parser (as l56): exactly one line
     `mcla_readback_sync_composite=false` turns the KI-047 fix (and with it the targeted wait)
     off; any other content (for example a multi-line l57+ owner file) is ignored and logged,
     and the fix stays on. Remove the file (`private\l55-fix-on.cmd`) before the session so the
     log is clean and the run reflects the volunteer defaults.
2. **Prepare.** Run `l65-volunteer-prepare.ps1` in PowerShell 7. Expect `DONE`,
   `SIGNED_LEASES_VERIFIED entries=5`, an audit with 0 findings and the receipt grant
   `1080p,1440p,unlocked`. Publishing `l65-active-builds.txt` needs the owner's explicit yes.

## Tester checks

1. Police livery: several police encounters per session; F10 on any wrong livery and once on a
   correct police car.
2. fps versus l56 at the same place and resolution (expected higher); report resolution and fps.
3. First launch of l65 slightly longer, then no shader stutters (also after changing resolution
   once); `store-seed.txt` in the session folder.
4. No `.partial` F10 folder after a normal close (KI-055).
5. Windows smoke as for l56 (launcher 1080p/1440p/Unlocked, no 2160p; kill switch offline/online;
   wheel and Steam Input unchanged); one second-machine tester; the Steam Deck when convenient
   (replay batched wake and CP event wake are expected to help most there; Deck numbers for this
   combination are not measured).

## Limits and risks

- Nothing was signed, staged, published or launched; no in-game, owner or Deck check is claimed
  for l65. **The exact l65 switch combination has never run** (l63 "all" minus bridging,
  upload-ahead, census and GPU timing).
- Targeted sync adds early submissions (+1-3 ms/frame CP work in l63 "all"); `kick_draws` stays
  512. CP event wake raises CP kernel time (PC evidence with vsync only).
- Pre-warm: a volunteer with many large profiles reads up to 256 MiB once per new build (seconds
  on a slow disk or an SD card); volunteer builds have no off switch for seeding.
- The `mcla-dev-overrides.txt` single-line read-back sync switch (l55 behaviour) is still compiled
  in, as in l56; testers are not told about it.
- Benign guest-address and instruction byte matches in the path scan (check 7).
- The clone came from the local committed `main`; the SDK and all submodules from GitHub.
  Output is not bit-identical across checkout paths.

## Owner signature and publication (2026-11-03)

- The owner ran `private/l65-volunteer-prepare.ps1`; the signed `l65-active-builds.txt` (issued 1793686298) lists 5 entries: l49 `0A46AE3E...`, l65 `422A20A6...` (expiry 1796278298), l48 `6FEC02D0...`, l52 `823BFBA4...`, l56 `BAED27A7...`; staging `private/l65-volunteer` present.
- With the owner's explicit approval ("publish l65") the primary agent published the list to `DebugBundle` of `2FED/RE-MCLA` (commit `d7a6d93`, replacing `0a96366`); the raw URL and the contents API returned bytes identical to the signed file.
- The owner plays one 720p and one 1440p session with a police car in view before sharing l65 with testers; sharing is the owner's step.
