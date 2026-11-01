# KI-047 read-back sync cost: targeted wait and early submission (l61 SDK, 2026-11-01)

Implemented by an Opus subagent for the primary agent; SDK build, install and
unit tests only. Nothing was launched (no `mcla.exe`, no launcher), no save,
profile or LocalAppData file was touched, no host candidate was built. Spark:
not used (deterministic source reading of one function family, build and
tests; no log to triage).

**Status: implemented, unit-tested; owner A/B pending. No physical PASS.**

Background: the Deck l56 arm
([M10-001-deck-l56-20261101.md](M10-001-deck-l56-20261101.md), section 5 and
consequence 2) measured the KI-047 composite read-back sync
(`d3d12_readback_sync_composite`, [build doc](M10-001-ki047-sync-fix-build-20261101.md))
at 9.37 ms/frame of fence wait at the title (1 forced wait per frame, rule c)
and 10.64 ms/frame in driving (about 1.8 per frame, 5.6-6.8 ms each), 6,841
waits and 47.2 s per 401 s session, on a CP-bound Deck. Of the read-back wait
(9.45 / 11.01 ms/frame) almost all is the fence wait; `stream_finish_wait` is
0.32 / 0.64 ms/frame for all submissions together (5.1 / 6.3 submissions per
frame).

## 1. What the full drain does (SDK 9d8ede4)

`AwaitAllQueueOperationsCompletion()` is
`CheckSubmissionFence(submission_current_)` plus the check
`submission_completed_ + 1 >= submission_current_`. For a forced resolve the
GPU copy into the resolve's own read-back slot has just been recorded into the
open submission, so the call does:

| Step | Needed by the forced copy? |
| --- | --- |
| `EndSubmission(false)`: end the occlusion query if one is active, `pipeline_cache_->EndSubmission()`, `SubmitBarriers()`, finish the streaming replay (`FinishStreaming`, waits for the replay worker) or replay the deferred list, read-back GPU timers, timestamps, `Close`, `ExecuteCommandLists`, `Signal(submission_fence_, N)` with N = the copy's submission | **Yes**: the copy must be closed, executed and fenced. |
| Out-of-submission queue-operation fence (`queue_operations_done_since_submission_signal_`) | **No**: `EndSubmission` clears the flag when it ends an open submission, so this branch only runs when no submission was open or ending it failed. |
| Wait for fence value `submission_current_ - 1` = N | **Yes**: exactly the copy's own submission. The direct queue executes in order, so it also covers every earlier submission; there is no shorter correct wait on one queue (the copy reads shared memory written by the resolve, which needs every earlier command of the list). |
| Reclaim completed submissions (command allocators, one-use descriptors, deferred deletions, shared memory / render-target / primitive / texture cache `CompletedSubmissionUpdated`, timestamp collection) | Not needed for the copy; cheap; also done by the next `BeginSubmission`. Kept (it is part of `CheckSubmissionFence`). |

Not involved: memexport read-back, upload heaps (shared memory upload
buffers are only reclaimed), resource barriers beyond `SubmitBarriers()`. The
next draw re-opens a submission (`BeginSubmission(true)`, mid-frame: cached
root signature, pipeline, viewport and descriptor heap bindings are reset,
as at every primary-buffer-end submission).

**Finding: the full drain already is the targeted wait.** It ends the
submission holding the copy and waits for that submission's fence value; the
only extra step can never run on this path. A literal "submit up to the copy
and wait for its fence" therefore has the same GPU wait (expected gain about
0). The 9-11 ms is the GPU executing the backlog recorded since the last
submission (plus wake latency), while the CP waits.

## 2. What l61 does

New SDK switches (hot reload, `GPU/D3D12`):

- `d3d12_readback_sync_targeted` (bool, default **false**).
- `d3d12_readback_sync_kick_draws` (int 0..1,000,000, default 512; effective
  only while the targeted switch and `d3d12_readback_sync_composite` are on).

With `d3d12_readback_sync_targeted=true`:

1. **Targeted wait.** A forced resolve with an open submission calls
   `AwaitReadbackCopySubmission`: remembers N = `submission_current_`, ends
   the submission (`EndSubmission(false)`), and, if the index advanced past N,
   `CheckSubmissionFence(N)` (below `submission_current_`: no submission
   ending, no queue-operation fence, only that fence value and the usual
   reclamation). Result (`ClassifyReadbackTargetedWait`): completed (copy
   the own slot, as before), fence failed (`await_failed`, not copied, as the
   full wait does), or fallback: no open submission or ending it failed ->
   `AwaitAllQueueOperationsCompletion()` exactly as before, counted in
   `fallbacks_to_full`.
2. **Early submission ("kick").** Before every non-copy draw
   (`IssueDraw`, before `BeginSubmission(true)`), the open submission is
   executed with `EndSubmission(false)` when all hold
   (`ShouldKickReadbackSyncSubmission`): it already holds
   `d3d12_readback_sync_kick_draws` draws; every earlier submission has
   completed (`GetCompletedValue() + 1 >= submission_current_`: the GPU is
   idle and would otherwise wait for the next submission);
   `CanEndSubmissionImmediately()` (no pipeline being created, the same test
   as the primary-buffer-end submission); no host occlusion query is active
   (ending a submission ends the query, and the guest's end would then fall
   back and disable host queries). The GPU then runs the frame while the CP
   records, so a forced wait only executes the draws since the last kick.

With the switch off nothing changes: the forced path calls the full wait,
the kick returns on its first test, the extra per-draw work is one
increment, and the log lines are byte-identical (the targeted fields are
appended only while the switch is on or has counted in the minute).

Every other resolve (non-forced, misses, `full` mode, memexport) is
unchanged. The Vulkan backend has no read-back sync and is untouched.

## 3. Correctness argument

- **Same bytes, fully written.** The forced resolve still reads its own slot
  (`read_index = write_index`) after the fence of the submission that holds
  the slot's copy has completed; `ReadbackTargetedWaitSatisfied` is the same
  condition as `AwaitAllQueueOperationsCompletion` returns once the copy's
  submission is the last one (unit-tested for completed 38..43 against
  submission 41). The memcpy happens at the same point of the CP packet
  stream as before, so the guest-visible order of the write is unchanged.
- **Kicks add no guest-visible event.** `EndSubmission(false)` writes no guest
  memory or register; mid-frame submissions already happen at every primary
  buffer end (`d3d12_submit_on_primary_buffer_end`, default true) at
  arbitrary packet boundaries, and at every forced or missed read-back. The
  two states that do not survive a submission boundary are excluded: pending
  pipeline creation (same guard as the primary-buffer-end path) and an active
  host occlusion query. Kicks only make earlier submissions complete earlier;
  the delayed `some`/`fast` resolves that read a previous slot can only find
  it more complete, never less.
- **Failure paths** are those of the full wait (fallback or `await_failed`).

### 3.1 Rule c per frame: overlapping the copy with the next frame (not done)

At the title one key is forced every frame. Waiting on the previous frame's
copy fence (consuming one frame later) would need proof that the guest never
reads the composite before the CP reaches a later point. That cannot be
shown: the guest CPU runs asynchronously to the CP and can learn about CP
progress through MEM_WRITE, EVENT_WRITE_SHD/EXT/ZPD, REG_TO_MEM, COND_WRITE,
INTERRUPT, XE_SWAP, scratch-register write-back, the read-pointer write-back
after every primary buffer batch, and MMIO register reads from guest threads
at any time. A one-frame-late copy from the previous slot is exactly the
KI-047 failure mode (a stale composite). Left unchanged. A lazy copy at the
next guest-visible CP event (flush points in the base command processor
before each of the packets above and the read-pointer write-back) would be
order-preserving for well-behaved guests but still not closed against MMIO
polling; not implemented, documented as a later option if the kick does not
remove enough.

## 4. Counters (`MCLA_READBACK_SYNC v=1 kind=minute|mark`, appended)

` targeted= kick_draws= targeted_waits= targeted_wait_ms= targeted_max_wait_ms=
targeted_submit_ms= targeted_fence_ms= targeted_gpu_busy= targeted_backlog_draws=
fallbacks_to_full= kicks= kick_ms=`

- `targeted_waits`: forced resolves that took the targeted wait (completed or
  fence failed); `targeted_wait_ms` / `targeted_max_wait_ms`: their total and
  longest submit + fence time; `targeted_submit_ms`: `EndSubmission` part
  (replay finish, execute); `targeted_fence_ms`: the fence part.
- `targeted_gpu_busy`: of those, the GPU had not finished the previous
  submission when the copy was submitted; `targeted_backlog_draws`: draws in
  the copy's submission (the backlog the fence wait executes).
- `fallbacks_to_full`: forced resolves of the targeted mode that took the full
  wait; `kicks`, `kick_ms`: early submissions and their CP time.
- The existing `forced`, `wait_ms`, `max_wait_ms` still count every forced
  wait (targeted or not), so arms compare on `wait_ms` directly.
- One line `MCLA_READBACK_SYNC v=1 kind=targeted_start|targeted_stop frame=
  kick_draws=` when the switch changes (checked at every frame end and forced
  resolve).

## 5. Tests

SDK (`private/l61-sdk-wt`, branch `mcla/m10-ki047-targeted`, commit
`561a737` from `9d8ede4`; build `private/l61-sdk-b`, output
`private/l61-sdk-o`, install `private/l61-sdk-install`, scripts and logs in
`private/l61-sdk-work`):

- ctest Release: **1985/1985 passed** (l60 baseline 1982 + 3 new cases).
- `[readback-targeted]` 3 cases / 38 assertions: wait classification
  (completed, later completion, fence behind, ending failed), equivalence with
  the full-wait condition, kick decision (each guard alone, threshold edge,
  GPU busy), counters. `[readback-sync]` 11 cases pass; every l60 tag
  (`perf-split`, `pipeline-wait`, `livery-diag2`, `wrm`, `replay-wake`,
  `readback-slots`, `storage-creation`) passes with the l60 counts.
- Compiler warnings: 503, multiset identical to the l60 build (no new
  warning). clang-format clean on every changed line (the files carry older
  unformatted regions, left alone). Sources ASCII.
- Installed: `rexgpu-xenos.dll` SHA-256
  `57D30B4FF2D8A33C4A39F8B9307B364F8C3710E2A5C7DBDB47C2485B0A56BA98`
  (carries the new cvars), `rexruntime.dll`
  `4BAF7BA7D84F2D999A53FD3B30AA60E5A0797A2434E25E7BF881A75ECBF4C19F`,
  `TracyClient.dll`
  `D42EEAEA72DC1A133EA5B1AF2FB6C48693CDA650E0CF3C13151CD2554A7395D0`.
  D3D12-only configuration (as l58/l60); a Deck Vulkan-capable SDK would need
  the l59vk configuration.

Host (`src/mcla_dev_overrides.h`, `tests/mcla_dev_overrides_test.cpp`): the
override-file whitelist gains `d3d12_readback_sync_targeted`
(`Phase::kGraphicsPlugin`, applied once the plugin registered it, like
`d3d12_readback_elide_unread`). The integer `d3d12_readback_sync_kick_draws`
is not whitelisted (the file format is boolean only); the build default 512
applies. Standalone compile of the test against the patched header: 106
checks passed (98 before).

## 6. Expected effect (estimate, not measured)

- Targeted wait alone (`d3d12_readback_sync_kick_draws=0`): about 0 (same
  GPU wait as the full drain).
- With the kick (default): the fence wait shrinks to the GPU time of the
  draws since the last kick plus wake latency, whenever the GPU was idle
  during recording. Deck: title 5.4 k draws per frame and 30 ms GPU busy per
  frame, so 512 draws are about 3 ms of GPU work; the 9.4 ms/frame title wait
  could fall to about 2-4 ms and the 10.6 ms/frame driving wait to about 4-6
  ms, i.e. about 5-7 ms/frame back to the CP (+6-9% guest frames per second
  while the CP is the wall, before vsync quantization). Cost: up to about 10
  extra submissions per frame at roughly 0.1-0.2 ms CP each on the Deck
  (replay finish, execute, re-binding), visible as `kick_ms`, plus more work
  on the vkd3d submission thread of an oversubscribed CPU. If
  `targeted_gpu_busy` is close to `targeted_waits`, the GPU was not idle and
  the kick cannot help (the wait is real GPU work).
- PC: the read-back sync waits are short (GPU fast); small gain expected,
  mainly a regression check.

## 7. Owner A/B (needs a host build against l61 with MCLA_DEV_OVERRIDES=ON)

Same override file, one line changed between arms
(`%LocalAppData%\MCLA-R-Launcher-v1\mcla-dev-overrides.txt`):

- Arm TARGETED: `mcla_readback_sync_composite=true` and
  `d3d12_readback_sync_targeted=true`.
- Arm FULL: `mcla_readback_sync_composite=true` and
  `d3d12_readback_sync_targeted=false`.

1. PC: KI-047 trap laps (police livery cases of the l54/l55 runs) in each arm;
   check the livery renders correctly, `composite_read_prev=0`,
   `composite_not_copied=0`, `forced_wait_failed=0`, `fallbacks_to_full=0`.
2. Deck: title idle about 60 s and the driving loop, sync on, TARGETED versus
   FULL (ABBA if time allows). Compare `MCLA_READBACK_SYNC` `wait_ms` per
   frame and `targeted_fence_ms`, `kicks`, `kick_ms`, the perf-split
   `readback_wait` / `fence_wait` / CP busy, guest frames per second
   (KI-059), and the police livery in F10 frames.
3. Accept only if the livery is correct in both arms and TARGETED lowers
   `wait_ms` without raising CP busy by more than it saves.

## 8. Risks

- Kicks change submission boundaries for every frame while on (more
  `ExecuteCommandLists`, `FinishStreaming` waits for the replay worker at
  each kick; on the Deck the replay worker is slower). Measured by
  `kick_ms`; `d3d12_readback_sync_kick_draws` can only be changed by the
  command line or config, not by the launcher override file.
- A guest with long occlusion queries or constant pipeline creation gets few
  kicks (guards), so less gain.
- Pre-existing, unchanged: if `EndSubmission` fails (no command allocator),
  the full wait returns true after waiting for the previous submission
  without the copy having executed (the slot may be stale). The targeted mode
  falls back to the same full wait in that case (`fallbacks_to_full`), so the
  behaviour is identical; it never happened in the l56 log
  (`forced_wait_failed=0`).
