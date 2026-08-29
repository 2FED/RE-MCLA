# M6-015 racing-wheel input and force feedback

## Decision

M6-015 passes as `model-agnostic-wheel-input-and-title-event-ffb-pass`.
The host accepts SDL-classified racing wheels without a model allowlist, maps a
configurable wheel layout to the Xbox 360 wheel guest surface, and gives guest
slot zero to the latest meaningfully active wheel or gamepad. The title's
64-slot force-feedback API reaches SDL haptics with bounded gain and lifecycle
behavior.

One Thrustmaster T300RS Racing Wheel is physically verified. Logitech, MOZA,
and other SDL wheel-class devices are configuration-compatible, not physically
verified. Exact force fidelity relative to Xbox 360 hardware is not claimed.

## Immutable result

- Compatibility run: `20260829-171205-28b1671a`.
- Result SHA-256: `63FEAD6AE42313844B9BBB384F9674E5D2C6FF6DEFBE0C1D407FEF1146956CCD`.
- Accepted title-event run: `event-ffb-20260829-164110-ddb9626b`.
- Event result SHA-256: `00FA3C5E03C19A1D865E81BFFD0A95D37B2FB083F78AC41DE8EA7EE43D9990D8`.
- Event runtime-log SHA-256: `0BF6AB521ED293C8804CA03AE3331D8281EF9CDDC883E5A4292DA84AAD78AC41`.
- Event observation SHA-256: `5072DAC9FDF4CC549B06DB7112FE6D7F042A4B07A69262BB22BC9DB92B5D718E`.
- ReXGlue SDK: `v0.10.0.0`, commit `5d3e98c064c38e0769b4f59d11729c8f6270eb83`.
- Preserved source-save tree SHA-256: `0E00655414C6822B40B464CAC9A872E43EE0DE12A19F04E08A47BB7D1048FC2A`.

Raw logs, device details, captures, and saves remain private. The public result
contains hashes and bounded aggregate observations only.

## Physical input and device lifecycle

The T300 reference reports four axes, thirteen buttons, one hat, and a two-
pedal set without clutch. Physical evidence covers steering, accelerator,
brake, paddles, face/menu buttons, L3/R3, disconnect/reconnect, and guest
control after hotplug. L2/R2 are present on the wheel rim but remain unmapped
for this two-pedal reference layout.

The accepted event session records 55 active-controller transitions while a
native Steam Controller and its XInput wrapper coexist with the wheel. Owner
observation confirms wheel -> gamepad -> wheel switching; wheel force stops
when gamepad owns guest slot zero and returns when wheel activity takes it back.
This is latest-meaningful-input routing, not a fixed physical slot number.

## Force-feedback surface

The guest exposes 64 effect slots. Implemented families are constant, ramp,
square, sine, triangle, sawtooth up/down, spring, and damper. Direct T300 probes
physically confirm constant, square, spring, and damper semantics. The title
event route creates and parameterizes constant, damper, spring, and square;
finite square events are physically confirmed in gameplay.

Host policy is deliberately bounded:

- overall/default gain: 100%;
- continuous periodic engine/road-texture gain: 40%;
- infinite directional constant gain: 0%;
- nonzero finite transient minimum: 75% of current overall gain;
- zero and infinite effects are not raised by the transient floor.

The accepted run did not create an infinite periodic engine effect, so it makes
no claim that engine vibration is present in every session. Centering, Alt-Tab
force recovery, and latest-active-device force recovery were physically felt.

## Causal curb and collision evidence

The final runner separates curb and collision attempts into operator-bounded
time windows before asking for physical results. Runtime telemetry contains
three finite square START edges in the curb window and six in the collision
window, each at or above the configured transient floor. The owner felt both
classes. Curb force is intermittent because the title does not submit a finite
event for every visually plausible curb or rough edge; the PASS is for causal
title-submitted events, not every contact with roadside geometry.

The game was closed manually with the window close button after the PowerShell
host disappeared. The runtime log then ended normally with `Window closing`,
`Execution complete`, and the title's controlled hard-exit marker. No crash or
device-loss marker was present. The observation/result records were recovered
from the contemporaneous owner report, recorded action-window boundaries, and
machine telemetry; the recovery provenance is explicit rather than presented
as an uninterrupted questionnaire capture.

## Verification

- focused wheel suite: 227 assertions / 9 cases;
- focused SDL input suite: 358 assertions / 31 cases;
- compatibility fixture: 1 positive / 13 fail-closed negatives / 31 source checks;
- event fixture: 2 positives / 30 fail-closed negatives / 44 source checks;
- host-config fixture: 1 positive / 18 fail-closed negatives / 22 source checks;
- exact ReXGlue tag and clean SDK worktree required by the final report;
- privacy-safe hashes bind the input result, focus probe, event result/log,
  operator observation, unit-test log, capability probe, reference policy, and
  preserved save tree.
