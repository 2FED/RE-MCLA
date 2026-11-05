# l68 PC A/B (KI-048 soft cap) and the KI-035 capture: read-only analysis (2026-11-05)

Read-only analysis by an Opus subagent on 2026-11-05 of the owner's two l68 PC sessions (soft cap on vs off, warp probe on; the cap-off session ended in the KI-035 freeze); accepted by the primary agent.

Owner-internal build l68 (`mcla_version 0.20.37.0`, build `BA0363C0...6B77EE`), PC, RTX 5060 Laptop GPU
(8 GB), profile MAIN, 1080p render target (scale 2), Unlocked, vsync 1, fullscreen on a 3840x2160 window.

| arm | session | process time | soft cap | warp probe | watchdog |
|---|---|---|---|---|---|
| probe | `98495fd8...` | 15:37:42-15:54:16 (16.6 min) | ON, 2048 MB (`soft 2048 / floor 2048 / hard 5595`) | on | **off** (`mcla_warp_watchdog=false`) |
| probe-capoff | `f785201a...` | 16:16:31-16:28:09 (11.6 min) | OFF (`soft 3565 / floor 3565 / hard 5595`) | on | off |

Both sessions are shorter than the planned 20 minutes; the cap-off arm ended in the KI-035 freeze at
minute 10.6. Sources: session `mcla.1.log` + `mcla.log` (complete, no rotation loss), per-package
`state.json`, `report.txt`, `frame.bmp` (downscaled once for viewing, preview deleted), launcher
`report.txt`, `launch-receipt.txt`, `store-seed.txt`, `state.txt`. Not opened: before/after-user,
save-private, minidumps, log-tail. Store seed: probe `seeded 760/7508`, capoff `existing 768/8178`.
Both `state.txt` `complete`, `exit 0`, `guest exit after window close`, no crash package.

## Part 1: KI-048 soft-cap A/B

### Per-minute table (perf split / pacing / read-back sync / census)

`gpu` = `gpu_busy_ms / frames`; `kd` = GPU ms per 1,000 draws (content-normalised GPU cost);
`sync` = `MCLA_READBACK_SYNC targeted_max_wait_ms`; `local/budget` from the `MCLA_VRAM` minute line
closest to the pacing line; clocks are `gpu_mhz` min/avg/max, `temp_c` avg/max. Power is `n/a`
(the driver leaves it 0), fan `n/a`.

**Cap ON (probe):**

| min | end | fps | p50 | p99 | max ms | >50 | >100 | gpu ms | kd | sync max | local / budget MB | gpu MHz avg (min-max) | temp avg/max | note |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 15:38:59 | 32.7 | 32.7 | 50.8 | 1416 | 25 | 8 | 12.4 | 4.06 | 14.7 | 3632 / 7131 | 993 (180-2640) | 54/68 | start-up load |
| 2 | 15:39:59 | 34.6 | 32.5 | 36.9 | 99 | 12 | 0 | 20.2 | 3.93 | 14.9 | 3977 / 7131 | 2113 | 68/72 | |
| 3 | 15:40:59 | 34.2 | 32.4 | 36.7 | 92 | 7 | 0 | 20.1 | 3.17 | 10.0 | 4323 / 7131 | 2321 | 69/72 | |
| 4 | 15:42:00 | 28.3 | 33.5 | 51.7 | 83 | 105 | 0 | 21.9 | 3.21 | 10.7 | 4323 / 7131 | 2287 | 65/67 | Alt-Tab 15:41:02-15:42:51 (paused) |
| 5 | 15:43:00 | 30.5 | 33.3 | 50.5 | 85 | 37 | 0 | 20.9 | 3.12 | 9.8 | 4323 / 7131 | 2224 | 65/66 | |
| 6 | 15:44:00 | 36.7 | 25.5 | 62.7 | 116 | 43 | 1 | 20.8 | 3.19 | 10.7 | 4868 / 7131 | 2425 | 68/70 | |
| 7 | 15:45:00 | 39.0 | 24.3 | 66.2 | 141 | 43 | 6 | 15.1 | 4.17 | 13.2 | 5282 / 6426 | 2556 | 71/74 | OS budget drop 7131 -> 6426 at 15:44; F10 #1 |
| 8 | 15:46:00 | 36.6 | 25.5 | 49.7 | 117 | 22 | 2 | 21.2 | 3.70 | 12.9 | 5301 / 6503 | 2458 | 73/76 | |
| 9 | 15:47:00 | 46.2 | 19.6 | 34.7 | 81 | 4 | 0 | 17.5 | 4.02 | 11.4 | 5301 / 6696 | 2316 | 71/73 | |
| 10 | 15:48:00 | 41.3 | 24.0 | 43.2 | 94 | 20 | 0 | 21.0 | 4.11 | 12.7 | 5301 / 6696 | 2597 | 73/74 | |
| 11 | 15:49:00 | 30.3 | 33.1 | 67.3 | 125 | 31 | 8 | 22.3 | 2.91 | 11.6 | 5302 / 6503 | 2352 | 71/73 | F10 #2 |
| 12 | 15:50:00 | 35.5 | 28.0 | 61.6 | 150 | 37 | 4 | 8.9 | 3.45 | 19.0 | 5306 / 6503 | 2604 | 70/71 | F10 #3 |
| 13 | 15:51:00 | 35.0 | 26.9 | 85.9 | 146 | 67 | 7 | 7.7 | 3.72 | 14.4 | 5306 / 6503 | 2523 | 70/71 | F10 #4/#5 |
| 14 | 15:52:00 | 45.5 | 20.6 | 35.9 | 120 | 13 | 2 | 17.6 | 4.24 | 53.4 | 5307 / 6696 | 2444 | 72/74 | |
| 15 | 15:53:00 | 37.6 | 25.5 | 48.0 | 116 | 19 | 2 | 19.2 | 3.29 | 10.0 | 5307 / 6696 | 2501 | 72/74 | |
| 16 | 15:54:00 | 42.4 | 21.4 | 50.4 | 310 | 31 | 4 | 18.5 | 3.60 | 17.1 | 5307 / 6696 | 2433 | 71/74 | garage warp 15:53:58 (worked) |

Census: `near_budget_steps=0`, `near_budget_s=0` on all 21 lines; tex limits `2048/5595/2048` unchanged;
`nonlocal_mb` 131-153 flat; peak 5,315 MB. `mem_mhz` 9,001 flat after the first minute.

**Cap OFF (probe-capoff):**

| min | end | fps | p50 | p99 | max ms | >50 | >100 | gpu ms | kd | sync max | local / budget MB | gpu MHz avg | temp avg/max | note |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 16:17:33 | 32.4 | 32.9 | 50.0 | 434 | 22 | 8 | 12.6 | 3.62 | 13.9 | 3890 / 7131 | 1458 | 54/67 | start-up load |
| 2 | 16:18:33 | 49.6 | 20.1 | 28.6 | 66 | 3 | 0 | 17.5 | 3.94 | 11.0 | 4071 / 7131 | 2592 | 72/74 | |
| 3 | 16:19:33 | 38.6 | 21.7 | 67.7 | 122 | 154 | 1 | 19.0 | 3.47 | 11.1 | 4317 / 7131 | 2518 | 71/74 | pause 16:19:16, Alt-Tab 16:19:20 |
| 4 | 16:20:33 | 24.1 | 34.6 | 67.5 | 85 | 327 | 0 | 24.7 | 2.90 | 10.4 | 4317 / 7131 | 2485 | 66/68 | unfocused until 16:20:41 (+3 flaps to 16:20:51) |
| 5 | 16:21:33 | 33.3 | 28.4 | 65.4 | 100 | 96 | 1 | 22.8 | 3.33 | 11.9 | 4749 / 5699 | 2541 | 69/72 | OS budget drop to 5551-5699 MB; F10 #1/#2 |
| 6 | 16:22:33 | 38.8 | 24.2 | 46.7 | 104 | 20 | 2 | 16.4 | 4.60 | 55.6 | 5087 / 5910 | 2357 | 72/75 | |
| 7 | 16:23:33 | 43.3 | 22.4 | 34.5 | 84 | 6 | 0 | 19.9 | 3.94 | 11.6 | 5313 / 6732 | 2543 | 73/75 | |
| 8 | 16:24:33 | 44.2 | 20.6 | 34.9 | 57 | 2 | 0 | 18.2 | 4.19 | 14.4 | 5415 / 6732 | 2375 | 73/75 | |
| 9 | 16:25:33 | 36.9 | 25.7 | 54.3 | 113 | 31 | 1 | 11.7 | 3.64 | 11.3 | 5961 / 6732 | 2562 | 72/73 | F10 #3 |
| 10 | 16:26:33 | 33.5 | 28.8 | 70.7 | 92 | 58 | 0 | 23.6 | 3.41 | 13.3 | 6695 / 6732 | 2454 | 70/72 | near budget from 16:25:58, 4 steps |
| 11 | 16:27:33 | 45.9 | 18.9 | 49.5 | 117 | 25 | 2 | 5.5 | (6.04) | 49.7 | 6797 / 6732 | 2540 | 73/74 | **over budget** from 16:26:38; 8 steps, hard at floor 3565 at 16:27:09; freeze from 16:27:12 |

Near-budget steps (256 MB, every 10 s): 16:25:58 5339, :08 5083, :18 4827, :28 4571, :38 4315
(usage 6,767 > 6,732), :48 4059, 16:26:59 3803, 16:27:09 3565 (floor). Over budget by 35-69 MB until
the end (`near_budget_s` 108 at 16:27:47). Peak 6,801 MB. Minute 11's `kd` is the static garage shot
(918 draws/frame), not comparable.

### Findings

1. **Budget:** with the cap the footprint rose to 5.3 GB by minute 7 and stayed **flat at 5,301-5,307 MB
   for the remaining 9.5 minutes**, 1.1-1.4 GB under a budget the OS moved between 6,426 and 6,696 MB;
   zero near-budget seconds. Without the cap the footprint kept growing (3.9 -> 6.8 GB), went near budget at
   minute 9.5 and over budget at minute 10, exactly the l67 pattern (over budget from minute 9). The cap
   arm's 5.3 GB plateau equals the cap-off level of minute 7 (5,313 MB) - i.e. the cap removed the
   ~1.5 GB of further texture-cache growth (6.8 - 5.3) that pushed l67/capoff over budget.
2. **Long hitches / late slowdown:** neither arm shows the l67 0.3-1 s sync-wait hitches: per-minute
   targeted sync max 10-19 ms (one 53 ms in the cap arm at 15:51-15:52; 56 ms and 50 ms in the cap-off
   arm, the latter in the over-budget minute). Minute max frames 57-150 ms in play (310 ms in the cap
   arm's garage-warp minute, load). GPU cost per 1,000 draws stays 2.9-4.2 ms in both arms with no
   trend. **But the comparison is not decisive for the late-session effect:** the cap-off arm was over
   budget for only ~1.5 minutes before the freeze, and l67's hitches and its 31-34 ms GPU frames built
   up over 20+ minutes over budget. What is shown: the cap keeps the card in budget for 16.6 minutes
   (and by the flat plateau, very likely longer); whether over-commit causes the late slowdown is still
   unproven by A/B.
3. **Thermal vs paging:** GPU clock avg 2,113-2,604 MHz every minute after the first (max 2,595-2,692 of
   a 3,090 MHz `gpu_max_mhz`), memory clock 9,001 MHz flat, temperature avg 65-73 °C, max 75.6 °C in
   both arms; no downward clock trend over the session, no plateau-and-sag. No sign of thermal
   throttling in 12-17 minutes. `nonlocal_mb` stayed 130-153 MB even over budget (no demotion visible
   yet in 70 s). Power is not reported by this driver (`n/a`).
4. **Cost of the cap:** no visible cost. Early in-budget minutes with comparable content: cap 3.2-4.1
   ms/kdraw vs cap-off 3.5-3.9; upload volume 8.5-25 GB/min (cap) vs 10-26 GB/min (cap-off), upload
   time 2.1-6.6 vs 2.7-7.2 s/min; stream reads in audits 1,430-2,642 (cap) vs 1,495-2,690 (cap-off);
   no `pso` or stream stalls. Texture evictions/re-creations are not counted by any line, so a cost
   when revisiting areas after >30 s cannot be excluded, but it does not show up as upload spikes or
   extra >50 ms frames (cap arm >50 ms counts are not higher than cap-off's outside the Alt-Tab minutes).
   fps differences between the arms track content (draws/frame), not the cap.
5. **Alt-Tab artefact:** both arms' Alt-Tab minutes (cap min 4: 105 frames >50 ms; cap-off min 3-5:
   154/327/96) are the unfocused/paused window, not the cap; exclude them from >50 ms comparisons.

### Hitch attribution at the owner's F10s (camera shakes / fps drops)

All 9 play audits PASS (300 samples, `stream_stalls=0`, `pso_count` 0-1 (87 µs), no audio
underruns). Per audit (host frame interval):

| arm | F10 | p50 | p99 | max | >50 ms | spike frames (interval / cpu / gpu ms) |
|---|---|---|---|---|---|---|
| cap | 15:44:17 | 29.1 | 91.1 | 141 | 13 | 141/33/32, 91/24/25, 79/29/29, 71/28/26 |
| cap | 15:48:05 | 32.7 | 85.5 | 100 | 8 | 100/33/32, 93/32/31, 86/33/38 |
| cap | 15:49:37 | 25.3 | 67.5 | 113 | 7 | 113/23/25, 80/36/35 (41 reads), 51/57/56 |
| cap | 15:50:39 | 29.3 | 92.3 | 124 | 20 | 124/32/29 (40 reads), 92/24/28, 71/123/125 |
| cap | 15:51:01 | 25.6 | 40.9 | 105 | 2 | 105/26/24, 75/24/28 |
| off | 16:21:35 | 29.6 | 99.9 | 104 | 14 | 100/32/30, 88/27/29, 84/33/32 |
| off | 16:25:02 | 25.9 | 50.7 | 54 | 4 | 50-54 ms only |
| off | 16:26:54 | 33.0 | 79.0 | 84 | 12 | 84/32/32 (x2), 79/23/25, 74/38/34 (41 reads) |
| off | 16:27:16, 16:27:47 | 16.1 | 22 | 58 | 1 | freeze windows (static garage shot) |

- The spikes are **not** the l67 class: in the same minutes the targeted read-back sync max was
  10-19 ms (not 70-140 ms), there were no pipeline creations and no stream stalls. Every F10 except cap-off #4 was in budget;
  cap-off #4 (16:26:54, 6,801 of 6,732 MB) has spikes no worse than the in-budget ones.
- The frame's own work (cpu/gpu 23-38 ms) is far below the interval, and the intervals sit on vsync
  multiples (50, 66.6, 83.7, 99.9, 116.6 ms = 3-7 x 16.67 ms), so the frame was produced late and snapped
  to the next vblank. Only two spikes were GPU-heavy frames (123/125 ms, 57/56 ms). Stream reads are
  present on 3 spikes (40-41 reads) but with no stall.
- With Unlocked timing each such 67-140 ms gap is one large dt step, which reads as a camera jerk.
  Cause: a frame-production gap outside the GPU/readback path (guest frame thread or host scheduling);
  the 10 s guest sampler cannot isolate single frames. Neither paging nor the cap is implicated.

### Verdict and recommendation

- The 2,048 MB soft cap keeps the 8 GB card in budget for the whole 16.6-minute session with a flat
  footprint and ample headroom, while the uncapped arm goes over budget at minute 10, as l67 did. No
  measurable cost. Thermal throttling is not present in either arm.
- Not yet shown: that the cap removes l67's late 0.3-1 s hitches and the 31-34 ms GPU frames, because
  the cap-off arm never reached the long over-budget phase (it froze at minute 10.6). The mechanism
  (stay under budget -> no over-commit) is now demonstrated; the consequence needs one longer run.
- **Recommendation:** ship the cap by default on LOCAL budgets under 10 GB at scale >= 2, value
  **2048 MB**, for the next volunteer build. 2048 keeps a >= 1.1 GB margin even at the lowest budget
  seen in the capped arm (6,426 MB); note the OS dropped the budget to 5,551 MB briefly in the cap-off
  arm (16:21), which a 5.3 GB plateau would sit just under (near-budget rule = usage + 512 > budget
  would fire and the monitor can then step hard down to the 2,048 floor, which is now effective).
  A lower value (1,536) is not justified by this data. Ask for one 30-40 minute cap-ON PC session
  (same route, 1080p Unlocked) to confirm the absence of late hitches before closing KI-048.

## Part 2: KI-035 capture (cap-off session end)

### Timeline (local time, UTC+2)

| time | event |
|---|---|
| 16:16:59.0 / 16:17:06.3 | first warps of the session (type 9, then type 14: 26 -> 29, camera `BF1E2EF0` UITransition_Lean, ready=1, time 0, duration 2.99, active 0/2): normal |
| 16:19:16.5 | pause (`sim 1`) |
| 16:19:19.7 - 16:20:40.9 | **Alt-Tab** (`MCLA_WINDOW_EVENT focus-lost` / `focus-gained`), then 3 short focus flaps to 16:20:51.3; game resumed 16:20:53.5. No `minimized/restored`, no controller disconnect/reconnect (only `first-connected` at 16:16:33), no device or audio events |
| 16:21:35, 16:21:44, 16:25:02 | F10 #1-#3 in free roam: warper 29, `pause_override 0`, `camera_paused 0`, pause manager 0, active list count 0, focused |
| 16:26:54.3 | F10 #4 during the freeway race (position 1/2 HUD) |
| 16:26:55.5 | race end (`game paused sim 1`); 16:27:02.1 running (free roam) |
| 16:27:04.8 | pause menu opened |
| 16:27:09.079 | `game running` (menu closed by "go to garage") |
| 16:27:09.083 | `MCLA_KI035_WARP_STATE from=15 to=26` request `BED351C0` type 17 req0=1 req3=1 req9=1 req7=0; previous camera Lean ready 0 time 0, list empty |
| 16:27:09.184 | `from=26 to=20` camera **`BF1F0BF0` UITransition_Parallel ready=1 time=0 duration=3.045 active_index=0 active_count=2** |
| 16:27:09.337 | texture-cache hard limit stepped to 3565 (floor), over budget 6,794 / 6,732 MB |
| 16:27:14.199 ... 16:28:04.260 | 11 `MCLA_KI035_WARP_WAIT` lines every 5 s, all identical: **ready=0 time=0 duration=3.045 flags16=00000042 active_index=-1 active_count=0**, paused 0, complete 1 |
| 16:27:16.4, 16:27:47.4 | F10 #5/#6: warper 20, request `BED351C0`, `camera300 = 00000000` (!), `request_camera108 = BF1F0BF0`, flags272 = 1,1,0,0,0,0,0,1,0 (+276 finish = 0, +273 = 1), prestream 3, timer_step 0.017/0.016 |
| 16:28:01.0 / 16:28:08.8 | owner left fullscreen, closed the window; clean exit |

Frame #5/#6: a close-up of an "Auto Repair" garage door, scene alive, camera still; this looks like the
*end* of a zoom-in, not its first frame. Audits #5/#6: 16 ms frames, 1 stream read in the first window,
CP alive. Content/save: no `[content]` WARN, F10 save copies 34-39 ms, transaction indicator 0.

The probe arm's garage trip (15:53:58, same pause -> go to garage after play) shows the healthy
sequence: 15 -> 26 -> 20 (Parallel `BF1F0980`, ready 1, time 0, duration 3.026, 0/2), then **20 -> 29 at
15:54:01.250 with ready=1, time=3.0210, duration=3.0260, active_index 0, active_count 1** (2.96 s later;
the companion transition had already finished). The garage loaded 1.2 s later.

### What the state means (generated-code citations; `mcla_recomp.8.cpp` unless noted)

Vtable slots read from the decrypted image in memory (nothing written to disk) for
UITransition_Parallel `0x82020DB4` (Lean `0x82027324` is identical in these slots): +92 `8221CE10`
(ready = flags16 & 0x80), +96 `82229400` (SetActive, recurses children then `826352C8`), +188
`82229AA0` (advance), +460 `821F84D0` (`*(+32) == 0`), +552 **`8221CF88` = `lbz r3,68(r3)`** (finished
byte), +556 `82228858` (SetFinished), +564 `8222F4E8` (Cancel), +568 `822292F8` (reset: f80/f84/f96/f104
= 0).

- **Active list per frame** `sub_82221DB0` (line 24948), called through `sub_822247A8` from the UI
  frame `sub_821FC588` (`mcla_recomp.7.cpp` 7623) *before* the warper update `sub_8221BE98`, both with the
  same dt (`f31`): for each entry, if `+552` (byte +68 "finished") -> `+96(0)`, `+64(0)`, `+112`, `+388`,
  `+128`, and the entry is removed (shift loop at `0x82221F48`); else if `+460` -> `+188(dt)`.
- **Finishing** `sub_82228858` (+556, line 41071): when byte +68 goes 0 -> 1 it calls `+96(0)`, which
  clears the ready bit (bit 0x80 at +16; setter `sub_8268D260` in `mcla_recomp.51.cpp`: `ori 128` /
  `rlwinm 0,25,23`). So **a transition clears its "ready" bit in the same list pass in which it
  finishes**, and the stop path later resets its time to 0 (`+568`).
- **A normally finished transition looks exactly like the frozen camera:** every idle snapshot of the
  previous camera (Lean `BF187960`/`BF1E2EF0`, which completed its 3 s shot) shows ready 0, time 0,
  flags16 `0x42`, not listed. So `ready=0 time=0 active_index=-1` does **not** mean "dropped before it
  ever advanced"; it is the finished state.
- **The warper was notified that the camera finished:** `sub_8221B8C8` (line 9510; the warper's
  transition-event callback, reached through a pointer table) checks `warper+256 == r4`,
  `req+108 == r5 == warper+300`, then calls `sub_82224A28` and **clears `warper+300`**; if
  `warper+276` (finish) is set it calls `sub_8221B790` (warper finalise: clear flags, state 29,
  recycle the request); in all cases sets `warper+273 = 1`. Snapshots #5/#6 show exactly that:
  `camera300 = 0` (written as the camera by `sub_82219DA0` at `0x82219E98`), `+273 = 1`, `+276 = 0`.
  The only other writer of `+300` is `sub_82219DA0` itself. So the camera completed and its
  finished-notification reached the warper **while the warper was still in state 20 with +276 = 0**.
- **State-20 exit** (`loc_8221C120`, `0x8221C124..0x8221C1E0`): exits only if `ready` (vfunc +92) and
  `time + dt + 0.0001 >= duration`, where `dt` is the *current* frame's dt. It never looks at the
  finished byte, `+273` or `+300`.

### Mechanism (most likely): a frame-time race between the camera's own finish and the warper's look-ahead

Per frame: list pass advances the camera `T_N = T_(N-1) + dt_N`; then the warper exits if
`T_N + dt_N + K >= D`. On frame N+1 the list pass advances by `dt_(N+1)` and, when the camera reaches its
end, finishes it (ready cleared). The warper therefore exits only if it predicted the crossing one frame
early using the **current** dt. It misses whenever `dt_N + K < D - T_N <= dt_(N+1)`, i.e. the frame
time **increases** at the last frame of the 3 s zoom. From then on ready = 0 forever and state 20 has
no other exit.

- On Xbox 360 (fixed 30 Hz step, dt constant) the miss window is empty (`K` = 0.0001 covers rounding);
  with Unlocked timing on PC dt varies frame to frame. In these audits frame time rises by > 1 ms on
  37-40% of frames; a rough miss probability per cinematic warp is `E[max(0, dt_(N+1) - dt_N)] / E[dt]`,
  about 5-15% depending on jitter (higher with vsync quantization and paging hitches). This fits an
  intermittent, PC-only freeze.
- It fits every field: Parallel finished (ready 0, time reset, unlisted, flags `0x42` like a completed
  Lean), companion entry finished too (count 0; in the healthy run it had already gone at 2.96 s),
  `+300` cleared and `+273` set by the finish callback with `+276 = 0`, frame shows the end of the zoom,
  pause manager not paused, request intact.
- Correlation, not proof: both captured freezes (l67 and this one) happened while the process was over
  the VRAM budget (l67 6.9 GB of 6.5-6.7; here 6,794 of 6,732 MB with the floor step at 16:27:09.3),
  i.e. during the most jittery frame pacing. The healthy probe-arm trip was in budget (5.3 of 6.7 GB).

### The other candidates

| candidate | verdict | evidence |
|---|---|---|
| (a) dropped as finished before it advanced (stale ready/flags) | unlikely in the "never advanced" form | finished/ready-0/time-0 is the normal post-completion state (see Lean snapshots); the frame shows the end of the zoom; byte +69 ("finish immediately", checked first in `sub_82229AA0` at `0x82229AD8`) is written only by the constructor `sub_82228090` (clears it). In its "finished before the warper saw it" form (a) **is** the race above |
| (b) pause menu close resets the whole list | no support | the only whole-list clearers are the manager constructor `sub_8221FE30` and destructor `sub_82220158` (no re-creation: `ui_logic` unchanged); count 2 -> 0 is explained by the companion's own shorter duration; menu closed at 16:27:09.079, before the camera was even appended (26 -> 20 at .184 with count 2) |
| (b') menu code cancels the shared named transition (`+564` = `sub_8222F4E8` -> `+556(1)` -> `sub_8221E408` remove) | possible, not preferred | gives the same end state (finished, unlisted, notification), but would stop the shot at its start; menu-system callers of +564 exist (`mcla_recomp.48.cpp` `sub_82625AC0`, `sub_826282C8`, ...) and the camera is looked up by name in the shared manager; frame #5 looks like the end of the zoom |
| (c) Warper_Abort / second BeginWarp | excluded | `sub_8221BB90` calls `sub_8221B790` (state 29, request recycled) before `sub_8221E310`; the warper stayed in 20 with the same request; `sub_8221E310` only stops (no removal) |
| (d) save / loading screen reset | no evidence | no content warnings, no save transaction at F10, no loading-screen state lines, warper already past the save wait |

What would settle (a)-race vs (b'): log, in the existing `sub_8221BE98` hook, the last state-20 sample
before ready drops (time, dt, duration) plus a hook on `sub_8221B8C8` logging `T`, dt and the warper
state at the notification. The race shows `T_last + dt_last + K < D` and a notification with `T >= D`
on the next frame; a cancel shows `T` well below `D`.

### Alt-Tab and the l67 comparison (owner detail)

- Alt-Tab is neither necessary nor sufficient: in this arm it happened 6.5 minutes before the freeze
  (16:19:20-16:20:41, plus 3 flaps), with three later F10s showing nothing left over (pause override 0,
  camera pause 0, pause manager 0, empty transition list, window focused, no pad reconnect line at all
  this session). The cap arm had a longer Alt-Tab (15:41:02-15:42:51, 110 s) and its garage trip worked.
  No lasting focus-related state was found.
- Shared by l67 and this freeze, besides "pause menu -> go to garage shortly after a race": PC, 1080p
  Unlocked, a type-17 cinematic warp reaching state 20, the warper frozen with request intact, and the
  process **over its VRAM budget** at the moment of the warp. l67 had a pad disconnect/reconnect and an
  Alt-Tab on the result screen; this one had neither near the freeze.

### Proposed real fix (guest-side behaviour, host hook)

Preferred: **complete the warp when the camera's finish notification arrives during state 20.** Hook
`sub_8221B8C8` (entry): if `r3 = warper` has `+288 == 20`, `+68` request non-zero, and the callback is
the matched one (`r4 == warper+256`, `r5 == req+108 == warper+300`), first perform the state-20 handler's
own exit (`*(0x8286D8F0)+293 = 0`, `warper+276 = 1`, `warper+288 = 29`), then run the original function.
The original code then sees `+276 = 1` and calls `sub_8221B790`, i.e. the exact sequence of a healthy
trip (handler exit, then finalise on the camera's finish notification), only without the frame-dt
look-ahead. Risk: low; it acts only when the camera the warper waits for has reported completion
(or was cancelled), which is the condition state 20 waits for; it never fires in the healthy path
(there `+276` is already 1 and `+288` 29 when the notification arrives). Residual: if (b') a menu cancel
is the real trigger, the shot is cut short instead of freezing, which is the desired outcome.

Alternative (no new hook point): in the existing `sub_8221BE98` post-hook, when state 20, request
non-zero and the camera's finished byte `+68` is 1 (or `warper+300 == 0 && +273 == 1`), perform the
same exit **and** the finalise that the missed notification would have done. That requires reproducing
`sub_8221B790` (flag clears, request reset to type 29, request recycled into the pool at +36/+40);
calling the guest function from the host hook would be simpler than mirroring it. A pure predicate
patch (`time + max(dt, last_dt)`) only narrows the race.

### Would the l68 watchdog (15 s) have released this case?

It would have **fired**: state 20, request/camera non-zero, Parallel time getter known (`f96`), not
paused; time stayed at 0 (below the episode maximum reached during the zoom), so about 15 s after the
shot stopped (~16:27:27) it would write `+293 = 0`, `+276 = 1`, `+288 = 29`. The camera finish
notification had already run with `+276 = 0`, so **`sub_8221B790` would not run afterwards**: the request
stays at `warper+68` un-recycled, flags 272/274/275/279/280 are not cleared, `+273` stays 1. Whether
the garage then loads depends on whether the load waits only on `*(0x8286D8F0)+293` (as the healthy run
suggests: garage 1.2 s after 20 -> 29) or also on the finalise; a repeated fire could leak request-pool
entries. It was off in both arms here, so this is untested; the watchdog-arm repro should check that
the garage loads and that later warps still work (and the fix above makes the finalise run).

## Safety counters

Both arms, every minute line: `forced_wait_failed=0`, `fallbacks_to_full=0`, read-back `aborted=0`,
`full_waits=0`, `guard_misses=0`, `gpu_blocks_dropped=0`; WRM single address `mem:1FCA3006` all session
(max 30.8 ms after start-up). Warnings only the known start-up set (VFS write probe, `t:\mc4\art\city`
test files, GameControllerDB, XeKeys, cross-thread leave, one invalid fetch constant). No `[content]`
WARN, no device removal, no crash package, `exit 0` both. F10 save copies 34-39 ms, 4 files, 0 skipped,
no transaction. 11 F10 packages complete (frame, state.json, audits).

## Spark

Not used: the inputs are private session logs and game-image reads that must not leave the PC.
