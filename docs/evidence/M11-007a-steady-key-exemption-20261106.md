# M11-007a: steady-key exemption for the KI-047 read-back sync (SDK)

Implementation by an Opus subagent on 2026-11-06 as SDK commit `e412c5c` on local branch `mcla/m11-steady-key` (parent `ae35ae3` = M11-016 on `d08813b`); accepted by the primary agent as code: ctest 2067/2067 (4 skipped), no new warnings, byte-identical behaviour with the cvar off. Ships in l73 behind `d3d12_readback_sync_steady_exempt` (default false) with the host dev key `readback_steady_exempt`; owner A/B + the KI-047 livery routine. No read trap (the SDK has no read-access watch).

---

# M11-007a: KI-047 rule c steady-key exemption (l73 SDK, draft)

Draft by the implementation subagent, 2026-11-06. Not accepted, not owner-tested.

- SDK worktree `private/l73-sdk-steady-wt`, branch `mcla/m11-steady-key`, parent `ae35ae3`
  (`mcla/m11-fetch-identity`, M11-016), commit `e412c5c` (local only, not pushed).
- Build `private/l73-sdk-steady-b` (Ninja Multi-Config, Release, same flags as l72-sdk-fetch-b,
  `-ffile-prefix-map` to `./third_party/rexglue-sdk`), output `private/l73-sdk-steady-o`, no install.
- Design: `docs/evidence/M11-007-steady-key-design-20261106.md` (step 1); priorities:
  `docs/M11-priorities.md` "M11-007a steady-key exemption".

## Change

| file | change |
|---|---|
| `include/rex/graphics/util/readback_sync.h` | `kReadbackSyncSteadyMinFrames = 30` (:201); `NextReadbackConsecutiveFrames` (:206) run update; `IsReadbackSyncSteadyExempt` (:221) = rule c && !rule a && !rule b && run >= 30; `ReadbackSyncRules::steady_exempt` and `forces()` (:152), `any()` unchanged; `ShouldForceReadbackSync` uses `forces()` (:187); `ReadbackSyncSteadyEndReason` (:227); `ReadbackSyncSteadyKeyEvent` + `FormatReadbackSyncSteadyKeyEvent` (:262); `ReadbackSyncCounters` gains `steady_exempt`, `steady_exempt_keys`, `steady_max_run`, `CountSteadyRun`, `CountSteadyExempt`; `ReadbackSyncSteadyReport` (:463) |
| `include/rex/graphics/d3d12/command_processor.h` | `ReadbackBuffer::consecutive_frames`, `steady_exempt`, `steady_exempt_epoch` (:506); `ReadbackSyncSteadyExemptEnabled()`, `ResetReadbackSyncMinute()`; state `readback_sync_steady_was_enabled_`, `readback_sync_minute_epoch_`, `readback_sync_steady_lines_` (PacedLineBudget 2/s, burst 16) (:865) |
| `src/graphics/d3d12/command_processor.cpp` | cvar `d3d12_readback_sync_steady_exempt` (bool, default false, hot reload) (:139); run computed for every read-back resolve before the rules see the live keys (:4462) and stored next to `last_used_frame` (:4496); exemption applied after `EvaluateReadbackSyncRules` only when `d3d12_readback_sync_composite` and the new cvar are on (:4490); `kind=steady_key` transition line; counting in the matched branch (:4787-4791); minute/mark line suffix (:3043); `ResetReadbackSyncMinute` replaces the two `readback_sync_minute_ = {}` (:3016, :5414); per-frame toggle check (:5488) |
| `tests/unit/graphics/readback_sync_test.cpp` | 7 new cases tagged `[readback-steady]` |

Line numbers are of the l73 commit.

## Rule

A non-miss read-back resolve in a delayed mode (fast/some) with `d3d12_readback_sync_composite`
on is exempt from rule c (not forced) when, at this resolve:

- rule c (composite shape) matches, rule a (shared range) and rule b (sporadic) do not, and
- its key (address, length) was resolved in each of the last 30 frames, this one included
  (`consecutive_frames >= 30`; several resolves in one frame keep the run, a frame without a
  resolve resets it to 1, a new or evicted key starts at 1).

The exempt resolve keeps the delayed behaviour of its mode (`some`: copied every 4th resolve from
the previous slot, l54 behaviour for this key). `matched`, `matched_c` and the rule names still
count it (any() is unchanged); only `forced*` drops. With the cvar off `steady_exempt` is always
false and `forces() == any()`, so planning, forcing and every existing log line are unchanged (the
steady fields are not appended and no steady line is written; the only extra work is one
`readback_buffers_` lookup per resolve and one cvar read).

Margins: the livery keys (`050A0000` chain, `054A0000`, `055A0000`, `055E0000`, `055F0000`) were
seen running at most 3 consecutive frames (Deck logs 2026-11-06) and match rules a/b anyway; the
threshold is 10x that run. A key re-used by a one-shot composite after a steady run is forced on its
first non-consecutive resolve (run back to 1) and by rule a if another live key of a different
length overlaps it. The first 29 frames of the steady key are still forced after every gap
(about 29 x 6 ms once per session start, scene change or gap).

## Logs

- `MCLA_READBACK_SYNC v=1 kind=steady_start|steady_stop frame=N steady_min_frames=30` when the
  cvar changes (checked once per frame and on every evaluated resolve).
- Suffix on the `kind=minute` and `kind=mark` lines while the cvar is on (or something was exempt
  in the minute): ` steady=<0|1> steady_min_frames=30 steady_exempt=<resolves not forced>
  steady_exempt_keys=<distinct exempt keys this minute> steady_max_run=<longest run of any non-miss
  rule c resolve this minute>`. `steady_max_run` is also accumulated with the cvar off but only
  printed with it on.
- `MCLA_READBACK_SYNC v=1 kind=steady_key event=exempt|end reason=<run|gap|rule_a|rule_b|shape|
  disabled> frame= draw= base=%08X len= size=WxH fmt= src_base= run= min_frames=30 suppressed=`,
  when a key becomes exempt (run = 30, normally) and when an exempt key's next evaluated resolve
  is not exempt (run = its last exempt run). Paced 2 lines/s, burst 16; `suppressed` counts lines
  dropped since the last printed one. A key that simply stops being resolved logs nothing until
  its next resolve.

Expected on the l72 PC South Central Tour run with the cvar on: one `event=exempt` line for
`base=03702000 len=262144 size=256x256 fmt=6 src_base=0` about 30 frames after the first
resolve and after every scene change/gap; `steady_exempt` ~1 per frame (~2,000/min at 36 fps),
`steady_exempt_keys=1`, `matched_c` unchanged, `forced_c` and `forced` down by about one per
frame, `forced_a`/`forced_b` unchanged.

## Optional read trap: not implemented

The SDK has no read-access watch: `PhysicalHeap::EnableAccessCallbacks` asserts
`!enable_data_providers` (`src/system/xmemory.cpp` ~1905, "TODO(Triang3l): Implement data
providers") and the watch callbacks are write invalidation only. Arming NoAccess on
`03702000`'s 64 pages would also be hit by host code that legitimately touches those pages every
frame (the resolve memcpy into guest memory and the shared-memory upload of the same range for the
GPU's `tex_load base=03702000`), which needs the global-lock/fault plumbing of M11-007 step 2.
So the sticky "touched -> force again" rule is not built; the remaining risk is covered by the
A/B below.

## Tests

`[readback-steady]`: run update (new key, same frame, next frame, gap, backwards, saturation);
predicate for runs 0..31 with rule c alone (exempt from 30), a+c, b+c, a only, none (never);
`forces()`/`ShouldForceReadbackSync` drop only the rule c force; a frame sequence (steady key
exempt exactly at its 30th consecutive frame, second resolve in a frame stays exempt, a one-frame
gap forces again; a 3-frame livery burst never exempt; a 101-frame run with a rule-a overlap
forced); end reasons; counter report formatting (empty when off and nothing exempt, field values);
the steady_key line format.

ctest (Release, l73-sdk-steady-b): 2067 passed, 4 skipped, 0 failed of 2071 (first -j 8 run had one unrelated flake, unit.Manifest: missing project name fails, `remove_all` on the shared %TEMP%manifest_test dir in use by a parallel test; passed on rerun and in a clean full rerun) (baseline ae35ae3: 2060 passed, 4 skipped).
Warnings in changed files: none new (normalized warning set identical to the l72-sdk-fetch-b full build, 500 lines). clang-format: changed lines clean (clang-format-diff empty).

## Expected saving

From the design (l72 PC 1080p South Central Tour, Deck 2026-11-06 logs): PC readback_wait
6.46 -> ~0.1 ms/frame, CP busy 26.7 -> ~20.4 ms/frame, GPU-bound at ~22.4 ms -> roughly
36 -> 42-44 fps; Deck -7.1 ms/frame of a ~49 ms CP frame (~14 %). Kicks remain (PC 0.69, Deck
1.4 ms/frame) and now feed an idle GPU; measure `kick_draws=0` as a separate arm.

## Risks and how the A/B detects them

- A steady key that is in fact a CPU-read composite would regress KI-047 for that key (stale
  content from the previous slot, the l54 mechanism). For `03702000` this is the pre-l55
  behaviour, which never showed a defect, but a CPU reader is not excluded (no read detection).
  Detection: the KI-047 livery routine in both arms (police close-up cold/warm, garage vinyl apply,
  traffic spawns, garage car switch, title/menu car views, KI-038/032 signs, KI-056 album preview)
  plus the counters: `steady_exempt_keys` must be 1 (or the named step-0 set), every
  `kind=steady_key` line must name `03702000`/256x256; a livery key appearing there, or
  `forced_a`/`forced_b` changing between arms, is a failure.
- A livery key that is resolved every frame for 30+ frames while matching only rule c (e.g. a
  paused garage view that keeps compositing) would be exempt; it is visible as a second key in the
  steady_key lines.
- Dev override key `readback_steady_exempt` (host dev-override file) is not part of this SDK
  change; the host side must map it to `d3d12_readback_sync_steady_exempt`.
