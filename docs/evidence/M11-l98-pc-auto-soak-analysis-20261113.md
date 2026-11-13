# l98 pc/auto soak (2026-11-13): Reflex guard trip, hitches, VRAM budget, ocean shimmer

Read-only analysis by an Opus subagent (logs, HWiNFO, proc-sampler), accepted by the primary agent. Spark not used: deterministic parsing.

**Reflex auto.** Owner soak on l98 pc/auto (2026-11-13, session 258f08ee, 44 min on MAIN, RTX 5060, 1080p Unlocked): owner: no artefacts. Instruments: Reflex initialised; the lead-2 guard TRIPPED ONCE at 18:25:11 (81 lead-2 frames in the 2700-frame window, 5 minutes in) and latched the strict bound for the rest of the session, so only 9.8 % of frames (10,318 of 104,877) ran with Reflex replacing strict; 105 lead-2 frames and 542 late fences in that active window, 0 after the latch; 65 active/inactive changes (33 stale-sleep at loads, menus, race starts). Sim-to-present 13.7-18.9 ms while active, 24-66 ms after the latch. So this soak did NOT validate Reflex-without-strict as a default; the guard threshold taken from one l96 run was too tight for real play. Keep strict as the default; next auto variant: on a guard trip fall back to adaptive, or re-arm after clean minutes.

**Hitches and VRAM.** Owner priority 2026-11-13: hard frame drops on PC, noticeable at 200 mph even with Reflex. l98 soak data (per-minute resolution only): 73 frames over 50 ms and 7 over 100 ms in 42 minutes; all seven > 100 ms fall in minutes with menu toggles or race-start warps (worst 149.7 ms); quiet driving has about one > 50 ms frame per minute and 17-73 frames over 34 ms per minute against a median of 21-25 ms. Excluded by counters: texture creation (max 0.37 ms), PSO waits, streaming stalls, timer queue, foreign CPU load, CP freeze; no logged quantity correlates above rho 0.35, and nothing is logged per frame. VRAM: our process was over its DXGI budget by 125-410 MB from 18:31 on (other processes hold ~1.45 GB of the 8 GB card, incl. a web wallpaper); dedicated memory 7.9 of 8.15 GB and shared memory rising to 0.8 GB (the Steam overlay showed 8.3 of 7.7 GB); the texture hard limit stepped down to the soft limit (2120 MB), so eviction is immediate and textures churn (2-9 k creates per minute); no heap is ever released. Hitch rate did not change when going over budget in this run. Next: a per-hitch log line (MCLA_HITCH) to attribute each long frame, then texture-churn and budget fixes.

---

# l98 pc/auto soak 2026-11-13 (session 258f08ee...), read-only analysis notes

Build B70FD8FE... (launch-receipt), RTX 5060 Laptop 8 GB, scale 2, 1080p render, Unlocked, 4K 120 Hz (window went
fullscreen during the run), MAIN. Log 18:20:32-19:04:32 local (44 min; 42 CP minute lines after the load minute).
Reflex init ok (mode=auto backend=reflex). ll_mode=1, guard 2700/80.

Scripts (python -I, this folder): inv.py (prefix inventory), mins.py -> recs.json + events.txt, table.py -> table.txt
(per-minute join, 3 s tolerance), corr.py -> V.json (Spearman + hit/calm means), menu.py (hitches vs menu/warp/F10),
audit.py (REX_PERF_AUDIT per-frame windows), ring.py (F10 flight rings), hwcols.py/hwall.py/hw.py/hw2.py (HWiNFO),
proc.py (proc-sampler), bmp2png (l86 script) -> f10_1.png.

## Low latency / guard
- Active (bound replaced) 18:21:03 -> guard_trip 18:25:11 frame 10737 lead2_in_window=81 (latched for the session).
- lead_ll_active_frames 10318 of 104877 (9.8 %); fallback 94559; guard_trips 1; changes 65 (33 stale_sleep lines).
- lead2 per minute while active: 6 (load), 15, 0, 53, 31 = 105; fence_late_f2p 24+74+0+268+176 = 542; after latch 0/0.
- GetLatency sim->present: active 13.7-18.9 ms; latched (strict + Reflex sleep p50 0.1 ms) 24-66 ms.
- Strict waits after latch 5-8 ms/frame (13-19 s per minute), per-minute max 10-67 ms.

## Hitches (MCLA frame pacing/min; per-frame data only in the two 300-frame F10 audit windows: no hitch there)
- Excl. load minute: 73 frames >50 ms, 7 >100 ms in 42 min. Quiet minutes (no menu/race start/F10): 12 min,
  12 frames >50 ms, 0 >100 ms, max 46-84 ms. Busy minutes: 30 min, 61 >50, all 7 >100 (max 149.7).
- No minute counter separates hitch minutes (Spearman |rho| <= 0.35: system CPU max 0.35, readback-sync max wait
  0.30, WRM max 0.23; VRAM over budget -0.14; tex creates 0.14; PSO wait -0.09).
- Texture create max 115-366 us/min, heaps_sync 0, refill waits 0; PSO wait <= 14 ms/min; perf audit pso/shader 0;
  stream reads no stall >5 ms; TIMERQ late max 3-7 ms; no foreign CPU load (proc-sampler other max 7-25 %).
- PresentMon (HWiNFO 2 s windows): p99 >50 ms in 13 play windows, >100 ms in 1 (18:34).

## VRAM
- Budget 7131 MB at start, 5680 MB from 18:24 (other processes took ~1.45 GB), 5424-6011 later.
- Process local 4078 -> 5897 MB in 256 MB steps (9 placed heaps, 2304 reserved / 2120 used); over budget from
  18:31 to the end by 125-410 MB; nonlocal 150-240 MB. Hard limit stepped 5595 -> 2048 (14 steps 18:27-18:29:43).
- Cache limit at scale 2 = 2048 + 24*3 = 2120 MB = used_mb from 18:32 on: soft == hard, eviction immediate.
- heaps_released 0 all session; heaps keep one spare (texture_cache.cpp:1683).
- HWiNFO system-wide: dedicated 7924-7971 of ~8150 MB (avail 180-230 MB), D3D shared 245 -> 800-870 MB from 18:29.
- Hitch density before over-budget 1.9/min vs after 1.5/min: no association in this run.

## Ocean flicker (F10 1, 18:52:47 local)
- TOD 10.7 h day, auto; adapt_lum 0.014-0.13 swings minute to minute; race lap 1/2 at 180 mph on Santa Monica beach.
- Strict on (latched), minute lead2 0, f2p 0. Flight ring: 124 composite 039CA000 copied now, prev complete, no
  miss/forced; resolve2 deferred only the day tree-impostor family 0317C000-03566000. Frame shows no artefact.
