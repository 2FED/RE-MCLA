# M11-007e: deferred EVENT_WRITE_SHD fence writes (experiment arm) and KI-071 frame-walk fix default on (SDK)

Implementation by an Opus subagent on 2026-11-07 as SDK commits `f6fc975` (xma_frame_walk_fix default true) and `590c484` (`d3d12_fence_write_deferred`, default false) on local branch `mcla/m11-fence` (parent `a4a5309`); accepted by the primary agent as code: ctest 2132/2132 (4 skipped), no new warnings. Installed as `private/l77-sdk-install` 0.20.0.12-dev.g590c484: rexruntime.dll 4e1d2cbfe76851758365d9d8417168e6d51eff8aee58cbdb6e0791e8735bb084, rexgpu-xenos.dll 41c8f2b3d130312b7487282c20de9766a6132592ecf89481501bb05b72ef18a4, TracyClient.dll ba2b84e14ccfad23616932eb233ff5bc1e35ba69a06bc8c59095edd994a0a4e2. Note: arms that relied on the old walk default now need `xma_frame_walk_fix=false` explicitly for a control.

---

# M11 l77-sdk-fence: draft evidence notes (Opus subagent, 2026-11-07)

SDK worktree `private/l77-sdk-fence-wt`, branch `mcla/m11-fence` from `mcla/m11-steady3` a4a5309 (thirdparty
copied from l76-sdk-steady3-wt). Build `private/l77-sdk-fence-b`, output `private/l77-sdk-fence-o` (Release, Ninja
Multi-Config, clang 20.1.8, VS 2022 BuildTools, flags as l76). Scripts: `%TEMP%\claude\C--BDU-MCLA-Recomp\l77-fence-scripts`
(build.ps1 / test.ps1 / install.ps1 adapted from l76-steady3-scripts; the python edit scripts ed_*.py).
Installed into the new `private/l77-sdk-install` (install.ps1 reconfigures with that prefix first).

Commits (author Bolodymyr <vovan00282@gmail.com>, not pushed):
- `f6fc975` audio: KI-071 xma_frame_walk_fix on by default
- `590c484` gpu: M11-007e deferred fence writes (d3d12_fence_write_deferred)

Version string `0.20.0.12-dev.g590c484`. SHA-256 (installed = build output):
- rexruntime.dll   4e1d2cbfe76851758365d9d8417168e6d51eff8aee58cbdb6e0791e8735bb084
- rexgpu-xenos.dll 41c8f2b3d130312b7487282c20de9766a6132592ecf89481501bb05b72ef18a4
- TracyClient.dll  ba2b84e14ccfad23616932eb233ff5bc1e35ba69a06bc8c59095edd994a0a4e2
The install also re-registered rexglue in the CMake user package registry (-> l77-sdk-install), as the l75/l76
installs did.

## A. xma_frame_walk_fix default true (f6fc975)

- `src/audio/xma_context.cpp`: default `true`; help: "KI-071 (on by default since l77, verified fixed) ...; false
  restores the upstream walk. In MCLA_XMA_SIB, walk_split_lost counts unclaimed split tails (unconfirmed: real or
  padding); the real loss is walk_split_real - walk_split_claimed - walk_split_counted".
- `frame_walk.h`: declaration comment and the `split_lost` counter comment say the same (field names unchanged, so
  the MCLA_XMA_SIB line format is unchanged; `walk_fix=1` now appears by default).
- No existing test asserted the default; new test "XMA walk: the frame-walk fix is on by default" (registry
  default_value == "true", trace stays "false", ResetToDefault gives true).
- Host side: arm files that set `xma_frame_walk_fix=true` keep working; an arm that wants the old walk must now
  write `xma_frame_walk_fix=false` explicitly (BASE / WALK-TRACE arms that rely on the default being false
  change meaning).

## B. Deferred fence writes (590c484)

### Fence semantics

Xenos EVENT_WRITE_SHD (`0xC0025800` from InsertFence `sub_82411CD0`): the GPU stores the value (or the vblank
counter, bit 31 of the initiator) to the address when the pipeline reaches the event, i.e. after all earlier
work. The guest's fences (BlockOnFence ring-space and frame-throttle waits `sub_82411E98`/`sub_82412F98`,
resource Fence/ReadFence, LockRect fence waits) all read that word. The host CP stored it when it parsed the
packet, so with the CP 1-2 frames ahead (exempt steady key, no rule-c wait) the guest saw fences for GPU work
that had not run.

### Implementation

- Base `CommandProcessor` (`command_processor.h/.cpp`): new virtual hooks with no-op defaults
  `DeferGuestFenceWrite(address, value)`, `PollDeferredMemoryWrites(waiting)`,
  `WaitIdleForDeferredMemoryWrites(wake)`, helper `StoreDeferredGuestWrite` (store + trace write), flag
  `deferred_memory_writes_pending_`. EVENT_WRITE_SHD calls `DeferGuestFenceWrite` after computing the swapped value;
  false -> the old store. ExecutePacket polls at every packet boundary while the flag is set; the WAIT_REG_MEM poll
  lambda polls (waiting = false on the first poll, true once the packet really waits); the idle loop, past its
  500-spin phase, lets the backend do the wait while the flag is set.
- Pure part `include/rex/graphics/util/deferred_fence_writes.h`: `DeferredFenceWriteQueue` (FIFO deque, Push /
  Flush(completed) / counters), `DeferredFenceTargetSubmission` (open ? submission_current : submission_current - 1;
  0 = never submitted, already complete), `ShouldDeferFenceWrite(enabled, queue_empty)` (= enabled || !empty),
  `PlanDeferredFenceProgress`, `FormatDeferredFenceFields`.
- D3D12 (`d3d12/command_processor.cpp`): cvar `d3d12_fence_write_deferred` (bool, default false, hot reload).
  - Event: if `ShouldDefer` -> Push {addr, value, target, frame, us}. Push stores at once (returns false) only when
    the queue is empty and the fence already reached the target (counted `fence_immediate`).
  - Flush: stores every queued write whose target <= completed, in order, stops at the first undue one.
  - Service(waiting, may_submit): GetCompletedValue (non-blocking) -> flush -> if may_submit and the plan says
    kEndSubmission and CanEndSubmissionImmediately (no pipeline creation in flight) -> EndSubmission(false)
    (`fence_early_submits`) -> flush again.
  - Call sites: packet boundary (flush only, never submits mid-stream); IssueDraw before BeginSubmission (may submit
    if the GPU is idle); WRM poll after the first (waiting: may submit); OnIdlePoll every idle iteration (waiting);
    idle wait (see below); CheckSubmissionFence after `submission_completed_` is updated (every synchronous wait,
    incl. the forced read-back waits, AwaitAllQueueOperationsCompletion, reclamation in BeginSubmission); shutdown
    after AwaitAllQueueOperationsCompletion stores everything left (UINT64_MAX) and clears.
  - Idle wait (`WaitIdleForDeferredMemoryWrites`): service; if the head is submitted: `SetEventOnCompletion(head,
    fence_defer_event_)` then `WaitForMultipleObjectsEx({fence_defer_event_, write_ptr_index_event_}, any, 5 ms,
    alertable)`; if it is still in the open submission (could not end it yet): 1 ms wait on the ring event; service
    again; return true (the base loop skips its own 5 ms ring-only wait). Dedicated auto-reset event; late signals
    of earlier registrations only cause an extra poll.
  - Occlusion query active while the head waits in the open submission: a busy draw boundary does not submit;
    while waiting the submit is delayed up to 2 ms (`kFenceDeferQueryGraceUs`), then done anyway (EndSubmission
    closes the query, which can later make EndGuestOcclusionQuery fall back / disable host queries; preferring
    that over a deadlock).
  - Hot toggle: turning the cvar off with writes queued keeps queueing new ones behind them until the queue drains
    (no fence overtakes an earlier one); afterwards the plain path.

### Deadlock analysis

- The guest render thread may spin (or block) on a queued word: ring-space waits, frame throttle, BlockOnFence,
  LockRect. While it spins it may submit nothing more, so the CP runs dry.
- CP idle (ring empty) with writes queued: every idle iteration ends the open submission if the head is in it
  (only obstacles: pipeline creation in flight - finishes on its own threads; an occlusion query - 2 ms grace),
  then waits on the GPU fence (or new ring data), bounded at 5 ms, and flushes. The GPU completes submitted work
  without CP help, so the write lands. Unit test "idle loop never strands a queued write" models it.
- CP in WAIT_REG_MEM on guest memory/registers that the guest writes only after its fence: from the second poll
  on, the poll ends the open submission when the head is in it and flushes due writes on every poll. The WRM
  block's own timeout (wait/0x100 ms) bounds the poll period.
- CP blocking waits that remain (CheckSubmissionFence INFINITE, pipeline waits, swap/presenter): they wait only
  for the GPU or host threads, never for the guest, so they cannot form a cycle with a guest fence spin; queued
  writes are flushed right after a fence wait returns.
- No lock is involved (everything on the CP thread).
- Not covered: a guest that waits for a fence and expects the CP to make progress through a path other than the
  ring/WRM/idle loop (none known).

### Pacing expectation

With the mode on the guest's ring/frame waits wait for the real GPU as on hardware: the CP can no longer run
1-2 frames ahead through fences. Expect part or all of the steady-exemption gain (40-41 vs ~33 fps base minutes)
to be given back when the GPU is the bottleneck; frame-throttle waits move from "never" to "GPU-bound". Early
submits at idle/WRM/GPU-idle split submissions into more, smaller ones (more ExecuteCommandLists, less batching).
Watch `fence_early_submits` per minute vs frames, `fence_deferred_max_frames` (should be <= 2) and
`fence_deferred_ms_max`.

### Counters and lines

- On the always-on `MCLA readback` minute/mark line (ReadbackCountersReport), appended only while the cvar is on
  or something was counted / is queued (unchanged otherwise):
  `fence_defer fence_events fence_deferred fence_immediate fence_written fence_deferred_max_frames
  fence_deferred_ms_max fence_flush_on_idle fence_early_submits fence_queue_max fence_queue_now`.
  `fence_flush_on_idle` = queued writes stored by an idle-loop or waiting WRM poll. Counters reset per minute
  (the mark line peeks).
- `MCLA_READBACK_SYNC v=1 kind=fence_defer phase=start|stop` on toggle (seen at the next fence event), and for the
  first 8 queued writes after each enable a `phase=parse` line (seq, frame, addr, value, submission, current,
  open, completed, queue) and a `phase=write` line (frames, us, waiting, queue_left).

### Other completion-type writes reviewed (unchanged)

- EVENT_WRITE_EXT: writes constant fake screen extents; no GPU-dependent content; left at parse time.
- EVENT_WRITE (no address) / INTERRUPT: no memory store / CP-time callback.
- MEM_WRITE, COND_WRITE, REG_TO_MEM: micro-engine (CP-time) writes on hardware too; unchanged. Note: a MEM_WRITE to a
  word that also has a queued fence store would now be overwritten later by the older fence value; this is
  hardware-like (ME writes are not ordered after pipeline events) and not seen in MCLA's fence code.
- EVENT_WRITE_ZPD: D3D12 host occlusion-query path, own readback handling; unchanged.
- Memexport results: `d3d12_readback_memexport` default false (not read back to guest RAM); unchanged.
- Resolve (RB_COPY) read-back: delivered by the existing read-back slot logic (forced/fast/some, ensure API);
  NOT tied to the deferred fence. With the steady exemption a fence can now land after the GPU copy but before the
  CP's memcpy of that slot into guest RAM (next resolve of the key). Possible follow-up (RE doc step 2): at flush
  time deliver pending read-back copies (readback_pending_) whose submission <= the fence's target before storing
  the fence. Not done here.

### Tests

- Baseline a4a5309: 2126 total (2122 passed, 4 skipped). After both commits: 2136 total, 2132 passed, 4 skipped,
  0 failed (ctest -C Release -j 8): +1 xma default test, +9 deferred_fence_writes_test (target submission, cvar
  off, immediate store, completion gating and order, no overtaking, latency counters and fields, progress plan,
  idle model, line budget).
- Cvar off: no Push, no flag, no polls, no line fields: the EVENT_WRITE_SHD store, idle loop, WRM and readback line
  are behaviourally identical (the WRM poll lambda gains a bool; the event pays one cvar read).
- Warnings: incremental-build warning messages (line numbers stripped) are a subset of the full base build's set
  (the first incremental build showed 4 unused-parameter warnings from the new base virtuals; fixed by unnamed
  parameters). clang-format-diff on all changed lines: clean; new files fully clang-formatted. ASCII, LF.
- Every build/ctest/install was preceded by wait-for-idle-pc.ps1 (IDLE each time). Game/launcher never launched.

### Risks

- Pacing/fps loss (above); more, smaller submissions.
- Occlusion queries ended early by the idle grace path (host query fallback/disable) if the guest waits on a fence
  inside a query span.
- Idle wake latency: busy spin first (immediate), then 5 ms bounded fence/ring waits; a WRM block can delay a flush
  by its own timeout.
- Not runtime-tested: the deadlock analysis and pacing are offline only; owner A/B (South Central Tour, steady arm
  with and without `d3d12_fence_write_deferred=true`) required. `fence_deferred_max_frames` > 2 or a hang would
  point at a missed service point.
- Does not by itself fix stale read-back data for fence-synchronised CPU readers of exempt keys (see the resolve
  note).

## Spark

No Spark job: implementation in a small C++ file set with deterministic gates (build, ctest, clang-format); no
bounded extraction or log triage whose validation would cost less than doing it.
