# M11-007c: bounded fence wait for exempt keys, no unfenced copy, F10 ring 120 frames; host read-back range API (SDK)

Implementation by an Opus subagent on 2026-11-07 as SDK commits `ca2e332` (bounded wait, `d3d12_readback_sync_steady_fast_wait_ms` default 20; 0 = the l75 cadence as an A/B control) and `1e65cc8` (`rex::gpu::ReadbackSyncQueryRange` / `ReadbackSyncEnsureRange` / `SetReadbackRangeProvider`, exported from rexruntime.dll, header include/rex/graphics/readback_range.h, pending-copy table, CP OnIdlePoll hook) on local branch `mcla/m11-steady3` (parent `bd9314f`); accepted by the primary agent as code: ctest 2112/2112 (4 skipped), no new warnings. Merged into the l76 SDK together with `mcla/m11-xma-walk`.

---

# M11-007c l76-sdk-steady3: draft evidence notes (Opus subagent, 2026-11-07)

SDK worktree `private/l76-sdk-steady3-wt`, branch `mcla/m11-steady3` from `mcla/m11-steady2` bd9314f.
Build `private/l76-sdk-steady3-b`, output `private/l76-sdk-steady3-o` (Release, Ninja Multi-Config, clang 20.1.8,
VS 2022 BuildTools, flags as l75). Not installed. Scripts: `%TEMP%\claude\C--BDU-MCLA-Recomp\l76-steady3-scripts`
(build.ps1 / test.ps1 adapted from l75-steady2-scripts; the install prefix points at a non-existent
`l76-sdk-steady3-noinstall`, install was never run).

Commits (author Bolodymyr <vovan00282@gmail.com>, not pushed):
- `ca2e332` gpu: M11-007c steady fast delivery: bounded previous-slot fence wait, no unfenced copy for exempt keys,
  200-line resolve flight ring, skip episodes
- `1e65cc8` gpu: M11-007c host read-back range API: rex::gpu::ReadbackSyncQueryRange / ReadbackSyncEnsureRange

## Part 1 (ca2e332): bounded wait

New cvar `d3d12_readback_sync_steady_fast_wait_ms` (int, default 20, range 0-1000, hot reload).
Pure decision in `readback_slots.h` (`PlanReadbackSteadyDelivery`, `ResolveReadbackSteadyDelivery`,
`NoteReadbackSteadyShadowCadence`). For an exempt resolve in some mode with `d3d12_readback_sync_steady_fast` on:

| previous slot (polled fence) | wait_ms > 0 | wait_ms = 0 |
|---|---|---|
| complete, length ok | fast copy (1 frame old) | fast copy |
| written, unfenced, its submission already executed (< submission_current_) | bounded wait on that fence value; reached -> fast copy; timeout -> force own slot | l75 cadence (every-4th unfenced copy) |
| unwritten / elided / other length / recorded into the still open submission | force own slot | l75 cadence |

- Force = `force_unless_miss = true`, i.e. the existing rule-c path (targeted wait for the own copy, read own slot,
  counted as forced_c). This is the pre-exemption behaviour.
- The wait is fence-only: `AwaitReadbackSubmissionBounded` loops `SetEventOnCompletion(P, dedicated event)` +
  `WaitForSingleObject(remaining)` + `GetCompletedValue` re-poll; no `EndSubmission`, no queue-operation wait,
  `submission_completed_` not updated (reclamation stays with `CheckSubmissionFence`). A dedicated auto-reset event
  (`readback_steady_wait_event_`) is used because a timed-out registration signals its event later; on
  `fence_completion_event_` that late signal would end a later INFINITE wait early. Spurious wakes are absorbed by
  the re-poll. Timed in PerfRegion kReadbackWait and added to `readback_wait_ns`.
- CP lead bound: the wait returns when the GPU has finished the submission holding the frame N-1 copy (draw ~34 of
  N-1), so at frame N draw 34 the CP is at most ~1 frame (plus the rest of that submission) ahead of the GPU. With
  the GPU as the bottleneck the queued rest of N-1 and N so far keeps it busy; no starvation.
- With wait_ms > 0 an exempt key never gets the unfenced every-4th copy (it is fast, waited-fast or forced; the some
  counter resets every resolve). wait_ms = 0 keeps the exact l75 path as the A/B control.
- Non-exempt keys: never reach the decision; unchanged `some` cadence (unit-tested bit for bit).
- Minute/mark line (`MCLA_READBACK_SYNC`, after the steady filter fields, only while the exemption is on or something
  counted): `steady_fast_wait_ms steady_fast_waits steady_fast_wait_ms_total steady_fast_wait_ms_max
  steady_fast_wait_timeouts steady_fast_forced steady_unfenced_avoided steady_skip_episodes steady_skip_longest`.
  - `steady_fast_forced`: timeouts + unwaitable slots.
  - `steady_unfenced_avoided`: a per-key shadow of the l75 cadence (reset by a fast copy, advanced by each
    not-complete-at-poll resolve); counted when l75 would have made its unfenced 4th copy here and this path waited
    or forced instead. Not counted with wait_ms 0.
  - Skip episode = run of consecutive exempt resolves of a key not complete at the poll (waited, forced or cadence);
    episodes started in the window and the longest run seen (a run spanning a minute line counts in both minutes'
    longest, once as an episode).
  - `steady_fast_skipped` (l75 field) now counts only cadence resolves (wait_ms 0).
- resolve2/composite diag2 lines of exempt resolves gain `steady_delivery=<fast|wait|force|cadence>
  steady_wait_us= steady_wait_timeout=` (other lines unchanged). `prev_known_complete` keeps its meaning (by
  `submission_completed_`), so a waited delivery can show 0 there; use `steady_delivery`.
- F10 flight ring: `DiagLineRing` is now `BasicDiagLineRing<N>`; resolve ring 64 -> 200 lines
  (`kLiveryDiag2ResolveFlightLines`; l75 rings were 64 lines = 37-42 frames, so ~120 frames, ~3 s at 40 fps); atlas
  ring stays 64. Memory: 200 std::string objects (~6 KB inline) + line text (~0.3-0.6 KB each, <= ~120 KB), filled
  only while diag2 is on. The diag2 `start` line's `flight_lines` now reports 200. F10 dump writes up to 200 lines.

## Part 2 (1e65cc8): host read-back range API (coordinator addition)

- Public header `include/rex/graphics/readback_range.h`, implementation `src/core/readback_range.cpp` (rexcore ->
  rexruntime.dll, WINDOWS_EXPORT_ALL_SYMBOLS, same route as `rex::graphics::MarkPerfSplit`):
  - `bool rex::gpu::ReadbackSyncQueryRange(uint32_t guest_phys, uint32_t len, ReadbackRangeInfo* out)`:
    key_base/key_length/slot_size/width/height/format, exempt, pending, unsubmitted, copy_age_frames (frames since
    the resolve whose bytes guest RAM holds; UINT32_MAX none), resolve_age_frames. Newest overlapping key.
  - `ReadbackSyncEnsureResult rex::gpu::ReadbackSyncEnsureRange(uint32_t guest_phys, uint32_t len, uint32_t
    timeout_ms)` -> {waited_ms, waited_us, pending, delivered, timed_out}.
  - `IReadbackRangeProvider` + `SetReadbackRangeProvider` (shared_mutex: calls shared, (un)registration exclusive,
    so removal waits at most one caller timeout). D3D12 CP registers at the end of SetupContext and removes it first
    in ShutdownContext (before fences/slots go).
- Export check (llvm-readobj --coff-exports on l76-sdk-steady3-o/Release/rexruntime.dll):
  `?ReadbackSyncEnsureRange@gpu@rex@@YA?AUReadbackSyncEnsureResult@12@III@Z`,
  `?ReadbackSyncQueryRange@gpu@rex@@YA_NIIPEAUReadbackRangeInfo@12@@Z`,
  `?SetReadbackRangeProvider@gpu@rex@@YAXPEAVIReadbackRangeProvider@12@@Z`; rexgpu-xenos.dll imports
  SetReadbackRangeProvider from rexruntime (single global, no duplicated rexcore state).
- `readback_pending.h` `ReadbackPendingTable` (short std::mutex): per key the newest recorded GPU copy not yet in
  guest RAM (slot, submission, mapped pointer, generation, resolve frame) + delivered generation/frame.
  CP hooks in IssueCopy_ReadbackResolvePath: `BeginResolve(key, write_index)` before the own slot is reallocated or
  recorded (drops a pending copy in it), the CP's own guest memcpy runs inside `DeliverByCommandProcessor` (marks a
  pending copy in the read slot delivered), `EndResolve` after the copy step (recorded copy becomes pending, or is
  delivered when the resolve read its own slot); eviction erases before Release; shutdown/clear clears.
  `SetNextSubmission` after every `Signal` in EndSubmission.
- Ensure (caller thread): find newest overlapping pending copy; `PlanReadbackEnsure`: deliver now / wait / request
  submit then wait. Request = `readback_ensure_submit_request_` atomic; the CP honours it
  (`HonourReadbackEnsureSubmitRequest`: EndSubmission if open, no occlusion query active and
  CanEndSubmissionImmediately; else keeps it) before each draw and in the idle loop via the new virtual
  `CommandProcessor::OnIdlePoll()` (called each idle-spin iteration). Wait on the submission fence with a
  thread_local auto-reset event (never closed; late signals absorbed by re-poll), bounded by timeout_ms.
  Then `DeliverPending` under the lock: copies only if same generation and undelivered (kAlreadyDelivered = CP or
  another caller delivered it; kSuperseded = retry newer within the timeout; kGone = evicted); up to 8 attempts
  (newest first, then next overlapping key).
- Thread safety: the slot cannot be rewritten (would need BeginResolve, which needs the lock) or released (Erase
  first) during the caller's memcpy; CP and caller copies are serialised and only ever deliver the newest pending
  copy, so guest RAM never regresses to an older resolve.
- Deadlock analysis: the caller may hold game locks; the CP never waits for a caller (lock held only for map
  updates or one slot memcpy, no wait inside); the caller waits only on the GPU fence with a timeout; an unsubmitted
  copy only sets a flag, honoured by the CP before a draw or while idle (so it progresses even when the ring is empty
  because the caller is blocked); if the CP itself is blocked on the guest, the caller times out and the request
  stays for later.
- Counters (minute/mark line, appended only when a call was made): `ensure_calls ensure_pending ensure_delivered
  ensure_delivered_by_cp ensure_timeouts ensure_wait_ms_total ensure_wait_ms_max ensure_submit_requests
  ensure_submits`. `kind=ensure` INFO line for the first 4 calls per key: key_base key_len base len unsubmitted
  timeout_ms waited_us delivered by_cp timed_out.
- Not done here: the host hook on sub_82421CA0 (host repo); memexport read-backs are not covered.

## Tests

- Baseline bd9314f: 2100 passed, 4 skipped. After ca2e332: 2110 total (2106 passed, 4 skipped; +6 tests). After
  1e65cc8: 2116 total (2112 passed, 4 skipped; +6 tests), 0 failed (ctest -C Release -j 8).
- New: readback_slots_test (steady delivery decision; plan never reads an unfenced slot; non-exempt unchanged;
  shadow cadence), readback_sync_test (wait counters/report/skip episodes; 200-line ring), readback_pending_test
  (overlap, ensure step, table lifecycle incl. superseded/already/gone, newest overlap and line budget, counters
  report, provider routing).
- Warnings: the set of warning messages (line numbers stripped) of the incremental builds is a subset of the full
  base build's set (no new warnings). clang-format-diff on all changed lines: clean; new files fully clang-formatted.
  All changed/new sources ASCII, LF.
- Every build/ctest was preceded by wait-for-idle-pc.ps1 (IDLE each time). Game/launcher never launched.

## Spark

No Spark job: the work was implementation in a single C++ file set with deterministic gates; no bounded
extraction/log triage task whose validation would be cheaper than doing it.

## Interpretation notes for the primary

- Requirement (2) "remove the unfenced copy entirely" is implemented for wait_ms > 0 (the default); wait_ms 0 was
  kept as the exact l75 behaviour because (1) asked for "0 = old behaviour". Say if 0 should instead mean
  "force when not complete".
- Owner test arms: steady with wait 20 (default) vs `d3d12_readback_sync_steady_fast_wait_ms=0` (l75) if a control
  is wanted.
