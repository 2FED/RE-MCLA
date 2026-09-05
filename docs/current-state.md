# Current state / AI handoff

Updated: 2026-09-05. Read this first, then relevant README-AI sections.
Replace stale facts here; actual Git/artifacts and accepted evidence win.
Detailed current findings: [CPU attribution](evidence/M7-016-deck-cpu-attribution.md).

## Active task and next action

- M6 closed at 0.8.2.2; M7 **0/16**. Active M7-016 / KI-027 stock Deck
  performance. M7-009 lineage and M7-001 campaign preparation remain partial.
- Descriptor comparison is complete, **no material cadence gain**: accepted
  0.16.0.1 gameplay/pause 12.63/8.63 outputs/s versus 12.78/8.60. Reuse
  38.87%/36.89% is avoided work, not a performance PASS. Pause resolve waits
  remain zero; this does not imply zero resolve copies.
- Direct owner-paired Devkit SSH works with pinned host key. Connection details
  and credentials stay private. No sudo, installs, sysctl, clocks or affinity
  changes. perf 6.15-3 on kernel 6.16.12-valve24.5-1-neptune-616-gb2f7cfe85e45,
  perf_event_paranoid=2, exact-user process attachment proven.
- Both requested captures are DONE: owner-confirmed settled pause 4,249 samples
  and moving gameplay 6,878, each 20 seconds at 99 Hz task-clock:u, leaf only.
  The owner was told they can keep playing or close normally; **do not ask
  them to stay paused, repeat the drive, or run another unchanged F10 set**.
- Exact SDK names recovered via private relink of existing objects with every
  .text/.pdata byte, RVA/size and preferred base matching published DLLs.
  All 30,034 guest mapping keys verified against exact host/generated source.
- Shared guest wait helper: 921/715 samples in PAUSE-loop/comparison, 52.39% /
  42.06% of hottest guest thread in pause/gameplay. Six different caller
  predicates, watchdog/recovery behavior. One static completion producer is
  GPU Commands' PM4 event write, but leaf samples do not identify all callers.
  Do not remove waits or add unnotified WaitOnAddress based on this profile.
- Additional 8-second one-GPU-thread capture has 573 raw samples, private
  4096-byte user stacks, phase explicitly unconfirmed. Exact SSE2 forward-loop
  caller attribution: 47 resolve readback / 26 upload / 2 binding samples.
  Resolve samples represent 64 KiB--5 MiB in-flight copies. Not call counts,
  total bytes or a controlled FPS estimate. Source already prefers cached
  readback memory; actual allocation flags remain unknown.
- Next bounded experiment: after the current game closes normally and its
  returned full profile is archived, a same-scene diagnostic-only readback-cost
  A/B using separate launches with existing fast versus some modes and
  copy/byte/output/audio counters. The some arm retains GPU transfers but
  skips warm CPU copies; stale data is expected, never a shipping default.
  Use isolated profile/cache/diagnostics, never import that arm's save. No
  live cvar mutation (unsynchronized string access); none is overwritten by
  this host's default override and auto is not a supported value. Full protocol
  is in the CPU attribution report. Do not ship disabled
  readbacks: Photo Mode/title-side consumers require them. Use the result to
  prioritize selective/lazy readback versus command/protection work. No new
  runtime candidate has been published and no performance fix is claimed.
- Last inspected game: Linux PID 93337 / start ticks 156476661; exact GPU TID
  93367 / start ticks 156476785. It was alive for the final capture. Recheck
  lifetime before any attach; **never kill/relaunch or publish over writers**.
- Optional new chat at this evidence boundary; no task or milestone closure.
  No M7 push, physical wheel testing or new long soak.

## Source versus published identity

- Root source 0.16.0.2: targeted launcher correction plus reviewed attribution.
  Published/profiled host is STILL 0.16.0.1; source version is not a new Deck
  artifact. SDK clean 0.18.0.1 / 9098e122afa116f4316d6cec47e561ebc5186974;
  parent gitlink matches. No SDK source edits or codegen this checkpoint.
- Sync root private/bundles/M7-016; current.txt remains
  mcla-0.16.0.1-win64-B6A19E748F517476-0BCEBB4FB89A8529-58B3385C89D9E499.
  Receipt: private/evidence/M7-016/bundle-20260905-144002-92771135/result.json.
  Manifest 546AF7AA8256738AE37896818338A96A624A6DC73FE9ACE2333229EF04E1FAA2;
  lock 0AA62A18D247FEF31D2C37B4D4C09E8C573C6AB598D7A654593406487300DBDD.
  Exact host/runtime/GPU DLL hashes are pinned in private capture receipts.
- The prior complete four-file profile/album was unchanged before this launch:
  tree 3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A;
  save 58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585;
  header 2FAEFBC7FF8CDEADBCE025D9BE914436ED05E27D041F72C8C88798612CFB0D60;
  album 8EE49926E1DFE804DB923EF2512219FC7A078FEB50944C3D2BD4E9AC39F81588.
  Owner may have progressed since: inspect the returned session, never reseed
  from this old hash. Evolved successor reload remains physically unproven.
- Prior session 20260905T151245Z-296 complete/exit0, one profile snapshot,
  zero watcher errors. Full mutable archive outside sync:
  private/evidence/M7-016/returned-bundles/<bundle-id>/
  3EAEDE327B845C041138C52D578C6A7569B630F302AA73F1CF6F8F62A492CCBF.
  Additional full-profile archives: private/save-archive/M7-009.

## Evidence, recovery and tests

- Private CPU folders: cpu-20260905T175611Z-93337,
  cpu-20260905T181559Z-93337, cpu-gpu-stack-20260905T183135Z-93337.
  Original recordings retained in bundle diagnostics and private/evidence/M7-016.
  No lost/throttled records. Gameplay excludes one newly created non-inherited
  TID; GPU capture phase/overhead unverified. Do not publish stacks or raw maps.
- Startup recovery: exact shutdown F10 live-20260905T151532Z-336-5.partial
  matched archived nine files, reversibly moved on both hosts to
  diagnostics/interrupted/*.interrupted. No deletion, promotion, save or
  binary change. Owner confirmed existing Steam shortcut launched afterward.
  Narrow launcher source fix is tested, **not deployed to current bundle**.
  [Recovery evidence](evidence/M7-016-launcher-diagnostic-partial.md).
- Root AST scan and 3 rule tests pass; path-policy unit 52/0. Native launcher
  recovery/verify-only plus bundle 1 positive/13 negatives/41 contracts and
  lineage 46 checks pass under PowerShell 7 and 5.1. WinPS handle-lifetime
  fixture bug fixed. No native reparse-negative rerun claimed.
- Independent CPU-parser/caller review accepted exact results. Fixed reusable
  decoded-line count and strengthened memcpy thunk/IAT validation; original
  GPU receipt's 775 sample_lines means decoded lines, not 573 sample records.
- Prior SDK full tests: 1,845 executed + 4 known BitStream skips, zero failures.
  Existing exact published Windows Vulkan/D3D12 saved-gameplay/pause/Settings
  controls pass, not Deck cadence evidence. Deck F10 GDI images remain black;
  overlay READY is package completion and can inflate presentation FPS.

## Operational constraints

- Publication uses out/build/win-amd64-portable-release. Runtime files must
  be independent copies; old developer DLL hardlinks can mutate bundles.
  Only immutable prepared game files may be hardlinked. Do not rebuild an
  artifact merely to label already-completed owner evidence.
- Serialize runtime/private/build writes, stop writers and archive before
  publication/pruning. Never select ambiguous or divergent save branches.
- WSL/native UI helper unavailable; no system feature changes. Scoped shell
  and Codex apply_patch fallback work. No custom UIAutomation/SendInput.
- Preserve unrelated untracked source-directory .stfolder. No private game,
  saves, generated source, dumps or profiling captures in Git/public packages.
- README-AI owns ledger/history; this is a replace-in-place handoff.
