# M9-001 clean-clone reproducibility on SDK v0.20.0.5 (l52, 2026-10-31)

Build-verification subagent report, for review by the primary agent. Nothing
was launched: no `mcla.exe`, no `MCLA-Launcher.exe`, no `Launch-MCLA.exe`, no
game. Only `rexglue.exe` (`--version`, `codegen`), SDK `ctest`, the 36 host unit
test executables and the fake image helper (started by the image client test)
ran. No save, profile or key was opened. No git write command was run in the
project worktree. Spark: not used (deterministic clone/build/test gates; the
only analysis is hash and byte comparison that a script answers exactly).

Exit criterion checked: "a clean clone builds the volunteer artifacts" for the
newly pinned SDK, including a fresh SDK build from the clone.

## Inputs

| Item | Value |
| --- | --- |
| Project | branch `claude/admiring-jang-24d485`, cloned at `737b6fcf0e5df289808b9a802601a68bc90cb8ef` (docs(M9): KI-047 mechanism and fix recorded in the ledger; SDK v0.20.0.5 pinned; l52 next). The brief named `63d61f5` (task(M9-005): pin ReXGlue SDK v0.20.0.5); `737b6fc` was committed on top of it before the clone and changes only `README-AI` and `docs/current-state.md` (4+/2-), so the built sources are those of `63d61f5`. |
| SDK pin | gitlink `851f03bd23797e72da4091c3fe7fec7566c0d1c9`, `https://github.com/2FED/rexglue-sdk.git`, branch `mcla/mcla-r-hotfixes`, annotated tag `v0.20.0.5` (tag object `fb6eb722`, peeled `851f03b`, checked with `git ls-remote`). Two commits since v0.20.0.4: `928f6d3` livery diagnostics (`d3d12_livery_diag`, default off) and `851f03b` always-on bounded pending-pipeline wait (`d3d12_async_pipeline_wait_always`). |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b` from `https://github.com/2FED/SDL.git` (`mcla/xinput-hotplug-poll`), unchanged |
| Toolchain | VS 2022 Build Tools CMake/Ninja, LLVM clang 20.1.8 |
| Script | `private/l52-clean-clone.ps1` (SHA-256 `2F44F5E7...852F77`), generated from `l48-clean-clone.ps1` by `private/l52-make-clone.py`: l52 names, tag/version `v0.20.0.5`/`0.20.0.5`, SDK phase enabled (output `l52-sdk-b`, `l52-sdk-o`, install `l52-sdk-install`, same flags as l48), host build `private/l52-build`, path-scan needles `l52`, `l51`, `l50`, `l49`, `l48`, `l43`. Run per phase with PowerShell 7.4.22 (`private/tools/pwsh`). |
| Game input | `private/p1/game/default.xex`, SHA-256 checked by the script (`C386F400...A432`, M1-010) |

## Commands

```
pwsh -NoProfile -ExecutionPolicy Bypass -File private\l52-clean-clone.ps1 -SdkSha 851f03bd23797e72da4091c3fe7fec7566c0d1c9 -Phase clone
... -Phase sdk
... -Phase codegen
... -Phase host
private\l52-sdk-ctest.ps1        (ctest -C Release -j 8 --output-on-failure in private/l52-sdk-b, VS dev shell)
private\l52-build-tests.ps1      (l41_targets.txt + mcla_wine_path_policy_test, mcla_internal_launcher, fake helper)
private\l52-run-tests.ps1        (each *_test.exe in private/l52-build; image client: fake helper + UPPERCASE SHA-256; adapter policy: config/mcla.toml.example from the clone)
ast-grep scan ; ast-grep test    (in private/l52-src, ast-grep 0.50.0)
llvm-objdump -p                  (imports of mcla.exe, exports of rexruntime.dll)
```

Phase reports (all in `private/`):

| Phase | Report | Result |
| --- | --- | --- |
| clone | `l52-clean-clone-clone-20261031-150404.txt` | done; 26 submodules; 15 of 15 libmspack links |
| sdk | `l52-clean-clone-sdk-20261031-150723.txt` | done; 1431 build actions; install; version checks |
| codegen | `l52-clean-clone-codegen-20261031-151344.txt` | done; 67 files, 0 mismatches |
| host | `l52-clean-clone-host-20261031-151427.txt` | done; 104 steps + helper; path scan clean |

Console logs: `l52-clone-console.log`, `l52-sdk-console.log`, `l52-sdk-ctest.log`,
`l52-codegen-console.log`, `l52-host-console.log`, `l52-host-tests-build.log`,
`l52-host-tests.log`, `l52-astgrep.log`, `l52-pathscan.log`, `l52-parsecheck.log`.
Every phase and test ran once; no script fix and no rerun was needed. (The first
parse-check invocation passed the file list as one string and was repeated with an
array; it parsed nothing the first time.)

## Acceptance

| # | Check | Result |
| --- | --- | --- |
| 1 | Clone pins | **PASS.** Project HEAD = worktree HEAD `737b6fcf`. Gitlink `851f03b`; SDK HEAD `851f03b`, `describe --exact-match` = `v0.20.0.5` (annotated tag object), origin `2FED/rexglue-sdk`. `sdl3` = `422c186`, origin `2FED/SDL`, branch `mcla/xinput-hotplug-poll`. 26 recursive submodules initialized (1 SDK + 25 nested), none `-`/`+`/`U`; all 25 nested SHAs equal those of the l48 (v0.20.0.4) clone `private/l48-src`. 15 of 15 libmspack links materialized (SDK submodule then shows ` M`, as before). |
| 2 | Version | **PASS.** `rexglue --version` = `0.20.0.5`; `rexglueConfigVersion.cmake` `set(PACKAGE_VERSION "0.20.0.5")`. Host CMake accepted `REXSDK_VERSION=0.20.0.5`. Embedded identity: `rexruntime.dll` `rexglue-v0.20.0.5-win-amd64-Release@20261031_1507`, `SDL-3.5.0-422c186c0`; `mcla.exe` `[rexglue-v0.20.0.5-Release]`. |
| 3 | SDK ctest (clean build) | **PASS.** `100% tests passed, 0 tests failed out of 1912` (ppc 1463, unit 449); 4 skipped: BitStream Write non-byte-aligned / byte-aligned / preserves surrounding bits / 16-bit value (the known four). SDK build: 1431 actions; 504 `warning:` lines, the same count as the l43 and l48 SDK builds (third-party code). |
| 4 | Codegen | **PASS.** `rexglue codegen` from the clean SDK: 67 files, 133,916,828 bytes, all SHA-256 equal to the worktree's `generated/default` (67 files), 0 missing/extra/differing. |
| 5 | Host + launcher build and tests | **PASS.** Volunteer options: `MCLA_TEST_BUILD_EXPIRY=ON`, `MCLA_RAW_TIMER_DEFAULT_ON=ON`, `MCLA_BUILD_IMAGE_CAPTURE_TESTS=ON`, `MCLA_BUILD_DIAGNOSTIC_TESTS=ON`, `MCLA_EXPERIMENTAL_F10_WGC=OFF`, Windows SDK 10.0.26100 cppwinrt, `-ffile-prefix-map` for the clone and the SDK install (both slash forms), `/pdbaltpath:%_PDB%`, `REXSDK_VERSION=0.20.0.5`. 104 steps + helper (2), then 115 steps for the 36 tests, launcher and fake helper. 0 compiler warnings, 0 errors; configure prints only the usual unused `CMAKE_C_COMPILER`/`CMAKE_C_FLAGS` notice. `mcla.exe` not relinked by the test build (hash unchanged). 36/36 executables return 0 on the first run (the known "bounded observed return" image-client timing flake did not occur). Every suite summary line equals the l49 run (`private/l49-host-tests-rerun.log`) except `mcla_photo_capture_test`: 214 checks (l49: 204), from `2a09c5a` task(M9-006): keep the bounded pipeline wait always on. Image client 57 (fake helper UPPERCASE `930536C1...6927`, fake child only, no capture), adapter policy 78 with `config/mcla.toml.example`, wine path policy 208, UI 995, controller 411. |
| 6 | ast-grep | **PASS.** `scan` exit 0, no findings; `test` 3 passed, 0 failed. |
| 7 | KI-022 resync linked | **PASS.** `mcla.exe` imports `?RequestWheelEffectResync@sdl@input@rex@@YAXPEBD@Z` from `rexruntime.dll`; `rexruntime.dll` exports it (ordinal 5065). |
| 8 | Always-on pipeline wait compiled in | **PASS.** `mcla.exe` contains `MCLA pipeline wait: always on (2 s per wait, 15 s per minute)` once, plus the `disabled by option`, `unavailable in this SDK`, `set by the user` and `could not set` messages, and the names `mcla_pipeline_wait_always` and `d3d12_async_pipeline_wait_always`. `rexgpu-xenos.dll` contains `d3d12_async_pipeline_wait_always` and `d3d12_livery_diag`. (String presence only; the log line itself needs a run.) |
| 9 | Artifacts | Table below. |
| 10 | Path strings | **PASS (one benign machine-code hit).** ASCII and UTF-16LE, case-insensitive, any offset, for `c:\bdu`, `c:/bdu`, `\users\`, `/users/`, `mcla-recomp`, `worktrees`, `l52`, `l51`, `l50`, `l49`, `l48`, `l43`, `l34-src` over the seven input files (`private/l52-pathscan.log`): 0 hits except one `L52` in `mcla.exe` at file offset `0xE1015C`, bytes `B8 4C 35 32 82` = `mov eax, 0x8232354C`, inside `.text` (recompiled guest address constant). The same instruction is in the l49 `mcla.exe` (offset `0xE0F66B`); not a path. The host phase's own scan of the five built executables was clean. |

## Artifacts (private/l52-build)

| File | Bytes | SHA-256 | vs l49 |
| --- | --- | --- | --- |
| `mcla.exe` | 53093376 | `823BFBA4A073ED49C4672180D107ED401106B682D44803B97BD7EA17E57B4C81` | new (always-on pipeline wait host option, SDK 0.20.0.5); +4608 bytes |
| `MCLA-Launcher.exe` | 1373696 | `15CBDA5A2F4F89811EA80B2C5AF3016890C24297211D958F1D71D179ECC07DC9` | rebuilt, same size |
| `mcla_crash_handler.exe` | 211968 | `4533FF9C1FA9AAEF248536605E29840700733B81E293034236D953C883F962E7` | rebuilt, same size |
| `mcla_diagnostic_image_helper.exe` | 60928 | `22CEB033A2EAA4A146926DCF23610DCA2BD6D8B5BE39B0ADFDC1C5CBBC171E86` | rebuilt, same size |
| `rexruntime.dll` | 10984960 | `F02510EFE2F7F820F88B20F5F1B5F715943D5BBFDB75599B3DCF2591653B57C1` | new SDK build, same size |
| `rexgpu-xenos.dll` | 3026432 | `1B81BD86D7DCA6712E8CE02E04DD604B70D81DE9C56B81AA6FB6220B4AD773FB` | new (pipeline wait + livery diagnostics), +29184 bytes |
| `TracyClient.dll` | 246784 | `7BDF8B816041BBD47420EA6C04EA2C036508F5898164B3B9CC6872003CB814B6` | rebuilt from the clone, same size |

The three DLLs are byte-identical to `private/l52-sdk-install/bin`. Also built:
`Launch-MCLA.exe` `509D8312...33E4` (411136 bytes), `rexglue.exe` `77BDD554...4EF0`
(4112896 bytes), fake helper `930536C1...6927`. `MCLA-Export-Metadata.exe` is not a
target of this tree (host report "missing", expected).

## Deviations

1. Clone HEAD `737b6fc` instead of the briefed `63d61f5` (docs-only commit on top; see Inputs).
2. `cmake --install` registered `private/l52-sdk-install` in the CMake user package
   registry (HKCU), as for l43/l48. The host build uses an explicit `rexglue_DIR`.
3. `vswhere.exe is not recognized` appears once per dev-shell launch (known noise).

## Limits

- Build and unit tests only. Nothing was launched in-game; no physical or owner
  check is claimed. The always-on wait is proven present by strings and the photo
  capture unit tests, not by a run.
- The clone came from the local worktree's committed branch, not from a GitHub
  copy of the project repository. The SDK and all submodules came from GitHub.
- Same machine and toolchain as l48/l49; output is not bit-identical across checkout
  paths (anonymous-namespace names, link timestamps).
