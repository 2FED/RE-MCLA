# M10-001 measurement harness: design (read-only study, 2026-10-31)

Read-only design by an Opus subagent on 2026-10-31; accepted by the primary agent with one plan change: a 720p arm is measured next to 1440p at the same place, because the 5060 is GPU-bound at 1440p in every recorded unlocked minute. Note: the old "2.5 us per draw" figure includes GPU fence waits (1.16 us at 720p).

Scope: design only. Nothing was built, launched or written outside the scratch
directory. Sources: `docs/M10-priorities.md`, `docs/M9-priorities.md`, the
M8-001 timebox, M9-006 KI-046 analysis and A/B, the Deck l48 analysis, the
KI-047 diagnostic runs, `docs/testing.md`; host `src/mcla_app.cpp`,
`src/mcla_diagnostics.cpp`, `src/mcla_guest_sampler.*`,
`src/mcla_launcher_controller_win.cpp`; SDK at the pinned v0.20.0.5
(`<sdk>` = the l52 source tree, HEAD `851f03b`, `git describe` = `v0.20.0.5`).
Owner session logs and F10 `state.json` files in the managed profiles were read
(logs and counters only; no save, frame image or minidump was opened).
Paths: `<repo>` = repository root, `<sdk>` = pinned SDK checkout,
`<profiles>` = the launcher's managed-profile root.

## 0. Findings that shape the harness

1. **At 1440p the RTX 5060 laptop is GPU-bound in every recorded unlocked
   driving minute** (56-59 s GPU busy per minute; 1080p is also 58-59 s/min at
   23-25 ms/frame). A 1440p-only run will name the GPU and hide the
   CP-versus-render-thread question that gates M10-006. Recommendation: keep
   1440p as the named M10-001 measurement and add a **720p (scale 1) arm on
   the same visit** (GPU about 13 ms/frame, 38-45 s/min there), where the CPU
   side becomes the limit. The 3080 Ti at 1440p is an optional third arm.
2. **The existing per-draw CP figure includes GPU waits.** The cumulative
   `command_draw_packet_us` scope wraps `IssueDraw`, and `IssueDraw` ->
   `BeginSubmission` -> `CheckSubmissionFence` blocks on the frame fence when
   the GPU is behind. Measured from F10 pairs: 2.0-2.9 us per draw packet at
   1080p/1440p (GPU-bound) against **1.16 us at 720p**. The "2.5 us per draw"
   in M9/M10 is therefore an upper bound that mixes CP work and GPU waiting.
3. **The guest render thread is identifiable today**: it is the guest thread
   with guest handle `F80004E0` on every PC session examined (named
   `XThreadNNNN (F80004E0)`, where NNNN is the host TID and changes per run),
   at 96-99% of one core. On the Deck l48 title package the top guest thread is
   `XThread03D4 (F80004DC)`: the handle differs by 4, so the handle is a
   strong hint, not a contract. The definitional identity is "the guest thread
   that calls `VdSwap`", which l53 records (section 2).
4. **Thread CPU per window is available today, but only by differencing two
   F10 `state.json` files of the same session** (worked examples in 1.4). No
   per-minute thread CPU line exists.
5. The per-minute lines come from **three independent 60 s timers** (frame
   close; deferred-list replay; shared-memory watch), so replay and watch
   numbers cannot be joined exactly with the pacing/draws/GPU minute. l53 puts
   everything in one line on one boundary.

## 1. What is measurable today without new code

### 1.1 Per-minute log lines (in `mcla.log` of every managed session)

| Line | Fields and units | Emitted where | Window |
|---|---|---|---|
| `MCLA frame pacing/min` | frames, fps; interval p50/p90/p95/p99/max (us); counts >20/34/50/100 ms | `<sdk>/src/graphics/d3d12/command_processor.cpp` `EndSubmission` (frame close, CP thread), lines ~4140-4182 | 60 s timer started at the first frame close; one shared timer for the three lines below |
| `MCLA draws per frame/min` | p10/p50/p90/max of `IssueDraw` calls per guest frame | same block (~4183-4190); counter `g_mcla_frame_draws` is incremented at `IssueDraw` entry (line 2690) | same |
| `MCLA gpu busy/min` | ms of GPU time over N submissions; ms per frame | same block (~4192-4200); sum of per-submission begin/end timestamps (`WriteSubmissionStart/EndTimestamp`, `CollectSubmissionTimestamps` ~4326-4337) | same |
| `MCLA replay/min` | submissions (streamed), replay us (worker), end-of-submission wait us, worker sleeps, commands, barriers, top command ids | `<sdk>/src/graphics/d3d12/deferred_command_list.cpp` `ReportReplayStats` (~50-80) | own 60 s timer, checked at each submission end; offset from the pacing minute |
| `MCLA watch stats/min` | valid calls (gpu), pages, invalidations, hot-page histogram | `<sdk>/src/graphics/shared_memory.cpp` ~107 | own timer checked every 1024 validations |
| `MCLA watch protect/min` | arm calls (us, lock wait us), disarm calls (us) | same, ~117 | same as above |
| `D3D12 async pipeline wait (always): minute summary` | waits, ms total/longest, timeouts, skips | `command_processor.cpp` ~2677 | only with `d3d12_async_pipeline_wait_always` |
| `MCLA_LIVERY_DIAG v=1 kind=minute` | resolves, scaled, copies now/deferred/skipped, prev-slot reads | `command_processor.cpp` `LiveryDiagMinuteReport` (~2622) | only with `d3d12_livery_diag` (it also emits up to 64 detail lines per minute) |
| `MCLA graphics: video memory ... of ... MB budget` | usage, peak, budget, texture-cache hard limit | host `src/mcla_texture_budget_win.cpp` (~121-153) | own 60 s report; Windows only (Wine keeps SDK limits) |
| Event lines | `MCLA UI cap: on/off (N draws in frame)`, `MCLA guest vblank: N Hz`, menu state, `Translated N shaders ... from the storage`, pipelines from storage | SDK `graphics_system.cpp`, `pipeline_cache.cpp`; host menu state | per event |

Read-back resolves/copies per second are **not** in a default per-minute line;
they need `d3d12_livery_diag` (detail-line noise) or the cumulative counters in
1.2 do not have them either (the `vulkan_resolve_*` fields are Vulkan-only).

### 1.2 F10 `state.json` (`graphics` object, cumulative since backend init)

Written by host `src/mcla_diagnostics.cpp` `BuildStateJson` (~860-960) from
`CommandProcessor::GetPerformanceDiagnosticSnapshot`
(`<sdk>/src/graphics/command_processor.cpp` 122-160):

- Populated on D3D12: `command_ring_idle_us`, `command_wait_reg_mem_us`
  (completed WAIT_REG_MEM wait episodes, ~1200-1260),
  `command_primary_buffer_count/us`, `command_draw_packet_count/us`
  (DRAW_INDX and DRAW_INDX_2 packet scopes, 1643/1662, steady_clock, two reads
  per packet; includes copies, fence waits and pipeline waits inside the
  draw), `command_swap_packet_count/us` (includes present on the CP thread),
  `command_interrupt_packet_count/us`, `memory_non_mmio_write_fault_*` (all
  guest threads), `memory_watch_arm/disarm_*`, gap splits, lock waits.
- **Zero on D3D12** (filled only by the Vulkan backend): `backend` (stays 0),
  `guest_frames_*`, `submission_count`, `frame_fence_wait_us`,
  `pipeline_end_submission_us`, `deferred_execute_us`, `queue_submit_us`,
  `guest_frame_wall_us`, and all `vulkan_*` (fence calls, resolves, read-back
  copies/bytes/waits, binding and descriptor updates).
- `native_threads[]`: id, base priority, `kernel_time_100ns`,
  `user_time_100ns`, name (`GetThreadDescription`), via Toolhelp +
  `GetThreadTimes` (~1216-1243). Cumulative since thread start, **F10 only**.
- `runtime.guest_output_sequence` / `guest_vblank_sequence` (frame and vblank
  counters, usable as the frame count of a two-F10 window).

### 1.3 What each F10 also records

- **Frame audit (300 samples)**, `<sdk>/src/core/perf/frame_audit.cpp`, armed
  by F10 through `ArmFrameAuditForDiagnostics` (host `RequestLiveSnapshot`,
  `mcla_app.cpp` ~3592-3606). One `REX_PERF_AUDIT_SAMPLE` per guest frame for
  300 frames (5-8 s unlocked, 10 s stock), then `REX_PERF_AUDIT_SUMMARY`.
  Per frame: `cpu_us` = **CP-thread wall time** from frame open
  (`BeginSubmission`, 3896-3901) to frame close (`EndSubmission`, 4203-4209),
  including fence and other waits (not CPU time); `gpu_us` = GPU timestamps at
  the frame's first and closing submission (span, includes inter-submission
  gaps); streaming reads/bytes/stalls (>= 5 ms), audio underruns, shader and
  PSO creations with times. No draws, no thread split.
- **Guest-thread sampler (10 s at about 500 Hz)**, `src/mcla_guest_sampler.*`:
  one `MCLA_GUEST_SAMPLE v=1` line per guest thread with the top 12 innermost
  guest functions and the last-import histogram. The render thread is the one
  whose top functions are `82412F98`/`82411E98` (the GPU-serial spin; e.g. l41
  1440p: `82412F98` 1319 of 3533 samples, `82411E98` 45). Gap: `thread=''` is
  empty, so the line cannot be joined to the thread table without inference.
- The live package (log tail, `state.json`, window frame).

### 1.4 Worked examples: two-F10 differencing (existing owner packages)

Window = time between the two packages of one session; frames from
`guest_output_sequence`; draws from `command_draw_packet_count`; CPU from
`native_threads` deltas. `''` = the unnamed streaming-replay worker
(`std::thread` in `DeferredCommandList::BeginStreaming`).

| Session (build, mode, target) | Window | fps | Draws/frame | Draw-packet us/draw | Draw-packet ms/frame | Render thread F80004E0 CPU | GPU Commands CPU | Replay worker CPU | WAIT_REG_MEM | GPU busy (minute line) |
|---|---|---|---|---|---|---|---|---|---|---|
| 34b8 (l41, unlocked, 1440p) | 54 s | 37.1 | 6,619 | 2.63 | 17.4 | 53.3 s (99%) | 30.0 s (55%) | 27.6 s | 5.8 s | 56 s/min, 26.4 ms/frame |
| 8ade (2E4BB9F3, unlocked, 1440p) | 39 s | 37.4 | 5,386 | 2.94 | 15.9 | 38.1 s (98%) | 19.3 s (49%) | 18.0 s | 7.0 s | GPU-bound minutes |
| 4a4d (2E4BB9F3, unlocked, 1080p) | 51 s | 38.3 | 5,758 | 2.02 | 11.6 | 50.0 s (98%) | 28.3 s (56%) | 26.6 s | 16.0 s | GPU-bound minutes |
| 76c8 (l41, unlocked, 720p) | 11 s | about 72 | 4,921 | **1.16** | 5.7 | 10.9 s (99%) | 8.6 s (78%) | 7.3 s | 1.5 s (ring idle 0.9 s) | 45 s/min, 12.3 ms/frame (min 3) |
| c932 (460EE32F, stock, 720p) | 17 s | 30 | 5,689 | 1.96 | 11.1 | 16.9 s (99%) | 8.2 s (48%) | 7.0 s | 8.1 s | vsync-limited |
| Deck l48 (stock, 720p, title, cumulative 41 s incl. boot) | n/a | 24 avg | about 2,200 avg (7 k in 3D title) | 4.90 | n/a | 03D4 (F80004DC) 34.8 s | 14.1 s | 9.8 s | 23.5 s | not in the tail |

Reading: in the GPU-bound windows the CP thread is about half busy and the
render thread "uses" a whole core because it spins in `82411E98`/`82412F98`.
Render-thread CPU is therefore not render-thread work; work = CPU minus spin
(today: sampler share; l53: measured).

### 1.5 Where the M9/M10 figures came from

- **6-10 k draws per frame:** the l28/l29 per-minute draw percentiles (UI-cap
  release at 6.7-10 k; driving p50 5-7 k, p90 7.5-8.3 k, max 10-17 k). The
  heaviest recorded driving minutes are listed in section 4.
- **2.5 us of CP per draw, 15-25 ms per frame:** an estimate at l29 (CP busy
  about 19-20 ms per frame unlocked at 6-8 k draws), not a per-draw
  measurement; the F10 differencing above shows it mixes in GPU waits.
- **Replay 0.53-0.65 us per draw:** l22d title measurement of
  `DeferredCommandList::Execute` (now on the streaming worker, off the CP).
- **Render thread 13-17 ms per frame:** M8 Day 2 phase census (diagnostic
  host no longer built), interval1 stock race at 720p via Arcade autostart,
  before l16-l29; it is wall time from render-scope start to swap, not CPU
  time, and not at a heavy intersection.

## 2. The gap and the minimal instrumentation (SDK + small host part)

Not instrumented today: the per-draw CP cost split; fence waits on D3D12;
read-back sync waits and copies outside the livery diag; CP CPU per window;
render-thread work versus spin; one aligned window for all of it. Tracy zones
exist (`SCOPE_profile_cpu_f`) but are compiled out of Release builds and not
aggregated.

### 2.1 Switch and clock

- SDK cvar `d3d12_perf_split` (int, hot-reload, default 0): 0 off, 1 sampled
  phase chain (1 in 16 draws, xorshift-selected to avoid aliasing with draw
  patterns), 2 phase chain on every draw (one bias-check run only). Read once
  per frame at frame close into a member; the per-draw cost when 0 is one
  predictable branch on that member.
- Clock: `__rdtsc()` (invariant TSC on the 5060 laptop CPU, the 3080 Ti host
  and the Deck's Zen 2; a plain user-mode instruction under Wine).
  Calibrated at every emission against `steady_clock` (TSC delta / steady
  delta -> `tsc_mhz`, printed in the line); CPUID 0x80000007 EDX bit 8
  (invariant TSC) logged once at start.
- Accumulators: plain `uint64_t` members on the CP thread (single writer),
  copied to relaxed atomics only at emission for the snapshot.

### 2.2 Phase chain inside `D3D12CommandProcessor::IssueDraw` (`command_processor.cpp`)

One timestamp at entry and one at each phase end; each phase gets
`t[k] - t[k-1]`. A small stack object holds the current phase index so early
returns close the open phase in its destructor (no nested zones).

| Phase | Ends after (v0.20.0.5 line) | Contains |
|---|---|---|
| T0 entry | 2690 (`++g_mcla_frame_draws`) | |
| P1 setup | `BeginSubmission(true)` 2744 | shader analysis, early outs, submission open (fence wait subtracted, 2.3) |
| P2 primitive | `primitive_processor_->Process` 2750 | index conversion, primitive processor, its shared-memory index requests |
| P3 render targets | `render_target_cache_->Update` 2781 | shader modifications, RT cache update (EDRAM transfers, dispatches) |
| P4 pipeline | after the pending-pipeline block ~2823-2844 | `GetOrCreateTranslation`, `ConfigurePipeline` 2809, pending wait (subtracted) |
| P5 textures | `texture_cache_->RequestTextures` 2852 | texture lookup/creation/loads and their shared-memory requests |
| P6 bindings | `UpdateBindings` 2915 | pipeline bind, viewport/scissor, `UpdateFixedFunctionState`, `UpdateSystemConstantValues`, `UpdateBindings` (root CBVs, descriptors, samplers) |
| P7 vertex/memexport ranges | before "Primitive topology" 2998 | vertex fetch `RequestRange` loop (~2920-2975), memexport ranges |
| P8 record | function exit | topology 3059, index buffer view, `UseForReading/Writing`, `SubmitBarriers`, D3D draw recording into the deferred list, render audit, memexport post |

Mapping to the requested categories: (a) state/bindings = P3 + P4 + P6;
(b) shared-memory requests = P7 plus the cross-cutting upload accumulator
below (P2 and P5 also contain requests; the upload accumulator measures them
wherever they happen); (c) primitive processing = P2; (d) recording = P8 (plus
the `WriteCommand` share inside every phase), replay = worker time and
end-of-submission wait; (e) resolves/read-back = the `IssueCopy` split below.

Per-sampled-draw cost: 9 TSC reads (P1-P8 plus entry). Draws that return
early (no-op, pipeline-pending skip) are counted by exit kind.

### 2.3 Cross-cutting accumulators (timed only when the slow path runs)

- **Frame/submission fence wait:** `CheckSubmissionFence` around the two
  `WaitForSingleObject` calls (3725, 3744). Subtracted from the phase it
  happens in and reported as `fence_wait`.
- **Pending-pipeline wait:** `WaitForPendingPipeline` only when it actually
  waits.
- **Shared-memory upload:** `SharedMemory::RequestRanges`
  (`shared_memory.cpp` 522) counts calls without a clock; the
  `UploadRanges(upload_ranges_)` call (end of 522-699, into
  `D3D12SharedMemory::UploadRanges`, `d3d12/shared_memory.cpp` 368) is timed,
  with pages and bytes. Watch arm/disarm and write faults already have
  timers; their deltas go into the line.
- **Copies (`IssueCopy` 3423 / `IssueCopy_ReadbackResolvePath` 3439):** count
  and total; split into `render_target_cache_->Resolve` (3441), read-back
  GPU-copy recording (~3629-3640), synchronous read-back wait
  (`AwaitAllQueueOperationsCompletion`, ~3653 and ~3663), and the CPU memcpy
  (3698) with bytes; copies now/deferred/skipped (same decision as the livery
  diag, without its detail lines). About 35 copies per frame, so 4-6 reads
  per copy are negligible.
- **WAIT_REG_MEM:** the existing episode timer
  (`command_processor.cpp` ~1200-1260) split into register-poll and
  memory-poll episodes (one extra branch per completed episode), so a Deck
  vblank wait can be told from a guest-memory wait.
- **Submission and present:** `EndSubmission` (4038) total minus
  `FinishStreaming` wait (4082), with `ExecuteCommandLists` (4111) separately;
  `IssueSwap` (1992) total (present runs on the CP thread).
- **Ring idle:** existing timer (`command_processor.cpp` ~338-357), delta.
- **Residual** `packets_other` = CP busy minus all of the above: PM4 decode,
  register writes, constant loads, other packets.

### 2.4 Thread CPU and GPU busy in the same window

- CP thread: `GetThreadTimes(GetCurrentThread())` at emission (the emitter is
  the CP thread), user and kernel deltas.
- Replay worker: name it `D3D12 Replay` (`SetThreadDescription` in
  `BeginStreaming`, `deferred_command_list.cpp` ~196) and keep its native
  handle for `GetThreadTimes`.
- Guest render thread: `VdSwap_entry` (`<sdk>/src/kernel/xboxkrnl/xboxkrnl_video.cpp`
  440) stores `GetCurrentThreadId()` in an atomic (one relaxed store per
  frame). The emitter opens/caches that thread handle and reads its CPU;
  `render_tid` and the guest handle go into the line.
- Render-thread work versus spin: host strong override of `sub_82411E98`
  (same pattern as `REX_EXTERN(sub_822D29E8)` in `src/mcla_menu_state.cpp`
  182): TSC before/after `__imp__sub_82411E98`, added through a new SDK sink
  `rex::perf::AddRenderSerialWaitTicks(ticks)` only when called on the render
  thread. Render work = render CPU minus serial wait. Two reads per call; the
  thread is waiting anyway.
- Other guest threads: host per-minute `MCLA_THREAD_CPU v=1` line (top 8 by
  delta, name, user/kernel ms) from a small host thread woken by the SDK
  minute boundary (Toolhelp snapshot once a minute, off the CP thread).
- GPU busy: the existing submission-timestamp sum (real D3D12 timestamp
  queries on the direct queue; through vkd3d on the Deck), copied into the
  line. It covers command-list execution time, not inter-submission idle, and
  not the copy queue.

### 2.5 Output

One line per window, emitted in `EndSubmission` immediately after
`MCLA gpu busy/min` on the same 60 s timer, plus an immediate line on demand:

```
MCLA_PERF_SPLIT v=1 reason=minute|f10|exit seq=N wall_ms= frames= mode=1 sample_div=16 tsc_mhz=
 draws= draws_issued= draws_noop= draws_pso_skip= sampled= copies= submissions=
 cp_cpu_ms= cp_user_ms= cp_kernel_ms= cp_busy_ms=
 ring_idle_ms= wrm_reg_ms= wrm_mem_ms= fence_wait_ms= pso_wait_ms= readback_wait_ms= stream_finish_wait_ms=
 draw_ms= p_setup_ms= p_prim_ms= p_rt_ms= p_pso_ms= p_tex_ms= p_bind_ms= p_vb_ms= p_record_ms=
 upload_calls= upload_ms= upload_kb= watch_arm_ms= watch_disarm_ms= write_fault_ms=
 copy_ms= resolve_ms= readback_gpu_ms= readback_memcpy_ms= readback_kb= copies_now= copies_deferred= copies_skipped=
 submit_ms= execute_ms= present_ms= packets_other_ms=
 replay_cpu_ms= replay_us= render_tid= render_handle= render_cpu_ms= render_wait_ms=
 gpu_busy_ms= gpu_submissions=
```

Phase values from sampled draws are scaled by `draws_issued / sampled`
(mode 1) and printed as window totals; per-frame and per-draw values are
derived offline. `reason=f10` lines come from a new
`rex::perf::RequestPerfSplitMark(reason)` that the host calls in
`RequestLiveSnapshot`; the CP emits at the next frame close, so F10 marks cut
exact route segments. The same cumulative totals go into a new
`GraphicsPerformanceDiagnosticSnapshot` group and a `perf_split` object in
`state.json`, so two F10s still difference cleanly.

### 2.6 Overhead estimate

- TSC read: about 25-40 cycles (about 6-10 ns at 4 GHz; 10-15 ns on the Deck).
- Mode 1: 9 reads x 1/16 of draws, plus scatter-adds and the xorshift: about
  6-9 ns per draw on the PC, 8-12 ns on the Deck. Against the measured
  1.16-2.9 us per draw (PC) and 4.9 us (Deck): **0.3-0.8%** of CP time.
  At about 300 k draws/s that is about 2-3 ms of CP per second.
- Cross-cutting timers: uploads (tens of thousands of calls per second at
  most, 2 reads each), copies (about 1,100 per second, 4-6 reads), fence and
  pipeline waits (only while waiting), submissions and swaps (about 50-100 per
  second): together under 0.1%.
- Per minute: one Toolhelp snapshot on a host thread, a few `GetThreadTimes`
  and one log line on the CP thread.
- Mode 2 (every draw): about 9 reads per draw, 3-6% of CP time. Used once to
  check that the mode 1 phase shares match mode 2 within 2 percentage points.
- Acceptance check for the overhead: l53 mode 0 against mode 1 on the same
  route; fps and CP busy within the run-to-run spread (target under 1%).

### 2.7 Host changes in l53 (owner-internal only)

- CMake option `MCLA_PERF_SPLIT_DEFAULT` (OFF by default, ON for l53): at
  `OnPreSetup` the host sets `d3d12_perf_split=1` with `SetFlagByName` unless
  the user set it. The launcher builds the command line itself and strips the
  environment, so a compiled default is the reliable path (the
  `MCLA_RAW_TIMER_DEFAULT_ON` pattern).
- F10 calls `RequestPerfSplitMark("f10")`.
- `sub_82411E98` override feeding the render-wait sink (2.4).
- `MCLA_GUEST_SAMPLE` lines carry `tid=` and the XThread name (fixes
  `thread=''`).
- `state.json` gains the `perf_split` object; `MCLA_THREAD_CPU` per minute.

## 3. Bench profile through the launcher (MAIN is never played)

Mechanism in the launcher (`src/mcla_launcher_controller_win.cpp`
`Branch(..., restore)` ~658-722; UI "Copy selected profile..." in
`src/mcla_launcher_ui_win.cpp` 314):

- **Copy selected profile** (duplicate) copies only the source's `user/` tree
  (`CopySaveTree`, SHA-256 verified) into a new managed profile and writes
  `import-receipt.txt` and `branch-receipt.txt` (`mode duplicate`,
  `source-profile`). It refuses a source with an unfinished operation or an
  unfinished session. The source is only read under its writer lock.
- **Restore** branches from a session's `before-user` / `after-user` archive
  (verified against the snapshot receipt) into a new profile.
- Each session records `before-snapshot.txt` and `after-snapshot.txt`
  (`MCLA-SAVE-SNAPSHOT-V1`). In MAIN, 23 of 26 sessions ended with an
  identical save (driving without saving changes nothing), so the receipts
  prove an unchanged start state per run.
- The shader/pipeline store is per profile and per build
  (`<profile>/cache/<build hash>/`) and is **not** copied, so the first launch
  of a fresh copy is a guaranteed cold store and the second launch is warm.

Owner, once (PC; same on the Deck with its own MAIN copy, typing the label with
STEAM+X because of KI-052):

1. Close the game. In the launcher select MAIN, "Copy selected profile...",
   label `BENCH-SEED`. Never start BENCH-SEED.
2. Select BENCH-SEED, "Copy selected profile...", label `BENCH-A`; again
   `BENCH-B`.
3. For BENCH-A and BENCH-B set the run setup: l53 build folder, Unlocked,
   RTX 5060, 2560x1440 (scale 2), vsync on; a second setup for the 720p arm on
   the same copies after the 1440p runs (or `BENCH-C` for it).
4. Tell the agent where the save puts the car after loading (garage or last
   position) and the in-game time/weather seen on the first run.

Identical restart per run: BENCH-A run 1 = cold, run 2 = warm; BENCH-B the
same. Do not save, buy or enter events. After each run the agent checks that
`before-snapshot.txt` equals `after-snapshot.txt`; if not, that copy is retired
and a new copy of BENCH-SEED replaces it. Time of day and weather: the save
starts each run from the same state; the in-game clock then advances with play
time, so keep the same timing from load to the route start (about 2 minutes)
and confirm it on the F10 frame. Traffic has no seed the host controls;
mitigated by the stationary segment, two laps and matching windows by draws.

## 4. Candidate heavy intersections

The logs carry no position, so candidates are given by evidence; the owner
names the physical place. The F10 packages in the heaviest windows (34b8 at
15:33:56Z/15:34:50Z, 8ade, 4a4d) contain the window frame and can show the
owner where those minutes were.

Heaviest recorded driving minutes (UI cap off, p10 > 2 k):

| Session | Build / mode / target | Minute | fps | Draws p10 / p50 / p90 / max | GPU s/min, ms/frame |
|---|---|---|---|---|---|
| 560b (10-30) | l48, unlocked, 1080p | 15:15-15:17 | 39.3-40.1 | 5.2-5.8 k / 6.6-6.8 k / 7.8-8.0 k / 9.8-10.5 k | 58-59, 24.6-25.0 |
| 34b8 (10-29) | l41, unlocked, 1440p | min 2 | 35.4 | 5.2 k / 7.0 k / 8.2 k / 10.7 k | 56, 26.4 |
| 88b4 (10-29) | F8FE376E, unlocked, 720p | min 2 | 49.1 | 5.3 k / 6.8 k / 8.3 k / 17.8 k | 38, 13.2 |
| 58e6 (10-28, 31 min) | A6E7D2F0, stock, 720p | min 11-28 | 33-37 | 4.7-5.7 k / 6.1-6.7 k / 7.2-10.0 k | 27-28, 13-14 |
| b5e3 / 57a0 (KI-046 A/B) | l41 / l33, unlocked, 2160p | W2 | 20.1-20.4 | 4.2-4.6 k / 6.3 k / 7.7-7.9 k | 59.5, 48.6-49.4 |

Candidates:

1. **C1 "high-rise turnaround / downtown high-rises"**: the l24 Test 1 hitch
   at the high-rise turnaround (130 ms CP stretches), the owner's l27 note
   that entering the high-rises drops to 30-40, and the KI-046 "downtown at
   6-7 k draws". Expected 6.5-7 k p50, p90 about 8 k.
2. **C2 "the big intersection on the daily route"** (where police stopped the
   owner in l24 Test 5 and where the l19 F10 was pressed): the 560b minutes
   (6.6-6.8 k p50, max 10.5 k) are the best sustained candidate.
3. **C3 control: the 3D title city at PRESS START**: deterministic, no
   driving variance, 4.6-5.5 k draws on the PC (l28) and about 7 k on the Deck
   (l48 `UI cap: off (7065 draws)`). Not an intersection; use as the
   reproducibility control and for the Deck baseline.

Route protocol per run (PC):

1. Launch from the bench copy, load the save, drive to start point S (a named
   landmark 150-200 m before the intersection, car stopped at the kerb facing
   it). Wait until the log has passed the 45 s unlock (`guest vblank: 240 Hz`)
   and no menu is open.
2. Lap: F10 at S (mark 1); 90 s stationary, no input, default chase camera;
   F10 (mark 2); slow drive through the intersection on a fixed path at a
   steady low speed to exit point E (about 90 s); F10 at E (mark 3); return to
   S off the clock.
3. Two laps; then normal exit. Total about 9-10 minutes.

Deviation detection from the log: segment length between marks 90 s +- 5 s;
segment draws p50 within 10% of lap 1 and of run 1 for the same segment; no
`UI cap: on`, menu-state pause or `guest vblank: 60 Hz` inside a segment;
audit `pso_count=0` in warm runs; mark frames show the same landmarks (owner
check); identical save snapshots. A segment failing any check is excluded,
not corrected.

## 5. Deck plan

- Same l53 build (V1 owner ticket), Deck launcher l49 or later (KI-050
  environment fix), the Deck's own bench copy made from its MAIN copy as in
  section 3. Stock mode, 1280x720 windowed (KI-057 fullscreen open), D3D12
  through vkd3d-proton (the launcher forces `--gpu_backend=d3d12`; native
  Vulkan is M10-004).
- Runs: (a) title: boot to PRESS START, idle 3 minutes, F10 at 60 s and
  150 s; (b) short drive: one lap of the same route (or the nearest
  equivalent if streaming makes the PC route impractical), F10 marks as on the
  PC. First launch of a fresh copy is cold, the second warm.
- Counter differences on the Deck: all D3D12 lines and `MCLA_PERF_SPLIT`
  apply; `vulkan_*`, `backend` and the Vulkan-only frame/submission fields
  stay 0; thread names and kernel/user times are available under Wine (l48
  shows `GPU Commands` 11.1 s user + 3.0 s kernel); TSC is native;
  `MCLA gpu busy/min` needs vkd3d timestamp support, which is expected but
  not yet seen in a Deck log (the l48 tail ended before the first minute):
  check it in the first Deck log. The host video-memory lines are absent
  under Wine (SDK limits kept). Expected wall on the Deck: WAIT_REG_MEM
  dominated CP (23.5 s of 36.4 s) with the render thread busy, so the
  `wrm_reg/wrm_mem` split and render work versus wait decide between guest
  CPU, vblank pacing and GPU.
- Package to the PC: after a normal exit, the owner copies the Deck profile's
  `sessions/<id>/` text files (`mcla.log`, `launch-receipt.txt`,
  `capabilities.txt`, `state.txt`, snapshot receipts; not `before-user` or
  `after-user`) and the session's F10 packages through the same path used for
  the l48 package, into `<repo>/private/owner-deck-<date>/`. The volunteer
  diagnostics export is not enough: it parses at most 10 minutes of the four
  existing line types and would not carry `MCLA_PERF_SPLIT`.

## 6. Acceptance for M10-001 and the l53 build

| Item | Accepted when |
|---|---|
| Harness document | This design reviewed by the primary agent; route, bench profile and segment rules recorded in the M10-001 evidence |
| l53 SDK branch | Branch from v0.20.0.5 (`851f03b`) with the changes in 2.1-2.5 behind `d3d12_perf_split` (default 0, hot-reload); unit tests for accumulation, scaling, calibration and line formatting; SDK ctest green; mode 0 produces no new line and no clock reads per draw |
| l53 host | `MCLA_PERF_SPLIT_DEFAULT` ON only in l53; F10 mark; `sub_82411E98` wait sink; sampler `tid`; `state.json` `perf_split`; `MCLA_THREAD_CPU`; host suites green; volunteer build configuration unchanged (option OFF, cvar 0) |
| Overhead | l53 mode 0 against mode 1 on the same route: fps and CP busy within the run-to-run spread; mode 2 once: mode 1 phase shares within 2 pp |
| Internal consistency | Per window: sum of phases + copies + submit + present + waits + residual equals CP wall within 5%; scaled phase total within 5% of the unsampled `draw_ms` |
| PC 5060, 1440p unlocked | 2 cold + 2 warm runs on the bench route, cache state and pipelines-from-storage stated, at least two valid 90 s segments of each kind per run, raw logs retained |
| PC 5060, 720p unlocked (recommended addition) | 2 warm runs, same route and segments |
| Deck | Title and short drive, stock, cold and warm stated, raw logs retained |
| Thread CPU table | Render (work and wait), CP, replay worker, top guest threads per segment |
| CP cost split | Per segment: ms per frame and us per draw for every phase and accumulator |
| Read-back | Resolves and CPU copies per second, sync wait and memcpy ms per frame |
| Verdict note | Names the wall per platform and arm with the rule below, and the order it implies for M10-002..006 |

Verdict rule per segment (matched draws): **GPU** if GPU busy is at least 90%
of wall and GPU ms per frame is within 10% of the frame interval; **CP** if CP
busy (wall minus ring idle, WAIT_REG_MEM and fence waits) is at least 90% while
GPU is below 85% and the render thread spends a large share in its serial
wait; **render thread** if render work (CPU minus serial wait) is at least 90%
of the frame interval while the CP idles or waits in WAIT_REG_MEM; **read-back**
if copies plus read-back waits exceed 15% of CP busy and the CP is the wall;
**wait/pacing** if WAIT_REG_MEM register waits dominate at the vblank cadence
(expected on the Deck in stock mode).

The l53 build is owner-internal (V1 ticket); its SDK branch stays local unless
the instrumentation is later folded into a volunteer candidate, which would
need a numbered fork version and the owner's push approval.

Spark was not used for this design: exact source reading and a small log
extraction were cheaper to do and verify directly.
