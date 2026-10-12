# Synthetic frame sequence and checkout contract

October 12, 2026. Two offline validators extend the accepted single-frame
content prerequisite: a bounded 91-publication sequence and a three-publication
checkout journal. Their 37 tests pass. Neither runs native capture nor admits
cadence, game timing or 60 FPS. M8-001 remains open; M8 remains YELLOW with
4 complete, 6 deferred and 19 total tasks.

## Sequence consistency

`scripts/mcla_content_sequence_v2.py` replays one warmup plus 90 timed samples
using the committed full-grid identity model. Each sample needs the next
session/challenge/paint identity, observation 1, successful producer cleanup
before acknowledgement and nonoverlapping host records. A rejected operation
permanently invalidates that oracle instance. Completion requires all samples
and explicit normal cleanup.

The schedule uses fixed rational 10 Hz slots, not a sliding deadline. For timed
samples, the interval from producer draw begin to host read completion must be
strictly below 200 ms. The conservative upper gap from the previous timed
sample's draw begin to the current read completion must also be below 200 ms.
Warmup, timed and whole-run deadlines are independently bounded.

These bounds describe synthetic content transport, not actual compositor
sampling. Two accepted slots can have observations only 31 ms apart; a positive
test locks that limitation and requires `cadence_admitted=False`. Raw WGC labels
remain opaque, including repeated or extreme signed values. Caller records do
not authenticate native execution, source freshness or first-frame selection.

## Checkout selection before content

`scripts/mcla_content_queue_v2.py` verifies a separate minimal three-publication
contract. One immutable journal declares exactly one session/pool creation,
stable held process birth identity and window, three consecutive publications,
then normal close. Each publication follows arm, synchronous acknowledgement,
bounded polling, first non-null checkout, one readback and release.

Null polls neither establish freshness nor reset a deadline. Draining,
recreating the session, a second checkout, missing release or identity drift
rejects the journal. A stale first frame rejects the result even if a matching
frame would be next. Repeated IDs and unsolicited paints also fail.

All journal structure, sample bindings, producer identities, frequency and
deadline checks run before any pixel decoding. Independent review caught an
earlier ordering gap: later metadata was checked after earlier pixels. The
corrected two-pass validator has decoder-trap tests for invalid final metadata
and whole-run duration. No partial approval escaped the earlier version.

This is post-collection replay, not a live queue implementation. Native
resource declarations still need independent provenance. The future runner
must terminate its complete attempt on any exception; calling a pure function
again with edited evidence cannot rehabilitate a failed native attempt.

## Verification

From the repository root, with Python 3.10 or newer:

```text
python -B -m unittest discover -s scripts -p test_mcla_content_sequence_v2.py
python -B -m unittest discover -s scripts -p test_mcla_content_queue_v2.py
```

Sequence 22 and queue 15 tests PASS. Existing v2 identity 19, producer 16,
controller 24, native admission 4 and reader 37 tests also pass: 137 total.
Frozen v1 fixture 25, schedule 8 and extractor 26 remain green. `ast-grep scan`
passes and its three rule tests pass. The host uses the bundled Python runtime;
no native window, game, capture, save write or network call is part of these tests.

The initial bunched-slot test used equality where the identity model requires
the next arm strictly after the previous read. Correcting the test to a 31 ms
gap preserved that guard. A missing-close test initially expected the later
incomplete-state error but correctly hit the minimum journal length first;
its expected error was corrected without relaxing validation. Final independent
review GO followed explicit unsigned host-tick checks and float-substitution
decoder-trap coverage; integer-equal floats must fail before any pixels.

## Next native boundary

The next adapter must collect only three distinct synthetic publications through
one persistent WGC session, recording actual checkout/release ordinals and
ownership. It must buffer selected frames without inspecting content and expose
them to the parent only after normal helper exit. A stale first checkout fails;
there is no search for a favorable frame. Selection, deadlines, visibility,
geometry and cleanup need native source/build review before any execution.

The accepted owner single-frame attempts 4 and 5 remain consumed. All existing
native admission flags stay false. Do not rerun them or change their evidence.
The prior burst timestamp NO-GO stays in force. Synthetic nonce-backed content
does not establish sample times in an unmodified game; that transfer remains a
separate unresolved prerequisite for stock/interval1/stock timing experiments.
