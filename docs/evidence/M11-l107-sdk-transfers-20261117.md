# l107 SDK: ownership-transfer census, uniform-stencil transfer pruning, forced HiDef, tile-boundary resolve fill (2026-11-17)

**SDK READY: `private/l107-sdk-install`** (CMake version `0.20.0.15-dev.gcb92be8`).

Implemented by an Opus subagent for the primary agent, from the measurements in
[l106 on the RTX 5060](M11-l106-pc-gpu-cat-20261117.md) and [GPU-time research](M11-gpu-time-research-20261115.md)
(ownership transfers a constant 5.65 ms of 18.5 ms per frame; stencil 3.2 ms as 8 per-bit passes because the
adapter has no pixel-shader stencil reference), plus three coordinator additions from the host agent's reverse
engineering (A1 forced HiDef, A2 KI-083 resolve fill, A3 draws per tile). SDK commits on the local branch
`mcla/m11-l107` (parent `6a4b82e` = l106; l105 = `13ef300`): `87076c8` (census, stencil, MCLA_OUTPUT) and
`cb92be8` (`cb92be8ae0141f0d1c38ce0d00db6508b6e2c0df`, the additions); worktree `private/l107-sdk-wt`; not pushed,
no tag. **Built and unit-tested only: nothing has run in the game, there is no physical PASS, and no number here is
measured with l107.** Host files, host gates and owner staging were not touched (a second agent owns them). Spark
not used: deterministic source work checked by unit tests; checking a model answer would cost more.

Licence: nothing was ported. Xenia Canary (BSD-3) was re-checked on 2026-11-17 through the public GitHub commit list
of `src/xenia/gpu/d3d12/d3d12_render_target_cache.cc` (branch `canary_experimental`): the newest commit is
`4ae7c9f538` (2026-10-20, extended EDRAM), i.e. nothing after the 2026-11-15 research
([AA and scaling research](M11-aa-and-scaling-research-20261115.md) section 2): upstream still draws 8 stencil-bit
passes without `SV_StencilRef` and has no pruning. Nothing was taken from GPL or unlicensed peers.

## 0. What shipped

| Goal | Status | Switch (default, phase) |
|---|---|---|
| G1 cause census `MCLA_XFER v=1` | **shipped**, always on, one line per perf-split window | none |
| A1 forced HiDef + `MCLA_VIDEOMODE v=1` | **shipped** | `video_mode_force_hidef` (false, init-only, `Display` group next to `video_mode_width/height/refresh_rate`) |
| A2 KI-083 tile-boundary resolve fill | **shipped**, minimal version; counters in `MCLA_XFER res_fill=` | `resolve_fill_half_pixel_only_at_origin` (false, hot: read at every resolve) |
| G4 `MCLA_OUTPUT` after resizes | **shipped** (`event=create|resize`) | none |
| G2a uniform-stencil skip | **shipped**, default off | `d3d12_stencil_transfer_uniform` (false, hot: read at every transfer batch) |
| G2b skip known bit planes | **shipped as part of G2a** (dirty-bit mask inside the uniform rectangle) | same |
| G2 verify | **shipped**: occlusion-query check of every skipped (or, with the skip off, skippable) bit | `d3d12_stencil_transfer_uniform_verify` (false, init-only) |
| A3 draws per surface key and window offset | **shipped** in `MCLA_GPU_SURF` (`draws_by_key=`) | none |
| G3 depth / colour elision | **census only, no skip** (section 7) | - |
| G2c lazy / deferred stencil elision | **not done** (section 7) | - |
| G5 `MCLA_GPU_CAT` overhead | recommendation only (section 6) | `d3d12_gpu_category_timing_every` |

With all defaults rendering is unchanged: the census and the audit only count, the stencil plans fall back to the
l106 path (clear to 0, all 8 passes), the video mode and the resolve fill are as before. The only default-on
change in behaviour is logging: `MCLA_XFER` per window, `MCLA_VIDEOMODE` once, `MCLA_OUTPUT` after resizes, the new
`MCLA_GPU_SURF` fields.

## 1. G1: MCLA_XFER v=1 (transfer census)

Code: pure part `include/rex/graphics/util/transfer_census.h` (new: `ClassifyXferCause`, `StencilUniformState`,
`PlanStencilTransfer`, `XferRoundTripTracker`, `XferCensus`, `FormatXferLine`); recording in
`src/graphics/d3d12/render_target_cache.cpp` `PerformTransfersAndResolveClears` :4735 (one record per `Transfer`
of every destination, before the draws); the draw trigger and overwrite candidates in `Update` :1279; write hooks
`OnDrawRenderTargetWrites` :1187, called from `command_processor.cpp` :5389 right after `UpdateRenderTargetsMemo`
(also after an l101 memo hit), trigger set before the update :5381; dumps mark the source resolved :6460; resolve
clears are writes :5402; resolves counted for A2 :1431; the line after `MCLA_GPU_SURF` in `PerfSplitEmit`
(`command_processor.cpp` :4717), window reset after printing (`TakeXferWindowLine` :1210).

Line (one per window, minute and F10, after `MCLA_GPU_SURF`):

```
MCLA_XFER v=1 reason= seq= frames= n= color= depth= samples=color:,depth: sigs= overflow=
  causes=reinterp:,pitch:,msaa:,fmt:,base:,same: trig=draw:,rect:,rectfull:,rclear: ow= rt= rt_clean=
  after_res= hd= st=xfers:,uni:,bits_skippable:,bits_skipped:,on: verify=on:,checks:,bad:,lost:
  res_fill=n:,filled:,suppressed: cat_ms_pf=color:,depth:
  top=<sig>:<n>:<samples>:<ms_est>:ow<n>:rt<n>/<clean>:res<n>:su<n>|...
```

| Field | Meaning |
|---|---|
| `reason`, `seq`, `frames` | as MCLA_PERF_SPLIT; all counts are window totals (divide by `frames`) |
| `n`, `color`, `depth` | Transfer entries (one source RT, one destination RT, one EDRAM tile range) by destination kind; the same entries as `xfer=color/depth` of MCLA_GPU_CAT |
| `samples` | destination host samples transferred: rectangle pixels (with the resolve-clear cutout) x draw scale x host samples (2x emulated as 4x counts 2) |
| `sigs`, `overflow` | distinct signatures in the 64-slot window table; records that found no slot (totals stay exact) |
| `causes` | first difference between source and destination keys in this order: `reinterp` colour<->depth, `pitch` a different pitch (tiling or another surface width over the same EDRAM), `msaa` same pitch other MSAA, `fmt` same pitch and MSAA other format (depth d24s8<->d24fs8, colour format), `base` only the EDRAM base, `same` identical (not expected) |
| `trig` | what needed the transfer: `draw` a draw that is not a rectangle list, `rect` a rectangle-list draw narrower than the surface pitch, `rectfull` a rectangle list whose guest scissor is at least the pitch wide (the game's clear / full-screen quads), `rclear` the ring around a resolve clear |
| `ow` | overwrite candidates: the triggering draw replaces the destination values (colour: all 4 mask bits and blend ONE/ZERO/ADD for colour and alpha; depth: z test ALWAYS with z write). Coverage of the transferred rectangle, pixel kill, alpha test and the stencil part are **not** checked: an upper bound for "result overwritten before it is read" |
| `rt`, `rt_clean` | round trips: this transfer B->A overlaps one of the last 64 transfers A->B; `clean` when its range is inside that one and neither A nor B was written since (write sequence: a draw with a non-zero colour mask, z write or stencil while bound; a transfer into it; a resolve clear). A clean round trip is the case where A still holds the data |
| `after_res` | the source was dumped by a resolve and not written since ("transfer after resolve") |
| `hd` | transfers with a separate host depth source |
| `st` | transfers that use the per-bit stencil path (`xfers`), of those with a source stencil known uniform (`uni`, candidates; counted with the switch off too), the bit passes the plans could skip (`bits_skippable`) and did skip (`bits_skipped`, 0 with the switch off), `on` the switch |
| `verify` | `on` the verify is latched; `checks` per-bit occlusion counts compared; `bad` counts contradicting the uniform assumption; `lost` checks not made (ring of 1024 queries full, verify pipeline missing) |
| `res_fill` | A2: resolves with valid resolve info (`n`), with the half-pixel fill (`filled`), with the fill suppressed by `resolve_fill_half_pixel_only_at_origin` (`suppressed`) |
| `cat_ms_pf` | GPU ms per measured frame of `xfer_color` and of `xfer_depth + xfer_stencil` in the same window when `d3d12_gpu_category_timing` measured frames, else `na` |
| `top` | up to 8 signatures by samples (then count, key). `<sig>` = `<cause>.<trigger>[.hd].<source>><dest>`, each RT as `c|d<format>m<samples>p<pitch tiles at 32bpp>b<EDRAM base>` (formats: depth 0 = D24S8, 1 = D24FS8; colour as `xenos::ColorRenderTargetFormat`: 0 8_8_8_8, 1 GAMMA, 2 2_10_10_10, 3 2_10_10_10_FLOAT, 4 16_16, 5 16_16_16_16, 6 16_16_FLOAT, 7 16_16_16_16_FLOAT, 10 2_10_10_10_AS_10_10_10_10, 12 2_10_10_10_FLOAT_AS_16_16_16_16, 14 32_FLOAT, 15 32_32_FLOAT); then count, samples, `ms_est` (the category ms per frame split by the signature's share of that category's samples: assumes cost proportional to samples; `na` without timing) and its `ow`, `rt`/clean, `after_res` (`res`), stencil-uniform (`su`) counts |

Cost: integer work per Transfer (tens per frame), four register reads per draw that needs transfers, and a
sequence update for up to 5 bound RTs per draw. No GPU work.

Reading it for G3: a large `trig=rectfull` signature with `ow` near its count is the "clear drawn as a rectangle"
case; `rt_clean` > 0 is the round-trip cache case; `after_res` marks transfers of already-resolved data.

## 2. A1: video_mode_force_hidef and MCLA_VIDEOMODE

- Cvar `video_mode_force_hidef` (bool, default false, init-only, `Display`; `src/ui/window.cpp` :61, declared in
  `include/rex/ui/flags.h` :38).
- One derivation for every flag computed from the mode: `video_mode_util::DeriveVideoMode`
  (`include/rex/graphics/video_mode_util.h` :133): `is_widescreen` = `w*3 >= h*4` (unchanged rule),
  `is_hi_def` = forced or `w >= 1280 || h >= 720`, `VdQueryVideoFlags` bits = 1 widescreen, 2 HiDef
  (`w >= 1024` or forced), 4 `w >= 1920`. Used by `VdQueryVideoMode` (`src/kernel/xboxkrnl/xboxkrnl_video.cpp` :240)
  and `VdQueryVideoFlags` (:277). XGetVideoMode (`xam_video.cpp`) and all host users (graphics system vblank,
  command processors) call `VdQueryVideoMode`, so they see the same values. Width, height, refresh,
  `is_interlaced = 0` and `VdGetCurrentDisplayInformation` are unchanged.
- Start-up line, once (first `VdQueryVideoMode`):
  `MCLA_VIDEOMODE v=1 width= height= hidef= forced_hidef= widescreen= interlaced=0 flags=0x<VdQueryVideoFlags> refresh=<Hz>`.
- Not checked: whether the game reads other SD/HD sources (AV pack, `XGetVideoStandard`); the line and the
  renderer bytes in a run will tell whether the 4x/two-tile path goes away.

## 3. A2: KI-083 tile-boundary half-pixel fill (`resolve_fill_half_pixel_only_at_origin`)

- Decision `draw_util::ResolveFillHalfPixelOffset` (`include/rex/graphics/util/draw.h` :597), used in
  `GetResolveInfo` (`src/graphics/util/draw.cpp` :1037): the fill applies as before (scaled resolve,
  `resolve_resolution_scale_fill_half_pixel_offset`, `half_pixel_offset`, D3D pixel centre); with the new cvar it is
  suppressed when the **applied** window offset (the one added to the resolve rectangle, i.e. with
  `vtx_window_offset_enable`) is not (0, 0). `ResolveInfo::fill_half_pixel_suppressed` (:516) feeds
  `MCLA_XFER res_fill=` (`render_target_cache.cpp` :1431). One bit, no shader change.
- Safe as default true? **Not yet.** The fill bit covers both edges: for the second tile (offset (0, -320)) it also
  removes the fill of the left edge, which is a real surface edge; at 2x2 scale that may leave a one-host-pixel
  column on the left of the lower tile's rows. The top edge of that tile is not a surface edge (the geometry
  continues above it, clipped by the window), so removing its fill is correct. Recommend an owner A/B first
  (seam at rows 640-642 against the left column); the exact fix is a per-axis fill (separate x / y bits in the
  resolve constants and shaders), not done here.

## 4. G4: MCLA_OUTPUT after resizes

`src/ui/d3d12/d3d12_presenter.cpp` :85 / :495: after every successful `ResizeBuffers` (window resize or the
borderless fullscreen transition) the line is logged again with the monitor the window is on at that moment
(`MonitorFromWindow`) and the new swap-chain size; new trailing field `event=create|resize`
(`gpu_category_timing.h` :689; the l105 test updated). At most 32 resize lines per session (a dragged window edge
cannot flood the log). A move to another monitor without a size change is not logged (no swap-chain event).

## 5. G2: uniform stencil (`d3d12_stencil_transfer_uniform`)

- State per depth render target (`D3D12RenderTarget::stencil_uniform()`): a resolve clear (host
  `ClearDepthStencilView` of depth **and** stencil over the clear rectangle) sets `rect` = the clear rectangle
  clipped to the texture (guest pixels of that RT), `value` = clear & 0xFF, `dirty = 0`, `epoch`
  (`render_target_cache.cpp` :5425). Every draw with stencil enabled while the RT is bound ORs both stencil write
  masks (`RB_STENCILREFMASK` and `_BF`; the host pipeline uses one of them) into `dirty` (:1187, after the
  render-target update also on an l101 memo hit, because the write masks are not memo inputs). Any transfer into
  the RT invalidates it (:4735 block, after the plans); an EDRAM snapshot restore bumps the epoch (:1839). These are
  all writers of a host depth RT's stencil on the host-RT path (draws, transfers, resolve clears; dumps and the
  host depth store only read it / write the EDRAM buffer).
- Plan (`PlanStencilTransfer`): for a depth->depth transfer on an adapter without PS stencil reference, the source
  rectangles of the tile range in the **source** RT (`GetXferSourceRects` :1225, split at the source base for EDRAM
  wrapping) must all lie inside the source's uniform rectangle at the current epoch. Then every source sample
  equals `value` in every bit outside `dirty`: the destination rectangles are cleared to `value & ~dirty` (one
  `ClearDepthStencilView` per distinct clear value, :4916; normally only 0) and only the bit passes of `dirty` run
  (:4882, counters :5040; none when nothing was written). Colour->stencil transfers and non-uniform sources keep
  the l106 path. G2b is the dirty mask.
- Verify (`d3d12_stencil_transfer_uniform_verify`, init-only; queries and pipelines only when on and the adapter
  uses the bit passes, :397, :4303): for every bit the candidate plan skips, that bit's stencil-bit shader is drawn
  over the transfer rectangles with depth and stencil disabled (nothing written) inside an occlusion query (:5288);
  it keeps exactly the samples whose source bit is 1. The count must be every covered sample (pixels x scale x host
  samples) when the assumed bit is 1 and 0 when it is 0. Read when the submission's fence passed
  (`StencilVerifyCollect` :1161): `verify=checks/bad/lost` and the first 16 mismatches as
  `MCLA_XFER_VERIFY v=1 result=bad bit= assumed= expected= got= src=<desc> dst=<desc>` (warning). With the skip
  **off** the verify still checks the skippable bits and changes nothing else: audit run =
  `d3d12_stencil_transfer_uniform=false` + `_verify=true`; A/B run = both true. About as costly as the skipped passes;
  its draws fall in `xfer_stencil` of MCLA_GPU_CAT but not in `stencil_bit_passes`.
- MCLA_GPU_CAT `stencil_bit_passes` now counts the passes drawn (popcount of the plan); `stencil_bit_xfers` counts
  invocations with at least one pass.

## 6. A3 and G5

- A3: `GpuSurfaceAudit::OnDraw` (`gpu_category_timing.h` :507), called per issued draw with the current key
  (`command_processor.cpp` :5833). `MCLA_GPU_SURF` gets three trailing fields (:4714,
  `TakeDrawsByKeyField(6)` :529): `draws_by_key=<key>:<draws>;...` (top 6 keys by draws in the window, key format
  `p<pitch>m<samples>c<colour base>d<depth base>o<x>,<y>` as `last_frame_keys`), `draw_keys=` distinct keys (at most
  32), `draws_overflow=` draws whose key found no slot. Draws per tile = the counts of the main surface key at
  `o0,0` and `o0,-320`.
- G5 (no code change): the command hook and timestamps exist only in sampled frames, so the CP cost (about 5 ms per
  measured frame at about 2300 timestamps) averages to 5/N ms with `d3d12_gpu_category_timing_every=N`.
  Recommended `every=4` for category runs (about 1.25 ms average, at 45 fps about 11 measured frames per second);
  timing **off** for fps comparisons. MCLA_XFER needs no timing except its `ms_est` / `cat_ms_pf`.

## 7. Deliberately not done

- **G2c lazy elision**: the ownership bookkeeping records owners, not reads; proving "never read before the next
  clear" needs a deferred transfer resolved at the next reader (draws with stencil test, depth resolves, transfers
  out, dumps). Too large to do safely blind.
- **G3 skips**: no design is provably safe without run data. Resolve clears already cut the cleared area out of
  their transfers; for draw-triggered clears the proof needs the draw's covered area (rectangle-list vertices) and
  pixel kill, which the RT cache does not have; the round-trip cache needs `rt_clean` to be non-trivial (the
  triggering draw usually writes B). The census now measures all three (`ow` by `trig=rectfull`, `rt_clean`,
  `after_res`).
- **A2 per-axis fill** (section 3) and Xenia ports (nothing new upstream).

## 8. Tests, build, install

- New `tests/unit/graphics/transfer_census_test.cpp` (8 cases, `[l107]`): key packing, cause classification,
  triggers; rectangle containment and clipping; uniform-stencil state machine and invalidation; transfer plans
  (not tracked, clean, partly outside, wrong epoch, dirty bits, all dirty, verify expectation); round trips (clean
  rule, writes since, containment, direction, ring eviction, reset); census accounting (signatures, overflow beyond
  64 slots with exact totals, top order, text, reset); the MCLA_XFER line; resolve-fill counters. New
  `tests/unit/graphics/video_mode_hidef_test.cpp` (2 cases): HiDef / widescreen / flags derivation with and without
  the switch (960x540 SD -> forced HiDef, 1280x720, 1920x1080, 1024x600); the fill decision (default unchanged,
  tile 1 kept, tile 2 suppressed, no-fill cases not counted). `gpu_category_timing_test.cpp`: +1 case
  (draws_by_key: two tiles, overflow) and the MCLA_OUTPUT `event=` expectation.
- Build: fresh `private/l107-sdk-b` with the l105 options (Ninja Multi-Config Release, clang 20.1.8, VS 2022 build
  tools, `REXGLUE_ENABLE_FFX_PRESENTER_SHADERS=ON`, Tracy and perf counters on, Vulkan off), output
  `private/l107-sdk-o`; third-party submodule trees copied into the worktree from `private/l106-sdk-wt` (as l105
  did). Builds without error; normalized warning set **212 entries, 0 new, 0 gone** against l105 (union of the
  three build logs).
- `git-clang-format` against the parent of each commit (include, src, tests): applied, rebuilt, clean.
- ctest (full suite, `-j 8`): after `87076c8` 2334 of 2334 three times; after `cb92be8` **2338 of 2338 passed three
  times in a row** (2334 passed + 4 skipped BitStream; l106 2327 + 11 new), and once more after the install.
- Install `private/l107-sdk-install`, CMake `0.20.0.15-dev.gcb92be8`; adds
  `include/rex/graphics/util/transfer_census.h`.

| bin file | Bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11776000 | 2B264954DC466DB19CDE27CD7A2B866D693D0AF66AA9151445393225E9789B07 |
| rexgpu-xenos.dll | 3678720 | BBB51A4EF4E41613B0115DB10093790903259C05B993986C31A8B773243FFBEE |
| TracyClient.dll | 246784 | B769F7E52FF233197B1B823E2D3091FA8D75AAF18AE0D18EF7EB121797E9C4CF |
| rexglue.exe | 4113920 | 8CE13FB512E177C738072214D25980AEA23C9BA15D3895E18DCB0BD70C803E5E |

TracyClient.dll has no source change but a new hash (fresh build tree). **ABI:** `D3D12RenderTargetCache`,
`D3D12CommandProcessor`, `GpuOutputInfo`, `GpuSurfaceAudit` and `draw_util::ResolveInfo` changed: the host must be
rebuilt against this SDK. The host needs no code change; the new cvars are SDK cvars (`video_mode_force_hidef`,
`resolve_fill_half_pixel_only_at_origin`, `d3d12_stencil_transfer_uniform`, `d3d12_stencil_transfer_uniform_verify`).

SDK diff `6a4b82e..cb92be8`: 17 files, 1696 insertions, 31 deletions.

## 9. Risks

- No game run: the census, the skip, the verify, the forced HiDef and the fill switch are unobserved.
- Forced HiDef changes what the game believes about the output (SD-specific state, camera / LOD factors, MSAA and
  tiling choice per the host agent's analysis): a gameplay-visible switch; A/B it alone.
- Stencil: first run the audit (verify on, skip off): `st=uni` says whether stencil transfers come from cleared
  sources at all, `verify=bad` must be 0 before the skip is tried. If `st=uni` stays 0 (sources cleared by
  rectangle draws, not resolve clears) the skip gains nothing; `trig`/`ow` then show the clears. The occlusion
  check measures the source bits, not the destination clear rectangles (they reuse the existing stencil-clear
  rectangles); image comparison remains the final check.
- `ow` and `rt_clean` are candidates, not proofs; `ms_est` assumes cost proportional to samples.
- A2 suppresses both fill edges (section 3).
- The evidence file could not be written into `docs/evidence/` by the subagent (shared-checkout write guard); it is
  in the scratch folder below for the primary agent to copy.
- Scratch: `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l107-sdk-work` (`sdk-build.ps1`, `sdk-test.ps1`,
  `sdk-install.ps1`, `copy_tp.ps1`, `normwarn.py`, `edit_lib.py`, `sdk-commit-msg*.txt`; logs `sdk-configure.log`,
  `sdk-build1..3.log`, `ctest1..6.log`, `ctest-install*.log`, `sdk-install*.log`, `w107all.txt`).
