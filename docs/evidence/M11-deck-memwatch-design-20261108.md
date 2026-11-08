# Deck memory-watch cost: hot pages with per-submission batched copies (design, read-only, 2026-11-08)

Read-only design by an Opus subagent on 2026-11-08 (SDK l79 sources, l66 Deck census logs, M10 hot-blocks evidence; external project used for the idea only); accepted by the primary agent.

Read-only design by an Opus subagent. Nothing was built, launched or committed. Spark was not used:
the work is exact source reading plus a deterministic parse of `MCLA_WATCH` census lines, and
checking a model's answer would cost more than doing the parse.

Sources:
- the l79 Deck A/B (`docs/evidence/M11-l79-deck-ab-20261108.md`);
- the M10 watch design and results: `M10-deck-watch-upload-design-20261101.md`,
  `M10-l64-deck-ab-20261103.md`, `M10-hot-blocks-sdk-20261103.md`, `M10-l66-deck-ab-20261103.md`;
- `M11-cp-cost-plan-20261106.md` section 2.5;
- `M11-external-repos-survey-20261108.md`. Only the idea "hot pages kept out of watched ranges" is
  used from it; no GPL code was consulted.

`<sdk>` is the l79 SDK worktree (`mcla/m11-l79` = `0755836`). The hot-blocks v1 code is on the
separate local branch `mcla/m10-hotblocks` (`cdf469a`, based on v0.20.0.7). It is **not merged into
l79** (`git merge-base --is-ancestor cdf469a HEAD` is false).

New numbers in section 1 come from a parse (`mwatch/agg.py` in the job tmp) of the census lines
(`MCLA_WATCH v=1`, census was on in l66) in the l66 owner Deck sessions:
- "all" runs `3c85462e` and `efe3a72d`, 12,074 frames;
- hot-on run `8a5c113f`, 7,103 frames.

Frames before frame 2500 (boot and title) were skipped. l66 "all" has the same watch defaults as
l79 on Proton: upload-ahead 16 and disarm bridging on.

## 0. Correction to the framing: what runs on the CP

The l79 report sums arm, disarm and write faults (11.0 + 4.4 + 6.6) as "22 ms/f, 39 % of CP busy".
That overstates the CP's share:

- **Arm** (11.0 ms/f) runs on the CP. It happens inside `upload`: `UploadRanges` calls
  `MakeRangeValid` (`<sdk>/src/graphics/d3d12/shared_memory.cpp:392`), which calls
  `EnablePhysicalMemoryAccessCallbacks` (`<sdk>/src/graphics/shared_memory.cpp:535`).
- **Disarm** (4.4 ms/f) is called only from the write-fault path (`TriggerCallbacks`,
  `<sdk>/src/system/xmemory.cpp:2046-`). It is therefore **inside** `write_fault` (6.6 ms/f), not in
  addition to it.
- **Write faults run on guest threads.** The census shows `faults_cp=0`; the render thread takes
  about 95 % of faults and the other guest threads the rest.
- The CP's kernel time (11.4-12.5 ms/f) matches the arm syscall time (11.0 ms/f), so arm is
  essentially all of the CP's kernel time.

So the memory-watch cost is:
- **CP**: about 11 ms/f of arm, plus the memcpy and copy recording in the rest of `upload` (about
  5 ms/f);
- **guest threads**: about 6.6 ms/f, mostly on the render thread (the second wall at about 46 ms/f
  of work), including 4.4 ms/f of disarm syscalls.

All of it runs with the global critical region held. That is the same `std::recursive_mutex` the
guest atomics use (`<sdk>/src/system/xmemory.cpp:1945`; disarm runs under the lock that the fault
path passes in). The census measured 8.8 ms/f of arm hold and 3.9 ms/f of disarm hold in l66.

## 1. Cost model

### 1.1 Per-frame census, Deck driving (l66 "all" ≈ l79 watch defaults)

| per frame | value | note |
|---|---|---|
| requests (CP) | VB 5,165, IB 5,045, tex 145, resolve 56 | 96-97 % of VB/IB requests take the lock-free all-valid fast path |
| uploads (runs) | VB 162, IB 236, tex 68 | upload bytes: VB 9.4 MB, IB 13.5 MB, tex 4.2 MB (27 MB/f with upload-ahead) |
| **arm calls** | **479 (8.0 ms, 16.7 us/call)** | by source: **IB 242 (4.15 ms), VB 167 (2.75 ms), tex 70 (1.15 ms)**, other 0 |
| pages per arm call | 14 (92 % of calls are in the 5-16 page bucket) | upload-ahead 16 already coalesces each run; one call per upload run |
| **disarm calls** | **185 (3.7 ms, 20 us/call)**, 38 pages/call | 73 % are 17-64 pages; bridging saves 39 gaps/f |
| **write faults** | **render 176 (6.4 ms, 36.6 us each)**, other guest 9 (0.45 ms), CP 0 | one fault = invalidate a 256 KiB block + about 1 bridged disarm |
| fault inner costs | VirtualQuery 0.3 ms/f; global-lock wait in fault 0.67 ms/f | Wine exception dispatch before the handler is not measured |
| global-lock hold in protect walks | arm 8.7, disarm 3.9 ms/f | |
| **blocks faulted in 8 of the last 8 frames ("h8")** | **65 blocks (16.3 MB)** | **92 % of arms (441/479), 91 % of faults (167/185), 92 % of upload pages** |
| h8 blocks with a texture watch | 22 of the 65-71 | not eligible under the v1 per-block rule |
| h8 upload pages | 6,200/f = 1.5x the 4,160 pages of those blocks | the hot set is re-uploaded about 1.5 times per frame |
| primitive-processor converted-index arms | about 0 | `arm_calls` equals the sum of the `src_*` arms |

**Interpretation.** The watch cost is almost entirely about 65 dynamic VB/IB ring blocks (16 MB)
that the guest rewrites every frame. The cycle runs as follows:
1. The guest writes the ring head and hits a read-only page, so it faults.
2. The fault invalidates the 256 KiB block (`<sdk>/src/graphics/shared_memory.cpp:930-960`) and
   disarms it (`<sdk>/src/system/xmemory.cpp:2193-2210`).
3. The CP's next draw requests its few-KB VB/IB range. The request is invalid, so the CP takes the
   global lock (`:802`), upload-ahead extends the run to about 14 pages (`:855`), and then it arms,
   copies and records `CopyBufferRegion` (`d3d12/shared_memory.cpp:373-398`).
4. A little later the guest writes the next draw's data into the just-armed pages, so it faults
   again.

This comes to about 2.6 arms per fault and about 1.5 uploads per hot page per frame.

**Textures** are a minor source: 70 arm calls/f (1.15 ms). **Constants** do not use this path:
shader constants are register writes, and `src_other` is 0.

### 1.2 l79 against l66

l79 shows arm 11.0-12.1 ms/f against 7.9-8.0 in l66, at similar draws/frame. l79 runs with the
census off, so it cannot say whether the increase is more calls or dearer calls. If the per-call
cost were the same 16.7 us, that would be about 660 calls/f. The count is available without a
build: the always-on line `MCLA watch protect/min: arm calls ... disarm calls ...`
(`<sdk>/src/graphics/shared_memory.cpp:131-136`) is in the l79 session logs. The primary agent
should extract it from the l79 archive; this design could not find that archive under `private/`.

### 1.3 Why each call is so expensive under Wine

| | PC (Windows) | Deck in-game | Deck micro-benchmark (M7-016) |
|---|---|---|---|
| VirtualProtect arm | 0.64-0.76 us | 16.7 us (l66); l79 is 11 ms/f | 2.7 us (1 page) - 3.9 us (64 pages); native mprotect 1.25 us |
| write fault in handler | 7.8 us | 36.6 us, plus unmeasured dispatch | - |

Each step below is described from general knowledge of Wine's ntdll design. None of it was checked
against the Proton source.

**VirtualProtect (in the game, 4-6x the micro-benchmark):**
- The call goes PE kernel32 -> ntdll -> the Wine syscall dispatcher, which saves the full XSAVE
  state -> the unix side.
- The unix side runs `server_enter_uninterrupted_section`, which blocks signals and takes the
  process-wide `virtual_mutex`. That mutex is shared with every virtual-memory call and with Wine's
  own fault pre-check. It then looks up the view, updates the per-page protection bytes, calls
  `mprotect` per run, and unblocks.
- There is no wineserver round trip for protecting the process's own memory. The expensive part is
  in Linux:
  - `mprotect` on a `MAP_SHARED` mapping that is mapped four times (physical, A0, C0, E0) splits or
    merges VMAs;
  - it takes `mmap_lock` for writing, so it contends with page faults from 8 busy threads;
  - making a page read-only needs a **TLB shoot-down IPI** to every CPU running another thread of
    the process.
- The IPIs also cost time on the receiving cores, including the render thread. The single-threaded
  micro-benchmark has none of this.

**Write fault (36.6 us in our handler on the Deck, against 7.8 us on the PC, plus Wine dispatch):**
1. Hardware fault -> kernel SIGSEGV on the signal stack -> Wine `segv_handler`, which first runs
   its own write-watch and guard check under `virtual_mutex`.
2. Wine builds the CONTEXT/EXCEPTION_RECORD and switches to the PE side ->
   `KiUserExceptionDispatcher` -> our VEH (`<sdk>/src/system/mmio_handler.cpp:384-`).
3. Our handler takes the global lock (contended with the CP's 16.7 us arms), calls `VirtualQuery`
   (another Wine call under `virtual_mutex`), runs the invalidation callbacks and the disarm
   `VirtualProtect`.
4. `NtContinue` restores the full context.

A fault therefore costs about three Wine virtual-memory calls plus two full context switches across
the PE/unix boundary.

**Implication.** Only fewer calls and fewer faults help. Larger calls barely do: upload-ahead
already made the arms about 14 pages each, and per-call cost grows only slowly with pages.

## 2. Options

Savings are ms per driving frame on the Deck. They are scaled to l79 (arm 11.0, fault 6.6) using the
l66 census shares.

### (a) Hot-page set with copies batched per submission ("hot pages v2"). Recommended.

**Idea.** Pages that the guest rewrites every frame are not watched at all. Every VB/IB request that
touches them copies the requested bytes. The GPU copy commands are not recorded between draws: they
go into a per-submission prologue command list that executes before the submission's main list.

**Why v1 failed and why batching avoids it.** In l66, v1 cut arm calls 474 -> 275/f, disarm
182 -> 110/f and render faults 172 -> 102/f. But the CP spent the saving again (draw +2.0, copy
+0.7, fence/sync +1.35 ms/f) and the GPU rose 27.1 -> 37.5 ms/f. The cause was 2,050 hot copies per
frame, 99.9 % of them recorded after a draw in the open submission (`hot_copies_after_draw`):
- Each inline upload does `CommitUAVWritesAndTransitionBuffer(COPY_DEST)` + `SubmitBarriers`
  (`d3d12/shared_memory.cpp:373-374`).
- The next draw transitions back through `UseForReading` + `SubmitBarriers`
  (`<sdk>/src/graphics/d3d12/command_processor.cpp:4419-4424`).
- That makes about 4,000 barriers per frame. Under vkd3d-proton each transfer between draws also
  ends and restarts the render pass, and every one of them is replayed on the vkd3d replay thread
  (23 % -> 31 % of a core).

In the prologue scheme:
- One prologue list per submission holds one transition to COPY_DEST, the merged copies, and one
  transition back to the buffer state the main list starts from.
- It is executed as `ExecuteCommandLists(2, {prologue, main})` at
  `command_processor.cpp:5870` (currently 1 list).
- That gives 2 barriers per submission (about 28 per frame), no render-pass splits, and 14 x (a few
  dozen) copies per frame instead of 2,000.
- A separate list is needed because streaming submissions replay the deferred list into
  `command_list_` while it is being recorded (`:5837`), so nothing can be prepended to it later.
  The comment at `:5825-5829` forbids two *deferred* lists in one call. A plain copy prologue is
  outside that restriction, and the ExecuteCommandLists boundary it adds is a full barrier, which
  is what we want there.

**When the snapshot is taken (the safety argument).**
- The CPU copy still happens **at request time**, on the CP, exactly when v1 and today's path
  sample guest memory. It goes into a cached per-submission staging buffer. Guest-visible semantics
  are therefore identical to v1: each draw's bytes are captured when the CP processes that draw.
- Only the GPU copy command moves earlier in GPU order, to before the submission's first draw.
- **Copying at submission end instead (deferring the CPU snapshot) is rejected.** The guest may
  reuse command-buffer or ring space once the CP read pointer or a fence passes (scratch writes,
  `EVENT_WRITE`, `MEM_WRITE`, interrupts, `WAIT_REG_MEM`, `fence_immediate`). This is the same
  hazard as the rejected "defer arms" option in M10 design (d).

**Correctness rule for moving GPU copies earlier.** A prologue write to range R is invisible to
draws that should have seen the old bytes of R only if no earlier draw in the same submission read
R. This is guaranteed by three rules:

1. **Byte-exact staging.** Stage exactly the requested byte range (the VB fetch size, or the IB
   needed bytes at `<sdk>/src/graphics/primitive_processor.cpp:931`), not whole pages. Consecutive
   draws using successive bytes of the same ring page then do not overlap. With page granularity
   almost every second request would overlap and fall back to an inline copy.
2. **Overlap check against what is already staged in this submission.** Use a per-page
   "staged this submission" bitmap for fast rejection, then the intervals of that page.
   - Overlap with equal bytes (memcmp between guest memory and the cached staging copy; for example
     the same IB reused by several draws): no copy at all. This also saves the memcpy.
   - Overlap with different bytes (the guest rewrote bytes that an already-processed draw of this
     submission uses): fall back to today's inline copy for this request. That is still correct in
     GPU order: prologue S1, draws read S1, inline S2, later draws read S2. Count these cases.
3. **Entry clears the block's or pages' CPU valid bits** (`valid &= gpu_written`, which is 0 for an
   eligible page). Every GPU read of a hot page then goes through a request and is staged; none goes
   through the fast path.
   - **This is a new hazard that v1 did not have.** v1 left pre-entry pages valid and armed. If such
     a page is read by a fast-path draw, then faults and is re-requested in the same submission, a
     prologue copy would overwrite what the earlier draw reads.
   - Entry happens only at frame end (a submission boundary), and exits only go hot -> normal, so
     the rule holds within a submission.

Interactions that keep today's behaviour:
- **GPU writes.** Any GPU write (`RangeWrittenByGpu`, resolve, memexport) exits the block before
  its pages become GPU-written valid. In GPU order the prologue runs before the resolve, so later
  draws read the resolve output.
- **Texture requests.** A texture request exits the block (immediate `source` exit, as in v1). Its
  later inline upload follows the prologue in GPU order.
- **Converted-index cache.** The primitive processor arms pages itself
  (`primitive_processor.cpp:1371-1382`; its invalidation callback is at `:1443`) and still does so.
  Its entries stay correct because the arm still happens and the fault still fires. The census
  shows this is about 0 calls today.
- **VB request skip.** The VB `vertex_buffers_in_sync_` skip (`command_processor.cpp:4275`,
  `:4296-4300`; reset on fetch-constant writes at `:296` and per swap at `:2221`) is unchanged. A
  fault never cleared it either, so the hot path adds no new skip.

**Granularity: per page, with hysteresis.**
- The v1 per-block rule lost 22 of the 65 h8 blocks to texture watches. The v1 shadow projected
  only 57 % of arms saved, and 42 % were measured.
- Track a per-page history: "re-uploaded (invalid at a VB/IB request) in K of the last N frames",
  with K=6 and N=8 as in v1. This is a better signal than faults, which are widened to the block.
- A page is eligible if no texture or range watch overlaps it (keep a per-page watch count in
  `WatchMemoryRange` / `UnlinkWatchRange`), it holds no GPU-written data, and in the last 64 frames
  it was requested only by VB/IB on the CP thread. That is the v1 foreign hold, applied per page.
- A texture-watched page in the same block stays armed. A fault on it still widens and invalidates
  the block, which is harmless because hot pages are already invalid.
- **Probe:** v1 re-armed every hot block for one window every 64 frames, all at the same time,
  which caused 586-600 probe exits/min. Use 256 frames and stagger the probes by page-group index.

**Caps.**
- v1's per-submission request cap of 256 made 28-41 requests/f fall back. With batching, the GPU
  cost per staged range is small, so cap by staged bytes per submission instead: 2 MB by default.
  The hot set is 16 MB/frame over about 14 submissions.
- An over-cap request takes the normal armed path.

**Expected Deck effect.**

| scenario | CP (arm + upload) | render thread (faults) | GPU |
|---|---|---|---|
| v1 eligibility, batched (42 % of arms, 41 % of faults) | -4 to -5 ms/f | -2.7 ms/f | +0.5 to +1 |
| per-page eligibility (75-85 % of arms and faults) | -7 to -9 ms/f | -5 to -5.5 ms/f | +0.5 to +1.5 |

Notes on the table:
- The memcpy shrinks (byte-exact, de-duplicated) instead of growing.
- Global-lock hold falls by a similar share (arm 8.7 + disarm 3.9 ms/f today), which helps the
  render thread's atomics.
- On fps: the CP chain falls from about 64 ms to about 55-60 ms. Median frames still need 4 vblanks
  (3 vblanks needs less than about 50 ms). Expect 15 -> about 16-17 fps and fewer >100 ms frames.
  Reaching 20 fps also needs lever 2 (CP draw/packet fast paths).

**Risks:**
- **Visual staleness** if any VB/IB consumer relies on a watch rather than a re-request. Known
  consumers re-request per draw, and the model test from v1 must be extended for the prologue
  ordering.
- **KI-047.** Composite and scratch resolve ranges are GPU-written, so they are excluded.
- **KI-033.** The vinyl atlas is texture-watched, so it is excluded per page.
- **Staging memory and an extra command allocator per submission.**
- **Two lists per `ExecuteCommandLists` under vkd3d** (one `vkQueueSubmit` with two command
  buffers) is untested.

### (b) Coalesced protect and deferred re-arm. Second, mostly made moot by (a).

- **Within a call:** nothing is left to gain. Upload-ahead 16 already produces one call per run of
  about 14 pages, and bridging already coalesces the disarms.
- **"Arm once per run per submission":** this breaks protect-before-copy unless it becomes
  "copy -> deferred coalesced protect at submission end -> verify (memcmp guest against the copy)
  -> only then set valid".
  - Until then the pages are invalid. Within the submission they behave like hot pages: a re-copy
    on every request, needing (a)'s staging and batching anyway.
  - After that they are armed until the next lap, when the guest faults again.
- So it cuts arm calls (from about 480/f to roughly one per ring run per submission) but **not the
  faults** (the render thread's 6.6 ms/f), and it adds a verify memcmp.
- Possible use: as an add-on for the warm non-hot remainder after (a), which is 10-25 % of arms.
  Expected gain under 1-2 ms/f. Not planned.

### (c) GetWriteWatch / MEM_WRITE_WATCH: not usable

- It applies only to private `VirtualAlloc(MEM_RESERVE | MEM_WRITE_WATCH)` memory. Guest physical
  memory is one section mapped four times (`<sdk>/src/system/xmemory.cpp:207-230` heap setup), which
  write watch cannot cover on Windows, and Wine follows the same API.
- In Wine, write watches are themselves page protection: a `VPROT_WRITEWATCH` bit, `mprotect`
  read-only, and on a fault `virtual_handle_fault` restores write access inside the signal handler.
  That fault is cheaper than ours because there is no PE exception dispatch, VEH or `NtContinue`,
  but it is still a SIGSEGV plus `mprotect` per first write per page.
- A reset (`GetWriteWatch` with `WRITE_WATCH_FLAG_RESET`, or `ResetWriteWatch`) is again `mprotect`
  over the range.
- Newer Proton may use a kernel-assisted path (userfaultfd write-protect / `PAGEMAP_SCAN`, Linux
  6.7+). That is **unverified** here, and it would still not apply to section views.
- Using it would need private guest memory without the aliases, an architectural change. No gain is
  expected; rejected.

### (d) userfaultfd or soft-dirty bits: not available

A PE program under Proton cannot issue Linux syscalls through any supported interface. Doing it
would need a custom Wine unixlib, which is not possible with stock Proton. Recorded as a
native-Linux research item only, as in the M10 design (f).

### (e) Skipping protection for buffers the GPU reads once per frame

- This is (a) under another name: a buffer that is read and rewritten every frame gains nothing from
  protection and must be re-copied per request.
- Static buffers (read every frame, never written) cost nothing today: no fault and no re-arm. They
  must stay protected so the fast path stays free.
- Without the write-every-frame signal, skipping protection would turn 96 % fast-path hits into
  copies (10,000 requests/f), so the per-page fault/re-upload history is the necessary filter.
  Folded into (a).

### (f) Extension once (a) works: batch normal uploads too

- The normal armed path also does about 476 inline copies per frame, each with a barrier pair.
- These could join the prologue if no draw of the current submission has read their pages. That
  needs a per-submission "GPU-read pages" bitmap, which the fast path would have to set too (about
  10,000 requests/f, a few ns each).
- Expected: the rest of the barrier and replay cost (part of the vkd3d replay thread's 24 % of a
  core), and some GPU time.
- Medium risk. Measure the (a) census first.

### Micro-cuts (no build of their own; fold into (a))

- `ProtectWatchRange` times each call twice, with `steady_clock` plus TSC, and two atomics
  (`<sdk>/src/system/xmemory.cpp:521-544`). Keep the TSC only. Its per-call
  `duration_cast<microseconds>` also truncates, which under-reports `watch_arm_us` by about 3 % on
  the Deck.
- The fault handler's `VirtualQuery` costs 0.3 ms/f.

## 3. Recommendation and staged plan

Recommended: **(a) hot pages v2**, i.e. per-page hysteresis, byte-exact per-submission staging,
and a prologue copy list. Build it on the v1 tracker (`mcla/m10-hotblocks` `cdf469a`) rebased onto
the current SDK line (l80), keeping the v1 cvars' structure.

**Stage 0a: census with no build (owner, Deck usual loop, one run).**
- Run l79 with the dev override `memory_watch_census=true` (startup phase; host key at
  `src/mcla_dev_overrides.h:331`).
- Gives the l79 numbers this design scaled from l66: arm and disarm calls and us per call, arms by
  source, faults by thread, h8 block count and share, h8 blocks with texture watches, upload pages
  by hotness.
- Also extract `MCLA watch protect/min` from the existing four l79 sessions now, to separate "more
  calls" from "dearer calls".
- Census overhead was within noise in l66 (shadow and all arms both about 15.0 fps).

**Stage 0b: new counters in the SDK (default off; read in the stage 1 build's shadow mode).**
- Per-page history: pages re-uploaded in 8/8, 4-7 and 1-3 of the last 8 frames; their arm, fault
  and upload shares.
- In h8 blocks, arms and faults on texture-watched pages against other pages: the v1-to-v2
  eligibility gain.
- Prologue projection per submission:
  - hot requests;
  - byte-exact bytes against page bytes;
  - re-references in the same submission, split into overlap-equal (no copy) and overlap-different
    (inline fallback);
  - merged copy runs;
  - staged bytes (for the cap).
- Run-length histogram of arms and disarms (already in `arm_calls_by_pages`). TLB-IPI proxy:
  `/proc/self/maps` line count at F10, as already proposed in the M10 design.

**Stage 1: implementation behind cvars.**
- `shared_memory_hot_pages` = 0 off / 1 shadow (the stage 0b projections) / 2 on.
- `shared_memory_hot_prologue` = true (mode 2 only; false gives v1-style inline copies, kept for
  A/B).
- `shared_memory_hot_k` / `_n` / `_probe_frames` (default 256, staggered) and
  `shared_memory_hot_stage_cap_kb` (2048).
- Host Wine-only default `shared_memory_hot_pages=2` in `mcla_m10_defaults` next to upload-ahead
  and bridging. Off on Windows, where arm costs 0.7 ms/f and v1 showed no PC gain.
- Unit tests:
  - extend the v1 model (40 seeds x 1,500 frames) with GPU-order simulation of prologue against
    inline copies;
  - negative controls: page staging (overlap misses), no valid-clear at entry (the fast-path
    hazard), and prologue copy after a GPU write;
  - zero stale GPU reads required.

**Stage 2: owner A/B on the Deck usual loop.**
- Arms, all with otherwise identical l79/l80 defaults: off, shadow, on, on, off.
- Read:
  - fps and >100 ms frames;
  - CP busy, upload and draw_regions;
  - CP kernel ms/f;
  - arm and disarm calls and ms;
  - render faults and fault ms; render work;
  - GPU busy (must stay within +2 ms/f; v1 was +10.4);
  - replay CPU %;
  - prologue copies per submission;
  - inline fallbacks;
  - `hot_exit` mix;
  - the staged-cap hits.
- Safety:
  - `memory_watch_protect_failures=0`;
  - no "Recovered stale physical page protection" warning;
  - no "Failed to get an upload buffer";
  - READBACK `guard_misses=0`;
  - KI-047 composite diagnostics unchanged and a police livery lap;
  - garage and driving vinyl (KI-033);
  - traffic, HUD and car geometry visually clean (stale VB/IB shows as flicker or exploded
    geometry).
- Then a PC regression check with hot pages left off (Windows default): mode 0 must be identical to
  l80.

**Acceptance.**
- Deck CP busy at least -4 ms/f.
- Render fault time at least -2.5 ms/f.
- GPU no more than +2 ms/f.
- fps not lower, and >100 ms frames not higher.
- All safety counters clean.
- If the census in stage 0a/0b projects less than -3 ms/f of CP, stop after stage 0 and move to
  lever 2.

## 4. Limits

- The per-source and per-hotness numbers come from l66, not l79 (l79 runs with the census off).
  The l79 arm increase (11.0 against 8.0 ms/f) is unexplained until stage 0a.
- The Wine and Linux mechanisms (VMA splits, `mmap_lock`, TLB IPIs, `virtual_mutex`, the
  write-watch implementation, a possible `PAGEMAP_SCAN` path) are described from general knowledge
  and are not checked against Proton source.
- The savings ranges assume the per-page eligibility recovers most of the h8 blocks that carry
  texture watches. Stage 0b measures that directly.
- GPU cost of the prologue under vkd3d and two lists per `ExecuteCommandLists` are untested.
- Nothing here is a physical PASS.
