# M11 (2026-11-20): predicated tiling, screen extents and bin masks: guest mechanism, SDK behaviour, options

Read-only research (Opus subagent). Sources: our recompiled guest code (`generated/default`, read only),
our SDK at `8d351cc` (read only), the text log of one owner session (1280x720 guest, scene 2x in 2 tiles,
`MCLA_GTILE ... tiles=2 msaa=2 surface=1280x720 ... tile_span=512`), and the public Xenia Canary
`pm4_command_processor_implement.h` (BSD-3), read only to compare its EVENT_WRITE_EXT handler. No GPL code
opened. No build, no test, no launch, no edits to the repository. Spark not used: this was deterministic
reading of code and logs, and no bounded extraction or triage task suited it.

**Headline.** The game's D3D runtime does real predicated tiling, and the whole chain runs in the guest:
- During tile 0 it captures one screen extent per draw.
- On the CPU it turns those extents into a per-draw bin mask (`sub_8242ED38`).
- It patches that mask into the command buffer before tiles 1 and later replay.

Our command processor already honours predication: `(bin_select_ & bin_mask_) == 0` skips the packet
(`command_processor.cpp:1092-1102`). The only missing piece is honest extents. Our EVENT_WRITE_EXT
answers every draw with 0..8192 px in x and y (`:1660-1687`, the same as Xenia Canary's comment "Let us
hope we can fake this"), so every mask selects every tile. Tile 0 always runs every draw, because masks are
patched only after it. The lever is therefore only the replay of tiles 1 and later. With two horizontal
tiles it needs only a conservative **max-Y** per draw.

## 1. Guest mechanism (addresses are our recompiled functions)

| Step | Where | What |
|---|---|---|
| Tiling begin | `sub_8241BE78` (TU27) | Stores the tile rectangles at device +12752. Writes a header into a side list at device +13368 (`sub_8242FD88`): 0x80000000, flags, tile count, the 16-byte rectangles. Then a type-2 marker 0x86000000 (loop begin). Then `EVENT_WRITE` with initiator 25 = SCREEN_EXT_INIT (0x8241C2B4). |
| Per draw (draw path around 0x8241B99C..0x8241BAD8, predicated tiling active: device +10940 bit 0 and draw flag 0x800) | TU27 | Before the draw: `SET_BIN_MASK_LO` (0xC0006000) with payload -1. The address of that packet goes into a 16-byte entry of list device +13248. After the draw: `SET_BIN_MASK_LO 0x80000000`. Then **predicated** `EVENT_WRITE_EXT` (0xC0015A01), initiator 26 = SCREEN_EXT_RPT, address = entry+4 with k8in16 swap: the GPU writes the extents into the list entry. Then predicated `EVENT_WRITE` 25 (SCREEN_EXT_INIT, the reset for the next draw). The same pattern exists at three more sites (0xC0015A01 in TU27 near source lines 32305, 32925 and 33583). |
| Tiling end | `sub_8241C308` | Predicated resolves per tile (`sub_8241BD08(3<<2i)` = SET_BIN_MASK_LO). Then markers 0x88000000 + 0x80000000 ("only in the bit-31 pass"), 0x89000000 + the physical bases of lists +13248 / +13348, and 0x87000000 (loop end). Then `sub_8242FEF8` submits the side list. |
| Replay engine (CPU) | `sub_8242F870` worker, interpreter `sub_8242F420` | Walks the marker list. 0x86 / 0x87 loop over tiles (+52 = tile). `sub_8242F040` -> `sub_8242ECD0` computes BIN_SELECT and emits `SET_BIN_SELECT_LO`: select = 3<<(2*tile) (masked by 0x2AAAAAAA / 0x15555555 for two sub-pass kinds), **+0x80000000 for tile 0**. So the extent capture happens during the real tile-0 render; there is no separate extents-only pass, which matches `draws_by_key` (identical counts at offsets 0 and -512). A GPU interrupt callback `sub_8242FD00` queues work and calls `KeSetEvent`, so the worker continues after the GPU has finished tile 0. |
| Extents -> mask | 0x89 handler `sub_8242EE40` -> **`sub_8242ED38`**, run only while select has bit 31 | Each entry holds u16 minx, maxx, miny, maxy in **8-pixel units, max inclusive**. For each tile: if `x1 < (maxx+1)*8 && x2 > minx*8 && y1 < (maxy+1)*8 && y2 > miny*8`, then mask \|= 3 at bits 2i. It writes `mask \| 0x80000000` to the payload of the pre-draw SET_BIN_MASK_LO (entry cmdbuf pointer +8). `sub_82425CD8` handles the second list. |

The game's explicit screen-extent query (D3D query type 10, Issue at 0x8241E028, non-predicated 0xC0015A00)
is separate and not part of tiling.

Packet counts per frame (session log, minute seq 62, 3358 swaps; the opcode counts include predicated-out
packets):

| Packet | Per frame |
|---|---|
| DRAW_INDX | 3950 |
| DRAW_INDX_2 | 130 |
| EVENT_WRITE_EXT | 2789 (about 1394 tiled draws x 2 tile passes: predicated out in tile 1) |
| EVENT_WRITE | 2910 |
| SET_BIN_MASK_LO | 6138 |
| SET_BIN_MASK_HI | 348 |
| SET_BIN_SELECT_LO | 4.0 (bit-31 pass, tile 0, tile 1, reset) |
| SET_BIN_SELECT_HI | 1.0 |
| INTERRUPT | 4.0 |
| EVENT_WRITE_ZPD | 4.0 |

Executed draw packets are 13,673,271 against 13,700,135 issued, so only about **8 draws per frame are
predicated out today**. The draw class costs 19,649 ms per minute = 5.85 ms per frame, about 1.44 µs per
draw packet, on this PC.

## 2. Our SDK (8d351cc)

- Predicate: `command_processor.cpp:1092-1102`. Any type-3 packet with bit 0 set is skipped when
  `(bin_select_ & bin_mask_) == 0` (and predicated swaps are always skipped). This is the same as Xenia.
- `SET_BIN_MASK(_LO/_HI)` and `SET_BIN_SELECT(_LO/_HI)` are at `:1184-1217`; they set the 64-bit values
  correctly.
- `EVENT_WRITE_EXT`: `:1660-1687` writes initiator & 0x3F to VGT_EVENT_INITIATOR and six u16 values
  {0, 1024, 0, 1024, 0, 1} (8192 >> 3) with k8in16 swap. It ignores SCREEN_EXT_INIT / RPT semantics.
- Result: the guest computes mask 0x8000000F (2 tiles), and every tiled draw runs in both tiles. If the
  guest received tighter extents, we would already skip the culled draws: **only honest extents are needed.**
- Ordering is safe: we write extents synchronously on the CP thread while executing tile 0. The guest
  reads them only after its own interrupt and worker handshake, then patches the command buffer before it
  submits tile 1. Our CP reads indirect buffers from guest memory at execution time; no SDK cache of
  command-buffer contents was found.
- Our `DrawExtentEstimator::EstimateMaxY` (`draw_extent_estimator.cpp:263-342`) gives only the
  viewport/scissor bottom for clipped draws. It runs the CPU vertex shader only for clip-disabled draws
  (cvar off by default). For 3-D world draws it returns full screen, so **today it yields no culling.**

## 3. Xenia Canary

Its handler is identical: constant extents, "Let us hope we can fake this", a patent reference, no TODO,
and no computation. The predicate check is the same. No public fork computing real extents is known to us;
no search beyond the canonical source was done.

## 4. Options

Tile geometry: tile 0 = rows 0-511, tile 1 = rows 512-719 (window offset -512). At 960x540 4x the split is
320 rows; at 1280x720 4x there are 3 tiles of 256 rows. With two tiles, a draw is culled from tile 1
exactly when `(maxy+1)*8 <= 512`, i.e. its max-Y bound is <= 511. Min-Y and x are irrelevant and should
stay conservative (0 / full width). With 3 tiles, min-Y also helps tile 1.

a. **Conservative max-Y sources**
   - Viewport/scissor: ~0 gain for world draws, exact for scissored draws.
   - The l108-l110 rectangle interpreter and clip-disabled CPU vertex shader: exact for rectangle lists and
     clears. These are mostly full-screen passes that span both tiles anyway, so small gain.
   - New, the real lever: **linear-position bound**. Cache per vertex shader: ucode dataflow proves that
     oPos is a linear (dp4/mul/mad) function of one fetched position attribute and constants. Otherwise the
     draw gets a full-screen bound (skinning, billboards, instancing, VS texture fetch). Per vertex-buffer
     range, keep a cached position AABB, invalidated through the existing shared-memory watch/validity. The
     index min/max needs a scan, cached per index-buffer range. Per draw: read 4 constant rows, transform 8
     corners natively (well under 1 µs), project. If any corner has w <= epsilon, use full screen. Add a
     safety margin of one 8-px unit. Dynamic or ring vertex buffers rescan or fall back.
b. **Host GPU feedback**: the guest needs the extents within the same frame (after tile 0). A host readback
   means a CP->GPU drain per tiled frame (the KI-074 pattern). Frame N-1 extents keyed by draw order would
   produce missing geometry on camera cuts and fast motion. Not acceptable except as a verification signal.
c. **Without extents**: nothing equivalent. The host must render both bands.

**Cullable fraction: UNKNOWN.** No log carries any per-draw y signal: `draws_by_key` is equal per offset by
construction, and MCLA_XFER_EXTENT covers rectangles only. Tile 1 is the bottom 29 % of the screen (road,
player car, near traffic), so draws wholly above row 512 (sky, distant buildings, far traffic) could
plausibly be 30-60 % of tiled draws. That is a guess and must be measured.

## 5. Verify and measure

- **M0 counter** (always-on, CP-only, cheap). At each EVENT_WRITE_EXT that executes (bit-31 pass), keep the
  bound class of the draws since the last SCREEN_EXT_INIT: `top` (bound <= 511), `bottom`, `both`,
  `unknown` (no estimate), and the reason it is unknown (nonlinear VS, w <= 0, dynamic VB, index
  type...). Emit one line per minute, e.g. `MCLA_TILE_EXT v=1 tiled=... top=... unknown=...`, plus
  per-reason counts. Write full screen as today.
- **Verify mode**: compute the bounds, still write full screen. In tile 1, for a small sampled set of draws
  claimed `top` (e.g. <= 8 per frame, rotating), split the host draw with an occlusion query on rows >= 512
  only, with depth/stencil test forced to always for the query part. This is the l110 two-part pattern.
  `bad` must be 0. Respect KI-085 (heavy query arms once hung the device): a small budget, no queries
  inside fence races. A CPU-only cross-check: run the CPU vertex shader on 1 of N claimed draws (few
  vertices) and compare.
- **Enable**: write `maxy = bound>>3` (min 0, x full) for `top` draws only, behind a default-off key. Expect
  the predicated-out draw count to equal the `top` count in tile 1.

## 6. Recommendation

Worth a measurement build: counter M0 first, then verify, then the default-off enable.

Expected saving, only if M0 shows f >= 0.3:
- Saved draws = f x tiled draws per tile, about 1400 here.
- PC: 1.4-2 µs per replayed draw, so about 0.6-1.4 ms CP per frame at ~4 k draws. At the 1440p 10 k-draw
  dips (about 2500 tiled draws per tile) about 1-2.5 ms. GPU vertex work for those draws is saved too.
- Deck: replayed-draw marginal cost about 3.5-6 µs, so about 1.5-5 ms of the 51-56 ms CP. Converting at
  about 0.68, that is 1-3.4 ms of frame time, until the guest render-thread floor of about 45-50 ms.
- Keeps 2x MSAA, unlike the l118 scene-1x one-tile fallback (which removes 100 % of the replay).

What could make it fail:
- Most world draws use non-linear or instanced vertex shaders, so `unknown` dominates.
- Shared huge vertex buffers or index ranges give useless AABBs.
- Per-draw estimator cost exceeds the saved replay cost: the Deck is CP-bound, so the budget is about
  0.5 µs per tiled draw.
- The `top` fraction is small because tile 1 is only 208 rows.

Not failure modes: the guest mode (it is per-draw predicated tiling with extent capture, verified) and
consumption timing (CPU after tile 0, satisfied by our synchronous write).
