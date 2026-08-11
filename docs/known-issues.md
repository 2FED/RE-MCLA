# Known issues

Owner: MCLA-R maintainers

Purpose: maintain user-visible and developer-relevant limitations with severity, affected builds, workaround, owner, and target milestone.

## Current limitations

| ID | Severity | Status | Description | Workaround | Target |
|---|---|---|---|---|---|
| KI-001 | S1 | Open | Canonical native startup now reaches verified guest-backed D3D12 presentation and a recognizable private game-derived frame, but frontend usability and playability are not yet validated in MCLA-R. | Use the original game or Xenia; do not treat the M4-001 frame gate as a playable release. | M4–M5 |
| KI-002 | S1 | Closed | ReXGlue feasibility for this exact XEX was unmeasured; M2 now has deterministic diagnostic-free codegen and a `GO WITH SDK FORK` decision. | See `docs/evidence/M2-feasibility-report.md`. | M2 |
| KI-003 | S2 | Open | Intro playback, frontend/gameplay rendering correctness, audio correctness, and deeper post-boundary offline-service behavior remain unverified; M4-001 proves presentation/readback activity but not visual parity. | None in MCLA-R yet. | M4–M7 |
| KI-004 | S2 | Open | MCLA-R currently requires its pinned ReXGlue v0.9.0.9 fork for valid FLOAT16_4 mask-3 diagnostics, Unicode-safe Windows paths, safe input/window teardown, fail-closed read-only VFS behavior, deterministic direct offline-service results, Xenia-compatible guest-thread startup ordering, privacy-safe guest crash context, and guest-backed D3D12 presentation telemetry. | The submodule pin is automatic; do not replace it with upstream v0.9.0 or unpinned `main`. | SDK/upstream |
| KI-005 | S1 | Closed | ReXApp destroyed the Window before Runtime/input drivers, causing a teardown use-after-free after XEX load. | Fixed in fork v0.9.0.3; see `docs/evidence/M3-003-module-config.md`. | SDK/upstream #336 |
| KI-006 | S1 | Closed | Read-only HostPathDevice write opens were downgraded, while rename and writable mappings could bypass the device policy. | Fixed fail-closed in fork v0.9.0.4; see `docs/evidence/M3-004-vfs-disc-root.md`. | SDK/upstream pending approval |
| KI-007 | S2 | Open | Generic ReXGlue `REX_EXPORT_STUB` preserves caller `r3` for return-bearing exports. MCLA-R's ten direct offline-service imports are fixed, but the broader SDK inventory remains unclassified. | Keep the exact v0.9.0.9 fork pin; do not add a return-bearing generic stub. | SDK/upstream #407 |
| KI-008 | S2 | Open | ReXGlue guest threads omitted Xenia's 10-ms compatibility grace period and could race creator-side shared-state initialization. | Fixed since fork v0.9.0.6 and retained in v0.9.0.9; keep the exact pin until upstream resolves the report. | SDK/upstream #408 |
| KI-009 | S2 | Open | Upstream ReXGlue lets generated C++ exceptions escape guest `XThread` execution without structured guest PC/function/thread/import context. | Fixed and regression-tested in fork v0.9.0.7 for C++ exceptions; hardware SEH/signals and fatal aborts remain separate work. | SDK/upstream #409 |
| KI-010 | S2 | Contained | Repeated probes reproduced the post-report hang on both cold and warm runs. Thread-only CDB localized it to ReXGlue force-terminating the Audio Worker and then blocking in `XThread::FreeStack -> BaseHeap::Release` on a poisoned guest-heap mutex. | The guest-free synthetic probe closes its report, emits shutdown, flushes, and hard-exits; the final 10/10 probes and independent 10/10 normal `WM_CLOSE` cycles exit 0 with no cleanup/orphans. A sanitized upstream issue draft is pending explicit publication approval. | SDK/upstream |
| KI-011 | S1 | Closed | Normal launches now select the staged Xenos plugin by project default; the verified route initializes graphics interrupts/pipelines and audio without a no-GPU marker or post-launch Bink blocker. | Fixed and bounded in `docs/evidence/M3-013-startup-traps.md`; guest-free probes intentionally remain GPU-independent. | M3-013 |
| KI-012 | S2 | Open | Non-Release ReXGlue host tests that call a profiled `REX_HOOK` before `Runtime::Setup` can dereference Tracy's null manual-lifetime profiler and raise `0xC0000005` at address `0x2C8`. The M4 UI subset is clean 12/12 and normal game startup initializes Tracy before hook dispatch; this is an SDK instrumentation/test-lifetime defect, not an observed game-runtime failure. | Use scoped SDK tests or Release until instrumentation safely no-ops before profiler startup; see upstream issue #410. | SDK/upstream #410 |

Do not remove an issue without linking verification evidence. Update this document when a new recurring defect is discovered, severity changes, a workaround changes, or a release claim is added.
