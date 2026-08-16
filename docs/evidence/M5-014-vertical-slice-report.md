# M5 first playable race vertical-slice regression report

Decision: `GO M6 — FIRST PLAYABLE RACE VERTICAL SLICE`.

This sanitized report links the public evidence for every M5 task. Raw logs, captures, saves, controller identity, host paths, and proprietary content remain private.

| Task | Accepted decision | Public evidence |
|---|---|---|
| M5-001 | `pinned-save-sunset-strip-race-v1` | `docs/evidence/M5-001-first-race-route.md` |
| M5-002 | `world-rpf-streaming-pass` | `docs/evidence/M5-002-world-streaming.md` |
| M5-003 | `rendering-categories-pass` | `docs/evidence/M5-003-rendering-categories.md` |
| M5-004 | `host-rtv-ordering-depth-bounded-s2` | `docs/evidence/M5-004-edram-depth-report.md` |
| M5-005 | `representative-material-pipeline-pass` | `docs/evidence/M5-005-material-pipeline.md` |
| M5-006 | `saved-gameplay-input-pass` | `docs/evidence/M5-006-gameplay-input.md` |
| M5-007 | `ffb-withheld-host-rumble-bounded` | `docs/evidence/M5-007-force-feedback.md` |
| M5-008 | `stock-30-fixed-step-and-real-time-throughput-pass` | `docs/evidence/M5-008-physics-timing.md` |
| M5-009 | `six-class-audio-stream-presence-pass` | `docs/evidence/M5-009-audio-event-matrix.md` |
| M5-010 | `canonical-route-streaming-io-allocator-bounded-pass` | `docs/evidence/M5-010-route-failure-report.md` |
| M5-011 | `save-content-prerequisite-pass` | `docs/evidence/M5-011-save-content-contract.md` |
| M5-012 | `first-series-results-return-and-release-restart-pass` | `docs/evidence/M5-012-race-results.md` |
| M5-013 | `five-race-bounded-resource-growth-pass` | `docs/evidence/M5-013-race-resource-growth.md` |
| M5-014 | `vertical-slice-regression-report-pass` | `docs/evidence/M5-014-vertical-slice-report.md` |

## Regression checklist

- [x] One complete Ian event series reaches final results and controllable free roam.
- [x] The changed result save reloads in a fresh optimized process.
- [x] Release gameplay sustains the stock 30 FPS / 60 Hz timing contract.
- [x] Road, buildings, vehicles, traffic, sky, shadows, particles, and HUD are usable in the bounded slice.
- [x] One controller, gameplay input, core six-class audio, streaming, and save/content paths are usable.
- [x] Five repeated race completions remain within bounded host memory, handle, thread, and GPU process-memory growth.
- [x] All accepted routes reject fatal, assertion, unregistered-function, guest-crash, and device-loss markers.

## Scope

This is one pinned-save, one-controller, one-event-series vertical slice. It is not first-run/OOBE coverage, broad race/content coverage, multi-pad validation, advanced title-driven force feedback, whole-frame console parity, every graphics backend, or a general leak-freedom proof. Open visual deviations remain tracked in `docs/known-issues.md`.
