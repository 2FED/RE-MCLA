# M8 lifetime diagnostic history capacity repair

Owner5 reached parked gameplay, but its exact frame-to-work analysis was blocked
by a full semaphore journal: 4,890 observed operations, 4,096 retained and 794
lost. The worker then lacked the exact SDK operation for helper 2,049. This is
a diagnostic storage failure, not evidence of a gameplay, idle-camera or HUD bug.
The [owner5 return](M8-owner5-gameplay-return-20261015.md) remains inconclusive.

## Bounded repair

Version 0.20.28.0 raises native semaphore history to 32,768 operations. Companion
ticket, annotation and snapshot arrays inherit that capacity. The private native
handoff adapter raises lifetime worker events from 16,384 to 65,536; its late
phase-scope capacity remains 2,048 and object epochs remain 64.

The design envelope is 180 seconds at 60 iterations per second, with two semaphore
operations and five worker events per iteration: 21,600 operations and 54,000
events. This is finite headroom, not a guarantee for arbitrary rates. Overflow
still records loss, withholds missing tickets and blocks clean associations.
Nothing resets history at READY, rewrites timestamps or uses nearest-time joins.

The Python readers accept exactly integer capacities 4,096 and 32,768. Each
document's declared capacity bounds its rows and paired annotations. Prefix
projection preserves that declaration. Semaphore JSON is bounded at 32 MiB;
handoff JSON at 48 MiB, including the independent owner-ready reader. Other
schema, identity, coverage and proof checks are unchanged.

## Regression evidence

`private/test-semaphore-capacity.ps1 -OutputName semaphore-capacity-red-20261015-1`
failed before the fix with `FAIL operation 4097 has no retained ticket; capacity=4096`.
The repaired original path retains operation 4,097 and its completion.

Native results are retained in separate fresh directories:

- `semaphore-capacity-full-20261015-3`: all 32,768 maximum-width records serialize;
  prefix 11,950,137 bytes, raw journal 12,704,074 bytes. The earlier `full-1`
  fixture supplied an impossible release count; its refusal is preserved, and
  only the test data was corrected. `full-2` also passed.
- `history-core-tests-20261015-1`: observer core 191 and prefix snapshot 83 checks.
- `history-foundations-20261015-1`: collector OFF 4, ON 106 and worker core 165 checks.
- `history-handoff-tests-20261015-1`: 16,506 wrapper checks, including 4,097 actual
  mock helper/SDK/dispatch iterations, crossing both old bounds with exact IDs.
  Event 65,537 records a sticky failure and one loss without overwriting history.

The full handoff serializer retained 65,536 maximum-width events, 2,048 scopes
and 32,768 annotations in 37,025,566 bytes. These injected rows test serialization,
not gameplay semantics. Retained/frozen structure sizes are State 10,158,584,
Frozen 1,048,664, Journal 3,412,616 and Snapshot 3,414,168 bytes: about 17.2 MiB
total, excluding temporary serialization strings and parsed JSON.

`private/check-history-readers.py` checks native/Python limit agreement, reads
the native 4,097-timeout complete prefix successfully with no associations, and
replays the actual owner5 derivative. That derivative is unchanged: zero
candidates and the same eight blockers. No old capture or result was overwritten.

Primary Python regressions pass: history limits 10, semaphore suites 83, handoff
suites 91 and owner-ready 13. Owner6 supervisor adds 50 passing offline tests.
AST scan is clean; all three AST rule tests pass. Independent read-only reviews
found no blocker in the capacity contracts, native stress test or overlay build.

## Native artifact and remaining gate

`private/host-history-overlay-20261015-1` reuses 82 of 83 linked objects and replaces
only the handoff module. Recompilation against 520 pinned dependencies and two
tools matches that object's bytes except the four-byte COFF timestamp. System
library resolution is not hermetic; this is not a reproducible whole-host claim.

Host SHA256 `1650F15B0DCC053813E08D33F2DFA3ABE43432E0EE94D49DA4F962CED3890D23`,
53,299,712 bytes. Unchanged crash helper SHA256
`293D4AC754A83B32252540E3E3A464F321C20758121F96192A0CA5199EAEC343`, 207,360 bytes.
The host inherits native version 0.20.21.0; repository version is not binary identity.

This commit closes the tested capacity/reader prerequisite only. The private
native integration and owner6 delivery remain separately reviewed experiment
work. A fresh isolated owner6 collection, integrity review and all-frame scene
review are still required. M8-001 stays open; no physical timing or 60 FPS claim.
Normal l12 and MAIN are not modified by this repair.
