# M11-007b: steady-key exemption + fast delivery A/B on l75 (PC, 2026-11-07)

Read-only analysis by an Opus subagent on 2026-11-07 of the l75 steady A/B (two steady runs with F10 at flickers, one base; HWiNFO + proc-sampler); accepted by the primary agent.

Read-only analysis by an Opus subagent of the owner l75 steady A/B (SDK 4bb8c02 steady narrowing + fast
delivery). Profile 7d6ff1dd..., Arcade "South Central Tour", 1080p render target, Unlocked, F10 packages,
HWiNFO + proc-sampler of 2026-11-07 (14:11-14:25). Parsed with an isolated Python outside the source folders.
Sources: session `mcla.log` (READBACK_SYNC minute/mark, PERF_SPLIT, frame pacing, LIVERY_DIAG2 composite lines
(paced sample, ~1 per 4-5 frames) and the F10 flight rings (`DIAG2_FLIGHT ring=resolve`, every resolve of the
last ~37-42 frames = ~0.9-1.2 s before each F10)), package state.json/manifest.json/report.txt/log-tail.txt/
frame.bmp. SDK source read: `command_processor.cpp` / `readback_slots.h` at 4bb8c02 (fast-delivery planner).

| arm | session | race window (game running -> finish pause) | F10 (s after race start) |
|---|---|---|---|
| steady 1 | c8a6754d... | 14:12:19.6-14:15:18.5 (179 s) | 14:12:53.0 (+33.4, checkpoint), 14:15:10.7 (+171.1, flicker) |
| steady 2 | 7352bf89... | 14:16:21.8-14:19:19.6 (178 s) | 14:16:56.5 (+34.7), 14:18:32.1 (+130.3), 14:18:51.5 (+149.8), 14:18:53.7 (+152.0), 14:18:59.6 (+157.9), 14:19:06.1 (+164.4) |
| base | b632bc55... | 14:20:36.9-14:23:34.1 (177 s) | 14:21:11.1 (+34.2, checkpoint) |

Note: the first steady-2 F10 (+34.7 s) is at the same spot/time as the routine checkpoint F10 of the other two
runs (+33.4, +34.2); it is treated as the checkpoint here, so run 2 has 5 flicker F10s (owner to confirm).
All flicker F10s again fall in +130..+172 s, the same race stretch as the four l74 flicker F10s (+131..+172).

## 1. Counters per window (READBACK_SYNC minute/mark, differenced; PERF_SPLIT per window)

`steady_exempt_keys=1` (039CA000 256x256 fmt 6) in every window; the only `steady_key` line is `event=exempt` at
frame 721 (menu) in both steady runs; no `kind=steady_cap` line; `steady_sticky_excluded = steady_shape_excluded =
steady_cap_skipped = 0`; `forced_wait_failed=0`, `fallbacks_to_full=0`. `steady_fast_kb` = 256 KB per copy.
steady_exempt = 1.00 per frame in the race (one exempt resolve per frame); fast + skipped = exempt exactly.

| arm | window (end) | frames | exempt | fast copies | skipped | fast % | forced (b/c) | RS wait ms/fr | fps | CP busy ms/fr | rb wait ms/fr | GPU busy ms/fr |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| s1 | min 14:12:32 (mostly pre-race) | 1967 | 1247 | 781 | 466 | 63 | 455 (40/423) | 0.40 | 32.8 | 12.97 | 0.63 | 12.56 |
| s1 | F10 14:12:53 | 866 | 866 | 456 | 410 | 53 | 1 (1/0) | 0.04 | 41.5 | 23.05 | 0.13 | 22.04 |
| s1 | min 14:13:32 | 1602 | 1602 | 891 | 711 | 56 | 36 (24/14) | 0.22 | 40.9 | 23.40 | 0.50 | 22.10 |
| s1 | min 14:14:32 | 2496 | 2496 | 1402 | 1094 | 56 | 50 (34/17) | 0.29 | 41.6 | 23.00 | 0.68 | 21.95 |
| s1 | F10 14:15:10 | 1545 | 1545 | 946 | 599 | 61 | 33 (28/6) | 0.32 | 40.1 | 23.92 | 0.77 | 23.15 |
| s2 | min 14:16:37 (mostly pre-race) | 2057 | 1337 | 685 | 652 | 51 | 364 (77/303) | 0.56 | 34.3 | 12.17 | 0.83 | 12.76 |
| s2 | F10 14:16:56 | 815 | 815 | 247 | 568 | 30 | 1 (1/0) | 0.06 | 42.9 | 22.24 | 0.29 | 22.30 |
| s2 | min 14:17:37 | 1710 | 1710 | 920 | 790 | 54 | 65 (38/32) | 0.41 | 41.7 | 22.95 | 0.79 | 22.58 |
| s2 | F10 14:18:32 | 2314 | 2314 | 1069 | 1245 | 46 | 27 (25/2) | 0.22 | 42.4 | 22.50 | 0.59 | 22.27 |
| s2 | min 14:18:37 | 202 | 202 | 165 | 37 | 82 | 0 | 0 | 37.2 | 25.75 | 0.35 | 25.20 |
| s2 | F10 14:18:51 | 530 | 530 | 257 | 273 | 48 | 8 (8/0) | 0.38 | 37.8 | 25.38 | 0.60 | 24.99 |
| s2 | F10 14:18:53 | 92 | 92 | 30 | 62 | 33 | 0 | 0 | 42.0 | 22.78 | 0.00 | 22.36 |
| s2 | F10 14:18:59 | 249 | 249 | 114 | 135 | 46 | 3 (3/0) | 0.13 | 42.2 | 22.51 | 0.54 | 22.16 |
| s2 | F10 14:19:06 | 252 | 252 | 184 | 68 | 73 | 11 (11/0) | 0.44 | 38.9 | 24.45 | 0.73 | 22.63 |
| base | min 14:20:50 (mostly pre-race) | 1950 | - | - | - | - | 1596 (75/1533) | 3.33 | 32.5 | 13.78 | 3.44 | 12.39 |
| base | F10 14:21:11 | 659 | - | - | - | - | 663 (4/659) | 6.00 | 32.7 | 29.57 | 6.02 | 23.18 |
| base | min 14:21:50 | 1322 | - | - | - | - | 1374 (28/1347) | 6.03 | 33.2 | 29.04 | 6.09 | 22.53 |
| base | min 14:22:51 | 2154 | - | - | - | - | 2202 (42/2162) | 6.59 | 35.9 | 26.83 | 6.65 | 22.38 |

(The READBACK window is not reset by every F10 mark; consecutive marks with the same start frame were differenced.)

Race totals (windows after the race start): steady 1: 6509 exempt, **3695 fast (56.8 %) / 2814 skipped (43.2 %)**;
steady 2: 6164 exempt, **2986 fast (48.4 %) / 3178 skipped (51.6 %)**. Forced in race: 120 / 115 (rule b 87 / 86,
rule c 37 / 34: the livery chain composite `05368000` 512x512 and `05B59000`/`05B99000` 256x256, i.e. still forced
as intended) vs base 4239 (4168 rule c = 039CA000 every frame).

### Fast vs fall-back, and why the previous slot is so often not complete

- The fast path needs the previous resolve's slot (frame N-1, draw 34) to have passed its submission fence by
  draw 34 of frame N, i.e. the GPU must be less than ~1 frame behind the CP. Polled, never waited.
- The flight rings show the state is **bimodal and sticky**: either every frame is fast (`F` x 36-41), or the
  previous slot is never complete and the key falls into the pure `some` cadence `...4...4...4` for the whole
  episode (pkc=0 on every frame, including the 4th). There is almost no `F.F.` alternation. Skip episodes in the
  paced sample: 133 / 119 per race, median a few frames, longest 178 / 270 frames.
- Cause: the GPU is the bottleneck (93-94 % busy, see 4); once a GPU hiccup lets the CP get >1 frame ahead the
  queue never drains (the CP produces as fast as the GPU consumes), so the N-1 copy is still queued at N. A forced
  rule-b wait drains it and fast delivery resumes at once (steady 2 ring at 14:18:32: rule-b wait `03238000`
  36.1 ms at frame 6870 -> skip episode 6860-6870 ends, `F` from 6871). `prev_age` (submissions between the two
  slots) is 5 in skip rings vs a median 7-11 in fast rings.
- Delivered age (model from the planner code, verified against the ring sequences):
  - fast: guest RAM = frame N-1 resolve, **1 frame** old (~24 ms).
  - skipped: deferred frames keep the last copy; the every-4th copy reads the previous slot **whose GPU copy is
    still in flight** (pkc=0; `some` memcpy from the mapped read-back buffer without a wait), so it delivers the
    slot's older content (the N-3 resolve) or, if the GPU copy runs during the memcpy, a **torn mix**. Age cycles
    **3, 4, 5, 6 frames** (avg ~4.5 = ~110 ms), not 1-4 as assumed in the M11-007a plan. Same path existed in l74.
  - Ring age histograms (frames after each resolve): steady 1 {1: 66, 2-6: 7}; steady 2 {1: 106, 2: 4, 3: 30,
    4: 29, 5: 24, 6: 22}; base {0: 38}.

## 2. Flicker timing

Per-frame data exist only in the F10 flight ring (last ~0.9-1.2 s); before that only the paced sample and the
per-window counters above (no per-second fast/skip counters).

| F10 | ring frames (s before F10) | skip episodes in ring (s before F10) | longest sampled frame (-2..0 s) | forced in ring | other |
|---|---|---|---|---|---|
| s1 +33.4 (checkpoint) | 2792-2833 (-1.18..0) | none (all fast) | 50 ms (-0.39) | - | - |
| s1 +171.1 (flicker) | 8437-8476 (-1.0..0) | **8451-8458 (-0.69..-0.52)**, ages 2-6 | 63 ms (-1.72), 51 ms (-0.59, inside the skip episode) | rule b `032CC000` 128x128, 6.1 ms (frame 8464) | - |
| s2 +34.7 (checkpoint) | 2834-2872 | none | 36 ms | - | - |
| s2 +130.3 | 6860-6896 (-0.92..0) | 6860-6871 (-0.92..-0.64), 6892-6896 (-0.11..0) | 37.5 ms | rule b `03238000` 128x128, **36.1 ms** (frame 6870) | - |
| s2 +149.8 | 7590-7628 (-0.91..0) | 7594-7597, 7606-7615, 7620-7628 | 36.8 ms | - | - |
| s2 +152.0 | 7679-7720 (-0.94..0) | **whole ring** (ages 3-6) | 44.7 ms (-1.38) | - | - |
| s2 +157.9 | 7932-7969 (-0.98..0) | **none (all fast, age 1)**; the 249-frame window before it was 54 % skipped | 34 ms | rule b `031E8000` 128x128, 5.3 ms (frame 7960) | - |
| s2 +164.4 | 8184-8221 (-0.87..0) | **whole ring** | 44 ms (-0.62) | - | one `kind=overlap` 0717D000 (round-robin family) |

- No `steady_key`/`steady_cap` event, no rule a, no atlas load (only the four menu-time atlas loads replayed in
  every flight ring) in any flicker window. Texture-cache lines in the 2.5 s before each F10 are the usual
  960x540 / 128x128 / 256x256 fmt 32 / 1280x1280 fmt 22 traffic (invals by GPU, frames_since_load 0-2), the same
  mix as at the checkpoint F10s and in base; `039CA000` tex_load shows `rb_copy_frames_ago` 2-3 in skip windows.
- Coincidence with skipped delivery: 5 of 6 flicker rings contain skip frames, 2 of 6 are entirely skipped, 1
  (+157.9) is entirely fast; both checkpoint rings are all fast. **Base rate**: a random 0.9 s window in the race
  contains a skip frame 74 % (s1) / 82 % (s2) of the time, is >= half skipped 46 / 56 %, all skipped 11 / 20 %.
  So 5/6 is what chance predicts; the data do not show that flickers need skipped delivery (n=6, owner reaction
  time unknown; one flicker ring was fast for its full 0.98 s).
- Run 1 vs run 2: run 2 skipped more (51.6 vs 43.2 % of race frames; 0.9-s windows with any skip 82 vs 74 %) and
  its GPU was busier (PERF_SPLIT GPU busy 94.2 vs 91.5 % of the frame; HWiNFO NVIDIA load 93.2 vs 89.9 %,
  power-limiter samples 70 vs 50 %). In the flicker stretch (+130..+175 s) the 5-s skip fractions are, however,
  similar (s1 0.12-0.69, s2 0.23-0.70). Consistent with "more GPU load -> more skip -> more flicker", but weak.
- Against the staleness explanation: l74 (no fast delivery, the cadence always applied, ages up to 6) had 2
  flicker F10s per steady run; l75 halved the stale share (57 / 48 % of frames at age 1) and gave 1 and 5.

## 3. F10 frames

All 9 frame.bmp: `guest-output-readback`, `presented-guest-output`, 1920x1080, nonuniform. Flicker frames:
s1 +171 (race clock 2:38.24, Alameda St., warehouses/houses), s2 1:56.15 Century Blvd. (pole sign column
"FURNITURE / ... / MARKET / TAILORS", warehouse), 2:15.68 Sebastian/Exposition (museum, Coliseum, office
blocks), 2:17.84 (Exposition Park: EXP banner, Coliseum, brick buildings with billboard), 2:23.71 Coliseum Dr.
("TRUTH'S" billboard, PARKING sign, Coliseum), 2:30.25 (110 Freeway North/South gantry sign, car billboard,
"Hustler" billboard). 1:1 crops show normal textures on all buildings, billboards and signs; no placeholder band,
no missing object. The 2:17.84 frame shows the car on a concrete strip beside a wide yellow-green lawn with a
blank HUD street label (off-road cut through the park, geometry appears genuine, not a texture swap). The base F10
is only at the checkpoint (+34 s, race clock 0:20), so no base frame at the flicker spots exists.
**No frame caught a flickered object** (transient, as in l74).

## 4. Validity and performance (race windows)

- Foreign load: run 1 had a leftover single-thread `cmake`/`clang` (~6 % of total CPU = 1 core) from 14:12:20 to
  14:12:48 (first ~29 s of the race), gone afterwards (its flicker F10 is at +171 s). Runs 2 and base: no build
  process. other_cpu avg 12.5 / 10.3 / 16.0 % (base max 31 % = MsMpEng 18 %, Defender).
- Power/thermal comparable: APU STAPM 35.7 / 34.8 / 35.6 W (limit 97.9 / 95.9 / 97.7 %), Tctl 90.0 / 89.5 / 89.9 C
  (max 96 / 94 / 93), HTC / PROCHOT / PROCHOT EXT = No in all samples, effective clock 1850 / 1654 / 1810 MHz,
  on AC (charge 100 %, 0 W). NVIDIA 66.1 / 66.4 / 61.4 W, 72 / 72 / 71 C.

| metric | steady 1 | steady 2 | base |
|---|---|---|---|
| fps (PERF_SPLIT, race windows) | 41.0 | 41.5 | 34.5 |
| fps (pacing minutes in race) | 41.1 / 41.6 | 42.1 / 41.9 | 33.0 / 35.9 |
| HWiNFO PresentMon fps | 43.9 | 46.1 | 36.2 |
| p50 / p99 interval ms | 22.2-21.9 / 53-61 | 22.1-22.0 / 57-61 | 29.2-26.5 / 55-56 |
| frames >50 ms per race minute | 34, 47 | 34, 41 | 28, 34 |
| CP busy ms/frame | 23.32 | 23.03 | 27.97 |
| read-back wait ms/frame | 0.58 | 0.59 | 6.37 |
| RS forced wait ms/frame | 0.25 | 0.26 | 6.32 |
| fence wait ms/frame | 0.96 | 1.25 | 6.13 |
| GPU busy ms/frame (% of frame) | 22.28 (91.5 %) | 22.70 (94.2 %) | 22.56 (77.7 %) |
| HWiNFO NVIDIA load / power-limiter samples | 89.9 % / 50 % | 93.2 % / 70 % | 78.3 % / 30 % |
| submissions/frame | 7.16 | 6.50 | 11.75 |
| readback memcpy ms/frame (all keys) | 0.68 | 0.60 | 0.67 |

The gain is intact (+6.8 fps, +20 %; read-back wait -5.8 ms) and fast delivery costs nothing measurable.

## 5. Conclusion and recommendation

- The data **do not discriminate** between the two hypotheses. Skipped delivery (ages 3-6, plus possibly torn
  4th-copies) is present in ~45-50 % of race frames and in ~75-80 % of 1-s windows, so finding it before most
  flicker presses is expected by chance; one flicker press had 1 s of pure age-1 delivery before it; halving the
  stale share vs l74 did not reduce the flicker count. Flickers are rare, location-bound (+130..+172 s in all 6
  steady runs over l74/l75, never in base) and somewhat more frequent with more GPU load/skip (run 2).
- Two weak signals lean towards "needs same-frame data" (or a frame-rate effect): no reduction from l74 to l75,
  and the all-fast flicker ring. One signal leans towards "stale >1": run 1's flicker ring has a skip episode with
  a 51 ms frame ~0.6 s before the press, and the run with more skip had more flickers.
- **Recommended next step (one decisive owner A/B, cheap to build):** make delivery deterministic at <= 1 frame
  and never read an in-flight slot: for a steady-exempt resolve whose previous slot is not complete, **wait for
  the previous slot's submission fence** (targeted, bounded e.g. 20 ms, fall back to forcing the own slot), and
  drop the unfenced 4th-copy path for exempt keys. Expected cost is small: the wait is for the N-1 draw-34 copy, so
  the GPU still has the rest of frame N-1 queued (no drain/starvation, unlike the base own-slot wait that costs
  ~6 ms); it just caps the CP lead at ~1 frame while the GPU is the bottleneck anyway. New counters:
  `steady_prev_waits`, `steady_prev_wait_ms`, `steady_prev_wait_max_ms`, timeouts.
  - Flicker gone over 2-3 steady runs on the same stretch (+130..+172 s; expected 1-5 per run otherwise) ->
    staleness >1 frame / torn copies were the cause; ship exemption + bounded previous-slot wait.
  - Flicker persists with every frame age 1 -> the CPU reader needs same-frame data: keep forcing `039CA000`
    (base behaviour) or move to the M11-007 step-2 read trap; close M11-007a as not shippable.
  - Optional control in the same session: a base run with a 41-fps-equivalent load is not possible; instead keep
    the blind base/steady order the l74 plan proposed to exclude a frame-rate effect.
- Independent of the flicker: the `some` cadence copy from an unfenced slot (pkc=0) can hand the guest a torn
  256 KB buffer; it predates l75 but should be closed for exempt keys by the change above.
- Diagnostics: lengthen the F10 flight ring for `039CA000` to ~120 frames (3 s) so the press reaction time is
  covered, and add per-window fast/skip episode counts (number, longest) to the minute line.
