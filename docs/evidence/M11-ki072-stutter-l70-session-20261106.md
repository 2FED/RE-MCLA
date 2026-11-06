# KI-072 high-speed stutter: l70 PC session 668aa5ee attribution (2026-11-06)

Read-only analysis by an Opus subagent on 2026-11-06 of the owner l70 PC session 668aa5ee... (6 logs, 5 F10 packages); accepted by the primary agent.

Read-only analysis by an Opus subagent on 2026-11-06. Owner l70 PC session, build 2788628F... (0.20.37.0), MAIN
profile, 1080p Unlocked (window 3840x2160 fullscreen, guest 960x540 at scale 2), volunteer defaults
(`d3d12_perf_split=1`, `d3d12_livery_diag2=true`, read-back sync composite on, texture soft cap 2048 MB, hard 5595 MB).
Session 13:53:02-14:57:27 local (64 min), exit code 0. Store seed: existing, 775 shaders / 20,772 descriptions;
20,103 pipelines created from the store in 185 ms at start. Sources: session mcla.5..1.log + mcla.log (complete from
launch, 57,392 lines), five F10 packages (manifest, state.json, report.txt with empty owner fields, frame.bmp of
package 5 viewed as a temporary downscaled PNG, since deleted). Method: scripts over the per-minute lines
(`MCLA frame pacing/min`, `MCLA_PERF_SPLIT`, `MCLA_WRM`, `MCLA_READBACK(_SYNC)`, `MCLA_VRAM`, `MCLA_THREAD_CPU`,
`MCLA watch protect/min`, async pipeline summaries), the 5 x 300 audit samples, and a 10-second frame-rate series
rebuilt from the frame-tagged livery-diag lines. Minute windows are labelled by their end time.

## 0. Answer in brief

- **The collapse is the KI-067 host slowdown again, not streaming, shaders, read-back or paging.** Three episodes
  of 2-3 minutes each (14:38:39-14:41:29, 14:43:49-14:45:59, 14:51:29-14:53:29, plus a 20-s dip at 14:48:39) start
  and stop within 10 s. Inside them every host and guest CPU unit cost rises 1.5-2.8x at an unchanged or smaller
  workload: CP CPU per draw 2.7-3.0 -> 4.2-5.9 us, CP kernel time 2.3-2.8 -> 4.4-6.5 ms/frame, VirtualProtect arm
  0.64-0.76 -> 1.14-1.98 us/call, upload memcpy 3.4-3.8 -> 1.85-2.6 GB/s, texture phase 0.66-0.75 -> 1.1-1.75 us/draw,
  and the guest render thread's own (recompiled game) work per draw 2.5-2.7 -> 3.9-5.6 us.
- **Decisive control inside this session:** the same paused frame (pause menu, exactly 7.1k draws every frame,
  window unfocused) cost 30.1 ms of CP CPU per frame at 14:41 and 19.4 ms at 14:43 (x1.55), and the guest render
  thread's own work 26.0 vs 16.8 ms per frame (x1.55). Identical content, identical ratio on a host thread and a
  guest thread: the CPU itself ran slower. At 14:45-14:46 the paused frame again ran at 15-17 fps.
- **High speed amplifies it but does not cause it.** The late-session city/race content is heavier (6-7k draws,
  9 MB read-back per frame vs 4-5k and 5.3 MB earlier); healthy late minutes run 35-36 fps CP-bound (the l69 freeway
  level). When the slowdown hits, that becomes 20-26 fps with 12-18 frames over 100 ms per minute. F10 package 5
  (14:44:05) shows a race, lap 2/4, 93 mph in 3rd gear on a city street, at 16.5 fps in its 0.7 s split window
  (CP CPU 6.70 us/draw, kernel 7.1 ms/frame, memcpy 2.04 GB/s).
- **p50 33 ms is a cap only in pause/menu minutes** (UI cap forced, guest vblank 60 Hz: p50 33, p99 35-36). In
  running minutes the guest vblank is 240 Hz, unlocked, and p50 is load: 20-26 ms healthy, 39-46 ms in the episodes.
  Under the pause cap the slowdown quantises frames to 50 ms (3 vblanks), which is why 14:41, 14:45 and 14:46 show
  p50 49-50 ms and 457-614 frames over 50 ms: those three windows were mostly the pause menu with the window
  unfocused, not driving.
- **No paging or disk wait seen:** VRAM flat 5.39 GB, non-local 130-190 MB (budget 15 GB), no budget step after
  14:20; audit stream reads max 1.08 ms, stalls 0; no multi-second freeze (session max interval 849 ms at boot,
  514 ms after a race restart).
- **Next experiment:** l71 `pc-triad` arm (MCLA_CPU census + async logging + no F10 minidump), a 20-25 min drive
  that ends with the same late-game freeway/city race at 100+ mph, F10 at the onset of the lag, 30-60 s into it,
  and right after it clears; plus a 30 s pause-menu control during an episode. The `MCLA_CPU` `eff_mhz`/
  `ntpi_limit_mhz` vs `sys_pct`/`proc_pct` columns decide between CPU clock/thermal/power limiting and a foreign
  process.

## 1. Timeline (menu-state, focus and F10 lines)

| time (local) | state | notes |
|---|---|---|
| 13:53:02-13:54:06 | boot, title, menus | fullscreen 13:53:14 |
| 13:54:06-13:56:20 | running | F10 #1 13:55:22 and #2 13:56:03 (cut-scene audio, KI-071); audits 58-59 fps |
| 13:56:20-14:00:25 | paused, window unfocused 13:56:23-14:00:19 | 30 fps UI cap; mild slowdown (CP 3.8-4.1 us/draw, arm 0.79-0.89) |
| 14:00:25-14:05:27 | running with short pause/cut transitions | F10 #3 14:01:34 (cut-scene audio), audit 56 fps |
| 14:05:27-14:10:18 | pauses and unfocused periods, short running | F10 #4 14:07:14 (police, cut transition), audit 39 fps |
| 14:10:18-14:17:12 | running (free roam), 40-44 fps | |
| 14:17:12-14:23:00 | garage: KI-035 hook fired 14:17:34 (state 29, complete); windowed and unfocused 14:17:30-14:19:24; cut/transition states to 14:22 | VRAM budget fell to 5.85-5.90 GB: 9 near-budget steps 14:19:24-14:20:44, texture hard limit 5595 -> 3291 MB (stayed there) |
| 14:23-14:31 | running with pauses, 39-49 fps | |
| 14:31:17 | KI-035 hook fired again (garage), cut states to 14:33 | |
| 14:33-14:38:38 | running/pauses, 34-41 fps | |
| **14:38:39** | **episode 1 starts** (10-s fps 37 -> 21) while running | race start transition 14:39:11-14:39:13 |
| 14:39:51-14:43:20 | paused, window unfocused 14:39:53-14:43:19 (brief refocus 14:40:14, 14:41:45-53) | paused frame at 20 fps until 14:41:29, then 30 fps (cap) |
| 14:43:20 | race resumed (lap 2/4) | |
| **14:43:49** | **episode 2 starts** (10-s fps 31 -> 16) | |
| 14:44:05.3 | **F10 #5** (lag press): HUD 01:11.96, lap 2/4, 93 mph | export took 7.3 s; minidump stop 409 ms at 14:44:12.6 |
| 14:44:14-14:44:19.7 | pause, race restart (cut 14:44:17.8-19.7) | |
| 14:44:28.9-14:46:08.9 | paused; window unfocused 14:44:31-14:46:06 | paused frame 14-17 fps until about 14:45:55 |
| 14:46:09 | running, 35-36 fps (episode 2 over about 14:45:59) | restarts 14:46:57 and 14:47:23 |
| 14:48:39-14:48:59 | short dip to 22-25 fps | |
| 14:49:49-14:50:23 | paused / unfocused | |
| **14:51:29-14:53:29** | **episode 3** while running (10-s fps 16-24) | |
| 14:53:39-14:57:27 | running 30-42 fps with pauses; fullscreen off 14:57:08; window closed 14:57:27 | |

## 2. Per-minute table

State seconds per window: run = game running, pause = sim paused (pause menu, 30 fps UI cap, guest vblank 60 Hz),
cut = racer-off transition/cut-scene, -unf = window unfocused. Per-frame values divide the minute totals by the
perf-split frame count. "render own" = render-thread CPU minus its wait on the CP (the guest render thread spins and
is pegged at 55-57 s/min in every minute). Not instrumented in l70: texture creations/evictions, pipeline creations
(only waits: the async-pipeline summary, logged only when waits occur), shader-store additions (store at start only),
`[content]`/file lines (none logged besides the usual `t:\mc4\art\city` VFS misses at boot).

### 2a. Pacing, work and waits

| window end | state (s) | fps | interval p50/p99/max ms | >50 / >100 ms | draws/f p50 (max) | CP busy ms/f | GPU ms/f | render own/wait ms/f | fence+rb wait ms/f | WRM ms/f (max ms) | targeted rb waits (max ms) | PSO waits / ms | uploads/f, MB/f | VRAM local/budget MB (non-local) | near-budget steps |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 13:54 | boot 27 pause 23 run 5 cut 5 | 28.9 | 33/51/849 | 23 / 5 | 5.7k (10.2k) | 12.2 | 13.9 | 7.9/26.2 | 1.3 | 22.3 (234) | 1254 (11) | 0 / 0 | 690, 6.1 | 3845/7131 (148) | 0 |
| 13:55 | run 57 pause 2 | 45.0 | 21/35/58 | 6 / 0 | 4.6k (8.1k) | 20.4 | 18.4 | 13.1/8.7 | 5.8 | 1.8 (31) | 3044 (13) | 7 / 1 | 882, 7.3 | 4295/7131 (135) | 0 |
| 13:56 (split 0.8 s, per-frame columns unreliable) | run 60 | 55.6 | 17/28/130 | 5 / 1 | 3.4k (6.6k) | 18.1 | 15.3 | 8.9/8.2 | 9.2 | 82.3 (5) | 3664 (120) | 8 / 0 | 326, 2.9 | 4483/7131 (138) | 0 |
| 13:57 | pause-unf 40 run 16 pause 3 | 36.2 | 32/50/86 | 19 / 0 | 4.9k (5.3k) | 19.4 | 18.4 | 15.4/11.8 | 3.5 | 8.2 (21) | 2225 (61) | 0 / 0 | 841, 6.8 | 4528/5912 (146) | 0 |
| 13:58 | pause-unf 60 | 29.9 | 33/36/65 | 6 / 0 | 4.9k (4.9k) | 20.5 | 20.4 | 18.4/14.7 | 1.0 | 13.0 (20) | 1793 (44) | 0 / 0 | 1065, 8.4 | 4528/7131 (146) | 0 |
| 13:59 | pause-unf 60 | 29.7 | 33/49/267 | 12 / 1 | 4.9k (4.9k) | 22.5 | 20.5 | 19.8/13.5 | 1.1 | 11.2 (19) | 1780 (8) | 0 / 0 | 1067, 8.4 | 4528/5914 (146) | 0 |
| 14:00 | pause-unf 60 | 29.8 | 33/49/52 | 10 / 0 | 4.9k (4.9k) | 22.0 | 20.4 | 19.5/13.5 | 0.9 | 11.5 (19) | 1789 (9) | 0 / 0 | 1063, 8.4 | 4528/7131 (146) | 0 |
| 14:01 | run 27 pause-unf 17 pause 15 | 37.3 | 26/50/72 | 22 / 0 | 4.9k (10.2k) | 20.2 | 19.0 | 14.8/11.7 | 4.0 | 6.6 (28) | 2565 (13) | 0 / 0 | 906, 7.5 | 4852/5821 (142) | 0 |
| 14:02 (split 30 s) | run 56 cut 4 | 54.0 | 17/34/63 | 5 / 0 | 2.7k (5.3k) | 16.4 | 15.6 | 8.4/9.2 | 8.7 | 3.6 (47) | 3398 (16) | 0 / 0 | 293, 3.0 | 5154/5992 (143) | 0 |
| 14:03 | run 59 | 41.9 | 24/41/209 | 14 / 2 | 4.8k (7.0k) | 22.6 | 20.4 | 12.6/10.9 | 8.4 | 1.2 (29) | 2602 (16) | 0 / 0 | 777, 6.7 | 5385/6648 (139) | 0 |
| 14:04 | pause 38 run 22 | 34.5 | 32/38/66 | 8 / 0 | 5.2k (9.4k) | 19.8 | 21.2 | 13.2/15.5 | 4.5 | 9.2 (29) | 2180 (12) | 3 / 0 | 988, 8.7 | 5385/6648 (152) | 0 |
| 14:05 | run 30 pause 29 | 37.2 | 26/35/107 | 5 / 1 | 6.2k (9.4k) | 20.5 | 21.1 | 13.8/12.9 | 4.5 | 6.3 (29) | 2290 (15) | 0 / 0 | 1075, 8.9 | 5385/6648 (145) | 0 |
| 14:06 | pause-unf 33 run 24 pause 2 | 34.8 | 32/50/114 | 28 / 2 | 6.8k (7.5k) | 22.8 | 19.8 | 15.5/12.7 | 2.9 | 5.8 (16) | 2253 (77) | 0 / 0 | 1456, 10.7 | 5385/6255 (146) | 0 |
| 14:07 | run 28 pause-unf 18 pause 13 | 38.2 | 23/46/83 | 14 / 0 | 5.0k (9.6k) | 20.2 | 19.1 | 13.6/12.3 | 4.0 | 6.0 (28) | 2321 (10) | 0 / 0 | 1147, 9.0 | 5385/6059 (138) | 0 |
| 14:08 (split 50 s) | pause-unf 25 run 24 cut 8 | 37.9 | 26/35/102 | 3 / 1 | 3.9k (6.8k) | 16.1 | 17.4 | 11.0/15.8 | 3.7 | 12.3 (46) | 2339 (12) | 0 / 0 | 803, 6.5 | 5385/6059 (143) | 0 |
| 14:09 | pause-unf 60 | 30.0 | 33/35/36 | 0 / 0 | 4.1k (4.1k) | 14.0 | 18.1 | 12.0/21.1 | 0.9 | 19.3 (23) | 1801 (8) | 0 / 0 | 895, 6.7 | 5385/6059 (143) | 0 |
| 14:10 | pause-unf 46 run 12 pause 2 | 33.6 | 33/35/59 | 3 / 0 | 4.1k (6.7k) | 15.7 | 18.0 | 12.0/17.5 | 2.5 | 14.0 (23) | 2313 (11) | 0 / 0 | 843, 6.5 | 5385/6321 (140) | 0 |
| 14:11 | run 53 cut 4 pause 2 | 39.8 | 24/65/162 | 42 / 4 | 4.8k (8.0k) | 22.4 | 20.3 | 12.6/12.1 | 7.8 | 2.6 (30) | 2509 (19) | 1 / 0 | 807, 7.0 | 5385/6321 (137) | 0 |
| 14:12 | run 60 | 41.9 | 23/42/103 | 15 / 1 | 4.9k (8.6k) | 22.8 | 20.6 | 13.2/10.4 | 7.9 | 1.0 (5) | 2644 (14) | 0 / 0 | 754, 6.3 | 5385/6321 (141) | 0 |
| 14:13 | run 56 pause 2 | 42.7 | 22/50/138 | 28 / 6 | 3.8k (7.1k) | 21.6 | 19.8 | 11.2/11.9 | 8.8 | 1.8 (27) | 2653 (18) | 0 / 0 | 645, 6.1 | 5385/6648 (139) | 0 |
| 14:14 | run 60 | 41.5 | 23/44/120 | 20 / 3 | 4.8k (8.5k) | 23.1 | 20.7 | 13.5/10.4 | 8.0 | 1.0 (5) | 2623 (14) | 0 / 0 | 791, 6.8 | 5385/6455 (144) | 0 |
| 14:15 | run 60 | 43.1 | 22/38/67 | 11 / 0 | 4.5k (7.4k) | 22.2 | 20.2 | 12.5/10.4 | 8.3 | 1.0 (5) | 2661 (14) | 0 / 0 | 699, 6.1 | 5385/6455 (139) | 0 |
| 14:16 | run 49 pause 11 | 41.1 | 22/50/275 | 27 / 5 | 4.3k (7.1k) | 20.9 | 19.8 | 11.7/12.3 | 7.8 | 3.4 (29) | 2579 (17) | 0 / 0 | 732, 6.4 | 5385/6648 (133) | 0 |
| 14:17 | run 60 | 43.5 | 22/50/89 | 27 / 0 | 4.5k (7.0k) | 22.0 | 19.9 | 13.1/9.7 | 8.2 | 1.0 (5) | 2771 (13) | 0 / 0 | 744, 7.1 | 5391/6648 (141) | 0 |
| 14:18 | run 17 cut-unf 17 pause 14 cut 11 | 33.9 | 33/41/326 | 17 / 3 | 4.4k (7.0k) | 17.1 | 20.0 | 13.1/16.2 | 3.7 | 12.3 (30) | 2075 (17) | 15 / 1 | 772, 7.2 | 5301/5994 (181) | 0 |
| 14:19 | cut-unf 60 | 30.0 | 33/35/36 | 0 / 0 | 4.4k (4.4k) | 14.7 | 22.3 | 15.4/17.6 | 1.0 | 18.6 (24) | 1800 (6) | 0 / 0 | 943, 9.6 | 5301/5987 (181) | 0 |
| 14:20 | cut 40 cut-unf 19 | 30.1 | 33/35/36 | 0 / 0 | 4.4k (4.5k) | 14.2 | 22.2 | 14.5/18.4 | 1.3 | 19.1 (26) | 1806 (14) | 0 / 0 | 941, 9.6 | 5386/5854 (183) | 5 |
| 14:21 | cut 59 | 29.9 | 33/35/368 | 2 / 1 | 4.3k (4.8k) | 13.9 | 21.3 | 14.0/19.2 | 1.8 | 19.5 (33) | 2473 (45) | 0 / 0 | 845, 8.3 | 5386/6425 (181) | 9 |
| 14:22 | cut 37 pause 18 run 5 | 31.5 | 33/35/84 | 2 / 0 | 4.2k (5.7k) | 13.7 | 19.8 | 13.0/18.6 | 2.1 | 18.0 (26) | 2027 (14) | 0 / 0 | 777, 7.1 | 5386/6425 (149) | 9 |
| 14:23 | pause 36 run 23 | 35.7 | 32/45/351 | 18 / 3 | 4.9k (10.1k) | 18.9 | 20.3 | 12.5/15.2 | 4.2 | 9.1 (29) | 2197 (14) | 0 / 0 | 992, 8.3 | 5386/6617 (141) | 9 |
| 14:24 | run 60 | 48.9 | 20/36/75 | 9 / 0 | 4.1k (7.0k) | 19.4 | 17.4 | 12.6/7.6 | 5.8 | 1.0 (5) | 3117 (14) | 0 / 0 | 701, 7.0 | 5386/6617 (143) | 9 |
| 14:25 | pause 47 run 13 | 32.0 | 33/36/116 | 12 / 3 | 7.1k (9.3k) | 20.4 | 24.1 | 13.5/17.5 | 5.4 | 10.9 (26) | 1955 (12) | 1 / 0 | 907, 8.5 | 5386/6617 (154) | 9 |
| 14:26 | run 46 pause 13 | 38.7 | 24/50/104 | 27 / 1 | 4.7k (9.3k) | 22.3 | 21.2 | 13.0/12.6 | 7.4 | 3.5 (26) | 2407 (15) | 0 / 0 | 824, 7.5 | 5386/6617 (144) | 9 |
| 14:27 | run 48 pause 11 | 40.0 | 24/38/84 | 12 / 0 | 4.5k (9.4k) | 21.7 | 20.9 | 12.4/12.4 | 8.0 | 3.3 (25) | 2472 (17) | 0 / 0 | 800, 7.8 | 5371/6617 (143) | 9 |
| 14:28 | run 57 pause 3 | 43.6 | 21/49/101 | 21 / 1 | 4.4k (8.5k) | 21.3 | 19.8 | 12.0/10.6 | 8.1 | 1.6 (23) | 2671 (14) | 0 / 0 | 656, 6.4 | 5387/6617 (143) | 9 |
| 14:29 | run 60 | 44.1 | 22/38/83 | 11 / 0 | 4.3k (8.8k) | 21.6 | 19.8 | 12.1/10.3 | 8.3 | 1.0 (5) | 2735 (13) | 0 / 0 | 736, 6.9 | 5387/6617 (143) | 9 |
| 14:30 | run 60 | 40.6 | 24/49/125 | 21 / 3 | 5.1k (8.2k) | 23.6 | 21.1 | 13.3/11.1 | 7.7 | 1.0 (5) | 2509 (15) | 0 / 0 | 893, 8.3 | 5387/6617 (143) | 9 |
| 14:31 | pause 33 run 26 | 39.0 | 23/35/117 | 3 / 1 | 3.8k (10.6k) | 18.2 | 19.0 | 11.3/14.0 | 5.7 | 7.4 (29) | 2459 (19) | 0 / 0 | 665, 6.0 | 5387/6617 (140) | 9 |
| 14:32 | cut 45 run 10 pause 4 | 34.1 | 33/35/332 | 6 / 1 | 3.8k (5.8k) | 14.3 | 19.5 | 11.8/17.2 | 3.6 | 14.9 (30) | 2726 (49) | 0 / 0 | 647, 6.5 | 5388/6617 (184) | 9 |
| 14:33 | cut 48 run 8 pause 4 | 34.1 | 33/35/83 | 6 / 0 | 5.0k (9.9k) | 14.6 | 19.9 | 12.2/16.8 | 3.4 | 14.7 (29) | 2743 (14) | 0 / 0 | 653, 7.2 | 5388/6617 (154) | 9 |
| 14:34 | pause 58 | 30.5 | 33/35/51 | 1 / 0 | 9.8k (10.1k) | 18.9 | 23.8 | 13.7/18.9 | 2.8 | 13.8 (29) | 1861 (15) | 0 / 0 | 1036, 9.1 | 5388/6617 (152) | 9 |
| 14:35 | run 40 pause 19 | 39.3 | 24/41/88 | 11 / 0 | 5.8k (9.7k) | 19.9 | 18.9 | 13.1/12.0 | 4.0 | 5.5 (29) | 2628 (13) | 0 / 0 | 901, 7.8 | 5388/6617 (141) | 9 |
| 14:36 | run 60 | 41.0 | 24/46/112 | 22 / 2 | 6.2k (8.5k) | 23.3 | 19.6 | 14.7/9.3 | 4.4 | 1.1 (5) | 2783 (11) | 0 / 0 | 1145, 9.9 | 5388/6617 (148) | 9 |
| 14:37 | pause-unf 29 run 17 pause 8 cut 4 | 35.1 | 32/35/209 | 10 / 1 | 5.3k (6.6k) | 18.6 | 20.6 | 12.3/15.7 | 5.2 | 9.7 (30) | 2237 (15) | 0 / 0 | 793, 6.7 | 5388/6322 (148) | 9 |
| 14:38 | run 36 pause-unf 22 | 34.3 | 29/53/170 | 26 / 4 | 5.6k (8.7k) | 23.8 | 22.1 | 14.9/14.0 | 5.6 | 5.4 (18) | 2142 (16) | 53 / 6 | 1009, 8.1 | 5388/6322 (180) | 9 |
| 14:39 | run 57 | 27.7 | 34/75/249 | 182 / 12 | 6.6k (11.9k) | 34.4 | 21.9 | 23.2/12.2 | 5.9 | 1.5 (27) | 1781 (17) | 0 / 0 | 1177, 8.7 | 5394/6320 (187) | 9 |
| 14:40 | run 43 pause-unf 11 pause 4 cut 2 | 25.9 | 37/99/266 | 244 / 16 | 6.5k (9.6k) | 35.5 | 22.0 | 24.0/13.4 | 5.4 | 2.7 (26) | 1605 (14) | 0 / 0 | 1206, 8.9 | 5394/6225 (184) | 9 |
| 14:41 | pause-unf 59 | 22.4 | 49/67/100 | 457 / 1 | 7.1k (7.1k) | 37.4 | 25.8 | 26.0/17.8 | 4.5 | 7.2 (17) | 1345 (12) | 0 / 0 | 1831, 12.7 | 5394/6063 (184) | 9 |
| 14:42 | pause-unf 59 | 25.3 | 34/67/101 | 272 / 1 | 7.1k (7.1k) | 31.3 | 26.4 | 21.6/17.1 | 4.0 | 8.1 (17) | 1521 (16) | 0 / 0 | 1846, 12.7 | 5394/6236 (184) | 9 |
| 14:43 | pause-unf 60 | 29.9 | 33/36/52 | 8 / 0 | 7.1k (7.1k) | 24.1 | 26.5 | 16.9/16.4 | 3.4 | 9.4 (16) | 1793 (12) | 0 / 0 | 1894, 12.9 | 5394/6236 (184) | 9 |
| 14:44 | run 44 pause-unf 15 | 24.6 | 34/112/200 | 358 / 18 | 7.1k (10.9k) | 36.6 | 24.7 | 25.5/14.4 | 4.7 | 4.1 (15) | 1522 (10) | 0 / 0 | 1666, 11.9 | 5394/6354 (181) | 9 |
| 14:45 | pause-unf 33 run 19 pause 5 cut 2 | 19.9 | 49/150/514 | 523 / 30 | 6.3k (8.8k) | 40.4 | 22.9 | 32.3/10.0 | 4.2 | 6.2 (19) | 1228 (23) | 0 / 0 | 1024, 8.5 | 5394/6354 (187) | 9 |
| 14:46 | pause-unf 60 | 19.0 | 50/100/150 | 614 / 7 | 6.3k (6.4k) | 38.0 | 23.6 | 31.5/7.2 | 4.0 | 9.1 (18) | 1143 (14) | 0 / 0 | 937, 8.0 | 5394/6354 (181) | 9 |
| 14:47 | run 52 pause 3 pause-unf 2 | 35.5 | 26/67/375 | 45 / 9 | 6.8k (9.7k) | 25.8 | 22.0 | 15.9/11.8 | 6.7 | 2.3 (25) | 2215 (17) | 0 / 0 | 1067, 8.2 | 5394/6354 (180) | 9 |
| 14:48 | run 56 cut 2 | 35.3 | 26/55/287 | 29 / 4 | 6.4k (9.2k) | 26.6 | 21.6 | 17.0/10.9 | 6.5 | 1.6 (25) | 2212 (17) | 0 / 0 | 996, 7.7 | 5394/6648 (180) | 9 |
| 14:49 | run 60 | 29.7 | 30/71/149 | 104 / 7 | 7.2k (11.6k) | 32.7 | 23.2 | 21.7/11.4 | 5.8 | 1.0 (5) | 1852 (11) | 0 / 0 | 1382, 10.3 | 5394/6648 (181) | 9 |
| 14:50 | run 44 pause-unf 9 pause 6 | 33.9 | 29/50/117 | 22 / 1 | 6.4k (9.9k) | 24.8 | 21.6 | 16.7/12.3 | 4.9 | 4.6 (23) | 2093 (13) | 0 / 0 | 1080, 8.3 | 5394/6341 (186) | 9 |
| 14:51 | run 41 pause-unf 13 pause 5 | 34.1 | 27/83/250 | 53 / 13 | 6.0k (9.6k) | 25.1 | 21.2 | 15.8/12.9 | 5.4 | 4.1 (27) | 2100 (14) | 0 / 0 | 920, 7.5 | 5394/6324 (181) | 9 |
| 14:52 | run 60 | 25.8 | 39/87/342 | 181 / 12 | 5.0k (7.0k) | 37.7 | 21.3 | 27.0/10.8 | 5.7 | 1.0 (5) | 1617 (17) | 0 / 0 | 963, 7.7 | 5394/6324 (180) | 9 |
| 14:53 | run 60 | 20.8 | 46/105/166 | 434 / 18 | 5.7k (8.2k) | 47.2 | 22.0 | 31.8/14.3 | 4.7 | 1.0 (5) | 1330 (11) | 0 / 0 | 1190, 9.0 | 5394/6584 (181) | 9 |
| 14:54 | run 60 | 30.4 | 26/78/400 | 204 / 10 | 5.8k (8.7k) | 31.8 | 21.7 | 20.1/11.9 | 6.6 | 1.1 (5) | 1903 (15) | 0 / 0 | 921, 7.6 | 5394/6648 (181) | 9 |
| 14:55 | run 43 pause-unf 12 pause 4 | 33.0 | 31/59/180 | 26 / 6 | 4.8k (7.8k) | 25.8 | 21.4 | 16.7/13.2 | 6.1 | 4.5 (21) | 2079 (16) | 0 / 0 | 789, 6.2 | 5394/6354 (181) | 9 |
| 14:56 | run 60 | 36.3 | 26/54/125 | 30 / 2 | 6.3k (8.6k) | 26.5 | 21.8 | 16.3/10.9 | 6.4 | 1.0 (5) | 2215 (12) | 0 / 0 | 1035, 8.1 | 5394/6354 (185) | 9 |
| 14:57 | run 36 pause 24 | 34.7 | 27/54/399 | 36 / 6 | 6.1k (9.1k) | 23.6 | 20.8 | 15.4/13.0 | 5.3 | 5.1 (27) | 2129 (17) | 0 / 0 | 879, 7.4 | 5309/6495 (178) | 9 |


### 2b. Unit costs, GPU and thread CPU

| window end | CP CPU us/draw | CP kernel ms/f | VirtualProtect arm us/call | p_tex us/draw | upload memcpy GB/s | render own us/draw | read-back MB/f | GPU MHz avg (min) | GPU temp max C | threads user s/min: render 9410 / GPU Commands / Main / A60C / Replay |
|---|---|---|---|---|---|---|---|---|---|---|
| 13:54 | 2.07 | 1.6 | 0.53 | 0.58 | 4.50 | 1.69 | 3.8 | 1608 (345) | 68.1 | 57 / 14 / 9 / 3 / 3 |
| 13:55 | 2.98 | 1.9 | 0.64 | 0.80 | 3.98 | 2.88 | 5.4 | 2493 (1335) | 73.6 | 56 / 32 / 24 / 15 / 10 |
| 13:56 (split 0.8 s) | 2.67 | 0.4 | 0.66 | 0.57 | 3.74 | 3.29 | 5.6 | 2580 (2580) | 75.2 | 1 / 0 / 0 / 0 / 0 |
| 13:57 | 3.46 | 2.0 | 0.78 | 0.83 | 3.43 | 3.56 | 5.7 | 2281 (1762) | 75.0 | 56 / 28 / 17 / 8 / 10 |
| 13:58 | 3.78 | 2.4 | 0.79 | 0.94 | 3.21 | 3.81 | 5.9 | 2264 (1890) | 68.6 | 57 / 29 / 12 / 4 / 9 |
| 13:59 | 4.12 | 2.5 | 0.88 | 1.04 | 2.97 | 4.11 | 5.9 | 2332 (1815) | 68.0 | 56 / 31 / 12 / 5 / 9 |
| 14:00 | 4.08 | 2.5 | 0.89 | 1.03 | 2.97 | 4.06 | 5.9 | 2340 (2175) | 67.9 | 56 / 31 / 12 / 4 / 10 |
| 14:01 | 2.91 | 1.9 | 0.67 | 0.74 | 3.81 | 2.89 | 5.6 | 2300 (1417) | 71.9 | 56 / 29 / 19 / 13 / 9 |
| 14:02 (split 30 s) | 2.77 | 0.8 | 0.69 | 0.53 | 4.59 | 3.14 | 5.3 | 2440 (1627) | 75.0 | 28 / 11 / 9 / 2 / 3 |
| 14:03 | 2.86 | 1.7 | 0.67 | 0.85 | 4.07 | 2.79 | 5.4 | 2551 (1582) | 75.2 | 57 / 28 / 17 / 12 / 8 |
| 14:04 | 2.41 | 1.9 | 0.55 | 0.59 | 4.46 | 2.20 | 5.7 | 2268 (1462) | 75.9 | 57 / 26 / 11 / 9 / 7 |
| 14:05 | 2.31 | 1.8 | 0.54 | 0.58 | 4.33 | 2.13 | 5.6 | 2414 (1522) | 72.5 | 57 / 29 / 15 / 13 / 8 |
| 14:06 | 3.04 | 2.8 | 0.53 | 0.86 | 3.81 | 2.58 | 5.6 | 2359 (1987) | 72.8 | 56 / 32 / 14 / 8 / 10 |
| 14:07 | 2.80 | 2.0 | 0.51 | 0.71 | 4.11 | 2.50 | 5.5 | 2378 (1215) | 72.3 | 56 / 30 / 16 / 11 / 8 |
| 14:08 (split 50 s) | 3.16 | 1.6 | 0.51 | 0.77 | 4.33 | 2.95 | 5.5 | 2089 (1582) | 73.0 | 46 / 19 / 11 / 4 / 6 |
| 14:09 | 3.10 | 2.1 | 0.42 | 0.72 | 4.33 | 2.95 | 5.8 | 1927 (1785) | 67.2 | 57 / 19 / 7 / 3 / 6 |
| 14:10 | 3.01 | 1.7 | 0.46 | 0.71 | 4.28 | 2.88 | 5.7 | 2026 (1702) | 71.7 | 57 / 22 / 10 / 6 / 7 |
| 14:11 | 2.99 | 1.8 | 0.64 | 0.83 | 4.17 | 2.82 | 5.3 | 2443 (1305) | 75.2 | 56 / 28 / 17 / 14 / 8 |
| 14:12 | 2.77 | 1.3 | 0.68 | 0.75 | 3.94 | 2.68 | 5.3 | 2532 (2040) | 75.8 | 57 / 31 / 19 / 17 / 8 |
| 14:13 | 3.06 | 1.5 | 0.71 | 0.86 | 4.25 | 3.00 | 5.3 | 2489 (1702) | 76.2 | 57 / 25 / 17 / 15 / 9 |
| 14:14 | 2.84 | 1.6 | 0.68 | 0.76 | 4.03 | 2.78 | 5.3 | 2557 (2332) | 76.0 | 57 / 30 / 19 / 18 / 8 |
| 14:15 | 2.90 | 1.7 | 0.70 | 0.72 | 4.02 | 2.82 | 5.3 | 2554 (2257) | 76.1 | 57 / 29 / 18 / 18 / 8 |
| 14:16 | 2.90 | 1.7 | 0.64 | 0.75 | 4.22 | 2.82 | 5.4 | 2425 (1485) | 73.8 | 56 / 26 / 16 / 14 / 7 |
| 14:17 | 2.80 | 1.6 | 0.72 | 0.72 | 4.29 | 2.93 | 5.3 | 2572 (2310) | 72.8 | 57 / 28 / 18 / 16 / 8 |
| 14:18 | 2.90 | 1.8 | 0.68 | 0.72 | 4.39 | 3.15 | 7.2 | 2165 (1305) | 71.9 | 57 / 21 / 13 / 6 / 6 |
| 14:19 | 2.97 | 2.0 | 0.69 | 0.50 | 4.70 | 3.53 | 9.4 | 2094 (2017) | 69.3 | 57 / 20 / 11 / 3 / 7 |
| 14:20 | 2.82 | 1.7 | 0.64 | 0.47 | 4.97 | 3.35 | 9.4 | 2131 (1770) | 70.4 | 57 / 19 / 11 / 3 / 6 |
| 14:21 | 2.86 | 1.7 | 0.66 | 0.49 | 4.81 | 3.42 | 9.7 | 2083 (1552) | 70.1 | 58 / 18 / 10 / 3 / 6 |
| 14:22 | 2.79 | 1.8 | 0.61 | 0.55 | 4.62 | 3.27 | 8.0 | 2002 (1687) | 71.2 | 57 / 18 / 10 / 3 / 6 |
| 14:23 | 2.28 | 1.6 | 0.53 | 0.59 | 4.39 | 2.17 | 5.6 | 2320 (1230) | 71.5 | 56 / 25 / 12 / 8 / 7 |
| 14:24 | 3.06 | 1.6 | 0.71 | 0.76 | 4.40 | 2.99 | 5.3 | 2544 (2040) | 74.7 | 56 / 33 / 29 / 19 / 10 |
| 14:25 | 2.00 | 1.5 | 0.60 | 0.46 | 4.62 | 1.95 | 5.8 | 2347 (1582) | 73.5 | 56 / 24 / 10 / 12 / 5 |
| 14:26 | 2.61 | 1.7 | 0.66 | 0.72 | 4.27 | 2.54 | 5.4 | 2508 (1920) | 75.4 | 57 / 27 / 15 / 16 / 8 |
| 14:27 | 2.74 | 1.7 | 0.67 | 0.73 | 4.46 | 2.71 | 5.4 | 2461 (1635) | 75.9 | 57 / 26 / 15 / 14 / 8 |
| 14:28 | 2.74 | 1.6 | 0.68 | 0.73 | 4.43 | 2.75 | 5.4 | 2508 (1837) | 76.5 | 57 / 28 / 18 / 13 / 8 |
| 14:29 | 2.84 | 1.3 | 0.68 | 0.74 | 4.28 | 2.82 | 5.3 | 2573 (2092) | 75.7 | 56 / 29 / 18 / 16 / 8 |
| 14:30 | 2.81 | 1.8 | 0.66 | 0.79 | 4.31 | 2.59 | 5.4 | 2574 (2242) | 73.2 | 56 / 30 / 17 / 16 / 9 |
| 14:31 | 2.33 | 1.4 | 0.57 | 0.50 | 4.56 | 2.20 | 5.6 | 2292 (1305) | 73.3 | 57 / 25 / 13 / 9 / 7 |
| 14:32 | 2.79 | 1.4 | 0.61 | 0.50 | 5.00 | 3.18 | 8.4 | 2092 (1297) | 72.5 | 57 / 18 / 11 / 4 / 6 |
| 14:33 | 2.55 | 1.2 | 0.62 | 0.49 | 5.28 | 3.02 | 8.5 | 2142 (1312) | 71.5 | 57 / 19 / 11 / 4 / 6 |
| 14:34 | 1.72 | 1.5 | 0.45 | 0.34 | 4.93 | 1.56 | 5.9 | 2304 (1425) | 70.7 | 57 / 25 / 6 / 8 / 6 |
| 14:35 | 2.54 | 1.7 | 0.61 | 0.68 | 4.22 | 2.32 | 5.5 | 2406 (1342) | 72.9 | 56 / 30 / 18 / 15 / 8 |
| 14:36 | 2.89 | 2.5 | 0.63 | 0.84 | 4.10 | 2.50 | 5.4 | 2512 (1642) | 74.1 | 56 / 36 / 23 / 18 / 11 |
| 14:37 | 2.82 | 1.8 | 0.56 | 0.69 | 4.28 | 2.75 | 5.7 | 2230 (990) | 74.7 | 56 / 23 / 11 / 6 / 7 |
| 14:38 | 2.94 | 2.5 | 0.64 | 0.81 | 3.88 | 2.69 | 5.8 | 2441 (1980) | 74.2 | 56 / 28 / 14 / 12 / 9 |
| 14:39 | 4.22 | 4.4 | 1.14 | 1.09 | 2.60 | 3.86 | 8.9 | 2320 (1035) | 73.0 | 55 / 35 / 19 / 23 / 12 |
| 14:40 | 4.69 | 4.6 | 1.18 | 1.19 | 2.46 | 4.22 | 9.0 | 2269 (825) | 72.2 | 55 / 34 / 17 / 19 / 11 |
| 14:41 | 4.28 | 5.1 | 0.94 | 1.01 | 2.66 | 3.70 | 9.4 | 2387 (1867) | 71.2 | 55 / 34 / 9 / 5 / 10 |
| 14:42 | 3.65 | 4.2 | 0.71 | 0.84 | 3.10 | 3.08 | 9.4 | 2466 (2115) | 71.7 | 55 / 33 / 9 / 4 / 10 |
| 14:43 | 2.77 | 3.1 | 0.45 | 0.63 | 3.92 | 2.41 | 9.4 | 2522 (2430) | 73.5 | 55 / 29 / 8 / 3 / 10 |
| 14:44 | 3.80 | 5.2 | 1.02 | 0.99 | 2.74 | 3.55 | 9.0 | 532 (532) | 62.4 | 55 / 33 / 15 / 21 / 11 |
| 14:45 | 5.13 | 5.8 | 1.74 | 1.20 | 2.22 | 5.45 | 9.3 | 1940 (270) | 70.5 | 47 / 29 / 13 / 11 / 10 |
| 14:46 | 4.58 | 4.2 | 1.52 | 0.90 | 2.44 | 5.01 | 9.5 | 2046 (1372) | 71.7 | 42 / 28 / 9 / 4 / 9 |
| 14:47 | 2.66 | 2.3 | 0.64 | 0.66 | 3.74 | 2.52 | 8.9 | 2446 (1087) | 74.3 | 55 / 31 / 16 / 16 / 9 |
| 14:48 | 2.98 | 2.8 | 0.75 | 0.70 | 3.42 | 2.74 | 8.9 | 2443 (967) | 74.7 | 55 / 33 / 18 / 17 / 9 |
| 14:49 | 3.30 | 4.1 | 0.87 | 0.85 | 3.02 | 3.01 | 8.8 | 2423 (1365) | 74.7 | 55 / 35 / 18 / 22 / 10 |
| 14:50 | 2.83 | 2.9 | 0.71 | 0.65 | 3.54 | 2.62 | 9.0 | 2334 (1807) | 75.2 | 56 / 31 / 16 / 15 / 8 |
| 14:51 | 2.85 | 3.0 | 0.77 | 0.72 | 3.53 | 2.58 | 9.0 | 2389 (1200) | 75.3 | 54 / 29 / 15 / 13 / 9 |
| 14:52 | 5.31 | 5.4 | 1.80 | 1.47 | 2.12 | 5.26 | 8.8 | 2124 (1455) | 74.8 | 55 / 34 / 20 / 27 / 12 |
| 14:53 | 5.93 | 6.5 | 1.98 | 1.75 | 1.85 | 5.56 | 8.8 | 2025 (1087) | 65.6 | 54 / 34 / 19 / 29 / 11 |
| 14:54 | 3.61 | 3.5 | 1.16 | 0.97 | 2.81 | 3.42 | 8.9 | 2340 (1612) | 67.5 | 55 / 32 / 18 / 23 / 11 |
| 14:55 | 3.56 | 2.8 | 0.99 | 0.83 | 2.98 | 3.39 | 9.0 | 2326 (1710) | 67.9 | 56 / 29 / 14 / 17 / 11 |
| 14:56 | 2.94 | 2.4 | 0.76 | 0.75 | 3.48 | 2.64 | 8.8 | 2459 (2152) | 67.9 | 56 / 34 / 20 / 20 / 11 |
| 14:57 | 2.82 | 2.3 | 0.71 | 0.66 | 3.77 | 2.60 | 9.0 | 2358 (1357) | 68.5 | 56 / 30 / 15 / 10 / 9 |


## 3. Attribution of the slow minutes and the >100 ms frames

### 3.1 What moves with the bursts

Running minutes compared (per frame unless noted). "Good early" = 14:12-14:17, 14:24, 14:28-14:30 (free roam and
races, 4-5k draws); "good late" = 14:47, 14:48, 14:56 (late race content, 6.3-6.8k draws); "bad" = the running
episode minutes 14:39, 14:40, 14:52, 14:53; "l69 freeway" = 18:10/18:15 of session 37a5ea6b.

| counter | good early | good late | bad (episodes) | l69 freeway | moves with the bursts? |
|---|---|---|---|---|---|
| fps | 41-49 | 35.3-36.3 | 20.8-27.7 | 31.9-33.4 | - |
| frames >50 / >100 ms per min | 9-28 / 0-6 | 29-45 / 2-9 | 181-434 / 12-18 | 69-75 / 5-8 | - |
| draws per frame p50 | 4.1-5.1k | 6.3-6.8k | 5.0-6.6k | 7.1-7.3k | **no** (bad minutes draw no more; 14:52-53 fewer) |
| CP busy ms/frame | 19-24 | 25.8-26.6 | 34.4-47.2 | 29.0-29.5 | yes (consequence) |
| CP CPU us/draw | 2.74-3.06 | 2.66-2.98 | **4.22-5.93** | 2.94-3.10 | **yes, x1.5-2.2** |
| CP kernel ms/frame | 1.3-1.8 | 2.3-2.8 | **4.4-6.5** | 2.8-3.0 | **yes, x2-2.5** |
| VirtualProtect arm us/call | 0.64-0.72 | 0.64-0.76 | **1.14-1.98** | 0.57-0.58 | **yes, x1.6-2.8** (content-independent syscall) |
| upload memcpy GB/s | 3.9-4.4 | 3.4-3.7 | **1.85-2.60** | 3.8-3.9 | **yes, halved** (content-independent copy) |
| texture phase us/draw | 0.72-0.86 | 0.66-0.75 | 1.09-1.75 | 0.90-1.06 | yes |
| guest render thread own work us/draw | 2.6-3.0 | 2.5-2.7 | **3.9-5.6** | n/a | **yes** (recompiled game code) |
| uploads per frame (calls, MB) | 645-893, 6.1-8.3 | 996-1067, 7.7-8.2 | 963-1206, 7.7-9.0 | n/a | no (late-content level) |
| read-back MB/frame | 5.3-5.4 | 8.8-8.9 | 8.8-9.0 | n/a | no (late-content level) |
| fence + read-back wait ms/frame | 5.8-8.8 | 6.3-6.7 | 4.7-5.9 | 4.5-7.5 | no (falls) |
| WRM wait ms/frame (max ms) | 1.0-3.4 (5-29) | 1.0-2.3 (5-26) | 1.0-2.7 (5-27) | n/a (5) | no |
| targeted read-back max ms | 13-18 | 12-17 | 11-17 | 10.6-11.7 | no |
| pipeline waits per minute | 0 | 0 | 0 | 0 | no (only 14:38: 53 waits, 6 ms total) |
| GPU busy ms/frame | 17.4-21.1 | 21.6-22.0 | 21.3-22.0 | 22.1-24.1 | no; GPU 45-55% busy in the episodes |
| VRAM local / non-local MB | 5385-5391 / 133-144 | 5394 / 180-185 | 5394 / 180-187 | 5250-5400 | no |
| audit stream reads | pkgs 1-4: 99-1,122, 0 stalls | - | pkg 5: 2,437 reads / 81 MB in 16 s, max 1.08 ms, 0 stalls | 0 stalls | no: pkg 5 frames with a read median 49.1 ms, without 50.5 ms |

The CP-busy residual (busy minus CPU minus fence/WRM waits) is about 0 in good minutes and 3.7-7.5 ms per frame in
14:52-14:53: the CP thread was also off-CPU (not scheduled) more, which hints at core contention or reduced
scheduling in addition to slower execution.

### 3.2 The F10 windows

- Packages 1-3 (13:55:22, 13:56:03, 14:01:34; cut-scene audio presses): 56-59 fps, p99 24-33 ms, one frame
  >100 ms (130 ms in package 1, CP work 18 ms). Perf-split windows: 2.76-2.86 us/draw, memcpy 4.0-4.3 GB/s. Healthy.
- Package 4 (14:07:14, police, cut transition then pause): 39 fps, p50 23 ms, max 60 ms; 3.16 us/draw, memcpy
  4.2 GB/s. Healthy.
- **Package 5 (14:44:05, lag press; frame: race, lap 2/4, 01:11.96, 93 mph in 3rd gear, city street at sunset):**
  300 frames in 16.3 s = 18.3 fps, p50 50 ms, p90 66, p99 385, max 514; 153 frames >50 ms, 13 >100, 4 >200. The
  f10 perf-split window (only 0.67 s, because the minute line had just been written) shows the slowdown at the
  press: 6,996 draws/frame, CP busy 59 ms, 6.70 us/draw, kernel 7.1 ms/frame, memcpy 2.04 GB/s, p_tex 2.72 us/draw,
  GPU 24.5 ms. The 13 frames over 100 ms:
  - 5 at 14:44:06-14:44:11 (103-141 ms, CP work 58-79 ms): slowdown tail.
  - 1 at 14:44:12.6 (409 ms): the F10 minidump suspension (removed in l71, KI-070).
  - 7 at 14:44:18.9-14:44:21.5 (119-514 ms) around the race restart (pause 14:44:14, cut 14:44:17.8-19.7): four
    with CP work 49-165 ms (restart loading), three with 18-19 ms of CP work in 119/244/514 ms intervals (the guest
    did not submit: a game-side restart hitch).
  - None correlates with stream reads, pipeline creation, WRM or read-back waits (audit shader/pso counts 0).

### 3.3 The >100 ms frames overall

- **Episode minutes (running): 12-18 per minute**, against 0-6 in healthy early minutes and 2-9 in healthy late
  minutes. They are the tail of a slowed distribution: heavy frames whose own CP work is 2-4x a healthy frame,
  not isolated hitches of another kind.
- **Transitions:** pause/restart/cut boundaries give 100-500 ms frames; the minute maxima of 326-375 ms at 14:18,
  14:21, 14:23, 14:32 and 14:47 all fall in transition windows.
- **F10 minidump** (l70 only): 409 ms.
- **Not seen:** shader/pipeline compile hitches, streaming stalls, WRM/read-back waits over 30 ms in running
  minutes, VRAM paging.

### 3.4 Is p50 33 ms a cap?

Only in the pause/menu windows. With the UI cap forced the guest vblank is 60 Hz and frames land on 2 vblanks
(33.3 ms): p50 33, p99 35-36, max 36-52 in every clean paused minute (13:58-14:00, 14:09, 14:19-14:21, 14:43).
When the host slows past 33 ms of work, those frames snap to 3 vblanks (50 ms): 14:41 p50 49, 14:45 49, 14:46 50.
In running minutes the guest vblank is 240 Hz (unlocked, no 30 fps lock) and p50 is load: 17-26 ms healthy,
34-46 ms in the episodes. Minutes that mix pause and driving show p50 32-34 by mixture (14:04, 14:06, 14:23,
14:25, 14:44). So of the minutes cited in the KI-072 row, 14:44 is a mixed window (44 s of race, slowdown from
14:43:49), and 14:45-14:46 are mostly the pause menu with the window unfocused, running at 15-17 fps on a
static frame.

## 4. Host paging and disk

- **VRAM:** local usage 5,385-5,394 MB from 14:03 to the end (peak 5,404), non-local 130-190 MB (max 222) against a
  15.1 GB non-local budget, with no growth inside the episodes (non-local 180-187 MB in both good late and bad
  minutes). The local budget dipped to 5,854-5,896 MB in the garage/unfocused window 14:17-14:21 and the
  near-budget monitor stepped the texture hard limit 5595 -> 3291 MB (9 steps, 14:19:24-14:20:44). It was never
  raised again, although the budget returned to 6.2-6.6 GB. No effect is visible (14:24-14:30 ran 39-49 fps at
  normal unit costs), but the rest of the session ran with a 3.3 GB texture hard limit and l70 has no eviction
  counter to prove the cache never thrashed.
- **Disk:** audit stream reads 99-2,519 per audit, max single read 0.12-1.85 ms, stalls 0. No multi-second freeze
  (largest interval 849 ms at boot, then 514 ms at a race restart). Log writes are not timed in l70; l71 adds
  `MCLA_LOG v=1 kind=minute` (write_max_ms, flush_max_ms, queue_max, dropped, slow), `MCLA_LOG slow write/flush`
  warnings over 100 ms, and the CP freeze watchdog.
- **Process:** working set 1.24 GB at package 5, 100 native threads, 2,862 handles; nothing unusual.

## 5. Conclusion

1. **Mechanism (high confidence): a host-wide CPU slowdown of x1.5-2.5 (the KI-067 signature) in 2-3 minute
   episodes, on top of a late-game workload that is CP-bound even when healthy.** Late race content needs about
   26 ms of CP time and 16 ms of guest render-thread work per frame (35-36 fps at 6.3-6.8k draws and 9 MB
   read-back). In an episode, CP CPU per draw goes to 4.2-5.9 us, VirtualProtect to 1.1-2.0 us and memcpy to
   1.9-2.6 GB/s; CP busy reaches 34-47 ms per frame and the frame rate falls to 20-26 fps with 12-18 frames over
   100 ms per minute. High speed matters because that is where the workload is highest and a 40-50 ms frame is felt
   most. The paused-frame control (x1.55 on an identical frame, on a host and a guest thread alike) rules out a
   content cause.
2. **Episodes in this session:** 14:38:39-14:41:29 (around a race start), 14:43:49-14:45:59 (race, F10 #5),
   14:51:29-14:53:29 (race), a short dip 14:48:39-59, and a mild one while paused and unfocused at 13:56-14:00. None
   in the first 45 minutes of driving. Onset and recovery each take under 10 s. That fits a CPU power/thermal limit
   cycling after heat soak, or a foreign process (scanner, indexer, update), better than any game path. The
   off-CPU residual of the CP thread slightly favours contention or reduced scheduling, but l70 cannot separate a
   lower clock from contention.
3. **Ruled out as the main cause:** streaming at speed (uploads per frame and stream reads unchanged; no stalls),
   shader/pipeline creation (20,103 pipelines pre-created; 0-53 waits per minute totalling 0-6 ms; audit pso/shader
   0), WRM and read-back waits (unchanged or lower), the GPU (45-55% busy; clocks drop because it is starved), and
   VRAM paging (flat usage and non-local).

### 5.1 What the l71 high-speed run must capture

- **Arm:** `pc-triad` (MCLA_CPU census, async logging with slow-write timer, CP freeze watchdog, F10 without the
  live minidump), 1080p Unlocked, MAIN profile.
- **Route:** 20-25 minutes of normal play first (the episodes started after 45 minutes here), then sustained
  100+ mph in the late-game area (freeway and the city race used here).
- **F10 presses (cheap in l71):** at the onset of a lag, again 30-60 s into it, and right after it clears. During
  one episode, open the pause menu for 30 s without alt-tabbing, then continue: the pause-menu frame rate is the
  content-free control.
- **Owner notes:** any alt-tab or other program, fan noise or heat at the time.
- **Counters per minute and per F10:** `MCLA_CPU` eff_mhz / perf_pct / ntpi_mhz / ntpi_limit_mhz (clock or limit),
  sys_pct vs proc_pct (foreign load: sys up while proc is flat), temp_c if WMI works; then the unit costs used here
  (CP CPU us/draw, CP kernel ms/f, VirtualProtect arm us/call, upload memcpy GB/s, render own us/draw),
  `MCLA_LOG` write_max_ms, VRAM non-local. Decision: eff_mhz or ntpi_limit_mhz down in the episode -> clock,
  thermal or power limit; sys_pct up with proc_pct flat -> a foreign process; neither -> ask the owner for an
  external CPU log (for example HWiNFO) on the next run.

### 5.2 Candidate fixes, ranked

1. **Cut the CP thread's per-frame CPU so a x2 slowdown stays near 30 fps** (largest, cause-independent gain; the
   healthy late workload is already 26 ms of CP busy per frame):
   - memory-watch cost: 1.5-3.4M VirtualProtect arm calls per minute (about 1,000 per frame; 1.3-1.8 s per minute
     healthy, 2.2-3.0 s in the episodes) plus 1.6-2.5 ms/frame of write-fault handling in the episodes; coarser watch granularity or batching arms per submission;
   - texture and bind phases per draw (0.7 and 0.35 us healthy, the largest per-draw phases);
   - read-back copies (9 MB per frame in the late content): the M10-002 read-back steps 2-5 (M11-007);
   - CP work split (M10-006 revisit): move texture and upload preparation off the CP thread.
2. **Confirm and mitigate the host cause (KI-067):** if clock/thermal/power, document the owner-side mitigation
   (cooling, power profile, a frame cap) and consider a 30 fps or lower-CPU preset; if a foreign process,
   identify it (for example scanner exclusions for the game and save folders).
3. **Texture budget steering (M11-006):** raise the texture hard limit again when the budget recovers (it stayed at
   3291 MB after a transient garage/unfocused dip) and add eviction/creation counters to the minute line, so a
   late-session cache thrash can be excluded directly.
4. **Upload-ahead / streaming budget:** not indicated (no stalls; upload volume unchanged in the episodes).
5. **Pipeline pre-warm coverage:** not indicated for this symptom (no waits).
