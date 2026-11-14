# l100 pc/auto-noguard soak (2026-11-14): Reflex without the strict bound

Read-only analysis by an Opus subagent, accepted by the primary agent with one correction (see the headline). Spark not used.

**Headline.** Owner soak on l100 pc/auto-noguard (2026-11-14, session 15c4d94f, MAIN save, 23.5 min in the log, F10 only at drops, no flicker seen): Reflex replaced the strict bound in 99 % of frames (55,132 active, 414 fallback; 25 stale-sleep fallbacks of 1-2 frames), no self-disable, guard would_trip 0, lead2_frames 274 (worst 2700-frame window about 63-67 against the 81 threshold), fence_late_f2p 1246. Third clean Reflex-without-strict run. Performance: no case where Reflex costs fps; at 6.5-8.9 k draws it is 4-15 % above the strict law. The low-fps MINUTES (31-34 fps) are the game's pause/menu state (UI cap: guest vblank 60, two vblanks per swap), not Reflex. Play was daytime and GPU-bound at 90-96 % (day costs about 2.5-3 ms more GPU per frame than night: frame = 15.3 ms + 1.26 us x draws). The owner's "40+ down to about 20 for 1-2 s in a race" is the F10 at 13:28:05: per-second fps 28/26/25/27 for 4 s with system CPU at 100 % while the game used 27 %: an external CPU load. PRIMARY CORRECTION: the subagent attributed it to a re-staging of l90/l93 at 13:27, but those file times are from 2026-11-11 and 2026-11-12; nothing under private changed at 13:27 today. The primary's own agents were active during the session (a read-only log analysis until about 13:33; the l101 SDK worktree checkout at 13:36:20, which matches the one-second 27 fps dip at 13:36:22), so agent activity is the likely cause of at least the 13:36 dip and a candidate for 13:28; no HWiNFO/proc-sampler in this run to prove it.

---

# l100 pc/auto-noguard soak, session 15c4d94f (2026-11-14 13:22:41-13:46:10, 23.5 min of log)

Evidence: private/owner-l100-20261114/{diagnostics,sessions}/15c4d94fbf0d059eda4666f1e5a1f556
(save-private, save-files-private.json, before/after-user, *.dmp excluded). Logs mcla.2 -> mcla.1 -> mcla
cover the whole process (first line = config). The log covers 23.5 min, not 40+.
Scripts here: budget.py (per-window budget + LOWLAT join + morning law), fit.py, audit.py (per-second fps
from the REX_PERF_AUDIT samples after each F10). m_<id>.log = morning sessions concatenated.

## Low-fps minutes = in-game pause/menu, UI cap at 60 Hz guest vblank (intended limiter)
- 13:24:43 31.5, 13:25:43 32.8, 13:31:43 30.8, 13:32:43 34.4, 13:38:43 33.2, 13:39:43 33.3 fps.
- "MCLA menu state: game paused (sim 1 ...), UI cap forced" -> "MCLA guest vblank: 60 Hz"; game running ->
  240 Hz. Paused spans: 13:24:06-13:25:12, 13:25:17-34, 13:25:39-58, 13:30:46-13:32:28,
  13:37:51-13:38:12, 13:38:16-28, 13:38:29-53, 13:39:08-15, 13:39:20-32, 13:39:36-53, 13:41:22-53.
- Budget there: wrm_mem 14.7-15.9 ms/f (WRM on mem:1FCA3006 = guest release flag written by the vblank
  interrupt callback), render_wait 14-17 ms/f, WRM signals 70-100/s (vs exactly 240/s = 14400/min in play),
  pacing p50 33 ms (2 vblanks per swap), Reflex sleep ~0 (p50 0.1 ms), GPU 65-71 %.
- MCLA_TOD paused=1 in the same minutes. Not Reflex, not guest simulation, not CP/GPU.

## Play windows: GPU-bound (daytime 14:15-18:00 after the start warp; morning runs warped to 03:02 night)
- GPU busy 90-96 % in clean play windows; fit (14 clean windows, wrm<1.2): frame = 15.3 + 1.26 us x draws,
  GPU = 14.7 + 1.14 us x draws (r 0.99), CP busy = 6.8 + 1.96 us x draws. Morning (night): GPU = 11.6-12.4 +
  1.16-1.32 us. Day costs ~+2.5-3 ms GPU per frame.
- vs morning CP law 8.66+2.51us*draws: below it at 4.4k draws (-9 %: 21.6 vs 19.7 ms, GPU 19.8 ms = limit);
  on it at 5-6k; above it at 6.5-8.9k (+4..+15 %: Reflex beats strict where CP-bound).
- Reflex sleep 7.7-10.2 ms/frame in play minutes (presleep 4.4-6.8), p99 14-17.5, max 19.5 (cap 30);
  higher sleep goes with HIGHER fps (sleep fills the CP slack while GPU-bound); ring idle 2.8-4.8 ms/f.

## Worst drop (owner "40+ -> ~20", race)
- F10 at 13:28:05: audit per-second fps 28/26/25/27 for 4 s, then 38, 47-51; frame max 50 ms.
  The race ran 13:26:42-13:28:59 (pause/results 13:28:59, autosave 13:29:05).
- Only one HITCH frame (12427, 50.2 ms, cp_busy 49.5 at 4928 draws, draw_ms 23.3, ll_sleep 0.02).
- MCLA_CPU: f10 window 13:27:43-13:28:05 and minute 13:28:05-13:28:44 are the only play windows with
  sys_pct max 100 / 99.9 % while the game's proc_pct max was 26.6/27.6 % (all other play windows max
  41-64 %); eff clock min 3139-3234 MHz.
- Inferred cause: host CPU contention from outside the game: private/l90-internal and l93-internal
  were re-staged 13:27:20-13:27:49 (two 54 MB mcla.exe + DLLs; AV scan likely), l93-CHECKLIST 13:29:06.
  Not proven (no proc-sampler/HWiNFO in this run).
- Other episodes: 13:29:39-46 (6 frames 50-57 ms, gpu_busy 46-55 ms, fence 18-20 ms, 6.5-7.9k draws:
  GPU-heavy frames), 13:36:22 (one second at 27 fps), 13:43:44-45 (CP-bound 9-9.6k draws).
  Morning default: min per-second fps 32 (11:16:03-34 CP dip), no 20-fps episode; 1 frame >50 ms.

## Reflex health
- init: backend=reflex result=ok max_sleep_ms=30 presleep_margin 1500 boost 0; minimumIntervalUs 0 by code.
- sleeping=1 every minute; no 3x>250 ms self-disable; 25 stale_sleep fallbacks, each 1-2 frames, mostly
  right after unpause/load; mismatch lines 8 (cap), minute sim_per_swap2p total 7, zero 1.
- GetLatency sim_to_present: play 15-40 ms; paused minutes 64-66 ms (30 fps x 2 frames).
- spin_ms_est 1.5-3.2 s/min in play (~1 ms/frame); overshoots 100-190/min.
- lead2 (lead_hist[2]) per minute max 63 (13:27:43); 274 total; max ~2700-frame window ~63-67 vs 81.
