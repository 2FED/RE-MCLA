# M9-005 volunteer candidate l67 (2026-11-03)

Build/packaging subagent report, for review by the primary agent. I built the l67 volunteer
candidate from a clean clone of `main` after the SDK v0.20.0.8 pin, including a fresh build of
the SDK from that clone (D3D12 only, the volunteer configuration), with the l65 volunteer
options (`MCLA_M10_SPEEDUP_DEFAULTS=ON`, `MCLA_PREWARM_SEED_DEFAULT=ON`, `MCLA_DEV_OVERRIDES`
OFF). The candidate is ready for the owner's pre-share sessions and signing step.

Nothing was signed, staged, published or launched. No `mcla.exe`, `MCLA-Launcher.exe`,
`Launch-MCLA.exe` or game process was started. The only programs run were:

- `rexglue.exe --version` and `rexglue.exe codegen`;
- CMake/Ninja and SDK `ctest`;
- the 47 host unit test executables (the image client test starts the fake helper);
- ast-grep;
- Python for the byte scans, section mapping, import/export tables and the pins file;
- PowerShell 7 to parse the scripts and to dry-run the entries merge.

No key, save, profile or LocalAppData file was opened, the HKCU CMake package registry was not
written, and no git write command was run in the project repository. `private/l34-src`, the
l65 and the l67 dev directories were only read. Spark was not used: every step is a
deterministic clone, build or test gate, or a byte comparison.

**l65 status:** l65 (`M9-005-volunteer-candidate-l65-20261102.md`, host `422A20A6...E499`) was
signed, published (list issued 1793686298, `DebugBundle` `d7a6d93`) and passed the owner's
pre-share check. l67 supersedes l65; the tester changelog is written against l65.

## What l67 adds over l65

- **Proton-only speed-up defaults** (`MCLA_M10_SPEEDUP_DEFAULTS`, host `88fdc01`): under
  Wine/Proton the defaults also write `shared_memory_upload_ahead_pages=16` (graphics plugin)
  and `memory_watch_disarm_bridge_safe_gaps=true` (startup). These were the two largest Deck
  gains in the l64 Deck A/B (`M10-l64-deck-ab-20261103.md`: upload-ahead CP -8.3 ms/frame,
  bridging -3.3 ms/frame). The l64 "all" arm against base: **+15% fps while driving**
  (12.73 -> 14.67 fps over both laps of both runs) and far fewer long frames (frames over
  100 ms per driving minute 26-94 -> 1-7; driving p99 100-234 -> 84-100 ms). The l64 "all" arm
  also held the generic speed-ups that l65 already ships, so the Deck gain of l67 over l65 is
  the Proton-only part; it has not been measured separately. On Windows both stay off (net
  negative in l63) and are logged as `... left at <value> (wine_only)`; the summary lines start
  with `platform=wine|windows` and count `wine_only=`.
- **KI-057 (Steam Deck runs in a 1280x720 window):** a first run under Wine/Proton starts with
  Fullscreen ticked; saved preferences and real Windows are unchanged (`267ae4b`).
- **KI-064 (stray file in `sessions` blocks CHECK SETUP):** the launcher's session-history loop
  skips plain files and adds ` (ignored N file(s) in sessions)`; failures name the entry
  (`88fdc01`).
- **KI-065 ("storage device was removed" after an F10 during a save, Deck):** the F10 export
  waits up to 2 s for an open save transaction (SDK indicator
  `rex_xam_content_transactions_active`, plus the `.rexpending`/`.rexbackup` markers), then
  copies saves with full sharing, skips transaction scratch, retries a sharing/lock violation
  once after 100 ms, and brackets the copy with `MCLA diagnostics: save copy start ... / save
  copy end ...`; if still busy it skips the copy and writes `save-copy-skipped.txt` (`c66bcf7`,
  `267ae4b`). SDK v0.20.0.8 (`c63895c`) adds paced `[content]` WARN logs for failing
  create/close/flush/delete/transaction/file/device operations (save names only, no host paths)
  and the indicator export.
- Hot blocks (SDK watch step 3) stay in the never-written list; Vulkan stays out of the
  volunteer SDK build.
- Everything in l65 (targeted KI-047 wait, read-back elision, CP event wake, replay batched
  wake, native refcount, pre-warm, KI-055/058 fixes, perf split, livery diag) is carried.

## Clone identity

| Item | Value |
| --- | --- |
| Project | Branch `main` cloned from the main checkout into `private/l67-src`. Clone HEAD = checkout HEAD at clone time = `e551fb6e197eb3775295185df2933c3d94f28517` (task(M10): pin ReXGlue SDK v0.20.0.8 (c63895c): KI-065 content logs and transaction indicator). The checkout later moved to `1ced4c4` (docs only). |
| SDK pin | Gitlink `c63895c8ed53748d04de91403c467ece2e8f6fa8`, origin `https://github.com/2FED/rexglue-sdk.git`; `describe --exact-match` gives `v0.20.0.8` (annotated, tag object `562f19cf`). |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b`, origin `https://github.com/2FED/SDL.git`. |
| Submodules | 26 recursive, none `-`/`+`/`U`; the 25 nested SHAs equal l65's. 15 libmspack links materialized in the clone. |
| SDK install | `private/l67v-sdk-install`, built from the clone (`l67v-sdk-b`, output `l67v-sdk-o`) with the l65 flags (D3D12 only). `rexglue --version` and `rexglueConfigVersion` = `0.20.0.8`. SDK ctest 2014/2014 on the first run, 4 known skips. |
| Host build dir | `private/l67-build`. |
| Script | `private/l67-clean-clone.ps1`, generated from `l65-clean-clone.ps1` by `private/l67v-work/mk_clone.py`. Full results in [M9-001 clean clone v0.20.0.8](M9-001-clean-clone-v0.20.0.8-20261103.md). |

## Build and test results

| # | Check | Result |
| --- | --- | --- |
| 1 | SDK build, version, ctest | **PASS.** 1444 actions; `0.20.0.8` from `rexglue --version` and the config file; ctest 2014/2014 first run (no manifest race this time), 4 known BitStream skips. |
| 2 | Codegen | **PASS.** 67 files, 133,916,828 bytes; every SHA-256 equals the main checkout's `generated/default`. |
| 3 | Host build | **PASS.** l65 volunteer options (expiry ON, raw timer ON, image/diagnostic tests ON, WGC OFF, prefix maps, `/pdbaltpath`, `REXSDK_VERSION=0.20.0.8`, perf split ON, livery diag2 ON, readback sync composite ON, `MCLA_M10_SPEEDUP_DEFAULTS=ON`, `MCLA_PREWARM_SEED_DEFAULT=ON`, `MCLA_DEV_OVERRIDES=OFF`; CMakeCache and `build.ninja` confirm). 0 compiler warnings, 0 errors. |
| 4 | Unit tests | **PASS, first run.** 47/47 executables return 0 (`private/l67v-work/l67-host-tests.log`), including `mcla_save_copy_guard_test` (43 checks), `mcla_shared_read_copy_win_test`, `mcla_m10_defaults_test` (2412 checks), `mcla_launcher_preferences_test` (2095 checks); all output equals the l67 dev run apart from fixture paths and PIDs. |
| 5 | ast-grep 0.50.0 in the clone | **PASS.** `scan` exit 0; `test` 3/3. |
| 6 | Strings and tables | **PASS.** `mcla.exe`: `MCLA m10 default: {}={}`, `platform=`, `wine_only`, `save copy start`, `save copy skipped`, `save-copy-skipped.txt`, the import of `rex_xam_content_transactions_active` from `rexruntime.dll` (exported there), `MCLA graphics: video memory {} MB near budget`; no `MCLA dev overrides` reader strings. `MCLA-Launcher.exe`: UTF-16 `file(s) in sessions` (KI-064), `store-seed.txt`. KI-057 is logic only: confirmed by the tests and the build, not by a string. |
| 7 | Path strings | **PASS.** Seven input files, ASCII and UTF-16LE, case-insensitive. Only `.text` machine-code hits: nine in `mcla.exe` (guest-address immediates and one `pshufb` rip displacement), one `movdqu` encoding in `MCLA-Launcher.exe` (offsets in the M9-001 report); not paths. |
| 8 | Expiry marker | **PASS.** UTF-16 `MCLA test-build admission` once in `mcla.exe`. |

## Artifacts (private/l67-build), copied to private/l67-vol-input

| File | Bytes | SHA-256 |
| --- | --- | --- |
| `mcla.exe` | 53293056 | `1B4642487AC6684BF1054930F7BD6B8F1DD2021127BFD7B34C43EDB300D7B64C` |
| `MCLA-Launcher.exe` | 1451520 | `77FC975A63C40793EDA2FDF3EC1308F7C70051ED8EF496B2750F66C72808750E` |
| `mcla_crash_handler.exe` | 211968 | `79A62642AC5DB488DF2DECF9ADE547989F0AD2557776DAFD525D4B03256DCEE3` |
| `mcla_diagnostic_image_helper.exe` | 60928 | `9506FE44ABCD95783301DF0450DE9E6693A89865DBC1E7AA48617914DE64457B` |
| `rexruntime.dll` | 11064320 | `7695301AE6AC8296B2B38D45FDF359317AEA1D32054236791F38E1557748892B` |
| `rexgpu-xenos.dll` | 3210240 | `1D081EB31000DA15855F9E2884875DC5CAB0B1E5F828AF74A7A2BACC7FABB862` |
| `TracyClient.dll` | 246784 | `8BE2138700AB787C46304D29EA0B230FDA4920906076C69075817E3CF9540A1A` |

`private/l67-vol-input` holds exactly these seven files; the prepare script adds
`mcla-test-build.ticket`. The three DLLs are byte-identical to `private/l67v-sdk-install/bin`.
Pins: `private/l67-pins.json`; hash list `private/l67v-work/l67-vol-input.sha256`.
`Launch-MCLA.exe` (not packaged): `8868D8B9B83FFF2641F43BFE9BE2B369095BAFD84FC5A6F5B5BB559E1D6B1B24`.

## Prepare script (owner step)

`private/l67-volunteer-prepare.ps1` (SHA-256
`9CECFE214B1818A926876C5BD32ECABFA9927B6FDBD7B20659041BC3732D8D12`) was generated from
`l65-volunteer-prepare.ps1` by `private/l67v-work/mk_prepare.py` with `private/l67-pins.json`.
Code changes against l65 (diff of the script bodies): l67 paths for the input, staging, entries
and list files; the seven new pins; carried entries from `l65-entries.json` (was
`l56-entries.json`); updated messages. The V2 flow is otherwise identical
(`New-MclaTestBuildTicket.ps1 -KillSwitch -Capabilities unlocked,1080p,1440p`, grant line
`capabilities 1080p,1440p,unlocked`, CreateNew entries with read-back,
`New-MclaTestBuildLeases.ps1`, staging with the l12 CRT and the tester guide, lease probe,
`Test-MclaVolunteerStaging.ps1 -RequireVolunteer`, receipt grant check). PowerShell 7.4.22
parse: 0 errors for the prepare, clone and test scripts (`private/l67v-work/l67-parsecheck.log`).
Run:

```
C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l67-volunteer-prepare.ps1
```

**Entries-merge dry run** (`-DryRunDirectory` on an empty temp directory, log
`private/l67v-work/l67-dryrun.log`): **6 entries**: l48 `6FEC02D0...` (valid_until 1795957688),
l49 `0A46AE3E...` (1796023588), l52 `823BFBA4...` (1796045342), l56 `BAED27A7...` (1796141989),
l65 `422A20A6...` (1796278298), all unchanged from `l65-entries.json`, then the l67 host
`1B464248...` with valid_until 1796320108 (2026-12-03 19:48 Kyiv at dry-run time; the real run
sets it at signing). A second dry run into the now non-empty directory was refused ("dry-run
directory is not empty", `l67-dryrun-refuse.log`). The temp directory was removed. No
`l67-entries.json`, ticket, list or staging output exists. Note: l48 expires 2026-11-29 and
l49 2026-11-30.

## Owner checks before sharing

1. **PC: one 30-minute session at 1080p** on the staged l67 (Unlocked or the usual mode), with
   police cars in view at times. This doubles as the **M10-007 video-memory budget
   measurement**: note every `MCLA graphics: video memory ... near budget ...` line (time, MB,
   the new texture cache limit) and whether stutters follow it.
   - Log on Windows: `MCLA m10 defaults (startup): platform=windows applied=1 ... wine_only=1`
     and `MCLA m10 defaults (graphics plugin): platform=windows applied=5 failed=0 ...
     wine_only=1`; `... left at ... (wine_only)` lines for upload-ahead and bridging.
   - Police livery correct; F10 once on a correct police car. That F10 should log
     `MCLA diagnostics: save copy start (gate waited N ms, ...)` and `save copy end ...`; no
     `save copy skipped` unless a save was in progress.
   - fps at the same spot equal to l65 (no Windows change is expected).
2. **PC: one 720p loop** (the usual BENCH loop): fps roughly as l65 (about 54 fps in the l65
   pre-share window), `fallbacks_to_full=0`, `forced_wait_failed=0`.
3. **Deck: one loop** (stock 30, 1280x720) on the staged l67 through Proton.
   - A fresh launcher profile (or the first run of the launcher on that Deck) starts with
     Fullscreen ticked (KI-057); an existing profile keeps its saved setting.
   - Log: `platform=wine`, `MCLA m10 default: memory_watch_disarm_bridge_safe_gaps=true`
     (startup, `applied=2`) and `MCLA m10 default: shared_memory_upload_ahead_pages=16`
     (graphics plugin, `applied=6`), `wine_only=0`.
   - fps while driving and frames over 100 ms against l65 on the same loop (expected better:
     the Proton-only part of the l64 gain).
   - One F10 while the game saves (for example right after a police fine) if convenient: no
     "storage device was removed"; the package holds either the save copy or
     `save-copy-skipped.txt` (KI-065).
4. Optional KI-064 check: a stray file in a profile's `sessions` folder no longer blocks CHECK
   SETUP; the status names the ignored file count.
5. **Prepare.** Run `l67-volunteer-prepare.ps1` in PowerShell 7. Expect `DONE`,
   `SIGNED_LEASES_VERIFIED entries=6`, an audit with 0 findings and the receipt grant
   `1080p,1440p,unlocked`. Publishing `l67-active-builds.txt` needs the owner's explicit yes.
   (If the owner prefers to run checks 1-3 on the staged build, signing comes first, as for l65.)

## Tester checks

1. Steam Deck: fps while driving and smoothness against l65 at the same place (expected
   better); report the resolution and the fps.
2. Steam Deck first run (new install): the game starts fullscreen.
3. F10 at any time, also during a save: no save error in the game; F10 packages still arrive.
4. Windows: fps and police livery as in l65; F10 on any wrong livery.
5. Launcher: a stray file in the sessions folder no longer blocks CHECK SETUP.
6. Windows smoke as for l65 (launcher 1080p/1440p/Unlocked, no 2160p; kill switch
   offline/online; wheel and Steam Input unchanged).

## Limits and risks

- Nothing was signed, staged, published or launched; no in-game, owner or Deck check is claimed
  for l67. The Wine-only defaults were measured only as part of the l64 "all" arm (with an
  owner-internal build and override file); the l67 combination through the build defaults has
  never run on the Deck. The l66 Deck run of the dev host is not this build.
- KI-065: the guard closes the F10/save race on the host side; the original Deck failure was
  never reproduced, so the fix is unverified. A save that stays in a transaction for over 2 s
  makes the F10 package skip the save copy (by design).
- KI-057: only a first run under Wine/Proton changes; testers with an existing Deck profile keep
  their windowed setting.
- The SDK `[content]` WARN logs are new; they are paced (16-line burst, then 1/s per kind), but a
  failing save path could add log lines.
- The `mcla-dev-overrides.txt` single-line read-back sync switch (l55 behaviour) is still compiled
  in, as in l65; testers are not told about it.
- Benign guest-address and instruction byte matches in the path scan (check 7).
- The clone came from the local committed `main`; the SDK and all submodules from GitHub.
  Output is not bit-identical across checkout paths.
