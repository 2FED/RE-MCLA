# KI-026 (Race Back camera softlock): warp stall fix implementation

Implementation by an Opus subagent on 2026-11-11 of the accepted design
`docs/evidence/M11-ki026-warp-stall-fix-design-20261110.md`, host tree only, **default off**. Not
committed, no owner build staged (the primary folds it into the next build). No physical test has
run: nothing here is a PASS for KI-026. Spark not used: private generated-code reading and a
deterministic C++ change with unit tests; no bounded packet was suitable.

Licence: implemented from our own design text and guest addresses only; no LARecomp, holdmysocks or
RenanQueiroz code was opened or copied.

## 1. What was implemented

| part | where | default |
|---|---|---|
| host init-only bool `mcla_warp_stall_fix` | `src/mcla_ki026_probe.cpp` (`REXCVAR_DEFINE_BOOL`) | false |
| Part 1: state-19 exit in the camera-finished callback | `REX_EXTERN(sub_8221B8C8)` | gated by the cvar |
| Part 2: stall poll after `__imp__sub_8221BE98` (modes A, B, signature 3) | `StallObserve`, called from `REX_EXTERN(sub_8221BE98)` | acts only with the cvar; logs with a probe key |
| pure decisions | `src/mcla_warp_probe.h`: `DecideFinish` (state 19 added), `StallDetector`, `ResolveStall`, `ViewOverrideWatch`, thresholds | - |
| dev keys | `src/mcla_dev_overrides.h`: `mcla_warp_stall_fix` (=true writes true, =false writes nothing), `mcla_ki026_probe` (plain bool, true/false as `mcla_warp_probe`); 90 -> 92 keys | - |
| never-written list | `src/mcla_m10_defaults.h`: both cvars added (76 -> 78) | - |
| F10 `guest_ki026` block | `MclaKi026StateJson` | always on, read-only |
| tests | `tests/mcla_warp_probe_test.cpp`, `tests/mcla_dev_overrides_test.cpp`, `tests/mcla_m10_defaults_test.cpp` | - |

`mcla_ki026_probe` had no dev key before (command-line only, and a test asserted that); it now has
one, so owner builds can enable the KI-026 edge / printf / warper-state log. `mcla_ki026_watchdog`
stays command-line only.

"Observe" switch: the callback part and the poll run when `mcla_warp_stall_fix` is on, or, logging
only, when `mcla_warp_probe` or `mcla_ki026_probe` is on (`StallObserveOn`). With the fix off and
both probe keys off nothing new runs and the hooks behave exactly as before.

## 2. Hook sites and guest writes (generated-code references)

Citations: `generated/default/mcla_recomp.8.cpp` line numbers at the current main generation.

### 2.1 Part 1: callback in state 19 (`REX_EXTERN(sub_8221B8C8)`)

- `DecideFinish` now covers state 19: matched as for state 20 (`FinishMatched`: request != 0,
  r5 != 0, warper+256 == r4, r5 == request+108, r5 == warper+300; the original's own compares at
  9526-9547), then the state-19 wait gates: **req+0 != 0 and req+3 == 0** (`lbz r30,3` / `lbz r29,0`
  at 10636-10638, `bne` at 10671 and `beq` at 10677 to `loc_8221CB90`), else
  `skip-request-pending`; exit addresses writable, else `refused-unwritable`; then
  `finish-cb19`. State 20 keeps `mcla_warp_finish_fix` unchanged; the stall cvar does not gate
  state 20 and the finish cvar does not gate state 19 (unit-tested).
- Action: the three stores of the state-19 exit (10732-10742: `lis r11,-32121; li r10,29;
  lwz r11,-10000(r11); stb r26,293(r11)` at 10738, `stb r28,276(r31)` at 10740,
  `stw r10,288(r31)` at 10742; identical to the state-20 exit at 10855-10859), then
  `__imp__sub_8221B8C8`. The original then clears +300 and calls `sub_82224A28` (view override
  release, 9550-9563), pauses the camera when req+7 is set (state is now 29, so the
  `sub_82204480` branch at 9569-9599), finalises through `sub_8221B790` because +276 = 1
  (9621-9631) and sets +273 = 1 (9636).

### 2.2 Part 2: the poll (`StallObserve` after `__imp__sub_8221BE98`)

Runs once per warper update after the stock update (and after the existing KI-026 watchdog and
KI-035 observer), reading the state again. Common preconditions (`StallDetector::Observe`): every
read ok including the camera fields and the active transition list; warper ==
`*(0x8286D8D4)+0x51240`; state 19 or 20; request != 0; warper+296 == request+108 when non-null; the
request flags match the state's own wait (19: req0 && !req3; 20: req0 && req3) else
`skip-flags-pending` once per episode and never an action; sim not paused (pause manager +4 &&
!+10 restarts the dt / wall counters). An episode is (state, request, camera); any change starts a
new one.

- **Mode A** (completed unseen): the camera was seen ready && listed in this episode and is now
  ready known && not ready && not listed, for >= 3 consecutive samples **and** >= 0.1 s of their dt
  **and** >= 100 ms of wall time since the first stuck sample.
- **Mode B** (never started / stalled): episode dt > bound **and** wall time > bound (bound =
  duration + 3 s; 10 s when the camera is null or the duration unknown / negative / NaN), **and**
  the camera time has not increased for >= 1.0 s of dt. A non-null camera needs a known time getter
  (unknown time: never B). A camera still listed is **log only** (`observed-stalled-listed`, once
  per episode); it may act later in the same episode once unlisted.
- **Action** (`ResolveStall`, mode A `poll-*`, mode B `timeout-*`): fix off ->
  `observed-fix-off`; warper+300 neither the camera nor 0 -> `skip-camera-mismatch`; exit address
  not writable -> `refused-unwritable`; else the same three exit stores (`StoreWarpExit`, the
  10738/10740/10742 stores), then
  - +300 == camera (callback never ran): `__imp__sub_8221B8C8` with r3 = warper, r4 = warper+256
    (the original's own compare at 9526-9529), r5 = camera (`*-handler`);
  - +300 == 0 (callback ran earlier and left +276 = 0): `sub_8221B790(warper)`, i.e. what the
    callback does when +276 is set (9621-9631) (`*-finalise`). Precedent: `Ki026Watchdog`
    signature 2 calls `sub_8221B790` from this same site.
  Once per episode.
- **Signature 3** (log only, `observed-view-override`): warper 29, queue (+64) 0, racer active
  (+656 == 3, +908 == 0), sim running, view override slot active (UILogic+0x10D50 +64 == 0 or
  +65 == 0) for 20 s; once per continuous signature. In state 29 the poll reads only the two
  slot bytes per frame unless a slot is active.

Guest writes by host code: only the three exit stores above. Everything else is guest code the
title runs on a healthy trip (the callback or the finalise).

### 2.3 `sub_8221E310` read in full (design open question)

`sub_8221E310(r3 = *(UIManager)+52 transition manager, r4 = transition)` (16225-16363): loads the
u16 count at +0x40854 (`ori 2132`), scans the pointer array at +0x40850 for r4, and on a hit calls
five virtual functions of that entry in order: slot +96 (r4 = 0), +64 (r4 = 0), +112, +388, +128.
It does not itself change the array or the count (no store in the body); the removal, if any,
happens inside those virtual calls, whose targets were not resolved (vtable data is not in the
generated code text). In `Warper_Abort` (`sub_8221BB90`, 9931-10064) it is called **after**
`sub_8221B790` (10022), which has already stored request+108 = 0 and warper+68 = 0 before moving the
request to the free list; so on that path it only runs when the finalise began the next queued
warp (`sub_8221A440`) and then targets the new request's camera. Conclusion: there is no clean
"remove this camera" path to reuse; mode B with a listed camera stays log-only as the design asked.

## 3. Why healthy trips do not trigger (false-positive argument)

- **State 20** leaves to 29 one frame before the camera finishes (l68: 3.021/3.026, 3.009/3.024),
  so a post-update sample never sees 20 + not ready + unlisted. When the KI-035 race happens the
  finish fix acts inside the callback (before the warper update) and the poll sees 29.
- **State 19** leaves while the camera is still listed (l68: 0.881/3.0, 2.847/5.2, 2.500/3.024).
  The state-19 exit uses its own +260 accumulator (`lfs f0,260` / `stfs f12,260`, 10699/10720)
  against duration - lead, so a healthy episode never outlives duration + 3 s either.
- **Camera started in the same call** (`sub_82219DA0` from state 26): `seen_running` is true from
  the first sample; a camera that is not yet ready / listed is not "finished" until it was seen
  running (unit-tested).
- **A list reshuffle** of one or two frames resets the stuck count (3 samples, 0.1 s dt, 100 ms).
- **A different transition**: camera identity (== request+108, == warper+296) and the episode key.
- **Mode B** requires a non-advancing camera for 1 s, both dt and wall bounds (a host stall with a
  huge dt, or a slowed game with tiny dt, cannot fire it early), and acts only on unlisted / null
  cameras. Pause restarts the counters.
- **Flags**: a request whose flags do not match the state's own wait is never acted on.

## 4. Log formats

```
MCLA_KI026_CAMFIX v=1 action=<finish-cb19|skip-request-pending|refused-unwritable|poll-handler|
  poll-finalise|timeout-handler|timeout-finalise|observed-fix-off|observed-stalled-listed|
  observed-view-override|skip-flags-pending|skip-camera-mismatch> mode=<cb19|A|B|S3|poll> fix=
  state= request= type= req0= req3= req7= camera= class= ready= listed= active_index=
  active_count= time= duration= lead= seen_running= stuck_frames= episode_dt_s= wall_ms=
  camera300= notified273= exit276= view_override=<b64>,<b65>,<b144>,<b145> camera_paused=
  racer_active= sim_paused= complete=
MCLA_KI026_CAMFIX_DONE v=1 action= state= request= camera300= notified273= exit276=
  view_override=<b64>,<b65>,<b144>,<b145> camera_paused= complete=
```

(one line each in the log). WARN level, at most 200 `CAMFIX` lines per process. Callback lines
carry `seen_running=-1 stuck_frames=0 episode_dt_s=-1 wall_ms=-1`; S3 lines carry the 20 s hold in
`wall_ms`. `lead` = `*(0x827D7558) * *(0x82006BF0)` (the state-19 look-ahead, 10695-10725). The
state-19 callback case is logged as `CAMFIX`, not as `MCLA_KI035_FINISH_RACE` (that line stays
state 20 only); `MCLA_KI035_FINISH_NOTIFY` (warp probe) shows `observed-fix-off` or `finish-cb19`
for it. Expected `_DONE` after an action: state 29 (or the next warp's state), request 0 or the
next request, camera300 0, notified273 1, view override 1,1,1,1.

F10 `guest_ki026` new fields: `warp_stall_fix`, `warp_stall_observe`, `warp_stall_fires`,
`warp_stall_observed`, `ki026_probe`, `view_override {b64, b65, b66, b144, b145, b146}`,
`state19_lead`, `state19_lead_scale`, `state19_lead_factor`.

## 5. Tests and gates

- `mcla_warp_probe_test`: 38129 checks (new: state-19 decision table incl. all 8 match masks and
  states 0..40; healthy 20 at 60/144/240 Hz, healthy 19, same-call start, late start; mode A at
  40 Hz and 240 Hz with exact firing sample, reshuffle reset, listed / ready-unknown non-triggers,
  pause; mode B never-started, listed-then-unlisted, null camera, advancing then frozen time, host
  stall vs wall floor, slowed game, unknown time, unknown duration, pause restart; preconditions
  (flags pending once, other states idle, camera mismatch, incomplete, other warper, no request,
  new episode); `ResolveStall` table; `ViewOverrideWatch`; generated-code mirror of the state-19
  wait and exit (10627-10742), the callback's `sub_82224A28` call (`-10028` / `addis 1` / `3408`),
  `sub_82224A28`'s four stores, `sub_8221B790`'s finalise stores, the `Warper_Abort` order and the
  `sub_8221E310` scan).
- `mcla_dev_overrides_test`: 1498 checks (was 1471 at l91); `mcla_m10_defaults_test`: 5581 (was
  5421).
- Gate build (`gate_host_build.ps1 build`, after wait-for-idle): 24 steps, 0 warnings, 0 errors.
- Host tests (`l57_host_tests.ps1`): count 56, failed executables: 0.
- `ast-grep scan -c sgconfig.yml` on the 7 changed files: clean; `git diff --check`: clean.

## 6. Differences from the design

- Mode A persistence: "3 samples and 100 ms of episode dt" is implemented as 3 consecutive stuck
  samples **and** 0.1 s of dt over those stuck samples **and** 100 ms of wall time (stricter than
  episode dt).
- `skip-camera-mismatch` (warper+300 neither the camera nor 0) and `mode=poll` for
  `skip-flags-pending` are additions; the design had no action for that case.
- Mode B with a non-null camera requires a known time getter (the non-advance window cannot be
  proven otherwise).
- `mcla_ki026_probe` is a plain bool key (=false writes false, its default); `mcla_warp_stall_fix`
  follows the design (=false writes nothing). Both keys are appended at the end of the key table
  (as l90/l91 did), not next to the KI-035 rows.
- "Probe key on" for `observed-fix-off` = `mcla_warp_probe` or `mcla_ki026_probe`.
- The poll calls `__imp__sub_8221B8C8` (not the hooked symbol), so its own `EDGE` /
  `FINISH_NOTIFY` lines do not appear for that call; the `CAMFIX` / `_DONE` lines cover it.

## 7. Not done / risks

- No runtime test; the stall has never been captured with guest state. Which state KI-026 sits in
  is still unknown (design 5).
- Mode A/B call guest code (`sub_8221B8C8` / `sub_8221B790`) from the post-update site instead of
  the transition-list update; the finalise may start the next queued warp there.
- `mcla_ki026_probe` also enables the existing `MCLA_KI026_EDGE` lines on 13 functions including
  `PlayerCamera_Apply` (`sub_822B0F10`, 3 call sites in the player-camera input update); the log
  volume with the key on is unmeasured.
- The view-override meaning (design 2.2) is still a code-shape reading; signature 3 only logs.
- Mode B on a listed camera is log-only; if KI-026 is that case, round 1 records it but does not fix
  it.

## 8. Owner test plan (from the design, section 4)

Arms (every key explicit; base = current owner arm with `mcla_warp_probe=true`,
`mcla_warp_watchdog=false`, `mcla_warp_finish_fix_off=false`, `mcla_ki026_probe=true`):
**fix** `mcla_warp_stall_fix=true`; **control** `mcla_warp_stall_fix=false` (logs
`observed-fix-off`).

1. PC 1080p **Unlocked** (check `guest vblank: 240 Hz` in gameplay). Start a **Red Light Driver**
   event.
2. Lose it deliberately; on the race-over screen choose **Race Back**; watch the zoom-out and the
   return to the car; check RB camera change and pause. Repeat **10-15 times per arm** in one
   session; also a few fresh Red Light starts and other race starts (state 19) and 2-3 garage trips
   (state 20 regression).
3. If the view sticks (control): F10, wait 10 s, F10 again, wait 20 s, close. In the fix arm a
   stick should self-release within about 0.2 s (mode A) or duration + 3 s (mode B); press F10
   once after a `MCLA_KI026_CAMFIX` line.
4. Send the session folder (mcla*.log, F10 packages) and the count of Race Backs / starts per arm.

PASS for the first A/B: zero `MCLA_KI026_CAMFIX` action lines (other than `observed-*` / `skip-*`)
on visibly healthy trips, every warp reaches 29, garage and race starts unchanged; if a stall
occurs, a fix-arm action line, `_DONE` with state 29 / request 0 / override released, the view
returns, pause and RB work, the next Race Back is healthy, and in the control arm an
`observed-fix-off` line with the same signature. KI-026 closes only after a recorded stall
signature plus a fix-arm recovery confirmed by the owner.
