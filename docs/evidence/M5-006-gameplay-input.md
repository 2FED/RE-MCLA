# M5-006 saved-gameplay input evidence

## Decision

M5-006 is accepted as `saved-gameplay-input-pass`. The current clean host build
causally delivers and observes saved-gameplay throttle, brake, left/right
steering, neutral release, and pause input. Separate immutable physical evidence
continues to prove the selected SDL controller's digital/analog route and
disconnect/reconnect behavior. No input behavior patch was needed beyond the
default-off bounded acceptance probe.

## Immutable evidence

- Accepted M5-006 run: `20260814-130533-0b95f6b6`.
- Accepted result SHA-256:
  `A89C0CC3E02C8D264B0DA29157021D050276BF46F028BBAAAD9B1FFC220CCEAB`.
- Physical controller evidence:
  - digital run `20260812-212030-5fc01c73`;
  - analog/focus run `20260813-124600-293c07b3`;
  - recovered disconnect/reconnect run `20260813-144406-2c1974da`.
- ReXGlue SDK: `v0.9.0.18`, commit
  `923c92d1d1cb721cb704ac603fba263a01ba06aa`.
- Runtime-log set SHA-256:
  `4795B5650CD91C2E1AAB1BC957297BBFEB339E43F3FF42B4CD76DAC7CFABB0BB`.
- Complete cycle-tree SHA-256:
  `9E964980160FF292B50BE4B94F80B52D2207DF9AA3FE45AE23C056992F8CD2FD`.

The final verifier re-parses every causal marker and frame, re-hashes the
complete private cycle, the 6.5 GB source-game tree and manifest, the pinned
save/header, the clean-build log and executable, and all four runtime artifacts.
It also re-runs the recovered M4-006 physical hotplug verifier. Raw runtime
logs, controller details, captures, save data, and private paths remain outside
Git.

## Current-build gameplay response

The current route records exactly 24 gameplay events: source active, guest
active, source neutral, and guest neutral for each of six sequences. These cover
title Start, full right-trigger throttle, full left-trigger brake, left and
right stick extremes under partial throttle, and gameplay Start/pause. A
separate 24-record sequence uses six slow A pulses to clear bounded startup
overlays before the response frames are collected.

| Measurement | Accepted value |
|---|---:|
| Gameplay source/guest records | 24 |
| Overlay-dismiss source/guest records | 24 |
| 1280x720 response frames | 8 |
| Neutral to throttle-release sampled difference | 158,995 |
| Throttle-release to brake-release sampled difference | 141,223 |
| Left-steer to right-steer sampled difference | 143,056 |
| Pause ROI edge correlation | 613,358 ppm |
| Controlled external exit | PASS |
| Forced cleanup / surviving process | 0 / 0 |

The frames show the vehicle leaving its initial position under throttle,
responding distinctly under braking/reverse input, changing direction under
opposite steering extremes, and opening the readable pause UI. The 500,000-ppm
pause floor is calibrated below two genuine accepted pause captures while a
non-pause gameplay frame produces only 46,465 ppm in the same ROI.

## Physical source and reconnect binding

The M4-006 physical evidence remains the source-of-truth for the selected SDL
controller. It covers the complete standard digital surface, both triggers and
both sticks, focus neutralization, physical removal, guest-visible disconnect,
reconnection to slot 0, and guest-visible success after reconnect. The current
SDK input sources and focused tests are byte-identical between the accepted
`v0.9.0.13` implementation and current `v0.9.0.18`, and the recovered hotplug
result is physically reverified during M5-006 finalization.

This split is explicit: the M5-006 run proves current executable gameplay
response autonomously, while M4-006 proves physical SDL causality and reconnect.
It does not mislabel synthetic input as a new physical-controller run.

## Scope boundary

The accepted route covers one default-layout controller and one pinned saved
night free-roam state. It does not claim:

- completion or parity of the canonical race maneuver sequence;
- multi-controller assignment or simultaneous-controller behavior;
- title-driven force feedback or any new rumble result;
- every vehicle, camera, overlay, input remapping, or gameplay state;
- whole-frame Xenia equivalence.

M5-007 owns force-feedback behavior. Later M5 tasks still own race physics,
audio, persistence, results, and the complete end-to-end vertical slice.
