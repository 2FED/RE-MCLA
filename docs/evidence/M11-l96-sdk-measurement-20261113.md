# l96 SDK: Deck command-processor census and lever-1 switches (2026-11-13)

Implementation by an Opus subagent for the primary agent, from
`docs/evidence/M11-deck-cp-budget-and-levers-20261112.md` (the counters proposed for a measurement
build and lever 1). SDK commit `ccac775` (`ccac775114e77186b7e6be6d6e6f5675a8454eda`) on the local
branch `mcla/m11-l96`, parent `f9f5178` (l95); worktree `private/l96-sdk-wt`. It is not pushed.
Built, ctest-ed and installed to `private/l96-sdk-install` (CMake version
`0.20.0.7-dev.gccac775`). **Unit-tested only: no game run, no physical PASS; nothing about the
Deck or the PC has been measured with it yet.** The census is always on (it reports while
`d3d12_perf_split` is on, which the owner builds default to); every switch is off by default.
Nothing was taken from GPL forks or LARecomp; no code was ported from Xenia Canary for l96.
Spark was not used: source work checked by deterministic unit tests.

## 1. Files

New: `include/rex/graphics/util/cp_packet_census.h` (pure helpers: classes, sampler, fence poll
gate, draw identity, IM_LOAD memo, constant store, schedstat parse, the two new line formats),
`tests/unit/graphics/cp_packet_census_test.cpp` (16 cases). Changed:
`include/rex/perf/perf_split.h` (packet classes, `PerfPacketCounters`, appended fields),
`include/rex/graphics/command_processor.h` + `src/graphics/command_processor.cpp` (census in the
packet loop, packet-boundary poll hook, TSC draw-packet path),
`include/rex/graphics/d3d12/command_processor.h` + `src/graphics/d3d12/command_processor.cpp`
(cvars, fence gate, register / binding / constant-buffer census, IM_LOAD memo, the lines),
`include/rex/graphics/pipeline/render_target/cache.h` (`LastUpdateRenderTargetKey`),
`tests/unit/CMakeLists.txt`. Diff: 9 files, 2085 insertions, 19 deletions.

## 2. Measurement (always on)

Counting is unconditional (plain integer increments on the CP thread). Timing runs only while
the perf split is on (`packet_perf_` set together with `perf_split_active_`); it uses the TSC
(`rex::perf::ReadTsc`) and is exclusive of the perf-split regions charged inside a packet (the
region time is the accumulator's `charged` delta). Classes timed on **every** packet: draw,
WAIT_REG_MEM, swap. Classes timed on a **1-in-16** xorshift sample (`CpPacketSampler`): type-0,
type-1, IM_LOAD, constants, event writes, other type-3. INDIRECT_BUFFER is counted, never timed
(its time is the time of its nested packets, which are counted and timed on their own). Sampled
milliseconds are scaled by packets / timed packets, like the draw phases.

### 2.1 MCLA_PERF_SPLIT v=1: appended fields (after `bulk_runs`, before `render_tid`)

All are per window (minute or F10 mark), deltas of cumulative counters; `_ms` fields use the
window's TSC rate.

| field | definition |
|---|---|
| `pkt_t0`, `pkt_t1`, `pkt_t2`, `pkt_t3` | PM4 packets by header type (bits 30-31), excluding zero dwords |
| `pkt_zero` | zero dwords (a type-0 header of value 0, skipped before the type dispatch) |
| `pkt_sampled` | timed packets of the sampled classes (t0, t1, imload, const, event, other) |
| `pkt_draw_ms` | DRAW_INDX + DRAW_INDX_2 packets, every packet timed, exclusive of regions |
| `draw_pkt_tsc_ms` | the same packets, inclusive of the regions inside them |
| `draw_pkt_outside_ms` | `pkt_draw_ms - draw_ms` (`draw_ms` = the scaled draw phases): draw-packet time outside the draw phases (packet decode, VGT register writes, `IssueDraw` entry before the phase chain, `IssueCopy` dispatch); it replaces the truncating microsecond `draw_packet_ms` for this purpose. Both terms exclude regions; the phases are sampled, so a small negative value is sampling noise |
| `pkt_t0_ms`, `pkt_t1_ms` | type-0 / type-1 packets (sampled, scaled) |
| `pkt_imload_ms` | IM_LOAD + IM_LOAD_IMMEDIATE (sampled, scaled; contains `im_load_hash_ms`) |
| `pkt_const_ms` | SET_CONSTANT, SET_CONSTANT2, LOAD_ALU_CONSTANT, SET_SHADER_CONSTANTS (sampled, scaled) |
| `pkt_wrm_ms` | WAIT_REG_MEM, every packet timed, **including** its waits (`wrm_reg_ms + wrm_mem_ms`) |
| `pkt_event_ms` | EVENT_WRITE, _SHD, _EXT, _ZPD (sampled, scaled) |
| `pkt_swap_ms` | XE_SWAP, every packet timed, exclusive of regions (the present and the frame-closing submission are regions) |
| `pkt_other_ms` | every other type-3 opcode (sampled, scaled) |
| `fence_polls` | `ServiceDeferredFenceWrites` calls = completed fence value reads of the deferred fence service (all callers) |
| `fence_polls_pkt` | of them at PM4 packet boundaries (through the gate) |
| `fence_polls_draw` | of them at draws (`IssueDraw`, through the gate) |
| `fence_polls_skipped` | optional poll points skipped by `cp_fence_poll_interval` (0 while it is 0) |
| `fence_poll_pkt_ms`, `fence_poll_draw_ms` | the cost of those two sources (1-in-16 sample, scaled, exclusive of regions such as an early submission); the per-draw polls are inside `p_setup_ms` |
| `packets_unattributed_ms` | `packets_other_ms - (pkt_t0 + pkt_t1 + pkt_imload + pkt_const + pkt_event + pkt_swap + pkt_other)_ms - draw_pkt_outside_ms - max(0, pkt_wrm_ms - wrm_reg_ms - wrm_mem_ms) - fence_poll_pkt_ms`: what the census still does not explain (packet header read and dispatch, ring and indirect-buffer loops, the idle loop outside its waits, frame-close work outside regions, the census's own clock reads) |
| `im_loads` | pointer-based IM_LOAD packets |
| `im_load_imm` | IM_LOAD_IMMEDIATE packets |
| `im_load_dw` | ucode dwords of both |
| `im_load_hashes` | XXH3 hash + shader map lookups (`PipelineCache::LoadShader`); fewer than the loads when the memo hits |
| `im_load_hash_ms` | their cost (1-in-16 sample, scaled to `im_load_hashes`) |
| `im_memo_hit`, `im_memo_miss`, `im_memo_inval`, `im_memo_mismatch` | the IM_LOAD memo (section 3.3); all 0 while it is off |
| `alu_dw_written` | float (ALU) constant dwords written (0x4000-0x47FF; per-register and bulk paths) |
| `alu_dw_unchanged` | of them, writes that left the stored value unchanged |
| `cb_uploads`, `cb_upload_kb` | constant-buffer uploads in `UpdateBindings` (system, float VS, float PS, bool/loop, fetch) and their requested bytes |
| `draws_ident` | draws that reached the end of the bindings phase (the identity point) |
| `draws_state_same` | of them: no write to a draw-state register since the previous identity draw and the same active vertex and pixel shaders |
| `draws_value_same` | of them: no draw-state register **value change** since the previous identity draw (writes of the same value allowed) and the same shaders |
| `draws_pso_same` | same pipeline handle as the previous identity draw |
| `draws_rt_same` | same render-target key (bound depth/colour bits, their formats and the render-target objects of the last render-target cache update; on the pixel-shader-interlock path the key is constant and the render targets are register state) |
| `draws_tex_same` | `UpdateBindings` wrote no texture or sampler descriptor for the draw (bindful: no texture/sampler descriptor write; bindless: neither stage's descriptor-index buffer rewritten) |
| `draws_const_only` | `draws_value_same` draws with a shader-constant write since the previous identity draw (the candidates of a constants-only fast path) |
| `cp_mcycles` | `QueryThreadCycleTime` of the CP thread, millions of cycles |

**Draw-state register ranges (exact).** Draw state = the context registers `0x2000..0x3FFF`
without the four the draw and event packets write themselves (`VGT_EVENT_INITIATOR 0x21F9`,
`VGT_DMA_BASE 0x21FA`, `VGT_DMA_SIZE 0x21FB`, `VGT_DRAW_INITIATOR 0x21FC`). Shader constants =
`0x4000..0x4927` (float `0x4000..0x47FF`, fetch `0x4800..0x48BF`, bool `0x4900..0x4907`, loop
`0x4908..0x4927`). Registers below `0x2000` (scratch, coherency, gamma, CP control) do not count.
The dirty flags are set in `D3D12CommandProcessor::WriteRegister` (every per-register write,
value compared before the store), in the three constant-block bulk branches of
`WriteRegistersFromMem`, and before the l82 bulk run path (`NoteDrawStateRange` compares the
range's draw-state dwords with the register file). The flags reset at every identity draw.

### 2.2 MCLA_CP_PACKETS v=1 (after each MCLA_PERF_SPLIT line, same reason and seq)

`MCLA_CP_PACKETS v=1 reason=<r> seq=<n> fence_poll_interval=<N> draw_packet_tsc=<0|1>
im_memo=<0|1> im_memo_verify=<0|1> t0= t1= t2= t3= zero= draw_packet_tsc_packets=
cls=<class>:<packets>:<timed>:<ms>,... ops=<hex>:<name>:<count>,... sched=<run_ms>:<wait_ms>:<slices>|na`

- `cls`: the ten classes `t0,t1,draw,imload,const,wrm,event,swap,indirect,other` (ms as in the
  split fields; `indirect` is always `:0:0.00`). Class counts exclude predicated-out packets.
- `ops`: every type-3 opcode with a non-zero count in the window (including predicated-out
  packets), highest count first; names from the PM4 table (`?` for an unnamed opcode); `-` when
  none. This makes `pkt_other_ms` attributable opcode by opcode.
- `sched`: the CP thread's Linux schedstat (`/proc/thread-self/schedstat` read as
  `Z:\proc\thread-self\schedstat` from the CP thread at each emission): time on CPU, time waiting
  on a run queue (preemption / SMT contention) and timeslices in the window. Only under Wine
  (`rex::content_log::RunningUnderWine()`); on Windows, or after any open / read / parse failure,
  `sched=na` (one `MCLA_CP_PACKETS v=1 kind=sched_unavailable error=<Win32 error>` line, then it
  stops trying). **Unverified:** that Proton maps `Z:` to `/` on the Deck and that the open
  resolves `thread-self` to the calling thread (the run time can be compared with `cp_cpu_ms`).
  Context switches on Windows are not available cheaply and are left out.

### 2.3 MCLA_CLOCK v=1 (once, at the end of `SetupContext` on the CP thread)

`MCLA_CLOCK v=1 qpc_ns= steady_ns= rdtsc_ns= tsc_invariant= qpf= tsc_mhz= calls=100000 wine=`:
nanoseconds per call of `QueryPerformanceCounter`, `std::chrono::steady_clock::now` and `rdtsc`
over 1e5 calls each (timed with `steady_clock` around each loop), CPUID invariant TSC,
`QueryPerformanceFrequency`, the TSC rate over the benchmark. Cost: about 3e5 clock calls once at
start-up (a few ms; more if QPC is a system call under Proton, which is what it measures).

### 2.4 Cost of the census

Per packet: one counter increment (and an opcode and class increment for type 3), one xorshift
step for a sampled class while the split is on; two `rdtsc` per timed packet (every draw,
WAIT_REG_MEM and swap packet, 1 in 16 of the others). The ALU constant bulk branch stores through
a swap-compare-store loop instead of `copy_and_swap` (same stores). The bulk run path (l82, off by
default) compares the draw-state part of each range once. Not measured; estimated well under
0.1 ms per frame on the Deck for about 6000 draws.

## 3. Lever-1 switches (default off; GPU/D3D12, hot reload, latched at every frame start)

One line `MCLA_CP_LEVER v=1 fence_poll_interval= fence_poll_max_us=20 draw_packet_tsc= im_memo=
im_memo_verify= frame=` at setup and at every change.

### 3.1 `cp_fence_poll_interval` (int 0..4096, default 0)

0 = before l96: the deferred fence service polls the submission fence at every PM4 packet
boundary and every draw while deferred fence writes are pending. N > 0: those two optional poll
points go through `FencePollGate`: a point polls when N optional points have passed since the
last poll, or when 20 us (TSC; checked on every 8th skipped point) have passed. Required points
always poll and restart the count: every WAIT_REG_MEM poll iteration, the ring-idle loop
(`OnIdlePoll`, `WaitIdleForDeferredMemoryWrites`) and, while the gate is on, the guest swap
(`IssueSwap`, a flush-only service). Paths that wait for completion flush on their own
(`FlushDeferredFenceWrites` after GPU waits, unchanged). Correctness: a skipped poll only delays
the completed-value read; `FlushDeferredFenceWrites` stores a fence only when its submission is
complete, so a fence can be stored later, never earlier. The per-draw point may still end the
open submission early (`may_submit`), only less often. Engagement: `fence_polls_skipped`.
Host key `cp_fence_poll_64` writes 64.

### 3.2 `cp_draw_packet_tsc` (bool, default false)

Consumers of the draw-packet diagnostic read first: `command_draw_packet_count/_us` feed
`MCLA_PERF_SPLIT draw_packet_ms` and the host's F10 runtime JSON and read-back cost sample. With
the switch on, `DRAW_INDX` / `DRAW_INDX_2` skip the `ScopedPerformanceDiagnostic` (two
`steady_clock::now` per draw packet); the census timer's TSC duration is added to a tick counter
and the count, and the snapshot converts the ticks to microseconds with a TSC rate calibrated
since the processor was created, so every consumer keeps working. Engagement:
`draw_packet_tsc_packets` in MCLA_CP_PACKETS.

### 3.3 `shader_im_load_memo` (bool, default false) and `shader_im_load_memo_verify` (bool, default false)

`D3D12CommandProcessor::LoadShader` for pointer-based IM_LOAD only (the base handler marks the
call; IM_LOAD_IMMEDIATE has a ring-buffer address and is never memoised): a 4096-entry
direct-mapped table keyed by (shader type, guest address, dword count) holds the shader the
XXH3 hash lookup returned and 8 guard dwords of the ucode (positions `(n-1)*i/7`, i = 0..7: the
first, the last and 6 between). A hit returns the stored shader without hashing; a key match
whose guard differs is `im_memo_inval` (rehashed, entry replaced); otherwise `im_memo_miss`. The
table is cleared when the switch turns on and at `ShutdownContext` (the shaders belong to the
pipeline cache, which lives until then). **No cheap memory generation exists for guest memory**
(a write watch would need `VirtualProtect` arming, 16 us per call on the Deck), so ucode rewritten
in place with the same length and the same 8 guard dwords would be missed: with
`shader_im_load_memo_verify` every hit is still hashed, the hashed shader is used, and a
difference is counted (`im_memo_mismatch`) and fixes the entry. The verify arm is the correctness
check; only the non-verify arm saves the hash.

## 4. Tests, build, install

- `tests/unit/graphics/cp_packet_census_test.cpp`, 16 cases (`[l96]`): opcode classes and names;
  the sampler rate; the fence gate (interval 0 polls everywhere without reading the clock;
  interval N never skips more than N-1 optional points, for N = 1, 2, 7, 8, 64, 100; required
  points always poll and restart the count; the time bound; a queue model with
  `DeferredFenceWriteQueue`, random GPU progress and random WAIT_REG_MEM packets: no fence stored
  before its submission completed, and a completed fence stored within N-1 optional points);
  the memo (hit / miss / invalidation on a guard change / other type, address and length /
  clear / zero length; the change outside the guard missed without verify and found, returned
  and fixed with verify); draw identity (ranges, the counting sequence, bulk range compare with
  byte swap); the swapped constant store; schedstat parsing; the MCLA_CLOCK and
  MCLA_CP_PACKETS texts; the perf-split window computation of every new field (scaling,
  `draw_pkt_outside_ms`, `packets_unattributed_ms`), each new key exactly once, field order, the
  totals.
- Build: fresh `private/l96-sdk-b` (Ninja Multi-Config, clang 20.1.8, the l95 options), output
  `private/l96-sdk-o`; compiled at the first attempt. Warnings: normalized set 218 entries,
  **0 new**, 1 gone against l95 (219): the `unused parameter 'guest_address'` of
  `D3D12CommandProcessor::LoadShader` (the memo uses it now).
- ctest: **100 % passed, 0 failed of 2257 (2253 passed + 4 skipped BitStream; l95: 2237 + 4;
  +16 new)**, before and after install.
- `git-clang-format` against `f9f5178`: applied (wraps in four files; the field macro's
  backslash column), then clean; `git diff --check`: clean.
- Install `private/l96-sdk-install`: CMake prints `0.20.0.7-dev.gccac775`.

| bin file | Bytes | SHA-256 |
|---|---|---|
| rexruntime.dll | 11674624 | 2CC5143E1A1964F5D3CB3EB2BFFC1F9C292C664665F3760E8DF77C4313C03676 |
| rexgpu-xenos.dll | 3513856 | B0C2B4E8613A7F640A4FAD9939A16D1AE46FBDB10A9A7E33BDCC9F80F69A6E04 |
| TracyClient.dll | 246784 | 43D8F06C0F700655A461AD65FFEC4EC1B2154DAB81DA9BFFF642BC6AE2571930 |
| rexglue.exe | 4113408 | 0CF30CA1FB8AB480D1C3587FF3F76F8E7434889B3D410AA718F5F1BD0ACE5046 |

File set against `l95-sdk-install`: l95 1233 files, l96 1234; 1 added
(`include/rex/graphics/util/cp_packet_census.h`), none removed; 26 differ (the 4 bin files, the
4 changed public headers, `rex/version.h`, the 2 CMake config files, 12 static libraries, 3 `.pc`
files). rexruntime.dll grows by 1536 bytes (the larger `PerfSplitCounters` / `PerfSplitWindow`
in the perf-split runtime), rexgpu-xenos.dll by 45568 bytes. TracyClient.dll and rexglue.exe have
no source change; their bytes differ in the fresh build tree (as earlier fresh SDK trees).

**ABI:** `PerfSplitCounters`, `PerfSplitInputs` and `PerfSplitWindow` grew (installed header);
a host must be rebuilt against this SDK (it is, for l96).

## 5. Not done / limits

- Per-thread context switches on Windows: not available cheaply (left out); only
  `QueryThreadCycleTime` (`cp_mcycles`) and, under Wine, schedstat.
- No packet timing of INDIRECT_BUFFER itself; the census's own clock reads land in
  `packets_unattributed_ms`.
- The memo has no write tracking (section 3.3); the default-off verify arm is the check.
- Nothing has run on the Deck or the PC: the counters, the Wine schedstat read, the gate's effect
  and the memo hit rate are all unobserved.

Scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\l96-work`: `copy_tp.ps1`,
`edit_lib.py`, `edit_base_h.py`, `edit_base_cpp.py`, `edit_d3d12_cpp.py`, `fix_test1.py`,
`sdk-build.ps1`, `sdk-test.ps1`, `sdk-install.ps1`, `normwarn.py`, `fileset_diff.py`,
`sdk-commit-msg.txt`; logs `copy_tp.log`, `sdk-configure.log`, `sdk-build1.log`, `sdk-build2.log`,
`w96all.txt`, `ctest1.log`, `sdk-install.log`, `ctest-install.log`, `fileset_diff.log`.
