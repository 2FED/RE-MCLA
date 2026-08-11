# Known issues

Owner: MCLA-R maintainers

Purpose: maintain user-visible and developer-relevant limitations with severity, affected builds, workaround, owner, and target milestone.

## Current limitations

| ID | Severity | Status | Description | Workaround | Target |
|---|---|---|---|---|---|
| KI-001 | S1 | Open | A native executable now builds, but startup and playability are not validated. | Use the original game or Xenia; do not treat the M3-001 link result as a runnable release. | M3–M5 |
| KI-002 | S1 | Closed | ReXGlue feasibility for this exact XEX was unmeasured; M2 now has deterministic diagnostic-free codegen and a `GO WITH SDK FORK` decision. | See `docs/evidence/M2-feasibility-report.md`. | M2 |
| KI-003 | S2 | Open | Intro/Bink, XMA audio, Xenos rendering, and offline XONLINE behavior are unverified. | None in MCLA-R yet. | M4–M7 |
| KI-004 | S2 | Open | MCLA-R currently requires its pinned ReXGlue v0.9.0.3 fork for valid FLOAT16_4 mask-3 diagnostics, Unicode-safe Windows paths, and safe input/window teardown. | The submodule pin is automatic; do not replace it with upstream v0.9.0 or unpinned `main`. | SDK/upstream |
| KI-005 | S1 | Closed | ReXApp destroyed the Window before Runtime/input drivers, causing a teardown use-after-free after XEX load. | Fixed in fork v0.9.0.3; see `docs/evidence/M3-003-module-config.md`. | SDK/upstream #336 |

Do not remove an issue without linking verification evidence. Update this document when a new recurring defect is discovered, severity changes, a workaround changes, or a release claim is added.
