# Synthetic timer policy and subdivision comparison

October 15, 2026. Exact setter, timer and subdivision bodies pass 72 bounded
synthetic scenarios with 432 full-state comparisons. Fixed policies keep the
same effective factor regardless of injected interval. The raw policy responds
to that interval but still applies lower and upper clamps. This identifies
policy differences; it does not validate an optional 60 FPS mode.

## Configuration seam

Observed update caller `822C1FA8` consumes and subdivides the timer value;
it does not establish the fixed mode. Setter `821BD9F8` sets selector `+60=1`
and computes `+32`, `+8` and `+88` from the reciprocal of incoming F1 times R5.
Setter `821BD9B8` establishes selector `+60=0` and an optional configured cap.
Their direct generated caller `822D2478` derives arguments from its configuration
object. Its live configuration fields were not captured; this experiment does
not infer their individual values from the observed output.

The [captured replay](M8-timer-operand-replay-20261015.md) supplies exact timer
and helper bodies and checked sparse memory. This experiment adds exact renamed
setter bodies in a private include; no game body is committed. Public source
`tests/mcla_timer_policy_replay_win.cpp` reuses the accepted operand harness
unchanged and runs its full 1398-row nearest baseline before policy scenarios.

## Synthetic inputs and checks

Seed timer ordinals are predeclared first, middle and last: 0, 116 and 232 of
233, not selected by outcome. Each seed runs four policies with six injected
integer tick differences: 500, 500000, 833333, 1666667, 3000000 and 10000000.
Captured scale, clamps and histories are preserved at initialization.

| Policy label | Explicit setter inputs | Meaning in this experiment |
| --- | --- | --- |
| fixed30 | F1=30, R5=1 | Fixed factor near 1/30 |
| fixed60 | F1=60, R5=1 | Fixed factor near 1/60 |
| raw_uncapped | F1=0, F2=30 | No configured `+32` cap; normal clamps remain |
| raw_cap30 | F1=30, F2=30 | Raw factor with a configured cap near 1/30 |

These labels describe inputs, not achieved FPS. Explicit positive F2 avoids the
raw setter's default-F2 branch; its extra constant is unmapped and that branch
is not claimed tested. The actual configuration caller is not executed.

Every scenario chains actual state through six stages: setter, timer,
split `(1,2)`, split `(1,2)` again, restore `(0,0)`, and final `(0,0)` no-op.
A separate scalar oracle evolves its own state without calling extracted
bodies. All 96 known bytes are compared after each stage. Unknown gaps reject
access; constants are read-only and stack reads require initialization.
Seven boundary negatives exercise gap/cross-gap reads, constant writes,
uninitialized stack, object overrun, duplicate timebase and zero-low timebase.

The oracle covers finite inputs with captured flags 56, 58 and 72 zero,
positive scale and ordered clamps, under explicit nearest rounding. It is not
a general NaN, reset, pause or all-mode equivalence test. Timebases are injected;
runtime MXCSR, live clock configuration and physical units remain unproved.

## Results

Both O3 and O2 reproduce all 1398 captured baseline rows and all 432 scenario
stages, with identical complete JSON. Independent Python arithmetic agrees
with every native and C++ oracle state: 41472 known bytes per report.

Fixed30 stays near 0.033333335 for every interval; fixed60 stays near
0.016666668. Raw without configured cap follows the injected interval between
the preserved lower/upper clamps. At the smallest tick difference its effective
factor is clamped up to approximately 0.0001; at the largest it is clamped down
to approximately 0.1. Raw-cap30 additionally caps the effective factor near
0.033333335. Raw history fields `+20/+24` advance before cap/clamp application,
so equal effective factors do not imply equal histories.

The first and second splits produce identical complete state in all 72 cases:
the helper restores the saved unsplit value before dividing again, rather than
quartering it. Subdivision does not reapply clamps; half of a floor-clamped
value is below that floor. Restore recovers the timer's effective value, and
the final reset changes nothing and performs no store.

The negative control flips an actual output byte after the timer and leaves
the oracle unchanged. The corruption propagates through subsequent actual
stages: 360 of 432 comparisons fail while the 72 setters remain unchanged.
It exits 1 after the clean captured baseline; an undetected mutation would
exit 3. Seven malformed fixture inputs reject with exit 2. AST scan and all
three AST rule tests pass; reused timer decoder/owner gates retain their
29/61 passing tests from the preceding accepted task.

Independent review verifies source, fixture ancestry, all 19800 pins per build,
126 project dependencies and result inventories. The 474 system/toolchain
dependencies remain explicitly nonhermetic. PE imports contain only system CRT
and kernel dependencies; no game or project DLL is launched by the test.

## Reproduction identities

Fixture: `private/timer-policy-fixture-20261015-1`, retaining the unchanged
1398-row operand binary. Exact setter include: 2808 bytes, SHA256
`504C18D6D91149BFE0387169C52749AC863B270B681744C6B3457F619A5765DF`.
Public harness SHA256:
`AE0C55F373DA4570DB51EEDA898FAD73F52AB3E9C9851D955F2F9F0DD7559557`.

Builds: `private/timer-policy-replay-o3-20261015-1` and
`private/timer-policy-replay-o2-20261015-1`. O3 executable: 256000 bytes,
SHA256 `5ACA6F87B8F818222E2976FA9DE3DFA95A8930A9E7EE24F1A459F309699C60AC`.
O2 executable: 240128 bytes,
SHA256 `B671320151CDB5D295AC593E2915F0095B146D7B2DD3878054F5DA5E1E9266D8`.
Baseline JSON: 338939 bytes,
SHA256 `464864E80651DA10982CB5BF810E0AE968D527BF616BD579765E51C663D905B3`.
Mutant JSON: 338186 bytes,
SHA256 `CB6AA5D4F202E744F8A5ED752BC6C9B0607F8FCAC84A663C7156B86073D1DA06`.
All fixture/build writers are consumed. No normal binaries or saves changed.

## Runtime preparation decision

Next is a discriminating presentation-only runtime comparison while retaining
the stock timer, not another identical parked run. The old September interval1
binary is not reused or considered tested. A fresh candidate must derive from
the exact current diagnostic stock build, with baseline reproduction and an
independent changed-code review.

Capacity preflight blocks simply repeating owner8's 10.1-second window: 3029
timing entries would exceed 4096 at twice the observed rate; 9786 router events
would exceed 16384. Linear scaling is only a warning, not a prediction. Resolve
a separately reviewed shorter complete collection or common capacity adjustment
before asking the owner to run anything. Preserve all requested frames and
failures; do not salvage a post-hoc convenient subset.

M8 remains 4 complete, 6 deferred, 19 total, YELLOW. M8-001 stays open. A later
candidate discriminator alone cannot establish normal speed or stable 60 FPS;
matched controls and representative driving/audio/dip tests remain required.
