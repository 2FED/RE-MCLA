# M11-007e: l77 fence arms (steady-fence, base-fence) against the l75/l76/l77 flicker and clean runs (2026-11-07)

Read-only analysis by an Opus subagent on 2026-11-07 of the l77 steady-fence / base-fence sessions against 10 earlier runs; accepted by the primary agent.

Read-only analysis by an Opus subagent. Profile 7d6ff1dd..., Unlocked, Arcade "South Central Tour", 1920x1080
render target, RTX 5060 Laptop, one run per arm. Sources: session `mcla.log` (`MCLA dev override`, `guest vblank`,
`timer scale`, `frame pacing/min`, `gpu busy/min`, `MCLA_PERF_SPLIT`, `MCLA_READBACK` incl. the `fence_*` fields,
`MCLA_READBACK_SYNC` incl. `kind=fence_defer` and the `steady_*` fields, `MCLA_WRM`, `MCLA_VRAM`,
`MCLA_LIVERY_DIAG2 kind=minute`, `MCLA_GUEST_SAMPLE`, `MCLA_PURSUIT dt=`), package `state.json` (`timer_step`,
`paused`) and `frame.bmp`. Python isolated (-I); nothing built or launched.

Sessions: steady-fence 4303070d (exemption + `d3d12_fence_write_deferred=true`), base-fence b22ecb01 (fence only);
flicker references l76 steady 31689c39 / c959447b, l75 steady c8a6754d / 7352bf89, l77 steady-rawoff a6a6c36b;
clean references l75 base b632bc55, l77 cap36 d3939438, cap40 4b816478, cap44 fcc70742, base-cap36 05bd358d.

Window convention: `rel` = seconds after the race-start vblank switch (240 Hz, or 72/80/88 Hz in capped arms). Race
clock = rel - ~13 s (F10 frames: steady-fence clock 0:20.41 at rel +33, 2:45.06 at rel +179; base-fence 0:21.23 at
+33, 2:46.92 at +179). The flicker stretch (race clock 2:00-2:35, Exposition Park / Coliseum) is therefore rel
~+133..+168. PERF_SPLIT and READBACK windows restart at each minute line; mark (F10) lines peek since the last minute.

## 1. Fence counters

Per window (`MCLA_READBACK` fields; frames = guest frames in the window):

| arm | window (rel s) | frames | events | deferred | immediate | max frames late | ms max | flush on idle | early submits | queue max |
|---|---|---|---|---|---|---|---|---|---|---|
| steady-fence | -44..+16 (load/menus) | 2001 | 15428 | 13979 | 1449 | **2** | 29.4 | 5322 | 699 | 18 |
| steady-fence | +16..+76 | 2483 | 27470 | 27355 | 115 | 1 | 27.5 | 714 | 867 | 14 |
| steady-fence | +76..+136 | 2401 | 24554 | 24364 | 190 | 1 | **54.6** | 652 | 1049 | 12 |
| steady-fence | +136..+178 (stretch + finish) | 1620 | 18672 | 18546 | 126 | 1 | 28.2 | 496 | 752 | 14 |
| base-fence | -44..+16 | 1948 | 14664 | 13171 | 1493 | 1 | 20.7 | 4606 | 786 | 10 |
| base-fence | +34..+76 / +76..+136 (minutes) | 2213 / 2207 | 24292 / 22762 | 24094 / 22548 | 198 / 214 | 1 | 15.9 / 14.3 | 730 / 692 | 1449 / 1407 | 10 |
| base-fence | +136..+179 | 1428 | 16642 | 16466 | 176 | 1 | 12.7 | 530 | 1264 | 10 |

- About 11 fence events per frame in both arms; 99 % deferred in race windows (immediate = fence already complete at
  parse, 0.5-1 %). `fence_written` tracks `fence_deferred` within the queue depth; `fence_queue_now` was 4-6 at every
  line and 0 at the last mark (drained).
- Lateness: never more than 1 frame in a race window; 2 frames only in the loading minute (the doc's expected bound).
  `fence_deferred_ms_max` 23-28 ms in steady race windows (one frame at ~22-24 ms GPU time) with one 54.6 ms
  outlier in the +76..+136 minute (that minute's longest frame interval was 83 ms; no >100 ms frame); 13-16 ms in
  base-fence (CP-bound: the GPU is rarely far behind). Correction to the KI-074 ledger line: the steady-fence maximum
  was 54.6 ms, not 28 ms (28 ms is only the last mark's value).
- The `kind=fence_defer` parse/write pairs cover only the first 8 queued writes after enable (startup, frames 3-131,
  menus): 7 of 8 written in the same frame (`frames=0`), one 1 frame late (seq 0, the first fence after enable);
  latency 3 us - 3.7 ms (steady-fence), 0.1-2.0 ms (base-fence); `waiting=1` once. They give no race distribution;
  only the per-minute max is available for the race.
- Early submits (the CP ended a submission early so a queued fence could land): steady 0.35-0.46 per frame,
  base 0.64-0.89 per frame. Idle flushes 0.27-0.31 per frame in the race (thousands per minute only while loading,
  when the CP idles anyway).
- Queue: max 12-14 in steady race windows, 10 in base = about one frame of fences. No backlog, no growth, no stall:
  0 frames >100 ms in any race minute of either arm, the session exited normally.

## 2. Pacing

Race windows (PERF_SPLIT per frame; frame pacing per minute):

| arm | fps (race minutes) | p50 / p99 / max ms | >50 / >100 ms | CP busy ms/f | GPU busy ms/f | GPU % | subm/f | rb wait ms/f | render_wait ms/f | ring idle ms/f | game dt (PURSUIT median) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| **steady-fence** | 41.4 / 40.0 | 23.2-24.2 / 46.9-50.2 / 83-87 | 19-25 / 0 | 22.6-24.6 | 21.8-22.9 | 89-91 | **8.8-9.7** | **0.33-0.59** | 6.5-7.7 | **0.12-0.23** | 0.022 (n=11) |
| l76 steady 1 (flicker) | 40.0 / 40.8 | 23.6-24.1 / 50.3-52.9 / 99-117 | 27-32 / 0-4 | 22.9-26.1 | 22.5-25.3 | 92-94 | 6.2-6.6 | 1.5-2.4 | 5.8-7.4 | 0.00-0.01 | 0.026 |
| l76 steady 2 (flicker) | 39.7 / 39.9 | 24.3-24.4 / 50.3-53.7 / 96-97 | 25-29 / 0 | 24.0-25.8 | 23.1-25.6 | 92-95 | 6.1-6.9 | 1.8-2.4 | 5.4-7.7 | 0.00-0.03 | 0.025 |
| l75 steady 1/2 (flicker) | 41.1-42.1 | 21.9-22.2 / 53.4-61.2 / 89-98 | 34-47 / 0 | 22.2-23.9 | 22.0-25.2 | 90-96 | 6.0-7.4 | 0.1-0.8 | 4.7-6.0 | 0.00-0.01 | 0.026-0.027 |
| **base-fence** | 36.9 / 36.8 | 25.7-25.9 / 49.9-53.8 / 103-116 | 23-28 / 1 | 25.7-29.2 | 22.1-23.7 | 78-82 | 12.4-15.1 | 5.1-5.7 | 10.8-11.5 | 0.01-0.02 | 0.029 |
| l75 base | 33.0 / 35.9 | 26.5-29.2 / 55.5-55.7 / 104-137 | 28-34 / 1-3 | 26.8-29.6 | 22.4-23.2 | 75-80 | 10.9-13.0 | 6.0-6.7 | 10.5-11.1 | 0.01-0.02 | 0.030 |

Render-thread guest samples (`MCLA_GUEST_SAMPLE`, 10 s after the first race F10, share of samples in the frame
throttle `82412F98`; ring-space wait `82411E98` 0-1.5 % everywhere):

| steady-fence | l76 s1 / s2 | l75 s1 / s2 | rawoff | cap44 / cap40 / cap36 | base-fence | l75 base | base-cap36 |
|---|---|---|---|---|---|---|---|
| 27.4 % | 23.7 / 23.6 % | 24.0 / 21.9 % | 24.2 % | 30.1 / 30.8 / 33.7 % | 36.2 % | 33.9 % | 39.5 % |

Reading:
- No fps cost: steady-fence 40.0-41.9 fps per race window vs 39.7-40.8 (l76) and 40.9-42.9 (l75). The GPU is still
  the bottleneck (21.8-22.9 ms GPU per frame), so the CP lead was buffering, not throughput.
- Tail slightly better: p99 47-50 ms vs 50-61, max 83-87 ms vs 89-117, fewer >50 ms frames (19-25 vs 25-47).
- Where the render thread waits: still the frame throttle, not ring space; its share rose from 22-24 % to 27 %
  (toward the capped arms' 30-34 %): the throttle wait is now a real GPU wait. render_wait per frame is about the same
  (6.5-7.7 vs 5.4-7.7 ms). The CP now runs dry occasionally (ring idle 0.12-0.23 ms/f vs 0.00-0.03 in every other
  run), and the GPU idles a little more (89-91 % vs 92-95 %).
- Submissions per frame rose about 40 % (8.8-9.7 vs 6.1-6.9). Early submits explain only 0.35-0.46 of the +2.7 per
  frame; the rest is unexplained by the current counters (likely ends at idle/WRM/forced-wait points that now come
  more often). No fps cost on this PC; worth a per-reason submission counter before the Deck run.
- CP lead, measured: `steady_fast_waits` (an exempt resolve found the previous frame's slot copy not yet executed on
  the GPU and waited for it) is the one counter that sees the GPU running behind the CP by about a frame:

  | run | fast waits / exempt resolves (race windows) | mean / max wait ms | longest skip run | flicker |
  |---|---|---|---|---|
  | l76 s1 | 48 % / 45 % / 35 % (stretch) | 3.3-3.8 / 10.7 | 27-53 | yes |
  | l76 s2 | 41 % / 38 % / 48 % (stretch) | 3.5-3.9 / 13.5 | 49-84 | yes |
  | rawoff | 39 % / 35 % / 36 % (stretch) | 3.3-3.6 / 10.4 | 48-77 | yes |
  | cap44 | 0.4 % / 2.2 % / 3.7 % (stretch) | 2.4-3.1 / 8.5 | 5-10 | no |
  | cap40 / cap36 | 0-0.3 % / 0 % | - | 0-3 | no |
  | **steady-fence** | **14 % / 7.4 % / 5.2 % (stretch)** | **1.4-1.6 / 4.4** | **8-21** | **no** |

  (l75 used the older cadence path, so these fields do not exist there.) The deferred fences cut the GPU-behind-CP
  episodes by 4-8x and halved their depth; in the stretch they are at the cap44 level. The PERF_SPLIT read-back wait
  drop (0.3-0.6 vs 1.5-2.4 ms/f) is the same effect.
- Guest-visible dt: frame interval p50 23-24 ms in both steady-fence and l76 (PURSUIT median 0.022 with only 11
  state-change samples, `timer_step` 0.024 at the stretch F10), so the game ran the same ~24-25 ms dt as the flicker
  runs; dt remains excluded.
- Base-fence faster than l75 base (36.9 vs 34.5 average): in the same late window (+76..+136) it is 36.8 vs 35.9
  (+2.5 %); the larger gap is l75 base's slow first race minute (33.2 fps, p50 29 ms) against base-fence 37.4.
  Measured part: the CP's forced rule-c read-back wait is 0.4-1.0 ms/f shorter (5.4-5.7 vs 5.8-6.7 ms/f; CP busy
  25.7-26.2 vs 26.8-29.0) and GPU utilisation higher (78-82 vs 75-80 %): early submits (0.65/f) hand work to the GPU
  before the forced wait, so less is queued behind it. The rest is run variance and cross-build drift (l75 vs l77
  host/SDK); there is no l77 uncapped base without fence to separate them (base-cap36 is capped).

## 3. The flicker stretch

Closest comparable windows (PERF_SPLIT per frame; steady-fence's mark window also contains the last ~10 s and the
finish, which dilutes the burst):

| run | window rel s | fps | GPU % | GPU ms/f | subm/f | upload MB/f | upload calls/f | tex memo miss/f | ring idle ms/f | fast waits | fence late (max) | flicker |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| steady-fence | +136..+178 | 38.7 | 89 | 22.9 | 9.7 | 13.0 | 1535 | 2322 | 0.12 | 5.2 % | 1 frame / 28 ms | **no** |
| l75 steady 1 | +132..+171 | 40.1 | 93 | 23.2 | 7.1 | 13.7 | 1672 | 2357 | 0.01 | (old path) | - | yes |
| l76 steady 1 (press) | +132..+157 | 36.8 | 93 | 25.3 | 6.6 | 16.1 | 1987 | 2713 | 0.00 | 35 % | - | yes |
| l76 steady 2 | +136..+151 | 37.3 | 95 | 25.6 | 6.1 | 16.3 | 1967 | 2700 | 0.00 | 48 % | - | yes |
| l75 steady 2 | +136..+150 | 37.8 | 95 | 25.0 | 6.3 | 16.9 | 2039 | 2750 | 0.00 | (old path) | - | yes |
| cap44 | +136..+172 | 37.4 | 87 | 23.3 | 7.7 | 13.5 | 1625 | - | 0.01 | 3.7 % | - | no |
| base-fence | +136..+179 | 33.0 | 78 | 23.7 | 15.1 | 14.3 | 1720 | 2438 | 0.02 | n/a (no exemption) | 1 frame / 13 ms | no |

- Upload and texture churn per frame are unchanged by the fence (the l75 s1 window of the same length: 13.7 vs 13.0
  MB/f, 2357 vs 2322 memo misses); texture limits unchanged (`tex_soft_mb=2048 tex_hard_mb=5595`, no near-budget
  step; local 5.18-5.30 GB, budget min 6627 MB, same as the flicker runs). LIVERY_DIAG2 per frame (texture loads,
  invalidations, reloads without invalidation, sporadic resolves) also unchanged.
- GPU saturation alone does not separate clean from flicker any more: steady-fence ran at 89-91 % GPU (l77
  boundary was clean <=87 %, flicker >=90-91 %) and 40-41 fps, yet was clean.
- What is different: how far the GPU runs behind the CP. Every flicker run has the previous frame's resolve copy
  unexecuted at the next resolve in 35-48 % of frames (waits 3.5 ms average, up to 13 ms, runs of 27-84 consecutive
  frames); every clean run, including steady-fence and cap44 in this very stretch, is at 0-5 % (steady-fence: 1.4 ms
  average, 4.4 ms max). With the fence written only at GPU completion, the guest's frame throttle holds the render
  thread until the GPU has finished, so the CP lead is bounded to under one frame (fence lateness max 1 frame) instead
  of 1-2 frames.
- Candidate mechanism (not proven): a host path that is ordered by CP parse time but consumes data the GPU produces
  later. The read-back delivery of the exempt key (slot N-1 copied into guest RAM at resolve N, RE doc section 3) and
  any texture reload from guest RAM over a range the GPU resolve has not yet written are the obvious ones: with the
  GPU a frame or more behind, the delivered or reloaded data is one frame older than the GPU-side content, and a
  surface sampled from it can show the previous state (flicker on buildings/billboards/signs during the stretch's
  upload burst). Bounding the CP lead closes that window without changing the read-back path. The 4-8x drop in
  fast waits is the measured correlate; the RE step 2 follow-up (deliver pending read-back slots whose submission is
  complete before storing the fence) would make it exact.

## 4. Results-screen F10 frames

- steady-fence #2 (rel +178.8, race clock 2:45.06, 60 ms before the slow-motion ramp `timer scale 0.66 -> 0.1`):
  player car, the opponent's red lowrider alongside, pedestrians, empty street ahead, only distant vehicles.
  #3 (rel +182, `paused=1`, after `menu state: game paused`): REWARDS panel on East Adams, opponent car, no traffic.
- base-fence #3 (rel +179.1, clock 2:46.92, 30 ms before the ramp): same composition (same billboard/building/
  lights), empty street, no traffic. Both match the cap40/rawoff/base-cap36/l76 ensure2 finish frames: the normal
  finish/rewards composition. Earlier F10s (#1 steady-fence clock 0:20.41, base-fence #1/#2 clock ~0:21) are routine
  race frames with traffic and no visible defect.

## 5. Conclusion

- "Deferred fences fix the flicker under GPU-saturated pacing" is consistent with the data: the first clean uncapped
  run at 40-41 fps and 89-91 % GPU after 7 of 7 flickering ones, at the same dt, uploads, texture churn and budget,
  with the one measurable change being a bounded CP lead (fast waits 35-48 % -> 5-14 %, readback wait 1.5-2.4 ->
  0.3-0.6 ms/f), at no fps cost and with a slightly better tail. It also refines the l77 reading: the flicker
  follows the GPU running a frame or more behind the CP, not GPU utilisation itself.
- Risks: (1) one run; the flicker hit 1-5 times per run, so a single clean run is suggestive (p of a clean run by
  chance is small but not negligible). (2) Stall/deadlock exposure is only ~7 min of runtime (two races, menus,
  loading); loading/menus exercised the idle path heavily (5322 idle flushes, queue max 18, 2 frames late) without
  a hang, but garage, photo mode, focus loss, replay, a cold shader cache (pipeline creation blocks early submits)
  and occlusion-query spans (2 ms grace path) are untested. (3) Submissions +40 % per frame: free on this PC, but a
  CPU cost under vkd3d on the CPU-bound Deck. (4) Deck GPU is slower: deeper fence waits there could cost fps.
  (5) The resolve-delivery race (fence before the CP's memcpy of the slot) remains by design.
- Confirm in the two extra steady-fence runs (same route/save): no flicker; race-minute `steady_fast_waits` below
  ~15 % of exempt resolves (stretch window ~5 %), `fence_deferred_max_frames` <= 1 in the race, ms_max under
  ~60 ms, 0 frames >100 ms, fps 40-41; an F10 inside the Coliseum stretch (race clock 2:00-2:35) for a same-window
  counter comparison, and one F10 just before and after the finish. If either flickers, compare its fast-wait share
  in that window. Optional same-session control: hot-toggle `d3d12_fence_write_deferred` off at the stretch (the cvar
  is hot-reload), expecting the fast-wait share to jump back to 35-48 %.
- Candidate defaults (if both runs are clean): Unlocked = steady exemption on + `d3d12_fence_write_deferred=true`;
  base/Locked = fence on as well (base-fence clean and faster; one switch for both modes), KI-071 walk fix default
  kept. Keep a dev key to turn the fence off.
- Regression checks before making it default: KI-047 police-livery routine with fence + exemption (no garbage
  livery; the fence now lands at GPU completion, the read-back delivery is not tied to it); KI-035 garage: two or
  three garage trips after races, finish-warp hook lines present, no freeze, fence counters in garage minutes
  (queue max, max frames, early submits); one Locked 30 fps run (no regression, pacing unchanged); Deck loop under
  Proton (D3D12 via vkd3d): fps vs the current Deck baseline, CPU of the GPU-commands thread, KI-060 audio underruns,
  fence counters and no hang; one cold-shader-cache start and a photo-mode/album visit for the pipeline-creation and
  occlusion grace paths.
- Instrumentation worth adding to the next SDK: per-minute fence lateness histogram (0/1/2+ frames; <5/5-20/20-50/
  >50 ms), a CP-lead gauge (submission_current - completed at each swap), and submissions by end reason.

## Spark

No Spark job: the extraction is deterministic log parsing over a dozen sessions with exact field names; validating a
model's summary would cost more than the parse.
