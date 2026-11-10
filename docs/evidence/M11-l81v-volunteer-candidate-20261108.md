# M11 volunteer candidate l81v (2026-11-08)

Build/packaging subagent report, for review by the primary agent. l81v is the volunteer build of the l81 sources. It uses host tree `a9de776` and the SDK install `private/l81-sdk-install` (`2aff82e`). The build uses the l70 volunteer options; the dev-override reader is compiled out. The candidate is ready for the owner's signing step and pre-share check.

**What was not done.**
- **Not signed, not staged, not published, not launched.**
- No `mcla.exe`, `MCLA-Launcher.exe`, `Launch-MCLA.exe` or game process was started.
- No key, save, profile, LocalAppData or registry entry was written.
- There were no git writes and no push or upload to RE-MCLA. Publication happens only on the owner's explicit "publish l81v".

**Standing rule.** The KI-067/KI-072 rule held: `private/tools/wait-for-idle-pc.ps1` ran before every build and test step, and every step started on IDLE.

**Spark** was not used: every step was a deterministic build, test, byte scan or parser check.

## What volunteers get compared with l70

l70 (`M9-005-volunteer-candidate-l70-20261105.md`, host `146FBBB5...4A25`, SDK `v0.20.0.8`) is the build volunteers have now. Status words are copied from `docs/known-issues.md` at `73cf3b3`.

**Fixes and changes in l81v that volunteers get by default:**

| Area | Change in l81v | Status (known-issues) |
|---|---|---|
| **Deck saves (KI-068)** | Stable save creation time in the SDK content layer. The second save on the Deck no longer shows "storage device removed". | Fixed, owner-confirmed on the Deck (l71) |
| **Doubled / hollow voice and music (KI-071)** | XMA frame-walk fix (W1), on by default. | Fixed; owner by-ear check of the default PASS on l78 |
| **Car reflections / neon lighting (KI-013)** | MIN/MAX blend source-factor fix, **on by default since l81**. | Fixed (owner PASS on the l80 blend arm). The default-on build l81 is not yet owner-run. |
| **Photo album preview (KI-056)** | Fix A: the new photo shows in the save-slot chooser after hovering an occupied slot. **On by default since l81.** | Fixed (owner PASS on l72 with the arm). The default-on build l81 is not yet owner-run. |
| **Read-back steady-key exemption + deferred fence writes (KI-047 cost, KI-074)** | The per-frame forced read-back wait on the steady key is skipped (about +6 fps on South Central Tour). Fence writes wait for the real GPU, which removes the flicker and floating cars the exemption alone caused. Both on by default since l78. | KI-074 mitigated (owner PC regression PASS on l78); KI-074b despawns still open |
| **PC performance defaults (Windows)** | Placed texture heaps, timer-queue blocking wait and the Canary XMA fixes, on by default on Windows since l80. | Owner A/B on l79 and l80 |
| **Deck audio work (KI-060)** | Under Wine: short-sleep high-resolution timer and audio worker priority (since l80); zero-delay backoff and game audio thread priority (since l81). | KI-060 open. The l81 Deck change is not yet owner-run. |
| **F10 no longer freezes the game (KI-070)** | Diagnostics package without a live minidump. | Fixed in code; confirmed on l71 and later |
| **Diagnostics** | `MCLA_CPU v=1` census line (KI-067), crash and F10 log flush. | Not applicable |

The owner-only probes (album, pursuit, XMA consume, lock probe) are compiled in but off by default. The A/B control keys exist only in owner builds.

**Carried from l70:** the KI-035 garage-freeze fix, the KI-048 texture soft cap, the `MCLA_VRAM` census, the Proton-only M10 speed-up defaults, the KI-065 F10/save guard, and the KI-064 launcher sessions fix.

**Not new for volunteers:** the arm-file mechanism is absent (`MCLA_DEV_OVERRIDES` OFF). The 2 KiB / 4 KiB arm limits are irrelevant for them.

**Tester guide.** `docs/volunteer-tester-guide.txt` was not changed by this task. The primary should check its "KNOWN PROBLEMS" section against the fixes above before publication.

## Sources and options

| Item | Value |
|---|---|
| Host source | Scratch copy `l81v-host-src` of checkout HEAD `73cf3b33b70c45b1b07e91a2d25b98200067b01c`. Build inputs (`src`, `tests`, `CMakeLists.txt`, `cmake`, `generated`, `scripts`) are identical to host tree `a9de776`; the later commits are docs only. |
| SDK | `private/l81-sdk-install`, ReXGlue `2aff82e` (`0.20.0.27-dev.g2aff82e`). CMake prints this version. The three DLLs are the same bytes as the owner build l81. |
| Options | `MCLA_TEST_BUILD_EXPIRY`, `MCLA_RAW_TIMER_DEFAULT_ON`, `MCLA_PERF_SPLIT_DEFAULT`, `MCLA_LIVERY_DIAG2_DEFAULT`, `MCLA_READBACK_SYNC_COMPOSITE_DEFAULT`, `MCLA_PREWARM_SEED_DEFAULT`, `MCLA_M10_SPEEDUP_DEFAULTS`, image and diagnostic tests ON. **`MCLA_DEV_OVERRIDES` OFF.** `MCLA_EXPERIMENTAL_F10_WGC` OFF. Prefix maps, `/pdbaltpath`. `CMakeCache.txt` confirms all of these. |
| Build dir | `private/l81v-host-build` |

**Deviations from l70** (for the primary to accept or reject):

1. l70 was built from a **clean clone** against the **released** SDK tag `v0.20.0.8` (`REXSDK_VERSION=0.20.0.8`). l81v uses the scratch copy and the development SDK install `2aff82e`, with `REXSDK_VERSION` empty. A dev SDK cannot match the version pin, and the task says "same sources".
2. SDK `2aff82e` is not yet published to the `2FED/rexglue-sdk` fork; the last published is v0.20.0.10 = `ca63f94`, the l80 SDK. For source parity with the volunteer DLLs, publish the SDK commit before or together with l81v, as was done for earlier volunteer builds.

## Build and test results

| # | Check | Result |
|---|---|---|
| 1 | Configure | PASS: fresh `private/l81v-host-build`, CMake `0.20.0.27-dev.g2aff82e` |
| 2 | Host build | PASS: 273/273 steps, rc 0, **0 warnings** |
| 3 | Unit tests | PASS on the first run: 53/53 executables return 0 (`mcla_dev_overrides_test` 1139, `mcla_m10_defaults_test` 4477). The 53 test targets in `build.ninja` match the 53 executables built. |
| 4 | ast-grep 0.50.0 | PASS: `scan src tests` rc 0; `test` 3/3 |
| 5 | Dev-override reader compiled out | PASS (details below) |
| 6 | Path strings | PASS (details below) |
| 7 | Expiry marker | PASS: UTF-16 `MCLA test-build admission` once in `mcla.exe` |

**Check 5, dev-override reader compiled out.** Strings expected to be absent:
- In `mcla.exe`: `MCLA dev override`, `MCLA dev overrides`, `conflicting keys for one cvar`, `line {} ignored`. All 0.
- In `MCLA-Launcher.exe`: `mcla-dev-overrides.txt`, `MCLA dev override`. Both 0.

Two related strings remain in `mcla.exe`, as they did in l70:
- The file name `mcla-dev-overrides.txt` appears once, in the F10 diagnostics file-name list next to `mcla-crash-report.txt` and `log-tail.txt`.
- The key names appear as constant table data used by the M10 defaults.

Strings expected to be present:
- `mcla.exe`: all 75 keys (data), `MCLA m10 default` (7), `save copy skipped` (2), `MCLA_KI035_FINISH_RACE` (2), `MCLA_ALBUM v=1`, `MCLA_CPU v=1`, and the l71–l81 probe strings.
- `MCLA-Launcher.exe`: `file(s) in sessions`, `store-seed.txt`.
- DLL strings as for l81.

The scan reported 0 failures (`l81v-scan.log`).

**Check 6, path strings.** No build, user, temp or patch paths. The remaining hits are:
- three-byte opcode patterns and binary tables;
- launcher UI text (`AppData`, `private/save-arc`);
- `since l81` / `l8x (KI-...)` inside cvar descriptions, which are descriptions, not paths.

## Artifacts: `private/l81v-volunteer-input`

| File | Bytes | SHA-256 |
|---|---|---|
| MCLA-Launcher.exe | 1451520 | 7C73E58A9A0FC7942EB90AC7F1400F38C590A8B27D9409E9F977DEEA7B9B0767 |
| mcla.exe | 53662720 | 83C8955A35B78B2D440F77EF6E31B28229A2FEE1B5CB92B0C06AB21F0A013128 |
| mcla_crash_handler.exe | 211968 | 941A71976C7B21B5152D41DE7CF50CF09F13D0415BC599D9110AB4888B6D0316 |
| mcla_diagnostic_image_helper.exe | 60928 | D0F7E67FA955B633752966D4B2A85BE0448CE6C062D61FFA3F32660951B672B8 |
| rexgpu-xenos.dll | 3384832 | C133CCA04A1A94137944A1B534EFF6D575739AD0229E57E1597BE36EE8B5FD6B |
| rexruntime.dll | 11548160 | FDBC21435727028AAF3FB4AB578B9B23EB10A7F35542D35874751F133B98F127 |
| TracyClient.dll | 246784 | 9694D41ABC68AD6DF73672386E9624898BA40701BC65734248C84E597CB3FA88 |

The three DLLs are the `l81-sdk-install/bin` files; the build-directory copies are byte-identical. The prepare script adds `mcla-test-build.ticket`.

## Prepare script (owner step)

`private/l81v-volunteer-prepare.ps1`, SHA-256 `43048E735A30C7F636B460D8296A37FA362DBE5CD31C386654F74C5907AF7BC4`. It is generated from `l70-volunteer-prepare.ps1` and the `l70-trim-leases-prepare.ps1` pattern, and parses with 0 errors in PowerShell 7.4.22 and 5.1.

The flow is the same V2 flow as l70:
1. `New-MclaTestBuildTicket.ps1 -KillSwitch -Capabilities unlocked,1080p,1440p`, with the grant line check.
2. Entries written with CreateNew and read back.
3. `New-MclaTestBuildLeases.ps1` signs the list.
4. Staging with the l12 CRT and the tester guide into `private\l81v-volunteer`.
5. `private\gate-host-build\mcla_build_lease_probe_win.exe --verify-leases`.
6. `Test-MclaVolunteerStaging.ps1 -RequireVolunteer`.
7. Receipt grant check.

**Change from l70: the allowance list holds l81v only.** This is the owner decision of 2026-11-05: older builds are withdrawn.
- `l81v-entries.json` holds only the l81v host, valid for 30 days. There are no carried entries.
- The script asserts exactly one entry.
- When the signed `l81v-active-builds.txt` is published, l70 and all older builds are refused at their next online check.
- No separate trim script is needed: the volunteer prepare itself produces the single-entry list.

The lease probe exists. A read-only `--verify-leases` of the published l70 trim list returned `SIGNED_LEASES_VERIFIED entries=1`. Probe SHA-256: `DDC4F90618B0D124F2EBDD536FA4B1B246FCA91A06A7F04504329AD341BFFC0D`.

**Entries dry run** (`-DryRunDirectory` on an empty temp directory, `l81v-dryrun.log`):
- Result: 1 entry, `83C8955A...3128`, with `valid_until 1796741047`. The real run sets the value at signing.
- A second dry run into the now non-empty directory was refused.
- The temp directory was removed. No entries, ticket, list or staging output exists in `private/`.

Run:

```
C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l81v-volunteer-prepare.ps1
```

## Owner checklist

`private/l81v-CHECKLIST.txt`, SHA-256 `EE2F3E031EE108CEE75BACC19B04E121285BEDA5CC28DB8F4006ADDA8B434F0C`.

1. Sign with the command above.
2. Short pre-share check with the internet **off**, because the build is refused online until the list is published:
   - PC: one race, one garage trip, one save, one cut-scene by ear, the reflection check and the album check.
   - Deck: one loop, one save, one cut-scene by ear.
   - In both, no `MCLA dev override` lines in the log.
3. Then the owner says "publish l81v" to the coordinator.

## Scripts and logs

All are in the patch's `build\` folder.
- Scripts: `make_src.py`, `l81v_host_build.ps1`, `l81v_host_tests.ps1`, `l81v_scan.py`, `l81v-volunteer-prepare.ps1`.
- Logs: `l81v-configure.log`, `l81v-build.log`, `l81v-tests.log`, `l81v-astgrep.log`, `l81v-scan.log`, `l81v-dryrun.log`, `l81v-dryrun-refuse.log`, `l81v-volunteer-input.sha256`.

## Owner signature and publication (2026-11-10)

- The owner signed l81v with `private/l81v-volunteer-prepare.ps1` (staging `private/l81v-volunteer`, mcla.exe 83C8955A..., receipt present) and said "sign, publish" ("підписав, викладай"); the offline pre-share check was not run separately (owner ran the l81 owner build on PC the same day; same sources).
- The primary verified the signed list (`SIGNED_LEASES_VERIFIED entries=1 issued_utc=1794292652`, l81v host valid until 1796884652) and published it as `active-builds.txt` on `DebugBundle` of `2FED/RE-MCLA` (commit `138b0ef`, replacing `31c806d`); the API blob and the raw file at the commit match the local file byte-for-byte (the branch raw URL is CDN-cached for a few minutes). l70 and all older builds are refused at their next online check.
