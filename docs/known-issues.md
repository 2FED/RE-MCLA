# Known issues

Owner: MCLA-R maintainers

Purpose: maintain user-visible and developer-relevant limitations with severity, affected builds, workaround, owner, and target milestone.

## Current limitations

| ID | Severity | Status | Description | Workaround | Target |
|---|---|---|---|---|---|
| KI-001 | S1 | Open | A native executable now builds, but startup and playability are not validated. | Use the original game or Xenia; do not treat the M3-001 link result as a runnable release. | M3–M5 |
| KI-002 | S1 | Closed | ReXGlue feasibility for this exact XEX was unmeasured; M2 now has deterministic diagnostic-free codegen and a `GO WITH SDK FORK` decision. | See `docs/evidence/M2-feasibility-report.md`. | M2 |
| KI-003 | S2 | Open | Intro/Bink, XMA audio, Xenos rendering, and post-boundary offline-service reachability are unverified. | None in MCLA-R yet. | M4–M7 |
| KI-004 | S2 | Open | MCLA-R currently requires its pinned ReXGlue v0.9.0.6 fork for valid FLOAT16_4 mask-3 diagnostics, Unicode-safe Windows paths, safe input/window teardown, fail-closed read-only VFS behavior, deterministic direct offline-service results, and Xenia-compatible guest-thread startup ordering. | The submodule pin is automatic; do not replace it with upstream v0.9.0 or unpinned `main`. | SDK/upstream |
| KI-005 | S1 | Closed | ReXApp destroyed the Window before Runtime/input drivers, causing a teardown use-after-free after XEX load. | Fixed in fork v0.9.0.3; see `docs/evidence/M3-003-module-config.md`. | SDK/upstream #336 |
| KI-006 | S1 | Closed | Read-only HostPathDevice write opens were downgraded, while rename and writable mappings could bypass the device policy. | Fixed fail-closed in fork v0.9.0.4; see `docs/evidence/M3-004-vfs-disc-root.md`. | SDK/upstream pending approval |
| KI-007 | S2 | Open | Generic ReXGlue `REX_EXPORT_STUB` preserves caller `r3` for return-bearing exports. MCLA-R's ten direct offline-service imports are fixed, but the broader SDK inventory remains unclassified. | Keep the exact v0.9.0.6 fork pin; do not add a return-bearing generic stub. | SDK/upstream #407 |
| KI-008 | S2 | Open | ReXGlue guest threads omitted Xenia's 10-ms compatibility grace period and could race creator-side shared-state initialization. | Fixed and regression-tested in fork v0.9.0.6; retain the exact pin until upstream resolves the report. | SDK/upstream #408 |

Do not remove an issue without linking verification evidence. Update this document when a new recurring defect is discovered, severity changes, a workaround changes, or a release claim is added.
