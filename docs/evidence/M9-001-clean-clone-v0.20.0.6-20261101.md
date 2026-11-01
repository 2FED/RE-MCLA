# M9-001 clean-clone reproducibility on SDK v0.20.0.6 (l56, 2026-11-01)

Build-verification subagent report, for review by the primary agent. Nothing
was launched: no `mcla.exe`, no `MCLA-Launcher.exe`, no `Launch-MCLA.exe`, no
game. Only `rexglue.exe` (`--version`, `codegen`), SDK `ctest`, the 39 host unit
test executables and the fake image helper (started by the image client test)
ran. No save, profile or key was opened. No git write command was run in the
project repository. Spark: not used (deterministic clone/build/test gates; the
only analysis is hash and byte comparison that a script answers exactly).

Exit criterion checked: "a clean clone builds the volunteer artifacts" for the
newly pinned SDK, including a fresh SDK build from the clone, with the three
M10-001/KI-047 host options ON for the volunteer build.

## Inputs

| Item | Value |
| --- | --- |
| Project | branch `main`, cloned from the main checkout at `85128b6e10b3bd87c1370bb318025dbb5f693d88` (task(M10-001): pin ReXGlue SDK v0.20.0.6 (483458d): perf split, livery diag v2/v3, composite read-back sync); clone HEAD = checkout HEAD. |
| SDK pin | gitlink `483458d2885419234a0729698f29a8850a052b55`, `https://github.com/2FED/rexglue-sdk.git`, branch `mcla/mcla-r-hotfixes`, annotated tag `v0.20.0.6` (tag object `22bfdc0c`, peeled `483458d`, checked with `git ls-remote`). Three commits since v0.20.0.5: `7892797` `d3d12_perf_split`, `83b093b` `d3d12_livery_diag2` + root signature after a pipeline wait, `483458d` `d3d12_readback_sync_composite` + diag v3. |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b` from `https://github.com/2FED/SDL.git` (`mcla/xinput-hotplug-poll`), unchanged |
| Toolchain | VS 2022 Build Tools CMake/Ninja, LLVM clang 20.1.8 |
| Script | `private/l56-clean-clone.ps1` (SHA-256 `5780F21F...E33DCB`), generated from `l52-clean-clone.ps1` by `private/l56-work/mk_clone.py`: project branch `main` from `C:\BDU\MCLA-Recomp`, l56 names, tag/version `v0.20.0.6`/`0.20.0.6`, SDK phase enabled (output `l56-sdk-b`, `l56-sdk-o`, install `l56-sdk-install`, same flags as l52), host build `private/l56-build` with the l52 volunteer options **plus** `-DMCLA_PERF_SPLIT_DEFAULT=ON -DMCLA_LIVERY_DIAG2_DEFAULT=ON -DMCLA_READBACK_SYNC_COMPOSITE_DEFAULT=ON`, path-scan needles `l56` down to `l48`, `l43`, `l34-src`, `sdk-wt`. Run per phase with PowerShell 7.4.22 (`private/tools/pwsh`). |
| Game input | `private/p1/game/default.xex`, SHA-256 checked by the script (`C386F400...A432`, M1-010) |

## Commands

```
pwsh -NoProfile -ExecutionPolicy Bypass -File private\l56-clean-clone.ps1 -SdkSha 483458d2885419234a0729698f29a8850a052b55 -Phase clone
... -Phase sdk
... -Phase codegen
... -Phase host
private\l56-sdk-ctest.ps1        (ctest -C Release -j 8 --output-on-failure in private/l56-sdk-b)
private\l56-build-tests.ps1      (l41_targets.txt + wine path policy, perf split, livery diag, readback sync tests, internal launcher, fake helper)
private\l56-run-tests.ps1        (each *_test.exe in private/l56-build)
ast-grep scan ; ast-grep test    (in private/l56-src, ast-grep 0.50.0)
private\l56-pathscan.py          (seven input files)
```

Phase reports (all in `private/`): `l56-clean-clone-clone-20261101-164221.txt`,
`l56-clean-clone-sdk-*.txt`, `l56-clean-clone-codegen-*.txt`,
`l56-clean-clone-host-*.txt`. Console logs: `l56-clone-console.log`,
`l56-sdk-console.log`, `l56-sdk-ctest.log`, `l56-codegen-console.log`,
`l56-host-console.log`, `l56-host-tests-build.log`, `l56-host-tests.log`,
`l56-astgrep.log`, `l56-pathscan.log`, `l56-parsecheck.log`. Every phase and
test ran once; no script fix and no rerun was needed.

## Acceptance

| # | Check | Result |
| --- | --- | --- |
| 1 | Clone pins | **PASS.** Project HEAD = checkout HEAD `85128b6e`. Gitlink `483458d`; SDK HEAD `483458d`, `describe --exact-match` = `v0.20.0.6`, origin `2FED/rexglue-sdk`. `sdl3` = `422c186`, origin `2FED/SDL`. 26 recursive submodules (1 SDK + 25 nested), none `-`/`+`/`U`; all 25 nested SHAs equal those of the l52 clone `private/l52-src`. 15 of 15 libmspack links materialized. |
| 2 | Version | **PASS.** `rexglue --version` = `0.20.0.6`; `rexglueConfigVersion.cmake` declares `0.20.0.6`. Host CMake accepted `REXSDK_VERSION=0.20.0.6`. Embedded identity: `rexruntime.dll` `rexglue-v0.20.0.6-win-amd64-Release@20261101_1644`, `SDL-3.5.0-422c186c0`; `mcla.exe` `[rexglue-v0.20.0.6-Release]`. |
| 3 | SDK ctest (clean build) | **PASS.** `100% tests passed, 0 tests failed out of 1952`; 4 skipped: the known four BitStream Write cases. SDK build: 1436 actions; 503 `warning:` lines (third-party code; the l55 dev build also had 503). |
| 4 | Codegen | **PASS.** 67 files, 133,916,828 bytes, all SHA-256 equal to the main checkout's `generated/default` (67 files), 0 missing/extra/differing. |
| 5 | Host + launcher build and tests | **PASS.** Options: `MCLA_TEST_BUILD_EXPIRY=ON`, `MCLA_RAW_TIMER_DEFAULT_ON=ON`, `MCLA_PERF_SPLIT_DEFAULT=ON`, `MCLA_LIVERY_DIAG2_DEFAULT=ON`, `MCLA_READBACK_SYNC_COMPOSITE_DEFAULT=ON` (all three confirmed in `CMakeCache.txt`), `MCLA_BUILD_IMAGE_CAPTURE_TESTS=ON`, `MCLA_BUILD_DIAGNOSTIC_TESTS=ON`, `MCLA_EXPERIMENTAL_F10_WGC=OFF`, Windows SDK 10.0.26100 cppwinrt, `-ffile-prefix-map` for the clone and the SDK install, `/pdbaltpath:%_PDB%`, `REXSDK_VERSION=0.20.0.6`. 107 steps + helper (2), then 121 steps for the tests, launcher and fake helper. 0 compiler warnings, 0 errors (only the usual unused `CMAKE_C_COMPILER`/`CMAKE_C_FLAGS` notice). 39/39 executables return 0 on the first run (l52's 36 + `mcla_perf_split_test`, `mcla_livery_diag_test`, `mcla_readback_sync_test`); every suite summary line equals the l55 run (`private/l55-work/l55-tests.log`). |
| 6 | ast-grep | **PASS.** `scan` exit 0, no findings; `test` 3 passed, 0 failed. |
| 7 | Default-on code compiled in | **PASS.** `mcla.exe` contains `MCLA perf split: build default {}=1`, `MCLA livery diag2: build default {}=true` and `MCLA readback sync: fix ON, build default {}=true` (one each), the `set by the user`, `unavailable in this SDK`, `could not set` and `fix OFF` variants, `mcla-dev-overrides.txt` and `mcla_readback_sync_composite`, plus `MCLA pipeline wait: always on`. It imports `MarkPerfSplit`, `GetPerfSplitSnapshot`, `PerfSplitActiveMode` and `RequestWheelEffectResync`. `rexgpu-xenos.dll` contains the `MCLA_READBACK_SYNC v=1 kind={}` start/stop, `kind=mark` and `kind=minute` formats, `MCLA_PERF_SPLIT v=1`, `MCLA_LIVERY_DIAG2 v=1`, `MCLA_LIVERY_DIAG2_FLIGHT` and `d3d12_readback_sync_composite`. (String presence only; the log lines need a run.) |
| 8 | Path strings | **PASS (two benign machine-code hits).** ASCII and UTF-16LE, case-insensitive, for `c:\bdu`, `c:/bdu`, `\users\`, `/users/`, `mcla-recomp`, `worktrees`, `l56`..`l48`, `l43`, `l34-src`, `sdk-wt` over the seven input files: 0 hits except in `mcla.exe` `.text`: `l54` at `0xF11644` (`C7 87 50 01 00 00 6C 35 34 82` = `mov dword [rdi+0x150], 0x8234356C`) and `L52` at `0xE231FC` (`B8 4C 35 32 82` = `mov eax, 0x8232354C`), recompiled guest address constants at the same offsets as in the l55 build; not paths. The host phase's own case-sensitive scan reported `l54` for `mcla.exe` (the same bytes). |
| 9 | Expiry marker | **PASS.** UTF-16 `MCLA test-build admission` once in `mcla.exe`. |

## Artifacts (private/l56-build, copied to private/l56-vol-input)

| File | Bytes | SHA-256 | vs l52 |
| --- | --- | --- | --- |
| `mcla.exe` | 53194752 | `BAED27A780C10173A78E87D46B7531A717C922DCB60C60FDC41B1E10730563D1` | new (+101376 bytes: perf split, livery diag, read-back sync host code and the other M10 host changes) |
| `MCLA-Launcher.exe` | 1373696 | `B7F3FB3614B2380763D272DBAA542F8CD54DCF1493F8E92408DDE35C90E5CC63` | rebuilt, same size |
| `mcla_crash_handler.exe` | 211968 | `67DBB64FAC3250647FFC7B92E1022654FA789A0D8A51EC1E2B2DE97CBCE43322` | rebuilt, same size |
| `mcla_diagnostic_image_helper.exe` | 60928 | `E702980B065E91CF02E3754086943ACD4289DDCBFEAD28766CB6D15980C45664` | rebuilt, same size |
| `rexruntime.dll` | 10995712 | `A44703D8222A1A99724972111660F1F61BAB197F05EFA57529EB0A65350780DA` | new SDK build (+10752) |
| `rexgpu-xenos.dll` | 3136000 | `7BC8C63605AD488BE824B72F96635A0E01CDAF654F4B7942128BCE89E40080DE` | new (+109568: perf split, diag v2/v3, read-back sync) |
| `TracyClient.dll` | 246784 | `C7CFD37AD9EC151D73A2E57F7D744E33A359A03642B513F994082195EF45D333` | rebuilt from the clone, same size |

The three DLLs are byte-identical to `private/l56-sdk-install/bin`. Also built:
`Launch-MCLA.exe` `64F7253F...4B34F` (411136 bytes), `rexglue.exe` `5CE74AAE...0C0F`
(4112896 bytes), fake helper `A8BEB3BC...AA42`. `MCLA-Export-Metadata.exe` is not a
target of this tree (host report "missing", expected).

## Deviations

1. The project is cloned from `main` of the main checkout (the M10 work lives on
   `main`), not from the `admiring-jang` worktree used up to l52; the codegen reference
   is the main checkout's `generated/default` (untracked, 67 files, identical to the
   clone's output).
2. `cmake --install` registered `private/l56-sdk-install` in the CMake user package
   registry (HKCU), as for earlier clones. The host build uses an explicit `rexglue_DIR`.

## Limits

- Build and unit tests only. Nothing was launched in-game; no physical or owner
  check is claimed. The read-back sync fix has not run in the game in any build yet.
- The clone came from the local committed `main`, not from a GitHub copy of the project
  repository. The SDK and all submodules came from GitHub.
- Output is not bit-identical across checkout paths (anonymous-namespace names, link
  timestamps).
