# M9-001 clean-clone reproducibility on SDK v0.20.0.4 (l48, 2026-10-30)

Build-verification subagent report, for review by the primary agent. Nothing
was launched: no `mcla.exe`, no `MCLA-Launcher.exe`, no `Launch-MCLA.exe`, no
game. Only `rexglue.exe` (`--version`, `codegen`), SDK `ctest`, the 36 host unit
test executables and the fake image helper (started by the image client test)
ran. No save, profile or key was opened. No git write command was run in the
project worktree. Spark: not used (deterministic clone/build/test gates; the
only analysis is hash and byte comparison that a script answers exactly).

Exit criterion checked: "a clean clone builds the volunteer artifacts" for the
pinned SDK, this time including a fresh SDK build from the clone.

## Inputs

| Item | Value |
| --- | --- |
| Project | branch `claude/admiring-jang-24d485`, cloned at `0e447a7b931315cce3de5c56c877c3d9f7800c60` (task(M9-005): pin ReXGlue SDK v0.20.0.4 (daa1ae5) with the wheel FFB resync), on top of `ea16801` (docs) and `521178b` (KI-022 host resync signals, T300 L2/R2 default 9,8). |
| SDK pin | gitlink `daa1ae5f751a55461a5f57e7a42677b6210240ed`, `https://github.com/2FED/rexglue-sdk.git`, branch `mcla/mcla-r-hotfixes`, annotated tag `v0.20.0.4` |
| SDL | `thirdparty/sdl3` = `422c186c0a272bf6f255b2f4537a726f804e924b` from `https://github.com/2FED/SDL.git` (`mcla/xinput-hotplug-poll`), unchanged since v0.20.0.3 |
| Toolchain | VS 2022 Build Tools CMake/Ninja, LLVM clang 20.1.8 |
| Script | `private/l48-clean-clone.ps1` (SHA-256 `ECCBEB8E...30026E`), a copy of `l46-clean-clone.ps1` with l48 names, tag/version `v0.20.0.4`/`0.20.0.4`, the SDK phase enabled again (output `l48-sdk-b`, `l48-sdk-o`, install `l48-sdk-install`) and `l48`/`l47` added to the host path scan. Run per phase with PowerShell 7.4.22 (`private/tools/pwsh`). |
| Game input | `private/p1/game/default.xex`, SHA-256 checked by the script (`C386F400...A432`, M1-010) |

## Commands

```
pwsh -NoProfile -ExecutionPolicy Bypass -File private\l48-clean-clone.ps1 -SdkSha daa1ae5f751a55461a5f57e7a42677b6210240ed -Phase clone
... -Phase sdk
... -Phase codegen
... -Phase host
private\l48-sdk-ctest.ps1        (ctest -C Release -j 8 --output-on-failure in private/l48-sdk-b, VS dev shell)
private\l48-build-tests.ps1      (l41_targets.txt + mcla_wine_path_policy_test, mcla_internal_launcher, fake helper)
private\l48-run-tests.ps1        (each *_test.exe in private/l48-build; image client: fake helper + UPPERCASE SHA-256; adapter policy: config/mcla.toml.example from the clone)
ast-grep scan ; ast-grep test    (in private/l48-src, ast-grep 0.50.0)
llvm-objdump -p                  (imports of mcla.exe, exports of rexruntime.dll)
```

Phase reports (all in `private/`):

| Phase | Report | Result |
| --- | --- | --- |
| clone | `l48-clean-clone-clone-20261030-144304.txt` | done; 26 submodules; 15 of 15 libmspack links |
| sdk | `l48-clean-clone-sdk-20261030-144604.txt` | done; 1431 build actions; install; version checks |
| codegen | `l48-clean-clone-codegen-20261030-145155.txt` | done; 67 files, 0 mismatches |
| host | `l48-clean-clone-host-20261030-145255.txt` | done; 104 steps + helper; path scan clean |

Console logs: `l48-clone-console.log`, `l48-sdk-console.log`, `l48-sdk-ctest.log`,
`l48-codegen-console.log`, `l48-host-console.log`, `l48-host-tests-build.log`,
`l48-host-tests.log`, `l48-astgrep.log`. Every phase ran once; no script fix was
needed. The first `l48-sdk-ctest.ps1` attempt did not start ctest (a backslash in the
VS path was lost when the helper was written); the helper was rewritten and ctest
run once, see row 3.

## Acceptance

| # | Check | Result |
| --- | --- | --- |
| 1 | Clone pins | **PASS.** Project HEAD = worktree HEAD `0e447a7b`. Gitlink `daa1ae5`; SDK HEAD `daa1ae5`, `describe --exact-match` = `v0.20.0.4` (annotated tag object), origin `2FED/rexglue-sdk`. `sdl3` = `422c186`, origin `2FED/SDL`, branch `mcla/xinput-hotplug-poll`. 26 recursive submodules initialized (1 SDK + 25 nested), none `-`/`+`/`U`; all 25 nested SHAs equal those of the l46 (v0.20.0.3) clone `private/l46-src`. 15 of 15 libmspack links materialized (SDK submodule then shows ` m`, as before). |
| 2 | Version | **PASS.** `rexglue --version` = `0.20.0.4`; `rexglueConfigVersion.cmake` `set(PACKAGE_VERSION "0.20.0.4")`. Host CMake accepted `REXSDK_VERSION=0.20.0.4` against `MCLA_REXGLUE_VERSION`. Embedded identity: `rexruntime.dll` `rexglue-v0.20.0.4-win-amd64-Release@20261030_1446`, `SDL-3.5.0-422c186c0`; `mcla.exe` `[rexglue-v0.20.0.4-Release]`. |
| 3 | SDK ctest (clean build) | **PASS.** `100% tests passed, 0 tests failed out of 1900`; 4 skipped: BitStream Write 16-bit value / preserves surrounding bits / non-byte-aligned / byte-aligned (the known four). SDK build: 1431 actions; 504 compiler warnings, the same count as the l43 SDK build (third-party code; host build has none). |
| 4 | Codegen | **PASS.** `rexglue codegen` from the clean SDK: 67 files, 133,916,828 bytes, all SHA-256 equal to the worktree's `generated/default` (67 files), 0 missing/extra/differing. |
| 5 | Host + launcher build and tests | **PASS.** Volunteer options: `MCLA_TEST_BUILD_EXPIRY=ON`, `MCLA_RAW_TIMER_DEFAULT_ON=ON`, `MCLA_BUILD_IMAGE_CAPTURE_TESTS=ON`, `MCLA_BUILD_DIAGNOSTIC_TESTS=ON`, `MCLA_EXPERIMENTAL_F10_WGC=OFF`, Windows SDK 10.0.26100 cppwinrt, `-ffile-prefix-map` for the clone and the SDK install (both slash forms), `/pdbaltpath:%_PDB%`, `REXSDK_VERSION=0.20.0.4`. 104 steps + helper, then 115 steps for the 36 tests, launcher and fake helper. 0 compiler warnings, 0 errors; configure prints only the usual unused `CMAKE_C_COMPILER`/`CMAKE_C_FLAGS` notice. `mcla.exe` not relinked by the test build (hash unchanged). 36/36 executables return 0. The last summary line of every suite is identical to the worktree dev run of the same sources (`private/l47-tests-resync4.log`): menu state 482, image client 57 (fake helper UPPERCASE `FDFE6F5C...6872`, fake child only, no capture), adapter policy 78 with `config/mcla.toml.example`, wine path policy 208; the 8 SKIP lines (Win32 1314 symlink) are identical. |
| 6 | ast-grep | **PASS.** `scan` exit 0, no findings; `test` 3 passed, 0 failed. |
| 7 | KI-022 fix active | **PASS.** `mcla.exe` imports `?RequestWheelEffectResync@sdl@input@rex@@YAXPEBD@Z` from `rexruntime.dll` (import table, `rexruntime.dll` block); `rexruntime.dll` exports it (ordinal 5065). So the host was compiled with `MCLA_HAS_WHEEL_EFFECT_RESYNC=1`, not the no-op fallback. |
| 8 | Artifacts | Table below. |
| 9 | Path strings | **PASS.** ASCII and UTF-16LE, case-insensitive, any offset, for `c:\bdu`, `c:/bdu`, `\users\`, `/users/`, `mcla-recomp`, `worktrees`, `l48`, `l47`, `l46`, `l43`, `l34-src`: 0 hits in all seven files (`private/l48-pathscan.log`). The previous `L45`/`l46` false positives inside the old `rexruntime.dll` machine code are gone with the new DLL. |

## Artifacts (private/l48-build)

| File | Bytes | SHA-256 | vs l46 |
| --- | --- | --- | --- |
| `mcla.exe` | 53087744 | `6FEC02D0310589AD4210D8D54342C629A4681F5B3C7A265779EC1480567FE3D0` | new (KI-022 host resync signals, L2/R2 9,8, SDK 0.20.0.4); same size |
| `MCLA-Launcher.exe` | 1364480 | `98522D9B81B7016FAC1D00DCCFF8252328695C4F8F999A321BD9DADE83E9DC26` | 202 bytes: 200 in the two anonymous-namespace names (`?A0x...`, derived from the checkout path), 2 COFF timestamp |
| `mcla_crash_handler.exe` | 211968 | `466B2C92BFFD324364FA40398EA00C2E7378C45A217A228E6D1C883C44ED761A` | 2 bytes (timestamp) |
| `mcla_diagnostic_image_helper.exe` | 60928 | `4A966260AC8A7B8DD85B887B57486ED3732B86043EFC74B3C01A423245276CA2` | 11 bytes (same anonymous-name/timestamp pattern as l43) |
| `rexruntime.dll` | 10984960 | `82E596CDF23CB2635AC12554B29B83ECD986B4292A8689085DDE2F42148168EE` | new SDK (+19456 bytes; same size as the l47 dev-SDK DLL) |
| `rexgpu-xenos.dll` | 2997248 | `23B7FE2367FE7D6C778D917D164A338FC5C33FC0FEC911B6245B5850AEE154D2` | rebuilt from the clone, same size |
| `TracyClient.dll` | 246784 | `080012FD01B28DDDE2554F7FD6F3501A3F4528CE6DBB8FEBB13BA83B40568750` | rebuilt from the clone, same size |

The three DLLs are byte-identical to `private/l48-sdk-install/bin`. Also built:
`Launch-MCLA.exe` `EE7A4D48...6A68` (411136 bytes), `rexglue.exe` `17BC11F4...CA2B`
(4112896 bytes), fake helper `FDFE6F5C...6872`. `MCLA-Export-Metadata.exe` is not a
target of this tree (host report "missing", expected).

## Deviations

1. `cmake --install` registered `private/l48-sdk-install` in the CMake user package
   registry (HKCU), as for l43. The host build uses an explicit `rexglue_DIR`.
2. `vswhere.exe is not recognized` appears once per dev-shell launch (known noise).
3. The SDK console has 504 `warning:` lines, identical in count to l43's clean SDK
   build; not inspected individually.

## Limits

- Build and unit tests only. Nothing was launched in-game; no physical or owner
  check is claimed. The KI-022 resync is proven only by unit tests with a fake
  haptic layer and by the import check above, not on a T300.
- The clone came from the local worktree's committed branch, not from a GitHub
  copy of the project repository. The SDK and all submodules came from GitHub.
- Same machine and toolchain as l43/l46; output is not bit-identical across checkout
  paths (anonymous-namespace names, link timestamps).
