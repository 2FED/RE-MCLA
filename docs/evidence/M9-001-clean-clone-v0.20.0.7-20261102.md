# M9-001 clean-clone reproducibility on SDK v0.20.0.7 (l65, 2026-11-02)

Build-verification subagent report, for review by the primary agent. Nothing was launched:
no `mcla.exe`, no `MCLA-Launcher.exe`, no `Launch-MCLA.exe`, no game. Only `rexglue.exe`
(`--version`, `codegen`), SDK `ctest`, the 45 host unit test executables and the fake image
helper (started by the image client test) ran. No save, profile, key or LocalAppData file was
opened. No git write command was run in the project repository. The HKCU CMake package
registry was not written (install ran with the Windows directories removed from PATH;
`reg query` before and after identical). Spark: not used (deterministic clone/build/test
gates; the only analysis is hash and byte comparison that a script answers exactly).

Exit criterion checked: "a clean clone builds the volunteer artifacts" for the newly pinned
SDK, including a fresh SDK build from the clone, with the l56 volunteer options plus
`MCLA_M10_SPEEDUP_DEFAULTS=ON` and `MCLA_PREWARM_SEED_DEFAULT=ON`.

## Inputs

| Item | Value |
| --- | --- |
| Project | branch `main`, cloned from the main checkout at `1ff00dcea2f5a596401c444eb0bdaaa7d2e27c78` (task(M10): pin ReXGlue SDK v0.20.0.7 (43577c8)); clone HEAD = checkout HEAD. 30 commits since l56 (`85128b6`). |
| SDK pin | gitlink `43577c89c7f5409ce5b2501d5d183b0f00bcbe04`, `https://github.com/2FED/rexglue-sdk.git`, branch `mcla/mcla-r-hotfixes`, annotated tag `v0.20.0.7` (tag object `4d41f2f1`, peeled `43577c8`, checked with `git ls-remote`; branch head = `43577c8`). Seven commits since v0.20.0.6: `62cd66d` WRM event wake + replay batched wake, `141e13f` read-back counters, GPU timing and elision of unread copies, `9d8ede4` storage pipeline creation thread count / waits for all creation threads, `561a737` KI-047 targeted read-back wait with early submission, `e57fd88` + `8081f63` watch census, bridging, upload-ahead (+ review fixes), `43577c8` diag counts elided slots as unwritten. |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b` from `https://github.com/2FED/SDL.git`, unchanged |
| Toolchain | VS 2022 Build Tools CMake/Ninja, LLVM clang 20.1.8 |
| Script | `private/l65-clean-clone.ps1` (SHA-256 `0D7F0ED8...B573C`), generated from `l56-clean-clone.ps1` by `private/l65-work/mk_clone.py`: l65 names (`l65-src`, `l65-build`), SDK build/output/install under the distinct names `l65v-sdk-b`, `l65v-sdk-o`, `l65v-sdk-install` (the `l65-sdk-*` directories belong to the earlier dev build and were not touched), tag/version `v0.20.0.7`/`0.20.0.7`, SDK flags as l56 (D3D12 only, Vulkan OFF), install with the Windows directories removed from PATH (as l59vk/l63vk), host options of l56 **plus** `-DMCLA_M10_SPEEDUP_DEFAULTS=ON -DMCLA_PREWARM_SEED_DEFAULT=ON -DMCLA_DEV_OVERRIDES=OFF`, path-scan needles `l65` down to `l48`, `l43`, `l34-src`, `sdk-wt`. Run per phase with PowerShell 7.4.22 (`private/tools/pwsh`). |
| Game input | `private/p1/game/default.xex`, SHA-256 checked by the script (`C386F400...A432`, M1-010) |

## Commands

```
pwsh -NoProfile -ExecutionPolicy Bypass -File private\l65-clean-clone.ps1 -SdkSha 43577c89c7f5409ce5b2501d5d183b0f00bcbe04 -Phase clone
... -Phase sdk
... -Phase codegen
... -Phase host
private\l65-work\l65-sdk-ctest.ps1    (ctest -C Release -j 8 --output-on-failure in private/l65v-sdk-b)
private\l65-work\l65-build-tests.ps1  (l41_targets.txt + l56 extras + every *_test target the tree defines)
private\l65-work\l65-run-tests.ps1    (each *_test.exe in private/l65-build)
ast-grep scan ; ast-grep test         (in private/l65-src, ast-grep 0.50.0)
private\l65-work\l65-pathscan.py      (seven input files) + l65-sect.py (section of each hit)
private\l65-work\l65-strings.py       (default-on strings)
```

Phase reports (all in `private/`): `l65-clean-clone-clone-20261102-211718.txt`,
`l65-clean-clone-sdk-20261102-211947.txt`, `l65-clean-clone-codegen-20261102-212451.txt`,
`l65-clean-clone-host-20261102-212516.txt`. Console and test logs in `private/l65-work/`:
`l65-clone-console.log`, `l65-sdk-console.log`, `l65-sdk-ctest.log`, `l65-sdk-ctest2.log`,
`l65-codegen-console.log`, `l65-host-console.log`, `l65-host-tests-build.log`,
`l65-host-tests.log`, `l65-astgrep.log`, `l65-strings.log`, `l65-pathscan.log`,
`l65-parsecheck.log`, `reg-before.txt`, `reg-after.txt`. Every phase ran once; SDK ctest
needed one rerun (row 3).

## Acceptance

| # | Check | Result |
| --- | --- | --- |
| 1 | Clone pins | **PASS.** Project HEAD = checkout HEAD `1ff00dce`. Gitlink `43577c8`; SDK HEAD `43577c8`, `describe --exact-match` = `v0.20.0.7`, origin `2FED/rexglue-sdk`. `sdl3` = `422c186`, origin `2FED/SDL`. 26 recursive submodules (1 SDK + 25 nested), none `-`/`+`/`U`; all 25 nested SHAs equal those of the l56 clone. 15 of 15 libmspack links materialized (the only reason the SDK submodule reads `-dirty` in the clone). |
| 2 | Version | **PASS.** `rexglue --version` = `0.20.0.7`; `rexglueConfigVersion.cmake` declares `0.20.0.7`; `REXSDK_VERSION=0.20.0.7` in the host cache. |
| 3 | SDK ctest (clean build) | **PASS on the rerun.** First run: 2005/2006, `unit.Manifest: parses sdk_version when present` failed with `remove_all: The process cannot access the file because it is being used by another process` (the known manifest temp-directory race under `-j 8`; not a code failure). Full rerun: `100% tests passed, 0 tests failed out of 2006`; 4 skipped: the known four BitStream Write cases. SDK build: 1442 actions, 503 `warning:` lines (third-party code; l56 also 503). |
| 4 | Codegen | **PASS.** 67 files, 133,916,828 bytes, all SHA-256 equal to the main checkout's `generated/default` (67 files), 0 missing/extra/differing. |
| 5 | Host + launcher build and tests | **PASS.** CMakeCache: `MCLA_TEST_BUILD_EXPIRY=ON`, `MCLA_PERF_SPLIT_DEFAULT=ON`, `MCLA_LIVERY_DIAG2_DEFAULT=ON`, `MCLA_READBACK_SYNC_COMPOSITE_DEFAULT=ON`, `MCLA_M10_SPEEDUP_DEFAULTS=ON`, `MCLA_PREWARM_SEED_DEFAULT=ON`, `MCLA_DEV_OVERRIDES=OFF`, `MCLA_EXPERIMENTAL_F10_WGC=OFF`, plus raw timer ON, image/diagnostic tests ON, Windows SDK 10.0.26100 cppwinrt, prefix maps, `/pdbaltpath:%_PDB%`. `build.ninja` carries `MCLA_M10_SPEEDUP_DEFAULTS=1` and no `MCLA_PREWARM_SEED_DEFAULT=0` / `MCLA_DEV_OVERRIDES=1`. 113 steps + helper, then 137 steps for 50 targets (45 `*_test` targets defined by the tree, the launcher, the fake helper). 0 compiler warnings, 0 errors (only the usual unused `CMAKE_C_COMPILER`/`CMAKE_C_FLAGS` notice). **45/45 executables return 0** on the first run (l56's 39 + `mcla_dev_overrides_test`, `mcla_native_refcount_test`, `mcla_shader_store_seed_test`, `mcla_launcher_shader_seed_win_test`, `mcla_vulkan_device_select_test`, `mcla_m10_defaults_test` (891 checks)); every suite summary line equals the l65 dev host run (`private/l57-work/tests-l65.log`). |
| 6 | ast-grep | **PASS.** `scan` exit 0, no findings; `test` 3 passed, 0 failed. |
| 7 | Default-on code compiled in | **PASS** (string presence only; log lines need a run). `mcla.exe`: `MCLA m10 default: {}={}`, `MCLA m10 defaults (startup)`, `MCLA m10 defaults (graphics plugin)`, `MCLA m10 defaults: off`, cvar `mcla_m10_speedup_defaults`; `MCLA readback sync: fix ON, build default {}=true` and `override file {} found: {}{}` with `mcla-dev-overrides.txt` (the l55 single-line handling, as in l56); perf split / livery diag2 build-default lines; `MCLA pipeline wait: always on`; `MCLA shader store at startup`; `mcla_native_d3d_refcount`. The `MCLA dev overrides: ... read` / `MCLA dev override:` strings are **absent** (no owner A/B file reader). `MCLA-Launcher.exe`: UTF-16 `store-seed.txt`, `MCLA store seed: source=`; no `mcla-dev-overrides.txt` (launcher reads no override file). `rexgpu-xenos.dll`: `d3d12_readback_sync_targeted`, `d3d12_readback_elide_unread`, `d3d12_wrm_event_wake`, `d3d12_replay_batched_wake`, `d3d12_readback_gpu_timing`, `prev_unwritten`; `rexruntime.dll`: `memory_watch_census`. |
| 8 | Path strings | **PASS (benign machine-code hits only).** ASCII and UTF-16LE, case-insensitive, for `c:\bdu`, `c:/bdu`, `\users\`, `/users/`, `mcla-recomp`, `worktrees`, `l65`..`l48`, `l43`, `l34-src`, `sdk-wt` over the seven input files. Hits, each mapped to its PE section: `mcla.exe` `.text` `l65` 0xF97A38, `L62` 0xE2D54E, `L60` 0xD4F0C9, `l58` 0x10EE743, `l54` 0xF1B304, `L52` 0xE2CEBC (all immediates of recompiled guest addresses `0x82xxxxxx`, e.g. `C7 81 50 01 00 00 6C 36 35 82`), `mcla.exe` `.pdata` `l64` 0x325528C (unwind-table RVA bytes); `MCLA-Launcher.exe` `.text` `L60` 0xACE57 (`F3 41 0F 7F 4C 36 30` = `movdqu [r14+rsi+0x30], xmm1`); `rexruntime.dll` `.text` `l65` 0x3CCF64 (`E8 6C 36 35 00`, a call rel32). No path strings. |
| 9 | Expiry marker | **PASS.** UTF-16 `MCLA test-build admission` once in `mcla.exe`. |
| 10 | Registry | **PASS.** `HKCU\Software\Kitware\CMake\Packages\rexglue` identical before and after (no `l65v-sdk-install` entry added). |

## Artifacts (private/l65-build, copied to private/l65-vol-input)

| File | Bytes | SHA-256 | vs l56 |
| --- | --- | --- | --- |
| `mcla.exe` | 53254144 | `422A20A604ABF15986CCCCD13C027467EF8BFBF861519C6D3774BC420F02E499` | +59392 |
| `MCLA-Launcher.exe` | 1442304 | `AA2A629EEC6583DDC44131CC0365D16D8ECC85148A354E9356DE91958B711C5B` | +68608 (store seeding) |
| `mcla_crash_handler.exe` | 211968 | `D2A4E2EE219319A43ED0D4A8FCD155CA375DDBA96118AB87C7E1150D83D20A8F` | rebuilt, same size |
| `mcla_diagnostic_image_helper.exe` | 60928 | `3712D81EB3938414DC64651F20160BE9FE3219CB63F67F56C816DC95E966E5F1` | rebuilt, same size |
| `rexruntime.dll` | 11018240 | `991AEED22913B485FBB1641F2CF6820521712E79B9EE342833C09A685C5FA41D` | +22528 |
| `rexgpu-xenos.dll` | 3210240 | `10153D6F91831C0DDB57D4897E1ED7DBE0EE41D7A7516A1D34FC6C997B828BC7` | +74240 |
| `TracyClient.dll` | 246784 | `DC4AD050012AB61C92C452BDCA0EF3452730B67ADBA947251309EB91769AEB6E` | rebuilt, same size |

The three DLLs are byte-identical to `private/l65v-sdk-install/bin`. Also built:
`Launch-MCLA.exe` `2DE732CD...091D1` (411136 bytes), `rexglue.exe` `4167C8EF...C3915`
(4112896 bytes), fake helper `0013EA1F74BDA014...`. `MCLA-Export-Metadata.exe` is not a target of
this tree (host report "missing", expected).

## Deviations

1. SDK install directories named `l65v-*` because `private/l65-sdk-*` already hold the dev build
   of the same SDK commit from the linked worktree (different paths, so different DLL hashes:
   dev `rexgpu-xenos.dll` `1eb95425...` vs clean clone `10153D6F...`).
2. SDK install ran with the Windows directories removed from PATH (l59vk/l63vk method), so the
   HKCU CMake package registry was not written, unlike l56.
3. SDK ctest needed one rerun for the known manifest temp-directory race.
4. The host test target list is discovered from the configured tree (all `*_test` targets) rather
   than an explicit list; it yields the 45 executables of the l65 dev build.

## Limits

- Build and unit tests only. Nothing was launched in-game; no physical or owner check is
  claimed. The exact l65 volunteer switch combination (l63 "all" minus bridging, upload-ahead,
  census and GPU timing) has never run.
- The clone came from the local committed `main`, not from a GitHub copy of the project
  repository. The SDK and all submodules came from GitHub.
- Output is not bit-identical across checkout paths (anonymous-namespace names, link timestamps).
