# l119 Steam Deck analysis (2026-11-22): default, scene-1x, gpu-new

Read-only analysis by an Opus subagent (text logs and JSON only, `python -I` with scripts kept outside the
archive; frames converted to small JPEGs with single-threaded ffmpeg), for acceptance by the primary agent.
One run per arm (2.5-5 minutes of play): fps differences under about 0.7 are noise. Spark not used:
deterministic log parsing. Local clock UTC+2.

Inputs: D:\119 copied (readable parts only: robocopy excluding save-private, before-user, after-user,
save-files-private.json, *.dmp, runtime-journal-private.log and executables/scripts) to
`private/owner-deck-20261122/`. Read: sessions\<id>\{mcla.log, launch-receipt.txt, state.txt, managed.toml,
capabilities.txt, store-seed.txt}, diags\<id>\{report.txt, live\*\{report.txt, manifest.json, state.json}},
and the seven F10 frame.bmp files as downscaled JPEGs. Not opened: save-private, save-files-private.json,
before/after-user, runtime-journal-private.log, *.dmp, vkd3d caches. Parsers (work folder): tags.py,
extract.py, win.py (per perf-split window), hitch.py, dip.py, vb.py, warn.py, js.py.

## 1. Identity and run mapping

All three sessions: `build 2B1BC8B1EED2CEA6BEDA2600C7D695F9AE5F2650F5C6B06B03E39B29E6CE8BBB` in
launch-receipt.txt, capabilities.txt and every report.txt = `build_manifest_sha256` of
`private/l119-internal/candidate-receipt.json`. **No mismatch.** state `complete`, `exit 0`, crash folders
empty. Mode stock, guest 1280x720 (`MCLA_VIDEOMODE width=1280 height=720 hidef=1 forced_hidef=1
refresh=60`), render target 1280x720, swapchain 1280x800, panel 90 Hz, vsync on, RADV VANGOGH.

| owner run | session | local time | length | `mcla-dev-overrides.txt` | arm (from the `MCLA dev override:` lines) | F10 |
|---|---|---|---|---|---|---|
| 1 | 0c20101f | 10:34:34-10:38:05 | 211 s | 9 values | **deck/default** (the 9 default lines only) | 10:35:49, 10:37:58 |
| 2 | 66ebc0a3 | 10:39:26-10:44:42 | 317 s | 10 values | **deck/scene-1x** (`guest_msaa_scene_1x=true (mcla_guest_msaa_scene=1)`) | 10:40:39, 10:41:30, 10:42:36 |
| 3 | 37ce0981 | 10:45:54-10:49:15 | 181 s | 11 values | **deck/gpu-new** (`xfer_elide=true`, `xfer_extent=true`) | 10:47:02, 10:48:51 |

The order matches the owner's report (checklist Run 1, Run 2, then Run 4 as the owner's "run 3";
scene-1x-fxaa, shm-census, rt-memo2-verify, vb-old, cp-verify2, levers-off not run). The longest session
(317 s) is the one with the race, as reported. `m10 defaults: d3d12_transfer_elide_full_overwrite /
d3d12_draw_extent_from_rect left at false (windows_only)` in runs 1-2; run 3 logs `MCLA_XFER_ELIDE on=1`
and `MCLA_XFER_EXTENT on=1`.

## 2. Per-run numbers (play windows; perf split, one draw in 16 sampled)

Play windows: run 1 minute 3, minute 4, F10 window 5 (54.6 + 60.1 + 14.2 s); run 2 F10 window 3, minutes 5,
7, 8 (50.4 + 60.0 + 58.7 + 60.0 s); run 3 minute 3, F10 window 4 (60.0 + 47.0 s). Ranges over those windows;
the representative window in brackets.

| | run 1 default | run 2 scene-1x | run 3 gpu-new |
|---|---|---|---|
| `MCLA_GTILE` in play | tiles=2 msaa=2 tile_span=512 rule_ok=1 (pause/map: tiles=3 msaa=4) | tiles=1 msaa=1 tile_span=720 game_msaa=1 mode=1 rule_ok=1 (pause/map: tiles=1 msaa=1 applied=1 for the forced-4x pass) | as run 1 |
| `MCLA_GMSAA` | not requested, mode 2 | requested=1 applied=1 mode 2->1 | not requested |
| `MCLA_GPU_SURF draws_by_key` | p1280m2c1024 at offsets 0 **and -512**, 1,948 each per frame; offsets_max 2 | p1280m1c1024 offset 0 only, 2,027 per frame; offsets_max 1, multi_offset_frames 0 | as run 1 (2,159 each) |
| draws per frame (= draws issued, noop 0) | 5,915-6,594 [m4 5,924] | 3,303-4,314 [m5 4,185] | 6,261-6,456 [m3 6,456] |
| fps (perf split) | 15.65-16.23 [15.83] | 18.46-20.82 [19.52] | 15.66-15.82 [15.66] |
| frame interval p50 / p90 / p95 ms | 66.3-66.4 / 67.5-67.6 / 82.9-83.2 | 49.6-50.2 / 66.5-66.7 / 66.9-67.3 | 66.45 / 67.5 / 82.6 |
| share of frames > 50 ms (66.7 ms bucket or worse) | 87 % | 41-60 % | 92 % |
| frames > 100 ms per minute | 1-3 | 2-16 (PSO, see 4) | 0 |
| CP busy ms/f | 54.2-55.95 [55.49] | 35.3-43.5 [41.90] | 55.8-56.1 [56.06] |
| ring idle / WRM / fence wait ms/f | 0.3 / 7.1-7.7 / 3.4-3.9 | 2.1-3.5 / 7.1-10.4 / 2.7-4.1 | 0.3 / 7.1-7.5 / 2.9 |
| GPU busy ms/f (timer queries work under Wine) | 27.0-27.4 | 21.7-23.7 | **24.1-24.4** |
| CP per draw us (busy / phases / packets_other) | 9.37 / 3.99 / 1.26 | 10.01 / 3.85 / 1.28 | 8.68 / 3.77 / 1.25 |
| render thread cpu / spin (MCLA_SPIN) ms/f | 58.5 / 11.1 | 47.3 / 1.2 (m5), 49.2 / 1.1 (m8) | 59.4 / 9.9 |
| render-thread own work (cpu - spin) ms/f | 47.4 | 46.1-48.2 | 49.5 |
| GPU Commands / render / audio-engine (F8000534) thread, % of a core | 69 / 93 / 72 | 63 / 93 / 72 | 70 / 93 / 72 |
| `MCLA_LOWLAT kind=init` | backend=none result=wine | same | same |
| `MCLA_FAULT_PROBE` status | ok | ok | ok |

Per-draw phases (us per draw, run 1 minute 4, 5,924 draws) against the PC l115 dense 6.1 k column of
`M11-l117-sdk-cp-per-draw-20261119.md`: setup 0.196 (PC 0.094, 2.1x), prim 0.419 (0.129, 3.2x), rt 0.454
(0.169, 2.7x), pso 0.287 (0.111, 2.6x), tex 1.163 (0.389, 3.0x), bind 0.923 (0.332, 2.8x), vb 0.403 (0.157,
2.6x), record 0.145 (0.082, 1.8x); phases 3.99 (1.46, 2.7x); upload region 2.26 (0.64, 3.5x), arm 1.36 (0.20,
6.7x), write fault 1.10 (0.27, 4.1x), disarm 0.60 (0.08, 7.8x); CP busy 9.37 against 3.13 (3.0x). Against l96
default (55.9 ms at 5,890 draws, 9.49 us/draw): CP per draw is unchanged within one-run noise; packets_other
fell 10.22 -> 7.46 ms/f (lever 1 is a default now) while phases read 21.8 -> 23.6 ms/f (different scene mix).

Memo and skip counters (verify off in every arm, so mismatch counters are structurally 0 and say nothing):
texture memo v2 hit 0.81-0.92 in play (`MCLA_TEXMEMO` mismatch 0, verify 0); RT memo hit 0.61-0.65
(`rt_mm` all 0, verify 0; `rt_memo2=0`); register-write skip 17.3-19.7 k registers and 228-291 k constant
dwords skipped per frame (`reg_side_mismatch=0 cb_mismatch=0`, verify 0); fetch dwords unchanged 66 %, ALU
dwords unchanged 64-70 %, same PSO 63-66 %, same RT key 99.5 %. **First Deck data for these levers is
throughput only; no Deck verify data exists yet** (rt-memo2-verify and cp-verify2 were not run).

Warnings: the same classes in all three (t:\mc4\art\city device, test_*.loc, XeKeys, cross-thread
RtlLeaveCriticalSection, one invalid fetch constant in runs 2-3); the only new class is `MCLA_CP_FREEZE` in
run 2 (section 4). No crash, no hang snapshot.

## 3. KI-088 (vertex-buffer request skips without a validity check): live in this game

`MCLA_VB v=1 kind=config validate=1` in all three. Totals over each session:

| | frames | vb_slots | vb_insync_skip | vb_same_skip | vb_insync_skip_invalid | vb_same_skip_invalid | invalid per frame | per insync skip |
|---|---|---|---|---|---|---|---|---|
| run 1 | 3,507 | 16.98 M | 2.72 M | 0 | **527** | 0 | 0.150 | 194 ppm |
| run 2 | 5,952 | 20.95 M | 2.68 M | 0 | **121** | 0 | 0.020 | 45 ppm |
| run 3 | 3,097 | 15.32 M | 2.51 M | 0 | **390** | 0 | 0.126 | 155 ppm |

In driving windows 0.18-0.32 invalid skips per frame at 2 tiles, 0.01-0.08 at 1 tile. `MCLA_VB_STALE`: 16
lines per session (the logging cap; they cover only the first 15-25 s of play, frames 696-1145), **every one
slot=95 kind=insync**; sizes 80 B, 512 B, 4.6-11.4 KB (7,616, 8,224, 8,384, 10,720), 21-42 KB (21,888, 23,872,
37,088, 37,408, 40,448, 41,792, 42,656) and 224,864 B; addresses in 0x0E11-0x0E19xxxx, 0x0FAD-0x0FBAxxxx and
0x1CC4-0x1CDAxxxx (0x0FB84000 size 10,720 and 0x1CDA64B0 size 80 repeat across sessions). `vb_same_skip` never
fires on the Deck. **Answer: the defect is live; without the l118 validation those draws (one every 3-7 frames
at 2 tiles) would have drawn from stale vertex data in fetch slot 95. The fix (default on) re-requests them.**
Whether slot 95 is the KI-082 "trembling" source cannot be said from counters (vb-old arm not run).

## 4. Shared memory under Wine

`MCLA_FAULT_PROBE` (private 4 KiB page, n=2000, status ok, no failures), p50 in us:

| | arm | dispatch | disarm | resume | dispatch+resume | round trip | total ms |
|---|---|---|---|---|---|---|---|
| run 1 | 3.85 | 3.42 | 3.99 | 0.36 | 3.79 | 7.84 | 47.5 |
| run 2 | 3.15 | 2.87 | 3.26 | 0.29 | 3.16 | 6.42 | 41.2 |
| run 3 | 3.20 | 2.85 | 3.30 | 0.29 | 3.14 | 6.44 | 41.2 |
| PC (unit test) | 0.49 | 0.70 | 0.40 | 1.0 | 1.7 | | |

Wine costs 6.5-8x the PC per protect call on a private page and 4-5x per exception dispatch, but its
resume is 3x cheaper. **The Wine exception path itself is only about 3.2-3.8 us per fault.** The real cost is
the protect calls on guest memory: `MCLA_SHM arm_us_per_call` 15.8-17.2 (run 2 up to 19.1) and
`disarm_us_per_call` 19.3-23.6, i.e. 4-6x the probe's private-page call (arms average 13.9 pages per call,
disarms 41 pages per call). Hypothesis to check (not proven here): the per-view protect of the guest physical
section (several alias views per arm) or per-page bookkeeping in Wine's VirtualProtect explains the factor.

`MCLA_SHM` (census=0, single_fast=1, inval_unwatched=1), run 1 minute 4 per frame: requests 10,528 (VB 5,217,
IB 5,091, texture 163, resolve 56); lock-free fast 95.5 %, single-range 99.7 %; locked = uploads 472 (27.5 MB,
58 KB each); arms 492 x 16.41 us = 8.07 ms; disarms 176 x 20.09 us = 3.53 ms; faults 176 (render 165, other 10,
CP 0); invalidations 176 (11.1 k pages); `protect_fail=0`; `unwatched_inval` 7 events / 2,076 pages in each
session's first minute (world load) and 0 in every later window. Run 2 minute 5: 7,461 requests, 355 uploads
(20.5 MB), 369 arms (6.27 ms), 131 faults. `fault_ms_*` are 0 (census only).

Cost per frame (run 1 minute 4): **CP: upload region 13.41 ms (24 % of CP busy), of which arm 8.07 ms;
upload sub-timers memcpy 3.20, build 2.60, barrier + pool + record 0.14. Render thread: write faults 6.52 ms
(37 us per fault including the 20 us disarm) plus Wine dispatch/resume 176 x 3.5 us = about 0.6 ms that no
counter holds.**

The study's levers with these numbers:
- (a2) compare-before-copy for hot pages. The hot share was not re-measured (census arm not run); with the
  l81 range h = 0.49-0.84: arms removed 241-413 per frame = -4.0 to -6.8 ms CP; compares at R/U about 5 and
  about 1 us each (2.5x the PC 0.3-0.6 us) = +1.2 to +2.0 ms; **net CP -2.8 to -4.8 ms per frame; render
  thread -3.5 to -6.0 ms per frame** (h x 176 faults x 40.5 us). Inside the study's -2 to -4 ms CP estimate,
  and larger on the render thread than the study assumed.
- (e1) copy prologue: the CPU-side barrier, pool and record time it removes is only 0.14 ms per frame on the
  Deck. Any gain would have to come from the replay worker (16 ms/f) or GPU render-pass splits, neither
  measured; `up_first_sub` unknown. **Expected CP gain under 0.2 ms; low priority on the Deck.**

## 5. scene-1x (run 2) against default (run 1)

- Draws: 5,924 -> 4,185 per frame (-29 %). The two-tile key carried 1,948 draws per tile (32.9 % of the
  default frame); the single-tile key carries 2,027. **About 88 % of the predicted tile-replay saving
  materialised** (-29 % against the 33 % Deck tile share and the PC's 35 %); the rest is the slightly larger
  single-tile key and route mix (run 2 includes the race).
- CP busy 55.5 -> 41.9 ms per frame (-13.6 ms, -24.5 %); GPU 27.0 -> 22.3 ms; fps 15.8 -> 19.5 (+23 %); frame
  p50 66.4 -> 50.0 ms. The panel quantises to 16.7 ms steps: about half the frames moved from the 15 fps bucket
  to the 20 fps bucket (66.7 ms frames 87 % -> 41-60 %). That is the owner's "13-22 instead of 13-15".
- Why the dips stayed (dip seconds, `MCLA_DIP` cause_hint `cp_draws` in 83 of 97 seconds): fps follows draws
  per frame in the same way in all three arms (4.0-4.9 k draws: 18.5 / 18.2 / 19.9 fps; 5.0-5.9 k: 18.2 / 16.2 /
  18.2; 7-8 k: 14.1 / - / 14.8), with CP busy rising about 9 us per draw and uploads 11-24 ms in the heavy
  frames. scene-1x moves the curve down by about 1.8 k draws, but the heavy places still reach 5-6 k draws at
  1x (dip frames 83 ms with CP 68-80 ms and uploads 17-24 ms). Second binder: **the guest render thread is now
  saturated** (spin 11 -> 1.2 ms/f; own work 46-48 ms/f, of which faults 5.2), so even when the CP is under
  50 ms the frame sits on the 50/66.7 ms edge. Third, run-2-only: **first-use pipeline compiles of the 1x
  variants**: 38 PSO-caused hitch frames totalling 11.8 s, three `MCLA_CP_FREEZE kind=begin scope=pipeline_wait`
  episodes (2.9 s at 10:40:00, 1.45 s at 10:40:07, 1.85 s at 10:40:22; GPU not behind; stack in vkd3d /
  winevulkan) and about 17 s of `pso_wait` over the session (run 1: 1.5 s, run 3: about 0). Guest simulation
  and audio do not bind (guest_sim under 3 ms in dip seconds; audio thread unchanged).
- F10 frame of the fence moment (run 2, package `live-20261122T084129Z-888-2`, 10:41:30): the chain-link
  fence in front of the building on the right is drawn as an irregular grid of isolated bright pixels
  (screen-door look), wires below a pixel wide; poles, cars and pedestrians are normal. The other two run-2
  frames show solid railings unaffected. **Expected cause: alpha-tested fence texture at 1x with render state
  15 off (the game's own 1x path, l106/l107 RE; state 15 is probably alpha-to-mask, an inference).** With 2x
  MSAA the coverage gradient spreads the wires; at 1x each pixel passes or fails the alpha test and the
  pattern changes from frame to frame with motion, which reads as flicker. Mitigation, without overclaiming:
  FXAA smooths geometric edges but cannot restore sub-pixel coverage; it may soften the dots, not stabilise
  them. A host-side ordered-dither (screen-door) alpha test for those draws would make the pattern stable
  in screen space but stay visible; alpha blending them would change sort order. Neither is tested.

## 6. gpu-new (run 3)

- Engaged: `MCLA_XFER_ELIDE on=1 verify=0` minute 3: 21,622 draws considered, 16,784 proven (17.9 per frame;
  full 5,640, partial 11,144), 57,336 transfers of which 44,978 cut; `restored=0`, check n=0 (verify off).
  `MCLA_XFER_EXTENT on=1`: 27,359 of 51,924 bounded (29 per frame), rows 54.0 M -> 7.8 M (-86 %); deferred
  claim 0, restore 0, missing 0, exact 3,266. No `result=bad` anywhere (none possible without verify).
- Transfer samples per frame 29.1 M (run 1) -> 20.2 M (-30 %, depth -40 %); **GPU busy 27.0 -> 24.4 ms per
  frame at 9 % more draws (-17 % GPU per draw)**. fps 15.66 against 15.83 (noise): the Deck is CP-bound, so
  the GPU saving does not show.
- Artefact indicators: none in counters or warnings; owner saw nothing (lap only, no race, so the KI-084
  minimap check was not done).
- "Zoom from the map onto the car after PRESS START is more stable": world entry (first `GTILE where=scene`,
  then 30 s): run 1 had a 366 ms upload frame and seven PSO hitches 73-484 ms at +5.5..+7 s (22 new
  pipelines, 1.50 s of PSO waits in 1.77 s of hitch frames); run 2 had the 2.9 s / 1.45 s / 1.1 s freezes; **run 3 had no PSO hitch at
  entry** (pso_new 0-1; worst 317 ms upload frame, then 66-99 ms CP frames). The shader store was identical at
  every start (xsh 677, rtv.d3d12 1515), so the difference is most likely the driver-side pipeline cache
  warmed by runs 1-2 for the same two-tile pipelines, not elision or extent. Not provable from one run.

## 7. Audio (KI-060)

`MCLA_AFILT kind=config native=1 verify=0 probe=0 lean=1` in all three; F10 state.json `audio_filter
fallback=0 ch_fallback=0`. No probe arm ran, so there are no per-call filter stats, no engine real-time share
and no underrun counts in these logs. `MCLA_XMA` clean (carry_nonzero 0, stale ring replays 0, lock races 0).
The audio-engine thread F8000534 uses 68-72 % of a core in play (l96 afnative about 70 %). Indirectly at the
l98 level; not measured. The owner reported no sound problem.

## 8. Verdicts

(a) **l104-l116 defaults on the Deck: no anomaly observed** (no new warning class, no crash, exit 0, owner run
1 "fine", the VB fix active); but every mismatch counter is 0 only because verify was off: the verify arms
(cp-verify2, rt-memo2-verify) are still owed for a Deck verdict.
(b) **scene-1x: a Deck option, not yet a default.** Gain: -29 % draws, -13.6 ms CP, +3.7 fps, half the frames in
the 20 fps bucket. Cost: alpha-tested fences dither and flicker, aliased edges, and first-session pipeline
freezes of 1.5-2.9 s. Re-test needed: a second scene-1x session (warm cache: do the freezes vanish?) and the
scene-1x-fxaa arm.
(c) **Elision + extent under Wine: keep off by default for now.** GPU -2.6 ms per frame and no artefact seen,
but no fps gain while CP-bound and no Deck verify (verify-gpu, free roam only per KI-085). Revisit with
scene-1x, where GPU and CP are closer.
(d) **Next Deck lever:** shared-memory protection, measured. CP: upload region 13.4 ms per frame, of which arms
8.1 ms at 16.4 us per call (4-5x a private-page protect; the Wine dispatch is only 3.5 us). Render thread:
faults 6.5 ms per frame. (a2) projects -2.8 to -4.8 ms CP and -3.5 to -6 ms render thread. The render-thread
part matters most at 1x, where the render thread (46-48 ms per frame: guest about 41 ms plus faults) is the
floor. Before (a2): run the shm-census arm (hot R/U, `up_same`) and a probe through the SDK guest-section
path to find where 16 us per arm goes. CP per draw (9.4 us, 3.0x PC) and the guest render thread (about
41 ms, code generation) come after that.

## 9. Questions for the owner

1. Run 2: was the fence moment the second F10 (10:41:30, the fence in front of the building on the right)? Does
   that fence also shimmer in the default build at the same spot?
2. Run 2: were the 1-3 s freezes in the first minute after loading noticeable? One more short scene-1x session
   would show whether they are gone with a warm cache.
3. gpu-new: a race with the minimap (KI-084) and shadows was not driven; is a race run possible?
4. Audio: any crackle in the three runs?
