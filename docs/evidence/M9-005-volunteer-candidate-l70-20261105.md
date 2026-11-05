# M9-005 volunteer candidate l70 (2026-11-05)

Build/packaging subagent report, for review by the primary agent. I built the l70 volunteer
candidate from a clean clone of `main` after the KI-035 fix, against the SDK install of the l67
clean clone (same pin, `v0.20.0.8`, reused read-only), with the l67 volunteer options
(`MCLA_M10_SPEEDUP_DEFAULTS=ON`, `MCLA_PREWARM_SEED_DEFAULT=ON`, `MCLA_DEV_OVERRIDES` OFF). The
candidate is ready for the owner's pre-share check and signing step.

Nothing was signed, staged, published or launched. No `mcla.exe`, `MCLA-Launcher.exe`,
`Launch-MCLA.exe` or game process was started. The only programs run were:

- `rexglue.exe --version` and `rexglue.exe codegen`;
- CMake/Ninja and the linker (including one scratch relink of `mcla.exe` with `/MAP`, never run);
- the 49 host unit test executables (the image client test starts the fake helper);
- ast-grep;
- Python for the byte scans, section mapping, import/export tables, the hook check and the pins;
- PowerShell 7 to parse the scripts and to dry-run the entries merge.

No key, save, profile or LocalAppData file was opened, the HKCU CMake package registry was not
written, and no git write command was run in the project repository. `private/l67v-sdk-install`
was only read (1201-file hash manifest identical before and after). Spark was not used: every
step is a deterministic clone, build or test gate, or a byte comparison.

**l67 status:** l67 (`M9-005-volunteer-candidate-l67-20261103.md`, host `1B464248...B64C`) was
signed, published (list issued 1793773100, `DebugBundle` `2e6c503`) and passed the owner's
pre-share check (2026-11-05). l70 supersedes l67; the tester changelog is written against l67.

## What l70 adds over l67

- **KI-035 garage freeze fixed** (`5f476f1`, M10-008). Root cause: with Unlocked timing the
  garage/cinematic zoom-in camera can finish one frame early (dt look-ahead), and the warper's
  camera-finished callback `sub_8221B8C8` then arrives while the warper is still in state 20,
  so the state never exits. A host hook on that callback performs the state-20 handler's exit
  first and then runs the original, which finalises the warp as on a healthy trip
  (`mcla_warp_finish_fix`, default ON; log `MCLA_KI035_FINISH_RACE v=1` /
  `MCLA_KI035_FINISH_RACE_DONE v=1` when it fires). **Owner-confirmed on l69** (owner build):
  five garage trips in a 55-minute PC session, the hook fired on 3 of them, each completed
  (state 29), no freeze ([M10-l69-pc-session](M10-l69-pc-session-20261105.md),
  [fix](M10-ki035-finish-race-fix-20261105.md)).
- **Video-memory soft cap on small cards** (KI-048 mitigation, `47487b0`, M10-007). At draw
  resolution scale >= 2 (1080p and 1440p) on a local video-memory budget under 10 GB, the
  texture cache soft limit and the monitor floor start at min(budget / 2, 2048 MB); the hard
  limit is unchanged; nothing changes under Wine/Proton or when the user set the limits. In the
  owner's l68 A/B on the 8 GB card the cap kept the process **in budget for the whole
  session** (16.6 minutes, `near_budget_steps=0` on all census lines), while the cap-off arm
  ran into OS budget drops ([A/B](M10-l68-pc-ab-ki035-root-cause-20261105.md)); the l69
  55-minute session ran with the cap as well. Log: `MCLA graphics: texture cache soft cap ...`.
- **`MCLA_VRAM v=1` census**: one line per minute, at each accepted F10 and at exit (local and
  non-local usage and budget, texture limits, near-budget steps, GPU clocks/temperature where
  available); `mcla_vram_census` default on, log only.
- **Window focus logging**: `MCLA_WINDOW_EVENT v=1 event=... focused=... fullscreen=...`, so a
  30 fps stretch can be told apart from the unfocused menu cap (`dc886a3`).
- **Warp probe off by default** (`mcla_warp_probe=false`, `mcla_warp_watchdog_seconds=0`): the
  KI-035 diagnostic probe and watchdog of the l68 owner build are compiled in but inactive; the
  warp camera fields are in the F10 state block.
- Everything in l67 (Proton-only speed-up defaults, KI-057 first-run fullscreen on Deck,
  KI-064 launcher sessions fix, KI-065 F10/save guard, SDK v0.20.0.8 content logs, and all of
  l65) is carried; the three SDK DLLs are byte-identical to l67.

## Clone identity

| Item | Value |
| --- | --- |
| Project | Branch `main` cloned from the main checkout into `private/l70-src`. Clone HEAD = checkout HEAD at clone time = `4f69018f6c1e43c1e0f6f25a93d1a294681cc3de` (docs: KI-066 third case). The requested base `dba3e58` differs from it only in `docs/known-issues.md`; all build inputs are those of `dba3e58`. |
| SDK pin | Gitlink `c63895c8ed53748d04de91403c467ece2e8f6fa8`, origin `https://github.com/2FED/rexglue-sdk.git`; exact tag `v0.20.0.8`. |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b`, origin `https://github.com/2FED/SDL.git`. |
| Submodules | 26 recursive, none `-`/`+`/`U`; identical to the l67 clone. 15 libmspack links materialized. |
| SDK install | `private/l67v-sdk-install`, reused read-only: same tag, built from the l67 clean clone (ctest 2014/2014); `rexglue --version` and `rexglueConfigVersion` = `0.20.0.8`; binaries equal to the l67 SDK report. |
| Host build dir | `private/l70-build`. |
| Script | `private/l70-clean-clone.ps1`, generated from `l67-clean-clone.ps1` by `private/l70-work/mk_clone.py` (sdk phase disabled). Full results in [M9-001 clean clone l70](M9-001-clean-clone-l70-20261105.md). |

## Build and test results

| # | Check | Result |
| --- | --- | --- |
| 1 | SDK | **PASS (reused).** `0.20.0.8` from `rexglue --version` and the config file; DLL and tool hashes equal the l67 SDK report; install unchanged by the l70 work. |
| 2 | Codegen | **PASS.** 67 files, 133,916,828 bytes; every SHA-256 equals the main checkout's `generated/default`. |
| 3 | Host build | **PASS.** l67 volunteer options (expiry ON, raw timer ON, image/diagnostic tests ON, WGC OFF, prefix maps, `/pdbaltpath`, `REXSDK_VERSION=0.20.0.8`, perf split ON, livery diag2 ON, readback sync composite ON, `MCLA_M10_SPEEDUP_DEFAULTS=ON`, `MCLA_PREWARM_SEED_DEFAULT=ON`, `MCLA_DEV_OVERRIDES=OFF`; CMakeCache and `build.ninja` confirm). 0 compiler warnings, 0 errors. |
| 4 | Unit tests | **PASS, first run.** 49/49 executables return 0 (`private/l70-work/l70-host-tests.log`), including `mcla_warp_probe_test` (5936 checks, incl. the generated-code mirror), `mcla_vram_census_win_test` (572), `mcla_m10_defaults_test` (2477), `mcla_launcher_preferences_test` (2095), `mcla_save_copy_guard_test` (43). |
| 5 | ast-grep 0.50.0 in the clone | **PASS.** `scan` exit 0; `test` 3/3. |
| 6 | Strings and tables | **PASS.** `mcla.exe`: `MCLA_KI035_FINISH_RACE` (2), `mcla_warp_finish_fix` (2), `MCLA_VRAM v=1`, `texture cache soft cap`, `MCLA m10 default:`, `save copy skipped` (2), `MCLA_WINDOW_EVENT v=1`, the import of `rex_xam_content_transactions_active`; no `MCLA dev overrides` reader strings. `MCLA-Launcher.exe`: `file(s) in sessions`, `store-seed.txt`; no `mcla-dev-overrides.txt`. |
| 7 | KI-035 hook linked | **PASS.** Scratch relink with `/MAP` byte-identical in all seven sections; `PPCFuncMappings` entry for guest `0x8221B8C8` in the built `mcla.exe` = `0x1400A3A30` = `sub_8221B8C8` from `mcla_ki026_probe.cpp.obj` (the override); `__imp__sub_8221B8C8` is at `0x140769C10` (`mcla_recomp.8.cpp.obj`). |
| 8 | Path strings | **PASS.** Seven input files, ASCII and UTF-16LE, case-insensitive. Only `.text` machine-code hits (nine guest-address immediates in `mcla.exe`, two `movdqu` encodings and one immediate in `MCLA-Launcher.exe`, one `call rel32` in `rexruntime.dll`) and launcher UI text (`AppData`, `private/save-arc`), as in l67/l69; no paths (offsets in the M9-001 report). |
| 9 | Expiry marker | **PASS.** UTF-16 `MCLA test-build admission` once in `mcla.exe`. |

## Artifacts (private/l70-build), copied to private/l70-vol-input

| File | Bytes | SHA-256 |
| --- | --- | --- |
| `mcla.exe` | 53396480 | `146FBBB5FB9ADC4E8BF29138405D2FCC9480EC512A306E7B70116738ED4B4A25` |
| `MCLA-Launcher.exe` | 1451520 | `23414C88B13269E3A1B38AE17078E0B0D77D33F9721361FF409C44F6BCC90200` |
| `mcla_crash_handler.exe` | 211968 | `B33C036D6AF935D0C83767FB606F76B9555159C5220CD22BD1E3466896E0143D` |
| `mcla_diagnostic_image_helper.exe` | 60928 | `D30563B47AEEB264E48E7767DE03E697DDF4C3CCB9CF80DDF23E07F46C4E7BB2` |
| `rexruntime.dll` | 11064320 | `7695301AE6AC8296B2B38D45FDF359317AEA1D32054236791F38E1557748892B` |
| `rexgpu-xenos.dll` | 3210240 | `1D081EB31000DA15855F9E2884875DC5CAB0B1E5F828AF74A7A2BACC7FABB862` |
| `TracyClient.dll` | 246784 | `8BE2138700AB787C46304D29EA0B230FDA4920906076C69075817E3CF9540A1A` |

`private/l70-vol-input` holds exactly these seven files; the prepare script adds
`mcla-test-build.ticket`. The three DLLs are byte-identical to `private/l67v-sdk-install/bin`
(and to l67). Pins: `private/l70-pins.json` (SHA-256
`EFF07C4DC06D9286DFF4EAFD266E8933B6852510AECE6D92EC4D45A1BCE07F58`); hash list
`private/l70-work/l70-vol-input.sha256`. `Launch-MCLA.exe` (not packaged):
`FABAB0310C6E25773F7526C52802D9FDFDE4766D7A0236F36B4BF788DEB4700F`.

## Prepare script (owner step)

`private/l70-volunteer-prepare.ps1` (SHA-256
`F82A877D6C2343EE57A574AF372D1B43E5998C4893D191B94EC4ED73EDB4F6D4`) was generated from
`l67-volunteer-prepare.ps1` by `private/l70-work/mk_prepare.py` with `private/l70-pins.json`.
Code changes against l67 (diff of the script bodies): l70 paths for the input, staging, entries
and list files; the four new host pins (the three DLL pins are unchanged, asserted by the
generator); carried entries from `l67-entries.json` (was `l65-entries.json`); updated messages.
The V2 flow is otherwise identical (`New-MclaTestBuildTicket.ps1 -KillSwitch -Capabilities
unlocked,1080p,1440p`, grant line `capabilities 1080p,1440p,unlocked`, CreateNew entries with
read-back, `New-MclaTestBuildLeases.ps1`, staging with the l12 CRT and the tester guide, lease
probe, `Test-MclaVolunteerStaging.ps1 -RequireVolunteer`, receipt grant check). PowerShell
7.4.22 parse: 0 errors for the prepare, clone and work scripts
(`private/l70-work/l70-parsecheck.log`). Run:

```
C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l70-volunteer-prepare.ps1
```

**Entries-merge dry run** (`-DryRunDirectory` on an empty temp directory, log
`private/l70-work/l70-dryrun.log`): **7 entries**: l48 `6FEC02D0...` (valid_until 1795957688),
l49 `0A46AE3E...` (1796023588), l52 `823BFBA4...` (1796045342), l56 `BAED27A7...` (1796141989),
l65 `422A20A6...` (1796278298), l67 `1B464248...` (1796365100), all unchanged from
`l67-entries.json`, then the l70 host `146FBBB5...` with valid_until 1796488786 (2026-12-05
18:39 Kyiv at dry-run time; the real run sets it at signing). A second dry run into the now
non-empty directory was refused ("dry-run directory is not empty", `l70-dryrun-refuse.log`).
The temp directory was removed. No `l70-entries.json`, ticket, list or staging output exists.
Note: l48 expires 2026-11-29 and l49 2026-11-30.

## Owner checks before sharing

1. **PC: one session at 1080p (Unlocked or the usual mode) with two or three garage trips
   after races** on the staged l70.
   - No garage freeze. In `mcla.log`, look for `MCLA_KI035_FINISH_RACE v=1` (with
     `action=finish-state20`) followed by `MCLA_KI035_FINISH_RACE_DONE v=1 state=29 ...` when
     the hook fires; on a trip where it does not fire the warp exits naturally. If the garage
     ever freezes, F10 there.
   - Soft cap active on the 8 GB card: `MCLA graphics: texture cache soft cap ...` at startup;
     `MCLA_VRAM v=1` lines each minute with `local_mb` under `local_budget_mb` and
     `near_budget_steps=0` (or few); no stutter series.
   - Log on Windows as l67: `MCLA m10 defaults (startup): platform=windows applied=1 ...` and
     `(graphics plugin): platform=windows applied=5 failed=0`; `MCLA_WINDOW_EVENT` lines on
     focus changes; no `MCLA dev override` lines.
2. **Deck: one loop** (stock 30, 1280x720) through Proton: `platform=wine`, `applied=2` and
   `applied=6`, both Proton-only values active as in l67; the loop feels as on l67; the soft
   cap does not engage under Wine (no `texture cache soft cap` change).
3. **Prepare.** Run `l70-volunteer-prepare.ps1` in PowerShell 7. Expect `DONE`,
   `SIGNED_LEASES_VERIFIED entries=7`, an audit with 0 findings and the receipt grant
   `1080p,1440p,unlocked`. Publishing `l70-active-builds.txt` needs the owner's explicit yes.
   (If the owner prefers to run checks 1-2 on the staged build, signing comes first, as for
   l65/l67.)

## Tester checks

1. Garage after races (several trips per session): no freeze. If it ever freezes, F10 on the
   frozen screen and report.
2. Windows with an 8 GB card at 1080p or 1440p: long sessions (30 minutes and more) stay smooth;
   report the card, resolution and when any slowdown starts.
3. Steam Deck: fps and smoothness as on l67 at the same place.
4. F10 at any time, also during a save: no save error; F10 packages arrive.
5. Windows smoke as for l67 (launcher 1080p/1440p/Unlocked, no 2160p; kill switch
   offline/online; wheel and Steam Input unchanged; police livery correct).

## Limits and risks

- Nothing was signed, staged, published or launched; no in-game, owner or Deck check is
  claimed for l70. The KI-035 hook and the soft cap were owner-tested only on the l68/l69 owner
  builds (dev overrides ON, warp probe on); the volunteer build has the probe off and no dev
  overrides, so the combination is new.
- KI-035: the hook covers the diagnosed state-20 race; three fires in one session is the
  whole runtime evidence. Other freeze paths, if any, would still freeze (F10 then).
- KI-048: the soft cap is a mitigation, measured on one 8 GB card (16.6-minute A/B plus the
  55-minute l69 session). Cards with 10 GB or more, 720p (scale 1) and Wine/Proton are
  unchanged. A smaller texture cache can raise texture re-uploads; not seen in the A/B.
- KI-067 (race-start lag, uniform host CPU slowdown) and KI-054 (varying time steps) are not
  addressed by l70.
- The `mcla-dev-overrides.txt` single-line read-back sync switch (l55 behaviour) is still
  compiled in, as in l65/l67; testers are not told about it.
- The SDK was not rebuilt (reused l67 clean-clone install, same tag, unchanged).
- Benign machine-code and UI-text matches in the path scan (check 8).
- The clone came from the local committed `main`; the SDK and all submodules from GitHub.
  Output is not bit-identical across checkout paths.

## Owner signature (2026-11-05)

- The owner ran `private/l70-volunteer-prepare.ps1`: V2 ticket (host `146FBBB5...`, capabilities 1080p,1440p,unlocked, created 1793897204), 7 entries, signed `l70-active-builds.txt` (issued 1793897204), staging `private/l70-volunteer`. The script's step 5 failed because the lease probe binary had been removed by the storage cleanup; the primary rebuilt the probe and ran steps 5-6: `SIGNED_LEASES_VERIFIED entries=7`, volunteer audit 195 checks / 0 findings, receipt capabilities correct. Publication awaits the owner's "publish l70".
