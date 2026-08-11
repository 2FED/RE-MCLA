# M3-008 deterministic early initialization

Date: 2026-08-11

Status: accepted

## Question

Does the native port expose the Xbox 360 50-MHz guest clock consistently and start raw/XAPI guest threads with a repeatable, Xenia-compatible ordering during early initialization?

## Reference audit

Current Xenia behavior was treated as the primary compatibility reference:

- [Xenia `clock.cc`](https://github.com/xenia-project/xenia/blob/master/src/xenia/base/clock.cc) keeps a process-global host-derived guest timebase with a configurable guest frequency and scalar.
- [Xenia `xthread.cc`](https://github.com/xenia-project/xenia/blob/master/src/xenia/kernel/xthread.cc) inserts a 10-ms compatibility grace period after the kernel thread-start notification and before initial APC delivery/guest dispatch.

The existing ReXGlue clock path already matched the reference design:

- `Runtime::Setup` configures `50,000,000` guest ticks per second, a host-system-time base, and scalar `1.0` before memory and kernel construction.
- generated `mftb` and `mftbu` operations route through `Clock::QueryGuestTickCount()`.
- `KeQueryPerformanceFrequency` exposes the configured guest frequency.
- `KeQuerySystemTime` exposes `Clock::QueryGuestSystemTime()`.

Resetting the guest tick count to zero per module launch was considered and rejected. It would diverge from the reference's process-global host-derived clock and was not supported by observed evidence.

The actionable discrepancy was guest-thread startup: ReXGlue initialized new threads suspended and prepared PCR/TLS/CPU state correctly, but omitted Xenia's compatibility delay. A child could therefore race creator-side shared-state initialization immediately after `ExCreateThread`.

## Implementation

The project ReXGlue fork now:

- defines an exact `10 ms` guest-thread startup delay;
- applies it after `KernelState::OnThreadExecute` and before initial `DeliverAPCs` and guest dispatch;
- centralizes raw and XAPI dispatch in a testable `StartPlan`:
  - raw thread: requested start routine, context in `r3`, returned `r3` used as exit code;
  - XAPI thread: XAPI trampoline, requested routine in `r3`, context in `r4`, trampoline return not used as the thread exit code;
- directly tests the 50-MHz clock ratio, monotonic progression, raw/XAPI plans, and the compatibility delay.

Immutable SDK state:

- tag: `v0.9.0.6`
- commit: `eda7aebf9dbe8140d45f67d3e15053383d142696`
- upstream report: [rexglue/rexglue-sdk#408](https://github.com/rexglue/rexglue-sdk/issues/408)

The issue was created only after searches for `thread startup`, `CreateThread race`, `10 ms`, `shared structures`, and `mandatory sleep` found no matching issue or pull request.

## Fail-closed project contract

`scripts/verify-early-init-contract.ps1` checks the three accepted imports, ordered clock setup, both generated timebase operations, both xboxkrnl timing exports, the exact 10-ms constant, guest `Execute` ordering, raw/XAPI start plans, and all four registered SDK regression cases.

The positive verifier reports 3 imports, 7 timebase checks, 10 thread-start checks, and 4 SDK regression cases. `scripts/test-early-init-contract.ps1` passes one positive case and rejects four mutations: a 49-MHz clock, removed delay, APC delivery before the delay, and a raw-thread context replaced by the start address.

The runtime trace verifier has one positive and three negative fixtures. It rejects a missing frequency event, a start plan before thread creation, and a fatal runtime log.

## Repeated runtime trace

`scripts/run-early-init-smoke.ps1` launches isolated user/cache roots under CDB, bounds each run to 30 seconds and 64 recorded events, records the three relevant guest imports, and stops at the first guest-thread `BuildStartPlan` after all three import classes have been observed. The final RelWithDebInfo series passed 3/3 runs:

| Run | `ExCreateThread` hits | `KeQuerySystemTime` hits | `KeQueryPerformanceFrequency` hits | Terminal plan | First-occurrence signature |
|---:|---:|---:|---:|---:|---|
| 1 | 14 | 2 | 1 | 1 | create-thread -> system-time -> time-frequency -> start-plan |
| 2 | 13 | 2 | 1 | 1 | create-thread -> system-time -> time-frequency -> start-plan |
| 3 | 14 | 2 | 1 | 1 | create-thread -> system-time -> time-frequency -> start-plan |

All traces stayed within both bounds, produced the same first-occurrence ordering, reached the terminal child start plan, and contained no fatal, invalid-function, or `PPC_UNIMPLEMENTED` marker. The varying thread-create count is scheduler-dependent activity before the shared terminal condition; it is recorded rather than falsely asserted to be identical.

The private transcripts, runtime logs, isolated roots, and `result.json` remain ignored under `private/evidence/M3-008`. The public evidence contains no proprietary memory or generated guest code.

## Regression totals

- focused SDK early-init tests: 4 cases / 14 assertions passed;
- relevant SDK unit suite: 214/214 tests passed, with four pre-existing BitStream skips;
- SDK PPC suite: 1,459/1,459 cases passed;
- complete project script suite: 19/19 tests passed;
- project early-init contract: positive plus four rejected mutations;
- project trace verifier: positive plus three rejected traces;
- repeated live trace: 3/3 bounded runs passed.

The unfiltered SDK unit target still has the previously documented unrelated template/migration baseline failures; M3-008 does not reclassify them.

## Acceptance decision

M3-008 is accepted. Early guest timing has an exact, reference-compatible 50-MHz route; raw/XAPI thread dispatch is unit-tested; the missing compatibility delay is fixed; and three isolated real-game startups produce the same bounded timing/thread first-occurrence order through a child guest-thread plan.
