# Alpha compatibility matrix

This document separates the historical M6 reference-host matrix from the later
M7 representative Windows set. Neither proves every Windows system, controller,
wheel, race or campaign path. [README-AI](../README-AI) owns milestone status;
final M7 closure and push are not declared by this compatibility summary.

## Current M7 Windows evidence

The current owner branch is private p1 on the RTX 5060 laptop: Direct3D 12,
stock timing, 2x target, VSync and fullscreen. Accepted hybrid artifact prefixes
are host `82A69B5E`, runtime `542DD347`, launcher `67B8F527`, GPU `E2EA29B7`
and WGC helper `6E766EDF`; [full identities and Windows limits](evidence/M7-016-windows-acceptance-20261010.md)
remain authoritative. Earlier routes retain their own October4/5/DCDE cohorts.

| Area | Accepted representative M7 scope | Limits |
|---|---|---|
| Events | Delivery, Ian Red Light, first Time Trial, Kenneth Hahn Series, Tournament/repaired tutorial, Pinkslip win/reload, Wager loss, Freeway, race-to-start, Goal Attack and Arcade Landmark. [Event report](evidence/M7-003-event-acceptance-20261010.md). | Outcomes differ by row; no inferred wins, rewards, per-event reloads or full campaign. Payback, unconfirmed SC event access and Unordered are deferred without PASS. |
| Profile and saves | Isolated Clean creation/restart; visible renamed p1 profile with progress/name retained after restart. [Save acceptance](evidence/M7-009-save-acceptance-20261010.md). | Clean, Sandbox, p1 and subtitle s2 are separate branches. No all-account, arbitrary-writer or universal durability claim. |
| Vehicles and lifecycle | Separate motorcycle/tuning/purchase reload, Restart/Abandon, police bust/fine recovery, driving/pause/audio, Alt+Enter and garage/free-roam transitions. [Vehicles](evidence/M7-004-vehicle-acceptance-20261010.md), [transitions](evidence/M7-007-transition-acceptance-20261010.md). | Historical customization/map coverage remains historical, not every vehicle/part or a repeated current-build city tour. |
| Media and language | English gameplay, reported promotional-video playback/Y skip, repaired phone tutorial and readable Great Palace/Lester captions on controlled s2. [Media](evidence/M7-006-subtitle-replay-20261010.md), [language](evidence/M7-010-language-acceptance-20261010.md). | Other gameplay languages, every scene/telephone subtitle and music continuity are unverified. |
| Diagnostics | Actual p1 F10 has a reviewed coherent window-only gameplay image, complete audit, verified artifacts/helper cleanup and session records. | WGC is explicitly enabled in p1 but OFF by default in source. No exact frame/sample timing, cadence, FPS or graceful-worker-teardown claim. |

The [collected set](evidence/M7-011-representative-set-20261010.md) and
[targeted regression](evidence/M7-013-targeted-regression-20261010.md) require no
replay of accepted name/reload/F10/subtitle/Landmark observations.
Fresh SDK20 source builds pass 82/82 WGC-enabled and 77/77 default-OFF build
actions; neither has replaced p1 or acquired its physical PASS.
[Integration provenance](evidence/M7-002-sdk-profile-integration-20261010.md)
separates SDK v0.20.0.0/`5dd58f89...` from the tested hybrid.
Deck/Proton, exact stock/interval1/stock timing and full campaign/end-game remain
deferred or unverified, not silently accepted.

## Tested build and host

The following table and route matrix retain the historical M6 cohort. The
machine-readable `config/alpha-compatibility-matrix.json` remains its frozen
27-issue snapshot; later issue additions and status changes do not expand that
historical coverage. M7 observations above do not rewrite those tested identities.

| Component | Tested value |
|---|---|
| MCLA-R | `0.7.0.0`, commit `6c167fae0805d8f1417e8e0484c8614fef27f77a`, clean Windows AMD64 `Release` |
| ReXGlue | `0.10.0.0`, commit `5d3e98c064c38e0769b4f59d11729c8f6270eb83` |
| Game | Complete Edition, Title ID `545407F8`, Media ID `5940C9DB`, module hash `1984A3354B78CE19` |
| Host | Windows 11 Pro x86-64 build 26200; AMD Ryzen 9 5900X; NVIDIA GeForce RTX 3090 driver `32.0.16.1088` |
| Graphics | Direct3D 12, stock 30 FPS timing |
| Resolutions | 1280x720 and 2560x1440 frontend/gameplay routes |
| Audio output | Windows default SDL playback endpoint; device identity withheld |
| Gamepads | Xbox-compatible SDL controller; Steam Controller through Steam Input/XInput wrapper |
| Wheel | Thrustmaster T300RS, PlayStation rim, two-pedal set |

The authoritative machine-readable form is
`config/alpha-compatibility-matrix.json`. Raw game data, saves, logs, screenshots,
and device identifiers remain private.

## Route matrix

Historical M6 routes and artifact-specific limits:

| Area | Alpha status | Tested path | Important limits |
|---|---|---|---|
| Boot and frontend | Verified | Exact Complete Edition image reaches the recognizable title/frontend through the unpatched guest-selected route. | Bink fidelity and first-run account/OOBE are not verified. |
| Saved single player | Verified with limitations | Progressed local save, autosave overwrite, fresh-process reload, and transactional recovery. | Native clean first-run creation and player-name editing are not alpha claims. |
| Gamepad input | Verified | Complete digital/analog matrix, focus recovery, hotplug, and latest-active arbitrary-slot selection. | Prompts are Xbox-style; remote software may not forward rumble. |
| Racing wheel | Verified with limitations | T300 steering/pedals/buttons, hotplug, wheel/gamepad switching, centering, finite curb/collision force, and Alt-Tab recovery. | Only T300 is physically verified; curb submission is intermittent; exact Xbox 360 force fidelity is not claimed. KI-022 remains open because centering can still disappear despite stable latest-active routing. |
| Graphics | Usable with open S2 defects | Frontend, HUD, city, garage, day/night, rain, reflections, motion, shadows, particles, and current-artifact non-black Photo Mode capture on RTX 3090/D3D12. | Colored light amplification, alpha shimmer, minimap flicker, neon projection geometry, and white-paint overexposure remain open. The historical tested build had black Photo Mode JPEGs; KI-021 is closed on the v0.10.0.2 successor. No alternate GPU or parity claim. |
| Audio | Verified with limitations | Six audible classes, long XMA/XAudio/SDL output, pause/resume, and default-device recovery. | Exact mix and music continuity are not claimed; XMP system music is unavailable. |
| City streaming | Verified | Continuous route through all project-defined major city zones and back to Hollywood/Sunset. | Not every road, collectible, event, or campaign item. |
| Race systems | Representative coverage | Head-to-head and series events, traffic, police observation, checkpoints, finishes, rewards, and repeated completions. | Not an exhaustive opponent/event/campaign matrix. |
| Garage/customization | Representative coverage | Vehicle/part/exhaust/paint purchase, switch, autosave, and fresh-process reload. | Not every vehicle, part, tune, paint, or garage. |
| Window/device lifecycle | Verified with limitations | Pause, focus loss, minimize/restore, controller hotplug, audio endpoint recovery, external close, and current-successor runtime Alt+Enter/LMB-double-click fullscreen switching. | No OS suspend/hibernate or display-adapter-removal claim; KI-023 records one separately classified host NVIDIA/Sunshine failure. Steam Deck/Proton fullscreen validation is deferred to M8 under the 2026-10-03 owner scope. |
| Offline services | Bounded offline behavior | Reached achievement/stat/voice/service surfaces fail or degrade deterministically without fabricated online state. | Multiplayer, leaderboards, Rate My Ride, voice transport, and retired services are unavailable. |
| Split long-session gate | Verified with limitations | Historical mixed free-roam/race coverage, a 7,202-second ReXGlue v0.10.0.1 frontend stage, and current-artifact 3,600-second mixed gameplay passed with bounded resources, physical captures, clean host audit, controlled exit, and a complete preserved save. | KI-026 remains intermittent/open and requires F10 capture if it recurs; KI-022 wheel-centering stability is outside the gamepad-only gate. Historical frontend and current gameplay executables differ, so no same-artifact, continuous three-hour, legacy five-stage, or ten-hour monolithic claim. |
| Full campaign / all Complete Edition content | Not verified | Exact edition and South Central city data load in covered routes. | Revised M7 requires representative identified event types/content/lifecycle only; full story and exhaustive event/vehicle/cutscene/unlock coverage remain unverified and outside that gate. |

## Known issues

The canonical issue register is [known-issues.md](known-issues.md). The most
visible current alpha limitations are:

- `KI-001`: closed for the bounded local Clean startup and p1 name/progress reload;
  other account/sign-in variants remain unverified;
- `KI-003`: representative promotional-video playback and Y skip are now observed;
  exact decoder identity and all-video fidelity remain unverified;
- `KI-013`, `KI-015`, `KI-016`, `KI-019`, `KI-020`: open rendering defects;
- `KI-017`: music may stop while ambient SFX continue;
- `KI-018`: retired-service unlocks remain retail-locked by policy;
- `KI-021`: Photo Mode JPEGs were black on the historical tested build; the
  historical successor has a physically and machine verified non-black route;
- `KI-022`: stable Steam Input mirror routing reduced false ownership changes,
  but wheel centering can still disappear from an unresolved FFB lifecycle fault;
- `KI-023`: one long run ended in a host NVIDIA device removal during a Sunshine
  crash storm, now classified separately by the soak harness;
- `KI-024`: the omitted delivery constructor callback is closed on its identified
  historical artifact by an owner-confirmed active transition and five playable minutes;
- `KI-025`: closed on the v0.10.0.2 successor by a real-title
  windowed-to-fullscreen-to-windowed run covering both Alt+Enter and LMB
  double-click; Steam Deck/Proton presentation is deferred to M8 validation.
- `KI-026`: selecting Race Back after a lost Red Light Driver event can leave
  a live, controllable car behind a stuck aerial camera with pause unavailable;
  three focused current-artifact retries returned normally, so it remains an
  intermittent open issue rather than a blocker or a claimed fix;
- `KI-027`: Deck cadence/audio remains unresolved and deferred to M8. Historical
  measured outputs and later subjective improvements do not establish stable 30
  or current platform support; no new Deck trace is requested for M7;
- `KI-028`: contained for the accepted Windows progressed successor route;
  external synchronization, bounded history and photo-only provenance limits remain;
- `KI-029`/`KI-030`: Deck recovery/runtime deployment remains deferred, despite
  separately tested source fixes and isolated candidates;
- `KI-031`: the reproduced Pinkslip tutorial callback is repaired and its
  tutorial/win/reload route passes. Unseen callbacks and new builds are not
  covered by that physical result.

Closed issues remain in the register so compatibility claims retain their
history and verification links.

## Reading the status labels

- **Verified** means the named route passed on the exact build and reference
  host above.
- **Verified with limitations** means the core route passed but the listed
  exclusions are material.
- **Representative coverage** means one or more representative paths passed;
  it is not exhaustive.
- **Usable with open S2 defects** means the route is playable but known visual
  or audio correctness defects remain.
- **Not verified** is not equivalent to known-broken; it means evidence is not
  yet sufficient for an alpha claim.
