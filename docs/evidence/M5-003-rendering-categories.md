# M5-003 rendering-category evidence

M5-003 is closed for the pinned post-OOBE night free-roam slice. One clean
RelWithDebInfo route autonomously reaches the title, dismisses reached
phone/tutorial overlays through six bounded causal `A` pulses, and captures 36
native 1280x720 frames before an external exact-window `WM_CLOSE` and exit 0.
There was no force cleanup, surviving canonical process, fatal marker, guest
crash, unsupported PPC marker, or D3D12 device loss.

The accepted private result is
`private/evidence/M5-003/20260814-104624-fde51a30/result.json`, SHA-256
`299392E59CB38AFB44256E773884856FF0C869A96D2045086A65396C1ED4EFCA`.
Raw frames, saves, shaders, logs, and the contact sheet remain ignored below
`private/`.

The eight-file runtime log set is 60,724,703 bytes with manifest SHA-256
`583EA73994FE6A114721C4DD221D80F7D2BF08E5858A4949D314DE1588087273`.
The complete cycle tree contains 50 files and 10 directories totaling
198,793,327 bytes, with SHA-256
`1EBCC00564CCEB6D52209C97285DEF0F2E0838F6AA1692370E6104234E102B61`.

Physical and visual evidence:

- thirty stationary one-second traffic samples are all hash-bound; the road-ROI
  selector chose `traffic-22` with 10,638 changed sampled pixels, and the owner
  confirmed the clearly visible AI vehicle;
- the raised-camera sky frame differs from the world frame at 184,030 sampled
  pixels and visibly includes the night sky above the buildings;
- particle intervals differ at 53,696 and 46,546 sampled lower-frame pixels;
  the owner confirmed visible burnout dust/smoke;
- the labeled Xenia/native contact sheet has SHA-256
  `9AA610D659AF0B95F12B0546B54A911FAAB9DF7CDBCF35039999BBCCFECD0D4A`;
- the owner gave a PASS for the presence/usability of road, buildings, player
  vehicle, AI traffic, night sky, shadows, particles, and HUD. A later
  fixed-camera daylight repro reclassified KI-013 from a supposed minor green
  shadow to an open S2 saturated local-light/specular defect on vehicle paint;
  reflection parity was outside this gate and is not claimed fixed.

The gameplay-timed frozen Xenos checkpoint records 332/332 successful PSOs,
6,195,685 issued draws, 39/39 render targets, 536,929 ownership draws, active
1x/2x/4x MSAA, 2,285 non-identity table-gamma dispatches, and 93,261/93,261
successful staged common-copy resolves. Translation, PSO, binding,
render-target, ownership, resolve, gamma, and refresh failure counters are zero.
The shader detail cap records 256 entries and explicitly reports 104 omitted
detail markers; complete aggregate translation counters are preserved, so this
is bounded privacy saturation rather than a renderer failure.

The final verifier rehashes the pinned M5-002 result and Xenia reference, clean
build log, executable, complete source-game tree, pinned save, four runtime
artifacts, every rotated log, all 36 native BMPs, the contact sheet, and the
complete cycle tree. The fixture suite passes one positive, rejects 61
fail-closed negatives, and checks 23 source contracts. PowerShell parsing is
clean for all three scripts; ast-grep passes 3/3 rule tests and bootstrap passes
12/12.

This evidence does not establish whole-frame Xenia parity, every city region,
daylight/weather rendering, long-session stability, ROV/interlock rendering,
PWL gamma, true-direct resolves, complete reflection/post-processing quality,
or stable alpha-tested/alpha-to-coverage silhouettes for foliage and signals.
Those remain M5-004/M5-010 and M6-004 scope.

The KI-013 reclassification and its three private fixed-camera captures are
documented in `docs/evidence/KI-013-colored-vehicle-reflections.md`.
The separate stipple/shimmer observation is documented in
`docs/evidence/KI-015-alpha-coverage-shimmer.md`.
