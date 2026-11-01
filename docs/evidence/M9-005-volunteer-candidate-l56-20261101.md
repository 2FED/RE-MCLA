# M9-005 volunteer candidate l56 (2026-11-01)

Build/packaging subagent report, for review by the primary agent. I built the l56
volunteer candidate from a clean clone of `main` after the SDK v0.20.0.6 pin, including a
fresh build of the SDK from that clone, with the M10-001/KI-047 options ON. The candidate
is ready for the owner's signing step.

Nothing was signed, staged, published or launched. No `mcla.exe`, `MCLA-Launcher.exe`,
`Launch-MCLA.exe` or game process was started. The only programs run were:

- `rexglue.exe --version` and `rexglue.exe codegen`;
- CMake/Ninja and SDK `ctest`;
- the 39 host unit test executables (the image client test starts the fake helper);
- ast-grep and llvm-objdump;
- Python for the byte scan and the pins file;
- PowerShell 7 to parse the scripts and to dry-run the entries merge.

No key, save or profile was opened, and no git write command was run in the project
repository. `private/l34-src` and the l38-l55 directories were only read. Spark was not
used: every step is a deterministic clone, build or test gate, or a byte comparison.

**l52 status:** l52 (`M9-005-volunteer-candidate-l52-20261031.md`, host `823BFBA4...4C81`)
was signed and published (list issued 1793453342: l48, l49, l52; `DebugBundle` `6c22440`).
l56 supersedes l52.

## What l56 adds over l52

All three come from SDK v0.20.0.6 (`483458d`) plus host options that turn them on at
graphics-plugin load (each one leaves a cvar alone if the user set it on the command line,
and logs one line either way).

- **KI-047 composite read-back fix, ON by default** (SDK `483458d`
  `d3d12_readback_sync_composite`, scope 0; host option `MCLA_READBACK_SYNC_COMPOSITE_DEFAULT`).
  Under the default `readback_resolve=some`, a resolve of the livery-composite family
  (colour, EDRAM base 0, k_8_8_8_8; sharing a live read-back range, sporadic after 8+
  frames, or a square power-of-two composite from 64 to 1024) is made synchronous like
  `full`: wait for the GPU, read this resolve's own slot, copy it now. This addresses the
  stale composite read-back found in the l54 catch (`decision=deferred read=prev`): the CPU
  built the car's atlas from the previous car's composite. Every other resolve keeps the
  `some` behaviour. Design and rules: [sync fix build](M10-001-ki047-sync-fix-build-20261101.md).
  **Not yet run in the game in any build** (l55 was prepared for the owner A/B but has no
  physical result).
- **Livery diagnostics v2/v3, ON by default** (SDK `83b093b` + `483458d`
  `d3d12_livery_diag2`; host option `MCLA_LIVERY_DIAG2_DEFAULT`): `MCLA_LIVERY_DIAG2 v=1`
  lines (read-back history, overlaps, waits, atlas loads, per-minute counts), paced budgets
  (burst 64 then 2 lines per second per kind; `composite` 128 then 8 per second), and an
  **F10 flight recorder** of the last 64 composite/resolve and 64 atlas lines
  (`MCLA_LIVERY_DIAG2_FLIGHT`), so a tester's F10 package on a broken livery carries the
  evidence even when the budget dropped it from the log.
- **Command-processor cost split, ON by default** (SDK `7892797` `d3d12_perf_split=1`; host
  option `MCLA_PERF_SPLIT_DEFAULT`): one `MCLA_PERF_SPLIT v=1` line per minute and one per
  F10, plus `MCLA_THREAD_CPU` per window and a `perf_split` block in `state.json`; makes
  tester fps reports comparable with the M10-001 owner runs.
- **Root signature after a pipeline wait** (SDK `83b093b`, behaviour fix, always on): a
  draw that waited for a pending pipeline binds that pipeline's root signature.
- `MCLA_READBACK_SYNC v=1 kind=minute` and `kind=mark` (F10) lines count the matched and
  forced resolves and the forced wait time.
- Everything in l52 (always-on bounded pipeline wait, KI-022 wheel resync, KI-050/051
  Steam Deck fixes, D3D12 everywhere) is carried unchanged.

**Measured overhead (owner runs of the internal builds, not l56):**

- Perf split mode 1: 0.1-0.8 % of CP time in the micro-benchmark
  ([instrumentation build](M10-001-instrumentation-build-20261031.md), section 4).
- Diag v2 on (l54) against l53 at 1440p, cold and warm: **no fps change above the
  run-to-run spread** (GPU-bound, 37-39 fps); at most about 0.3-0.5 ms/frame of CP time
  (about 2 % of CP busy) in `copy` and `p_tex`
  ([720p and KI-047](M10-001-720p-and-ki047-20261101.md), section 6.1). Not yet measured at
  720p, where the CP is close to the wall.
- Read-back sync fix: **unmeasured**. Expected a handful of forced resolves (about 6-10)
  per car spawn and a short hitch of up to about one GPU frame each, nothing between spawns.
  A steady `forced` rate of tens per second, or fps clearly below l52, means the rules match
  routine resolves.

**Log size:** larger than l52. The l54 1440p session (diag v2 with the older per-minute
budgets, perf split on) wrote 3,367 lines in about 6 minutes. Diag v3 replaces the
per-minute budgets with paced ones, so the bound is now per second: per kind 64 lines then
2 per second, composite 128 then 8 per second; in practice the budgets are exhausted at
world load and the log stays quiet between car spawns. Each F10 adds the flight dump (up to
128 lines plus begin/end). Testers' F10 packages and session logs grow accordingly.

## Clone identity

| Item | Value |
| --- | --- |
| Project | Branch `main` cloned from the main checkout into `private/l56-src`. Clone HEAD = checkout HEAD = `85128b6e10b3bd87c1370bb318025dbb5f693d88` (task(M10-001): pin ReXGlue SDK v0.20.0.6 (483458d): perf split, livery diag v2/v3, composite read-back sync). |
| SDK pin | Gitlink `483458d2885419234a0729698f29a8850a052b55`, origin `https://github.com/2FED/rexglue-sdk.git`; `describe --exact-match` gives `v0.20.0.6` (annotated, tag object `22bfdc0c`). |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b`, origin `https://github.com/2FED/SDL.git`. |
| Submodules | 26 recursive, none `-`/`+`/`U`; the 25 nested SHAs equal l52's. 15 libmspack links materialized in the clone. |
| SDK install | `private/l56-sdk-install`, built from the clone (`l56-sdk-b`, output `l56-sdk-o`) with the l52 flags. `rexglue --version` and `rexglueConfigVersion` = `0.20.0.6`. SDK ctest 1952/1952, 4 known skips. |
| Host build dir | `private/l56-build`. |
| Script | `private/l56-clean-clone.ps1`, generated from `l52-clean-clone.ps1` by `private/l56-work/mk_clone.py`. Full results in [M9-001 clean clone v0.20.0.6](M9-001-clean-clone-v0.20.0.6-20261101.md). |

## Build and test results

| # | Check | Result |
| --- | --- | --- |
| 1 | SDK build, version, ctest | **PASS.** 1436 actions; `0.20.0.6` from `rexglue --version` and the config file; ctest 1952/1952, 4 known BitStream skips. |
| 2 | Codegen | **PASS.** 67 files, 133,916,828 bytes; every SHA-256 equals the main checkout's `generated/default`. |
| 3 | Host build | **PASS.** Volunteer options (expiry ON, raw timer ON, image/diagnostic tests ON, WGC OFF, prefix maps, `/pdbaltpath`, `REXSDK_VERSION=0.20.0.6`) plus `MCLA_PERF_SPLIT_DEFAULT=ON`, `MCLA_LIVERY_DIAG2_DEFAULT=ON`, `MCLA_READBACK_SYNC_COMPOSITE_DEFAULT=ON` (CMakeCache confirms all three). 0 compiler warnings, 0 errors. The CMake defaults in `CMakeLists.txt` stay OFF. |
| 4 | Unit tests | **PASS, first run.** 39/39 executables return 0 (`private/l56-host-tests.log`): l52's 36 + perf split, livery diag and read-back sync (38 checks) tests; all summaries equal the l55 run. |
| 5 | ast-grep 0.50.0 in the clone | **PASS.** `scan` exit 0; `test` 3/3 (`private/l56-astgrep.log`). |
| 6 | Default-on strings | **PASS.** `mcla.exe`: `MCLA perf split: build default {}=1`, `MCLA livery diag2: build default {}=true`, `MCLA readback sync: fix ON, build default {}=true`, `MCLA pipeline wait: always on`; imports `MarkPerfSplit` and `RequestWheelEffectResync`. `rexgpu-xenos.dll`: `MCLA_READBACK_SYNC v=1 kind={}` (start/stop), `kind=mark`, `kind=minute`, `MCLA_PERF_SPLIT v=1`, `MCLA_LIVERY_DIAG2 v=1`, `MCLA_LIVERY_DIAG2_FLIGHT`. |
| 7 | Path strings | **PASS.** Seven input files, ASCII and UTF-16LE, case-insensitive (`private/l56-pathscan.py`, log `private/l56-pathscan.log`). Only two machine-code hits in `mcla.exe` `.text`: `l54` at `0xF11644` and `L52` at `0xE231FC`, both immediates of recompiled guest addresses (`0x8234356C`, `0x8232354C`), the same as in l55; not paths. |
| 8 | Expiry marker | **PASS.** UTF-16 `MCLA test-build admission` once in `mcla.exe`. |

## Artifacts (private/l56-build), copied to private/l56-vol-input

| File | Bytes | SHA-256 |
| --- | --- | --- |
| `mcla.exe` | 53194752 | `BAED27A780C10173A78E87D46B7531A717C922DCB60C60FDC41B1E10730563D1` |
| `MCLA-Launcher.exe` | 1373696 | `B7F3FB3614B2380763D272DBAA542F8CD54DCF1493F8E92408DDE35C90E5CC63` |
| `mcla_crash_handler.exe` | 211968 | `67DBB64FAC3250647FFC7B92E1022654FA789A0D8A51EC1E2B2DE97CBCE43322` |
| `mcla_diagnostic_image_helper.exe` | 60928 | `E702980B065E91CF02E3754086943ACD4289DDCBFEAD28766CB6D15980C45664` |
| `rexruntime.dll` | 10995712 | `A44703D8222A1A99724972111660F1F61BAB197F05EFA57529EB0A65350780DA` |
| `rexgpu-xenos.dll` | 3136000 | `7BC8C63605AD488BE824B72F96635A0E01CDAF654F4B7942128BCE89E40080DE` |
| `TracyClient.dll` | 246784 | `C7CFD37AD9EC151D73A2E57F7D744E33A359A03642B513F994082195EF45D333` |

`private/l56-vol-input` holds exactly these seven files; the prepare script adds
`mcla-test-build.ticket`. The three DLLs are byte-identical to `private/l56-sdk-install/bin`.
Pins: `private/l56-pins.json`; hash list `private/l56-work/l56-vol-input.sha256`.
`Launch-MCLA.exe` (not packaged): `64F7253FABA6BCBBAD8291F4CE42B9F4EA6B9337F679C9800B547DD6C5B4B34F`.

## Prepare script (owner step)

`private/l56-volunteer-prepare.ps1` (SHA-256
`864A538F7557A0B6FB40C459867C7E259A72C988496DDFAAE039EB3C99FEF40E`) was generated from
`l52-volunteer-prepare.ps1` by `private/l56-work/mk_prepare.py` with `private/l56-pins.json`.
Code changes against l52 (diff of the script bodies): `$Repo` is now the main checkout
`C:\BDU\MCLA-Recomp` (scripts and `docs/volunteer-tester-guide.txt` from `main`); l56
paths for the input, staging, entries and list files; the seven new pins; carried entries
from `l52-entries.json` (was `l49-trim-entries.json`); updated messages. The V2 flow is
otherwise identical (`New-MclaTestBuildTicket.ps1 -KillSwitch -Capabilities
unlocked,1080p,1440p`, grant line `capabilities 1080p,1440p,unlocked`, CreateNew entries
with read-back, `New-MclaTestBuildLeases.ps1`, staging with the l12 CRT and the tester
guide, lease probe, `Test-MclaVolunteerStaging.ps1 -RequireVolunteer`, receipt grant
check). PowerShell 7.4.22 parse: 0 errors for the prepare, clone and test scripts
(`private/l56-parsecheck.log`). Run:

```
C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l56-volunteer-prepare.ps1
```

**Entries-merge dry run** (`-DryRunDirectory` on an empty temp directory, log
`private/l56-dryrun.log`): **4 entries**: l48 `6FEC02D0...` (valid_until 1795957688),
l49 `0A46AE3E...` (1796023588), l52 `823BFBA4...` (1796045342), all unchanged from
`l52-entries.json`, then the l56 host `BAED27A7...` with valid_until 1796136929
(2026-12-01 16:55 Kyiv at dry-run time; the real run sets it at signing). A second dry run
into the now non-empty directory was refused ("dry-run directory is not empty",
`private/l56-dryrun-refuse.log`). The temp directory was removed. No `l56-entries.json`,
ticket, list or staging output exists.

## Owner and tester checks

1. **Prepare.** Run `l56-volunteer-prepare.ps1` in PowerShell 7. Expect `DONE`,
   `SIGNED_LEASES_VERIFIED entries=4`, an audit with 0 findings and the receipt grant
   `1080p,1440p,unlocked`. Publishing `l56-active-builds.txt` needs the owner's explicit yes.
2. **Arm check in the first log:** `MCLA readback sync: fix ON, build default
   d3d12_readback_sync_composite=true` and `MCLA_READBACK_SYNC v=1 kind=start`,
   `MCLA livery diag2: build default d3d12_livery_diag2=true`, `MCLA perf split: build
   default d3d12_perf_split=1`. **If the owner's l55 A/B left
   `%LocalAppData%\MCLA-R-Launcher-v1\mcla-dev-overrides.txt` in place** (after
   `l55-fix-off.cmd`), l56 reads it too and logs `override file ... found: false (applied)`
   and `fix OFF`: run `private\l55-fix-on.cmd` (deletes the file) before the l56 smoke.
3. **Police livery:** several police encounters per session (1440p and 720p); the police
   car shows its own livery. F10 on any wrong livery, and once on a correct police car.
4. **Normal fps versus l52:** same route and resolution; fps within the usual spread, no
   new long hitches between car spawns; short hitches at spawns are acceptable. Compare
   `cp_busy_ms`, `readback_wait_ms`, `fence_wait_ms` (`MCLA_PERF_SPLIT`) and
   `wait_ms`/`max_wait_ms`/`forced` (`MCLA_READBACK_SYNC kind=minute`).
5. **F10 packages carry the new lines:** the package's runtime log contains
   `MCLA_PERF_SPLIT v=1` (including the `reason=f10` mark line at the next frame close) and
   `MCLA_READBACK_SYNC v=1 kind=mark`, plus the `MCLA_LIVERY_DIAG2 v=1 kind=flight_begin`
   ... `flight_end` block; `state.json` has a `perf_split` block.
6. Windows smoke as for l52 (launcher settings 1080p/1440p/Unlocked, no 2160p; kill switch
   offline/online; wheel and Steam Input unchanged), one tester smoke on a second machine,
   the Steam Deck when convenient (vkd3d: the forced waits are a full queue drain).

**A/B off switch: internal use only.** Because l56 is built with
`MCLA_READBACK_SYNC_COMPOSITE_DEFAULT=ON`, it reads the one-line per-user file
`%LocalAppData%\MCLA-R-Launcher-v1\mcla-dev-overrides.txt`
(`mcla_readback_sync_composite=false` turns the fix off at the next game start) exactly
like l55; `private\l55-fix-off.cmd` / `private\l55-fix-on.cmd` work unchanged for l56. They
are owner tools in `private/`, not part of the staged package, not in the tester guide and
not in the changelog; testers are not told about the file. Every turn-off is visible in the
log (`override file ... (applied)`, `fix OFF`).

## Limits and risks

- Nothing was signed, staged, published or launched; no in-game, owner or Steam Deck check
  is claimed for l56. **The read-back sync fix has no physical result in any build**; l56
  ships it to volunteers before the l55 owner A/B, by owner decision.
- Fix cost unmeasured: each forced resolve drains the GPU queue mid-frame. If the scope-0
  rules match routine resolves on some scene or GPU, fps drops (the evidence for that is
  the `forced` count). Scope 1 is not used.
- Fix coverage: scope 0 is based on one l54 session; a composite outside the family is not
  covered, and the texture-cache side (atlas uploaded before the CPU rebuilt it) is not
  addressed. The F10 flight recorder is the diagnostic for both.
- Diagnostics overhead at 720p and on the Deck is not measured; log and F10 package size
  grow (see above).
- A stale owner override file (item 2 above) silently turns the fix off on the owner's PC;
  a tester could do the same only by creating that file by hand.
- Two benign guest-address byte matches in `mcla.exe` machine code (check 7).
- The clone came from the local committed `main`; the SDK and all submodules from GitHub.
  Output is not bit-identical across checkout paths.

## Owner signature and publication (2026-11-01)

- The owner ran `private/l56-volunteer-prepare.ps1` and staged `private/l56-volunteer`; the signed `l56-active-builds.txt` (issued 1793549989) lists 4 entries: l49 `0A46AE3E...`, l48 `6FEC02D0...`, l52 `823BFBA4...`, l56 `BAED27A7...` (expiry 1796141989).
- With the owner's explicit approval ("publish l56") the primary agent published the list to `DebugBundle` of `2FED/RE-MCLA` (commit `0a96366`, replacing `6c22440`); the raw URL and the contents API both returned bytes identical to the signed file.
- Sharing the staged folder with testers and the Steam Deck is the owner's step. Nothing in l56 has run in the game yet.
