# Wheel tester, 2026-10-30: analysis of three l46 F10/L2/R2 packages

Read-only analysis by an Opus subagent on 2026-10-30 of l46 packages -2, -3, -5 from a volunteer T300RS tester (packages named by id; raw material retained under `private/tester-reports/20261030-wheel-tester/`, saves and minidumps not opened). Accepted by the primary agent. Owner-relayed facts: T300RS in PC mode (PID B66E), firmware 34.00, driver 2.11.57.0, Thrustmaster panel master gain 75%, constant/periodic/spring/damper 100%, BOOST off, Auto-Center "by the game"; Alt-Tab reliably restores centering.

Read-only analysis. Packages are referred to by id only. No save data was opened, no
executable or minidump was run. The frames were viewed as temporary downscaled PNGs,
which have since been deleted. Log times are local (UTC+1); the package ids are UTC.

Session: one process (pid 10724), started 12:31:21, wheel connected 12:31:23.
The session produced **five** packages. The tester sent three of them:

| # | Package | Local time | Trigger (as recorded) | Sent |
|---|---|---|---|---|
| 1 | live-20261030T113130Z-10724-1 | 12:31:30 | general / f10 | no |
| 2 | live-20261030T113358Z-10724-2 | 12:33:58.8 | wheel / wheel-l2 (raw button 8) | yes |
| 3 | live-20261030T113404Z-10724-3 | 12:34:04.8 | general / wheel-r2 (raw button 9) | yes |
| 4 | live-20261030T113425Z-10724-4 | 12:34:25.3 | general / wheel-r2 (raw button 9) | no |
| 5 | live-20261030T114016Z-10724-5 | 12:40:16.4 | wheel / wheel-l2 (raw button 8) | yes |

## 1. Per package

All three packages show `mcla_version 0.20.37.0`, mode `stock`, a 1920x1017 windowed
window, a 1920x1080 render target, and build fingerprint `9EF5F784BA27...145C`. That is
the l46 **build-manifest** SHA-256 (`build_manifest_sha256` in the l46 candidate
receipt), not the exe hash. It identifies the l46 volunteer build, whose `mcla.exe` is
`FC1F0E2D...`. **The build is l46.** All three frame captures are fresh guest output
(`presented-guest-output`, `nonuniform`).

- **-2 (12:33:58, wheel-l2).** Q1: "was driving to the race start". Q2: "flash the
  headlights to start the race, then drive to the start". Q3: "strange game freezes
  while driving (also during the race itself)". Q4: "no freezes". Frame: in-car view,
  night, with a burning wreck and a smoke column ahead.
- **-3 (12:34:04, wheel-r2, general).** Q1: "race start, after I reached the start".
  Q2: "drive to the start so the cutscene begins". Q3: **"the wheel centering
  disappeared"**. Q4: "centering should not disappear during short camera fly-bys,
  scene changes, etc.". Frame: the overhead race-start camera over the player's car and
  an opponent (#9). Guest state: cutscene time 0.93, warper request active,
  `prestream_frames 3`. There is no wheel section, because R2 packages do not record
  one.
- **-5 (12:40:16, wheel-l2).** Q1: "just cruising the city". Q3: **"the steering-wheel
  animation inside the car disappeared while driving (the car itself still responded
  to the wheel)"**. Coordinator detail: it froze until he drove backwards and forwards
  again. Frame: in-car view, night city, speed about 66; the in-car wheel looks roughly
  level.

## 2. Wheel identification

- SDL name `44F B66E`, VID 0x044F, PID 0xB66E, GUID `0300da604f0400006eb6000000000000`
  (USB bus 0003, VID/PID 044F/B66E, version 0), joystick_type 2 (WHEEL). Flags:
  `wheel: true` (admitted by the SDK as a supported wheel), gamepad false, opened true,
  haptic true. It reports 4 axes, 13 buttons and 1 hat (`SDL_WHEEL_CONNECTED ...
  haptic=1`).
- Haptic: `SDL_WHEEL_FFB_DEVICE_INFO v=2 guest_slots=64 host_effects=128
  host_playing=128 features=0x000D87FF`. The bits decoded against the pinned SDL3
  header are: constant, sine, square, triangle, saw up/down, ramp, spring, damper,
  inertia, friction, custom, GAIN, **STATUS** (bit 18) and PAUSE. AUTOCENTER is absent.
  STATUS is present, so the l47 status-based watchdog can run on this wheel.
- **Model: Thrustmaster T300RS**, the same model as the owner's rim. Owner
  confirmation: the PID equals the M6-015 reference PID (B66E), and the axis, button and
  hat counts match. From memory of the Linux hid-tmff2 IDs (not verified in this repo),
  B66E is the T300RS in normal (PS3/PC) mode, B66D is PS4 mode and B66F is PS3-advanced.
  The tester's rim therefore enumerates exactly like the owner's reference, and no
  renumbering for another mode applies.
- Side observation: SDL received no product string (the name is the generic "44F
  B66E"). That is cosmetic.

## 3. L2/R2 mapping

- Config in the package: `l2_button 8, r2_button 9` (the default `8,9`). The log
  confirms it: `controller triggers enabled wheel_l2=8 wheel_r2=9`.
- Packages -2 and -5 (recorded as "wheel-l2"): the raw button array at capture is
  `[0,0,0,0,0,0,0,0,1,0,0,0,0]`. **Raw index 8 was held**, and nothing else.
- Packages -3 and -4 (recorded as "wheel-r2"): no button array is captured for R2. By
  construction the trigger fires only for raw index **9** (`OnJoystickButton` role
  match).
- The M6-015 physically checked T300 reference (`config/wheel-input-reference.json`)
  names the two unmapped rim buttons **`r2_unmapped: 8`, `l2_unmapped: 9`**. Buttons
  0-7 and 10-12 are mapped (paddles, face, Share/Options, L3/R3, PS). Our default "8,9"
  assumed "Thrustmaster order puts L2 before R2", and M9-010 marks it as unverified. It
  contradicts our own reference.
- The tester says he pressed **L2** for the centering bug and the package says R2. That
  is exactly what an L2 at raw 9 produces under the "8,9" default.
- **Verdict: on the T300, L2 and R2 are swapped, not different indices.** Physical L2 =
  raw 9, physical R2 = raw 8. Packages -3/-4 are his L2 presses. Packages -2/-5 are R2
  presses, which the build labelled "wheel L2" and so attached the wheel section.
- **Recommended setting: `mcla_wheel_diagnostic_buttons = "9,8"`.** Make this the new
  default too, and update the M9-010 text. One cheap confirmation on the owner's T300
  (press L2 with `mcla_wheel_button_probe=true`, expect `button=9`) would move this from
  inference to physical evidence. The reference JSON plus the tester's account already
  agree.
- Consequence for this batch: the package taken at the centering loss (-3) has **no
  wheel section**. The FFB evidence below comes from the full log-tails (the union of
  the three tails covers 12:31:21-12:40:16 except a gap of about 10 s at
  12:34:04.8-12:34:14.8) and from the 32-line arrays in -2 and -5.

## 4. KI-022 (centering loss)

### Effect layout, from the create lines

| Slot | Type | Duration | Role |
|---|---|---|---|
| 1 | spring | infinite | centering (guest coeff 114, sat 25; first update coeff 89, sat 178) |
| 2 | constant | infinite | host gain 0% by policy |
| 3 | damper | infinite | |
| 4 | square | 150 ms | finite event (impact/curb) |
| 5 | constant | 150 ms | finite event |
| 6 | square | infinite | continuous periodic (engine/road), created 12:35:47, gain 40% |

Slots 1-3 were created and started at 12:31:52. Before them came
`SDL_WHEEL_FFB_DEVICE_CONTROL v=1 command=4` (12:31:23, reset at open).

### Timeline around the loss (package -3, race start)

- 12:33:58.88-12:33:59.15: timer-scale ramp 0.58 -> 0.10 (race-start slow-down).
- 12:33:59.81: `menu state: game paused (sim 1 ...)`, timer scale back to 1.
- 12:34:02.118-12:34:02.880: **17 finite `action=start slot=4 type=square`** at about
  frame rate (some in pairs with the same millisecond), while the simulation was still
  paused. This is the densest FFB burst of the session.
- 12:34:02.845: `game running`.
- 12:34:04.780: tester presses L2 (recorded as wheel-r2): centering already gone.

### Reading against the "if you see X then Y" table

- **No `action=stop` line anywhere in the session**, for any slot. In particular there
  is no stop for spring slot 1 or damper slot 3. So there are no stops without matching
  starts, and nothing like the P1/P2 pattern (a stop of the spring/periodic slots
  followed by a single start).
- **No `SDL_WHEEL_FFB_DEVICE_CONTROL`** after the open-time command 4: no pause,
  continue or stop-all from the title.
- **No `SDL wheel FFB ... failed`** line (no create, update, start or resume errors),
  so P5 is excluded.
- **Routing**: one `SDL_ACTIVE_CONTROLLER` only (12:31:23, `first-connected-wheel`).
  The host history has one transition (-1 -> 3). No pad or mirror is present, and the
  device array holds only the T300. P7 is excluded.
- **Device**: one `SDL_WHEEL_CONNECTED`, with no removal, summary or reconnect. P9 is
  excluded.
- **No `create` that reuses slot 1** with another type. A retype through UpdateEffect
  would be silent in l46, so P4 is not excluded.
- No `XINPUTD`/`XINPUTD_FF_AUDIT` lines (l46 lacks the audit). `EffectOperation`
  appears only in the import-resolution line. No unsupported-effect or rumble lines.
- Time scale: the M8 raw timer logs ramps to 0.1 at every transition (12:32:56,
  12:33:18, 12:33:28, 12:33:58, 12:34:22, 12:34:24, 12:36:01, 12:36:07, 12:38:30). The
  FFB path has no time-scale input. The only FFB activity tied to these ramps is finite
  square/constant starts (for example 12:38:31.110 and 12:38:32.260, slots 4+5 during
  the 12:38:30 slow-down, which looks like a crash slow-motion).
- Focus: **12:38:42.858 `action=resume ... reason=focus` for slots 1 (spring), 2, 3
  and 6.** A focus loss and regain happened (Alt-Tab, overlay or popup). This line
  matters: the SDK marks an infinite effect for resume only if
  `SDL_GetHapticEffectStatus` read **playing** at the focus loss. On resume it calls
  `SDL_RunHapticEffect`, which really restarts the spring on the device.

### Verdict per package

- **-3 (centering lost at race start): pattern P3 / P6 / P4** ("only finite starts, no
  stop for the infinite slots, no errors, no routing change"). The title issued no
  logged stop, device control or retype by SetEffect. The loss coincides with the
  race-start slow-down, then the sim pause, then the burst of 17 finite square starts.
  Two readings stay open:
  - P3: the T300/DirectInput dropped the infinite effects behind the host's back. The
    host still believes `running`, and later updates never restart.
  - P6/P4: a silent re-parameterisation of the spring through UpdateEffect during the
    cutscene (l46 logs only the first update of a slot).
- **The batched-start/stop defect (P1) is not supported by these lines.** P1 requires
  at least one `action=stop` for the spring/periodic slot. There is none. The 17 slot-4
  starts are finite and would be consistent with entry 0 of a batch whose later entries
  were dropped. That would only matter if the title also batched spring/damper starts
  after a stop, and no such stop exists. The l47 batch fix is harmless here but is not
  shown to be the cure.
- **-2 (12:33:58):** taken about 6 s before the loss, and its ring ends at 12:33:29. It
  is a pre-loss baseline: spring slot 1 started at 12:31:52 and was never stopped,
  routing on the wheel.
- **-5 (12:40:16):** the newest 32 FFB lines (12:34:35-12:39:44) show only finite
  starts, the slot-6 periodic create/start, and the focus resume above. Again there is
  no stop and no error.

### Caveat for the l47 watchdog

At 12:38:42 the T300 reported the spring as **playing**. Otherwise slot 1 would not have
been resumed. Either centering had already come back by then (unknown), or the T300
status "lies" for a device-dropped infinite effect. In the second case the l47
`reason=watchdog/update` re-arm (which acts only on status = not playing) will **not**
fire. The focus resume, however, would have restored centering at 12:38:42 by an
unconditional `SDL_RunHapticEffect`.

**Whether centering returned after that Alt-Tab is the single most useful answer the
tester can give.** Yes plus no tester action means a lying status (P3 with stale status):
the next fix is a bounded unconditional re-start of infinite effects on triggers (for
example the end of a finite burst, or a menu/sim pause transition). No means a
title-side zeroing (P6/P4), visible in l47's `retype` line or needing per-update
parameter logging.

### Relation to the tester's statements

- (b) "whenever there is some camera movement or game-speed change": every cutscene
  and slow-motion transition in the log is a timer-scale ramp plus a sim pause. Most of
  them are followed by finite square starts. The 100%-vs-not pattern fits a device-side
  race (P3) or a title update during the transition.
- (c) crashes were fine today; race start lost it immediately: confirmed by the timing
  (loss between 12:33:59 and 12:34:04).
- The earlier slow-motion crash report (centering and vibration lost, impacts kept) is
  the same signature: infinite effects lost, finite effects working.

## 5. Other bugs in the reports

- **Freezes while driving and during the race (package -2).** The perf audit armed at
  the press (157 samples to 12:34:04) shows three hitches: **301 ms, 163 ms, 157 ms at
  12:34:02.1-12:34:03.1**. All fall inside the race-start transition (paused -> running
  plus the 17-start FFB burst). They show no streaming stalls, no shader/PSO compile and
  normal CPU/GPU times (cpu 75 ms / gpu 77 ms on the worst frame). Per-minute pacing:
  about 29-30 fps (stock 30-fps cap), with a per-minute max of 350-430 ms in most
  minutes (12:34:22 minute: 7 frames >100 ms; 12:36, 12:38, 12:39 also have
  355-401 ms maxima). The minute before the press (12:32:22-12:33:22) was clean (max
  84 ms). The second audit (-4, 12:34:25) passed with one 116 ms frame. GPU busy is
  about 18-20 ms/frame, and VRAM 4-5.5 GB of a 9.3 GB budget. The hitches cluster at
  game transitions (cutscene, slow-motion, sim pause). There is no host streaming or
  compile cause in the audit. This is a new perf item (no matching KI), a candidate for
  the KI-046 or transition-hitch triage. The "while driving" freezes before the press
  are not visible in the available minute summaries beyond the 7 frames >50 ms.
- **Steering-wheel animation frozen (package -5)**, coordinator item:
  - **Routing: no change.** Only the initial `SDL_ACTIVE_CONTROLLER`, one host
    transition, `host_latest_active` = the T300 at 535.5 s (just before the press). No
    mirror or pad exists in the device list. A routing hand-off is excluded.
  - **Raw input alive:** steering axis -4961 at capture (367 in -2), pedals released
    (32767). The tester says the car still steered.
  - Guest racer flags look normal and match the other packages: `racer.state656 = 3`,
    `camera_input_off908 = 0`, `pad904 = 0`, `timer_scale 1`, `game_time_scale 1`, no
    pause, no cutscene. The in-car wheel in the frame looks about level. A raw -4961
    (about 15% left) would usually show some rotation, which weakly supports the
    freeze, but one frame is not proof.
  - Nearby events: 12:39:39.259-12:39:41.625 a short `game paused (sim 1 ... racer-off
    1)` then running; 12:38:42 the focus resume; 12:39:44.895 one finite square start.
    The FFB events do **not** coincide with an input re-route, so there is no single
    device event (for example a re-acquire) behind both.
  - Most likely a guest/title-side animation state issue: it recovered after reversing,
    and the input path is intact. No KI matches. Ask whether it began after a
    cutscene, short pause or camera cut (the racer-off pause at 12:39:39 is a
    candidate).

## 6. Recommendations

1. **Wheel button setting:** `mcla_wheel_diagnostic_buttons = "9,8"` for this tester
   now (L2 = raw 9, R2 = raw 8). Change the default to "9,8" for T300 builds. Confirm
   once on the owner's T300 with `mcla_wheel_button_probe=true`. Tell the tester his
   "R2" packages are his L2 presses.
2. **l47 expectation:**
   - Batch fix: no evidence that it applies (no stop lines). Harmless.
   - Watchdog/update re-arm: it covers his case **only if** the T300 reports the dropped
     spring as "not playing". The 12:38:42 resume shows status "playing" at least then,
     so coverage is uncertain.
   - l47 adds `retype` and `start-suppressed` lines, which will tell P4 and a
     status-lie apart. Worth giving him l47 (once a V2/pinned variant exists) mainly for
     its diagnostics. Prepare the next step (bounded unconditional re-start of the
     infinite spring/damper on transition or burst end) in case l47 shows no `rearm`
     line at a loss.
3. **Ask the tester:**
   - After centering vanished at the race start, did it come back later by itself, and
     specifically after an Alt-Tab or overlay around 6.5 minutes into the session
     (12:38:42 local)? What did he do then (Alt-Tab, Steam overlay, a popup)?
   - Does Alt-Tab out and back restore centering every time? Does it still restore it
     when nothing else is touched?
   - Was the damper (low-speed heaviness) also gone, or only the spring?
   - With "9,8" (or on l47): press **L2** within 1-2 s of the loss, so the 32-line FFB
     array is not flushed by finite starts, and once more after an Alt-Tab.
   - Thrustmaster Control Panel settings (autocenter "by the game", gain), driver and
     firmware version, and rim mode switch position.
   - Freezes: do they happen in free driving with no transitions, or only around race
     start, slow-motion and cutscenes? Press F10 right after a freeze in free driving.
   - Steering animation: what happened just before it froze (cutscene, pause, camera
     change, crash)? Did reversing always fix it? Has it happened again?
   - Please also send packages -1 and -4 (-4 is an R2/L2 press at 12:34:25, 20 s after
     the loss; its full log is the only source for the 10 s gap after -3).
