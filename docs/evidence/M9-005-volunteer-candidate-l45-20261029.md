# M9-005 refreshed volunteer candidate l45 (2026-10-29)

Build/packaging subagent report, for review by the primary agent. The l45
volunteer candidate was built from a clean clone of the current M9 head and is
ready for the owner's signing step. Nothing was signed, staged, published or
launched. No `mcla.exe`, `MCLA-Launcher.exe`, `Launch-MCLA.exe` or game process
was started. The only programs run were `rexglue.exe codegen`, CMake/Ninja, the
35 host unit test executables (the image client test starts the fake helper),
ast-grep, and PowerShell 7 to parse the prepare script and dry-run its entries
merge. No key, save or profile was opened, and no git write command was run in
the project worktree. Spark was not used: every step is a deterministic
clone/build/test gate or a byte comparison that a script answers exactly.

## Clone identity

| Item | Value |
| --- | --- |
| Project | branch `claude/admiring-jang-24d485` cloned from the local worktree into `private/l45-src`; clone HEAD = worktree HEAD = `aeb3aefdc66f251e9f259d082ba40129afea4459` (task(M9-005): signed capability grants in test-build tickets) |
| SDK pin | gitlink `f519dd53fbabffca5492497cb42f782602aae9a1`, origin `https://github.com/2FED/rexglue-sdk.git`, `describe --exact-match` = `v0.20.0.3` |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b`, origin `https://github.com/2FED/SDL.git` (`mcla/xinput-hotplug-poll`) |
| Submodules | 26 recursive submodules, none `-`/`+`/`U`; 15 of 15 libmspack links materialized inside the clone (this leaves the SDK submodule marked ` m`, as in l43) |
| SDK install | the existing `private/l43-sdk-install` (v0.20.0.3), reused read-only because the SDK did not change. No `l45-sdk-*` directory was created. |
| Script | `private/l45-clean-clone.ps1` (SHA-256 `240B0CDC...AC785F`), a copy of `l43-clean-clone-plan.ps1`. Changes: src/build renamed to l45; SDK phase disabled; temporary manifest renamed; host path scan extended to `/Users/`, `mcla-recomp`, `worktrees`, `l45`, `l43`, `l34-src`. Run per phase with Windows PowerShell 5.1. |

Phase reports (in `private/`): `l45-clean-clone-clone-20261029-193115.txt`,
`l45-clean-clone-codegen-20261029-193414.txt`,
`l45-clean-clone-host-20261029-193433.txt`. Console logs: `l45-clone-console.log`,
`l45-codegen-console.log`, `l45-host-console.log`, `l45-host-tests-build.log`,
`l45-host-tests.log`, `l45-astgrep.log`.

## Build and test results

| # | Check | Result |
| --- | --- | --- |
| 1 | Codegen | **PASS.** `rexglue codegen` (l43 SDK) on the hash-checked `default.xex`: 67 files, 133,916,828 bytes, every SHA-256 equal to the worktree's `generated/default`; 0 missing/extra/differing. |
| 2 | Host build | **PASS.** Same options as l38/l43: `MCLA_TEST_BUILD_EXPIRY=ON`, `MCLA_RAW_TIMER_DEFAULT_ON=ON`, `MCLA_BUILD_IMAGE_CAPTURE_TESTS=ON`, `MCLA_BUILD_DIAGNOSTIC_TESTS=ON`, `MCLA_EXPERIMENTAL_F10_WGC=OFF`, Windows SDK 10.0.26100 cppwinrt, `-ffile-prefix-map` for the clone and the SDK install (both slash forms), `/pdbaltpath:%_PDB%`, `REXSDK_VERSION=0.20.0.3`. 104 steps + helper, then 113 steps for the 35 tests, launcher and fake helper. 0 compiler warnings, 0 errors. Configure prints only the usual unused `CMAKE_C_COMPILER`/`CMAKE_C_FLAGS` notice. `mcla.exe` was not relinked by the test build (hash unchanged). |
| 3 | Unit tests | **PASS.** 35/35 executables return 0. Image client: fake helper with its uppercase SHA-256 (`C38B8F4E...3201`), 51 checks, 0 failures (fake child only, no capture). Adapter policy: 78 checks with the clone's `config/mcla.toml.example`. Compared with l43, five counts are higher: lease policy 1438 -> 1499, lease runtime 69 -> 71, diagnostic export policy 585 -> 597, capabilities 262 -> 293, controller 375 -> 401. These match the l44 run of the capability commit (`private/l44-tests.log`). All other counts and the 8 SKIP lines (Win32 1314 symlink) are identical. |
| 4 | ast-grep 0.50.0 in the clone | **PASS.** `scan` exit 0, no findings; `test` 3 passed, 0 failed. |
| 5 | Path strings | **PASS.** All seven input files were searched in ASCII and UTF-16LE (both byte alignments), case-insensitively, for `c:\bdu`, `c:/bdu`, `\users\`, `/users/`, `mcla-recomp`, `worktrees`, `l45`, `l43` and `l34-src`. The only hit was case-insensitive `l45` in `rexruntime.dll`. That match is two uppercase `L45\0` sequences inside machine code. The file is byte-identical to `l43-sdk-install/bin/rexruntime.dll`, which was built before any l45 path existed. There is no lowercase `l45` in it. Not a path. |
| 6 | Expiry marker | **PASS.** `mcla.exe` contains the UTF-16 text `MCLA test-build admission` (expiry ON). |
| 7 | SDL strings in `mcla.exe` | **Note.** There are no `sdl3`, `thirdparty` or SDL source-path strings. There are 6 `SDL_*` identifiers (`SDL_ACTIVE_CONTROLLER`, `SDL_HOTPLUG`, `SDL_WHEEL_CONNECTED`, `SDL_WHEEL_FFB`, `SDL_XINPUT`, `SDL_E`). These are log tags and options from the M9-010/KI-040 SDL observer code. `l42-int-input/mcla.exe` has the same 6, while `l38-vol-input/mcla.exe` has none because it predates the observer. A literal "no SDL strings" check therefore does not hold for any build since l39. The volunteer audit does not test for SDL. |

## Artifacts (private/l45-build), copied to private/l45-vol-input

| File | Bytes | SHA-256 | vs l43 |
| --- | --- | --- | --- |
| `mcla.exe` | 53074944 | `1D4F72A66D97FACC287B4EA3FCD65E313F7B4E67964F001EB28926C6CC7F3CDD` | new (+1536 bytes; ticket capability code) |
| `MCLA-Launcher.exe` | 1351680 | `D0BA223457D741C3CA473C77324DD06CF7FD358D589D028420FCD13CDBBE89E6` | new (+6656 bytes; signed grant) |
| `mcla_crash_handler.exe` | 211968 | `04CCB7744CBC316DAC4368BFDCE6967A94054F8850FE81C22DA62C7E362F0D74` | rebuilt, same size |
| `mcla_diagnostic_image_helper.exe` | 60928 | `DE6AB50C8551545619E743D07D55AEDFEC1491A8217DF0B5E570FD2DAE1D4846` | rebuilt, same size |
| `rexruntime.dll` | 10965504 | `04B37359AEE3D990FD240FEFD1A7122AFE36168E7A899C5953986B05EA059685` | identical (l43 SDK install) |
| `rexgpu-xenos.dll` | 2997248 | `5B2AE2F816BABF39FE393141E19EFF509DEEC3C37B2EC49F36B74A46AD397F5E` | identical |
| `TracyClient.dll` | 246784 | `49E997AF74EEB9CE42B6CACB0C4DB4CE82D4B9D3BBD9471801BD582FFA9F4104` | identical |

Also built but not part of the package: `Launch-MCLA.exe` `9F45A680...A33F`
(411136 bytes) and the fake helper `C38B8F4E...3201`. The input folder
`private/l45-vol-input` holds exactly these seven files, the same set as
`private/l38-vol-input` without its ticket. The prepare script adds
`mcla-test-build.ticket`.

## Prepare script (owner step)

`private/l45-volunteer-prepare.ps1` (SHA-256 `2558E0A0...C785F`). It is
modelled on `l38-volunteer-prepare.ps1` and on the l39 entries merge. PowerShell
7 parse: 0 errors. Run:

```
C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l45-volunteer-prepare.ps1
```

1. Pre-checks, before anything is written:
   - refuses if `l45-volunteer`, `l45-entries.json`, `l45-active-builds.txt` or
     the input ticket already exists;
   - requires exactly the seven input files, each matching its pinned SHA-256
     (including `mcla.exe` and `rexruntime.dll`);
   - warns about any carried entry that has already expired.
2. Signs a V2 ticket with
   `New-MclaTestBuildTicket.ps1 -KillSwitch -Capabilities unlocked,1080p,1440p`.
   2160p is not granted. It then checks the ticket kind (`MCLA-TEST-BUILD-V2`),
   that the row names the pinned host, and the grant line
   `capabilities 1080p,1440p,unlocked`.
3. Writes `l45-entries.json` (CreateNew). It contains all three
   `l39-entries.json` entries, unchanged (l2 grant `9195117A...` until
   2026-11-07, l38 `9777D5BE...`, l39 `6D073E4D...`), plus the l45 host
   `1D4F72A6...` valid for 30 days from signing. The script reads the file back
   and checks every row.
4. Signs `l45-active-builds.txt` with `New-MclaTestBuildLeases.ps1`.
5. Stages `private/l45-volunteer` with `Stage-MclaInternalLauncher.ps1`, CRT
   from `private/l12` and `-TesterGuide docs/volunteer-tester-guide.txt`.
6. Verifies the list with
   `l35-launcher-build/mcla_build_lease_probe_win.exe --verify-leases`.
7. Runs `Test-MclaVolunteerStaging.ps1 -RequireVolunteer`, prints the
   receipt's `test_build_ticket_capabilities` and fails if it differs from the
   grant.
8. Prints the reminder: publishing `l45-active-builds.txt` to `DebugBundle`
   needs the owner's explicit yes; offline, the build starts only until 3 days
   after the ticket's creation time.

Entries-merge dry run: `-DryRunDirectory <empty temp dir>` runs the same
pre-checks and merge code, skips signing and staging, and writes only the
temporary JSON. It produced 4 entries: the three l39 entries byte-for-byte and
the l45 host with `valid_until` 1795887779 (2026-11-28 19:42 Kyiv at dry-run
time; the real run sets the value at signing). The temporary directory was
removed. No `l45-entries.json`, ticket, list or staging output exists.

Receipt field: the clone's and worktree's `scripts/Stage-MclaInternalLauncher.ps1`
parses the optional `capabilities` line from the ticket and writes
`test_build_ticket_capabilities` into `candidate-receipt.json`.
`Test-MclaVolunteerStaging.ps1` requires that field to equal the ticket
grant. No patch was needed.

## Owner checklist

1. Run `l45-volunteer-prepare.ps1` in PowerShell 7. Expect `DONE`, the lease
   probe `SIGNED_LEASES_VERIFIED entries=4`, an audit with 0 findings, and the
   receipt grant `1080p,1440p,unlocked`.
2. In-game smoke on `private/l45-volunteer`, offline or after publication:
   - F10 twice, in two different scenes; each report should be fresh;
   - wheel L2/R2 and pad Start+Back diagnostic triggers, if the hardware is
     available;
   - the report prompt;
   - launcher settings: 1080p, 1440p and Unlocked selectable; 2160p not offered;
   - kill switch, offline: starts within 3 days of signing;
   - kill switch, online: refused as withdrawn until the list naming l45 is
     published, then allowed.
3. Publication decision: `l45-active-builds.txt` to `DebugBundle` (owner's
   explicit yes).
4. One tester smoke on a second machine.

## Limits and risks

- Nothing was signed, staged, published or launched. No physical, in-game or
  owner check is claimed.
- T300 L2/R2 triggers are unverified on hardware. Pad Start/Back also reach the
  guest.
- KI-048 (2160p) is not offered: the grant has no `2160p`.
- The published list would also carry the l39 internal host and the l38 host,
  as `l39-entries.json` does. These entries are deliberately kept so the owner
  decides retention at publication time.
- `mcla.exe` contains SDL log-tag identifiers (row 7). These are expected, not
  paths.
- The clone came from the local worktree's committed branch, not from a GitHub
  copy of the project. The SDK and all submodules came from GitHub.
- Output is not bit-identical across checkout paths (anonymous-namespace names,
  link timestamps), as recorded for l43.
