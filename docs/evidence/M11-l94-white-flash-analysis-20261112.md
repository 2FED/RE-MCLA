# l94: one white whole-frame flash (2026-11-12)

Read-only analysis by an Opus subagent, accepted by the primary agent; cause not established. Spark not used: log parsing and source reading.

**Headline.** Owner, l94 pc/default (strict lead default on), MAIN save, ~30 min of normal play (2026-11-12, session 715bbb93, build D82D1F3C confirmed): otherwise fine, but once the WHOLE frame flashed white for an instant; F10 right after (package live-20261112T160703Z, local 18:07:03). Logs: lead2_frames 0, fence_late_f2p 0, 0 strict timeouts in the whole session; pacing steady (43.9 fps, max frame 41 ms in that minute, no hitch); in-game hour 4.95 (night; not the 06:00 switch); no forced warp finish, no pursuit, no presenter fallback or error; the captured frame (after the flash) is a normal night race on Sunset Blvd. The texture soft limit (2120 MB) was first reached in that minute, but the trim only destroys textures unused for 30 s whose last submission completed, so it cannot hit targets sampled every frame: ruled out. Not explained: a one-frame GPU-side exposure/bloom glitch (KI-077 / KI-041 class) or a legitimate game flash are the candidates; the logs have no per-frame post-fx or luminance record. Seen once; not a regression signal for l94.

---

# l94 whole-frame white blink (owner 2026-11-12, package live-20261112T160703Z-45652-1)

Evidence copied (save-private / save-files-private.json excluded) to
private/owner-l94-20261112/{diagnostics,sessions}/715bbb93fff5ec65b492699b9c27eec8.
Work: all.log (concatenated mcla.2/1/log), fr.py (frame pacing from composite lines), frame.png.

## Package
- flight ring = livery resolve ring only (200 entries, frames 25712..25898, F10 at frame 25898).
  Composite 039CA000 resolved every frame 25713..25898, no frame skipped, all miss=0 decision=now
  read=prev steady fast wait 0 timeout 0. 14 resolve2 entries (128x128 livery slots), normal.
- No per-frame frame-time / present / swap / post-fx record exists in the package.
- Pace from composite frame numbers: 37-42 frames per second every second 18:06:50..18:07:05,
  no gap > 33 ms/frame at 5-frame sampling. Minute pacing 18:06:17-18:07:17: 43.9 fps, max 41.3 ms,
  >50 ms 0, >100 ms 0.
- Non-livery lines 18:06:18..18:07:03: only minute aggregates (TIMERQ, LOG, VRAM, CPU, TOD);
  no warning/error in the whole minute. Guest capture success sequence=25897 = last presented.
- frame.bmp (after the blink): night career race, timer 01:40.52, lap 1/2, 176 km/h, Sunset Blvd,
  dry road, clear sky, yellow smoke checkpoint marker; normal exposure, nothing anomalous.
- Pursuit change at 18:07:06 is changed=breaking (speed bits), score 0, phase none: not a pursuit.
- No window event 17:58:46..18:09:26. No CP_FREEZE after 17:56:42. Race started ~18:05:22.

## Texture soft limit
- limit_soft = 2048 + 24*(2*2-1) = 2120 MB (cache.cpp:263-267) = the used_mb plateau: trim engaged
  first in the minute 18:06:17-18:07:17 (used 2112 -> 2120). creates 9793 (prior minutes 6255-10505).
- tex_gen_bumps tracks creates in every minute (also below the cap) -> not a trim counter. No destroy
  counter exists.
- Trim (cache.cpp:272-305) destroys LRU only when last_usage_submission_index <= completed (280)
  AND unused >= soft_lifetime 30 s unless hard exceeded (283; hard 5595 far away). Any texture used
  in the current or recent frames (whole post-fx chain, adapted luminance, bloom) is ineligible;
  ResetTextureBindings on first destroy (293). Placed range reused only after completion
  (d3d12/texture_cache.cpp:1497-1503); a recreated texture is fully reloaded before use.
  Independent of the frame-lead bound (submission-index based).
- Earlier sessions at the cap without a blink report: l88 5a50 22 min, cap44 7, steadyoff 6,
  l90 7585 14, add7 5, l91 3b8f 28 min (adaptive).

## Verdict
No host anomaly recorded; instruments are minute/sampled and cannot see a single frame.
