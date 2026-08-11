# Known issues

Owner: MCLA-R maintainers

Purpose: maintain user-visible and developer-relevant limitations with severity, affected builds, workaround, owner, and target milestone.

## Current limitations

| ID | Severity | Status | Description | Workaround | Target |
|---|---|---|---|---|---|
| KI-001 | S1 | Open | No native executable or playable build exists yet. | Use the original game or Xenia; MCLA-R is not runnable. | M3–M5 |
| KI-002 | S1 | Closed | ReXGlue feasibility for this exact XEX was unmeasured; M2 now has deterministic diagnostic-free codegen and a `GO WITH SDK FORK` decision. | See `docs/evidence/M2-feasibility-report.md`. | M2 |
| KI-003 | S2 | Open | Intro/Bink, XMA audio, Xenos rendering, and offline XONLINE behavior are unverified. | None in MCLA-R yet. | M4–M7 |
| KI-004 | S2 | Open | MCLA-R currently requires its pinned ReXGlue v0.9.0.1 fork for valid FLOAT16_4 mask-3 codegen diagnostics. | The submodule pin is automatic; do not replace it with upstream v0.9.0 or unpinned `main`. | SDK/upstream |

Do not remove an issue without linking verification evidence. Update this document when a new recurring defect is discovered, severity changes, a workaround changes, or a release claim is added.
