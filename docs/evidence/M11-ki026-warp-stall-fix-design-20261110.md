# KI-026 (Race Back camera softlock): warp stall-fix design

Read-only design by an Opus subagent on 2026-11-10, accepted by the primary agent as a design (nothing built, no code written). Extends the KI-035 state-20 warper hook (`src/mcla_ki026_probe.cpp`) to state 19 and adds a stall detector for two further failure modes described by the GPL forks (mechanism only, no code taken): camera finished but the completion callback never ran; camera never started. New cvar/dev key `mcla_warp_stall_fix`, default off for the first owner A/B. Spark not used: private generated-code reading, no bounded packet suitable.

---

# KI-026 (Red Light Race Back camera stuck) — design notes for extending the KI-035 warp fix

Read-only research, 2026-11-10. Nothing was built, run or edited in the repo. Citations: `src/` files at
main `f6e7140` (working tree); generated code `generated/default/mcla_recomp.N.cpp`. Fork facts are taken
only from our own survey text (docs/evidence/M11-external-repos-survey-20261108.md §4 row KI-035/KI-026
and recommendation 4). No GPL or LARecomp code was opened or copied.

## 1. What our KI-035 hook does today

Everything lives in `src/mcla_ki026_probe.cpp` (865 lines) with the pure logic in `src/mcla_warp_probe.h`.

| piece | where | guest address | what it checks | what it does |
|---|---|---|---|---|
| **finish-race fix** (cvar `mcla_warp_finish_fix`, init-only bool, **default true**, `mcla_ki026_probe.cpp:73`) | `REX_EXTERN(sub_8221B8C8)` `mcla_ki026_probe.cpp:581-718` | `0x8221B8C8` = mcWarper camera-finished callback (r3 warper, r4 event source, r5 finished transition) | `DecideFinish` (`mcla_warp_probe.h:279`): fix on; all reads ok; r3 == `*(0x8286D8D4)+0x51240`; **warper+288 == 20 only**; request(+68)!=0, r5!=0, warper+256==r4, r5==request+108, r5==warper+300 (`FinishMatched`, :274); request+0 and +3 non-zero (else `skip-request-pending`); the three exit addresses writable (else `refused-unwritable`) | stores the state-20 handler exit (`0x8221C1C8..DC`): `*(0x8286D8F0)+293 = 0`, warper+276 = 1, warper+288 = 29 (`:700-702`), then runs `__imp__sub_8221B8C8` (`:704`), which finalises via `sub_8221B790`. Logs WARN `MCLA_KI035_FINISH_RACE v=1 action=...` (≤200/process) and `MCLA_KI035_FINISH_RACE_DONE`; with `mcla_warp_probe` every callback `MCLA_KI035_FINISH_NOTIFY`. |
| state-20 watchdog (cvar `mcla_warp_watchdog_seconds`, default 0; dev key `mcla_warp_watchdog` writes 15) | `WarpObserve` `:476-520`, class `Watchdog` `mcla_warp_probe.h:346` | post-update of `sub_8221BE98` | state 20, request+camera stable, camera time known and not increased for N s (timer reset while sim paused) | same 3 exit stores only; **does not finalise** (no `sub_8221B790`, request left in place). |
| warp probe (`mcla_warp_probe`, dev key) | `WarpObserve` `:443-473` | post-update | — | `MCLA_KI035_WARP_STATE` on each state change, `MCLA_KI035_WARP_WAIT` every 5 s in state 20. |
| KI-026 watchdog (`mcla_ki026_watchdog`, default off, **no dev key**, command-line only) | `Ki026Watchdog` `:324-398`, called from `REX_EXTERN(sub_8221BE98)` `:560` | post-update | camera paused (UILogic+0x51382), sim running, no override byte `0x8286E800`, no cutscene, racer active (+656==3, +908==0); sig 1: warper idle 29 + empty queue for 20 s; sig 2: state 12/17 with +274 and no cutscene clock for 15 s | sig 2: +274=0 then `sub_8221B790`; both: `sub_82204468(*(*(0x8286D804)+48))` (unpause). `MCLA_KI026_WATCHDOG v=1`. |
| KI-026 probe (`mcla_ki026_probe`, default off, **no dev key**) | `:251-312`, `:524-558` | printf `0x82130000`, edges | — | `MCLA_KI026_WARPER_TEXT/PAUSE_REQUEST/GAME_CAMERA/EDGE/WARPER_STATE`. |
| F10 `guest_ki026` block | `:730-865` | — | always on, read-only | warper, request, warp camera, `warp_finish_fix`, `warp_finish_races`, `warp_finish_fixes`, watchdog counters. |

Gap: the event-driven fix acts **only in state 20** and only when the callback actually arrives. State 19
and "callback never arrives" / "camera never runs" have no fix (the 15 s watchdog is state 20 only and
leaves the request unfinalised).

## 2. Guest facts relevant to KI-026 (new reading of the generated code)

### 2.1 State 19 (`loc_8221C044`, `mcla_recomp.8.cpp:10627-10751`)
- Chosen by state 26 (`loc_8221C1F0`, :10869): after a +260 delay accumulator passes `*(0x82004288)`, if
  request+0 is set: `288 = 19 + (req+3 != 0)` → **19 when req+3 == 0, 20 when req+3 != 0**; else 28/29.
  Then +260 = 0.0 (`0x82000ED4`) and `sub_82219DA0` sets up and starts the zoom-in camera (stores
  request+108, warper+300, appends to the active list via `sub_82220980`).
- State 19 wait: proceeds only if **req+3 == 0** (`bne` at `0x8221C08C`), **req+0 != 0** (`beq` at
  `0x8221C098`) and **ready** (camera vfunc +92, `0x8221C068`; camera null → not ready). Only then
  `+260 += dt` (`0x8221C0CC/0x8221C0E4`) and exit when
  `+260 + 0.0001 >= duration - (*(0x827D7558) * *(0x82006BF0))` (`0x8221C0D8..0x8221C0F4`).
- State 19 exit stores (`0x8221C0F8..0x8221C10C`) are **identical** to state 20's: `*(0x8286D8F0)+293 = 0`
  (`stb r26`), +276 = 1 (`stb r28`), +288 = 29 (`li r10,29`).
- Observed healthy (docs/evidence/M10-l68-watchdog-arm-ki067-20261105.md §4): race-start warps are
  **type 4, 26 -> 19 -> 29**, and exit far before the transition ends (time 0.881/3.0, 2.847/5.2,
  2.500/3.024 s) with the camera still listed. So state 19 has a large lead (0.5-2.4 s): a pure dt race
  like KI-035 is unlikely there, but **ready is required every frame**, so if the camera finishes or never
  becomes ready while the warper is in 19, state 19 waits forever (no timeout, no other exit).
- A callback (`sub_8221B8C8`) arriving in state 19 is therefore always a stall: the camera's
  SetFinished clears ready (`sub_82228858`, l68 root-cause doc), and state 19 cannot exit without ready.
  Our hook passes it through today (state != 20): +300 cleared, +273 = 1, +276 = 0, no finalise.

### 2.2 The view override released by the callback (lead for the "aerial view stays" symptom)
- In `sub_8221B8C8`, when r5 == warper+300, the callback calls `sub_82224A28(UILogic + 0x10D50)`
  (`mcla_recomp.8.cpp:9550-9560`; `lwz -10028` from `0x82870000` = `0x8286D8D4`; `addis 1` + 3408).
- `sub_82224A28` (:31505) sets bytes +64, +65, +144, +145 of that object to 1. `sub_822249B8` (:31434)
  stores a 64-byte matrix and sets +64 = 0, +65 = !r5, +66 = r6; `sub_82224930` (:31349) is applied each
  frame from `sub_82212298` and passes the stored matrix to `0x822D5340` while the slot is active
  (+64 == 0 or +65 == 0), else (0,0,0). Reading: **a two-slot view-matrix override at UILogic+0x10D50**.
- The only direct setter `sub_822249B8` call is in `sub_82657678` (`mcla_recomp.50.cpp:1238`; callers
  `sub_82658F58`, `sub_8266BF08`, `sub_8266EA40`, i.e. the 0x8265-0x8266 race-over/race UI region that
  also holds the Race Back command `0x82666C50`). Releasers: the warper callback (above), `sub_822077D8`
  (script-bound region next to UILogic_PauseCamera, `mcla_init.cpp:4035`), nav map `sub_8261FF78`.
- So if the warper's callback **never runs** (or runs with r5 != +300), the override can stay active and
  the screen stays at the stored matrix while the car drives. This fits KI-026 better than the camera-pause
  hypotheses of M9-006 H1/H2. **Unverified:** the matrix meaning and whether Race Back uses this path
  were not traced; the F10 block does not read these bytes yet.

### 2.3 Fork mechanism (from our survey text only)
holdmysocks `src/warp_fix.cpp` (origin LARecomp `warper.cpp`), fix for their issue #6 "Red Light race
entry": after `__imp__sub_8221BE98`,
- **(A) transition completed, handler never invoked**: a state 19/20 transition that "was running and no
  longer is" → clear the "GPS interlock" (our `*(0x8286D8F0)+293`), +276 = 1, state 29, then call
  `sub_8221B790` (finalise) or the handler `sub_8221B8C8`;
- **(B) transition never started**: time out at transition duration (+76) + 3 s.
Keys: +288 state, +296/+300 camera, +276, transition +76 duration / +96 time.

Mapping onto ours: our callback fix is the event-driven subset of (A) for state 20 only. (A)-state-19 and
(A)-no-callback and all of (B) are not covered. Our 15 s state-20 watchdog is a partial (B) for state 20
without finalise.

## 3. Design (not implemented)

New host init-only bool cvar **`mcla_warp_stall_fix`** (default **false**), in `mcla_ki026_probe.cpp`;
pure decisions in `mcla_warp_probe.h` (unit-testable like `DecideFinish`). The existing
`mcla_warp_finish_fix` (state 20, default on, owner-confirmed l69) stays untouched.

### 3.1 Part 1 — event-driven: callback in state 19 (hook site `REX_EXTERN(sub_8221B8C8)`)
- Generalise `DecideFinish` to take the state: state 20 needs req0 && req3 (as today); **state 19 needs
  req0 && !req3** (the state's own wait gates). State 19 acts only when `mcla_warp_stall_fix` is on;
  state 20 keeps `mcla_warp_finish_fix`.
- Action: the same three exit stores (state-19 exit `0x8221C0F8..0x8221C10C`), then the original. The
  original then releases +300 (`sub_82224A28` = view override), pauses the camera if req+7, finalises.
- Why no false positive: state 19 exits only with ready = 1; a finish notification clears ready; so a
  callback in state 19 with the matching request/camera means the warper can never leave 19. Healthy
  state-19 trips are already in 29 when the callback arrives (l68: all three exited with the camera still
  listed). Same argument as KI-035, without the dt subtlety.

### 3.2 Part 2 — poll after `__imp__sub_8221BE98` (hook site `REX_EXTERN(sub_8221BE98)` `:524`)
Add `|| StallFixOn()` to the gate at `:529-530`. Per episode (enter 19/20, or request/camera change),
track: `seen_running` (camera ready && listed at least once), `episode_dt` (sum of the f1 dt the warper
receives), wall ms, `max_time`, `last_advance_dt`, `stuck_frames`.

Common preconditions (else no action): every read ok; warper == UILogic+0x51240; state ∈ {19,20};
request != 0; camera = warper+296 and == request+108 (when non-null); request flags match the state's own
wait (19: req0 && !req3; 20: req0 && req3) — if not, log `skip-flags-pending` once per episode, never act;
exit addresses writable; simulation not paused (pause-manager +4 && !+10 resets the counters, like
`Watchdog`).

- **Mode A (completed unseen):** `seen_running` and now ready == 0 and `active_index == -1` with the
  active list read ok (the post-finish signature from the l68 doc: ready 0, time 0, unlisted), for
  **≥ 3 consecutive post-update samples and ≥ 100 ms of episode dt**. Action: exit stores; then
  - if warper+300 == camera (callback never ran): set r3 = warper, r4 = `*(warper+256)`, r5 = camera and
    call `__imp__sub_8221B8C8` (it releases the view override, req+7 pause, finalises, +273 = 1);
  - else (callback ran earlier, +300 == 0, +273 == 1): call `sub_8221B790(warper)` (finalise only; the
    callback already did +300/req+7). Precedent: `Ki026Watchdog` sig 2 calls `sub_8221B790` from this
    same site (`:393-394`).
- **Mode B (never started / stalled):** `episode_dt > duration + 3.0 s` (duration via the existing
  getters; unknown or camera null → 10 s) **and** wall time over the same bound **and** camera time has
  not increased for ≥ 1.0 s of episode dt. Action: as mode A, but when the camera is still listed first
  remove it the way Warper_Abort does (`sub_8221E310`, called from `0x8221BC4C`) so a late finish of the
  same named transition cannot hit a later request. **First owner round: mode B acts only when the camera
  is unlisted (or null); listed-and-stalled is log-only (`observed-stalled-listed`)** until the abort
  path is read in full.
- **Signature 3 (log only):** warper 29 with an empty queue, racer active, sim running, and the
  UILogic+0x10D50 override slot active (+64 == 0 || +65 == 0) for 20 s: `observed-view-override`. This
  tests §2.2 without acting.

False-positive analysis:
- Healthy state 20 leaves to 29 one frame *before* the camera finishes (l68: 3.021/3.026, 3.009/3.024),
  so the post-update sample never sees 20 + not-ready + unlisted. Healthy 19 leaves while the camera is
  still listed. `sub_82219DA0` starts the camera synchronously (SetActive sets ready), so `seen_running`
  is true from the first sample in a healthy episode.
- 3 frames + 100 ms rules out a one-frame list reshuffle; the camera identity check (== request+108,
  == warper+296) rules out a different transition. `sub_82220980` has 14 callers (nav map etc.); if another
  system restarts the same named transition it becomes ready/listed again and A stops matching.
- B can only fire after duration + 3 s with a non-advancing camera; no healthy 19/20 episode outlives its
  transition (19 exits early, 20 at the end). Pause menus reset the timer. Alt-Tab: the game keeps
  running; dt-based accumulation plus the wall-clock floor means a long host stall cannot fire it early.
- Interaction: callback fix (state 20) and Part 1 (state 19) act first and leave state 29, so the poll
  sees nothing; keep `mcla_warp_watchdog` off in the arms (its 15 s non-finalising exit would race B).

### 3.3 Logging (follow `MCLA_KI035_FINISH_RACE` style)
- WARN `MCLA_KI026_CAMFIX v=1 action=<finish-cb19|poll-handler|poll-finalise|timeout-handler|timeout-finalise|observed-fix-off|observed-stalled-listed|observed-view-override|skip-flags-pending|refused-unwritable> mode=<cb19|A|B|S3> fix= state= request= type= req0= req3= req7= camera= class= ready= listed= active_index= active_count= time= duration= lead= seen_running= stuck_frames= episode_dt_s= wall_ms= camera300= notified273= exit276= view_override=<b64 b65 b144 b145> camera_paused= racer_active= sim_paused=`
  — at most 200 per process; logged also with the fix off (`observed-fix-off`), so the control arm
  records the signature. `lead` = `*(0x827D7558) * *(0x82006BF0)` (state-19 look-ahead, unknown value).
- `MCLA_KI026_CAMFIX_DONE v=1 state= request= camera300= notified273= view_override= camera_paused= complete=`
  after each action (expected: 29, request 0, +300 0, +273 1, override released).
- F10 `guest_ki026`: `warp_stall_fix`, `warp_stall_fires`, `warp_stall_observed`, and the UILogic+0x10D50
  override bytes (+64/+65/+66/+144/+145/+146), plus `lead`.

### 3.4 Dev-override key (`src/mcla_dev_overrides.h` conventions)
- Key **`mcla_warp_stall_fix`**, `Phase::kStartup`, plain host bool (cvar = key name, `=true` writes
  true, `=false` writes nothing), like `mcla_warp_probe` / `mcla_album_probe`. Add the header comment
  bullet, the `Key` enum entry, `kKeyCount` 75 → 76, the `kKeys` row next to the KI-035 rows (:394-402).
- Add `mcla_warp_stall_fix` to `kNeverWrittenCvars` (`src/mcla_m10_defaults.h:110`, array size 63 → 64).
- When the default flips to on later: add `mcla_warp_stall_fix_off` (writes false), as was done for
  `mcla_warp_finish_fix_off`, and keep `=false` of the plain key writing nothing.
- Recommended in the same change: dev key **`mcla_ki026_probe`** (plain bool) — today it is command-line
  only and owner builds cannot pass flags, so the KI-026 edge/printf probe cannot be enabled on owner PCs.
- Tests: `tests/mcla_warp_probe_test.cpp` (decision table for 19/20 × flags × matches; mode A/B state
  machine; generated mirror of the state-19 lines `0x8221C044..0x8221C10C`: `lbz r30,3`/`lbz r29,0`,
  `lwz r10,92`, `lfs f0,260`, `lfs f0,88(r7)` with `0x827D7500`, `lfs f13,27632`, the `bne/beq` gates,
  `lis -32121`, `li r10,29`, `stb r26,293`, `stb r28,276`, `stw r10,288`; the callback's
  `sub_82224A28` call with `-10028`/`addis 1`/`3408`); `tests/mcla_dev_overrides_test.cpp`;
  `tests/mcla_m10_defaults_test.cpp`; 4 KiB file limit check for the new arms.

## 4. Test plan (owner, PC; owner dev-override build; respect wait-for-idle before any build)

Arms (every key explicit; base = current owner arm with `mcla_warp_probe=true`,
`mcla_warp_watchdog=false`, `mcla_warp_finish_fix_off=false`, `mcla_ki026_probe=true` if added):
- **fix**: `mcla_warp_stall_fix=true`; **control**: `mcla_warp_stall_fix=false` (logs `observed-fix-off`).

Repro (both arms, PC 1080p **Unlocked**; check the log shows `guest vblank: 240 Hz` in gameplay, as in the
2026-10-28 recurrence on l33-sunc):
1. Start a **Red Light Driver** event (the 2026-10-28 recurrence was at a Red Light race *start*; the
   September one after a lost event).
2. Lose it deliberately; on the race-over screen choose **Race Back**; watch the zoom-out and the return
   to the car; check RB camera change and pause work. Repeat 10-15 times per arm in one session; also
   enter a few fresh Red Light starts and other race starts (state 19 path) and 2-3 garage trips
   (state 20 regression).
3. If the view sticks (control arm): F10, wait 10 s, F10 again, wait 20 s, then close. In the fix arm a
   stick should self-release within ~0.2 s (mode A) or duration + 3 s (mode B); press F10 once after a
   `MCLA_KI026_CAMFIX` line.
4. Send the session folder (mcla*.log, F10 packages) and the count of Race Backs / starts per arm.

What is looked at: per warp the `MCLA_KI035_WARP_STATE` sequence (26 -> 19/20 -> 29) with type and
req3 (this alone answers which state Race Back and Red Light starts use); every `MCLA_KI026_CAMFIX`
and `_DONE`; `MCLA_KI035_FINISH_RACE`; F10 `guest_ki026` (warper state, camera, view-override bytes,
camera_paused).

PASS for the first A/B:
- **No false positives:** zero `MCLA_KI026_CAMFIX` action lines (other than `observed-*`/`skip-*`) on
  trips that were visibly healthy; every warp in both arms reaches 29; garage and race starts unchanged.
- If a stall occurs: in the fix arm a `MCLA_KI026_CAMFIX action=<A or B action>` line, `_DONE` with
  state 29 / request 0 / override released, the view returns to the car, **pause and RB work**, and the
  next Race Back is healthy; in the control arm an `observed-fix-off` line with the same signature.
- KI-026 closes only after at least one recorded stall signature + fix-arm recovery (owner-confirmed);
  N healthy trips alone are not evidence (the bug is rare: 2 sightings since September).

## 5. Risks and unknowns (honest)

- **Which state KI-026 sits in is not known.** No guest state was captured in either sighting (the
  `guest_ki026` block postdates them). Candidates: 19 (this design), 20 (already fixed since l69 if Race
  Back uses req3=1: the 2026-10-28 recurrence on l33 **predates the l69 fix**), 12/17 cutscene (existing
  default-off `mcla_ki026_watchdog` sig 2), idle 29 with camera pause or view override left set (sig 1 /
  new S3 log). The first owner round is as much a measurement as a fix.
- **The September sighting was at stock timing** (pre-raw-timer, fixed dt), where the KI-035 dt race is
  impossible; so at least that sighting is not a dt race — consistent with mode A/B (camera never runs,
  or the event goes to a different listener) but not proven.
- The view-override reading (§2.2) is from code shape only; Race Back's use of `sub_82657678` is not
  traced. Mode A calls `sub_8221B8C8` / `sub_8221B790` from the post-update site; precedent exists, but
  these are guest calls the KI-035 fix avoided.
- Mode B with the camera still listed needs the abort path (`sub_8221E310`) read in full; hence log-only
  in round 1. Calling the handler while the camera is still running could let its later finish hit a
  later request using the same named transition.
- The state-19 lead `*(0x827D7558) * *(0x82006BF0)` is unknown (runtime data); the observed exits imply
  0.5-2.4 s. Logged by the new line.
- The fork's exact thresholds ("was running", duration + 3 s) are known only from our survey text; our
  persistence values (3 frames / 100 ms; duration + 3 s with a 1 s non-advance window) are our own.
- Probe cost: the poll reads ~20 guest words per UI frame only while in 19/20 (a few seconds per warp).
- Spark: not used by this subagent (private generated-code reading); the primary records the reason.
