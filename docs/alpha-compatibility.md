# Alpha compatibility matrix

This matrix describes the bounded compatibility of the tested MCLA-R alpha. It
is a record of observed routes on one reference host, not a promise that every
Windows system, controller, wheel, race, or campaign path works.

## Tested build and host

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

| Area | Alpha status | Tested path | Important limits |
|---|---|---|---|
| Boot and frontend | Verified | Exact Complete Edition image reaches the recognizable title/frontend through the unpatched guest-selected route. | Bink fidelity and first-run account/OOBE are not verified. |
| Saved single player | Verified with limitations | Progressed local save, autosave overwrite, fresh-process reload, and transactional recovery. | Native clean first-run creation and player-name editing are not alpha claims. |
| Gamepad input | Verified | Complete digital/analog matrix, focus recovery, hotplug, and latest-active arbitrary-slot selection. | Prompts are Xbox-style; remote software may not forward rumble. |
| Racing wheel | Verified with limitations | T300 steering/pedals/buttons, hotplug, wheel/gamepad switching, centering, finite curb/collision force, and Alt-Tab recovery. | Only T300 is physically verified; curb submission is intermittent; exact Xbox 360 force fidelity is not claimed. KI-022 remains open because centering can still disappear despite stable latest-active routing. |
| Graphics | Usable with open S2 defects | Frontend, HUD, city, garage, day/night, rain, reflections, motion, shadows, particles, and current-artifact non-black Photo Mode capture on RTX 3090/D3D12. | Colored light amplification, alpha shimmer, minimap flicker, neon projection geometry, and white-paint overexposure remain open. The historical tested build had black Photo Mode JPEGs; KI-021 is closed only on the v0.10.0.1 successor. No alternate GPU or parity claim. |
| Audio | Verified with limitations | Six audible classes, long XMA/XAudio/SDL output, pause/resume, and default-device recovery. | Exact mix and music continuity are not claimed; XMP system music is unavailable. |
| City streaming | Verified | Continuous route through all project-defined major city zones and back to Hollywood/Sunset. | Not every road, collectible, event, or campaign item. |
| Race systems | Representative coverage | Head-to-head and series events, traffic, police observation, checkpoints, finishes, rewards, and repeated completions. | Not an exhaustive opponent/event/campaign matrix. |
| Garage/customization | Representative coverage | Vehicle/part/exhaust/paint purchase, switch, autosave, and fresh-process reload. | Not every vehicle, part, tune, paint, or garage. |
| Window/device lifecycle | Verified with limitations | Pause, focus loss, minimize/restore, controller hotplug, audio endpoint recovery, and external close. | No OS suspend/hibernate or display-adapter-removal claim; KI-023 records one separately classified host NVIDIA/Sunshine failure. |
| Offline services | Bounded offline behavior | Reached achievement/stat/voice/service surfaces fail or degrade deterministically without fabricated online state. | Multiplayer, leaderboards, Rate My Ride, voice transport, and retired services are unavailable. |
| Split long-session gate | In progress | Historical mixed free-roam/race coverage and a ReXGlue v0.10.0.1 two-hour frontend stage passed; one current-artifact 60-minute mixed-gameplay process is prepared. | M6-014 remains open until the physical hour passes. Historical frontend and current gameplay executables differ; no same-artifact, continuous three-hour, legacy five-stage, or ten-hour monolithic claim. |
| Full campaign / all Complete Edition content | Not verified | Exact edition and South Central city data load in covered routes. | Full campaign, every event/vehicle/cutscene/unlock, and all content are M7 scope. |

## Known issues

The canonical issue register is [known-issues.md](known-issues.md). The most
visible current alpha limitations are:

- `KI-001`: first-run/account/save-permission behavior is not fully native-verified;
- `KI-003`: Bink decode/playback fidelity is not verified;
- `KI-013`, `KI-015`, `KI-016`, `KI-019`, `KI-020`: open rendering defects;
- `KI-017`: music may stop while ambient SFX continue;
- `KI-018`: retired-service unlocks remain retail-locked by policy;
- `KI-021`: Photo Mode JPEGs were black on the historical tested build; the
  current v0.10.0.1 successor is physically and machine verified non-black;
- `KI-022`: stable Steam Input mirror routing reduced false ownership changes,
  but wheel centering can still disappear from an unresolved FFB lifecycle fault;
- `KI-023`: one long run ended in a host NVIDIA device removal during a Sunshine
  crash storm, now classified separately by the soak harness;
- `KI-024`: the omitted delivery constructor callback is closed on the current
  artifact by an owner-confirmed active transition and five playable minutes;
- `KI-025`: startup fullscreen is configurable, but runtime Alt+Enter and
  double-click window-mode switching are not implemented.

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
