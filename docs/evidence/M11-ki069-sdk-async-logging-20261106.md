# M11: KI-069 fix in the SDK (asynchronous non-blocking logging, slow-write timer, CP freeze watchdog)

Implementation by an Opus subagent on 2026-11-06 as SDK commit `5207195` on `mcla/m11-content-unmount` (on top of `109116b`, base v0.20.0.8); accepted by the primary agent as code: ctest 2046/2046, no new warnings. Not run in the game; the owner l71 sessions are the acceptance (no multi-second picture stalls, `MCLA_LOG` minute lines with dropped=0 in normal play, `MCLA_CP_FREEZE` lines only on real stalls). Host follow-ups noted: the crash filter should call `FlushLoggingFor(500)`; `FlushLogging` callers must not run on the CP thread.

---

# l71 SDK draft notes: KI-069 (blocked synchronous log write on the GPU-command thread)

Draft by an Opus implementation subagent, 2026-11-06. Implemented and tested on the PC with unit tests
only. No game, launcher, Deck or Proton run exists for this SDK. Nothing is pushed.

## Identity

- SDK repo `private\l39-sdk-src`, worktree `private\l71-sdk-wt`, branch `mcla/m11-content-unmount`.
  The new commit sits on top of the KI-068 commit `109116b`, so one SDK carries both M11-001 and KI-069.
  Commit **`52071957ebd8ed836bddae855f3178e595bb8a10`** (local only, author Bolodymyr).
- Build `private\l71-sdk-b`: Release, same configuration as 109116b. Output `private\l71-sdk-o`,
  install `private\l71-sdk-install`.
- Scripts and logs are in `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l71-log-scripts\`:
  `build.ps1`, `test.ps1`, `b1..b8.log`, `ctest-final.log`, `warn-new.txt`, and the edit scripts.
- SHA-256 of the installed binaries (`private\l71-sdk-install\bin`):
  - `rexruntime.dll` `5cca90ba455dbc414702bbed6176067e06d1fa2bd8b496e2ca5148bf003fcbf6` (11,234,304 bytes)
  - `rexgpu-xenos.dll` `6eab36db2e03e26063c251e1d047d77a48ba3d6091b830b5848949472e18773e` (3,232,768 bytes)
  - `rexglue.exe` `806b6a2ceae386281b9a507f4e5086835dc0ce6833c46e1ec4f62b30e3892d4e`
  - `TracyClient.dll` `30e8aa912f64d638ccc3e67c25ef6be03d36151901d9a72bd6f21472c2bfd37a` (unchanged)
  - The previous l71 (109116b) `rexruntime.dll` was `1eb4b1c9...420a`. The l71 host has to be relinked.

## 1. What the logger was

- `src/core/logging.cpp` (at 109116b) gave every category its own `spdlog::logger`.
- Each logger held the default sinks directly:
  - `msvc_sink_mt` (the early sink, OutputDebugString; kept on Windows)
  - optional `stdout_color_sink_mt`
  - `rotating_file_sink_mt` (mcla_NNN.log)
  - every `AddSink` sink. The host adds two: the `LogCaptureSink` and the runtime journal
    `rotating_file_sink_mt` (host `src/mcla_diagnostics.cpp:1150-1163`).
- Every logger had `flush_on(LogConfig::flush_level)`, which defaults to `info` (`types.h`).
- As a result, every info line was formatted, written and flushed on the calling thread, under each
  sink's mutex. That includes the livery diag v3 lines from inside IssueCopy on the CP thread.
- `log_flush_interval` (0 = off) existed but was redundant.

## 2. What it is now

### Dispatcher sink: `LogDispatchSink`, `src/core/logging.cpp:107`

- **Sink layout.** `InitLogging` (`:769`) creates one dispatcher, and every logger's sink list is just
  that dispatcher. The old default sinks and every `AddSink` sink become its downstream list, a
  copy-on-write `shared_ptr<vector>`.
  - `AddSink`, `RemoveSink` and `ReplaceConsoleSink` edit the downstream list.
  - `RemoveSink` first drains (bounded 2 s), so the host journal still receives its queued lines before
    it is removed.
  - Per-category sinks (`AddSink(category, ...)`, `LogConfig::category_sinks`) stay direct and
    synchronous; nothing in SDK or host uses them.
  - Before `InitLogging` (the early phase) loggers keep using the early sink directly.
- **Async mode (`log_async=true`, the default).** `log()` (`:126`) copies the message into a
  `spdlog::details::log_msg_buffer`, which keeps the original timestamp, thread id, level, logger name
  and source location. It pushes the copy into `BoundedLogQueue` under a short mutex and returns. It
  never waits for space.
  - **Overflow policy** (`include/rex/logging/async_queue.h:55`, pure): a full queue (lines or bytes)
    drops a new line below warn and counts it, which keeps the queued run contiguous. A warn-or-above
    line evicts the oldest lines instead and counts them. An important line always fits an empty queue.
  - **Bounds:** `log_async_queue_lines` (default 16384, range 256-1048576) and `log_async_queue_mb`
    (default 16, range 1-256). Each line's cost is its fixed entry (`sizeof` the queued line, about
    0.3-0.4 KB including spdlog's 250-byte inline buffer) plus payload plus logger name.
  - **Writer thread "Log Writer"** (`WorkerMain`, `:457`): takes the whole queue at once and writes the
    lines in queue order to each downstream sink. This keeps per-thread order and the global push order.
- **Flush points (async):**
  - every `log_async_flush_ms` (default 1000, range 100-10000) when anything was written
  - at once after a batch containing warn/error/critical
  - on a logger `flush()` call: a non-blocking request (`flush_on` is raised to warn in async mode by
    `EffectiveFlushLevel`, `:585`)
  - `FlushLogging()`: bounded drain, at most 2 s (was unbounded)
  - after a critical line, the producer waits at most 0.5 s for it to reach disk. This is the only
    producer wait; it covers `REX_FATAL` + abort.
  - `ShutdownLogging()`: stop, drain and join
  - process exit: a static `LogExitFlusher` (`:577`) writes what is left on the exiting thread, using
    try-lock only
  - new `FlushLoggingFor(timeout_ms)` (`:856`): bounded drain with no registry lock, then a try-lock
    emergency drain. It is meant for crash handlers. **The host's `LastChanceExceptionFilter`
    (`src/mcla_native_crash_win.cpp`) does not call it yet. Recommended host change:
    `rex::FlushLoggingFor(500)` at the start of the filter.**
- **Sync mode (`log_async=false`, init-only cvar).** The producer writes under one write lock and the
  logger's `flush_on(info)` flushes synchronously, as before. Timing, slow lines and minute lines still
  work.
- **Pattern and format.** The downstream sinks keep their own patterns. File:
  `[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] [t%t] %v`. Journal: without `[t%t]`. The timestamp and `t%t` are
  those of the producing thread, at the time the line was created. The dispatcher ignores
  `set_pattern`/`set_formatter`.

### Slow write/flush timer: `SinkTiming`, `async_queue.h:168`

- Every single downstream `log()` or `flush()` is timed with steady_clock. That covers the writer
  thread in async mode, and the producer under the write lock in sync mode.
- A write or flush over 100 ms gives a warn line such as
  `MCLA_LOG slow write: ms=5530.0 thread=<producer tid> bytes=<payload> suppressed=<n>` or
  `MCLA_LOG slow flush: ms=.. thread=0 bytes=0 suppressed=..`.
- At most one slow line per second is logged; the rest are counted in `suppressed=`.
- The slow line is logged after the write lock is released (`ReportSlow`, `:394`), so it is
  asynchronous in async mode. In sync mode the rate limit bounds the recursion.

### Minute line: `EmitMinute`, `:425`

The writer thread emits it every 60 s:

`MCLA_LOG v=1 kind=minute mode=async lines=.. dropped=.. dropped_total=.. queue_max=.. queue_max_kb=.. writes=.. flushes=.. write_max_ms=.. flush_max_ms=.. slow=..`

At init:

`MCLA_LOG v=1 kind=config mode=async queue_lines=16384 queue_mb=16 flush_ms=1000 flush_level=warning`

### New API (`include/rex/logging/api.h`)

- `FlushLoggingFor(uint32_t)`
- `GetLogRuntimeState()`. Lock-free: active, async, queue_lines, dropped_total, slow_total, plus the
  sink operation in progress (op_busy_ms, op_thread, op_flush) and writer_thread.
- Cvar declarations for `log_async`, `log_async_flush_ms`, `log_async_queue_lines` and
  `log_async_queue_mb`.

## 3. CP freeze watchdog (D3D12)

- **Cvars:**
  - `d3d12_cp_freeze_watchdog_ms`, default 1000, 0 = off, hot (`command_processor.cpp:139`)
  - `d3d12_cp_freeze_stack`, default true
- **What the CP thread publishes** (relaxed atomics, a few stores per copy and per frame):
  - frame counter and last-frame time, at each frame close in `EndSubmission` (`:5217`)
  - IssueCopy state: active, sequence, start time, resolve address/length (`:4298`)
  - its thread id and stack limits (`SetupContext`, `:992`)
- **Perf-split scope stack.** `rex::perf::CpScopeStack` (`include/rex/perf/perf_split.h:143`) is pushed
  and popped by `ScopedPerfRegion` (`:323`), only on the thread marked by `MarkCpScopeThread`, and only
  while `d3d12_perf_split` is on (the volunteer default is 1). It gives the region path, for example
  `copy>readback_wait`, and the time in the innermost region.
- **CP wait state.** The base CP publishes `cp_wait_state_`: busy, ring_idle, or wrm
  (`src/graphics/command_processor.cpp:377/394/1289/1317`).
- **Watchdog thread "GPU CP Watchdog"** (`FreezeWatchdogMain`, `:3378`). It polls every threshold/4
  (50-250 ms). With no frame for longer than the threshold it logs one begin line per episode at warn,
  which is flushed promptly:
  `MCLA_CP_FREEZE v=1 kind=begin episode= stall_ms= frames= cp_tid= cp_state=busy|ring_idle|wrm perf_split= scope=copy>.. scope_depth= scope_ms= copy_active= copy_seq= copy_ms= copy_addr= copy_len= log_mode= log_queue= log_dropped= log_op=none|write|flush log_op_ms= log_op_thread= log_writer=`
  When frames resume it logs `MCLA_CP_FREEZE v=1 kind=end episode= episode_ms= frames=`.
  Nothing is logged before the first frame.
- **Stack scan** (`FreezeStackScan`, `:3306`), once per episode:
  - Suspends the CP thread, reads its context, copies up to 32 KB from RSP into a buffer allocated
    beforehand, then resumes it. Nothing that takes a lock runs while the thread is suspended.
  - After the resume, it logs
    `MCLA_CP_FREEZE v=1 kind=stack episode= status=ok rip=<module+off> words= scan=<up to 24 module+off>`,
    listing only the stack words that point into executable image pages.
  - It is a scan, not an unwind, so stale return addresses can appear. Offsets can be symbolized
    offline with the PDBs. In sync log mode it would show, for example, `ntdll.dll+..` /
    `rexruntime.dll+..` for a blocked `NtWriteFile`/`NtFlushBuffersFile`.
- **Episodes.** "Loading screens" or any guest stall without swaps also form episodes. `cp_state=ring_idle`
  marks them as guest-side.

## 4. Tests

- **`tests/unit/core/log_async_test.cpp`**:
  - 11 pure cases: queue order, drop-newest by count and by bytes, eviction of the oldest by important
    lines, the oversized important line, high-water windows, zero limit, slow threshold (100 ms is not
    slow, above it is), slow rate limit and suppression count, window reset, minute-line format.
  - 2 dispatcher end-to-end cases (one process each under ctest):
    - Real file sink plus a sink that sleeps 150 ms on one line.
    - Async: the producer returns in under 100 ms, the slow-write line and the config line reach the
      file, and 4 threads x 500 lines arrive complete and in per-thread order.
    - The same with `log_async=false`: the producer is blocked for 150 ms or more, as expected.
- **`tests/unit/graphics/cp_freeze_watchdog_test.cpp`**, 7 cases: no episode before the first frame;
  one begin and one end per episode, durations, second episode; threshold 0; begin/end line format;
  scope stack path, innermost time, depth overflow; region names.
- **ctest:** 100% of 2046 passed (2026 at 109116b plus 20 new); the same 4 BitStream skips. Log:
  `ctest-final.log`.
- **Warnings:** no new ones. Compared with `l71-scripts\warn-l71.txt`, the only extra entry is the
  existing `FunctionDispatcher` delete warning, which appears in the 109116b build logs too. A
  `sscanf` deprecation in the new test was removed.
- **clang-format:** clean on all new files and changed hunks. Untouched pre-existing hunks in
  `command_processor.cpp/.h` were left alone.
- **Pitfall found and fixed during the build.** At first, `InitLogging` logged through `log::core()`
  while holding the registry mutex. The lazy category registration then re-locked it, and rexglue.exe
  aborted with 0xC0000409 in `recompile-tests`. The dispatcher now logs through
  `spdlog::default_logger_raw()`, which is set to "core" by InitLogging.

## 5. Risks

- **Ordering across threads.**
  - The file keeps the global order of the queue pushes, and per-thread order exactly.
  - Timestamps are taken before the push, as before. Two threads can therefore still show slightly
    non-monotonic times.
  - Lines now reach the files up to about 1 s later; warn and above are flushed at once.
  - The capture sink, the overlay and the host journal see lines asynchronously.
- **Lost lines at a hard crash.**
  - Lines still queued when the process dies are lost unless something drains them first. Those are
    lines from the last write batch, or up to 1 s of info lines if the writer is behind.
  - Mitigations: warn and above flush at once; critical waits up to 0.5 s; `FlushLogging` before
    `_Exit` (OnClosing, the guest exception path, host paths); the exit-time emergency drain.
  - **A native access violation goes through the host filter, which should call `FlushLoggingFor(500)`
    (host change, not done here).**
  - If the writer itself is blocked in a disk write at crash time, the emergency drain cannot take the
    write lock and skips.
- **Memory bound.** At most `log_async_queue_lines` (16384) lines and `log_async_queue_mb` (16 MB) are
  queued. At the normal 20-60 lines/s that is several minutes of a completely blocked disk before any
  drops. Drops are counted in the minute line and in the watchdog line.
- **Blocking left.**
  - Producers take a short queue mutex, held only for a push or a swap with the writer. It never waits
    on I/O.
  - `FlushLogging` (2 s), `RemoveSink` (2 s) and critical lines (0.5 s) can wait, all bounded. The host
    calls `FlushLogging` at F10 / exit paths: check that none of them runs on the CP thread.
- **Watchdog stack scan.**
  - `SuspendThread`/`GetThreadContext` on the CP thread happen once per episode. On Windows they are
    safe with the copy-only window.
  - Under Wine/Proton the suspend is signal-based and has not been exercised here.
  - `d3d12_cp_freeze_stack=false` disables it; `d3d12_cp_freeze_watchdog_ms=0` disables the whole
    watchdog.
- **Scope stack accuracy.** It is only populated with `d3d12_perf_split` on. An explicit out-of-order
  `ScopedPerfRegion::End()` would mislabel the path; it would not corrupt anything.
- **Default change for every SDK consumer.** `log_async` defaults to true, and `log_flush_interval` is
  now mostly redundant. Anyone relying on "info line is on disk when the call returns" needs
  `FlushLogging()` or `log_async=false`.

## 6. What to look for in the next owner/volunteer log

- One `MCLA_LOG v=1 kind=config mode=async ...` line at start.
- `MCLA_LOG v=1 kind=minute ...` every minute: `dropped=0`, small `queue_max`, `flush_max_ms`.
- A disk stall like the 12:22:50 one should now show:
  - `MCLA_LOG slow flush|write: ms=5xxx ...` from the writer thread
  - frame pacing without a 5.5 s CP gap
  - no `MCLA_CP_FREEZE` line, or, if the CP still stalls, a `MCLA_CP_FREEZE ... kind=begin` line with
    scope, copy and log state plus a stack scan
