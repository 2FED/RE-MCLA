# M10 CPU waits: WAIT_REG_MEM event wake and batched replay-worker wake (SDK only, 2026-11-01)

Status: **SDK implemented, built and unit-tested; not run in the game; no host
build.** Implemented by an Opus subagent for the primary agent (tasks 2 and 6
of [M10-cpu-targets-20261101.md](M10-cpu-targets-20261101.md), M10-CPU-B and
M10-CPU-F). Both behaviour switches default **off**; the new counters are
always on. No physical result exists.

## 1. SDK

- Repository `private/l39-sdk-src`, new local branch `mcla/m10-cpu-waits`
  from the published v0.20.0.6 commit `483458d`, linked worktree
  `private/l57-sdk-wt` (submodule trees copied from `l39-sdk-src`; gitlinks
  identical to `483458d`). One local commit
  **`62cd66df49a332a4414960ced19bbcbe146141a2`**, author Bolodymyr, not
  pushed. 11 files, 1062 insertions, 70 deletions.
- Files: new `include/rex/graphics/util/wait_reg_mem_stats.h` and
  `include/rex/graphics/util/replay_wake.h` (pure helpers),
  `tests/unit/graphics/wait_reg_mem_stats_test.cpp`,
  `tests/unit/graphics/replay_wake_test.cpp`; changed
  `src/graphics/command_processor.cpp` and `include/rex/graphics/command_processor.h`
  (cvar, event, WAIT_REG_MEM loop, counters), `src/graphics/graphics_system.cpp`
  (signal after interrupt dispatch), `src/graphics/d3d12/command_processor.cpp`
  (minute and F10 lines), `src/graphics/d3d12/deferred_command_list.cpp` and
  its header (cvar, wake batching, spin accounting), `tests/unit/CMakeLists.txt`.
- clang-format 20.1.8 clean on the changed lines and the new files; sources
  ASCII with LF line ends like their neighbours.
- Scripts and logs: `private/l57-sdk-work/` (the `private/l57-work` folder
  belongs to the separate l57 host build and was not used).

## 2. Task 2 (M10-CPU-B): WAIT_REG_MEM

### 2.1 Design

- The poll loop of `CommandProcessor::ExecutePacketType3_WAIT_REG_MEM` is now
  `RunWaitRegMemLoop(poll, block, on_block, running)` (header template): the
  predicate is polled first and again after **every** blocking step, so a wake
  never skips a re-check, and a stale or early signal costs one extra poll.
  Predicate, endian swap, `COHER_STATUS_HOST`/`MakeCoherent`, `PrepareForWait`,
  `SyncMemory`, `ReturnFromWait`, the `worker_running_` exit after a long wait,
  the short-wait `MaybeYield` and the vsync-off `MaybeYield` are unchanged, as
  is the `wrm_reg`/`wrm_mem` time fed to `MCLA_PERF_SPLIT`.
- With `vsync` on and `wait >= 0x100`, the switch selects the blocking step:
  off = `Sleep(wait / 0x100 ms)` (old); on = `Wait(auto-reset event, timeout =
  wait / 0x100 ms)`. The old sleep is the upper bound of each event wait, so a
  writer that does not signal costs no more than before.
- Signal: `GraphicsSystem::DispatchInterruptCallback` calls
  `CommandProcessor::NotifyWaitRegMemWake()` after `ExecuteInterrupt` returns
  (this covers `MarkVblank` on the "GPU VSync" thread, which dispatches through
  it). It sets the event only while the switch is on, and not when called on
  the CP thread itself (the CP's own `INTERRUPT` packets run the callback
  before its next poll; a signal there would only be stale). Auto-reset plus
  the re-check loop: a signal set before the wait is kept by the event and
  returns the wait at once; coalesced signals still lead to a re-poll that
  sees every write made before the last `Set`.
- Ordering: the guest callback's write precedes `SetEvent` on the signalling
  thread; `WaitForSingleObjectEx` and `SyncMemory` precede the next poll.

### 2.2 Switch

`d3d12_wrm_event_wake` (bool, category GPU, **hot reload**, default false).
Defined in the base command processor (the handler is not D3D12-specific);
read at every blocking step and in every interrupt dispatch. A change is
logged once on the CP thread as `MCLA WRM event wake: on|off` (only when a
vsync blocking step happens).

### 2.3 Counters (always on)

One line per pacing minute (with the `MCLA frame pacing/min` block, whether
or not `d3d12_perf_split` is on) and one at every F10 (`kind=mark`,
counts of the current minute so far, not reset):

`MCLA_WRM v=1 kind=minute|mark reason= frame= event_wake=0|1 packets=
immediate= waited= aborted= failed_polls= yields= sleeps= event_waits=
event_wakes= event_timeouts= stale_wakes= signals= wait_ms= blocked_ms=
max_wait_ms= h_lt0_5ms= h_0_5to1ms= h_1to2ms= h_2to4ms= h_ge4ms=
predicates= other_episodes= top=mem|reg:<address> top_func= top_ref=
top_mask= top_episodes= top_wait_ms=`

- `packets`: all WAIT_REG_MEM packets; `immediate`: matched at the first
  poll; `waited`: matched after at least one failed poll (an episode);
  `aborted`: left because the CP stopped.
- `failed_polls`; `yields` (MaybeYield, short waits and vsync off);
  `sleeps` (old Sleep steps); `event_waits` = `event_wakes` +
  `event_timeouts`; `stale_wakes`: signalled wakes after which the predicate
  still failed; `signals`: event sets by interrupt dispatch in the window.
- `wait_ms`: total episode wall time (first failed poll to match, the same
  quantity as perf split `wrm_reg + wrm_mem`); `blocked_ms`: time inside
  Sleep/event waits; `max_wait_ms`; histogram of episode times.
- Top predicate: the awaited location with the most waited time among up to 8
  distinct predicates (`address` is the raw poll address with the endian bits
  for memory, the register index otherwise; the M7 census predicts
  `top=mem:1FCA3006 top_func=3 top_ref=00000000 top_mask=FFFFFFFF`);
  `other_episodes` counts episodes of further predicates.
- Cost: plain integer updates on the CP thread per packet, one steady-clock
  read pair per blocking step (Sleep or event waits); none on immediate
  matches.

## 3. Task 6 (M10-CPU-F): replay worker

### 3.1 Design

- `ReplayWakeBatcher` (pure), fixed per submission at `BeginStreaming` from
  the switch. After each publish (visibility granularity unchanged: every 32
  commands) the CP decides: worker awake: nothing, batch reset; worker
  asleep and unbatched: notify (old behaviour); asleep and batched:
  accumulate the published commands and notify once 512 are pending, or at
  once after a block seal (`NoteSeal` in `WriteCommand`) and on the forced
  publishes of `BeginStreaming` and `FinishStreaming` (which additionally
  notifies under the mutex as before).
- Worker spin before sleeping: 4096 PAUSE (off) or 128 (on), read under the
  worker mutex at the start of each submission.
- Lost-wake-up argument unchanged: the CP's seq_cst fence after publishing
  pairs with the worker's seq_cst store of `worker_sleeping_` and its
  re-check of the published position under the mutex. A deferred wake leaves
  the worker asleep for at most one batch (512 commands) or until the next
  seal or the finish; `FinishStreaming` always wakes it, so no submission can
  hang.

### 3.2 Switch

`d3d12_replay_batched_wake` (bool, category GPU/D3D12, **hot reload**, default
false); takes effect at the next submission.

### 3.3 Counters (always on)

One line right after each `MCLA replay/min` line (that line is unchanged, so
the diagnostic-export parser in `src/mcla_diagnostic_export_policy.h` is not
affected):

`MCLA_REPLAY_WAKE v=1 batched= spin_limit= wake_batch= submissions=
batched_submissions= idle_episodes= spin_pauses= spin_ms= sleeps= wakes=
forced_wakes= deferred_wakes=`

- `batched`/`spin_limit`/`wake_batch` show the switch at report time
  (`wake_batch=0` = every publish); `submissions`/`batched_submissions`:
  streamed submissions of the minute and those run batched (mixed after a
  toggle).
- Worker: `idle_episodes` (times it caught up and started spinning),
  `spin_pauses`, `spin_ms` (wall time spinning; one clock read at each end of
  an episode, none per pause), `sleeps` (= `worker sleeps` of the replay
  line).
- CP: `wakes` (notifications at a publish: every one when off, full batches
  when on), `forced_wakes` (seal, stream start/finish publish),
  `deferred_wakes` (publishes to a sleeping worker without a notification).

## 4. Tests and builds

- Toolchain as l55 (VS 2022 Build Tools CMake/Ninja, LLVM 20.1.8); configured
  fresh into `private/l57-sdk-b` (output `l57-sdk-o`, install
  `l57-sdk-install`) with the l55 options (`l57_sdk_build.ps1` derived from
  `l55_sdk_build.ps1`).
- Full build: 503 warnings, the normalized set **identical** to l55's 503.
  Rebuild after clang-format: 146 warnings, all in the l55 set: **no new
  warning** in any touched file.
- SDK ctest: **1970/1970 passed** (l55 baseline 1952 + 18 new cases:
  11 `[wrm]`, 7 `[replay-wake]`; the same 8 BitStream tests report Skipped
  as in l55). `[wrm]` 517 assertions in 11 cases, `[replay-wake]` 93 in 7;
  `[perf-split]` 229/14, `[pipeline-wait]` 113/10, `[livery-diag2]` 158/18
  unchanged.
- Covered: histogram bucket edges; packet counting, abort, top predicate and
  slot overflow; line format (ASCII, sanitized reason, `top=none`); block
  counting; the loop with a deterministic fake auto-reset event: immediate
  match without blocking, wake plus re-check, stale signals (extra polls, no
  missed release), coalesced signals, timeout fallback for a non-signalling
  writer, CP stop after a long wait versus short yields; a real
  `rex::thread::Event` stress (300 releases from a writer thread with
  spurious extra signals, 5 s bound per wait: 0 timeouts). Batcher: spin
  limits, unbatched = old behaviour, 512-command batching, reset by an awake
  worker, seal and forced wakes, a submission model (0 to 40,000 commands)
  showing the final forced wake and fewer than 512 commands pending between
  wakes; wake line format.
- Not covered by unit tests: the D3D12 worker/CP threading itself (needs the
  renderer) and real interrupt timing.

`private/l57-sdk-install/bin` (SHA-256):

| File | SHA-256 |
|---|---|
| rexgpu-xenos.dll | 2D7BE512D9223734D01D23624549E46DEC1E0243CFDD1A3C95DC86BA8D08D33B |
| rexruntime.dll | 2E0429342ABB932F3957C2D210E7EF8DC6EBECE6CEEF0825ACE6DFDBC9068E8D |
| TracyClient.dll | F2C80E4BEBEBAACB09B71A0FDB26E260764905A98BB237A4DDB660D34137F83D |
| rexglue.exe | 5E9F45305F3D54D4431200FB61492C8D0FF024666E4AE4FCEEAAECD9184ECC6E |

`rexgpu-xenos.dll` contains the `MCLA_WRM v=`, `MCLA_REPLAY_WAKE v=` and
`MCLA WRM event wake` formats and both cvar names. The install was built from
the tree that was then committed unchanged.

## 5. What the owner measures (A/B, after the primary's host build)

Both switches are hot reload, so a host build can set them per session (or
the primary can add an override as in l55). Suggested protocol on the BENCH
720p route, warm store, at least 2 sessions per arm, then 1440p and a Deck
arm:

1. **Baseline (both off):** read `MCLA_WRM` with `event_wake=0`. It settles
   the open reading of the target analysis: expected about 1 waited episode
   per frame (`waited` close to the frame count), `sleeps` close to
   `waited`, histogram mass in `h_1to2ms`, top predicate `mem:1FCA3006`
   ref 0. If `waited` is far above the frame count or the mass sits at
   2-4 ms, the release is bound to the vblank grid and the event-wake gain
   will be small.
2. **WRM arm (`d3d12_wrm_event_wake=true`):** `event_wake=1`, `sleeps=0`,
   `event_waits` with `event_wakes` much larger than `event_timeouts`;
   `wait_ms` per frame and perf split `wrm_mem_ms` per frame should drop
   (target 0.5-1.7 ms of 1.93); `stale_wakes` small. Check `cp_busy_ms`,
   `ring_idle_ms`, fps, frame-pacing p99/max, the `MCLA guest vblank` line
   unchanged; stock (non-Unlocked) mode keeps its cadence. `event_timeouts`
   close to `event_waits` means the release does not come from an interrupt
   dispatch: report it.
3. **Replay arm (`d3d12_replay_batched_wake=true`):** `batched=1`,
   `spin_limit=128`; compare per frame `spin_ms` (expected large drop),
   perf split `replay_cpu_ms` minus `replay_ms` (worker CPU outside replay;
   target from 8-10 down to a few ms at 720p), `sleeps`, `wakes`,
   `deferred_wakes`, `stream_finish_wait_ms` (may rise by up to about 50 us
   per submission), `cp_busy_ms`, fps at 720p and 1440p. No fps gain is
   expected on the PC at 720p; the 1440p laptop power case and the Deck are
   where it may show.
4. Both on together as a final arm.

## 6. Limits and risks

- Not run: no `MCLA_WRM` or `MCLA_REPLAY_WAKE` line has been seen yet.
- The event wake helps only if the release flag is written by the guest
  interrupt callback (static attribution from M7); otherwise each wait times
  out at the old bound (no regression; `event_timeouts` shows it).
- `NotifyWaitRegMemWake` is called from the VSync thread through
  `command_processor_`, the same pointer `MarkVblank` already uses (the
  pre-existing shutdown ordering is unchanged).
- With the switch on, a failed poll can return within microseconds of the
  release instead of after a 1 ms quantum, so the CP proceeds earlier (never
  later) than before; the guest-visible vblank rate is unchanged.
- Batched replay wake: a sleeping worker may lag the CP by up to 511
  commands; replay overlaps CP production less, and `stream_finish_wait` can
  rise. With a slow PAUSE (Intel, about 140 cycles) 128 pauses are about
  5-10 us; on Zen shorter, so the worker sleeps more often (more `sleeps` and
  `wakes`).
- `MCLA_WRM` and `MCLA_REPLAY_WAKE` are new log lines once per minute (plus
  F10 for `MCLA_WRM`), also in builds with both switches off.
- The commit was made with `--no-verify`; the repository has no hooks
  installed (no `core.hooksPath`, only sample hooks), so nothing was skipped.
- Spark was not used: the work was source edits with exact builds and tests,
  with no bounded extraction or triage job whose validation would be cheaper
  than doing it directly.
