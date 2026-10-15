# Captured state store arithmetic replay

October 15, 2026. Offline native replay of the exact short store body reproduces
all 1578 captured owner8 results bit for bit under the predeclared nearest
rounding configuration. Half, double and zero input factors change 646 stored
vectors. This establishes arithmetic sensitivity for these captured operands,
not physical units, player identity, runtime clock behavior or 60 FPS correctness.

## Input and execution boundary

The [owner8 return](M8-owner8-gameplay-return-20261015.md) supplies all 1578 raw
operand tuples, including 618 source-selected bodies. No convenient finite-only
subset is substituted. Every tuple retains seven four-lane vectors: incoming
V1, old, rate, computed, auxiliary, scaled and stored. Row identities and the
selected subset are recorded separately.

`tests/mcla_vector_store_operand_replay_win.cpp` runs in 64 KiB of owned synthetic
memory. Its separately prepared `store-plain.inc` contains the hash-guarded
original body, renamed only; the original tail is replaced with a recording
stub. Guest byte order and register lane order are reversed exactly at loads
and stores. The harness checks the three loaded vectors, exactly one tail call
and unchanged memory outside the sixteen-byte destination. It launches no game
and does not touch saves or the GPU.

The binary input is bounded to 4096 rows: eight magic bytes `MCLASR1\0`, little
endian version 1 and count, then 28 little-endian uint32 words per row. Truncated,
trailing, wrong-magic, wrong-version, zero-count and excessive-count inputs reject.
Captured operand data and the extracted game body remain private.

## Results and negative controls

Both O3 and O2 builds produce identical complete baseline and mutant JSON.
Each clean replay checks all 1578 tuples at original, half, double and positive
zero factors under four declared MXCSR configurations. Hardware MXCSR and the
context's cached CSR are both initialized. The exact body enables its normal
flush behavior. The runtime capture did not record MXCSR; agreement does not
retroactively establish its configuration.

| Requested rounding | Rows with exact original computed, scaled and stored outputs |
| --- | ---: |
| Nearest | 1578 / 1578 |
| Down | 943 / 1578 |
| Up | 945 / 1578 |
| Toward zero | 940 / 1578 |

Only the predeclared nearest mode is the baseline acceptance gate. Other modes
are retained diagnostics, not discarded failures or a search for a passing mode.
Nearest mode matches 6312 lanes for each of computed, scaled and stored vectors,
including all 3156 NaN output occurrences across those three vectors.

Half, double and zero each change 646 computed/stored vectors and 644 scaled
vectors. Within the 618 source-selected tuples, 254 stores change. Zero preserves
all 4734 eligible finite old lanes bitwise; this condition requires both old
and rate finite and does not apply to the NaN lane.

Two negative controls alter actual synthetic data while leaving expected outputs
untouched. Input mutation produces 1577 mismatching rows; stored-byte mutation
produces 1578. Both exit 1 after a separate clean 1578-row baseline. All six malformed
inputs exit 2. An undetected requested mutation would exit 3, never success.

Independent review verifies all binary rows and identities, all 19797 pins per
build, and every nearest-mode result for all four variants and both mutants:
100992 raw output words per report match an independent float32 round/flush
calculation. Source, memory confinement, endianness and negative controls pass.
The 78 target/store Python tests, disarmed owner8's 61 tests, AST scan and all
three AST rule tests also pass.

## Reproduction identities

Frozen fixture: `private/vector-store-operands-owner8-20261015-1`.
`rows.bin`: 176752 bytes, SHA256
`DCE6D402B112618C58778B27725F1BC411816FE0DEB9BCE2EE6C53E99E305222`.
Original normalized body SHA256:
`0d9b6bc342c2eef502021af2df2f3c7fc772c7fb440434cdd3eb7c7f9146ab67`.
Harness SHA256:
`B6F99342D93A369D37859882278CFB86F73511104B990791271C89A9944F469F`.

Private builds: `vector-store-replay-o3-20261015-1` and
`vector-store-replay-o2-20261015-1`. Their `inputs.json` records exact compiler
arguments and pinned headers; fixture `receipt.json` records extraction and
row provenance. The producer and build-output writers are consumed. Future
reproduction requires a fresh output root, not overwriting these receipts.

O3 executable: 143360 bytes, SHA256
`78B06D698B18E942E3FA9E300B85C4A1BB09990BB8F9894F998FDB610F3A226D`.
O2 executable: 134144 bytes, SHA256
`D9EDB9078A7D6C6B6B89598129E3A8F9475828758E71357C390445C1CED6AB30`.
Identical baseline JSON: 1472486 bytes, SHA256
`1990A8CD751A85F450F1217E1EC6A6E2B854C1DF2DCD19C0B4A49315BE2B9110`.
System-library resolution is not claimed hermetic.

## Milestone checkpoint

M8 remains 4 complete, 6 deferred, 19 total, YELLOW. The prerequisite closes the
captured-operand replay question, not M8-001. No game binary or 60 FPS mode is
promoted, and MAIN/l12 remain unchanged. Next is the bounded origin of the fixed
factor in the captured timer/subdivision path; another identical parked run
does not add discrimination for the current constant selector.
