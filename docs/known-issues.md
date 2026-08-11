# Known issues

Owner: MCLA-R maintainers

Purpose: maintain user-visible and developer-relevant limitations with severity, affected builds, workaround, owner, and target milestone.

## Current limitations

| ID | Severity | Status | Description | Workaround | Target |
|---|---|---|---|---|---|
| KI-001 | S1 | Open | Canonical native startup now reaches verified graphics/audio work, but frontend usability and playability are not yet validated in MCLA-R. | Use the original game or Xenia; do not treat the M3 startup gate as a playable release. | M4–M5 |
| KI-002 | S1 | Closed | ReXGlue feasibility for this exact XEX was unmeasured; M2 now has deterministic diagnostic-free codegen and a `GO WITH SDK FORK` decision. | See `docs/evidence/M2-feasibility-report.md`. | M2 |
| KI-003 | S2 | Open | Intro playback, frontend/gameplay rendering and audio correctness, and deeper post-boundary offline-service behavior remain unverified; M3 proves only startup subsystem activity. | None in MCLA-R yet. | M4–M7 |
| KI-004 | S2 | Open | MCLA-R currently requires its pinned ReXGlue v0.9.0.7 fork for valid FLOAT16_4 mask-3 diagnostics, Unicode-safe Windows paths, safe input/window teardown, fail-closed read-only VFS behavior, deterministic direct offline-service results, Xenia-compatible guest-thread startup ordering, and privacy-safe guest crash context. | The submodule pin is automatic; do not replace it with upstream v0.9.0 or unpinned `main`. | SDK/upstream |
| KI-005 | S1 | Closed | ReXApp destroyed the Window before Runtime/input drivers, causing a teardown use-after-free after XEX load. | Fixed in fork v0.9.0.3; see `docs/evidence/M3-003-module-config.md`. | SDK/upstream #336 |
| KI-006 | S1 | Closed | Read-only HostPathDevice write opens were downgraded, while rename and writable mappings could bypass the device policy. | Fixed fail-closed in fork v0.9.0.4; see `docs/evidence/M3-004-vfs-disc-root.md`. | SDK/upstream pending approval |
| KI-007 | S2 | Open | Generic ReXGlue `REX_EXPORT_STUB` preserves caller `r3` for return-bearing exports. MCLA-R's ten direct offline-service imports are fixed, but the broader SDK inventory remains unclassified. | Keep the exact v0.9.0.7 fork pin; do not add a return-bearing generic stub. | SDK/upstream #407 |
| KI-008 | S2 | Open | ReXGlue guest threads omitted Xenia's 10-ms compatibility grace period and could race creator-side shared-state initialization. | Fixed since fork v0.9.0.6 and retained in v0.9.0.7; keep the exact pin until upstream resolves the report. | SDK/upstream #408 |
| KI-009 | S2 | Open | Upstream ReXGlue lets generated C++ exceptions escape guest `XThread` execution without structured guest PC/function/thread/import context. | Fixed and regression-tested in fork v0.9.0.7 for C++ exceptions; hardware SEH/signals and fatal aborts remain separate work. | SDK/upstream #409 |
| KI-010 | S2 | Open | The first crash probe immediately after a large relink has intermittently written its report and shutdown marker but failed to signal the Windows process handle within 20–60 seconds; immediate subsequent runs exit 0. | Treat warm runs as M3-009 report evidence only; reproduce and classify cold/repeated exit behavior before M3 closure. | M3-015 |
| KI-011 | S1 | Closed | Normal launches now select the staged Xenos plugin by project default; the verified route initializes graphics interrupts/pipelines and audio without a no-GPU marker or post-launch Bink blocker. | Fixed and bounded in `docs/evidence/M3-013-startup-traps.md`; guest-free probes intentionally remain GPU-independent. | M3-013 |

Do not remove an issue without linking verification evidence. Update this document when a new recurring defect is discovered, severity changes, a workaround changes, or a release claim is added.
