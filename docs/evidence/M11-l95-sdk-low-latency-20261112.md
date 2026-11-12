# l95 SDK: vendor-neutral low-latency layer (NVIDIA Reflex, AMD Anti-Lag 2), default off (2026-11-12)

Implementation by an Opus subagent for the primary agent, from
`docs/evidence/M11-ki076-pacing-and-low-latency-design-20261111.md` (the Reflex / Anti-Lag 2
section). SDK commit `f9f5178` (`f9f5178df233dd2635f7bae343dada03a51aa468`) on the local branch
`mcla/m11-l95`, parent `e6d6bd3` (l92); worktree `private/l95-sdk-wt`. It is not pushed. Built,
ctest-ed and installed to `private/l95-sdk-install`. **Unit-tested only: no game run, no physical
PASS, and nothing about the drivers' behaviour has been observed.** Everything is off by default.
Spark was not used: source work checked by deterministic tests.

## 1. Vendored sources and licences

Code comes only from the two vendors' public MIT headers, fetched on 2026-11-12 from their official
GitHub repositories (`gh api` for the commit ids and trees, raw.githubusercontent.com for the bytes).
Every vendored file is byte-identical to the upstream commit: its git blob id equals the blob id in
the upstream tree (12 of 12 checked). No vendor library is bundled or linked: `nvapi64.dll` and
`amdxc64.dll` come with the user's driver. Nothing was taken from the GPL-3.0 forks or LARecomp.

**NVIDIA NVAPI** - https://github.com/NVIDIA/nvapi, commit
`51e5aae4bc40c3ccd3fb7d4dfd94256e7bfc496e` (branch main). Licence: MIT; every header carries the
SPDX MIT notice ("Copyright (c) 2019-2026 NVIDIA CORPORATION & AFFILIATES"); `License.txt` (MIT,
stated for the static libs, which are not used) is vendored too. GitHub's licence detection reports
`NOASSERTION` for the repository; the per-file SPDX headers are MIT. Folder
`thirdparty/nvapi/` (with `README-MCLA.md`):

| File | Bytes | SHA-256 | git blob |
|---|---|---|---|
| License.txt | 1248 | 84726B60C17FE0F71F92A6C074C3D4087370886EBA5D21CCC4EC55DB37D6C8BA | 34fa4289b1e2e1934b535f022d2b1ae51338791d |
| nvapi.h | 1393432 | 9F301D0B362A017143D833CCB64D55B2D9CBD6A5C9705C11032C1D74335A6858 | 3a3a023c4f0562c8f76fa7c523062c7a6ad0c19f |
| nvapi_interface.h | 31781 | 57DC6D5651B5811B5E76E3AD07B37B40DC1A16B50DA571F262F20AE3F31326BB | 2d1d252d222f1424c79e110958241361872a21cb |
| nvapi_lite_common.h | 43711 | 06AD8425F599025043FD8ACB9AC35AB41B4F25DD314EDD5EDEDED14F93884DDE | c9fdee57fa902d201a87d4bee37cbc8bc8a63afb |
| nvapi_lite_d3dext.h | 10093 | 2322DB28A2F7384FF6DF6FD1110B48E99660FA29956A2CBCD28A9D7157017E1A | 615f965f675036a982c13f4ec3ec7e9da43f70a7 |
| nvapi_lite_salend.h | 27072 | 92017F8E26F585601F4EAAC475FA9E542EAA2F99A80E376DDE52F2F73AB18E67 | 0e4de0c3e6939f22007e77061118ff70edc09503 |
| nvapi_lite_salstart.h | 26409 | 9DB770E52947B33D68B0C1F17BC21EDD9D736B824CCECEE2C5C527D48A89BE93 | c5099206f1964983cface88230041c855910a8d5 |
| nvapi_lite_sli.h | 14268 | 2A9C75FAECB32BC7A5237A4B83CC28A6A488C75493E44BF28688ABF3FEC01A34 | c55643ae20dcc35c5c6817dea561d2edb4a9337f |
| nvapi_lite_stereo.h | 29647 | BD6FE69136CFF36C73A9967A7CEF37186791D9B7C3EB3AE7AC61B4EFA628325B | 7b6e642d07f75da220dbd3cd31532cb518be162e |
| nvapi_lite_surround.h | 6205 | 197AD1E4762B2243904B0A13418747436AE70E4824099BF56BE4804C4C29956F | c0686b004a3d878e1f2e6241d8354ac668ddde8f |

**AMD Anti-Lag 2 SDK** - https://github.com/GPUOpen-LibrariesAndSDKs/AntiLag2-SDK, commit
`390aa4a8c8655d0ae6e90079db2c85e103a96da3` (branch main). Licence: MIT ("Copyright (C) 2024
Advanced Micro Devices, Inc."; `LICENSE.txt` and the header's own notice). GitHub reports
`NOASSERTION`; the licence text is MIT. Folder `thirdparty/amd_antilag2/` (with `README-MCLA.md`):

| File | Bytes | SHA-256 | git blob |
|---|---|---|---|
| LICENSE.txt | 1145 | 020463D3B92D6D901C9B9D93A432359529BE76883BB48F76EAEEECAEA2F15C71 | 2559e2a00cd7a4baf3d9562dd6f280e8ae54b0d6 |
| ffx_antilag2_dx12.h | 9765 | 0554C804CFB02442974B90CF1887031BFD18D41FBA2054A2CD23C8EE65E6265F | 446b5fcbd635b33601a68f4859e3a194b8aef72e |

Convention: the SDK keeps third-party code in `thirdparty/<name>/` with its licence file (as
`thirdparty/aes_128/LICENSE`); the SDK installs no licence files otherwise. Because inline code of
the Anti-Lag 2 header and NVAPI types end up in `rexruntime.dll`, both licences and the README files
are also installed, to `share/rexglue/thirdparty/{nvapi,amd_antilag2}/`. **Follow-up for any
distributed build (volunteer or public):** ship these two licence texts with the bundle (owner
builds are not distributed).

## 2. Design as built

Files: `include/rex/ui/low_latency.h` (pure logic, interfaces, runtime API, C entry points),
`src/ui/low_latency.cpp` (cvars, pure helpers, controller, runtime),
`src/ui/d3d12/d3d12_low_latency.cpp` (the two backends), `tests/unit/ui/low_latency_test.cpp`.
Changed: `src/ui/CMakeLists.txt`, `tests/unit/CMakeLists.txt`, `include/rex/ui/presenter.h`,
`src/ui/presenter.cpp`, `src/ui/d3d12/d3d12_presenter.cpp`, `src/ui/d3d12/d3d12_provider.cpp`,
`src/graphics/d3d12/command_processor.cpp`, `src/kernel/xboxkrnl/xboxkrnl_video.cpp`.
Diff: 26 files, 32294 insertions, 0 deletions: 14 vendored files (12 upstream files + 2 README-MCLA.md) with 30549 lines, 12 SDK files with 1745 lines.

### Cvars (UI/D3D12, all in rexruntime, registered before host start-up)

| cvar | default | read |
|---|---|---|
| `d3d12_low_latency_mode` (string `off`/`reflex`/`antilag2`/`auto`, also `0`..`3`) | `off` | once, at the first frame call after device creation |
| `d3d12_low_latency_boost` (bool) | false | once (Reflex `bLowLatencyBoost`) |
| `d3d12_low_latency_markers_only` (bool) | false | once for the backend (Reflex low-latency mode off, Anti-Lag 2 disabled), every frame for the sleep decision |
| `d3d12_low_latency_presleep_margin_us` (int 0..20000) | 1500 | every frame; 0 disables the pre-sleep |
| `d3d12_low_latency_max_sleep_ms` (int 1..250) | 30 | every frame (cap of the pre-sleep) |
| `d3d12_low_latency_allow_wine` (bool) | false | once: the only way to touch a vendor library under Wine / Proton |

### Detection and activation

- `D3D12Provider::Initialize` calls `low_latency::Init(device, adapter VendorId)` at its end;
  `~D3D12Provider` calls `low_latency::Shutdown(device)` first (backend turned off: Reflex
  `SetSleepMode` all zero, Anti-Lag 2 `Update(false)` + `DeInitialize`).
- Lazy activation at the first frame call (any of the hooks below), so a mode written by the host at
  start-up applies whatever the provider-creation order: `SelectBackend(mode, vendor, wine,
  allow_wine)`: off -> none; under Wine without the override -> none (`wine`); reflex needs 0x10DE,
  antilag2 0x1002, auto picks by vendor; otherwise none (`vendor`). Wine detection is the SDK's
  `rex::content_log::RunningUnderWine()`.
- Reflex: `nvapi64.dll` = `GetModuleHandleW`, else `LoadLibraryExW(..., LOAD_LIBRARY_SEARCH_SYSTEM32)`;
  `nvapi_QueryInterface` resolves `NvAPI_Initialize`, `NvAPI_D3D_SetSleepMode`, `NvAPI_D3D_Sleep`,
  `NvAPI_D3D_SetLatencyMarker`, `NvAPI_D3D_GetLatency`, `NvAPI_D3D_GetSleepStatus` by the ids of the
  MIT `nvapi_interface.h` (included with internal linkage; looked up by name); the pointer types are
  `decltype` of the MIT `nvapi.h` declarations. `SetSleepMode`: `bLowLatencyMode` = !markers_only,
  `bLowLatencyBoost` per cvar, `minimumIntervalUs` 0 (the guest vblank limiter stays the only
  limiter), `bUseMarkersToOptimize` 0. `GetSleepStatus` is logged (`driver_low_latency=`, `fs_vrr=`,
  `cpl_vsync_on=`).
- Anti-Lag 2: `amdxc64.dll` = `GetModuleHandleW` (the AMD D3D12 driver loads it; the vendor header's
  `Initialize` itself uses `GetModuleHandleA`), else `LoadLibraryExW` System32 only; then the
  header's `AMD::AntiLag2DX12::Initialize(&ctx, device)`. The frame-generation markers of the header
  are not used.
- One line: `MCLA_LOWLAT v=1 kind=init mode= vendor=0x.... backend= result= markers_only= boost=
  presleep_margin_us= max_sleep_ms= wine=` (only when the mode is not `off`; `result=ok ...` with the
  driver details, or the reason: `wine`, `vendor`, `bad_mode`, `nvapi_not_found`,
  `nvapi_initialize_<status>`, `set_sleep_mode_<status>`, `amdxc64_not_found`,
  `antilag2_initialize_0x...`). Any failure leaves the layer off for the session.

### Frame ids, markers, sleep

- Frame id = swap ordinal. The guest side counts VdSwap calls (`OnGuestSwap` in
  `VdSwap_entry`, SIMULATION_END), the command processor counts its frame-closing submissions; a
  frame's id while it is being built is "swaps so far + 1" on each side, so guest frame N and CP
  frame N carry the same id even when the CP lags (ring buffer).
- SIMULATION_START + the sleep: `rex_low_latency_simulation_start()` (C export, called by the host's
  frame-timer override, section 4 of the owner-build evidence). **At most one sleep per guest
  swap:** a second simulation start before the next VdSwap is counted (`sleep_skipped`) and does
  nothing (the NVAPI contract is exactly one `Sleep` per frame); a loading phase without swaps
  therefore sleeps at most once.
- RENDERSUBMIT_START at the frame open in `BeginSubmission` (after the l90/l91 lead waits and the
  frame fence check); RENDERSUBMIT_END in `EndSubmission` when the frame closes (the IssueSwap
  submission), after the l78 frame-end lead gauge, which is passed along for the minute histogram.
  `RefreshGuestOutput` stores the CP frame id in the new `GuestOutputProperties::frame_id`;
  `PaintAndPresentImpl` sets PRESENT_START / PRESENT_END around `Present`, once per guest frame
  (repeated presents of the same image and UI-only paints set none).
- The one-simulation-start-per-swap check: at each VdSwap the simulation starts since the previous
  one are classified 0 / 1 / 2+; 0 and 2+ are mismatches (minute counters, and up to 8
  `MCLA_LOWLAT v=1 kind=mismatch guest_swap= sims= render_swaps=` lines per session). `drift_min/max`
  = guest swaps minus CP swaps.
- Pre-sleep (busy-wait bound): with the last 8 total sleeps (pre-sleep + driver) averaging T (at
  least 4 samples), a high-resolution waitable-timer wait of min(T - margin, max_sleep_ms) runs
  before `NvAPI_D3D_Sleep` / `Update`, so the driver waits at most about the margin. An overshoot (the
  driver then waited under 50 us) clears the history: the next frame sleeps in the driver alone and
  measures again. Three consecutive sleeps over 250 ms turn the sleep off for the session (markers
  stay). Spin estimate: `QueryThreadCycleTime` across the driver call, divided by the TSC rate measured
  over the minute (`spin_ms_est`, an estimate).
- Locks: one small mutex for the counters, never held across a backend call or a wait; the runtime
  holds a shared lock while a hook uses the backend (exclusive only at activation and shutdown).
  Cost while off: one exported call and one atomic load per hook.

### Counters: one line per minute (CP thread, at a frame close)

`MCLA_LOWLAT v=1 kind=minute mode= backend= markers_only= sleeping= frames= fps= sims= sleeps=
sleep_skipped= sleep_ms_p50= sleep_ms_p99= sleep_ms_max= sleep_ms_total= presleep_ms_total=
spin_ms_est= overshoots= markers= present_frames= marker_mismatch= sim_per_swap0= sim_per_swap2p=
drift_min= drift_max= lat_frames= [gpu_ms= sim_to_present_ms= sim_to_gpu_end_ms=] lead_hist=0,1,2,3+`
(sleep percentiles from 100 us bins; latency from one `NvAPI_D3D_GetLatency` per minute: averages
over the frames with a simulation start and a GPU end).

## 3. Tests, build, install

- `tests/unit/ui/low_latency_test.cpp`, 7 cases `[l95][low_latency]`: mode parsing and backend
  selection (vendor, Wine and its override); frame ids and the sim-per-swap check; the pre-sleep
  governor (minimum samples, margin, cap, ring, overshoot reset, long-sleep back-off); histogram
  percentiles, latency summary, spin estimate; the null backend and the init / minute line texts;
  the controller with a test backend (marker ids, one sleep per swap, mismatch lines, pre-sleep
  2.5 ms after 4 sleeps of 4 ms, markers-only, a backend without markers, minute line and reset);
  the runtime stays off without a device. No test calls a vendor library.
- Build: fresh `private/l95-sdk-b` (Ninja Multi-Config, clang 20.1.8, the l92 options), output
  `private/l95-sdk-o`. The first build stopped on one compile error in the new backend file
  (`IDXGISwapChain` unknown: `nvapi.h` needs `dxgi1_4.h` first; fixed). Warnings: 500 raw lines;
  normalized set 219 entries, **0 new and 0 gone** against l92 (219).
- ctest: **100 % passed, 0 failed of 2241 (2237 passed + 4 skipped BitStream; l92: 2230 + 4; +7
  new)**, before and after install. One intermediate run after install had 1 failure
  (`unit.Manifest: sdk_version is nullopt when absent`, an existing test unrelated to l95); it
  passed alone and in the full rerun (parallel-run flake, not investigated).
- `git-clang-format` against `e6d6bd3`: applied (wraps only in the three new files), then clean;
  `git diff --check`: clean. The vendored headers are not formatted.
- Install `private/l95-sdk-install`: CMake prints `0.20.0.6-dev.gf9f5178`. `rexruntime.dll` exports
  `rex_low_latency_abi_version`, `rex_low_latency_enabled`, `rex_low_latency_simulation_start` and
  imports neither vendor DLL.

| bin file | Bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11673088 | 2FFE32EAC6B9F65E314D8DBA8E29F41AF5F926D1FB7CC0693EDC8ABFD8CC116D |
| rexgpu-xenos.dll | 3468288 | 379C787CC02F376EE432F342EB64F4461CBEDA1E0CDDA89E41CA4DF47A1DB9BD |
| TracyClient.dll | 246784 | 0879907B6BD5D641C49BA336F331D4563F2C4A46AC8140B22C6F39EF9077EB7F |
| rexglue.exe | 4113408 | 0F774879D3FA04D5502D56AD7C97FB2ABAA23B8F1F9A9C41D8A20C1F8CEE3222 |

File set against `l92-sdk-install`: l92 1228 files, l95 1233; 5 added (`include/rex/ui/low_latency.h`
and the four `share/rexglue/thirdparty/...` licence / README files), none removed; 24 differ (the 4
bin files, `ui/presenter.h`, `rex/version.h`, the 2 CMake config files, `rexruntime.lib` and 12
other static libs, 3 `.pc` files). rexruntime.dll grows by 76288 bytes; rexgpu-xenos.dll keeps its
size.

## 4. What could not be verified without the hardware

Everything about the drivers: whether `nvapi_QueryInterface` returns the functions on the owner's
driver, whether `SetSleepMode` / `Sleep` / markers succeed on a D3D12 device whose presents come
from the command-processor thread, how long `NvAPI_D3D_Sleep` sleeps and how much of it spins,
whether the pre-sleep margin is right, whether `GetLatency` reports frames with our marker ids,
whether the AMD driver has `amdxc64.dll` loaded and accepts the Anti-Lag 2 interface, and what any
of it does to fps, the frame lead and the KI-074 flicker. Also unverified: that the game's main timer
runs once per guest swap (the mismatch counters are there to check it before the Reflex results are
trusted).

## 5. Risks

- Reflex `Sleep` is called on the guest main thread inside the game's frame-timer function; if the
  game holds a guest lock there, the sleep delays the other guest threads waiting for it (not known
  from the code; the host takes no lock).
- With the strict frame lead bound ON, the bound and the Reflex sleep act together; with it OFF
  (`reflex-nostrict`) nothing guarantees lead <= 1: Reflex predicts.
- The driver busy-wait is bounded only by the pre-sleep heuristic; on the 35 W laptop the spin
  competes with the command-processor thread for package power (watch `spin_ms_est`).
- `markers_only` turns the driver low-latency mode off; it measures, it does not reduce latency.
- `GuestOutputProperties` grew by one field (installed header): a host must be rebuilt against this
  SDK (it is, for l95).
- Scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l95-work`: `vendor/` (the fetched
  files), `copy_tp.ps1`, `sdk-build.ps1`, `sdk-test.ps1`, `sdk-install.ps1`, `normwarn.py`,
  `fileset_diff.py`, `sdk-commit-msg.txt`; logs `sdk-configure.log`, `sdk-build1.log` (the compile
  error), `sdk-build-rt.log`, `sdk-build2.log`, `w95all.txt`, `ctest1.log`, `ctest-install.log` (the
  flake), `ctest-install2.log`, `sdk-install.log`, `fileset_diff.log`.
