# M5-007 force-feedback degradation evidence

## Decision

M5-007 is accepted as `ffb-withheld-host-rumble-bounded`. The advanced Xbox 360
force-feedback surface is not implemented, but it is also not advertised to the
title and does not block saved gameplay. Basic XInput-style vibration remains a
concrete host path and has bounded physical evidence. No new runtime or physical
rumble test was required for this report.

## Immutable evidence

- Accepted report run: `20260814-132533-40ee5698`.
- Accepted result SHA-256:
  `709609A904C3A49AD0C88E8CC88DFD794D849FB9A7B9B6B5F8AA0887BA9C1E18`.
- Current saved-gameplay run: `20260814-130533-0b95f6b6`.
- Current saved-gameplay result SHA-256:
  `A89C0CC3E02C8D264B0DA29157021D050276BF46F028BBAAAD9B1FFC220CCEAB`.
- Physical rumble/digital run: `20260812-212030-5fc01c73`.
- Physical disconnect/reconnect run: `20260813-144406-2c1974da`.
- ReXGlue SDK: `v0.9.0.18`, commit
  `923c92d1d1cb721cb704ac603fba263a01ba06aa`.

The report result contains only bounded booleans, counts, public ordinal ranges,
decision strings, and hashes. The final verifier independently re-hashes and
re-parses the source-game, save, current runtime artifacts, complete M5-006 log
and frame set, and the three-layer M4-006 controller evidence. Raw controller
identity, logs, captures, save data, host paths, and device properties remain
private.

## Advanced FFB degradation

The title resolves eight `XInputdFF*` imports at module load, covering ordinals
`0x282..0x289`. All eight remain explicit SDK stubs. The SDL capabilities path
deliberately withholds `X_INPUT_CAPS_FFB_SUPPORTED`, with a source comment tying
that choice to the incomplete guest export surface. Consequently the current
saved-gameplay run has:

| Measurement | Accepted value |
|---|---:|
| Resolved `XInputdFF*` imports | 8 |
| Advertised advanced FFB capability | false |
| `XInputdFF* STUB` call markers | 0 |
| Saved-gameplay input records | 24 |
| Pause ROI correlation | 613,358 ppm |
| Controlled saved-gameplay exit | PASS |

The absence claim is observable because an invoked `REX_EXPORT_STUB` emits an
exact warning containing its export name. Import resolution alone is not
misclassified as execution. The accepted route reaches saved free roam, responds
to controls, opens pause, and exits cleanly without entering the unsupported
advanced FFB path.

## Basic rumble and device matrix

`XamInputSetState` is concrete: it validates the vibration pointer, normalizes
the user, and delegates to `InputSystem::SetState`. The input system returns the
first backend result that is not `DEVICE_NOT_CONNECTED`. The SDL backend checks
the physical rumble property, submits through `SDL_RumbleGamepad`, and maps the
SDL boolean result to an X result.

The immutable physical matrix contains six successful records in exact order:

1. LEFT start, LEFT stop;
2. RIGHT start, RIGHT stop;
3. BOTH start, BOTH stop.

Every record reports `supported=1` and result `00000000`. The owner separately
reported feeling LEFT, RIGHT, and BOTH patterns, but this attestation was not
recorded in the run and is not independently machine-verified. Later immutable
evidence proves physical removal yields guest `0000048F` and reconnection yields
success; an always-connected NOP backend no longer masks those results.

## Scope boundary

This task establishes safe degradation, not advanced effect fidelity. It does
not claim:

- that the title invoked or validated `XInputdFF*` effects;
- title-driven rumble timing, amplitude, or gameplay-event identity;
- a new physical vibration test or in-run operator confirmation;
- multi-controller rumble routing;
- steering-wheel or other specialized force-feedback devices.

If a later canonical route requires advanced FFB, the eight guest exports must
be implemented and tested before the capability bit is advertised. Until then,
withholding the capability is the fail-closed behavior.
