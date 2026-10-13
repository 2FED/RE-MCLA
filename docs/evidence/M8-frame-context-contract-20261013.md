# Frame context and semaphore credit contracts

Two offline prerequisites now reject ambiguous update and render associations.
They do not establish native observation coverage, simulation steps, frame rate,
or a game-frame-to-update relationship. M8-001 remains open.

## Scope bookkeeping

`src/mcla_frame_context_core.h` tracks a bounded dynamic call stack on one
independently identified native thread lifetime. IDs contain `thread_epoch` and
`serial`; a reused native thread number cannot serve as the epoch by itself.
An update can pass its ID to one following coordinator invocation. A nested
synchronous render inherits that ID. A worker render instead consumes a unique
successful wait-operation ID; this core does not assign its update.

Invalid callers, changed context/base identity, stale updates, credit reuse,
incorrect nesting, unwinding and counter overflow poison only the observer.
The tracker cannot be copied or moved. Original game calls belong to a separate
native wrapper and must execute exactly once even when observation fails.

The primary compiled the standalone regression using Clang C++23 with
`-Wall -Wextra -Werror`: **115 checks, zero failures**, plus four compile-time
copy/move restrictions. A replay regression first produced six failures in
89 checks; a monotonically increasing wait-operation high-water mark now rejects
both reused and reordered credits, even under a changed coordinator epoch.
Private test outputs remain in `private/frame-context-20261013`.

## Semaphore accounting

`scripts/mcla_semaphore_credit_audit.py` reads at most 2 MiB of immutable JSON,
4096 operations and 64 object epochs. Exact schemas, bounded integers, token,
clock, counters and complete-history assertions precede accounting. Update IDs
use the same two-field form as the C++ tracker. Returned nested containers do not
alias one another.

The sufficient association rules admit either strictly disjoint single-credit
operations or an isolated overlap component containing exactly one successful
release of one credit and one successful wait, starting with zero credits.
The latter does not assume FIFO or select the nearest timestamp: the complete
supplied history leaves only one possible credit. A touching third operation,
competing credits, unknown initial state, incomplete history, unsupported wait,
Save, SignalAndWait, restore or overflow suppresses every candidate.

Object epochs represent lifetimes, not handles. Aliases retain one epoch, and
pointer reuse requires witnessed destruction. These are input assertions until
a separately reviewed native observer supplies the journal.

All **44 Python tests pass** with the pinned Python `-E -S -B`, independently
rerun in review. Coverage includes malformed IDs, alias/reuse, overlapping and
disjoint operations, failure results, bounded input and detached outputs. An
in-memory negative control that split touching components incorrectly was caught
by its neighbor-touch test. Native and proof flags remain false in every result.

## Review and next step

Primary review and an independent read-only review accept these contracts only.
The actual generated-function link experiment separately passed 4 baseline and
8 wrapped checks; it does not establish interception in the title.

The next bounded native diagnostic inventories actual update, coordinator and
render dynamic scopes before assuming a synchronous or worker path. It must
preserve original calls, PPC state, exceptions and LastError. A later worker
association needs complete semaphore object history; a scope census alone cannot
supply that evidence. No accepted package, MAIN save or SDK default changes.
