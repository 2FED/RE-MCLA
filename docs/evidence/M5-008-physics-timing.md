# M5-008 stock physics timing

Decision: `stock-30-fixed-step-and-real-time-throughput-pass`

## Scope

This gate validates the stock 30-FPS timing path during a bounded saved free-roam
route. It does not enable the known 60-FPS patches. The host enters gameplay
from the pinned post-OOBE save, dismisses the bounded startup overlays, captures
a neutral start frame, holds full throttle for ten seconds, and captures an end
frame. Three isolated cycles use fresh user/cache roots and close externally by
exact PID/window `WM_CLOSE`.

The canonical performance baseline is an optimized `Release` build. A separate
calibration showed that `RelWithDebInfo` instrumentation/optimization overhead
reduces this CPU-bound route to roughly 20 updates and outputs per second on the
same RTX 3090 host. RelWithDebInfo remains useful for correctness diagnostics,
but it is not a valid stock-speed performance baseline and is not used for the
acceptance claim.

## Instrumentation

The default-off, InitOnly `mcla_physics_timing_probe` wraps the existing
generated function registered at guest address `0x821BDA90`. This is the stock
timer function containing the audited Complete Edition 60-FPS game-speed patch
site; no guest byte is changed. The wrapper calls the original function first,
then records only bounded float bit patterns and aggregate ranges:

- effective delta at timer offset 8;
- clamped delta at offset 88;
- raw host-derived delta at offset 92.

ReXGlue v0.9.0.19 adds nonvirtual, thread-safe Presenter diagnostics for the
latest active guest-output sequence and the guest-vblank count. The graphics
plugin ABI is unchanged. Guest time is measured independently through the
existing 50-MHz clock.

## Accepted physical result

Private result:
`private/evidence/M5-008/20260814-150733-da37caed/result.json`

- ReXGlue: `v0.9.0.19`, commit
  `53c16fcfcbfee83752b7689cf74aba1d69a185fa`.
- Build: clean Windows AMD64 `Release`.
- Result tree: 25 files, 34 directories, 35,680,283 bytes,
  SHA-256 `3BEDA1E70C9CE386293528B8032E1A21AE1D837A9344F9C60A8DAD522F35BD35`.
- Clean-build log SHA-256:
  `5630DEC3D1F4EB65E556C3307DA06AB3BC9DF20A0FC40288E3B544F8590B857D`.

| Cycle | Host window | Timer calls | Simulated/wall | Guest clock/wall | Vblank | Guest output | Raw delta range | Frame difference |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 10.000000 s | 300 | 999,990 ppm | 1,000,002 ppm | 600 / 60.000 Hz | 300 / 30.000 FPS | 29,865-36,499 us | 152,775 |
| 2 | 10.000000 s | 300 | 999,990 ppm | 1,000,002 ppm | 600 / 60.000 Hz | 300 / 30.000 FPS | 31,525-35,401 us | 161,873 |
| 3 | 10.000507 s | 300 | 999,939 ppm | 1,000,002 ppm | 600 / 59.996 Hz | 299 / 29.898 FPS | 30,812-49,293 us | 172,175 |

All first sixteen effective/clamped records in every cycle contain the exact
float bits `3D088889`, which round to 33,333 microseconds. Each cycle records
eight causal synthetic gameplay-input markers, two canonical 1280x720x32 BMPs,
one controlled close, one execution-complete marker, one hard-exit marker, and
zero fatal/assert/device-loss markers. Source-game, pinned-save, four Release
runtime artifacts, logs, captures, and the complete evidence tree are rehashed
after execution.

## Calibration and decision boundary

The route was calibrated before acceptance:

- RelWithDebInfo info logging: 199 timer calls and 198 presents per ten seconds;
- disabling primary-buffer-end submission: 195/194;
- increasing D3D12 frames in flight from three to four: 199/199;
- synchronous shader/pipeline creation: 204/203;
- optimized Release with synchronous pipeline creation: 300/299 in the initial
  calibration and 300/300, 300/300, 300/299 in the canonical run.

The first three A/B results rule out submission boundaries, frames-in-flight,
and asynchronous pipeline creation as the primary cause of the non-Release
slowdown. No renderer or guest-timing behavior patch was justified. The
acceptance claim is therefore intentionally build-qualified: production Release
sustains stock game time and output cadence; RelWithDebInfo does not establish a
performance baseline.

## Automated checks

- `scripts/test-physics-timing-smoke.ps1`: one physical positive, 20
  fail-closed negatives, 18 source-contract checks.
- PowerShell AST parsing: all three M5-008 scripts pass.
- Focused ReXGlue `[ui],[chrono],[controller-matrix],[rtl]`: 42 cases and 216
  assertions pass.
- `ast-grep scan` and `ast-grep test`: 3/3 project rules pass.
- Both Release and RelWithDebInfo SDK/app configurations compile and link; only
  Release is used for the stock-speed evidence.

The verifier rejects timer/address drift, non-exact fixed-step bits, malformed
records, guest-clock or vblank drift, output below the calibrated 30-FPS band,
simulation/wall mismatch, static response frames, malformed log rotations,
missing lifecycle markers, reparse traversal, data drift, and result/physical
evidence disagreement.

## Limits

This is a ten-second full-throttle saved free-roam timing baseline, not a full
race, collision/AI parity matrix, 60-FPS validation, whole-frame comparison, or
proof for non-Release builds. M5-009 through M5-014 still own audio, route
failures, persistence, race completion, repetition/leaks, and final vertical-
slice regression coverage.
