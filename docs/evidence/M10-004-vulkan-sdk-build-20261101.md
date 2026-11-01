# M10-004 preparation: Vulkan-enabled SDK build (l58vk, 2026-11-01) - DRAFT

Build by an Opus subagent on 2026-11-01; accepted by the primary agent. Status: the Vulkan-enabled SDK builds and passes ctest; the Vulkan backend has not run in the game. The host switch patch (`private/l58vk-work/M10-004-host-vulkan-switch.patch`) is NOT applied yet: it edits the dev-override whitelist that the M10-002 l58 work also edits, so it will be rebased onto that change and combined with a Vulkan rebuild of the l58 SDK for the Deck A/B.

Build subagent report for review by the primary agent. Nothing was launched:
no `mcla.exe`, no launcher, no game. Only CMake/Ninja/clang, the SDK ctest
executables and `rexglue.exe --version` ran. No git write anywhere (`git apply
--check` and `git diff --no-index` only). No download, no install outside
`private/l58vk-*`. Spark: not used (deterministic build/test; nothing to triage
that a script does not answer exactly).

Relates to KI-053 (Proton runs D3D12 through vkd3d instead of native Vulkan)
and M10-004 (Vulkan-enabled SDK and Deck A/B).

## Inputs

| Item | Value |
| --- | --- |
| SDK source | read-only `private/l56-src/third_party/rexglue-sdk`, HEAD `483458d2885419234a0729698f29a8850a052b55` = `v0.20.0.6` (the l56 clean clone; submodules `glslang`, `spirv-tools`, `spirv-headers`, `volk`, `vulkan-headers`, `vulkan-memory-allocator` already populated) |
| Host source (link check only) | `private/l56-src` (project `85128b6e`, before the l57 dev-override commit `169daf9`) |
| Toolchain | VS 2022 Build Tools CMake/Ninja, LLVM clang 20.1.8, PowerShell 7.4 (`private/tools/pwsh`) |
| Scripts | `private/l58vk-work/l58vk-sdk.ps1` (`-Step configure, build, install, ctest`), `private/l58vk-work/l58vk-host.ps1` |
| Outputs | build `private/l58vk-sdk-b`, output root `private/l58vk-sdk-o`, install `private/l58vk-sdk-install`, host link check `private/l58vk-host-b` |
| Logs | `private/l58vk-work/` configure.log, build.log, install.log, ctest.log, ctest2.log, host.log |

## Flags

Identical to the l56 SDK phase (`private/l56-clean-clone.ps1` Phase-Sdk) except
the backend switch and one registry guard:

```
-G "Ninja Multi-Config" -DCMAKE_CONFIGURATION_TYPES=Debug;Release;RelWithDebInfo -DCMAKE_CXX_STANDARD=23
clang/clang++/llvm-rc from C:/Program Files/LLVM/bin
C/CXX flags: -march=x86-64-v3 -gcodeview -g -ffile-prefix-map=<l56-src sdk>=./third_party/rexglue-sdk
EXE linker: /DEBUG /pdbaltpath:%_PDB% ; SHARED linker: /DEBUG /OPT:REF /OPT:ICF /pdbaltpath:%_PDB%
-DREXGLUE_BUILD_TESTS=ON -DREXGLUE_USE_D3D12=ON -DREXGLUE_USE_VULKAN=ON      <-- was OFF
-DREXGLUE_ENABLE_FIDELITYFX=OFF -DREXGLUE_ENABLE_TRACY=ON -DREXGLUE_ENABLE_PERF_COUNTERS=ON
-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF                                          <-- new (find side only)
```

CMake summary line: `Graphics: D3D12=ON Vulkan=ON`.

## What was missing, and how it was resolved

Nothing external was needed.

| Need | Source | Status |
| --- | --- | --- |
| Vulkan headers | `thirdparty/vulkan-headers` (submodule, `add_subdirectory`) | vendored |
| Vulkan loader | none at build time: `volk` (submodule) loads `vulkan-1.dll` at run time (`VOLK_PULL_IN_VULKAN=OFF`). `rexgpu-xenos.dll` and `rexruntime.dll` import no Vulkan DLL (llvm-readobj); the `vulkan-1.dll` string is in `rexruntime.dll`. On the Deck Proton supplies `vulkan-1.dll` (winevulkan); on Windows the GPU driver does. | vendored / run time |
| Memory allocator | `thirdparty/vulkan-memory-allocator` | vendored |
| SPIR-V Tools, SPIR-V headers, glslang | submodules, built as static libs (`SPIRV-Tools-static`, `glslang`, `SPIRV`); no executables | vendored |
| Shader compilation | none: the Vulkan backend's shaders are checked in as precompiled SPIR-V headers (`src/graphics/shaders/vulkan_spirv/*.h`, `src/ui/shaders/vulkan_spirv/*.h`). No glslangValidator/DXC/LunarG SDK step. FidelityFX (the only part that wants the LunarG SDK) stays OFF. | in tree |
| Python 3 | SPIRV-Tools generates its grammar tables with Python at build time. CMake found the machine's `C:/Python315/python.exe` 3.15.1 (already installed). New build-time dependency compared with the D3D12-only build. | already installed |

Side effect found and reverted: the SPIRV-Tools table generator wrote
`thirdparty/spirv-tools/utils/Table/__pycache__/` (5 `*.cpython-315.pyc`) into
the read-only `l56-src` SDK tree. The 5 files and the directory (all created by
this build, after `private/l58vk-work/marker-start`) were recorded in
`private/l58vk-work/pycache-removed.txt` and removed; only the mtime of the
`utils/Table` directory remains changed. Nothing else under `l56-src` is newer
than the marker (checked after the SDK build, after ctest and after the host
build). `l58vk-sdk.ps1` now sets `PYTHONDONTWRITEBYTECODE=1`; a clean-clone
script for the published Vulkan build must do the same (or accept an untracked
`__pycache__` in the clone).

CMake user package registry: `cmake --install` always runs
`reg add HKCU\Software\Kitware\CMake\Packages\rexglue`. The install step ran
with the Windows directories removed from `PATH`, so `reg` was not found; the
"Registered rexglue ..." message prints anyway, but `reg query` shows no
`l58vk` entry (state before in `private/l58vk-work/reg-before.txt`: l39..l57;
unchanged after).

## Build

- Configure 112 s; build 1842 actions (D3D12-only l56: 1436), `rc=0`, 0 errors.
- 912 `warning:` lines (l56: 503). All in SDK code or vendored headers; the
  increase comes from `src/graphics/vulkan` (186), `include/rex/graphics`
  (254 vs 123: headers now also included by the Vulkan sources),
  `thirdparty/vulkan-memory-allocator/include` (53) and `src/ui/vulkan` (29);
  mostly `-Wunused-parameter`, `-Wmissing-field-initializers`,
  `-Wnontrivial-memcall`.
- Install: 1356 files, `rexglue.exe --version` = `0.20.0.6`; embedded identity
  `rexglue-v0.20.0.6-win-amd64-Release@20261101_1747`. Installed targets export
  `REX_HAS_D3D12=1` and `REX_HAS_VULKAN=1` and ship `VulkanHeaders`,
  `VulkanMemoryAllocator` and `volk` CMake configs (needed by the
  `find_dependency` calls in `rexglueConfig.cmake`). Install size 190 MB
  (l56: 80 MB, mostly static libs and headers).

### Artifacts (`private/l58vk-sdk-install/bin`)

| File | Bytes | SHA-256 | l56 (D3D12 only) |
| --- | --- | --- | --- |
| `rexgpu-xenos.dll` | 6703104 | `35EAF81E46740B5DEBC81A6AC3B9C4580FB07A65B7E25713BEEDF83230574CEA` | 3136000, `7BC8C636...0080DE` (+3567104: Vulkan backend, SPIR-V translator, glslang) |
| `rexruntime.dll` | 11786240 | `08F9EBA8137D8110E1E2660C51750DABBE63FC19F91BBF0B7B528ADD8BE3D4FB` | 10995712, `A44703D8...0780DA` (+790528: Vulkan UI/presenter, VMA, SPIRV-Tools) |
| `TracyClient.dll` | 246784 | `A94D40D8DBF63F479E30C4FE90D7DEA62C9518895122D7540DFAAB582D19E885` | same size, new hash (rebuild) |
| `rexglue.exe` | 4112896 | `9D5B7988E70C34E33764C9B68693F152B330ECC95420939E23B788D216AFC142` | same size, new hash (rebuild) |

Same DLL set as l56 (no new runtime DLL to ship) and no new system DLL import
(`rexgpu-xenos.dll`: KERNEL32, MSVCP140, VCRUNTIME140, CRT, ole32, rexruntime).

### Host link check (not a candidate)

The l56 host tree (`85128b6e`, l56 volunteer options) configured and built the
`mcla` target against `l58vk-sdk-install` (explicit `rexglue_DIR`, package
registry off): `Found ReXGlue SDK 0.20.0.6 at .../l58vk-sdk-install`, 94 steps,
0 warnings, `rc=0`. `private/l58vk-host-b/mcla.exe` 53194752 bytes (same size
as l56), SHA-256 `9B1478A31135D7653DECDBA60BA8C082AB5BE914C75A486E80BEB551C536789A`.
Only `mcla` was built (no launcher, no tests, no package); not a candidate,
not to be shipped.

## SDK ctest

| Run | Result |
| --- | --- |
| 1 (`ctest.log`) | 1951/1952 passed, 4 skipped (the known four BitStream Write cases), 1 failed: `unit.Manifest: parses sdk_version when present`: `remove_all: The process cannot access the file because it is being used by another process: C:\Users\orange\AppData\Local\Temp\manifest_test`. Tests 259 and 260 ran concurrently under `-j 8` and share that fixed temp directory: a test-isolation race, not a Vulkan defect. |
| 2 (`ctest2.log`, same build) | **100% tests passed, 0 tests failed out of 1952**, same 4 skipped. |

No Vulkan-specific tests exist: the count equals the D3D12-only build
(1952 = 1463 ppc + 489 unit) and no test name contains `vulkan` or `spirv`.
The Vulkan backend is covered by compilation only; its behaviour needs a run.
The manifest race is worth a small SDK fix (unique temp dir per test) or a
note in the ctest procedure (rerun once).

## How the backend is chosen

- SDK cvar `gpu_backend` (string, init-only, default `any`; values `any`,
  `d3d12`, `vulkan`) in `src/ui/rex_app.cpp`; copied into
  `RuntimeConfig::gpu_backend`, then host hook `OnPreSetup(config)`, then
  `LoadGpuPlugin("xenos", backend)`.
- Plugin (`src/graphics/plugin_main.cpp`): `any`/`d3d12` gives D3D12 when
  compiled in (D3D12 wins `any`); `any`/`vulkan` gives Vulkan. A backend not
  compiled in logs "requested backend '...' is not compiled into this plugin"
  and setup fails with a message box (fatal by design, no fallback). A Vulkan
  device that fails to initialize also fails setup; no automatic D3D12 fallback.
- Vulkan device: `vulkan_device` (default -1 = automatic); the launcher's
  `d3d12_adapter` does not apply to Vulkan.
- Launcher (`src/mcla_launcher_gpu_policy.h`, `GpuBackendArgument`): always
  `--gpu_backend=d3d12`, also under Wine (KI-053 deferred).
- Host (`src/mcla_app.cpp` `OnPreSetup`): switches to `vulkan` only when the
  backend is `any` and `STEAM_COMPAT_DATA_PATH`/`WINEPREFIX`/`SteamDeck` is
  set; the launcher's explicit `d3d12` means this branch never fires.
- Portable launcher: `CompatibleNamedBackend` accepts only empty or `d3d12`;
  managed TOML may not set `gpu_backend`.
- Dev overrides (l57, `MCLA_DEV_OVERRIDES` builds): boolean whitelist only, and
  a cvar already set on the command line is not overridden, so the existing
  file cannot select Vulkan.

Result: with today's sources Vulkan cannot be selected through the launcher,
even with a Vulkan-enabled SDK. A host change is needed for the A/B.

## Host change proposal

Patch: `private/l58vk-work/M10-004-host-vulkan-switch.patch` (SHA-256
`E9C1DB720B4E0891599E47BEF306CCE03BBC3E92BC408D67424DBDC3006F286F`), against
`main` `8c57df9`; `git apply --check` passes on the main checkout (not
applied). Three files:

1. `src/mcla_app.cpp`: new host cvar `mcla_gpu_backend_vulkan` (bool,
   init-only, default false). In `OnPreSetup`, before the KI-053 block: when
   true and the host was compiled against an SDK with `REX_HAS_VULKAN`, log
   `MCLA graphics: native Vulkan backend requested for this session
   (mcla_gpu_backend_vulkan; launcher backend 'd3d12')` and set
   `config.gpu_backend = "vulkan"`; without `REX_HAS_VULKAN`, log that it is
   ignored and keep the launcher's backend (a D3D12-only SDK can never fail
   setup because of it).
2. `src/mcla_dev_overrides.h`: whitelist key `mcla_gpu_backend_vulkan`
   (`Phase::kStartup`; `kKeyCount` 5 to 6). `OnStartup` runs in
   `OnPostInitLogging` (inside `SetupEnvironment`), before
   `SetupPresentation`/`OnPreSetup`, so the value is in place in time. The
   launcher never sets this cvar, so the "set by the command line" rule does
   not block it.
3. `tests/mcla_dev_overrides_test.cpp`: six keys, the new key's lookup and
   phase, `gpu_backend` stays out of the whitelist. Compiled standalone with
   clang++ `-std=c++23` against the patched header: `80 checks passed`.

Per-session selection is one line in the owner override file
(`%LocalAppData%\MCLA-R-Launcher-v1\mcla-dev-overrides.txt`; on the Deck inside
the Proton prefix:
`.../compatdata/<appid>/pfx/drive_c/users/steamuser/AppData/Local/MCLA-R-Launcher-v1/`):

```
mcla_gpu_backend_vulkan=true
```

Without the line (or `=false`): D3D12/vkd3d with the same binaries. No launcher
change, Windows default unchanged, volunteer builds unaffected
(`MCLA_DEV_OVERRIDES` OFF; the cvar can still be set on the command line for
direct starts). The final KI-053 fix (launcher passes `--gpu_backend=vulkan`
under Wine, host detects Proton with `RunningUnderWine()`) stays gated on the
A/B result.

Open review points: check that the diagnostics `backend=` field reports the
backend actually created (the `gpu_backend` cvar itself stays `d3d12`); the
patch has not been built or tested in the full host tree.

## Deck A/B protocol (proposal)

1. Owner build (V1 ticket) from a clean clone pinned to the Vulkan-enabled SDK
   at the same SDK commit as the D3D12 reference, `MCLA_DEV_OVERRIDES=ON`, with
   the patch. The l58vk install is preparation, not that build. One package,
   one DLL set for both arms.
2. PC smoke first (5060): override line absent, D3D12 as before; log shows
   `backend=d3d12`. Then one PC Vulkan session to catch obvious Vulkan defects
   before the Deck.
3. Deck, same route as the M10-001 harness, stock mode, 1280x800, through the
   launcher: arm A = no line (D3D12 via vkd3d-proton), arm B =
   `mcla_gpu_backend_vulkan=true`. Two runs per arm, order A B B A; state cold
   or warm per run (pipeline caches differ per backend: the first Vulkan run is
   a cold pipeline run).
4. Record per run: the `native Vulkan backend requested` log line (arm B),
   backend and counters (`vulkan_*` non-zero in B, zero in A), title outputs/s,
   play fps median and 1% low per minute, CP WAIT_REG_MEM and write-watch
   costs, audio tearing, visual checks (photo/vinyl/livery, KI-047 police
   livery, read-back paths), crash/hang.
5. Decision per M10-priorities: switch the launcher under Wine only if Vulkan
   wins or ties without new defects; Windows stays D3D12.

## Risks

- **Vulkan backend untested here.** No Vulkan ctest; behaviour known only from
  older M7-016 Deck data (Vulkan 8.86 against D3D12 7.34 outputs/s), from a
  build kept nowhere in this project.
- **M10 D3D12-only work is absent on Vulkan:** `d3d12_perf_split`,
  `d3d12_readback_sync_composite` (KI-047 fix candidate), `d3d12_livery_diag2`,
  the pending-pipeline wait (KI-037/047) and, from l57, the WAIT_REG_MEM event
  wake / replay batched wake live in `src/graphics/d3d12` only. Under Vulkan
  the KI-047 fix and the M10-001 cost split are missing; livery/photo
  correctness and counters are not comparable one to one. The shared
  `readback_resolve` cvar is honoured by Vulkan.
- **SDK pin:** this build is v0.20.0.6 (`483458d`). The l57 SDK CPU-wait work
  is newer; a Deck A/B against l57 needs the Vulkan build at that commit.
  Publication needs a fork push (owner approval) and a clean-clone record with
  Vulkan on.
- **D3D12 regression risk:** the same `rexgpu-xenos.dll` now also carries
  Vulkan code and shared code is compiled with `REX_HAS_VULKAN`. Needs the PC
  smoke and host suites on this SDK before any candidate.
- **No fallback:** a Vulkan init failure is fatal (message box), so the switch
  stays owner-only until proven; the patch refuses to act on a D3D12-only SDK.
- **Build dependencies:** Python 3 now needed at build time (machine
  `C:\Python315`); SPIRV-Tools writes `__pycache__` into the source unless
  `PYTHONDONTWRITEBYTECODE=1`; install always tries the HKCU package registry.
- **Size:** `rexgpu-xenos.dll` 3.1 to 6.7 MB, `rexruntime.dll` +0.8 MB;
  packages grow by about 4.4 MB.
- **ctest race:** `Manifest: parses sdk_version when present` can fail under
  `-j 8` from a shared `%TEMP%\manifest_test`; passed on rerun.

## Limits

Build and unit tests only. No in-game run; no physical or owner check is
claimed. The patch is a proposal and was not applied to the main checkout.