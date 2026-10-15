# Captured timer and subdivision replay

October 15, 2026. Offline replay reproduces all 233 captured timer calls and
1165 subdivision calls from owner8 under the declared nearest rounding mode.
Controlled inputs establish that the selected fixed branch ignores the raw
interval for its effective factor. This is not a live clock measurement or
acceptance of a 60 FPS patch. MAIN and the normal l12 build remain unchanged.

## Inputs and boundary

The [accepted owner8 collection](M8-owner8-gameplay-return-20261015.md) supplies
all timer and subdivision records, including 233 pre-loop, 699 loop and 233
post-loop helper calls. Router records remain in the original journal and are
not executable inputs to these two functions. The raw metadata validator finds
no issues, contexts or pending records. Row identities preserve observation ID,
exact Update binding and caller address; no nearest-time association is used.

`tests/mcla_timer_operand_replay_win.cpp` executes the separately extracted,
hash-guarded original bodies `821BDA90` and `821BD910`. Only their names and
memory/timebase interfaces change. Checked sparse memory provides the 96 known
object bytes, eight bytes of owned stack scratch and three read-only constants.
The twelve unobserved object bytes are poisoned; accesses to them reject.
Scratch reads require a preceding write. The original pointer's high word is
retained while its low word maps to the synthetic object. No game, GPU, real
clock, saves or guest runtime is launched.

Baseline timer input uses the captured after-state qword at offset 64 as its
one injected timebase return. Reproducing that same qword is therefore not
independent evidence of clock behavior. Every other known state byte is still
compared. Helper calls use no clock return. Captured R4, R5 and raw F1 are retained.
Runtime MXCSR was not captured; hardware and cached context state are explicitly
initialized for each offline call.

The bounded input format has a 28-byte header and 244-byte rows: magic
`MCLATR1\0`, version, count, three constant words, then kind, registers,
controlled timestamps and two packed 96-byte snapshots. Count must be 1–4096
and EOF exact. Input data and extracted game bodies remain private.

## Constants and preserved reader failure

The exact admitted XEX is decoded only in memory using the SDK's basic-image
format. No whole image or keys are exported. The initial reader rejected an
overly broad condition requiring every PE section to fit the decoded image.
A separate metadata-only diagnostic located the unrelated `.reloc` section
extending beyond it; the SDK also permits such section records.

The fresh third reader counts all matching sections before applying bounds. It
requires the entire unique target section, as well as each four-byte read, to
fit the decoded image. All three constants lie within `.rdata`. Expanded size
equals the XEX security image size. Exact input/helper hashes, section bounds
and expanded-image hash are retained in
`private/timer-image-constants-v3-20261015-1.json`.

| Guest address | Raw word |
| --- | --- |
| 82011110 | 32AC3AB1 |
| 82001D14 | 3F800000 |
| 82000ED4 | 00000000 |

The first value is approximately 2.0050125e-8, not exactly 1/50,000,000.
The failed first reader and successful diagnostic/third-reader identities are
preserved; none is a new game run.

## Results and controls

Both O2 and O3 builds produce identical baseline and mutant JSON. All 1398
nearest-mode baselines match all 96 known bytes. Other rounding modes remain
diagnostics rather than a search for a passing mode.

| Declared rounding | Timer matches | Helper matches | Total matches |
| --- | ---: | ---: | ---: |
| Nearest | 233 | 1165 | 1398 |
| Down | 124 | 1165 | 1289 |
| Up | 0 | 466 | 466 |
| Toward zero | 124 | 1165 | 1289 |

Each timer receives four controlled cases: before-timebase plus 500000 or
3000000, each with selector word `+60` set to zero or one. Eligibility guards
require the captured relevant flags zero, negative finite F1 and positive
finite cap/scale. The resulting pre-cap field `+104` must actually bracket the
configured `+32` threshold; a rejected scenario cannot count as success.

For all 233 rows, the below-threshold raw case produces effective `+8`
`3C244029` (about 0.01002506), while both fixed cases retain `3D088889`
(about 0.033333335). Above threshold, the raw branch caps `+8` to that same
value, but accumulator `+20` differs from the fixed case in every row. Equal
effective factors do not establish equal whole timer state. These are raw
floating values, not a claim of physical simulation seconds.

Two negative controls follow a separate clean 1398-row baseline. Changing the
actual reciprocal-numerator input changes outputs in 932 baseline rows; 466
no-op helpers remain unchanged. Flipping an actual stored byte changes all
1398. Both mutants exit 1; an undetected mutation would exit 3. Seven malformed
inputs reject with exit 2: truncated, trailing, wrong magic/version, zero or
excessive count, and invalid kind.

Independent Python float32 state arithmetic reproduces all 2330 nearest-mode
output states (223680 known bytes) for each of the baseline and two mutant
reports, with zero mismatches. Full per-row output is retained for nearest mode;
the other three modes retain aggregates only. Input/body/source reviews, the
29 timing decoder tests, 61 disarmed owner8 tests, AST scan and three AST rule
tests pass. No consumed owner collector was rerun.

## Reproduction identities

Fixture: `private/timer-operands-owner8-20261015-1`.
`rows.bin`: 341140 bytes, SHA256
`BD8C5A50782DCDC50E748027EBC2BC4DB08EC977CA29320C5728216AA42465DC`.
Exact include: 14710 bytes, SHA256
`9198FA53116CA82DA0015E259D4E40EC7F67DC1D27C129015C7F65118B312732`.
Harness SHA256:
`F9C5804108B0B8D2AE8A286DE5109AAAE30CDD0C13AEFE7C6F106C2826C1D471`.

Build roots: `private/timer-operand-replay-o3-20261015-1` and
`private/timer-operand-replay-o2-20261015-1`. Each retains exact compiler
arguments, source/dependency/library pins, binary identity and test exits.
System libraries are not claimed hermetic. Baseline JSON: 1111901 bytes,
SHA256 `3F4857372638B4E9070DAAC3DD3F3BCD4F9124DD140E305B0A7056A78966F506`.
Producer and build writers are consumed; reproduction requires fresh output
identities rather than overwriting these receipts.

## Next gate

The fixed factor's source and arithmetic are now established for the captured
path, complementing the [state-store replay](M8-store-operand-replay-20261015.md).
M8 remains 4 complete, 6 deferred, 19 total, YELLOW; M8-001 stays open. No
timing patch, normal-build replacement or FPS mode is promoted.

Next, audit the exact writer/caller and choose a bounded stock versus candidate
timing experiment. Another identical parked capture without a discriminating
change would add little. A current runtime clock configuration witness may be
needed: static defaults and compatible tick ratios alone do not prove the live
settings. Game-world speed and below-target behavior still require their own
physical evidence.
