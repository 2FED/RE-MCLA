# M10-001 instrumentation build l53 (2026-10-31)

Status: **built and unit-tested; not run in the game.** No physical result
exists. `private/l53-internal-prepare.ps1` has been written and parse-checked
but **has not been run** (no ticket signed, nothing staged into
`private/l53-internal`, owner-admitted-builds.txt unchanged). Design:
`docs/evidence/M10-001-harness-design-20261031.md`; owner protocol:
`docs/M10-001-route-protocol.md`.

Implemented by an Opus subagent for the primary agent; the SDK branch is local
and unpushed, the host changes are uncommitted in the admiring-jang-24d485
worktree for the primary's review.

## 1. SDK

- Repository `private/l39-sdk-src`, new local branch `mcla/m10-perf-split`
  from the published v0.20.0.5 (`851f03b`), linked worktree
  `private/l53-sdk-wt` (submodule trees and their `.git` marker files copied
  from l39-sdk-src; submodule pins at 851f03b are identical to l39's).
- Commit **`7892797ec1203a48b3b209e67305c2e074d2f53f`**
  ("gpu(d3d12): optional command-processor cost split, d3d12_perf_split
  (M10-001)"), author Bolodymyr. Not pushed; other `mcla/*` branches untouched.
- Files: new `include/rex/perf/perf_split.h` (pure accounting, window
  arithmetic and line format), `src/core/perf/perf_split.cpp` (runtime state
  in rexruntime: mode flag, render-thread id, render-wait sink, mark queue,
  published snapshot, CPUID invariant-TSC check),
  `tests/unit/core/perf_split_test.cpp`; changed
  `src/graphics/d3d12/command_processor.cpp` and its header,
  `src/graphics/command_processor.cpp` and `include/rex/graphics/command_processor.h`
  (WAIT_REG_MEM memory-poll share, new snapshot field
  `command_wait_reg_mem_memory_us`), `src/graphics/shared_memory.cpp` and its
  header (upload region, request count), `src/graphics/d3d12/deferred_command_list.cpp`
  and its header (worker named "D3D12 Replay", cumulative replay time, worker
  native handle), `src/kernel/xboxkrnl/xboxkrnl_video.cpp` (VdSwap records the
  calling thread), CMake lists. 14 files, 1526 insertions and 17 deletions
  (the three new files are 1206 of the insertions).

### 1.1 Switch

`d3d12_perf_split` (int32, range 0-2, hot-reload, default 0). Read once per
guest frame at frame close (`EndSubmission`, closing frame). 0: off, the
accumulator pointer is null, every probe is one branch on it, no clock read.
1: a xorshift32 picks one draw in 16 for the phase chain (no fixed period, so
no aliasing with draw patterns); all draws are counted. 2: every draw.
Turning it on starts a window; turning it off or changing the mode emits a
`reason=off` / `reason=mode` line first. `MCLA perf split: invariant TSC
yes|no` is logged once at the first enable.

### 1.2 Clock and accounting

`__rdtsc()`; every window calibrates `tsc_mhz` = TSC delta / steady_clock
delta over the window itself. All counters are plain integers on the CP
thread (single writer); the window is computed from cumulative inputs at
emission.

- **Regions** (exclusive): each slow path reads the TSC at entry and exit;
  its exclusive time is its duration minus nested regions, so regions never
  overlap. Regions: `fence_wait` (both `WaitForSingleObject` in
  `CheckSubmissionFence`), `pso_wait` (`AwaitPipeline` inside
  `WaitForPendingPipeline`, only when it really waits), `upload`
  (`SharedMemory::UploadRanges`, CP thread only, with bytes), `copy`
  (`IssueCopy`), `resolve` (`render_target_cache_->Resolve`, both paths),
  `readback_gpu` (read-back buffer setup and GPU copy recording),
  `readback_wait` (the synchronous `AwaitAllQueueOperationsCompletion` calls),
  `readback_memcpy` (with bytes), `submit` (`EndSubmission`),
  `stream_finish_wait` (`FinishStreaming`), `execute`
  (`ExecuteCommandLists`), `present` (`IssueSwap`), `emit` (building and
  logging the line; counted in the next window).
- **Phase chain** (sampled draws): TSC at `IssueDraw` entry (after the
  copy-mode dispatch) and at each phase end; a phase gets its wall time minus
  the region time charged inside it. Phases (v0.20.0.5 line numbers from the
  design): `setup` (to `BeginSubmission`, 2744), `prim` (`Process`, 2750),
  `rt` (`render_target_cache_->Update`, 2781), `pso` (translations,
  `ConfigurePipeline`, the pending-pipeline block; the pipeline-skip return
  closes here), `tex` (`RequestTextures`, 2852), `bind` (to `UpdateBindings`,
  2915), `vb` (vertex fetch and memexport `RequestRange` loop, before
  "Primitive topology", 2998), `record` (to function exit). An early return
  closes the phase in progress. Exit kinds: issued, pipeline skip, other
  early return (`draws_noop`: no-op draws and failures).
- **Window identity**: `cp_busy = wall - ring_idle - wrm_reg - wrm_mem`;
  `packets_other = cp_busy - draw - sum(exclusive regions)`. `draw` is the
  scaled phase sum (`phase x draws / sampled`).
- **Unsampled check**: `draw_packet_ms` is the existing unsampled
  DRAW_INDX scope (steady_clock, every draw packet, including copy-mode
  packets and waits); `draw_regions_ms` is the region time charged inside
  `IssueDraw` on all draws. Offline: `draw_packet_ms ~= draw_ms +
  draw_regions_ms + copy_ms` (+ packet decode); the design's 5% check uses
  this.

### 1.3 Thread CPU and GPU

- CP thread: `GetThreadTimes(GetCurrentThread())` at emission (the emitter is
  the CP thread).
- Replay worker: named `D3D12 Replay` (`set_current_thread_name` at
  `WorkerMain` start); its `std::thread` native handle is used for
  `GetThreadTimes`; `replay_ms` is the cumulative replay time (worker and
  non-streamed `Execute`).
- Render thread: `VdSwap_entry` stores the calling thread's host id and guest
  handle (two relaxed compares per frame, always on). The emitter opens
  (`THREAD_QUERY_LIMITED_INFORMATION`) and caches that thread's handle; if the
  thread changes inside a window, `render_cpu_ms=-1` for that window.
- Render serial wait: the host times `sub_82411E98` (`render_wait_ms`) and
  `sub_82412F98` calls outside it (`render_poll_ms`) and adds TSC ticks to
  the SDK sink, which counts only calls on the VdSwap thread (others go to a
  separate total, not printed).
- GPU busy: the existing per-submission D3D12 timestamp sum, kept as a
  never-reset total (`gpu_busy_ms`, `gpu_submissions`); it lags by the
  completed-fence collection, like the minute line.

### 1.4 Output

One line per window, on the CP thread at frame close, right after the
existing `MCLA gpu busy/min` line when the pacing minute fires
(`reason=minute`), and at the next frame close after a mark (`reason=<tag>`,
for example `f10`; a second mark before that is merged), plus `reason=off`,
`mode`, `exit` (CP `ShutdownContext`). Field order (one line, ms with two
decimals):

```
MCLA_PERF_SPLIT v=1 reason= seq= wall_ms= mode= sample_div= tsc_mhz=
 frames= draws= draws_issued= draws_noop= draws_pso_skip= sampled= copies= submissions=
 cp_cpu_ms= cp_user_ms= cp_kernel_ms= cp_busy_ms= ring_idle_ms= wrm_reg_ms= wrm_mem_ms=
 fence_wait_ms= pso_wait_ms= readback_wait_ms= stream_finish_wait_ms=
 draw_ms= p_setup_ms= p_prim_ms= p_rt_ms= p_pso_ms= p_tex_ms= p_bind_ms= p_vb_ms= p_record_ms=
 draw_packet_ms= draw_regions_ms= request_calls= upload_calls= upload_ms= upload_kb=
 watch_arm_ms= watch_disarm_ms= write_fault_ms=
 copy_ms= resolve_ms= readback_gpu_ms= readback_memcpy_ms= readback_kb=
 copies_now= copies_deferred= copies_skipped=
 submit_ms= execute_ms= present_ms= emit_ms= packets_other_ms=
 replay_cpu_ms= replay_ms= render_cpu_ms= render_wait_ms= render_poll_ms=
 gpu_busy_ms= gpu_submissions= render_tid= render_handle=
```

Differences from the design text: `draws` counts non-copy `IssueDraw` calls
(copy-mode packets are `copies`); phases scale by `draws / sampled` (all
draws that entered the chain, not only issued ones); `copy_ms` and
`readback_wait_ms` are inclusive (the rest is exclusive); `replay_us` is
printed as `replay_ms`; added `draw_packet_ms`, `draw_regions_ms`,
`request_calls`, `render_poll_ms`, `emit_ms`. The mark entry point is
`rex::graphics::MarkPerfSplit(const char*)` (no effect while off), backed by
`rex::perf::RequestPerfSplitMark`.

## 2. Host (admiring-jang-24d485, uncommitted)

- `CMakeLists.txt`: option `MCLA_PERF_SPLIT_DEFAULT` (OFF); new source
  `src/mcla_perf_split.cpp` (always compiled; with an SDK without
  `rex/perf/perf_split.h`, such as the pinned v0.20.0.5 install, it compiles
  to no-ops with no overrides, checked by compiling it against
  `l52-sdk-install`: 0 warnings with -Wall -Wextra, no `sub_8241*` symbols);
  test `mcla_perf_split_test`.
- `src/mcla_perf_split.{h,cpp}`: at `OnPostLoadGraphicsPlugin`, with the
  option ON, sets `d3d12_perf_split=1` through the cvar registry unless the
  user set it (one log line in each case); strong overrides of
  `sub_82411E98` (verified in `generated/default/mcla_recomp.27.cpp`: weak
  `DEFINE_REX_FUNC` alias; loops on `sub_82412F98` while the GPU read
  position has not passed the requested point, i.e. the render thread's
  GPU-serial wait) and `sub_82412F98` (timed only outside sub_82411E98, via
  a thread-local depth; five other call sites); while the split is off each
  override costs one exported relaxed load. A host thread polls the published
  snapshot every 250 ms and, after each new window, logs
  `MCLA_THREAD_CPU v=1 reason= seq= wall_ms= threads= top= thread=<tid>,'<name>',<user_ms>,<kernel_ms> ...`
  (top 8 threads by CPU in the window, from one Toolhelp walk, off the CP
  thread).
- `src/mcla_app.cpp`: calls the above at plugin load, `Mark(reason)` in the
  shared `RequestLiveSnapshot` path for each accepted snapshot (tag = the
  trigger source, `f10` for F10), `StateJson()` into the diagnostic state,
  `Shutdown()` at `OnShutdown`.
- `src/mcla_diagnostics.{h,cpp}`: `state.json` gains `"perf_split": {...}`
  (version, active_mode, seq, last_reason, tsc_mhz, render ids, wall_ms and
  every line field as a cumulative total) when the SDK has emitted a window.
  It reflects the totals **as of the last emitted line**: the F10 line itself
  is emitted at the next frame close, so it is normally not yet included in
  that F10's `state.json` (difference the log lines for exact segments).
- `src/mcla_guest_sampler.{h,cpp}`: `MCLA_GUEST_SAMPLE v=1 thread='<name>'
  tid=<host id> handle=<guest handle> samples=...`; the name falls back to the
  host thread name (`XThreadNNNN (F80004E0)`) when the guest object has none.
- Tests: `tests/mcla_perf_split_test.cpp` (default decision, thread CPU
  deltas, ordering, ties, clamping, sanitizing, JSON), the sampler test (new
  format and a thread-id case), the export policy test (a state with a
  `perf_split` block projects byte-for-byte the same as without it).

## 3. Builds, tests, hashes

Toolchain: VS 2022 Build Tools (CMake, Ninja), LLVM 20.1.8 clang/clang++,
llvm-rc, Windows PowerShell 5.1. Scripts and logs in `private/l53-work/`
(`l53_sdk_build.ps1`, `l53_sdk_test.ps1`, `l53_host_build.ps1`,
`l53_host_build_targets.ps1`, `l53_host_tests.ps1`, `mk_prepare.py`,
`edit_cp.py`, `l53_pathscan.py`, `warn.py`).

- SDK: configured fresh into `private/l53-sdk-b` (output `l53-sdk-o`,
  install `l53-sdk-install`), same options as l50/l51. Full build: 504
  warnings, the normalized set identical to the l52 v0.20.0.5 full build (no
  new warning in any touched file). After the clang-format commit, an
  incremental rebuild and re-test.
- SDK ctest: **1925/1925 passed** (baseline 1912 + 13 new `[perf-split]`
  cases); `unit_tests [perf-split]` 108 assertions in 13 cases;
  `[pipeline-wait]` unchanged and passing.
- Host: configured `private/l53-build` against `l53-sdk-install`
  (REXSDK_VERSION empty; l51 options plus `-DMCLA_PERF_SPLIT_DEFAULT=ON`);
  0 warnings; **37/37 test executables passed** (36 + `mcla_perf_split_test`;
  diagnostic export policy 608 checks, guest sampler 10, perf split 16);
  ast-grep 0.50.0 scan clean, rule tests 3/3; path-string scan of the seven
  inputs clean (one `l52` byte match in mcla.exe machine code, 0xE20C8C,
  `B8 4C 35 32 82` = an immediate guest address, not a path).
- New sources are ASCII.

`private/l53-int-input` (seven files, SHA-256):

| File | SHA-256 |
|---|---|
| mcla.exe | 5526CB6EB6FED6F1A21AF66B50003FE173935EDB38D644F897C757664EE633AC |
| MCLA-Launcher.exe | 58A2670190AF294D55CB20E80555DD7F7B81EB02C29AF26809E6688D0E0867A6 |
| mcla_crash_handler.exe | C5B9D7501DFED0AE8391B06B5414474C68E766D5B734D4FE7E5149765EA0868A |
| mcla_diagnostic_image_helper.exe | D46CE6207389EF30B6ACBA1B95271664142C74F2B094CED8E9045563ED09DFD3 |
| rexruntime.dll | FB4BCCF3A9901A63D270429BBBE5528732EF62F00635060262CCA828BA4E8ACE |
| rexgpu-xenos.dll | 12527FCB917D60850D3F10CAC27600B5E8C6DEBF8A21D28E41ADC439F20AF9FB |
| TracyClient.dll | 109F4DBF68FE3CAD522E97014DC588ECFE00A2304A2A926734C3795832A51CBE |

`private/l53-internal-prepare.ps1` SHA-256
`96124CD1A20E9DE8657F5897D5DD6BC6EF9A99970063BB0CDD86F151E9396C02`
(generated by `l53-work/mk_prepare.py` from the l51 script: seven pins, plain
V1 ticket, stage `private/l53-internal` with the l12 CRT, internal staging
audit, KI-045 admission of l39-l42, l33-sunc and the l53 fingerprint, existing
rows kept). Parse-checked with Windows PowerShell 5.1 and PowerShell 7: 0
errors. Run it with
`private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l53-internal-prepare.ps1`.

## 4. Overhead

Measured (micro-benchmark `private/l53-work/bench/perf_split_bench.cpp`, the
real `PerfDrawChain` with 8 marks per draw, AMD Ryzen AI 7 350, clang -O2
x86-64-v3, best of 7 x 2 M draws, log `l53-overhead-bench.log`):

| Draw body | mode 0 | mode 1 added | mode 2 added |
|---|---|---|---|
| empty | 3.0 ns | +8.8 ns | +72.5 ns |
| about 30 ns of work between marks | 31.8 ns | +0.7 ns | +43.9 ns |

The empty-body figure is the worst case (TSC reads cannot overlap work). One
TSC read here costs about 8 ns. Against the measured 1.16-2.9 us per draw
(PC) mode 1 adds about 0.1-0.8% of CP time; mode 2 about 2-6%, as in the
design. Slow-path regions (uploads, copies, waits, submissions, about
1-100 k events/s) add two TSC reads each: under 0.1% at the design's rates.
The render-wait overrides add two TSC reads per sub_82411E98 call (the
thread is waiting anyway) and, while off, one exported load. Per window: a
few `GetThreadTimes`, one snapshot copy, one log line on the CP thread, and
one Toolhelp walk on a host thread.

Pending (physical): l53 `--d3d12_perf_split=0` against the default 1 on the
same route (fps and `cp_busy`, target under 1% and inside the run-to-run
spread), and one `--d3d12_perf_split=2` run (mode 1 phase shares within 2
percentage points). These need direct scripts (the launcher builds its own
command line), which are not written yet.

## 5. Limits and risks

- Not run: no log line has been seen yet; the first owner log must confirm
  the line, `tsc_mhz`, a plausible `packets_other_ms`, the VdSwap thread
  (`render_tid` matching the `XThread... (F80004E0)` row) and, on the Deck,
  `gpu_busy_ms` through vkd3d.
- `draws_noop` includes failed draws; failures are rare and logged elsewhere.
- A window that ends inside an open region (the minute line is emitted inside
  `IssueSwap` -> `EndSubmission`) carries that region's earlier part into the
  next window; a mark and a minute on the same frame give a second, nearly
  empty window.
- `state.json` `perf_split` lags by one window (section 2).
- Uploads and request counts are timed only on the CP thread; uploads from
  other threads (if any) are not counted.
- Render-wait attribution relies on `sub_82411E98` / `sub_82412F98` being the
  serial wait; other spin sites (if any) stay in render "work".
- The MCLA_THREAD_CPU worker polls every 250 ms whenever the SDK has the
  cvar, also with the split off (no line is logged until a window exists).
- The volunteer configuration is unchanged in behaviour (option OFF, pinned
  SDK without the header: no overrides, no cvar write), but
  `src/mcla_perf_split.cpp` is now compiled into every host build;
  the clean-clone volunteer build was not rebuilt here.
- TracyClient.dll differs from the l50/l51 copy (fresh SDK build tree).
- Spark was not used: the work was source edits and builds with exact
  verification, not a bounded extraction or triage task.
