# Semaphore prefixes before a live collection cut

PREFIX-v2 can identify unique credits in a completed part of a supplied history
even when a later wait is still pending. It does not reinterpret the old v1
audit or establish native completeness. The intended collection boundary is an
atomic freeze while the game is alive, before termination wakes and `_Exit`.

## Frozen admission inventory

The strict reader accepts at most 2 MiB, 64 epochs, 4096 retained operations and
4096 loss intervals. Stored operation IDs plus loss intervals must partition
every admitted ID exactly once. Counts distinguish actual completions, pending
operations and losses. Admission clocks must be ordered consistently with IDs.
The range check uses intervals, not enumeration of potentially huge missing IDs.

Every row, including the excluded suffix, is schema-validated before candidate
evaluation. A pending operation has a null exit and null outcome; a failed clock
can remain null but prevents any candidate. Unknown coverage, observer failure,
nonfresh lifetimes and initial credits also prevent candidates. Here epoch
`history_complete` means admission coverage through freeze, not drained or fully
supported operations. It cannot be obtained by clearing the fixed-window core's
failure flags; the native adapter must establish this separate property.

## Conservative prefix rule

The earliest pending, unsupported or lost operation's reliable admission lower
bound, limited by the actual freeze time, becomes a global barrier. The caller
cannot supply a convenient salvage cutoff. Each epoch is analyzed from creation
as inclusive, transitively connected operation intervals. A component touching
or crossing the barrier is excluded whole. An unconsumed tail rolls back to the
last completed zero-credit boundary.

The unchanged v1 audit checks each eligible derived projection. Projections use
observed exclusive boundaries, original operation IDs and brackets; they are
explicitly not original native receipts. A later evaluated contradiction clears
all candidates. After an unsupported component, suffix ledger semantics remain
unmodeled rather than declared consistent. The original digest, barriers, loss
inventory and excluded operation IDs stay in the result.

An operation that began strictly after a completed wait cannot explain that
wait's consumed credit. Conversely, an earlier-started pending wait can compete
and excludes the affected prefix. No FIFO, nearest timestamp, global latest
update or synthetic completion is used.

## Verification

39 new standard-library tests pass together with the unchanged 44 v1 tests and
24 source/phase join tests: 107 total. Tests cover pending and unsupported
operations, shutdown batches, aliases/reuse, zero-credit rollback, transitive
touching, counter contradictions, malformed suffixes and complete ID partitions.
A trap verifies malformed final metadata never reaches the candidate evaluator.

An independent finite oracle enumerates all 24 possible native-effect orders
for each of 256 bracket assignments containing two one-credit releases and two
successful waits. Every emitted association must agree with every legal order;
the matrix includes accepted cases. This is finite synthetic coverage, not a
proof for arbitrary native execution. Removing the unknown-coverage guard in
memory makes its regression fail; the original module remains unchanged.

Independent source review accepts the bounded offline contract. Actual locked
admission, object selection, original-call outcomes and complete SDK coverage
remain separate integration gates. Native, simulation, render, frame mapping,
prefix-history and FPS proof flags remain false.

| Source | SHA256 |
| --- | --- |
| scripts/mcla_semaphore_prefix_audit.py | 79C1B4439C98657E2911736D9C79DD20BF1577B13A22DDADD379228E27F5DE9D |
| scripts/test_mcla_semaphore_prefix_audit.py | D03341C247F8BC101AFB82159B83CE4E40DA797725DFA0E401DEEE8806C001B3 |

No game run, owner interaction, save mutation, SDK installation or l12 change
belongs to this offline prerequisite. M8-001 remains open.
