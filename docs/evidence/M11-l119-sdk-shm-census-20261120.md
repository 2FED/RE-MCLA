# M11 l119 SDK (2026-11-20): MCLA_SHM shared-memory census, MCLA_FAULT_PROBE, the Canary aed81ca9 unwatched-invalidation port, the exact single-range request fast path, TSC-only protection timing; designs of (a2) and (e1)

**Headline.**
- SDK branch `mcla/m11-l119` (worktree `private/l119-sdk-wt`, parent `8d351cc` = l118), one local
  commit `c503e32 (c503e3208d624fd0bb25640e3d02f80ed297eac4)`, not pushed, no tag; the published fork stays v0.20.0.12 = `d889a16`.
- Behaviour changes by default: only **M3** (the correctness port of Xenia Canary `aed81ca9`,
  `memory_invalidate_unwatched`, default true) and **M4** (the single-range request fast path,
  `shared_memory_single_range_fast`, default true, proven equal to the general path). Everything
  else is counters, one start-up probe and a default-off census.
- Built and unit-tested only: not run in the game, no physical PASS. Spark not used
  (deterministic source work checked by unit tests; no bounded extraction or triage task suited it).
- Source: the study [M11-shared-memory-upload-and-watch-study-20261120.md](M11-shared-memory-upload-and-watch-study-20261120.md)
  (sections 2.4, 4 (a2), (e1), (g), (h), 5). The vertex-slot numbers stay on `MCLA_VB v=1`
  ([M11-l118-sdk-extended-edram-20261120.md](M11-l118-sdk-extended-edram-20261120.md) section 3);
  `MCLA_SHM` does not repeat them.

## 1. What shipped (M1-M5)

| item | status | switch | default |
|---|---|---|---|
| M1 `MCLA_SHM v=1` per perf-split window | shipped | always-on counters; census part `mcla_shm_census` | counters on; census **off** (owner arms turn it on) |
| M2 `MCLA_FAULT_PROBE v=1` once at graphics start-up | shipped | `mcla_fault_probe` (restart) | on |
| M3 Canary `aed81ca9` unwatched invalidation | shipped (the unwatched-invalidation part) | `memory_invalidate_unwatched` (hot) | **on** |
| M4 (g) single-range fast path | shipped, exact | `shared_memory_single_range_fast` (hot) | on |
| M4 (h) TSC-only protection timing | shipped | none | always |
| M5 (a2), (e1) | designs only (section 6) | names reserved | not built |

## 2. M1: the MCLA_SHM line

Emitted by `D3D12CommandProcessor::PerfSplitEmit` after `MCLA_VB` in every perf-split window
(the pacing minute and each F10 mark; same `reason` and `seq` as `MCLA_PERF_SPLIT`). Pure
formatter `shm::FormatShmLine` in `include/rex/graphics/util/shm_census.h`. All counts are
window deltas of cumulative counters (a counter that went back prints 0); `_ms` fields use the
window's TSC rate (`tsc_mhz` of `MCLA_PERF_SPLIT`).

```
MCLA_SHM v=1 kind=config census=<0|1> single_fast=<0|1> inval_unwatched=<0|1> hot_k=6 hot_m=8 hash_sample=1/16 frame=<n>
MCLA_SHM v=1 reason=<r> seq=<n> census=<0|1> single_fast=<0|1> inval_unwatched=<0|1>
  req= req_ranges= req_kb= req_src=other:N,vb:N,ib:N,tex:N,resolve:N,memexport:N fast= single= locked= req_offthread=
  up_calls= up_runs= up_pages= up_kb=
  arm_calls= arm_pages= arm_ms= arm_us_per_call= disarm_calls= disarm_pages= disarm_ms= disarm_us_per_call= protect_fail=
  faults= faults_cp= faults_render= faults_other= fault_ms_cp= fault_ms_render= fault_ms_other=
  inval= inval_req_pages= inval_pages= inval_exact= inval_exact_pages= unwatched_inval= unwatched_inval_pages=
  up_timed= up_build_ms= up_barrier_ms= up_pool_ms= up_valid_ms= up_memcpy_ms= up_rec_ms=
  census_frames= up_first_sub= up_retouch_sub= up_gpu_sub= hash_pages= hash_new= hash_same= same_share=
  hot_k=6 hot_m=8 hot_blocks_avg= hot_req= hot_req_kb= hot_fast= hot_up= hot_up_pages= hot_faults= hot_arms= hot_req_per_up=
```
(one line in the log; wrapped here). The config line is written at the first window and whenever
one of the three switches changes.

| field | meaning | thread / cost |
|---|---|---|
| `req` | `SharedMemory::RequestRanges` calls on the command-processor (CP) thread | increment |
| `req_ranges` | ranges passed to them (before merging) | increment |
| `req_kb` | requested KiB after merging | add |
| `req_src` | calls by request source (`ScopedRequestSource`: other, vertex, index, texture, resolve, memexport) | increment |
| `fast` | calls whose pages were all valid: the lock-free return | increment |
| `single` | calls that took the l119 single-range path (M4) | increment |
| `locked` | calls that took the global lock to build upload runs | increment |
| `req_offthread` | calls from any other thread (not in the counts above) | relaxed atomic |
| `up_calls` | `UploadRanges` calls (= requests that upload) | increment |
| `up_runs`, `up_pages`, `up_kb` | uploaded page runs, pages, KiB (CPU uploads only) | add |
| `arm_calls`, `arm_pages` | read-only protection calls (watch arm) and their pages, all threads | relaxed atomics in `Memory::ProtectWatchRange` |
| `arm_ms`, `arm_us_per_call` | **true** arm time: TSC ticks summed without truncation | rdtsc pair that existed before l119 |
| `disarm_*` | the same for the read-write calls (fault-side disarm) | as above |
| `protect_fail` | failed protection calls | existing |
| `faults` | handled non-MMIO write faults (`non_mmio_write_fault_count`, all threads) | existing |
| `faults_cp/render/other` | the same by faulting thread class (`watch_census::ClassifyThread`: the CP thread of the last frame end, the perf-split render thread, others) | increment per fault |
| `fault_ms_*` | handler time by class (TSC, from the handler's write-fault entry to the end of recovery; excludes the OS / Wine exception dispatch); **census only**, else 0 | rdtsc pair per fault, census only |
| `inval`, `inval_req_pages`, `inval_pages` | CPU write-fault invalidations (`MemoryInvalidationCallback`, non-exact), their pages as requested (1 per fault) and after the 64-page widening | relaxed atomics under the global lock |
| `inval_exact`, `inval_exact_pages` | exact invalidations (guest protect, decommit, release; since M3 also of unwatched ranges) | as above |
| `unwatched_inval`, `unwatched_inval_pages` | **M3 footprint**: guest protect/decommit/release calls that invalidated a range with no watch armed (before l119 these did nothing), and their system pages | relaxed atomics |
| `up_timed` | upload calls timed: one locked request in four while the perf split is on | counter |
| `up_build_ms` | lock acquire, run building, upload-ahead (scaled by `up_calls / up_timed`) | sampled TSC |
| `up_barrier_ms` | `CommitUAVWritesAndTransitionBuffer(COPY_DEST)` + `SubmitBarriers` | sampled TSC |
| `up_pool_ms` | `D3D12UploadBufferPool::RequestPartial` (and the trace-writer no-op) | sampled TSC |
| `up_valid_ms` | `MakeRangeValid`: valid bits, the minute histogram and the **arm** | sampled TSC |
| `up_memcpy_ms` | guest memory to the write-combined upload page | sampled TSC |
| `up_rec_ms` | `D3DCopyBufferRegion` into the deferred command list | sampled TSC |
| `census_frames` | frames closed while the census was on | census |
| `up_first_sub` | upload runs none of whose pages any earlier request (fast path included) or GPU write referenced in the current submission: the (e1) prologue eligibility | census: per-page uint32 submission stamps |
| `up_retouch_sub` | runs with a page referenced earlier in the submission (CPU data) | census |
| `up_gpu_sub` | runs with a page GPU-written (resolve / memexport, `RangeWrittenByGpu`) earlier in the submission | census |
| `hash_pages` | uploaded pages of the fixed 1/16 hash sample that were hashed (XXH3 of the 4 KiB guest page right after the copy) | census |
| `hash_new` | of them, pages without a previous hash (first upload, or GPU-written since) | census |
| `hash_same` | of them, the same bytes as the page's previous upload | census |
| `same_share` | `hash_same / (hash_pages - hash_new)`: the share of re-uploaded bytes that did not change (false invalidation by the 256 KiB widening): the input of option (c) | derived |
| `hot_blocks_avg` | average number of hot 256 KiB blocks per frame (CPU-write-faulted in at least 6 of the last 8 closed frames) | census: 2048-entry history advanced at frame end |
| `hot_req`, `hot_req_kb`, `hot_fast` | requests (and their KiB, fast-path share) whose first page lies in a hot block | census |
| `hot_up`, `hot_up_pages` | upload runs and pages of those requests | census |
| `hot_faults` | CPU write faults whose page lies in a hot block | census |
| `hot_arms` | arm calls of `MakeRangeValid` for hot blocks | census |
| `hot_req_per_up` | `hot_req / hot_up`: **R/U for hot blocks, the (a2) decision input** | derived |

**up_same method (the brief's either-or).** No CPU shadow of the host buffer exists, so the line
does not compare against the host bytes. It samples by page: a fixed 1/16 of all pages
(`shm::IsHashSampledPage`, a multiplicative hash of the page index so ring buffers do not alias
with the sample), and **every** upload of a sampled page hashes it and compares the hash with the
one stored at that page's previous upload (`std::vector<uint64_t>` of 131,072 entries, 1 MiB,
allocated when the census is first switched on). A GPU write (`RangeWrittenByGpu`) clears the
stored hashes of its pages, so the next upload counts as new. The host buffer holds the previous
upload of a page unless the GPU wrote it in between, so `hash_same` approximates "the bytes the
host buffer already holds".

**Cost.** Always on: plain increments on the CP thread per request and per upload, relaxed
atomic adds per protection call and per fault (on paths that cost 0.5-35 us each), one thread-id
read per request. Timing: the protection calls already had an rdtsc pair; the upload split is
sampled (1 in 4 locked requests, about 8 rdtsc per sampled call) and only while the perf split is
on. Census (default off): per request a lock-free history load and a page-stamp fill, per upload
run a stamp scan, 1/16 of uploaded bytes hashed (about 0.75 MB per frame at 1,300 uploads of
9 KB: about 0.05 ms per frame on the PC by XXH3 throughput), the 2048-block history advance under
the lock at each frame end (a few microseconds), and per fault one rdtsc pair; memory 2.5 MiB.
Nothing was measured in the game.

**Other changes for M1.** `runtime::MemoryAccessDiagnosticSnapshot` (installed header
`rex/system/mmio_handler.h`) gains, at the end, `watch_arm_ticks`, `watch_disarm_ticks`,
`watch_arm_pages`, `watch_disarm_pages`, `write_fault_class_count[3]`,
`write_fault_class_ticks[3]`, `unwatched_invalidation_calls`, `unwatched_invalidation_pages`
(ABI change: a host must be rebuilt against this SDK, as every l1xx host is).

## 3. M2: MCLA_FAULT_PROBE

```
MCLA_FAULT_PROBE v=1 n=2000 done=2000 wine=<0|1> page_kb=4 tsc_mhz=<f> arm_ns=p50:..,p90:..,mean:..,min:.. dispatch_ns=... disarm_ns=... resume_ns=... dispatch_resume_ns=... round_trip_ns=... arm_failures=0 no_fault=0 restore_failures=0 total_ms=<f> status=ok
```
- Where and when: `D3D12CommandProcessor::SetupContext` after `MCLA_CLOCK`, on the CP thread,
  once per process (`mcla_fault_probe`, default true, restart), before the first guest frame.
  `rex::memory::watch_census::RunFaultProbe(2000)` in `src/system/watch_census.cpp`.
- Method (Windows and Wine only; another platform prints `status=unsupported`): a private
  `VirtualAlloc` page (never guest memory, never the guest section views); a read-only /
  read-write round trip first (`status=protect_failed` and no probe if it fails); its **own**
  vectored handler added **first** in the process chain with `AddVectoredExceptionHandler(1, ...)`
  and removed after the loop: it handles only a write access violation inside the probe page,
  passes every other exception on unchanged, so the SDK's handler chain (`rex::arch` handlers,
  the MMIO handler, the watch path) never sees the probe's faults and nothing is added to it.
  Per iteration, all TSC: `t0`, `rex::memory::Protect(read-only)` (the SDK's arm call), `t1`, a
  volatile store that faults; in the handler `entry`, `VirtualProtect(read-write)`, `restored`,
  `EXCEPTION_CONTINUE_EXECUTION`; after the store `t2`.
  - `arm_ns = t1 - t0`; `dispatch_ns = entry - t1` (CPU trap, kernel or Wine SIGSEGV delivery,
    user-mode dispatcher, the VEH list up to our first entry); `disarm_ns = restored - entry`;
    `resume_ns = t2 - restored` (handler return, `NtContinue` or Wine's equivalent);
    `dispatch_resume_ns` = dispatch + resume (the OS / Wine cost of one fault that the
    `MCLA_SHM fault_ms_*` handler time does not contain); `round_trip_ns = t2 - t1`.
  - TSC rate over the whole probe after a 20 ms sleep (at least 20 ms of calibration).
  - Cost: 2000 x (arm + round trip): about 4-5 ms on the build PC (unit test: arm p50 0.49 us,
    dispatch 0.70 us, disarm 0.40 us, resume 1.0 us, private page, native Windows); on the Deck
    with the l81 numbers (arm 16.6 us, disarm 19.5 us, fault 35 us plus dispatch) about 0.15-0.2 s
    at start-up.
- Caveat: a private page is cheaper to protect than a page of the guest physical section view
  (l63: about 0.66 us truncated vs 1.2 us true for the watch arm on the PC). The probe gives the
  dispatch and resume costs (the unknown); the real arm and disarm cost per call is
  `MCLA_SHM arm_us_per_call` / `disarm_us_per_call`.
- Safety argument: the handler gate is the page pointer (set only during the loop); the faults
  are synchronous on the probing thread; the restore is retried three times and the page was
  checked to round-trip before the handler is installed. If the restore still failed the handler
  would pass the exception on (a crash report, not a hang); `restore_failures` would show it.

## 4. M3: port of Xenia Canary aed81ca9 (correctness)

- Source: public commit `aed81ca93a1f3e8dd043107babd33438379f379d` of xenia-canary
  (`canary_experimental`), "[Memory] Rework guest access resolution and unwatched invalidation"
  (author date 2026-08-01, merged 2026-08-18; "a combination of three different Edge commits";
  co-authored). Fetched read-only from the public GitHub API. **Licence BSD-3-Clause; attribution
  in the headers of `src/system/xmemory.cpp` and `include/rex/system/xmemory.h`** with the commit
  id, and here. Adapted, not copied verbatim.
- Ported (the unwatched-invalidation part):
  - `PhysicalHeap::TriggerCallbacks` gains `invalidate_unwatched` (default false): with it the
    invalidation callbacks also run when no page of the range is watched (exact range, nothing to
    unprotect), counted in `unwatched_invalidation_calls/pages`.
  - `PhysicalHeap::Protect`: invalidate also with no watch armed, and **write-combine counts as
    writable** (Canary `IsWritableProtect`; our `ToPageAccess` already treated write-combine as
    writable).
  - `PhysicalHeap::Decommit` and `PhysicalHeap::Release`: invalidate also with no watch armed.
  - All three behind `memory_invalidate_unwatched` (default true; false = exactly the l118
    behaviour: watched ranges only, the write flag only). Host control `inval_unwatched_off`.
- Not ported, and why:
  - The read-watch / data-provider half (`notify_on_read`, `RegisterPhysicalMemoryReadCallback`):
    a new feature, not part of the correctness gap; our l92 read watch is a different mechanism.
  - `SystemPageGuestAccess` (the most permissive access of the guest pages a host page covers):
    it matters only when the host page is larger than the guest page; on x86-64 Windows and Wine
    the host page is 4 KiB and the smallest guest physical page is 4 KiB, so our per-page lookup
    gives the same result.
  - "A write fault on an unwatched page counts as handled only if the guest mapping allows the
    write": in Canary this is the signal-safe path where `TriggerCallbacks` returned true for an
    unwatched page. Ours returns false there and `Memory::AccessViolationCallback` then restores
    write access only when the guest protection or the committed allocation allows it (the
    existing recovery path), so the behaviour already matches.
- Guest-visible situations it changes (only these three calls, on physical memory):
  1. A page that was **read-only when it was made valid** (the arm skips read-only pages, so it
     got no watch) and is later made writable by the guest (`NtProtectVirtualMemory` /
     `MmSetAddressProtect` paths reaching `PhysicalHeap::Protect` with write or write-combine):
     before l119 the GPU copy stayed "valid" while the guest wrote freely (stale vertices,
     indices or textures until something else invalidated the page); now it is invalidated and
     re-uploaded at its next request.
  2. A **protect to write-combine only** (no write flag) of a watched range: before l119 no
     invalidation; now exact invalidation (as for the write flag).
  3. A **decommit or release of a valid range without a watch** (read-only pages, or pages whose
     watch a fault had cleared but whose valid bit was set again without a re-arm because the
     page was read-only at that time): now invalidated, so a later allocation that reuses the
     physical range is not served the old GPU copy (the issue the upstream comment cites).
  Watched ranges behave exactly as before. A range that is valid and watched never had this
  problem. GPU-written pages in an unwatched range are invalidated too (as upstream; the same
  semantics the watched case already had).
- Interactions:
  - Texture and range watches (`FireWatches` from `MemoryInvalidationCallback`): they now also
    fire for these three calls on unwatched ranges; a texture over such a range is marked outdated
    and reloaded: more correct, a possible extra reload. `unwatched_inval` counts the calls.
  - The l118 vertex-slot validity check (`d3d12_vb_skip_validate`) relies on the valid bitmap;
    M3 makes the bitmap correct in case 1 and 3, so a skip over such a page is now refused and the
    slot re-requested (counted in `MCLA_VB vb_*_skip_invalid`).
  - Upload-ahead (Wine default 16 pages) adds only invalid, committed, writable pages: unaffected.
  - Disarm bridging (Wine default): `invalidate_unwatched` touches no protection (nothing to
    unprotect when nothing is watched), so no bridge decision changes.
  - Cost: one extra callback (under the global lock, a bitmap clear and the watch walk) per such
    guest call; zero when the game never makes valid read-only memory writable. Unknown until
    `unwatched_inval` is read from a run.
- Tests: `tests/unit/memory/unwatched_invalidation_test.cpp` (both cvar values: protect-to-writable
  of a read-only unwatched range, write-combine, decommit, release, watched ranges unchanged, the
  fault path unchanged).

## 5. M4: single-range fast path (g) and TSC-only protection timing (h)

**(g) Exact single-range path** (`shared_memory_single_range_fast`, default true, hot):
- The request normalization (drop empty ranges, fail outside the buffer, sort, merge) and the
  invalid-page run building moved unchanged into `shm::NormalizeRequestRanges` and
  `shm::AppendInvalidPageRuns` (util/shm_census.h); `RequestRanges` uses them for every request.
- For `count == 1` with the switch on, `shm::NormalizeSingleRequestRange` replaces the general
  normalization: no `thread_local` vector access, no `clear`/`reserve`/`emplace_back`, no sort
  call, no merge loop. Everything after the normalization (sparse residency, the lock-free valid
  check, the locked run building, upload-ahead, the upload) is the same code over a pointer and a
  count.
- **Proof of equality** (`tests/unit/graphics/shm_census_test.cpp`):
  1. "l119 single-range normalization equals the general path": a table of 15 edge cases (length
     0, start at and beyond the buffer end, `UINT32_MAX`, exact fit, one byte over) and 100,000
     random ranges: same plan (ok / empty / out of range) and the same range.
  2. "l119 invalid page runs against a page-by-page reference": 4,000 random bitmaps (all valid,
     all invalid, random words) with 1-3 ascending, sometimes touching ranges per call: the runs
     equal a page-by-page reference, including the coalescing across ranges.
  3. "l119 RequestRange: the single-range path uploads exactly what the general path does": a
     real `memory::Memory` and a recording `SharedMemory` subclass (marks runs valid as the D3D12
     upload does): 3,000 random layouts of valid pages and random requests (empty, out of range,
     at the buffer end, page-crossing); for each, the general path and the single path from the
     same state: the same return value, the same upload runs and the same valid bitmap after the
     request over the whole 8 MiB window; plus "several ranges always take the general path".
  Because both paths call the same helpers after normalization, (1) and (3) cover the whole
  difference.
- **Measured saving** (micro-benchmark test case, not a gate; build PC, native, an all-valid
  9 KiB range, the lock-free path of about 97 % of requests, best of 3 x 200,000 calls):
  **general 17.5 ns, single 13.8 ns per request: about 3.8 ns saved**. At about 1.8 requests per
  draw and 6 k draws per frame that is about 0.04 ms of CP per frame on the PC: smaller than the
  study's 0.1-0.3 ms estimate (the merged-range vector was already reused per thread). The Deck
  figure is not known (expected a few times larger per request).

**(h) TSC-only protection timing** (always):
- `Memory::ProtectWatchRange`: the `steady_clock::now()` pair and the per-call microsecond
  truncation are gone; the existing rdtsc pair feeds `watch_arm_ticks_` / `watch_disarm_ticks_`
  (and the pages). `EnableAccessCallbacks`'s lock-wait `steady_clock` pair is an rdtsc pair too.
- `watch_arm_us`, `watch_disarm_us`, `watch_arm_lock_wait_us` of the diagnostic snapshot (read by
  `MCLA_PERF_SPLIT watch_arm_ms`, `MCLA watch protect/min`, `MCLA_HITCH` and the host's F10 JSON)
  are now derived from the ticks with the TSC rate since the `Memory` was created: **no longer
  truncated per call**. Expect `watch_arm_ms` about 1.4-1.7x the l118 values on the PC for the
  same work (the study's truncation estimate) and almost unchanged on the Deck (16 us calls).
- Saving: two `steady_clock::now()` per protection call (QueryPerformanceCounter: about 20-30 ns
  native, about 110 ns under Proton per l96 `MCLA_CLOCK`), so about 0.05-0.1 us per arm and per
  disarm; not measured separately.

## 6. M5 designs (not built; for the build after the owner's census run)

### 6.1 (a2) compare-before-copy for hot pages

Decision gate (from `MCLA_SHM`): build if `hot_req_per_up <= 3` and `hot_req_kb / hot_req <= 4`
on the PC, or if the Deck projection (hot arms x `arm_us_per_call` + hot faults x (handler +
`dispatch_resume_ns`) - hot requests x compare cost) is at least -3 ms per frame.

- **Cvars** (rexgpu, hot, latched at the frame end):
  `shared_memory_hot_compare` (int: 0 off, 1 verify, 2 on; default 0),
  `shared_memory_hot_compare_k` (int 1-8, default 6: uploaded in K of the last 8 frames),
  `shared_memory_hot_compare_exit_equal` (int, default 32: consecutive equal compares before a
  page is returned to the watch), `shared_memory_hot_compare_max_mb` (int, default 16: the shadow
  budget). Host keys `hot_compare_verify`, `hot_compare` (and later `_wine` defaults if the PC is
  neutral).
- **Data** (CP thread unless noted):
  - `page_upload_history_` uint8 per page (131,072 B): bit 0 = uploaded in the frame that just
    closed; shifted at frame end (the census already has the per-upload hook);
  - `hot_bits_` bitmap (16 KiB), atomic words: read lock-free by the request fast path;
  - `shadow_slot_` uint32 per page (512 KiB) into `shadow_pool_` (4 KiB slots up to
    `max_mb`, free list);
  - `equal_run_` uint8 per page; `src_mask_` uint8 per page (request sources seen while hot);
  - `watch_count_` uint16 per page, maintained by `WatchMemoryRange` / `UnlinkWatchRange` under
    the global lock (texture and range watches).
- **Entry** (at frame end): history popcount >= K, page not GPU-written
  (`system_page_flags_valid_and_gpu_written_` clear), `watch_count_ == 0`, only vertex / index
  sources so far, a free shadow slot. The page's notify bit is not re-armed from then on
  (`MakeRangeValid` skips `EnablePhysicalMemoryAccessCallbacks` for hot pages in mode 2); its
  valid bit stays **clear**, and the hot bit is set.
- **Request** (mode 2): the fast path treats a hot page as "compare": for each hot page of the
  range, `memcmp(guest bytes of the requested sub-range, shadow)`; equal: nothing to do (the host
  buffer holds the shadow's bytes in queue order); different: the page joins the upload runs
  (inline copy, the same barrier pair as today), and the **copy goes guest -> shadow -> upload
  buffer** (the shadow is the single source of truth, so a guest write during the copy cannot make
  shadow and host differ).
- **Exit** (to the normal watched state with the valid bit clear): `exit_equal` consecutive equal
  compares (static data), a texture or range watch registered on the page (`watch_count_` > 0,
  under the lock), `RangeWrittenByGpu` over the page (before the GPU data becomes valid),
  a non-vertex/index request source, shadow budget pressure (oldest out). Exit frees the slot and
  clears the hot bit; the next request uploads and arms as today.
- **Verify (mode 1)**: arming and validity unchanged (today's path decides everything); for
  candidate pages the shadow is kept (guest -> shadow at each upload) and each request compares
  in shadow and counts, with the valid bit read before **and** after the compare (an
  invalidation between the two means the guest wrote: counted as invalid):
  `cmp_eq_valid` (agree), `cmp_eq_invalid` (false invalidation: the saving), `cmp_diff_valid`
  (**must be 0**: the compare saw a change that protection missed), `cmp_diff_invalid` (agree),
  bytes compared, projected arms and faults saved. Line:
  `MCLA_SHM_HOT v=1 reason= seq= mode= k= pages_hot= entries= exits_static= exits_watch= exits_gpu= exits_src= exits_budget= cmp= cmp_kb= cmp_eq_valid= cmp_eq_invalid= cmp_diff_valid= cmp_diff_invalid= copies= copies_kb= arms_saved= faults_saved= shadow_mb=`.
- **Owner runs**: PC verify (10 minutes daylight with a race, KI-047 police livery, garage vinyl;
  every `cmp_diff_valid` 0), then on/off A/B at 1080p and 1440p with gpu-cat; Deck verify, then
  off/on/on/off on the l81 loop (CP >= -2 ms per frame, safety counters 0).

### 6.2 (e1) per-submission copy prologue for first-touch uploads

Decision gate: build if `up_first_sub / up_runs >= 0.5` in the census run.

- **Cvar** `d3d12_upload_prologue` (int: 0 off, 1 shadow, 2 verify, 3 on; default 0; latched at
  submission start). Host keys `upload_prologue_shadow`, `upload_prologue_verify`,
  `upload_prologue`.
- **Data**: the census's per-page submission stamps (`touch`, `gpu`; uint32 per page, no
  per-submission clear) become always-on while the cvar is >= 1. New: a draw-level reference
  stamp `bound_` (uint32 per page) written in `IssueDraw` for every vertex-fetch range and index
  range the draw binds, **including** the l118 vertex-slot skips A/B and the l117 index-buffer
  view cache hits (they read shared memory without a request); and per submission a prologue
  record list `{upload buffer, offset, destination, size}`.
- **Eligibility** at upload time: no page of the run stamped (`touch`, `gpu` or `bound_`) in the
  current submission. Rationale: GPU order changes only for commands that never touch those
  pages; every reader of the shared buffer stamps.
- **Recording**: the CPU copy stays at request time (guest -> upload buffer, as today); eligible
  runs skip the inline barrier pair and the inline `CopyBufferRegion` and are appended to the
  prologue record; others go inline. At submission close a second command list (from the same
  per-submission allocator ring) is recorded: one barrier from the buffer's state at submission
  start (saved in `BeginSubmission`) to `COPY_DEST`, the merged copies, one barrier back; then
  `ExecuteCommandLists(2, {prologue, main})`. Streaming (mid-frame) submissions close their own
  prologue the same way.
- **Verify (mode 2)**: both the prologue copy and the inline copy are recorded (the same bytes,
  so the picture is identical); `pro_conflict` counts eligible runs whose pages the draw-level
  `bound_` stamps or a resolve / memexport marked earlier in the submission by a path that
  bypassed the request stamps (**must be 0**), plus a KI-047 composite and KI-033 vinyl check.
  Shadow (mode 1) only counts. Line:
  `MCLA_SHM_PRO v=1 reason= seq= mode= eligible= eligible_kb= inline= prologue_lists= prologue_copies= merged= conflict= barriers_saved=`.
- **Owner runs**: PC verify then on/off with gpu-cat at 1440p (the GPU drain is the expected
  gain) and single tile; Deck verify then on/off.

## 7. Tests, build, warnings

- New tests: `tests/unit/graphics/shm_census_test.cpp` (7 cases `[l119][shm]`: single vs general
  normalization, general normalization, runs vs reference, RequestRange equality through a real
  `SharedMemory`, the micro-benchmark, the census helpers, the census through `SharedMemory`
  (first touch, retouch and GPU classes, same-bytes hashing, hot blocks from faults, census off),
  the line formats); `tests/unit/memory/unwatched_invalidation_test.cpp` (8 cases `[l119][memory]`:
  M3 in both modes for protect, write-combine, decommit and release, watched ranges unchanged, the
  fault path unchanged, the TSC timing and the faults by thread class, the fault probe on Windows
  (and the SDK's handlers still working after it), the probe statistics). `tests/unit/CMakeLists.txt`:
  the two files, and `xxHash::xxhash` for `unit_tests` (the compiled-in `shared_memory.cpp` now
  includes `rex/hash.h`).
- ctest (`private/l119-sdk-b`, Release): **2436 / 2436 passed** three times in a row before the commit (two rounds of three: after the first build and after the clang-format rebuild) and three times after the install (l118 2420 + 16 new cases); the known flaky `physical_mapping_test.cpp:184` did not fail in any of the nine runs.
- Warnings: the full build's normalized set: **210 = the l118 set, 0 new, 0 gone**
  (scratch `l119-work/w119all.txt` against `l118-work/w118all.txt`); incremental rebuilds: 0 new.
- `git-clang-format` against `8d351cc`: applied (whitespace-only wraps in `src/graphics/shared_memory.cpp` and `src/system/mmio_handler.cpp`; the three new files formatted whole), then "clang-format did not modify any files"; `git diff --check`: clean.
- Installed to `private/l119-sdk-install` (`0.20.0.4-dev.gc503e32`):

| bin file | bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11794944 | C09485B396AF5E2FA406E96AC22F1592B99F047F8EB7CC10B202C970AAB61E40 |
| rexgpu-xenos.dll | 3972608 | 14ED326C7600AFBAAB950FB1235BB4C0984AB9D163ADF62D39B9621A46DD0844 |
| TracyClient.dll | 246784 | 562511ACBD2D70E2716C71753717054662ED39B7203E139FB01D332D338EAA82 |
| rexglue.exe | 4113408 | 88B58E965F999A2F2ADA21BFA728661878BB9E375F597C47C6C2DD4FCCFDCC3C |

TracyClient.dll and rexglue.exe have no source change; their bytes differ in the fresh build tree
(as with every fresh SDK tree).

## 8. Limits

- Nothing ran in the game: every number in this document other than the unit-test probe and the
  micro-benchmark is an expectation.
- `fault_ms_*` exist only with the census; `faults_*` counts are always on.
- `up_first_sub` treats every request as a GPU reference; a draw that reads shared memory without
  a request (vertex-slot skips, the index-buffer view cache) is not stamped, so `up_first_sub`
  may overstate the (e1) eligibility a little: the (e1) design adds the draw-level stamps for that
  reason.
- `same_share` comes from 1/16 of the pages, hashed from guest memory right after the copy (a
  guest write between the copy and the hash is possible, rare and counted as a change).
- The fault probe measures a private page; the guest views are file-mapped sections (more
  expensive protection calls); the dispatch and resume numbers are the point.
