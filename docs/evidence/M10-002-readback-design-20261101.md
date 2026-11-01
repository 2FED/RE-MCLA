# M10-002 design: cutting the GPU and CP cost of the read-back path

Read-only design by an Opus subagent on 2026-11-01 (SDK v0.20.0.6 source, owner l53/l54 logs); accepted by the primary agent. GPU gains are estimates until step 0 measures them.

Status: **design only, read-only research (2026-11-01).** No build, no run, no
source change. Source: ReXGlue SDK at v0.20.0.6 as checked out for l56
(`third_party/rexglue-sdk`, line numbers below are from that tree), the MCLA host
in the main checkout, the M10-001 evidence, and the six 2026-11-01 runtime logs
(five l53 sessions with `MCLA_PERF_SPLIT`, one l54 BENCH-C session
`runtime-20261101T135425Z-41508.log` with diag v2). Helper scripts used for the
log numbers: `fam.py` (diag v2 families) and `perf.py` (perf split per window),
kept next to this note.

## 0. Summary

- **Most of the GPU read-back work is wasted, and that is provable from the
  source alone.** In `some` mode every read-back resolve records a downscale
  dispatch (at scale > 1), a GPU copy into a READBACK-heap slot and three
  barrier batches. The decision whether anything is copied to guest memory is
  made only afterwards. The slot written by resolve N is read only if
  resolve N+1 of the same key copies. That happens for about 1 resolve in 4
  (plus misses and forced resolves). **About 74% of the GPU read-back copies
  are therefore never read by anyone.** Removing them changes no byte that
  reaches guest memory (step 1, exact).
- The volume is large. The copied bytes are 11.3 MB per frame at 14.0
  copies per frame of 56.2 resolves, in every l53/l54 window at both 720p and
  1440p. So about **45 MB per frame** go GPU to system memory over PCIe, plus,
  at scale 2, a downscale that reads about 180 MB of VRAM per frame. The
  estimated GPU cost of the read-back block at 1440p is **2.9-5.5 ms per
  frame** (11-21% of the 25.6 ms GPU frame). Step 0 measures it.
- **Which resolves the CPU reads.** Only two families are proven CPU
  consumers: the **livery/vinyl composite** (KI-033/KI-047; the car atlas is
  DXT5, which the Xenos cannot render or resolve, so the CPU builds it from
  the read-back) and the **Photo Mode capture** (KI-021: the JPEG was black
  without read-back). Every steady per-frame family is sampled by the GPU from
  the scaled-resolve buffer or shared memory. `rb_overlap` is true for 99.98%
  of texture loads, and the CPU copy never feeds the GPU (section 2). For three
  small families a CPU read is plausible and unproven: the k_32_FLOAT
  luminance chain, the tiny fp16 targets and the k_8 targets. A census decides.
- **No per-key "stays on GPU" mode exists.** `readback_resolve=none` is exactly
  that, but it is global. The host forces `some` because Photo Mode and the
  livery need CPU data. Read watches ("data providers") are stubbed out in
  the SDK.
- **Design.** (1) Elide unread GPU copies, exactly. (2) Add a read-watch
  detector: NoAccess instead of ReadOnly on resolved pages, a per-key
  "CPU touched" bit and a census. (3) Skip both the GPU copy and the memcpy
  for keys that are steady and never touched. The composite family stays on
  the l55 synchronous rules, the photo window suspends skipping, and any
  touch promotes the key. (4) An optional synchronous fallback on a first
  touch, serviced by the CP thread. (5) Batching only if the counters still
  show it is worth it.
- **Expected gain at 1440p (RTX 5060 Laptop, GPU-bound):** step 1 saves
  2.1-4.1 ms GPU per frame (38 to about 42-46 fps; the render thread caps it
  near 49). Step 3 saves another 0.7-1.4 ms GPU per frame and **0.8 ms CP per
  frame** (the memcpy). At 720p the GPU is not the wall: steps 1-3 give
  headroom there, and the 0.7 ms CP saving is where fps can move.

## 1. How the path works today (source evidence)

| Fact | Where |
|---|---|
| Modes: `none` (no CPU read-back), `fast` (previous slot, copied every time), `some` (previous slot, copied on a miss or every 4th resolve of the key), `full` (wait for the queue, own slot) | `src/graphics/command_processor.cpp:51-58`; mode choice `:294-302` |
| MCLA sets `some` unless the user set the cvar: "Photo Mode encodes its JPEG from a CPU-visible resolve buffer" | host `src/mcla_app.cpp:1332-1349` |
| `none` = the existing GPU-only path: `IssueCopy` returns right after `Resolve`; the data lives in the scaled-resolve buffer (scale > 1) or in shared memory (scale 1), and the texture cache samples it from there | `d3d12/command_processor.cpp:3930-3946`; `d3d12/render_target_cache.cpp:1349-1444` |
| A resolve calls `MarkRangeAsResolved`, then `RangeWrittenByGpu`: watches fire, the pages are marked valid and GPU-written, and the guest views A0/C0/E0 are write-protected (ReadOnly) | `pipeline/texture/cache.cpp:319-348`; `shared_memory.cpp:429-497` |
| The CPU copy goes through `TranslatePhysical`, the unprotected physical view. It never fires watches and never updates shared memory or the scaled buffer, so **the CPU copy can only serve CPU readers** | `system/xmemory.h:365`; `d3d12/command_processor.cpp:4277-4290` |
| Read-back path: `Resolve`; key = (address, length); slot pair `rb.buffers[2]` in a READBACK heap (`CreateNotZeroed`); **the downscale dispatch (scaled) or shared-memory copy (unscaled) and the copy into slot `write_index` are recorded unconditionally** with three `SubmitBarriers` | `:3948-4194` |
| Only then: `some` reads slot `1 - write_index` (the previous resolve of the key). It is a miss (full queue wait, own slot) if that slot is absent or too small; the l55 sync rules force a wait for composite resolves; otherwise it copies only if `++resolves_since_copy >= 4` | `:4196-4262`; constants `command_processor.h:480-484` (interval 4, 256 keys, 60-frame eviction) |
| The previous slot is read without checking that its GPU copy has completed (`submission_written` is maintained only when a diag is on) | `:4191-4194`, `:4277` |
| Read watches are not implemented: `EnableAccessCallbacks(..., enable_data_providers)` asserts false (NoAccess would be the data-provider protection); `TriggerCallbacks` asserts `is_write` ("TODO: Support read watches"). The fault handler already forwards non-write faults on NoAccess pages to the callback. | `system/xmemory.cpp:1862-1866, 1895-1897, 1986-1990`; `system/mmio_handler.cpp:467-487`; `xmemory.cpp:535-562` |

Consequences:

1. In `some` mode the GPU pays for 100% of read-back resolves, and the guest
   gets 25%. M10-001 already said "75% deferred and never used". The source
   shows that it is also **predictable before recording**. Whether a miss
   happens depends on the slot sizes, the sync rules are evaluated before the
   entry is touched, and the copy counter is known. So the unread GPU copies
   can be removed with no semantic change.
2. For a family the CPU never reads, everything after `Resolve` is waste:
   downscale, GPU copy, barriers, slot memory, memcpy and miss waits. `none`
   proves the GPU side works without it. MCLA has used `some` since l20,
   and the `none` A/B in M8-001 showed render p90 at 37.8 ms against
   47.7 ms with read-back on.

## 2. Resolve families (l53/l54 logs)

Rates: from the perf split, a steady 55.3-57.0 resolves per frame, 13.6-14.3
copied now (25.0%), 0 skipped, **11.15-11.35 MB copied per frame at 720p and
at 1440p** (the read-back is at guest resolution), memcpy 0.68-0.76 ms per
frame at 720p and 0.84-0.88 ms at 1440p, copy (inclusive) 0.9-1.0 ms at 720p
and 1.2-2.4 ms at 1440p, readback_wait about 0 stationary and 0.5-0.9 ms
while driving at 1440p. Every key is copied 1 time in 4, so the recorded GPU
read-back volume is about 11.3 / 0.25 = **45 MB per frame**.

Family keys: from the l54 diag v2 lines (448 `resolve2`, 91 `wait_resolve`,
92 `overlap`, 448 `tex_load`). These lines are a **biased sample**. `resolve2`
logs only "sporadic" keys (2 frames or more since the previous resolve), the
first frames of each minute and the cold-store world-load frame 692. Keys
resolved every frame are seen only through `overlap`/`wait_resolve`. The
counts per frame below are therefore estimates that close on the measured
56/frame and 45 MB/frame. Step 0 replaces them with exact counters.
Format codes: 2 k_8, 6 k_8_8_8_8, 22 k_24_8, 23 k_24_8_FLOAT,
32 k_16_16_16_16_FLOAT, 36 k_32_FLOAT.

| # | Family | Key evidence (EDRAM base, size, fmt, guest address) | Cadence | Est. resolves/frame | Est. MB/frame | Consumer |
|---|---|---|---|---|---|---|
| F1 | Scene colour, ping-pong | base 720, 1280x720 fmt 6, 07C48000 / 08378000 (3.77 MB) | each key every 2 frames | 1 | 3.8 | GPU (`tex_load` 1280x720 fmt 6 at both) |
| F2 | Predicated-tiling scene bands (256+256+208 = 720 rows) | base 1024: 1280x512 / 1280x256 / 1280x208 fmt 32 (5.24/2.62/2.29 MB) and fmt 6 (2.6/1.31/1.15 MB), depth 1280x256 fmt 23 (078B0000/079F0000) | per frame, ping-pong addresses | about 9 | about 15 | GPU (1280x720 fmt 32/23/6 loads) |
| F3 | Shadow atlas quadrants | depth base 0, 640x640 fmt 22, extents of 3.19 MB at 06E65000/06E79000/07185000/07199000 inside a 1280x1280 fmt 22 texture | per frame | 4 | 12.8 | GPU (1280x1280 fmt 22 load) |
| F4 | Colour tiles into the shadow key | base 224, 256x256 fmt 6 at 06EB5000/06EF5000/06F35000/06F75000 | per frame | 4 | 1.0 | GPU |
| F5 | Small reflection/impostor tiles | base 224, 128x128 fmt 6, about 30 keys 02EC0000-0327C000; plus 128x128/256x256 fmt 2 at `since_prev=49` | every 2 frames (fmt 2: about 49) | about 15 | 1.0 | GPU (128x128 fmt 6 loads) |
| F6 | Bloom/blur chain | base 0, 640x360 fmt 32 (06FB5000/0720D000), 320x184 fmt 32 (02D6E000/02E22000/07195000) | per frame | about 5 | about 5 | GPU |
| F7 | Luminance chain | base 0, fmt 36: 128x128 073ED000, 64x64 073FD000, 16x16, 8x8 07402000, 8x8 02D6D000, 8x8 02D6C000 (loaded as 4x4 and 1x1) | per frame | about 6 | 0.1 | GPU; **CPU read possible** (auto-exposure) |
| F8 | Small HDR targets | fmt 32: 512x256 032E2000 (1 MB), 256x128 033E2000, 64x32 032DE000, 256x256 05AC3000 | per frame | about 5 | about 2 | GPU; 64x32 **CPU read possible** |
| F9 | Shared scratch 06ACD000 | 1280x512 fmt 6 (2.6 MB), 256x256, 224x224 fmt 6, 64x32 fmt 32, depth 1280x512: several lengths, one base | several times per frame | about 4 | about 3.5 | GPU (1280x720 fmt 6, 220x220, 256x256 loads) |
| F10 | Misc k_8 / colour | 512x512 fmt 2 base 832 (0329E000), 256x256 fmt 2 (0328C000), 256x256 fmt 6 (03702000), 128x128 fmt 6 base 1601 (036F2000) | per frame or slower | about 3 | about 1 | GPU; k_8 **CPU read possible** (occlusion or flare) |
| F11 | **Livery/vinyl composite** | base 0, fmt 6, 1024x1024 at 050A0000 (4 MB) and mips 512..8 in the same range (two keys of different length share 050A0000), 512x512 at 054A0000, 256x256 at 05711000 | at car spawns only (6-10 resolves per composite) | rare | rare | **CPU** (builds the DXT5 atlas; KI-033 fixed the CPU reader of this read-back) |
| F12 | **Photo capture** | not yet seen in a diag log (no photo in l54) | Photo Mode window | rare | rare | **CPU** (guest JPEG encoder; KI-021) |

Totals: about 56 resolves and about 45 MB per frame, consistent with the
measured values.

On "CPU never reads": the logs prove GPU consumption (every steady family has
`tex_load` lines of its range with `scaled_resolve=1`). They do not prove the
absence of CPU reads. The diag has no read detection at all. That is why the
design measures CPU touches before skipping anything. F7, F8 (64x32) and F10
(k_8) are the families where a CPU read would be a normal console pattern
(exposure, histogram, sun occlusion). Signs (KI-038/032) are not identified as
a family. Their H2 hypothesis ("the game consumes the resolve on the CPU") is
open, so the census must cover the Selma/Highland signs.

Readback_wait while driving (0.5-0.9 ms per frame at 1440p, about 0
stationary) comes only from misses (no forced resolves in l53/l54). Each miss
is a full queue drain. Fresh keys while driving are dominated by car spawns
(F11) and evicted sporadic keys. Step 0 attributes the waits to families
before any gain is claimed for them.

## 3. Design

### 3.1 Step 1: exact elision of unread GPU copies (`some` only)

Move the read decision before the GPU recording in
`IssueCopy_ReadbackResolvePath`. All inputs are known there:

```
prev_ok    = rb.buffers[1-w] && len <= rb.sizes[1-w] && rb.mapped_data[1-w]
miss       = !prev_ok
forced     = sync rules (already evaluated before the entry is touched) && !miss
copies_now = forced || miss || rb.resolves_since_copy + 1 >= 4
own_slot   = forced || miss                 // this resolve reads slot w itself
next_cnt   = copies_now ? 0 : rb.resolves_since_copy + 1
need_gpu   = mode != some || own_slot || next_cnt + 1 >= 4   // the next resolve of this key reads slot w
```

If `!need_gpu`, skip the downscale dispatch, the slot copy and their barriers.
Keep the slot allocation, the counters, `current_index`, the miss logic and
eviction exactly as now. The slot then keeps older bytes that nobody reads.
Eviction and size growth already turn the next resolve into a miss (own
slot). A later forced resolve reads its own slot. So the bytes delivered to
guest memory are identical in every case.

- Cvar: `d3d12_readback_elide_unread` (bool, hot reload, SDK default false).
  Add it to the host dev-override whitelist (`src/mcla_dev_overrides.h`
  `kKeys`) for the A/B.
- Counters (perf split): `rb_gpu_recorded`, `rb_gpu_elided`,
  `rb_gpu_bytes_recorded`, `rb_gpu_bytes_elided`.
- **A/B invariant (correctness check in the run itself):** `copies_now`,
  `copies_deferred` and `readback_kb` per frame must match between the arms
  within route noise (they did to 1% across all l53/l54 windows), and
  `MCLA_READBACK_SYNC` `forced` must be unchanged. Unit test: a model of the
  two-slot state machine (miss, forced, interval, eviction, size growth) run
  over random key sequences with and without elision. The bytes copied to
  "guest memory" must be identical.
- `fast` mode reads the previous slot every time, so nothing can be elided.
  `full` reads its own slot. Neither changes.

### 3.2 Step 2: a read-watch detector (census, no behaviour change)

Implement the read half of the watch machinery for resolved ranges only:

- `MakeRangeValid(..., written_by_gpu=true)` arms the guest views today with
  ReadOnly. For resolved ranges whose key is not `CPU_READ`, arm **NoAccess**
  instead, in the same `EnableAccessCallbacks` pass. This is the existing
  `protect_access` choice at `xmemory.cpp:1895`, so it adds no VirtualProtect
  call. Add a per-system-page `read_watch` bit next to
  `notify_on_invalidation`. The key decision must reach the arm, so `Resolve`
  needs the key decision before `MarkRangeAsResolved`: pass a small policy
  callback, or compute the extent first.
- Fault path: in `Memory::AccessViolationCallback`, before `TriggerCallbacks`,
  if the page has `read_watch`, clear it and restore ReadOnly (the page is
  still write-watched). Record the touch (key lookup by page, thread id, read
  or write) and return handled. A write fault on a read-watched page goes to
  the normal invalidation path and also counts as a write touch.
  `TriggerCallbacks` keeps its `is_write` assert because reads never reach it.
- Per-key state in the read-back key map, which lives longer than the
  60-frame slot cache. Keep a separate small map keyed by (address, length)
  with a family id. States: `UNKNOWN` (armed NoAccess, normal `some`
  behaviour), `CPU_READ` (touched; armed ReadOnly as today, sticky for the
  session) and later `SKIP` (step 3).
- Only guest code and HLE functions that use the virtual views fault.
  Texture uploads, the CP and the read-back memcpy use the physical view.
  Partial pages shared with other data can cause false touches. That errs on
  the safe side (more copies).
- Cvar: `d3d12_readback_read_watch` (bool, hot reload, default false).
  Counters: `rw_armed_resolves`, `rw_touch_read`, `rw_touch_write`,
  `rw_keys_cpu_read`, fault time; one `MCLA_READBACK_SCOPE v=1 kind=touch`
  line per newly touched key (family, base, len, size, fmt, EDRAM base,
  thread, frame), paced like the diag v3 buckets.

### 3.3 Step 3: skip CPU copies for steady, untouched keys

A key enters `SKIP` when all of these hold:

- at least K = 120 resolves (about 3-6 s), each within 2 frames of the
  previous one, while armed, with zero touches;
- it is not in the composite family (l55 rules a/c/b in scope 0: colour from
  EDRAM base 0 to k_8_8_8_8, square power of two 64-1024, a shared range of
  different length, or sporadic after 8 or more frames). Composite resolves
  keep the forced synchronous copy, so KI-047 is unchanged;
- the host photo window is closed. The window bracket in
  `src/mcla_photo_capture.cpp` already toggles SDK cvars, so it would also set
  `d3d12_readback_skip_suspend`.

In `SKIP` a resolve does no downscale, no slot copy, no barriers and no
memcpy. Its slots can be released after 60 frames as usual. The pages stay
read-watched.

A key leaves `SKIP` on any touch (it becomes `CPU_READ`), on a cadence break
(more than 8 frames between resolves, back to `UNKNOWN`) or on a suspension
(photo window). **The first resolve after leaving `SKIP` must be synchronous
(own slot, as a miss).** The previous slot was never written while skipped.
Without this the photo capture (if it reuses F1) or a promoted key would read
very old bytes.

A touch while in `SKIP` means the guest read bytes from the last copy before
skipping started. With step 3 alone that is one stale read per key per
session. It is logged (`kind=touch state=skip`) and counted as `skip_violation`.
**The acceptance gate is `skip_violation=0`.** Step 4 removes the stale read
if needed.

- Cvar: `d3d12_readback_skip_untouched` (bool, hot reload, default false;
  requires `read_watch`).
- Counters: `skip_resolves`, `skip_bytes`, `skip_keys`, `promotions`,
  `skip_violation`, `skip_leave_sync`; per-family resolves, bytes and state
  in the minute line; the key table at F10.

### 3.4 Step 4 (conditional): synchronous fallback on a first touch

Needed only if step 3 shows violations, or as insurance before a volunteer
build.

- The fault handler records {key, range}, **releases the global critical
  region**, posts a request to the CP and waits on an event with a timeout
  (50 ms).
- The CP services requests at packet boundaries and inside its waits
  (WAIT_REG_MEM poll, ring-idle wait, fence wait). Servicing a request means
  recording a downscale and copy of the *current* scaled range (or shared
  memory) into the key's slot, ending the submission, waiting for its fence,
  copying to guest memory through the physical view and signalling.
- The current content is what the guest should see, because every resolve
  the CP has processed is in the recorded stream before the service point.
- On a timeout the read proceeds stale and `fallback_timeout` is logged. This
  bounds the risk of a deadlock when the CP waits on the faulting thread.
- Cvar: `d3d12_readback_touch_fallback`. Counters: `fallbacks`,
  `fallback_ms`, `fallback_timeout`.

Two cheap correctness items fit here, each with its own switch:

- (a) **Previous-slot fence check** for CPU-read keys. Before the memcpy,
  wait for the specific submission of the previous slot if it is not yet
  complete. Track `submission_written` always. Counters `prev_slot_waits`
  and `prev_slot_wait_ms`. This closes the "previous slot torn while in
  flight" race noted for KI-037/KI-047/KI-056.
- (b) **Full sync inside the photo window**, as a KI-056 (album preview)
  candidate. The host switch would be `mcla_photo_readback_sync`.

### 3.5 Step 5 (probably not needed): batching the remaining copies

After steps 1+3 the GPU read-back that remains is about 1 in 4 resolves of
the CPU-read and `UNKNOWN` keys: a few per frame. Batching them (one
transition of the scaled buffers, N dispatches into a sub-allocated downscale
buffer, one barrier, N copies, one barrier back at frame end) is safe only
with two flush triggers. The batch must flush before any later resolve
overlapping a pending range (F9 reuses 06ACD000 several times per frame) and
before any slot read of a pending key. The gain is about 3 barrier batches
per remaining resolve. Decide from step 3's `rb_gpu_recorded` per frame:
below about 5 per frame, close as not justified.

### 3.6 Vulkan and the Deck

`vulkan/command_processor.cpp:4454-4700` has the same structure. Port steps
1-3 after the D3D12 A/Bs, then run M10-004/005. On the Deck (APU, vkd3d) the
copy is memory to memory in shared DRAM: no PCIe, but the same 45 MB per
frame of bandwidth plus about 11 MB per frame of memcpy on Zen 2, and write
faults that cost about 80 us each (M7-016). Measure there rather than
extrapolate.

## 4. Expected gains (arithmetic)

Inputs: B = 11.3 MB copied per frame / 0.25 = **45 MB recorded per frame** in
56 resolves. 1440p = scale 2.

| Component of GPU read-back cost per frame (1440p) | Estimate |
|---|---|
| GPU copy to the READBACK heap (system memory over PCIe; x8 Gen4 about 12-14 GB/s effective, faster if Gen5 or overlapped) | 45 MB / 12-25 GB/s = 1.8-3.8 ms |
| Downscale dispatch (scale 2): read 4B = 180 MB, write 45 MB of VRAM at about 250-300 GB/s effective | 0.75-0.9 ms |
| Serialisation: about 3 barrier batches per resolve x 56, a few us of drain each | 0.3-0.8 ms |
| **G_rb, total** | **2.9-5.5 ms (11-21% of 25.6 ms)** |

At 720p (scale 1) there is no downscale: G_rb is about 2.1-4.6 ms of about
12.5 ms GPU per frame. That GPU time is not on the critical path at 720p.

| Step | GPU per frame, 1440p | CP per frame | Notes |
|---|---|---|---|
| 1 elision | -0.74 x G_rb = **-2.1 to -4.1 ms** | -0.06 ms (readback_gpu 0.08 x 0.74) | 1440p GPU-bound: 25.6 to 21.5-23.5 ms, so **38 to about 42-46 fps** (+10-19%), capped near 49 fps by the render thread (M10-001 lap 1). 720p: GPU headroom only. |
| 2 read watch | about 0 | about 0 (protection type changed in the existing arm; one fault per CPU-read key until it is sticky) | overhead check, census |
| 3 skip | -0.26 x G_rb x s = -0.7 to -1.4 ms (s = share of steady bytes in skippable families, expected >= 0.95) | **-0.8 ms** memcpy (0.85 x s), plus miss waits of skipped families (step 0 attributes; expected small), about 90 MB less READBACK-heap memory | 720p: -0.7 ms CP where the CP is within 3-6% of the wall |
| 4 fallback / fence / photo sync | 0 in steady play | 0 in steady play | correctness |
| 1+3 together | **-2.9 to -5.5 ms** | **-0.85 ms** | must beat run-to-run spread and reach >= 5% (M10 rule) |

The GPU numbers are estimates. Step 0 measures G_rb directly before any
behaviour change. If G_rb is below about 1.3 ms (5% of the frame), steps 1
and 3 are re-ranked by their CP value only.

## 5. Correctness risks and how a test sees them

| Risk | Mechanism | Detection |
|---|---|---|
| Step 1 changes delivered bytes | wrong `need_gpu` (forced, miss, interval, eviction, size growth) | model-equivalence unit test; A/B invariant `copies_now`/`readback_kb`/`forced` equal per frame; KI-047 trap laps |
| A steady family is read by the CPU (exposure F7, 64x32 F8, k_8 F10, signs) | skip would freeze or stale its data | the step 2 census over a full tour before step 3; `skip_violation` (must be 0); visual checks: a tunnel or underpass (exposure adapting), the sun low in front (flare or occlusion), Selma/Highland signs at 1440p and 2160p (KI-038/032), GPS map and minimap |
| Guest partial write into a skipped range | write-watch reload merges stale bytes (same class as `none`) | `rw_touch_write` on SKIP keys gives a promotion and a log line |
| Photo capture reuses a per-frame key (F1) | skipped slot, then a stale JPEG or preview | photo window suspend plus "first resolve after SKIP is synchronous"; check the first photo, the album slot preview (KI-056), and the saved album image after reopening (KI-037) |
| Livery composite | must never skip | composite family excluded; `MCLA_READBACK_SYNC forced` and flight-recorder `composite sync_forced=1`; police trap laps (KI-047), garage vinyl apply then drive (KI-033), traffic spawns |
| Read-watch plumbing | NoAccess pages hit by host code under the global lock, or the `is_write` assert path | unit tests of the fault path; a 30-minute soak with `rw_*` counters; crash-free F10 packages |
| Deck fault cost (about 80 us) | CPU_READ keys that fault often | arm NoAccess only in UNKNOWN or SKIP; `rw_touch_read` per minute |
| Fallback deadlock (step 4) | CP waiting on the faulting thread | 50 ms timeout plus `fallback_timeout`; the CP services requests inside its waits |

## 6. Incremental plan (each step one owner BENCH A/B)

Common to all steps: same BENCH copy and route (stationary, lap 1, lap 2),
warm store stated, 1440p Unlocked (plus a 720p pair for steps 3-4).
Switches go through the dev-override file (bool keys only). Read
`MCLA_PERF_SPLIT`, frame pacing, `MCLA_READBACK_SYNC` and the new
`MCLA_READBACK_SCOPE` minute lines.

| Step | Switch (default) | What the run shows | Pass |
|---|---|---|---|
| 0 Measure | `d3d12_readback_census` (diag; on in internal builds). Per-family minute counters (resolves, bytes, copies, misses, miss_wait_ms, forced), GPU timestamps around each read-back block (`rb_gpu_ts_ms`), a key table at F10 | one baseline run, no A/B: G_rb, exact families and rates, where the miss waits come from | overhead within spread (as diag v2); G_rb known |
| 1 Elide | `d3d12_readback_elide_unread` (false) | gpu_busy/frame and fps on/off; `rb_gpu_elided` about 74% | invariants equal; GPU -5% or more at 1440p; police lap clean |
| 2 Census | `d3d12_readback_read_watch` (false) | overhead (arm, fault ms) on/off; the touched-key list over a tour (photo, garage vinyl, police, signs, tunnel, map, menus) | overhead within spread; touched families = F11/F12 (+ any of F7/F8/F10) |
| 3 Skip | `d3d12_readback_skip_untouched` (false; needs 2) | memcpy, copy, readback_wait and GPU on/off at 1440p and 720p | `skip_violation=0`; CP -0.5 ms or more per frame; GPU further down; named checks pass |
| 4 Fallback and fences | `d3d12_readback_touch_fallback`, `d3d12_readback_prev_slot_fence`, host `mcla_photo_readback_sync` (false) | only if 3 had violations, or for KI-056 | KI-056 preview correct; no timeouts |
| 5 Batch | (decide from counters) | only if `rb_gpu_recorded` stays above about 5 per frame | otherwise closed as not justified |
| 6 Vulkan/Deck | ports of 1-3 | Deck before/after (M10-004/005) | Deck frame time down; checks pass |

Step 1 has the best ratio of value to risk. It is exact, local (one
function), easily unit-tested, and its A/B carries its own correctness
invariant. It can ship in the next owner build as soon as step 0's timestamps
(or the A/B itself) confirm the GPU gain.

## 7. Open points

- G_rb is an estimate (PCIe link generation, copy overlap, drain cost).
  Step 0 measures it.
- The family table comes from a biased sample of one l54 session. Every-frame
  keys are visible only through `overlap`/`wait_resolve`.
- The photo capture key is not identified (no photo in a diag-v2 session).
  Step 0's F10 key table during a photo would show it.
- Signs (KI-038/032): whether any sign path reads a resolve on the CPU is
  unknown. The census answers it, but only at the Selma/Highland location,
  at both 1440p and 2160p.
