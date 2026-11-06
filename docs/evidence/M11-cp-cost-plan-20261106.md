# KI-072: cutting the GPU-command (CP) thread's CPU time per frame (read-only plan, 2026-11-06)

Read-only design by an Opus subagent on 2026-11-06 (SDK d08813b sources, l63/l69/l70/l71 perf-split data); accepted by the primary agent as the M11 performance plan input. PC rows marked inferred come from l63 ratios and need confirming on the next owner run.

Read-only design by an Opus subagent. Sources: `docs/evidence/M11-ki072-stutter-l70-session-20261106.md`
(l70 PC session 668aa5ee, per-minute derived table), `docs/evidence/M10-l69-pc-session-20261105.md`,
`docs/evidence/M11-ki067-hwinfo-baseline-20261106.md`, the l71 South Central analysis (job scratch
`ki072-l71-southcentral.md`, post-build windows 16:14:47 and 16:15:47), the full-field l63 PC segment data
(job scratch `l63/seg.json`, same RTX 5060 laptop, 1440p and 720p, arms "base" and "all"), the l70 Deck
session `2902c412...` (minute ending 2026-11-06 11:52:26), M10 design notes (`M10-cpu-targets`,
`M10-deck-watch-upload-design`, `M10-002-readback-design`, `M10-ki047-targeted-wait`), README-AI 17b,
`docs/M10-priorities.md`, `docs/M11-priorities.md`. SDK source: worktree of branch `mcla/m11-content-unmount`
at `d08813b` (`<sdk>` below). Nothing built, launched or committed. Spark: not used (exact source reading and
arithmetic over private logs; a model answer would need the same checks).

**Limit of the data.** The raw l69/l70 PC session logs are not on this machine, so the l69/l70 per-phase split is
not available field by field. The PC budget below combines the l70/l69 derived unit costs that the evidence
reports (CP CPU us/draw, kernel ms/f, VirtualProtect us/call, upload GB/s, p_tex us/draw, read-back MB/f, waits)
with the per-draw phase costs of the full-field l63 PC segments and the l71 post-build windows. Rows marked
"(l63 ratio)" are scaled from l63 per-draw costs; they must be confirmed on the next owner run.

## 0. Answer in brief

- **Healthy late-game PC frame (l70 14:47/14:48/14:56, l69 18:10/18:15):** 6.3-7.3k draws, 32-36 fps, CP busy
  26-29.5 ms/frame = **CP CPU about 18.5 ms** (2.66-3.10 us/draw) + **fence/read-back waits about 6.5 ms** + about
  1 ms off-CPU. GPU 21.6-24 ms/frame, render-thread own work about 16 ms/frame. The GPU therefore caps the gain at
  about 4-6 ms of CP busy (about 42-45 fps) at 1080p; beyond that, CP savings buy robustness: under the KI-067/KI-072
  x1.5-2.5 host slowdown every 1 ms of CP CPU saved is 1.5-2.5 ms of frame time.
- **Where the 18.5 ms goes:** draw phases about 10.5 ms (texture 4.6, bind 2.6, render targets 1.2, pipeline 0.65,
  primitive/setup/vertex/record 1.4), upload region about 2.3 ms (of which VirtualProtect arm about 0.7), PM4/register
  decode ("packets_other") about 2.4 ms, IssueCopy/resolve/read-back memcpy about 1.1 ms, submit/execute/present about
  1.1 ms. Kernel time 2.3-2.8 ms is inside these numbers (about 0.6 of it attributable to VirtualProtect).
- **The memory watch is not the PC's big lever** (0.7 ms/frame of CP arm; write faults run on guest threads). Upload-
  ahead and bridging were already net negative on the PC (l63). It is the Deck's lever (arm 6.2 ms/frame there).
- **Top 3 (ms saved per engineering day, PC 1080p late content):**
  1. **Read-back sync kick-interval A/B** (cvar only, `d3d12_readback_sync_kick_draws` 512 -> 256/128): -1 to -3 ms
     of CP busy (the 6.5 ms/frame of fence/read-back wait is the largest single item of CP busy), +0.1-0.4 ms CPU;
     0.5 day plus one owner run. Risk low (hot-reload cvar, KI-047 path unchanged).
  2. **Fetch-constant identity cache** (compare-on-write for fetch constants; per-slot memo of raw fetch dwords to
     binding/texture; lock-free scaled-resolve check): -1.5 to -3 ms of CP CPU (texture phase 4.6 ms plus part of
     bind/vertex/packets), 2-3 days. Risk low-medium (invalidation completeness). Deck: -3 to -5 ms (tex+bind 10 ms).
  3. **ThinLTO + instrumented PGO of the SDK runtime (CP loop) and the host**: -5 to -15% of CP user time = -0.8 to
     -2.4 ms, plus render-thread gains, 2-4 days. Risk low for behaviour, medium for build reproducibility.

## 1. CP CPU budget per frame

### 1.1 Field definitions used (from `<sdk>/include/rex/perf/perf_split.h`)

`MCLA_PERF_SPLIT v=1` fields are window totals; "per frame" = field / `frames`; "per draw" = field / `draws`.

| Budget item | Fields and formula | Code region (`<sdk>/src/graphics/d3d12/command_processor.cpp` unless noted) |
|---|---|---|
| CP CPU | `cp_cpu_ms` = `cp_user_ms + cp_kernel_ms` (GetThreadTimes of the CP thread) | whole thread |
| CP busy | `cp_busy_ms` = wall - `ring_idle_ms` - WRM total | |
| Draw phases | `p_setup` .. `p_record` (sampled 1 in 16, scaled; exclusive of timed regions) | `IssueDraw` 3517-4125, marks at 3588/3595/3628/3698/3705/3771/3858/4122 |
| texture | `p_tex_ms` | `texture_cache_->RequestTextures` 3704 |
| bind | `p_bind_ms` | 3709-3771: pipeline bind, viewport key, scissor, `UpdateFixedFunctionState` 5711, `UpdateSystemConstantValues` 5776, `UpdateBindings` 6197 |
| render targets | `p_rt_ms` | shader modifications + `render_target_cache_->Update` 3601-3628 |
| pipeline | `p_pso_ms` | translations, `ConfigurePipeline` 3633-3698 |
| vertex ranges | `p_vb_ms` | vertex-fetch `RequestRange` and memexport 3774-3858 |
| Upload | `upload_ms` (exclusive region `kUpload`), `upload_calls`, `upload_kb` | `SharedMemory::RequestRanges` `<sdk>/src/graphics/shared_memory.cpp:880-888` -> `D3D12SharedMemory::UploadRanges` `<sdk>/src/graphics/d3d12/shared_memory.cpp:368-405` |
| Watch arm | `watch_arm_ms` (us timer, all threads, but arm runs on the CP inside `upload`); calls in `MCLA watch protect/min` | `MakeRangeValid` `shared_memory.cpp:486-552` -> `PhysicalHeap::EnableAccessCallbacks` `<sdk>/src/system/xmemory.cpp:1905-2044` -> `Memory::ProtectWatchRange` `xmemory.cpp:521-547` -> `VirtualProtect` `<sdk>/src/core/memory_win.cpp:143` |
| Write fault / disarm | `write_fault_ms`, `watch_disarm_ms` (guest threads, mostly the render thread; not CP time) | `PhysicalHeap::TriggerCallbacks` `xmemory.cpp:2046-` |
| Copy / resolve / read-back | `copy_ms` (inclusive), `resolve_ms`, `readback_gpu_ms`, `readback_memcpy_ms`, `readback_kb`, `readback_wait_ms` (inclusive of the fence wait) | `IssueCopy` 4287, `IssueCopy_ReadbackResolvePath` 4316, waits 4632/4658, memcpy 4728 |
| Waits | `fence_wait_ms` (WaitForSingleObject 4776/4803), `stream_finish_wait_ms`, `wrm_mem_ms` (outside busy) | `CheckSubmissionFence`, replay worker, `WAIT_REG_MEM` |
| Submission | `submit_ms`, `execute_ms`, `present_ms` | `EndSubmission`, `ExecuteCommandLists`, `IssueSwap` 2082 |
| PM4 decode | `packets_other_ms` = `cp_busy` - phases - exclusive regions | `CommandProcessor::ExecutePacketType0/3` `<sdk>/src/graphics/command_processor.cpp:896/949`, `WriteRegister` 508, D3D12 `WriteRegister`/`WriteRegistersFromMem` 1952-2065 |

The l70/l71 evidence derives: CP CPU us/draw = `cp_cpu_ms/draws`; kernel ms/f = `cp_kernel_ms/frames`;
"upload memcpy GB/s" = `upload_kb*1024/upload_ms`. **Note:** that last ratio is the throughput of the whole upload
region, which includes the VirtualProtect arm calls, the upload-pool request and the copy recording, not the
memcpy alone (`UploadRanges` calls `MakeRangeValid` before each `memcpy`, `d3d12/shared_memory.cpp:389-393`).

### 1.2 PC, 1080p Unlocked, healthy late content (6.5k draws/frame, 35.5 fps, CP busy 26 ms)

Measured inputs (l70 "good late" 14:47/14:48/14:56 and l69 freeway 18:10/18:15): CP CPU 2.66-2.98 us/draw (l69
2.94-3.10); CP kernel 2.3-2.8 ms/f (l69 2.8-3.0); VirtualProtect arm 0.64-0.76 us/call at about 1,000 calls/frame
(1.5-3.4 M/min); uploads 996-1,067 calls/f, 7.7-8.2 MB/f, 3.42-3.74 GB/s; p_tex 0.66-0.75 us/draw (l69 freeway
0.90-1.06); read-back 8.8-8.9 MB/f; fence+read-back wait 6.3-6.7 ms/f; WRM 1.0-2.3 ms/f; GPU 21.6-22.0 ms/f;
render own 15.9-17.0 ms/f. l71 post-build windows (same PC, 6.1k and 4.8k draws/f): texture 3.4-4.3 ms/f, bind
2.3-3.1 ms/f, watch arm 0.7-0.8 ms/f, write fault 0.95-1.1 ms/f.

| Phase / item | ms per frame | us per draw | Basis |
|---|---|---|---|
| **Draw phases, total** | **about 10.5** | 1.6 | sum below |
| - texture (`p_tex`) | 4.3-4.9 | 0.66-0.75 | l70 measured |
| - bind (`p_bind`): fixed-function, system constants, constant buffers, samplers, bindless indices, root params | 2.5-2.7 | 0.38-0.41 | l63 (stable in every arm and resolution), l71 2.3-3.1 ms/f |
| - render targets (`p_rt`) | 1.1-1.3 | 0.17-0.20 | l63 ratio |
| - pipeline lookup (`p_pso`) | 0.6-0.7 | 0.09-0.11 | l63 ratio |
| - primitive + setup + vertex ranges + record | 1.2-1.7 | 0.18-0.26 | l63 ratio |
| **Upload region** (`upload_ms`) | **2.1-2.4** | | 8 MB / 3.4-3.7 GB/s |
| - of which VirtualProtect arm | 0.65-0.75 | | 1,000 calls x 0.64-0.76 us |
| - of which memcpy + pool + CopyBufferRegion record | about 1.5 | | remainder (true memcpy rate about 5 GB/s) |
| **PM4 decode and register writes** (`packets_other`) | **2.2-2.7** | 0.35-0.40 | l63 ratio (2.1-2.7 ms/f at 5.6-7.4k draws) |
| **IssueCopy/resolve/read-back CPU** | **0.9-1.3** | | resolve 0.25-0.30, readback_gpu 0.03-0.1, readback_memcpy 0.55-0.8 (9 MB; l63: 0.65-0.77 for 11.5 MB) |
| **submit + execute + present** | **0.9-1.2** | | l63 0.88-1.19 (more with kicks) |
| Sum (CPU) | about 17-18.5 | | measured 17.3-19.4 (2.66-2.98 us x 6.5k) |
| *of which kernel* | 2.3-2.8 | | VirtualProtect about 0.6; ExecuteCommandLists/present/event syscalls and wake/sleep transitions the rest: **not attributable with current fields** |
| **Waits inside CP busy** (not CPU) | **about 6.5** | | fence + read-back wait 6.3-6.7 (the KI-047 composite sync; in l63 `fence_wait` is 98% inside `readback_wait`) + stream finish 0.4-0.5 |
| WRM (outside CP busy) | 1.0-2.3 | | vblank pacing with event wake on; no CPU |
| Off-CPU residual | 0-1 | | busy - CPU - waits |

Not on the CP but relevant: write faults about 1 ms/frame on guest threads (render thread), replay worker 3-4 ms/frame.

### 1.3 Steam Deck, l70 driving minute (2026-11-06 11:52:26; 17.6 fps, 4.82k draws/frame; stock 30, 1280x720)

All values field/frames from the `MCLA_PERF_SPLIT` line: CP CPU 36.9 ms/f (7.66 us/draw; user 30.3, kernel 6.6);
CP busy 49.2. Draw phases 18.6 (tex 4.6 = 0.95 us/draw, bind 5.4 = 1.12, rt 2.7, prim 1.9, vb 1.5, pso 1.4, setup
0.8, record 0.4); upload 10.9 ms/f for 23.6 MB/f in 411 calls (upload-ahead 16 is a Proton default), of which arm
6.2; packets_other 6.9 (1.44 us/draw); resolve 0.9; read-back memcpy 1.6 (12 MB/f); submit/execute/present 0.7;
waits: read-back 7.1 (fence 6.8 inside), stream finish 2.1, WRM 7.45 (stock-30 pacing). Guest threads: write fault
5.2, disarm 3.0. GPU 28.7, render CPU 53.7. **On the Deck every CP item above is 2.5-4x the PC; watch arm/upload
(10.9) and PM4 decode (6.9) weigh far more than on the PC.**

## 2. Top cost centres: code path, why it costs, change, saving, risk

### 2.1 Fence / read-back wait (6.3-6.7 ms of CP busy per frame; Deck 7.1)

- **Path:** forced composite resolve (KI-047, `d3d12_readback_sync_composite`) -> targeted wait in
  `IssueCopy_ReadbackResolvePath` (4632/4658) -> `CheckSubmissionFence` -> `SetEventOnCompletion` +
  `WaitForSingleObject` (4797/4803). Early kicks: `MaybeKickReadbackSyncSubmission` 3043 at each draw, interval
  `d3d12_readback_sync_kick_draws` (default 512, line 117).
- **Why:** the CP waits for the GPU to execute the backlog recorded since the last submission up to the composite
  copy (the KI-047 targeted-wait note shows there is no shorter correct wait on one queue). About one forced wait
  per frame (2.2k targeted waits per minute at 35 fps). The GPU then idles while the CP resumes: a pipelining loss,
  visible as GPU busy 22 ms in a 28 ms frame.
- **Change A (cheap): kick-interval A/B** 512 vs 256 vs 128 (hot-reload cvar; no build). Smaller interval = less
  backlog at the forced wait; each kick costs about 0.1-0.15 ms on the PC (l63: 2.6-4.8 kicks/f cost 0.36-0.65 ms).
  **Estimate: -1 to -3 ms/frame of CP busy, +0.1-0.4 ms of CPU (verify).** Risk low (same sync, more submissions;
  watch `stream_finish_wait`, submissions/frame and GPU busy). Deck: kicks cost 0.23 ms each under vkd3d, so the
  Deck optimum is likely larger (already an M11-003 sub-item).
- **Change B (large, risky): lazy composite completion** behind a read trap on the composite destination pages (the
  M10-007 budget design's H4 and M11-007 step 4 "first touch" machinery), so the CP never blocks and the guest
  thread that first reads the composite waits instead. **Estimate: up to -4 to -6 ms of CP busy**, but only if the
  guest's first read comes well after the resolve; the render thread's own time may rise by the same wait. Needs the
  M11-007 step 2 read-watch census first. Risk high (KI-047 failure surface: police livery, vinyl).

### 2.2 Texture phase (4.3-4.9 ms/frame; 0.66-0.75 us/draw; the most inflated phase under contention, x2-3.5)

- **Path:** `IssueDraw` 3704 -> `D3D12TextureCache::RequestTextures` `<sdk>/src/graphics/d3d12/texture_cache.cpp:700-767`
  -> `TextureCache::RequestTextures` `<sdk>/src/graphics/pipeline/texture/cache.cpp:455-598`:
  - for every used slot not in `texture_bindings_in_sync_`: `BindingInfoFromFetchConstant` (990; decodes the 6-dword
    fetch constant into a `TextureKey`), `GuestToHostSwizzle`, sign checks, key compare;
  - on a key change: `FindOrCreateTexture` (850-962): `key.GetGuestLayout()` (per-mip layout arithmetic) and two
    `IsRangeScaledResolved` calls (1105-), **each taking `global_critical_region_`** (the process-wide recursive
    mutex shared with guest atomics and the fault handler), then an `unordered_map<TextureKey>` hash lookup;
  - `PrepareTextureLoad` (367) takes the global lock again when `outdated_mask` is set;
  - then per used slot: `MarkAsUsed` (LRU relink, 712), `SetResourceState` + `PushTransitionBarrier` (745-765),
    `UpdateTextureBindingsImpl` on change.
- **Why it costs per draw:** the in-sync bit is cleared by **every** fetch-constant register write
  (`D3D12CommandProcessor::WriteRegister` 1974-1981 and `WriteRegistersFromMem` 2050-2062 call
  `TextureFetchConstantsWritten` and `InvalidateVertexBufferResidency` without comparing old and new values). If the
  XDK re-sends the fetch-constant block per draw (likely; to be confirmed by the counters in section 3), every used
  slot is fully re-decoded every draw, and every texture change does layout math, two lock round trips and a hash
  lookup. Content-dependent (Hollywood 0.43-0.63, freeway 0.90-1.06, late city 0.66-0.75 us/draw) = number of slots
  and key changes, not fixed overhead.
- **Change T1, compare-on-write:** in both D3D12 write paths, compare the incoming fetch dwords with the register
  file before storing; if unchanged, skip `TextureFetchConstantsWritten`, `InvalidateVertexBufferResidency(Range)` and
  `cbuffer_binding_fetch_.up_to_date = false`. Equality of the raw constant means identical binding inputs, so the
  existing in-sync logic stays exact. (The `texture_became_outdated_` path still resets bindings.)
- **Change T2, raw-fetch memo:** a small direct-mapped table (for example 1,024 entries) keyed by the 6 raw fetch
  dwords + `IsDrawResolutionScaled()` -> {TextureKey, swizzled signs, host swizzle, Texture*, Texture* signed}, valid
  while a `texture_cache_generation_` counter is unchanged. Bump the generation on texture creation, destruction or
  eviction (the soft-cap evictions), any `texture_became_outdated_`, and a scaled-resolve page bit changing 0<->1
  (not on every resolve). Hit = skip decode, `GetGuestLayout`, both scaled-resolve checks and the map lookup.
- **Change T3:** read `scaled_resolve_pages_l2_`/`scaled_resolve_pages_` without the global lock on the CP (the CP
  is the only setter at resolve time; a concurrent clear by a fault is equally racy today, because the lock is
  released before the result is used).
- **Estimate:** T1-T3 together -0.25 to -0.45 us/draw = **-1.6 to -2.9 ms/frame** on the PC (most of the texture
  phase except the barrier/LRU tail), plus part of `p_vb` and `packets_other`. Deck: tex 4.6 + vb 1.5 ms/f there,
  **-2 to -3.5 ms/frame**. Risk low (T1, T3) to medium (T2: a missed generation bump binds a stale texture;
  mitigation: a debug cvar that runs both paths and counts mismatches). Effort 2-3 days with tests.

### 2.3 Bind phase (2.5-2.7 ms/frame; 0.38-0.41 us/draw on the PC in every arm and resolution; Deck 1.12 us/draw)

- **Path:** 3709-3771 and `UpdateBindings` 6197-: viewport key build and compare (3725-3748), `GetScissor`,
  `UpdateFixedFunctionState` 5711, `UpdateSystemConstantValues` 5776 (large struct, field compares),
  constant-buffer pool requests and copies (float constants 6295-6348 by bitmap walk; bool/loop 160 B; **the whole
  768 B fetch block 6362-6373 on any fetch-constant write**), sampler parameters per sampler per draw
  (`GetSamplerParameters` `d3d12/texture_cache.cpp:928`, decode from the fetch constant) at 6395-6444, bindless
  `AreActiveTextureSRVKeysUpToDate` 6463-6476 and descriptor-index writes, root-parameter sets into the deferred list.
- **Why:** a fixed instruction path per draw (its per-draw cost does not move with content or resolution), much of
  it re-validating state that did not change because the dirty bits are coarse.
- **Change B1:** with T1 in place, `cbuffer_binding_fetch_` is invalidated only by real changes; additionally limit
  it to vertex-fetch constants used by the bound shaders (texture fetch state reaches shaders through descriptors and
  system constants; verify in the DXBC translator before relying on it).
- **Change B2:** memoize `GetSamplerParameters` per slot by raw fetch dwords (same key as T2).
- **Change B3:** a per-draw "bindings fingerprint" (pipeline handle, root signature, texture/sampler layout UIDs,
  T2 generation, cbuffer up-to-date bits): equal fingerprint = skip the descriptor/sampler checks entirely.
- **Estimate: -0.1 to -0.15 us/draw = -0.6 to -1.0 ms/frame** PC; Deck -1 to -2 ms. Risk low-medium. Effort 2 days
  (needs the sub-split first; see section 3).

### 2.4 PM4 decode and register writes (`packets_other` 2.2-2.7 ms/frame PC; 6.9 ms/frame Deck)

- **Path:** `ExecutePacketType0/3` (`<sdk>/src/graphics/command_processor.cpp:896/949`; SET_CONSTANT 1729) ->
  `WriteRegisterRangeFromRing` 676 -> virtual `WriteRegistersFromMem`; the D3D12 override handles float/bool/fetch
  ranges in bulk (1984-2063) but **every other register range falls back to one virtual `WriteRegister` per dword**
  (D3D12 1952 -> base 508: volatile store, `known_registers` table, scratch-range check, a switch, then the D3D12
  range checks).
- **Change P1:** bulk path for context-register ranges: `copy_and_swap` into the register file, then a precomputed
  per-register "has side effect" bitmap so only the few special registers (scratch, COHER, gamma LUT) run the
  switch. **Estimate -0.3 to -0.8 ms/frame PC, -1 to -2.5 ms Deck** (needs the packet census in section 3). Risk low
  (pure refactor; unit-testable against the per-register path).

### 2.5 Upload memcpy and watch arm (2.1-2.4 ms/frame PC; 10.9 ms Deck)

- **Path:** `RequestRanges` (`shared_memory.cpp:683-890`) -> `UploadRanges` (`d3d12/shared_memory.cpp:368-405`):
  per run and pool chunk `MakeRangeValid` (global lock, valid bits, `EnablePhysicalMemoryAccessCallbacks` -> three
  alias heaps, page walk, one `VirtualProtect(PAGE_READONLY)` per run under the global lock), then `memcpy` from the
  physical view into the WC upload heap, then `D3DCopyBufferRegion`.
- **Why:** about 1,000 small uploads per frame (average 8 KB, the dynamic vertex/index ring re-validated after
  each write fault). The 0.64-0.76 us per protect is syscall + PTE update + TLB shoot-down IPIs to the other cores
  running process threads; arm count = upload count, so coalescing inside a call has nothing left (M10 design 1.1).
  PC experience: upload-ahead 16 and disarm bridging were **net negative on the PC** (l63: arm calls 788 -> 305 per
  frame but arm time 0.99 -> 0.96 ms, uploads 7 -> 17-19 MB/f, faults 67 -> 98/f); hot blocks v1 moved the cost to
  GPU copies (Deck +10.4 ms GPU).
- **PC change (small):** none worth a build on its own. Remove the double timing in `ProtectWatchRange`
  (`steady_clock::now` twice plus `ReadTsc` twice plus two atomics per call; keep TSC only): about -0.03 to -0.05 us
  per call = -0.05 ms/frame. Not ranked.
- **PC change (medium): asynchronous upload copy.** Keep `MakeRangeValid` (protect) on the CP, but hand the `memcpy`
  to a helper thread that must finish all queued copies before the CP crosses any guest-visible sync point
  (`EVENT_WRITE*`, `MEM_WRITE`, scratch/interrupt packets, `WAIT_REG_MEM`, submission end). Protect-before-copy is
  preserved; the copy may sample data written after the protect only when that write faults (then the range is
  invalidated and re-uploaded anyway). **Estimate -1.0 to -1.5 ms/frame PC, -2 to -4 ms Deck (if a core is free,
  which the M10-006 gate says is not the case on the Deck).** Risk medium-high (ordering), effort 4-6 days.
- **Deck change:** hot blocks v2 (M11-004) remains the Deck lever for arm+fault; not recommended for the PC.

### 2.6 Read-back memcpy (0.55-0.8 ms/frame PC; 1.6 ms Deck) - M11-007 steps 2-5

- Step 2: read-watch census (NoAccess instead of ReadOnly on resolved pages, per-key touch bit; no behaviour
  change; about 0 ms). Step 3: skip the GPU copy and the CPU memcpy for steady keys never touched by the CPU
  (composite family excluded; photo window suspends skipping). Step 4: synchronous fallback on a first touch,
  previous-slot fence, photo sync. Step 5: batching, only if `rb_gpu_recorded` stays above about 5 per frame.
- **Estimate (M10-002 design 4, scaled to 9 MB/f): CP -0.45 to -0.7 ms/frame** (0.85 x s of the memcpy), GPU -0.3
  to -0.7 ms/frame; Deck CP -1.2 to -1.4 ms. Step 2 also produces the first-touch timing that decides 2.1 change B.
  Effort 4-6 days; risk medium (a steady family the CPU does read would freeze; read-fault cost under Wine about
  80 us per fault).

### 2.7 Kernel transitions and waits

- Fence waits use `SetEventOnCompletion` + `WaitForSingleObject` (4769-4803). On a laptop at the 35 W STAPM cap with
  deep C-states, each blocking wait adds wake latency (typically tens to a few hundred us). **Change K1 (PC only,
  cvar):** poll `GetCompletedValue` with `_mm_pause` for up to about 50-100 us before blocking when the expected
  remaining GPU work is short (after a kick). **Estimate -0.1 to -0.5 ms/frame**, risk low; off on the Deck (CPU
  oversubscribed). Measure first: number of blocking waits per frame and their wake-to-run latency (ETW CSwitch).
- WRM is already event-woken (`d3d12_wrm_event_wake=true`); its 1.0-2.3 ms/frame is vblank pacing, not CPU.
- The replay worker already uses the batched wake.

### 2.8 Compiler and build flags

- **What l69/l70 build with:** host `mcla` in `Release` with `-O3 -DNDEBUG`, clang (LLVM for Windows), **no
  `-march`** on the host compile line (the SDK helper adds only `-msse4.1` for x86 non-MSVC targets) and no LTO
  (l70 build tree `build.ninja` FLAGS; l69 and l71 host caches the same). SDK runtime (where the CP lives):
  `CMakePresets.json` windows base `-march=x86-64-v3`, configuration Release `-O3`, no
  `CMAKE_INTERPROCEDURAL_OPTIMIZATION`, no PGO. M7-016 `-mtune=znver2` alone gave no repeatable gain.
- **Change C1: ThinLTO for the SDK runtime and the host** (cross-TU inlining of `RegisterFile` accessors,
  `RequestRanges`, texture-cache helpers, deferred-list writers). **Change C2: instrumented PGO**
  (`-fprofile-generate` training on a 10-minute owner route, `-fprofile-use` with the `.profdata` published next
  to the SDK pin so clean clones reproduce). **Estimate -5 to -15% of CP user time = -0.8 to -2.4 ms/frame PC**
  (16 ms user), similar percentage on the render thread's host helpers; Deck -1.5 to -4.5 ms. Risk: behaviour low
  (same semantics; strict FP flags must be kept for generated code), build reproducibility and debugging medium
  (profile drift; gate on the existing ctest suites plus the M7 ABI witnesses). Effort 2-4 days. Also check whether
  the host should get `-march=x86-64-v3` like the SDK (Deck Zen 2 supports it); the generated game code is
  render-thread time, not CP.

### 2.9 Moving CP work to a helper thread (M10-006 revisit)

A two-stage CP (stage 1: PM4 decode, register file, texture/shared-memory resolution and uploads; stage 2:
binding, recording) could take 30-40% of the CP CPU off the critical path, but at 1080p the GPU caps the PC gain at
about 4-6 ms, and on the Deck there is no free core (M10 gate). Estimate design 3-4 + implementation 10-15 days;
risk high (ordering of register file, write-watch, EDRAM/texture cache). Keep gated; the upload-copy helper (2.5)
is the bounded first step of the same idea.

## 3. Measurement plan before coding (confirms the top two)

### 3.1 New perf-split sub-fields (counts are cheap; timings only on sampled draws)

Add to `PerfSplitCounters`/`REX_PERF_SPLIT_FIELDS` (schema v=2, appended so existing parsers keep working):

| Field | Where | Decides |
|---|---|---|
| `fetch_dw_written`, `fetch_dw_unchanged` | both D3D12 fetch-constant write paths (1974, 2050) | the T1/B1 hit rate (expect most writes unchanged if the XDK re-sends blocks) |
| `tex_slots`, `tex_key_changed`, `tex_find_calls`, `tex_find_created`, `tex_scaled_checks`, `tex_reset_bindings`, `tex_loads`, `tex_barriers` | `TextureCache::RequestTextures`, `FindOrCreateTexture`, `IsRangeScaledResolved`, `ResetTextureBindings`, D3D12 `RequestTextures` | T2 hit rate and how often outdated textures reset all slots |
| sampled sub-phase marks `t_decode`, `t_find`, `t_prep`, `t_d3d12`, `t_update` (inside p_tex) and `b_fixed`, `b_sysconst`, `b_cbuf`, `b_samplers`, `b_descr`, `b_root` (inside p_bind) | same functions, on the 1-in-16 sampled draws only | where the 0.7 and 0.4 us/draw go |
| `pkt_type0`, `pkt_type3`, `reg_dw_bulk`, `reg_dw_single`, sampled `t_packets` by packet class | `ExecutePacketType0/3`, `WriteRegistersFromMem` | P1 size |
| `gpu_waits_blocking`, `gpu_wait_backlog_draws` (draws since the last kick at a forced wait), `kicks` | `CheckSubmissionFence`, `MaybeKickReadbackSyncSubmission` | the kick A/B response and change K1 |

Cost: a few increments per draw plus, on sampled draws, about 10 extra `rdtsc` (the existing chain overhead is
0.1-0.8% of CP time). One owner run on the late-content route (freeway and city race, 10-15 minutes) on an
owner-internal build gives the answers; the kick-interval A/B can run in the same session as three 3-minute arms
(512, 256, 128) because the cvar hot-reloads.

### 3.2 One-off sampling profile (owner PC, administrator shell, about 60 s)

- Windows Performance Recorder: `wpr -start CPU -start CSwitch -filemode` (or `xperf -on PROC_THREAD+LOADER+
  PROFILE+CSWITCH+DISPATCHER -stackwalk Profile+CSwitch -setprofint 1221` for about 8 kHz), during the late freeway
  race, then `wpr -stop`. Symbols: the SDK is built with `-gcodeview -g`; keep the matching PDBs for the build.
- In WPA, filter the "GPU Commands" thread: CPU Usage (Sampled) by function confirms the shares of
  `BindingInfoFromFetchConstant`, `FindOrCreateTexture`/`GetGuestLayout`, `IsRangeScaledResolved` and
  `RtlEnterCriticalSection`/`std::recursive_mutex::lock`, the unordered_map hash, `UpdateBindings`,
  `WriteRegister`; kernel stacks split the 2.3-2.8 ms/frame of kernel time into `NtProtectVirtualMemory` (and
  `KiFlushProcessTb`/IPI), `NtWaitForSingleObject`, dxgkrnl submission/present and page faults. CPU Usage (Precise)
  gives the wake-to-run latency of fence waits (K1).
- Non-destructive; no save or profile change. It is an owner action (needs an elevated shell and the trace file
  sent back, about 0.5-1 GB).

## 4. Ranking by ms saved per engineering day

PC numbers are CP-thread ms/frame at 1080p late content; under the KI-072 x1.5-2.5 slowdown multiply by 1.5-2.5.
The GPU (about 22 ms/frame) caps visible fps gains near 42-45 fps once CP busy falls by about 4-6 ms.

| Rank | Change | PC ms/frame | Deck ms/frame | Days | PC ms/day | Risk | Deck relevance |
|---|---|---|---|---|---|---|---|
| 1 | Read-back sync kick interval A/B (2.1 A; cvar only) | -1 to -3 busy (+0.1-0.4 CPU) | uncertain (kick 0.23 ms each) | 0.5 | 2-6 | low | medium; already an M11-003 sub-item |
| 2 | Fetch-constant compare-on-write + raw-fetch memo + lock-free scaled check (2.2 T1-T3, with B1/B2) | -2 to -3.5 (tex -1.6 to -2.9, bind -0.3 to -0.6) | -3 to -5 | 3-4 | 0.6-1.0 | low-medium | high (tex+bind+vb 11.5 ms/f there) |
| 3 | ThinLTO + PGO (2.8) | -0.8 to -2.4 | -1.5 to -4.5 | 2-4 | 0.3-0.8 | low (behaviour), medium (build) | high (CP-first there) |
| 4 | PM4 bulk register path (2.4 P1) | -0.3 to -0.8 | -1 to -2.5 | 1-2 | 0.3-0.5 | low | high (6.9 ms/f there) |
| 5 | Bindings fingerprint (2.3 B3) | -0.3 to -0.5 | -0.5 to -1 | 1.5-2 | 0.2-0.3 | medium | medium |
| 6 | Fence wait spin-before-block (2.7 K1, PC only) | -0.1 to -0.5 | none (off) | 0.5-1 | 0.2-0.5 | low | none |
| 7 | Read-back steps 2-3 (M11-007) | -0.45 to -0.7 CP, -0.3 to -0.7 GPU | -1.2 to -1.4 | 4-6 | 0.1-0.15 | medium | medium |
| 8 | Asynchronous upload memcpy with sync-point joins (2.5) | -1.0 to -1.5 | -2 to -4 if a core is free | 4-6 | 0.2-0.3 | medium-high | gated on a free core |
| 9 | Lazy composite completion (2.1 B) | up to -4 to -6 busy | up to -5 to -7 | 5-8 (after M11-007 step 2) | 0.5-1.0 if the census allows | high (KI-047) | high |
| 10 | Two-stage CP (M10-006) | -4 to -6 (GPU-capped) | none until a core is free | 13-19 | 0.3 | high | gated |
| - | Watch arm on the PC (upload-ahead, bridging, hot blocks) | about 0 or negative (l63) | Deck lever (M11-004) | - | - | - | Deck only |

Recommended order: (1) the sub-fields and the kick A/B in one owner-internal build and run; (2) T1-T3 with B1/B2,
since the counters will show the fetch-constant hit rate within the same run; (3) P1 if `reg_dw_single` is large;
(4) LTO/PGO as a build-only experiment on the same SDK pin (A/B against the same content); (5) M11-007 step 2, whose
census also decides the lazy composite. Each step is accepted only with the M10 harness rules: two clean windows per
run, two runs per arm, a gain beyond the run spread and at least 5%, KI-047 livery, KI-033 vinyl and KI-056 album
checks unchanged.

## 5. Open points (verify)

- l69/l70 per-phase fields (`p_rt`, `p_pso`, `p_bind`, `packets_other`, `readback_memcpy`, `submit/execute/present`)
  are inferred from l63 per-draw costs; the next owner run must confirm them.
- Whether the XDK re-sends unchanged fetch constants per draw (the T1/T2 premise) is not measured.
- The kick-interval response at 1080p on the PC is not measured (l63 compared kick 512 against no kick only).
- About 1-1.5 ms/frame of CP kernel time has no field; the ETW trace attributes it.
- Whether the guest's first read of the composite comes long after its resolve (the premise of 2.1 B) needs the
  M11-007 step 2 census.
