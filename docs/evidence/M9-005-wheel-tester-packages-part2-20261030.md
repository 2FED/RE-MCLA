# Wheel tester, 2026-10-30, part 2: packages -1 and -4 (session 10724, l46)

Read-only analysis by an Opus subagent on 2026-10-30 of l46 packages -1 and -4 from a volunteer T300RS tester (packages named by id; raw material retained under `private/tester-reports/20261030-wheel-tester/`, saves and minidumps not opened). Accepted by the primary agent. Owner-relayed facts: T300RS in PC mode (PID B66E), firmware 34.00, driver 2.11.57.0, Thrustmaster panel master gain 75%, constant/periodic/spring/damper 100%, BOOST off, Auto-Center "by the game"; Alt-Tab reliably restores centering.

Read-only analysis. This follows part 1 (`wheel-tester-20261030-analysis.md`). No save
data was opened and no minidump or executable was run. Times are local (UTC+1).
Packages are referred to by id only.

Owner-supplied wheel facts: T300RS in PC mode, firmware 34.00, driver 2.11.57.0. Panel
settings: master gain 75%; constant, periodic, spring and damper 100%; BOOST off;
Auto-Center "by the game". **Alt-Tab reliably restores centering.**

## 0. Correction to part 1

The race-start burst is **28** finite `action=start slot=4 type=square` lines, not 17.
They run from 12:34:02.118 to 12:34:02.880, and both the -3 and the -4 tails show 28.
Thirteen frames carry two starts in the same millisecond, followed by two single starts.

## 1. The 20 s after the loss (package -4 log-tail, 12:32:01 to 12:34:25)

Leaving out the perf-sample, guest-sampler and minute-summary lines, the complete event
sequence is:

| Time | Event |
|---|---|
| 12:33:58.88-59.15 | timer scale ramps 0.58 -> 0.10 (race-start slow-down) |
| 12:33:59.811 | `game paused (sim 1 local 0 racer-off 0)`, timer scale back to 1 |
| 12:34:02.118-02.880 | 28 x `start slot=4 type=square` (finite, 150 ms), issued while the sim is paused |
| 12:34:02.845 | `game running` |
| 12:34:04.780 | L2 press (logged as wheel-r2), package -3; centering already gone |
| 12:34:05.010-12:34:22.3 | **no FFB, input, menu or timer-scale line at all** (only the guest sampler and the -2 perf-audit summary, PASS) |
| 12:34:22.343-22.676 | timer scale 0.90 -> 0.10 (another slow-down/camera transition) |
| 12:34:22.843 | `game paused (sim 1 local 0 racer-off 1)` |
| 12:34:24.613 | `game running`, timer 0.1 -> 1 at 24.745 |
| 12:34:25.345 | L2 press (logged as wheel-r2), package -4 |

Lines that are **absent** from the whole -4 tail: any `action=stop`, any `start`,
`create` or `first-update` for slot 1 (spring), slot 2, slot 3 (damper) or a periodic
slot, `resume`, `defer-start`, `drop-start`, `SDL_WHEEL_FFB_DEVICE_CONTROL`,
`SDL wheel FFB ... failed`, `SDL_ACTIVE_CONTROLLER`, `XINPUTD`, focus lines, and any
disconnect or reconnect. The window stayed focused: `state.json` reports
`window.focused true`. The guest state at -4 is a normal driving state: racer `state656
3`, camera input on, no pause, no cutscene, and time scale 1.

**Confirmed: no stop and no error.** Nothing on the host side touched the spring or the
damper across the loss, the second transition, or the 20 s that follow.

### What the missing update lines can and cannot show

l46 logs a parameter line only for the **first** `UpdateEffect` of each slot
(`update_count == 0`). Later updates are silent unless they fail. The absence of update
lines at 12:33:58-12:34:05 therefore **neither shows nor excludes** a title-side
re-parameterisation (P4/P6) from the log alone. The verdict comes from the Alt-Tab fact
and the code path, below.

There is also a second silent path. In l46 a title `start` for an infinite slot is
suppressed without a log line when the host believes the effect is running and
`SDL_GetHapticEffectStatus` returns playing (`ShouldSubmitWheelEffectStart`). A start
that was submitted would have been logged as `start slot=1`. So if the title did
re-issue a spring start at the race start, the T300 reported the spring as **playing**
at that moment.

## 2. Verdict, P3 against P4/P6

**P3 (the device or driver silently stopped the infinite effects, with the host still
believing them running) is strongly favoured. P4/P6 (the title zeroing the spring) is
effectively refuted.** The reasoning:

1. The l46 focus path (`StopControllerFeedback`, then `ResumeControllerFeedback`) runs
   `SDL_StopHapticEffects` on focus loss. On focus regain it runs
   `SDL_RunHapticEffect(effect_id)` for each preserved infinite slot. It **never
   re-sends parameters**. The device restarts the effect with the parameters it already
   holds.
2. Alt-Tab reliably restores centering. So the parameters held on the device are still
   good centering values. If the title had written zero or neutral spring coefficients,
   a plain stop+run would bring back a zero spring and centering would stay gone.
3. The title updates the spring continuously, with speed-dependent coefficients (first
   update 89/178 two seconds after creation). If it had neutralised the spring during
   the cutscene, its next ordinary update in the race would have fixed it, and the loss
   would not last until Alt-Tab. A loss that lasts until a stop+run is a device-side
   "not playing" state.
4. `SDL_UpdateHapticEffect` on DirectInput (`SetParameters` without `DIEP_START`) does
   not restart a stopped effect. This explains why ongoing title updates never bring it
   back.
5. The 12:38:42 focus resume of slots 1, 2, 3 and 6 means the status was either
   "playing" at that focus loss, or the effect was still flagged running. Together with
   the silent-suppression rule, the likeliest reading is that the T300 driver keeps
   reporting a dropped infinite effect as **playing**. In that case any status-gated
   re-arm (the l47 watchdog) will not fire.

The likely trigger is the dense finite burst on slot 4: 28 starts in 0.76 s, many of
them two in the same millisecond, sent while the sim was paused. It follows a sim pause
and a timer-scale ramp. That matches the tester's "camera fly-by or game-speed change"
description. The device-side mechanism itself (firmware or driver) cannot be observed
from these logs.

## 3. Button index

Package -4 is a wheel-r2 trigger and, like -3, carries **no wheel section and no raw
button array** in `state.json` or the manifest. The l46 R2 path does not record one.
Index 9 therefore cannot be confirmed from the raw array. By construction it is still
certain: the log line `controller trigger source=wheel-r2` fires only on raw button 9
(`wheel_l2=8 wheel_r2=9`, confirmed again in -1's startup log). The tester says he
pressed L2. That is consistent with **physical L2 = raw 9 and physical R2 = raw 8**,
and the recommendation `mcla_wheel_diagnostic_buttons = "9,8"` stands. The physical
probe on the owner's T300 (`mcla_wheel_button_probe=true`) is still the only direct
proof.

## 4. Package -1 (F10, 12:31:30)

- **report.txt: all four answers are blank.** The same is true of -4's report.txt.
- The F10 was pressed at 12:31:30, in the menus, **22 s before the FFB effects
  existed**. The log contains only startup, `SDL_WHEEL_CONNECTED` (044F/B66E, 4 axes,
  13 buttons, 1 hat, haptic), `SDL_ACTIVE_CONTROLLER ... first-connected-wheel`,
  `SDL_WHEEL_FFB_DEVICE_INFO features=0x000D87FF` and `DEVICE_CONTROL command=4`
  (12:31:23-12:31:24). The -1 tail has no effect creation.
- The baseline effects come from the -2 tail (already received), created at
  12:31:52.210 in this order, each one create then start:

| Slot | Type | Duration | Guest params (create) | Host params (create) | Gain | Later first-update |
|---|---|---|---|---|---|---|
| 1 | spring | infinite | center 0, coeff +/-114, sat +/-25, deadband 0 | coeff 29412, sat 6425 | 100 | 12:31:54.079: coeff 89, sat 178 -> host 22962 / 45746 |
| 2 | constant | infinite | level 0 | level 0 | **0** (policy) | 12:32:34.893: level -25 -> host 0 |
| 3 | damper | infinite | coeff +/-76, sat +/-255 | coeff 19608, sat 65535 | 100 | 12:32:32.026: coeff 70 -> host 18060 |
| 6 | square (periodic) | infinite | magnitude 38, period 100 | magnitude 1953, period 100 | 40 | created and started 12:35:47.426 (after the loss) |

  The finite slots are 4 (square, 150 ms, magnitude 114 -> 24575, period 75, fade 20)
  and 5 (constant, 150 ms). Slot 6 did not exist at the race-start loss, so the periodic
  effect was not involved in that loss.

## 5. Will the planned resync fix this sequence?

Plan: a bounded, unconditional re-run of host-believed-running infinite effects on
pause->running, on racer re-activation, after finite bursts, and every 10 s.

**Expected yes.** It reproduces the one action proven to restore centering, a re-run of
the existing effect without new parameters, at the moments the loss occurs.

- Its triggers cover this exact case: pause->running at 12:34:02.845; burst end around
  12:34:02.9 or later; the second pause->running at 12:34:24.613; and the 10 s timer as
  a backstop. Any of these would have re-armed slots 1-3 within 0-10 s.
- It must be **unconditional**, ignoring `SDL_GetHapticEffectStatus`. Point 5 of the
  verdict suggests the T300 status lies.
- **Recommendation: mirror the Alt-Tab path as closely as is safe.** Alt-Tab does
  stop-all and then run. A bare `SDL_RunHapticEffect` on an effect DirectInput believes
  is still playing should restart it (DirectInput `Start` restarts a playing effect), but
  this has not been checked on the T300. If the first build with a run-only resync does
  not restore centering, the next variant is a per-slot `SDL_StopHapticEffect` followed
  by `SDL_RunHapticEffect` for the infinite slots only, so that finite effects in flight
  are not cut.
- **No evidence in -4 argues for re-sending parameters.** No create, retype or failure
  appears, and Alt-Tab works without any parameter update. A parameter re-send would
  only be justified if the resync leaves the effect playing but at zero strength. Cheap
  insurance: in the resync build, log the last host parameters per slot at each resync
  (rate-limited). A future package would then show directly whether the spring
  coefficients were neutral at the moment of loss.
- Keep the resync bounded. Do not re-run inside the 28-start burst itself, which could
  add to whatever overwhelms the device. Run it once after the burst has settled (for
  example 200-500 ms without a finite start) and on the pause->running edge.
- The damper (slot 3) and the 0%-gain constant (slot 2) should be re-run together with
  the spring, matching the set the focus resume restarted.

## 6. Other bugs in the two reports

- **None.** Both report.txt files (-1 and -4) are unanswered templates. The only bug
  reports from this session remain those in part 1: the freezes and transition hitches
  (-2), the centering loss (-3) and the frozen in-car steering animation (-5).
- In -1's startup log, the title probes the dev paths `t:\mc4\art\city\test_*.loc`
  (device not found). This is an existing benign title probe, not a tester-visible bug.
  The `gamecontrollerdb.txt does not exist` warning is also benign.
- In -4, the second transition (12:34:22-24) caused no FFB activity and no new hitch
  information beyond the part 1 minute summary (7 frames >100 ms in that minute). The
  perf audit armed at -4 is not in this tail.
