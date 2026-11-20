# M11 l118 (2026-11-20): one scene tile at guest 1280x720 2x: the guest side, our RE, and why l118 ships the 1x fallback instead

**Headline.** The game cannot be made to lay out its 1280x720 2x scene in ONE tile with a
function-level override: the 1024-EDRAM-tile budget is not a parameter or a global but an
immediate shift inside BeginTiledRendering `sub_8217A470`, and the same function creates the tile
surfaces from the derived row count before it returns, so a post-fix after the original would leave
512-row surfaces. Only a host re-implementation of the whole function (or a mid-function hook,
which needs regeneration) could do it, and even then the SDK must model more than 2048 EDRAM tiles
(the [SDK document](M11-l118-sdk-extended-edram-20261120.md), section 1). l118 therefore ships the
fallback that works by the game's own rule: the main scene at 1x at guest 1280x720 (one tile,
`guest_msaa_scene_1x`, keys and code since l107) as the arms `pc/scene-1x-1440`,
`pc/scene-1x-1440-fxaa`, `deck/scene-1x` and `deck/scene-1x-fxaa`. No `guest_single_tile` key, no
`gpu_edram_extended` cvar and no `MCLA_EDRAM` line exist in l118.

**Licence statement.** Everything below comes from our own recompiled guest code
(`generated/default/mcla_recomp.2.cpp`, read only), our earlier RE
([M11-l107](M11-l107-guest-tiling-and-msaa-20261117.md)) and our SDK source. The hook point
0x8217A700 and the "single tile" idea are named in the research note from GPL-3.0 / unlicensed
projects; only those facts were used. No third-party code was opened, copied or paraphrased for
the guest side.

## 1. The rule inside `sub_8217A470` (TU2), what it derives from the sample count

Registers as in l107 (r6 = sample count kept in r18, r7 flags in r31, r8 overlap in r27, r28 /
r25 the surface width / height):

- Tile height from the sample count: 16 rows at 1 sample, 8 at 2 and 4 (tile width 80, 80, 40).
- **The budget is an immediate**: rows mode at **0x8217A680**: `addi r8,r28,159`, `divwu r11,r8,160`
  (width padded to 160, in 160-pixel units), then `rlwinm r5,r11,10,0,21` = tile height << 10,
  i.e. 1024 x tile height; rows per tile = that / tiles per row, rounded down to 32 (`rlwinm
  r8,r8,0,0,26`); count = (rows - 2 overlap + height - 1) / (rows - overlap), stored at
  `-104(r16)` = **0x827D42A4**. The column branch just before 0x8217A680 uses the same `rlwinm
  r5,r10,10,0,21`. There is no load of a budget word and no parameter for it: candidate (i) of the
  brief does not exist.
- **0x8217A700**: the tile surface size: r24 = padded width (160 x units), r26 = rows (or height)
  rounded up to 32.
- **0x8217A724-0x8217A864**: an 8-slot cache at renderer +384 (colour) / +416 (depth), keyed by the
  count (r18), r24, r26 and the format (r19). A new key creates both surfaces through the device
  object at **[0x82839D70]**, vtable **+56**: colour with type 2 and the parameter block at
  r1+160 (+164 = sample count, +178 = 1, +180 = 0); depth with type 3 and +180 = **the colour
  surface just created**, +192 = the word at 0x828393D4, +200 = the format. So the depth surface's
  EDRAM placement is decided inside that factory (and the D3D runtime under it) from the colour
  surface, not in this function. A ninth key ends in `sub_821BD618`.
- **0x8217A87C-0x8217A91C**: the rectangles at **0x82839258** (16 bytes each, rows mode: [0, i
  (rows - overlap), width, min(.. + rows, height)]), +448 / +452 the surface and r4.
- Then `sub_82415D78` and the D3D tiling begin `sub_8241BE78(device, flags, count, rects, ...)`.

## 2. 1280x720 by sample count (the rule reproduced)

| samples | tile | tiles per row | rows per tile | tiles | surface (EDRAM tiles) |
|---|---|---|---|---|---|
| 1 | 80x16 | 16 | 1024 | **1** | 16 x 46 = 736 colour + 736 depth |
| 2 | 80x8 | 16 | 512 | **2** (512 + 208) | 16 x 64 = 1024 + 1024 |
| 4 | 40x8 | 32 | 256 | **3** | 32 x 32 = 1024 + 1024 |
| 2, one tile | 80x8 | 16 | 736 (720 rounded up) | 1 | 16 x 92 = **1472 + 1472 = 2944 > 2048** |

The surface height is the row count rounded up to 32, so one 2x tile is 736 rows (not 720), which
is why the brief's 2880 becomes 2944. 4x in one tile would need 5888: never.

## 3. Consumers of the count and rectangles (audit)

| consumer | what it does with them | one full-surface tile |
|---|---|---|
| EndTiledRendering `sub_8217B430` | per tile: predication `sub_8241BD08(3 << 2i)`, depth then colour resolve `sub_82420BA8` of the tile's rectangle to the same position, then the D3D tiling end `sub_8241C308`; count reset to -1 | the game's own path at 1x (and at 960x540 2x): one predication 3, one resolve pair of [0, 0, 1280, 720] |
| tile surface cache (+384 / +416) | keyed by count and size | a new key: new surfaces (created once) |
| scene targets +396 / +400 / +404 (`sub_822E4CC8`), render state 15 (`sub_822E55D0`), collector display depth (`sub_825F4750`) | use the renderer mode +384, not the tile count | unchanged at 2x |
| SD pass `sub_822E5358` (return 0x822E543C) | its own call with r3 = 0 | not touched (and off with forced HiDef) |
| shadow / depth collector `sub_825F48E8` (return 0x825F494C) | its own call, `li r6,4` | must stay untouched |
| paused 4x path (+393 / pending request) | the scene begin passes 4 | 3 tiles at 1280x720: stays tiled |

Other readers of 0x827D42A4 / 0x82839258 (an l118 search of the generated code for
`17060(rN)` after `lis rN,-32131` and `-28072` after `lis rN,-32124`): `sub_82178F38` (a getter:
stores the count to its r3 and returns the rectangle pointer while the count is above 0),
`sub_82178F70` (loops over the stored tiles and calls the resolve `sub_82420BA8` per tile: a second
per-tile resolve user), `sub_82179C20` and `sub_8217B7B0` (read the count; a "tiling active" test
or loop; not traced further). All of them iterate over the stored count and rectangles, so a
one-tile layout is one iteration over the full surface, the same as the game's own 1x path. The
one-tile layout is consistent for every consumer found, as the game itself shows at 1x; the
readers in other TUs with `-28072` use other bases and are unrelated.

## 4. What blocks a one-tile 2x scene on the guest side

1. **The budget**: an immediate shift in the middle of `sub_8217A470` (section 1). A
   function-level override can change only inputs and outputs. r6 (the only input that changes the
   row count) is also the sample count of the created surfaces: lowering it gives one tile at 1x
   (the shipped fallback).
2. **Post-fix after the original** (candidate ii): by then the 512-row surfaces exist and the D3D
   tiling has begun with two rectangles; rewriting 0x827D42A4 / 0x82839258 would make EndTiled
   resolve rows the surfaces do not have. Not sound.
3. **Wrapping the D3D begin `sub_8241BE78`** (candidate iii): it receives the already created
   surfaces; same problem.
4. **The depth placement** beyond 2048 tiles happens inside the device factory (vtable +56 of
   [0x82839D70]) and the D3D runtime; whether their arithmetic tolerates bases above 2047 (the
   register field is 12 bits; our SDK reads 11) is not traced.

The sound guest route, for a later build: a host re-implementation of `sub_8217A470` (about 900
lines of generated code) with the budget raised for the scene caller at 2x only, plus tracing the
factory's depth base; and the extended EDRAM in the SDK. Unit tests of the rule (section 2) would
come with it.

## 5. What l118 ships for Goal A

- The 1x scene at guest 1280x720: arms `pc/scene-1x-1440` (`guest_msaa_scene_1x`),
  `pc/scene-1x-1440-fxaa` (+ `present_fxaa`), `deck/scene-1x`, `deck/scene-1x-fxaa`; the meaning
  is the 1440p (or 720p) target profile, guest 1280x720. Expected `MCLA_GTILE ... tiles=1 msaa=1 ...
  where=scene ... rule_tiles=1 rule_ok=1` against the clear run's `tiles=2 msaa=2`, about half the
  scene draws per frame, aliased edges (no MSAA; FXAA arm), render state 15 off as in the game's own
  1x path, the collector depth at 1x (l107 section 7).
- Risks as in l107 section 7; nothing new in the code.
