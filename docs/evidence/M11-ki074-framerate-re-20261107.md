# KI-074: what in the game changes above ~36 fps (static RE, 2026-11-07)

Read-only RE by an Opus subagent on 2026-11-07 (recompiled game, host timer/swap code); accepted by the primary agent.

Read-only RE by an Opus subagent. Sources: `generated/default/mcla_recomp.*.cpp` (cited as `TUn:line`
= `mcla_recomp.n.cpp:line`), the retail XEX expanded in temp only to read strings, vtables and RTTI
names (same method as `docs/evidence/M11-ki047-lock-fence-re-20261107.md`; image deleted afterwards),
host `src/mcla_raw_timer.cpp`, the l76 SDK worktree (`mcla/m11-steady3` a4a5309,
`src/graphics/graphics_system.cpp`), and the M8/M9 timer evidence. Nothing was built, launched or changed.

## 0. Result in short

- **The game was written for a fixed 1/30 s step. The shipping host gives it a variable step.**
  `mcla_raw_timer` (default ON, Locked and Unlocked alike) feeds the main timer the smoothed measured
  frame time. So the game's dt is about 33-34 ms in Locked, 28-30 ms in base Unlocked (33-36 fps) and
  24-25 ms in the steady arms (40-42 fps). The KI-074 boundary (~36 fps = 27.8 ms) lies between the
  last two. Every game-side dt consumer runs with a step it was never tuned for. Below 1/30 s that had
  never happened before the Unlocked work.
- **Physics runs `physicsupdates` (default 2) substeps per frame with dt/2**, then one non-physics pass.
  At 30 fps that is 2 x 16.7 ms (60 Hz physics); at 41 fps 2 x 12.2 ms. The game adds a third substep
  (and a 10-frame dt average) only in a network session or with the `extraoversample` debug parameter.
- **Traffic (the ambient manager) runs on its own guest thread, "[MC] Ambient Traffic".** The main update
  kicks it once per frame after the physics substeps and joins it near the end of the update. It
  reads the global timer step itself. Placement is time-based (dt), not frame-counted.
- **City sector streaming is distance-driven.** No frame-count timeout was found in the sector state
  machine. The only frame-counted pieces are a 3-update settle counter before the next page-in and
  render-side helpers that use the render frame counter (`frame & 3`, `frame & 1`, `frame % 3`, "seen
  last frame", 8-frame history). These run once per rendered frame, independent of vblank.
- **The game never reads the vblank counter for timing.** Game time comes from `mftb` only (no
  KeQueryPerformanceCounter call). The vblank count feeds only the D3D swap scheduler. So a "vblank step"
  of 6 instead of 8 per frame cannot cause KI-074, and vblank-step normalisation is not a mitigation.
- **A 36 fps cap needs no new code:** `--mcla_guest_vblank_hz=72` (the game swaps every 2nd vblank).
  The swap scheduler flips a late frame at once instead of waiting for the next pair, so a cap does not
  fall to 24 fps.
- **Not found statically:** one specific dt- or frame-count-sensitive routine that explains floating
  cars or texture flicker. Section 4 ranks the candidates, and section 5 gives flag-only experiments
  that separate "dt magnitude" from "frames per second" from "GPU-bound pacing" without a build.

## 1. How the game computes dt

### Timer object 0x827D7500 (`+8` = 0x827D7508 is the effective step every system reads)

- Tick `sub_821BDA90` (TU5:3033): `+92` = measured seconds from `mftb`, `+88` = caller-supplied
  step (the host override), `+8` = effective step. Fixed mode (`+60 = 1`, the stock config) uses
  `+32 = 1/30 * scale(+84)` and ignores measured time. Raw mode (`+60 = 0`, set by config `+72 = 1`)
  uses the measured or supplied step, clamped to about 0.0001..0.1 s
  (`docs/evidence/M8-001-timebox-20261025.md` "The code"; `docs/evidence/M8-timer-policy-replay-20261015.md`).
- Host (`src/mcla_raw_timer.cpp`): `sub_822D2478` override (`:223`) switches the stock fixed30 config
  to raw (`+88` 30 -> 0, `+72` 0 -> 1). `sub_821BDA90` override (`:185-218`) supplies `f1` = the
  exponentially smoothed measured step (`StepSmoother`, `:88-123`, weight 0.05, hold +-10 %), rounded
  to whole ms (`:127-141`). Steps above 0.1 s apply 0.1 s and carry the rest as debt, up to 1 s
  (`kHitchSeconds`, `:76`). There is **no lower bound**: the step follows the frame rate down to 0.
- Config object (`*0x828CCEA0`, 108 bytes, vtable 0x8203BE80; ctor `sub_822D2818` TU14:31678; param
  read `sub_822D2548` TU14:31241). Command-line params `framerate`, `framelock`, `realtime`,
  `swapearly`, `noswapearly`, `physicsupdates` (registrations `sub_827ABE98`..`sub_827ABF38`, TU61).
  `cfg+16` = **physicsupdates, default 2** (ctor `li r9,2; stw r9,16(r31)`), and vtable +24
  (`sub_82618698`) returns it.

### Main update `sub_822C1FA8` (TU13:59203), once per frame from main loop `sub_82131508`

1. Timer tick through the vtable (`f1 = -1`, i.e. no caller override: the host hook supplies it).
2. `N = cfg->vfunc24()` = physicsupdates (TU13:59706). Only if `extraoversample` (0x8287480C, read
   TU13:59725) or a network session (`sub_82387A18` -> net manager 0x8286F1D8, TU13:59731) is set:
   `N += 1` when measured `+92 > N * 0.02` (TU13:59772-59781). Then `total = min(max(+92, +88),
   N * 0.02) * scale`, pushed into the 10-sample average `sub_822BF2E8` (TU13:52230, ring at
   0x828747B0), and `+8 = average` (TU13:59831 ff.). **Single-player skips this.** The averager is
   reset and `+8` stays the tick's step.
3. Substep loop `r23 = N .. 0` (`addic. r23,r23,-1; bge`, TU13:60224):
   - `r23 > 0`: `sub_821BD910(timer, 1, N)` (TU5:2782) saves `+8` and sets `+8 = dt/N`,
     `+73 = 1`, `+76 = N`.
   - `r23 = 0`: `sub_821BD910(timer, 0, 0)` restores `+8 = dt` and `+76 = 0` (TU13:59910).
   - Each pass updates layer 1 (`sub_822C52C8(layers, 1, +8)`, TU13:59965) and the per-object lists
     at 0x82874374 / 0x8286EBE4 / 0x8286D8E0 (vfunc +104).
   - Layer 1 is **mcLayerCity**. Its update `sub_822C7870` (TU14:5177) branches on timer `+76`:
     split -> physics world step(s) with dt/N (`sub_825883C0`, world vfunc +4, `sub_82588428`,
     `sub_82476238`); unsplit (final pass) -> the non-physics city update.
   - So: **2 physics steps of dt/2 plus 1 city pass of dt per frame.**
4. `stfs f28,8(r27)` restores the full dt (TU13:60250). The ambient thread is kicked
   (`sub_826D9578`, TU13:60274). Layers updated once with the full dt: 4 mcLayerPlayer (TU13:60358),
   0 mcLayerSystem (TU13:60388), 8 mcLayerCinematics (TU13:60447; uses `+88` while paused) and
   2 mcLayerAmbients (TU13:60529; only when the ambient thread is not used). Layer map: switch in
   `sub_822C5070` (TU13:66703) with RTTI of each layer vtable.
5. Ambient thread joined (`sub_826D95B0`, TU13:61263). Then the coordinator hands the frame to the
   render worker (`sub_82305D00` -> `sub_82305E38`; see
   `docs/evidence/M8-frame-update-handoff-design-20261013.md`).
- A second timer splitter, `sub_822A7E28` (TU12:68964, `sub_821BD910` at TU12:69057), is reached from
  mcRacer/mcOpponent/mcAiRacer/mcPolice vfunc +72 (`sub_822AC490`). Racers split the same global timer
  for their own substeps.

**Answer to (1):** variable step, no fixed-step accumulator in single-player. dt is clamped only by
the 0.1 s ceiling (and the 0.0001 s floor) in the raw path, plus `N * 0.02` in network mode. The
game does not expect dt >= 1/30, but it was only ever tuned at exactly 1/30, with 2 x 1/60 physics
substeps. "dt clamps at 0.1" in the pursuit RE is this raw-path ceiling.

## 2. Traffic, streaming and frame-counted logic

### Traffic (ambient manager, `mcAmbientMgr` singleton at 0x8286D8E4)

- Thread "[MC] Ambient Traffic": created by `sub_826D9638` (TU54:4866) from `sub_826D89F0`, with
  5 request/done event pairs. Body `sub_826D9448` (TU54:4540), jump table 0x826D94B8:
  - slot 0: quit.
  - slot 1: **per-frame update `sub_826D75D8`**, kicked asynchronously by `sub_826D9578` and joined by
    `sub_826D95B0`.
  - slot 2: reset `sub_826D8B48`, synchronous (`sub_826D95B8`).
  - slot 3: `sub_826D56F0`, synchronous (`sub_826D9530`).
  - The non-threaded path is mcLayerAmbients::Update `sub_822D3C20` (TU14:34856, vtable 0x8203C2C0
    slot +28), which calls `sub_826D75D8` directly.
- `sub_826D75D8` (TU54:3) takes its step from the global timer **at the moment it runs on the thread**:
  - `f30 = timer+8` (TU54:545).
  - In network mode: `f30 = min(timer+92, 0.05)` (TU54:559-563).
  - It accumulates an ambient clock at mgr+0x64238 and counts down mgr+0x64244 by the step.
  - It passes `timer+88` (TU54:588) to `sub_826ED250`.
  - Placement and spawn/unspawn work is therefore **time-based**; no "N spawns per frame" constant
    was found in the parts read.
- **Concurrency.** The thread runs between the kick (TU13:60274) and the join (TU13:61263). Meanwhile
  the main thread updates the Player/System/Cinematics layers and racers (`sub_822A7E28` re-splits the
  same timer). A thread that reads `+8` during a racer split sees dt/N. This is a latent race. Its
  exposure depends on thread timing, not directly on fps.
- Ambient wheels and ground: mcAmbientCollider vfunc +56 `sub_826FA0F8` (TU55:19816) casts one probe
  per wheel (`sub_826E5540` builds the segments with 0.5, the world test is `sub_827974F0`):
  - hits with `|normal.y| < 0.7071` are discarded;
  - the suspension update `sub_826FBA60` gets `dt = timer+8` (0 while paused).
  mcAmbientPhysInst vfunc +108 `sub_826FA8A8` (TU55:21031) re-validates the pose every 0.5 s of dt,
  for a counted number of times. Both are dt-scaled, not frame-counted. A missed probe
  (missing/unstreamed collision under the car) leaves the car where its path puts it. Static reading
  cannot tell whether dt affects this; the 1/30 s step is the only tuned point.
- Spawn on terrain: `sub_826E4A48` logs "[Ambient] Triggered ambient '%s' could not spawn on terrain
  at %f, %f, %f" (0x820C26D8) through the stripped printf `sub_82130000`.

### City streaming

- Sector state machine `sub_822D9A28` (TU14:49411), states 1-5:
  - 1: schedule with priority `-(dist + 2003)`;
  - 2: wait until the resource flags read resident;
  - 3: resident, re-prioritise on distance change;
  - 4: evict;
  - 5: wait for eviction, then back to state 0.
  - The per-sector age `+272` counts updates but **gates nothing**. Distance-driven, not
    frame-counted. Its debug prints ("Sector %d scheduled for paging...", "Evicting Sector %d...",
    "Sector %d Evicted...", "EvictionCount = %d") are gated by the byte 0x8287C7DB and go to the
    stripped printf.
- City update `sub_822D5A30` (TU14:39517):
  - a smoothed streaming-lookahead value using `timer+8`;
  - a real-time check (`mftb`, 2 s / 4 s);
  - a **3-update settle counter** (0x8287C780 must exceed 2 after the eviction count reaches 0)
    before choosing the next page-in (`sub_822D9D30`);
  - `frame & 3 == 0` re-arms one state (`+32 = 5`).
  - At 41 fps the settle is 73 ms instead of 100 ms: minor, and it would reduce flicker if anything.

### Render frame counter 0x827D2DCC

- Incremented once per rendered frame in `sub_8217B7B0` (TU2:45981, store TU2:46019). It does not
  depend on vblank.
- Readers with frame-count semantics:
  - `sub_822EA7C0` (TU15:20416): `frame % 3` ring index;
  - `sub_8230DDD0` (TU16:45067): round-robin mask `1 << (frame & 3)` or `frame & 1`; a render-target
    object (reflection/cubemap type) updated in parts, called from `sub_8230EE20` <- `sub_823115E8`;
  - `sub_822FC250` (TU16:741) and `sub_822FB058` (TU15:62204): "was it visible in frame-1"
    continuity and per-frame countdowns;
  - `sub_822FBDB0` (TU16:3): an 8-frame position/frame history, clamped to 8;
  - the vinyl/chunk streaming logs (`sub_823720C0` "PlaceChunk (0x%p), frame = %d").
- At 41 fps these run 37 % more often per second, and frame-counted fades and countdowns finish 27 %
  sooner. They are candidates for light/flare/reflection effects (KI-041, KI-043), less so for
  building textures or traffic.

## 3. Host side: what the guest sees in Unlocked mode

- Vblank thread (`graphics_system.cpp:186-245`, l76 SDK):
  - the rate is `mcla_guest_vblank_hz` (launcher Unlocked = 240; `:43-46`, hot-reloadable);
  - stock 60 Hz applies for the first `mcla_guest_vblank_unlock_delay_s` = 45 s and while the
    draw-count UI cap holds (<= 250 draws);
  - 1 ms sleep loop with catch-up `MarkVblank` calls (`:229-235`);
  - `MarkVblank` (`:427-443`) bumps the CP counter and dispatches the guest's GPU interrupt.
- Guest vblank handler `sub_82419718` (TU27:23347):
  - increments `device+16548` (vblank count);
  - flips queued swaps whose target vblank has been reached;
  - calls an optional user vblank callback at `device+16544`; no game store to it was found.
- Swap scheduler `sub_82419818` (TU27:23495): `target = last_target + interval`, interval 2
  (`sub_824199B0`, `+13596 = 2`).
  - If that target has already passed, `target = current` (+1 only when the swap-early percentage
    test says so), and the frame flips without waiting.
  - **Result: the vblank rate caps the frame rate at hz/2 with no extra quantisation.** A late frame
    is not pushed to a third vblank.
- CP-bound (base) vs GPU-bound (steady), as the guest sees them:
  - the same 240 Hz vblank in both;
  - the vblank count advances about 7 (base) vs about 6 (steady) per frame, but **no game code
    reads it except the swap scheduler**;
  - render-thread waits move from the CP rule-c read-back waits (base) to GPU fence/throttle waits
    `sub_82411E98`/`sub_82412F98` (steady);
  - dt shrinks from 28-30 to 24-25 ms;
  - no host frame skipping or present throttling is involved.
  The KI-035 warp state machine is a UI-only path (mcWarper state 20) and does not run during
  driving.
- **Frame cap options:** the launcher has only Locked (no argument) and Unlocked (`--mcla_guest_vblank_hz=240`,
  `TimingLaunchArguments`, `src/mcla_launcher_capabilities.h:267-269`). A 36 fps cap is
  `--mcla_guest_vblank_hz=72` (35 fps: 70; 40 fps: 80). It is a launcher-only change: one more
  timing code and the per-build allowlist gate of M9-003. The SDK already documents "120 caps at
  60 FPS", and the M8 120 Hz arm ran 40 fps (`docs/evidence/M8-001-timebox-20261025.md` test 4).
  Caveat: the 1 ms sleep granularity gives +-1 ms jitter per vblank, harmless at 13.9 ms periods.

## 4. Most plausible mechanisms per symptom (ranked; none proven)

**Floating traffic car, despawned cars**

1. *dt below the tuned 1/30 s in the ambient/traffic and physics code* (medium):
   - Placement, ground validation and suspension all scale with `timer+8`; physics substeps drop
     from 16.7 to 12.2 ms.
   - Any threshold tuned per step (rest/sleep displacement per step, probe length vs travel per
     step, "settled" checks) behaves differently at 24 ms frames than at 33 ms.
   - Fits "never at 33-36 fps, always at 40-42".
   - Predicts: disappears with `--mcla_raw_timer=false` at the same 40 fps.
2. *Ambient thread race with the main update* (low-medium):
   - The thread reads the global timer and world state while the main thread updates layers and
     racers (which re-split the timer) and the physics world.
   - GPU-bound pacing changes how long the main thread's post-kick work takes relative to the thread.
   - Predicts: occurs regardless of the timer mode, correlates with GPU-bound frames, not with
     fps as such.
3. *Collision (phBound) streaming under the car lagging* (low): a probe miss leaves the car at path
   height ("well above the overpass deck, nothing under it"). Speed per real second is the same at
   any fps with the raw timer, so this would need a frame-counted stream step. None was found in the
   sector machine.

**Texture flicker on buildings, billboards and signs**

1. *Same family as KI-038* (medium):
   - transient sign placeholder seen at 37-40 fps GPU-bound 1440p on l49, pre-l55, i.e. before any
     read-back wait existed);
   - The common factor of KI-038 and KI-074 is GPU-bound Unlocked pacing above ~36 fps, not the
     exemption.
   - Host-side candidate: texture-cache or upload reuse keyed on GPU completion, which a saturated
     GPU (93-95 % busy, never idle) stretches.
   - Predicts: present when GPU-bound at <36 fps (higher resolution), absent at 40+ fps CP-bound
     (720p).
2. *Render-side frame-counted logic* (`frame % 3`, `frame & 3`, visibility continuity) (low-medium):
   runs per frame, not per vblank, so it is not a vblank-step artefact. A frame-counted fade
   finishing sooner is possible but would look like blinking lights (KI-041), not texture flicker.
3. *Streaming lookahead tied to dt* (`sub_822D5A30` smoothing uses `timer+8`) (low).

## 5. Discriminating experiments

All except E4 are flags only, need no build, and use the same route and save as l76.

- **E1, timer.** Steady arm with `--mcla_raw_timer=false` (stock fixed 1/30 s per frame) at 40-42 fps.
  - The game runs about 1.35x fast; diagnostic only.
  - Every game-side dt is then exactly the stock value; frames per second and GPU-bound pacing are
    unchanged.
  - KI-074 gone -> dt-magnitude dependence. KI-074 present -> frame-count or pacing (go to E3).
  - Confound to note: the faster game moves the camera 1.35x faster through streaming.
- **E2, threshold and cap.** Steady arm at `--mcla_guest_vblank_hz=72 / 80 / 88` (caps 36 / 40 / 44).
  - It locates the onset and checks the 36 cap directly.
  - The cvar is hot-reloadable, but use one value per session for clean A/B.
- **E3, fps vs GPU-bound.**
  - (a) Base at 1440p/2160p, GPU-bound at about 25-30 fps.
  - (b) Base or steady at 720p, CP-bound at 45-60+ fps.
  - Flicker only in (a) -> GPU saturation / host-side; only in (b) -> frame rate; in both -> either.
  - Also check old reports: l41-l54 ran 37-40 fps GPU-bound at 1080p/1440p and logged only the
    KI-038 sign flicker, no floating cars.
- **E4, instrumented, small host build.** Extend the existing stripped-printf hook
  (`src/mcla_ki026_probe.cpp:251`, `sub_82130000`) and add a per-frame ring:
  - formats 0x820C26D8 (ambient could not spawn on terrain), 0x820C21A0 (AddAmbientToLayout failed),
    0x8203CD0C / 0x8203CCE0 / 0x8203CCA8 / 0x8203CCCC (sector paging; set or force the gate byte
    0x8287C7DB only after checking its other readers `sub_822D78D8`, `sub_822D8BE8`, `sub_822D9950`),
    0x82037770 / 0x82049994-0x820499E0 (car evictions), 0x8204981C (PlaceChunk);
  - per frame: timer `+92/+88/+8`, N, ambient kick->join time;
  - dump the last ~3 s into the F10 ring;
  - an F10 at a flicker or floating car then shows whether streaming/eviction or spawn failures
    coincide.

## 6. Candidate mitigations (ranked)

1. **Launcher frame cap for Unlocked at 36 fps** (`--mcla_guest_vblank_hz=72`).
   - No SDK or game change; one timing code plus the allowlist gate (M9-003 pattern).
   - Protects every machine that would exceed ~36 fps; on the owner PC the steady exemption then
     buys a steady 36 instead of 33-36.
   - Validate with E2 first: if the onset is at 38-40, use 76-80 Hz.
2. **Game-side fix behind a hook, once E1/E4 name the subsystem:**
   - if dt: e.g. clamp the step only for the ambient update or the physics substep (feed
     `sub_826D75D8` / the collider an accumulated step of at least 1/30 s, or raise `physicsupdates`
     handling to keep the substep near 1/60 s);
   - if frame-count: scale the specific counters.
   - Higher cost; needs the E1 result.
3. **Fixed 1/30 s timer above 36 fps** (stock timer): not a mitigation. The game runs fast.
4. **Vblank step normalisation:** not applicable. The game reads no vblank counter for timing
   (section 3).

## 7. Limits

- Static reading only. Virtual calls are not resolved in the reachability checks.
- The floating-car code path, the texture-flicker path and the purpose of the render-target round
  robin are not identified.
- The `sub_826D75D8` spawn internals were read only at the dt level.
- No Spark job (this subagent has no Spark access).
- Temp workspace (expanded image, listing, scripts) deleted after writing this note.
