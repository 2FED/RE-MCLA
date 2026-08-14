# M5-010 streaming, I/O, and allocator failure report

Status: `PASS — bounded five-process route contains no fatal allocator, I/O,
or streaming event`

Decision: `canonical-route-streaming-io-allocator-bounded-pass`

## Evidence set

M5-010 reuses three immutable accepted routes instead of launching another
behaviorally identical process:

- M5-002 `20260814-093131-ddca5b9d`, result SHA-256
  `A582A41BB254D9187BC6F8147E89E2FF2E92D0EA4B79945760BA6FBCF334BC28`;
- M5-008 `20260814-150733-da37caed`, result SHA-256
  `C705E09FF88CA33D705E11326B34B7E643604D0C40FC210414B6431D3A86CAA9`;
- M5-009 `20260814-170657-f44949d7`, result SHA-256
  `4E3D514386501D92B43CD4F2C4C89ECD8BA000ACF23D8B168CFA431C8F67C62F`.

Together these are five isolated physical processes: one noisy world-streaming
trace, three optimized stock-speed gameplay cycles, and one longer
RelWithDebInfo audio/gameplay route. All five exited 0 through external
`WM_CLOSE`; none required force cleanup.

The sanitized aggregate is
`private/evidence/M5-010/20260814-173720-4ac5744f/result.json`, SHA-256
`2D36EA7C39438B1D54DC8AF5C39AE905D53AB4DB08CD0669DAF1BC61AECE149F`.
Raw logs, frames, saves, caches, and host paths remain private.

## Observed I/O and allocation results

The M5-002 noisy trace contains:

- 8,055 `NtReadFile` results: 8,054 synchronous successes and one legitimate
  asynchronous `0x103` pending result whose IOSB status is success;
- zero read failures;
- 15 successful `NtCreateFile` results;
- exactly seven allowlisted retail development-path misses per process, or 35
  across the five accepted processes; these are the known railyard and
  exposition-park `.loc` probes, not required archive data;
- 87/87 successful `NtAllocateVirtualMemory` results;
- 49/49 successful `NtFreeVirtualMemory` results;
- 2,590/2,590 non-null `MmAllocatePhysicalMemoryEx` results.

The archive workload includes 8,011 cache-RPF reads totaling 262,821,908 bytes
and 41 audlo-RPF reads totaling 3,290,825 bytes. The prior M5-002 gate also
binds their near-end coverage and mixed-case path resolution. Across all five
complete log sets the M5-010 scanner found zero fatal allocator, I/O, archive,
streaming, guest-crash, unregistered-function, assertion, or device-loss
events.

## Decision and scope

No target failure was reproduced, so no behavior patch is justified. The task
closes as a bounded no-defect report, not as a claim that allocation or file
access can never fail. Long-session memory/resource leak checks remain
M5-013; repeated complete races remain M5-012/M5-014; all-region streaming
remains M6-001 and M7-007.

The verifier physically rehashes the exact accepted result files, the complete
M5-002/M5-008/M5-009 evidence trees or their declared physical members, the
current source-game identity through the M5-009 gate, the pinned save, current
ReXGlue/runtime artifacts, and every inspected runtime log. It rejects reparse
points, topology drift, unexpected open/read/allocate status, unknown
development misses, private paths in the aggregate, and scope inflation.

## Commands

```powershell
scripts/test-route-failure-report.ps1
scripts/run-route-failure-report.ps1
scripts/verify-route-failure-report.ps1 `
  -ResultPath private/evidence/M5-010/20260814-173720-4ac5744f/result.json
```

The fixture suite has one positive, 26 fail-closed negatives, and seven source
contract checks.
