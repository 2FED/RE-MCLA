# M9-007 menu state from the game: static analysis

Time-boxed read-only reverse-engineering by an Opus subagent on 2026-10-29,
reviewed and accepted as evidence by the primary agent. Nothing is verified
at runtime; the hook in
[M9-007-menu-state-proposal-20261029.diff](M9-007-menu-state-proposal-20261029.diff)
is not compiled or applied yet. Game strings were read in memory only and
no derived string lists were kept; this document cites guest addresses and
minimal identifiers as the source-data policy allows.

2026-10-29. Read-only static analysis of the recompiled guest code
(`generated/default`, SDK a239b83 pin) and of strings in the decrypted XEX
image (decrypted in memory only; nothing exported). Nothing was built or
launched. All addresses are guest addresses.

## Result

The game has an explicit simulation-pause state, and the UI screens that the
owner reported as too fast set it through `UILogic_PauseGame`.

| Item | Address / function | Evidence |
| --- | --- | --- |
| Game-state object pointer | `*(be32*)0x828CCEA0` (copy at `0x828CCEA4`) | allocated (108 bytes) and stored by `sub_822D29A0` (mcla_recomp.14.cpp:31921) |
| Constructor | `sub_822D2818` (14.cpp:31678), base `sub_8238A988` (21.cpp:45883) | base clears bytes +4..+13; vtable `0x8203BE80`, followed by the string `Requesting State: %s` |
| Simulation paused | byte `G+4` | set by request 1 (`PauseSimulation`) and 7 (`PauseOnly`), cleared by 2 (`ResumeSimulation`) and 8 (`ResumeOnly`) |
| Paused locally (online) | byte `G+6` | set by request 3 (`PauseLocally`), cleared by 4 (`ResumeLocally`) |
| One-frame step | byte `G+10` | request 6 (`FrameAdvance`, only while paused); cleared at the start of every update |
| Fast forward | byte `G+8` | request 5 (`FastForward`) |
| Request queue | vtable slot 2 `sub_822D26D0` (14.cpp:31487) | 10-entry ring at G+20, index +64, count +68; name table `0x827E91A8` = {?, PauseSimulation, ResumeSimulation, PauseLocally, ResumeLocally, FastForward, FrameAdvance, PauseOnly, ResumeOnly} |
| Request applied | vtable slot 3 `sub_822D29E8` (14.cpp:31970) | jump table `0x822D2A9C`, cases write the bytes above |
| Request pending query | vtable slot 8 `sub_822D2768` | |
| Per-frame caller | `sub_822C1FA8` (13.cpp:59203, game frame update) | first action is `G->vtable[3]()`, then reads G+4/G+10 |
| Script binding `Game_IsPaused` | name `0x8203AB4C`, native `sub_822C8A20` -> `sub_822BF228` (13.cpp:52117) | returns `G+4 && !G+10` |
| `UILogic_PauseGame` | name `0x8201E044`, native `sub_82204620` -> `sub_82215A60` (7.cpp:68594) | offline: request 1; when `sub_82387A18()` is true (online): request 3; sets byte `0x8286E16B` = 1 |
| Unpause path | `sub_82204628` -> `sub_822150D8` (7.cpp:67078) | request 8 (or 4 when locally paused); clears `0x8286E16B` |

Readers of `G+4 && !G+10` (the "is paused" idiom) are spread through
gameplay: car and physics updates (`sub_822A2988`, `sub_822C1FA8`), camera,
audio and effects (`sub_822E8448`, `sub_82793360`, about 70 sites in all). That
fits a global simulation pause.

### Which screens pause (direct C++ callers of `sub_82215A60`)

Classified by the strings each caller references:

- `sub_82634140`: `PAUSEMOVIE` - the pause menu.
- `sub_82650038`: `CareerNavSystemScene`, `CareerRaceNavScene`,
  `OLNavSystemScene`, `TestDriveNavSystemScene`, `ArcadeMenuDefault` - the
  map/navigation scenes.
- `sub_8265CA00`: `ARCADE_MENU_TITLE`, `ArcadeMenuChange` - the Arcade menu.
  Its sibling `sub_8265CC90` (`Game/Race/RaceWrapper`, `nNumLaps`, `LapMax`)
  calls the unpause path when a race is launched.
- `sub_82639B68`: prompt/popup movies (`PROMPTMOVIE`, `POPUPMOVIE`); pauses
  only when the prompt's own flag +125 is set, remembering the earlier state.
- `sub_82641E80`: `ReplayPause`; `sub_82675910`: cop fine/ticket screen;
  `sub_82646618`: repair/purchase (`full_fix`, `G_MoneySpent`).
- Tutorial popups (`UI_MENU_TUTORIAL_OPEN_MASTER`) request 1 directly
  (`sub_8267C5B0`, `sub_8267CA40`, `sub_8267DCA0`); close requests 2.
- Five more callers (`sub_82631F00`, `sub_82665BB8`, `sub_82674120`,
  `sub_82674540`, `sub_8267B328`) reference no strings and are unclassified.
- The script binding lets UI scripts (game data, not the executable) pause
  too; those call sites cannot be seen statically.

## Owner cases

| Case | Expected state | Confidence |
| --- | --- | --- |
| Pause menu over the city | G+4 = 1 (offline), G+6 = 1 (online) | High: the PAUSEMOVIE screen calls `UILogic_PauseGame` |
| Arcade menu over the map | G+4 = 1 | High for the map and Arcade screens (both call it); medium that the owner's exact path goes through them |
| Wager / race-start screen at a meet spot | probably G+4 = 1 | Medium-low: the race-launch path unpauses (`sub_8265CC90`), which implies a paused setup screen, and prompts can pause; not tied to the meet-spot screen by name |
| Photo mode | unknown; earlier evidence calls it "paused" | If it sets G+4, it is capped at 30 like stock. That is acceptable but needs an owner check of free-camera feel |
| Race-start cinematics, cutscenes | expected G+4 = 0 (they need the simulation) | Medium; would stay uncapped as today |
| Driving at traffic lights | G+4 = 0 | High; no static-scene logic involved |
| Loading/warper | warper requests resume | Loading screens stay on the draw-count cap |

## Existing cap and where the flag plugs in

- Cap state machine: `MclaReportGuestFrameDraws` in the SDK
  `src/graphics/graphics_system.cpp` (lines 390-420), called once per closing
  frame from the D3D12 command processor (`command_processor.cpp:3829`). The
  vsync worker (lines 196-235) forces the stock vblank rate while
  `g_mcla_ui_capped` is true. The cvars (`mcla_ui_cap_draws_low` 250,
  `_high` 600, `_unlock_frames` 8) are hot-reload.
- The Vulkan command processor does not call `MclaReportGuestFrameDraws` at
  this pin. On Vulkan, `g_mcla_ui_capped` therefore stays at its initial
  `true`, so the unlocked mode never unlocks there. This is a side observation;
  check whether it is intended.
- The host already overrides guest functions with strong `REX_EXTERN`
  symbols that call `__imp__sub_...` (`src/mcla_raw_timer.cpp`:
  `sub_821BDA90`, `sub_822D2478`). `sub_822D29E8` is not overridden anywhere.

## Proposed hook (`m9-007-proposal.diff`, not applied)

This is host-only; no SDK change is needed.

1. New `src/mcla_menu_state.cpp`, added to `MCLA_SOURCES`.
2. A strong override of `sub_822D29E8` runs the original, then reads G+4 and
   G+6, but only when `ctx.r3 == *(be32*)0x828CCEA0`.
3. When the game pauses in unlocked mode (`mcla_guest_vblank_hz > 0` and the
   UI cap enabled), it raises `mcla_ui_cap_draws_low` to 100000 through
   `rex::cvar::SetFlagByName`. The next presented frame then turns the
   existing cap on. The GPU cvars are read through `rex::cvar::Query` only on
   transitions, because they live in the GPU plugin.
4. On resume it restores the saved threshold. The unchanged 8-heavy-frame
   hysteresis then releases the cap, so the draw-count heuristic remains the
   fallback for everything else.
5. The init-only cvar `mcla_ui_cap_guest_pause` (default on) is the kill
   switch. Up to 200 `MCLA menu state:` log lines record each transition.

Cleaner alternative, which needs a new SDK pin: add an exported
`MclaReportGuestPaused(bool)` that sets an atomic, and OR it into the vsync
worker's cap test. This avoids mutating a user-visible cvar.

## Unverified and risks

- No runtime confirmation yet. The three owner screens, photo mode and
  cutscenes need an owner run.
- The wager/meet-spot screen is not proven to pause the simulation. If it
  does not, it stays at about 80 fps; the hook does no harm, and that case
  remains a documented limit.
- The cvar-toggle variant writes a user-visible cvar. A settings save while
  paused (the SDK settings overlay; disabled in managed builds) could persist
  100000. The init-only kill switch and the SDK-atomic variant avoid this.
- Online sessions: G+6 caps the local pause menu while the world keeps
  running behind it, which is the same as stock 30.
- The hook works only on D3D12 (see the Vulkan observation above).
- The five unclassified callers and script-driven pauses can also cap. All of
  them are pause-type screens by construction.
- The proposal has not been compiled.

## Verification experiment (one owner run)

Build the proposal (or a log-only variant with the `SetFlagByName` calls
removed) as a sunc-style candidate. The owner then plays this route:

1. drive (10 s);
2. pause menu (10 s);
3. map, then the Arcade menu (10 s);
4. resume, then drive to a meet spot and open the wager/race-start screen
   (10 s);
5. start the race and watch the intro cinematic;
6. photo mode (10 s);
7. drive past a traffic light.

Accept if:

- `MCLA menu state: game paused` and `MCLA UI cap: on` appear within one
  frame of steps 2-4 (and of 6, if photo mode pauses);
- `running` and `UI cap: off` appear after resume;
- F10 or per-minute statistics show about 30 fps in those menus and 55-74
  fps in driving;
- the traffic-light segment stays uncapped.

## Recommendation

Proceed with the hook as a time-boxed candidate. The state is unambiguous in
code: one global object, an explicit request queue with named states, and a
script binding called `Game_IsPaused`. The pause menu and map/Arcade screens
call the pause path directly. Keep the draw-count heuristic as the fallback.
If the owner run shows that the wager screen does not set G+4, document that
single screen as the remaining limit instead of adding heuristics.

No Spark job was used for this investigation. The work was serial static
disassembly reading that needed deterministic cross-checks, so it was not a
bounded extraction or triage task.
