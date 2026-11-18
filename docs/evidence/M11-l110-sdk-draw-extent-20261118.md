# l110 SDK: draw extent of rectangle-list draws bounded by their interpreted rectangle (2026-11-18)

**SDK READY: `private/l110-sdk-install`** (CMake version `0.20.0.18-dev.gc403f35`).

Implemented by an Opus subagent for the primary agent, from the owner's l109 results (session
c81fe57b `pc/refuse-audit`: 19 proven rectangle draws per frame, 0 bad of 100,738 checks, but only
15 % of the transferred samples cut; session a420b57b `pc/elide-gpu-cat`: transfers 8.15 -> 7.11 ms
per frame). SDK commit on the local branch `mcla/m11-l110` (parent `4a29e93` = l109): `c403f35`
(`c403f3542b85a3facc06ac0931093e92bd2f8ae5`), worktree `private/l110-sdk-wt`; not pushed, no tag. **Built and unit-tested only:
nothing has run in the game, there is no physical PASS, and no number here is measured with l110.**
Spark not used: deterministic source work checked by unit tests; checking a model answer would cost
more than writing and testing the code.

Licence: no code was copied. The new code reuses, inside this SDK, the vertex mapping of the l108
proof and the Xenia `ShaderInterpreter` / `DrawExtentEstimator` (BSD-3, already in the tree). The
public Xenia Canary source was read only to compare its `draw_extent_estimator.cc` (BSD-3) with ours
(section 1); nothing was ported. Nothing was taken from GPL or unlicensed peers.

## 0. What shipped

| Goal | Status | Switch (default, phase) |
|---|---|---|
| E1 why the clear rectangles claim the whole owned extent | **answered** (section 1) | - |
| E2 bound the extent by the interpreted rectangle | **shipped** for clip-disabled rectangle lists of 3 auto-indexed vertices; rows only (section 2) | `d3d12_draw_extent_from_rect` (false, hot: read at every rectangle-list draw) |
| E3 non-rectangle unclipped draws (CPU vertex execution) | **not built**; opportunity reported from the l109 data (section 3) | - |
| E4 verify and counters | **shipped**: savings counted with the old extent, a two-part draw with an occlusion query on the rows below the bound | `d3d12_draw_extent_from_rect_verify` (false, init-only; wins over the bound) |
| E5 consistency (scale, half pixel, window offset, memo, resolves, snapshot) | **checked** (section 5) | - |

With both switches off nothing changes: the rectangle is not interpreted, the common `Update` gets
no bound (`UINT32_MAX`), the draw is issued in one part, and no line is written.

## 1. E1: how the extent is decided, and why the clears claim everything

**Our SDK at 4a29e93.**

- `RenderTargetCache::Update` (`src/graphics/pipeline/render_target/cache.cpp` :570-578 at 4a29e93)
  takes `height_used = min(GetRenderTargetHeight(pitch, msaa), DrawExtentEstimator::EstimateMaxY(...))`
  and claims, per bound render target, `ceil((height_used << (msaa >= 2x)) / 16) * pitch_tiles`
  tiles from the render target's EDRAM base (:631-662), each clamped to the next render target's
  base; `ChangeOwnership` (:1284-1432) then records a transfer for every sub-range another render
  target owns. Ownership ranges are contiguous tiles (80x16 samples at 32bpp) from the base, i.e.
  whole rows of tiles of the pitch; there is no column bookkeeping.
- `DrawExtentEstimator::EstimateMaxY` (`src/graphics/util/draw_extent_estimator.cpp` :263-342):
  the scissor bottom (window offset applied, clamped by the screen scissor); with clipping **on**
  the viewport bottom (half-pixel and window offset, |y scale| + y offset); with clipping **off**
  the vertex shader runs on the CPU (`EstimateVertexMaxY` :81-261) **only if**
  `execute_unclipped_draw_vs_on_cpu` is true **and** either
  `execute_unclipped_draw_vs_on_cpu_with_scissor` is true or the scissor's bottom and right are both
  at least 8192 (:285-308).
- In our SDK `execute_unclipped_draw_vs_on_cpu` is defined **false** (:27) and
  `execute_unclipped_draw_vs_on_cpu_with_scissor` false (:30); upstream's comment block (:33-58)
  shows Xenia's default true. The host render-target path passes `try_to_estimate_vertex_max_y =
  true` (`cache.cpp` :574-577).
- The game's clears (`MCLA_XFER_REFUSE` in c81fe57b: no `z=`, so clipping is off; host rectangles
  `r=0,0,240,256` with scissor `0,0,320,16384`, `0,0,960,528` / `0,0,960,1072` with `0,0,960,16384`,
  the strip `960,0,1024,512` with `0,0,1040,16384`; host pixels at scale 2) have a guest scissor
  8192 rows high but only 160 / 480 / 520 guest pixels wide. So: clipping off -> the viewport is not
  used; the CPU vertex path is off by cvar; and even with Xenia's default (true) the 8192 x 8192
  condition fails because the scissor right is below 8192. `EstimateMaxY` returns 8192, the
  render-target height caps it (for a pitch of 4 tiles at 4x the whole 2048-tile EDRAM is 512 tile
  rows), and the claim covers every row up to the next render target's base - the whole extent the
  previous owner held (for example 320x1024 host for a 240x256 clear). Rectangle lists are not
  excluded anywhere; the estimator is applied to transfers (it *is* the transfer extent); it simply
  never sees the vertices of these draws.

**Xenia Canary upstream today** (`src/xenia/gpu/draw_extent_estimator.cc`, canary_experimental,
read 2026-11-18): the same algorithm; `execute_unclipped_draw_vs_on_cpu` defaults **true**,
`execute_unclipped_draw_vs_on_cpu_with_scissor` **false**; no rectangle-list handling, the X extent
only in the 8192 test, no minimum Y. So upstream has the same behaviour for these clears; it would
bound them only with `..._with_scissor` on, which runs the CPU vertex shader for every unclipped
draw (all vertices, every primitive type). Nothing to port.

## 2. E2: design

`include/rex/graphics/util/draw_extent_rect.h` (new, pure):

- `ComputeRectExtent(const ElideDrawState&)`: conditions: rectangle list, exactly 3 auto-indexed
  vertices, not tessellated (`rect`); clipping off (`clip`); vertex shader interpretable, i.e. no
  texture fetch (`tfetch`); every position exported, not killed, W finite / positive / equal,
  finite host positions (`vs`); three distinct corners of an axis-aligned rectangle (`corners`).
  Host positions exactly as `ProveFullOverwrite` maps them (`draw_util::GetHostViewportInfo`
  ndc_scale / ndc_offset and the host viewport: resolution scale, half-pixel offset, window offset
  and the clip-disabled huge viewport are all inside).
- The bound: `bottom_hi` = the largest possible snapped bottom edge over the 3 vertices
  (`ElideSnapEdge(y).hi`: 1/256 snapping with the CPU / GPU rounding range of 1/1024 host pixel or
  4 ulps); every covered sample lies in a host row r < bottom (bottom exclusive under the top-left
  rule, every sample of a pixel inside the pixel), so `host_rows = ceil(bottom_hi / 256)` (outward
  to whole host pixels: at most one host pixel beyond the edge, independent of the sample
  positions) and `guest_rows = max(1, ceil(host_rows / scale_y))`. The pixel stage does not matter
  (kill, tests, blending only remove writes). Only the bottom: ownership is rows from the base.
- `RenderTargetCache::SetNextUpdateExtentBound(rows, apply)` (`cache.h`): the next `Update` caps
  `height_used` to `min(estimate, rows)` when `apply`, and always records the estimated height, the
  bounded height and per render target the claimed and the bounded length in tiles
  (`last_update_*`). Rows below the bound are not claimed and keep their previous owner - exactly
  what happens today for any draw with a smaller scissor or viewport; the next draw that needs them
  (with its own extent) transfers them then, resolves read each range from its owner.
- `D3D12RenderTargetCache::Update`: for a rectangle-list draw on the host render-target path, with
  the bound or its verify on, before the common `Update`: `GatherExtentDrawState` (the l108 proof
  state; the 3 vertices interpreted whenever the bound can use them), `ComputeRectExtent`, and the
  bound (applied only with `d3d12_draw_extent_from_rect` on and the verify off). After it: `bounded`
  when the bound is below the estimate, otherwise `nogain`. The l108 / l109 proof then runs on the
  same state (`ProveFullOverwrite(extent_state_)`: identical result, the proof reads the vertices
  only after every other condition passed) instead of interpreting the vertices a second time.
- Applies to all interpretable rectangle draws, overwriting or not. With the elision on, the proven
  cut applies inside the bounded transfers as before.

## 3. E3: non-rectangle unclipped draws

Not built. From c81fe57b (`MCLA_XFER`, last minute window, 2563 frames): `trig=draw:28935` of
203,355 transfers (14 %; 11 per frame); the largest draw-triggered signature
`msaa.draw.d1m4p12b0>d1m2p12b0` 2393 transfers, 10.45e9 samples, 0.444 ms per frame (the 4x -> 2x
scene depth round trip on an ordinary draw), next in `MCLA_XFER_TRIG`
`msaa.draw.c3m4p12b1008>c3m2p12b1008` (`p2v32`: raw xenos primitive 2, a line list, up to 32
vertices; scissor full) 1424, 5.95e9, 0.04 ms. The draw class does not record clipping, so whether these draws are unclipped (and
whether a CPU vertex bound would shrink them) is unknown; with clipping on the viewport already
bounds them. The upper bound of the opportunity is therefore about 0.5 ms per frame, and only if
those draws are unclipped and smaller than their scissor. A port would be enabling
`execute_unclipped_draw_vs_on_cpu(_with_scissor)` (already in the tree), which runs the CPU vertex
shader over every vertex of every unclipped draw; not done without data.

## 4. E4: verify and counters

`d3d12_draw_extent_from_rect_verify` (init-only; an occlusion query heap of 1024 and a mapped
read-back buffer, `render_target_cache.cpp` Initialize): ownership and transfers use the old extent
(the bound is computed but not applied) and:

- per bounded draw, in `PerformTransfersAndResolveClears`, each transfer into its render targets is
  clipped to the bounded length (`ExtentTilesBeyond`, EDRAM wrap included): the tiles beyond are what
  the bound would leave out (`shrunk`: a tail; `dropped`: all of it), their host samples by plane
  (`Transfer::GetRangeRectangles` of the left-out tiles x scale x samples; stencil = the area of the
  per-bit passes on adapters without a pixel-shader stencil reference; the number of passes is only
  saved for a dropped transfer: `st_passes_dropped`);
- the draw command is issued in two parts (`DrawVerifyBegin` / `BeginPart` / `EndPart` / `End`,
  `command_processor.cpp` at both draw sites): part 0 with the host scissor cut to the rows above
  `guest_rows x scale_y`, part 1 with the rows below; the scissors are disjoint and together the
  original one, so every sample is written exactly once as before (two triangles of a rectangle do
  not overlap). An occlusion query around part 1 must count 0. With the elision verify also on, its
  query is taken around part 0 and the sum of both queries is compared with its expectation (one
  query open at a time: D3D12 queries do not nest). Not measured (`lost`) while a guest occlusion
  query is open or the ring is full. Read when the submission's fence passed
  (`ExtentVerifyCollect`, after `ElideVerifyCollect`).

```
MCLA_XFER_EXTENT v=1 reason= seq= frames= on= verify= considered= bounded=
  refuse=rect:,clip:,tfetch:,vs:,corners:,nogain: rows=before:,after: xfers= shrunk= dropped=
  samples= saved=color:,depth:,stencil: st_passes_dropped= check=n:,ok:,bad:,lost:
MCLA_XFER_EXTENT_VERIFY v=1 result=bad got= rows= host_row= scissor=l,t,r,b   (warning, first 16)
```

`considered` rectangle-list draws evaluated (every `Update` of one, transfers or not), `bounded`
those whose bound is below the estimate, `refuse` the reason otherwise, `rows` guest rows of the
estimate and of the bound (bounded draws), `xfers` / `samples` the transfers into the render targets
of bounded draws (with the bound on: what remains), `shrunk` / `dropped` / `saved` /
`st_passes_dropped` only with the verify, `check` the verify. Written after `MCLA_XFER_ELIDE` while
either switch is on. Window totals like `MCLA_XFER`.

## 5. E5: consistency

- Resolution scale, half-pixel offset, tiled-rendering window offset: inside the host positions
  (`GetHostViewportInfo`), rows converted with `ceil(host / scale_y)`; unit-tested at scales 1-3,
  with and without the half-pixel offset, with a window offset of -320.
- l101 RT Update memo: the bound applies only with clipping off; the memo never skips such a draw
  (`RtMemoMiss::kCpuVs`, `cp_l101_levers.h`), so the memo inputs need no extent; a following clipped
  draw differs in `PA_CL_CLIP_CNTL` and misses as well.
- Resolves and resolve clears: they read / claim ranges from the ownership map, which stays exact
  (the unclaimed rows keep their owner and its data).
- EDRAM snapshot restore: claims the whole EDRAM for its own render target; unaffected.
- Pixel-shader interlock (ROV) path: the bound is never set (`extent_bound_apply` also requires the
  host render-target path in the common `Update`).
- A draw that is not issued after the update (`RestoreElidedTransfers`) performs the update's
  transfers, which are already the bounded ones.

## 6. Tests, build, install

- New `tests/unit/graphics/draw_extent_rect_test.cpp` (7 cases, `[l110]`, 6388 assertions):
  the rows of the game's clears (the four `MCLA_XFER_REFUSE` examples, scales 1 / 2 / 3, without the
  half-pixel offset, an edge inside a host pixel, the second tile with a window offset, a rectangle
  above the render target; each for 1x / 2x / 4x and checked against the l109 sample-coverage
  arithmetic: no covered sample at or below the bound); the bottom edge at every 1/64 host pixel
  over plus and minus 2 host pixels at scales 1-3 (never cuts a covered sample, never more than one
  host pixel beyond the edge, guest rows exact); an edge exactly on a row, 1/256 beyond, within the
  rounding; the rows helper with its clamps; corner orders, agreement with the l109 proof's host
  edges, the pixel stage ignored; 16 refusal rows and clip-space positions; the ownership-range
  arithmetic (tile lengths for 1x / 2x / 4x, transfer tails inside / straddling / beyond / at the
  bound / wrapped / base above the EDRAM size, the 240x256 clear inside a 320x1024 extent: 192 of
  256 tile rows of 4 tiles left with the previous owner); the exact `MCLA_XFER_EXTENT` line.
- Build: fresh `private/l110-sdk-b` with the l109 options (Ninja Multi-Config Release, clang 20.1.8,
  VS 2022 build tools, `REXGLUE_ENABLE_FFX_PRESENTER_SHADERS=ON`, Tracy and perf counters on, Vulkan
  off), output `private/l110-sdk-o`; third-party submodule trees copied into the worktree from
  `private/l109-sdk-wt`. Normalized warning set (union of the full, post-edit, post-format and install
  build logs) **211 entries, 0 new, 0 gone against l109**.
- `git-clang-format` against `4a29e93` (include, src, tests) and `clang-format -i` on the two new
  files: applied, rebuilt, then "clang-format did not modify any files"; `git diff --check`: clean.
- ctest (full suite, `-j 8`): **2362 of 2362 passed** (l109 2355 + 7 new; the 4 BitStream skips
  counted as in l109) three times in a row before formatting and three times in a row after
  formatting and the rebuild.
- Commit `c403f35` on `mcla/m11-l110`, parent `4a29e93`; 9 files, 1161 insertions, 69 deletions (`draw_extent_rect.h` 259 new, `render_target_cache.cpp` +285 / -32, `command_processor.cpp` +101 / -33, the D3D12 headers +74 / -4, the common `cache.h` / `cache.cpp` +56, tests 386). Not pushed, no tag.
- Install `private/l110-sdk-install`, CMake `0.20.0.18-dev.gc403f35`; adds
  `include/rex/graphics/util/draw_extent_rect.h`.

| bin file | Bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11776000 | 1B2C80ED329A612C6DCABF1938548BCC1E5BCC0E8A394A0D1EE03F616894897D |
| rexgpu-xenos.dll | 3796480 | FEDDACB6B0C6381E6419E67CF4ED1AB25735D6A8D0D0D67D4BAE88867E65587B |
| TracyClient.dll | 246784 | 0217F7BAF7A82544477A03632998A099CDB35A04ECF93B623B052952234166D8 |
| rexglue.exe | 4113920 | 075DAD05BCDBA58C1152E1180DB316248640719E9E79E23B74285EECFB0427AD |

rexruntime.dll and TracyClient.dll have no source change but new hashes (fresh build tree, version
string).

**ABI:** `RenderTargetCache`, `D3D12RenderTargetCache` and `D3D12CommandProcessor` changed: the host
must be rebuilt against this SDK. New SDK cvars: `d3d12_draw_extent_from_rect` (hot),
`d3d12_draw_extent_from_rect_verify` (init-only), both default off.

## 7. Risks

- No game run. Run 1 (both verifies, `bad` must stay 0) comes before any run with the bound on.
- The bound trusts that the GPU sees the same vertex data and computes the same positions as the
  CPU interpreter (as the l108 / l109 proof; 100,738 checks were exact). Vertex data written by the
  GPU (a resolve or memexport into the vertex buffer) would not be visible to the CPU; the verify
  measures it.
- A wrong bound shows as stale data in the unclaimed rows (the bottom of a surface: shadows,
  reflections, the bottom screen edge), and only after a later draw reads those rows from the old
  owner: the by-eye run looks for exactly that.
- The verify doubles the draw command of each bounded draw (about 20 to 30 per frame) and adds one
  query; measurement only.
- Interpretation cost: 3 vertices per rectangle-list `Update` while a switch is on (no longer only
  for draws that need transfers); the elision proof reuses them.
- Only the rows are bounded. Rectangles not starting at row 0 (the bottom strips, `r=0,1056..`) gain
  nothing; a minimum-row bound would need a nonzero start of the ownership change and was not
  needed for the dominant clears.
- Scratch: `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l110-work` (`sdk-build.ps1`,
  `sdk-test.ps1`, `sdk-install.ps1`, `ctest3.ps1`, `build_then_test.ps1`, `copy_tp.ps1`,
  `normwarn.py`, `edit_common.py`, `edit_d3d12.py`, `draw_extent_rect.h`,
  `draw_extent_rect_test.cpp`, `sdk-commit-msg.txt`; logs `sdk-configure.log`, `sdk-build0..2.log`,
  `ctest-a1..3.log`, `ctest-b1..3.log`, `sdk-install.log`, `w110all.txt`).
