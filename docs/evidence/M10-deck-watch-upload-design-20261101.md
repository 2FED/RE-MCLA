# M10 Deck: shared-memory watch / upload path under Proton (design, read-only, 2026-11-01)

Read-only design by an Opus subagent on 2026-11-01 (SDK dev branch mcla/m10-prewarm, the l56 Deck log, M7-016 Proton micro-benchmarks); accepted by the primary agent. Gains are ranges until the step-0 census measures them; the Wine internals are described from general knowledge, not checked against Proton source.

Read-only design by an Opus subagent. Sources: `docs/evidence/M10-001-deck-l56-20261101.md`,
`docs/evidence/M10-cpu-targets-20261101.md` (section 2.4, task 5),
`docs/evidence/M10-001-720p-and-ki047-20261101.md` (PC l53 720p), the earlier Deck protection
evidence `docs/evidence/M7-016-disarm-fragmentation.md` and
`docs/evidence/M7-016-protection-api-comparison.md`, the l56 Deck session log (session
`7fa98843...`, `mcla.log` only), and the SDK at branch `mcla/m10-prewarm` (`<sdk>` below).
Nothing built, launched or committed. Spark not used (exact source reading and arithmetic on a
private log).

## 0. Numbers this design works from

Deck l56, driving minute seq 9 (762 frames, 60.06 s; the segment averages in the l56 report agree):

| Quantity | Per minute | Per frame | Note |
|---|---|---|---|
| `RequestRange(s)` calls (CP) | 7,202,929 | 9,453 | about 89% hit the all-valid fast path |
| Upload calls / bytes | 802,200 / 5.93 GB | 1,053 / 7.97 MB | average upload 7.6 KB (1.9 pages) |
| `MakeRangeValid` calls (CPU / GPU) | 816,453 / 42,683 | 1,071 / 56 | GPU = resolve/memexport targets, 11.5 k pages/frame |
| Arm protect calls / time | 816,417 / 10.55 s | 1,071 / 13.8 ms | **12.9 us/call** (PC 0.63) |
| Disarm protect calls / time | 340,576 / 4.27 s | 447 / 5.6 ms | 12.5 us/call; **4.1 disarm calls per fault** |
| Write faults (= CPU invalidations) | 82,215 | 108 | 63.3 pages (about 256 KB) invalidated each |
| Fault handler time (all threads) | 6.21 s | 8.15 ms | **75 us per fault** (PC 7.8 us), excludes Wine's signal/exception dispatch |
| Arm calls per fault | | about 9.9 | one fault, then about 10 piecewise re-arms |
| Hottest 256 KB blocks | `1B100000` 51 k, `16700000` 48 k, `1B140000` 46 k valid calls | 60-67 each | the same block re-validated 60+ times per frame |
| Pages validated >= 600 times/min | 582 pages | | 34% of all CPU page-validations |

Accounting correction: the l56 summary lists "arm 14.7 / disarm 5.9 / write fault 8.7 ms". These
overlap. Arm is inside `upload` (CP, `UploadRanges` calls `MakeRangeValid`), and disarm is inside
the write-fault handler (`TriggerCallbacks` is called only from the fault path). Distinct thread time
is **upload 23.4 ms (CP; 14.7 of it protect syscalls) + faults 8.7 ms (guest threads; 5.9 of it
protect syscalls) = about 32 ms/frame**, of which **about 20.6 ms is `VirtualProtect`**, all of it
**with the global critical region held**.

PC l53 720p: arm 1,206 calls/frame 0.65-0.76 ms, disarm 0.20 ms, faults 116/frame 0.79-0.94 ms
(7.8 us each), upload 2.2 us/call.

## 1. Exact call pattern

### 1.1 Request and upload (CP thread)

1. Callers: vertex fetch constants, one `RequestRange` per fetch constant whose address/size
   changed since the last draw (`<sdk>/src/graphics/d3d12/command_processor.cpp:3476-3490`); index
   buffers (`<sdk>/src/graphics/primitive_processor.cpp:929`); textures, batched
   (`<sdk>/src/graphics/pipeline/texture/cache.cpp:578`, `:979`); resolve destinations
   (`<sdk>/src/graphics/d3d12/render_target_cache.cpp:1362`); memexport
   (`command_processor.cpp:3505`).
2. `SharedMemory::RequestRanges` (`<sdk>/src/graphics/shared_memory.cpp:523-707`): sort and merge the
   ranges; lock-free all-valid check on the active valid bitmap (`:581-618`, the common case). If not
   all valid, take the **global lock**, build page runs of invalid pages (`:634-685`); runs are
   page-granular (4 KiB) and contain only invalid requested pages, nothing around them.
3. `D3D12SharedMemory::UploadRanges` (`<sdk>/src/graphics/d3d12/shared_memory.cpp:368-405`): per
   run, per upload-pool chunk: `MakeRangeValid` **first** (`:392`), then `memcpy` from the physical
   view (`:393-395`), then `CopyBufferRegion`. Protect-before-copy is what makes it correct: a guest
   write after the protect faults; a write before it is captured by the copy.
4. `SharedMemory::MakeRangeValid` (`shared_memory.cpp:455-499`): set valid bits under the global
   lock, then `Memory::EnablePhysicalMemoryAccessCallbacks` (`<sdk>/src/system/xmemory.cpp:682-691`),
   which calls `PhysicalHeap::EnableAccessCallbacks` for **each of the three physical aliases**
   `A0000000` (64 KiB guest pages), `C0000000` (16 MiB pages, only physical 0-16 MiB allocated, see
   `xmemory.cpp:206-228`) and `E0000000` (4 KiB pages).
5. `PhysicalHeap::EnableAccessCallbacks` (`xmemory.cpp:1862-1984`): re-acquires the **global lock**
   (`:1902`, its wait is `watch_arm_lock_wait_us`), walks the run **one 4 KiB host page at a time**,
   and for pages that are guest-RW and not yet watched sets `notify_on_invalidation` and extends a
   protect run; a page already watched or not guest-writable **ends the run** (`:1958-1983`). Each run
   is one `Memory::ProtectWatchRange` (`:519-533`) = one `rex::memory::Protect` =
   **one `VirtualProtect(PAGE_READONLY)`** (`<sdk>/src/core/memory_win.cpp:136-155`), done **while the
   global lock is held**. Aliases where the pages are not allocated cost a page walk but no call.

So the arm granularity is the 4 KiB host page, the unit is the requested invalid run, and the count
is about **one `VirtualProtect` per upload chunk** (Deck: 1,071 arm calls for 1,053 uploads per frame;
arm-side gap splits were 77 per 20 s in the M7 Deck control, so the arm side is already one call per
run). Coalescing *inside* a call has nothing left to win on the arm side; the arm count is the number
of separate small uploads.

### 1.2 Write fault and disarm (guest threads)

1. Guest store to a read-only page in an `A0/C0/E0` view -> host access violation -> vectored handler
   (`<sdk>/src/core/exception_handler_win.cpp:106`) -> `MMIOHandler::ExceptionCallback`
   (`<sdk>/src/system/mmio_handler.cpp:384-490`): takes the **global lock**, `QueryProtect` =
   **`VirtualQuery`** on the page (race check "already cleared"), then the access-violation callback.
2. `Memory::AccessViolationCallback` (`xmemory.cpp:535-635`) -> `PhysicalHeap::TriggerCallbacks`
   (`:1986-2181`) with `unwatch_exact_range=false`:
   - invokes `SharedMemory::MemoryInvalidationCallback` (`shared_memory.cpp:720-793`): **widens to the
     whole 256 KiB block** (64 pages) except GPU-written pages (`:735-762`; env
     `MCLA_INVALIDATE_WIDEN_PAGES`, default 64, `:127-138`), clears valid bits, fires texture/range
     watches (`FireWatches`, `:398-427`; 4 MiB buckets) and the scaled-resolve global watch;
   - disarm loop (`xmemory.cpp:2093-2166`): over the widened window, each run of **watched** pages
     becomes one `VirtualProtect(PAGE_READWRITE)`; any unwatched page splits the run. The window still
     has many unwatched pages (invalidated earlier but not yet re-requested), so a fault costs **4.1
     disarm calls on average**; the M7 Deck control measured **78% of disarm calls as safe-gap splits**
     (both neighbours watched, the gap committed guest-RW and unwatched, i.e. already host-RW);
   - clears the notify bits for the whole window (`:2168-2178`). Only the faulting alias is
     disarmed.
3. Return `EXCEPTION_CONTINUE_EXECUTION`.

### 1.3 Why the faults happen (which guest data)

Per frame: 108 faults, each invalidating a 256 KiB block, followed by about 10 small uploads and
10 re-arms into the same block (arm calls per fault 9.9), while the same three blocks are re-validated
60+ times per frame. 89% of requests hit valid memory. That is the **dynamic vertex/index ring
pattern**: the guest CPU writes per-draw vertex data (and per-draw index/constant-like buffers fetched as
vertex data) into a ring that it rewrites every frame or every few frames; each draw's fetch constant
points at a new small sub-range, so the CP requests, uploads and **arms 1-2 pages per draw**; the next
time the guest laps the ring it hits one of those read-only pages, the fault invalidates the whole
256 KiB block and disarms what is watched, and the cycle repeats. Shader constants are not in this
path (they are register writes, not shared memory). Textures fault rarely (the vinyl atlas is one
known CPU-built texture). Block attribution by request source and by faulting thread is not measured
yet (step 0).

## 2. Why Proton is 6-20x slower here

The same `VirtualProtect` costs **12.9 us in the game on the Deck, but 2.7 us (1 page) to 3.9 us
(64 pages) in the guest-free Proton micro-benchmark on the same Deck** (M7-016 protection-API
comparison; native Linux `mprotect` 1.25 us). So the Wine API path itself is only about 2x a raw
`mprotect`; most of the 12.9 us comes from the game's environment. Contributors, in the order to test:

1. **Wine's `NtProtectVirtualMemory`**: kernel32 -> ntdll PE side -> Wine syscall dispatcher (saves and
   restores the full XSAVE state) -> unix side: block signals (`pthread_sigmask`), take the
   process-wide `virtual_mutex`, find the view, check/update its per-page protection bytes, then
   `mprotect` per run of identical host protection, release, unblock. At least three host syscalls
   per call, and one process-wide mutex shared with every other virtual-memory call, including
   `NtQueryVirtualMemory` in our own fault handler and Wine's own fault pre-check.
2. **Linux `mprotect` on a big multiply-mapped shared section**: guest physical memory is one section
   mapped in four views (physical, `A0`, `C0`, `E0`). Every 4 KiB protect splits/merges VMAs of a
   shared file mapping (maple tree plus the file's `i_mmap` interval tree under its write lock), and
   with about 33 k distinct watched pages per minute the process carries many thousands of VMAs. It
   takes `mmap_lock` for write, so it waits for page faults of the other 4-8 busy threads.
3. **TLB shoot-down**: making a page read-only in a multi-threaded process running on several CPUs
   sends IPIs; the single-threaded micro-benchmark had none.
4. **Preemption**: the CP is runnable but not running about 9% of wall (CPU oversubscribed, 4.2-4.5
   busy logical CPUs on 4 cores); the steady-clock timer around the call includes that.
5. **Faults** (75 us inside our handler, plus unmeasured dispatch): hardware fault -> kernel SIGSEGV
   delivery on the signal stack -> Wine `segv_handler` (Wine first checks its own write-watch/guard
   state under `virtual_mutex`) -> builds the Windows CONTEXT and EXCEPTION_RECORD, switches to the
   PE side -> `KiUserExceptionDispatcher` -> `RtlDispatchException` -> our VEH: global lock (contended
   with the CP, which holds it for 12.9 us per arm), `VirtualQuery` (another Wine syscall under
   `virtual_mutex`), callbacks, about 4 `VirtualProtect` -> `NtContinue` (syscall, full context restore).
   On Windows the same fault is a kernel trap plus a user-mode dispatch with no signal layer.

Lock coupling makes it worse than the sum: the global critical region is the same static
`std::recursive_mutex` (`<sdk>/include/rex/thread/mutex.h:56-83`) used by every guest atomic
(`mfmsr/mtmsrd` regions, AddRef/Release on the render thread; see M10-cpu-targets 2.3). On the Deck
the CP and the fault handlers hold it for about **20 ms per 81 ms frame inside protect syscalls**; this
fits the render thread being off-CPU 10-22% of wall with `RtlEnterCriticalSection` among its last
imports.

## 3. Options, ranked by expected Deck gain against risk

Gains are thread time per driving frame on the Deck (81 ms frame, CP the wall) and on the PC
(720p, about 16.5 ms). Only CP time moves Deck fps directly; fault time is on guest threads (likely
mostly the render thread, the close second wall) and shortens global-lock hold.

| Rank | Option | Deck gain (ms/frame) | PC gain | Correctness risk | Effort |
|---|---|---|---|---|---|
| 1 | (b)+(e) hot buffer blocks unprotected, exact uploads, CP-source gated | CP arm -5 to -10; faults -4 to -7 (guest threads) | -0.5 to -1.2 (mostly render) | medium | medium |
| 2 | (c) upload-ahead: re-upload and arm the whole invalid run of the block at once | CP arm -4 to -8 (fewer, larger calls); memcpy +0.5 to +2 | about 0 (+-0.3) | low | small |
| 3 | (a) disarm safe-gap bridging (coalesce disarm runs) | faults -3.5 to -4.5 (78% of 447 calls) | -0.1 to -0.15 | low | small |
| 4 | (g) protect syscalls outside the global lock | lock hold -15 to -20; render-thread wait unknown, CP lock wait -0.6 | small | medium | medium |
| 5 | (f) Proton write-watch / native soft-dirty | potentially all of the 32 ms | n/a | high | large; not usable now |
| 6 | (d) defer arms and batch per frame | in theory most of the arm time | small | high (staleness) | large; rejected |

(b) and (c) overlap: hot blocks are removed by (b); (c) helps the remaining warm blocks. Together the
realistic expectation is **CP -8 to -12 ms/frame and guest-thread fault time -5 to -8 ms/frame on the
Deck**, about +10-15% fps if the CP stays the wall (it may hand the wall to the render thread). The
KI-047 targeted read-back wait (10.6 ms/frame) is independent and of similar size.

### (a) Coalesce adjacent ranges into one protect call

- Arm side: already one call per run; arm gap splits are negligible (M7: 77 per 20 s). No gain from
  coalescing within a call. Across calls it needs batching, see (c) and (d).
- Disarm side: bridge a gap of unwatched, committed, guest-RW pages between two watched runs in the
  same callback window and same alias, issuing one `PAGE_READWRITE` call over the whole span. The gap
  pages are already host-RW, so re-asserting RW is a no-op. The classification already exists
  (`xmemory.cpp:2108-2165`, `watch_disarm_safe_gap_splits`); it reads page metadata only under the
  non-blocking heap-mutex try-lock, and must fall back to the legacy split on a miss
  (`watch_disarm_gap_lock_misses`). Arm and disarm both run under the global lock, so no arm can
  interleave.
- Expected: Deck disarm calls 447 -> about 100-130 per frame, -3.5 to -4.5 ms of fault-handler time
  (and as much global-lock hold); PC -0.1 to -0.15 ms. Larger calls cost little more (micro-benchmark:
  64 pages 3.9 us vs 1 page 2.7 us).
- Risks: none for texture/KI-047/KI-033 (no watch bit or valid bit changes, only fewer syscalls); a
  wrong gap classification would make a guest read-only page writable, which the committed-RW check
  excludes. The M7 decision "do not coalesce merely because the ratio is high" was made when disarm
  was 5.6% of a paused interval; it is now inside a CP-walled frame, under the global lock.
- Test: `watch_disarm_calls` down by about the gap-split fraction, `watch_protect_failures=0`, no
  "Recovered stale physical page protection" warnings, gap-lock misses normal; KI-047 trap laps.

### (b) Hot blocks: do not re-arm, treat as dynamic (task 5)

- Policy per 256 KiB block (the existing widening unit): a block is hot when it was invalidated by a
  CPU fault in K of the last N frames (start N=8, K=6), has **no texture/range watches**, **no
  GPU-written pages**, and its requests come only from vertex-fetch/index sources (see (e)). In hot
  mode `MakeRangeValid` neither arms nor marks valid; `RequestRanges` therefore uploads the exact
  requested pages on every request. Correct by construction: the CP copies guest memory when it
  processes the draw, which is when the real GPU would read it.
- Exits (re-arm with valid bits cleared, normal path): a texture or range watch is registered in the
  block (`WatchMemoryRange`, needs a per-block watch count since buckets are 4 MiB); any GPU write
  into it (`RangeWrittenByGpu`, resolves, memexport); M frames without a request; or the per-block
  upload bytes per submission over a cap (repeated requests of an unchanged range).
- Expected (needs step 0 to firm up): hot pages already carry 34% of page-validations and the top three
  blocks alone 13-18% of valid calls. If 40-70% of arms and 60-90% of faults are in hot blocks:
  Deck CP arm -5 to -10 ms, faults -4 to -7 ms (on guest threads, plus their lock hold); extra memcpy
  for requests that today hit the valid fast path, likely under 1 ms. PC: -0.5 to -1.2 ms render
  (the task 5 estimate) plus -0.3 to -0.6 CP arm.
- Risks: texture staleness if a texture lives in a hot block (excluded by the watch count and the
  exit on registration); **KI-047**: the composite/scratch resolve ranges (e.g. `050A0000`) are
  GPU-written, so excluded, and any GPU write exits hot mode; the KI-047 diag v2 counters
  (`composite_read_prev`, `sporadic_read_prev_unfenced`, `composite_not_copied`) must stay 0;
  **KI-033 vinyl**: the atlas is CPU-composited and texture-watched, so excluded; verify garage and
  driving vinyl. Scaled-resolve global watch: hot blocks contain no GPU-written pages, and a GPU
  write exits hot mode before any scaled data could be stale. Upload blow-up: cap per submission.
  Hidden dependence on the fault as an ordering side effect: none known (faults only invalidate).
- Test: shadow mode first (classify and count, no behaviour change), then on/off A/B.

### (c) Larger watch/upload granularity: upload-ahead within the block

- In `RequestRanges`, when a requested run is invalid, extend it to the contiguous invalid
  (non-GPU-written, i.e. not valid) pages around it inside the same 256 KiB block, bounded by a cvar
  (16 or 64 pages). One upload and **one arm** then cover what today takes about 10 requests, 10
  uploads and 10 arms after each fault. Faults are unchanged.
- Correctness: protect-before-copy is preserved, so extra pages are uploaded with fresh data and
  watched; nothing becomes valid without being protected first. Unallocated pages are skipped by the
  arm loop as today. Low risk for textures, KI-047 and KI-033 (GPU-written pages are valid and never
  extended into).
- Cost: more bytes (worst case all 6,800 invalidated pages per frame = 27 MB/frame, more than 3x
  today) unless bounded; upload-pool growth; if the guest is still writing ahead inside the block,
  pre-uploaded pages fault again sooner (that block is a (b) candidate).
- Expected: Deck arm calls -40 to -70% on non-hot blocks, CP -4 to -8 ms, memcpy +0.5 to +2 ms;
  PC near zero (arm is 0.63 us there, memcpy bytes go up). Make the bound a cvar and test 16 and 64.

### (d) Defer arms and batch per frame

The arm must precede the copy and the copy must be recorded before the draw that uses it. Deferring
the arm without deferring the copy loses writes made between copy and arm (stale GPU data, KI-047-like
symptoms); deferring the copy changes the time the data is sampled and needs reordering of recorded
copies before already-recorded draws. Batching within one `RequestRanges` call is already done.
**Rejected**; (c) gets most of the batching benefit safely.

### (e) Use the CP's knowledge of who writes what

The CP knows the request source: vertex fetch and index requests are buffer data; texture requests,
resolve destinations and memexport are not. Use it as the eligibility gate for (b) (only VB/IB-sourced
blocks can go hot) and for the step-0 attribution (arms, uploads and faults by source). It does not
save anything by itself, but it lowers (b)'s texture risk. An address-keyed "dynamic VB ring" detector
is possible but the 256 KiB block granularity is simpler and matches the invalidation unit.

### (f) Proton-specific

- `GetWriteWatch` / `MEM_WRITE_WATCH` is **not usable**: it applies only to private `VirtualAlloc`
  memory, not to views of a section, and guest physical memory must be a section mapped four times
  for the aliases. Under Wine, write watches are themselves implemented with page protection and the
  SIGSEGV path (cheaper than ours because they are resolved inside ntdll without the PE exception
  dispatch); a userfaultfd-based write-watch path in Proton is plausible on the Deck's 6.16 kernel
  but unverified here and would still not apply to section views.
- A native Linux build could use soft-dirty or `PAGEMAP_SCAN` with userfaultfd write-protect on the
  shared mapping to poll dirtiness at request time with no faults and no arms; a large, later item.
- Cheap Proton-relevant micro-cuts: replace the fault handler's `VirtualQuery` with the SDK's own
  notify bits under the global lock (saves one Wine syscall under `virtual_mutex` per fault, about
  0.3-0.6 ms/frame on the Deck, needs care for genuine read-only guest pages); keep VMA count low
  (fewer, larger protect runs from (a)/(c) also reduce VMA fragmentation).

### (g) Protect calls outside the global lock

Set the bits under the lock, release it, then call `VirtualProtect`, and copy after the protect. A
guest write before the protect is captured by the copy. A concurrent fault in the same window can
disarm and clear the notify bit before the CP's late read-only protect lands, leaving a read-only
page with no notify bit; the next write then takes the existing recovery path (`xmemory.cpp:564-618`:
restore RW and log a warning), and its valid bit was already cleared by that fault, so no stale data,
but a warning per occurrence. Gain: about 15-20 ms/frame less global-lock hold on the Deck, which
helps the render thread's atomics; CP lock wait -0.6 ms. Only if step 0 shows significant render-thread
waits on the global lock after (a)-(c).

## 4. Step plan (counters first; each step one owner A/B: Deck title 60 s + one driving loop, PC 720p BENCH-B)

All new behaviour behind SDK cvars defaulting to off in the A/B build, with host `mcla_*` mirrors;
every step keeps `watch_protect_failures=0`, no stale-protection warnings, and the KI-047 diag v2
composite counters at 0.

**Step 0, counters only (no behaviour change)** - SDK cvar `memory_watch_census` (default on in the
measurement build), per-minute `MCLA watch census` line plus F10 state:
- protect calls and us by kind (arm/disarm) and by pages-per-call bucket (1, 2-4, 5-16, 17-64, >64);
  global-lock hold time inside protect calls;
- arms, uploads, upload bytes and requests by source (VB, IB, texture, resolve dest, memexport,
  RangeWrittenByGpu);
- faults and handler us by faulting thread class (render tid, CP, other), plus `VirtualQuery` us;
- per 256 KiB block hotness over N=8 frames (faulted in 8/8, 4-7, 1-3, 0 frames): share of arms,
  uploads, faults, requests and fast-path hits; blocks with texture watches / GPU-written pages;
- shadow projections: (b) arms and faults avoidable and extra upload bytes for eligible hot blocks;
  (c) arms avoidable and extra bytes at 16 and 64 pages; (a) the existing
  `watch_disarm_safe_gap_splits` and gap-lock misses in the per-minute line (today only in F10 state);
- one-shot F10 probes: 1,000 RO/RW protect round trips on a private scratch page and on a scratch
  page inside the guest section view; one deliberate write fault on an SDK-owned scratch page timed
  end-to-end (rdtsc around the store, includes Wine dispatch); on the Deck the line count of
  `/proc/self/maps` (VMA count).
- Owner A/B: l56-equivalent build vs census build on the Deck title + driving loop (overhead within
  noise), and PC 720p. Output: firm per-option projections, choose (b) thresholds.

**Step 1, (a) disarm gap bridging** - cvar `memory_watch_disarm_bridge_safe_gaps`. Expect Deck
disarm calls -70 to -78%, fault time -3.5 to -4.5 ms/frame; PC -0.1 ms. A/B on/off, same routes.

**Step 2, (c) upload-ahead** - cvar `shared_memory_upload_ahead_pages` = 0 / 16 / 64. Expect Deck
arm calls -40 to -70%, CP -4 to -8 ms; watch upload MB/s, upload-pool errors, PC 720p neutral. A/B
0 vs the better of 16/64 (pick from step 0's projection, so one A/B).

**Step 3, (b)+(e) hot blocks** - cvars `shared_memory_hot_blocks` = 0 off / 1 shadow / 2 on,
`shared_memory_hot_k`, `shared_memory_hot_n`, `shared_memory_hot_upload_cap_kb`; VB/IB sources only.
Expect Deck CP -5 to -10 ms and fault time -4 to -7 ms; PC -0.5 to -1.2 ms. A/B 0 vs 2 plus KI-047
trap laps (police car, 720p and 1440p on the PC), garage and driving vinyl (KI-033), traffic and HUD
visual check, Deck F10 frames.

**Step 4 (conditional), (g) protect outside the global lock** - cvar
`memory_watch_protect_unlocked`; only if step 0/3 counters show render-thread global-lock waits worth
more than about 2 ms/frame on the Deck. A/B on/off; count recovery-path warnings.

**Not planned**: (d) rejected; (f) recorded as a native-Linux research item; the free env knob
`MCLA_INVALIDATE_WIDEN_PAGES` (16 instead of 64) could be tried with no build, but on the Deck a fault
(75+ us) costs far more than an arm (12.9 us), so narrower widening is expected to lose.

## 5. Limits

- Fault split by thread and by source, arm share in hot blocks, and Wine dispatch cost are not
  measured yet; gains for (b) and (c) are ranges until step 0.
- The 78% safe-gap figure is from an M7 Deck paused interval on Vulkan, not from l56 driving.
- The causes of 12.9 us vs 2.7 us per protect (VMA count, IPIs, `mmap_lock`/`virtual_mutex`
  contention, preemption) are hypotheses; step 0's probes separate them.
- Wine/Proton internals are described from general knowledge of Wine's ntdll design, not from the
  Proton source; the userfaultfd write-watch point is unverified.
