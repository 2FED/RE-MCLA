# M8 port review: pre-M9 working tree to main (2026-11-01)

The uncommitted main working tree from the M8 continuation (2026-10-11 to
2026-10-24) and the untracked M8 exact-clock lane (probes, scripts, tests,
evidence, patches) are archived on the local branch `wip/main-dirty-m8-20261101`
(head `f65ea53`; base `eb04d03`). The branch is not published. This report
records what was carried into main, what stays only on that branch, and why.

Method: `git diff eb04d03 wip/main-dirty-m8-20261101 -- <file>` against
`git diff eb04d03 main -- <file>`, file by file. For every ported file except
README-AI, main still matched `eb04d03`, so each wip hunk applied without
conflict. Line endings were kept as main has them (CRLF in tracked files; the
new export procedure stays LF like its working-tree copy). No build or launch.

## Ported

| File | Hunk | Adaptation |
| --- | --- | --- |
| `docs/source-data-policy.md` | `@@ -44,6 +44,15 @@` M8 raw sidecars privacy paragraph | Verbatim. |
| `docs/source-data-policy.md` | `@@ -53,4 +62,25 @@` metadata-export and timing-experiment paragraphs | Timing paragraph verbatim. Export paragraph rewritten for what ships now: M8-018 to M9-005 history, Stock/Unlocked admission with owner-file and signed-ticket records, numeric per-minute summary from the log tail, per-package `report.txt` not a manifest artifact and never read by the export. |
| `docs/evidence/M8-storage-cleanup-20261013.md` | `@@ -37,3 +37,192 @@` October 18 Temp and retired vinyl capture deletion audit, October 20/22 scheduled runs, October 24 retention update | Verbatim except two sanitized phrases: a personal Temp path and "this user's LocalAppData/Temp" now read "the operator's `%LocalAppData%\Temp`". Hashes, sizes, reasons and recovery limits unchanged. |
| `docs/evidence/M8-timer-source-observer-20261013.md` | `@@ -104,6 +104,28 @@` Timer consumer limitation | Verbatim (finished analysis: caller `0x822C1FA8` is the main-loop global timer, not a vehicle-physics step). |
| `docs/diagnostic-metadata-export.md` | whole document (untracked; in neither branch's history before the archive) | Rewritten as a current procedure: Unlocked timing, owner-file and ticket-grant admission, `performance_per_minute`, M9-010 trigger reasons, `report.txt` excluded, evidence links to tracked M9 reports. Checked against `src/mcla_diagnostic_export_policy.h` and `src/mcla_diagnostic_export_win.cpp`. |
| `AGENTS.md` | `@@ -57,7 +69,25 @@`, the "Owner-authorized storage maintenance" section only | Verbatim. The Spark line in the same hunk ("Observe the latest usage override above") was not taken, because the override it points to was not ported. |
| `README-AI` | 31 dated M8 checkpoint entries, 2026-10-11 to 2026-10-24 | Inserted verbatim into the section 16 checkpoint log in its newest-first date order, next to the matching dates; existing entries unchanged. None restates a later main entry with different facts, so no conflict note was needed. A dated 2026-11-01 archive note at the top of the log points to the archive branch and to this report. |
| `docs/dgx-spark-worker.md` | quota sentence | Neither the main sentence (5,000-credit floor) nor the wip one (50% weekly) was kept; it now reads "Usage limits follow the owner's current instruction in AGENTS.md; no credit purchases or reset redemption." |
| `scripts/mcla_fixture_identity_v2.py` | `PixelIdentityError` (`@@ -44`, `@@ -195`) | Verbatim. Main had the same structure; `ValueError` subclass with the same message, so the existing rejection test still covers it. `test_mcla_fixture_identity_v2`: 19 tests OK (owner Python `-E -S -B`). `ast-grep scan` clean. Its only consumer, `mcla_wgc_triplet_reader.py`, stays on the archive branch. |

Restored README-AI entries that link evidence reports not tracked on main
(for example `M8-renderdoc-vinyl-20261011.md`, `M8-metadata-export-20261011.md`,
`M8-arcade-binding-20261023.md`; 11 in total) resolve on the archive branch only.

## Left on the archive branch

- `src/mcla_app.cpp` probe wiring (12 `MCLA_*_PROBE` includes, timer/phase/
  handoff/guest-source/RenderDoc/garage-camera hooks) and the matching CMake
  probe options and about 55 probe/test targets: they need untracked sources and
  the exact-clock lane was abandoned when M8-001 was accepted as the measured-time
  unlocked mode (2026-10-28).
- `src/mcla_timer_observer_core.h` binding extensions: their consumer is archived.
- The 18 decoder/join scripts and their tests: they import archived modules
  (`mcla_boundary_exclusion`, `mcla_phase_census_adapter`) and would break main.
- `tests/mcla_render_target_probe_win.cpp` pins for private one-off rd2-rd8 builds.
- M8 sections of `docs/current-state.md`, `docs/M8-priorities.md`,
  `docs/build.md`, `docs/testing.md`, `docs/rexglue-sdk.md`: replaced by the M8
  closure and the M9/M10 handoff.
- The 2026-10-23 usage override in `AGENTS.md` (see owner decisions).
- PPC fault crash context and the KI-033 seeded-inspection sections (see owner
  decisions).

## Open owner decisions

1. **PPC fault crash context.** `src/mcla_crash_ipc.h` (IPC version 3 and a
   `PpcFaultRecord`), `src/mcla_native_crash.h/_win.cpp` (provider),
   `src/mcla_crash_handler_win.cpp` (optional `ppc-context-private.json` and
   manifest fields), plus the archived `mcla_ppc_fault_*` sources and four tests.
   Finished, opt-in, would add private guest register context to crash packages.
   It must be ported as one unit or not at all.
2. **KI-033 seeded-inspection evidence.** "Selecting the same car side" and
   "Bounded camera instrumentation" in
   `docs/evidence/M8-seeded-render-inspection-20261011.md`; useful only if the
   KI-033 (vinyl sharpness) work resumes.
3. **Usage limit rule.** The 2026-10-23 override ("continue until 50% weekly
   allowance remains", plus "refresh all current documentation before stopping")
   was the live rule in the pre-M9 tree but is not in main's `AGENTS.md`. Main
   still carries the older 5,000-credit text in the Spark section. The owner will
   decide the current limit; `docs/dgx-spark-worker.md` now defers to AGENTS.md.
4. **Untracked M8 lane.** Now preserved on the archive branch; deleting the
   working-tree copies is a separate owner decision.

## Notes for the coordinator

- The export records `guest_refresh_hz` as the stock constant 60 for every
  session with a capability record, Unlocked sessions included. The procedure
  document states this; it may deserve a follow-up fix.
- `docs/source-data-policy.md` still says live packages are unsafe to share as a
  whole, while the M9-005 tester flow asks testers to send each report folder by
  hand. That wording was not changed here.
