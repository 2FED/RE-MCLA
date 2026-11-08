# l80 Steam Deck A/B: default / census / audio-off (2026-11-08), KI-060 zero-delay spin, stage-0a census

Read-only analysis by an Opus subagent on 2026-11-08 of the l80 Deck sessions (default / census / audio-off) and the recompiled game code; accepted by the primary agent.

Read-only analysis by an Opus subagent. Nothing was built, launched or executed. The owner-copied Deck
archive was treated as untrusted data: only `mcla.log`, F10 `state.json` thread entries and session
text files were read. `save-private`, `before-user`/`after-user` and minidumps were not opened. Spark
was not used: deterministic parses of three logs plus exact reading of generated code, where checking
a model's answer would cost more than doing the work.

| arm | session | switches |
|---|---|---|
| default | 8441ce0c (15:30) | l80 Wine defaults: `ke_delay_short_sleep_mode=hires_timer`, `apu_worker_priority=above_normal`, timer blocking with spin tail 50, heaps off |
| census | 42b51290 (15:36) | default + `memory_watch_census=true` |
| audio-off | 94b51b61 (15:42) | default + `short_sleep_native` + `apu_priority_normal` (log: `mode=native`, no KI-060 priority lines) |

All three: `complete exit 0`, three F10 packages each, same city loop. Method as in the l79 report
(`docs/evidence/M11-l79-deck-ab-20261108.md`, Method): minute 1 is excluded. "Driving" means every
`MCLA_PERF_SPLIT` window with seq >= 2 (minute windows plus F10 marks, which do not overlap). Values
per frame are window sums divided by frames.

`<sdk>` = the l80 SDK worktree `private/l80-sdk-wt` (ca63f94); generated code = `generated/default`.

## 0. Headline findings

1. **Nothing changed for audio or fps.** Underruns per post-F10 audit: default 467 / 2038, census 325 /
   1907, audio-off 212 / 1622 (l79: 494 / 1492 default, up to 1982). The CP cost per draw is the same
   10.0-10.1 us. The fps differences follow the draw load.
2. **l80 did not touch the KI-060 load.** About 99.5 % of the short delays are zero-interval
   `Sleep(0)` calls from the guest **sound-manager thread** F800068C (entry `sub_82144AB8`). This is a
   free-running tick loop with no producer to wait on, so `hires_timer` leaves it native by design.
   The thread burns 0.45-0.85 of a logical CPU, about 45 % of it in the kernel.
3. **`apu_worker_priority` raised the wrong thread.** F10 `state.json` shows `Audio Worker (F8000014)`
   at base priority 1, but it uses only about 3 % of a core. The heavy audio thread F8000534 is a
   *guest-created* thread (base priority 0, 70-85 % of a core) and was not affected.
4. **Side finding with more fps impact than (2).** The owner Deck override file switches the M10 Wine
   watch defaults off. Upload-ahead (`ahead=0`) and disarm bridging (`bridge=0`) are both off in all
   three arms, because of the `=false` lines (section 4.1). The census shows 917 arm calls per frame
   at 2.1 pages per call. In the l66 "all" run, with both on, it was 479 per frame at 14 pages per call.
   This explains the l79/l80 arm cost of 11-12.5 ms/f against 8.0 ms/f in l66.

## 1. Per-arm driving numbers

| driving | default | census | audio-off |
|---|---|---|---|
| fps per minute (min 2..4) | 14.4 15.1 14.3 | 14.9 15.3 14.0 | 17.3 16.4 13.5 |
| fps (perf windows / pacing frame-weighted) | 14.29 / 14.60 | 14.45 / 14.73 | 15.47 / 15.73 |
| draws / frame | 6263 | 6086 | 5644 |
| p50 ms (per min) | 66.7-66.8 | 66.6-67.0 | 50.9-67.5 |
| p99 ms mean of minutes / max | 94.2 / 116 | 94.5 / 117 | 84.0 / 117 |
| frames > 100 ms per min | 8.7 | 12.3 | 6.0 |
| CP busy ms/f (% wall) | 62.8 (90 %) | 61.6 (89 %) | 57.0 (88 %) |
| CP busy per draw | 10.03 us | 10.13 us | 10.09 us |
| CP thread CPU ms/f (kernel) | 51.6 (12.9) | 50.8 (12.6) | 47.4 (11.6) |
| wrm_mem ms/f | 6.7 | 7.2 | 7.3 |
| GPU busy ms/f (% wall) | 29.5 (42 %) | 28.9 (42 %) | 27.9 (43 %) |
| render CPU / wait ms/f | 65.3 / 16.5 | 64.4 / 16.2 | 60.2 / 14.3 |
| read-back / fence wait ms/f | 1.17 / 1.01 | 0.81 / 0.70 | 0.53 / 0.46 |
| draw / draw_regions / upload ms/f | 25.1 / 20.5 / 18.2 | 24.5 / 20.3 / 18.1 | 22.9 / 18.3 / 16.1 |
| watch arm / disarm / write fault ms/f | 12.5 / 4.7 / 7.2 | 12.3 / 4.7 / 7.1 | 11.3 / 4.3 / 6.4 |
| packets_other / copy ms/f | 11.4 / 4.4 | 11.3 / 4.1 | 10.6 / 3.8 |
| replay CPU ms/f | 16.5 | 16.2 | 15.0 |
| watch protect/min arm calls (us/call) | 790-881 k (11.9-13.3) | 795-823 k (12.3-13.4) | 754-875 k (12.5-13.1) |
| TimerQueue late >1 ms / min (tail 50 us) | 1127-1797 | 931-1978 | 1174-1656 |
| texture creates: avg / max us (heaps off) | 17 / 192 | 18 / 3769 | 17 / 408 |

- **audio-off looks faster only because its scene was lighter.** It had about 10 % fewer draws per
  frame (5644 against 6263), and the CP cost per draw is identical (10.09 us against 10.03 us). Its
  best minute (17.3 fps, p50 50.9 ms, 0 frames >100 ms) is that lighter load. The owner's "maybe +1-2
  fps" against l79 (l79 default: 15.5 fps at 5707 draws/f) is scene variance. The CP chain is still
  about 62 ms against a 66.7 ms (4-vblank) interval.
- **Census overhead is within noise** (CP 61.6 against 62.8 ms/f).
- **XMA:** every `MCLA_XMA` fault counter is 0. The KI-013 blend fix was not in any arm.

### Threads (MCLA_THREAD_CPU, % of one logical CPU, kernel share in brackets)

| window | F80004DC render | CP F8000018 | F8000534 audio (guest) | **F800068C sound mgr** | F8000028 main |
|---|---|---|---|---|---|
| default min 1 (menus/load) | 94 (5) | 57 (13) | 31 (1) | 25 (8) | 40 |
| default F10 #1 (6 s) | 92 (7) | 70 (17) | 79 (3) | 73 (32) | 36 |
| default min 2 / F10 #2 | 94 (8) | 76-77 (17-18) | 70-73 (2) | **48 (19)** | 37-41 |
| default min 3-4 / F10 #3 | 92-93 (8-9) | 71-74 (18-21) | 79-81 (3-4) | **83-84 (38-39)** | 40-45 |
| census min 2 / F10 #2 | 94 (8) | 76 (17) | 70-76 (2-3) | 45-53 (17-21) | 38-41 |
| census min 3-4 / F10 #3 | 92-96 (8-9) | 71-77 (18-20) | 79-85 (3-4) | 79-84 (36-39) | 43-45 |
| audio-off min 2 / F10 #2 | 93-94 (8) | 73-75 (16) | 71-72 (2) | 47 (18-19) | 46-48 |
| audio-off min 3-4 / F10 #3 | 92-93 (8-9) | 72-74 (18-21) | 79-82 (3-4) | 79-85 (37-39) | 40-43 |

- F800068C shows the same two phases as l79 (l79: 40-53 % (16-25) early, then 69-84 % (36-45)).
  hires_timer and native are identical here (38-39 % kernel in both).
- **The audio thread named in the l80 change is not F8000534.** F10 `state.json` (default, F10 #2):
  - `Audio Worker (F8000014)`: base_priority 1, 1.1 s user + 0.6 s kernel for the session up to F10 #2;
  - `XMA Decoder (F8000010)`: base_priority 1;
  - `XThread046C (F8000534)`: base_priority 0, 86 s user;
  - `audio_client_main` (Wine/SDL device thread): base_priority 15.

  F8000534's guest samples include KeWaitForSingleObject, KeSetEvent and
  XAudioGetVoiceCategoryVolumeChangeMask, with hot functions in the title's 823Exxxx-823Fxxxx audio
  library. So it is the title's own audio engine/mixing thread, and `apu_worker_priority` could not
  help it.

### Audio (REX_PERF_AUDIT, 300-frame windows after each F10)

| audit | default | census | audio-off | l79 range |
|---|---|---|---|---|
| after F10 #1 (low phase follows) | 467 (151/300 samples) | 325 (111) | 212 (110) | 0-494 |
| after F10 #2 (high phase follows) | 2038 (297) | 1907 (298) | 1622 (268) | 1492-1982 |
| stream stalls #1 / #2 | 0 / 0 | 0 / 3 | 0 / 4 | 0-2 / 1-4 |
| interval of frames with underruns / clean frames (high audit) | 72.2 / 72.5 ms | 70.6 / 67.4 | 71.5 / 57.8 | 72-75 / 60-64 |

- **Same coupling as l79.** The low audit falls in the window where F800068C runs at about 48 %. The
  high audit falls where it runs at 79-85 %, with F8000534 at about 80 % and the process at about 5
  cores.
- **The arm makes no difference.** hires_timer + above_normal (default) is not better than native +
  normal (audio-off): 2038 against 1622, which is within run-to-run variance. Underruns also happen in
  the low phase (0.7-1.6 per frame), which matches "crackles everywhere".

### MCLA_SLEEP (per minute)

| minute | default (hires_timer) | census (hires_timer) | audio-off (native) |
|---|---|---|---|
| 1 (menus/load) | 2.41 M short, 2.36 M zero, 53.2 k timers, avg 26 us | 2.44 M / 2.39 M / 53.7 k | 3.53 M / 3.48 M / 0 |
| 2 | 5.49 M / 5.44 M / 53.2 k, avg 13 us | 6.86 M / 6.80 M / 52.8 k | 5.12 M / 5.06 M / 0 |
| 3 | 7.50 M / 7.45 M / 52.6 k, avg 10 us | 5.47 M / 5.42 M / 53.3 k | 7.11 M / 7.06 M / 0 |
| 4 | 10.62 M / 10.57 M / 50.3 k, avg 7 us | 10.78 M / **10.73 M** / 50.0 k | 10.96 M / 10.91 M / 0 |
| top_thread / share of zero | F800068C, 99.99 % | F800068C, 99.99 % | F800068C, 99.99 % |

- **Every short delay is non-alertable** (`alertable=0`); there are no fallbacks, and max_us is
  4.8-7.7 ms (34.6 ms once at load).
- **hires_timer handled only about 880 delays per second**, all non-zero (1-2 ms) and from other
  threads.
- **The zero-delay rate follows the CPU the sound thread gets.** Cost per loop iteration: 48 % of a
  core / 5.44 M ≈ 5.3 us, and 83 % / 10.57 M ≈ 4.7 us, of which about 2.2 us is kernel. Roughly 2-3
  us of each iteration is inside the delay; the integer `avg_us` makes this rough.

## 2. What F800068C does (guest code)

`MCLA_GUEST_SAMPLE` for F800068C, the same in all arms. Most-sampled functions:
- `82460270`: 56-61 % of samples, last_import `KeDelayExecutionThread`;
- `8240ECD0`: 11-16 %, last_import `XMsgInProcessCall`;
- the rest is spread across `82136700`, `821C91B8`, `82149818`, `82142720` and title audio-library
  functions (`823F4270`, `823EA1A8`, `823EE918`).

Call chain (generated/default):

- `sub_82460270` is the title's **SleepEx(ms, alertable)** (`mcla_recomp.30.cpp:1579-1665`):
  - interval = ms x -10000;
  - `KeDelayExecutionThread(1, alertable, &interval)` at `:1632`;
  - loops on 257 (STATUS_ALERTED) only when alertable.
- **Sleep(ms)** = `sub_8244FEC0` (`mcla_recomp.29.cpp:31628-31635`, r4=0), plus thin wrappers
  `sub_821C91B8` and `sub_821C91B0` (`mcla_recomp.5.cpp:31324-31338`; the second forces 0).
- **Thread creation.** `sub_82144B18` (`mcla_recomp.0.cpp:50260-`) creates the thread through
  `sub_821C9308` (`:50295`) with entry `0x82144AB8`, a 64 KiB stack, and r9=4. r9 is the processor
  argument (`sub_82131E48` applies it when < 6), so **on the console this thread is pinned to hardware
  thread 4**.
- **Thread loop** `sub_82144AB8` (`mcla_recomp.0.cpp:50202-50258`): `while (g_snd[+777]) sub_82144888(&g_snd)`.
  The object is at 0x82834D90.
- **One tick** `sub_82144888` (`mcla_recomp.0.cpp:49828-50033`):
  1. stopwatch start (mftb; guest timebase 50 MHz, `<sdk>/src/system/runtime.cpp:120`);
  2. dt;
  3. sound updates `sub_82160088` (voices, reaches `82149818`) and `sub_8214D050`;
  4. `sub_82135F80` -> `sub_82136E10`, which:
     - calls `sub_82136570`, i.e. **XMP GetStatus** (`XMsgInProcessCall(0xFA, 0x00070009)` via
       `sub_8240EFE0` / `sub_8240ECD0`, `mcla_recomp.26.cpp:48676`; SDK handler
       `xmp_app.cpp` case 0x00070009);
     - calls `sub_82136700`;
  5. vtable call at `+6464`;
  6. then, if byte 0x827D1998 is non-zero (`:49987`):
     `Sleep((int)clamp(A - elapsed_ms, 0, A))` (`fsel`/`fctidz`, `bl 0x821c91b8` at `:50025`).
  The truncation to whole ms means **any remaining time below 1 ms becomes Sleep(0)**. The game
  constant A (at 0x820436EC) could not be read: there is no decrypted image in the workspace.
- **Inner spin** `sub_82136700` (`mcla_recomp.0.cpp:15593-15634`): `while (*(u8*)0x828317A1) Sleep(0);`.
  The flag is set by `sub_8213AB48` (`:25990`) and cleared by `sub_8213A0E8` (`:24404`). Both are only
  called indirectly; they act like a sound-bank busy lock.
- Static reachability from `sub_82144AB8` confirms the chain to `821C91B8`, `82136700`, `8240ECD0`,
  `82149818` and `82142720`. The other Sleep(0) loops in the title (`82148A10`, `82547140`,
  `82550AA0`, `822C1BF0`, `827C2058`) are not in this thread's tree.

**What it waits for.** Nothing from another thread. It is a **timed polling tick**: the sound
update, the XMP status poll and the XAudio calls all run, then it sleeps for "the rest of A, truncated
to whole ms". Here the rest is always below 1 ms, so the call is always Sleep(0). On the console this
was free (its own hardware thread, and a cheap yield in the kernel). Under Wine each call is
`SwitchToThread` -> `NtYieldExecution` -> `sched_yield`:
- SDK path: `<sdk>/src/system/xthread.cpp:1216-1255` `DelayNative`, then `MaybeYield`
  (`<sdk>/src/core/threading_win.cpp:87-90`);
- `PlanShortSleep` keeps zero native (`<sdk>/include/rex/system/short_sleep.h:83-93`).

At 90-180 k/s this is a syscall plus a scheduler pass per tick, and about 0.4 core of kernel time.

**Likely underrun mechanism (hypothesis).** In every tick the loop also calls the title's audio library
(`823Exxxx` / `823Fxxxx`) and takes guest spinlocks and critical sections:
- on F800068C: RtlEnterCriticalSection about 12 % of samples, KeAcquireSpinLockAtRaisedIrql about 3 %;
- on F8000534, the mixer: KeAcquireSpinLockAtRaisedIrql about 10 % of samples.

The SDK spinlock spins with `MaybeYield` (`<sdk>/src/kernel/xboxkrnl/xboxkrnl_threading.cpp:967-976`).
On an oversubscribed 4C/8T this gives two problems:
- a ~150 k/s ticker that keeps taking the audio-engine lock;
- when it is preempted while holding the lock, the mixer spins and yields until it runs again (lock
  holder preemption).

This fits the fact that underruns follow the F800068C phase, not general CPU load. It is not proven:
there is no per-lock contention counter.

## 3. Fix design, ranked

### (a) Host SDK: zero-delay backoff in `XThread::Delay` (recommended first; one SDK build)

Where: `<sdk>/src/system/xthread.cpp:1160-1214`, for `scaled_ticks == 0 && !alertable`. Alertable
SleepEx(0, TRUE) stays native, so APCs and wake callbacks are still drained.

Per-XThread state (members, no locks):
- `zero_streak`;
- `last_zero_return_tsc`.

New cvars:
- `ke_delay_zero_mode` = `native | backoff`. Default: backoff under Wine (via `platform_defaults.h`),
  native on Windows until a PC A/B.
- `ke_delay_zero_backoff_after` (default 32);
- `ke_delay_zero_backoff_us` (default 500, range 50-2000);
- `ke_delay_zero_gap_us` (default 200).

Logic:
1. On a zero delay, measure the guest work since the previous zero delay returned (now -
   `last_zero_return_tsc`).
   - If it is above the gap, or a non-zero delay or a real wait came in between: streak = 0.
   - Otherwise streak++.
2. If streak < after: native yield, as today.
3. Otherwise sleep with `rex::thread::HighResolutionSleep`, starting at 50 us and doubling up to the
   backoff cap. The per-thread waitable timer is already proven under Proton by the l79/l80 timer
   queue and the l80 short-sleep unit test.
4. Record `last_zero_return_tsc` after the delay returns, so the sleep itself does not reset the
   streak.

Counters appended to `MCLA_SLEEP`:
- `zero_backoffs`, `zero_backoff_us`, `zero_streak_max`;
- `top_site`: the guest return address of the Sleep call, read from guest memory at `ctx.r1 + 104`
  inside `sub_82460270`'s frame (its saved LR). This tells 0x821449D0 (tick end) apart from
  0x82136738 (bank-lock spin) and confirms the source in the first run.

Optional, same build: a spin/yield counter per contended `X_KSPINLOCK` address in
`xeKeKfAcquireSpinLock`, to test the lock-holder hypothesis.

Expected effect:
- The loop drops from about 180 k/s to about 2 k/s ticks.
- F800068C goes from 0.45-0.85 core (0.2-0.4 kernel) to about 0.02 core, which frees 0.45-0.8
  logical CPU on the Deck.
- Fewer SMT pairs are shared with the render, CP and F8000534 threads, and there is less audio-lock
  churn.
- Underruns: a large reduction if the hypothesis holds, at least the l79 "low phase" level (about 1
  per frame).
- fps: small (+0-0.5), from less SMT contention on render and CP.

Latency risk:
- The sound tick runs at about 2 kHz instead of 150 kHz. The XAudio frame is 5.33 ms and game sound
  updates are frame-rate work, so 0.5 ms granularity is ample.
- The bank-lock spin and the other title Sleep(0) spin-waits (`82148A10`, `82547140`, `82550AA0`,
  `822C1BF0`) gain at most about 0.5 ms per wait, and only after 32 consecutive spins.
- An isolated Sleep(0) is unchanged, so hand-off yields keep their behaviour.
- Watch for:
  - stream stalls (REX_PERF_AUDIT);
  - XMA counters;
  - sound start latency, by ear;
  - `MCLA_SLEEP max_us`.

### (b) Guest side: midasm hook at the tick's Sleep call (exact, narrower)

`[[midasm_hook]]` at 0x821449CC (`bl 0x821c91b8` in `sub_82144888`; syntax as in
`config/mcla_functions.toml:46-`), registers `r3`: if r3 == 0, set r3 = 1. Under l80 this becomes a
1 ms hires-timer sleep, which makes the loop run like a console tick that always had at least 1 ms
left.

- It needs codegen regeneration plus a host hook function.
- It covers only that call site, and only if the gate byte 0x827D1998 is set. If it is 0, the zeros
  come from the `82136700` spin instead.
- So run (a)'s `top_site` counter first.
- (b) cannot be "wait on the producer's event": the tick has no producer. Only the inner bank-lock
  spin (`82136700`) could become an event wait, keyed on the 0x828317A1 writers. It is minor and
  needs a function override of `sub_8213A0E8`.
- Prefer (a) as the general Wine fix. Keep (b) as a fallback if a backoff side effect shows up in
  other titles' loops.

### (c) Other

1. **Re-target the priority experiment.** Raise the guest audio engine thread F8000534 (identify it by
   its start address at thread creation, not by handle) to above_normal in the host. Guest priorities
   are ignored (`ignore_thread_priorities`, `<sdk>/src/system/xthread.cpp:45`).
   `apu_worker_priority` should stay neutral or be removed: its two threads use about 3 % of a core.
2. **Soft affinity hints:** keep F800068C (after (a), it hardly matters) and F8000534 off the SMT
   siblings of render and CP. Lower value; do it only if (a) plus (c1) leave underruns.
3. **Not useful:** more `hires_timer` coverage (the non-zero short delays are about 880/s and harmless)
   and the timer-queue spin tail (TimerQueue is not in the top 8).

## 4. Census arm (stage 0a) and the hot-pages gate

### 4.1 Configuration caveat (applies to l79 and l80 Deck owner runs)

All three sessions log the following:
- `MCLA dev override: memory_watch_disarm_bridge_safe_gaps=false`;
- `MCLA dev overrides: shared_memory_upload_ahead_16=false leaves shared_memory_upload_ahead_pages unchanged`
  (and the same for `_64`);
- `MCLA m10 defaults: shared_memory_upload_ahead_pages left at 0 (dev_override)`.

Every census line has `bridge=0 ahead=0`. This is by design:
- a `=false` upload-ahead line counts as the owner's "off" arm (`src/mcla_m10_defaults.h:26-34`;
  `docs/evidence/M10-proton-defaults-and-ki064-20261103.md:33-37`);
- an explicit bridge `false` line wins over the Wine default (`src/mcla_m10_defaults.h:91,97`).

So the Deck owner baselines since that template run **without** the M10 Wine watch defaults that
volunteers get. l66 "all" had both on.

**Zero-build lever.** In the Deck arm file, set `shared_memory_upload_ahead_16=true` and
`memory_watch_disarm_bridge_safe_gaps=true`, or delete those lines.

### 4.2 Stage-0a numbers (census minutes 2-4, 2646 frames, 180 s; l66 "all" for reference)

| per frame | l80 census (ahead 0, bridge 0) | l66 "all" (ahead 16, bridge on) |
|---|---|---|
| arm calls (ms, us/call) | **917 (11.9 ms, 13.0 us)** | 479 (8.0 ms, 16.7 us) |
| arms by source | VB 386 (5.0 ms), IB 473 (6.1 ms), tex 58 (0.8 ms), resolve/other 0 | IB 242, VB 167, tex 70 |
| pages per arm | **2.14**; 1-page 632/f (8.2 ms), 2-4 210 (2.5 ms), 5-16 67 (1.0 ms), 17-64 9 | 14 |
| us per arm by size | 1 p 13.0, 2-4 p 11.9, 5-16 p 15, 17-64 p 11 (flat) | - |
| disarm calls (ms, pages) | 364 (4.6 ms, 5.4 pages/call) | 185 (3.7 ms, 38 pages) |
| bridge shadow: saved disarm calls | 266/f (1311 pages) | (bridging on) |
| upload-ahead shadow 16 / 64: saved arms | **624/f** / 820/f | (ahead on) |
| write faults render (ms, us each) | 94 (6.6 ms, **70 us**) | 176 (6.4 ms, 36.6 us) |
| write faults other guest / CP | 3.9 (0.22 ms) / 0 | 9 / 0 |
| fault lock wait / VirtualQuery (render) | 0.65 / 0.18 ms | 0.67 / 0.3 ms |
| arm / disarm global-lock hold | 12.7 / 4.9 ms | 8.7 / 3.9 ms |
| VB/IB/tex requests; fast-path share | 5312 / 5181 / 97; VB 93 %, IB 91 %, tex 50 % | 96-97 % VB/IB |
| h8 blocks (snapshots) | 42-48 (l66 65) | 65 |
| h8 with texture watch | 4-11 (avg 7 of 44, about 17 %) | 22 of 65-71 |
| h8 share of arms / faults / upload pages / uploads | **88.1 % / 81.1 % / 81.8 % / 88.1 %** | 92 / 91 / 92 % |
| h4-7 / h1-3 / h0 share of arms | 7.9 / 3.0 / 1.1 % | - |

- **The l79 arm increase was "more calls", and the census shows why.** Per call the cost is lower
  (13.0 us against 16.7), but there are twice as many calls. With upload-ahead and bridging off, each
  arm or disarm covers about 2-5 pages instead of 14-38.
- **Faults halved, but each costs twice as much** (70 us). Render fault time is unchanged at 6.6 ms/f.

### 4.3 Projection against the gate (CP >= -3 ms/f)

1. **Restore ahead16 + bridge (no build).**
   - Empirical estimate from l66: arm about 8 ms/f, i.e. **CP -3 to -4 ms/f**.
   - The shadow counters give an upper bound of -624 arms/f, i.e. about -8 ms/f. They do not price the
     extra faults that upload-ahead causes.
   - Bridging removes about 266 disarm calls/f, about -3 ms/f. This is on the **render thread** (inside
     faults), not on the CP.
   - This alone probably meets the gate.
2. **Hot pages v2 on top of today's config (ahead 0).**
   - h8 arms: 808/f x 13 us = 10.5 ms/f. About 83 % is eligible under the per-block rule (minus h8
     blocks with a texture watch), so about -8.7 ms/f of arm.
   - Minus 1-2 ms/f for the prologue and staging, by the v1 lesson.
   - Net **CP -6.5 to -7.5 ms/f**: meets the gate.
   - Render-thread faults: 81 % of 6.6 ms gives about **-5 ms/f** on the second wall.
3. **Hot pages v2 on top of restored ahead16 + bridge** (the fair baseline).
   - h8 arms are about 88 % of 3.8-8 ms/f, about 83 % eligible: -2.8 to -5.8 ms/f, minus 1-2 ms/f
     overhead.
   - Net **CP -1 to -4.8 ms/f**. The gate is borderline and has to be measured.
   - The render-fault saving (about -4 to -5 ms/f) holds, and is the stronger argument.

**Verdict.**
- The census does not block hot pages: the projection meets the gate against today's config.
- The cheaper first step is a Deck run with ahead16 + bridge restored, using the same census switch.
  That number is the real baseline for the stage 0a/0b decision.

## 5. Recommended next owner runs (Deck, usual loop)

1. Override file fixed (ahead16 + bridge on), census on, everything else at the l80 defaults. Gives
   the watch baseline and the actual CP gain.
2. SDK build with (a) `ke_delay_zero_mode=backoff` + `top_site` + spinlock contention counters, arms
   backoff / native. Read:
   - F800068C %, process cores, underruns per audit, stream stalls;
   - `MCLA_SLEEP zero_backoffs` and `top_site`;
   - fps;
   - sound by ear: engine, music, start latency.

## Limits

- One run per arm, with driving that differs between runs. Percentiles exist only per minute.
  Underruns rest on two 20 s audits per arm.
- The game constant A (the tick period) and the gate byte 0x827D1998 could not be read: there is no
  decrypted image. Which Sleep(0) call site dominates (tick end or bank-lock spin) is inferred, not
  measured. `top_site` would settle it.
- The lock-holder-preemption mechanism for the underruns is a hypothesis.
- The census rates use minute windows only, because F10 mark windows overlap them. The shadow
  savings ignore the extra faults that upload-ahead causes.
- Wine/Linux syscall costs are inferred from thread kernel shares, not measured per call.
- Nothing here is a physical PASS.
