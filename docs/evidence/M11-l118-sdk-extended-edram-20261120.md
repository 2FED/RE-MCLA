# M11 l118 SDK (2026-11-20): the extended EDRAM did NOT ship (blocking facts and the design); the second-level RT update memo and the vertex-slot correctness fix did

**Headline.**
- SDK branch `mcla/m11-l118` (worktree `private/l118-sdk-wt`, parent `e6eeeb9` = l117), two
  local commits, not pushed, no tag; the published fork stays v0.20.0.12 = `d889a16`:
  - `4807ea1`: second-level render-target update memo `d3d12_rt_update_memo2` (default off) with
    a per-input miss census and the `MCLA_CP_L118 v=1` line (Goal B);
  - `8d351cc`: vertex-slot skip validation `d3d12_vb_skip_validate` (default TRUE, a correctness
    fix), `SharedMemory::IsRangeValidLockFree`, the always-on `MCLA_VB v=1` line and up to 16
    `MCLA_VB_STALE v=1` lines per session.
- Goal A1 (an extended EDRAM so that the 1280x720 2x scene fits in ONE tile) is **not in this
  build**. It cannot be made exact in the default path within one build: the resolve, resolve-clear
  and host-depth-store compute shaders ship only as precompiled DXBC with the 2048-tile wrap and
  the 11-bit base hard-coded, and the tree has no shader sources to rebuild them. Section 1 lists
  every blocking fact and the design. The fallback (scene 1x at guest 1280x720, one tile by the
  game's own rule) is in the owner build as arms.
- Built and unit-tested only: not run, no physical PASS. Spark not used (deterministic source
  reading, edits, builds and tests; no bounded extraction or triage task suited it).

## 1. A1 extended EDRAM: why it is not in l118

### 1.1 What the 2x one-tile scene needs

- The game's rule (own RE, [M11-l107](M11-l107-guest-tiling-and-msaa-20261117.md) section 1 and
  [the l118 guest document](M11-l118-guest-single-tile-20261120.md)): tile surfaces are created
  with the padded width (multiple of 160) and the tile height rounded up to 32 rows. One tile of
  1280x720 at 2x: 1280 / 80 = 16 EDRAM tiles per row, 736 / 8 = 92 rows: **1472 tiles colour, 1472
  tiles depth, 2944 in all** (the brief's 1440 is without the 32-row rounding). The Xenos has
  2048 (10 MiB). 4x would need 5888: never.
- The hardware registers' base fields are 12 bits (0-4095), so 0 + 1472 and 1472 + 1472 = 2944 fit
  the register field. Our SDK splits it (`include/rex/graphics/registers.h:741-742` `color_base :
  11` + `color_base_bit_11`, `:840-841` the same for depth) and reads only the 11 bits.

### 1.2 Blocking facts in our SDK (l118 tree, file:line)

| # | Fact | Where |
|---|---|---|
| 1 | `kEdramTileCount = 2048`, `kEdramSizeBytes` derived; `kEdramBaseTilesBits = 11` ("periodic 11-bit EDRAM tile addressing") | `include/rex/graphics/xenos.h:413-421` |
| 2 | **Resolve shaders are prebuilt DXBC with the wrap hard-coded**: e.g. `resolve_fast_32bpp_1x2xmsaa_cs.h:62` extracts the base as `ubfe 11 bits at bit 13` of `xe_resolve_edram_info`, `:92` wraps with `udiv ... l(0x00280000)` (= 2048 x 1280 dwords). 13 unscaled shaders contain the 2048-tile wrap (resolve_clear 32/64bpp ± scaled, resolve_fast 32/64bpp 1x2x/4x, resolve_full 8/16/32/64/128bpp); the `_scaled` variants and the Vulkan SPIR-V headers have the same arithmetic | `src/graphics/shaders/bytecode/d3d12_5_1/*.h`, `vulkan_spirv/*.h` |
| 3 | **No shader sources in the tree** (only `resolve_downscale.cs.hlsl`); the bytecode cannot be regenerated or proven equal without importing the upstream `.xesl` sources and a shader build | `git ls-files` of `src/graphics/shaders` |
| 4 | `ResolveEdramInfo.base_tiles : 11` (bit 13), the layout the shaders decode | `include/rex/graphics/util/draw.h:342-359` |
| 5 | `RenderTargetKey.base_tiles : 11` (26 of 32 bits used: widening to 12 or 13 fits) | `include/rex/graphics/pipeline/render_target/cache.h:271-282` |
| 6 | Ownership ranges, transfers, clears and dumps wrap with `& (kEdramTileCount - 1)` and split at `kEdramTileCount` (about 60 sites) | `src/graphics/pipeline/render_target/cache.cpp:180-1641` |
| 7 | C++-generated transfer / dump DXBC use `LU(kEdramTileCount - 1)` | `src/graphics/d3d12/render_target_cache.cpp:3994`, `:4582`, `:6847` |
| 8 | EDRAM buffer and snapshot sized by `kEdramSizeBytes` (times the resolution scale) | `d3d12/render_target_cache.cpp:294`, `:2423-2572`; `trace_writer.cpp:256-271`; `trace_player.cpp:179` |
| 9 | Host render-target height is `kEdramTileCount / pitch` rows (an extended EDRAM doubles every RT's VRAM unless RTs grow on demand, as Canary does) | `cache.cpp:903`, `:1246-1247` |
| 10 | MCLA modules mask bases with `0x7FF` and assume 2048 | `include/rex/graphics/util/extent_below_audit.h:116`, `transfer_census.h:60-73`, `draw_extent_rect.h:178` |
| 11 | The ROV path bakes the tile count into the translated pixel shaders | `pipeline/shader/dxbc_translator_om.cpp:326`, `:1931`; `spirv_translator_rb.cpp:909`, `:1630` |

Why a "wrap-tolerant" 2048 model does not work instead: with 2048 tiles the depth at base 1472
(1472 tiles) wraps to tiles 0-895, overlapping the colour at 0-1471. Both are bound in the same
draw, so every draw would transfer ownership between them (cache.cpp Update: the later base
truncates the earlier one, `:637-664`, and ChangeOwnership claims both ranges).

### 1.3 Xenia Canary's extended EDRAM (studied, not ported)

- Public commit **`4ae7c9f538a1e0dc758be29d1d8f8df69e47cb37`** ("[GPU] Extended 40MB EDRAM path",
  2026-10-20): cvar `edram_tile_count` = "standard" (default, as before) or "extended" (8192
  tiles, 13-bit bases using bits 11 and 12 of RB_COLOR_INFO / RB_DEPTH_INFO); the render-target
  cache keeps the tile count for ownership, transfers, dumps and resolve clears; resolve / clear
  shaders get it through the coordinate-info constant; EDRAM buffers, trace snapshots, resolve
  EDRAM info and transfer constants are sized for it; host RTs start at the needed height and grow.
  36 files, about +830 / -470 lines, including the shader sources `edram.xesli`, `resolve.xesli`,
  `resolve_clear_32bpp.xesli`, `resolve_clear_64bpp.xesli`, `host_depth_store_{1x,2x,4x}msaa.cs.xesl`.
  Its message warns that the D3D runtime and game code may still mask to 11 bits.
- Later commits touching the same files (titles only; none extends the EDRAM): `42c2b880ee6`
  (16-bit EDRAM scaling), `83226586785` (8-bit format aliases), `534aeb64ea1` (gamma resolve).
- Not ported because: (a) the shader half needs the upstream shader sources and a shader build
  that we do not have, and the default path's bytecode must then be proven byte-identical to ours;
  (b) its render-target-cache half (`render_target_cache.cc` +183/-112,
  `d3d12_render_target_cache.cc` +93/-57) lands on the files that carry our l107-l114 levers
  (extent bound, transfer elision, deferred claims, RT memo); (c) the RT growth redesign is a
  behaviour change of its own. No Canary code is in l118. (Licence: BSD-3; a port would carry
  the attribution in the file headers and here, with the commit id above.)

### 1.4 The design for a later build (A1)

1. Import the upstream shader sources at the base our bytecode came from; add a shader build
   (fxc 10.1 is installed with the Windows SDK) and prove that the regenerated DXBC of every
   existing shader is byte-identical to the checked-in headers. Only then change a shader.
2. Make the tile count a cache member (`edram_tile_count_`, 2048 or 4096, from an init-only cvar
   `gpu_edram_extended`, default false) and replace every `kEdramTileCount` site of facts 6-10;
   widen `kEdramBaseTilesBits` to 12 in RenderTargetKey and ResolveEdramInfo (the latter moves
   fields: the shader change of step 1), read `color_base | bit_11 << 11` only when extended.
3. Pass the wrap modulus in the resolve constants (as Canary) or keep 2048 in the shaders and
   relocate each resolve's source window so it never wraps (the host-RT path dumps the RT into
   the EDRAM buffer right before the resolve, so the buffer is scratch there).
4. Refuse the ROV path when extended (fact 11).
5. Logs: `MCLA_EDRAM v=1 tiles=<n> extended=<0|1>` at start-up, `MCLA_EDRAM kind=oob` once with a
   count for any base or extent beyond the configured size.
6. Tests: the default configuration's keys and ranges equal the l117 ones for the real layouts of
   the l117v session (`MCLA_GPU_SURF last_frame_keys`: `p1280m2c1024d0o0` with offsets 0 / -512),
   ownership splitting at 2048 / 4096, resolve-info spans across the wrap.
7. The guest side then still has to be proven (the l118 guest document, section 4).

### 1.5 A cheaper lever found on the way (not implemented)

The D3D runtime records per-draw screen extents with `PM4_EVENT_WRITE_EXT` for predicated tiling;
our command processor writes a fixed full-screen extent (0-8192 in x and y,
`src/graphics/command_processor.cpp:1660-1687`) and executes predicated type-3 packets when
`bin_select & bin_mask` is non-zero (`:1092-1102`). With real extents the runtime could predicate
away the draws that do not touch a tile (the l117v session shows both tiles replaying all
4,585,424 draws). How the runtime turns extents into bin masks is not traced yet; an extent that
is too small would drop geometry, so this needs its own RE and a verify mode.

## 2. Goal B: second-level render-target update memo (`4807ea1`)

### 2.1 What the session data says

The l116 / l117 owner sessions (`c46e0301...`, `0248eadb...`) have 47 `MCLA_CP_L101` window
lines; at the default `rt_memo=1` a typical minute is `rt_lookups=13.07 M rt_memo_hits=7.68 M`,
misses `inputs:4.998 M` (38 % of lookups; 93 % of the misses), `cpu_vs:289 k`, `first:54 k`,
`epoch:34 k`, `transfers:12 k`. The line has no per-input split, so which input changes could not
be read from the logs: l118 adds that census.

### 2.2 The second level

- `include/rex/graphics/util/rt_memo2.h` `ProjectRtUpdateInputs`: keeps exactly the bits that
  `RenderTargetCache::Update` and `DrawExtentEstimator::EstimateMaxY` read on the memoised path
  (clip_disable = 0; with clip_disable set the inputs are returned unchanged) and zeroes the rest.
  The per-input proof is in the header comment, with the reading lines:

  | input | kept | why the rest is not read |
  |---|---|---|
  | RB_DEPTHCONTROL (normalized) | z_enable, stencil_enable | Update `cache.cpp:513`; the D3D12 part reads more only when the Update produced transfers (`d3d12/render_target_cache.cpp:1862`), which a hit excludes |
  | colour mask | nibble != 0 per RT | `cache.cpp:522`; the `== 0xF` test is also under transfers |
  | RB_SURFACE_INFO | pitch, MSAA | `:457-468`, `:604-605`; hiz_pitch never |
  | RB_DEPTH_INFO | base (11 bits), format; only with depth used | `:513-519` |
  | RB_COLOR_INFO0-3 | base (11 bits), format; only rasterizing and nibble used | `:521-530` |
  | PA_SC_WINDOW_OFFSET | y offset | EstimateMaxY `:270` (x only in the clip_disable branch `:298`) |
  | PA_SC_WINDOW_SCISSOR_TL | window_offset_disable | `:275` |
  | PA_SC_WINDOW_SCISSOR_BR, PA_SC_SCREEN_SCISSOR_BR | br_y | `:274`, `:280` |
  | PA_CL_CLIP_CNTL | clip_disable (0) | `:283` |
  | PA_CL_VTE_CNTL | y scale / offset enables | `:325`, `:328` |
  | PA_SU_SC_MODE_CNTL | vtx_window_offset_enable | `:318` (cull, face, polygon offset never) |
  | PA_SU_VTX_CNTL | pix_center | `:321` |
  | PA_CL_VPORT_YSCALE | the bits without the sign, only with its enable | `:328-330` (std::abs) |
  | PA_CL_VPORT_YOFFSET | the bits, only with its enable | `:325` |
  | vertex shader | dropped | read only in the clip_disable branch (`:307`) and by the rectangle extent bound, which refuses non-clip-disabled draws (`draw_extent_rect.h:109`) |
  | rasterization done, MRT clamp cvar | kept | |

- `d3d12_rt_update_memo2` (hot, latched at the next frame, `command_processor.cpp:228`): the
  memo decides and records with projected inputs (`:2677`); it implies the l101 memo; a switch
  invalidates the memo. Every other hit condition (epoch, resolve, submission, frame, no transfers
  in the recorded Update) is unchanged. `d3d12_rt_update_memo_verify` (the existing verify) runs
  the full Update for every hit and compares the state before and after (`rt_mm` on MCLA_CP_L101).
- Census (with memo2 or the l101 verify): each lookup whose raw inputs differ from the last
  recorded Update counts which raw words differ (`raw=`) and which projected words differ
  (`rel=`), and `rescued` = raw differs but the projection is equal (`:2675`). Line, after
  MCLA_CP_L101 in every perf-split window (`:4943`):
  `MCLA_CP_L118 v=1 reason=minute seq=3 rt_memo2=1 census=1 compared=... rescued=... raw=raster:..,depthctl:..,colormask:..,clamp:..,surface:..,depth:..,color0:..,color1:..,color2:..,color3:..,win_off:..,win_tl:..,win_br:..,scr_br:..,clip:..,vte:..,sc_mode:..,vtx:..,vp_ys:..,vp_yo:..,vs:.. rel=...`;
  config line `MCLA_CP_L118 v=1 kind=config rt_memo2= census= rt_memo= rt_memo_verify= frame=`.
- With the cvar off (the default) the l101 memo compares raw inputs exactly as in l117; the added
  cost is one branch per draw and one line per window.
- Expected: unknown until the census runs; the likely large misses (vertex shader pointer, cull
  bits, depth function, unused RT info) are all projected away.

## 3. Vertex-slot skip validation (`8d351cc`; a correctness fix, default ON)

- Defect (static check [M11-vertex-slot-cache-check-20261120](M11-vertex-slot-cache-check-20261120.md)):
  `IssueDraw` skipped `RequestRange` for a vertex fetch slot (A) while its in-sync bit was set and
  (B) when its fetch constant had the same address and size as its last request, without looking
  at the shared-memory valid bitmap; a buffer the CPU rewrote in place could be drawn with the
  stale host copy. Upstream requests every slot on every draw.
- `include/rex/graphics/util/vb_slot_check.h`: `IsPageRangeValidInBitmap` (the lock-free scan of
  the RequestRanges fast path, moved: one implementation), `DecideVbSkip`, the line formatters.
- `SharedMemory::IsRangeValidLockFree(start, length)` (public, const; `shared_memory.h:144`,
  `shared_memory.cpp:878-890`): true for length 0, false outside the buffer or without an active
  bitmap (the request then decides); RequestRanges uses the same helper (`:749`).
- `D3D12CommandProcessor::IssueDraw` (`command_processor.cpp:6312-6371`): skip A uses the slot's
  recorded state (the bit is set only together with it, and a fetch constant write clears it);
  both skips happen only while the range is valid (`DecideVbSkip`, `:6333`, `:6366`); a slot
  refused at A goes straight to RequestRange (no double count). Cvar `d3d12_vb_skip_validate`
  (hot, **default true**, `:237`); false = the old skips for an A/B. `NoteVbStale` (`:2596`)
  logs the first 16 hits per session:
  `MCLA_VB_STALE v=1 slot=3 addr=0x1A2B3C40 size=4096 kind=same validate=1 frame=900 draw=12345`
  (`draw` = the draw sequence number of the session, counted by this loop).
- Always-on line in every perf-split window (`:4947`):
  `MCLA_VB v=1 reason=minute seq=4 validate=1 vb_slots= vb_insync_skip= vb_same_skip= vb_insync_skip_invalid= vb_same_skip_invalid= vb_requests=`
  (`_invalid` = a skip taken (validate=0) or refused (validate=1) with an invalid page: non-zero
  means the fix changed a draw); `MCLA_VB v=1 kind=config validate= frame=` when it changes.
- Vulkan (`vulkan/command_processor.cpp:4048-4087`): the same check, always on, no counters. This
  backend is **not compiled** in the MCLA builds (`REXGLUE_USE_VULKAN=OFF`); the edit was not
  built.
- Cost (estimated, not measured): one or two relaxed 64-bit loads of the bitmap per slot for
  buffers up to 256 KiB (one 64-page word), about 5-15 ns; at about 6 k draws and 1-2 slots per
  draw roughly 0.05-0.2 ms of CP per frame; a refused skip costs a RequestRange as upstream.

## 4. Tests, warnings, identities

- New unit tests: `tests/unit/graphics/rt_memo2_test.cpp` (7: "l118 memo2 ignores the bits Update
  never reads", "... keeps every bit Update reads", "... conditional fields", "... projection is
  idempotent and a function of the bits read", "... with the l101 memo predicate", "l118
  per-input census", "l118 MCLA_CP_L118 lines"); `tests/unit/graphics/vb_slot_check_test.cpp` (5:
  "l118 valid bitmap: partial first and last pages", "... ranges crossing 64-page words", "...
  against a page-by-page reference", "l118 vertex slot skip decision and counters", "l118 MCLA_VB
  lines").
- ctest (`private/l118-sdk-b`, Release): after `4807ea1` 3 x **2415 / 2415 passed** (l117 2408 +
  7), again 3 x after the install; after `8d351cc` and the second install: 3 x **2420 / 2420
  passed** (+5; 4 BitStream cases skipped as always). The known flaky `physical_mapping_test.cpp:184`
  did not fail in any of the nine runs.
- Warnings: full build normalized set 210 = the l117 set, **0 new**; the incremental rebuild for
  `8d351cc`: 0 new.
- `git-clang-format` against the parent of each commit: applied; `git diff --check`: clean.
- Installed to `private/l118-sdk-install`; the DLL hashes are in the owner-build document.
