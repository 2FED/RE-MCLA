# KI-015 alpha/coverage stipple and shimmer

KI-015 is an open S2 rendering defect. Some traffic-signal housings and foliage
show a dotted partial-coverage pattern where the console-intended silhouette is
expected to be stable. The owner also observes visible shimmer on the affected
signals and trees in motion. During the canonical M6-014 free-roam soak, the
owner broadened the observation to shadowed or translucent objects and
intermittently vehicle surfaces that appear to “noise” in motion. That report
expands the affected surface set but does not prove that every symptom shares
one renderer fault.

## Current evidence

The three fixed-camera PNGs captured for KI-013 under
`private/evidence/M6-004/diagnostic-colored-reflections-20260817-000206/user-captures`
also contain the defect. In particular, the central traffic-signal housing in
`traffic-light-000529.png` has regular background-colored holes across a dark
surface rather than a clean opaque silhouette. Tree foliage in the same view
contains related unstable-looking stipple. The image SHA-256 is
`72EA8F650313735F72C4C1E1F0B1EFA325F2402A682FDFDF4247E7197D2D6E9C`.

A still image proves the incorrect coverage pattern; the owner's motion review
supplies the shimmer classification. No current gate binds the temporal phase
or identifies which draw produced the affected geometry, so the exact renderer
stage is not claimed.

The later owner frames are retained privately under canonical suite
`20260826-100036-1b80ac82`; the white-vehicle frame has SHA-256
`BC2F34D55E372D440C2F6441A934BCA4B591DC8BCC75D1981020F51BAA0DE8F5`.
That still primarily binds KI-020 overexposure rather than temporal noise, so it
is supporting context only and is not presented as a high-cadence KI-015 proof.

## Diagnosis boundary

Relevant candidates include the guest alpha-test state/reference, Xenos
dithered alpha-to-coverage emulation and host `SV_Coverage` sample mapping,
texture mip/LOD selection for thin billboard geometry, and the MSAA ownership or
resolve path. Shadow/compositing coverage is now also in scope because the owner
observes noise outside the original signal/foliage set. ReXGlue intentionally implements guest dithered alpha-to-coverage
manually, so the mere presence of a dither pattern is not itself proof of which
calculation is wrong. M6-004 should capture one affected draw/state tuple and a
short fixed-camera temporal burst before changing alpha thresholds, coverage
offsets, sample masks, texture LOD, or resolve behavior.

The read-only pre-M6 source comparison found that the current DXBC translator
uses the same Xenos 2x2 alpha-to-mask offset extraction and 1x/2x/4x threshold
layout as current Xenia. The fork's optional fuzzy alpha-test epsilon is
default-off, including in the accepted native and stock-Xenia configurations.
The first controlled diagnostic should therefore leave that option false and
capture identical short motion bursts on explicit host-RTV and ROV paths. A
bounded capture should include one opaque vehicle ROI, one shadowed object, and
one alpha-tested signal/foliage ROI. The draw-state record needs the pixel-shader hash, alpha-test function and
reference class, alpha-to-mask enable and four offsets, guest/host sample count,
native-versus-emulated 2x mapping, output coverage mode, color target format,
and resolve path. If the affected draw is alpha-test-only, a later false/true
fuzzy-epsilon comparison may be diagnostic, but it is not an acceptance fix and
must not be mixed into the initial RTV/ROV comparison.
