# Fixture content identity prerequisite

October 12, 2026. The new offline fixture model distinguishes each synthetic
publication by its session and monotonic IDs. It passes 19 isolated unit tests
and independent review. It is a prerequisite for M8-001, not a native capture
gate, WGC timestamp correction, compositor-time proof or 60 FPS acceptance.
M8 remains YELLOW with 4 complete, 6 deferred and 19 total tasks.

## Why a separate model

The existing colored fixture repeats its palette every eight phases:
`block_layout(1) == block_layout(9)`. That pattern cannot identify a unique
publication. The earlier WGC burst temporal rejection remains a NO-GO;
matching colors must not override a failed timing check.

The new module `scripts/mcla_fixture_identity_v2.py` is independent of that
fixture and its Windows adapters. Importing or testing it performs no capture,
window creation, process launch, disk write, network call or clock read.
`NATIVE_ADMITTED` remains false. No existing gate or native helper changed.

## Pixel and lifecycle contract

A full 1280 by 720 BGRA grid encodes exactly 40 bytes: four magic bytes, a
nonzero 128-bit session nonce, unsigned 64-bit challenge and paint IDs, and a
CRC32 corruption check. The 320 cells use two fixed opaque colors. Decoding
checks the complete canonical pixel buffer, including cell interiors and alpha;
a valid sampled marker cannot approve mixed-frame pixels elsewhere. CRC is not
authentication and this is not a security boundary against hostile callers.

One sequential challenge reserves one pair of IDs, permits one completed
publication and consumes only observation index 1. IDs never wrap or retry;
the session has a 1024-publication bound. Rejection through the model's methods
poisons the run, preventing a later favorable frame from replacing a failure.
Producer draw and cleanup outcomes must both be explicitly successful.

Caller-supplied host brackets must be ordered. A producer may begin drawing
inside the host issue call, so the model does not invent an
`issue_after <= draw_before` constraint. Observation starts after both host
issue completion and producer cleanup. Raw WGC timestamps remain opaque signed
metadata, including repeated or apparently future labels.

Successful results claim only `fixture_content_model_pass`. Native admission,
compositor time, guest timing and FPS proof flags are all false. The model cannot
attest nonce freshness, measured records, window ownership or the caller's
assertion that observation 1 really was the first eligible capture.

## Reproducible tests and review

Run with Python 3.10 or newer from the repository root:

```text
python -B -m unittest discover -s scripts -p test_mcla_fixture_identity_v2.py
```

Primary and independent review run: 19 tests PASS. This host used the bundled
Python runtime because the system `python` is a Windows Store alias. The final
suite depends only on this module and the standard library, not on untracked
v1 sources. Earlier 18- and 20-test runs included the historical v1 collision
test; removing that external dependency gives the final 19-test suite.

Coverage includes unique IDs beyond eight paints, zero/type/overflow rejection,
all 320 cell interiors, final alpha, truncation, magic and CRC corruption,
alternating scanlines from two valid frames, stale/wrong session or IDs, failed
draw/cleanup, missing/duplicate publications, overlapping/reordered challenges,
early/later-frame rejection, terminal retries, successful second publication,
repeated opaque WGC labels, malformed method arguments and budget exhaustion.

Frozen local v1 regression suites also passed: capture fixture 25, burst
schedule model 8 and cycle extractor 26. Those results preserve existing checks;
they do not turn the prior physical temporal failure into success.

## Native boundary still pending

A separate reviewed adapter must bind a fresh nonce and owned fixture window,
measure actual producer records, and consume the first eligible observation
without content-dependent skipping. Exceptions constructing records before
calling the model must also terminate the whole run. The pure model's poisoned
state cannot catch exceptions raised outside its methods.

Native visibility, cleanup, exact pixel and independent visual/integrity gates
remain mandatory before any physical admission. A future one-challenge test
must not enable the consumed v1 entry points or weaken their timestamp checks.
Even a successful content test cannot establish compositor cadence or game
simulation correctness. Stock gameplay and owner l12 are unchanged.
