# KI-072 / KI-067: l71 South Central "lag-fest" session vs HWiNFO (2026-11-06)

Read-only analysis by an Opus subagent on 2026-11-06 of the l71 South Central session b0b7fdd5... with the owner HWiNFO CSV and the ninja logs of the project builds on the same PC; accepted by the primary agent.

Read-only analysis (Opus subagent) of the l71 owner session b0b7fdd5... (PC, MAIN profile, 1080p render target,
3840x2160 fullscreen, arm triad, build 0.20.37.0), 16:08:45-16:15:57 local (UTC+2). Sources: session `mcla.log`
(10,800 lines, complete from launch to exit), the F10 packages (state.json, manifest.json, report.txt, frame.bmp
only), and the owner HWiNFO CSV `HWiNFO_LOG_96037703.CSV` (214 rows, 211 samples, 16:08:59.7-16:15:59.8, 2.0 s,
same 385-column layout as the baseline CSV; parsed with an isolated Python outside its folder). Column mapping as in
[HWiNFO baseline](M11-ki067-hwinfo-baseline-20261106.md). Additional read-only source: the `.ninja_log` files of the
project's own build directories on the same PC (see section 3).

## 0. Result in one paragraph

The "foreign process" is **the project's own build**: a ninja + clang++ Release build in `private/l72-album-build`
(8 parallel jobs compiling the 17-18 s `mcla_recomp.N.cpp` units) started at 16:10:29-31 and its parallel phase ended
with the `mcla.exe` link at 16:13:53. HWiNFO total CPU went 28 -> 38 -> 55 -> 94 % at 16:10:29.7 / 31.7 / 33.8 / 35.7
and fell 94 -> 74 -> 47 % at 16:13:53.8 / 55.7 / 57.7: onset and end match the build to within 2 s. The same build
directories also explain the earlier episodes: the l68 KI-067 episode (2026-11-05 16:53:40-16:56:47) vs
`l69-host-build` 16:53:52-16:58:12; the l70 Hills Tournament races (12:20:17-12:27:42) and the KI-069 5.5 s freeze
(12:22:50) vs the SDK build `l71-sdk-b` 12:21:12-12:26:28; the session 668aa5ee episodes 14:38-14:41 and 14:51-14:53 vs
`l71-minidump-build` 14:38:15-14:42:57 and `l71-sdk-b` 14:50:53-14:53:40; and the police-session 8 s dip (16:02:36-46)
and post-exit burst (16:03:51) vs `l72-album-build` invocations at 16:02:35 and 16:03:50. Under the build the game's
own load stayed flat (proc_pct 22-26 %, 3.5-4.0 cores), the CPU stayed at its 35 W STAPM cap, the game's threads lost
clock (eff 4.0 -> 3.7 GHz) and cache/memory bandwidth, every CP unit cost rose x1.6-1.9 against the post-build minutes,
and the CP thread ran 97-98 % busy (CP-bound) with the GPU only 41-57 % busy. Fix lines: (owner/process side) no
builds on the play PC while an owner session runs; (project side) cut CP CPU per frame, because even without the build
this content runs 36-40 fps with the CP thread 95-96 % busy.

## 1. Session timeline

| local time | state |
|---|---|
| 16:08:45-16:09:11 | boot, title (focus lost 16:09:02-06) |
| 16:09:11-16:10:28 | mostly paused (UI cap forced, 30 fps; pauses 16:09:23-16:10:08 and 16:10:09-16:10:28) |
| **16:10:28.2** | game running (free roam, South Central streets; frames -4 and -11 show street driving, pursuit phase none, score 0) |
| **16:10:29-31** | `l72-album-build` ninja invocation starts (see section 3) |
| 16:10:35-16:13:39 | 11 F10 presses (16:10:35, 10:50, 10:55, 11:10, 11:29, 11:43, 11:48, 12:09, 12:35, 13:22, 13:39) |
| **16:13:53** | build's parallel phase ends (mcla.exe link); one long single compile continues to 16:15:38 |
| 16:14:12-16:14:16 | brief pause (timer-scale ramp, sim 1) |
| 16:14:16-16:15:53 | free roam again, 36-40 fps |
| 16:15:57 | window closed, exit 0 |

Only 10 F10 package folders are on disk (-2 ... -11); the first press (16:10:35, presumably -1) has a log line but no
folder in `live/`. The report.txt answers are empty in all ten.

## 2. Per window (game MCLA_PERF_SPLIT / MCLA_CPU windows; HWiNFO aggregated over the same window)

Definitions as in the baseline doc: CP us/draw = cp_cpu_ms/draws; kern ms/f = cp_kernel_ms/frames; memcpy GB/s =
upload_kb*1024/upload_ms; render own us/draw = (render_cpu_ms - render_wait_ms)/draws; CP ms/f = cp_cpu_ms/frames.
fps here = frames/wall. sys/proc = MCLA_CPU avg/max; eff = MCLA_CPU eff_mhz avg (min). HWiNFO: n samples, total CPU
usage avg/max, threads >50 % busy (of 16), package W, STAPM Limit %, Thermal Limit % max, Tctl avg, Zen5 per-thread
effective clock / Zen5 core clock (MHz), commit GB, physical available min GB, C: read and write MB/s avg/max,
NVIDIA W / core load %. HTC / PROCHOT CPU / PROCHOT EXT were **No in all 211 samples**.

| end | win s | fps | draws/f | CP us/draw | kern ms/f | memcpy GB/s | own us/draw | CP ms/f | sys % | proc % | eff MHz | n | HW total % | thr>50 | pkg W | STAPM % | Th.lim % | Tctl C | Zen5 eff/clk | commit GB | avail GB | C: rd | C: wr | NV W/% |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 16:09:47 (boot/paused) | 60.0 | 29.1 | 5746 | 1.79 | 1.81 | 4.50 | 1.48 | 10.3 | 25.7/53.9 | 12.7/34.3 | 4301 (4093) | 24 | 26/41 | 2 | 34.5 | 77 | 84 | 79.3 | 1758/4861 | 53.0 | 4.5 | 4/26 | 1/17 | 43/48 |
| 16:10:35 F10 (paused, 4 s driving + build onset) | 48.1 | 29.9 | 8801 | 1.88 | 2.66 | 4.11 | 1.69 | 16.6 | 29.0/96.7 | 14.1/23.1 | 4238 (3688) | 24 | 26/55 | 2 | 33.6 | 81 | 87 | 86.7 | 1801/4844 | 54.0 | 4.5 | 4/57 | 2/11 | 56/64 |
| **16:10:47** | 11.9 | 22.9 | 6527 | 5.36 | 7.01 | 2.05 | 4.31 | 35.0 | 89.2/95.2 | 24.0/26.6 | 3757 (3711) | 6 | 91/96 | 16 | 35.0 | 100 | 83 | 83.4 | 4110/4305 | 55.3 | 3.6 | 7/24 | 2/8 | 44/46 |
| **16:10:50 F10** | 3.1 | 20.2 | 6981 | 5.36 | 9.18 | 2.12 | 4.09 | 37.5 | 91.6/93.6 | 24.3/24.5 | 3730 (3721) | 2 | 92/92 | 16 | 35.0 | 100 | 82 | 82.3 | 4118/4307 | 55.9 | 3.3 | 6/6 | 9/15 | 45/40 |
| **16:10:55 F10** | 5.0 | 18.2 | 7534 | 5.74 | 7.55 | 2.23 | 3.94 | 43.3 | 92.2/97.5 | 23.6/24.5 | 3727 (3713) | 2 | 90/91 | 16 | 35.0 | 100 | 81 | 81.6 | 3889/4308 | 56.6 | 2.4 | 7/8 | 18/36 | 40/48 |
| **16:11:10 F10** | 15.2 | 23.5 | 6361 | 5.34 | 6.43 | 2.44 | 4.02 | 34.0 | 90.4/99.9 | 25.0/29.7 | 3716 (3670) | 8 | 92/97 | 16 | 35.0 | 100 | 82 | 81.8 | 4018/4234 | 56.9 | 1.2 | 22/53 | 2/7 | 46/51 |
| **16:11:29 F10** | 18.8 | 21.3 | 7024 | 5.43 | 7.32 | 2.37 | 3.88 | 38.1 | 92.0/96.7 | 24.0/26.5 | 3704 (3674) | 9 | 92/96 | 16 | 35.0 | 100 | 81 | 81.2 | 3885/4261 | 57.9 | 1.2 | 59/230 | 11/34 | 47/50 |
| **16:11:43 F10** | 14.0 | 20.9 | 7491 | 5.07 | 7.36 | 2.59 | 3.88 | 38.0 | 86.7/91.6 | 21.9/24.1 | 3734 (3689) | 7 | 89/95 | 16 | 35.0 | 100 | 81 | 81.1 | 3843/4299 | 57.9 | 1.8 | 34/156 | 10/18 | 45/47 |
| **16:11:47** | 3.9 | 25.1 | 5638 | 5.68 | 7.17 | 2.00 | 4.56 | 32.0 | 93.2/95.2 | 24.5/25.2 | 3728 (3719) | 2 | 91/92 | 16 | 35.0 | 100 | 81 | 80.8 | 3820/4338 | 58.6 | 1.6 | 111/209 | 16/29 | 46/57 |
| **16:11:48 F10** | 1.5 | 22.5 | 5844 | 6.29 | 6.43 | 2.08 | 4.82 | 36.8 | 95.2 | 23.8 | 3728 | 1 | 95 | 16 | 35.0 | 100 | 81 | 81.5 | 4082/4295 | 57.0 | 3.2 | 8 | 1 | 47/33 |
| **16:12:09 F10** | 20.3 | 27.9 | 5770 | 4.96 | 5.85 | 2.42 | 4.15 | 28.6 | 89.5/98.9 | 21.5/26.4 | 3726 (3689) | 10 | 91/99 | 16 | 35.0 | 100 | 82 | 81.2 | 3871/4290 | 58.2 | 1.9 | 14/24 | 5/9 | 50/52 |
| **16:12:35 F10** | 26.5 | 30.0 | 5446 | 4.86 | 5.19 | 2.45 | 4.22 | 26.5 | 87.2/94.2 | 21.8/26.3 | 3711 (3667) | 13 | 87/92 | 16 | 35.0 | 100 | 83 | 81.9 | 3713/4294 | 58.1 | 1.7 | 11/22 | 6/13 | 54/62 |
| **16:12:47** | 11.8 | 25.6 | 5838 | 5.32 | 5.35 | 2.45 | 4.33 | 31.0 | 91.0/96.0 | 26.2/30.7 | 3690 (3658) | 6 | 91/94 | 16 | 35.0 | 100 | 83 | 82.8 | 3875/4243 | 58.6 | 1.6 | 12/16 | 6/10 | 47/49 |
| **16:13:22 F10** | 35.1 | 23.3 | 6656 | 4.88 | 5.39 | 2.58 | 4.14 | 32.5 | 89.4/96.5 | 23.6/29.4 | 3703 (3666) | 18 | 91/95 | 16 | 35.0 | 100 | 84 | 83.1 | 3898/4242 | 58.0 | 1.5 | 8/18 | 6/15 | 53/54 |
| **16:13:38 F10** | 16.5 | 20.8 | 6692 | 5.53 | 6.61 | 2.27 | 4.23 | 37.0 | 91.4/95.9 | 24.8/27.5 | 3718 (3677) | 8 | 92/94 | 16 | 35.0 | 100 | 84 | 83.9 | 4068/4287 | 57.2 | 3.2 | 2/7 | 2/8 | 49/48 |
| **16:13:47** | 8.4 | 21.0 | 6861 | 4.99 | 7.72 | 2.31 | 4.34 | 34.3 | 90.7/93.7 | 25.0/28.4 | 3729 (3703) | 4 | 92/92 | 16 | 35.0 | 100 | 84 | 83.8 | 4028/4268 | 57.8 | 2.7 | 3/4 | 3/7 | 49/52 |
| 16:14:47 (build ends 16:13:53) | 60.0 | 35.7 | 6061 | 3.09 | 3.46 | 3.57 | 2.82 | 18.7 | 50.0/98.6 | 21.8/28.9 | 3978 (3721) | 30 | 53/96 | 7 | 35.0 | 100 | 91 | 88.5 | 2658/4574 | 56.4 | 3.1 | 4/30 | 2/35 | 63/77 |
| 16:15:47 | 60.0 | 40.1 | 4792 | 3.20 | 2.53 | 3.61 | 3.20 | 15.3 | 44.6/61.4 | 22.8/25.9 | 4010 (3884) | 30 | 46/55 | 5 | 35.0 | 100 | 91 | 90.0 | 2455/4590 | 56.1 | 4.6 | 2/10 | 2/13 | 66/84 |

(The 16:14:47 window still contains 16:13:47-16:13:57, i.e. the last 10 s of the build; its sys_pct max 98.6 and eff
min 3721 come from there.)

Per-minute lines (MCLA frame pacing/min, watch protect/min, draws per frame/min, gpu busy/min; windows end at :47):

| minute ending | fps | >50 / >100 ms | draws/f p50 (p90) | VirtualProtect arm us/call | GPU busy ms/f |
|---|---|---|---|---|---|
| 16:09:47 (boot/paused) | 29.1 | 16 / 4 | 6787 (10883) | 0.52 | 13.3 |
| 16:10:47 (paused to 16:10:28, build from 16:10:31) | 28.5 | 100 / 3 | 9890 (10660) | 0.83 | 20.7 |
| **16:11:47** | **21.7** | 333 / **20** | 7007 (8476) | **1.46** | 21.3 |
| **16:12:47** | 28.3 | 114 / 5 | 5447 (7041) | **1.35** | 19.4 |
| **16:13:47** | **22.3** | 291 / 9 | 6690 (8043) | **1.61** | 22.8 |
| 16:14:47 | 35.7 | 45 / 3 | 6142 (7583) | 0.86 | 21.1 |
| 16:15:47 | 40.1 | 15 / 0 | 4512 (6659) | 0.82 | 20.3 |

CP thread and GPU state per window (from MCLA_PERF_SPLIT): during the build cp_busy 97-98 % of wall, cp_cpu 72-83 %,
WAIT_REG_MEM 2-3 %, fence + read-back waits 4-13 %, GPU busy 41-57 %; texture phase 6.1-15.2 ms/frame, bind phase
3.8-5.6 ms/frame, watch arm 1.4-2.9 ms/frame, write-fault 1.6-3.4 ms/frame. After the build: cp_busy 95-96 %, cp_cpu
62-67 %, fence + read-back waits 21-29 % (waiting on the GPU), GPU busy 75-82 %; texture 3.4-4.3, bind 2.3-3.1, watch
arm 0.7-0.8, write-fault 0.95-1.1 ms/frame. MCLA_THREAD_CPU: GPU Commands thread 30.9-32.6 s CPU per 60 s after the
build vs 37.9-39.6 s per 60 s equivalent during it (e.g. 12.4 s in the 18.8 s window to 16:11:29).

## 3. The foreign load: identification and characterisation

**HWiNFO signature.** Onset 16:10:31.7 (38 %, 4 threads >50 %) -> 16:10:33.8 (55 %, 9 threads) -> 16:10:35.7
(94 %, all 16 threads >80 %, 8.0 active cores). Plateau 16:10:35.7-16:13:53.8: total 84-99 %, all 16 logical
threads >50 % in every sample (10-16 of them >80 %). End 16:13:55.7 (74 %) -> 16:13:57.7 (47 %). Duration **3 min
22 s**. Afterwards 39-60 % with 2-15 threads >50 % (the game now at 36-40 fps plus one remaining compile to 16:15:38).
Disk: C: reads mostly 5-25 MB/s with bursts of 90-230 MB/s at 16:11:23-29 and 16:11:43 (209 MB/s), writes 1-36 MB/s;
D: idle (0) throughout. Memory: commit 55.2 -> 60.5 GB (+5.3 GB), physical available 5.1 -> 1.2 GB min
(16:11:09-11), back to commit 57.5 GB / 4.6 GB available after 16:13:57. Network: no traffic during the load (the
only downloads are 1-12.5 MB/s at 16:09:45-49 and 4.3-4.5 MB/s at 16:15:29-31 on the USB Ethernet adapter).

**Power and clocks.** Package power pinned at 35.0 W with APU STAPM Limit 100 % in every sample of the load (before:
paused game at 30 fps, 31-36 W and STAPM 70-98 %). Zen5 core clock 4.86 GHz (paused) -> 4.17-4.44 GHz under the load
-> 4.4-4.7 GHz afterwards. Tctl actually **fell** during the load (86-88 -> 80-84 C, thermal limit 80-84 %) and rose
to 88-91 C (thermal limit 91 %) after it: the build spread the fixed 35 W over all 8 cores. No HTC/PROCHOT flag ever.
NVIDIA power 37-54 W and load 30-62 % during the load vs 57-70 W and 66-91 % after it (GPU starved by the CP thread).
CPU skin temperature rose monotonically 54.7 -> 60.4 C over the 7 minutes.

**Identity: the project's own parallel build.** `private/l72-album-build/.ninja_log` (ninja v6; timestamps
calibrated against the on-disk modification times of its outputs `mcla_crash_handler.exe` 16:10:37.64,
`Launch-MCLA.exe` 16:10:40.15, `mcla_build_lease.lib` 16:13:02.93, `mcla.exe` 16:13:53.26 and
`mcla_diagnostic_export_policy_test.exe` 16:15:38.02, which agree to <0.4 s) shows an invocation starting at
16:10:29-31 (Release, clang++, ninja), 268 edges and ~1,590 CPU-seconds of compile/link, **average 8.0 concurrent
jobs from 16:10:30 to 16:13:20**, 7 to 16:13:48, the `mcla.exe` link ending at 16:13:53, then one compile
(`mcla_diagnostic_export_policy_test.cpp`, 16:13:25-16:15:38) alone. The 8 heaviest jobs are generated recompiled
units `mcla_recomp.N.cpp` at 17-18 s each. Signature fit: 8 x clang++ on huge generated units = all 16 SMT threads
busy, +5 GB commit, moderate source/header reads, object writes; matches onset (<2 s) and end (<2 s) exactly. The
project's build-lease mechanism is evidently not gating builds against an active owner session on this PC.

**Repeat pattern: every earlier host-slowdown episode coincides with a build in this PC's build directories.**
Per-invocation start/end reconstructed the same way from all ten `.ninja_log` files modified since 2026-11-05:

| episode (source) | episode time (local) | concurrent build (start-end, CPU-s, avg jobs) |
|---|---|---|
| l68 KI-067, first episode (2026-11-05) | 16:53:40-16:56:47 | `l69-host-build` 16:53:52-16:58:12, 1,469 CPU-s, 8.7-9.4 jobs in 16:54-16:55 |
| l70 Hills Tournament, both races x2 (KI-067 second episode) | 12:20:17-12:27:42 | `l71-sdk-b` 12:21:12-12:26:28 + 12:26:44-12:27:18, ~2,590 CPU-s, 15.8 jobs at 12:21 |
| KI-069 5.5 s CP freeze (same session) | 12:22:50-55 | inside the same SDK build |
| 668aa5ee episode 1 (KI-072) | 14:38-14:41 | `l71-minidump-build` 14:38:15-14:40:46 (1,147 CPU-s, 7.2-8.8 jobs) + 14:40:46-14:42:57 |
| 668aa5ee episode 2 | 14:43-14:46 | only partial: `gate-host-build` 14:47:39-14:49:43 (small); no ninja build 14:43-14:46 (and 14:45-14:46 was mostly the unfocused pause menu per the KI-072 correction) - unexplained by ninja logs |
| 668aa5ee episode 3 | 14:51-14:53 | `l71-sdk-b` 14:50:53-14:53:40, 2,346 CPU-s, 16.5-18.7 jobs |
| police baseline 8 s dip | 16:02:36-46 | `l72-album-build` invocation starting 16:02:35 (configure 16:02:13) |
| police post-exit burst | 16:03:51-16:04:09 | `l72-album-build` 16:03:50-16:07:14 (1,089 CPU-s) |
| **this session** | **16:10:31-16:13:55** | **`l72-album-build` 16:10:29-16:15:38 (1,592 CPU-s, 8 jobs to 16:13:53)** |

Not periodic: it repeats whenever an agent builds while the owner plays. The candidates in the brief are excluded by
signature: antivirus full scan / Windows Search / fossilize / ngen / Windows Update would not produce 8-16 fully busy
threads with a +5 GB commit step that starts and stops with a ninja invocation to the second, and there was no network
download. The launcher is excluded: while the game runs it only runs a 1 s UI timer (`kRefreshTimer`, 1000 ms) that
calls `LauncherController::State::Poll()` -> `process.Poll()` (exit code and job-process count) and returns early
while the game is alive (`src/mcla_launcher_controller_win.cpp` Poll, `src/mcla_launcher_ui_win.cpp` SetTimer).

Correction to the baseline doc: the 16:02:36-46 dip and the 16:03:51-16:04:09 post-exit burst were the l72 build,
not "a process start / scan / update" or "the launcher's after-session work".

Caveats: the process identity is inferred from timing and signature, not from per-process counters (HWiNFO has none);
ninja start times are derived from relative edge times (+-1 s); builds outside these ten directories (or non-ninja
work such as test runs) are not covered, which may explain the 14:43-14:46 minutes.

## 4. The game under the load

- Workload flat or lower: draws/frame 5.4-7.5k during the build vs 4.8-6.1k after it; game proc_pct 21.5-26.2 %
  avg (3.5-4.0 cores) during vs 21.8-22.8 % (3.5-3.6 cores) after. The game did not do more work.
- Unit costs during vs after the build (same session, same area): CP CPU per draw 4.86-6.29 vs 3.09-3.20 us
  (x1.5-2.0); kernel time 5.2-9.2 vs 2.5-3.5 ms/frame (x2-3); upload memcpy 2.0-2.6 vs 3.6 GB/s (x0.6); render
  thread own time 3.9-4.8 vs 2.8-3.2 us/draw (x1.3-1.6); VirtualProtect arm 1.35-1.61 vs 0.82-0.86 us/call (x1.6-1.9);
  texture phase 6-15 vs 3.4-4.3 ms/frame (x2-3.5, the most inflated phase: memory/cache-bound under 8 compilers).
  This is exactly the KI-067 pattern (x1.5-2 at flat workload), here with a known cause. The game's threads ran at
  eff 3.69-3.76 GHz (vs 3.98-4.01 after, 4.24-4.30 paused before) and shared L3/DRAM bandwidth with 8 clang++ jobs.
- CP-bound: cp_busy 97-98 % of wall with 26.5-43.3 ms CP CPU per frame; GPU only 41-57 % busy (19-23 ms/frame);
  WAIT_REG_MEM 2-3 %. fps 18-30 per window, 21.7 / 28.3 / 22.3 per minute, 20 frames >100 ms in the worst minute.
- Without the load: 16:14:16-16:15:53 ran 35.7 / 40.1 fps at 6.1k / 4.8k draws/frame with CP 15-19 ms/frame. At the
  post-build unit cost the build-minute content (5.4-7.5k draws) would need ~17-24 ms CP CPU per frame, i.e. about
  35-40 fps, where the GPU (20-21 ms/frame, 75-82 % busy) also starts to bind. So yes: with the build gone this
  content runs 36-40 fps, as the last minutes show. Headroom is thin: even then the CP thread is 95-96 % busy and the
  CPU is at its 35 W STAPM cap with Tctl 88-91 C, so any host contention costs fps 1:1.
- Pre-load windows (16:09, 16:10:35; CP 1.8-1.9 us/draw) are not a driving baseline: the game was paused at the
  30 fps UI cap until 16:10:28 and driving started only 3 s before the build.

## 5. l71 novelties in this session

- `MCLA_LOG kind=minute` (7 lines): mode=async, **dropped=0** in every minute (dropped_total=0), writes 4,420-12,185
  per minute, queue_max 4-16 lines outside the build and 38-85 lines (33-73 KB) during it; write_max_ms 0.2-0.4
  except **105.6** (16:11) and **18.1** (16:12); flush_max_ms <=0.2; slow=1 only in the 16:11 minute.
- The single slow line: `[2026-11-06 16:11:38.881] [warning] [core] [t42864] MCLA_LOG slow write: ms=105.6
  thread=38736 bytes=550 suppressed=0` - the Log Writer thread (t42864) spent 105.6 ms writing a 550-byte line produced
  by the GPU Commands thread (38736), during the build with physical memory nearly exhausted (1.2-2.1 GB available)
  and C: read bursts (16:11:23-29 up to 230 MB/s, 16:11:43 209 MB/s). With async logging the CP thread was not blocked
  (no MCLA_CP_FREEZE); under the old synchronous sink this would have been a 105 ms CP stall - which supports the
  KI-069 mechanism (the 12:22:50 freeze also ran inside an SDK build).
- **No MCLA_CP_FREEZE** line at all (watchdog 1000 ms).
- Minidump policy: config line `live minidump for user snapshots off (skipped-by-default)`; all 11 F10s logged
  `live minidump policy=skipped-by-default written=0 ms=0 game_stall_upper_bound_ms=0`; package timing total
  207-304 ms (process census 86-150 ms, save 57-93 ms, frame 29-42 ms, publish 22-45 ms), minidump_ms=0. Manifests
  confirm `minidump: false`, frame source guest-output-readback 1920x1080 "nonuniform".
- MCLA_CPU: ntpi_limit 2000/2000/2000 throughout (no OS-visible limit), perf_pct 184-189 during the build vs 199-201
  after and 212-215 paused; temp unavailable (temp_src=acpi_tz). MCLA_CPU sys_pct matches HWiNFO total usage within
  0-3 points in every window again.

## 6. Conclusion for KI-072 / KI-067

The South Central lag-fest of this session is a host CPU contention episode caused by a parallel build of the
project itself (8 x clang++ on the same laptop) during 16:10:31-16:13:55, on top of a game that is CP-bound with little
headroom (CP thread 95-98 % busy, 35 W STAPM-capped CPU). It has the full KI-067 signature (all CP/host unit costs
x1.5-2 at flat workload, game load flat, power capped, no thermal flag), and the same coincidence with a build holds
for the l68 episode, the l70 Hills Tournament episode (and the KI-069 freeze), two of the three 668aa5ee episodes and
the police-session dip. It is not thermal throttling, not the power mode, not streaming/pipelines/VRAM, not the
launcher.

Two fix lines:

1. **Process / owner side (immediate):** no project builds, tests or other heavy work on the owner's play PC while an
   owner session runs (gate builds on "mcla.exe running" / an active launcher session, or build elsewhere). Lower
   priority or -j alone is not enough: the build still takes the shared 35 W STAPM budget, L3 and DRAM bandwidth.
   Re-test the same South Central free roam and a championship with no build running (and keep HWiNFO + MCLA_CPU) to
   confirm 36-40 fps and the healthy unit costs. Then decide whether KI-067 can be closed as an environment cause.
2. **Project side:** cut CP-thread CPU per frame (texture/bind phases, ~1.6-2.1 M VirtualProtect arm calls per minute,
   read-back steps, CP work split), because even uncontended the CP thread runs 95-96 % busy at 36-40 fps; more
   headroom makes the game robust against any background load on volunteer PCs.
