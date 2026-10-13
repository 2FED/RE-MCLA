# Bounded semaphore observation journal

The offline journal records supplied semaphore lifetimes and operation brackets
without assigning release credits to waits. It is a prerequisite for the game
update handoff investigation, not a native observer or frame timing acceptance.
The original credit audit remains unchanged.

## Contract

Storage is fixed at 64 object epochs and 4096 operations. Aliased guest handles
share the resolved object cookie; a reused cookie needs witnessed destruction
and a strictly later creation. Tickets bind the journal lifetime, epoch and
operation, so completion cannot silently attach to another instance or kind.
A saturated process-lifetime generation allocator never wraps. Its containing
module must remain loaded and the allocator must not be reset.

The adapter must serialize each journal method, not the original native call's
begin-to-end interval. Const spans are views, not independent frozen snapshots.
The journal performs no SDK, clock, file, game-memory or OS calls. Its only
internal atomic operation allocates a journal identity during construction;
disabled construction leaves that allocator untouched.

Pending rows have no observed completion. Overflow and unknown-object attempts
remain explicit and cannot manufacture complete history. An early fixed-window
cut or a cut with pending operations remains incomplete even after later drain.
The v1 serializer shape guard refuses pending or malformed records rather than
inflating completed counts. A compatible shape is not causal acceptance.

## Verification

Clang 20 C++23, Release optimization and `/W4 /WX /EHsc` pass 191 checks. Coverage
includes aliases, pointer reuse, lifetime boundaries, overlapping calls, actual
release/wait outcomes, unsupported operations, invalid enums/counts/clocks,
capacity exhaustion, exact-once completion, cuts and disabled behavior. Compile
time assertions also forbid copying or moving journal instances.

Independent review found that address-only ticket ownership accepted a stale
ticket after same-storage reconstruction. The regression deliberately rebuilds
the journal at the identical address. A private mutant removing only generation
equality fails 2 of 191 checks; corrected code passes all 191. The first 165-check
run predates this fix and is not the final acceptance.

Independent source re-review accepts the generation fix and the added malformed
input tests. Its native integration caveat is to keep a single process-lifetime
issuer, not independent unloadable module instances.

Private test outputs are `private/semaphore-core-red-20261013` and
`private/semaphore-core-green-20261013/gate.log`. Existing credit-audit and phase
join regressions pass 68 Python tests; AST scan and all three rule tests pass.

| Source | SHA256 |
| --- | --- |
| src/mcla_semaphore_observer_core.h | E14E1B311B7EF671556E0706731156525F5C34316306D9916C6084E1EDD7290F |
| tests/mcla_semaphore_observer_core_test.cpp | 9DDF6D88E8D5CC01792B24F4BB8836D720ADA84072CBD7063186901FF187B066 |

## Remaining integration

Runtime object selection, complete SDK access-path coverage, actual QPC bounds,
context preservation and freeze serialization require separate implementation
and review. Normal game closure is not a completeness boundary: a consumed wait
may terminate without returning to its caller, and the app closes with `_Exit`.
A separate prefix-aware audit must preserve this uncertainty. No native game
run, save write, default SDK change or l12 replacement belongs to this result.
All simulation, frame mapping, render and FPS admission flags remain false.
