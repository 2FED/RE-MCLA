# Review: SDK ff30a86 (mcla/m10-hotblocks, parent 43577c8 = v0.20.0.7): M10 watch step 3, hot blocks

Scope: a read-only review of `git show ff30a86 --ignore-submodules=all` in `private/l66-sdk-wt`. I
also read the code around the change:
- `shared_memory.cpp` in full
- d3d12 `shared_memory.cpp` `UploadRanges`
- in d3d12 `command_processor.cpp`: `IssueDraw` (the vertex, memexport and buffer-state transitions)
  and `EndSubmission` (the frame-end hook)
- texture cache request and watch order (`pipeline/texture/cache.cpp`)
- `primitive_processor.cpp` (the index request and its own arming)
- `xmemory.cpp` `EnablePhysicalMemoryAccessCallbacks`
- the design notes in `M10-hot-blocks-sdk-20261103.md`

I did not build, run tests or launch anything. Line numbers refer to the ff30a86 tree.

## Verdict

**No blockers.**

**Defaults (mode 0) are safe for the next owner build.** Apart from counting and bookkeeping,
control flow, validity bits, protect/arm calls and fault results are identical to 43577c8 (see N1).

**Mode 2 is safe enough for an owner Deck A/B behind the switch.** A mode 2 request never sets a
valid bit without arming the page. Every exit only clears valid bits. So the invariant "valid CPU
page ⇒ armed" holds in every interleaving I could construct. The set of pages whose GPU copy can be
stale is the same as on the normal path: pages written after their last upload. The only exception
is the upload-ahead side effect in N3, which is not a correctness mechanism.

The main open risk is performance, not correctness. Each hot request records its own copy and a
buffer-state transition, and index requests happen on every draw (S2). Measure this on the Deck before
judging the CP-ms gain.

## Blockers

None.

## Should-fix (not blocking the A/B)

**S1. The documented "foreign" rule does not match the code for other-thread requests.**
- Where: `include/rex/graphics/util/hot_blocks.h:16-17` and the evidence doc say "any request from
  another thread" makes a block foreign. In the code (`shared_memory.cpp:890-899`), `hot_mode` is
  forced to `kOff` off the CP thread, so `NoteRequest` is never called and no immediate exit happens.
- Correctness is unaffected. Such a request takes the normal path, which sets valid and arms. That
  creates a valid+armed page inside an active block, and later mode 2 requests skip it (fast path or
  outside the upload ranges) until a fault. Exit restore clears it while the page stays armed, so
  the result is redundant uploads, never stale ones.
- What goes wrong: a block that another thread also requests can stay hot and never record a
  `source` exit or hold. The A/B log then reads differently from the documented rule.
- Fix: correct the comment and the doc. Do not call `NoteRequest` from other threads: its
  per-block fields are non-atomic and written by the CP thread without the lock.

**S2. Copy and barrier count per hot request: the cap bounds bytes, not calls (performance, Proton).**
- Where: `shared_memory.cpp:1047-1066` and d3d12 `UploadRanges`.
- Every hot request calls `UploadRanges`, which does
  `CommitUAVWritesAndTransitionBuffer(COPY_DEST)`, then `SubmitBarriers`, then one or more
  `CopyBufferRegion`. `IssueDraw` then transitions back (`UseForReading`, `command_processor.cpp:3725`
  and `3784`).
- Index buffers are requested on every draw (`primitive_processor.cpp:930`, "TODO: request cache").
  So every draw whose index buffer sits in a hot block gets a copy, a read→COPY_DEST→read transition
  pair, and a new upload-pool chunk.
- With 4 KiB requests, the 4096 KiB cap still allows about 1,000 copies per submission.
- Under vkd3d-proton, a transfer between draws ends the Vulkan render pass and adds a pipeline
  barrier. The normal path pays this only when something was actually invalidated.
- Failure scenario: CP-side arm and fault time drops as designed, but GPU time or CPU submit
  overhead rises from render-pass splits. The fps change is then a net of two effects, and the
  current counters do not separate them.
- Fix (cheap, before or with the A/B): report `hot_requests` per frame next to the existing
  per-frame figures, and consider also capping the number of hot requests per submission. Correctness
  of the barriers is fine: same-range re-upload after an earlier draw is ordered by the same
  transitions the normal path already exercises.

**S3. Formal data race on the request flags.**
- Where: `shared_memory.cpp:541-543` reads `hot_request_on_` and `hot_request_shadow_` before
  calling `OnCommandProcessorThread()`. These are plain members written by the CP thread
  (`:1096`, `:1110`).
- Any `MakeRangeValid` on another thread, such as a GPU-written mark if one is ever issued off the CP
  thread, races on them. This is undefined behaviour in C++ but harmless on x86/MSVC/clang today.
- Fix: test the thread first, or make the two flags `thread_local`. Either is a one-line change.

## Notes (verified OK, or for interpreting the A/B)

**N1. Mode 0 is equivalent to 43577c8, path by path.**
- **RequestRanges:** adds `OnCommandProcessorThread()` per request, before the fast path. That is a
  relaxed atomic load plus `GetCurrentThreadId`, which is a TEB read on Windows and under Wine, so
  no syscall. `hot_.mode()` is `kOff`, so nothing else runs. The exit and charge blocks are gated off,
  `ExtendUploadRangesLocked(..., false)` behaves as before, and the flags are written false around
  `UploadRanges`. There are no early returns between setting and resetting the flags.
- **MakeRangeValid:** `arm_runs` is a single run `[valid_page_first, valid_page_last]`. Valid bits,
  GPU-written bits, `CountValidPagesLocked`, and the single `EnablePhysicalMemoryAccessCallbacks`
  call all have identical arguments. The arm result pointer is unchanged (`hot_shadow` is false).
  The `thread_local` vector allocates once per thread, on its first `emplace_back`, under the lock.
  That is negligible.
- **Invalidation callback (fault path):** `NoteFault` returns at once when off. Nothing in the VEH
  path allocates, logs or throws.
- **WatchMemoryRange / UnlinkWatchRange:** the per-block counts are maintained in every mode, under
  the global lock (FireWatches → Unlink is under the lock), with a few block iterations per watch.
  The counts stay consistent through ClearCache and Shutdown because both go through FireWatches →
  Unlink. The decrement is saturating.
- **RangeWrittenByGpu / memexport:** `active_count()==0`, so no exit happens.
- **Frame end:** the off→off case takes no lock and only advances `frame_`.
- **ClearCache, InvalidateAllPages, staging swap:** untouched.
- **Lock order:** no new locks. Everything new runs under the existing recursive global region,
  including `MakeUpToDateAndWatch`, which already holds it.

**N2. Vulkan never activates hot blocks.** Only D3D12 `EndSubmission`
(`command_processor.cpp:5088`) calls `WatchCensusFrameEnd`. So under Vulkan `census_thread_id_`
stays 0 and the tracker stays off. This matches the evidence doc.

**N3. A pre-existing upstream request cache bounds the "every request uploads" premise.**
- Where: d3d12 `command_processor.cpp:3600-3603`, from upstream e6407f3. `vertex_buffer_states_`
  skips `RequestRange` whenever a slot's address and size are unchanged. Only `ClearCaches` resets it;
  `IssueSwap` clears only the in-sync bits.
- So a VB at a stable address is not re-requested in later frames on either path. The GPU keeps the
  bytes from the last upload of those pages in both modes, so this is equivalent, not a regression.
- One difference: in a hot block the hot path skips the upload-ahead extension (`:747`). On the
  normal path that extension can incidentally refresh invalid neighbour pages, which a cached-slot VB
  would then read fresh. In a hot block it stays stale until some request touches it.
- This is theoretical and not a hot-mode bug, because upload-ahead is not a correctness mechanism.
  But the stable-address VB skip itself deserves its own KI check. Stale geometry from it would look
  the same as a hot-mode artefact in the visual checks.

**N4. Guest write between the hot copy and GPU execution.**
- Both paths deliver the bytes as they were at request time:
  - Normal path: arm before memcpy (`UploadRanges` calls `MakeRangeValid` per chunk before
    `memcpy`). A later write faults and invalidates the page, but the recorded copy already carries
    the earlier bytes.
  - Hot path: the same recorded copy, and no fault.
- A second request of the same range in the same frame gets the current bytes on both paths. The
  normal path re-uploads only if a fault invalidated the page; the hot path always re-uploads.
- Copy-before-draw ordering in the same deferred command list and submission is the same code as the
  normal path.
- A partly hot, partly normal request is split into arm runs at block boundaries (`:560-590`). I
  checked the run bounds: the first-block case and the final run to `valid_page_last` are correct.
- Pool chunking is re-evaluated per `MakeRangeValid` chunk, so an exit between chunks just turns the
  later chunks normal.
- A request over the cap takes the whole normal path. This leaves valid+armed pages inside an active
  block, which is the mixed state from S1 and is correct.

**N5. KI-047 and KI-033 exclusions hold under all the orderings I checked.**
- **Resolve:** a `kResolve` request exits the block (foreign, before its uploads are computed). Then
  `RangeWrittenByGpu` exits and restores under the lock (`:554`) before setting the GPU-written bits.
  Any hot copy recorded earlier in the submission precedes the resolve on the GPU timeline. After
  that, the GPU-written bits exclude re-entry.
- **Memexport draw:** vertex requests (`:3600`) come before the memexport request (`kMemexport`, a
  foreign exit), then the draw, then `RangeWrittenByGpu`, which matches the normal-path order.
- **Texture:** `RequestRanges(kTexture)` comes before `MakeUpToDateAndWatch`
  (`texture/cache.cpp:579`, `:699`). The request exits the block on the slow path. On the fast path
  the pages are already valid+armed, and the watch registration then exits the block.
- **Atlas between a watch firing and the next use:** the block can only go hot if VB/IB also hit it
  and no texture request happened within 64 frames. The next texture request then exits before
  uploading.
- **Restore** keeps valid GPU-written pages (`valid &= gpu_written`). Clearing valid bits for
  non-resident sparse pages is harmless.

**N6. The whole-buffer memexport fallback (`command_processor.cpp:3899`) is safe but disables hot
mode for a long time.**
- It exits every block.
- It then marks every page valid, GPU-written and armed. Fault widening stops at GPU-written pages,
  so blocks become eligible again only after every page has been CPU-faulted individually.
- In the A/B, a large `exit gpu:` count with `hot_blocks_now=0` points to this fallback.
- Staleness at the fallback is the same as on the normal path: pages written after their last upload.

**N7. The double-buffered valid flags remain safe.** This covers `clear_memory_page_state` and the
ClearCache swap. Restore only clears bits in the active buffer and does not mark dirty blocks. A
stale "valid" bit in the older staging copy belongs to a page that was valid, and therefore armed,
when that buffer was last active. Unprotecting a page always goes through a fault, which marks the
block dirty. The hot path never disarms anything.

**N8. Probe and flap behaviour (question 4).**
- Under mode 2, an active block requested in K of N frames stays hot even if the guest has stopped
  writing it. That lasts up to `probe_frames` (64), or forever with 0. Correctness is unaffected;
  the cost is one copy per request.
- After a probe exit the history is cleared, so re-entry needs at least K real fault frames. The
  cycle is therefore at least 64+6 frames.
- A block written every frame but requested in fewer than K of N frames flaps: it enters, has a
  `cold` exit after a few frames, then re-enters. Each cycle costs a restore plus one normal
  upload+arm, about the normal path's cost, so there is no pathology. `hot_enter` against
  `exit cold:` shows it.
- Mode 1 can flap every frame at the K threshold. That is harmless because mode 1 exits do nothing.

**N9. Lock-hold cost.**
- Modes 1 and 2 add a frame-end walk over 2,048 `Block` entries (~40 KB) under the global lock
  (`:663`), a few µs per frame.
- The mode 2 additions inside `RequestRanges` are O(blocks touched).
- There is no foreign-hold scan; it is a per-block compare at frame end.
- The cost the mode 2 request path adds is `NoteRequest` per merged range, plus the fact that hot
  requests by design never take the fast path.

**N10. Smaller points.**
- `shared_memory_hot_upload_cap_kb=0` means every hot request counts as a cap hit. That is
  documented.
- Mode 1 projections read `Active` outside the lock. This is a benign counting race.
- `hot_shadow_saved_arms` is attributed by the first page's block (documented).
- On a shadow→on change, blocks may re-enter at the same frame end, which is correct.
