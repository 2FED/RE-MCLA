# M9-001 clean-clone reproducibility on SDK v0.20.0.8 (l67, 2026-11-03)

Build-verification subagent report, for review by the primary agent. Nothing was launched:
no `mcla.exe`, no `MCLA-Launcher.exe`, no `Launch-MCLA.exe`, no game. Only `rexglue.exe`
(`--version`, `codegen`), SDK `ctest`, the 47 host unit test executables and the fake image
helper (started by the image client test) ran. No save, profile, key or LocalAppData file was
opened (the unit tests create and keep their own synthetic fixtures under `%TEMP%`, as in every
earlier run). No git write command was run in the project repository. The HKCU CMake package
registry was not written (install ran with the Windows directories removed from PATH;
`reg query` before and after identical in content). Spark: not used (deterministic
clone/build/test gates; the only analysis is hash and byte comparison that a script answers
exactly).

Exit criterion checked: "a clean clone builds the volunteer artifacts" for the newly pinned
SDK, including a fresh SDK build from the clone, with the l65 volunteer options
(`MCLA_M10_SPEEDUP_DEFAULTS=ON`, `MCLA_PREWARM_SEED_DEFAULT=ON`, `MCLA_DEV_OVERRIDES=OFF`).

## Inputs

| Item | Value |
| --- | --- |
| Project | branch `main`, cloned from the main checkout at `e551fb6e197eb3775295185df2933c3d94f28517` (task(M10): pin ReXGlue SDK v0.20.0.8 (c63895c): KI-065 content logs and transaction indicator); clone HEAD = checkout HEAD at clone time. 25 commits since l65 (`1ff00dc`). The checkout later moved to `1ced4c4`, a docs-only commit (`docs/known-issues.md` and the KI-065 evidence); no build input changed. |
| SDK pin | gitlink `c63895c8ed53748d04de91403c467ece2e8f6fa8`, `https://github.com/2FED/rexglue-sdk.git`, branch `mcla/mcla-r-hotfixes`, annotated tag `v0.20.0.8` (tag object `562f19cf`, peeled `c63895c`, checked with `git ls-remote`; branch head = `c63895c`). New since v0.20.0.7: content-layer WARN logs (`[content]`, paced) and the save-transaction indicator export `rex_xam_content_transactions_active` (KI-065). |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b` from `https://github.com/2FED/SDL.git`, unchanged |
| Toolchain | VS 2022 Build Tools CMake/Ninja, LLVM clang 20.1.8 |
| Script | `private/l67-clean-clone.ps1` (SHA-256 `C52855FBE48E78EDDC38A681963F5DF46D469BBD47118B1DD2B8C750B1966AE7`), generated from `l65-clean-clone.ps1` by `private/l67v-work/mk_clone.py`: l67 names (`l67-src`, `l67-build`), SDK build/output/install under the distinct names `l67v-sdk-b`, `l67v-sdk-o`, `l67v-sdk-install` (the `l67-sdk-*` directories belong to the earlier dev build and were not touched), tag/version `v0.20.0.8`/`0.20.0.8`, SDK flags as l65 (D3D12 only, Vulkan OFF), install with the Windows directories removed from PATH, host options of l65 unchanged, path-scan needles `l67`, `l66` added. Run per phase with PowerShell 7.4.22 (`private/tools/pwsh`). |
| Game input | `private/p1/game/default.xex`, SHA-256 checked by the script (`C386F400...A432`, M1-010) |

## Commands

```
pwsh -NoProfile -ExecutionPolicy Bypass -File private\l67-clean-clone.ps1 -SdkSha c63895c8ed53748d04de91403c467ece2e8f6fa8 -Phase clone
... -Phase sdk
... -Phase codegen
... -Phase host
private\l67v-work\l67-sdk-ctest.ps1    (ctest -C Release -j 8 --output-on-failure in private/l67v-sdk-b)
private\l67v-work\l67-build-tests.ps1  (explicit list as l57_host_build_targets.ps1: l41 list + l56 extras
                                        + the eight M10 tests; cross-checked against every *_test target)
private\l67v-work\l67-run-tests.ps1    (each *_test.exe in private/l67-build)
ast-grep scan ; ast-grep test         (in private/l67-src, ast-grep 0.50.0)
private\l67v-work\l67-pathscan.py      (seven input files) + l67-sect.py (section of each hit)
private\l67v-work\l67-strings.py       (strings, mcla.exe import table, rexruntime.dll export table)
```

Phase reports (all in `private/`): `l67-clean-clone-clone-20261103-193149.txt`,
`l67-clean-clone-sdk-20261103-193456.txt`, `l67-clean-clone-codegen-20261103-194033.txt`,
`l67-clean-clone-host-20261103-194112.txt`. Console and test logs in `private/l67v-work/`:
`l67-clone-console.log`, `l67-sdk-console.log`, `l67-sdk-ctest.log`, `l67-codegen-console.log`,
`l67-host-console.log`, `l67-host-tests-build.log`, `l67-host-tests.log`, `l67-astgrep.log`,
`l67-strings.log`, `l67-pathscan.log`, `l67-sect.log`, `l67-parsecheck.log`, `reg-before.txt`,
`reg-after.txt`. Every phase and the SDK ctest ran once (no rerun needed).

## Acceptance

| # | Check | Result |
| --- | --- | --- |
| 1 | Clone pins | **PASS.** Project HEAD = checkout HEAD `e551fb6`. Gitlink `c63895c`; SDK HEAD `c63895c`, `describe --exact-match` = `v0.20.0.8`, origin `2FED/rexglue-sdk`. `sdl3` = `422c186`, origin `2FED/SDL`. 26 recursive submodules (1 SDK + 25 nested), none `-`/`+`/`U`; all 25 nested SHAs equal those of the l65 clone. 15 of 15 libmspack links materialized (the only reason the SDK submodule reads modified in the clone). |
| 2 | Version | **PASS.** `rexglue --version` = `0.20.0.8`; `rexglueConfigVersion.cmake` declares `0.20.0.8`; `REXSDK_VERSION=0.20.0.8` in the host cache. |
| 3 | SDK ctest (clean build) | **PASS, first run.** `100% tests passed, 0 tests failed out of 2014` (l65: 2006); 4 skipped: the known four BitStream Write cases. The manifest temp-directory race did not occur. SDK build: 1444 actions, 502 `warning:` lines (third-party code; l65 503). |
| 4 | Codegen | **PASS.** 67 files, 133,916,828 bytes, all SHA-256 equal to the main checkout's `generated/default` (67 files), 0 missing/extra/differing. |
| 5 | Host + launcher build and tests | **PASS.** CMakeCache: `MCLA_TEST_BUILD_EXPIRY=ON`, `MCLA_RAW_TIMER_DEFAULT_ON=ON`, `MCLA_PERF_SPLIT_DEFAULT=ON`, `MCLA_LIVERY_DIAG2_DEFAULT=ON`, `MCLA_READBACK_SYNC_COMPOSITE_DEFAULT=ON`, `MCLA_M10_SPEEDUP_DEFAULTS=ON`, `MCLA_PREWARM_SEED_DEFAULT=ON`, `MCLA_DEV_OVERRIDES=OFF`, `MCLA_EXPERIMENTAL_F10_WGC=OFF`, image/diagnostic tests ON, Windows SDK 10.0.26100 cppwinrt, prefix maps, `/pdbaltpath:%_PDB%`. `build.ninja` carries `MCLA_M10_SPEEDUP_DEFAULTS=1` and no `MCLA_PREWARM_SEED_DEFAULT=0` / `MCLA_DEV_OVERRIDES=1`. 113 steps + helper, then 141 steps for 52 targets (explicit list: 47 `*_test`, the launcher, the fake helper and the l41 tools; the tree defines exactly these 47 `*_test` targets, none unlisted). 0 compiler warnings, 0 errors (only the usual unused `CMAKE_C_COMPILER`/`CMAKE_C_FLAGS` notice). **47/47 executables return 0** on the first run (l65's 45 + `mcla_save_copy_guard_test` (43 checks) and `mcla_shared_read_copy_win_test`); `mcla_m10_defaults_test` 2412 checks (l65 891), `mcla_launcher_preferences_test` 2095 checks; every output line equals the l67 dev host run (`private/l67-work/l67-tests2.log`) except fixture paths and PIDs. `mcla.exe` hash unchanged by the test build. |
| 6 | ast-grep | **PASS.** `scan` exit 0, no findings; `test` 3 passed, 0 failed. |
| 7 | Default-on and new code compiled in | **PASS** (string/table presence only; log lines need a run). `mcla.exe`: `MCLA m10 default: {}={}`, both `MCLA m10 defaults (...)` summary formats, `MCLA m10 defaults: off`, `platform=` (2), ` wine_only=` and `wine_only`, the Wine-only cvar names `shared_memory_upload_ahead_pages` and `memory_watch_disarm_bridge_safe_gaps`, `shared_memory_hot_blocks` (never-written list); `MCLA diagnostics: save copy start` (1), `MCLA diagnostics: save copy skipped` (2), `save-copy-skipped.txt`; **import table: `rex_xam_content_transactions_active` from `rexruntime.dll`** (so the linker fallback is not used), and `rexruntime.dll` exports it; readback sync, perf split and livery diag2 build-default lines, `MCLA pipeline wait: always on`, `MCLA shader store at startup`, `mcla_native_d3d_refcount`, `MCLA graphics: video memory {} MB near budget` (the M10-007 line); `MCLA dev overrides: ... read` / `MCLA dev override:` **absent**. `MCLA-Launcher.exe`: UTF-16 ` (ignored ` and `file(s) in sessions` (KI-064), `store-seed.txt`, `MCLA store seed: source=`; no `mcla-dev-overrides.txt`. KI-057 (first run under Wine/Proton starts fullscreen) is logic, not a string: covered by `mcla_launcher_preferences_test` / `mcla_launcher_store_win_test` and confirmed only as built. `rexgpu-xenos.dll`: the four speed-up cvars, `d3d12_readback_gpu_timing`, `prev_unwritten`, `shared_memory_upload_ahead_pages`; `rexruntime.dll`: `memory_watch_census`, `memory_watch_disarm_bridge_safe_gaps`. |
| 8 | Path strings | **PASS (benign machine-code hits only).** ASCII and UTF-16LE, case-insensitive, for `c:\bdu`, `c:/bdu`, `\users\`, `/users/`, `mcla-recomp`, `worktrees`, `l67`..`l48`, `l43`, `l34-src`, `sdk-wt` over the seven input files. Hits, all in `.text`: `mcla.exe` `l67` 0x1083FEF (`BF 6C 36 37 82`, guest-address immediate), `l67` 0x1E9E5A0 (`66 0F 38 00 0D 6C 36 37 01` = `pshufb xmm1, [rip+disp32]`), `l66` 0x10111F3, `l65` 0xF9E858, `L62` 0xE3436E, `L60` 0xD55EE9, `l58` 0x10F5563, `l54` 0xF22124, `L52` 0xE33CDC (guest addresses `0x82xxxxxx`, as in l65); `MCLA-Launcher.exe` `L60` 0xAE857 (the l65 `movdqu` encoding). `rexruntime.dll`, `rexgpu-xenos.dll`, `TracyClient.dll`, crash handler and image helper: 0 hits (the l65 `.pdata` and `rexruntime.dll` hits are gone). No path strings. |
| 9 | Expiry marker | **PASS.** UTF-16 `MCLA test-build admission` once in `mcla.exe`. |
| 10 | Registry | **PASS.** `HKCU\Software\Kitware\CMake\Packages\rexglue` identical in content before and after (22 lines; no `l67v-sdk-install` entry). |

## Artifacts (private/l67-build, copied to private/l67-vol-input)

| File | Bytes | SHA-256 | vs l65 |
| --- | --- | --- | --- |
| `mcla.exe` | 53293056 | `1B4642487AC6684BF1054930F7BD6B8F1DD2021127BFD7B34C43EDB300D7B64C` | +38912 |
| `MCLA-Launcher.exe` | 1451520 | `77FC975A63C40793EDA2FDF3EC1308F7C70051ED8EF496B2750F66C72808750E` | +9216 |
| `mcla_crash_handler.exe` | 211968 | `79A62642AC5DB488DF2DECF9ADE547989F0AD2557776DAFD525D4B03256DCEE3` | rebuilt, same size |
| `mcla_diagnostic_image_helper.exe` | 60928 | `9506FE44ABCD95783301DF0450DE9E6693A89865DBC1E7AA48617914DE64457B` | rebuilt, same size |
| `rexruntime.dll` | 11064320 | `7695301AE6AC8296B2B38D45FDF359317AEA1D32054236791F38E1557748892B` | +46080 |
| `rexgpu-xenos.dll` | 3210240 | `1D081EB31000DA15855F9E2884875DC5CAB0B1E5F828AF74A7A2BACC7FABB862` | rebuilt, same size |
| `TracyClient.dll` | 246784 | `8BE2138700AB787C46304D29EA0B230FDA4920906076C69075817E3CF9540A1A` | rebuilt, same size |

The three DLLs are byte-identical to `private/l67v-sdk-install/bin`. Also built:
`Launch-MCLA.exe` `8868D8B9B83FFF2641F43BFE9BE2B369095BAFD84FC5A6F5B5BB559E1D6B1B24` (411136
bytes), `rexglue.exe` `E01FB52EDE33E8C2B20A2515C284D17FEDB2114923095056310A3DD31EC1D423` (4112896
bytes), fake helper `F6A13A2D8E4C6CF5...0105B`. `MCLA-Export-Metadata.exe` is not a target of this
tree (host report "missing", expected).

## Deviations

1. SDK install directories named `l67v-*` because `private/l67-sdk-*` already hold a dev build.
2. Host test targets use the explicit l57 list (52 targets) with a cross-check against the tree
   instead of l65's discovery; both give the same 47 test executables.
3. Work scripts and logs live in `private/l67v-work/` (`private/l67-work/` holds the dev build's).

## Limits

- Build and unit tests only. Nothing was launched in-game; no physical or owner check is
  claimed. The Wine-only defaults, KI-057, KI-064 and KI-065 code have no runtime result in l67.
- The clone came from the local committed `main`, not from a GitHub copy of the project
  repository. The SDK and all submodules came from GitHub.
- Output is not bit-identical across checkout paths (anonymous-namespace names, link timestamps).
