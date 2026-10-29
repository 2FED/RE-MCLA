# KI-026 analysis: Race Back / Red Light race-start camera softlock (M9-006)

Read-only analysis by an Opus subagent on 2026-10-29, reviewed and accepted as
evidence by the primary agent. The three 2026-10-28 F10 packages hold no guest
camera state, so nothing here is physically confirmed. The diagnostic/watchdog
proposal in [M9-006-ki-026-proposal-20261029.diff](M9-006-ki-026-proposal-20261029.diff)
is not compiled or applied. Game strings were read in memory only; this
document cites guest addresses and minimal identifiers.

Status: read-only static analysis plus a read of the three 2026-10-28 F10 packages.
Nothing was built, launched or changed in the repository. The game image was decrypted
only in memory, to resolve names and debug strings. No image, key or asset was saved.
The proposal diff (`ki-026-proposal.diff`) is **not applied**.

## 0. Inputs and what they can and cannot show

- Ledger: `docs/known-issues.md` KI-026, `docs/M9-priorities.md` (KI-026 = time-boxed fix attempt,
  hybrid hook direction), `docs/evidence/KI-026-race-back-camera-softlock.md`.
- Earlier evidence that this note cites but does not redo: v2 trace run `20260904-062842-fb1043bf`.
  It recorded three healthy Race Back returns: 3 command selections and returns, 0 legacy handler
  calls, and 27 direct apply edges (6 at `0x822B3460`, 21 at `0x822B359C`). Each return began with
  `822B359C` about 1.7-2.0 s after the command, then `822B3460` about 0.63-0.66 s later, then two
  rapid `822B359C`.
- **New caveat on that trace.** The camera-apply function `sub_822B0F10` has **17** direct call
  sites in `generated/default`. The v2 probe traced 6 of them (`config/mcla_functions.toml`
  midasm hooks). The untraced sites are: `822551A0` (sub_82254ED0), `8228B9E4`, `8228BB74`,
  `822B1598` (sub_822B1490), `822B2A50` (sub_822B28C8), three in `sub_822B35E8`
  (`822B37E0`, `822B3898`, `822B3AE8`: the RB/LB camera-change paths), `8233679C`, `8264C7BC` and
  `82703B74`. The "healthy two-site sequence" is therefore a partial picture.
- F10 packages `live-20261028T125442Z-16636-1/2/3` (stored under
  `<profile>/diagnostics/<id>/live/`; a fourth `...125526Z-16636-4.partial` is an empty directory):
  - The process started 12:52:47Z. Gameplay began about 12:53:12Z (`UI cap: off (7312 draws)`).
    Guest vblank switched to 240 Hz at 12:53:32Z. F10 #1 was taken at 12:54:42Z, so the softlock
    started within roughly 90 s of entering gameplay.
  - Between dumps 1 and 2 (9.5 s), guest vblank advanced 2296 (240 Hz), output advanced 1126 frames
    (about 118 fps), and the CP saw 923,353 draws, about 820 per frame. Dumps 2-3 show the same
    rates. No device loss, no guest exception, no new log events after 12:53:49 except perf summaries.
  - Thread CPU deltas: render thread `XThread864C` about 96% of one core, `Main XThread` about 64%,
    and `GPU Commands` about 36%. **The game is alive and updating. No thread is idle-blocked.**
  - `race_back_trace` is `{0,0,0}`. The opt-in `mcla_race_back_probe` was off, so this build carries
    no camera trace data.
  - `process-private.dmp` is a MiniDumpNormal (about 645 KB). It holds no guest memory.
  - This l33-sunc log reports `mcla_raw_timer.cpp:220`, a line number that predates the l32
    timer-scale logger, so the timer scale (+84) was not logged.
  - The frame images are KI-042 stale/dark captures and were not used.
  - Conclusion: these packages contain **no guest camera state**. They prove liveness, 240 Hz pacing
    and a constant scene of about 800 draws per frame. They cannot discriminate between the
    hypotheses below. That gap is what section 3 closes.

## 1. Guest state machine (addresses, generated-file references)

Names come from MSVC RTTI and script registrations (`sub_82554798(name, fn)`) in the image. All
code references are `generated/default/mcla_recomp.N.cpp`.

### 1.1 Player camera (per racer)

- Local racer = `*( *(0x82874374) + 8 )` (player manager; `+136` is the render-thread copy, see
  `sub_822A39C8`). The camera controller **`mcPlayerCamera`** (vtable `0x820375B0`) is at
  `racer + 0x370`. Construction is in `sub_822B1B58` (file 13).
- `mcPlayerCamera` layout:
  - `+4`/`+8` owner racer.
  - `+16` **`camViewCS`**: `+48` = camera on screen, `+56` = active `camTransitionCS`, whose
    `+192`/`+196` are the from/to cameras (`sub_8271EB28`).
  - `+20` `camTransitionCS`, `+24` `mcCameraEffect`.
  - `+44..+60` five `camDynamicBoomCS` chase cameras, indexed by `+80` (the RB/LB cycle).
  - `+64` a sixth `camDynamicBoomCS`.
  - `+68`/`+72` `vehTrackCS`, `+76` `mcReplayCamera`, `+84` `mcCinematicCS` (`+784` sub-state
    11/12/13/17), `+96` `mcTrackCS`.
  - `+100` "spin cam" flag (script `spinCam`/`driveCam`, `sub_822152A0`/`sub_822151E0`, file 7).
- `sub_822B0F10(cam, mode r4, blend f1)` (file 13) applies camera `+44 + 4*idx`. Mode 4 is an
  instant cut (`sub_8271E930`); other modes blend (`sub_8271E9A0`). It then calls `sub_822A93A8`,
  which consults the camera-pause state, and prints "Game Camera is %s" through the nullsub printf.
- `sub_822B34B0` = **mcPlayerCamera::Reset**. It reads the preferred camera, sets flags
  `+88..+101`, and applies mode 4 at site **`822B359C`**. It is called from the racer
  reset/teleport functions `sub_822A6910`/`sub_822A69F8` (file 12) **only if `racer+656 == 3`**.
  Those functions also force the game time scale `*(0x827E1C34) = 1.0` and clear `racer+908`.
- `sub_822B33F0` = **mcPlayerCamera::RestoreGameCamera** (applies mode 4 at site **`822B3460`**).
  Callers:
  - `sub_822A3FF8`, the pause manager's **ResumeSimulation** handler;
  - script `driveCam` / `spinCam`.
- `sub_822B35E8` = **per-frame camera input** (RB/LB = buttons 12/13, look-back = 26). It is called
  from `sub_822A56C8` only when `racer+908 == 0`. It returns early (RB ignored) when any of these
  holds:
  1. `sub_82209150(*(0x8286D8D4))` is true. Among other things, this includes **UILogic camera
     paused (`+0x51382`) unless `*(0x8286E800)`**, GPS-jumping (`+0x51381`), and the UI state
     `MenuStylePaused`.
  2. A cutscene is playing: `sub_823CDC90(*(0x8286D8E8))` = `+20 == 0 && +12 != 3 && [+8]+120 >= 0`.
  3. The simulation is paused: `*(0x828CCEA0)+4 && !+10`.
  4. The racer has no input object, or `racer+48` is in a bad state.
  5. Inside the change path: `camViewCS` is mid-transition, or a `vehTrackCS` /
     `camDynamicBoomCS +64` / `mcCinematicCS` is on screen.

### 1.2 Pause manager (`*(0x828CCEA0)`, created in `sub_822D29A0`)

- `sub_822D29E8` (file 14) drains a 10-entry request ring at `+20` (index `+64`, count `+68`).
- Requests:
  - 1 **PauseSimulation**: freezes players through `sub_822A3C18`, which also switches cameras to
    pause/spin (`sub_822B32D0`). Sets `+4=1`.
  - 2 **ResumeSimulation**: restores every player camera (`sub_822B33F0` = site 822B3460), then
    `sub_822A3D80`. Clears `+4`.
  - 3/4 Pause/ResumeLocally.
  - 5 FastForward.
  - 6 FrameAdvance (`+10`).
  - 7 PauseOnly.
  - 8 ResumeOnly (no camera restore).
- While `+4 && !+10`, the main update `sub_822C1FA8` takes its paused path (no simulation, timer
  scale forced to 1.0). A drivable car therefore means the simulation was **not** paused.

### 1.3 The warp that produces the aerial zoom: `mcWarper`

- `mcWarper` (RTTI `.?AVmcWarper@@`) lives inline at **UILogic + 0x51240**, with
  UILogic = `*(0x8286D8D4)` (created in `sub_8220ED58`). The camera-pause writers reach the same
  object through `*(0x8286D804)+48`.
- Requests come from a 3-slot queue (`+44`, count `+64`, `sub_82219FE8`). Each request has byte
  flags `+0..+10` and a type `+12` (29 = none). Three flags are set by script:
  - `+1` Warper_SetIsEnterOnly
  - `+7` **Warper_SetPauseCameraAtEndOfWarp**
  - `+9` Warper_SetUseCinematicWarp
- `sub_8221A440` = **BeginWarp**. Callers: the nav map (`sub_82623748`), the race-over UI
  (`sub_8265D2B8`, `sub_8265D770`) and Finish. It sets `state(+288) = request type` and
  **deactivates the local racer** (`Racer_Deactivate` = `sub_822AB258`: `racer+908=1` and input
  off) unless `sub_82387A18()`. It then starts the zoom-out warp camera (`sub_82219BA0`, which
  clears any camera pause).
- `sub_8221B8C8` = **OnCameraFinished** (registered callback). If the finishing camera is the
  current request's warp camera (`request+108`):
  - if `request+7` is set, it **pauses the camera**: at the request matrix in states 12/17/18
    (`sub_82204558`), otherwise at the current view (`sub_82204480`, id -1);
  - it Finishes if `+276`;
  - it sets **`+273 = 1` ("ready to load")**.
- `sub_8221BE98` = **mcWarper::Update** (file 8, called each UI frame from `sub_821FC588`). It
  switches on `+288` (0..28; 29 = idle). The developers' own (stripped) debug strings name the
  waits:
  - states 0,1,4,7,10,15,24, in order:
    1. "waiting to be ready to load stuff" (`+273`);
    2. "waiting for time warping to return to 1.0" (exact `*(0x827D7500+84) == 1.0f`);
    3. "waiting for saving to complete / be successful";
    4. "waiting for cutscene to load";
    5. `m_CityPrestreamWaitFrames` (3 **UI frames**, `+268`);
    6. "waiting for city sectors to prestream … timing out in %f" (timeout accumulated from game
       time step `timer+92` into `+264`).
    Then "done waiting, starting replay recording/playback": it pushes **ResumeSimulation (2)**,
    loads, and goes to state 26.
  - state 23 "**RECORD WARP CAM MATRIX**": camera pause at the current view matrix
    (`sub_82204558`, sets `+280`).
  - state 26: sets up the zoom-in camera (`sub_82219DA0`, which **clears the camera pause**), then
    moves to 19/20 (cinematic zoom-in) or 28 and then 29.
  - **states 12/17 (warp-to-cutscene)**:
    - if `+273` and the cutscene launches (`sub_823C0D10` true): push **ResumeSimulation (2)** and
      set `+274 = 1`;
    - while `+274`: wait until `sub_823BBF60()` (cutscene time `[*(0x8286D8E8)+8]+120 >= 0`). Then
      Finish (`sub_8221B790`) and **release the camera pause** (`sub_82204468`, at `0x8221C94C`);
    - otherwise print "waiting on cutscene that has not been launched" or "... to complete loading
      assets".
    **None of these 12/17 waits has a timeout.**
  - `sub_8221B790` Finish (state 29, flags cleared, next queued request started) and
    `sub_8221BB90` Abort **do not release a camera pause**.
- Camera pause = UILogic **`+0x51382`**:
  - set by `sub_82204480` / `sub_82204558`;
  - `+0x51383` = requested (script UILogic_PauseCamera); `+0x51384` = id; the matrix is stored at
    `+0x51390..`;
  - cleared only by `sub_82204468` (callers `82219CB4`, `82219FD8`, `8221C94C`) or script
    **UILogic_UnPauseCamera** (`sub_822079E8`).

### 1.4 Healthy return, in this model

1. Race-over Race Back (`0x82666C50`, raceOverCommand("raceBack")) leads to BeginWarp. The zoom-out
   warp camera runs; the racer is deactivated.
2. The camera finishes. `+273` is set. The camera is frozen at the aerial matrix (`+7` or state 23).
3. The load waits pass, and **ResumeSimulation** is pushed.
4. The car is teleported or reset through `sub_822A6910`/`69F8`. This produces the first
   **`822B359C`** about 1.8 s after the command.
5. ResumeSimulation restores the camera (**`822B3460`**, about 0.65 s later).
6. The zoom-in or cutscene completes, the camera pause is released, and later resets produce the
   two further `822B359C`.

This matches the v2 timing exactly.

## 2. Ranked hypotheses for the stuck state

The observed symptoms:
- the aerial frame stays on screen;
- the car drives (the simulation runs and the racer is re-activated);
- RB does nothing;
- pause does nothing;
- the scene is constant at about 800 draws per frame;
- the guest is alive at 118 fps.

**H1 (most likely, about 45%). Warp-to-cutscene stall in mcWarper states 12/17.** A Red Light
race start has a racestart cutscene (`Race_ShouldPlayStarterCinematic`,
`CineScript_PickRacestartCutscene…`). In states 12/17, ResumeSimulation is pushed when the cutscene
launches, which explains why the car drives. The warper then waits, with no timeout, for the
cutscene clock to reach 0 while the camera stays paused at the recorded aerial matrix. That
explains the frozen view and the dead RB (gate 1.1-1). Pause is very likely refused while a
warp/cutscene is pending (`sub_82209258` and the UI read the same flags; this is not proven).
- Variant H1a: the cutscene was never launched (empty name; the "not launched" message). Here
  ResumeSimulation would *not* have been pushed, which fits "car drives" less well unless the
  simulation was never paused.
- Variant H1b: the cutscene launched but was killed or skipped before its clock reached 0
  (`CineScript_Camera_Kill`, `SKIPPING_CUTSCENE`, a player button press during the transition).
- Supports: the symptom set; the developer messages show that these waits exist; the trigger is
  specific to race starts.
- Against: none found. It is unverified because the packages have no guest state.

**H2 (about 30%). The warp finished but the camera pause was left set.** `request+7`
(SetPauseCameraAtEndOfWarp) makes OnCameraFinished freeze the camera. Release then relies on the
race script calling `UILogic_UnPauseCamera`. If the script's post-warp sequence for a lost
event / Race Back takes a branch that never unpauses, the warper is idle (29) but `+0x51382` stays
1. That gives exactly the observed symptoms, with the car already re-activated by the script.
- Supports: the same symptom fit, and Finish/Abort never clear the pause.
- Against: unknown; the script logic is in game data and was not read.

**H3 (about 10%). The warper is stuck in an earlier wait (states 0..24): ready-to-load `+273`,
time scale exactly 1.0, save, or prestream.**
- Against: the simulation would normally still be paused before "done waiting" pushes
  ResumeSimulation, and the racer is deactivated by BeginWarp, which conflicts with "car drives".
  The prestream wait also has a timeout.
- Keep it for the time-scale check, because it uses exact equality with 1.0. `sub_82653920`
  stores its target exactly, so this was not found to be a convergence trap.
- Abort paths (`sub_8221BB90` from the UI) that run after RECORD WARP CAM MATRIX would also leave
  the pause set. Those fold into H2's signature.

**H4 (about 10%). The camera pause was released but `camViewCS` stayed on a non-chase camera**
(`mcCinematicCS` `+84` or `vehTrackCS`), or a `camTransitionCS` never completed. RB would be
blocked by `sub_8271EB28` checks.
- Against: it does not explain the dead pause button or the frozen aerial view as cleanly. The
  F10 block below reports the on-screen slot.

**H5 (under 5%). A pause-manager imbalance or racer deactivation** (`racer+908 = 1`, PauseOnly
without ResumeOnly). This is contradicted by the drivable car: Racer_Deactivate also turns off
input, and PauseSimulation freezes players.

**Frame-rate dependence.**
- Every wait identified above is level-triggered: flags, an exact float, and the cutscene clock
  `>= 0`. None is a one-frame pulse, and the bug reproduced at stock 30 in September.
- Unlocked mode changes only the speed of some steps relative to streaming and script:
  - the 3-frame prestream wait is counted in UI frames (about 4x shorter at about 118 fps);
  - UI and warper updates run more often per simulation tick.
  That can shift ordering between the warper and the race script or cutscene (H1b/H2), but it
  cannot create the hang by itself.
- Verdict: frame rate is plausibly a frequency modifier, not the root cause.

## 3. Proposed diagnostic (decisive on the next occurrence)

The hooking mechanism already exists: every generated function is a weak alias
(`DEFINE_REX_FUNC` in `third_party/rexglue-sdk/include/rex/ppc/context.h`). A strong
`REX_EXTERN(sub_X)` in `src/` replaces **all direct calls** and can call the original
`__imp__sub_X`. `src/mcla_raw_timer.cpp` (sub_821BDA90 / sub_822D2478) already uses this, so no
codegen rerun is needed. The midasm-hook TOML used by the v2 probe is not needed either.

**A. F10 `state.json` block `guest_ki026`.** This is always on and read-only. Every pointer is
validated with `Memory::LookupHeap` + `QueryProtect`. It records:

- UILogic `*(0x8286D8D4)` and `*(*(0x8286D804)+48)` (expected equal); `+0x51381` gps_jumping;
  **`+0x51382` camera_paused**; `+0x51383` requested; `+0x51384` id; `*(0x8286E800)` override.
- mcWarper (UILogic+0x51240):
  - `+288` **state**, `+64` queue, `+68` request;
  - request `+12` type and flags `+0..+10` (esp. `+7`);
  - flags `+272..+280` (`+273` ready-to-load, `+274` cutscene wait, `+276`, `+280` recorded);
  - `+268` prestream frames, `+264` prestream timer, `+296` warp camera.
- Pause manager `*(0x828CCEA0)`: `+4/+6/+8/+10`, ring count `+68`, entries `+20..+56`.
- Timer: `0x827D7500+84` scale, `+92` step, `*(0x827E1C34)` game time scale.
- Cutscene player `*(0x8286D8E8)`: `+12`, `+20`, clock `[+8]+120`.
- Racer `*(*(0x82874374)+8)`: `+656` state, `+908` camera-input-off, `+904` pad.
- mcPlayerCamera: `+80`, `+100`, on-screen camera `camViewCS+48`, its vtable, the slot offset it
  occupies (44..96), and the transition `+56`.

Reading the result:
- `camera_paused=1, warper.state=12/17, flags[274]=1, cutscene.time<0` means H1.
- `camera_paused=1, state=29, queue=0` means H2.
- `camera_paused=1, state in {0..24}` means H3; the wait message (B) names which wait.
- `camera_paused=0` with `view_current_slot` not in 44..60 means H4.

**B. Opt-in `--mcla_ki026_probe=true` (default off; logging only).**
- A strong override of **`sub_82130000`**. This is the title's stripped debug printf: a bare
  `blr` with about 2,100 call sites. The override logs only exact formats: the mcWarper messages
  `0x8201F980..0x8201FCC4` (on change, then every 600 repeats), "Processing Request: %s"
  `0x82039E34` (pause-manager requests by name), and "Game Camera is %s" `0x820374F8`. The cost is
  two compares per call when enabled, and none when disabled.
- Edge logs with caller `lr` for:
  - CameraPauseAtCurrent/AtMatrix/Unpause (`82204480/82204558/82204468`);
  - script UILogic_PauseCamera/UnPauseCamera and Warper_SetPauseCameraAtEndOfWarp
    (`822079C8/822079E8/82207A48`);
  - Warper BeginWarp/Finish/Abort/OnCameraFinished (`8221A440/8221B790/8221BB90/8221B8C8`);
  - mcPlayerCamera Reset/Restore (`822B34B0/822B33F0`);
  - **Apply (`822B0F10`)**, where `lr` identifies all 17 sites, closing the v2 probe's gap.
- Warper state transitions from the `sub_8221BE98` override.
- This probe can stay enabled during normal owner play. Nothing in it fires per frame, apart from
  rate-limited warper wait text.

## 4. Minimal hook-level fix proposal (watchdog; default off)

This is in the same file, behind `--mcla_ki026_watchdog=true`, and is evaluated after each stock
`mcWarper::Update`. It fires once per episode and logs `MCLA_KI026_WATCHDOG`. It fires only when
**all** of these hold continuously:
- UILogic camera paused;
- simulation not paused;
- pause override not set;
- no cutscene playing;
- local racer active (`+656 == 3`, `+908 == 0`);
- and one of:
  - **signature 1 (H2)**: warper idle (29), empty queue, no current request, for **20 s**. Action:
    `sub_82204468(*(*(0x8286D804)+48))`, the same call the stock code and script unpause use;
  - **signature 2 (H1)**: warper state 12/17, `+274 = 1`, cutscene clock < 0, for **15 s**. Action:
    replicate the stock success exit at `0x8221C8C8`: `+274 = 0`, `sub_8221B790` (Finish), then the
    same unpause. The Career_Loading movie calls of that branch are skipped.

No fix is proposed for H3/H4 until the diagnostic identifies the wait. Skipping a load or save wait
blindly would be unsafe. Recommended order: ship A+B first, collect one occurrence, then enable
only the confirmed signature.

## 5. Risks

- **Legitimate camera pauses.** Story cutscenes, GPS jumps, garage/Rate-My-Ride transitions and
  scripted holds use the same flag. The guards require: no cutscene clock running, the simulation
  running, an active and controllable racer, an idle warper (sig 1) or a launched-cutscene wait
  (sig 2), and 15-20 s persistence. A scripted hold that legitimately lasts over 20 s with the car
  drivable is the residual risk; I found no such hold, but the script is unknown. The override byte
  `0x8286E800` is honoured.
- **Photo mode / pause menu.** These are assumed to use PauseSimulation (the simulation is
  frozen), which blocks the watchdog. This is **unverified**; photo mode should be checked once
  with the probe log before enabling the watchdog.
- **Sig 2 side effects.** If the cutscene starts later, it plays after the warper has finished
  (the cutscene owns the camera; probably harmless). The skipped Career_Loading movie hide could
  leave a loading overlay. The probe log will show whether that overlay is visible in the stuck
  state.
- **Address and offset assumptions.**
  - UILogic is assumed to equal `*(0x8286D804)+48`; this is checked in the F10 block.
  - Several meanings are inferred from code shape: `+656 == 3` (active), the cutscene clock, the
    pause-manager ring layout, and `0x82000ED4 = 0.0f`, `0x82001D14 = 1.0f`.
  - The strong overrides replace symbols that `mcla_app.cpp` and `config/mcla_functions.toml` do
    not override today. The midasm hooks sit at call sites and remain compatible.
- **Patch hygiene.** The diff was generated from the current worktree with CR stripped. Use
  `git apply --ignore-whitespace` on the CRLF files. It is not compiled.
- **Process.** This adds a source file and a JSON field. It needs the normal build, the
  diagnostics tests (state.json schema consumers) and owner testing. Nothing here is a fix claim:
  KI-026 stays open until a recorded occurrence shows the signature and a watchdog release.

## 6. Method notes (reproducibility)

- The helper scripts are in the job scratch directory: a caller index, a script-registration
  extractor, field-access and global-write finders, and an in-memory image reader for strings and
  RTTI that decrypts the admitted XEX with the SDK key in RAM only. Nothing derived from the image
  was persisted beyond short identifiers quoted here.
- Spark was not used. This subagent's scope was read-only, and the work depended on private
  game-image reads that must not leave the PC.
