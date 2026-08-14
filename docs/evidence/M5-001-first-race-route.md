# M5-001 canonical first-race route

Date: 2026-08-14

Status: accepted route definition; normative, not yet native-calibrated

The canonical M5 vertical slice is
`pinned-save-sunset-strip-race-v1`. It starts from the exact pinned post-OOBE
save used to close M4, enters nighttime free roam in the white 1998 Nissan
240SX, selects `Sunset Strip Race`, challenges Trevor in the 1975 Datsun 280Z,
finishes first in the two-car race, observes results, returns to controllable
free roam, and then closes the exact game window externally with `WM_CLOSE`.

The machine-readable contract is `config/first-race-route.json`. The contract
is deliberately fail-closed: if the pinned save does not expose the named
event, opponent, or two-car field, a later native calibration must reject the
route instead of silently substituting a different race. M5-001 defines that
identity; it does not claim that the native host has already reached it.

## Pinned identities

The private seed is two files and 537,756 bytes across six directories. Its
canonical tree SHA-256 is
`5F3B045D690CFACE51F43AF504EF6006B7C5B0A913BF99C719F7EE179ABBC471`.
The raw save and header remain ignored and are represented only by their
published hashes in the route contract.

Three immutable 1280x720 stock-Xenia frames anchor the state classes without
publishing the images:

| State | SHA-256 | Contract use |
| --- | --- | --- |
| Nighttime free roam | `A490553067A8F375191F618C34556B82C9FD244FF295519AB4383AAB40434F8B` | White 240SX, dry night, Sunset Blvd starting state |
| Active nighttime race | `C5EC83D8A7DFB1AFD6E64CECAD3CA785E339CD9A0FF7ABC1978D189CDE7E67CB` | Two-car race HUD, opponent, route markers, minimap, and speed HUD |
| Race pause | `6C7BE43B4A6C5C49B7542DC848EF7EF49E0BC8102588A0BF3D9FD8A25C31ADA5` | Race-context pause with Continue, Quit Race, Restart Race, and View Replay |

The existing M2 reload evidence independently records that this exact save
loaded, entered a nighttime race, and exited without rewriting the save. That
is a behavioral baseline, not proof of the event name or race result.

## Exact route

The supported environment is the pinned Complete Edition image, ReXGlue
v0.9.0.18, D3D12 host-RTV rendering at draw scale 1, stock 30 FPS, no title
update or patch, EN/US locale, and exactly one SDL controller in guest slot 0.
The saved control layout is fixed to the default button-automatic layout:
`BACK` opens GPS, `START` pauses, `Y` flashes headlights, the left stick
steers, RT accelerates, and LT brakes/reverses.

The action sequence is:

1. Boot the exact image to the verified Complete Edition title.
2. Press `START` and reach the pinned nighttime free-roam state.
3. Press `BACK`, open GPS, and select exactly `Sunset Strip Race`.
4. Follow GPS to Trevor cruising Sunset Blvd.
5. Press `Y` to flash headlights and challenge Trevor.
6. Reach a two-car race start with position HUD `2/2`.
7. Finish in position `1/2`.
8. Observe the completed-race results transition.
9. Return to controllable free roam.
10. Close the exact game window externally with `WM_CLOSE`.

The owner manual establishes the Xbox 360 default `Y` headlights binding and
the semantic headlights-to-challenge flow. The event/opponent expectation is
cross-checked against the contemporary story walkthrough, which identifies
Trevor and the Datsun 280Z for Sunset Strip Race. These references define the
target; private runtime evidence must still calibrate the target against this
specific saved state before any M5 race run is accepted.

References:

- [Midnight Club: Los Angeles Xbox 360 owner's manual](https://manuals.plus/m/5d4246d3f7123b1252b71d46a945682079fc08ca7561a4a1c2f5ebd90b3e6070)
- [Story walkthrough: Sunset Strip Race](https://midnightclub.fandom.com/wiki/Midnight_Club%3A_Los_Angeles/Walkthrough/Story)

## Acceptance and boundaries

Every transition has a bounded timeout in the JSON contract. A wrong event,
wrong opponent, wrong field size, second-place finish, missing result screen,
missing return state, fatal marker, forced cleanup, or surviving process fails
the eventual physical route. Successful process closure means external
exact-window `WM_CLOSE`; this console title has no in-game Exit command.

M5-001 does not claim first-run OOBE, Bink playback, alternate races, race
result persistence, multiple controllers, title-driven force feedback,
whole-frame Xenia parity, non-D3D12 backends, or non-stock timing. Save
persistence remains M5-011. The first physical event calibration and deeper
world-streaming diagnostics begin with M5-002.

## Verification

`scripts/verify-first-race-route.ps1` enforces the exact schema, image/runtime
identity, seed, stock-Xenia frame identities, start state, controller binding,
event, ordered actions, timeouts, acceptance fields, and exclusions. It also
rehashes the physical `default.xex`, pinned seed tree, and three private Xenia
PNGs after rejecting reparse traversal. `scripts/test-first-race-route.ps1`
provides a physical positive, fail-closed schema/semantic mutations, physical
seed and baseline corruption probes, and source-contract checks.
