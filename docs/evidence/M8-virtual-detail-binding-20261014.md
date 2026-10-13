# Virtual dispatch observations linked to game frames

M8-001 now follows the selected consumer child into the actual generated
`8258D1A0` body and records its three virtual dispatch requests. A passive stock
title run links 91 source frames to 91 Update identities and 182 complete body
invocations by exact IDs. This is frame and call bookkeeping on the title scene,
not vehicle-physics validation or admission of 60 FPS.

## Observer contract

Four insertions in frozen TU41 add one lexical scope and wrap three existing
indirect-call expressions. Reversing them and the include/version sentinel
restores the entire original translation unit. Observers read existing register
values, never additional guest memory. The original calls execute once outside
observer locks and preservation guards, including disabled and failed paths.

The parent accessor accepts only the exact active consumer child 0. It returns
saved parent identity and operands, not mutable current registers, an ancestor
lookup or a lifetime lease. Every virtual boundary rechecks the complete parent
Entry and Child, native thread and lexical top. Rejected nested scopes shadow
outer scopes. Incoming F1 and each dispatched F1 remain separate opaque values:
the original body restores F1 before calls 0 and 2, but not call 1.

The subordinate journal shares the consumer's preallocated lanes, mutex, clock,
admission window and immutable cut. It cannot freeze later or backfill returns.
It has 16384 events per lane, depth 8, eight lane lifetimes and 256 rejected-entry
headers. Serialization stops with an error above 32 MiB. Consumer-v1 JSON remains
unchanged; virtual records use their own sidecar and invocation namespace.

The decoder replays all transitions and unfinished stacks before selecting
records. The join requires the entire shared lane inventory, exactly equal cuts,
full parent operands and one complete body for every applicable selected child.
Every event must fit its parent call and the closed source frontier. Missing
bodies exclude the whole source group; duplicates, orphans or contradictions
block acceptance. Other targets and zero branches stay explicit context, and
no nearest-time association or F1 sum is used.

## Verification

| Check | Result | Private evidence |
| --- | --- | --- |
| Scalar model | 314 checks, zero failures | virtual-record-20261014-1 |
| Active-child accessor | 417 checks, zero failures | consumer-child-access-unit-20261014-1 |
| Consumer regression with virtual macro OFF | 417 checks, zero failures | consumer-virtual-off-regression-20261014-1 |
| Actual parent body differential | 8400 checks, zero failures | consumer-child-access-differential-20261014-1 |
| Native virtual adapter | 142 checks, zero failures | virtual-adapter-unit-20261014-1 |
| Actual TU41 differential, O2 and O3 | 10260 checks, zero failures each | virtual-adapter-differential-20261014-1 and 2 |
| Every-access and constructor-only register mutants | 890 and 480 failures respectively | corresponding mutant outputs |
| Actual native JSON serialization | 18 cases pass | virtual-serializer-20261014-1 |
| Python decoder, join and supervisor | 22, 13 and 36 tests pass | pinned Python unittest |

The differential compares full PPC bytes at immediate body entry, mocked callee
entry and final return/exception, plus synthetic memory, dispatch slots,
LastError/MXCSR and original/resolver call counts. It uses actual generated code
and the real indirect macro with synthetic callees, not a gameplay simulation.
Mutation controls are broad preservation checks, not isolated proof of every
new accessor instruction.

Independent reviews accepted the scalar/native adapter, decoder/join, generated
transform, scout integration and supervisor. Supervisor review found a required
file path for a nonexistent virtual implementation file; removing that path and
its test expectation preserved the actual implementation pin in consumer.cpp.
The complete 71-test suite passed after correction.

The exact staged implementation/test export in `private/virtual-index-20261014-1`
passes 314 C++ checks and 378 Python tests. Repository ast-grep scan and all
three rule tests pass; independent final review accepts the nine-file scope.

## Private build and admission

`private/host-virtual-overlay-20261014-1` replaces three of 82 objects and reuses
79 from the pinned consumer overlay. Three object recompiles with 578 pinned
dependencies match except COFF timestamps. Independent review verifies 958 build
input pins, the exact reverse transform and strong target/version symbols.
System-library resolution remains nonhermetic. Frozen build version definitions
remain 0.20.21.0; this diagnostic is not a normal l12 replacement.

| Artifact | SHA256 |
| --- | --- |
| Host | 9DC05B0677349455F867BC320AB86DB21D58D36ECF3A09322C8A668CE359A01A |
| Unchanged paired crash helper | 293D4AC754A83B32252540E3E3A464F321C20758121F96192A0CA5199EAEC343 |
| Final input manifest | F11AE11E5F38085655408C95F259F22D50DA5B4D4C2030EF67027738E615048B |
| Archived armed supervisor | 65ACF42214E8E3A236574C9F0745DE7D7BC730D1AB61921F5FC19EC3043EA1CF |

The fresh package `private/gvirtual1-bin` has ten files and a host-bound ticket;
the canonical game has 15 pinned files. Six files from the authorized historical
seed were copied into a separate run root. Three expiry negatives reject missing,
malformed and expired tickets without creating title/profile/cache directories.
The consumed expiry runner has a latent unbounded wait after its timeout kill;
that path was not exercised and must be corrected before any future reuse.

An initial read-only preflight rejected a relative manifest path. The corrected
absolute-path check then rejected a file attribute change during hashing. A
diagnostic read found seven attribute transitions from 262176 to 32 and only the
intentional preparation-script pin change; no other content drift. The cause of
the attribute transition is unproven. No guard was relaxed. Fresh preflight 2
passed 1234 pins; final admission passed 1236 pins, including signing/preflight
scripts, archived armed source and the before-run selected MAIN/l12 baseline.

## Native observations

Consumed attempt: `private/evidence/M8-001/game-virtual-detail-20261014-1`.
Token `ffda0dd4a2c9468988d14f610350b9ff`, PID 46008, birth 134364012709929019.
It closed with acknowledged WM_CLOSE, exit 0 and zero active Job processes;
there was no forced cleanup, ownership remainder or cleanup error. All input
pins remained unchanged. The six current MAIN user files, managed-profile file
and two l12 executables match their actual before-run hashes afterward.

Slots 00, 45 and 90 show the sunny pedestrian shopping-street title scene with
the game logo and start prompt. They are not driving evidence. The virtual
journal contains 474 completed normal invocations, zero pending/rejected/lost
records, zero clock/binding/observer errors and zero original exceptions.
Its entry partition is 1288 before-window plus 474 admitted entries.

| Call | Return LR | Requested target | Count | F1 bits |
| --- | --- | --- | --- | --- |
| Virtual slot +12 | 8258D1E8 | 82741010 | 474 | 3F91111120000000 |
| Virtual slot +16 | 8258D1FC | 8258E4D0 | 474 | 3F91111120000000 |
| Virtual slot +20 | 8258D214 | 82741810 | 474 | 3F91111120000000 |

All three receive r3 BDF9D620 in this run. Requested targets are not observations
inside those three callee bodies; their semantic role remains a separate check.

The immutable offline derivative
`private/evidence/M8-001/game-frame-virtual-20261014-1` retains original analyses
and links 91 sources, 91 Updates, 637 router invocations, 273 consumer invocations
and 182 virtual-body invocations. It excludes no source candidates and has no
candidate blockers. Its 149 retained context issues keep the overall diagnostic
inconclusive and every simulation/FPS proof false. It does not rewrite the
original reader's pending-handoff disposition.

| Result | SHA256 |
| --- | --- |
| Supervisor result | BF5AA86805E8153891726CB24CD4E608C41009DAD416D2D2BC70E8E6B05A7F7A |
| Native source receipt | CD12289EAF4B5BF4D3959497EFDD3F3541C54550F7A2706BC096CC0096734E38 |
| Offline joined analysis | FE306161F904E8BAF435E792AA42631BEE6A081944B5DDC6E46718D2C1D25504 |

The root supervisor is disabled. Never rerun this consumed attempt. Native
adapter/scout/CMake/supervisor changes remain separate private integration;
the bounded task commit covers the scalar model, pure readers and tests.
M8-001 remains open, with no stock timing change, 60 FPS admission or product
promotion. The next boundary is the requested callees' argument use and a
controlled gameplay observation rather than treating title evidence as driving.

Independent collection review reproduced all saved analyses and verified all
1236 pins, 91 images and unique source identities, nine sidecars, seven package
modules and the before/after/live save baseline. The shared consumer/virtual cut
is QPC 10519117747542. Independent derivative replay matches the complete joined
analysis, 11 raw-input pins and 17 decoder/join pins plus its driver, including
all retained context and false proof flags.

## Next work emission boundary

Bounded frozen-source review finds forwarding rather than a local scalar
integrator. `82741010` tail-calls `82588490`, which saves incoming F1 and can
forward it as scalar F1 or splatted vector V1 through gated/container work.
`8258E4D0` coordinates container mutation and cleanup calls with no local F1
arithmetic. `82741810` calls `8258A300` and then `827412D8`; the former similarly
forwards saved F1, while the latter computes vector distances from loaded data
and constants. No incoming-F1 dependence of those distances is established.

Source references are frozen TU57:57023 and 58263, TU41:13159 and 3071–3752,
TU40:49140–49536 and TU57:57452–58261. A minimal next autonomous step is a bounded
gate/work-emission witness inside `82588490`, retaining its exact active virtual
call identity. Static reachability must not be substituted for observed work,
and loops over containers must not be called time subdivisions.
