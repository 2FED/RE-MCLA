# M11 KI-082 traffic probe analysis (l107 `pc/traffic-probe`, 2026-11-17)

Read-only analysis by an Opus subagent, 2026-11-17. No builds, no tests, no game launches, no repo edits.
Spark not used (one session's logs; a deterministic Python pass over about 1,000 lines was cheaper than
writing a source packet and checking the answer).

KI-082 is "traffic cars tremble when they are close to the player, while driving and while standing
still". Probe design: `docs/evidence/M11-l107-guest-tiling-and-msaa-20261117.md` s.5. Code:
`src/mcla_traffic_motion.{h,cpp}`. Compare KI-054 (traffic and camera jumps, fixed by hitch hold v2).

## 0. Inputs and identity

- Session `fc3467b88bf52e56c5a6ec78ca5b1b4c`, managed profile 68e5. It was the newest in the profile and
  ran 16:50:05 to 16:51:59 local time. Copied, with `save-private`, `before-user`, `after-user`,
  `save-files-private.json` and `*.dmp` excluded, to `private/owner-l107-20261117/` (`session/`,
  `diagnostics/`).
- Build `F0378B8D4A0E1172A0E13C04A4C4C6E9F5750CA866A94D9A75A6444BBA18B5CD` (launch receipt, F10
  report, cache root). Timing was Unlocked, RTX 5060 Laptop, scale 2, render target 1920x1080, guest
  960x540, vsync 1. The output was 3840x2160 at 120 Hz with `fs_vrr=0`, and presentation tearing was
  disabled. The low-latency backend was Reflex (`mode=auto`).
- `MCLA dev override: traffic_motion_probe=true (mcla_traffic_motion_probe=true)` was applied (log line
  10), with the l107 arm's other 12 values.
- The owner made four F10 packages, not two: `live-20261117T145116Z-25080-1` .. `-4` at 16:51:16, :25,
  :36 and :53. Gameplay started at about 16:50:43 (menu state "game running"). It was night on Sunset
  Blvd at about 29 mph in the HUD. In the frame of package 2 a traffic car stands in the next lane about
  2-3 m from the player car.

## 1. Probe health: it did not see a single car

| item | value |
|---|---|
| `kind=config` | once, 16:50:21 (`cars=8 burst_s=5 minute_s=60`) |
| `kind=minute` (16:51:21) | `updates=2363 ... samples=0 new_cars=0 ... calls=0 bad_matrix=0 paths=0/0/0` |
| `kind=burst` | 4 (seq 1-4, upd 2088 / 2614 / 3215 / 4084) |
| `kind=frame` | 999 lines, every one `cars=0 calls=0` with no car fields |
| F10 `traffic_motion` | `samples 0, bad_matrix 0` in all four packages (updates 2088 / 2614 / 3215 / 4085) |

- **The hook `sub_826FA0F8` recorded zero calls in the whole session**, although a traffic car stood
  beside the player in at least one package. No car was tracked, so `bad_matrix` and the three
  matrix-path counts are all 0. We do not know which matrix path a car uses.
- The probe counts a call (`m.calls`) only inside `Tracker::OnCar`. `Observe()` returns silently
  before that when the collider, instance, matrix pointer or buffer index (> 16) is implausible. So
  "0 calls" can mean either of two things: the function never ran through our override, or it ran
  but every call was dropped early. The log cannot tell these apart, which is a probe design gap.
- The update boundary is healthy. There is one timer update per presented guest frame: between
  packages 1 and 2 `updates` rose by 526 and `guest_output_sequence` by 527. The minute line shows
  host 25.40 ms mean, sd 10.37, max 389 ms; step dt 16.00 / 25.47 / 43.00 ms (min / mean / max); and
  `dt_jumps=2`. That minute includes loading and the menus.

Why 0 calls is plausible (static facts, not verified at runtime):

1. `sub_826FA0F8` is mcAmbientCollider vfunc +56, reached only through the function table. Its loop
   runs per wheel: `r18` = wheel count from `sub_826E5540`, with one `sub_826FBA60` suspension call
   per wheel at lr `0x826FA378`.
   - In the owner's l83 runs, the direct-call hook on `sub_826FBA60` (site B of the KI-054 ambient
     probe) counted 2,100 and 26,164 site-B reads. That is the same order as the number of updates,
     so roughly 0.2-0.4 cars per update at 4 wheels each.
   - So this collider is live in retail, but it runs for few cars. Plausibly it covers only ambient
     cars in physics mode (after contact, spawn/settle, or pose re-validation), not every
     path-following car near the player.
   - KI-074 RE s.2: "A missed probe ... leaves the car where its path puts it". Cruising and
     queueing traffic is most likely placed kinematically along its path. The l107 probe does not
     hook that code.
2. The alternative explanation is that the vtable dispatch does not reach our strong override. The
   weak/strong scheme (`DEFINE_REX_FUNC` weak alias; `SetFunction(0x826FA0F8, sub_826FA0F8)` and
   `PPCFuncMappings` resolve the symbol) should send indirect calls to the override, and
   `tests/mcla_traffic_motion_test.cpp` checks that registration statically. However, no other
   project hook on a function reached only through the function table has a runtime proof I could
   find. This is the less likely explanation, but one run can settle it (s.4, step 1).

## 2. What the bursts do show: game step against host interval (no per-car data)

Per-burst statistics. Host = interval between two main-timer ticks on the game thread; step = the
game's `timer+8` (whole milliseconds, raw timer with smoother and hitch hold v2).

| burst | frames | step mean / sd (ms) | host mean / sd (ms) | host p50 / p95 / max (ms) | abs(host-step)/host p50 / p90 / p99 | sign flips > 5 % | lag-1 autocorr of host |
|---|---|---|---|---|---|---|---|
| 1 | 257 | 19.69 / 2.46 | 19.56 / 3.84 | 18.5 / 24.8 / 62.6 | 4.0 / 15.1 / 35.1 % | 7 | 0.56 |
| 2 | 287 | 17.53 / 0.91 | 17.47 / 0.99 | 17.7 / 18.7 / 20.5 | 2.8 / 8.9 / 16.1 % | 21 | 0.54 |
| 3 | 267 | 18.80 / 1.96 | 18.76 / 1.91 | 19.5 / 20.6 / 25.1 | 3.3 / 9.3 / 18.7 % | 17 | 0.85 |
| 4 | 188 | 24.82 / 7.54 | 26.49 / 29.26 | 20.5 / 34.7 / 416.3 | 4.7 / 15.3 / 29.1 % | 9 | 0.06 |

- The frame rate was 50-57 fps during the presses (burst 4 has one 416 ms hitch, later a DIP to 23-30
  fps at 16:51:56).
- The step follows the host interval closely. The cumulative drift (sum of host-step) stays within
  -16..+21 ms in burst 2 and -13..+35 ms in burst 3; the smoother repays it.
- The step is quantized to whole ms (16/17 or 15/16 alternating, for example burst 2:
  `17 17 16 17 17 17 17 16 ...`). Per frame this is a speed error of up to 1/17 = 6 %, about 1 cm per
  frame at 10 m/s relative speed.
- There is no alternating pattern of the RenanQueiroz kind (12/22 ms). Sign flips above 5 % happen in
  only 2-8 % of frames, and the host intervals are positively autocorrelated (slow drift, not a
  zig-zag).
- There is no periodic pattern in the step. The game itself runs one variable step per frame (KI-074
  RE). This probe could not test a lower internal rate for traffic (for example a 30 Hz AI or path
  update shown at 55 fps), because no car was tracked.
- What the probe cannot see: the real presentation time. The output is 120 Hz with no VRR and
  tearing disabled, so frames that took 17-19 ms on the game thread are shown for 16.7 or 25 ms (the
  8.33 ms grid). That presentation judder applies to everything that moves relative to the camera.
  It is not specific to traffic.

Amplitude scale, an estimate with an assumed horizontal FOV of about 70 degrees (not measured): at a
1920-pixel render width the focal length is about 1,370 px. 1 cm at 7 m is therefore about 2 px in
the render target and about 4 px at the 3840 output; 1 cm at 5 m is about 2.7 / 5.5 px.

Timing-only mismatch, worked through for a traffic car passing a standing player at 10 m/s, 7 m away:

| percentile | mismatch | position error | on-screen error |
|---|---|---|---|
| p50 | 3 % of 17.5 ms = 0.5 ms | 5 mm | about 1 px render |
| p90 | 1.6 ms | 1.6 cm | about 3 px render / 6 px output |

A stationary car next to a stationary player has no relative motion, so no timing mechanism can make
it tremble. If the owner saw stationary cars tremble, the cause is a pose or render effect, not dt.

## 3. Hypotheses against the evidence

| # | hypothesis | status |
|---|---|---|
| a | traffic simulated at a fixed lower rate and drawn without interpolation | **Not testable from this run** (no car samples). No game-level fixed-step accumulator exists (KI-074 RE), but a path-follower or AI update could still run at its own rate. Open. |
| b | traffic uses a different dt from the player and camera | The KI-054 split-dt race is refuted for retail (l84: the ambient worker is never kicked; the step runs inline on the main thread with the same `timer+8`). The collider's site B reads `timer+8` like the player. Racers re-split the timer only for racers (pursuits and races), not for free-roam traffic. **Unlikely** in free roam. |
| c | double-buffered pose read from a stale or mixed slot | **Still plausible; the probe cannot see it.** New static fact: the global `0x827E5640` is the *render-thread* index of a two-slot buffer whose *update-thread* index is `0x8288B9F0`. Both are flipped `(i + 1) & 1` in `sub_82305B38`, called from the frame hand-off `sub_82305D00` (the same pair as the time-of-day render copies, `docs/evidence/M11-time-of-day-re-20261112.md`). The TLS byte `[[r13]] + 72` in `sub_826FA0F8` selects `inst + (render index + 4) * 64`, the render copy of the instance matrix. Under our threading the render worker draws frame F while the main thread prepares F+1 (KI-074 root-cause notes). If any traffic pose is read live (`inst + 128` or the path-follower's own transform) while the camera comes from the buffered slot, a moving car is drawn up to one step off its camera, which is v_rel x dt (10 m/s x 17 ms = 17 cm, 30-35 px render at 7 m). That is far above any timing mismatch, and it would hit large (near) cars hardest. |
| d | float precision far from the origin | **Unlikely.** LA world coordinates are within a few km; float32 resolution at 4 km is about 0.5 mm. |
| e | render side (MSAA resolve, tile seam, LOD, shadows) | **Open.** The KI-083 two-tile seam (`tiles=2 msaa=4` in every package) is a fixed screen line, not a per-car motion. A per-pixel MSAA resolve does not move geometry. LOD or shadow-cascade switching flickers rather than trembles. Neither is ruled out without a capture. |

Most likely cause, with honest confidence:

- About 45 %: (c) or a related pose/camera mismatch between the update and render threads. It fits
  "close to the player" (screen size), fits "while driving", and fits standing still with traffic
  moving past. It does not explain trembling of fully stationary cars.
- About 25 %: presentation judder (step against the 120 Hz grid, no VRR) that the owner notices most
  on near cars.
- The remainder: (a) or (e).

The run gives no data for any per-car hypothesis, because the hook saw no cars.

## 4. Next steps

### Step 1, no build (arm-file edits only; keys exist in l107)

Arm `pc/traffic-probe2`: `traffic_motion_probe=true` plus `ambient_dt_probe=true`. The latter's
`inline_reads` b-count is a direct-call hook on `sub_826FBA60`, proven live in l83.

Owner, about 2 minutes:
1. Stand next to queued traffic for 30 s and press F10.
2. Then gently bump one traffic car, wait 10 s and press F10.

Reading the result:

| site B reads | TRAFMOT calls | meaning |
|---|---|---|
| > 0 | 0 | The override is not reached through the function table (wiring bug). |
| 0 at the queue, rising after the bump | appear after the bump | The collider covers only physics-mode cars; cruising traffic is kinematic and the probe must move. |

### Step 2, cheap owner A/Bs today (same l107 build, same spot, same traffic, about 1 min each)

Ask whether the tremble also shows on a car that is completely still beside a still player.

| A/B | if the tremble... | conclusion |
|---|---|---|
| Launcher Stock (30 fps) against Unlocked | is gone at Stock | It depends on frame rate or timing: (a), (c) or the 120 Hz judder. |
| Launcher Stock (30 fps) against Unlocked | stays at Stock | It is not frame-rate related; render side (e). |
| Unlocked with `low_latency_off` (Reflex off) | changes | The Reflex sleep is involved (the KI-082 row candidate). |
| The l107 `force_hidef` arm | vanishes | The two-tile path was involved. |

A 10 s screen capture at 120 fps (ShadowPlay) of a passing car is the most informative cheap
artifact. Frame by frame it gives the amplitude, the axis and the period, all of which the probe
cannot see.

### Step 3, build (probe v2, default off, read-only)

Needs RE first. Proposed changes:

- Count every early return in `Observe()` by reason (`no_collider`, `no_inst`, `no_matrix`,
  `index>16`) and log the hook's raw call count in the minute line, so that "0 calls" is
  unambiguous.
- Move the per-car source from the collider to the code that places every ambient car each update.
  RE needed: in the inline ambient step `sub_826D75D8` (called from `sub_822D3C20`, lr `0x822D3D20`),
  find the loop over active ambients and the path-follower pose write. Hook its direct callee per car,
  and record the position at the end of each update.
- Add the axis split the current line lacks: along heading, lateral and vertical components of each
  displacement (heading = row 2 or row 0 of the car matrix). Also add the second difference (the
  acceleration sign) to expose a periodic Nth-frame pattern.
- For (c), in the same car, at the timer tick, log both buffer slots `inst + 256` and `inst + 320`
  plus `inst + 128`, and the two indices `0x8288B9F0` (update) and `0x827E5640` (render). Then:
  - The slot the renderer will read (render index) should equal the previous update's live pose.
  - Any frame where it equals the current pose, or a mix (live written after the copy), is a
    stale/mixed read.
  - Also hook the hand-off `sub_82305D00` (render-thread entry) to timestamp the flip relative to
    the car writes.
- Candidate fix arms (only after step 1 and the v2 data):
  - "copy-then-flip" ordering: snapshot traffic poses into the update slot before `sub_82305B38`.
  - Forcing the render index for traffic reads (the TLS-gated pick) to the slot written last.

A render-side traffic interpolation is a larger change and should be built only if (a) is shown.

## 5. Files

- Copied raw inputs: `private/owner-l107-20261117/session/` (`mcla.log`, receipts, snapshots) and
  `private/owner-l107-20261117/diagnostics/` (runtime log, 4 live packages with `state.json`,
  `log-tail.txt`, `frame.bmp`, reports; private save files excluded).
- Generated-code facts cited:
  - `generated/default/mcla_recomp.55.cpp:19816` (`sub_826FA0F8` matrix pick, wheel loop, site B at
    `:20130`, suspension call `:20205`);
  - `mcla_recomp.16.cpp:24870` (`sub_82305B38`, flip of `0x8288B9F0` / `0x827E5640` at `:24925`), called
    only from `sub_82305D00` (`:25265`).
