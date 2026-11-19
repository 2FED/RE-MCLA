# l116 on the RTX 5060: GPU time by category, day against night (2026-11-19)

Owner run, l116 (manifest DA88D447), arm pc/gpu-cat-race (the l115/l116 defaults + category timing every 4th frame, no verify queries), MAIN profile, session c46e0301, 3840x2160 fullscreen, 1080p target (guest 960x540 x2, forced HiDef, transfer elision, rectangle draw-extent bound with resolve-only deferred claims), Unlocked. In-game time from 14:13 (day) to 02:43 (night) per MCLA_TOD; free driving and races. Read by the primary agent. Spark not used.

GPU ms per frame, play minute windows (more than 1,800 draws per frame):

| Category | day (5 minutes, 3,234 draws) | night (9 minutes, 2,527 draws) |
|---|---|---|
| scene draws to 2x targets | 3.73 | 4.44 |
| scene draws to 4x targets | 1.57 | 1.09 |
| scene draws to 1x targets | 0.63 | 0.65 |
| transfers: stencil / depth / colour | 1.29 / 0.63 / 0.57 | 1.38 / 0.62 / 0.60 |
| resolves: EDRAM dump / copy / clear | 1.41 / 0.62 / 0.07 | 1.10 / 0.56 / 0.07 |
| texture load | 1.10 | 0.88 |
| shared-memory upload | 0.81 | 0.70 |
| read-back | 0.32 | 0.29 |
| GPU busy | 12.93 | 12.45 |

Reading:
- Scene draws are 5.9-6.2 ms (46-50 %); copying between surfaces is still 6.7 ms by day: transfers 2.5 ms, resolves 2.1 ms, texture loads and uploads 1.9 ms, read-back 0.3 ms.
- Day against night: +700 draws per frame, more 4x work (+0.5 ms: shadows) and more resolve dump and texture load (+0.5 ms), but less 2x scene time; GPU busy differs by only 0.5 ms: the daylight cost is mostly on the CP side (draw count), not the GPU.
- The fixed GPU floor to attack next: resolves (dump + copy 2.0 ms, about 60 per frame), the remaining transfers (2.5 ms, stencil half of it through the NVIDIA 8-pass path), texture loads (1.0 ms).
- No guard trip, freeze or device event in the session. Dips (MCLA_DIP, play only): eight episodes; the two longest are CP-bound by draw count: 7.1 s down to 46.2 fps at up to 6,200 draws and 4.0 s down to 47.0 fps at up to 6,136 draws (medians 63-71 fps); three one-to-two-second guest-simulation dips at 50.7-57.8 fps. Play minutes: 52.7-73.3 fps (lowest minute 4,775 draws per frame, CP 14.0 ms, GPU 14.7 ms; category timing adds some CP cost in this arm).
