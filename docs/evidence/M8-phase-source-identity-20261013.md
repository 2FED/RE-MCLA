# Explicit phase identity transported with captured game sources

The next M8-001 prerequisite connects a captured source to the exact host phase
callback that observed its swap. It does not connect that render operation to a
simulation update, establish display timing, or admit a 60 FPS experiment.
The preceding [phase census](M8-game-phase-census-20261013.md) observed a worker
render route but deliberately supplied no source-to-phase join.

## Identity contract

Snapshot ABI2 appends an opaque unsigned64 callback ID. Packet version1 and its
eleven words stay unchanged: the SDK ledger transports the snapshot with its
unique swap token. The source mailbox retains that binding with the acquired
resource. Callback order can differ from token issuance order, so neither a
latest global ID nor nearest timestamp is a valid substitute.

The title phase observer assigns consecutive IDs under its journal mutex only
after admitting a row. Zero means no recorded phase callback. A failed timer
read clears the entire snapshot, including its ID; the phase row can remain
unmatched. A suppressed scope stays explicitly suppressed rather than acquiring
an ancestor. Runtime serializers use phase-census-v2 and swap-observer-v2.

The offline join first validates both complete journals and the source receipt.
It requires unique consecutive phase IDs, rejects unknown positive IDs and
conflicting token associations, and only then selects the row by exact ID.
Thread, caller and QPC brackets are consistency checks, not lookup heuristics.
Missing bindings, zero IDs and unhealthy journals remain explicit limitations.

## Engineering checks

- Core ledger:224 checks; native SDK adapter OFF10 and ON238 checks. Tests include
  nested callbacks, two threads issuing in reverse callback order, shutdown after
  ID assignment and whole-snapshot clearing.
- Python join24, phase census21, swap decoder20, synthetic supervisor12 and game
  supervisor27 pass,104 combined. Independent review caught an impossible healthy
  journal with nonconsecutive IDs; the new negative test rejects it.
- Fresh SDK build in private/sdk-phaseid-s/b/o completes. Metadata135,
  provenance30 and packet100 checks pass. PPC1462 cases/5753 assertions pass.
- Every discovered SDK unit case passes using the existing source-isolated
  runner:401 cases,3364 assertions,41 source suites and4 known BitStream skips.
  Receipt: private/evidence/M7-002/sdk-unit-isolated-1eac220eeead471280a972ac46b88d4e.

The initial monolithic unit invocation failed Memory::Initialize. The old SDK
fails identically. A minimal pair reproduces the cause: the heap diagnostic test
retains a process-lifetime Memory, then the primitive cache test tries to install
a second singleton MMIO handler. Each isolated source suite passes. The existing
scripts/test-sdk-unit-isolated.ps1 is the intended full-coverage runner; no test
was removed and no runtime singleton guard was weakened.

Fresh SDK staging initially omitted vendored Git metadata and CMake rejected
CLI11. Copying actual metadata from the immutable parent resolved configuration;
no dependency fetch or fake repository sentinel was used. Parent and metadata
copy receipts remain in private/sdk-phaseid-parent-receipt.json and
private/sdk-phaseid-git-metadata-receipt.json.

## Native preparation and synthetic acceptance

The fresh166-file host snapshot built successfully in host-phaseid-s/b/o.
Its tests pass3222 phase checks,103 actual phase/swap glue checks,129 swap and973
source-scout checks. All three public entry symbols resolve to the phase observer
in the actual linker map, with distinct originals. Static routes pass19 checks
across66 generated files. Independent review verifies all166 snapshot files.

One synthetic GPU attempt is accepted by primary and independent audits:
private/evidence/M8-001/phaseid-source-binding-20261013-1. All45 pins matched
before disarm. Nine captured rows retain seven unique token/source identities;
IDs are1001 three times, then1003,1004,1012,1020,1028,1036. The native fixture
checks the complete synthetic pixel oracle; no pixels were persisted for
independent replay. PID4500 self-exited0 and its creation-owned Job drained,
without forced cleanup, errors or pending ownership. GateFalse; never rerun.

Result SHA256:1AA4C5136FF2022D2F7FFD3DAB58D77B5965E09F881004526B3D59D36EBB3D19.
TSV SHA256:5D7118C5756CCC209ADD920C546E8E1C43A9B48445B7F61D53CDB4083B683FF4.
This accepts synthetic ID transport, not actual game phase association.

## Actual game result

Fresh gphaseid1-bin host9DEAE22C885B70D67F1E63907D1E80E7ED9A993D18E51021C94B20343FC407C0
passes missing, invalid and expired ticket rejection using asset-free copies.
One passive run completed in
private/evidence/M8-001/game-phaseid-scout-20261013-1. Primary post-run116 pins
match, and all91 images and seven decoded receipt components match canonical
JSON replay. Source sequences1015 through1191 contain91 distinct exact-ID joins
to render scopes, with no missing, suppressed, unavailable or mismatched rows.

The phase journal contains584 completed scopes:195 update,194 coordinator and195
render, plus192 callbacks. Render uses worker thread29156/epoch1 and caller
82305F80; update/coordinator use thread38004/epoch2. Different counts are retained,
not forced into a one-to-one update mapping. SDK statistics192 issued/190 matched,
zero rejects/callback failures are a non-quiescent snapshot, not complete delivery
proof. The observer reports no overflow, invalid clock, exception or incomplete
drain.

PID17752/birth134363754611981853 closed through normal WM_CLOSE acknowledgement,
exit0 and Job0, without force, errors or pending ownership. Seven loaded package
modules and the original six-file historical seed verify. Slots00/45/90 show
downtown river/bridges and traffic at PRESS START, not active gameplay. Primary
and independent visual reviews accept those scene descriptions, not a controlled
performance comparison. Independent review also replays all91 images and all
seven decoded components, verifies116 pins before disarm and confirms91 distinct
completed render scopes. Accepted scope is captured resource to explicit callback
to observed render-function scope, not update or simulation causality.

| Artifact | SHA256 |
| --- | --- |
| Supervisor result | CE2425842D1E4F4CF55BFC3B9BC87E30A46C0D8B5FB19E272048B9BF1961A775 |
| Source receipt | B6A99CB666444E115D8C8E078CB869A4600068BC6D5190890B4B6612291681E2 |
| Swap journal | 6F811C5ACBA122895CD70B2BC29E3778FB156BD3FF0304403AD5004A0882901F |
| Phase journal | EB05870AF538844DB740A8B265A7F2C9EA09577A363D711B8530085FF9CA9860 |

Both supervisors are now disarmed and their attempts consumed. First preflight
omitted admission receipts because the copy targeted their parent folder; the
corrected second manifest includes all10 receipts, byte-verified from the actual
evidence directory. The first direct Python object-equality replay assertion
failed on in-memory versus JSON container representation; canonical JSON replay
matches every component. Neither issue caused another game launch or altered
the preserved captures.

Normal l12, MAIN and previously accepted evidence are unchanged. This work does
not close M8-001 or change milestone counts. Worker semaphore handoff remains the
next causal boundary. Its observer must start before semaphore creation and keep
complete object history. Stopping admissions during a pending wait cannot later
be relabeled complete just because that wait drains. Unsupported operations,
straddling cuts and ambiguous credits must remain inconclusive.
