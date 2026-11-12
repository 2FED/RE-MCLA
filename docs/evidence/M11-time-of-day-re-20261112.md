# Time of day: reverse engineering and dev-control design (2026-11-12)

Read-only RE by an Opus subagent, accepted by the primary agent; proven and inferred points are marked in the body. Lead from LARecomp (no licence): addresses and mechanism only, no code. Spark not used: private generated-code reading.

**Headline.** Time of day is a float hour 0..24 at lighting manager (*0x8287E26C) + 0x38CC, advanced in sub_822F1DB0 by rate x scaled dt: night 3 real minutes per game hour, dawn/dusk (4.5-7.0, 16-19) 6 minutes, day 2 minutes; render copies at +0x38C4/+0x38C8. Night state = hour > 18.0 or hour < 6.0 (sub_822F3BD8): it flips render-pass mask bits exactly at 06:00, the leading candidate for the dawn sidewalk event. Arcade presets 0.0 / 6.0 / 12.0 / 17.4 are applied by a warp at race start (sub_8265D2B8 -> sub_822F1518), which is the reset on restart. Lead: LARecomp has a Time of Day tab (addresses and mechanism only; no code taken). Design: stage 0 read-only MCLA_TOD log line and F10 field; rate keys (x10, x30, freeze) by post-update fix-up; start keys 04:30 / 05:30 gated to the Arcade preset warp. Whether the hour reaches the MAIN save is not proven: test on a copy profile.

---

# Time-of-day RE notes (2026-11-12, read-only)

Lead source: LARecomp src/mc_engine/time_of_day.cpp header comments (addresses/mechanism only; fetched as data
into fetched/). Every address below re-verified against generated/default and the retail image expanded in
temp (scripts/xex.ps1, AES retail key + basic compression; 10,354,688 bytes, matches earlier evidence; deleted).

- Manager ptr: *(be32*)0x8287E26C (mcLightingManager). ctor sub_822F3498 (TU15:42453).
- +0x38CC (14540) float hour 0..24 (sim/current). +0x38C4/+0x38C8 (14532/14536) render double-buffer copies
  indexed by *(0x8288B9F0) (update thread) / *(0x827E5640) (render thread, getter). +0x38D1 auto byte,
  +0x38D2 garage byte, +0x38D4 rate h/s, +0x38D8 warp flag, +0x38DC warp target, +0x38E4/+0x38E8 night
  window 6.0/18.0.
- Update sub_822F1DB0 (TU15:38792-39162), called only by task fn sub_823045B0 (TU16:21540).
  Rates: <=4.5 or >=19: 1/180 h/s; (4.5,7) or [16,19): 1/360; [7,16): 1/120. dt = *(0x827D7508) scaled.
  Pause gate *(0x828CCEA0)+4 && !+10 -> no advance. Wrap at 24 (0x82007340). Warp uses unscaled dt 0x827D7558.
- Warp sub_822F1518 (TU15:37314). Callers: race preset sub_8265D2B8 (lr 0x8265D61C, rate 3.0),
  sub_826563E8 (lr 0x82656494, hour [obj+1184], rate 5.0), sub_8226C6C8 (lr 0x8226C804, rate 3),
  sub_822D2258 (lr 0x822D2290, rate 2), replay restore sub_82260F20, script Graphics_WarpToTimeOfDay
  sub_82304E90 / sub_82304630.
- Preset table sub_8238EB90 (idx 1..4): 0.0, 6.0, 12.0, 17.4 (other idx -> keep incoming f1).
- Night flag in sub_822F3BD8: hour > +14568 (18.0) || hour < +14564 (6.0) -> render mask bits.
- Boot hour 15.0 (0x82007F9C). Dev option ptr 0x8287E2A4 parses into +14540 and disables auto.
- Forcing flag byte 0x8288E5B8 -> hour = 0.0 in sub_822F3BD8 (TU15:43705); no static writer found.
- Xbox Live session properties include "TimeOfDay" (table 0x82033E48) and "TODBase".
- mcAmbientTod with TodArray / TodDensityArray: ambient density by hour (risk for traffic/peds).
