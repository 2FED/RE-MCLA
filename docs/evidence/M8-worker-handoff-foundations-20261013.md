# Worker handoff collection foundations

Two dependent foundations connect the already tested semaphore journal to a
future exact wait/render inventory: a default-off Win32 callback collector and
a platform-independent worker-iteration state machine. This prerequisite does
not promote the separate, unfinished PPC wrappers or claim update causality.

## Collector contract

An explicit external registry and process-lifetime collector are required.
The selected creation is annotated at caller823062D8, initial0/max32767; a
separate returned-handle lookup must bind its actual object cookie. Auxiliary
creation823060B8 is excluded. All selected-object operations remain recorded,
including those with unknown helper context. Other objects are not invented as
selected journal rows. Duplicate selection, object reuse and integrity errors
fail closed. Typed coverage defaults unknown and remains an external claim.

Helper eligibility counts every SDK admission, including nested or other-object
operations. Multiple admissions invalidate its annotation. Invalidating an
already recorded update-release label sets a sticky integrity fault instead of
rewriting raw history. Completion must come from the admitting native thread.

Freeze takes the collector mutex, reads the actual QPC cut, stops the journal,
disables transport admissions and copies the snapshot before releasing the
lock. Late End calls increment only a separate counter. Frozen annotations,
counters and exact cut accompany the immutable raw/prefix snapshot. No observer
lock spans original native work; no SDK/object-table reentry occurs in callbacks.

## Worker iteration contract

The bounded, allocation-free core accepts explicit thread lifetime, worker,
lane, owner and selected-object identities. A validated successful SDK wait
belongs only to its current helper iteration. Next wait entry, no-op dispatch,
worker exit and freeze explicitly retire unused eligibility. Actual render
consumes the slot once, even if its phase ID is zero. It cannot borrow a prior
iteration or infer a release/update identity. Helper brackets are not fabricated
SDK event timestamps; raw operation timing stays in the journal.

## Verification and limits

Clang20 C++23 Release /W4 /WX: collector OFF4/ON106 and worker165 checks pass.
Independent review found two collector defects, reproduced before correction:
ambiguous release metadata RED99/3 then GREEN99/0; wrong-thread completion
RED106/1 then GREEN106/0. Failed binaries, logs and pre-fix headers remain in
private/handoff-foundations-release-red-20261013 and
private/handoff-foundations-thread-red-20261013. Final results are under
private/handoff-foundations-thread-green-20261013.

The wrapper must additionally verify that a null-handle fast return caused zero
SDK admissions, preserve unsupported outcomes, and establish real hook coverage.
Tests use synthetic clocks/callback metadata, not game execution. Actual queued
callback/freeze races and PPC wrapper behavior require separate integration tests.
No simulation, display, FPS, native admission or M8-001 completion follows.

| File | SHA256 |
| --- | --- |
| Collector | 4B5D7B940A5C6B20AB375B64015D2A576DB0149ABE124BBDC0D462483A2BA0FF |
| Collector tests | B817737D16CBCAD0FC2B6900A4DF2A0D3398D6C5FF9103050BC150CE9C60BBAA |
| Worker core | 94106DCA191FFC1D1499C37634DF2DAF8C558444E3F867C9CAD3C6A2FC1D27B2 |
| Worker tests | 9447B1A43CE07787A06304CEFB7CC9E24B58F31B2F50229D69F0604F779D37F6 |
