# KI-056 analysis: album "Choose a slot" never shows the new photo

Read-only analysis by an Opus subagent on 2026-11-06 of the owner's album F10 (l69 session, package 3) and the generated album code; accepted by the primary agent. It supersedes the "stale texture cache" reading recorded on 2026-11-05.

Read-only analysis, 2026-11-06. Sources: l69 session `37a5ea6b...` (runtime log
17:44-17:47 local, F10 package `live-20261105T154602Z-27716-3`: manifest, state.json,
log tail, frame.bmp), the l52 package `live-20261031T142517Z-21084-1` (frame.bmp only),
the four owner screenshots (17:46:00-17:46:28), SDK v0.20.0.8 tree (`l67-sdk-wt`),
`generated/default/mcla_recomp.49.cpp`. No save data or minidump was opened. Nothing
was built or launched. Spark: not used (log facts were extracted deterministically; the
guest-code reading needs the local generated corpus).

## 1. What the diagnostics show

Timeline (local time, session frame numbers):

| Time | Event |
|---|---|
| 17:45:27.274 | menu state: game paused (Photo Mode) |
| 17:45:32.455 | `MCLA photo capture: window open, pipeline wait on` |
| 17:45:32.491 / .524 | the capture: two `resolve2` lines, colour, EDRAM base 408, 960x544, fmt 6 (k_8_8_8_8), to `08172000` (frame 52538) and `07D76000` (frame 52539), len 2088960 each, `miss=1 decision=now reason=copied read=this`, `sync_rules=none` |
| 17:45:36.535 | capture window closed by the 120-frame timeout |
| 17:45:37 - 17:46:32 | album "Choose a slot" screen (paused) |
| 17:46:02.039 | F10 (package 3) |
| 17:46:32 | leaves the album (gameplay resolves resume) |

- **The capture resolves are synchronous read-backs.** Both are cache misses, copied in
  the same frame (`decision=now`), so the CPU saw a fresh frame (the KI-037 fix holds).
  `07D76000`, `07F74000` and `08172000` are three consecutive 0x1FE000-byte 960x544
  buffers: the menu/UI front-buffer ring (gameplay resolves `07F74000` in two parts,
  960x320 + 960x224 at `081CC000`). The photo is read from that ring by the CPU.
- **During the album screen** (F10 mark window, 743 frames): `resolves=743` (one
  front-buffer resolve per frame), `copies_now=186`, `READBACK_SYNC matched=0 forced=0`.
  Logged `tex_load` lines are only the 960x540 tiled front-buffer textures alternating
  `08172000`/`07D76000`, `scaled_resolve=1 first=0 reload_without_inval=0 rb_overlap=1`;
  `tex_inval` lines are their per-frame `by_gpu=1` invalidations. Minute 17:45:37-17:46:37:
  `tex_loads_tracked=13478` (all `rb_overlap`), `tex_invals=12843` of which `by_gpu=12828`
  (15 CPU-write invalidations of tracked textures), `tex_load_suppressed=13358`.
- **There are no lines for the preview or thumbnail textures (no 128x72/256x144/512x288
  or 960x540/1280x720 linear loads).** That is a coverage gap, not evidence of absence:
  the tracker only records a texture load when it is atlas-shaped or overlaps a recent
  read-back destination (`include/rex/graphics/util/livery_diag2.h:649-653`), and the
  2 lines/s budget is spent on the front-buffer loads. The album preview is a CPU-filled
  linear texture, so it is never tracked.
- **No texture/resolve/read-back errors or warnings** in 17:45:15-17:47:10. The F10 frame
  is a fresh presented guest output (`frame_signal freshness=presented-guest-output`).
- **KI-047 composite sync is not involved:** the capture keys are misses (always
  synchronous; rules a/b/c apply only to non-miss resolves), `sync_rules=none`, and the
  album window has `matched=0`.

Frames: the l52 F10 frame (first open, cursor not moved) shows the first empty slot
highlighted with the **camera icon** and the big panel with the logo. The l69 frame and
screenshots show the highlighted empty slot carrying the photo of the occupied slot
visited before (row 3/col 3 city street, row 4/col 2 camo car, row 3/col 4 green car, then
a grey car), and the big panel always shows the logo. The new photo appears nowhere,
including on first open before any hover.

## 2. Mechanism

### 2.1 Guest side (generated code)

The album object owns one CPU image `+1140`, backed by physical memory at `+1144` and
exposed as one linear texture `+1132` (LIN_A8R8G8B8, KI-037 analysis), bound to the
preview material `+1136`. Three paths decode JPEGs into it:

- `sub_8263C8B0` (state 4 after capture): checks the SOI marker of the new JPEG at `+1148`,
  decodes it into `+1140` (`sub_82775DE0`), recreates `+1132` if the dimensions changed
  (vtable +92), and binds `+1136` to `+1132`. **It fires no event.**
- `sub_8263C9C8(slot)` (hover preview): **only if the slot's bit in the occupied mask
  `+860` is set**, decodes that slot's JPEG (`*(+428) + slot*0x32000 + 204`) into the
  same `+1140`, rebinds `+1136`, and fires album event 4 (`sub_8268EE10(...,4,3,-1)`);
  for an empty slot it does nothing, otherwise it takes a failure path.
- `sub_8263DCF8(slot)` (save): copies `+1148` into the slot, decodes it again into `+1140`,
  builds the slot's own thumbnail texture (`sub_8218EE38(64,32,...)`, per-slot texture
  table at `+608 + slot*16`, material `+604 + slot*16`) and sets the occupied bit.

So the "copy of the last occupied slot" in the highlighted empty slot is exactly what the
guest code produces when the highlighted cell shows texture `+1132`: an occupied hover
re-decodes `+1140`, and an empty hover leaves it untouched. **Every one of those hover
decodes reached the screen**, which shows that the host texture cache does pick up CPU
writes into `+1144`. The defect is narrower: the state-4 decode of the **new** photo is
never visible (first open in l52: empty highlight; big panel: logo).

### 2.2 Host side (SDK), checked against the stated hypothesis

- **CPU writes into a GPU-written range are caught.** `RangeWrittenByGpu`
  (`src/graphics/shared_memory.cpp:460-476`) fires the watches, then `MakeRangeValid(...,
  true)` marks the pages valid and GPU-written and re-arms protection (`:486-553`, arm at
  `:533-538`). A CPU store faults into `MemoryInvalidationCallback` (`:903-996`); the widening
  stops at GPU-written neighbours (`:940-952`), but the faulting page itself is always
  invalidated and `FireWatches(page_first, page_last, false)` runs (`:993`). The texture's
  `WatchCallback` marks it outdated (`src/graphics/pipeline/texture/cache.cpp:753-768`). The
  widening stop costs one fault per 4 KB page; it does not lose writes.
- **The scaled-resolve copy cannot be the source for this texture.** `FindOrCreateTexture`
  sets `key.scaled_resolve` only for `key.tiled` textures (`cache.cpp:850-860`); the
  preview is linear. CPU writes also clear the scaled-resolve page bits
  (`ScaledResolveGlobalWatchCallback`, `cache.cpp:1162-1206`).
- **Upload ordering is safe.** `D3D12SharedMemory::UploadRanges` re-arms protection
  (`MakeRangeValid`) before the `memcpy` (`src/graphics/d3d12/shared_memory.cpp:368-405`).
  A narrower race remains (a CPU write between `RequestRanges` and `MakeUpToDateAndWatch`,
  `cache.cpp:436-445`/`693-708`, misses the not-yet-registered texture watch). It would
  leave a few stale pages, which heal on the next write fault anywhere in the texture. It
  cannot produce a whole, consistent previous image, so it does not explain KI-056.
- **The KI-047 composite sync (rules a/b/c)** cannot touch this range: the preview memory
  is not a resolve destination, and the capture resolves are misses.

**Conclusion.** The owner's reading ("the texture cache misses the guest write") is
contradicted for the hover path, and nothing in the SDK treats `+1144` differently on the
new-photo decode. The most likely failure is guest-level and host-timing-sensitive: the
state-4 decode does not run (SOI check on `+1148` fails, or state 4 runs against the other
buffer of `SwapNewJpegBuffers`) or runs and is overwritten before the chooser draws. The
logo in the big panel is a guest choice (the logo is a game asset, not a host fallback; a
host texture failure would give transparent black). Separate saved photos look correct
(the l52 photo is in row 4/col 2), so the JPEG reaches `+1148` by save time.

Resolution is a candidate trigger. KI-021's accepted M6 run (720p, scale 1, guest 1280x720)
showed a non-black new preview. At 1080p/1440p the guest frame is 960x540, so state 3
resizes `+1140` (`sub_8263C638`) and state 4 recreates the texture with new dimensions.
The 1280x720 slot JPEGs and the 960x540 new JPEG then alternate through the same object.
This is unproven.

## 3. Why the big panel shows the placeholder

- No texture creation or load failure is logged, and the frame shows the guest's own logo
  asset, so the guest decides to draw the placeholder.
- The panel does not follow `+1132`: the highlighted cell shows hover decodes, while the
  panel stays on the logo. The panel likely waits for an event or flag that the new-photo
  path never raises (`sub_8263C8B0` fires none; the hover path fires event 4), or for a
  successful state-4 decode that did not happen.
- A zero-size or unsupported format is unlikely: the same object and format display hover
  decodes correctly.

## 4. Proposal

### 4.1 First, a targeted diagnostic (host only, low risk; no SDK change)

Extend `src/mcla_photo_capture.cpp` (it already wraps `sub_8263D120`, `sub_8263D1E0` and
`sub_8263EAA8`) with pass-through `REX_EXTERN` wrappers that log one line per call
(`MCLA_ALBUM v=1 ...`, gated by the existing `mcla_photo_pipeline_wait` family or a new
`mcla_album_diag` flag that defaults on for test builds):

- `sub_8263CB78` state/sub-state transitions (`+132/+136` relative to the album base +456)
  and the frame counter `+732`;
- `sub_8263C8B0`: entry, the first 4 bytes of `*(+1148)` (SOI check result), the `+1140`
  image dimensions before and after, the texture `+1132` fetch header (base address,
  width/height, linear/tiled, format), whether the bind ran, and a CRC32 of the first
  64 KB of `+1144` after the decode;
- `sub_8263C9C8(slot)`: slot, occupied bit, success or failure path, same CRC;
- `sub_8263DCF8(slot)` (save) and the `SwapNewJpegBuffers`/`SetPreviewPicture` handlers
  once identified (the function table at `828260D8`).

SDK side, optional and diagnostics-only: a `livery_diag2` "watch range" so the tracker
logs `tex_load`/`tex_inval` for one registered guest range (`+1144`, 0x385000) regardless
of read-back overlap. The host wrapper registers it on album init. Counters: loads,
CPU invalidations and `reload_without_inval` for that range.

What it decides:
- state-4 decode never runs or fails SOI: guest flow or timing (section 4.2 A);
- it runs, the CRC changes and a `tex_load` of the `+1132` key follows, but the screen
  stays empty or logo: guest UI logic (event/flag), section 4.2 B;
- it runs and the CRC changes, but no `tex_load`/`tex_inval` follows for that key: a host
  invalidation miss after all, section 4.2 C.

### 4.2 Fix candidates, in order of risk

- **A (most likely; guest flow).** If state 4 is skipped or reads the wrong JPEG buffer,
  a host hook re-runs `sub_8263C8B0` once when the chooser opens in save mode (or calls the
  guest's own `SetPreviewPicture` path) after confirming the SOI at `+1148`. Risk: low to
  medium, because it touches guest UI state. Bound it to the save chooser, at most once per
  snapshot.
- **B (guest event).** If the decode runs but the panel waits for event 4, fire the same
  `sub_8268EE10(photoAlbum, 4, 3, -1)` after the state-4 bind. Risk: medium (script
  side effects). Needs the event table to confirm that index 4 is the preview-loaded event.
- **C (host invalidation, only if the diagnostic shows a miss).** After `sub_8263C8B0` and
  `sub_8263C9C8` return, call an exact invalidation of `[+1144, +0x385000)` (the
  `MemoryInvalidationCallback(..., exact_range=true)` path, i.e. what a CPU write fault
  does, through the existing physical-memory invalidation API). Cost: one 3.6 MB
  re-upload per decode (a few per album visit). Risk: low, because invalidating more
  never shows wrong data. A generic SDK rule ("invalidate a read-back destination on copy
  completion") is not justified: read-back copies use the unprotected physical view by
  design (a GPU-to-CPU copy must not re-invalidate GPU data), and the preview range is not
  a read-back destination.

### 4.3 Owner test (after the diagnostic or fix build)

1. Take a photo, open the chooser, press F10 **before moving the cursor**. Expected: the
   highlighted empty slot and the big panel show the new photo.
2. Move to an occupied slot and back to the empty one, press F10 again. Expected: the new
   photo again, not the hovered one. If not, compare with A.
3. Save into the slot, close the album and reopen it from the menu. Expected: the stored
   thumbnail and its preview are the new photo.
4. A/B, no build needed: the same steps at 720p (scale 1, guest 1280x720) versus 1080p.
   A correct 720p chooser points to the 960x540 resize path (section 2).

Counters to confirm: `MCLA_ALBUM` lines show state 4 with SOI ok, a CRC change on
`+1144`, and a following `tex_load` of the `+1132` key (scaled_resolve=0, tiled=0); no
`tex_load` of that key with `reload_without_inval=1`; no new read-back sync waits
(`READBACK_SYNC forced` unchanged in the album window).

## 5. Unresolved

- The script-side consumer of the big panel and the meaning of flag `+1225` are not traced.
  The event-name table needs the in-memory image reader (KI-037 method).
- The `+1144` physical address in this session is unknown (no line covers it). Overlap with
  the front-buffer ring is unlikely: hover decodes persist across frames.
- It is not established whether the original console shows the new photo in the
  highlighted cell or only in the panel. Item 1 of the owner test, at 720p, is the
  reference.
