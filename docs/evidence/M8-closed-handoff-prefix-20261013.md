# Closed frame-handoff candidates from the immutable stock capture

Status: offline candidate inventory accepted; timing and simulation remain
unproved. M8-001 stays open, with no 60 FPS admission or normal-build change.

## Contract

The new `mcla_frame_handoff_closed_prefix.py` first runs the unchanged original
decoder over every metadata inventory. It retains that entire analysis, all
source/phase/swap/handoff/semaphore rows and input hashes. It does not reinterpret
the raw semaphore companion, choose edges by timestamps, or edit native results.

Only two contextual conditions can cease to block earlier candidate chains:

- A tracked pending worker-render scope with its exact successful wait and
  eligible render-consumption identity establishes a strict suffix barrier at
  its entry. Every selected scope, operation, worker stage, dispatcher completion,
  emission bracket and packet-consumption witness must end before that barrier.
- An unscoped callback can remain context only when it is nonsuppressed and
  absent from every captured source's callback ID. Captured or suppressed
  unscoped callbacks remain global blockers.

All other health, coverage, loss, unsupported-operation and pending-semaphore
issues remain global blockers. A later phase completion cannot fill a pending
handoff row. Pending phase/wait IDs cannot be reused. Dispatcher completion is
selected by exact worker-lifetime and dispatch identities, never nearest time.
The original no-candidate outcome is retained as an outcome, not erased.

Context forces `FRAME_HANDOFF_CLOSED_PREFIX_PARTIAL_INCONCLUSIVE`. All proof
flags remain false, including closed-prefix, coverage, causality, simulation and
FPS. These are sufficient diagnostic candidates, not a full execution replay or
proof that all game state consumed by a render belongs to one update.

## Existing evidence, new offline derivative

Input is the single consumed `game-handoff-scout-20261013-1` session described in
[native preparation and collection](M8-frame-handoff-native-preparation-20261013.md).
No game was relaunched. Seven original journal/receipt pins remain unchanged.

The separate derivative is under
`private/evidence/M8-001/game-handoff-closed-prefix-20261013-1`:

| Artifact | SHA256 |
| --- | --- |
| analysis.json | B4B61EBDFF6E289EBFA2D417BCBAB5676CAC02C002CE00FC41B50AF04679816F |
| receipt.json | E5E80C1FD49CE5B0028755051DC3D653DF53F99DB8AA8C35ACB7B6DF3B2B0E53 |
| New decoder | 50B59D7FBBAFBAE3DCACB33B40573D1D9352DC0B1E08B80D711972110B8AC40C |
| New tests | 7BC4B319FC9ACA283DF71BA1F09CA9AE72FF14ECA433B50A5589F61AF8933BA7 |

All 91 captured sources yield completed explicit-ID candidate chains, with zero
candidate blockers or exclusions. Barrier QPC is 10349496739845, the entry of
pending render720. The latest checked chain witness is 10349495472520, strictly
earlier by1267325 ticks. The pending row remains incomplete, with exit0; its
later phase-journal completion is not donated to the handoff inventory.
Uncaptured nonsuppressed callback1 remains present.

The embedded original analysis exactly matches the saved supervisor result,
including its zero candidates and all three original issues. Primary replay
and independent read-only typed-tree, hash and identity assertions agree.
The independent review cross-checks all91 source/callback/render/operation/
update identities and exact dispatch completions without running the decoder.
The underlying pictures remain PRESS START traffic, not owner-driven gameplay.

## Verification and next gate

The new26 synthetic methods plus original34 pass60 tests. The combined decoder,
supervisor, semaphore, phase, swap and source regression set passes252 tests;
printed FAILED fixture outcomes are intentional mocked supervisor negatives.
AST scan and all3 rule tests pass. Source and actual-derivative independent
reviews are GO within the bounded candidate-only contract.

Tests cover touching/crossing barriers, emission/consume bounds, pending-ID reuse,
multiple pending workers, missing/duplicate/mismatched dispatch completion,
captured/suppressed unscoped callbacks, later-completion backfill, other faults,
malformed tails before evaluation and detached input containers.

Next is an exact active-update binding for timer/subdivision/router observations.
The caller temporarily divides/restores timer state, so outer timer snapshots or
equal frame/update counts do not establish the actual consumed delta. Actual
downstream consumers, coherent state, physics and correct stock/60 simulation
remain separate gates. l12, MAIN and all consumed native attempts stay intact.
