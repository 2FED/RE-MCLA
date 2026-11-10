# Storage cleanup, round 4, 2026-11-10

Owner approval on 2026-11-10, answering three proposals from the primary agent with "1. давай 2. так 3. так":
1. Delete F10 diagnostics packages in `C:\BDU\MCLA-Saves` from before owner build l81 (before 2026-11-08 00:00 local), except sessions for open bugs.
2. Delete the bmp frame captures in the M8-001 runs that no report cites, and the driver shader caches in the cited M8-001 runs.
3. Review the M2-M6 task evidence report by report and reduce closed items to before/after.

An Opus subagent did the work under the AGENTS.md section "Owner-authorized storage maintenance", using the same method as [round 3](M11-storage-cleanup-round3-20261110.md). No exe was launched. Nothing was committed.

The following were not touched: `private\l8*-*`, `l67-sdk-install`, `l39-sdk-src`, `gate-host-build`, `l34-src`, `_archive`, `owner-l86-flicker-20261110`, `tools`, keys, tickets, MAIN, and all save data.

**Record:** `private/storage-maintenance-20261110/record-round4.tsv` (untracked, because it holds local paths). It has 217 rows, one per deleted set: group, set, base path, bytes, files, oldest..newest write, issue mapping, what was kept, manifest name + sha256, result. All 217 sets were deleted, with no partial delete and no skip.

The per-set file manifests (relative path, size, sha256) and the scripts used (`del.ps1`, `g1inv.ps1`, `g1cite.ps1`, `g3targets.ps1`) are in `private/storage-maintenance-20261110/record-round4/`. In Group 1 manifests, files under a package's `save-private\` and `save-files-private.json` are listed with their size only (`not-hashed-private`), so their contents were never read.

## Method (all groups)
- Targets were listed first, then deleted one at a time with `Remove-Item -LiteralPath <exact path>` (files) or `-Recurse` (exact session/cache dirs), from PowerShell 7 (`private\tools\pwsh\pwsh.exe`). No wildcards were used. After each delete, a check confirmed the path was gone.
- Before each delete:
  - the target and every ancestor up to `C:\` are not reparse points;
  - a directory target contains no reparse point;
  - no process command line or executable path names the target.
- The script aborts if `mcla.exe` or `MCLA-Launcher.exe` is running. Neither ran during the work.

## Group 1: `C:\BDU\MCLA-Saves` diagnostics (137,087,199 bytes, 0.13 GiB)
**Checked:** 196 diagnostics sessions in 12 profiles (`<profile>\diagnostics\<session>`). Only `diagnostics\` was entered. `sessions\`, `backups`, `metadata`, `cache`, `managed-profile.dat`, receipts and save data were not opened. Only `report.txt` files were read.

**Rules.** A session was kept whole if any of these held:
- (a) it is the newest session of its profile, by newest file time;
- (b) it is cited:
  - the full session id, its first 8 hex characters (the docs abbreviate ids as `5d582fcc…`), a package folder name, or a package timestamp appears in `docs/known-issues.md`, `docs/current-state.md`, `docs/evidence/*.md` or README-AI;
  - this matched 104 sessions;
- (c) it contains a `*.dmp` file or a non-empty `crash\` folder;
- (d) its newest file is dated 2026-11-08 or later;
- the owner typed text into a `report.txt`. Only session `34b8579d…` (profile `5b71…`, 2026-10-29) has such text: a LOD problem behind the photo-mode camera and a perf drop against earlier builds. It names no KI number, but it is an owner report, so it is kept.

**Finding on (c).** All crash folders are empty, and there is no crash dump anywhere. All 203 `.dmp` files are `process-private.dmp`: the MiniDumpNormal snapshot that every F10 package carries (see [KI-070 minidump](M11-ki070-f10-minidump-20261106.md)). Rule (c) was applied literally, so 32 old, uncited, non-newest sessions are kept only because of that file. They are listed under "Undecided".

**Deleted:** 39 whole sessions, 125 files:
- 30 contained `live\` packages: F10 packages with save-private copies, frame.bmp and logs, but no `.dmp`.
- 9 held only `runtime\` logs.

| Profile | Sessions | Deleted (count / bytes) | Kept (count / bytes) | Why kept: newest / ≥11-08 / cited / dmp-only / owner text |
|---|---|---|---|---|
| MCLA-254a4ba8… | 18 | 5 / 54,201,839 | 13 / 338,762,258 | 1 / 3 / 9 / 0 / 0 |
| MCLA-29838443… | 1 | 0 | 1 / 1,883,150 | 1 / 0 / 0 / 0 / 0 |
| MCLA-4c4528c5… | 19 | 7 / 78,360 | 12 / 236,671,490 | 1 / 0 / 3 / 8 / 0 |
| MCLA-5b71c3e9… | 44 | 2 / 77,117,361 | 42 / 1,850,042,497 | 1 / 1 / 27 / 12 / 1 |
| MCLA-6ed686e0… | 12 | 0 | 12 / 639,366,298 | 1 / 0 / 8 / 3 / 0 |
| MCLA-7d6ff1dd… | 44 | 5 / 5,311,852 | 39 / 1,064,981,471 | 1 / 9 / 29 / 0 / 0 |
| MCLA-bc1cf77e… | 6 | 0 | 6 / 346,384,669 | 1 / 0 / 5 / 0 / 0 |
| MCLA-d5d79c8f… | 1 | 0 | 1 / 116,644,283 | 1 / 0 / 0 / 0 / 0 |
| MCLA-d66b8f07… | 1 | 0 | 1 / 1,148,332 | 1 / 0 / 0 / 0 / 0 |
| MCLA-d69aab6a… | 3 | 0 | 3 / 202,217,734 | 1 / 2 / 0 / 0 / 0 |
| MCLA-f5d835a6… | 44 | 19 / 366,828 | 25 / 496,913,170 | 1 / 0 / 16 / 8 / 0 |
| MCLA-f7d807f1… | 3 | 1 / 10,959 | 2 / 1,626,220 | 1 / 0 / 0 / 1 / 0 |

Profiles `4470…`, `4f6b…` and `af26…` have no diagnostics sessions.

**Recovery limit:** the deleted packages and runtime logs are gone. Their file lists and hashes remain in the manifests (save-private copies by size only). None was cited, and none had owner text.

## Group 2: M8-001 (5,426,376,704 bytes, 5.05 GiB)
M8 closed on 2026-10-28. Round 3's citation list (`subcite.tsv`) was used: 164 cited runs and 98 uncited.

**Before/after check.** No M8-001 report names a before/after capture set inside an uncited run. The reports cite runs by exact name, and all of those runs are in the 164-run cited set, which was not thinned.

As an extra guard, uncited runs whose name stem (the name without its trailing `-N`) appears in the docs were exempt from (a): `autostart-motion-config-20261017-2` and `game-source-capture-20261013-3`, 13 bmp files, about 36 MB.

**(a) Uncited runs: bmp frame captures.**
- 1,192 `.bmp` files in 41 of the 98 runs, 3,372,429,168 bytes (3.14 GiB).
- Locations: `user\autostart`, `user\motion-calibration`, `user\arcade-navigation`, `user\source-scout`, and `return-*\user\`.
- Only `.bmp` files were removed. All saves, receipts, logs, json and configs in those `user\` folders and runs are kept.

**(b) Cited runs: driver shader caches.**
- 198 cache dirs in 52 runs, 297 files, 2,053,947,536 bytes (1.91 GiB).
- Removed dirs: `profile\AppData\Local\D3DSCache`, `...\Local\NVIDIA\DXCache`, `...\Local\AMD\DxCache`, `...\locallow\NVIDIA\DXCache`. This is the same rule as round 3, which excluded `\user\` and MAIN.
- Everything else in the cited runs is untouched.

**Recovery limit:**
- The uncited-run frames cannot be replayed. Their names, sizes and sha256 are in the manifests.
- The caches are driver-generated and rebuild on the next launch.
- The derived reports `docs/evidence/M8-001-*.md` remain.

## Group 3: M2-M6 task evidence (4,512,385,325 bytes, 4.20 GiB)
**Status.** M2-M6 are all closed. Each closure review records its decision: M2 "GO WITH SDK FORK M3", M3 "GO M4", M4 "GO M5 WITH PINNED SAVE", M5 "GO M6", M6 "GO M7". The KI-to-task mapping comes from the task ids that each `docs/known-issues.md` row cites.

**Kept whole as open or uncertain:**
- M6-014: KI-022 (fix built, tester check pending), KI-023 (contained), KI-026 (open), KI-015 (open).
- M4-003, M4-012, M5-003: KI-003 (open).
- M6-004: KI-003, KI-015 and KI-016 (open).
- M5-013: KI-016 (open).
- M6-007: KI-014 (contained), KI-017 (open).
- M6-013: KI-007 (open).
- M6-009: KI-018 (policy).

**Rule for each closed item:**
- "After": the accepted run(s) the report cites, kept whole.
- "Before": one earlier run of the same item, kept whole.
- All other runs: only `.bmp`/`.png` captures and `.log` files of at least 1 MiB were deleted. `.sav`, `.json`, `.txt`, `.tsv`, `.toml`, `.cmake`, `.xsh`/`.xpso` and logs under 1 MiB are kept in every run. Top-level item files (design notes, address audits, RTL debug logs) are kept.

Most of these items are acceptance gates, not defect fixes, so there is no defect before/after pair. For those, "before" is the earliest pre-acceptance attempt that has a capture, and the report itself describes the later runs as superseded iterations.

**Generated-code trees (M3-013, M2-016):** a lossless dedup. A `.cpp`/`.h` file was deleted only if the reference tree, kept whole, has the same relative path with the same sha256. Every differing file stays. So each iteration tree can still be rebuilt exactly from the reference tree plus its kept files. M2-012 `pre-clean-08-generated` has no identical file against `09-final-clean-a` and is untouched.

| Item | Deleted bytes | After (kept whole) | Before (kept whole) | Derived report |
|---|---|---|---|---|
| M4-013 | 831,395,001 | `20260814-023505-e69d76a2` (canonical 20-cycle gate) | `20260814-010355-4077a5b4` (first attempt) | M4-013-known-deviations.md, M4-closure-review.md |
| M3-013 | 635,384,639 (dedup) | `20260811-152722-e7412213` (final trace) + `generated-manifest-v0.9.0.7.json` | `pre-8249CBF0-generated` (first invalid-target state); 4-5 differing files per later tree kept | M3-013-startup-traps.md |
| M4-002 | 577,181,212 | `20260812-085022-cc01a857` | `delayed-capture-20260811-214906-100` | M4-002-render-paths.md |
| M6-002 | 460,806,750 | `20260817-155005-1dd57bd3` | `20260817-133107-bee86b5d` | M6-002-garage-lifecycle.md |
| M4-011 | 437,454,217 | `20260813-230502-1f0de720` | `calibration-a` | M4-011-frontend-smoke.md |
| M2-016 | 384,086,085 (dedup) | `codegen-regression` (whole) | `failed-shift-predicate` (the failed predicate run named in the report: logs, json and every differing source kept) | M2-016-rexglue-vector-regression.md |
| M5-006 | 330,671,645 | `20260814-130533-0b95f6b6` | `diagnostic-20260814-122622` | M5-006-gameplay-input.md |
| M4-006 | 158,295,993 | `20260812-212030-5fc01c73`, `20260813-124600-293c07b3`, `20260813-144406-2c1974da` | `20260812-182750-556baaf2` | M4-006-controller-matrix.md |
| M4-001 | 143,804,582 | `20260811-190657-eae1665b` | `20260811-181241-0cd04485` | M4-001-first-valid-frame.md |
| M5-008 | 132,712,344 | `20260814-150733-da37caed` | `diagnostic-20260814-134553` | M5-008-physics-timing.md |
| M4-004 | 114,847,469 | `20260812-123316-db0f1cf4` | `20260812-114220-edf88c6d` | M4-004-xam-profile.md |
| M4-007 | 91,044,696 | `20260813-170202-44d2c7d8` | `diagnostic-20260813-155633` | M4-007-audio-route.md |
| M6-010 | 80,874,562 | `20260819-180446-730942f2` | `20260819-173711-cdb514d0` | M6-010-window-device-lifecycle.md |
| M4-010 | 68,749,223 | `20260813-213301-505ca1ca` | `20260813-202912-e5ad75f7`, plus `ru-reference-20260813-205305` | M4-010-locale-paths.md |
| M5-002 | 65,076,907 | `20260814-093131-ddca5b9d` | `20260814-091518-e4e601f1` | M5-002-world-streaming.md |

**Recovery limit:**
- For the non-kept runs listed in the record, the full runtime logs (≥1 MiB) and frame captures cannot be replayed.
- Their names, sizes and sha256 are in the per-run manifests.
- Each run's small receipts, result json and save copies remain.
- The dedup deletions are recoverable from the reference trees.

## Undecided / kept-large (owner or primary to decide)
| Item | Size | Why kept |
|---|---|---|
| MCLA-Saves: 32 old, uncited, non-newest sessions whose only F10 packages contain `process-private.dmp` (profiles 4c45 ×8, 5b71 ×12, 6ed6 ×3, f5d8 ×8, f7d8 ×1) | 444,776,604 bytes | Rule (c) as written covers any `*.dmp`. These are F10 snapshot minidumps, not crash dumps (all `crash\` folders are empty). A ready target list is in `record-round4/g1-undecided-dmp-targets.tsv` (run with `del.ps1 <tsv> run`). |
| MCLA-Saves session `34b8579d…` (5b71) | 38,951,409 bytes | owner-typed report text (LOD behind the photo camera; perf drop). Keep unless the primary confirms it is catalogued. |
| MCLA-Saves cited old non-newest sessions (97 of the 104 cited) | 4,108,606,033 bytes | Rule (b). Most are cited only by an 8-hex session-id abbreviation in KI/M10/M11 evidence. |
| M2-M6 open or uncertain: M5-003 (0.85 GB), M6-014 (0.58), M4-012 (0.34), M6-004 (0.15), M4-003 (0.07), M5-013 (0.03), M6-007 (0.03), M6-013, M6-009 | ~2.1 GB | mapped to open, contained or pending KIs |
| M2-M6 closed but not reviewed (<0.13 GB each): M2-011 (0.12, single cited tree), M2-012 (0.24, both trees differ), M5-012 (0.06), M6-001 (0.05), M4-009, M4-005, M6-015 (wheel/FFB, near KI-022), M4-008, M6-003, M5-009, M4-010-diagnostic-*, and the rest | ~0.7 GB | small; little to gain |
| M8-001 cited runs: everything except shader caches | ~9.4 GB | Rule (b) for cited runs. Not thinned in this round. |

## Free space (C:)
| Point | Bytes | GiB |
|---|---|---|
| Before round 4 | 114,390,126,592 | 106.53 |
| After round 4 | 123,774,058,496 | 115.27 |

Round 4 deleted 10,075,849,228 bytes (9.38 GiB): Group 1 0.13, Group 2 5.05, Group 3 4.20. The free-space gain is 8.74 GiB because other writes on the disk happened at the same time.
