# Stock timer observations alongside source frames

This M8-001 investigation adds a separate, default-off observer for the stock
timer entry at `0x821BDA90`. It records timer calls in the same host QPC window
as the passive source-frame scout. A timer call is not a simulation step, and
sharing a clock does not establish which timer update produced a particular
image. The accepted l12 runtime and MAIN save remain unchanged.

## Observation contract

The private host replaces one dispatch-table entry before the guest main thread
resumes. Installation requires the exact generated stock function pointer; the
wrapper calls that original exactly once with the same context and memory base.
It does not patch the timer algorithm, synthesize time, drive input, or restore
the dispatch entry while the process is running. Direct generated calls would
bypass this hook; the pinned generated source contains no direct calls to this
entry, but that does not establish coverage of every game timing system.

Each retained invocation records its entry ordinal, timer address, incoming
caller address and native Windows thread ID. These identifiers do not prove an
object lifetime. Before and after snapshots contain only the allowlisted opaque
timer fields:18 words,3 bytes and the qword at offset64. The reader uses the
pinned Windows guest-address translation and `ReadProcessMemory` on this process.
Unreadable or partial copies are unavailable, never a direct-dereference retry.
Successful reads do not establish multi-field coherence.

The outer QPC interval encloses both snapshot attempts and the original work.
It includes instrumentation and preemption, so it is not original-only CPU
cost. Native LastError, context changes, memory effects and original exceptions
are preserved. Instrumentation exceptions cannot skip or retry the original.
No observer lock spans the original function.

The fixed window starts40seconds after the scout readiness observation and ends
9.1seconds later, exclusively. This includes all91 scheduled100ms slots.
Storage holds at most1024 records. Healthy calls wholly outside the window are
counted separately; straddles and diagnostic failures remain visible. Overflow
never overwrites earlier records or blocks original execution. Disarming stops
new observations, not guest work. A three-second bounded drain precedes an
immutable snapshot; a failed drain retains process-lifetime observer storage.

## Verification so far

The standalone journal passes65 checks. The observer core passes49 checks after
independent review, including concurrent reservations, reentrance, original
exceptions, unchanged pending-freeze destinations, overflow and callback errors.
Offline classification preserves callback/unwind flags rather than presenting
an otherwise valid interval as a clean call.

The native self-memory reader passes43 checks: exact endian decoding, address
boundaries, protected pages, unavailable reads, stale-state clearing and
LastError preservation. The SDK-context wrapper passes36 synthetic checks
against the pinned runtime. Its complete PPCContext and memory effects match an
unwrapped baseline, including an original exception and a changed return r3.
The after snapshot still uses the original timer address. Injected drain-clock
tests reject cancellation and late freeze/serialization success.

Private synthetic evidence is in `private/timer-journal-core-20261013-2`,
`private/timer-observer-core-20261013-2`, `private/timer-memory-20261013-1` and
`private/timer-native-20261013-1`. None launches the game or reads a real save.
The first journal compiler command used an unsupported language switch and
failed before compilation; that failed preparation remains preserved.

## Accepted private native observations

The fresh157-file source snapshot under `private/host-timer-s` builds in Release
with Clang20.1.8 against the private provenance SDK. Native wrapper36 and source
scout973 checks pass on that build. The strict timer decoder adds17 tests; final
paired-supervisor/decoder/publication regressions total70 tests. The broader
earlier run including the old supervisor passes92. AST scan and all3 rules pass.
Missing, invalid and expired signed-ticket admission rejects before game setup.
One preflight command supplied a relative metadata path and was rejected before
launch; the corrected absolute-path preflight verifies all93 inputs.

One separately reviewed run collected91 unique images/source IDs945..1109.
Primary and independent audits replay all BMPs and timer JSON, verify93 input
pins before disarming, seven loaded package modules and six unchanged historical
seed files. PID10372/birth134363628185762609 closes normally with WM_CLOSE ACK,
exit0 and an empty Job; no forced cleanup, cleanup error or pending ownership.
The consumed supervisor is disabled and must not be rerun.

Timer counts are708 observed/admitted/completed, comprising543 healthy calls
before the window and165 retained calls inside it. One observed tuple has timer
address`0x827D7500`, caller`0x822C21B4` and native thread28500. There is no retained
overlap, missing state, callback failure or overflow. This is call coverage for
the selected diagnostic window, not165 proved simulation steps.

First/middle/last images show the PRESS START screen above a downtown arched
bridge and traffic at sunset. This differs from the previous beach/pier scout
and cannot support a same-scene performance comparison. The first selected image
was refreshed before the timer window began; wholly-before timer calls have
counts but no retained state. In general, GPU/producer timing does not establish
the simulation update that produced an image. Do not match nearest timestamps
or equal endpoint counts and call that a causal frame-to-step binding.

Run: `private/evidence/M8-001/game-timer-scout-20261013-1`.
Host52793344bytes, SHA256
9B8F88916B222C8E14502E48EEDB08209A03A6EDD652F0EFC437CA8991BE22C4.
Supervisor613788bytes, SHA256
C366253C765EF5890D454148049EB98D1431336607310C42B89BA058856007D4.
Timer receiptSHA256
BD30774E5E6ED4210FBD2EFFCC1E7BBE987AD691D63DADFD5A287D5C4BEBBD3E.

Raw timer JSON, source snapshots and images remain private. The native hook,
source scout and supervisor integration are still uncommitted prerequisites;
the bounded journal/observer/reader/decoder contract can be reviewed separately.

The next analysis follows the observed caller and actual state transitions.
Animation phase, game-time correctness, frame-to-step causality and safe60FPS
remain unproved. M8-001 remains open.
