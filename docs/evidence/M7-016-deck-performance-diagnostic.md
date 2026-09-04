# M7-016 Steam Deck performance diagnostic

## Decision

`native-vulkan-performance-candidate-and-visible-f10-ready-physical-rerun-required`

MCLA-R `0.11.0.0` pins ReXGlue `v0.12.0.0` commit
`84f9ec28ef76781768fe23080bfc08defdcfc334`. The current private child is:

`mcla-0.11.0.0-win64-86CDF916E35F65DF-6E42A5CA4417D209-58B3385C89D9E499`

Its immutable manifest SHA-256 is
`89D4D648E49BE467563AA52B8ED06AB05F90933106FE2EB014EAEB7CE6C3E3A8`;
the 25-file hash lock SHA-256 is
`347696A2D91E914B8350E2A72DDEEDF92E260346E902F52EE8DB95C0DF36D8A2`.
Clean Windows build, relocation, diagnostics, launcher verification, and
current-child-only retention passed in private run
`bundle-20260904-171436-fdf6e2c9`. The obsolete 0.10.0.0 child's returned
diagnostics, logs, and results were preserved below
`private/evidence/M7-016/deck-return-20260904-proton-9.0-4` before its verified
stale unlocked writer lock and multi-gigabyte payload were pruned.

## Returned Proton 9.0-4 trace

The owner observed MangoHUD reporting 86--90 presentations per second while
static and roughly 50 while driving, while perceived gameplay was mostly 1--5
FPS and at best roughly 10. Driving also caused audio breakup while MangoHUD
reported only about 50--60 percent CPU/GPU utilization and an 800--900 MHz GPU
clock.

The same-process active-driving F10 pair measured 7.34 unique guest outputs/s
against 59.93 guest vblanks/s, or 12.24 percent. Its guest-frame interval was
116.86 ms median and 134.46 ms p95. The bounded audit recorded 1,152 audio
underruns, two streaming stalls, five shader translations, and six pipeline
creations. The renderer CPU/GPU fields are wall-clock submission spans that
include queue idle and therefore are not pure active-work timings; no specific
CPU or GPU bottleneck is claimed from them.

## Current candidate

ReXGlue v0.12.0.0 exposes the already compiled Xenos backend through
`RuntimeConfig::gpu_backend` and `--gpu_backend=any|d3d12|vulkan`. MCLA keeps
the `any`/D3D12 default on native Windows and selects native Vulkan when Proton
or Wine environment markers are present, avoiding the extra D3D12-to-VKD3D
translation layer. The portable launcher accepts
`--gpu-backend=d3d12` as an explicit fallback and A/B route. This is a physical
test candidate, not a performance-fix claim.

F10 no longer relies on the inaudible Windows completion sound under Proton. A
top-right, input-transparent overlay displays `F10: RECORDING...` during the
worker capture and `F10: READY` for approximately one second only after the
package and latest pointer are published. Repeated and failed requests display
`BUSY` and `FAILED` respectively.

`scripts/analyze-portable-deck-performance.ps1` selects a monotonic pair of
complete F10 packages from one process and reports:

- guest output and guest vblank deltas and rates;
- frame-interval p50/p95/max;
- renderer wall-clock submission-span p50/p95/max where that audit exists;
- streaming reads/stalls, audio underruns, shader translations, and pipeline
  creations;
- a conservative long-span, outside-recorded-span, or inconclusive
  classification.

Vulkan packages need not contain the D3D12-only triggered audit; the unique
guest-output/vblank pair remains valid and the analyzer reports the audit as
unavailable rather than rejecting the package.

## Gates

- ReXGlue v0.12.0.0 clean dual-backend Release build/install: PASS.
- ReXGlue full RelWithDebInfo suite: 1,802 executed PASS, four known skips,
  zero failures out of 1,806 registered tests.
- Focused ReXGlue frame-audit tests: 11 assertions / 3 cases PASS.
- Existing performance telemetry contract: 1 positive / 14 fail-closed
  negatives / 22 source checks PASS.
- Deck analyzer fixture: 2 positives / 2 fail-closed negatives PASS.
- Portable bundle fixture: PASS.
- Build-matrix fixture: 1 positive / 6 negatives PASS.
- Diagnostics source fixture: 1 positive / 7 fail-closed negatives / 41 source
  checks PASS.
- Real Release diagnostics/crash package run
  `20260904-171409-9cfb20e3`: PASS.
- Clean current bundle/relocation run `bundle-20260904-171436-fdf6e2c9`: PASS.

No MangoHUD correction or performance improvement is claimed yet. Physical
closure requires launching the new child under Proton 9.0-4, confirming Vulkan
selection in the returned runtime log, comparing perceived gameplay/audio and
unique-output cadence, verifying the visible F10 `RECORDING` then `READY`
states, and completing the remaining fullscreen-toggle/save/reload/recovery
matrix. Use the explicit D3D12 override only if Vulkan fails or for a bounded
A/B comparison.
