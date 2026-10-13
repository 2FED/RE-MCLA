# Immutable semaphore snapshot at the live cut

The offline snapshot bridge preserves the exact stopped journal for PREFIX-v2
analysis. Late completions cannot replace pending rows in that receipt. Native
collection, complete SDK coverage and game-frame timing remain separate gates.

The caller must hold its observer lock, read the real QPC cut, mark the host
frozen, stop journal admissions, disable transport admissions, and copy the
snapshot before releasing the lock. Serialization happens afterward. Disabling
the transport is not a drain barrier, and normal game shutdown is not the cut.

## Frozen identity and coverage

Capture binds a 32-character lower-hex token, frequency, observation start,
actual first stop time and each epoch's ID, cookie and creation time. It owns
copies of all retained rows and counts. Source destruction, caller edits and
later completions cannot change an existing snapshot.

Admission coverage is a separate typed claim. It does not clear the raw core's
incomplete-history or unsupported-operation flags. The raw companion preserves
those flags and diagnostics; PREFIX-v2 retains pending results as null and
unlocalized overflow as an unknown loss boundary. Malformed raw fields remain
available in the companion but cannot be serialized as valid prefix evidence.
Each JSON output is limited to 2 MiB. All proof/admission flags remain false.

## Regression and review

Independent review found that testing only whether pending operations remained
could accept a partially drained journal: two waits were pending at the cut,
then one completed. The initial 47-check suite missed that case. Added tests
failed 6 of 73 checks, including a completion sharing the cut's QPC tick.

The core now records a sticky post-stop mutation flag at every mutating public
entry, including rejected and duplicate calls. Capture refuses changed-cut
state. Repeated stop remains a no-op; read-only queries do not invalidate the
original cut. The old v1 issue/history semantics are unchanged.

Clang 20 C++23 Release with /W4 /WX passes all 83 expanded checks. Ten emitted
C++ fixtures also pass the unchanged Python PREFIX-v2 parser and evaluator:
three retain one conservative candidate each, seven remain inconclusive.
The existing 107 Python prefix/credit/phase-join tests pass. Independent source
re-review accepts the correction. RED and GREEN outputs are preserved under
private/sem-snapshot-red-20261013 and private/sem-snapshot-green-20261013.

| Source | SHA256 |
| --- | --- |
| src/mcla_semaphore_observer_core.h | A80C4B620784CA6B7666BB2AD4487A3E1DF287CEA18E5A9ECE0D7BF870BD7048 |
| src/mcla_semaphore_prefix_snapshot.h | D5A11933DCCE342F4C0F9C116856604169B559135C6EA877B874F6159AB86A6D |
| tests/mcla_semaphore_prefix_snapshot_test.cpp | 0231E4DFD96740992B7765430DA3529643AB38F2B7E6DC90A358665DE29E9075 |

This is an offline prerequisite, not a native completeness or simulation proof.
M8-001 stays open; l12 and MAIN are unchanged.
