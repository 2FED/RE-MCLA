# M9-007 hangout gap and KI-047 police livery (read-only analysis, 2026-10-29)

Read-only analysis by an Opus subagent on 2026-10-29 from the owner's l41 F10 packages and runtime log, reviewed and accepted as evidence by the primary agent. The hook change in [M9-007-hangout-proposal-20261029.diff](M9-007-hangout-proposal-20261029.diff) is not applied or compiled yet.

Read-only analysis of the owner's l41 diagnostics (host from merge `dd7cca7`,
build hash `ABD97918...`) and of the recompiled guest code in `generated/default`.
Nothing was built or launched; no save was opened; no game asset was copied.
Frame images were viewed as temporary downscaled PNG crops, deleted afterwards.
Spark: not used (image inspection plus short source reading; validation would
cost more than doing it directly).

## Item 1: M9-007 cap missing at the hangout / meet spot

### Packages

Session `42656` (unlocked mode, render target 1280x720, window 3840x2160):

| Package | Source | Owner text |
| --- | --- | --- |
| `live-20261029T151819Z-42656-1` | pad | at the Mack's Burger hangout parking lot (Wager / Pinkslip); FPS not locked to 30, UI sped up; expected the same lock as pause/Arcade |
| `live-20261029T151830Z-42656-2` | pad | (no text; 11 s later, same screen) |
| `live-20261029T151906Z-42656-3.partial` | unexpected-guest-exit | session end (paused, after leaving the hangout) |

Frame of package 1: the hangout race-type menu (WAGER RACE / PINKSLIP RACE,
EXIT HANGOUT / ACCEPT) is a small panel in the top-left corner, drawn over a
live 3D scene (player avatar, animated crowd, parked cars). It is a UI panel
over a running world, not a paused one.

### Guest state at the hangout (`state.json` `guest_ki026`)

| Field | Pkg 1 | Pkg 2 | Driving (`43544-2`) | Pause menu / map / photo (7 packages) |
| --- | --- | --- | --- | --- |
| pause_manager.paused (G+4) | **0** | **0** | 0 | 1 |
| pause_manager.paused_locally (G+6) | 0 | 0 | 0 | 0 |
| warper.state | 29 (idle) | 29 | 29 | 29 |
| camera_paused / gps_jumping / pause_override | 0 | 0 | 0 | 0 |
| cutscene.state12 | 0 | 0 | 0 | 0 |
| racer.state656 | 3 | 3 | 3 | 3 |
| **racer.camera_input_off908** | **1** | **1** | **0** | 0 (1 only in `20800-2`, the photo snap, which is paused anyway) |
| timer_scale / game_time_scale | 1 / 1 | 1 / 1 | | |

The hangout does **not** pause the simulation (G+4 = G+6 = 0), so the M9-007
hook correctly saw "running". The earlier medium-low prediction (G+4 = 1) is
refuted.

### Rate and draw count at the hangout (runtime log)

- Menu-state transitions: paused 17:16:37.9, running 17:17:30.9 (resume),
  then none until 17:18:46.7 (pause menu before quitting). Both hangout
  packages (17:18:19, 17:18:30) fall in the "running" interval.
- `MCLA UI cap: off (6034 draws in frame)` at 17:17:31; no cap-on until 17:18:46.
- Between packages 1 and 2: 796 guest swaps and 3.92 M draw packets in 11.2 s,
  i.e. about **71 fps** and **about 4,900 draws per frame**.
- Minute ending 17:18:27: `frame pacing/min 3673 frames (61.2 fps)`,
  `draws per frame/min p10 4786 p50 5098 p90 7574`. Far above the draw-count
  threshold (250), so the heuristic cannot catch it either.
- A game time-scale ramp 1.0 -> 0.1 -> 1.0 at 17:18:00-17:18:01 (raw-timer
  scale log) lies inside the hangout interval; plausibly the hangout
  slow-motion. Not used by the proposal.

### Reliable guest state: local racer deactivated (racer+908)

- `racer = *(be32*)(*(be32*)0x82874374 + 8)` (local racer; same path as the
  KI-026 probe).
- Script native **`Racer_Deactivate`** = `sub_822AD200` (13.cpp:7314, name from
  the script binding table) -> `sub_822AB258` (13.cpp:2362): turns the racer's
  input object off (`sub_8232C788(..., 1)`) and stores byte **`racer+908 = 1`**.
  Hangout logic is script (game data; the binding table also has
  `CineScript_*HangoutAnimPack` natives), which explains why no executable
  hangout screen calls `UILogic_PauseGame`.
- **`Racer_Activate`** = `sub_822AD1F0` -> `sub_822AB1D8`, which clears +908
  (store at 13.cpp:2332). The racer reset/teleport functions (12.cpp:64493,
  65728, 65882) also clear it.
- Other C++ setters: `BeginWarp` `sub_8221A440` (8.cpp:6776, offline only),
  `sub_822B7720` (13.cpp:32974), two network-player handlers `sub_82293218` /
  `sub_822932C0` (12.cpp:17721/17819). All are control-less states.
- Consumer: per-frame camera input `sub_822B35E8` is called from `sub_822A56C8`
  only when `racer+908 == 0`. With +908 = 1 the player has neither car nor
  camera control, so it cannot be set during normal driving.

Confidence: **medium-high** that `racer+908 != 0 && racer+656 == 3` marks the
hangout/meet-spot menu (2/2 hangout packages = 1 with G+4 = 0; driving = 0;
mechanism matches). Not proven: which script deactivates at the hangout, and
every other script use of `Racer_Deactivate`.

### Proposed hook change (`m9-007-hangout-proposal.diff`, not applied)

`git apply --check` passes against the worktree; not compiled.

1. New init-only cvar `mcla_ui_cap_racer_inactive` (default true), a separate
   kill switch.
2. In the `sub_822D29E8` override, after reading G+4/G+6, read the local
   racer (`*(0x82874374)+8`), require non-null pointers and `+656 == 3`, read
   byte `+908`.
3. `paused = sim || local || racer_off`. The `Controller` and its tests are
   unchanged (it only sees a bool), so the same edge logic, forced
   `mcla_ui_cap_draws_low = 100000`, restore on release, shutdown restore and
   8-heavy-frame hysteresis apply.
4. The log line gains `racer-off {}` so the owner run shows which signal fired.

### Risks

- Scripts may deactivate the racer in other control-less states (race intro,
  arrest/tow, showroom transitions, warps). Capping them is stock 30 fps
  behaviour, but it changes the earlier "race-start cinematics stay uncapped".
- If some state leaves +908 set while the player can drive (not expected: the
  camera input is gated on it), driving would be capped. The kill switch and
  the `racer-off` log field make that observable and reversible.
- Guest reads are direct (unchecked) on the guest update thread, like the
  game's own code; null pointers are guarded, a stale non-null player-manager
  pointer during teardown is a residual risk.
- Online: BeginWarp skips the deactivation when `sub_82387A18()`; online
  hangouts untested.
- Photo mode unchanged (pauses via G+4); free-camera feel at 30 still needs the
  owner's check.
- D3D12 only (Vulkan does not report draws), as before.

Verification: owner run, route drive -> hangout menu (10 s) -> exit hangout
and drive -> accept a wager race (intro) -> drive. Accept when `racer-off 1` /
`UI cap: on` appear at the hangout menu, `running` / `UI cap: off` after exit
or race start, per-minute pacing about 30 fps at the hangout and 55-74 while
driving.

## Item 2: KI-047 police livery at 2160p

- The only 2160p session of the day is `20800` (render target 3840x2160 =
  draw scale 3). Its "map" package is `live-20261029T152507Z-20800-1` (pad
  trigger; owner text: 15-20 fps): the frame is the full-screen GPS MAP and
  **contains no car**, so it cannot show the livery. Map icons render correctly.
- Nearest evidence: `live-20261029T152633Z-20800-3` (photo mode; owner: some
  cars fine, some very bad LODs, maybe VRAM). Two distant cars across the
  junction are blocky low-LOD meshes with a flat white body, black square
  wheels and a red glow at the roof; one is plausibly a police car whose
  black/white livery is reduced to plain white. Low confidence that it is the
  reported car.
- **Not** the KI-038/032 red/yellow/blue band pattern: no bands; the defect is
  a uniformly flat white, low-detail body.
- Same session log: from 17:23:34, eight `video memory ... near budget` lines,
  VRAM up to 7090 MB against a 6676 MB budget (8 GB laptop GPU), texture cache
  hard limit shrinking 5595 -> 3565 MB. No other session of the day (720p,
  1440p) logged budget pressure. One "texture fetch constant has invalid type"
  warning at 17:23:26 (also seen at l7).
- Fit with the scale-3 hypothesis: **partial**. Scale 3 is the common factor,
  but the likely mechanism is texture-cache eviction under the DXGI budget (the
  budget cache was added 2026-10-25, after the l7 sign evidence), not stale
  scaled-resolve content. Bad LODs were also reported at 1440p (`43544-1`, no
  budget pressure), so part of the LOD complaint may be the game's own distance
  LOD magnified by photo zoom.
- The same VRAM pressure is a candidate contributor to KI-046 (2160p minutes at
  22.6 / 15.5 / 18.9 / 16.3 fps after it began).

Next step: an F10 with a police car on screen at 2160p (and the same spot at
1440p), and a comparison with an l31/l33 2160p log for `near budget` lines.
