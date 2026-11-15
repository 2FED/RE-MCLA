# M11 GPU-time research: why the RTX 5060 needs about 18 ms per frame at "1080p" (2026-11-15)

Read-only research by an Opus subagent for the primary agent (no build, no game run, no code
change). Sources: SDK worktree `private/l101-sdk-wt` at `a49e511` (the l101-l103 SDK), host
`src/`, today's owner sessions on profile 68e5 (`fb4bf9d9` pc/fast2, `aaf16693` pc/fast,
`2e79a43f` verify-all; Radeon 860M `cfcb6950`, `eb68d035`), yesterday's copies under
`private/owner-l100-20261114/sessions/` (`15c4d94f`, `34aec854`, `63fec055`, `5f69c8b7`), the
HWiNFO CSV for `fb4bf9d9` (copied to `private/owner-hwinfo-20261115/`, see section 7) and
the earlier evidence named inline. **Spark not used:** deterministic aggregation of logs and
source reading; checking a model answer would cost more than the work. Scripts and outputs
are in the scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\gpu-time-work`
(`split.py`, `pool.py`, `rep.py`, `repfit.py`, `hw.py`).

## Headline

- `gpu_busy_ms` is the sum of the per-submission GPU spans of the CP's own command lists:
  a timestamp written as the first command of each list and one as the last command. It
  includes everything the CP records (draws, resolves, dumps, transfers, texture loads,
  uploads, read-back copies, the guest-output refresh). It does not include the presenter's
  list (bilinear 1920x1080 to the swapchain), idle gaps between submissions, or CPU waits
  (a list runs only after it is closed). It **does** include stalls inside a list (barrier
  drains, PCIe fetches), and the timestamp clock is fixed-rate, so lower GPU clocks show up
  as more busy time.
- GPU-bound is shown at the queue level: 17.2-18.8 ms busy against an 18.5-21 ms frame
  (86-93 %), HWiNFO core load 90-95 %, and no thermal limit. The GPU is **not limited by
  throughput**: the memory controller load is 35-37 %, and the clock is power-capped at
  2.55-2.67 GHz against a 3.09 GHz maximum (about 70 W, with the power limiter set in 70-80 %
  of the samples). So the GPU timeline is full of serialized, small, latency-bound work.
- "1080p" in the launcher means **guest 960x540 x draw scale 2x2 = 1920x1080** (2.25x the
  pixels of the game's native 1280x720), not a 1.5x scale. Three points on this GPU lie on
  a straight line: 720p 12.4 ms (l53, M10-001), "1080p" 17.3-18.3 ms (today), 1440p 25.8 ms
  (M10-001). That is about **7.9 ms that does not depend on pixels plus 4.5 ms per
  720p-worth of pixels**, so about 10 ms of the 18 ms grows with pixel count. Raw shading
  should cost well under 1 ms on a 5060 (Xenos is about 240 GFLOPS). Both terms are
  emulation structure: 60 resolves per frame, each a dump, a scaled copy and a texture
  reload; render-target transfers (8 stencil-bit passes each on NVIDIA); about 1.6-1.9 k
  barrier calls and 28 k commands per frame.
- First owner A/B (no build needed): **720p (guest 1280x720, scale 1) against "1080p" with
  arm pc/fast2**. Expected GPU about 12-13 ms per frame (-5 to -6 ms); fps then becomes
  CP-bound at about 65-75. Second (no build): the laptop's power or MUX mode. Third (existing
  key, new arm file only): `shared_memory_upload_ahead_64`.

## 1. What gpu_busy_ms measures

Source (`src/graphics/d3d12/command_processor.cpp` in the SDK worktree):

- `WriteSubmissionStartTimestamp` (7592-7639): `EndQuery(TIMESTAMP)` slot 2n as the first
  command of the real command list, written right after `command_list_->Reset`. With
  streaming replay this happens at submission open (7083-7096, call at 7094); otherwise it
  happens before the deferred list is executed into the real one (7258-7268, call at 7267).
- `WriteSubmissionEndTimestamp` (7641-7653): slot 2n+1 after `ResolveReadbackGpuTimers`
  (7270-7271), then `ResolveQueryData` into a readback buffer. The list is closed (7283),
  executed (7287) and fenced (7302).
- `CollectSubmissionTimestamps` (7655-7669): for each completed submission it adds
  `end - begin` to `perf_gpu_busy_ticks_total_`. The perf split converts the total with
  `GetTimestampFrequency` (4592-4594, 4805). The "MCLA gpu busy/min" line (7480-7486) uses
  the same ticks. `gpu_submissions` counts the measured lists.
- `d3d12_readback_gpu_timing` (cvar, 411; timers 7671-7773) is something else: timestamp
  pairs around the read-back GPU copies only (`readback_gpu_ms`,
  `gpu_readback_ms_per_frame`). These copies are inside the submission spans, so they are
  part of `gpu_busy_ms`, not added to it.
- The `REX_PERF_AUDIT_SAMPLE gpu_us` (300 samples after each F10) is a different
  measurement. It is one span from the first submission of a frame to its last one
  (7043-7050, 7262-7282, 7901-7916), so it includes the gaps between submissions. In
  `fb4bf9d9` it averages 20.8-22.9 ms per frame, against `gpu_busy` 17.7-19 ms per frame in
  the same windows. About 2-3.5 ms per frame are gaps where the GPU waits for the next
  submission (Reflex keeps the CP just in time; the targeted kicks split frames).

What is included: every CP-recorded command, including clears, resolves (EDRAM dumps and
copy dispatches), ownership transfers, texture loads, shared-memory uploads
(`CopyBufferRegion`), read-back copies, query resolves and the guest-output refresh.
Excluded: the presenter's own command list on the same direct queue
(`src/ui/d3d12/d3d12_presenter.cpp:309-321`, `:1179`; effect `present_effect` default
`bilinear`, `src/ui/presenter.cpp:45`), any DWM/Optimus copy, and idle time between lists.

Can it be inflated?
- **CPU waits inside a submission: no.** The list executes only after `Close` and
  `ExecuteCommandLists`, and with streaming replay after `FinishStreaming` (7248-7256). The
  GPU never sees an unfinished list.
- **GPU stalls inside a list: yes, by design.** Barrier drains, waits on PCIe reads and
  front-end bubbles all count as busy. The ~1.6-1.9 k `ResourceBarrier` calls per frame
  (section 3) make this the likely dominant form of "busy".
- **Clock and power: yes, proportionally.** The ticks are fixed-rate, so time at a lower
  clock is longer. HWiNFO and `MCLA_VRAM gpu_mhz` show a power-limited clock rather than
  thermal throttling (section 7).
- Overlap between consecutive lists is not double-counted: ExecuteCommandLists boundaries
  are full barriers (comment at 7242-7246), so the spans are serial.

GPU-bound verdict: proven at the queue level (busy 86-93 % of wall, Reflex sleep filling
the CP slack, CP busy 12.5-14.6 ms against frames of 18.5-21 ms). It is not
throughput-bound. Existing logs cannot tell executing time from time stalled inside a list.

## 2. Resolution and sample budget

| Launcher target | Guest video mode | Draw scale | Host pixels per full-screen pass | vs native 1280x720 |
|---|---|---|---|---|
| 720p | 1280x720 | 1x1 | 921,600 | 1.00x |
| **1080p (owner)** | **960x540** | **2x2** | **2,073,600** | **2.25x** |
| 1440p | 1280x720 | 2x2 | 3,686,400 | 4.00x |
| 2160p | 1280x720 | 3x3 | 8,294,400 | 9.00x |

- Sources: `launch-receipt.txt`/`capabilities.txt` of `fb4bf9d9` (`scale 2`,
  `render_target 1920x1080`, `guest_mode 960x540`, `draw_scale 2x2`,
  `render_limitation experimental-960x540-guest-plus-2x`). The launcher passes
  `draw_resolution_scale_x/_y` (`src/mcla_launcher_controller_win.cpp:941-942`), and the SDK
  scale is an integer (`src/graphics/pipeline/texture/cache.cpp:78-90`; no 1.5x,
  `docs/evidence/M8-render-targets-20261011.md`). The guest caps automatic modes at 921,600
  pixels (`sub_8217AC30`, same document), so the native target is 1280x720.
- Log: `D3D12 guest present ... source=1920x1080 swapchain=1200x675` at start; fullscreen
  toggled at 18:14:14 (the panel is 3840x2160, so the presenter scales 1920x1080 to it
  with bilinear, outside `gpu_busy`). On the 860M: `source=1280x720 swapchain=1600x900`.
- MSAA: the game uses guest 1x, 2x and 4x MSAA passes (M4-002/M5-003 audits: "active
  1x/2x/4x MSAA"). The backend keeps the guest sample count (`native_2x_msaa` true,
  `src/graphics/flags.cpp:21`, so 2x is native 2x, not emulated 4x) and multiplies the
  pixel count by the scale. Every pass therefore has 2.25x the native samples (a 4x MSAA
  main pass is about 8.3 M samples against 3.7 M on the 360). Which passes use which
  count at the 960x540 guest mode is not logged (`XENOS_AUDIT_RESOLVE guest_msaa` needs
  `gpu_render_audit`).
- Render path: **RTV** (host render targets). The default chooses RTV on NVIDIA and AMD
  (`src/graphics/d3d12/render_target_cache.cpp:179-206`, cvar `render_target_path_d3d12`
  init-only at 46-48). The shader store has `rtv.d3d12=83241`, `rov.d3d12=unreadable`.
- Depth: `depth_float24_convert_in_pixel_shader` false and `depth_float24_round` false
  (`flags.cpp:22-23`). There is no per-pixel oDepth conversion and early Z stays on; the
  conversion cost sits in transfers and dumps (`host_depth_store_*` compute).
- Memory (`MCLA_VRAM_SDK`): `rt_mb` 2249-2345, `edram_mb` 40, `scaled_resolve_mb` 224 at
  scale 2 (`fb4bf9d9`), against `rt_mb` 678, `edram_mb` 10, `scaled_resolve_mb` 0 at scale 1
  (860M `cfcb6950`/`eb68d035`). The EDRAM mirror is 10 MB x scale^2 (`render_target_cache.cpp:208-210`).
- Texture filtering: `anisotropic_override` default **3 = forced 4x AF on eligible
  textures** (`src/graphics/pipeline/texture/cache.cpp:64-76`, hot-reload). The host does
  not override it.

GPU time against pixels (RTX 5060 Laptop):

| Mode | Session | draws/frame | GPU ms/frame |
|---|---|---|---|
| 720p (1.00x) | l53 runs A/B, `M10-001-720p-and-ki047-20261101.md` | 5957-6652 | 12.28-12.99 |
| 1080p (2.25x) | `fb4bf9d9` 15 windows (mean) | 4765 | 17.62 |
| 1080p (2.25x) | `aaf16693` 8 windows | 5036 | 18.03 |
| 1440p (4.00x) | `M10-001-run1-1440p-cold-20261101.md` | about 6 k | 25.4-26.5 |

A line through 720p and 1440p (7.9 ms + 4.5 ms per 1.00x) predicts 18.0 ms at 2.25x, which
matches today's 17.6-18.0 ms. The 860M at 720p (1.00x) needs 32.5-36.3 ms (`eb68d035`,
`cfcb6950`), so the 5060 does 1.00x of pixels about 4-5x faster per pixel than the 860M.
That fits their throughput ratio, so the 5060 is not anomalously slow per pixel; the work
per pixel is just large.

## 3. Per-frame GPU work inventory

`fb4bf9d9` minute 18:20:11-18:21:11 (52.0 fps, 4866 guest draws, GPU 17.68 ms/frame), next
to 860M `eb68d035` minute 15:42:29-15:43:29 (25.8 fps, 5344 draws, GPU 36.26 ms/frame).
Sources: `MCLA_PERF_SPLIT`, `MCLA_READBACK`, `MCLA replay/min` (command ids from
`include/rex/graphics/d3d12/deferred_command_list.h`), `MCLA_TEXHEAP`, `MCLA_LIVERY_DIAG2`.

| Per frame | 5060 1080p (fb4bf9d9) | 860M 720p (eb68d035) | Note |
|---|---|---|---|
| guest draws | 4866 | 5344 | |
| DrawIndexedInstanced / IASetIndexBuffer | 3977 / 3977 | 4557 / 4557 | one IB set per indexed draw |
| DrawInstanced | **1174** | 867 | NVIDIA +300-450: stencil-bit transfer draws |
| SetGraphicsRoot32BitConstants | **323** | **0** | stencil-bit masks (8 per stencil transfer), NVIDIA only |
| SetPipelineState (external: compute, transfers) | **511** | 306 | +200: one PSO per stencil bit |
| SetPipelineStateHandle (guest PSOs) | 1550 | 1761 | |
| SetGraphicsRootConstantBufferView | 12,103 | 14,602 | about 2.7 per draw; upload heap = system memory |
| ResourceBarrier calls / entries | 1630 / 2019 | 2062 / 2471 | about 0.38 calls per draw |
| Dispatch | 302 | 302 | resolve copies, EDRAM dumps, texture loads, depth store |
| CopyBufferRegion (shared-memory uploads) | 728 | 951 | each upload run switches the 512 MB buffer to COPY_DEST and back |
| replayed commands | 28.4 k | 32.2 k | |
| copies (resolves) | 60.6 | 59.7 | `copies=` |
| GPU read-back copies / elided | 16.0 / 44.6 | similar | 5.6 MB/frame copied, `gpu_readback_ms_per_frame` 0.49 (5060) / 0.45 (860M) |
| texture loads (`tex_loads`) | about 97 | about 93-104 | about 56 of them reload resolved data (`tex_loads_rb_overlap`) |
| submissions | 8.17 (frame_end 1, primary_end 5.0, kick 1.8, forced 0.15, early 0.06) | 5.8 (primary_end 3.4, kick 0.57, idle 0.6) | |
| uploads | 6.1 MB | 8.1 MB | `upload_kb` |
| constant-buffer bytes | 8.4-11 MB | 8.8-10.5 MB | `cb_upload_kb` |
| memexport | not logged | not logged | |

The same counters exist on both GPUs. The frame structure is the same per frame (about 60
resolves, about 300 dispatches, about 95 texture loads, about 2 k barriers, 5-6 submissions
plus kicks). Two things are NVIDIA-specific:
1. **Stencil transfers.** The 5060 reports `Pixel-shader-specified stencil reference: no`
   (the 860M: yes). Every depth ownership transfer then writes stencil with 8 per-bit passes,
   each with its own root constant, PSO switch and draw (`render_target_cache.cpp:4499`,
   `:4907-4916`; `use_stencil_reference_output_` at `:377-380`). With about 320 root-constant
   sets per frame, that is about 40 stencil transfers and 320 extra full-region passes per
   frame, at 2x2 scale and the guest MSAA count.
2. **Kicks.** 1.8 targeted kicks per frame against 0.6 on the 860M; the fast GPU idles more
   often, so `ShouldKickReadbackSyncSubmission` fires (`command_processor.cpp:4405-4430`;
   `d3d12_readback_sync_kick_draws` 512, `:253`).

Resolve path: every copy is a **dump** of the owning render targets into the EDRAM buffer,
then a **copy dispatch** into the scaled-resolve buffer (scale 2), then later a **texture
reload** from that buffer. The dump needs the render target in `NON_PIXEL_SHADER_RESOURCE`
(a decompress or flush on the GPU) and back (`render_target_cache.cpp:1313-1351`,
`:1356-1436`, `:5983-6035`). `direct_host_resolve` defaults to true, but
`TryResolveCopyDirectly` is a stub that still calls `DumpRenderTargets`
(`:5936-5981`: "keep using the existing dump path until source-image direct shaders land").
The direct resolve saves nothing today.

What scales with resolution rather than draws: dumps, resolve copies and texture reloads
(their size is scale^2 x samples), transfers (including the 8 stencil-bit passes), clears,
the game's full-screen passes (bloom, motion blur and the like at 2.25x), and the
guest-output refresh. The read-back bytes are at guest resolution (5.4 MB/frame at
960x540 against 7.5 MB at 1280x720) and do not scale.

Regression of GPU time per frame on per-frame counts. All seven 5060 sessions together: 133
windows with more than 400 frames, more than 3000 draws/frame and more than 25 fps; script
`pool.py`.

| Model | Fit | R2 |
|---|---|---|
| draws | 9.83 ms + 1.67 us x draws | 0.62 |
| draws + copies | 2.22 ms + 1.42 us x draws + **0.157 ms x copies** | 0.72 |
| fb4bf9d9 alone | 1.89 + 1.48 us x draws + 0.150 ms x copies | 0.97 (n=15) |
| aaf16693 alone | 3.65 + 1.10 us x draws + 0.151 ms x copies | 0.74 (n=8) |
| 860M (both sessions) | 5.55 - 0.27 us x draws + 0.541 ms x copies | 0.95 (n=10; corr(draws, copies) 0.80) |

The copy coefficient is stable at 0.15-0.16 ms on the 5060 across sessions. About 57 copies
per frame would then be about 8.5-9 ms, close to the pixel-proportional 10 ms of section 2.
**Caveat:** copies are a proxy for render passes (shadow, reflection, post-processing), so
the coefficient carries each pass's own drawing as well as the resolve machinery. It is an
upper bound for the resolve path, not its cost. On the 860M the per-draw term vanishes and
copies (pixel work) explain the time: the iGPU is pixel-throughput-bound, the 5060 is not.
Replay-census fits (`repfit.py`, 27 minute windows) cannot separate draws, barrier calls,
PSO switches and uploads, which are collinear (R2 0.71-0.74 each).

## 4. Per-draw GPU cost (1.1-1.4 us per draw)

Candidates, from the code and the census:

| Candidate | Evidence | Status |
|---|---|---|
| Barrier drains: about 0.38 `ResourceBarrier` calls per draw; shared-memory uploads interleaved with draws switch the whole 512 MB buffer `COPY_DEST` <-> read (`src/graphics/d3d12/shared_memory.cpp:184-196`, `:381`, `:404`), and every switch waits for all in-flight vertex/index fetch | 1630-1887 calls/frame, 728-903 upload copies/frame; memory controller load 36 % with 94 % core load fits drain-bound | likely main term, unproven |
| Small draws with full state: 2.7 root-CBV sets, 1 IB set, 0.33 PSO switch per draw. An NVIDIA front end with state changes manages roughly 1-2 M draws/s, about 0.5-1 us per draw | id 25 / 13 / 36 counts | plausible floor |
| Constants read over PCIe: the constant pool is an UPLOAD heap (`command_processor.cpp:1379`, `src/ui/d3d12/d3d12_upload_buffer_pool.cpp:78`), so 8-11 MB/frame of per-draw constants are fetched from system memory with PCIe latency on first touch | HWiNFO bus load 95-100 % at only about 1-1.5 GB/s of payload | possible; unmeasured |
| Primitive conversion (`p_prim` CPU 0.07 us), tessellation | no tessellation lines; conversions are CPU-side | minor |
| Depth float24 conversion in the pixel shader | off (`flags.cpp:23`) | not a factor |
| Sample-rate shading / alpha to coverage | RTV path with host MSAA; no forced sample-rate shading in the RTV path | not indicated |
| ROV | not in use (RTV path) | n/a |
| 4x AF forced (`anisotropic_override=3`) | a per-pixel cost, small at 2.07 MP | minor (section 5) |

## 5. Levers, ranked by expected GPU ms saved and risk

Estimates are labelled as such; only lever 1 has a measured analogue.

| # | Lever | Where | Expected | Risk | Testable how |
|---|---|---|---|---|---|
| 1 | **720p mode** (guest 1280x720, scale 1) | launcher target; `mcla_launcher_controller_win.cpp:941-942` | **-5 to -6 ms** (12.4 ms measured on l53 at about 6 k draws); fps CP-bound about 65-75 | low; softer image on the 4K panel | existing target + arm pc/fast2, **no build** |
| 2 | **Laptop power / MUX mode** (vendor performance profile, Dynamic Boost, dGPU-only display) | none | 0 to -2 ms (clock 84-86 % of max under the 70 W limit); unknown gain if the panel goes through the iGPU (4K frame copy over PCIe) | none | one run in the other mode, no build |
| 3 | **Upload-ahead** `shared_memory_upload_ahead_64` (fewer, larger shared-memory uploads, so fewer COPY_DEST switches) | `src/graphics/shared_memory.cpp:31`; key exists, `src/mcla_dev_overrides.h:672-675`; Deck default since l67 | 0 to -1 ms GPU, some CP | low (Deck default) | new arm file = pc/fast2 + that line, **no build** |
| 4 | `anisotropic_override` -1 (game) or 0 instead of 3 (4x forced) | `texture/cache.cpp:64-76` | -0.2 to -1 ms | low (texture sharpness at oblique angles) | needs a one-line allowlist key in `src/mcla_dev_overrides.h` (host rebuild) |
| 5 | Measurement build (below) | SDK | 0 (decides 6-9) | none | one 5-minute run |
| 6 | ROV path `render_target_path_d3d12=rov`: no dumps, transfers or stencil-bit passes; adds per-pixel interlock | `render_target_cache.cpp:46-48`, `:179-182` | unknown sign, plus or minus several ms | high: untested on MCLA (M4-002 excludes ROV), cold ROV store (`rov.d3d12=unreadable`) | key + build; measurement arm only |
| 7 | Fewer stencil-bit transfer passes on NVIDIA (skip when stencil is unchanged or cleared, write only used bits, clear instead of copy) | `render_target_cache.cpp:4499`, `:4907-4916` | about 320 draws + 200 PSO switches per frame; ms unknown (estimate 0.5-3) | medium (stencil correctness) | code + verify arm |
| 8 | Real direct resolve (read the host RT directly, no EDRAM dump) | `render_target_cache.cpp:5936-5981` | estimate -1 to -3 ms (one RT read plus an EDRAM write and read fewer per resolve, fewer barriers) | medium-high (all resolve formats, MSAA) | code |
| 9 | Constants in a GPU_UPLOAD heap (ReBAR) for the constant pool; a fixed IB view over shared memory with `StartIndexLocation` | `d3d12_upload_buffer_pool.cpp:78`; IASetIndexBuffer per draw | 0 to -3 ms (unknown) | medium (needs OPTIONS16 GPUUploadHeapSupported and ReBAR) | code + arm |
| 10 | Guest patches from the public patch file: Disable MSAA (`0x822E4B80`), Disable Motion Blur (`0x8260D0B8`/`0x8260D0D4`), imposter shadows (`M2-015-xenia-patch-audit.md`) | the host has no patch mechanism; needs a recompiled-code override | MSAA off: estimate -2 to -5 ms (samples in dumps, transfers, resolves); motion blur: a full-screen pass + resolve | medium: visual change, alpha-to-coverage foliage (KI-015) | code |

Not levers: `depth_float24_convert_in_pixel_shader` (already off), `native_2x_msaa` (already
the cheap setting), submissions (gaps are outside `gpu_busy`; the fit coefficient is
negative), read-back elision (already 74 % elided; 0.49 ms left).

### Owner run for lever 1 (first A/B)

- Arm `l103-arm-fast2.cmd` (unchanged). Launcher target **720p**: one 5-minute free-roam on
  the `fb4bf9d9` route, same save, same time of day (game clock about 05-06 h), HWiNFO
  logging on; F10 only at drops. Then 5 minutes at **1080p** on the same route (or reuse
  `fb4bf9d9` as A if the time of day matches).
- Compare per qualifying minute window (more than 600 frames, more than 3000 draws):
  - `MCLA_PERF_SPLIT gpu_busy_ms/frames`, `draws/frames`, `copies/frames`,
    `submissions/frames`, `cp_busy_ms/frames`, fps;
  - the fit `gpu = a + b x draws (+ c x copies)` (`pool.py`);
  - `MCLA_VRAM gpu_mhz`; `MCLA_VRAM_SDK rt_mb`;
  - `MCLA replay/min` barriers and counts by id (ids 9, 19, 23, 35);
  - `MCLA_DIP` counts and minimum fps;
  - HWiNFO core load, memory-controller load, bus load, power, clock and limiters.
- Expected: `a` drops by about 5 ms, `b` stays about the same, fps settles about 65-75 and
  CP-bound (dip hints become cp_draws only).

Lever 2 is the same run shape with the laptop in its highest performance or dGPU-only
mode at 1080p. Lever 3 is the same shape at 1080p with arm `pc/fast2 +
shared_memory_upload_ahead_64=true` (watch `upload_calls`, `CopyBufferRegion` (id 3),
barrier calls (id 19) and `gpu_busy`).

## 6. What cannot be concluded from existing logs

- The split of the 18 ms into resolves/dumps, texture reloads, transfers (and the
  NVIDIA stencil-bit share), shared-memory uploads, the game's own passes and front-end/
  barrier bubbles. `gpu_busy` is one number per submission.
- Whether the per-draw slope is barrier drains, PCIe constant fetches or front-end limits
  (all collinear with draws).
- Which guest MSAA count the main passes use at the 960x540 guest mode, and how many
  transfers there are per frame and of what area.
- Whether the panel is driven by the 860M (Optimus copy of each 4K frame over PCIe, which
  would also explain the bus load) or by the 5060 directly.
- Whether ReBAR and GPU upload heaps are available.

**One extra measurement build would settle most of it** (default-off cvar, one minute line
`MCLA_GPU_CAT`):
- GPU timestamp pairs bracketing each category block in the deferred list (resolve dump,
  resolve copy, texture load, transfer colour/depth/stencil-bit, clear, upload copy,
  read-back copy, guest draws between them), accumulated as ms per frame per category. This
  is about 600 timestamps per frame; approximate where blocks overlap.
- Counters: transfer invocations, stencil-bit passes and transfer host pixels; resolve host
  pixels by guest MSAA; barrier calls by resource class (shared memory, render target,
  texture, scaled resolve, EDRAM); shared-memory COPY_DEST switches.
- Two startup lines: D3D12 OPTIONS16 `GPUUploadHeapSupported`, and the adapter that owns
  the swapchain's containing output (iGPU or dGPU).

## 7. HWiNFO cross-check (`fb4bf9d9`, RTX 5060)

Files copied unchanged from the HWiNFO program folder to `private/owner-hwinfo-20261115/`
(SHA-256): `HWiNFO_LOG_875892234.CSV` 16:46-16:58
(`5cbd2c095b2f6afe135fb926eb08d867af099ebc43a725d75011c54eb87c623a`, covers verify-all
`2e79a43f`), `HWiNFO_LOG_879823609.CSV` 17:51-17:58
(`9320ed8dd27be59028e6741148b51c4e0ce38cff50295cb9e535d0d0c85c6428`, covers fast
`aaf16693`), `HWiNFO_LOG_881153890.CSV` 18:14-18:26
(`7784f8b1a2fa88b2421b77958e233901d53db2c1be0b2a2b419252ed585c5776`, covers fast2
`fb4bf9d9`). There is no HWiNFO file for the 860M sessions (14:13 and 15:40). The table
below uses `881153890` (2 s samples) over the perf-split play windows.

| Window | GPU core load % | MC load % | Bus load % | GPU W (limit 60 nominal / 80 max) | Clock MHz (eff) | Power limiter on | Thermal limiter | GPU / hot spot C | Dedicated VRAM MB | CPU package W / STAPM % / Tctl |
|---|---|---|---|---|---|---|---|---|---|---|
| 18:18:11-19:11 (53.9 fps) | 94.3 | 36.9 | 98.2 | 70.4 | 2619 | 70 % of samples | 0 | 74.2 / 79.6 | 7116 | 33.1 / 95 / 88.5 |
| 18:19:11-20:11 (53.9 fps) | 94.5 | 37.1 | 94.9 | 72.3 | 2571 | 80 % | 0 | 74.4 / 79.8 | 7312 | 31.1 / 89 / 86.1 |
| 18:20:11-21:11 (52.0 fps) | 93.0 | 36.3 | 95.5 | 70.0 | 2609 | 80 % | 0 | 74.5 / 79.7 | 7500 | 34.1 / 97 / 90.1 |
| 18:21:30-22:11 (47.4 fps) | 90.5 | 35.5 | 100 | 69.5 | 2589 | 70 % | 0 | 74.5 / 79.6 | 7764 | 34.1 / 97 / 90.9 |
| 18:23:01-23:16 (dip, about 38-41 fps) | 87.6 | 29.3 | 100 | 65.3 | 2685 | 10 % | 0 | 73.4 / 77.9 | 7825 | 35.0 / 100 / 91.2 |
| 18:24:11-25:11 (50.8 fps) | 91.6 | 35.9 | 94.5 | 69.8 | 2603 | 70 % | 0 | 74.9 / 80.0 | 7887 | 33.4 / 95 / 89.2 |

- The in-log `MCLA_VRAM` agrees: `gpu_mhz` minute averages 2569-2670 with a minimum around
  2400, `gpu_max_mhz=3090`, memory 9001 MHz flat. The same picture is in `aaf16693` (2535-2670).
- Reading: the GPU sits on its power limit (about 70 W) at 84-86 % of its maximum clock,
  is not thermally limited and has low memory-controller load. That is latency- and
  serialization-bound work at a power-capped clock. In the dip (more draws) load and power
  fall and the clock rises, which fits the CP-bound dips found earlier
  (`M11-l100-pc-dips-analysis-20261114.md`).
- CPU package power stays at the 35 W STAPM cap (89-100 %), so a laptop mode that moves
  power to the GPU (Dynamic Boost) would cost CPU headroom; that is why lever 2 is a
  measurement, not a recommendation.
- Dedicated VRAM use reaches 7.1-7.9 GB of 8 GB system-wide (the game's own `usage_mb` is
  4.2-5.9 GB against a 6.7-7.1 GB budget). There is no pressure in the game's census, but
  little headroom for other applications.

## 8. Files

- New: `docs/evidence/M11-gpu-time-research-20261115.md` (this file); copied HWiNFO CSVs
  under `private/owner-hwinfo-20261115/`.
- Read only: the SDK worktree (paths above), `src/mcla_dev_overrides.h`,
  `src/mcla_launcher_controller_win.cpp`, the session logs named above, and
  `docs/evidence/M8-render-targets-20261011.md`, `M10-001-720p-and-ki047-20261101.md`,
  `M10-001-run1-1440p-cold-20261101.md`, `M2-015-xenia-patch-audit.md`,
  `M4-002-render-paths.md`, `M5-003-rendering-categories.md`, and the M11 l96/l100/l103
  evidence. The excluded folders and files were not opened (before-user, after-user,
  save-private, save-files-private.json, *.dmp).
