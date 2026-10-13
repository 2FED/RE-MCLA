# Exact update observations before the pending frontier

The offline reader links 91 captured title-screen sources to 91 completed Update
identities and their timing observations. It excludes the unfinished journal
suffix without completing or rewriting it. This is diagnostic lineage, not
evidence of simulation work, display timing or correct 60 FPS behavior.

## Contract

The original handoff and closed-prefix readers run first. Their complete
verdicts and inventories remain in the new result. The new reader classifies a
pending coordinator only on the known route, with the selected owner and object
epoch, a unique preceding Update identity, matching thread identity and the
immediately following tracker serial. Both journals must positively close that
Update before the coordinator enters. A later phase completion cannot repair a
pending handoff record.

The exclusive frontier is the earliest pending outer-phase or handoff entry.
Every accepted chain must finish before it, including phase scopes, semaphore
operations, dispatch, callback, emission and source consumption. Pending scope
IDs, waits and coordinator-owned operations cannot donate evidence. Other
observer failures, malformed records, unknown routes and incomplete timing rows
still reject or block the analysis.

The timing adapter uses exact Update IDs, never nearest clocks or a global last
value. Its input is opaque register and timer-memory observations. Router entry
arguments do not show whether a virtual consumer ran or what work it performed.
All native, causal, simulation and FPS proof flags remain false.

## Actual saved evidence

Source: `private/evidence/M8-001/game-update-fault-20261013-1`.
Derivative: `private/evidence/M8-001/game-update-frontier-20261013-1`.
Replay recipe: `private/replay-update-frontier.py`.

The source collection has 91 distinct images and source IDs, 3120 completed
timing observations in 240 Update identities, and no timing read, binding,
overflow or observer failures. Independent collection review verifies 1188 input
pins, all images, six sidecars, seven loaded modules and normal owned-process
closure. The scene is a daylit city title screen with PRESS START, not gameplay.

At the handoff cut, render 718 and coordinator 720 remain pending. Their outer
phases finish later. The old reader correctly returns zero candidates; the new
derivative retains that exact result and selects only earlier completed chains.
Its frontier is QPC `10428196180286`. All 91 selected Update groups are healthy;
149 other Update groups remain unselected context. There are no candidate
blockers, but 151 retained context issues keep the overall status inconclusive.

| Artifact | SHA256 |
| --- | --- |
| Original supervisor result | B684276D57B5D03BA4283C1CC700D1BB300BA75206C7F11A2C3F65E7C0797CF3 |
| Original native receipt | 39BC982260002E96C33E76CAEC2C908AC4C2F08C4051D2CEEE8D67B2A223F9C1 |
| New offline analysis | 31B8C4F02F305D44092DF1583E8787BDA7D12BA3E8FE39B8B70B64A9816E932A |

The derivative verifies all eight original metadata hashes before and after
analysis. Raw semaphore bytes are retained by hash, not semantically redecoded.
No game was launched for this derivative; original evidence, l12 and MAIN were
not changed. The native run is consumed and must not be repeated.

## Tests and review

The five focused suites pass 134 tests: original handoff 34, closed-prefix 26,
new frontier 31, original timing 29 and timing-frontier 14. Independent source
reviews accept both new adapters for offline diagnosis. Regressions cover
pending phase boundaries earlier than handoff entry, reused coordinator credit,
wrong owner/thread/epoch, malformed suffixes, read failures, unknown routes and
pending observations that later phase completion must not heal.

Run with the bundled Python from `scripts`:

```text
python -E -S -B -m unittest test_mcla_update_timing_frontier test_mcla_frame_handoff_closed_frontier test_mcla_update_timing_decode test_mcla_frame_handoff_closed_prefix test_mcla_frame_handoff_decode
```

## Next boundary

Observe the router's actual predicate and consumer call sites in a separate,
default-off private build. Record the already-loaded object, vtable, target and
raw delta per invocation, including false predicates, exceptions and pending
calls. A completed dispatch will establish only that the target returned, not
that its internal simulation work was complete. M8-001 remains open; no runtime
promotion or milestone push follows from this offline prerequisite.
