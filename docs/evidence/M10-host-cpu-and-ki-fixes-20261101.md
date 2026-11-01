# M10 host: native AddRef/Release (CPU task 1), KI-055 and KI-058 (2026-11-01)

Implementation by an Opus subagent for the primary agent (M10). Host-only changes on `main`
(uncommitted when written); SDK unchanged (`private/l55-sdk-install`, v0.20.0.6 content).
No game or launcher was started; no save, profile or key was touched.
Spark: not used. The three items are exact transliterations and small policy changes whose
validation is the deterministic tests below; a model answer would need the same checks.

## 1. CPU task 1 (M10-CPU-A): native XDK D3D AddRef/Release and XAPI Interlocked*

Source: `docs/evidence/M10-cpu-targets-20261101.md` sections 2.3 and 2.6 (rank 1).

### Design

- `src/mcla_native_refcount.h` (new, header-only, templated on the context and guest-memory
  types so the unit test runs the production code): one host atomic per guest region on the
  big-endian guest word (`std::atomic_ref<uint32_t>`; CAS loop with byte swapping for the
  add/sub forms, `xchg` for the exchange, one `compare_exchange_strong` for the compare
  exchange). No global mutex and no extra fences (the locked instruction is a full barrier
  on x86).
- `src/mcla_native_refcount.cpp` (new): strong `REX_EXTERN` overrides (the pattern of
  `src/mcla_menu_state.cpp` and `src/mcla_raw_timer.cpp`) of
  `sub_82422160` (AddRef), `sub_824221D8` (Release), `sub_821CE080` (InterlockedExchange),
  `sub_821CE0A0` (InterlockedCompareExchange), `sub_821CE0D8` (InterlockedIncrement),
  `sub_821CE100` (InterlockedDecrement), `sub_821CE128` (InterlockedExchangeAdd).
  Guest memory goes through the generated macros (`REX_RAW_ADDR`, `REX_LOAD_U32/U64`,
  `REX_STORE_U32/U64`), so the physical-range host offset (>= 0xE0000000) is identical.
- Cvars (init-only, read once): `mcla_native_d3d_refcount` (default true) for AddRef/Release,
  `mcla_native_xapi_interlocked` (default true) for the five XAPI helpers. `false` runs the
  generated body (`__imp__sub_...`) unchanged, so one binary gives the A/B.
- A counter word that is not 4-byte aligned (impossible for `lwarx` on the console) is left to
  the generated body; nothing is changed before that check.
- Release, count reaching zero: the original's zero path is reproduced as generated, inside
  the original frame (`mflr r12; stw r12,-8(r1); std r31,-16(r1); stwu r1,-96(r1)`), under a
  `rex::ppc::GuestFunctionScope` for 0x824221D8 (crash breadcrumb kept for the destroy path):
  parent rule (`lwz r11,0(r31)`, type `& 0xF == 4`, flag `0x40000000`, then
  `lwz r3,24(r31); bl 0x824221D8` with `lr = 0x82422238`), then `mr r3,r31; bl 0x82421AA8`
  with `lr = 0x82422240` (the guest destroy stays guest code), `li r3,0`, and the epilogue.
- AddRef parent rule (new count 1, type 4, flag set): the same frame, `r31 = new count`,
  `lwz r3,24(r3); bl 0x82422160` (`lr = 0x824221BC`, the call goes through the override
  again), `mr r3,r31`, epilogue.
- Calls into the overrides: `PPCFuncMappings` (indirect calls) holds `sub_82422160` etc.,
  and the scratch link map below shows the direct callers (e.g. `__imp__sub_82415F78`)
  calling the override's address.

### Equivalence note (generated instructions mirrored)

Generated bodies: `generated/default/mcla_recomp.27.cpp` (`sub_82422160`, `sub_824221D8`) and
`generated/default/mcla_recomp.5.cpp` (`sub_821CE080` to `sub_821CE128`).

| Function | Generated instructions | Native result (registers on return) |
|---|---|---|
| `sub_82422160` | `mflr r12; stw r12,-8(r1); std r31,-16(r1); stwu r1,-96(r1); addi r11,r3,4;` loop `mfmsr r9; mtmsrd r13,1; lwarx r10,0,r11; addi r10,r10,1; stwcx. r10,0,r11; mtmsrd r9,1; bne`; `mr r31,r10; cmplwi cr6,r31,1; bne cr6,out; lwz r11,0(r3); clrlwi r10,r11,28; cmplwi cr6,r10,4; bne cr6,out; rlwinm. r11,r11,0,1,1; beq out; lwz r3,24(r3); bl 0x82422160;` out: `mr r3,r31; addi r1,r1,96; lwz r12,-8(r1); mtlr r12; ld r31,-16(r1); blr` | `r3 = (u64)old + 1` (64-bit: 0xFFFFFFFF gives 0x100000000); `r9 = 0x8000`; `r10`, `r11`, `cr6`, `cr0` as the last executed instruction left them (`r11 = r3 + 4`, or the header word / masked flag on the type path); `cr0 = {0,0,1,xer.so}` from `stwcx.` unless `rlwinm.` ran; `r12 = lr = (u32)entry lr`; `ctx.reserved = old raw word`; `msr` bits 0x8020 = 0x8000; `r1`, `r31` unchanged |
| `sub_824221D8` | same prologue; `mr r31,r3; addi r11,r31,4;` loop with `addi r10,r10,-1`; `mr r3,r10; cmplwi cr6,r3,0; bne cr6,out; lwz r11,0(r31); clrlwi r10,r11,28; cmplwi cr6,r10,4; bne cr6,d; rlwinm. r11,r11,0,1,1; beq d; lwz r3,24(r31); bl 0x824221d8;` d: `mr r3,r31; bl 0x82421aa8; li r3,0;` out: epilogue | non-zero: `r3 = (u64)old - 1` (0 gives 0xFFFFFFFFFFFFFFFF), registers as above; zero: the generated zero path verbatim (above), `r3 = 0` |
| `sub_821CE080` | `mfmsr r10; mtmsrd r13,1; lwarx r11,0,r3; stwcx. r4,0,r3; mtmsrd r10,1; bne; mr r3,r11` | `r3 = r11 = old`, `r10 = 0x8000`, `cr0 = {0,0,1,so}` |
| `sub_821CE0A0` | `mfmsr r10; mtmsrd r13,1; lwarx r11,0,r3; cmpw cr6,r11,r5; bne cr6,keep; stwcx. r4,0,r3; mtmsrd r10,1; bne; mr r3,r11` / keep: `stwcx. r11,0,r3; mtmsrd r10,1; mr r3,r11` | `r3 = r11 = observed`, `cr6 = cmpw(observed, r5)` (signed 32-bit), `r10 = 0x8000`, `cr0 = {0,0,1,so}`; a mismatch still performs a locked write (lock cmpxchg), as the original `stwcx.` does |
| `sub_821CE0D8` / `sub_821CE100` | `mfmsr r10; mtmsrd r13,1; lwarx r11,0,r3; addi r11,r11,+1/-1; stwcx. r11,0,r3; mtmsrd r10,1; bne; mr r3,r11` | `r3 = r11 = (u64)old +/- 1` (64-bit), `r10 = 0x8000` |
| `sub_821CE128` | `mfmsr r9; mtmsrd r13,1; lwarx r11,0,r3; add r10,r4,r11; stwcx. r10,0,r3; mtmsrd r9,1; bne; mr r3,r11` | `r3 = r11 = old`, `r10 = r4 + old` (64-bit), word = low 32 bits, `r9 = 0x8000` |

`mfmsr` is `REX_CHECK_GLOBAL_LOCK()`, which returns 0x8000 unless the calling thread is inside a
locked region (never true in these leaf helpers); `ctx.msr` is written only by `mtmsrd` in the
whole corpus (never read), mirrored anyway. Not mirrored, deliberately: the fast paths' stores
below the stack pointer (saved LR/r31 and back chain of a frame popped before return; dead
memory under the PPC ABI) and the global lock itself, whose interrupt-mask exclusion is per
hardware thread on the console and is provided against other threads by the CAS alone
(M10-cpu-targets 2.3). Lost breadcrumb: samples and crashes inside the fast paths name the
caller in `current_function` (accepted in M10-cpu-targets 2.6).

### Tests

`tests/mcla_native_refcount_test.cpp` (new, `mcla_native_refcount_test`, pure, no SDK): a
fake context with the SDK `Register`/`CRRegister` semantics and a 64 KiB big-endian fake guest
memory; the reference is a line-by-line transliteration of the seven generated bodies (with the
lock single-threaded and `mfmsr = 0x8000`). Every case compares all 32 GPRs, LR, CTR, XER,
`reserved`, `msr`, CR0-CR7 and the memory outside the dead area below the caller's SP:

- directed AddRef/Release cases (plain, 0->1 with and without the parent rule, parent chain
  of two, wrap 0xFFFFFFFF, 0x7FFFFFFF, Release 1->0 with parent destroyed or surviving, type
  or flag not matching, Release of 0), with the destroy stand-in clobbering volatile registers
  and recording object and return address: same destroy sequence (parent before this object);
- directed XAPI cases (6 values x exchange, compare-exchange with equal/different/upper-bit
  comparands, increment, decrement, exchange-add with 64-bit addends);
- 4,000 randomized cases (random register file, LR with upper bits, random headers, counts and
  three-level parent chains, `r3` with upper bits set);
- misaligned words left to the generated body with nothing changed;
- 8 threads x 20,000 iterations of native AddRef/AddRef/Release and the interlocked helpers on
  shared words: no lost update.

Mutation check (scratch only): six single-line mutations of the header (LR not truncated,
32-bit instead of 64-bit sum, unsigned compare-exchange compare, parent release skipped,
reservation not mirrored, +2 instead of +1) each fail the test (31 to 4,074 failed checks).

## 2. KI-055: no `unexpected-guest-exit` snapshot after a window close

Cause (SDK `src/ui/rex_app.cpp`): a close request reaches `OnCloseRequested` ->
`MclaApp::OnWindowCloseRequested` (MCLA never vetoes), then `ReXApp::OnClosing` logs "Window
closing, shutting down...", calls `TerminateTitle()` and `std::_Exit(0)`. Terminating the title
ends the guest main thread, so the module thread calls `OnGuestThreadExit`, which queued the
snapshot that the hard exit cuts off (the `live-*.partial` folder).

Fix: `MclaApp::window_close_requested_` (atomic) is set (release) at the start of
`OnWindowCloseRequested` and before the readback-cost probe's programmatic
`window()->RequestClose()` (that path skips `OnCloseRequested`). `OnGuestThreadExit` reads it
(acquire) and asks the pure policy `mcla::diagnostics::ShouldSnapshotGuestExit(close_requested,
snapshot_probe, native_crash_probe)` (in `src/mcla_diagnostic_trigger.h`); after a close it logs
`MCLA lifecycle: guest exit after window close; no unexpected-guest-exit snapshot` and returns.
A guest exit without a close request (a real unexpected exit) still gets the snapshot; the
two probes keep their existing exclusion. `ReXApp::OnClosing` is private in the SDK, so the
flag is set at the last overridable point before it.

Test: `TestGuestExitSnapshotPolicy` in `tests/mcla_diagnostic_trigger_test.cpp` (8 checks).
Physical check for the owner: close the window normally; the session's diagnostics folder
gets no new `live-*.partial` and the log has the new lifecycle line.

## 3. KI-058: export `guest_refresh_hz` for Unlocked sessions

`ProjectLiveMetadata` (`src/mcla_diagnostic_export_policy.h`) already resolves and
re-serializes the recorded plan byte for byte; it now reports that plan's guest rate:
`plan->guest_vblank_hz` when non-zero (Unlocked: 240), else `plan->guest_refresh_hz` (60).
Stock and legacy outputs are unchanged. `docs/diagnostic-metadata-export.md` updated.
Tests (`tests/mcla_diagnostic_export_policy_test.cpp`): the Unlocked fixture and the
ticket-granted Unlocked fixture now require 240 (both fail against the old header: 2
failures), the owner-admitted 2160p stock fixture requires 60; all existing byte-for-byte
re-serialization and historical-identity cases still pass.

## 4. Files

- New: `src/mcla_native_refcount.h`, `src/mcla_native_refcount.cpp`,
  `tests/mcla_native_refcount_test.cpp`, this report.
- Changed: `CMakeLists.txt` (source and test target), `src/mcla_app.h`, `src/mcla_app.cpp`,
  `src/mcla_diagnostic_trigger.h`, `tests/mcla_diagnostic_trigger_test.cpp`,
  `src/mcla_diagnostic_export_policy.h`, `tests/mcla_diagnostic_export_policy_test.cpp`,
  `docs/diagnostic-metadata-export.md`.

## 5. Build and tests

Options: those of `private/l55-build` (`MCLA_TEST_BUILD_EXPIRY`, `MCLA_RAW_TIMER_DEFAULT_ON`,
`MCLA_PERF_SPLIT_DEFAULT`, `MCLA_LIVERY_DIAG2_DEFAULT`, `MCLA_READBACK_SYNC_COMPOSITE_DEFAULT`,
image-capture and diagnostic tests ON, F10 WGC OFF), `REXSDK_VERSION` empty, against
`private/l55-sdk-install`. Scripts: `private/l57-work/l57_host_build.ps1`,
`l57_host_build_targets.ps1` (adds `mcla_native_refcount_test` when defined),
`l57_host_tests.ps1`.

- Before (unpatched main checkout, `private/l57-host-build`): 136 steps, 0 warnings;
  **39/39** test executables passed (export policy 608 checks, trigger 102).
- After (patched tree, built by the subagent in a scratch copy outside the checkout because
  its session could not write the shared checkout; same options and SDK): 233 steps (full
  rebuild), **0 warnings**, 45 targets; **40/40** test executables passed
  (export policy 611, trigger 110, native refcount 4,711).
- Link map of the patched `mcla.exe`: `sub_82422160`, `sub_824221D8`, `sub_821CE0A0` etc.
  resolve to `mcla_native_refcount.cpp.obj`; `__imp__sub_...` stay in the generated objects;
  `__imp__sub_82415F78` calls the override's address.
- ast-grep 0.50.0: scan clean, rule tests 3/3. New and changed sources ASCII, CRLF (CMake and
  Markdown LF as before), no trailing whitespace.
Primary verification 2026-11-01: the patch was applied byte-exact into the main checkout (hashes match), `private/l57-host-build` rebuilt with 0 warnings, all test executables passed (`failed executables: 0`, including mcla_native_refcount_test, mcla_diagnostic_trigger_test 110 and mcla_diagnostic_export_policy_test 611), `git diff --check` clean, ast-grep scan clean.
  rerun the 40 tests, `git diff --check`.

## 6. What the owner measures

Same binary, BENCH 720p protocol (M10-001), warm, two arms:
A = `--mcla_native_d3d_refcount=false --mcla_native_xapi_interlocked=false` (generated bodies),
B = defaults (native).

- `MCLA_GUEST_SAMPLE` render thread (`F80004E0`): share of `sub_82422160` (6.2% in l53) and
  `sub_824221D8`. In B the fast paths carry no guest scope, so their samples are attributed to
  the callers (`sub_82417E40`, `sub_82415F78`/`FC0`, `sub_82185D80`, `sub_82185648`,
  `sub_82177AC0`, `sub_821792A0`): expected A about 6% -> B about 0% for `82422160`; the
  callers' shares must not rise by the removed amount (that would mean the time moved, not
  disappeared). `sub_821CE0A0` likewise on worker threads.
- `MCLA_PERF_SPLIT` / `MCLA_THREAD_CPU`: render-thread work ms/frame (target about -1.0 ms at
  720p), CP busy and `wrm_mem` (unchanged), fps and pacing p99 (the CP cap may hide an fps gain,
  M10-cpu-targets 3).
- Correctness: a normal race and garage/menu session without new crashes, missing textures or
  leaks (D3D objects are destroyed through the same guest path).

## 7. Risks

- Global-lock exclusion removed for these seven helpers: any host code that relied on
  `global_critical_region` to exclude them (none found; interrupt callbacks are guest code
  using the same atomics) would now race. The CAS keeps every update atomic.
- Breadcrumbs: crashes inside the native fast paths name the caller (the destroy path keeps
  the scope).
- Write-watched pages: the locked instructions write in every outcome, as the generated CAS
  did, so watch faults are unchanged.
- KI-055: a genuine guest crash during the close (after the flag) no longer gets the
  unexpected-exit snapshot; the native crash handler path is separate and unchanged.
- KI-058: reports exported before and after the fix differ for Unlocked sessions (60 vs 240);
  the field remains a recorded launch argument, not a measurement.
