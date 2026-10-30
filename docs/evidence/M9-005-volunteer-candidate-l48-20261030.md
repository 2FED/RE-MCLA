# M9-005 volunteer candidate l48 (2026-10-30)

Build/packaging subagent report, for review by the primary agent. The l48
volunteer candidate was built from a clean clone of the current M9 head (SDK pin
v0.20.0.4 with the KI-022 wheel force-feedback resync) and is ready for the owner's
signing step. Nothing was signed, staged, published or launched. No `mcla.exe`,
`MCLA-Launcher.exe`, `Launch-MCLA.exe` or game process was started. The only
programs run were `rexglue.exe` (`--version`, `codegen`), CMake/Ninja, SDK ctest,
the 36 host unit test executables (the image client test starts the fake helper),
ast-grep, llvm-objdump, Python for the byte scan, and PowerShell 7 to parse the
prepare script and dry-run its entries merge. No key, save or profile was opened,
and no git write command was run in the project worktree. Spark was not used:
every step is a deterministic clone/build/test gate or a byte comparison.

**l46 status:** l46 (`docs/evidence/M9-005-volunteer-candidate-l46-20261029.md`,
host `FC1F0E2D...DB8C`) was signed, its list published (`DebugBundle` `ad386f4`) and
distributed to the closed group on 2026-10-30. l48 supersedes it with the KI-022
fix; the l46 host stays in the l48 entries because testers hold it. l47 was an
owner-only candidate on an unpinned SDK and is not in any entries file.

## Clone identity

| Item | Value |
| --- | --- |
| Project | branch `claude/admiring-jang-24d485` cloned from the local worktree into `private/l48-src`; clone HEAD = worktree HEAD = `0e447a7b931315cce3de5c56c877c3d9f7800c60` (task(M9-005): pin ReXGlue SDK v0.20.0.4 (daa1ae5) with the wheel FFB resync), on top of `521178b` (KI-022 resync signals from the pause hook; T300 L2/R2 default 9,8) |
| SDK pin | gitlink `daa1ae5f751a55461a5f57e7a42677b6210240ed`, origin `https://github.com/2FED/rexglue-sdk.git`, `describe --exact-match` = `v0.20.0.4` |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b`, origin `https://github.com/2FED/SDL.git` |
| Submodules | 26 recursive, none `-`/`+`/`U`; 15 of 15 libmspack links materialized inside the clone |
| SDK install | built from the clone: `private/l48-sdk-b` / `l48-sdk-o`, installed to `private/l48-sdk-install`; `rexglue --version` = `0.20.0.4`; ctest 1900/1900 (4 known skips) |
| Host build dir | `private/l48-build` |
| Full clone record | [M9-001 clean clone v0.20.0.4](M9-001-clean-clone-v0.20.0.4-20261030.md) |

## Build and test results

| # | Check | Result |
| --- | --- | --- |
| 1 | Codegen | **PASS.** 67 files, 133,916,828 bytes, every SHA-256 equal to the worktree's `generated/default`. |
| 2 | Host build | **PASS.** Volunteer options (expiry ON, raw timer ON, image/diagnostic tests ON, WGC OFF, prefix maps, `/pdbaltpath`, `REXSDK_VERSION=0.20.0.4`). 0 warnings, 0 errors. |
| 3 | Unit tests | **PASS.** 36/36; summaries identical to the worktree dev run of the same sources (menu state 482, image client 57 with the fake helper's UPPERCASE hash, adapter policy 78 with `config/mcla.toml.example`). |
| 4 | ast-grep 0.50.0 in the clone | **PASS.** `scan` exit 0; `test` 3/3. |
| 5 | KI-022 fix active | **PASS.** `mcla.exe` imports `RequestWheelEffectResync` from `rexruntime.dll`, which exports it: the real API, not the no-op fallback. |
| 6 | Path strings | **PASS.** All seven input files, ASCII and UTF-16LE, case-insensitive, any offset, for `c:\bdu`, `c:/bdu`, `\users\`, `/users/`, `mcla-recomp`, `worktrees`, `l48`, `l47`, `l46`, `l43`, `l34-src`: 0 hits in every file (`private/l48-pathscan.py`, log `private/l48-pathscan.log`). |
| 7 | Expiry marker | **PASS.** `mcla.exe` contains the UTF-16 text `MCLA test-build admission` once (expiry ON). |

## Artifacts (private/l48-build), copied to private/l48-vol-input

| File | Bytes | SHA-256 | vs l46 |
| --- | --- | --- | --- |
| `mcla.exe` | 53087744 | `6FEC02D0310589AD4210D8D54342C629A4681F5B3C7A265779EC1480567FE3D0` | new (KI-022 resync requests, L2/R2 9,8, SDK 0.20.0.4) |
| `MCLA-Launcher.exe` | 1364480 | `98522D9B81B7016FAC1D00DCCFF8252328695C4F8F999A321BD9DADE83E9DC26` | rebuilt; 202 bytes differ (checkout-path anonymous names, timestamp) |
| `mcla_crash_handler.exe` | 211968 | `466B2C92BFFD324364FA40398EA00C2E7378C45A217A228E6D1C883C44ED761A` | rebuilt; timestamp only |
| `mcla_diagnostic_image_helper.exe` | 60928 | `4A966260AC8A7B8DD85B887B57486ED3732B86043EFC74B3C01A423245276CA2` | rebuilt; 11 bytes |
| `rexruntime.dll` | 10984960 | `82E596CDF23CB2635AC12554B29B83ECD986B4292A8689085DDE2F42148168EE` | new SDK v0.20.0.4 |
| `rexgpu-xenos.dll` | 2997248 | `23B7FE2367FE7D6C778D917D164A338FC5C33FC0FEC911B6245B5850AEE154D2` | rebuilt SDK |
| `TracyClient.dll` | 246784 | `080012FD01B28DDDE2554F7FD6F3501A3F4528CE6DBB8FEBB13BA83B40568750` | rebuilt SDK |

`private/l48-vol-input` holds exactly these seven files; the prepare script adds
`mcla-test-build.ticket`. Also built but not packaged: `Launch-MCLA.exe`
`EE7A4D48A33289066F1EE4F8A48D1504A5073A7619A8FC19B2129A176F649A68` (411136 bytes).

## Prepare script (owner step)

`private/l48-volunteer-prepare.ps1` (SHA-256 `8ECE6191...8D3400`), generated from
`l46-volunteer-prepare.ps1` by `private/l48-make-prepare.py` with the pins in
`private/l48-pins.json`. Code changes against l46: l48 paths, the seven new pins,
carried entries from `l46-entries.json` (instead of `l39-entries.json`), messages.
Same V2 flow. PowerShell 7.4.22 parse: 0 errors. Run:

```
C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l48-volunteer-prepare.ps1
```

1. Pre-checks, before anything is written: refuses if `l48-volunteer`,
   `l48-entries.json`, `l48-active-builds.txt` or the input ticket exists; requires
   exactly the seven input files with their pinned SHA-256; refuses if the l48 host
   is already in `l46-entries.json`; warns about any expired carried entry.
2. Signs a V2 ticket with
   `New-MclaTestBuildTicket.ps1 -KillSwitch -Capabilities unlocked,1080p,1440p`
   (no 2160p, KI-048) and checks kind, host row and grant line
   `capabilities 1080p,1440p,unlocked`.
3. Writes `l48-entries.json` (CreateNew): the four `l46-entries.json` entries
   unchanged (l2 grant `9195117A...` until 2026-11-07, l38 `9777D5BE...`, l39
   `6D073E4D...`, l46 `FC1F0E2D...` until 1795936182) plus the l48 host `6FEC02D0...`
   for 30 days from signing; reads it back and checks every row.
4. Signs `l48-active-builds.txt` (`New-MclaTestBuildLeases.ps1`).
5. Stages `private/l48-volunteer` (`Stage-MclaInternalLauncher.ps1`, CRT from
   `private/l12`, `-TesterGuide docs/volunteer-tester-guide.txt` from the worktree,
   which since `521178b` says "L2 = button 9, R2 = button 8").
6. Verifies the list with `l35-launcher-build/mcla_build_lease_probe_win.exe --verify-leases`.
7. Runs `Test-MclaVolunteerStaging.ps1 -RequireVolunteer` and checks the receipt grant.
8. Prints the publication reminder (owner's explicit yes; offline start only within
   3 days of the ticket).

Entries-merge dry run (`-DryRunDirectory` on an empty temp directory, log
`private/l48-dryrun.log`): 5 entries; the output begins with the exact
`l46-entries.json` bytes minus its closing bracket, then the l48 host with
`valid_until` 1795957261 (2026-11-29 15:01 Kyiv at dry-run time; the real run sets
it at signing). A second dry run into the same, now non-empty directory refused
("dry-run directory is not empty"). The temp directories were removed. No
`l48-entries.json`, ticket, list or staging output exists.

## Owner checklist

1. Run `l48-volunteer-prepare.ps1` in PowerShell 7. Expect `DONE`, the lease probe
   `SIGNED_LEASES_VERIFIED entries=5`, an audit with 0 findings, and the receipt grant
   `1080p,1440p,unlocked`.
2. In-game smoke on `private/l48-volunteer` (Windows), as for l46: F10 twice in two
   scenes (fresh reports); report prompt; launcher settings (1080p, 1440p, Unlocked
   selectable, 2160p not offered); kill switch offline (within 3 days) and online
   (refused until the list naming l48 is published, then allowed).
3. Wheel (T300 or another force-feedback wheel), KI-022:
   - drive normally: centering and engine vibration as on l46; the log shows
     `SDL_WHEEL_FFB_EFFECT v=2 action=resync ... reason=periodic` lines (every 10 s,
     later while impacts keep coming); report a noticeable tick in centering;
   - race start (headlights, cutscene): centering survives or returns within about a
     second of the race going live; `action=resync ... reason=host:resume` and/or
     `reason=burst` right after `MCLA menu state: game running`;
   - head-on collision with the slow-motion camera: same expectation, with
     `reason=host:resume` or `host:racer-active` at the end of the slow motion;
   - press **L2** (raw button 9) right after each transition: the package must be
     recorded as **wheel-l2** with the wheel section and FFB lines; R2 (raw 8)
     gives a general package;
   - if centering is still lost although resync lines appear, say whether Alt-Tab
     restores it; report any `resync-limit`, `resync failed` or `update-change` line
     with the package.
4. Steam Deck (KI-049, carried from l46; with `PROTON_LOG=1`): save/profile and build
   folders under `Z:\home\deck\...` and on the SD card accepted, no "ancestor" error;
   CHECK SETUP ends with "Proton: accepted symlink ancestor Z:\..." (record the text);
   START GAME starts and a session folder appears; F10 reaches PACKAGE READY with the
   package under the profile's `diagnostics\live\` on Z: (note the image separately);
   record the launcher and image client "Proton: accepted symlink ancestor" log lines;
   regression: `C:\...` paths inside the prefix still work without the notice.
5. Publication decision: `l48-active-builds.txt` to `DebugBundle` (owner's explicit
   yes). Publishing it keeps l46 valid (its entry is carried).
6. One tester smoke on a second machine.

## Limits and risks

- Nothing was signed, staged, published or launched. No physical, in-game, wheel,
  Steam Deck or owner check is claimed.
- KI-022 is unverified on hardware: unit tests use a fake haptic layer and clock.
  The periodic resync restarts the spring every 10 s (expected imperceptible, not yet
  felt); a burst-end pass or an expired host request can cut a playing finite effect,
  as Alt-Tab does. Per-slot fallbacks are not proven on a T300.
- Host triggers depend on the M9-007 pause hook; with `mcla_ui_cap_guest_pause=false`
  only the burst and periodic triggers remain.
- The L2/R2 default 9,8 comes from T300 packages; other wheels may number differently.
- KI-049 on the Deck is still unverified (l46 Deck check pending).
- The published list would carry the l2 grant, l38, l39 internal, l46 and l48 hosts;
  the owner decides retention at publication time.
- The clone came from the local worktree's committed branch; the SDK and all
  submodules came from GitHub. Output is not bit-identical across checkout paths.
