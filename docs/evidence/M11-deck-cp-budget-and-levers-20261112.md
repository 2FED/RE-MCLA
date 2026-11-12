# Steam Deck command-processor budget and next levers (2026-11-12)

Read-only analysis by an Opus subagent (Deck and PC logs, SDK e6d6bd3 source), accepted by the primary agent; estimates carry the stated uncertainty. Spark not used: deterministic log parsing and source reading.

**Headline.** Reconciled Deck CP budget (l82 default, 5968 draws, 62.2 ms/frame, 16.1 fps): draw phases 22.2 ms (bind 6.4, texture 5.7, render target 3.4, primitive 1.9, PSO 1.6, vertex 1.5), packets_other 11.0 ms (5.6-8.6 ms of it untimed), upload 11.3 ms of which watch arm 6.6 ms (VirtualProtect: 16.4 us per call on the Deck vs 0.65 us on the PC, 25x), stream-finish wait 2.5 ms, read-back copies 1.7 ms. Per draw the Deck is 2.3-2.4x the PC for pure CPU work; arm, disarm, write faults and kernel time are Wine/kernel-bound (4-25x). Suspects inside packets_other: a submission-fence poll on every packet while deferred fences are pending, two QPC calls per draw packet, an XXH3 hash of the whole shader ucode on every IM_LOAD. Ranked levers: (1) packets_other cleanup -1..-3.5 ms; (2) ThinLTO+PGO of the SDK runtime -1.9..-5.6 ms and x86-64-v3 for the generated game code (render thread); (3) asynchronous submission end -1.5..-2.7 ms; (4) unchanged-state draw fast path 0..-7 ms (needs a count first); (5) SMT placement 0..-3 ms. Ceilings: 1 ms of CP busy is about 0.68 ms of frame time; -10 ms gives about 18 fps; the guest render thread does about 45 ms of its own work per frame, flooring the frame at 50-54 ms (18.5-20 fps); 30 fps is not reachable incrementally: it needs a structural change (a native renderer without the command processor).

---

# Deck CP next lever: notes (read-only analysis, 2026-11-12)

Inputs: D:\82 default b5ca5328 (Deck, 7 driving windows), D:\81/84/85/87 (consistency), PC owner-l94 2471789 (49 windows,
4672 draws/f) and owner-l91 3b8faee (5340 draws/f). Parser: budget.py (this folder), output budget_out1.txt. python -I.
Only mcla*.log text read. SDK read: private/l92-sdk-wt (read-only). No build, no launch, no edits outside this folder.

Accounting identity (perf_split.h:30-32, 594-682): wall = cp_busy + ring_idle + wrm; cp_busy = scaled phases + exclusive
regions + packets_other (remainder). arm runs inside upload (CP); disarm runs inside write_fault (render thread, not CP).
draw_packet_ms is per-packet steady_clock microseconds truncated (command_processor.h:258-271): PC "draw_packet - phases -
in-draw regions" is negative (-235 ns/draw), so truncation is >= 0.24 us/draw; Deck outside-chain draw-packet time is
>= 2.35 ms/f, ~5 ms/f with an expected 0.5 us/draw truncation.

Deck l82 default (5968 draws/f, 62.15 ms/f): busy 51.56 = phases 22.23 + regions 18.36 + packets_other 10.97.
Ratios per draw Deck/PC: phases 2.32 (reference "pure CPU"), packets_other 2.44, bind 2.39, tex 2.12, rt 2.56, prim 3.46,
upload 4.2 (arm 8.2 per draw, 25x per call 16.4 vs 0.65 us; non-arm 2.5 per draw but 1.1x per KB: Deck moves 2.9x bytes
from ahead16), write fault 5.2 (render thread; 35 vs 7.8 us per fault), disarm 9.6, readback_wait 6.4 (GPU),
stream_finish 2.6 (= replay worker 2.64), readback memcpy 2.8, submit 1.0, execute 0.35, present 0.19.
Kernel: Deck 7.26 ms/f vs PC 2.94 (arm syscalls). Off-CPU inside busy: 6.8 ms, known waits ~3.7 -> ~3.1 ms preemption/SMT.
CP idle 10.6 ms/f (ring_idle 3.8 + wrm_mem 6.78): CP waits on the guest render thread.
Render thread: cpu 53.9 + serial wait 8.55 (sub_82412F98 spin = 10-18 % of render guest samples) -> own work ~45 ms/f.
Empirical transfer l80->l81: CP busy -12.4 ms gave frame -8.4 ms (0.68).

Code findings (l92-sdk-wt):
- per packet: command_processor.cpp:873-875 PollDeferredMemoryWrites -> d3d12/command_processor.cpp:3606-3611
  submission_fence_->GetCompletedValue() on every PM4 packet while deferred fence writes are pending (and per draw 4353).
- per draw packet: command_processor.cpp:1726/1745 ScopedPerformanceDiagnostic = 2 steady_clock::now (QPC) per draw.
  LARecomp found per-packet QPC profiling = 25 % of the CP in stock 0.10.0 (survey line 91).
- IM_LOAD/IMMEDIATE: command_processor.cpp:1841-1890 -> d3d12/pipeline_cache.cpp:873-878 XXH3 of the whole ucode +
  unordered_map find on every shader load (count unknown).
- regs_single 40.6 k/f -> virtual WriteRegister per dword (d3d12 2125-2163 + base 512-670): ~0.5-1 ms/f, not the 11 ms.
- arm: xmemory.cpp:1041-1051 walks 3 alias heaps; 2253-2392 one VirtualProtect per contiguous run under the global lock.
- tex: per used slot every draw MarkAsUsed + PushTransitionBarrier (d3d12/texture_cache.cpp:804-833); out-of-sync slots
  0.71/draw, memo hit 61 %, fetch dwords unchanged 67 %.
- stream_finish: d3d12/command_processor.cpp:6273-6280 -> deferred_command_list.cpp:263-282 blocks until the replay
  worker drains, at each of 14.4 submissions/f (2.5 ms/f).
- host x86 builds add only -msse4.1 for generated code (rexglue_helpers.cmake:29); rexruntime is x86-64-v3 already.
No draw-state-identity counter exists.
