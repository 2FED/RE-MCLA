# l111 MAIN session on the RTX 5060: play is GPU-bound at 64-79 fps, dips are mild (2026-11-19)

Read-only analysis by an Opus subagent (logs and F10 packages). Spark not used: deterministic parsing of one session.

**Headline.** Owner session on l111 (manifest 48E59690, pure defaults, no dev-override file; MAIN profile 5b71; RTX 5060, 3840x2160 fullscreen on the 120 Hz external display, render target 1920x1080 = guest 960x540 x2, forced HiDef, Unlocked; session 4fc85a45, 09:22:44-10:24:02, 59 min of log, 6 F10 packages). The 44.5 fps all-minute average is not play: 65 % of the logged time (about 38 min) is the game's own 30 fps pause/menu limiter, including two long focus-lost pauses (09:27:38-09:37:06 and 09:58:35-10:21:39). **Play-only fps is 71.4** over 20.7 min (minutes 65.5-79.2). Play is GPU-bound (GPU busy 12.1-13.4 ms per frame against CP busy 8.2-10.7 ms; Reflex sleeps 2.2-4.4 ms per frame to fill the CP slack). There is no sustained dip: the deepest play second is 51.4 fps, every play dip lasts one second, and no driving frame exceeds 50 ms (the three play-time hitches over 50 ms are the first frames after an unpause, guest simulation 42-105 ms). Against l100 (sustained 31.7-46 fps for up to 50 s, CP-bound at 6-8.7 k draws) the dip problem is gone at this load. Races with three opponents cost about 5-9 fps against free roam through +300-600 scene draws; nothing else is race-specific.

Evidence: private/owner-l111-20261119/{session,diagnostics} (save-private, before-user, after-user, save-files-private.json and *.dmp excluded). Logs mcla.5 -> mcla.4 -> ... -> mcla cover the whole process. Scripts (agent notes folder): budget.py (per-window budget from MCLA_PERF_SPLIT joined with pause/vblank state, MCLA_LOWLAT, MCLA_TEXHEAP, MCLA_CPU), dips.py (MCLA_DIP seconds and MCLA_HITCH classified against the menu-state timeline), fit.py (laws over clean play windows).

## 1. Session structure

| Span | State | Notes |
|---|---|---|
| 09:22:44-09:23:57 | boot, title, menus | guest vblank 60, 30 fps cap; start warp 17.8 -> 19.4 h (night) |
| 09:23:58-09:27:22 | free roam | 61-68 fps per minute; F10-1 at 09:24:47 |
| 09:27:22-09:37:24 | paused (focus lost 09:27:38-09:37:06) | 30.0 fps, 3750 draws, wrm 22.7 ms/frame |
| 09:37:24-09:41:42 | free roam | 72.4-73.5 fps at 2.1-2.3 k draws |
| 09:41:50-09:58:31 | event loop: races and menus | races confirmed by the on-screen timer (place 1/4, three opponents): starts 09:42:00 (F10-2), 09:44:09 (F10-3), 09:49:45 (F10-4/5/6); the starting pattern is "game paused (sim 1)" -> running -> "racer-off 1" (1.5-2 s) -> running, about 10-12 s of intro before the timer starts; also at 09:47:05, 09:49:03, 09:49:31, 09:53:31, 09:54:38 (restarts or further events, not confirmed); results/menu pauses of 2-21 s at 09:43:54, 09:45:57, 09:47:03, 09:47:19-09:49:01 (focus lost 09:47:23-09:48:57), 09:51:33, 09:53:29, 09:54:35, 09:56:15, 09:56:46 |
| 09:58:31-10:22:41 | paused (focus lost 09:58:35-10:21:39) | 30.0 fps, 5675 draws |
| 10:22:41-10:23:53 | free roam | 71.5 fps; exit 10:24:02 |

No pursuit (pursuit=na in all host-hitch lines; pursuit phase none in all six packages). Pause/menu state is the game's own limiter: "MCLA menu state: game paused ... UI cap forced", guest vblank 60 Hz, wrm_mem 13-25 ms per frame on the guest vblank flag, ring idle 0.01-0.02. In pause the game switches the scene back to two tiles at 4x MSAA (MCLA_GTILE tiles=2 msaa=4 forced4x=1 sd=1 tile_span=320 at every pause; tiles=1 msaa=2 tile_span=540 at every resume), which is why paused frames carry 3750-5675 draws: it is capped at 30 fps by the game and costs nothing visible.

Play distribution (12 clean minutes plus 3 clean F10 windows, 45,711 frames, no pause or 60 Hz vblank inside): fps 63.7 / 69.0 / 73.5 (min / p50 / max), draws per frame 2132 / 2505 / 2891, CP busy 8.20 / 9.34 / 10.71 ms, GPU busy 12.13 / 12.67 / 13.40 ms, ring idle 3.4 / 4.2 / 4.8 ms, Reflex sleep 2.2 / 3.5 / 4.4 ms, wrm 1.0 ms (240 Hz guest vblank). Pause-corrected fps of all 27 minutes with more than 5 s of play: 65.5-79.2, 71.4 overall.

Laws over the 15 clean play windows (draws 2.1-2.9 k only, so the frame law is weak):
- frame = 8.36 ms + 2.43 us x draws (r 0.86)
- CP busy = 1.58 ms + 3.08 us x draws (r 0.96); CP work (busy - fence - wrm) = 0.30 + 3.10 us (r 0.96); draw phase 1.77 us/draw, uploads 0.72 us/draw
- GPU busy = 8.34 ms + 1.72 us x draws (r 0.93)
- CP equals GPU at about 4.97 k draws; play never comes near it. The 1-s dip seconds (3.5-3.9 k draws, CP 11.8-14.3, GPU 13.7-15.0) sit on both laws.
- Comparison with the old laws: the CP cost per draw is unchanged (3.1 us against 2.85-2.96 us at l100). The GPU floor fell from 11.6-14.7 ms to 8.3 ms (transfer elision plus extent bound, l109/l110). The GPU per-draw slope rose from 1.14-1.21 to 1.72 us, because the draws per frame are not the same unit: one tile no longer submits the scene twice (l100 normal 3.9-4.9 k draws, now 2.1-2.9 k for the same city), so each draw now covers 540 rows instead of 320.

## 2. Dips and hitches

MCLA_DIP: 20 summaries. 15 are the pause limiter (seq 1, 2, 5-7, 9-12, 14-18, 20: 27-30 fps, inside a "game paused" span or with wrm 17-31 ms per frame; seq 5, 11 and 20 are the 76-77 s focus-lost pauses). The 5 play dips, each one second:

| time | fps (median) | draws (base) | CP busy / work | GPU | guest sim | sleep | fence | wrm | upload | draw | tex creates | pso | hint |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 09:26:04 free roam | 53.8 (68.5) | 3909 (2545) | 13.31 / 13.25 | 14.95 | 4.24 | 0.01 | 0.06 | 1.06 | 2.57 | 6.23 | 53 | 0 | cp_draws |
| 09:26:38 free roam | 54.2 (68.1) | 3474 (2837) | 11.83 / 11.70 | 13.82 | 5.76 | 0.01 | 0.13 | 0.87 | 2.03 | 5.48 | 191 | 0 | guest |
| 09:45:25 race 2 | 51.4 (64.9) | 3575 (2924) | 14.32 / 14.28 | 13.67 | 4.23 | 0.04 | 0.04 | 0.91 | 2.45 | 6.66 | 51 | 0 | cp_other |
| 09:50:29 race 3 | 53.0 (66.4) | 3571 (2658) | 12.84 / 12.69 | 14.76 | 5.03 | 0.01 | 0.14 | 1.01 | 2.54 | 6.40 | 303 | 0 | guest |
| 09:57:36 | 57.5 (74.7) | 3582 (2470) | 12.76 / 12.68 | 14.57 | 3.39 | 2.54 | 0.08 | 1.23 | 2.25 | 5.99 | 69 | 0 | cp_draws |

MCLA_HITCH (CP-side, 86 lines): 77 inside pause spans (2-3 vblank frames of 35-51 ms). Outside pause: 09:22:46 (boot, 255.8 ms, wrm), 09:23:09 (loading, 284.7 ms, 547 texture creates, 117.8 ms draw phase), 09:45:24 (37.4 ms, guest sim 9.7 ms, 3851 draws: the frame inside the 09:45:25 dip second), 09:45:56 (47.5 ms, draw phase 37.1 ms at 2234 draws, one frame), 09:49:33 (108.6 and 54.7 ms) and 09:53:33 (50.3 and 66.1 ms): guest sim 42-105 ms at 565-659 draws, the first frames after "game running" at a race start. MCLA_HITCH_HOST adds 97-150 ms host frames at 09:25:11, 09:43:59, 09:47:06, 09:49:06, 09:49:32, 09:53:33 and 09:54:40, each within 0.3 s of a resume from a race-start pause (the same transition). No frame over 50 ms while driving.

Classes:
- (a) Draw count: all five play dips are 3.5-3.9 k draws (+25-55 % on the base), with CP and GPU both rising on their laws. This is the l100 mechanism at a third of the depth: 51-58 fps for one second, not 31-46 fps for tens of seconds.
- (b) GPU-bound: GPU busy is larger than CP busy in 3 of 5 dips (13.8-15.0 ms) and in every play minute; GPU is the frame floor.
- (c) Guest-bound: guest sim 3.4-5.8 ms per frame in the dip seconds (no figure for normal seconds in this log); the >50 ms play-time hitches are all guest-sim frames at resume. Ring idle stays 3.4-4.8 ms in play.
- (d) Texture/PSO creation: pso_new 0 everywhere, no shader or PSO creation in any audit (shader_count 0, pso_count 0); texture creates 51-303 per dip second at 9 us average (MCLA_TEXHEAP create_us_avg); no evictions. Not a cause.
- (e) Uploads: 2.0-2.6 ms per frame in the dips against 1.1-1.85 ms in play minutes; this follows the draws (0.72 us/draw), it is not a separate spike.
- (f) Reflex: init backend=reflex result=ok; 0 guard trips and 0 would-trip in all 67 windows; 4 stale_sleep fallbacks of one frame each (09:51:40, 10:19:43, 10:22:36, 10:22:38: all at a resume); lead_hist[2] small (for example 19 of 4138 frames).
- (g) External: system CPU max 34-55 % in play minutes with the game at 26-37 %, effective clock 4.26-4.33 GHz average; no external load episode.

Dominant class the owner can feel: none during driving beyond one-second draw-count dips to 51-58 fps. The noticeable stalls are the resume frames at race starts (100-150 ms, guest simulation while the race loads) and the 30 fps menu/results limiter. Depth compared with l100: the worst play second went from 31.7 fps (CP-bound) or 25-28 fps (external) to 51.4 fps.

## 3. Races with three opponents

| | free roam 09:38:46-09:40:46 | race 3, F10-4/5/6 windows and the 09:50:46 minute |
|---|---|---|
| fps | 72.4-73.5 | 63.7-69.0 |
| draws per frame | 2132-2314 | 2574-2891 |
| main scene key p960m2c1008 (draws per frame) | 1535-1681 | 1895-2142 |
| CP busy / CP work | 8.2-8.8 / 7.0-7.6 | 9.4-10.7 / 8.2-9.6 |
| CP busy per draw | 3.8-3.9 us | 3.6-3.8 us |
| GPU busy | 12.13-12.16 | 12.67-13.40 |

The extra race cost is scene draws on the main 2x MSAA target (+300-500 per frame: the opponents and their effects); the per-draw costs do not change, and the frame stays GPU-bound. Start grid: no MCLA_DIP second at any of the three confirmed starts (09:42:00, 09:44:09, 09:49:45); the only cost at a start is the resume frame before the intro. MCLA_GPU_SURF: about 27-28 surface keys per frame in both free roam and race, 67-85 rectangle draws per frame; no key appears only in races, and the small targets (p240m4c168 about 30 draws, p320m2c128 110-140, p520m4c416 150-200, p520m4c832 40-140) are steady across states. The minimap target cannot be named from these keys alone.

## 4. The six F10 windows (MCLA_PERF_SPLIT reason=f10, ms per frame)

| # | press | state (from the frame) | window | fps | draws | CP busy / work | GPU | ring idle | draw | upload | wrm | audit fps per second after the press |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 09:24:47 | free roam, Santa Monica Blvd | 1.2 s | 61.8 | 3461 | 11.15 / 10.03 | 13.58 | 3.93 | 5.08 | 1.90 | 1.09 | 66 74 69 68 |
| 2 | 09:42:11 | race 1, 00:11.46, 1st of 4 | 25.1 s (14 % pause) | 67.6 | 2206 | 8.39 / 5.72 | 11.82 | 3.87 | 3.67 | 1.14 | 2.53 | 73 71 77 66 |
| 3 | 09:44:25 | race 2, 00:16.60 | 39.6 s (12 % pause) | 64.9 | 2301 | 8.68 / 6.28 | 12.17 | 4.53 | 3.74 | 1.27 | 2.20 | 73 65 67 71 |
| 4 | 09:50:10 | race 3, 00:24.56 | 23.6 s | 69.0 | 2574 | 9.41 / 8.24 | 12.67 | 4.08 | 4.25 | 1.52 | 1.02 | 63 61 62 62 |
| 5 | 09:50:28 | race 3, 00:42.99 | 18.3 s | 63.7 | 2755 | 10.25 / 8.97 | 13.31 | 4.42 | 4.65 | 1.74 | 1.03 | 53 64 68 64 |
| 6 | 09:50:39 | race 3, 00:53.39 | 10.4 s | 66.3 | 2891 | 10.45 / 9.22 | 13.40 | 3.58 | 4.77 | 1.80 | 1.05 | 66 65 58 63 |

What each press was for cannot be read from the logs. No press falls on a logged dip second; the 53 fps second right after press 5 (09:50:28.3-29.3) is the only dip next to a press, and the package capture itself (frame 25 ms, process census 57 ms, off the game thread; game_stall_upper_bound 0) may contribute. Presses 4-6 come in the lowest-fps, highest-draw race windows (61-64 fps); they are the most likely dip presses. Presses 1-3 do not follow a dip and are candidates for the minimap flicker (KI-084). The minimap is drawn intact in all six captured frames, so a flicker, if present, was not in the captured frame.

## 5. Verdict and next lever

Worst clean play window (09:50:09-28, 63.7 fps, 15.7 ms frame at 2755 draws): GPU busy 13.31 ms (85 % of the frame) is the limit; CP busy 10.25 ms (draw phase 4.65, uploads 1.74, watch arm and write faults 1.26, wrm 1.03, present 0.69, stream finish 0.34, read-back wait 0.27) leaves about 3 ms of slack that Reflex sleeps away. By the GPU law, 8.3 ms of the 13.3 ms is a fixed per-frame floor and about 4.7 ms is draws on the 2x main target (1.72 us per draw). Category timing was off in this session; at l110 (about 2,400 draws) that floor split into transfers 2.3, resolves 1.5, texture load, upload and read-back 1.8 ms, plus the fixed part of the scene. CP per draw (3.1 us) matters again only above about 5 k draws, which play never reached here (maximum 3.9 k in a second).

Single most valuable next step: one l111 `gpu-cat` run on the same race route (category timing on), to split the 8.3 ms GPU floor and the 1.72 us per-draw scene cost by category on the one-tile path. The candidate levers are the remaining transfers and resolves (about 3.8 ms together at l110) and the cost of the 2x MSAA scene target. A fixed cost removed from the GPU floor raises every play frame, while the CP path has slack at current draw counts. KI-084 (minimap flicker from the rectangle draw-extent bound) is a correctness fix and goes first; it is being handled separately.
