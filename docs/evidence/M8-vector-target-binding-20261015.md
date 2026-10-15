# Actual vector target journal and exact parent association

October 15, 2026. The bounded journal and offline join extend M8-001 from a
requested indirect target to observations inside the actual `8259F300` body.
Native synthetic tests pass. A new gameplay collection is required before
claiming that this association occurred in the game; M8-001 remains open.

## Why another observation is needed

[Owner6](M8-owner6-gameplay-return-20261015.md) supplies 91 parked-world source
records linked to 182 unique selected work bodies and 728 vector call/return
pairs. A requested CTR target is not evidence that its body executed. The new
observer saves the actual body entry and its exact active parent VectorPre,
including the lane-global event index, full ancestry, LR, R3 and V1 raw lanes.
No nearest-time search, floating-point sum or inferred work subdivision is used.

## Contract

The portable state machine observes the initial V126 value, seven explicit
call sites with normal/exceptional returns, the flag store at object+657,
the slot100 gate, and the load/store pair at object+664. Its counter branch
uses the original signed low32 comparison and preserves wraparound. Registers
are copied only after original loads; the observer adds no guest-memory reads.

Each of eight thread lifetimes is bounded to 4096 entries, 65536 events and
depth eight. Rejected entries have 256 metadata slots; JSON is capped at128MiB.
Faults and capacity loss remain visible and prevent candidate acceptance.
The target journal freezes before its parent work journal. An independently
later parent completion cannot complete a target group across that cut.

The strict decoder replays the native grammar before the exact-ID join uses
any record. The join also validates the complete existing source-to-work chain,
rejects orphan/duplicate/contradictory target bodies even when unselected, and
requires one body for every applicable selected VectorPre. Unknown other
targets remain unknown. Every simulation, FPS, total-work and physical proof
flag remains false.

## Native and differential checks

- Portable model:586 checks; active parent accessor:174 checks.
- Native adapter:218 checks; real work-parent adapter integration:83 checks.
- Original work-body differential regression:16947 checks. Three deliberate
  mutations fail1788,798 and399 checks respectively.
- Actual frozen target-body differential:15180 checks across normal and seven
  exceptional call paths, unusual raw vectors, counter/gate cases, disabled,
  healthy, faulty and mid-call-frozen observers. Full PPCContext, mapped memory,
  callees/order, LastError and MXCSR agree. A deliberate entry mutation fails
  672 checks rather than escaping the oracle.
- Native serializer:24 immutable fixtures cover normal, exceptional, pending,
  rejected, clock-failed and empty cases; strict-reader replay passes.
- Strict decoder17 and full-chain join15 tests pass. Independent review found
  an impossible first-entry identity fault in an empty lane; its regression
  went red before the decoder rejected that contradiction. Legitimate later
  duplicate-parent faults remain preserved as inconclusive evidence.
- Transformer:13 unique seams reconstruct the exact original function; original
  guest-memory expression count remains28. Three pure transform tests pass.

Private runs are under `private/vector-target-core-20261015-1`,
`vector-target-native-20261015-1`, `vector-target-parent-20261015-1`,
`vector-target-differential-20261015-3` and `vector-target-serializer-20261015-1`.
The differential's first two preparations remain preserved. One failed before
compilation on a self-copy path guard. The second exposed an invalid synthetic
FPSCR cache: zero initialized CSR unmasked floating exceptions when the original
function enabled flush mode. Initializing the fixture cache to its actual MXCSR
fixed the test; it was not a game bug or a product fix.

## Build identity

Private `host-vector-overlay-20261015-1` links84 objects:80 unchanged, three
replaced and one new. All four compiled objects reproduce except the COFF
timestamp, with585 dependencies and two tool binaries pinned. This is not a
hermetic claim about system library resolution.

Host:53388288bytes, SHA256
`3C9856FE23A748D50742DB2A9C06E6284A562B23F332F33259C7716987C9ED1A`.
Unchanged helper:207360bytes, SHA256
`293D4AC754A83B32252540E3E3A464F321C20758121F96192A0CA5199EAEC343`.
Native version remains the inherited0.20.21.0; repository version is not the
runtime identity. Stock timing and VSync remain unchanged.

Native integration, exact generated-code transformation and private differential
inputs remain outside this portable prerequisite's commit. No generated guest
body, game assets, save data or binary is published. The gameplay package and
owner acceptance are separate gates.
