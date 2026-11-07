# l78 SDK: defaults flipped (steady exemption + deferred fences on), fence/CP-lead instrumentation, fence_defer_min_gap_us

Implementation by an Opus subagent on 2026-11-07 as SDK commits `636927c` (defaults) and `d61c597` (instrumentation + min-gap cvar) on local branch `mcla/m11-defaults` (parent `590c484`); accepted by the primary agent as code: ctest 2137/2137 (4 skipped), 0 new warnings. Installed as `private/l78-sdk-install` 0.20.0.14-dev.gd61c597: rexruntime.dll 367e194d28c153d5473d927f52e1095a3498fa38693d7c145dc30fb69c62fc6c, rexgpu-xenos.dll 77186d98548fc238da6701fc54ca5f199269325a76ca8b5918b70e1947d1f5c3, TracyClient.dll 34a05c5e6f4cd0a7de743a05028274ea2c00f51f6392427b2a6365cf712da730. Note: the `subm_primary_end` bucket (d3d12_submit_on_primary_buffer_end) is the suspected source of the unexplained part of the +40 % submissions; the min-gap cvar does not cover it.

---

# M11 l78-sdk-defaults: draft evidence notes (Opus subagent, 2026-11-07)

SDK worktree `private/l78-sdk-defaults-wt`, branch `mcla/m11-defaults` from `mcla/m11-fence` 590c484 (thirdparty
submodule folders copied from l77-sdk-fence-wt). Build `private/l78-sdk-defaults-b`, output
`private/l78-sdk-defaults-o` (Release, Ninja Multi-Config, clang 20.1.8, VS 2022 BuildTools, flags as l77). Scripts:
`%TEMP%\claude\C--BDU-MCLA-Recomp\l78-defaults-scripts` (build.ps1 / test.ps1 / install.ps1 adapted from
l77-fence-scripts; edit scripts ed_a.py, ed_bc.py + submission_stats.h, test_tail.cpp.txt). Install prefix
`private/l78-sdk-install` (new).

Commits (author Bolodymyr <vovan00282@gmail.com>, not pushed):
- `636927c` gpu: M11-007e defaults: steady exemption and deferred fence writes on
- `d61c597` gpu: l78 fence/CP-lead instrumentation and d3d12_fence_defer_min_gap_us

STATUS: built, ctest green, installed. Version `0.20.0.14-dev.gd61c597`. SHA-256 (installed = build output):
- rexruntime.dll   367e194d28c153d5473d927f52e1095a3498fa38693d7c145dc30fb69c62fc6c
- rexgpu-xenos.dll 77186d98548fc238da6701fc54ca5f199269325a76ca8b5918b70e1947d1f5c3
- TracyClient.dll  34a05c5e6f4cd0a7de743a05028274ea2c00f51f6392427b2a6365cf712da730
The install (like l75-l77) re-registers rexglue in the CMake user package registry (-> l78-sdk-install).
Note: the first configure/build attempt (19:10) was killed on the coordinator's request (owner test session);
the partial build dir was removed and everything was redone after "resume".

## A. Defaults (636927c)

- `d3d12_readback_sync_steady_exempt` false -> **true**; `d3d12_fence_write_deferred` false -> **true**. Unchanged:
  `d3d12_readback_sync_steady_fast` true, `d3d12_readback_sync_steady_fast_wait_ms` 20, `xma_frame_walk_fix` true (l77).
- Help texts: "On by default since l78; A/B evidence M11-007e. false: ... (byte-identical with the pre-l78
  default)". Header comments (readback_sync.h steady block and ReadbackSyncSteadyReport, deferred_fence_writes.h
  top and ShouldDeferFenceWrite): the "unchanged / byte-identical when off" guarantees now refer to the switch
  set to false.
- No test asserted the old defaults (both cvars live in the rexgpu-xenos plugin, which unit_tests does not link,
  so a registry default test like the l77 xma one is not possible there).
- Host consequence: arms that relied on the defaults being off (base / control arms) must now write
  `d3d12_readback_sync_steady_exempt=false` and/or `d3d12_fence_write_deferred=false` explicitly. The always-on
  readback minute line now carries the fence_* fields by default, MCLA_READBACK_SYNC the steady* fields.

## B. Instrumentation (appended to the `MCLA readback` minute/mark line; existing fields unchanged)

1. Fence lateness histogram (inside the fence_* group, after `fence_queue_now`; per minute, mark peeks), over
   QUEUED writes at their store (immediate stores not counted):
   `fence_late_f0 fence_late_f1 fence_late_f2p` (frames between parse and store: same frame / 1 / 2+) and
   `fence_late_lt5ms fence_late_5_20ms fence_late_20_50ms fence_late_gt50ms` ([0,5) [5,20) [20,50] (50,inf) ms).
   Then `fence_min_gap_us` (cvar) and `fence_early_coalesced` (see C).
2. CP-lead gauge, sampled once per frame at the frame-closing EndSubmission (after the swap submission is
   signalled, GetCompletedValue non-blocking): `cp_lead_subs_*` = submissions ended but not completed;
   `cp_lead_frames_*` = frames (of the last 8) whose closing submission is not completed, this frame included
   (so 1 = the GPU is still on the frame just closed, 0 = the GPU already finished it). Fields
   `cp_lead_samples cp_lead_subs_min/avg/max cp_lead_frames_min/avg/max` per minute (mark line peeks the minute so
   far) plus `cp_lead_subs_now cp_lead_frames_now` = the latest sample (at a mark: the F10 frame).
3. Submissions by end reason: `subm subm_frame_end subm_draw_full subm_idle subm_fence_early subm_forced_wait
   subm_primary_end subm_other`, counted at the ExecuteCommandLists/Signal of each submission. Mapping:
   - frame_end: EndSubmission(true) at the swap;
   - draw_full: `d3d12_readback_sync_kick_draws` early execution (open submission reached the draw threshold);
   - idle: deferred-fence early submit from the idle loop or a waiting WAIT_REG_MEM poll;
   - fence_early: deferred-fence early submit at a draw boundary (GPU out of work);
   - forced_wait: AwaitReadbackCopySubmission (targeted read-back) and CheckSubmissionFence on the open
     submission (forced read-back waits, queue drains, reclamation);
   - primary_end: `d3d12_submit_on_primary_buffer_end` (default true; the CP caught up with the ring). Added as
     its own bucket because it is the most likely source of the unexplained part of the +40 % submissions
     (with deferred fences the render thread waits on the throttle more often, so the CP drains the ring more
     often); NOT covered by the min-gap coalescing below;
   - other: read-back ensure request, host occlusion query disable, unlabelled.
   `fence_early_submits` = subm_idle + subm_fence_early (modulo EndSubmission failures).
   The subm/cp_lead fields are always appended (also with the fence off), so a fence-off control arm gets the
   same gauge.

## C. `d3d12_fence_defer_min_gap_us` (int, default 0, range 0-100000, hot reload)

- Pure decision `ShouldCoalesceFenceEarlySubmit(min_gap_us, submission_begin_us, now_us)` in
  deferred_fence_writes.h: skip when min_gap > 0, the begin time is known and now - begin < min_gap (clock behind
  = elapsed 0). `submission_begin_us_` is taken when BeginSubmission opens a submission.
- Applied in ServiceDeferredFenceWrites right before the early EndSubmission (after the pipeline-creation and
  occlusion-grace checks), for idle, WRM and draw-boundary early submits. Skipped -> `fence_early_coalesced`++
  and return; the next poll point retries. Note the counter counts skipped attempts (the idle loop polls every
  ~1 ms while the head sits in the open submission), not distinct submissions.
- Deadlock: unchanged bound. With the head in the open submission the idle wait does a 1 ms ring wait and
  re-services; once the submission is min_gap old the submit happens. Extra fence latency <= min_gap + one poll.
- 0 = l77 behaviour exactly (decision returns false before reading anything else). The l78 Deck arm: 2000.

## Tests

- Unit tests added: deferred_fence_writes_test "lateness histogram buckets", "min-gap coalescing of early
  submits" (incl. the bounded-retry model); the existing field-format test extended to the new fields; new
  submission_stats_test.cpp (3 cases: reasons, in-flight helpers/ring, gauge + exact line fields).
- Baseline 590c484: 2132 passed, 4 skipped. Result: 2141 total, 2137 passed, 4 skipped (BitStream), 0 failed (+5 new tests, all passed).
- clang-format-diff on all changed lines: clean; new files fully clang-formatted; ASCII; LF.
- Warnings: full build warning set (line/col and worktree path stripped) identical to the l77 full build: 222 unique, 0 new (500 raw warning lines in both logs).

## Spark

No Spark job: small C++ change set with deterministic gates (build, ctest, clang-format); no bounded extraction
or log triage whose validation would cost less than doing it.
