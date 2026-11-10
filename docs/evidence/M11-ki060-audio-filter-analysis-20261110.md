# KI-060 Steam Deck audio crackle: static analysis of the XAudio engine thread

Read-only research by an Opus subagent on 2026-11-10 (generated code and SDK sources; nothing built or run), accepted by the primary agent as analysis; hypotheses are unmeasured. Spark not used: deterministic source reading.

**Headline.** The starved XAudio engine thread F8000534 spends ~80 % of its time in two per-voice scalar biquad filter effects (sub_82146BE0: low-pass + high-pass on 256 mono samples; sub_82147738: per-channel biquad with gain), recompiled as fma + float round trips + byte-swapped guest-stack loads/stores per sample. The audio queue never drops or resamples: a late producer means inserted silence, so the deficit is pure throughput and a larger queue cannot fix it. The sound-bank flag 828317A1 wait is a symptom (engine rendering frames back to back). Plan: a one-run probe (calls and ns per function, filter calls per frame, bypass mix, zero/denormal input share, MXCSR DE/UE), then a default-off native replacement of sub_82146BE0 (expected 3-8x on 80 % of the thread), then sub_82147738.

---

# KI-060 Deck audio crackle: read-only research notes (2026-11-10)

## The two hot functions
- sub_82146BE0 (mcla_recomp.0.cpp:55181-56898): per-voice two-stage biquad effect (low-pass + high-pass).
  Bank A on if param +80 < 23900 (Hz cutoff; >=23900 means bypass), bank B on if param +76 != 0.
  Coefficients recomputed by sub_82146AD0 only when the cutoff changes (+88/+84 cache). 5 coeffs + 4 history per bank,
  on guest stack (144..176 / 96..128). Loop: 32 iterations x 8 samples = 256 mono samples in place
  (r11 += 32, loop at slice 82146DA0..821474DC). Per-block denormal guard: after the loop each history
  value with exponent bits 0 is replaced by f31 = 0.0 (rlwinm 0x7F800000 test). Returns via __restgprlr_27.
- sub_82147738 (mcla_recomp.0.cpp:57113-57852): per-channel biquad with output gain (+60), channel mask +74,
  channel count from the effect buffer descriptor (sub_823EAF00 -> r1+97 count, r1+104 data), 256 samples/channel
  (r5 += 1024 stride), per-channel history arrays (+32/+36/+40/+44), two variants by byte +75. Same exponent guard.
  Looks like an XAudio effect Process() on a submix/multichannel voice.
- Both are scalar FPU code: every op is double(float(std::fma(...))) etc.; every lfs/stfs is a byte-swapped guest
  memory access (state/coeffs live on the guest stack); ctx.fpscr.disableFlushMode() before each block
  (FTZ+DAZ cleared: include/rex/ppc/context.h:226, platform/fpscr.h:33). No calls, no lwarx in the loop.
  Prior PC evidence (docs/evidence/M7-016-audio-mixer-cost.md): body has 96 hardware FMAs; no libcalls.
- Called indirectly (effect vtable), no direct callers in generated code: call count = active voices with the effect.

## Denormals
- Scalar PPC FPU: IEEE denormals (FPSCR NI=0) -> SDK clears MXCSR FTZ/DAZ for scalar code. VMX: NJ=1 -> enableFlushMode.
- Arithmetic is in double, so float denormals are normal doubles; assists can only hit cvtss2sd/cvtsd2ss of float
  denormals. Title flushes history once per 256-sample block, so denormal work is bounded (one block per decay)
  unless inputs are themselves denormal. Unproven on Zen 2 -> probe (MXCSR DE/UE sticky bits).

## Frame protocol (SDK)
- audio_system.cpp:28 audio_maxqframes default 8 (4-64); semaphore count = free slots; Audio Worker calls guest
  callback per slot; sdl_audio_driver.cpp:300-347 SDL callback pops a frame (6ch x 256, released slot) or writes 256
  silent samples WITHOUT releasing a slot. No drop, no resample. Producer deficit is throughput, so a bigger queue
  only absorbs bursts; it cannot fix a sustained 25-40 % deficit.
- Engine thread 823EC8C8 (mcla_recomp.25.cpp:38185): KeWaitForSingleObject(event 0x828CD9AC) -> sub_823EC160 +
  sub_823EC248 (one frame) -> KeSetEvent(0x828CD99C) loop.

## Flag 0x828317A1
- sub_8213AB78 (mcla_recomp.0.cpp:26002, ~26225-26245) registers via sub_823EA888 (XAudio lib, engine object
  0x828CD9BC area): index 0 -> 0x8213AB48 (calls sub_8213A600 then flag=1), index 1 -> 0x8213A0E8 (flag=0).
  = pre/post engine-frame callbacks on F8000534. Flag = "engine rendering a frame".
- sub_82136700 at 0x82136730-0x82136744 (mcla_recomp.0.cpp:15616-15635) waits while flag != 0 (Sleep(0) via
  sub_821C91B8). 0.3 s streaks = F8000534 rendering frames back to back. Symptom, not an independent lock.

## Affinity
- ignore_thread_affinities default true (xthread.cpp:44); SetActiveCpu (xthread.cpp:906-912) maps guest hw thread
  N -> host CPU N only when false. No per-thread host pinning exists. Deck topology pairs 0-1,2-3,4-5,6-7
  (M7-016-deck-affinity.md). Whole-process 4-CPU tests in M7-016 showed no benefit (not an audio isolation test).

## Licence
- Nothing copied from LARecomp/GPL forks.
