# M11 l107 (2026-11-17): why the scene is 4x in two tiles, the real scene-MSAA control, KI-083 and the KI-082 probe

**Headline.** The 1080p configuration runs the guest at 960x540, which our SDK reports as an SD output
(`is_hi_def = width >= 1280 || height >= 720`, `src/kernel/xboxkrnl/xboxkrnl_video.cpp:245`). The
renderer constructor then sets its "not HD" byte +373 = 1, and every frame the scene begin
`sub_822E5ED8` forces the tiled-rendering sample count to 4 because of it, although the mode word +384
reads 2. At 4x the game's own EDRAM rule splits 960x540 into two tiles of 320 rows; at 2x or 1x the same
rule gives one tile. That is why the l106 arm (+384 = 1) changed nothing. l107 adds the default-off
owner keys `guest_msaa_scene_2x` / `guest_msaa_scene_1x` (host init-only int `mcla_guest_msaa_scene`): a
function-level override of BeginTiledRendering `sub_8217A470` lowers the count for the two renderer
passes; single-tile rendering follows from the game's own rule, so no separate single-tile hack is
needed. An always-on `MCLA_GTILE` line reports tiles, count, surface and the force reason. KI-083 (three
duplicated rows at 59 % of the height) is our backend: the scaled-resolve half-pixel fill applied to the
top edge of the second tile's resolve. KI-082 gets a default-off motion probe `traffic_motion_probe`.
The SDK agent then added `video_mode_force_hidef` (owner key `force_hidef`, l107 SDK `cb92be8`): the root-cause fix, the game itself then draws the scene 2x in one tile (expected `MCLA_GTILE ... tiles=1 msaa=2 ... sd=0`). Built and unit-tested (see [the owner build](M11-l107-owner-build-20261117.md)); not run, no physical PASS. Spark not used (deterministic static
reading of our generated code and source edits; no bounded extraction or triage task suited it).

**Licence statement.** The research note (`M11-aa-and-scaling-research-20261115.md`, levers 4 and 6)
names `sub_8217A470` as BeginTiledRendering, the hook point 0x8217A700 and `sub_8217B430` from GPL-3.0
and unlicensed projects; only those facts were used. No third-party code was opened, copied or
paraphrased. Every statement below comes from our own recompiled guest code (`generated/default`, read
only), our SDK source (`private/l105-sdk-wt`, read only) and our own analysis; the host code is original.

## 1. BeginTiledRendering `sub_8217A470` (TU2), our reading

| Register | Kept in | Meaning |
|---|---|---|
| r3 | r17 | colour surface wrapper; size from vtable +24 / +28, format from +60 (null: the display-size globals 0x827D426C.. and format 2 / 3) |
| r4 | 396(r1) | stored at 0x82839258 +452 for EndTiledRendering |
| r5 | 404(r1) | clear parameters (7 words copied to 0x827D4330; null: depth 1.0, colour 0xFF000000) |
| r6 | r18 | **sample count** (1, 2, 4) |
| r7 | r31 | flags, stored at 0x82839258 +296: bit 0 = split into columns (else rows), bits 2 / 3 = clear flags |
| r8 | r27 | tile overlap in pixels |

- **EDRAM tile size** (0x8217A5B8..0x8217A5E8): <= 1 sample 80x16 pixels, 2 samples 80x8, otherwise
  40x8.
- **Rule** (rows, 0x8217A680..0x8217A6FC): the width is padded to 160; tiles per row = padded width /
  tile width; rows per tile = 1024 x tile height / tiles per row, rounded down to 32; count = (rows - 2
  overlap + height - 1) / (rows - overlap), stored at **0x827D42A4**. Columns (0x8217A5FC..): the same
  with the height padded to 32 and the width per tile rounded down to 160. 1024 EDRAM tiles is half of the
  2048: colour and depth of a tile each take one half.
- Results: 960x540 at 4x: rows 320, **2 tiles**; at 2x: rows 672, 1 tile; at 1x: rows 1344, 1 tile.
  1280x720: 4x 3 tiles (256 rows), 2x 2 tiles (512 + 208, as the research note says), 1x 1 tile.
- Tile surfaces: an 8-slot cache at 0x82839258 +384 (colour) / +416 (depth) keyed by count, padded size
  and format (0x8217A724..); a new key creates both through the device factory (vtable +56); a ninth key
  ends in `sub_821BD618`.
- **Rectangles** at **0x82839258** (16 bytes: x1, y1, x2, y2): row tiling gives tile i = [0, i (rows -
  overlap), width, min(i (rows - overlap) + rows, height)]: they abut when the overlap is 0.
- Then the D3D tiling begin `sub_8241BE78(device, flags, count, rects, clear colour, clear Z, stencil)`.
- **EndTiledRendering `sub_8217B430`**: for each tile, predication `sub_8241BD08(3 << 2i)` and two
  resolves `sub_82420BA8` of that tile's rectangle to the same position of the destination (depth if a
  depth texture is bound, then colour), then the D3D tiling end `sub_8241C308`; the count is reset to -1.
  So two tiles mean two colour and two depth resolves of the scene per frame, and the D3D runtime replays
  the bracketed commands per tile (draws that touch both tiles run twice; our counters cannot split draws
  per tile: `MCLA_GPU_SURF` counts window offsets per surface key, not draws per offset).

## 2. The callers and the force-4 bytes

| Caller | Return | Count handed (r6) | Surface / overlap / flags |
|---|---|---|---|
| scene begin `sub_822E5ED8` (TU15), every frame | 0x822E6020 | +384 (3 -> 4); **4 if +393, or the SD flag** (+373; or +433 while +432 is set on a thread whose TLS byte [[r13]+72] is 0) | the scene target +396 / +400 (index from 0x82839374), overlap 0, flags 0 or 12 |
| SD pass `sub_822E5358` (TU15, from `sub_82227428`), only when +375 | 0x822E543C | +380 (3 -> 4); 4 if +393 or +373 | r3 = 0 (display size), overlap 0, flags 0 |
| shadow / depth collector `sub_825F48E8` (TU46) | 0x825F494C | `li r6,4` | the collector's +60 / +64 surfaces |

- **+373 = output is not HD.** Constructor `sub_822E47E0`: +373 = 1 when the parameter word at 0x8287E06C
  +4 is set or `sub_82178880` (XGetVideoMode fIsHiDef) returns 0, else 0. Writers afterwards: the deferred
  request `sub_822E5B20` (+433 = value, +432 = 1), applied by `sub_822E5B60` once per frame (+373 =
  +433); getter `sub_822E5B30`. Users of the request: two save / request-1 / restore pairs
  (`sub_82215A60` / `sub_822150D8` in TU7, `sub_8263BFC8` / `sub_8263CF80` in TU49, next to the photo / album functions of KI-056): temporary 4x for
  some game states, restored afterwards. Other readers of +373: `sub_822E60E0` (state 9 = 16 instead of
  a configured value), `sub_822D5A30` and `sub_8230F3A0` (scale camera / LOD factors via the getter),
  `sub_822E6408` (+375 = (+380 != 0 or +373) and +364 != 0x28000000). Because it is more than an MSAA
  switch, the control does not clear it.
- **+393**: constructor 0; `sub_82304648(r3)` / `sub_823050B8` (a scripted command) set it, also from
  `sub_8269F8C0` (1), `sub_8269FB18` (0), `sub_8269FCC8` (1) and a string-command handler
  `sub_823D76A8`; read by about seventeen render functions (`sub_822E8BA8`, `sub_822F0F90`, `sub_822F3918`,
  `sub_822FA550` ...): a per-state "high quality / special view" flag, 0 in free roam (the
  `MCLA_GTILE` line reports it as `force4=`).
- **Our configuration:** the 1080p / 2x2 build sets the guest video mode to 960x540; the SDK answers
  fIsHiDef = 0 (`xboxkrnl_video.cpp:245`). The creation `sub_822E4AB0` keeps +384 = 2 (its SD-4x branch
  needs <= 720x576), but +373 = 1 forces the scene (and the SD pass, which only runs because +373 is set)
  to 4x: 960x540x4 = two tiles, the main surface key `p960m4c960` and the window offsets 0 / -320
  measured in l106 Run 1 and Run 4. At 720p (1280x720, HD) the scene would be 2x in two tiles.
- Other consumers of the scene count (+384, unchanged with 2x; 1 with 1x): the scene target descriptors
  (+396 / +400 / +404, `sub_822E4CC8`), the display-size depth of the shadow / depth collector
  (`sub_825F4750`, from `sub_82310E40`: descriptor +132 = count), render state 15 (`sub_822E55D0`, on for
  +384 > 1; probably alpha-to-mask), the per-frame tests `sub_822E5218` / `sub_823115E8` (> 0). The tile
  surfaces themselves are created inside BeginTiledRendering with r18, so they always match the count
  handed in, and EndTiledRendering resolves whatever was drawn. Note: in the stock build the collector
  depth is already 2x while the scene is 4x.

## 3. The mechanism (T2)

- `REX_EXTERN(sub_8217A470)` (`src/mcla_guest_tiling.cpp`): the caller is told by `ctx.lr` (the three
  return addresses above). With `mcla_guest_msaa_scene` = 1 or 2 and a call from the scene begin or the
  SD pass whose r6 is higher, r6 is lowered to the request; never raised; the collector pass and any
  other caller are untouched. Then the original runs.
- With 1, `mcla_guest_msaa.cpp`'s creation hook also writes +384 = 1 (the l106 write), so the scene
  targets, the collector depth and render state 15 take the game's own 1x path. With 2 nothing else is
  written (+384 is 2 already).
- After the original (always on, key or not): the tile count (0x827D42A4) and rectangles (0x82839258),
  the renderer bytes (+384, +380, +393, +373 / +432 / +433 with the TLS byte) and the game's rule
  recomputed from the covered size (`rule_ok`). One line per caller at the first call and on every
  change (32 per caller, then counted):
  `MCLA_GTILE v=1 tiles=2 msaa=4 surface=960x540 forced4x=1 requested_msaa=0 applied=0 where=scene game_msaa=4 mode=2 sd=1 force4=0 tile_span=320 rule_tiles=2 rule_ok=1 calls=1`
  (expected stock line), and with `guest_msaa_scene_2x`: `tiles=1 msaa=2 ... applied=1 ... tile_span=540
  rule_tiles=1`.
- F10: `"guest_tiling": {"version": 1, "requested_msaa", "cvar", "cvar_valid", "scene": {...},
  "sd_pass": {...}, "shadow": {...}, "other": {...}}` (calls, changes, suppressed, rewrites,
  rule_mismatches and the last observation).
- **Single tile (T2 b):** no separate key. 960x540 at 2x or 1x is one tile by the game's own rule, so
  single-tile rendering follows from the count. 4x in one tile would need 24 x 68 colour plus as many
  depth EDRAM tiles (3264 > 2048); our SDK has a fixed EDRAM (`kEdramSizeBytes`, no extended EDRAM), so
  it is not forced. The line asserts the count (`tiles=1`, `rule_ok=1`).
- Keys: `guest_msaa_scene_1x` / `guest_msaa_scene_2x` -> `mcla_guest_msaa_scene` = 1 / 2 (startup, a
  conflict pair). The cvar is in `kNeverWrittenCvars`.

## 4. KI-083: the seam is our scaled-resolve half-pixel fill at the tile boundary

- Guest: the scene tiles abut with no overlap (r8 = 0 at 0x822E6008; rectangles [0,0,960,320] and
  [0,320,960,540]); EndTiledRendering resolves tile 2's rectangle to destination row 320. The guest has
  no one-line overlap.
- SDK (`private/l105-sdk-wt`): with resolution scaling, `GetResolveInfo` sets
  `fill_half_pixel_offset` for every resolve (`src/graphics/util/draw.cpp:1030-1034`, cvar
  `resolve_resolution_scale_fill_half_pixel_offset` default true at `draw.cpp:31`, stored at
  `:1047` / `:1068`, bit in `include/rex/graphics/util/draw.h:352-356`): the resolve shaders
  (bytecode `src/graphics/shaders/bytecode/d3d12_5_1/resolve_*_scaled_cs.h`; HLSL not in this tree)
  stretch the first surely covered host row / column over the left and top edges of the resolve area,
  meant for the frame edge where the +0.5 guest-pixel offset (`draw.cpp:325-328`) leaves host pixels
  uncovered. Tile 2 is drawn with window offset (0, -320) (`draw.cpp:820-827` applies it to the resolve
  rectangle), so its resolve starts at EDRAM row 0 and the fill overwrites the top host rows of guest line
  320 with the next covered row, although the scene geometry covers them (it continues across the
  boundary). At 2x2 scale that is scaled row 640: rows 640-642 identical with a step from 639, as
  measured.
- What the SDK agent would change: in `GetResolveInfo` apply the fill only to edges that are a render
  target origin: no top fill when the window y offset (with `vtx_window_offset_enable`) is non-zero, no
  left fill when the x offset is; with the single bit available, the minimal change is to disable the
  fill when either window offset is non-zero (tile 2 then has no left fill either: at most a 1-pixel
  column at the left screen edge for the lower 40 %, the case the fill was made for); separate x / y bits
  need the resolve shaders rebuilt. Not edited here.
- l107 offers two A/Bs without an SDK change: `guest_msaa_scene_2x` / `_1x` (one tile: the boundary is
  gone) and the key `resolve_fill_off` (SDK graphics-plugin bool
  `resolve_resolution_scale_fill_half_pixel_offset` = false: no fill anywhere; the seam should vanish,
  a 1-pixel line may appear at the top / left frame edge).

## 5. KI-082 probe `traffic_motion_probe` (T3)

- Hook: `sub_826FA0F8` (TU55, mcAmbientCollider vfunc +56, no direct caller; the KI-054 site B is inside
  it), main thread, once per simulated ambient car per step (l84: inline ambient step, one per timer
  update). At entry: collider r3, instance +672, the matrix the function itself is about to use
  (0x826FA110..0x826FA178: TLS byte set -> inst + (word 0x827E5640 + 4) x 64; inst +500 bit 0x20 with
  +52 -> +48 non-null -> [that +28] + 16; else inst + 128). Rows 0-2 must be unit length and row 3 (the
  position, +48) finite: else counted as `bad_matrix`. Only words the original reads are read.
- Update boundary: the raw-timer override calls `MclaTrafficMotionOnTimerUpdate` after each main-timer
  tick (step = timer +8, host time = steady clock).
- Per car (8 slots by collider pointer): position at the first call of each update; displacement over
  the previous step dt and the host interval; `vr_sim` (speed ratio over steps), `vr_vis` (over host
  intervals), `reversal` (consecutive moves in opposite directions, > 2 mm each, also while standing).
  `sim_jitter`: |vr_sim - 1| > 0.25 at an even step (simulation uneven); `vis_jitter`: |vr_vis - 1| >
  0.25 while |vr_sim - 1| <= 0.10 (smooth simulation shown unevenly: step versus presented interval). A
  tremble the owner sees with neither and no reversals is on the render side (pose interpolation or
  skinning), which this probe cannot see.
- Lines: `MCLA_TRAFMOT v=1 kind=config ...` once; `kind=minute` every 60 s (step and host-interval
  stats, ratio quantiles, reversals, jitter counts, multi-call steps, matrix paths); after each accepted
  F10, `kind=burst` and 5 s of `kind=frame` lines (one per update, up to six cars:
  `c<slot>=<calls>:<dp_mm>:<v_m/s>:<vr_sim>:<vr_vis>:<reversal>`). F10 block `"traffic_motion"` while on.
- Not located (limits): the player position (so "nearby" means "physically simulated", which the game
  does for cars near the player), the render-side pose of traffic cars, and an interpolation alpha (the
  game runs one variable step per frame; there is no fixed-step accumulator to log).

## 6. The preferred route: forced HiDef (SDK)

- With `force_hidef` the SDK reports the 960x540 mode as HiDef, so the constructor stores +373 = 0: the scene
  begin passes +384 = 2 (2x, one tile by the game's own rule), the SD pass does not run (+375 = 0 unless +380 is
  set), state 9 and the two camera / LOD factors take their HD values: the same as a 720p / 1080p console, only at
  960x540. Expected `MCLA_VIDEOMODE v=1 ... hidef=1 forced_hidef=1` and `MCLA_GTILE ... tiles=1 msaa=2 ...
  forced4x=0 ... game_msaa=2 mode=2 sd=0`. The scene keys remain for 1x and as a control that does not change
  the SD state.
- `resolve_fill_origin_only` (SDK) is the KI-083 fix for runs that keep two tiles; `resolve_fill_off` the crude
  A/B.

## 7. Expected visual effects and risks

- 2x: half the shaded samples of the stock 4x (at 2x2 scale 8 instead of 16 per guest pixel); edges
  slightly less smooth than now, the same as a 720p HD console's 2x. One tile: the KI-083 seam should be
  gone; one colour + one depth scene resolve fewer per frame; draws spanning the boundary no longer run
  twice. 1x: aliasing on edges, thin poles and wires (FXAA arm), render state 15 off as in the game's own
  1x path (foliage / fences may lose their coverage gradient), the collector depth at 1x.
- Ownership transfers may change either way: the game's clears and the 4x collector pass share EDRAM
  with the scene; mixed-count transfers (2x scene against 4x collector) already exist in stock. Measure
  with `pc/scene-2x-gpu-cat` and `pc/scene-1x-gpu-cat` against `pc/xfer-census`.
- The SD pass is lowered with the scene; if it carries the HUD, HUD edges are drawn at the requested count.
- The +393 / pending-request states still force 4 in the game; the control lowers those frames too
  (consistent: the tile surfaces follow the count).
- Init-only: the count must not change during a session (tile surface cache, targets created once).
- With the keys off: two always-on reads per BeginTiledRendering call and at most 32 lines per caller.
