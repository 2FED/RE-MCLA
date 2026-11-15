# l105 SDK: GPU time by category, per-pixel counters, tiling audit, start-up lines, FFX presenter shaders (2026-11-15)

Implemented by an Opus subagent for the primary agent, from the measurement build proposed in
[GPU-time research](M11-gpu-time-research-20261115.md) section 6 (MCLA_GPU_CAT, start-up lines) and
the tiling audit and FidelityFX check asked for after
[anti-aliasing and scaling research](M11-aa-and-scaling-research-20261115.md) sections 3a and 3c.
SDK commit `13ef300` (`13ef300cbe6538032df5b8e2bf1401e4fdd39162`) on the local branch
`mcla/m11-l105`, parent `a49e511` (l101); worktree `private/l105-sdk-wt`; not pushed, no tag.
Installed to `private/l105-sdk-install` (CMake version `0.20.0.12-dev.g13ef300`). **Built and
unit-tested only: nothing has run in the game, there is no physical PASS, and no number in this
document is measured with l105.** Spark not used: deterministic source work checked by unit tests;
checking a model answer would cost more than the work. Nothing was taken from GPL forks; no code
was ported. Owner build: [l105 owner build](M11-l105-owner-build-20261115.md).

## 0. What shipped and what did not

| Item | Status | Switch (default) |
|---|---|---|
| S1 GPU time by category, `MCLA_GPU_CAT v=1` | **shipped** | `d3d12_gpu_category_timing` (false), `d3d12_gpu_category_timing_every` (1) |
| S1 counters (transfers, stencil-bit passes, resolves and host samples, barriers by class, PSO switches, draws by MSAA, scissor x samples) | **shipped**, always on, in the same line | - |
| S1 tiling audit `MCLA_GPU_SURF v=1` (coordinator addition) | **shipped**, always on, SDK-side counters only | - |
| Guest renderer probe (`0x8217A700` tile count, renderer+384 AA mode) | **not done**: the host has no comparable read-only guest-structure probe for the renderer; the SDK counters answer the one-tile question (section 4) | - |
| S2 `MCLA_OUTPUT v=1`, `MCLA_GPUCAPS v=1` | **shipped**, always on, start-up | - |
| CAS / FSR1 presenter shaders without the FidelityFX SDK fetch | **shipped as an opt-in CMake option**, ON in the l105 build | `REXGLUE_ENABLE_FFX_PRESENTER_SHADERS` (OFF) |
| Rendering changes | none with the defaults (`present_effect` stays `bilinear`) | - |

## 1. MCLA_GPU_CAT: how the time is split

- Categories (`include/rex/graphics/util/gpu_category_timing.h:59-81`, `GpuCat`): `other`,
  `draw1x`, `draw2x`, `draw4x` (guest draws and their state / barriers by the guest MSAA of the
  bound surface, `RB_SURFACE_INFO.msaa_samples`), `res_dump` (render targets dumped into the
  EDRAM buffer), `res_copy` (the resolve copy dispatch and the rest of IssueCopy),
  `res_clear` (the resolve clear), `xfer_color`, `xfer_depth` (including the host depth store),
  `xfer_stencil` (the 8 per-bit stencil passes), `tex_load`, `shm_upload`, `readback`
  (resolve and memexport read-back copies), `output` (the guest-output refresh at the swap:
  gamma, FXAA).
- Sites (`GpuCatScope`, `include/rex/graphics/d3d12/command_processor.h`, end of file): IssueDraw
  `command_processor.cpp:5312`; IssueCopy `:6124`; read-back copies `:6545`, `:6600`; memexport
  read-backs `:5940`, `:5987`; IssueSwap `:2922`; DumpRenderTargets `render_target_cache.cpp:6031`;
  resolve clear `:1489`; host depth store `:4222`; transfers per destination `:4503` and per
  invocation (stencil bit) `:4700`; texture loads `texture_cache.cpp:1866`; shared-memory uploads
  `shared_memory.cpp:382`. A scope only sets a *pending* category (two byte moves) and restores
  the previous one at its end, so nesting is correct (an upload inside a texture load inside a
  draw is `shm_upload`, then `tex_load`, then the draw again).
- Timestamps: `DeferredCommandList::WriteCommand` calls a hook before every command
  (`deferred_command_list.cpp:181-184`; null when the frame is not measured: one predictable
  branch per command). The hook (`GpuCatCommandHook`, `command_processor.cpp:7964`) writes a
  `TIMESTAMP` EndQuery into the deferred list only when the pending category differs from the
  category of the submission's previous timestamp. A scope that records nothing costs no query.
  The GPU time between two consecutive timestamps of a submission belongs to the first one's
  category; the last segment ends at a closing timestamp written on the real list after its last
  replayed command (`GpuCatEndSubmission`, `:7973`, before the existing read-back resolves), so a
  category change closes the previous segment and the segments never overlap. Streaming replay
  replays the queries in order like any other command.
- Ring and budget (`GpuCatRecorder`, header `:189`): own query heap of 32768 timestamps and a
  mapped read-back buffer (`GpuCatEnsureHeap`, `:7900`), separate from the submission and
  read-back timers. The timestamps of a submission are contiguous modulo the ring and resolved
  at its end with one or two `ResolveQueryData` calls; slots are released when the submission's
  fence has passed (`GpuCatCollect`, `:7995`, from `CheckSubmissionFence` `:6907`). A frame may
  use 8192 timestamps; a frame that needs more, or finds the ring full (GPU far behind), stops
  writing, is **dropped** as a whole and doubles the interval `every` (at most 64), so then only
  every Nth frame is measured. Frames are accounted at their end only (a frame of several
  submissions is complete or not counted).
- Cost when off: the cvar is latched at every frame open (`GpuCatFrameOpen`, `:7944`, called at
  `:7156`); off means no hook, no heap and no query. The counters are always on (integer
  increments; the barrier classification loops over the barriers already being submitted).
- Accuracy limits: a timestamp marks when the GPU has finished the preceding work as the
  driver sees it; overlapping work at a boundary is attributed to the earlier category;
  timestamps inside a list can serialise the pipeline (why the switch is measurement only). The
  sum of the categories (`sum_ms_pf`) covers the span from a submission's first category
  timestamp to its end and is compared with `busy_ms_pf` (the existing `gpu_busy_ms`).
  Windows are filled as frames complete on the GPU (up to three frames after their CPU end).

## 2. Line formats and field meanings

Start and every switch change: `MCLA_GPU_CAT v=1 kind=config enabled= min_every= capacity=
budget= freq= frame=` (`enabled` 1 when the switch is on and the heap exists; `min_every` the
`d3d12_gpu_category_timing_every` cvar; `capacity` 32768 ring slots; `budget` 8192 timestamps per
frame; `freq` timestamp ticks per second; `frame` the CP frame).

Every perf-split window (minute and F10, after MCLA_CP_L101, `command_processor.cpp:4706`):

`MCLA_GPU_CAT v=1 reason= seq= enabled= every= frames= sampled_frames= dropped_frames= scale=
submissions= ts_pf= busy_ms_pf= sum_ms_pf= ms_pf=<cat>:<ms>,... segs=<cat>:<n>,...
xfer=color:,depth:,draws:,stencil_bit_passes:,stencil_bit_xfers:,host_depth_store:
res=n:,dump:,copy:,clear:,1x:,2x:,4x:,samples: bar_calls=shm_copy:,rt_sr:,other:
bar_entries=shm_copy:,rt_sr:,other: pso=guest:,external: draws=1x:,2x:,4x: scissor_samples=`

| Field | Meaning |
|---|---|
| `reason`, `seq` | as MCLA_PERF_SPLIT (`minute`, the F10 mark) |
| `enabled` | the timing was on at the last frame open |
| `every` | the current sampling interval (1 = every frame) |
| `frames` | frames of the window (MCLA_PERF_SPLIT `frames`) |
| `sampled_frames` | measured frames completed in the window |
| `dropped_frames` | measured frames dropped (budget or ring full) |
| `scale` | `frames / sampled_frames`: multiply a per-sampled-frame value by `scale x sampled_frames` (= `frames`) for the window total |
| `submissions` | measured submissions completed |
| `ts_pf` | timestamps per measured frame |
| `busy_ms_pf` | `gpu_busy_ms / frames` of the same window (all frames) |
| `sum_ms_pf` | sum of all categories, ms per measured frame |
| `ms_pf=` | ms per measured frame for each of the 14 categories, in the order of section 1 |
| `segs=` | segments (category runs) per category in the window, measured frames only |
| `xfer=color`, `depth` | ownership-transfer entries into a colour / depth render target |
| `xfer=draws` | transfer draws (one per merged invocation, 8 for a stencil-bit invocation) |
| `stencil_bit_passes`, `stencil_bit_xfers` | the per-bit stencil draws and the invocations that needed them (NVIDIA without PS stencil reference) |
| `host_depth_store` | host depth store dispatches |
| `res=n` | resolves with a non-empty area (IssueCopy) |
| `res=dump`, `copy`, `clear` | EDRAM dumps with rectangles, copy dispatches, resolves with a clear |
| `res=1x/2x/4x` | copy dispatches by the source MSAA |
| `res=samples` | sum of width x height x draw scale x samples of the copied rectangles |
| `bar_calls`, `bar_entries` | ResourceBarrier calls and entries by class: `shm_copy` the shared-memory buffer to or from COPY_DEST; `rt_sr` render target / depth to or from a shader resource; `other`. A call takes the first present class of shm_copy, rt_sr, other |
| `pso=guest`, `external` | guest pipeline switches and external ones (compute, transfer, clear, resolve) |
| `draws=1x/2x/4x` | issued guest draws by the surface MSAA |
| `scissor_samples` | sum over draws of the host scissor area x samples: an upper bound of the samples a draw can shade ("shaded samples" proxy) |

All counts are window totals (divide by `frames`). With the switch off `ms_pf`, `segs`,
`sampled_frames` are 0 and the counters are still real.

## 3. MCLA_OUTPUT and MCLA_GPUCAPS (start-up)

- `MCLA_OUTPUT v=1 adapter= vendor= output= same_adapter= refresh_hz= desktop=WxH render_adapter=
  render_vendor= containing_same= adapters= swapchain=WxH`, once per new swap chain
  (`d3d12_presenter.cpp:85`, call `:541`): the adapter whose DXGI output holds the window's
  monitor (`MonitorFromWindow`), its output name, refresh (current display mode) and desktop size;
  `same_adapter` 1 when that is the render adapter (LUID), 0 when another (an iGPU-attached panel:
  each frame crosses PCIe), `na` when no output matched; `containing_same` the same test for
  `IDXGISwapChain::GetContainingOutput`; `adapters` enumerated. Spaces in names are `_`.
- `MCLA_GPUCAPS v=1 vendor= dedicated_mb= shared_mb= gpu_upload_heap= uma= cache_coherent_uma=
  ps_stencil_ref= stencil_transfer_passes= rov= sample_positions_tier= msaa_quality=`
  (`d3d12_provider.cpp:470-526`): `gpu_upload_heap` OPTIONS16 `GPUUploadHeapSupported` (requires
  Resizable BAR; `na` if the query fails: there is no other ReBAR query in D3D12); `uma` /
  `cache_coherent_uma` ARCHITECTURE1; `ps_stencil_ref` OPTIONS `PSSpecifiedStencilRefSupported`
  and `stencil_transfer_passes` 1 or 8 from it (the render-target cache also needs
  `native_stencil_value_output`, default on); `msaa_quality` NumQualityLevels at 2x and 4x for the
  host render-target formats (rgba8, rgba16_unorm, rgb10a2, rgba16f, rg16, r32f, rg32f, d24s8,
  d32fs8; 0 = unsupported, `na` = query failed).

## 4. MCLA_GPU_SURF: the tiling audit

Every guest draw reports its EDRAM configuration key (surface pitch, MSAA, colour 0 base, depth
base) and its `PA_SC_WINDOW_OFFSET` only when the key differs from the previous draw's
(`command_processor.cpp` IssueDraw, after the draw scope). Per frame (`EndFrame` at the frame end,
`:7434`): distinct keys (up to 32, more counted as `overflow`), distinct window offsets, key
changes. A frame drawn in tiles renders the same surface at two or more window offsets, so
`multi_offset_frames` > 0 and `offsets_max` >= 2 mean tiling; one offset per frame means one tile.

`MCLA_GPU_SURF v=1 reason= seq= frames= keys_sum= keys_max= offsets_sum= offsets_max=
multi_offset_frames= changes= overflow= rect_draws= rect_full= swaps= swap=WxH last_frame_keys=`
(`:4711`): `keys_sum` / `offsets_sum` sums over frames (divide by `frames`), `*_max` the window
maximum, `rect_draws` rectangle-list draws (the game's clear and full-screen quads) and
`rect_full` those whose scissor is at least the surface pitch wide (clear-like full-surface
quads, as against the resolve clears in `res=clear`), `swap` the last guest front buffer size,
`last_frame_keys` the keys of the last completed frame as `p<pitch>m<samples>c<colour base>
d<depth base>o<x>,<y>` (guest units, EDRAM tiles for the bases). For the research question: at
"1080p" (960x540 guest, 2x MSAA, 816 tiles per 32-bpp surface) one offset is expected; at 720p
2x (1,472 tiles) two. The guest's live AA mode is not read: the per-frame `m<samples>` of the
main surface keys shows it.

## 5. CAS / FSR1 presenter shaders without the FidelityFX SDK

- Finding: the CAS (sharpen, resample, dither) and FSR1 (EASU, RCAS, dither) pixel shaders are
  committed as compiled bytecode only (`src/ui/shaders/bytecode/d3d12_5_1/guest_output_ffx_*`,
  `src/ui/shaders/vulkan_spirv/guest_output_ffx_*`); the code that uses them is gated by
  `REX_HAS_FIDELITYFX_SDK` (`src/ui/presenter.cpp:44-79`, `:96`, `:113`, `:253`;
  `src/ui/d3d12/d3d12_presenter.cpp` effect cases from `:1559`), the FidelityFX runtime
  (`ffx_api`, temporal FSR2/3) by the separate `REX_HAS_FIDELITYFX_RUNTIME`. Upstream defines
  `REX_HAS_FIDELITYFX_SDK` only when `REXGLUE_ENABLE_FIDELITYFX=ON` has fetched the SDK
  (`src/ui/CMakeLists.txt:114-119`), and PRIVATE to `rexui`.
- Layout problem found while enabling it: the define changes public headers
  (`include/rex/ui/presenter.h`: `GuestOutputPaintConfig` members, the effect enums,
  `kMaxGuestOutputPaintEffects`; `include/rex/ui/d3d12/d3d12_presenter.h` root signature indices),
  so a PRIVATE define gives `rexui` another `Presenter` layout than `rexgpu-xenos` and the host
  that include the same headers. The new option `REXGLUE_ENABLE_FFX_PRESENTER_SHADERS`
  (`CMakeLists.txt:19`, default OFF) therefore defines `REX_HAS_FIDELITYFX_SDK=1` PUBLIC on `rexui`
  (`src/ui/CMakeLists.txt:120`) and on `rexruntime` (`src/system/CMakeLists.txt:103`), so it is in
  the exported `rex::runtime` interface definitions (checked in the installed
  `rexglueTargets.cmake`) and every target compiles one layout (checked in the build: rexui,
  rexruntime, rexgpu-xenos and unit_tests all carry the define). Without the option nothing
  changes; with `REXGLUE_ENABLE_FIDELITYFX=ON` the upstream path is used unchanged.
- Result: compiles and links without the download (no new third-party code, no network), 0 new
  warnings (3 unused-variable warnings of the gated code disappear), ctest green. `present_effect`
  then accepts `bilinear, cas, fsr, fsr2, fsr3` (default `bilinear`, restart-only, read at
  presenter initialisation); `fsr2` / `fsr3` need the runtime and fall back (upstream behaviour;
  no host key selects them). Sharpness cvars: `present_cas_additional_sharpness` (0..1, default
  0), `present_fsr_sharpness_reduction` (0..2 stops, default 0.2),
  `present_fsr_max_upsampling_passes` (default 4).
- Licence: no FidelityFX source or licence text is in the tree (searched every tracked file:
  only the CMake fetch script and the gating headers mention FidelityFX; the bytecode headers
  carry only the HLSL compiler listing). The bytecode is compiled from the Xenia presenter
  shaders that include AMD's FidelityFX CAS and FSR 1 headers (MIT licence, Advanced Micro
  Devices, Inc.); the CPU-side constant setup in `presenter.h` follows `CasSetup` / `FsrEasuCon`.
  **Before a volunteer build ships these effects, the MIT notices of FidelityFX CAS and FidelityFX
  FSR 1 must be added to the third-party notices from the upstream repositories** (not
  fetched here).

## 6. Tests, build, install

- New `tests/unit/graphics/gpu_category_timing_test.cpp` (10 cases, `[l105]`): names and the
  MSAA mapping; barrier classes and the call class; segments follow category changes and an empty
  scope costs nothing; a frame of several submissions is added at its end and gaps are not
  counted; off and the sampling interval; budget overflow drops the frame and doubles the interval
  (capped); ring wrap ranges and a full ring; the window line (every field, no division by zero);
  the tiling audit and its line (negative offsets, overflow); the MCLA_OUTPUT and MCLA_GPUCAPS
  lines.
- Build: fresh `private/l105-sdk-b` (the l101 options plus `REXGLUE_ENABLE_FFX_PRESENTER_SHADERS=ON`),
  output `private/l105-sdk-o`; the first build compiled without error. Normalized warning set 212
  entries, **0 new** against l101 (3 gone, see section 5).
- `git-clang-format` against `a49e511` (include, src, tests): applied (7 files), rebuilt, then
  clean.
- ctest: first run **100 % passed, 0 failed of 2327 (2323 passed + 4 skipped BitStream; l101
  2313 + 4; +10 new)**. Two later full parallel runs each failed one *different* pre-existing
  `unit.Manifest:*` case (`IsManifest detection` exit 0xc0000409, `parse manifest with inline
  modules`): all Manifest cases share the fixed temp directory `%TEMP%\manifest_test`
  (`tests/unit/codegen/manifest_test.cpp:22-28`), so parallel ctest processes race; the new tests
  only shifted the schedule. Serially they pass (5 repeats). Accepted runs (after the format and
  after the install): `-E ^unit.Manifest -j 8` 2317 of 2317 passed (4 skipped) plus
  `-R ^unit.Manifest -j 1` 10 of 10 passed. The race is not fixed here (no test change outside
  scope).
- Install `private/l105-sdk-install`, CMake `0.20.0.12-dev.g13ef300`; file set against
  `l101-sdk-install` 1240 -> 1241 (added `include/rex/graphics/util/gpu_category_timing.h`; none removed; 26 files differ).

| bin file | Bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11769344 | 74663325AFD8A4477CBE5C0CBFC8CB5ED7EA086FCEFB1FFAC871BD77CF369173 |
| rexgpu-xenos.dll | 3631616 | 3A71267522F0191183126ADA0F4B6518C79745A9922864EF7EEA2A1A75F28657 |
| TracyClient.dll | 246784 | BFF5B2DADDC2F574625A6E922E40B41083D284A855A92E28BE7E4D75F80E815F |
| rexglue.exe | 4113920 | A78802D0965CA0DA3BC75C7792F70459F856B0F6B48F280669FCA965601B3F4E |

TracyClient.dll has no source change but a new hash (fresh build tree). **ABI:**
`D3D12CommandProcessor`, `DeferredCommandList` and (with the option) `Presenter` changed: a host
must be rebuilt against this SDK. The host change uses no new SDK symbol, so the main checkout
also builds against the gate SDK (l67).

SDK diff (`git show --numstat 13ef300`): 15 files, 1606 insertions, 0 deletions: CMakeLists.txt +2,
include/rex/graphics/d3d12/command_processor.h +56, include/rex/graphics/d3d12/deferred_command_list.h
+12, include/rex/graphics/util/gpu_category_timing.h +680 (new), src/graphics/d3d12/command_processor.cpp
+240, src/graphics/d3d12/deferred_command_list.cpp +6, src/graphics/d3d12/render_target_cache.cpp +45,
src/graphics/d3d12/shared_memory.cpp +2, src/graphics/d3d12/texture_cache.cpp +2,
src/system/CMakeLists.txt +4, src/ui/CMakeLists.txt +6, src/ui/d3d12/d3d12_presenter.cpp +84,
src/ui/d3d12/d3d12_provider.cpp +61, tests/unit/CMakeLists.txt +1,
tests/unit/graphics/gpu_category_timing_test.cpp +405 (new).

## 7. Risks and limits

- No game run: the category times, their cost and the start-up lines are unobserved. The hook
  and the queries only exist while `d3d12_gpu_category_timing` is on.
- Category attribution is approximate at boundaries (section 1); barriers are attributed to the
  category active when they are submitted (an upload's switch back to read belongs to the next
  draw).
- The per-frame budget (8192) is a guess from the research's command census (about 2-3 k category
  changes per frame expected); `every`, `dropped_frames` and `ts_pf` show whether it holds.
- With FFX shaders compiled in, `present_effect` is the only switch; at the default nothing
  changes. The CAS / FSR paths have not run on our hosts; they are upstream Xenia code.
- The tiling audit sees one key change per draw at most; an MSAA or base change without a draw is
  not seen (not needed for the tile question).
- Scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l105-work`: `sdk_edit1.py` ..
  `sdk_edit4.py`, `sdk_edit3b.py`, `new/` (the new header and test), `sdk-build.ps1`,
  `sdk-test.ps1`, `sdk-test-split.ps1`, `sdk-install.ps1`, `normwarn.py`, `sdk-commit-msg.txt`;
  logs `sdk-configure.log`, `sdk-build1.log`, `sdk-build2.log`, `ctest1.log` .. `ctest4.log`,
  `sdk-install.log`, `ctest-install.log`, `w105all.txt`.
