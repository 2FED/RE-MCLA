# M11 shared-memory uploads and memory watches: mechanism, cost and levers (design study, read-only, 2026-11-20)

Read-only design study by an Opus subagent for the primary agent. Nothing was built, launched,
edited or committed. Sources: the SDK worktree at `e6eeeb9` (l117; `<sdk>` below), the l117 per-draw
budget ([M11-l117-sdk-cp-per-draw-20261119.md](M11-l117-sdk-cp-per-draw-20261119.md)), the earlier
watch evidence (M10 watch/upload design, M10 l63 PC A/B, M10 hot blocks + l66 Deck A/B, M11 Deck
memwatch design, l81 Deck census, GPU-time research), and a deterministic parse of the
`MCLA_PERF_SPLIT v=1`, `MCLA watch stats/min` and `MCLA watch protect/min` minute lines of two PC
sessions: **c46e0301** (l116, pc/gpu-cat-race, single tile, day and night, 25 minutes) and
**194fcd9a** (l117v pre-share check, guest tiling 2-3 tiles per `MCLA_GTILE`, day and night,
60 minutes). Xenia Canary (BSD-3) was read on GitHub for section 3 only; no code was copied. No
GPL or unlicensed peer project was consulted. Spark was not used: the work is source reading plus
a small deterministic log parse whose validation would cost more than doing it.

## 0. Headline: correct the "about 1 us per draw of CP"

The l117 table adds four numbers that do not belong to one thread and overlap:

| field | thread | how measured | relation |
|---|---|---|---|
| `upload_ms` | CP | TSC region `kUpload` around `UploadRanges` (`<sdk>/src/graphics/shared_memory.cpp:880-888`), exclusive of nested regions only | **contains the arm** (`UploadRanges` -> `MakeRangeValid` -> `EnablePhysicalMemoryAccessCallbacks`, `<sdk>/src/graphics/d3d12/shared_memory.cpp:402`, `shared_memory.cpp:533-539`) |
| `watch_arm_ms` | all threads (in practice the CP) | process-wide atomic, per call `duration_cast<microseconds>` (`<sdk>/src/system/xmemory.cpp:522-545`) | inside `upload_ms`; **truncated per call** |
| `write_fault_ms` | guest threads (render thread about 95 %, CP 0) | `<sdk>/src/system/mmio_handler.cpp:432-446`, excludes OS/Wine exception dispatch | **not CP time** |
| `watch_disarm_ms` | guest threads | inside the fault path (`PhysicalHeap::TriggerCallbacks`, `xmemory.cpp:2500-2648`) | **inside `write_fault_ms`** |

So the CP cost is `upload_ms` alone: **0.44-0.64 us per draw single tile, 0.24-0.45 us two/three
tiles**. Of that, the arm syscall is not 0.15-0.20 but about 0.25-0.30 us: the per-call
microsecond truncation under-reports a ~1.2 us Windows `VirtualProtect` by about 40 % (l63 PC census,
TSC ticks: 1.16 us per call against 0.66 us from `protect/min`, `M10-l63-pc-ab-20261102.md:290,333`;
the M11 Deck design measured only 3 % on the Deck, where calls take 16 us). The fault side
(0.23-0.33 us per draw including disarm) runs on the render thread and holds the global lock. The
Deck design of 2026-11-08 made the same correction for the Deck (`M11-deck-memwatch-design-20261108.md`
section 0); the l117 summary re-added the parts.

## 1. Mechanism (source at e6eeeb9)

**Granularity.** One host buffer of 512 MiB mirrors guest physical memory (`kBufferSizeLog2 = 29`,
`<sdk>/include/rex/graphics/shared_memory.h:43`), a reserved (tiled) default-heap resource with 4 MiB
sparse allocations (`d3d12/shared_memory.cpp:48-53`, `shared_memory.h:166`). Validity and
protection are per **host system page, 4 KiB** (`page_size_log2_` from `rex::memory::page_size()`,
`shared_memory.cpp:155-157`; `dwPageSize` on Windows). Bitmaps are 64-page (256 KiB) words:
131,072 page bits.

**State.** `valid` flags are double-buffered (`valid_buffer_a_/b_`, `active_valid_flags_`,
`shared_memory.h:318-321`) for the lock-free check; `system_page_flags_valid_and_gpu_written_` marks
GPU-written (resolve / memexport) pages and is folded into the staging copy at frame end
(`SetSystemPageBlocksValidWithGpuDataWritten`, `:266-309`). Protection state is separate, in the
physical heap: `notify_on_invalidation` per system page (`xmemory.cpp:2327-2352`).

**Request (CP thread).** Per draw: the index buffer (`primitive_processor.cpp:931`, about 0.8 per
draw), each vertex fetch slot that is not in sync (`d3d12/command_processor.cpp:6244-6292`), memexport
ranges (`:6310`), texture data (`pipeline/texture/cache.cpp`). `RequestRanges`
(`shared_memory.cpp:683-890`) sorts and merges the ranges into a thread-local vector (`:696-735`),
ensures sparse residency (`:737-741`), then **lock-free** checks the active valid bitmap (`:743-785`):
if every page is valid it returns (96-97 % of VB/IB requests on the Deck census; about 1.7-1.8
requests per draw on the PC, 0.18-0.23 uploads per draw). Otherwise it takes the global critical
region (`:801-857`), builds page runs of invalid pages (`append_upload_range` coalesces adjacent runs,
`:788-851`), optionally extends them (upload-ahead, `:853-856`, off on Windows), and calls
`UploadRanges` inside the `kUpload` region (`:880-888`).

**Upload (CP thread), per call, not batched** (`d3d12/shared_memory.cpp:376-415`):
1. `CommitUAVWritesAndTransitionBuffer(COPY_DEST)` + `SubmitBarriers()` (`:383-384`): one transition
   of the whole 512 MiB buffer unless it is already in COPY_DEST;
2. per run: `D3D12UploadBufferPool::RequestPartial` (2 MiB upload-heap pages in system memory,
   `graphics_upload_buffer_pool.h:30`, `d3d12_upload_buffer_pool.cpp:78`), tied to the current
   submission and reclaimed on its fence (`:175-177`);
3. `MakeRangeValid` **before** the copy (`:402`; the comment at `shared_memory.h:194-201`): under the
   global lock it sets the valid bits, clears GPU-written bits, updates the per-page histogram of the
   minute line (`CountValidPagesLocked`, `shared_memory.cpp:62-140`), then **arms**: the physical heap
   walks the pages and calls `VirtualProtect(PAGE_READONLY)` once per run of not-yet-watched writable
   pages (`xmemory.cpp:2253-2392`), each call timed twice (steady_clock and TSC) plus atomics;
4. `std::memcpy` from guest memory into the write-combined upload page (`:403-405`);
5. `D3DCopyBufferRegion` into the deferred command list (`:406-408`).
The next draw transitions the buffer back with `UseForReading()` + `SubmitBarriers()`
(`command_processor.cpp:6398-6402`, `:6510-6512`). So each upload between draws costs **a barrier
pair**, replayed by the D3D12 replay thread and executed as a GPU drain.

**Invalidation (guest thread).** A guest store to a read-only page raises an access violation on the
**writing guest thread** (the render thread in about 95 % of faults; CP 0, Deck census). VEH
`MMIOHandler::ExceptionCallback` (`mmio_handler.cpp:394-460`) -> `Memory::AccessViolationCallback`
(`xmemory.cpp:547-581`) with the global lock -> `PhysicalHeap::TriggerCallbacks` (`:2394-2663`) ->
`SharedMemory::MemoryInvalidationCallback` (`shared_memory.cpp:903-996`): the invalidation is
**widened to the whole 64-page / 256 KiB block** unless GPU-written pages bound it (`:930-957`; env
`MCLA_INVALIDATE_WIDEN_PAGES`, default 64, `:141-152`), valid bits are cleared, texture and range
watches fire (`FireWatches`, `:429-458`), then the heap **disarms** the watched pages of that window
with `VirtualProtect(PAGE_READWRITE)` (`xmemory.cpp:2500-2648`, bridging of safe gaps on Wine only)
and clears the notify bits. Exact-range invalidations (guest kernel protect, decommit, release,
`:2205-2242`) take the same path without the widening.

**Locking.** Arm, fault and disarm all run under `global_critical_region`, the same static recursive
mutex as the guest atomics. The CP's fast path takes no lock; its slow path takes the lock twice
(run build, `MakeRangeValid`) plus once more inside the heap walk (recursive). PC lock wait is small
(arm lock wait 13-37 ms per minute, `protect/min` lines).

**Per-frame life cycle of a dynamic VB/IB ring block** (numbers: c46e0301 day, about 3.9 k draws):
```
frame N   guest writes ring head -> hits a READONLY page -> fault (render thread, ~10 us in handler
          + Windows dispatch) -> invalidate 64 pages (256 KiB) -> fire watches -> ~3.5 disarm calls
          CP draw k  : request few KB -> page invalid -> lock -> run of ~2.2 pages -> barrier
                       ->COPY_DEST, arm (VirtualProtect RO ~1.2 us), memcpy ~9 KB, CopyBufferRegion
                       -> draw: barrier ->read
          CP draw k+1: next sub-range, same or next page ... (~10 uploads + 10 arms per fault)
          guest writes the next draw's bytes into a page the CP just armed -> fault again
frame N+1 same pattern on the same blocks
```
Measured per fault: 9-14 arm calls (= uploads), 3.0-3.6 disarm calls, 63-64 pages invalidated, about
22 pages re-uploaded.

**A finding outside the brief (correctness, not perf): the cross-frame vertex-slot cache.** The VB
loop skips the request when the slot's fetch constant has the same address and size as at its last
request (`command_processor.cpp:6275-6278`, `vertex_buffer_states_`). `IssueSwap` clears only the
in-sync bits (`:3092-3093`); the address/size states are reset only by `ClearCaches`, `SetupContext`
and shutdown (`:611`, `:1407`, `:2266`). A buffer that the guest rewrites **at the same address
and size** in a later frame is therefore never re-requested, whatever the fault invalidated. Xenia
Canary requests every vertex range every draw (its own TODO "Cache residency for ranges"). The cache
came with the ReXGlue port commit `e6407f3`. It is harmless if the game's dynamic data always moves
(rings) and static data never changes. That is plausible but unproven. A one-line counter settles it
(section 2.4, `vb_skip_invalid`). It also means the PC request counts already exclude same-address
re-uses.

## 2. Where the time goes (PC logs; Deck from l81)

### 2.1 Per frame and per draw (minute windows above 1,500 draws per frame)

| | c46e0301 day (1 tile) | c46e0301 night | c46e0301 dense minute | 194fcd9a day (2 tiles) | 194fcd9a night (3 tiles) | 194fcd9a static 30 fps (3 tiles) |
|---|---|---|---|---|---|---|
| draws / frame, fps | 2.7-4.8 k, 53-71 | 2.3-2.7 k, 57-73 | 5.2 k, 44 | 4.6-7.6 k, 48-60 | 3.9-7.2 k, 43-59 | 10.4 k, 30.0 |
| requests / draw | 1.72-1.83 | 1.65-1.72 | 1.86 | 1.82-1.84 | 1.57-1.85 | 1.83 |
| uploads / frame (= arm calls) | 580-1,040 | 500-650 | 1,180 | 620-1,510 | 440-1,570 | 1,770 |
| KB per upload | 8.7-10.5 | 8.9-9.7 | 9.1 | 8.5-9.7 | 9.0-10.7 | 9.0 |
| CP upload ms / frame | 1.45-2.39 | 1.24-1.52 | 2.54 | 1.51-3.24 | 1.12-3.05 | 3.30 |
| CP upload us / draw | 0.44-0.53 | 0.51-0.64 | 0.49 | 0.33-0.43 | 0.24-0.45 | 0.32 |
| CP us per upload (incl. arm) | 2.3-2.5 | 2.4-2.5 | 2.15 | 2.1-2.4 | 1.9-2.4 | 1.9 |
| arm us / call (truncated; true about +0.5) | 0.71-0.87 | 0.78-0.90 | 0.64 | 0.67-0.83 | 0.50-0.92 | 0.46 |
| write faults / frame (invalidations, exact 0) | 54-94 | 50-72 | 106 | 62-117 | 49-125 | 125 |
| render-thread fault ms / frame (handler only) | 0.59-1.00 | 0.49-0.61 | 1.09 | 0.57-1.30 | 0.45-1.30 | 1.43 |
| us per fault in handler | 10-11 | 9.5-10 | 10.3 | 9.3-11.1 | 9.2-11.5 | 11.4 |
| disarm calls / fault | 3.4-3.6 | 3.4-3.8 | 3.6 | 3.5-3.8 | 3.0-3.6 | 3.6 |
| arm calls / fault | 9-11 | 9.8-10.8 | 11.3 | 10-11 | 8-14 | 14.2 |
| CP busy / GPU busy ms / frame | 9.8-14.0 / 12.5-14.7 | 8.1-10.8 / 12.0-13.1 | 15.1 / 15.8 | 12.8-19.1 / 15.8-19.2 | 11.2-16.8 / 15.1-19.4 | 22.6 / 21.6 |

Readings:
- **Uploads scale with guest frames, not with draws.** Tiling doubles or triples the draws but not
  the uploads: per draw the upload cost falls from 0.44-0.64 to 0.24-0.45 us. The replayed tile's
  requests are already the lock-free fast path (question 4g is answered by the data).
- **At 6 k draws the PC spends about 2.5-3.2 ms of CP per frame here**, 13-17 % of a 15-19 ms CP
  frame. About 55 % of it is the `VirtualProtect` arm (true about 1.2-1.4 us x 1,200-1,500 calls,
  1.5-2.0 ms). The rest is memcpy (9 KB into write-combined memory, about 0.4-0.6 us), the barrier
  pair, the pool and the copy record (about 0.5-0.8 us per upload together).
- Fault side, render thread: 1.0-1.3 ms per frame at 6 k draws plus the unmeasured Windows dispatch.
- The CP's kernel time, 2.0-4.6 ms per frame, is mostly the arms.

### 2.2 Hot versus cold pages (the always-on `watch stats/min` histogram, CPU uploads only)

| | c46e0301 | 194fcd9a (moving) | 194fcd9a static |
|---|---|---|---|
| CPU upload pages / frame | 1,280-2,700 | 1,330-2,340 | 3,970 |
| distinct pages uploaded per minute | 34-37 k (about 140 MB) | 35-42 k | 3,566 |
| pages uploaded about every frame (>= 1,800 per min) / their share of page uploads | 430-1,140 / 30-44 % | 190-640 / 13-27 % | 3,000-3,100 / 94-96 % |
| pages uploaded at least every ~6 frames (>= 600 per min) / share | 1,400-4,000 / 48-84 % | 1,450-3,700 / 45-63 % | 3,480 / 99 % |
| invalidated pages / frame (64 per fault) / re-uploaded | 3.2-6.8 k / about 1/3 | 4-7.5 k / about 1/3 | 8 k / 1/2 |

While driving, only 13-44 % of uploads hit pages rewritten every frame; most of the volume is
"warm" (rewritten every few frames). That is the instability that sank hot pages v2 on the Deck
(l81: the h8 share fell from 84 % to 49 % in fast driving). In a static scene the dynamic set is
small and fully hot.

### 2.3 Deck reference (l81 census, Proton, 5.7 k draws, 16.5 fps)

Arm 383 calls per frame at 16.65 us (6.4 ms); upload region 11.5 ms per frame (2.0 us per draw,
about 23 % of the 50 ms CP); disarm 142 at 19.5 us (2.8 ms); render faults 133 at 35 us (4.7 ms) plus
unmeasured Wine dispatch; VB/IB fast path 97 / 96 %. On the Deck an arm costs 13x the PC and a fault
3.5x. The CP has the same structure.

### 2.4 What the counters cannot say, and the counters that would

Unknown today:
1. the true arm and disarm time on the PC (truncation);
2. the split of the 1 us non-arm upload cost (memcpy, barriers, pool, lock);
3. Windows and Wine exception dispatch per fault;
4. requested bytes per request, compared with uploaded pages;
5. for hot pages, requests per upload (R/U), which decides every "do not protect" option;
6. how many uploads re-send bytes that did not change (false invalidation from the 256 KiB
   widening);
7. how many uploads are the first touch of their pages in the current submission (prologue
   eligibility);
8. whether the cross-frame VB slot cache ever skips an invalid range.

"Uploaded but never drawn" is zero by construction: an upload happens only for a request. The waste
that matters is items 6 and 5.

Proposed counters (cheap; TSC only; one line per perf-split window, `MCLA_SHM v=1`; items marked (c)
only while `memory_watch_census` is on):

`MCLA_SHM v=1 reason= seq= req=vb:N,ib:N,tex:N,oth:N req_kb=vb:N,ib:N,tex:N fast=N up=N up_pages=N
up_req_kb=N arm_calls=N arm_ms= disarm_calls=N disarm_ms= faults=N fault_ms= fault_lock_ms=
up_memcpy_ms= up_rec_ms= up_first_sub=N up_retouch_sub=N up_gpu_sub=N up_sampled=N up_same=N
vb_skip=N vb_skip_invalid=N hot8_req=N hot8_fast=N hot8_up=N hot8_req_kb=N(c) hot8_pages=N(c)`

- `arm_ms`/`disarm_ms` from TSC ticks summed without truncation. Drop the second `steady_clock`
  pair in `ProtectWatchRange` and the lock-wait `steady_clock` pair in `EnableAccessCallbacks`. That
  is also a micro-saving of about 0.1 us per arm.
- `faults` = delta of the existing `non_mmio_write_fault_count_` (`mmio_handler.cpp:445`; it is
  counted but never logged per minute).
- `up_memcpy_ms`, `up_rec_ms`: two TSC reads around the memcpy and around the barrier and copy
  recording in `UploadRanges`.
- `up_first_sub` / `up_retouch_sub` / `up_gpu_sub`: a per-submission page bitmap stamped by every
  request, including fast-path ones (one relaxed OR per request page, CP thread), and by
  `RangeWrittenByGpu` / memexport. At upload time it tells whether the pages were already read or
  written by the GPU in this submission.
- `up_same`: 1 in 16 uploads hashes each uploaded page (XXH3) and compares the hash with the one
  stored at that page's previous upload (a 4-byte hash per page, 512 KiB table). `up_same / up_sampled`
  is the share of upload bytes that did not change.
- `vb_skip_invalid`: in the `state.address == ...` skip (`command_processor.cpp:6276`), a lock-free
  valid check of the range. Above 0 means the cache skipped a rewritten buffer (a correctness
  report, see section 1).
- `hot8_*` (census): per-page 8-bit history "uploaded in frame", shifted at frame end
  (`WatchCensusFrameEnd` already does this per block). The request, fast-path and upload counts of
  pages with history 0xFF give R/U and requested bytes for the hot set. The block-level census already
  has `by_hot_requests/by_hot_fast/by_hot_uploads` (`shared_memory.cpp:1189-1193`): **no build needed
  for a block-level R/U** (section 5, step 0).
- A start-up probe line `MCLA_FAULT_PROBE v=1 n=256 rt_us_p50= rt_us_p90= handler_us_p50=
  protect_ro_us_p50= protect_rw_us_p50=`: protect a scratch watched page, store into it and time the
  full round trip with TSC around the store. The difference from the handler time is the OS or Wine
  dispatch. It runs once, on the CP thread, before the game starts. It gives the Deck's missing
  Wine dispatch number and the PC's true `VirtualProtect` cost.

## 3. Xenia Canary upstream today (BSD-3, `canary_experimental`)

- `src/xenia/gpu/shared_memory.cc` (latest `aed81ca9`, 2026-08; also `5cf409d6`, `9781a75a`):
  - `RequestRange` takes the global critical region on **every** call and scans the valid bitmap
    under it. There is a "todo: optimize" note. Ours is ahead: a lock-free double-buffered fast path.
  - The same 64-page widening on CPU faults, stopped at GPU-written pages, with the same comment about
    4 KiB/64 KiB/256 KiB callbacks.
  - `MakeRangeValid` calls `EnablePhysicalMemoryAccessCallbacks` on every upload.
  - **No hot-page or "watch until first write" heuristic, no per-page counters, no
    `GetWriteWatch`/`MEM_WRITE_WATCH`, no dirty-bit scanning.**
- `src/xenia/gpu/d3d12/d3d12_shared_memory.cc` (latest `65d42843`, 2026-03 merge; `1e23aaf4`,
  `17bc5b48`): `UploadRanges` uses the same per-call transition, staging and `CopyBufferRegion`
  pattern, recorded into the submission's command list. There is no GPU upload heap, ReBAR or
  direct-write path. One difference: copies above 8 KiB use `memory::vastcpy` followed by
  `swcache::WriteFence()`, a streaming copy. Our uploads average 9 KB, right at that threshold.
- `src/xenia/memory.cc` `aed81ca9` "Rework guest access resolution and unwatched invalidation"
  (2026-08-01, from three Edge commits):
  - invalidation callbacks also fire on Decommit, Release and Protect-to-writable when **no** watch
    is armed;
  - a write fault on an unwatched page counts as handled only if the guest mapping allows the write;
  - system-page access is resolved as the most permissive guest access.
  Our `PhysicalHeap::Protect/Decommit/Release` (`xmemory.cpp:2195-2250`) still fire only when a
  watch is armed. That is a correctness gap: a page made valid while read-only, then made writable and
  written, is never invalidated. It is not a perf item. **Portable** (BSD-3, with attribution) as a
  separate correctness change.
- Vertex buffers: Canary re-requests every vertex range on every draw (local arrays, the TODO quoted
  in section 1). It has no cross-frame slot cache.

**What would port:** nothing that cuts this cost. Canary has the same mechanism and is slower on the
request side. Candidates are the unwatched-invalidation fix (correctness) and, optionally, the
streaming copy for uploads above 8 KiB (a micro-item; expect under 0.1 us per upload, because
write-combined stores already stream; measure with `up_memcpy_ms`).

## 4. Options

Savings use PC numbers at 6 k draws (about 1,300 uploads, 1,300 arm calls and 110 faults per frame;
CP 15-19 ms) and the Deck l81 numbers (5.7 k draws; CP 50 ms). Notation: A = arm cost, C = copy cost
without the arm (PC about 1.0 us), R/U = requests per upload on the pages concerned.

### (a) Adaptive "always-dirty" pages: stop protecting after N faults in M frames

**Mechanism.** Hot pages are never armed or marked valid. Every request touching them uploads.

**What was tested before.**
- M10 hot blocks v1: per 256 KiB block, K=6 of N=8 frames, inline copies.
  - l66 Deck A/B: arm calls -42 %, but upload calls went from 476 to 2,408 per frame (R/U about 5),
    each with its own barrier pair. GPU went from 27.1 to 37.5 ms per frame, and CP was flat at
    60.0 against 59.6 while driving.
  - PC: l66 hot-on ran only for "nothing broken". No PC numbers were analysed.
- Hot pages v2 (per page, per-submission prologue): not built. It was a NO-GO on the l81 census,
  projected at -1.2 to -2.2 ms against a -4 ms threshold, because the hot set is unstable.

**Why plain always-dirty loses on the PC as well.**
- Per hot cycle, today costs U x (C + A) on the CP plus one fault on the render thread.
- Always-dirty costs R x C.
- With A about C on Windows (1.2 against 1.0 us), it wins only if R/U < 2. The Deck measured about 5.
- So it is not worth building again without new data.

**New variant worth measuring: (a2) compare-before-copy for hot pages.**
- Mechanism:
  - keep a cacheable shadow of each hot page's last uploaded bytes;
  - on a request, compare the requested byte range of guest memory with the shadow;
  - if the bytes are equal, nothing happens: the host buffer already holds them in GPU order,
    because copies on one queue are ordered;
  - if they differ, copy the page (inline, as today, so the barrier count does not rise) and update
    the shadow.
- Why it differs from v1:
  - the copy count is bounded by real changes (at most today's U), not by R;
  - the cost term R x C becomes R x compare + U' x C, with U' <= U;
  - it removes the arm and the fault for hot pages.
- Entry and exit: per page, with hysteresis "uploaded in 6 of the last 8 frames". Pages are
  eligible only if they hold no GPU-written data, carry no texture or range watch (a per-page watch
  count), and have been requested only as VB/IB. A page exits after 32 consecutive equal compares,
  then is re-armed with its valid bit cleared.
- Correctness:
  - the compare reads guest memory at request time, which is the same sampling point as today's copy;
  - an equal compare proves that the GPU sees the right bytes;
  - GPU writes and texture watches exit the page before they take effect, as in v1.
- **VERIFY** (`shared_memory_hot_compare_verify`): arming stays on, so today's path still decides.
  The compare runs in shadow and counts:
  - `cmp_eq_valid` (agree);
  - `cmp_eq_invalid` (false invalidation, which is fine);
  - `cmp_diff_valid` (**must be 0**: the compare saw a change that protection missed);
  - `cmp_diff_invalid` (agree);
  - bytes compared and the projected arms and faults saved.
- Expected:
  - PC CP: -0.3 to +0.5 ms per frame. The sign depends on hot R/U and the requested bytes: a 9 KB
    compare from cache costs about 0.3-0.6 us.
  - PC render thread: -0.3 to -0.8 ms per frame.
  - Deck: CP -2 to -4 ms per frame (255 h8 arms x 16.7 us, minus compares at R/U unknown), render
    thread -2 to -3 ms per frame, plus Wine dispatch.
- Wine: removes `VirtualProtect` and SIGSEGV round trips, which are the expensive operations there.
  The compare is plain user-mode work.
- Risk: medium (eligibility rules, shadow memory of about 2-14 MB). Effort: medium (local to
  `shared_memory.cpp`, plus `xmemory` watch counts).

### (b) Write-watch (`GetWriteWatch` / `MEM_WRITE_WATCH`) instead of protection

**Not applicable, unchanged verdict.**
- On Windows it exists only for private `VirtualAlloc(MEM_RESERVE | MEM_WRITE_WATCH)` memory.
- Guest physical memory is one file-mapping section with four views (`xmemory.cpp` heap setup),
  which write-watch cannot cover. Making it private would remove the aliases.
- Under Wine, write-watch is implemented with page protection and an internal fault (no PE dispatch,
  but still SIGSEGV plus `mprotect` per first write and per reset). This is from general knowledge,
  not checked against Proton source.
- No new argument. Saving 0; rejected (M10 design (f), M11 Deck design (c)).

### (c) Coarser or finer watch granularity

- **Coarser arms (upload-ahead 16 or 64):** measured on the PC in l63.
  - Arm calls about halved, but per-call cost rose from 0.66 to 2.3-2.9 us. Upload went up
    1.5 ms per frame and render faults +60 %.
  - Net negative. It arms pages ahead of the guest's ring write head, which then fault.
  - It stays Wine-only, where it won (l81: CP -12 ms per frame).
- **Finer invalidation (`MCLA_INVALIDATE_WIDEN_PAGES` 16 instead of 64):**
  - fewer false invalidations, so fewer re-uploads of unchanged pages;
  - but more faults per ring lap: up to 4x on a sequentially written block, at about 11 us each on
    the PC and 35 us plus dispatch on the Deck.
  - Worth one measurement only if `up_same` shows that more than about 30 % of upload bytes are
    unchanged. Then try 16 on the PC (env variable, no build, if the launcher can pass it).
  - Expected -0.3 to +0.3 ms per frame. The Deck is expected to lose (M10 design).
- Below 4 KiB is impossible with host pages.

### (d) Skip the re-arm when a page is re-requested within the frame

**Nothing to skip.**
- A re-request of an uploaded page is already the lock-free fast path. No arm, no lock.
- Re-arms happen only after an invalidation.
- `EnableAccessCallbacks` already protects only pages that are not already watched
  (`xmemory.cpp:2344-2352`).
- "Upload now, arm later" loses writes between copy and arm. It was rejected in M10 design (d); the
  safe version (deferred coalesced arm plus memcmp verify) degenerates into (a2) for the pages
  concerned.
- Saving 0; rejected.

### (e) Batch uploads per submission (one transition) or write through a GPU upload heap

**(e1) Copy prologue for first-touch uploads.**
- Mechanism:
  - the CPU copy stays at request time;
  - the GPU copy goes into a per-submission prologue command list executed before the main list
    (`ExecuteCommandLists(2, {prologue, main})`);
  - one transition to COPY_DEST, merged copies, one transition back;
  - eligible only if no command recorded earlier in this submission read or GPU-wrote those pages
    (the per-submission bitmap of section 2.4);
  - otherwise the inline path, as today.
- Correctness: GPU order differs only for commands that never touch those pages; resolves and
  memexport stamp the bitmap. This is the M11 Deck design's (f) extension, applied to normal uploads.
- **VERIFY:**
  - shadow mode counts `pro_eligible`, `pro_bytes` and `pro_runs`;
  - verify mode records both the prologue and the inline copy. They carry the same bytes, so the
    result is identical;
  - it checks every later request in the submission against the prologue set: `pro_conflict` (a page
    in the prologue was read or written earlier in the submission) must be 0;
  - KI-047 composite and KI-033 vinyl checks.
- Expected:
  - PC CP -0.2 to -0.5 ms per frame (a barrier pair and two `SubmitBarriers` per eligible upload,
    plus replay-thread work);
  - PC GPU -0.3 to -1.0 ms per frame (fewer whole-buffer COPY_DEST drains; the l116 shared-memory
    upload category is 0.7-0.8 ms, and drains between scene draws come on top). At 6 k draws the
    GPU is only 1-2 ms behind the CP, so both sides matter;
  - Deck: CP -1 to -2 ms per frame, and fewer vkd3d render-pass splits.
- Eligibility is unknown until `up_first_sub` is measured: ring pages re-uploaded within a
  submission stay inline.
- Risk: medium-high (two lists per submission, streaming submissions, ordering). Effort: high.

**(e2) GPU upload heap (ReBAR; `MCLA_GPUCAPS gpu_upload_heap=1` on the RTX 5060).**
- Direct CPU writes into the 512 MiB shared buffer remove the copy and the barriers, but:
  1. the buffer is a reserved (tiled) resource in a default heap. Mapping its tiles from a GPU-upload
     heap has to be checked against the D3D12 rules and is likely not allowed;
  2. a CPU write lands immediately, while draws recorded earlier, and frames still in flight, may
    still read the old bytes. That needs per-page "last GPU read" fence tracking and a fallback.
- The safe sub-variant is staging pages from a GPU-upload heap: same CP work, faster VRAM-to-VRAM
  GPU copies, about -0.1 to -0.4 ms of GPU.
- The Deck (UMA) gains nothing.
- Low priority. It pairs with GPU-time item 9 (constants in a GPU-upload heap).

### (f) Upload-ahead revisited

- On the PC it is measured net negative (above).
- A new argument exists only for the GPU side: fewer copies mean fewer transitions. The l105
  `pc/upload-ahead` arm (no build) was staged but never reported.
- It can be run with gpu-cat for the GPU number. It is not a default while the dips are CP-bound
  (+1.5 ms CP).
- A direction-aware variant ("extend only below the request", i.e. behind the write head) would avoid
  the extra faults, but those pages are usually already valid. Expected near 0. Not proposed.

### (g) Two-tile: do not re-request ranges for the replayed tile

- Already effectively done: uploads do not scale with tiles, and the second tile's requests take
  the lock-free fast path (section 2.1).
- Skipping even that check would need proof that guest memory did not change between tiles; the
  bitmap check is that proof.
- At most about 0.05-0.1 us per replayed draw (sort, merge, `EnsureHostGpuMemoryAllocated`, bitmap
  scan): 0.15-0.3 ms per frame at 6 k draws.
- A cheaper fast path (skip the sort and vector for single-range requests, which are the VB and IB
  case) gets most of it for **all** draws, with no correctness argument: -0.1 to -0.3 ms per frame on
  the PC. Small, safe, a good add-on to the measurement build.

### (h) Micro-cuts (fold into the measurement build)

- TSC-only timing in `ProtectWatchRange` and `EnableAccessCallbacks`: four `steady_clock::now()`
  calls per arm removed, about 0.1 us per arm, 0.1-0.15 ms per frame on the PC. It also fixes the
  truncation.
- The single-range fast path from (g).

### Summary

| option | PC CP ms/f at 6 k | PC render ms/f | Deck CP ms/f | verify | risk / effort | status |
|---|---|---|---|---|---|---|
| (a) plain always-dirty (v1) | about 0 to +1 (loses at R/U above 2) | -0.5 to -1 | measured: no gain, GPU +10 | v1 model | medium / done | do not rebuild |
| **(a2) compare-before-copy, hot pages** | -0.3 to +0.5 | -0.3 to -0.8 | **-2 to -4** (render -2 to -3) | `cmp_diff_valid`=0 with arming on | medium / medium | measure R/U first |
| (b) write-watch | 0 | 0 | 0 | - | - | rejected |
| (c) widening 16 | -0.3 to +0.3 | + | lose | `up_same` | low / none (env) | only if `up_same` > 30 % |
| (d) skip re-arm | 0 | 0 | 0 | - | - | nothing to skip |
| **(e1) copy prologue** | -0.2 to -0.5 (GPU -0.3 to -1.0) | 0 | -1 to -2 | `pro_conflict`=0, double copies | med-high / high | measure `up_first_sub` first |
| (e2) GPU upload heap | about 0 (GPU -0.1 to -0.4) | 0 | n/a | - | high / medium | later |
| (f) upload-ahead | +1.5 (measured) | +0.6 | Wine default | - | - | keep Wine-only |
| (g)/(h) single-range fast path, TSC timing | -0.2 to -0.45 | 0 | -0.5 to -1 | exact (same result) | low / small | in the measurement build |

The ceiling on the PC is about 2.5-3.2 ms of CP per frame at 6 k draws (the whole upload region).
A realistic combined gain is 0.5-1.5 ms, i.e. 46 -> about 48-50 fps in a CP-bound dip second, before
the GPU (1-2 ms behind) becomes the limit. **On the PC this is a medium lever, not the "third of the
CP" the l117 summary suggests.** The Deck stands to gain more, in absolute and relative terms.

## 5. Recommendation

**Step 0, no build (owner, PC; and an archive read for the Deck).**
- PC on l117, otherwise defaults plus `memory_watch_census=true` (startup, dev override): 10 minutes
  of daylight driving including a race, single tile, then 5 minutes in a two-tile scene.
- From the `MCLA_WATCH v=1` lines, read:
  - `arm_ms` from TSC (true us per call);
  - `src_vb/ib/tex` requests, fast, uploads and arms;
  - `by_hot_requests`, `by_hot_fast`, `by_hot_uploads`, `by_hot_faults` and `by_hot_arms` per
    hotness class. This gives block-level R/U for h8 and h4-7: the deciding input for (a2);
  - `cpu_faults`.
- Census overhead was within noise on both platforms (l63, l81).
- The primary agent can extract the same `by_hot_*` fields from the l81 Deck census session
  (5968d12c) now, giving R/U for the Deck with no run.

**Step 1, measurement build (SDK, default-on counters, no behaviour change):**
- `MCLA_SHM v=1` (format in section 2.4);
- `MCLA_FAULT_PROBE v=1` at start-up;
- TSC-only protect timing;
- `vb_skip_invalid`;
- the single-range `RequestRanges` fast path, which is exact. Gate it on a cvar
  (`shared_memory_single_range_fast`, default on, off = old path) so its own A/B costs nothing.

Owner runs: PC 10 minutes daylight with a race (single tile) plus 5 minutes two-tile; Deck one usual
loop. Decision rules:
- if `vb_skip_invalid` > 0, fix the slot cache first (correctness);
- build **(a2)** if hot R/U <= about 3 and hot requested bytes per request are at most about 4 KB, or
  if the Deck projection from step 0 is at least -3 ms per frame;
- build **(e1)** if `up_first_sub` / `up` >= 0.5;
- try widening 16 (env) only if `up_same` / `up_sampled` > 0.3.

**Step 2, the first lever: (a2) compare-before-copy, then (e1).**
- (a2) is local to `shared_memory.cpp`. Its verify (arming kept on, `cmp_diff_valid` must be 0)
  proves safety in a run, and it attacks the two Wine-expensive operations, so it is the better first
  build for the Deck. Its PC effect may be neutral; ship it Wine-only if so.
- (e1) is the PC and GPU lever. It follows when `up_first_sub` justifies it, because it carries the
  larger ordering risk.
- Owner runs per lever:
  - PC: verify arm (10 minutes daylight with a race, KI-047 police livery, garage vinyl), then
    on/off A/B with gpu-cat at 1440p (two tiles) and single tile;
  - Deck: verify arm, then off/on/on/off with the l81 loop. The acceptance criteria are those of
    the M11 Deck design (CP >= -2 ms per frame for (a2), safety counters 0).

## 6. Open questions

1. Hot R/U and requested bytes per request on the PC and on the Deck: step 0 and step 1.
2. Does the cross-frame VB slot cache (`command_processor.cpp:6275-6278`) ever skip rewritten data
   (`vb_skip_invalid`)? If it does, it is a latent staleness bug, and it also hides requests from
   these counts.
3. Windows and Wine exception dispatch per fault (`MCLA_FAULT_PROBE`).
4. Whether a reserved buffer can map tiles from a GPU-upload heap (D3D12 rules), relevant to (e2).
5. How much of the GPU per-draw slope is COPY_DEST drains (GPU-time research: "likely main term,
   unproven"). The (e1) shadow numbers plus a gpu-cat run with the existing `pc/upload-ahead` arm
   would bound it.
6. Port of Canary `aed81ca9` (unwatched invalidation on protect-to-writable, decommit and release)
   as a separate correctness change.

## 7. Limits

- PC numbers come from two sessions. Day and night come from `MCLA_TOD night=` and tiles from the last
  `MCLA_GTILE`. The `watch stats/min` window is about one second offset from the perf-split window.
- The 194fcd9a 30 fps windows at 17:36-17:42 have identical values minute to minute (a static scene)
  and are shown separately.
- The true arm cost on the PC (about 1.2-1.4 us) is inferred from the l63 TSC census, not from
  these sessions.
- Wine and Proton internals are from general knowledge and the earlier designs, not checked against
  Proton source.
- All savings are estimates; nothing here is a physical PASS.
- Scratch: `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\shm-study-work` (`shm.py`, `hot.py`,
  `c46.txt`, `194.txt`).
