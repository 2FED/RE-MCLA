# M9-005 volunteer candidate l49 (2026-10-31)

Build/packaging subagent report, for review by the primary agent. I built the l49
volunteer candidate from a clean clone of the current M9 head. The SDK pin is unchanged at
v0.20.0.4, and the existing clean-clone SDK install from l48 was reused read-only. The
candidate is ready for the owner's signing step.

Nothing was signed, staged, published or launched. No `mcla.exe`, `MCLA-Launcher.exe`,
`Launch-MCLA.exe` or game process was started. The only programs run were:

- `rexglue.exe codegen`;
- CMake/Ninja;
- the 36 host unit test executables (the image client test starts the fake helper);
- ast-grep and llvm-objdump;
- Python for the byte scan;
- PowerShell 7 to parse the scripts and to dry-run the entries merge.

No key, save or profile was opened, and no git write command was run in the project
worktree. Spark was not used: every step is a deterministic clone, build or test gate, or
a byte comparison.

**l48 status:** l48 (`docs/evidence/M9-005-volunteer-candidate-l48-20261030.md`, host
`6FEC02D0...E3D0`) was signed, its list was published (`DebugBundle` `c2069e7`), and it
was distributed on 2026-10-30. l49 supersedes it. The l48 host stays in the l49 entries
because testers hold it.

## What l49 adds over l48

Details are in [M9-006 Deck fixes](M9-006-deck-fixes-20261031.md).

- **KI-050: Steam Deck / Steam Input controls are not reaching the game.** The launcher
  passes a bounded list of Steam/SDL variables through to `mcla.exe`. Since `0872baa` this
  happens **on every platform**, not only under Wine. The list is:
  - `SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD`, `SDL_GAMECONTROLLERCONFIG`,
    `SDL_GAMECONTROLLER_IGNORE_DEVICES`, `SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT`;
  - `SteamVirtualGamepadInfo`, `SteamAppId`, `SteamGameId`, `SteamDeck`;
  - `STEAM_COMPAT_DATA_PATH`, `WINEPREFIX`.

  The existing size, duplicate and NUL rules still apply.
  - After START GAME, the status line reports
    `Steam/SDL environment passed through: N variables.` when N > 0. The debugger
    (Proton) log always gets the count.
  - The game logs `Steam virtual gamepad not allowed by environment` on every platform
    when the allow variable is missing.
  - When none of the names is set, the environment block is the same as in l48.
- **KI-051: the GPU choice is empty although an adapter exists.** If the saved GPU name is
  empty, the launcher preselects:
  - the only adapter when there is one;
  - otherwise the adapter with the most dedicated memory (on a tie, the first one
    enumerated).

  A saved name is kept exactly.
- **KI-053 (native Vulkan under Wine): deferred.** Every SDK install here, including
  `l48-sdk-install`, is built with `REXGLUE_USE_VULKAN=OFF`, and `rexgpu-xenos.dll` has no
  Vulkan backend. Asking for `vulkan` would make startup fail. `252aad8` therefore keeps
  `--gpu_backend=d3d12` on every platform. On the Deck, the game still runs D3D12 through
  vkd3d-proton, as in l48.

## Clone identity

| Item | Value |
| --- | --- |
| Project | Branch `claude/admiring-jang-24d485` was cloned from the local worktree into `private/l49-src`. Clone HEAD = worktree HEAD = `0872baa045ee0ce5fa32ec0ea1ea1a145bb7b00e`: "fix(M9-006): pass the Steam/SDL controller variables through on every platform (KI-050)". It sits on top of `252aad8` (KI-053 deferred, D3D12 kept) and `897ee88` (KI-050/051 launcher fixes). |
| SDK pin | Gitlink `daa1ae5f751a55461a5f57e7a42677b6210240ed`, origin `https://github.com/2FED/rexglue-sdk.git`. `describe --exact-match` gives `v0.20.0.4`. |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b`, origin `https://github.com/2FED/SDL.git` (branch `mcla/xinput-hotplug-poll`). |
| Submodules | 26 recursive, none marked `-`, `+` or `U`. All 15 libmspack links were materialized inside the clone. |
| SDK install | `private/l48-sdk-install` (clean-clone v0.20.0.4, built 2026-10-30), reused read-only; the SDK phase is disabled in the l49 script. The three SDK DLLs in the package are byte-identical to that install and to l48. |
| Host build dir | `private/l49-vol-build`. The dev build `private/l49-build` was not touched. |
| Script | `private/l49-clean-clone.ps1`, generated from `l48-clean-clone.ps1` by `private/l49-make-clone.py`. Phase reports are `private/l49-clean-clone-{clone,codegen,host}-20261031-*.txt`. |

**Discarded first attempt.** A first clone, build and test run from `252aad8` was discarded
on the primary agent's instruction when `0872baa` followed. In that run, codegen matched
(67/67), the build was clean and all 36 executables passed. `private/l49-src` and
`private/l49-vol-build` were deleted; no input folder had been made. Its logs are kept in
`private/l49-discarded-252aad8/`.

## Build and test results

| # | Check | Result |
| --- | --- | --- |
| 1 | Codegen | **PASS.** 67 files, 133,916,828 bytes; every SHA-256 equals the worktree's `generated/default`. |
| 2 | Host build | **PASS.** Volunteer options: expiry ON, raw timer ON, image/diagnostic tests ON, WGC OFF, prefix maps, `/pdbaltpath`, `REXSDK_VERSION=0.20.0.4`. 0 compiler warnings, 0 errors. The only CMake notice is the usual one about unused `CMAKE_C_COMPILER`/`CMAKE_C_FLAGS`, as in l48. |
| 3 | Unit tests | **PASS on rerun.** All 36 executables pass (`private/l49-host-tests-rerun.log`). Counts include: launcher process 55, controller 411, UI 995 (equal to the M9-006 dev counts), menu state 482, image client 57 (run with the fake helper's UPPERCASE hash), adapter policy 78 (run with `config/mcla.toml.example`). See the note below for the first run. |
| 4 | ast-grep 0.50.0 in the clone | **PASS.** `scan` exit 0; `test` 3/3 (`private/l49-astgrep.log`). |
| 5 | KI-022 resync still linked | **PASS.** `mcla.exe` imports `?RequestWheelEffectResync@sdl@input@rex@@YAXPEBD@Z` from `rexruntime.dll`, which exports it. |
| 6 | Path strings | **PASS.** All seven input files were scanned in ASCII and UTF-16LE, case-insensitive, at any offset, for `c:\bdu`, `c:/bdu`, `\users\`, `/users/`, `mcla-recomp`, `worktrees`, `l49`, `l48`, `l47`, `l46`, `l43` and `l34-src`. Every file has 0 hits (`private/l49-pathscan.py`, log `private/l49-pathscan.log`). The host phase's own scan of the five built executables was also clean. |
| 7 | Expiry marker | **PASS.** `mcla.exe` contains the UTF-16 text `MCLA test-build admission` once. |

**Note on the first test run.** In the first full run (`private/l49-host-tests.log`), one
check failed: `mcla_diagnostic_image_client_test` reported "bounded observed return", a
wall-clock limit of under 6 s per helper run. The run came straight after the test build,
on a freshly built fake helper. The other 56 checks passed. Three standalone reruns and a
full rerun of all 36 executables then passed with 0 failures. This looks like a slow first
start of a new binary, not a code change; the image client code is unchanged since l48.

## Artifacts (private/l49-vol-build), copied to private/l49-vol-input

| File | Bytes | SHA-256 | vs l48 |
| --- | --- | --- | --- |
| `mcla.exe` | 53088768 | `0A46AE3E5C845BD4A9D8CF5C2A9E46ECAD002C2E4052367A268F04892AD3FE89` | new (KI-050 allow-variable notice; SDK backend switch unchanged) |
| `MCLA-Launcher.exe` | 1373696 | `BB67370521CEF0DD247F90B33B67AD269CC017962320E617FDDFA8369C2CEA73` | new (KI-050 pass-through, KI-051 GPU default) |
| `mcla_crash_handler.exe` | 211968 | `2FE083D2B8CAAA762400B9B0FFAFA99AF17C66F7D4CC7A3BD4EADA2746DC3717` | rebuilt, same size |
| `mcla_diagnostic_image_helper.exe` | 60928 | `662CD3112685B0B4B6A2112228A8455B7761419F08D2D75684B27C4680069FF9` | rebuilt, same size |
| `rexruntime.dll` | 10984960 | `82E596CDF23CB2635AC12554B29B83ECD986B4292A8689085DDE2F42148168EE` | identical (SDK v0.20.0.4) |
| `rexgpu-xenos.dll` | 2997248 | `23B7FE2367FE7D6C778D917D164A338FC5C33FC0FEC911B6245B5850AEE154D2` | identical (D3D12 only) |
| `TracyClient.dll` | 246784 | `080012FD01B28DDDE2554F7FD6F3501A3F4528CE6DBB8FEBB13BA83B40568750` | identical |

`private/l49-vol-input` holds exactly these seven files; the prepare script adds
`mcla-test-build.ticket`. `Launch-MCLA.exe` was also built but is not packaged:
`9BDF7B43A4915D058AD085810AEB06D195D79D2F87CCE7999DF8540814D0458D` (411136 bytes). The pins
are recorded in `private/l49-pins.json`.

## Prepare script (owner step)

`private/l49-volunteer-prepare.ps1` (SHA-256
`c825807e6addec3eddb4e9251989669d82366fee7ab164c1dec69d1a0ce749d5`) was generated from
`l48-volunteer-prepare.ps1` by `private/l49-make-prepare.py` with `private/l49-pins.json`.

Code changes against l48:

- l49 paths for the input, staging, entries and list files;
- the seven new pins;
- carried entries read from `l48-entries.json` instead of `l46-entries.json`;
- updated messages.

The V2 flow is otherwise the same. PowerShell 7.4.22 parse: 0 errors
(`private/l49-parsecheck.log`, which also covers the clone and test scripts). Run:

```
C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l49-volunteer-prepare.ps1
```

1. **Pre-checks, before anything is written.** The script refuses to run if any of these
   already exists: `l49-volunteer`, `l49-entries.json`, `l49-active-builds.txt` or the
   input ticket. It requires exactly the seven input files with their pinned SHA-256. It
   refuses if the l49 host is already in `l48-entries.json`, and it warns about any expired
   carried entry.
2. **Ticket.** It signs a V2 ticket with
   `New-MclaTestBuildTicket.ps1 -KillSwitch -Capabilities unlocked,1080p,1440p` (no 2160p,
   KI-048) and checks the kind, the host row and the grant line
   `capabilities 1080p,1440p,unlocked`.
3. **Entries.** It writes `l49-entries.json` (CreateNew):
   - the five `l48-entries.json` entries unchanged: l2 grant `9195117A...` until
     2026-11-07, l38 `9777D5BE...`, l39 `6D073E4D...`, l46 `FC1F0E2D...` and l48
     `6FEC02D0...` until 1795957688;
   - the l49 host `0A46AE3E...`, valid for 30 days from signing.

   It reads the file back and checks every row.
4. **List.** It signs `l49-active-builds.txt` (`New-MclaTestBuildLeases.ps1`).
5. **Staging.** It stages `private/l49-volunteer` with `Stage-MclaInternalLauncher.ps1`,
   the CRT from `private/l12` and `-TesterGuide docs/volunteer-tester-guide.txt` from the
   worktree.
6. **List check.** It verifies the list with
   `l35-launcher-build/mcla_build_lease_probe_win.exe --verify-leases`.
7. **Audit.** It runs `Test-MclaVolunteerStaging.ps1 -RequireVolunteer` and checks the
   grant recorded in the receipt.
8. **Reminder.** It prints the publication reminder: publishing needs the owner's explicit
   yes, and offline the build starts only within 3 days of the ticket.

**Entries-merge dry run** (`-DryRunDirectory` on an empty temp directory, log
`private/l49-dryrun.log`): 6 entries. The output begins with the exact `l48-entries.json`
bytes minus the closing bracket, followed by the l49 host with `valid_until` 1796022643
(2026-11-30 09:10 Kyiv at dry-run time; the real run sets the value at signing). A second
dry run into the same, now non-empty directory was refused ("dry-run directory is not
empty", `private/l49-dryrun-refuse.log`). The temp directory was removed. No
`l49-entries.json`, ticket, list or staging output exists.

## Owner checklist

1. **Prepare.** Run `l49-volunteer-prepare.ps1` in PowerShell 7. Expect:
   - `DONE`;
   - the lease probe line `SIGNED_LEASES_VERIFIED entries=6`;
   - an audit with 0 findings;
   - the receipt grant `1080p,1440p,unlocked`.
2. **Windows smoke on `private/l49-volunteer`, as for l48:**
   - F10 twice in two scenes (fresh reports) and the report prompt;
   - launcher settings: 1080p, 1440p and Unlocked selectable, 2160p not offered;
   - KI-051: on a fresh setup, the GPU field is preselected (on a multi-GPU PC, the
     adapter with the most dedicated memory);
   - kill switch offline (within 3 days) and online (refused until the list naming l49 is
     published, then allowed);
   - the wheel checks from l48 (KI-022) still behave as on l48.
3. **Windows Steam Input check (KI-050, now platform-independent):**
   - Add the staged l49 `MCLA-Launcher.exe` to Steam as a non-Steam game and force Steam
     Input on for the pad in its controller settings.
   - Start it through Steam, then START GAME. The status should show
     `Steam/SDL environment passed through: N variables.` with N >= 1.
   - Expected: the pad works at PRESS START. The session `mcla.log` shows
     `SDL OnControllerDeviceAdded ... VendorID(0x28DE), ProductID(0x11FF)` and
     `SDL_ACTIVE_CONTROLLER`, and does not show
     `Steam virtual gamepad not allowed by environment`.
   - Negative control: run the l48 launcher the same way. No 28DE/11FF device line should
     appear, and the pad should be ignored while Steam Input is forced.
   - Without Steam: the status line has no pass-through notice (it appears only when
     N > 0), and the game behaves as in l48. `mcla.log` now contains the informational
     line `Steam virtual gamepad not allowed by environment ...` on every platform
     whenever the allow variable is missing. That is expected and harmless when Steam
     Input is not used.
4. **Steam Deck** (from [M9-006](M9-006-deck-fixes-20261031.md); use `PROTON_LOG=1`):
   1. **Controls at PRESS START** with the usual Steam Input layout. `mcla.log` shows
      `SDL OnControllerDeviceAdded ... VendorID(0x28DE), ProductID(0x11FF)` and
      `SDL_ACTIVE_CONTROLLER ... reason=first-connected`. It does not show
      `Steam virtual gamepad not allowed by environment`.
   2. **GPU preselected.** On a fresh setup, or when no GPU was ever chosen, the GPU field
      shows `AMD Custom GPU 0932 (RADV VANGOGH)` without picking it by hand.
   3. **Launcher status** after START GAME contains
      `Steam/SDL environment passed through: N variables.` with N >= 1. If the line is
      missing, Steam set none of the variables, and the pad will stay missing.
   4. **Backend (KI-053 deferred).** The log still shows `backend='d3d12'`, which runs
      through vkd3d-proton as in l48. It must not show a "selected native Vulkan backend"
      line or a Vulkan plugin error. The `vulkan_*` counters stay 0; that is expected for
      this package.
   5. **KI-049 carry-over.** Save/profile and build folders on `Z:\home\deck\...` and on
      the SD card are accepted. CHECK SETUP ends with
      "Proton: accepted symlink ancestor Z:\..." (record the text). F10 reaches PACKAGE
      READY with the package under the profile's `diagnostics\live\` on Z:.
5. **Publication decision.** Publish `l49-active-builds.txt` to `DebugBundle` only with the
   owner's explicit yes. Publishing it keeps l48 and l46 valid, because their entries are
   carried.
6. **Second machine.** One tester smoke on a second machine.

## Limits and risks

- Nothing was signed, staged, published or launched. No physical, in-game, Steam Input,
  wheel, Steam Deck or owner check is claimed.
- KI-050 has not been tested on a device; the analysis estimate is about 85%. If the pad is
  still dead with N >= 1 and the allow variable present, the remaining causes listed in the
  M9-006 analysis apply.
- On Windows, a launcher started through Steam now also passes `SteamAppId`/`SteamGameId`
  to the game. A very large `SDL_GAMECONTROLLERCONFIG` (more than about 32 KB together with
  the other variables) makes START GAME fail with "Child environment exceeds the bounded
  size"; it is not truncated.
- KI-053 is deferred. Native Vulkan on the Deck needs an SDK built with
  `REXGLUE_USE_VULKAN=ON`, which has never been built on Windows in this project. If
  `mcla.exe` is started directly, outside the launcher, under Proton with the Steam/Wine
  variables and `gpu_backend=any`, the host still switches to `vulkan` (the
  environment-variable form, as in l48). With this SDK, that start would fail. The launcher
  always passes `d3d12`, so this does not happen through the launcher.
- The image client wall-clock check failed once on the first run of a freshly built helper,
  then passed four times in a row (see the note above).
- The published list would carry the l2 grant, l38, l39, l46, l48 and l49 hosts; the owner
  decides retention at publication time.
- The clone came from the local worktree's committed branch; the SDK sources and all
  submodules came from GitHub. The SDK binaries are the l48 clean-clone install. Output is
  not bit-identical across checkout paths.

## Owner signing and publication (2026-10-31)

- The owner ran `private/l49-volunteer-prepare.ps1`: V2 kill-switch ticket with `capabilities 1080p,1440p,unlocked`, `l49-entries.json` (l2, l38, l39, l46, l48 carried; l49 host `0A46AE3E...` added for 30 days), signed `l49-active-builds.txt` issued 1793431588, staged `private/l49-volunteer`, lease probe `SIGNED_LEASES_VERIFIED entries=6`, volunteer audit passed.
- With the owner's explicit approval the primary agent published the list to branch `DebugBundle` of `2FED/RE-MCLA` (commit `ea1a7b0`, replacing the l48 list `c2069e7`); the raw URL returned HTTP 200 with bytes identical to the signed file.
- Not done by agents: distribution, the Windows Steam Input check, the Deck checks.

## Allowance trimmed to l48 and l49 (2026-10-31)

- Owner request after sharing l49: withdraw every older build. `private/l49-trim-entries.json` keeps only the l48 (`6FEC02D0...`, until 1795957688) and l49 (`0A46AE3E...`, until 1796023588) hosts; the owner signed `private/l49-trim-active-builds.txt` with `private/l49-trim-leases-prepare.ps1` (`SIGNED_LEASES_VERIFIED entries=2`, issued 1793435793).
- Published with explicit approval to `DebugBundle` (`2f609fb`, replacing `ea1a7b0`); the raw URL serves it byte-identically. Effect: l2, l38, l39 and l46 are refused at their next online check and the withdrawal is cached offline; testers must move to l48 or l49.
