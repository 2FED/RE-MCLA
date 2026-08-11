# M3-015 repeated launch and exit cycles

Date: 2026-08-11

Status: accepted

## Scope

M3-015 proves that a clean RelWithDebInfo build survives ten synthetic
crash-report probes followed by ten consecutive normal startup and controlled
window-close cycles. It binds every invocation to isolated ignored user/cache
roots, checks prior-cycle immutability, verifies the complete private game
manifest before and after, and rejects any surviving exact-path process.

The gate does not claim frontend usability, guest-save compatibility, gameplay
shutdown, or long-duration resource stability. Those remain M4+ work.

## Automated contract

`scripts/run-launch-exit-cycles.ps1` performs the following exact sequence:

1. verify all 15 supported game files and hashes;
2. perform a canonical RelWithDebInfo `--clean-first` build;
3. snapshot `mcla.exe`, `rexruntimerd.dll`, `TracyClientrd.dll`, and
   `rexgpu-xenosrd.dll`;
4. run ten guest-free crash probes, with probe 1 immediately after relink;
5. run ten consecutive canonical startups and close each owned window with
   `WM_CLOSE`;
6. verify all prior evidence trees after every later invocation;
7. re-verify the four binaries and the complete private game manifest;
8. emit and verify a sanitized aggregate with no absolute/private paths.

Every normal cycle requires all 15 M3-014 startup markers, exactly one
`Window closing, shutting down...` marker followed by exactly one
`Title terminated; hard-exiting process.` marker, exit code 0, and no force
cleanup. Every crash probe requires the bounded schema-1 report, ordered report
and shutdown markers, exit code 0, and no force cleanup. A development cycle
count can exercise the harness but is permanently ineligible for acceptance.

The fixture suite passes one complete physical evidence tree and rejects 23
mutations,
including wrong counts/order, missing clean build, game or binary drift,
nonzero exit, missing or duplicate close markers, force cleanup, crash-probe
timeout/no signal, prior-tree mutation, a surviving process, private-path
leakage, an invalid cycle index, mutated/deleted/extra evidence, and reparse
redirection in both the verifier tree and runner build-root preflight.
It also rejects an in-range forged host-stack count that does not match the
physical crash report.

## KI-010 diagnosis and containment

The first final attempt reproduced KI-010 on crash probe 4. A separate
thread-only CDB reproduction hung on probe 2. In both cases the report and
`MCLA lifecycle: shutdown` marker were already complete while the process
remained alive.

The captured UI-thread stack localized the defect to ReXGlue teardown:

```text
BaseHeap::Release
XThread::FreeStack
XThread::Terminate
AudioSystem::Shutdown
Runtime::Shutdown
Runtime::~Runtime
ReXApp::OnDestroy
```

ReXGlue commit `b99f472` in upstream PR #250 force-terminates the Audio Worker
to break a guest-callback shutdown deadlock. If termination occurs while the
worker owns the guest heap mutex, normal C++ unwinding cannot release it and
the UI thread blocks when `FreeStack()` reacquires the poisoned lock. This is
an evidence-backed diagnosis from repeated behavior and thread stacks; no
memory dump or guest memory was captured.

The synthetic probe already closes its report and deliberately never launches
guest entry-point code. Its UI callback now emits the normal project shutdown
marker, flushes logging, and uses a process hard exit, matching the containment
already used by ReXGlue's normal `WM_CLOSE` path. This avoids unsafe Runtime
teardown only for the bounded diagnostic probe; it is not claimed as a general
SDK teardown fix.

Open and closed upstream issues and pull requests were searched for
`AudioSystem Shutdown`, `shutdown deadlock`, `teardown hang`, `exit hang`,
`XThread Terminate`, and `BaseHeap Release`. PRs #152 and #250 are related but
do not report this force-termination heap-lock poisoning. A sanitized issue
draft is retained privately; publishing it requires explicit external-
disclosure approval from the execution environment.

## Accepted live run

Private run: `20260811-165807-b340ca91`.

| Check | Result |
|---|---|
| clean build | 51,618 ms, exit 0 |
| crash probes | 10/10, 741-6,038 ms, exit 0 |
| normal startups | 10/10, 2,824-7,825 ms |
| controlled exits | 10/10, 154-361 ms, exit 0 |
| force cleanup | 0/20 |
| surviving exact-path processes | 0 |
| supported game identity | 15 files, 6,569,586,392 bytes, 15/15 hashes |
| prior-cycle tree immutability | confirmed across all 20 invocations |
| executable SHA-256 | `1F06BC1EBEB79CA9CCA0E89C9320286484990FB42C4D9D09C7E393C7357FB0AD` |
| game/binary identity after run | unchanged |

The full result verifier reports `Passed=True`, `CleanBuildVerified=True`,
`ProcessCleanupVerified=True`, and `DataIntegrityVerified=True`.

Independent review found two pre-commit P1 gaps: the first verifier trusted
self-reported JSON instead of re-hashing sibling evidence, and lexical path
containment did not reject a junction in an existing ancestor before
`--clean-first` or evidence writes. The accepted run is after both fixes. The
verifier now requires exact 10+10 sibling topology, re-hashes all logs/reports
and user/cache trees, reruns the lower-level crash/startup verifiers, and
rejects missing/extra/reparse evidence. Runner preflight walks every existing
build/game/evidence/input path segment and recursively rejects reparse content
before the first build or write. Each crash log also ends with one exact
`MCLA crash probe: controlled hard exit` marker, making the KI-010 containment
runtime-verifiable.

Two harness-only defects were found during development before acceptance: an
empty `ArrayList` was rejected at the first immutability call, and
`Measure-Object` could not read an ordered-dictionary `length` key. The final
runner explicitly allows the empty initial ledger and sums byte counts through
key access; the one-cycle development rehearsal and final 10+10 run both pass.

## Re-evaluation

M3-015 is accepted. Repeated process startup and controlled close are bounded,
no exact-path process survives, approved source data and runtime artifacts are
unchanged, and KI-010 is localized and contained on the diagnostic path. All
M3 implementation tasks are complete; milestone closure still requires the
final cross-task review, release version/documentation update, commit, and
push gate.
