# M9-005 volunteer candidate l52 (2026-10-31)

Build/packaging subagent report, for review by the primary agent. I built the l52
volunteer candidate from a clean clone of the current M9 head, including a fresh build of
the newly pinned SDK v0.20.0.5 from that clone. The candidate is ready for the owner's
signing step.

Nothing was signed, staged, published or launched. No `mcla.exe`, `MCLA-Launcher.exe`,
`Launch-MCLA.exe` or game process was started. The only programs run were:

- `rexglue.exe --version` and `rexglue.exe codegen`;
- CMake/Ninja and SDK `ctest`;
- the 36 host unit test executables (the image client test starts the fake helper);
- ast-grep and llvm-objdump;
- Python for the byte scan;
- PowerShell 7 to parse the scripts and to dry-run the entries merge.

No key, save or profile was opened, and no git write command was run in the project
worktree. Spark was not used: every step is a deterministic clone, build or test gate, or
a byte comparison.

**l49 status:** l49 (`docs/evidence/M9-005-volunteer-candidate-l49-20261031.md`, host
`0A46AE3E...FE89`) was signed, published and distributed; the published allowance was
later trimmed to l48 and l49 (`DebugBundle` `2f609fb`). l52 supersedes l49.

## What l52 adds over l49

Details are in [M9-006 pipeline wait always on](M9-006-pipeline-wait-always-20261031.md)
and the KI-047 reports.

- **KI-047 / KI-037: always-on bounded pipeline wait** (SDK `851f03b`
  `d3d12_async_pipeline_wait_always`; host default `mcla_pipeline_wait_always`, turned on
  by the host unless the user set it). While a D3D12 pipeline is still compiling, the draw
  waits (bounded: 2 s per wait, 15 s per minute) instead of being skipped. So no draw is
  skipped while a pipeline compiles: a cold shader cache gives short hitches instead of
  baked corruption (wrong first photo, wrong police car livery). Owner-measured on a cold
  store (l51): 2.1 s of waits in the first minute and 0 pending skips; a warm store waited
  1.2 s in total; the no-wait control skipped 117 draws per minute on a warm store.
- **Livery diagnostics** (SDK `928f6d3`, `d3d12_livery_diag`): available behind a cvar that
  is off by default. Nothing changes unless it is set.
- SDK v0.20.0.5 replaces v0.20.0.4; all three SDK DLLs are new builds from the clone.
  Everything in l49 (KI-050 Steam/SDL pass-through, KI-051 GPU default, KI-022 wheel
  resync, KI-053 deferred with D3D12 on every platform) is carried unchanged.

## Clone identity

| Item | Value |
| --- | --- |
| Project | Branch `claude/admiring-jang-24d485` cloned from the local worktree into `private/l52-src`. Clone HEAD = worktree HEAD = `737b6fcf0e5df289808b9a802601a68bc90cb8ef`: "docs(M9): KI-047 mechanism and fix recorded in the ledger; SDK v0.20.0.5 pinned; l52 next". It sits on `63d61f5` (task(M9-005): pin ReXGlue SDK v0.20.0.5 (851f03b): always-on pipeline wait and livery diagnostics), the commit named in the brief; `737b6fc` changes only `README-AI` and `docs/current-state.md`. |
| SDK pin | Gitlink `851f03bd23797e72da4091c3fe7fec7566c0d1c9`, origin `https://github.com/2FED/rexglue-sdk.git`. `describe --exact-match` gives `v0.20.0.5` (annotated). |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b`, origin `https://github.com/2FED/SDL.git` (branch `mcla/xinput-hotplug-poll`). |
| Submodules | 26 recursive, none marked `-`, `+` or `U`; the 25 nested SHAs equal l48's. All 15 libmspack links were materialized inside the clone. |
| SDK install | `private/l52-sdk-install`, built from the clone (`l52-sdk-b`, output `l52-sdk-o`) with the l48 flags. `rexglue --version` and `rexglueConfigVersion` = `0.20.0.5`. SDK ctest 1912/1912, 4 known skips. |
| Host build dir | `private/l52-build`. |
| Script | `private/l52-clean-clone.ps1`, generated from `l48-clean-clone.ps1` by `private/l52-make-clone.py`. Full results in [M9-001 clean clone v0.20.0.5](M9-001-clean-clone-v0.20.0.5-20261031.md). |

## Build and test results

| # | Check | Result |
| --- | --- | --- |
| 1 | SDK build, version, ctest | **PASS.** 1431 actions; `0.20.0.5` from `rexglue --version` and the config file; ctest 1912/1912, 4 known BitStream skips. |
| 2 | Codegen | **PASS.** 67 files, 133,916,828 bytes; every SHA-256 equals the worktree's `generated/default`. |
| 3 | Host build | **PASS.** Volunteer options: expiry ON, raw timer ON, image/diagnostic tests ON, WGC OFF, prefix maps, `/pdbaltpath`, `REXSDK_VERSION=0.20.0.5`. 0 compiler warnings, 0 errors. Only the usual unused `CMAKE_C_COMPILER`/`CMAKE_C_FLAGS` notice. |
| 4 | Unit tests | **PASS, first run.** All 36 executables return 0 (`private/l52-host-tests.log`); no rerun needed. Counts equal l49 except photo capture 214 (l49: 204, new pipeline-wait checks from `2a09c5a`). Image client 57 with the fake helper's UPPERCASE hash; adapter policy 78 with `config/mcla.toml.example`. |
| 5 | ast-grep 0.50.0 in the clone | **PASS.** `scan` exit 0; `test` 3/3 (`private/l52-astgrep.log`). |
| 6 | KI-022 resync still linked | **PASS.** `mcla.exe` imports `?RequestWheelEffectResync@sdl@input@rex@@YAXPEBD@Z`; `rexruntime.dll` exports it (ordinal 5065). |
| 7 | Pipeline wait string | **PASS.** `mcla.exe` contains `MCLA pipeline wait: always on (2 s per wait, 15 s per minute)` once; `rexgpu-xenos.dll` contains `d3d12_async_pipeline_wait_always` and `d3d12_livery_diag`. |
| 8 | Path strings | **PASS.** Seven input files, ASCII and UTF-16LE, case-insensitive, any offset, for `c:\bdu`, `c:/bdu`, `\users\`, `/users/`, `mcla-recomp`, `worktrees`, `l52`, `l51`, `l50`, `l49`, `l48`, `l43`, `l34-src` (`private/l52-pathscan.py`, log `private/l52-pathscan.log`). The only hit is `L52` inside the instruction `mov eax, 0x8232354C` (`B8 4C 35 32 82`) in `mcla.exe` `.text`, also present in l49's `mcla.exe`; not a path. |
| 9 | Expiry marker | **PASS.** `mcla.exe` contains the UTF-16 text `MCLA test-build admission` once. |

## Artifacts (private/l52-build), copied to private/l52-vol-input

| File | Bytes | SHA-256 | vs l49 |
| --- | --- | --- | --- |
| `mcla.exe` | 53093376 | `823BFBA4A073ED49C4672180D107ED401106B682D44803B97BD7EA17E57B4C81` | new (always-on wait default, SDK 0.20.0.5) |
| `MCLA-Launcher.exe` | 1373696 | `15CBDA5A2F4F89811EA80B2C5AF3016890C24297211D958F1D71D179ECC07DC9` | rebuilt, same size |
| `mcla_crash_handler.exe` | 211968 | `4533FF9C1FA9AAEF248536605E29840700733B81E293034236D953C883F962E7` | rebuilt, same size |
| `mcla_diagnostic_image_helper.exe` | 60928 | `22CEB033A2EAA4A146926DCF23610DCA2BD6D8B5BE39B0ADFDC1C5CBBC171E86` | rebuilt, same size |
| `rexruntime.dll` | 10984960 | `F02510EFE2F7F820F88B20F5F1B5F715943D5BBFDB75599B3DCF2591653B57C1` | new SDK build |
| `rexgpu-xenos.dll` | 3026432 | `1B81BD86D7DCA6712E8CE02E04DD604B70D81DE9C56B81AA6FB6220B4AD773FB` | new (pipeline wait, livery diagnostics) |
| `TracyClient.dll` | 246784 | `7BDF8B816041BBD47420EA6C04EA2C036508F5898164B3B9CC6872003CB814B6` | new SDK build, same size |

`private/l52-vol-input` holds exactly these seven files; the prepare script adds
`mcla-test-build.ticket`. The three DLLs are byte-identical to `private/l52-sdk-install/bin`.
`Launch-MCLA.exe` was also built but is not packaged:
`509D8312EDE13E259A4D93EF1EE610D4CAC939AD517C8663E46FFEFD8EF733E4` (411136 bytes). The pins
are recorded in `private/l52-pins.json`.

## Prepare script (owner step)

`private/l52-volunteer-prepare.ps1` (SHA-256
`50805811BAEA7FC1A34AE49BD1F7B4570CAFF01B6372D93F0407FFCC23A66F32`) was generated from
`l49-volunteer-prepare.ps1` by `private/l52-make-prepare.py` with `private/l52-pins.json`.

Code changes against l49 (checked with a diff of the script bodies):

- l52 paths for the input, staging, entries and list files;
- the seven new pins (all seven change, because the SDK is new);
- carried entries read from `l49-entries.json` instead of `l48-entries.json`;
- updated messages (`l52 SDK v0.20.0.5 install required`).

The V2 flow is otherwise the same. PowerShell 7.4.22 parse: 0 errors
(`private/l52-parsecheck.log`, which also covers the clone and test scripts). Run:

```
C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l52-volunteer-prepare.ps1
```

1. **Pre-checks, before anything is written.** Refuses if `l52-volunteer`,
   `l52-entries.json`, `l52-active-builds.txt` or the input ticket exists. Requires exactly
   the seven input files with their pinned SHA-256. Refuses if the l52 host is already in
   `l49-entries.json`; warns about any expired carried entry.
2. **Ticket.** Signs a V2 ticket with
   `New-MclaTestBuildTicket.ps1 -KillSwitch -Capabilities unlocked,1080p,1440p` (no 2160p,
   KI-048) and checks the kind, the host row and the grant line
   `capabilities 1080p,1440p,unlocked`.
3. **Entries.** Writes `l52-entries.json` (CreateNew): the six `l49-entries.json` entries
   unchanged (l2 grant `9195117A...` until 2026-11-07, l38 `9777D5BE...`, l39
   `6D073E4D...`, l46 `FC1F0E2D...`, l48 `6FEC02D0...`, l49 `0A46AE3E...` until
   1796023588) plus the l52 host `823BFBA4...`, valid for 30 days from signing. Reads the
   file back and checks every row.
4. **List.** Signs `l52-active-builds.txt` (`New-MclaTestBuildLeases.ps1`).
5. **Staging.** Stages `private/l52-volunteer` with `Stage-MclaInternalLauncher.ps1`, the
   CRT from `private/l12` and `-TesterGuide docs/volunteer-tester-guide.txt` from the
   worktree.
6. **List check.** `l35-launcher-build/mcla_build_lease_probe_win.exe --verify-leases`.
7. **Audit.** `Test-MclaVolunteerStaging.ps1 -RequireVolunteer` and the receipt grant check.
8. **Reminder.** Publishing needs the owner's explicit yes; offline the build starts only
   within 3 days of the ticket.

**Entries-merge dry run** (`-DryRunDirectory` on an empty temp directory, log
`private/l52-dryrun.log`): 7 entries. The output is the six `l49-entries.json` entries
unchanged, followed by the l52 host with `valid_until` 1796044826 (2026-11-30 15:20 Kyiv at
dry-run time; the real run sets the value at signing). A second dry run into the same, now
non-empty directory was refused ("dry-run directory is not empty",
`private/l52-dryrun-refuse.log`). The temp directory was removed. No `l52-entries.json`,
ticket, list or staging output exists.

## Owner checklist

1. **Prepare.** Run `l52-volunteer-prepare.ps1` in PowerShell 7. Expect `DONE`, the lease
   probe line `SIGNED_LEASES_VERIFIED entries=7` (fewer if the entries are trimmed first),
   an audit with 0 findings and the receipt grant `1080p,1440p,unlocked`.
2. **Entries decision.** The script carries all six l49 entries, but the published list
   is currently trimmed to l48 and l49 (`2f609fb`). Publishing the l52 list as generated
   would make l2, l38, l39 and l46 valid again. If they should stay withdrawn, prepare a
   trimmed entries file (l48, l49, l52) before signing, as done with
   `l49-trim-leases-prepare.ps1`.
3. **Windows smoke on `private/l52-volunteer`, as for l49:** F10 report; launcher settings
   (1080p, 1440p, Unlocked; no 2160p); kill switch offline/online; wheel and Steam Input
   checks unchanged from l49.
4. **l52-specific tester checks (KI-047/KI-037):**
   - **First photo correct on a fresh build:** on a fresh install (empty shader/pipeline
     store), the first in-game photo has correct textures, no baked corruption.
   - **Police car livery:** the police car shows its correct livery on first sight.
   - **No new long freezes beyond the first minute on a fresh install:** short hitches in
     the first minute are expected (owner measured 2.1 s of waits in minute 1 on a cold
     store); after that, no freezes that l49 did not have. `mcla.log` shows
     `MCLA pipeline wait: always on (2 s per wait, 15 s per minute)`.
5. **Publication decision.** Publish `l52-active-builds.txt` to `DebugBundle` only with the
   owner's explicit yes.
6. **Second machine.** One tester smoke on a second machine.

## Limits and risks

- Nothing was signed, staged, published or launched. No physical, in-game, Steam Deck or
  owner check is claimed for l52; the l51 numbers above are the owner's earlier dev-build
  runs, not l52 runs.
- The wait trades corruption for hitches: on a very cold cache or a slow GPU/driver
  (for example vkd3d on the Deck), the per-minute budget (15 s) can be reached, after which
  draws are skipped again for the rest of that minute. Not measured on the Deck.
- The entries carried from `l49-entries.json` include hosts the owner has already withdrawn
  from the published list (see checklist item 2).
- One benign `L52` byte match in `mcla.exe` machine code (see check 8).
- The clone came from the local worktree's committed branch; the SDK sources and all
  submodules came from GitHub. Output is not bit-identical across checkout paths.
