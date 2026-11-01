# Review: SDK e57fd88 (mcla/m10-watch, parent 561a737): M10 watch census, step 1 bridging, step 2 upload-ahead

Scope: read-only review of `git show e57fd88` in `private/l62-sdk-wt`, plus the surrounding source
(xmemory.cpp, shared_memory.cpp, d3d12/vulkan shared_memory.cpp, mmio_handler.cpp,
exception_handler_win.cpp, d3d12 command_processor.cpp EndSubmission). No build, test run or launch.
Line numbers are in the e57fd88 tree.

## Verdict

**No blockers. The defaults are safe for tonight's owner-internal build.** These defaults are
census ON, probes OFF, bridging OFF and upload-ahead 0. With them, the protect, disarm, arm, fault
and upload control flow matches 561a737. The legacy `TriggerCallbacks` walk is unchanged apart from
a shadow `Feed` and some counters. Arm and disarm make the same protect calls, and the fault path
returns the same results. The VEH path does not allocate, log or throw.

The defaults still differ from 561a737 in one way that is not just counting: the process-wide
global critical region is now taken more often and held longer.
- It is taken once per frame in `WatchCensusFrameEnd`.
- It is taken once per minute or F10 mark in `WatchCensusReport`.
- `MakeRangeValid` and the invalidation callback do extra shadow bitmap work while holding it.

This costs some performance but cannot cause a correctness problem or a deadlock. The census also
measures the lock hold it adds to, so the C0 versus C1 arms are the right way to price that cost.

## Should-fix (not blocking)

**S1. Report formatting and the per-frame walk happen under the global critical region.**
- `shared_memory.cpp:1006-1019`: `WatchCensusFrameEnd` holds the lock every frame while it walks
  2,048 blocks. It runs on the CP thread, inside `is_closing_frame`.
- `shared_memory.cpp:1092-1160`: `WatchCensusReport` walks 2,048 blocks and their watch-bucket
  lists. It then builds roughly 2 KB of text with `fmt::format_to`, which allocates, while still
  holding the lock.
- That lock is the process-wide recursive mutex from `rex/thread/mutex.h:58`. VEH write faults wait
  on it, and so do the guest kernel paths.
- Failure scenario: on the Deck (CPU-bound) at each minute boundary or F10 mark, every guest thread
  that faults or makes a kernel call stalls while the report is built. The stall is probably tens to
  low hundreds of microseconds, so this is a hitch rather than a hang.
- No deadlock: `EndSubmission` holds no other mutex at these points (checked lines 4857-5110), and
  the lock is recursive.
- Fix: copy the 2,048 history bytes and the lock-side counters under the lock, then format after
  releasing it. Leave the per-frame shift as it is (about 2,048 byte operations, a few µs), or do it
  lock-free with a per-frame double buffer for `faulted`.

**S2. Shadow work runs on by default under the global lock in `MakeRangeValid`
(`shared_memory.cpp:497-500`, `561-583`).**
- `ShadowUploadAheadLocked` runs on every CP upload chunk when upload-ahead is 0. It calls
  `AllPagesSet`, `ExtendUploadRun` and `SetPages` once for each of the two caps.
- `SetPages` counts new bits one at a time (`watch_census.h:446`). For a 4 MB upload chunk that is
  about 1,024 iterations per cap, all inside the global lock.
- `MemoryInvalidationCallback:970-974` clears both shadows on every invalidation, regardless of the
  census.
- Failure scenario: the lock hold grows on exactly the paths the census is trying to measure. This
  is an observer effect: the census-on arms then overstate `arm_hold` and the fault lock wait.
- Fix: use `std::popcount(mask & ~bits[word])` in `SetPages`. Also consider computing the shadow
  only when `census` is on (the invalidation clear does not check this today).

**S3. The memexport `ScopedRequestSource` covers the rest of `IssueDraw`
(`d3d12/command_processor.cpp:3611`).**
- It is declared at function scope and constructed even when `memexport_ranges_` is empty.
- Every unscoped `RequestRange`/`RequestRanges` call later in the same `IssueDraw` is therefore
  counted as `src_memexport`.
- This affects census attribution only, not behaviour. Fix: put it in a `{}` around the loop, or
  only when `!memexport_ranges_.empty()`.

**S4. `SetSystemPageBlocksValidWithGpuDataWritten` now takes the global lock
(`shared_memory.cpp:298-302`).**
- The function used to be lock-free (an atomic flag swap).
- It now locks and clears 32 KiB of shadow, whatever the census setting.
- It is only reached with `clear_memory_page_state=true` (default false) or from `ClearCache`, on
  the CP thread with no other lock held, so there is no deadlock risk.
- It is still a change in the default code path. Fix: clear the shadows lazily, for example with a
  generation counter, or gate the clear on `census_enabled_`.

## Notes

**N1. Benign data races.** `census_thread_id_` and `request_source_` are plain fields
(`shared_memory.h:121-127`). `CensusCounting()` reads `census_thread_id_` from any thread that
calls `MakeRangeValid`/`RequestRanges`, while the CP thread writes it every frame (always the same
value). On x64/MSVC this is a benign race. Making both `std::atomic<uint32_t>` with relaxed ordering
would remove the UB.

The CP-only counter arrays have no race:
- They are only incremented when `CensusCounting()` is true, which means on the CP thread.
- `history` is written only by the CP thread, under the lock. Other threads read it only under the
  lock (in `MemoryInvalidationCallback`).
- `faulted`, `cpu_faults`, `class_faults` and the shadows are only touched under the lock.

**N2. VEH and fault path (`mmio_handler.cpp:443-450`, `xmemory.cpp:521-543`).** The additions are
`REXCVAR_GET`, relaxed `fetch_add` calls, `GetCurrentThreadId`, an atomic load of the render thread
ID, and `__rdtsc`. None of them allocate, lock, log or throw. The cvar storage is a function-local
static of trivial type, so it is constant-initialized with no guard.

**N3. Step 1 (bridging on): no case found where it makes a page read-write that must stay
protected.**
- A safe gap needs all of these, read under the alias heap's `heap_mutex_` try-lock: unwatched in
  this heap's `notify_on_invalidation`, guest read-write and committed.
- `BaseHeap::Protect`/`Alloc`/`Decommit` change `page_table_` and the host protection together under
  `heap_mutex_` (`xmemory.cpp:1509`). Classification is therefore consistent with the host state.
- Arms run under the global lock, which the disarm caller holds for the whole walk.
- Notify flags are cleared only after the walk (`xmemory.cpp:2302-2313`).
- Protection is per view, so watches in other aliases (A0, C0, E0) do not matter. Each heap's window
  lies inside a single mapped view, so no protect call spans two views.
- Leading and trailing gap pages are never included. A try-lock miss turns every unwatched page into
  a barrier, which is the legacy split.
- The E0 `host_address_offset` mapping uses the same `sat_sub` as the legacy code.
- Decommit and Release call `TriggerCallbacks` before `BaseHeap::Decommit` runs. The host is still
  read-write at that point, so bridging changes nothing.
- When bridging is on, `watch_disarm_safe_gap_splits_` stays at zero. Compare arms with the census
  fields instead.

**N4. Step 2 (upload-ahead on): sound.**
- `blocked` = valid | GPU-written | ~(committed and read-write in `heaps_.physical`).
- Invalidation widening stops at GPU-written neighbours (`shared_memory.cpp:934-945`). An invalid
  page therefore always has guest memory as its authority, and an extra upload only replaces stale
  buffer bytes.
- Resolve destinations and memexport ranges are requested (so already valid) before the GPU write,
  and are marked GPU-written after it, all in sequence on the CP thread.
- KI-047 read-back pages stay valid and GPU-written, so they are blocked.
- Protect-before-copy holds for each chunk, extended pages included: `MakeRangeValid` runs before
  the `memcpy` (d3d12 `shared_memory.cpp:392-395`, vulkan `:377`).
- Upload ranges are sorted (`merged_ranges` is sorted and merged). `min_page` and `max_page` use the
  neighbour that was already extended, so ranges never overlap, and touching runs are coalesced.
- I found no path where an extra upload overwrites GPU data newer than guest memory.

**N5. Step 2 sparse-unit safety depends on an invariant nothing checks.**
- `EnsureHostGpuMemoryAllocated` is called on the unextended merged ranges (`shared_memory.cpp:726`)
  before extension happens.
- This is safe only because the sparse granularity is 4 MiB (`d3d12/shared_memory.cpp:58`, and
  vulkan `max(22, 19)`), which is at least the 256 KiB block size and aligned to it.
- Add `static_assert(kHostGpuMemoryOptimalSparseAllocationLog2 >= 6 + 12)`. Without it, a future
  64 KiB granularity would let ahead pages be copied into unmapped tiles and still be marked valid.

**N6. The physical-heap try-lock.** `TryQueryWritablePages` is a try-lock on `heap_mutex_` taken
while the global lock is held, so it cannot deadlock. A miss falls back to the exact upload.
`ahead_lock_misses` also counts "outside the heap" returns, a minor labelling issue.

**N7. The probe handler cannot intercept real guest faults.**
- It is installed after the MMIO handler (`exception_handler_win.cpp:115`), so it is only consulted
  for access violations the MMIO handler declined.
- It accepts only addresses inside `g_probe_page`. That pointer is non-null only during the 8
  probe stores, and it points to a host page from `AllocFixed(nullptr)`, outside the guest
  reservation.
- For every other access violation in the process it is a cheap rejection.
- The `handlers_[]` slot is written non-atomically while other threads may be dispatching. This is
  benign on x64: the data pointer is nullptr either way.
- If the store does not fault, `g_probe_handler_ticks` keeps its old value, so the reported figure
  is stale. This is cosmetic.

**N8. Section page 0x1FFFF000.**
- It lies inside `heaps_.physical.AllocFixed(0x1FFF0000, 64 KiB, reserve, NoAccess)`
  (`xmemory.cpp:223`). No guest allocation in any alias can obtain it.
- The probe writes nothing to it. It only does RO/RW protect round trips, only after
  `QueryProtect` reports read-write, and it leaves the page read-write.
- If the restore to read-write ever failed, nothing reads that page.
- Under Wine each round trip splits and then re-merges one VMA of the 512 MiB physical view, which
  is transient.

**N9. Proton specifics (probes only, which are off by default).**
- `Z:\proc\self\maps` needs the Proton default `Z:` → `/` mapping, which works inside
  pressure-vessel. Wine reads unix files in-process, so `/proc/self` is the game process.
- The probe line costs a hitch on the CP thread at that F10 mark: the 20 ms TSC busy-wait, about
  2,000 protects, 8 faults, and the maps read (which can be large under Wine).
- Nothing else in the defaults is specific to Proton. Bridging, when on, should help Wine because it
  means fewer, larger `mprotect` calls and VMA merging.

**N10. Vulkan.** Vulkan never calls `WatchCensusFrameEnd`, so the shared-memory census stays
inactive there and no `MCLA_WATCH` lines are produced. The memory-side atomics still count. This is
expected (the commit message says the lines are D3D12 only).

## Not verified

- I did not build anything or run the `[watch-census]` tests.
- I did not check cross-DLL linkage of `REXCVAR_DECLARE(memory_watch_census)` from the graphics
  module.
- I have no runtime measurement of the census overhead (S1/S2).
