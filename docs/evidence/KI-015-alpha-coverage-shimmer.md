# KI-015 alpha/coverage stipple and shimmer

KI-015 is an open S2 rendering defect. Some traffic-signal housings and foliage
show a dotted partial-coverage pattern where the console-intended silhouette is
expected to be stable. The owner also observes visible shimmer on the affected
signals and trees in motion.

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

## Diagnosis boundary

Relevant candidates include the guest alpha-test state/reference, Xenos
dithered alpha-to-coverage emulation and host `SV_Coverage` sample mapping,
texture mip/LOD selection for thin billboard geometry, and the MSAA ownership or
resolve path. ReXGlue intentionally implements guest dithered alpha-to-coverage
manually, so the mere presence of a dither pattern is not itself proof of which
calculation is wrong. M6-004 should capture one affected draw/state tuple and a
short fixed-camera temporal burst before changing alpha thresholds, coverage
offsets, sample masks, texture LOD, or resolve behavior.
