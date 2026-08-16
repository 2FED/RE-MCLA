# M5 closure review

Date: 2026-08-17

Decision: `GO M6 — FIRST PLAYABLE RACE VERTICAL SLICE`

Release version: `0.5.0.0`

## Closure decision

All 14 M5 tasks are complete. The accepted pinned-save route now covers a
first playable race vertical slice rather than only free roam: the owner enters
an available Ian event series, completes every event in that series, reaches
final rewards/results, returns to controllable free roam, and reloads the
changed save in a fresh optimized process. The title is closed externally with
`WM_CLOSE`, matching its console-style lifecycle; an in-game desktop-exit menu
is neither expected nor claimed.

The tracked sanitized report in
`docs/evidence/M5-014-vertical-slice-report.md` links every task decision and
public evidence document. Its accepted private source report is normalized-
content-bound by the verifier and contains no private paths, raw digests, saves,
logs, captures, controller identity, or proprietary content.

## Accepted capability boundary

- the exact pinned completed save reaches saved gameplay and the selected race
  route;
- one complete Ian event series reaches final standings/rewards and returns to
  controllable free roam;
- the resulting save loads in a fresh Release process;
- optimized gameplay satisfies the stock 30 FPS / 60 Hz fixed-step timing
  contract;
- one SDL controller, representative gameplay input, and physical reconnect
  are usable;
- music, ambient, voice, engine, collision, and UI audio classes are both
  machine-present and owner-heard, with presence rather than mix parity claimed;
- world streaming, representative rendering/material/depth paths, and the
  concrete save/content surface are exercised without a fatal route failure;
- five owner-confirmed race completions in one Release process remain inside
  declared private-memory, working-set, handle, thread, and dedicated/shared
  GPU-memory growth bounds.

## Final verification

The closure regression passed all 18 selected M5 and integration gates:

- all fourteen M5 task gates, including separate race restart and repeated-race
  resource verification;
- current analysis-config, generated-integration, and snapshot-generated-
  integration gates;
- project structural review with ast-grep: 3/3 rules;
- aggregate environment and immutable-input bootstrap: 12/12 checks.

M5-013 independently passes five positive fixtures, fourteen fail-closed
negatives, and 39 source checks. M5-014 independently rehashes eleven immutable
accepted result files plus the current physical M5-013 result, and passes one
positive fixture, five fail-closed negatives, and 28 source checks. Task commits
run from `bdea051` through `a596f5d`, with the final M5-013 and M5-014 task
commits at `e60f444` and `a596f5d` respectively.

## Residual scope and M6 handoff

No S0/S1 issue remains inside this bounded vertical slice. This release does
not claim first-run/OOBE correctness, every race or city region, multi-pad
behavior, advanced title-driven force feedback, indefinite-session leak
freedom, whole-frame console parity, or every graphics backend.

The known visual defects remain explicit S2 work rather than hidden acceptance
exceptions: KI-013 tracks over-strength saturated vehicle-light reflections,
KI-015 tracks alpha/coverage stipple and shimmer on some signals and foliage,
and KI-016 tracks intermittent minimap flicker. Profile/save-flow expansion,
broader streaming/content coverage, and long-session stability also remain M6
scope. These limitations do not block the demonstrated first playable race
vertical slice, so the milestone advances to M6.
