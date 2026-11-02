# M10: read-back slot diagnostics fix and SDK v0.20.0.7 (2026-11-02)

Implementation by an Opus subagent; accepted by the primary agent. SDK commit `43577c8` (parent `8081f63`) proves by code path and randomized model tests that no guest copy can read a slot whose GPU copy was elided (the l63 `sporadic_read_prev_unfenced=56` were deferred resolves that copy nothing), splits the counter into unfenced (a real hazard, now counted only for resolves that copy) and unwritten, and fixes slot bookkeeping that was only updated while a diag was on. ctest 2006/2006.

**Published 2026-11-02:** tag `v0.20.0.7` = `43577c89c7f5409ce5b2501d5d183b0f00bcbe04` on `2FED/rexglue-sdk` `mcla/mcla-r-hotfixes` (fast-forward from v0.20.0.6 `483458d`; commits `62cd66d`, `141e13f`, `9d8ede4`, `561a737`, `e57fd88`, `8081f63`, `43577c8`; publication check: no private paths or data, single author, sdl3 unchanged `422c186`). Gates: SDK ctest 2006/2006; independent review of the watch diff ([review](M10-watch-review-e57fd88-20261101.md)); every switch except the counter split exercised in the owner l63 sessions ([l63 PC A/B](M10-l63-pc-ab-20261102.md)) with 0 failures.

---

# l65 SDK: read-back diag counts elided slots as unwritten (draft for the primary)

Opus subagent, 2026-11-02. SDK build, install and unit tests only. Nothing was launched (no
`mcla.exe`, no launcher); no save, profile or LocalAppData file touched; main checkout untouched.
Spark: not used (one function family read deterministically, build and tests; no log to triage).

**Status: implemented, unit-tested, local SDK commit; not pushed. No physical PASS.**

## 1. Was a real stale read possible? No.

The l63 "all" lines: `read=prev prev_age=-1 decision=deferred reason=some_interval`,
`sporadic_read_prev_unfenced=56` in the loading minute.

Code path (SDK 8081f63, `D3D12CommandProcessor::IssueCopy_ReadbackResolvePath` and
`include/rex/graphics/util/readback_slots.h`):

- `PlanReadbackResolve` sets `read_index` to the previous slot whenever the resolve is not
  own-slot (miss, guard miss, forced), **also for a deferred resolve** (`copies_now=false`). The
  diag field `read_previous_slot = read_index != write_index` therefore says `prev` for
  resolves that read nothing.
- The guest copy (`memcpy` from `rb.mapped_data[read_index]`) runs only when
  `should_copy = plan.copies_now`. A deferred resolve copies nothing.
- A copying, non-own resolve whose previous slot is `slot_elided` becomes `guard_miss` (wait,
  read the own, freshly recorded slot). `slot_elided[w]` is written on every resolve that
  reaches the recording step; resolves that leave earlier (buffer/descriptor/pipeline
  failures) do not flip `current_index`, so a slot created there stays the write slot of the
  next resolve and never becomes "previous" without being written or marked elided. Eviction
  removes both slots (next resolve is a miss). Forced resolves (`d3d12_readback_sync_*`)
  read the own slot and always record.
- In a constant `some` mode, the elision condition (`record = own || next_since + 1 >= 4`)
  means only deferred resolves point at an elided slot: for a steady key after a miss the
  counter-2 and counter-3 resolves both point at an elided slot, both deferred; the every-4th
  copy reads the slot recorded by counter 3.

So the 56 are deferred resolves pointing at elided slots; no guest bytes came from an
unwritten slot. Already covered by the existing random-sequence model (`elided_reads == 0`,
guest bytes identical with elision on/off); now also asserted per class (below).

Secondary diag defect found and fixed: `submission_written` was updated only while a diag
was on and only for recorded copies. An elided slot kept the fence of an **older** recorded
copy, so `prev_known_complete=1` could be shown for a slot whose last resolve was elided; a
diag switched on mid-run saw 0 (never written) for slots written while it was off.

## 2. Change (SDK `mcla/m10-watch` 43577c8, parent 8081f63; local only)

- `readback_slots.h`: `ClassifyReadbackPreviousSlot(slot_elided, submission_written,
  submission_completed)` -> `kComplete` / `kUnfenced` / `kUnwritten` (unwritten = last copy
  elided or none recorded). Pure, unit-tested.
- `submission_written` / `written_size` are kept on every resolve (two stores): the
  submission of a recorded copy, 0 after an elided copy; reset to 0 when the slot buffer is
  (re)created. Diagnostics only; the plan still uses `slot_elided`. Memexport read-back
  unchanged.
- Scope field `previous_slot_written`; lines gain `prev_written=0/1` (v1 `kind=resolve`, v2/v3
  `resolve2` and `composite`, placed before `prev_known_complete`); `prev_age=-1` for unwritten
  slots.
- Counters:
  - v2/v3 minute: new `sporadic_read_prev_unwritten` (read_index on a slot without a recorded
    copy of its last resolve; any decision). `sporadic_read_prev_unfenced` now counts only
    resolves that **copy to the guest** from a previous slot not known complete (the real
    hazard; it would also include an unwritten slot, which the plan prevents). Deferred
    resolves are no longer counted there.
  - v1 minute: `reads_prev_slot_unwritten` added; `reads_prev_slot_unfenced` same rule.
  - Comparability: earlier sessions' `*_unfenced` included deferred resolves; l63 base had 0,
    so base comparisons are unaffected. Expect `sporadic_read_prev_unwritten` > 0 and
    `sporadic_read_prev_unfenced` = 0 with elision on in `some` mode.
- Tests (`tests/unit/graphics/readback_slots_test.cpp`): classifier cases; the l63 sequence
  (miss, counter 1 elided, counter 2 points at the counter-1 elided slot = `kUnwritten`,
  deferred; counter 3 points at the counter-2 elided slot, deferred; every-4th copy reads the
  counter-3 recorded slot; a switch to fast after an elided resolve hits the guard). The
  random models now track per slot the last writer and `submission_written` and require
  `prev_unwritten_copied == 0` (elision on and off), no unwritten slot at all without elision,
  class == modelled contents (`class_mismatches == 0`), and > 1000 deferred-unwritten events
  across the 300 seeds.

## 3. (b) Other default-off counters with elision on

| Counter | Verdict |
|---|---|
| `MCLA_READBACK` recorded/elided/bytes | Correct (counted from the plan). Pre-existing, independent of elision: `CountPlan` runs before the early exits, so an aborted resolve is still counted as recorded/elided (`aborted` counts them). Not changed. |
| `gpu_readback_ms`, `gpu_blocks` | Timers only around recorded blocks; correct. |
| `MCLA_READBACK_SYNC` (targeted, forced) | Forced resolves are own-slot and always record; unaffected. |
| diag2 `composite_read_prev` | Composite resolves are forced unless a miss (own slot); unaffected (0 in l63). |
| diag2 tex tracker (`NoteReadbackDestination`) | Uses `copied`; unaffected. |
| v1/v2 `prev_known_complete`, `prev_age`, `prev_len_ok` | Fixed (section 1, secondary defect). `prev_len_ok=0` now also for unwritten slots. |

## 4. Build and tests

- Worktree `private/l62-sdk-wt` (branch `mcla/m10-watch`), build `private/l65-sdk-b`, output
  `private/l65-sdk-o`, install `private/l65-sdk-install`; scripts and logs
  `private/l65-sdk-work` (`l65_sdk_build.ps1` = the l61 script with l65 paths, D3D12 only,
  Vulkan off, Ninja Multi-Config, clang 20.1.8, VS 2022 Build Tools, `-march=x86-64-v3`,
  prefix map `l62-sdk-wt=./third_party/rexglue-sdk`). Note: `cmake --install` registers
  rexglue in the CMake user package registry (as l61/l62 did).
- Installed `bin/rexgpu-xenos.dll` sha256
  `1eb954258bac7d7e42c40313652f6187d95614bf2c2c8dc6c276ad6666e3e069` (build of the committed
  tree, 20:41); `lib/rexruntime.lib` `03990569ea1e2d84c370216d1d7a39de3cb2ff2d4f457dc20da9beeb36559822`.
- ctest Release **2006/2006** (baseline 2004 + 2 new cases; the pre-existing BitStream
  skips unchanged; no manifest race rerun needed). Tagged runs all pass, `[readback-slots]`
  10 cases / 765,682 assertions.
- Warnings: in the changed files the same 8 pre-existing `command_processor.cpp` warnings as
  l62 (line numbers shifted), none new; test file none.
- clang-format clean on changed lines (remaining diffs in the files are pre-existing lines).

## 5. Task 2: SDK cvars, defaults and host-default recommendation for volunteer builds

All listed cvars are hot-reload except `d3d12_readback_sync_kick_draws` and `d3d12_perf_split`
(plain range cvars, read live). Host currently applies (in `OnPostLoadGraphicsPlugin`): dev
overrides, texture budget, perf split, livery diag, readback sync, then `readback_resolve=some`
unless the user set it.

| Cvar | SDK default | Recommendation (volunteer, set by host) | Evidence / cost |
|---|---|---|---|
| `d3d12_readback_sync_composite` | false | **ON** (host `mcla_readback_sync_composite`, already the KI-047 volunteer fix) | Prerequisite of targeted; full drain costs ~15% at 1440p, ~22% at 720p without targeted (l63 sec. 2). |
| `d3d12_readback_sync_targeted` | false | **ON** | Forced wait 720p 5.1 -> 1.7 ms/f, 1440p 14.9 -> 7.7-9.0 ms/f; `forced_wait_failed=0`, `fallbacks_to_full=0`. No effect unless composite is on; falls back to the full wait when no submission is open. |
| `d3d12_readback_sync_kick_draws` | 512 | keep **512** (no host write) | Only active with targeted + composite. Kicks 0.36-1.04 ms/f CP, part of the +1-3 ms/f CP work of "all"; tuning (e.g. 1024) is a later A/B, not a default change now. |
| `d3d12_readback_elide_unread` | false | **ON** | GPU read-back 4.0 -> 1.0 ms/f (1440p), 3.46 -> 0.88 (720p); CPU bytes identical; `guard_misses=0`. Effective only in `readback_resolve=some` (host default); harmless in fast/full; guard covers a runtime switch to fast. l65 removes the only misreport. |
| `d3d12_readback_gpu_timing` | **true** | **OFF** for volunteers (host write needed, SDK default is on); ON in owner measurement builds | Per recorded block: one `SubmitBarriers`, two timestamp `EndQuery`; one `ResolveQueryData` per submission. Cost not isolated (l63 both arms ran with it); ~15 blocks/f with elision, 56 without. Only feeds `gpu_readback_ms`. |
| `d3d12_wrm_event_wake` | false | **ON** | wrm wait 1.9-2.4 -> 1.0-1.4 ms/f (1440p), 1.93 -> 0.96 (720p stat); CP kernel time up 4 -> 6-8% / 8 -> 12-13%. Only with vsync on. PC evidence only. |
| `d3d12_replay_batched_wake` | false | **ON** | Replay worker CPU 60 -> 23% (720p), 40 -> 16% (1440p); replay +0.1-0.8 ms/f. Read at submission start. Main value on the Deck. |
| `memory_watch_census` | **true** | per plan **OFF** -> host must write false (rexruntime cvar, dev-override phase `kStartup`; set at startup, not graphics-plugin load) | Cost <= 0.05-0.1 ms/f; one `MCLA_WATCH` line/min. Alternative: keep ON if volunteer Deck logs should feed the watch analysis (cheap). |
| `memory_watch_probes` | false | **OFF** (no write) | Active probes at F10 (1000 protect round trips etc.). |
| `memory_watch_disarm_bridge_safe_gaps` | false | **OFF** (no write) | Net negative on PC: disarm ms/f up 0.43 -> 0.62-0.70 (720p), per call 1.0 -> 3.3 us. |
| `shared_memory_upload_ahead_pages` | 0 | **0** (no write) | Net negative: +~20 MB/frame upload, upload ms/f 2.58 -> 4.08 (720p), render-thread write faults +60%. |
| `d3d12_livery_diag` (v1) | false | **OFF** | Superseded by v2/v3. |
| `d3d12_livery_diag2` (v2/v3) | false (host default via `MCLA_LIVERY_DIAG2_DEFAULT`) | **ON** while the KI-047 police-livery confirmation is owed, if ~2.4 MB/session of log is acceptable | ~5,400-6,000 lines / 2.4 MB per 9-min session (~10 lines/s, rate limited); tex tracker ~135k loads/min bookkeeping, not separately measured (bounded inside the <= 0.6-1.1 ms/f residual with scene effects). There is **no reduced mode** in the SDK: a "counters only" level (minute lines, no per-resolve/tex lines) would need an SDK change. |
| `d3d12_perf_split` | 0 | **1** (host build option `MCLA_PERF_SPLIT_DEFAULT`, `mcla_perf_split.cpp`, already user-set aware) | Mode 1: +0.7-8.8 ns per draw in the micro-benchmark = 0.1-0.8% of CP time; l53 mode 0 vs 1 within run-to-run spread. |

SDK/host notes for making host-set defaults safe:

1. No SDK change is required for targeted/elide/WRM/replay: each is hot-reload, self-gated
   and has a safe fallback (targeted -> full wait; elide only in `some`, guard on a mode
   switch).
2. Two SDK defaults are already ON (`memory_watch_census`, `d3d12_readback_gpu_timing`):
   turning them off needs an explicit host write; census must be written at startup.
3. Host defaults must not overwrite explicit user values or dev-override A/B arms: apply
   them only when `rex::cvar::HasNonDefaultValue(name)` is false (as `readback_resolve`
   does), and after `dev_overrides::OnGraphicsPluginLoaded()`.
4. `d3d12_readback_elide_unread` has no effect if a user sets `readback_resolve=fast/full`;
   that is safe, but the KI-047 fix and the sync counters assume `some`.
5. Reduced livery diag would need a new SDK level cvar; not done here.
