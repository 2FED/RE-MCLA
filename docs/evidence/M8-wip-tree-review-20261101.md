# wip/main-dirty-m8-20261101 vs main: port review

Read-only review by an Opus subagent on 2026-11-01 of branch `wip/main-dirty-m8-20261101` (the uncommitted main working tree preserved at the move back to main: commits 2082436 tracked changes, 9d75165 untracked export sources, f65ea53 archive of 686 untracked M8-lane files) against main at the M9/M10 head; accepted by the primary agent. The branch is local history and is not published.

Read-only review, 2026-11-01. Base `eb04d03` (M8-001 automatic navigation), wip = `9d75165`
(two preservation commits), main = `a7eb0af`. Commands: `git diff eb04d03 wip -- F`,
`git diff eb04d03 main -- F`, `git diff wip main -- F`, `git log 0bae39e..main -- F`.

The wip branch changes **54 files**, not 55: 45 modified tracked files plus 9 new export files.

## Summary

| Class | Count |
| --- | ---: |
| (a) fully ported (identical, or a later M9 version of every hunk) | 15 |
| (b) partially ported | 2 |
| (c) not ported, no replacement in main | 11 |
| (d) superseded by design (M8 exact-clock lane dropped by the timebox `f035661` and M8 closure `ca11999`; launcher rows replaced by M9-003/M9-004) | 26 |

Main findings:

1. **M9-002 port is byte-exact.** At `0bae39e` all 9 export files and all 6 launcher files
   (`mcla_launcher_controller_win.cpp`, `mcla_launcher_ui_win.cpp/.h` and three launcher tests) are
   identical to wip. Seven are still identical on main. The other eight changed later on purpose:
   M9-003 `7b4ee19` (30 FPS/Unlocked timing), M9-004 `b571689`/`430ca82` (Setup category rows
   removed and deferred to REL-019), M9-005 `a7f88db`/`d9f1ad9` (log-tail performance summary,
   report prompt), M9-006 `37e5c3a`/`aeb3aef` (owner admission KI-045, ticket grant), M9-010
   `f31a4d3` (wheel/pad triggers), M10 `99d6e56`. No wip hunk is lost.
2. **The wip branch cannot build on its own.** Its `src/mcla_app.cpp` includes 12 headers that
   are not in wip or main, and its `CMakeLists.txt` references 65 sources that are not in wip.
   Its decoder scripts import `mcla_boundary_exclusion.py` and `mcla_phase_census_adapter.py`,
   which are also missing. Its crash hunks need `mcla_ppc_fault_*.h/.cpp`. All of these exist only
   as **untracked files in the current checkout**: about 413 under scripts, 73 under src, 135 under
   tests and 64 under docs, including `docs/diagnostic-metadata-export.md` and every M8
   arcade/clock evidence report. No commit holds them.
3. Main covers M8-001 differently. It uses a measured-time timer plus the timebox, and the owner
   accepted it as the experimental unlocked mode (README-AI M8-001 [x], closure 2026-10-28). The
   wip October 11-24 exact-clock/swap-binding instrumentation lane, described in the wip
   current-state as "Next: pure ALL25-slot raw callback/lexical inventory", is therefore an
   abandoned experiment. It is not unfinished product work.

## Per-file table

| File | Class | What is missing from main | Risk / value | Recommendation |
| --- | --- | --- | --- | --- |
| src/mcla_diagnostic_export_main_win.cpp | a (identical) | nothing | - | none |
| src/mcla_diagnostic_export_ui_win.cpp | a (identical) | nothing | - | none |
| src/mcla_diagnostic_export_ui_win.h | a (identical) | nothing | - | none |
| src/mcla_diagnostic_export_win.h | a (identical) | nothing | - | none |
| tests/mcla_diagnostic_export_ui_win_test.cpp | a (identical) | nothing | - | none |
| tests/mcla_diagnostic_export_win_test.cpp | a (identical) | nothing | - | none |
| tests/mcla_launcher_ui_render_win.cpp | a (identical) | nothing | - | none |
| src/mcla_diagnostic_export_policy.h | a (evolved) | wip `timing == "stock"`-only checks replaced by Stock/Unlocked keys, owner/ticket admission, log-tail summary | - | none |
| src/mcla_diagnostic_export_win.cpp | a (evolved) | main adds optional log-tail.txt input | - | none |
| tests/mcla_diagnostic_export_policy_test.cpp | a (evolved) | superset in main | - | none |
| src/mcla_launcher_controller_win.cpp | a (evolved) | Stock-only `AdmitCapabilities`/launch args evolved (timing, admission, Steam/SDL env) | - | none |
| tests/mcla_launcher_controller_win_test.cpp | a (evolved) | `caps.size()!=17`/`timing stock` asserts evolved | - | none |
| src/mcla_launcher_ui_win.cpp | a + d | read-only "Frame timing" label replaced by a combo (M9-003 `7b4ee19`); Compatibility/Accessibility/Cheats rows removed (M9-004 `430ca82`, REL-019) | - | none |
| src/mcla_launcher_ui_win.h | a + d | `UiCompatibilitySummary/UiAccessibilitySummary/UiCheatsSummary` removed by `430ca82` | - | none |
| tests/mcla_launcher_ui_win_test.cpp | a + d | `TestConfigurationCategorySummary` and stock-only timing asserts removed with the features | - | none |
| CMakeLists.txt | b | Launcher/export targets are ported (the include lines only moved). Missing: 25 `MCLA_*_PROBE`/`*_EXPERIMENT` options, about 55 probe/test targets (timer/phase/router/vector/work/handoff/guest-source/renderdoc/vinyl rd2-rd8 render probes/ppc-fault/garage-control) and `mcla_renderdoc_probe` lib | These need untracked sources, so they cannot build from git | drop (archive in wip) |
| README-AI | b | 31 dated checkpoint-log entries from 2026-10-11 to 10-24 are in wip only: M8-017 rd checkpoints x7, M8-018 export engineering, M8-007, l12 owner return/vinyl, support-folder repair candidate, M8-001 triplet/STREAM2/source-binding/owner2/3/5, Oct 18 Arcade feasibility, Oct 23/24 exact-clock/H-history/S-return. The wip M8 status lines are superseded by the closure. | AGENTS rule: keep dated history. These are history, not live state. | ask owner: port the 31 entries into the checkpoint log as history, or add one line pointing at the wip branch |
| docs/current-state.md | d | wip Oct 24 handoff (replace-in-place) | Replaced by M9/M10 handoff | drop |
| docs/M8-priorities.md | d | "October24 current checkpoint" paragraph (M8 4/6/9, 50%-floor wording) | Wrong after M8 closure | drop |
| docs/build.md | d | "Current M8 private exact-clock diagnostic boundary" (private host-step/sdk-sem recipes, V2 export ordinal shift) | Only about the abandoned lane | drop |
| docs/testing.md | d | "Current M8 exact-clock prerequisite boundary (October24)", clock/arcade unittest counts (218/663 PASS, for untracked tests), M7-016 "historical" relabels | Tests named are untracked | drop. Optional: port the two M7-016 "historical, not a current owner request" relabels |
| docs/rexglue-sdk.md | d | Oct 24 private diagnostic SDK paragraph. One lasting SDK fact: swap `StopIssuing` is not a drain, because started callbacks can finish later | Low | drop. Optional: keep the StopIssuing sentence if SDK swap work resumes |
| docs/dgx-spark-worker.md | c | Replaces the "5,000 credit floor" sentence with the Oct 23 "stop at 50% weekly" policy | Codex-era quota policy. Main text is stale too. | ask owner (both versions may be obsolete under the current coordinator policy) |
| AGENTS.md | c | Hunk 1: 2026-10-23 "continue until 50% weekly remains" override plus "refresh docs before stopping". Hunk 2: whole **Owner-authorized storage maintenance** section (heartbeat `temp` every 48 h, scope, preserve list, record-before-delete rules). Hunk 3: Spark line "Observe the latest usage override" | Owner policy. The storage section governs a live recurring automation. | ask owner. Hunk 2 has the highest value; hunk 1 is probably superseded. |
| docs/source-data-policy.md | c | Three paragraphs: (1) M8 raw sidecars, token/clock graphs: whole raw stays private, no routine model worker upload, a hash does not make raw disposable; (2) metadata-export backend: typed two-file report, preview + confirm, not provenance-authenticated, links `diagnostic-metadata-export.md`; (3) timing experiments/dependency archives stay private and are not redistributable, Spark packets carry selected in-scope source only | Export ships in main (M9-005), so (2) describes a live feature. (1)/(3) are generic privacy rules for data still on disk. | **port** (update (2) for Unlocked timing and the performance summary) |
| docs/evidence/M8-storage-cleanup-20261013.md | c | "October18: Temp and retired vinyl capture payloads": 4157 Temp dirs + 13 files, 150 RDC/cache files of 28.5 GB, plan/script SHA, recovery limits (rd2-rd6 raw RDC replay lost) | Deletion audit trail. AGENTS says to record recovery limits. Contains one personal Temp path. | **port** (sanitize the path to `%LocalAppData%\Temp`) |
| docs/evidence/M8-timer-source-observer-20261013.md | c | "Timer consumer limitation": caller `0x822C1FA8` is the main-loop global timer, not a vehicle-physics step; subdivision helper `0x821BD910`; summed returns are not proven simulation time | Correct, finished analysis. Consistent with the later timebox. | port (small, historical correctness) |
| docs/evidence/M8-seeded-render-inspection-20261011.md | c | "Selecting the same car side" plus "Bounded camera instrumentation" (KI-033 vinyl repro route, 5 capture hashes, garage camera probe design) | Supports deferred KI-033 sharpness work only | ask owner (port if KI-033 sharpness resumes) |
| src/mcla_crash_handler_win.cpp | c | PPC fault context: copies `state.ppc_fault`, writes optional `ppc-context-private.json`, manifest fields `ppc_context_requested/cleanup_failed/ppc_context` and artifact entry. Main's report.txt hunk is unrelated. | Needs untracked `mcla_ppc_fault_artifact.h` and friends. Finished, guarded, opt-in. Used only by the M8 update-fault probe. | ask owner (candidate for crash-diagnosis backlog, port as a unit) |
| src/mcla_crash_ipc.h | c | `kCrashIpcVersion` 2 to 3, `PpcFaultRecord ppc_fault` member | Same feature (IPC ABI bump) | same as above |
| src/mcla_native_crash.h | c | `InstallPpcFaultProvider` declaration | Same feature | same as above |
| src/mcla_native_crash_win.cpp | c | lock-free provider atomic, snapshot on faulting thread, `InstallPpcFaultProvider` | Same feature | same as above |
| scripts/mcla_fixture_identity_v2.py | c | `PixelIdentityError` (bounded scalar rejection diagnostic) replaces the bare `need()` | Small, self-contained, backward compatible (ValueError subclass). Its only consumer is untracked. | drop, or port with its test if WGC fixtures return |
| src/mcla_app.cpp | d | All 11 hunks are M8 probe wiring: 12 `#if MCLA_*_PROBE` includes; autostart/timer-config/fixed60/raw experiments; update-timing/handoff/phase census (exact-dispatch check on `sub_822C1FA8`, `sub_82305D00`, `sub_82131008`); timer observer on `sub_821BDA90`; guest-source probe/sequence/scout threads; RenderDoc two-state capture; `mcla_garage_camera_probe` cvar + `PulseCamera` + `garage_control::ParseAction`. No timer-observer-core, native-crash or readback-probe product change. | Needs 12 untracked headers. The lane was abandoned. | drop (archive) |
| src/mcla_timer_observer_core.h | d | `ScopeBinding`/`EntryEnvelope`, `read_binding`, `invoke_with_ordinal`/`invoke_with_entry`, binding-aware discard | Consumer `mcla_timer_observer_win.cpp` is untracked | drop |
| tests/mcla_render_target_probe_win.cpp | d | Per-macro pinned host/build hashes for camera/renderdoc/atlas/upload/producer/fault/downscale/clean rd2-rd8 probes | Pinned to private one-off builds | drop |
| scripts/mcla_consumer_detail_decode.py, mcla_router_dispatch_decode.py, mcla_vector_store_decode.py, mcla_vector_target_decode.py, mcla_virtual_detail_decode.py, mcla_work_emission_decode.py | d | v2 schemas with `boundary_exclusions` (`boundary.validate/count_keys/add_issues/add_context`) | `import mcla_boundary_exclusion` is untracked. Porting alone breaks main with ImportError. | drop |
| scripts/mcla_phase_census_decode.py, mcla_phase_source_join.py, mcla_frame_handoff_closed_prefix.py | d | phase-census v3 (`validate_phase_census_window`, prewindow Render provenance, v3 never relaxes unknown unscoped) | `import mcla_phase_census_adapter` is untracked | drop |
| scripts/mcla_frame_consumer_join.py, mcla_frame_router_join.py, mcla_frame_virtual_join.py | d | propagate the local `retained_context_issues` prefix | Part of the same v2 set | drop |
| scripts/test_mcla_frame_consumer_join.py, test_mcla_frame_handoff_closed_prefix.py, test_mcla_frame_router_join.py, test_mcla_frame_virtual_join.py, test_mcla_phase_source_join.py, test_mcla_router_dispatch_decode.py | d | tests for the above (import untracked `test_mcla_phase_census_adapter`) | Same | drop |

## Highest-value items to port

1. **docs/source-data-policy.md**: both wip hunks (`@@ -44,6 +44,15 @@` "The private M8 exact-clock/swap-source diagnostics include ..." and `@@ -53,4 +62,25 @@` "The M8 metadata-export backend constructs a new typed report ..." + "Private timing experiments may also retain ..."). In the export paragraph replace "M8" with "M9-005" wording, and either drop the `diagnostic-metadata-export.md` link or port that doc too (item 5).
2. **AGENTS.md** hunk `@@ -57,7 +69,25 @@`: the whole "## Owner-authorized storage maintenance" section (5 bullets). Needs owner confirmation because the heartbeat automation `temp` is still active. Hunk `@@ -13,6 +13,18 @@` (50% weekly override) needs an owner decision and is probably obsolete.
3. **docs/evidence/M8-storage-cleanup-20261013.md** hunk `@@ -37,3 +37,192 @@` ("## October18: Temp and retired vinyl capture payloads" to end). Replace the personal Temp path with `%LocalAppData%\Temp`. Keep the plan/script SHA and the rd2-rd6 "replay no longer possible" boundary.
4. **docs/evidence/M8-timer-source-observer-20261013.md** hunk `@@ -104,6 +104,28 @@` (replaces the last 3 lines with "## Timer consumer limitation" + the token-transport paragraph). Optionally drop the forward-looking "next bounded diagnostic" paragraph, since that lane is closed.
5. **docs/diagnostic-metadata-export.md** (untracked, in neither branch, 96 lines). Main ships the exporter but has no tracked user-facing contract for it. Refresh for M9 (timing `stock`/unlocked key, `performance_per_minute` + disclaimer, owner/ticket admission records) before tracking it.
6. Ask owner: **README-AI** 31 historical checkpoint entries (2026-10-11..24; list above) as one block in the checkpoint log, or a single pointer line to `wip/main-dirty-m8-20261101`.
7. Ask owner / backlog: **PPC fault crash context**, ported as one unit: `mcla_crash_ipc.h` (v3 + member), `mcla_native_crash.h/.cpp` (provider), `mcla_crash_handler_win.cpp` (3 hunks), plus the untracked `src/mcla_ppc_fault_{record,artifact,json}.h`, `mcla_ppc_fault_context_win.{h,cpp}` and four tests. It adds private guest register context to crash packages, which could help KI crash triage, but no wiring in main provides it today.

## Separate risk (outside the 54 files)

About 685 untracked working-tree files (scripts 413, src 73, tests 135, docs 64) are the only copy
of the M8 instrumentation sources and evidence reports, such as `M8-arcade-binding-20261023.md` and
`M8-metadata-export-20261011.md`. Main does not link to any of them; no README-AI link is broken.
They are lost if the checkout is cleaned. Owner decision: commit them onto the wip branch as an
archive, so its mcla_app/CMake/scripts hunks become buildable history, or record that dropping them
is intended.
