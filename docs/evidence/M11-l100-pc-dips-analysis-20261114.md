# l100 PC fast-driving run: sustained fps dips are CP-bound by draw count (2026-11-14)

Read-only analysis by an Opus subagent (logs, HWiNFO, proc-sampler), accepted by the primary agent. Spark not used: deterministic parsing.

**Headline.** Owner run on l100 pc/default (2026-11-14, 11 minutes of fast free-roam driving, 9 F10 presses at drops; RTX 5060, 1080p Unlocked; session 34aec854 on the copy profile). The "hard drops" are SUSTAINED fps dips, not long frames: only one frame over 50 ms while driving (91.5 ms at 11:15:19, a fence wait); fps in the press windows 46.4, 44.6, 42.1, 37.7, 31.7, 42.7, 49.0, 48.8, 42.0 against a 48-52 baseline; the main dip lasted about 50 s. All dips are CP-bound and driven by the number of draws the game submits: frame time = 8.66 ms + 2.51 us x draws (r = 0.985 over 18 windows); normal 3.9-4.9 k draws, dips 6.0-8.7 k. In a dip: CP work 12.5 -> 22.1 ms (draw phases +5.7, uploads +2.0, watch/faults +1.4), GPU load FALLS to 72-80 %, the strict wait shrinks but its remaining 3-4 ms is pure loss in a CP-bound frame; constant-buffer uploads 8.1 k -> 18.3 k per frame; 79 % of constant dwords unchanged. Not GPU-bound, not guest-bound, not thermal (clock flat; package at the 35 W cap without throttling), not VRAM (1.27 GB under budget), not streaming (evictions and creates cost < 0.1 ms). Remedies ranked: Reflex instead of strict (-3..3.7 ms in dips), register-write skip and constants path (cp-verify then cp-reg), a 44 fps cap for consistency; dynamic resolution would not help.

---

# l100 pc/default 2026-11-14 11:11-11:23: sustained fps dips (read-only notes)

Evidence: private/owner-l100-20261114/{diagnostics,sessions}/34aec854... (save-private and
save-files-private.json excluded), private/owner-hwinfo-20261114/{HWiNFO_LOG_769393328.CSV,
proc-sampler-20261114-111120.csv}. Scripts here: parse.py (split/grep), hw.py, proc.py, audit.py.

## Per-frame budget, perf-split windows (driving)
Fit over 18 driving windows: frame_ms = 8.66 + 2.51 us x draws (r=0.985);
CP work (cp_busy - fence_wait) = 0.98 + 2.96 us x draws (r=0.995); GPU busy = 12.16 + 1.21 us x draws (r=0.96).
Crossover CP work = GPU at ~6.4k draws/frame. Dips = draws/frame 6.0-8.7k vs 3.9-4.9k normal.

| ms/frame | normal (11:22 min, 52.0 fps) | dip (11:16:03-32, 37.7 fps) | deepest (11:16:32-34, 31.7) |
|---|---|---|---|
| frame | 19.24 | 26.53 | 31.56 |
| GPU busy | 17.16 (89 %) | 21.23 (80 %) | 22.86 (72 %) |
| CP busy | 18.17 | 25.45 | 30.61 |
| fence wait (strict inside) | 5.65 (strict 5.47) | 3.33 (strict 3.02) | 3.90 (strict 3.68) |
| CP work = busy - fence | 12.52 | 22.12 | 26.71 |
| draw_ms | 6.36 | 12.03 | 15.27 |
| p_tex / p_bind | 1.98 / 1.76 | 4.09 / 3.12 | 5.70 / 3.89 |
| upload ms / MB | 1.68 / 6.7 | 3.71 / 14.4 | 4.45 / 16.0 |
| watch_arm + write_fault | 1.22 | 2.66 | 3.21 |
| wrm_mem / readback / stream_finish | 1.07 / 0.20 / 0.50 | 1.07 / 0.36 / 0.77 | 0.91 / 0.25 / 0.87 |
| ring idle (guest starve) | 0.01 | 0.02 | 0.03 |
| draws / subm | 3885 / 6.7 | 7155 / 12.4 | 8681 / 14.6 |
| cb_uploads / tex_memo_miss | 8.1k / 1660 | 14.7k / 3796 | 18.3k / 5309 |
| CP thread CPU (user+kernel) | 11.8 (k 3.1) | 20.9 (k 5.1) | 26.6 (k 9.7) |
Per draw CP cost does not rise in dips (3.2 -> 3.1 us): no clock/power slowdown of the CP.

## HWiNFO / proc-sampler same seconds
Dip 11:16:03-32 vs normal 11:21:51-22:51: GPU core load 80.7 vs 90.6 %, GPU power 62.2 vs 70.3 W,
GPU eff clock 2636 vs 2596 MHz, GPU perf-limit power 0.0 vs 0.7, bus load 100 vs 99 %, GPU temp 61/60 C;
CPU package 34.8 vs 32.9 W, STAPM 99.4 vs 93.8 %, Tctl 90 vs 86 C, HTC 0; MCLA_CPU eff 4293 vs 4272 MHz.
proc-sampler: game 21.8 vs 18.5 %, others 6-8 %, no background spike in any dip.

## Streaming
TEXHEAP creates/min 12.3k in 11:15:51-11:16:51 (dip minute) vs 3.1-6k later; create avg 15-29 us -> <0.1 ms/f.
Evictions 0 before 11:19 (all dips at 11:15-11:16 had none); 11:21 minute 5757 evicts at 50.6 fps: no cost seen.
Guest file reads (audit after F10): 12.5 MB/s in the 35 fps audit (11:16:03) vs 5.5 MB/s at 51 fps; 0 stalls.
VRAM over budget never (min -1265 MB). 11:22:56 dip coincides with a police pursuit start (11:22:41).

## Strict lead
Strict wait/frame is SMALLER in dips (3.0-3.7) than normal (4.1-5.5): no amplification; but in CP-bound
frames it is pure loss (the CP idles 3-4 ms per frame while it is the bottleneck).
