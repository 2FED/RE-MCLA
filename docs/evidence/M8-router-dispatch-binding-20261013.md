# Router dispatch observations within exact Update identities

M8-001 now has a default-off observer at the actual predicate and consumer
dispatch sites inside the guest router. It records explicit Update identity,
the requested target, raw arguments, branch outcome and entry/return clocks.
These observations do not establish what the consumer does internally or make
60 FPS safe. The owner build l12 and MAIN remain outside the experiment.

## Observation contract

The private transform changes only four unique sites in the frozen generated
router body. It adds a lexical observer, wraps each original indirect-call
expression in a void lambda, and records the false-predicate branch. The
original dispatch macro executes once even when observation is disabled,
frozen or faulty. Original exceptions propagate. Observer work preserves
LastError and MXCSR and never modifies the PPC context.

Each admitted invocation has a non-reused thread-lifetime/serial identity and
the exact active Update ID. The predicate records its loaded object, table and
CTR; only returned r3 low8 determines the branch. The consumer independently
records its reloaded object, table, CTR and raw f1 bits. A router entry or shared
return label is not treated as consumer execution. Requested CTR is not proof
of target resolution, object lifetime, state coherence or simulation work.

Storage is bounded to eight thread lifetimes, eight nested invocations and
16384 events per thread. Loss, binding errors, unknown routes, pending calls,
exceptions and clock faults remain visible. Freeze stops admission and retains
the first immutable cut without completing pending calls. Its mutex-based cut
is cooperative, not a hard real-time deadline. Binding is observed before the
event clock; it is not an independently held phase lease.

The reader validates the full journal and explicit phase references before
evaluating candidates. All current phase diagnostic issues block candidates.
A failed cut retains its raw timestamp and valid earlier history while yielding
no candidates; it cannot supply an ordering bound. Event clocks still require
positive signed range, per-lane monotonicity and exact phase containment.

## Verified synthetic tests

| Check | Result | Private evidence |
| --- | --- | --- |
| Scalar model | 188 checks, zero failures | router-record-20261013-2 |
| Native adapter and sentinel | 168 checks, zero failures | router-adapter-unit-20261013-3 |
| Actual generated body plus native adapter | 7455 checks, zero failures | router-adapter-differential-20261013-2 |
| Deliberate entry-register mutation | 600 failures in 7455 checks | same directory, mutant.txt |
| Actual native JSON to Python | 13 cases pass | router-serializer-20261013-3 |
| Router supervisor, decoder and old fault supervisor | 82 tests pass | isolated Python unittest |

The six new implementation/test files exported from the index, together with
their indexed dependencies in `private/router-index-20261013-1`, pass355
Python tests (165 decoder and190 join/frontier/audit tests) and the scalar
C++ suite188/0. Structural scan and all three rule tests pass. Native adapter,
scout, CMake wiring and one-shot supervisor remain a separately reviewed private
integration; the bounded commit contains the scalar core, pure readers/tests,
this evidence, VERSION and one README checkpoint only.

The generated differential uses the real dispatch macro with synthetic owned
memory and a mock fallback resolver. It compares callee-entry and final PPC
context, 64 KiB memory, dispatch table, LastError/MXCSR, exception propagation
and exact-once call counts. Cases include object reselection, predicate false,
nested calls, fallback/null slots, resolver failure, changed FP state and
disabled, unbound, failed or frozen observers. It is not an actual-game result.

Native serializer cases include normal and skipped calls, exceptions, nested
Updates, freeze during dispatch, unbound or changed bindings, event clock
failure, capacity overflow and failed cut after valid history. A regression
first reproduced rejection of zero/backward failed cuts; the corrected reader
retains those failures without accepting their candidates.

## Private build identity

`private/host-router-overlay-20261013-2` contains the diagnostic host:
`295143591F6F078B6FC5677C6A4CC4A0614930AD551EEDBBC5FB7AA4D64099E2`.
Its paired IPCv3 helper remains
`293D4AC754A83B32252540E3E3A464F321C20758121F96192A0CA5199EAEC343`.

The link uses 78 unchanged objects, two replacements and one new object. Both
replacement targets occur exactly once in the original response file. The map
places the transformed guest body and required version sentinel in generated13;
the existing outer timing wrapper remains separate. The frozen compilation
defines still report version 0.20.21.0; this is not a rebuilt owner release.

Three recompiles with 574 dependency pins match except COFF timestamp bytes4–7.
Resolved Windows system libraries and process environment are not hermetically
pinned. The first build attempt is retained: its PowerShell function used the
automatic `$args` variable and passed no compiler inputs. A minimal check gave
zero arguments for that function and two after renaming the parameter. The
successful build is a separate output, not an overwrite of that failure.

Missing, malformed and expired tickets for the exact new host all reject before
title/profile/cache creation. Full canonical game and named RTX5060 verification
pass without launching the title.

## Native preparation

The separately reviewed one-shot supervisor uses `private/grouter1-bin` and
`private/evidence/M8-001/game-router-dispatch-20261013-1`. Its token is
`95ac7621f0bd4e63b82d19c705ea3f46`. The final manifest pins1227 inputs, including
the archived armed supervisor, build closure and unchanged historical six-file
seed. It supplies no game input and requires exact owned-process/Job cleanup
before reading images. Seven sidecars are bounded and read first;
raw semaphore metadata remains digest-retained rather than semantically proved.

An initial read-only preflight rejected changing filesystem identity metadata
on newly built files. Subsequent hash/stat checks and the complete preflight
passed with the same strict guards. No native attempt was consumed by those
read-only failures.

## Collected native observations

The one-shot run collected91 distinct images and closed normally with exit0,
no forced cleanup and zero remaining Job processes. Its owned process was
PID26296, birth134363955579393412. All1227 input pins matched at completion.
The gate is consumed and disabled; the archived armed supervisor preserves the
exact executable input. Independent replay verifies all1227 pins (substituting
only that archived supervisor),91 BMPs,seven sidecars and seven loaded package
modules, and reproduces the complete saved analysis. Selected l12 launcher/host
and MAIN campaign/name hashes match their prior recorded baselines; this is not
a new full-tree before/after snapshot of the owner's save collection.

The router journal contains1617 completed invocations in231 exact Update
identities, on one admitted thread lifetime. It has no pending router calls,
unbound entries, unknown routes, dropped records, binding/clock errors or
original exceptions. These are normal-return observations of requested targets:

| Selector | Requested consumer | Calls | Raw f1 interpretation |
| --- | --- | --- | --- |
| 1 | 0x822C7870 | 693 | 462 near1/60 and231 near1/30 |
| 4 | 0x822CB350 | 231 | near1/30 |
| 0 | 0x822D3E78 | 231 | near1/30 |
| 8 | 0x822D3030 | 231 | near1/30 |
| 2 | 0x822D3C20 | 231 | near1/30 |

The exact f1 bit patterns are0x3F91111120000000 and0x3FA1111120000000;
these are opaque observed arguments, not a measured frame rate. One inspected
middle image shows the daylit city title screen with a start prompt, not active
gameplay. The original timing analysis still reports zero candidates because
of a pending handoff scope. Separate closed-frontier analysis selects91 source
chains and91 Updates before QPC10461992753190 while retaining142 context issues.
The original result remains unchanged.

## Exact source to router grouping

The new pure reader validates both full raw input chains before joining them.
It selects router groups only by the same Update phase ID, thread-lifetime epoch
and native thread ID as a closed source chain. Every matched invocation must be
complete, phase-contained and strictly before the frontier; one bad invocation
excludes the entire source group rather than filtering that invocation away.
Multiple sources may share an Update. Timer observation IDs are not assigned
to individual router invocations. Normalized groups retain all router inventory,
including groups that no accepted source references.

Twelve new synthetic tests cover identity/epoch/thread mismatches, equal clocks
with different valid IDs, duplicates, loss/pending failures, malformed unrelated
metadata before evaluation, equality/late frontier edges and input immutability.

The separate derivative `private/evidence/M8-001/game-frame-router-20261013-1`
links91 source candidates to91 exact Updates and637 completed router invocations
(seven per selected Update in this run). It preserves both original analyses,
142 context issues and all false proof flags. There are no candidate blockers
or excluded selected groups; overall status remains diagnostic inconclusive.
Its analysis SHA256 is
`DF23258BBB46883C8523965EF1791A594144F3DF3BE23BE2243ED4884A488AAA`.

| Original input | SHA256 |
| --- | --- |
| Final manifest | 9CAE5C4A8DC8E659CADB549D18C6614343E95D149D6B948B58FEB04D05240DA2 |
| Supervisor result | AF1A37968642D72B05E1C059EE6DB737264B0399036EFE140D9280A17625F096 |
| Native source receipt | 4F69275F6C2E5CFB6BAD195349AA3890CE6629B74C837671F2EEC9869AEFC1B2 |
| Archived armed supervisor | 4399B75714716CFFC9560F0D24BAF64BBB74CAF1ADF8FCB66F047E67BD18E256 |

## Next timing question

Static inspection of the requested targets shows why router arguments alone are
insufficient. Target822C7870 saves incoming f1 and branches on timer+76. Its
nonzero branch explicitly forwards that saved value to three calls; the other
branch does not explicitly restore it. Target822D3030 can replace delta from
timer+100 and adjust it again using an object float32 countdown. Other targets
can read timer globals or pass through values without explicit local f1 use.
Do not sum the two near1/60 and one near1/30 router arguments as elapsed simulation.

The next bounded witness is actual822C7870 entry linked to the active router
consumer, its already-loaded branch operand and exact child-call arguments.
Consumer internals, vehicle physics and gameplay timing remain open. M8-001
does not close and the owner build is not promoted.
