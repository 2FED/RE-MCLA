# M9-005 refreshed volunteer candidate l46 (2026-10-30)

Build/packaging subagent report, for review by the primary agent. The l46
volunteer candidate was built from a clean clone of the current M9 head (KI-049
Proton path fixes) and is ready for the owner's signing step. Nothing was signed,
staged, published or launched. No `mcla.exe`, `MCLA-Launcher.exe`,
`Launch-MCLA.exe` or game process was started. The only programs run were
`rexglue.exe codegen`, CMake/Ninja, the 36 host unit test executables (the image
client test starts the fake helper), ast-grep, Python for the byte scan, and
PowerShell 7 to parse the prepare script and dry-run its entries merge. No key,
save or profile was opened, and no git write command was run in the project
worktree. Spark was not used: every step is a deterministic clone/build/test gate
or a byte comparison that a script answers exactly.

**l45 status:** l45 (`docs/evidence/M9-005-volunteer-candidate-l45-20261029.md`,
host `1D4F72A6...3CDD`) was signed by the owner but never distributed. It is
superseded by l46 because it lacks the KI-049 fixes (Proton refused `Z:` paths as
reparse ancestors). The l45 host is not carried into the l46 entries.

## Clone identity

| Item | Value |
| --- | --- |
| Project | branch `claude/admiring-jang-24d485` cloned from the local worktree into `private/l46-src`; clone HEAD = worktree HEAD = `fbb18226f791c24f8557a0808cf2d3457bb2d1f2` (fix(M9-006): F10 image helper path follows the Wine ancestor policy (KI-049)), on top of `1719c00` (Wine reparse ancestors on Proton) and `aeb3aef` (ticket capability grants) |
| SDK pin | gitlink `f519dd53fbabffca5492497cb42f782602aae9a1`, origin `https://github.com/2FED/rexglue-sdk.git`, `describe --exact-match` = `v0.20.0.3` |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b`, origin `https://github.com/2FED/SDL.git` (`mcla/xinput-hotplug-poll`) |
| Submodules | 26 recursive submodules, none `-`/`+`/`U`; 15 of 15 libmspack links materialized inside the clone (this leaves the SDK submodule marked ` m`, as in l43/l45) |
| SDK install | the existing `private/l43-sdk-install` (v0.20.0.3), reused read-only because the SDK did not change. No `l46-sdk-*` directory was created. |
| Host build dir | `private/l46-vol-build`. `private/l46-build` already held an earlier worktree dev build (l41 SDK) and the script refuses to overwrite an existing output, so the volunteer build uses the new name. The dev build was not touched. |
| Script | `private/l46-clean-clone.ps1` (SHA-256 `2A8957C8...510DD8`), a copy of `l45-clean-clone.ps1`. Changes: src/report/manifest names to l46, host build dir `l46-vol-build`, host path scan also looks for `l46` and `l45`. SDK phase stays disabled. Run per phase with Windows PowerShell 5.1. |

Phase reports (in `private/`): `l46-clean-clone-clone-20261030-084107.txt`,
`l46-clean-clone-codegen-20261030-084509.txt`,
`l46-clean-clone-host-20261030-084528.txt`. Console logs: `l46-clone-console.log`,
`l46-codegen-console.log`, `l46-host-console.log`, `l46-host-tests-build.log`,
`l46-host-tests.log`, `l46-vol-astgrep.log`. Helper scripts: `l46-build-tests.ps1`
(l45's target list plus `mcla_wine_path_policy_test`), `l46-run-tests.ps1`,
`l46-pathscan.py`. (`l46-astgrep*.log`, `l46-tests*.log`, `l46-build*.log` and
`l46-build.ps1` belong to the earlier dev build.)

## Build and test results

| # | Check | Result |
| --- | --- | --- |
| 1 | Codegen | **PASS.** `rexglue codegen` (l43 SDK) on the hash-checked `default.xex`: 67 files, 133,916,828 bytes, every SHA-256 equal to the worktree's `generated/default`; 0 missing/extra/differing. |
| 2 | Host build | **PASS.** Same options as l45: `MCLA_TEST_BUILD_EXPIRY=ON`, `MCLA_RAW_TIMER_DEFAULT_ON=ON`, `MCLA_BUILD_IMAGE_CAPTURE_TESTS=ON`, `MCLA_BUILD_DIAGNOSTIC_TESTS=ON`, `MCLA_EXPERIMENTAL_F10_WGC=OFF`, Windows SDK 10.0.26100 cppwinrt, `-ffile-prefix-map` for the clone and the SDK install (both slash forms), `/pdbaltpath:%_PDB%`, `REXSDK_VERSION=0.20.0.3`. 104 steps + helper, then 115 steps for the 36 tests, launcher and fake helper. 0 compiler warnings, 0 errors. Configure prints only the usual unused `CMAKE_C_COMPILER`/`CMAKE_C_FLAGS` notice; the test build log starts with the usual dev-shell `vswhere.exe` notice (also in l45). `mcla.exe` was not relinked by the test build (hash unchanged). |
| 3 | Unit tests | **PASS.** 36/36 executables return 0. Image client: fake helper with its uppercase SHA-256 (`EA84962F...AE07`), 57 checks, 0 failures (fake child only, no capture). Adapter policy: 78 checks with the clone's `config/mcla.toml.example`. Compared with l45: new `mcla_wine_path_policy_test` 208 checks; launcher filesystem guard 76 -> 97; image client 51 -> 57 (new owned junction fixture). These match the worktree dev run of the KI-049 commits (`private/l46-tests2.log`). All other counts and the 8 SKIP lines (Win32 1314 symlink) are identical to l45. |
| 4 | ast-grep 0.50.0 in the clone | **PASS.** `scan` exit 0, no findings; `test` 3 passed, 0 failed. |
| 5 | Path strings | **PASS.** All seven input files were searched in ASCII and UTF-16LE (any byte offset, so both alignments), case-insensitively, for `c:\bdu`, `c:/bdu`, `\users\`, `/users/`, `mcla-recomp`, `worktrees`, `l46`, `l45`, `l43` and `l34-src`. Only `rexruntime.dll` had hits, all inside machine code: two uppercase `L45\0` sequences (offsets 0x3c530c, 0x9995f8; the known l45 false positive) and one `l46\0` at 0x4662e3, which is the displacement bytes of `lea rax,[rip+0x36346c]` (`48 8D 05 6C 34 36 00`). The file is byte-identical to `l43-sdk-install/bin/rexruntime.dll` (and to the l45 input), built before any l45/l46 path existed. Not paths. The host-phase scan of the build outputs also reported all five executables clean. |
| 6 | Expiry marker | **PASS.** `mcla.exe` contains the UTF-16 text `MCLA test-build admission` (expiry ON), 1 occurrence. |
| 7 | SDL strings in `mcla.exe` | **Note.** No `sdl3` or `thirdparty` strings. The same 6 `SDL_*` log tags/options as l42-l45 (`SDL_ACTIVE_CONTROLLER`, `SDL_E`, `SDL_HOTPLUG`, `SDL_WHEEL_CONNECTED`, `SDL_WHEEL_FFB`, `SDL_XINPUT`). Expected, not paths. |

## Artifacts (private/l46-vol-build), copied to private/l46-vol-input

| File | Bytes | SHA-256 | vs l45 |
| --- | --- | --- | --- |
| `mcla.exe` | 53087744 | `FC1F0E2D2585888BC43B4AB467D046CFD2C579A5837367F9DCFB6D2EEC80DB8C` | new (+12800 bytes; KI-049 Wine ancestor policy, F10 helper path) |
| `MCLA-Launcher.exe` | 1364480 | `40233CB75B0EC66330E8C8DA22979CB346FED6891E8FE9A8C09647F74E8B5B31` | new (+12800 bytes; Wine ancestor policy, CHECK SETUP notice) |
| `mcla_crash_handler.exe` | 211968 | `D4968E91085B3B910F6446F7C4303F41F6BC2DA42F778C0BF7597518ADE19FA0` | rebuilt, same size |
| `mcla_diagnostic_image_helper.exe` | 60928 | `33355DC4A324044CF84D62A16A8694FB4B5D1E36DD495388DDB2F9142F16201A` | rebuilt, same size |
| `rexruntime.dll` | 10965504 | `04B37359AEE3D990FD240FEFD1A7122AFE36168E7A899C5953986B05EA059685` | identical (l43 SDK install) |
| `rexgpu-xenos.dll` | 2997248 | `5B2AE2F816BABF39FE393141E19EFF509DEEC3C37B2EC49F36B74A46AD397F5E` | identical |
| `TracyClient.dll` | 246784 | `49E997AF74EEB9CE42B6CACB0C4DB4CE82D4B9D3BBD9471801BD582FFA9F4104` | identical |

Also built but not part of the package: `Launch-MCLA.exe`
`40DA7205730F9D0820EB9E54F955082D3046C84BA2F57BEFFE8236A5F8CA7E3A` (411136
bytes) and the fake helper `EA84962F...AE07`. The input folder
`private/l46-vol-input` holds exactly these seven files. The prepare script adds
`mcla-test-build.ticket`.

## Prepare script (owner step)

`private/l46-volunteer-prepare.ps1` (SHA-256 `45D81FE7...10F33C`), a copy of
`l45-volunteer-prepare.ps1` with l46 names and the new pins. Carried entries come
from `l39-entries.json`, not `l45-entries.json`, because the l45 host was never
distributed. PowerShell 7.4.22 parse: 0 errors. Run:

```
C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l46-volunteer-prepare.ps1
```

1. Pre-checks, before anything is written:
   - refuses if `l46-volunteer`, `l46-entries.json`, `l46-active-builds.txt` or
     the input ticket already exists;
   - requires exactly the seven input files, each matching its pinned SHA-256
     (including `mcla.exe` and `rexruntime.dll`);
   - refuses if the l46 host is already in `l39-entries.json`; warns about any
     carried entry that has already expired.
2. Signs a V2 ticket with
   `New-MclaTestBuildTicket.ps1 -KillSwitch -Capabilities unlocked,1080p,1440p`.
   2160p is not granted. It then checks the ticket kind (`MCLA-TEST-BUILD-V2`),
   that the row names the pinned host, and the grant line
   `capabilities 1080p,1440p,unlocked`.
3. Writes `l46-entries.json` (CreateNew). It contains all three
   `l39-entries.json` entries, unchanged (l2 grant `9195117A...` until
   2026-11-07, l38 `9777D5BE...`, l39 `6D073E4D...`), plus the l46 host
   `FC1F0E2D...` valid for 30 days from signing. The script reads the file back
   and checks every row.
4. Signs `l46-active-builds.txt` with `New-MclaTestBuildLeases.ps1`.
5. Stages `private/l46-volunteer` with `Stage-MclaInternalLauncher.ps1`, CRT
   from `private/l12` and `-TesterGuide docs/volunteer-tester-guide.txt` from the
   worktree.
6. Verifies the list with
   `l35-launcher-build/mcla_build_lease_probe_win.exe --verify-leases`.
7. Runs `Test-MclaVolunteerStaging.ps1 -RequireVolunteer`, prints the
   receipt's `test_build_ticket_capabilities` and fails if it differs from the
   grant.
8. Prints the reminder: publishing `l46-active-builds.txt` to `DebugBundle`
   needs the owner's explicit yes; offline, the build starts only until 3 days
   after the ticket's creation time.

Entries-merge dry run: `-DryRunDirectory <empty temp dir>` produced 4 entries:
the three l39 entries byte-for-byte (the output starts with the exact
`l39-entries.json` text minus its closing bracket) and the l46 host with
`valid_until` 1795935081 (2026-11-29 08:51 Kyiv at dry-run time; the real run
sets the value at signing). The temporary directory was removed. No
`l46-entries.json`, ticket, list or staging output exists.

## Owner checklist

1. Run `l46-volunteer-prepare.ps1` in PowerShell 7. Expect `DONE`, the lease
   probe `SIGNED_LEASES_VERIFIED entries=4`, an audit with 0 findings, and the
   receipt grant `1080p,1440p,unlocked`.
2. In-game smoke on `private/l46-volunteer` (Windows), offline or after
   publication:
   - F10 twice, in two different scenes; each report should be fresh;
   - wheel L2/R2 and pad Start+Back diagnostic triggers, if the hardware is
     available;
   - the report prompt;
   - launcher settings: 1080p, 1440p and Unlocked selectable; 2160p not offered;
   - kill switch, offline: starts within 3 days of signing;
   - kill switch, online: refused as withdrawn until the list naming l46 is
     published, then allowed.
3. Steam Deck (KI-049, from `docs/evidence/M9-006-ki-049-proton-paths-20261030.md`),
   with `PROTON_LOG=1` in the launch options:
   - choose the save/profile folder under `Z:\home\deck\...` (for example
     `Z:\home\deck\MCLA-saves`) and the build folder under `Z:\home\deck\...`:
     both accepted, no "ancestor" error;
   - repeat both on the SD card (`Z:\run\media\<card>\...`, or the card's own
     drive letter);
   - CHECK SETUP: the status ends with the notice "Proton: accepted symlink
     ancestor Z:\..." (record the exact text);
   - START GAME: the game starts and a session folder appears under the profile;
   - F10: the status dialog reaches PACKAGE READY and the new package folder
     appears under the profile's `diagnostics\live\` on Z:; note separately
     whether the image is present (capture under Proton is outside KI-049);
   - record the "MCLA launcher: Proton: accepted symlink ancestor" and "MCLA
     image client: Proton: accepted symlink ancestor" lines from the Proton log;
   - regression: paths inside the prefix (`C:\...`) still work without the
     Proton notice.
4. Publication decision: `l46-active-builds.txt` to `DebugBundle` (owner's
   explicit yes).
5. One tester smoke on a second machine.

## Limits and risks

- Nothing was signed, staged, published or launched. No physical, in-game,
  Steam Deck or owner check is claimed.
- KI-049 on the Deck is unverified: the Wine facts in the M9-006 evidence are
  partly from memory, and the SD card case is the least certain.
- T300 L2/R2 triggers are unverified on hardware. Pad Start/Back also reach the
  guest.
- KI-048 (2160p) is not offered: the grant has no `2160p`.
- The published list would also carry the l39 internal host and the l38 host,
  as `l39-entries.json` does; the owner decides retention at publication time.
  The signed-but-undistributed l45 host is deliberately not carried.
- `mcla.exe` contains SDL log-tag identifiers (row 7). These are expected, not
  paths.
- The clone came from the local worktree's committed branch, not from a GitHub
  copy of the project. The SDK and all submodules came from GitHub.
- Output is not bit-identical across checkout paths (anonymous-namespace names,
  link timestamps), as recorded for l43.

## Owner signing and publication (2026-10-30)

- The owner ran `private/l46-volunteer-prepare.ps1`: V2 kill-switch ticket with `capabilities 1080p,1440p,unlocked`, `l46-entries.json` (l2, l38, l39 carried; l46 host `FC1F0E2D...` valid until 1795936182), signed `l46-active-builds.txt` issued 1793344182, staged `private/l46-volunteer`, lease probe `SIGNED_LEASES_VERIFIED entries=4`, volunteer audit passed.
- With the owner's explicit approval the primary agent published the list to branch `DebugBundle` of `2FED/RE-MCLA` (commit `ad386f4`, replacing the l38 list `7097aea`); `https://raw.githubusercontent.com/2FED/RE-MCLA/DebugBundle/active-builds.txt` returned HTTP 200 with bytes identical to the signed file.
- Not done by agents: distribution to the closed group, the owner smoke, the Steam Deck checks (planned for the evening), the tester smoke.
