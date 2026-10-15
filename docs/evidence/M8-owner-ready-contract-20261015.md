# Explicit owner readiness for diagnostic capture

The M8 diagnostic scheduler has a tested, bounded owner-readiness gate.
This prerequisite does not close M8-001 or establish gameplay, simulation timing
or 60 FPS. Game integration and its owner-run package have separate admission.

## Why the gate is needed

Owner3 reached the parked car, but all 91 saved frames covered the preceding
loading transition. The final captured frame preceded automatic closure by
9.456 seconds. The old source-sequence threshold was not a loading-completion
signal. The private collection is retained at
`private/evidence/M8-001/game-work-owner3-20261015-1`.

The new gate waits for an explicit owner assertion before observing source
readiness. The unchanged collector then starts its 91 fixed slots 40 seconds
after that fresh source observation. The assertion does not classify images;
all-frame visual review remains mandatory. No fourth repetition of the old
early-start schedule is requested.

## Contract and resource limits

The supervisor creates two fresh, non-inheritable, initially unsignaled
manual-reset Windows events with role-specific names and a random attempt token.
An existing object is rejected, never reset or adopted. Native code only opens
these objects, checks both are initially empty, and announces that it is waiting.
Only then does the supervisor flush stale console input and accept one exact
`READY` line. Console work is nonblocking and bounded; process attestation,
cancellation and the overall deadline remain active.

Both the signal observation and fresh source observation must precede the
original journal end by at least 60 seconds. The journal retains its existing
180-second horizon and storage capacity. History is never reset at READY;
overflow remains a failure or inconclusive result. The actual recorded freeze,
not an earlier outside clock sample, must be within the original horizon.

A separate versioned sidecar binds the token, QPF, configured journal window,
WAITING and READY clock brackets, source observation and actual freeze. It is
cross-checked against the supervisor's SetEvent bracket and source/handoff
headers before image decoding. Native wake-up may precede the supervisor's
post-SetEvent clock sample. The existing source-v1 wire schema is unchanged.

The protocol assumes a trusted local owner; named events do not authenticate a
sender PID. `owner_assertion_only` is true and `scene_proven` is false.
No game input, focus activation, desktop capture or save operation is provided
by this prerequisite.

## Verified prerequisite

- `private/owner-ready-tests-20261015-2`: 24 C++ checks passed, including actual
  kernel-event rendezvous without a window or game, stale objects, cancellation,
  reversed clocks and deadline/cut boundaries. The first compile retained a
  format-warning failure; the corrected fixture uses the proper DWORD format.
- `test_mcla_owner_ready.py`: eight tests passed. They cover real-shaped source
  metadata, early-anchor rejection, native-before-supervisor wake-up, malformed
  schema/token/integers, exact-once console lines, real event-name collisions
  and cleanup after partial event creation failure.
- A bounded Windows PTY test exercised the actual `ConsoleReady` reader:
  entering READY produced `CONSOLE_FIXTURE_RESULT READY`. No game was launched.
- Independent read-only design and source reviews found no remaining blocker
  after the source-QPF field and expected-token checks were corrected.

The prerequisite consists only of the gate headers, their standalone C++ test,
event/console helper, strict pairing decoder and Python tests. Native game
integration, preparation scripts and one-shot supervisor remain separate work.

## Next gate

Review and pin the separate owner4 package, then have the owner load free roam,
park, enter READY in its console and return to the game. Accept the collection
only after full identity, cleanup, journal and all-frame scene review.
