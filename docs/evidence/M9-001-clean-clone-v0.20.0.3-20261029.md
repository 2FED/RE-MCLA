# M9-001 clean-clone reproducibility on SDK v0.20.0.3 (l43, 2026-10-29)

Build-verification subagent report, for review by the primary agent. Nothing
was launched: no `mcla.exe`, no `MCLA-Launcher.exe`, no `Launch-MCLA.exe`, no
game. Only `rexglue.exe` (`--version`, `codegen`), SDK `ctest`, the 35 host unit
test executables and the fake image helper (started by the image client test)
ran. No save, profile or key was opened. No git write command was run in the
project worktree. Spark: not used (deterministic clone/build/test gates; the
only analysis is a byte diff that a script answers exactly).

Exit criterion checked: "a clean clone builds the volunteer artifacts" for the
pinned SDK.

## Inputs

| Item | Value |
| --- | --- |
| Project | branch `claude/admiring-jang-24d485`, cloned at `6981f514ab43` (task(M9-005) pin). The worktree later moved to `4119b05` (docs only). |
| SDK pin | gitlink `f519dd53fbabffca5492497cb42f782602aae9a1`, `https://github.com/2FED/rexglue-sdk.git`, branch `mcla/mcla-r-hotfixes`, tag `v0.20.0.3` |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b` from `https://github.com/2FED/SDL.git` (`mcla/xinput-hotplug-poll`) |
| Toolchain | VS 2022 Build Tools CMake/Ninja, LLVM clang 20.1.8 |
| Script | `private/l43-clean-clone-plan.ps1`, run per phase with Windows PowerShell 5.1 |
| Game input | `private/p1/game/default.xex`, SHA-256 checked by the script (`C386F400...A432`, M1-010) |

## Commands

```
powershell -NoProfile -ExecutionPolicy Bypass -File private\l43-clean-clone-plan.ps1 -SdkSha f519dd53fbabffca5492497cb42f782602aae9a1 -Phase clone
... -Phase sdk
... -Phase codegen
... -Phase host
ctest -C Release -j 8 --output-on-failure            (in private/l43-sdk-b, VS dev shell)
cmake --build private/l43-build --target <l41_targets.txt> mcla_internal_launcher mcla_diagnostic_image_fake_helper --parallel 8
<each of the 35 *_test.exe in private/l43-build>      (image client: fake helper + uppercase SHA-256; adapter policy: config/mcla.toml.example from the clone)
ast-grep scan ; ast-grep test                        (in private/l43-src, ast-grep 0.50.0)
```

Phase reports (all in `private/`):

| Phase | Report | Result |
| --- | --- | --- |
| clone (1st) | `l43-clean-clone-clone-20261029-185318.txt` | failed: script defect 1 (call depth overflow), nothing created |
| clone (2nd) | `l43-clean-clone-clone-20261029-185411.txt` | ran to `done`, but `libmspack links materialized: 0 of 0` (script defect 2); output removed |
| clone (accepted) | `l43-clean-clone-clone-20261029-185741.txt` | done; 15 of 15 libmspack links |
| sdk | `l43-clean-clone-sdk-20261029-190027.txt` | done; 1430 build actions; install; version checks |
| codegen | `l43-clean-clone-codegen-20261029-190610.txt` | done; 67 files, 0 mismatches |
| host | `l43-clean-clone-host-20261029-190656.txt` | done; 104 steps; path scan clean |

Console logs: `l43-clone-console{,2,3}.log`, `l43-sdk-console.log`,
`l43-codegen-console.log`, `l43-host-console.log`, `l43-sdk-ctest.log`,
`l43-host-tests-build.log`, `l43-host-tests.log`,
`l43-host-imageclient-upper.log`.

## Script fixes (private/l43-clean-clone-plan.ps1)

SHA-256 before `5AC758F0...C774E`, after `E56742F0...048A0`. Four lines changed:

1. The helper function `Git` shadowed the native `git` (PowerShell names are
   case-insensitive), so `& git` inside it and `Native git ...` recursed until
   "call depth overflow". Fix: `& git.exe @a 2>&1` in `Git`, and
   `Native git.exe @(...)` for the clone and the submodule update (3 lines).
2. `@(Git @(...) -split "`n" | ...)` is command-mode parsing: `-split` and
   "`n" were passed as extra arguments to `Git`, so the libmspack list was one
   string and no link was found. Fix: `@((Git @('-C', $ms, 'ls-files', '-s')) -split "`n" | ...)`.

The same parsing pattern does not affect the submodule-state check
(`$status -split` is a variable). That check also loses the leading space of
the first line through `Trim()`; harmless here, and the full recursive status
was verified by reading the logged list (no `-`, `+` or `U` prefix).

After defect 2 only `private/l43-src` was removed (the partial clone output) and
the clone phase rerun. No other l4x directory was touched.

## Acceptance

| # | Check | Result |
| --- | --- | --- |
| 1 | Clone pins | **PASS.** Project HEAD = worktree HEAD `6981f514`. Gitlink `f519dd5`; SDK HEAD `f519dd5`, `describe --exact-match` = `v0.20.0.3`, not shallow, origin `2FED/rexglue-sdk`, contained in `origin/mcla/mcla-r-hotfixes`. `sdl3` = `422c186`, origin `2FED/SDL`, branch `mcla/xinput-hotplug-poll`. 26 recursive submodules initialized, none `-`/`+`/`U`. |
| 2 | Version | **PASS.** `rexglue --version` = `0.20.0.3`; `rexglueConfigVersion.cmake` `set(PACKAGE_VERSION "0.20.0.3")`. Host CMake accepted `REXSDK_VERSION=0.20.0.3` against `MCLA_REXGLUE_VERSION`. |
| 3 | SDK ctest (clean build) | **PASS.** `100% tests passed, 0 tests failed out of 1874`; 4 skipped: BitStream Write 16-bit value / non-byte-aligned / preserves surrounding bits / byte-aligned (the known four). |
| 4 | Codegen | **PASS.** `rexglue codegen` from the clean SDK: 67 files, 133,916,828 bytes, all SHA-256 equal to the worktree's `generated/default` (67 files), 0 missing/extra/differing. |
| 5 | Host + launcher build and tests | **PASS.** Default build 104 steps plus 113 steps for the 35 tests, launcher and fake helper: 0 compiler warnings, 0 errors (configure shows only the same unused `CMAKE_C_COMPILER/CMAKE_C_FLAGS` notice as l42). 35/35 test executables pass (the image client after its rerun, deviation 1) with check counts identical to l42 (`private/l39-work/l42_tests.log`), including menu state 468, capabilities 262, controller 375; the 8 SKIP lines (Win32 1314 symlink) are identical. Image client 51 checks, 0 failures (fake child only, no capture); see deviation 1. Adapter policy 78 with `config/mcla.toml.example`. |
| 6 | ast-grep | **PASS.** `scan` exit 0, no findings; `test` 3 passed, 0 failed. |
| 7 | Artifacts | Table below. All seven differ from l42 by hash; sizes are identical; the differences are explained below. |
| 8 | Path strings | **PASS.** ASCII and UTF-16LE, case-insensitive, for `c:\bdu`, `c:/bdu`, `users\`, `/users/`, `mcla-recomp`, `worktrees`, `l43`, plus `l42`, `l41`, `l34-src`: 0 hits in all seven files. |

## Artifacts (private/l43-build)

| File | Bytes | SHA-256 | l42-int-input | Same size |
| --- | --- | --- | --- | --- |
| `mcla.exe` | 53073408 | `B75DEBEA12CEF7748D254C15FE45182EDF95CFD1F09CF83DE989BAEC0BE86B6A` | `63B8CE29...` | yes |
| `MCLA-Launcher.exe` | 1345024 | `A65A7239F3EFA981020BC34881F781E0956E35080DFEC551EE2EA19ECBE359B3` | `1FB8E19A...` | yes |
| `mcla_crash_handler.exe` | 211968 | `3BEA0FBA96DE7E0ECDDA8944C6621587BA5F32AE239FD2CB6AE893CCAB45F0F8` | `E4594529...` | yes |
| `mcla_diagnostic_image_helper.exe` | 60928 | `3FEC82EE5934AAFDD557003E97D9701D57D44A9DBAD63372ECF52E6AB96025AC` | `11482F3B...` | yes |
| `rexruntime.dll` | 10965504 | `04B37359AEE3D990FD240FEFD1A7122AFE36168E7A899C5953986B05EA059685` | `3A7A346A...` | yes |
| `rexgpu-xenos.dll` | 2997248 | `5B2AE2F816BABF39FE393141E19EFF509DEEC3C37B2EC49F36B74A46AD397F5E` | `F3CECECC...` | yes |
| `TracyClient.dll` | 246784 | `49E997AF74EEB9CE42B6CACB0C4DB4CE82D4B9D3BBD9471801BD582FFA9F4104` | `C2CC7F09...` | yes |

The three DLLs equal the `l43-sdk-install/bin` copies. `mcla.exe` was not
relinked by the test-target build (same hash as the host report). Also built:
`Launch-MCLA.exe` `5636C7AD...` (411136 bytes), `rexglue.exe` `63929498...`,
fake helper `89FE2015...`. `MCLA-Export-Metadata.exe` is not a target of this
tree (host report "missing", expected).

### Why they differ from l42 (byte diff by PE section)

The SDK source is the l41 SDK: `f519dd5` vs `8b10d62` (the l41 build commit)
changes only `.gitmodules` (SDL URL); the sdl3 gitlink is the same `422c186`.
The host source differs from the l42 build commit `c7afbd1` only in the pin
(`CMakeLists.txt`, `CMakePresets.json`, `scripts/bootstrap.ps1`, gitlink).

- `rexgpu-xenos.dll`, `TracyClient.dll`: 12 bytes each: COFF and debug-directory
  timestamps and the RSDS PDB GUID. Link identity only.
- `mcla_crash_handler.exe`: 2 bytes (COFF `TimeDateStamp`), as in l42.
- `rexruntime.dll`: the embedded build identity string changed from
  `build: rexglue-v0.20.0.6-dev.g8b10d62-win-amd64-Release@20261029_1648` to
  `build: rexglue-v0.20.0.3-win-amd64-Release@20261029_1900`, and the SDL
  revision string from `SDL-3.5.0-release-3.4.0-429-g422c186c0` to
  `SDL-3.5.0-422c186c0` (the fork clone has no upstream tags to describe). These
  shorter strings shift `.rdata`, so 1.88 M bytes differ: `.rdata` 1,841,699,
  `.pdata` 26,644, `.text` 10,898 (RIP-relative displacements shifted by 0x20),
  `.data` 350, `.reloc` 96, headers 8. The string
  sets differ only in these entries, the RSDS/PDB record and 2 anonymous
  namespace names.
- `mcla.exe`: the SDK app build tag `[rexglue-v0.20.0.6-dev.g8b10d62-Release]`
  became `[rexglue-v0.20.0.3-Release]`; same shift pattern (`.rdata` 2.36 M,
  `.text` 570 k displacement bytes, `.pdata` 65 k), plus 3 anonymous namespace
  names and link identity. Section-level diff only; no instruction-level
  equivalence proof was attempted for `mcla.exe`.
- `MCLA-Launcher.exe` (214 bytes, all `.data` + 2 header bytes) and
  `mcla_diagnostic_image_helper.exe` (11 bytes): only clang's MSVC-ABI
  anonymous-namespace hashes (`?A0x348A2C4D` vs `?A0xD15AB899`) and the
  timestamp. These hashes derive from the source file path, which differs
  between the worktree and the clone (they are not covered by
  `-ffile-prefix-map`).

So no difference beyond the version/identity strings, link timestamps/GUIDs and
path-derived anonymous-namespace names was found. Bit-identical output across
checkout paths is not achieved (anonymous-namespace names, timestamps).

## Deviations

1. The image client test was first run with the fake helper's lowercase
   SHA-256 and reported 13 failures (every mode `failure=2`, rejected before
   any launch). It was rerun with the uppercase hash (`89FE2015...518A`): 51
   checks, 0 failures, all 11 mode lines identical to l42. Both logs
   are kept. The test expects the uppercase form.
2. The host phase sets `CMAKE_EXE_LINKER_FLAGS=-Xlinker /pdbaltpath:%_PDB%` and
   `REXSDK_VERSION=0.20.0.3`; l42 left both empty. All `MCLA_*` cache options
   are identical to l42. Sizes are equal, and l42 had no path strings either.
3. Materializing the 15 libmspack links leaves the clone's SDK submodule marked
   ` m` (modified content) in `git status`. It is a local build step inside the
   clone only.
4. `cmake --install` registered `private/l43-sdk-install` in the CMake user
   package registry (HKCU, next to the existing l39/l41 entries). The host build
   uses an explicit `rexglue_DIR`, so the registry is not consulted.
5. `vswhere.exe is not recognized` appears once per dev-shell launch (VS dev
   shell noise, same in earlier runs); the toolchain resolved correctly.

## Limits

- Build and unit tests only. Nothing was launched in-game; no physical or
  owner check is claimed. The l43 binaries are not staged, signed or audited.
- The clone came from the local worktree's committed branch, not from a GitHub
  copy of the project repository (the project is not published). The SDK and
  all submodules came from GitHub.
- Same machine and toolchain as l41/l42; a second machine was not tried.
