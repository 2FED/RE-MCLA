# M11 KI-082 traffic tremble: suspension research (peers + static RE), 2026-11-18

Read-only research by an Opus subagent while the owner was playing: no builds, tests, launches or repo
edits; targeted greps of `generated/default/` and public upstream sources only. Spark not used (external
web reading plus a few hundred lines of disassembly; a source packet would have cost more than reading).

KI-082 (docs/known-issues.md): nearby traffic cars tremble while they move, are still when they stand;
mostly vertical (owner); persists at an even 40 fps cap (l109 `pc/cap40`) and unlocked 42-55 fps; Stock 30
check pending. Clip analysis: cars sit on the world within 0.2-0.5 px, except a vertical term (0.69 px RMS
at 4K on one car). This note builds on `M11-ki054-traffic-jitter-design-20261110.md` (LARecomp timing
digest) and `M11-ki082-traffic-probe-analysis-20261117.md`; it does not repeat them.

Licence rule applied: LARecomp (no licence) and holdmysocks / RenanQueiroz (GPL-3.0): ideas, measurements,
descriptions and guest addresses only; no code copied, quotes at most a line.

## Part A: peers

| # | source (URL, commit, licence) | symptom | their cause | their fix |
|---|---|---|---|---|
| A1 | github.com/mzzvxm/LARecomp commit `1258f5d87` (2026-08-20), "Ported my recomp stuff"; no licence | "high-framerate suspension vibration, ground clipping, and chassis jitter" (player car) | chassis ground-depth filter in `sub_82563298` steps a per-frame constant: at `0x82563704` the game takes `round(1/dt)` from timer+12, uses 0.05 (`0x82003A90`) at >= 60 fps, else 0.10 (`0x82004288`), then state(+20) = (1-a) state + a target | replace a with 1 - 0.90^(30 dt) (continuous time) |
| A2 | LARecomp `5c51735dd` (2026-10-14) "continuous vehicle physics filters and suspension pushback damping"; no licence | "limit-cycle hopping" of the player chassis under high G | in `sub_82563298` the ground pushback is a PD solver whose velocity (x - x_prev)/dt is clamped to +-2.5 before `bl sub_825A3D90` (`0x8256384C`); the returned acceleration (`0x82563850`) can spike for one frame | widen the velocity clamp to +-12, clamp the acceleration; plus continuous-time versions of the car gyro filters `0x8236895C` (0.80/0.40), `0x823689F0` (0.30/0.15) and tyre slip `0x82369E40` (0.30/0.15) |
| A3 | LARecomp `0cc78fb4d` (2026-10-14); no licence | "suspension over-stiffening or hopping" above 60 fps | the pushback in `sub_82563298` is a per-update impulse, so its total per second grows with the update rate | scale it by min(1, 60 dt) (only above 60 fps) |
| A4 | LARecomp `documentation/MCLA_WORKPLAN.md` at HEAD `86d138f25` (2026-11-18); no licence | - | "Traffic and NPC smoothing sweep" is still listed as open: vehicles other than the player car "have their own per-frame interpolation constants that have not been swept" | none yet (the same doc claims traffic behaves the same at 30-144 fps, without a traffic measurement) |
| A5 | LARecomp TECHNICAL_NOTES s.1 (same HEAD) | traffic cars "bounce and flip" | dt pinned at the 0.1 s ceiling during a frame-rate collapse | not a high-fps effect; n/a to KI-082 |
| A6 | LARecomp `7651cc5b7` (2026-08-14) | chase cam snaps at 60 | per-frame camera lerp | continuous-time decay; the message notes that feeding the 60-fps decision from another clock "broke traffic timing" (no detail) |
| A7 | github.com/holdmysocks/mcla-recomp PR #18 (merged `41217a96`); GPL-3.0 | "moving things lurch" at a 60 cap | frames 20.8/16.7/20.9/16.6 ms apart while the world moved 5.6/32.4/5.6/32.4 ms (step measured before the limiter wait) | measure the step after the wait (pace at swap); our probe already showed no such zig-zag |
| A8 | github.com/RenanQueiroz/mcla-recomp issues/PRs (search: traffic, suspension, bounce, jitter, shaking); GPL-3.0 | nothing on traffic or suspension (only stutter / audio / fps threads) | - | - |
| A9 | github.com/xenia-canary/game-patches `545407F8 - Midnight Club Los Angeles*.patch.toml` (repo commit `12c7e7e0`; no licence file) and the RDR `5454082B` patches | 60 fps patch notes describe only game speed and 30-fps drops with vsync; no physics side effects listed | - | - |

Not found: any PCSX2/RPCS3 MC3/MCLA-PS3 60 fps note on physics, any RAGE modding write-up on ambient-vehicle
suspension rate (searches stopped by the GitHub search quota; general web search not attempted further).

Peer summary: every fix in the three repos targets the **player** chassis (`sub_82563298`) and camera; the
recurring pattern is a per-update constant (filter coefficient or impulse) that makes the chassis stiffer /
hop at a higher update rate. **No peer has examined ambient traffic suspension**; LARecomp lists it as open.

## Part B: our code (static, `generated/default/`)

### B1. Ambient wheel/suspension path (instruction evidence)

- `sub_826FA0F8` (TU55:19816, mcAmbientCollider vfunc +56): builds one probe per wheel (`sub_826E5540`,
  count in r18), world test `sub_827974F0`; discards hits with |normal.y| < f30 (float at `0x820185A4`,
  `lfs f30,-31324(r8)` with r8 = 0x82020000; 0.7071 per the KI-074 RE). dt for each wheel: `lfs f1,8(r19)` at `0x826FA300`
  (timer+8), or f1 = 0.0 when the pause byte (r20) is set. Per wheel it calls `sub_826FBA60` with
  r3 = collider+676+24*i (per-wheel state), r5 = r17 = `sub_826E94E8(car)`.
- `sub_826E94E8` (TU54:44513): returns whether the car's phInst is in the physics level (level index
  halfword at +8 != 0xFFFF and the level slot state). So the same wheel code runs for cars that are **not**
  physically active; r5 only gates whether forces are applied. (Static reading; this conflicts with the
  l107 `calls=0`, which is still unexplained: see the l107 analysis s.1.)
- `sub_826FBA60` (TU55:23768) is a **ray-cast spring-damper**:
  - f30 = dt; returns at once if dt == 0.0 (`0x82000ED4`).
  - compression x = dot(probe geometry) stored at wheel+8 (`stfs f0,8(r31)` at TU55:24104).
  - velocity v = (x - x_prev) * (1.0 / dt) (`0x82001D14` = 1.0, `fdivs f29,f28,f30`, TU55:24055,24109);
    v is forced to 0 when x_prev == 0.0 exactly (first contact).
  - force = k x + c v with k = tune+244, c = tune+248 (tune = car+448), TU55:24161; scaled by a contact
    dot product, plus a weight term from `sub_826E9658` (mass x two constants) added to the y component.
  - when active (r21/r5 != 0) applied through phInst vfuncs +128 and +132 (force and moment; guess from the
    operands), i.e. a force integrated by the body with its own dt: correctly dt-scaled.
  - **bump stop**: if x > tune+216 (max travel), the excess (x - max) times a contact vector is passed to
    phInst vfunc +108 with r4 = `0x828608D0` (TU55:24285-24327). This is an **unscaled per-update push**
    (no dt factor in the operands); whether +108 is an impulse or a position correction is a guess.
  - wheel+12 = |v_contact| x dt (wheel travel this step), clamped to +-tune+228 (TU55:24352-24411), flag
    wheel+20 = 1 when clamped; then a friction section uses (wheel+12 - prev) * (1/dt).
- After the wheel loop `sub_8259E920` copies the instance matrix into collider+32/+96 (previous-pose
  bookkeeping, likely motion-blur / last pose); no filtering.

### B2. Which dt

- The ambient step `sub_826D75D8` (TU54:3): f30 = timer+8 (`0x826D796C`), f31 = timer+88 (unscaled) or
  f30; network branch min(timer+92, 0.05). Per-object update loop over the list at mgr+4: vfunc +20 with
  f1 = f30 (TU54:959-981); mgr+16704 vfunc +12 with f30; `sub_826D5B30` with f31; dt stored at mgr+8488.
- No clamp of dt to 1/30 and no fixed-substep accumulator in the ambient step or in the wheel code;
  no 0.0333 / 30.0 / 60.0 constant in `sub_826FA0F8` / `sub_826FBA60`. The only fps branch found in this
  area is the **player** chassis one (`sub_82563298`, 60-fps threshold, A1). The suspension is therefore
  integrated with the frame step (16-26 ms in our build) and was tuned at 33.3 ms.

### B3. Answers to the three questions (static only)

1. Spring-damper with frame dt: **yes** (B1). The spring and damper forces are dt-correct, and a smaller dt
   is normally *more* stable for them, so a classic Euler instability does not explain a tremble at
   40-55 fps. Two terms are not rate-neutral: (a) the damper uses a backward difference of a **per-frame
   ground probe**, so probe noise (triangle edges, the 0.5 probe segment, normal changes on road seams)
   becomes a damper kick c dx per update, i.e. more kicks per second at a higher rate; (b) the **bump-stop
   push** via vfunc +108 is per update and not dt-scaled, the same class of defect LARecomp fixed for the
   player (A3). Both act only while the ground under the wheels changes, i.e. only on moving cars.
2. A procedural speed-proportional bob / random shake: **not found** in the parts read (no RNG call, no
   sin/cos, no time accumulator in `sub_826FA0F8`/`sub_826FBA60`). Not excluded elsewhere (render-side body
   animation of ambient cars was not read).
3. Stationary cars: with x constant, v = 0 and the bump stop is inactive, so the wheel code is quiet but
   still runs; nothing in it skips stationary cars. "Still when standing" fits this model.

Caveat for "the player car does not tremble": the player chassis uses a different path (`sub_82563298`)
and the chase camera follows the chassis, which masks a vertical micro-bounce of the player's own car.

## Part C: proposal

Leading mechanism (about 40 %, unproven): a per-update, not dt-scaled term in the ambient wheel contact
(`sub_826FBA60`: the bump-stop push via phInst vfunc +108, and/or damper kicks from the per-frame probe
difference) that at 40-55 updates per second gives traffic a small vertical limit cycle on uneven road
mesh; the same class as LARecomp's player pushback "hopping" (A3), which no peer has fixed for traffic.
Alternatives: retail behaviour also present at 30 fps (about 25 %; the Stock 30 check decides); a render or
pose effect specific to ambient cars (about 20 %); presentation timing (about 15 %, weakened by cap40).

Cheapest A/Bs:
1. Already in progress: Stock 30 against Unlocked on the same street. Gone at Stock 30 -> rate-dependent.
2. l109 `fps_cap_30` key in Unlocked mode (same raw-timer path, dt = 33 ms): separates "update rate" from
   "Stock-mode differences". If cap30 is calm and cap40 trembles, the mechanism is per-update.
3. Reference: the same street at 30 fps in Xenia or console footage, to tell retail behaviour apart.

Next build (default off, read-only probe): extend the existing direct-call hook on `sub_826FBA60` (proven
live in l83, lr `0x826FA378`) with per-wheel logging for the 4 nearest cars: wheel+8 (x), v (f13 after
`0x826FBC9C`), the bump-stop branch taken (x > tune+216) and its magnitude, r5 (active flag), dt; plus the
car's matrix y. A vertical tremble that correlates with bump-stop hits or with |dv| spikes confirms the
mechanism; zero bump-stop hits and smooth x point elsewhere (render/pose).

Fix shapes (after the probe): (i) scale the bump-stop excess by min(1, 30 dt) before vfunc +108
(cheapest, mirrors A3); (ii) low-pass the damper velocity with a continuous-time coefficient
1 - (1 - k30)^(30 dt); (iii) run the ambient wheel update at a fixed 1/30 s substep for ambient cars only
(largest change; the forces feed a body integrated elsewhere, so substepping only this function is not
physically consistent). Risks: (i)/(ii) change traffic handling after contacts (crash behaviour, settling
on spawn), so A/B on collisions too; any change to the active-car path also affects police and traffic hit
by the player.

## Sources and identities

- LARecomp: https://github.com/mzzvxm/LARecomp, HEAD `86d138f25b93f0a77607f5c0cf1c40c3cd895087`, commits
  `1258f5d87`, `5c51735dd`, `0cc78fb4d`, `7651cc5b7`; no licence (ideas/addresses only).
- holdmysocks: https://github.com/holdmysocks/mcla-recomp PR #18 (merge `41217a965b84`), GPL-3.0.
- RenanQueiroz: https://github.com/RenanQueiroz/mcla-recomp issues/PRs, GPL-3.0 (nothing relevant).
- Xenia Canary game patches: https://github.com/xenia-canary/game-patches commit `12c7e7e05b9c`, no licence
  file (patch notes cited only).
- Ours: `generated/default/mcla_recomp.55.cpp` (`sub_826FA0F8` :19816, `sub_826FBA60` :23768),
  `mcla_recomp.54.cpp` (`sub_826D75D8` :3, `sub_826E94E8` :44513, `sub_826E9658` :44727),
  `mcla_recomp.39.cpp` (`sub_82563298` :15326, fps branch near :15899), `mcla_recomp.41.cpp`
  (`sub_8259E920` :53618); `src/mcla_ambient_dt_probe.cpp` (hook of `sub_826FBA60`),
  `src/mcla_traffic_motion.cpp`. Constants: `0x82000ED4` = 0.0, `0x82001D14` = 1.0, `0x82004288` = 0.1
  (from `docs/evidence/M8-timer-operand-replay-20261015.md`); `0x82003A90` = 0.05 per LARecomp (not
  verified here).
