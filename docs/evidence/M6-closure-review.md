# M6 closure review

Date: 2026-09-04

Decision: `GO M7 — STABLE SINGLE-PLAYER ALPHA`

Release version: `0.8.2.2`

## Closure decision

All 16 M6 tasks are complete. The accepted progressed-save route is now a
repeatable offline single-player alpha rather than only a first-race vertical
slice. It covers every project-defined city zone, representative race and
garage systems, transactional save recovery, profile/settings behavior,
bounded offline services, audio and device lifecycle, performance telemetry,
model-agnostic wheel input with one physically verified T300, local live/crash
diagnostics, and a bounded split long-session gate.

The long-session decision combines the immutable 7,202-second frontend stage
from suite `20260831-133236-2cecb67b` with current-artifact mixed-gameplay run
`20260904-095603-fb908583`. The latter completed exactly 3,600 measured seconds
with thirteen ordered resource samples, four nontrivial captures from five
attempts, owner-confirmed garage/pause/focus lifecycle actions, controlled exit
zero, zero fatal/NVIDIA/Sunshine markers, and a complete evolved-save archive.
Historical frontend and current gameplay executable identities remain explicit;
no same-artifact continuous three-hour or legacy five-stage soak is claimed.

## Accepted capability boundary

- saved offline gameplay, races, deliveries, police/traffic, garage transitions,
  and progression are practical across fresh processes;
- all project-defined major city coverage zones stream in one continuous route;
- representative head-to-head, series, reward, repeated-race, customization,
  purchase, and save/reload paths pass;
- autosave overwrite, interrupted-write recovery, corruption handling, and
  storage-full rejection preserve a prior valid state;
- six audio classes, two-hour audio stability, pause/resume, and default-device
  recovery pass without claiming exact mix or uninterrupted music;
- arbitrary-slot/latest-active gamepad routing and T300 wheel/gamepad switching
  pass; other wheel models remain configuration-compatible, not physically
  verified;
- F10 live packages and automatic native crash packages are bounded, local-only,
  privacy-labeled, and preserve diagnostic/save context;
- current mixed gameplay stays within the declared memory, handle, thread, and
  host-display-health bounds and preserves the final progressed profile.

## Final verification

The aggregate review ran 23 selected M6 and integration fixture gates covering
M6-001 through M6-016 plus the constructor-registry, delivery, Photo Mode,
Race Back, and diagnostics successors. All pass. The real M6-014 result also
revalidates independently with 3,600 seconds, thirteen samples, four captures,
controlled exit, and archive SHA-256
`6539EB94AA60E0FBF7403D760B8B3FB35A1AB7876584118BE55EB9CDE1105888`.
Project structural review passes ast-grep 3/3, and the immutable environment
bootstrap passes 12/12.

Closure review found and fixed two stale cross-task assumptions before the gate
was accepted. The host-config verifier now knows all 54 current keys, including
the later wheel and diagnostics additions. The reached-unsupported verifier now
preserves the historical M6-013 eight-stub result while separately requiring
the current M6-015 eight-hook, zero-stub, wheel-only 64-slot FFB surface. These
are verification/documentation fixes in `0.8.2.2`; the accepted `0.8.2.0`
physical-hour executable was not rebuilt or substituted.

## Residual scope and M7 handoff

No S0/S1 defect remains inside the bounded progressed-save alpha route. KI-001
first-run/profile creation remains outside that route and is explicit M7 work;
KI-023 is a contained host NVIDIA/Sunshine failure classification whose accepted
successor hour had zero recurrence. Open S2/S3 limitations include Bink fidelity,
colored-light amplification, alpha shimmer, minimap flicker, music continuity,
neon projection geometry, white-paint overexposure, intermittent wheel-centering
loss, runtime fullscreen switching, and intermittent Race Back camera restore.
F10 should be used immediately if a transient issue recurs.

M7 begins with M7-016, not the campaign matrix itself. That task must produce
one private toolchain-free Syncthing folder with launcher-relative game, user,
cache, log, diagnostic, and result roots; prove atomic round-trip evidence and
save handling after relocation; and physically disposition one pinned-Proton
Steam Deck route before campaign testing relies on it. This is not yet a Steam
Deck, native Linux, public-package, full-campaign, rendering-parity, variable-FPS,
or macOS support claim.
