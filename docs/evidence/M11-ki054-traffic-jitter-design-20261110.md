# KI-054 traffic / camera jitter: LARecomp timing notes digest and design

Read-only research by an Opus subagent on 2026-11-10, accepted by the primary agent as a design (nothing built; ideas, guest addresses and measurements only from LARecomp (no licence), holdmysocks and RenanQueiroz (GPL-3.0); no code copied). Spark not used (external doc reading).

**Headline.** LARecomp has no "dFPS" section as such; the material is TECHNICAL_NOTES s.1-4 and 10 plus the WORKPLAN. None of the three repos touches NPC traffic (LARecomp lists it as open); they smooth the chase camera, boom, chassis and bike with 1-(1-k30)^(30*dt) and cap hitch steps near 1.5x median. Our new lead H1 (static code, unmeasured): the "[MC] Ambient Traffic" thread runs between kick 822C26B8 and join 822C2D04 and reads the global dt 827D7508 live, while in the same window the main-thread racer splitter sub_822A7E28 sets that global to dt/N and restores it; a traffic car can therefore step dt/2 one frame and dt the next. H2: our StepSmoother gives any >100 ms frame a full 0.1 s step (2.5-3x normal), matching LARecomp's camera dip. Plan: probe first (ambient_dt_probe), then snapshot fix, hitch hold, continuous-time camera filters; all default off.

---

# KI-054 research notes: dFPS / delta-time in the colleague recomps, and a fix design for us

Read-only research, 2026-11-10 (Opus subagent). Nothing built, run or edited in the repo.
Licence: mzzvxm/larecomp has no licence (README asks not to copy code); holdmysocks/mcla-recomp and
RenanQueiroz/mcla-recomp are GPL-3.0. Only ideas, measurements, guest addresses and mechanism
descriptions are taken below; no code. Short phrases quoted with attribution.
External files were fetched with `gh api` into `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\ki054-work\ext\`
(data only, nothing executed from there).

Sources read:
- L = mzzvxm/larecomp: `documentation/TECHNICAL_NOTES.md` (all 10 sections), `documentation/MCLA_WORKPLAN.md`,
  `larecomp_config.toml` (hook table), `src/mc_engine/hooks/frame_timing.cpp`, `hooks/camera.cpp`,
  `hooks/vehicle.cpp`, `hooks/hooks_internal.h` (guest constants).
- R = RenanQueiroz/mcla-recomp: `config/frame_timing.toml`, `KNOWN-ISSUES.md` (frame pacing, bike gyro),
  `src/frame_timing.cpp` (skimmed for pace-at-swap / step cap).
- H = holdmysocks/mcla-recomp: `config/frame_timing.toml`.
- Ours: `docs/evidence/M11-external-repos-survey-20261108.md`, `docs/known-issues.md` KI-054 row,
  `docs/evidence/M10-l68-watchdog-arm-ki067-20261105.md` s.5, `docs/evidence/M11-ki074-framerate-re-20261107.md`
  s.1-2, `src/mcla_raw_timer.cpp`, `src/mcla_dev_overrides.h`, generated code `generated/default/mcla_recomp.{12,13,54,55}.cpp`.

---

## 1. LARecomp TECHNICAL_NOTES / WORKPLAN: frame timing, "dFPS", delta time

There is no section literally titled "dFPS"; the material is TECHNICAL_NOTES s.1 "The engine frame timer",
s.2 "Why the Xenia 60 FPS patch is not enough", s.3 "Frame pacing", s.4 "long-session frame rate collapse"
(vblank storm), s.10 "Plausible-sounding things that are wrong", plus WORKPLAN "Frame timing above 30 FPS",
"Camera, chassis and ground depth at high frame rates", "Traffic and NPC smoothing sweep". Their cvar is
`real_frame_delta` ("REAL FRAME DELTA"), plus `frame_step_cap`, `fps_limit`, `pace_at_swap`.

### 1a. Measured facts about the game (theirs; cross-checked with ours where possible)

- Timer `sub_821BDA90`, object at fixed `0x827D7500`: +0x08 `0x827D7508` frame delta (time-scaled, "what the
  engine consumes"); +0x0C `0x827D750C` 1/delta (the frame rate the game branches on); +0x14/+0x18 accumulated
  times; +0x24 `0x827D7524` delta ceiling 0.1; +0x28 `0x827D7528` floor 0.0001; +0x38 one-shot "timer was reset"
  flag ([r3+56]); +0x54 time scale 1.0; +0x58 `0x827D7558` unscaled delta. (Agrees with our RE: +8, +88, +84 scale.)
- Guest timebase 49,875,000 Hz (from `flt_82011110` = 2.00504e-8), not 50 MHz. (We already read that constant:
  `kTicksToSeconds = 0x82011110` in `mcla_raw_timer.cpp`.)
- Delta clamp at `0x821BDC78`/`0x821BDC88` (fsel pairs) applies floor 0.0001 / ceiling 0.1 to [r3+8] and [r3+88].
  An engine dt pinned at 100 ms = real frames >= 100 ms. LARecomp: "Physics integrating at 100 ms steps is also why
  traffic cars bounce and flip during a frame rate collapse."
- Two fixed-step paths in the stock (fixed30) mode: `loc_821BDB58` (overwrites [r3+8]/[r3+88] with the fixed step
  when the measured delta >= it) and `loc_821BDB90` (taken when [r3+0x3A] or [r3+0x3C] is set; measured "taken on
  every frame during gameplay"; overwrites with 33.3 ms regardless). The Xenia blanket jump at `0x821BDB08`
  (`bne cr6,0x821BDBC8`, not fcmpu) also skips the [r3+0x20]/[r3+0x24] accumulator block (frozen at 0.0333).
- `0x82419AA0` `li r11,2` is the PM4 present interval (vblanks per present), not a dt value.
- [r3+56] reset guard: on a reset frame the last-tick field is stale, the measured delta is garbage. Skipping the
  guard (Xenia patch style) with the clamp removed "produced a loud audio blowout" (L, s.2).
- Substeps: `sub_821BD910` divides dt by the substep count correctly; three passes per frame (two of dt/2, one of
  dt), so the summed substep dt reads 2.00x real time at both 30 and 60 caps (rate-invariant). r24 at `0x822C2430`
  is the substep count; r24 = 0 clears the sub-tick gate `0x827D754C` and freezes physics; `MCLA_SUBSTEPS=0` leaves
  the player car "with no wheels". (Agrees with our KI-074 RE s.1: 2 physics steps of dt/2 + 1 city pass of dt.)
- Callers of the timer: `sub_822611B0` and `sub_822C1FA8` (the game loop); the timer runs on more than one thread.
  Game updates on the main loop `sub_82131508` and presents from "[MC] Render Thread" `sub_82306238`, one frame behind.
- Chase camera (camBoomCS::Update = `sub_82320298`, slot 10 of vtable `0x82044DB4`; mcPlayerCamera `sub_822B1B58`
  owns six camBoomCS): position factor (VelocityFilter, tune+224) at `0x82320468` in f13, look-at factor
  (AccelFilter, tune+228) at `0x823204F4` in f0. The game itself computes r11 = round(1/dt) and **halves the tune
  factor when the frame rate is below 60** (`0x82320454..0x82320464`). Each is a fixed fraction per update, so the
  camera's lag behind the car scales with dt.
- Boom frame build `sub_8231D3A8`: OrientationFilter (tune+216) / DirectionFilter (tune+220) loaded at
  `0x8231D814`/`0x8231D81C`, hook point `0x8231D820` (f13, f9; f1 = camera dt from `sub_8271F020`, unscaled +88 when
  cs+184 set, else +8). Never halved by fps. Pitfall: `sub_8231D3A8`'s shared lerp is also used by cockpit view,
  wheel animation, speedometer and HUD ("Hooking it breaks all of them. Hook the specific caller.").
- Chassis ground-depth filter `sub_82563298` at `0x82563720` (f0): 0.10 per frame at 30, 0.05 at 60.
- Bike gyro `sub_823688B8` (mcBikeGyro::Update): `0x8236895C` f13 (0.40 from `0x827E6740`, x2 below 60 fps),
  `0x823689F0` f0 (0.15 from `0x827E6744`, x2 below 60). Cars use mcCarGyro::Update, which already steps with dt.
- Tyre slip filter at `0x82369E40` (f0: 0.30 below 60 fps, 0.15 at >= 60).
- Camera transition (`sub_8271F438`, `0x8271F920`): Euler-yaw interpolation flips dest yaw by 2*pi mid-transition
  -> a 180-degree camera flip; end of transition `sub_8271F3B0`.
- Stall effect on the camera (L frame_timing.cpp comment, measured on a 60 FPS frame-by-frame capture): after a
  70-230 ms texture-streaming stall the next delta is the whole stall; "the camera dipped for a frame or two after a
  stall and came back"; capping the step near 34 ms removed the dips.
- Frame pacing: frame times quantised to a 15.625 ms grid = Windows default timer resolution; fix needs
  `timeBeginPeriod(1)` AND vsync off (95% -> 34% of frames on grid, 37.2 -> 48.4 fps).
- R (KNOWN-ISSUES 2026-10-14): when the limiter waited inside the timer update (step measured before the wait, the
  frame reaching the screen after a variable amount of work), at a 60 cap frames alternated about 12/22 ms and
  "moving objects shook"; moving the wait to the swap (`pace_at_swap`) made "each step measure one presented
  interval" (sigma 3.1 -> 0.38 ms on the title screen).
- vblank storm: SDK 0.10.0 unsigned `current - last >= interval` loop never exits after one backwards tick;
  1 ms vblank period with vsync off (~1,010/s baseline).
- Race clock `sub_82256220` adds `(int)(dt*1000)` to whole ms at [obj+0xEA4]; `0x822562E8` is the fctiwz (f0 = ms).
  Float accumulators [r3+0x14]/[r3+0x18] drift over hours (8 store sites `0x821BDB40/B54/BB0/BC0/C0C/C20/CA8/CC0`);
  audio clock `sub_821E63F0` accumulates [timer+0x60] at `0x821E6634`.
- WORKPLAN "Traffic and NPC smoothing sweep" (open): "Vehicles and pedestrians other than the player car have their
  own per-frame interpolation constants that have not been swept ... Same treatment likely applies. Not blocking."

### 1b. Their design choices (not facts about the game)

- Real delta in fixed30 mode: jump `0x821BDB58` -> `0x821BDC34`; at `0x821BDB90` replace f11 with [r3+0x58].
  (We instead switch the game to its own raw policy via `sub_822D2478` config +72=1/+88=0 and supply the step in f1
  of `sub_821BDA90`; same outcome, different seam.)
- Hitch clamp at `0x821BDAB0` (after `subf r8,r10,r11`, elapsed ticks), unconditional: "adaptive" cap =
  min(125 ms, max(34 ms, 1.5 x median of the last 30 uncapped deltas)), fixed 125 ms until 8 samples; a fixed
  16-1000 ms option; excess time is dropped (slow-motion instant, "as the console did"). No debt repayment.
- Continuous-time filters: k(dt) = 1 - (1 - k30)^(30*dt*scale); camera k30 = incoming factor if the engine already
  halved it (round(1/dt) < 60), else 0.5 x incoming (fixes a flip at the 60 fps boundary that was "the camera jitter
  seen while drifting"); boom filters use the raw tune as k30 with the hook's f1 dt; chassis k = 1 - 0.90^(30 dt);
  gyro k30 = 0.80 / 0.30; tyre slip 1 - 0.70^(30 dt). User multiplier `chase_cam_smoothing_factor` on pos/look-at only.
- Also: camera shake amplitude scale (`0x82315188`, default 0.6), suspension pushback clamps (`0x8256384C`,
  `0x82563850`) - feel changes, not timing corrections; not relevant to us at <= 40 fps.
- Pacing: limiter at the swap (`0x82419AA0` hook) with a high-resolution waitable timer + PAUSE spin; untimed
  swaps (intro) held to 30/s.

## 2. Car/camera jump fixes in the three repos

| Item | L | R | H |
|---|---|---|---|
| Camera pos/look-at `0x82320468`/`0x823204F4` | yes (origin) | yes | yes |
| Boom orientation/direction `0x8231D820` | yes | no (not in toml) | no |
| Chassis depth `0x82563720` | yes | yes | yes |
| Bike gyro `0x8236895C`/`0x823689F0` | yes | yes | no |
| Tyre slip `0x82369E40` | yes | no | no |
| Transition yaw unwrap `0x8271F920` | yes | no | no |
| Step cap after stalls | adaptive 34-125 ms | clamp at `0x821BDAB0` | clamp |
| Pace at swap (step = presented interval) | yes | yes (measured) | no |
| **NPC / ambient traffic positions** | **no** (listed open) | **no** | **no** |

Formula everywhere: k(dt) = 1 - (1 - k30)^(30*dt). None of the three touches ambient traffic dt or traffic
interpolation; their camera fixes target behaviour above 30 fps and after stalls. The "car jump" they fixed is the
player car's chassis/suspension filter and the camera, not NPC cars.

## 3. Our dt feed and where the KI-054 jumps would come from

Our feed (`src/mcla_raw_timer.cpp`): strong override of `sub_821BDA90`; only for the main timer `0x827D7500` with
timer+60 == 0 (raw policy), +56 == 0 (reset flag; so we keep the reset guard) and +58 == 0. Raw = (now - last) *
`flt_82011110`. Then: (1) `StepSmoother`: EMA weight 0.05 (~20 updates), applied = clamp(avg + 0.02 * debt,
0.9 avg, 1.1 avg), debt += raw - applied; raw > 0.1 s -> applied = exactly 0.1 s, debt += raw - 0.1 (cap 1 s), avg
untouched; (2) whole-ms steps with carry (+1e-7 bias); passed in f1, so the game's own clamp/scale still run.

A Deck frame at 25-27 fps (present intervals mixing ~33 and ~50 ms, average ~38-40 ms): the game gets ~38-40 ms
nearly constant (+-10% hold), rounded to 38/39 ms; after any frame > 100 ms it gets one 100 ms step, then ~+10%
steps until the debt is repaid (a 400 ms hitch -> ~0.3 s debt -> ~80 updates at +10%).

Hypotheses (ranked):

H1 (new, strongest for NPC cars specifically): **ambient-traffic thread reads a split global dt.**
Our own KI-074 RE s.2 ("Concurrency") already found it: the "[MC] Ambient Traffic" thread runs `sub_826D75D8`
between the kick (`bl sub_826D9578` at `0x822C26B8`) and the join (`bl sub_826D95B0` at `0x822C2D04`) and reads
the global `timer+8` *live*; meanwhile the main thread runs the racer splitter `sub_822A7E28` (racers in state
+656 == 3: mcRacer/mcOpponent/mcAiRacer/**mcPolice**, from the loop at `0x822A7494`), which calls `sub_821BD910`
on the same global timer, setting +8 = dt/N for N passes and restoring dt on the last. Verified in generated code
today: the splitter's r30 = `0x827D7500` (lis -32131 / addi 29952). Ambient-thread reads of `0x827D7508` found:
- `0x826D796C` `lfs f30,8(r30)` in `sub_826D75D8`: the manager's step (ambient clock mgr+0x64238, countdown
  mgr+0x64244, placement); the f31 read at `0x826D79A8` is +88 (unscaled, not split).
- `0x826FA300` `lfs f1,8(r19)` in `sub_826FA0F8` (mcAmbientCollider vfunc +56): dt for the per-wheel suspension
  update `sub_826FBA60`, per car.
- `0x826FA8E0` `lfs f0,8(r9)` in `sub_826FA8A8` (mcAmbientPhysInst vfunc +108): 0.5 s pose re-validation countdown.
- Also +8 reads (thread membership not verified) in `sub_826DD948`, `sub_8270AE38`, `sub_8270CEC0`, `sub_8270D800`,
  `sub_8270FC40` (approximate sweep; exact addresses to confirm).
Effect: a car whose read lands inside a racer split advances dt/2 (N = 2) that frame and dt the next: a per-car
back-and-forth along its path; different cars in one frame disagree. Fits every report: l52 PC was *racing* on
the freeway (opponents = racers), Deck l66 cars stacked *while being pulled over* (police = mcPolice racer), l68
freeway presses in races; exposure depends on thread wake timing (worse on an oversubscribed Deck, worse in heavy
frames), and the visible error scales with dt (dt/2 of a 100 ms hitch step = 50 ms of motion = ~1 m at 20 m/s),
so it shows "during frame drops". Free roam with no racers/police should be immune (testable prediction). No
colleague repo addresses this. Status: hypothesis with static evidence; not yet measured at runtime.

H2 (camera jumps; also amplifies H1): **our hitch branch applies exactly 0.1 s.** Frames 50-100 ms are held to
<= 1.1 x avg, but a 101 ms frame yields a 100 ms step (2.5-3x normal). The camera's per-update filters (fraction
per update, independent of dt below 60 fps) then lag by ~3x for one frame and catch up: LARecomp measured exactly
this "dip" after stalls and removed it by capping the step near 1.5 x median. KI-054 l52 had up to 9 frames > 100 ms
per minute (max 402 ms); l68 had 80-120 ms single frames.

H3: **per-update (not per-time) camera/chassis filters.** With our near-constant step this is a steady feel offset
(at 25 fps the camera lags ~33% longer in time than on a 30 fps console), and only becomes a jump on big step
changes (H2). The 60-fps-threshold flip only matters on PC Unlocked when fps crosses 60 (rare for us, CP-bound at
33-43 fps; possible at the 120 cap in light scenes).

H4: **step vs presented interval mismatch** (global judder). With the smoother the world advances ~38 ms per frame
while presents are 33 or 50 ms; tracked objects drift +-(5..12 ms x relative speed) and are pulled back by the debt
repayment. This judders the whole scene relative to the eye (strongest for objects with large relative speed,
e.g. oncoming freeway traffic) but is not NPC-specific. R measured the same mechanism ("moving objects shook") and
fixed it by making the step equal the presented interval. For us the analogue is a steady cadence on the Deck:
25 fps exactly (`mcla_guest_vblank_hz=50`, display at 50 Hz) so step = present = 40 ms.

H5 (unlikely): whole-ms rounding (<= 1 ms, ~2.6% at 38 ms, alternating 38/39) - too small to see; can be excluded
with `mcla_raw_timer_ms_steps=false` if needed. H6 (unlikely): debt repayment (+10% for seconds) is a slow speed-up,
not a shake.

## 4. Design for us (re-implemented from the facts above; no code copied)

Naming follows `src/mcla_dev_overrides.h`: host init-only bools `mcla_*`, default off; dev-override keys
`name=true` (=false changes nothing); controls added later as `*_off` if a default flips.

### Arm 0: probe (no behaviour change) - `mcla_ambient_dt_probe` / key `ambient_dt_probe`
- Hook (mid-asm, before) `0x822C26B8` (bl kick): snapshot full dt = f32 at `0x827D7508`, frame id, set
  window_open (atomic, release). Hook `0x822C2D08` (after the join returns): window_open = false.
- Hooks after the loads at `0x826D796C` (f30), `0x826FA300` (f1), `0x826FA8E0` (f0): if window_open and the thread
  is not the main thread, count reads; count mismatches where |loaded - snapshot| > 1e-6; keep min(loaded/snapshot).
- Hook entry of the racer splitter `0x822A7E28`: count splits inside the window.
- Log once per 300 frames: `MCLA_AMBDT frames=300 reads=<n> split_reads=<n> min_ratio=<x> racer_splits=<n>
  sites=<a/b/c>`. Prediction: split_reads > 0 only when racer_splits > 0 (races, pursuits), ~0 in free roam.

### Arm 1: the fix - `mcla_ambient_dt_snapshot` / key `ambient_dt_snapshot`
- Same hooks; at the three read sites replace the loaded register with the snapshot when window_open (otherwise
  leave it: the non-threaded path `sub_822D3C20` reads while +8 is the full dt anyway). The snapshot is exactly the
  value the thread reads when there is no overlap, so pause (scale 0 -> +8 = 0), photo mode, cinematics and the
  network path (`min(+92, 0.05)` branch, untouched) keep their semantics. Racers are untouched.
- Extend to the other +8 readers in the ambient call tree after the probe shows which ones run on that thread.
- Fallback arm if the site list grows too long: `mcla_ambient_join_before_racers` (wait for the ambient join at
  the splitter entry while the window is open) - serialises, costs CPU parallelism; measure fps.
- Expected effect: Deck 25-27 fps and PC alike - traffic cars stop jumping back and forth in races/pursuits; no
  fps change (a few hook calls per car per frame). Free roam unchanged.

### Arm 2: hitch hold - `mcla_raw_timer_hitch_hold` / key `raw_timer_hitch_hold`
- In `StepSmoother`, the raw > 0.1 s branch applies min(0.1, (1 + kHold) x avg) instead of 0.1, and the rest goes to
  debt (same 1 s cap, same 2%/update, +-10% repayment). Removes the 2.5-3x single step that dips the camera (and
  doubles H1's visible error). Race clock stays exact (debt kept), as today.
- Variant if the owner prefers LARecomp's model: drop the excess beyond the cap (no debt) - not recommended
  because the owner measured race-clock loss before (2026-10-28).
- Expected: Deck - a hitch becomes a brief slow-motion instant instead of a camera lurch; PC Unlocked same.

### Arm 3 (later, feel): continuous-time camera filters - `mcla_camera_ct_filters` / key `camera_ct_filters`
- Hooks `0x82320468` (f13) and `0x823204F4` (f0): k30 = in if round(f32 at `0x827D750C`) < 60 else 0.5 x in;
  out = 1 - (1 - k30)^(30 x dt), dt = f32 `0x827D7508`, only if 0 < k < 1 and 0 < dt <= 0.25.
  Optional `0x8231D820` (f13, f9; dt = f1), k30 = in. Do NOT hook `sub_8231D3A8` itself (shared lerp).
- Expected: Deck - camera lag in time matches the 30 fps console (slightly tighter than now); PC Unlocked - no
  stiffening above 60 / no flip at 60. Chassis `0x82563720` (k = 1 - 0.90^(30 dt)) only if the owner reports
  suspension feel differences.

### Arm 4 (cheap pacing test): `vblank_hz_50` key -> `mcla_guest_vblank_hz=50` (25 fps cap)
- On the Deck with the display at 50 Hz: steady 40 ms presents = smoothed step; tests H4 without code beyond the key.

### Risks
- Determinism: single-player is variable-step already (KI-074 RE); ambient traffic is not replay-recorded as far
  as we know (open question). The snapshot reproduces the no-race value, so it moves toward console behaviour.
- Threads: snapshot written before the kick's event signal (happens-before for the ambient thread); use atomics.
  Main-thread reads of +8 must never be replaced (thread check).
- Cut-scenes / photo mode / pause: values pass through (scale 0 -> 0). Network mode branch untouched.
- Hitch hold: longer slow-motion after big stalls; debt repayment runs +10% for a few seconds (already the case for
  50-100 ms frames). Loading screens: raw <= 0 resets the smoother as now.
- Camera filters: changes the feel at 25 fps (the camera follows faster than today); keep default off until owner A/B.

### Test plan (owner)
1. One build with keys: Arm 0 always on; A = `ambient_dt_snapshot=false`, B = `ambient_dt_snapshot=true`; then
   `raw_timer_hitch_hold` on/off as a second pair.
2. Deck l8x, Locked, the same freeway race (and one police pursuit that ends in a pull-over), plus 2 min free roam
   with traffic as the control. Owner watches traffic cars ahead/alongside during frame drops and the chase camera
   right after a stutter; F10 at each jump.
3. Logs: `MCLA_AMBDT` (split_reads, racer_splits) and `MCLA timer stats` (`mcla_raw_timer_stats=true`: applied
   step change, hitch count).
4. PASS (H1 confirmed + fixed): arm A shows split_reads > 0 in races/pursuit and 0 in free roam, and the owner sees
   the shake; arm B shows replaced reads with 0 residual mismatches and the owner sees no traffic shake in 2 races +
   1 pursuit at 25-27 fps. PASS (H2): with hitch hold on, no camera lurch after stutters, race time vs stopwatch
   within the previous 0.5% margin. If arm A shows split_reads = 0 while the shake persists, H1 is rejected: go to
   H4 (Arm 4) and the traffic per-frame interpolation constants (LARecomp's open item) next.

## 5. Other cheap, relevant items from LARecomp's notes we do not have

- P2: race clock fix at the clock itself (`0x822562E8` fctiwz in `sub_82256220`, carry the fraction) - would let us
  drop whole-ms timer steps (`mcla_raw_timer_ms_steps`) and keep dt exact.
- P2: transition yaw unwrap (`sub_8271F438` / `0x8271F920`, end `sub_8271F3B0`) - a 180-degree camera flip during
  camera transitions; check against owner "camera jumps" during transitions (possibly KI-026-adjacent).
- P2: `resolution_scale=2` corrupts the frustum-cull projection aspect (race-start showcase cameras pick opponents
  behind the camera; grid cars spawn tilted/floating) per L s.5; KI-054 l52 ran at scale 2 - check whether our
  scale path is the same SDK cvar and whether we ever saw those symptoms.
- P3: exact double accumulators for [timer+0x14]/[timer+0x18] (8 store sites `0x821BDB40..0x821BDCC0`) and the
  audio clock `0x821E6634` - float drift after hours.
- P3: chassis depth `0x82563720` and bike gyro `0x8236895C`/`0x823689F0` continuous-time (bikes at 25 fps / 120 cap).
- P3: vblank loop clamp (already survey item 10); timer-resolution check (15.625 ms grid) on our host if the
  launcher does not already call `timeBeginPeriod(1)` - verify only.

Spark: no Spark job was run (read-only subagent; source facts were taken directly from files/disassembly).
