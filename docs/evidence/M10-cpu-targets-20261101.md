# M10 CPU-side targets: CP waits, replay worker, render thread (read-only design analysis, 2026-11-01)

Read-only analysis by an Opus subagent on 2026-11-01 (SDK at 483458d, generated code, owner l53/l54 sampler logs); accepted by the primary agent as the CPU-side M10 task list.

Read-only analysis by an Opus subagent. Inputs: `docs/evidence/M10-001-720p-and-ki047-20261101.md`,
`M10-001-run1-1440p-cold-20261101.md`, `M10-001-harness-design-20261031.md`, the M7 evidence
`M7-016-wait-predicate-census.md`, `M7-016-wait-reference-census.md`,
`M7-016-guest-scope-overhead.md`, `M7-016-guest-scope-candidate.md`; the SDK worktree
`<sdk>` = l53 SDK worktree, branch `mcla/m10-perf-split`, head `483458d2885419234a0729698f29a8850a052b55`
(v0.20.0.6 line); the generated corpus `generated/default`; host `src/`; the F10 sampler
(`MCLA_GUEST_SAMPLE`), `MCLA_PERF_SPLIT`, `MCLA_THREAD_CPU` and watch lines of the two l53 720p warm
sessions (`<profile B>/diagnostics/<3a393a03...>` and `<1f22d9bb...>`) and one `state.json`.
No build, launch, git write, save or minidump access. Line numbers refer to the commit above.
Spark: not used (source reading plus exact counting; a model answer would need the same checks).

## 0. Summary

| Finding | Nature | Size at 720p (per frame) |
|---|---|---|
| CP `WAIT_REG_MEM` polls the guest present-release word with `Sleep(1 ms)` because `vsync=true` | host polling quantum on the CP | 1.93 ms constant; most of it plausibly one `Sleep(1)` quantum |
| Replay worker spins 4096 `PAUSE` between 32-command chunks and almost never sleeps while streaming | host spin | 8.2-9.6 ms CPU of one core; not on the critical path on PC |
| Every recompiled function runs `GuestFunctionScope`, which calls `std::uncaught_exceptions()` twice (CRT FLS lookup with Get/SetLastError) | host overhead in all guest code | estimated 5-15% of render-thread CPU (0.8-2.4 ms), not measured on PC |
| Guest `mfmsr`/`mtmsrd r13` interrupt-disable idiom around `lwarx`/`stwcx.` is emitted as two lock/unlock pairs of the global `std::recursive_mutex` plus 3 `seq_cst` fences; the hottest instance is the D3D resource AddRef `sub_82422160` | host overhead | `sub_82422160` alone = 6.2% of render-thread samples, about 1.0 ms |
| Write faults on watched pages (116 per frame, about 7.8 us handler each, plus the kernel trap) | host overhead, shared by guest threads | 0.72-0.94 ms handler; render-thread kernel time 1.1-1.8 ms |
| Small XDK D3D state setters (`sub_82415DC8`, `sub_82415EE8`, `sub_82410F88`) and their register save/restore helpers | guest work dominated by per-call host overhead (scope, helper calls) | 8-9% of samples together; benefit mostly through the two codegen items above |
| Game render code (`sub_821792A0`, `sub_822DF960`, `sub_8230E8F8`, `sub_8218C9D8`, ...) | genuine guest work | about 10% in the top lists; no cheap host fix |

## 1. Part 1: CP waits

### 1.1 The `WAIT_REG_MEM` handler (exact logic)

`<sdk>/src/graphics/command_processor.cpp:1189-1290` (`ExecutePacketType3_WAIT_REG_MEM`; no D3D12
override; `PrepareForWait`/`ReturnFromWait` at 747-751 only flush the trace writer):

- Reads `wait_info, poll_reg_addr, ref, mask, wait`; `is_memory = wait_info & 0x10`.
- Loop: read the value (memory: `TranslatePhysical(addr & ~3)`, endian swap by `addr & 3`; register:
  `ReadRegisterValue`, plus `MakeCoherent` for `COHER_STATUS_HOST`), compare by `wait_info & 7`.
- Not matched (1258-1284): if `wait >= 0x100`: `PrepareForWait()`, then
  **`if (!vsync) MaybeYield(); else Sleep(milliseconds(wait / 0x100))`** (1268-1273), `SyncMemory()`,
  `ReturnFromWait()`; otherwise `MaybeYield()`.
- `vsync` is the SDK cvar (`command_processor.cpp:41`, default true; the logs show `vsync true`).
- `rex::thread::Sleep` (`<sdk>/src/core/threading_win.cpp:96-102`) maps 1000 us to `::Sleep(1)`.
  The host sets `timeBeginPeriod(1)` (`src/mcla_timer_resolution_win.cpp:21`), so one `Sleep(1)`
  returns after 1-2 ms (the next 1 ms tick after at least 1 ms).
- The `wrm_mem` timer covers the whole episode from the first failed poll to the match.

Why vsync makes it sleep: the only alternative in the code is `MaybeYield` (`SwitchToThread`, a busy
poll), reserved for `vsync=false` ("fast and dangerous"). `vsync=false` also switches the guest vblank
to 1000 Hz (`graphics_system.cpp:211-212`), so it is not an isolated control (as M7 already noted).

### 1.2 What MCLA waits on

From M7-016 (two censuses, native and Deck): **one memory predicate only**: physical word
`0x1FCA3004` (raw `0x1FCA3006`, endian mode 2), equality, full mask, `wait = 256` (so `Sleep(1 ms)`),
**reference 0 only**. Static attribution (M7): the guest driver's block at device `+0x2A94`; the packet
sequence writes `SCRATCH_REG1 = 1` (the CP's scratch write-back stores 1 at `SCRATCH_ADDR + 4`,
`command_processor.cpp:499-506`), waits for 1 (satisfied at once), raises an `INTERRUPT` packet, then
**waits for the word to become 0**. The guest graphics interrupt callback either clears it at once or
queues a release against the guest vblank count; the vblank interrupt (source 0) clears due entries.
So the awaited value is a **guest present/vblank release flag written by the guest interrupt
callback**, i.e. the swap-interval throttle, not a GPU fence.

Who runs the writer:

- `INTERRUPT` packets run the callback **synchronously on the CP thread**
  (`command_processor.cpp:1120-1136` -> `GraphicsSystem::DispatchInterruptCallback`,
  `graphics_system.cpp:368-387`, which calls `ExecuteInterrupt` while holding the global critical
  region, `src/system/function_dispatcher.cpp:136`). If the callback clears the word immediately, the
  next poll matches with no wait.
- Otherwise the release comes from `MarkVblank` on the "GPU VSync" host thread
  (`graphics_system.cpp:195-240`, `MarkVblank` 422-438). In Unlocked mode the guest vblank is 240 Hz
  (`src/mcla_launcher_capabilities.h:28`, `mcla_guest_vblank_hz`), but that thread itself loops on
  `Sleep(1 ms)` (`graphics_system.cpp:235`) and fires due ticks in bursts.

Reading of `wrm_mem = 1.93 ms/frame` (constant at 38 fps 1440p and 60 fps 720p, cold or warm): the
most economical explanation is **about one waited episode per frame that costs one `Sleep(1)` quantum
(1-2 ms, about 1.9 ms on this laptop)**, with the actual release arriving well inside that quantum. A
wait to a random phase of the 240 Hz grid (mean 2.08 ms) plus a polling overshoot would exceed 2.5 ms
and would vary with frame rate. Not proven: the l53 counters have no episode or sleep count (the M7
census had them: 3,543 episodes and 42,722 sleeps in 120 s at stock 30 fps, i.e. real pacing waits of
about 18 ms at 60 Hz vblank, a different regime).

### 1.3 Proposal WRM-1: wake on the writer instead of sleeping a quantum (recommended)

The writer is always the guest interrupt callback, dispatched only through
`GraphicsSystem::DispatchInterruptCallback`. Signal a host auto-reset event after each dispatch and let
the CP wait on it, with the original timeout as the upper bound.

```diff
 // command_processor.h (member, created in Initialize)
+  std::unique_ptr<rex::thread::Event> wrm_wake_event_;   // auto-reset
+ public: void NotifyWaitRegMemWake() { if (wrm_wake_event_) wrm_wake_event_->Set(); }

 // command_processor.cpp:1265-1273
       if (wait >= 0x100) {
         PrepareForWait();
         if (!REXCVAR_GET(vsync)) {
           rex::thread::MaybeYield();
-        } else {
-          rex::thread::Sleep(std::chrono::milliseconds(wait / 0x100));
+        } else if (REXCVAR_GET(mcla_wrm_event_wait)) {
+          ++wrm_sleeps_;  // diagnostic counter
+          rex::thread::Wait(wrm_wake_event_.get(), false,
+                            std::chrono::milliseconds(wait / 0x100));
+        } else {
+          rex::thread::Sleep(std::chrono::milliseconds(wait / 0x100));
         }

 // graphics_system.cpp:386 (after ExecuteInterrupt returns)
   function_dispatcher_->ExecuteInterrupt(thread->thread_state(), interrupt_callback_, args, ...);
+  if (command_processor_) command_processor_->NotifyWaitRegMemWake();
```

- Semantics unchanged: same predicate, same poll, same timeout; the event only shortens the sleep. A
  stale signal (event set by an earlier interrupt) costs one extra poll and loop. Writers that are not
  interrupt callbacks fall back to the old timeout.
- Cost: one `SetEvent` per interrupt (about 240 vblank plus about 60 packet interrupts per second), a
  few microseconds per second.
- Expected gain: **0.5-1.7 ms/frame of CP time** (upper bound 1.93 if the release usually lands within
  tens of microseconds; lower if part of the wait is a genuine wait for the 240 Hz grid). Raises the
  CP's 720p cap (busy + wrm_mem) from about 58-66 fps towards 65-76 fps. **No fps change while the
  render thread is the first wall**; it matters once render-thread work drops below about 15 ms.
- Risks: low. Ordering: the CP still reads guest memory after the wake (`SyncMemory` stays); the
  callback ran under the global lock and returned before the signal. Power: none (it blocks; no spin).
  Deck/Proton: an event wait is standard.
- Test: first add counters `wrm_episodes`, `wrm_sleeps` and a `wrm_wait_us` histogram (0-100 us,
  100-500 us, 0.5-1, 1-2, 2-5, 5+ ms) to `MCLA_PERF_SPLIT`, also with the cvar off (this settles the
  reading in 1.2). Then A/B on the BENCH 720p protocol: wrm_mem per frame, CP busy, ring idle, fps,
  frame-pacing p99, guest vblank Hz line unchanged; the stock (non-Unlocked) mode must keep its
  cadence (the release is still vblank-driven).

### 1.4 Proposal WRM-2: precise guest vblank ticks (optional, after WRM-1)

The VSync thread polls on `Sleep(1 ms)` (`graphics_system.cpp:235`), so 240 Hz ticks are late by
0-2 ms and bursty. A high-resolution waitable timer (`CreateWaitableTimerExW(...,
CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, ...)`, absolute due time = next tick; fall back to the current
loop when unavailable) makes releases on time.

- Gain: at most the tick lateness (mean about 0.5-1 ms) on episodes that really wait for a tick;
  smaller and less certain than WRM-1. Also smoother frame pacing in Unlocked mode.
- Risk: low-medium: guest-visible vblank timing becomes more regular (same rate); the flag's behavior
  under Proton must be checked on the Deck.
- Not recommended: raising the Unlocked vblank above 240 Hz, or answering `WAIT_REG_MEM` early. Both
  change guest timing semantics (an M9-003 decision), not host overhead.

### 1.5 CP ring idle loop (for completeness)

`command_processor.cpp:321-383`: 500 `SwitchToThread` spins, then `Wait(write_ptr event, 5 ms)`. Ring
idle is only 0.5-0.9 ms/frame at 720p; no change proposed.

### 1.6 Replay worker

`<sdk>/src/graphics/d3d12/deferred_command_list.cpp`:

- Writer side: `WriteCommand` publishes every `kPublishBatchCommands = 32` commands
  (`include/rex/graphics/d3d12/deferred_command_list.h:694`, `.cpp:154-156`); `Publish`
  (`.cpp:134-146`) notifies the condition variable only when `worker_sleeping_` is set.
- Worker (`WorkerMain`, `.cpp:243-327`): replays any published range (timed: `replay_ms`); when caught
  up it spins **`++idle_spins < 4096` with `_mm_pause()`** (`.cpp:300-303`) before taking the mutex and
  sleeping on the condition variable (`.cpp:304-316`).
- 4096 `PAUSE` is roughly 0.15-0.3 ms on current Intel cores (about 140 cycles each), much shorter on
  Zen. The CP publishes every 32 commands (32-41 k commands per frame, so about 1,000-1,300 publishes
  per frame, roughly every 10 us of CP time), always before the spin budget runs out. So **the worker
  spins for the whole time the CP is producing a submission** and sleeps only between submissions
  (5.5-7.5 sleeps per frame against 4.4-4.6 submissions). This matches the measurement: worker CPU
  minus timed replay = 8.2-9.6 ms/frame at 720p (about the CP's command-producing time), 4.8-7.8 ms at
  1440p.

Proposal **RW-1: batched wake plus short spin**:

```diff
 // deferred_command_list.h
   static constexpr uint32_t kPublishBatchCommands = 32;     // visibility granularity (unchanged)
+  static constexpr uint32_t kWakeBatchCommands = 512;       // wake a sleeping worker only this often
+  uint32_t unwoken_commands_ = 0;

 // Publish(bool force_wake = false): notify only when enough work is pending
-  if (streaming_ && worker_sleeping_.load(std::memory_order_relaxed)) {
+  unwoken_commands_ += kPublishBatchCommands;
+  if (streaming_ && worker_sleeping_.load(std::memory_order_relaxed) &&
+      (force_wake || unwoken_commands_ >= kWakeBatchCommands)) {
+    unwoken_commands_ = 0;
     std::lock_guard<std::mutex> lock(worker_mutex_);
     worker_cond_.notify_all();
   }

 // WorkerMain(): spin about 20 us, then sleep
-      if (++idle_spins < 4096) {
+      if (++idle_spins < REXCVAR_GET(d3d12_replay_spin_pauses)) {   // default e.g. 128
```

- The lost-wake-up argument is unchanged (seq_cst fence plus the `worker_sleeping_` re-check).
  `FinishStreaming` already notifies under the mutex (`.cpp:226-229`); block seals should force a wake.
- Cost on the CP (second wall at 720p): about 2 wakes per 1,000 commands instead of none; a
  `notify_all` with a sleeper is a 1-3 us syscall, so about 60-80 wakes per frame = 0.1-0.2 ms of CP
  time. The wake must be batched: waking per 32-command chunk would cost the CP 1-3 ms/frame.
- Expected gain: worker CPU from about 11.5-13 ms/frame to about 4-5 ms/frame (frees about 0.4-0.5 of
  a core at 720p). **No direct fps change on this PC**; indirect gain where the CPU package shares power
  or thermals with the GPU (laptop at 1440p, where render and CP per-frame costs are 15-40% higher) and
  on the 4-core Deck. `stream_finish_wait` may rise by up to one batch of replay (512 commands at about
  0.1 us = about 50 us per submission, about 0.2 ms/frame worst case on the CP); measure it.
- Test: add `spin_us` (time in the spin loop) and `wakes` to `MCLA replay/min` first, to confirm the
  attribution; then A/B (spin 4096 vs 128, wake batch 32 vs 512): worker CPU, `stream_finish_wait`, CP
  busy, fps at 720p and 1440p, and a Deck arm.

## 2. Part 2: render thread (guest thread `F80004E0`, the VdSwap caller)

### 2.1 What the sampler measures

`src/mcla_guest_sampler.cpp` reads `PPCContext::current_function` and `last_import` every 2 ms for
10 s. `current_function` is set by `GuestFunctionScope` (`<sdk>/include/rex/ppc/context.h:508-534`)
and restored on return, so **`fn` is the self (exclusive) time of the innermost recompiled function,
including any host code it calls inline** (lock calls, CRT calls, fault handlers). `last_import` is
only the most recent import called, not time spent in imports.

Aggregate over the 8 complete 10 s windows of the two l53 720p warm sessions (28,269 samples;
functions outside a window's top 12 are not counted, so lower ranks are undercounted):

| Function | % samples | Size, calls | What it is | Cost nature |
|---|---|---|---|---|
| `sub_82422160` | **6.20** | 29 insns: `mfmsr; mtmsrd r13; lwarx; addi 1; stwcx.; mtmsrd`, recursion to the parent at +24 when type 4 | XDK D3D resource **AddRef** (atomic refcount +1 at +4). Callers: `sub_82417E40` (device getter returning the object at device+12696, called per draw from `sub_821792A0`), `sub_82415F78/FC0`, `sub_82185D80` (4 sites), `sub_82185648`, `sub_82177AC0` | **host**: global-lock idiom (2 recursive_mutex lock/unlock pairs, 3 `mfence`, 1 CAS) plus scope; the guest work is 5 instructions |
| `sub_82415DC8` | 3.62 | 72 insns; calls `__savegprlr_26`/`__restgprlr_26`, `sub_8241A630` (rare) | XDK D3D **SetStreamSource**-style setter: writes a vertex fetch constant at device+(239-i)*8, dirty mask at +24, resource tracking list at +13524 | guest work small; per-call host overhead (3 scopes = 6 `uncaught_exceptions` calls) of the same order |
| `sub_821792A0` | 3.40 | 79 insns, 9 calls | game per-draw submit routine (calls the setters, `sub_8241D620`, `sub_82417E40`) | guest |
| `sub_82415EE8` | 2.56 | 36 insns | XDK D3D **SetIndices**-style setter (device+12436) | as `82415DC8` |
| `sub_822DF960` | 2.18 | 415 insns, 181 VMX | game vector math (transform/culling) | guest |
| `sub_8241D620` | 2.04 | 276 insns, 17 calls | XDK draw packet builder (calls `sub_82412710`, the ring reserve that leads to the `sub_82411E98` wait) | guest |
| `sub_82410F88` | 1.95 | 95 insns | XDK D3D setter (texture/sampler slot) | as `82415DC8` |
| `sub_8218C9D8`, `sub_8218AAF0`, `sub_8219F410`, `sub_8218AC58`, `sub_8218A6E0` | 1.5, 1.2, 1.1, 0.2, 0.2 | 30-78 insns, loops | game scene traversal | guest |
| `sub_8230E8F8`, `sub_8230CF50` | 1.3, 0.2 | about 220 insns, 60 VMX | game vector work | guest |
| `sub_82412F98` (spin pair with `82411E98`) | 0.6 | | GPU ring/fence wait | wait, 0.6-4.5% (known) |
| `__savegprlr_26` / `__restgprlr_26` (`823D91F0`/`823D9240`) | 0.3 each (undercounted) | 7 stores / loads | PPC ABI register save/restore helpers, called by almost every non-leaf function | **host**: each is a separate recompiled function with its own `GuestFunctionScope` |

Imports: `RtlLeaveCriticalSection` is the last import in 84% of samples, `RtlEnterCriticalSection`
8.5%, `KfReleaseSpinLock` 3.9%. Their host implementations
(`<sdk>/src/kernel/xboxkrnl/xboxkrnl_rtl.cpp:384-458`, `xboxkrnl_threading.cpp:951-985`,
`src/system/xthread.cpp:666-677`) are already short atomic fast paths without the global lock; a high
"last import" share means they are called often, not that time is spent in them. **No import fast path
is proposed**; at most `XThread::GetCurrentThread()` plus a cvar read per call, a few ns.

### 2.2 Host overhead H1: `GuestFunctionScope` in every function (largest; already designed in M7)

Every recompiled function starts with `REX_FUNC_PROLOGUE` -> `REX_GUEST_FUNCTION_SCOPE`
(`generated/default/mcla_init.h:63-100`; template `resources/templates/codegen/init_h.inja`). The class
(`context.h:508-534`) calls `std::uncaught_exceptions()` in the constructor and the destructor; in
MSVC's runtime this is a DLL call reaching `__vcrt_getptd` -> `FlsGetValue` with
`GetLastError`/`SetLastError` preservation (M7-016-guest-scope-overhead: 220 of 2,933 process samples
in these helpers on the Deck; 83,559 static call sites; 29,788 function bodies). It is still present in
v0.20: the M7 private "published-scope" candidate (header `3157F9BC...`, calls 83,559 -> 8, reviewed and
built in M7) was never promoted.

On the render thread the hot functions are small XDK setters whose bodies are 30-100 instructions but
which enter 3 scopes each (the function plus `__savegprlr_N` and `__restgprlr_N`), i.e. 6 CRT calls of
about 15-30 ns: the bookkeeping is of the same order as the work. Estimate **5-15% of render-thread
CPU (0.8-2.4 ms/frame at 720p)**; the range is wide because the PC cost of the FLS path was never
measured.

Change: promote the M7 published-scope design (save the previous PC/function on entry, restore after a
normal return, skip the restore during exceptional unwinding; no `uncaught_exceptions`), in the codegen
template or as an SDK `GuestFunctionScope` rewrite, keeping the `current_function`/`guest_pc`
breadcrumbs used by the F10 sampler and crash reports. Second, cheaper step: emit the
`__savegprlr_*`/`__restgprlr_*` (and FPR/VMX save/restore) helpers without a scope, or inline them at
the call sites.
Risk: medium (all generated code; exception and longjmp unwinding semantics; M7 already has 12-case ABI
and vector fixtures passing on Windows and Proton). Test: re-run the M7 witness fixtures, the crash-report
unit test (`tests/unit/system/crash_report_test.cpp`), a forced guest fault to check the crash report
still names the faulting function, F10 sampler output sanity, then the BENCH 720p A/B: render work
ms/frame (target -0.8 ms or better), fps, and a Deck arm.

### 2.3 Host overhead H2: interrupt-disable idiom as a global mutex (`sub_82422160` and 44 more sites)

Codegen (`<sdk>/src/codegen/builders/system.cpp:252-261` `build_mfmsr`, `:320-337` `build_mtmsrd`)
turns `mfmsr rX` into `seq_cst fence + REX_CHECK_GLOBAL_LOCK()` and `mtmsrd r13,1` / `mtmsrd rX,1` into
`seq_cst fence + REX_ENTER/LEAVE_GLOBAL_LOCK()` (`generated/default/mcla_init.h:333-356`).
`REX_CHECK_GLOBAL_LOCK` itself **locks and unlocks** the global `std::recursive_mutex`
(`rex::thread::global_critical_region`, `<sdk>/include/rex/thread/mutex.h:56-83`) only to read a
count; `ENTER` locks it again. Per guest atomic: 2 lock/unlock pairs (each a non-inlined MSVC STL
`_Mtx_lock`/`_Mtx_unlock` call), 3 `mfence`, the CAS. The same mutex is used by shared-memory watch
arm/disarm, texture-cache invalidation, write-fault callbacks and interrupt dispatch.

All 45 such regions in the corpus (33 functions) are pure reservation atomics (checked by script):
15 `lwarx; addi; stwcx.`, 11 `lwarx; add; stwcx.`, 12 `lwarx; cmpw; bne; stwcx.`, 5 `lwarx; stwcx.`,
2 `ldarx; cmpd; bne; stdcx.`. Functions: XAPI `Interlocked*` (`sub_821CE080`-`sub_821CE128`); XDK D3D
`sub_82421CA0`, `82421F38`, `82422160` (AddRef), `824221D8` (Release), `82424530`, `82424EB0`,
`824252A8`, `8242D9E0`, `8242DE08`, `8242EBA0`, `8242F870`, `8242FB88`, `824113B8`, `82411420`,
`82450B58`, `82450C00`; runtime/library code `823EFD30`, `823F3780`, `823F3840`, `8274D1F8`,
`8274D220`, `8274DA40`, `8274E2F0`, `82750EC0`, `82751150`, `82751870`, `827C7410`, `827C78C8`. The CAS
that implements `stwcx.` already makes each region atomic against all other threads. The global lock
adds only exclusion against emulated interrupt dispatch and SDK code, which the hardware does not
provide across cores (an interrupt is delivered to one hardware thread; `mtmsrd` masks only the local
one), so guest code cannot depend on it.

Options, in increasing scope:

1. **Native AddRef/Release via REX_EXTERN** (smallest, host-only, see 2.6).
2. **Codegen idiom elision**: when an `mfmsr rX; mtmsrd r13,1; [l(w|d)arx ... st(w|d)cx.]; mtmsrd rX,1`
   region contains only the reservation pair and register ALU/compare/branch-out, emit it without the
   lock and without the three fences (the CAS is a full barrier on x86), and set `rX` from `ctx.msr`.
   Covers all 45 sites; each can be listed and reviewed. Keep the lock for any other `mtmsrd r13` use
   (there is none in this corpus today).
3. (Independent of 2) make `mfmsr` lock-free: derive the value from the per-thread `ctx.msr` that
   `mtmsrd` already maintains. This halves the lock traffic of any remaining regions. It needs a check
   that `ctx.msr` starts with EE set for every guest thread.

Expected gain: **about 1.0 ms/frame** from AddRef alone (6.2% of 16 ms); **1.0-1.5 ms** with Release
and the XAPI interlocked functions (the latter also appear on worker threads). It also removes render
thread traffic on the global mutex that CP watch work uses (measured lock waits are small today:
90 ms/min on arm, 28 ms per session on faults).
Risk: low for option 1 (two functions, exact semantics reproducible), low-medium for option 2 (a
codegen change, but a closed and checkable set of 45 regions). Test: per-function semantic witness
(refcount and parent recursion, return value, CR0, r31 and caller-visible stack unchanged), then the
BENCH 720p A/B on render work ms/frame and the sampler share of `82422160`/`824221D8`.

### 2.4 Host overhead H3: write faults on GPU-watched pages

Per l53 Run B minute (seq 8, 720p): 393,323 CPU invalidations per minute = **116 write faults per
frame**; handler `write_fault_ms` 3,194 ms/min = 0.94 ms/frame (`state.json` session totals: 159,961
faults, 1.25 s, 7.8 us each, of which callback 6.2 us, query 0.75 us, lock wait 0.18 us). Each fault is
widened to 64 pages (256 KB) by `SharedMemory::MemoryInvalidationCallback`
(`<sdk>/src/graphics/shared_memory.cpp:720-793`, the Xenia 256 KB widening) and re-armed on the next
upload by `MakeRangeValid` (`:455-499`): watch arm 4.08 M calls/min = 1,206 per frame, 0.76 ms/frame on
the CP. The kernel exception trap and `VirtualProtect` (with TLB shoot-down) are **not** in
`write_fault_ms`; the render thread's kernel time is 1.6 ms/frame (5.39 s/min), and faults are its most
likely main source. The split of faults by thread is unknown.

- **First measure (cheap, no behavior change):** count faults and handler microseconds by faulting
  thread (render tid, CP, others) and by top 256 KB block (the watch line already names `1B140000`,
  `17B40000`, `0C740000` as the hottest), and whether each block has texture watches.
- **Then change (medium risk): hot buffer blocks stay unprotected.** For a 256 KB block faulted by the
  CPU in K of the last N frames (e.g. 8 of 8) that has no texture watches and no GPU-written pages, do
  not re-arm protection in `MakeRangeValid` and do not mark it valid; `RequestRange` then uploads the
  exact requested range on every request (correct by construction: the CP reads guest memory at request
  time, as the real GPU reads at draw time). Leave the mode after M frames without requests. This trades
  an 8-15 us fault plus a 256 KB re-upload for an exact-size upload per request.
- Expected gain: up to the fault cost on the faulting threads (0.9 ms handler plus about 0.3-0.8 ms
  kernel time per frame in total; render-thread share unknown) and part of the CP's 0.76 ms arm time;
  CP upload time may rise or fall (exact ranges instead of 256 KB blocks). Net render-thread estimate
  **0.5-1.2 ms/frame if most faults are on it**.
- Risks: texture-cache staleness if a texture lives in such a block (excluded by the texture-watch
  check); GPU-written data (excluded); upload volume for ranges requested many times per frame (cap per
  block per submission, else fall back to protection). The KI-047 work touches the same code
  (read-back/composite ranges): keep those ranges out and gate the mode behind a cvar.
- Test: the per-thread counters before and after; upload MB/s, CP busy, render kernel ms; KI-047 trap
  laps unchanged (no new livery or texture corruption); visual check of traffic and HUD.

### 2.5 Other checks

- Time queries: `mftb` maps to `REX_QUERY_TIMEBASE()`; none of the hot functions above use it.
- MMIO: none in the hot functions (PM4 is written to the ring in guest memory; the CP reads it).
- `MmQueryAddressProtect` is the last import in 0.5% of samples: not hot.
- `NtWaitForSingleObjectEx` 1.4%: the render thread almost never waits (confirmed).
- Untimed spin sites: none in the hot list apart from the known `82411E98`/`82412F98` pair.

### 2.6 Can REX_EXTERN replace hot guest helpers?

Mechanism (`<sdk>/include/rex/ppc/context.h:50-52`; used in `src/mcla_menu_state.cpp:182` and
`src/mcla_perf_split.cpp:221-241`): generated bodies are weak (`REX_WEAK_FUNC`); a host
`REX_EXTERN(sub_X)` is a strong definition that replaces every direct call and can still call the
original as `__imp__sub_X`. Indirect calls resolve through the same function table, so they reach the
override too.

Good candidates (small, self-contained, no guest stack contract beyond the PPC ABI):

| Function | Native version | Notes |
|---|---|---|
| `sub_82422160` AddRef | loop `CAS(be32 refcount at r3+4, +1)`; if the new value is 1, `(type & 0xF) == 4` and bit `0x40000000` of word 0 is set: AddRef `[r3+24]` (iterate instead of recursing); `r3 = new count` | no lock, no fences beyond the CAS; leaves r31 unchanged; r9-r12, CR0 and CR6 are volatile |
| `sub_824221D8` Release | `CAS(-1)`; if the result is 0: the same parent rule with Release, then call the guest `sub_82421AA8(ctx, base)` with `r3 = this`, and return `r3 = 0`; else `r3 = new count` | the destroy path stays guest code; set `ctx.lr` as the original would for the call |
| XAPI `sub_821CE080`-`sub_821CE128` (`InterlockedExchange`, `CompareExchange`, `Increment`, ...) | one host atomic each | hot on worker threads (`821CE0A0` appears in their samples) |

Not candidates: the D3D setters (`82415DC8`, `82415EE8`, `82410F88`) and `8241D620` (device state
layout, dirty-bit and fence-tracking contracts; their gain is mostly the scope overhead, better fixed
by H1), and all game code.

Risk of the overrides: low; they reproduce the guest semantics minus the emulated interrupt mask. They
lose the `current_function` breadcrumb inside those leaf functions (the caller is shown instead),
which is acceptable. Test: a host unit test that runs the guest and native versions on the same
synthetic memory for the parent-recursion and destroy paths, then the BENCH 720p A/B.

## 3. Ranked CPU-side M10 tasks

Gains are for the 720p render-thread wall unless stated. They turn into fps only up to the CP's cap
(busy + wrm_mem about 15.5-16.5 ms today), so the CP items must ship with or right after the
render-thread items.

| Rank | Task | Change | Expected gain at 720p | Risk | Test strategy |
|---|---|---|---|---|---|
| 1 | M10-CPU-A: native AddRef/Release | `REX_EXTERN(sub_82422160)`, `REX_EXTERN(sub_824221D8)` (host atomics; guest destroy path kept) | render about -1.0 ms/frame (6-7% of samples) | low | guest-vs-native semantic unit test; sampler share of both functions; BENCH 720p A/B render work, fps |
| 2 | M10-CPU-B: CP `WAIT_REG_MEM` event wake | wrm counters first (episodes, sleeps, histogram); then `Wait(event, timeout)` instead of `Sleep`, signalled after every interrupt dispatch, behind a cvar | CP -0.5 to -1.7 ms/frame (of 1.93); CP cap from about 62 to about 68-72 fps; no fps on its own | low | counters with the cvar off and on; wrm_mem, CP busy, ring idle, fps, pacing p99; stock-mode cadence unchanged; Deck arm |
| 3 | M10-CPU-C: published function scope | promote the M7 published-scope design (no `uncaught_exceptions`), then scope-free save/restore helpers | render -0.8 to -2.4 ms/frame (5-15%), and every other guest thread | medium | M7 ABI/vector witnesses, crash-report unit test, forced-fault crash report, sampler sanity, BENCH A/B, Deck |
| 4 | M10-CPU-D: interrupt-idiom elision in codegen | no global lock or fences for the 45 pure reservation regions; `mfmsr` from `ctx.msr` | render a further -0.2 to -0.5 ms beyond task 1 (XAPI interlocked, other D3D atomics); less global-mutex traffic for the CP | low-medium | list and review all 45 emitted regions; codegen golden test; BENCH A/B |
| 5 | M10-CPU-E: write-fault attribution, then hot-block policy | per-thread/per-block fault counters (no behavior change); then unprotected hot buffer blocks with exact uploads, behind a cvar | render -0.5 to -1.2 ms/frame if the faults are mostly on it; CP watch arm -0.3 to -0.6 ms | counters none; policy medium (texture and KI-047 interactions) | counters first; A/B on upload MB/s, CP busy, render kernel ms; KI-047 trap laps and visual checks |
| 6 | M10-CPU-F: replay worker batched wake | `spin_us`/`wakes` counters; spin about 128 pauses, wake every 512 commands, forced wake on finish and block seal | no direct fps on PC; frees about 0.5 core; helps laptop power sharing (1440p) and the Deck | low | counters; A/B worker CPU, stream_finish_wait, CP busy, fps at 720p/1440p, Deck |
| 7 | M10-CPU-G: precise guest vblank timer | high-resolution waitable timer in the VSync thread, fallback to the current loop | CP wrm a further 0-0.5 ms; smoother Unlocked pacing | low-medium (guest timing, Proton) | vblank rate log, pacing p50/p99, wrm, stock cadence, Deck |

Combined plausible render-thread saving from tasks 1, 3, 4 and 5: about 2.5-5 ms/frame (16 -> 11-13.5
ms), a render cap of about 75-90 fps; with task 2 the CP cap is about 68-72 fps, after which task 7 and
the CP `p_tex`/`p_bind` work from the 720p document decide the next wall. All estimates come from sample
shares and source structure, not measurements; each task's A/B decides.

## 4. Unresolved risks and open points

- The 1.93 ms "one `Sleep(1)` per frame" reading is inferred from its constancy and the timer
  resolution; the episode counters of task 2 must confirm it before the event wake is credited.
- The swap interval, the vblank-due rule's phase threshold and the callback override (M7 static notes)
  were never measured in Unlocked mode.
- The sampler gives exclusive time per recompiled function and only the top 12 functions per window;
  the `GuestFunctionScope` cost is spread over all functions and not directly visible in it. A host
  sampling profile (ETW/WPR on the PC) of the render thread would size H1 and H3 directly.
- AddRef at 6.2% while Release stays below the top 12 is unexplained (call counts are not logged);
  both are covered by the same change.
- The write-fault split by thread is unknown; H3's gain could fall mostly on other guest threads.
- The replay worker's `PAUSE` latency depends on the CPU model (not logged).
