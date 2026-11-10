# Storage cleanup, round 3, 2026-11-10

Owner approval on 2026-11-10: (1) "так, прибирай" for the l71-l81 experiment build trees; (2) in `C:\BDU\MCLA-Saves`, delete diagnostics items that are duplicates of what is kept in evidence; in `private\evidence`, keep everything for unclosed issues and, for closed issues with far more than a before/after pair, delete what is not before/after. An Opus subagent did the work under the AGENTS.md section "Owner-authorized storage maintenance", in parallel with round 2 (`_archive`, old SDK installs, old source trees) and a build agent (`l88-*`, `l82-sdk-*`, `l67-sdk-install`, `gate-host-build`, `l86/l87-host-build`). None of those was touched, except the five SDK worktrees round 2 handed over (below). No exe was launched. Nothing was committed.

The per-target record is untracked because it holds local paths: `private/storage-maintenance-20261110/record-round3.tsv` (path, bytes, files, newest write, issue mapping, what was kept, detail, result). Per-item file manifests (relative path, size, sha256) and the preserved build logs are in `private/storage-maintenance-20261110/record-round3/`.

## Method (all groups)
- One target at a time, `Remove-Item -LiteralPath <exact path> -Recurse -Force` from PowerShell 7 (`private\tools\pwsh\pwsh.exe`). No wildcards. After each delete, a check that the path is gone.
- Before each delete: the target and every ancestor up to `C:\` are not reparse points, and the target contains no reparse point (0 found everywhere). No process command line or executable path named the target. A read-only `du.exe` scan run by another agent named every folder; it was excluded from the in-use check.
- Results: no partial deletes, no in-use errors.

## Group 1: l71-l81 experiment build trees and worktrees (19.18 GiB)

### Build and object dirs: 39 deleted, 11,756,948,640 bytes (10.95 GiB)
`l71-audio-keys-build`, `l71-crashflush-build`, `l71-minidump-build`, `l71-sdk-audio-b/-o`, `l71-sdk-o`, `l72-album-build`, `l72-kick-build`, `l72-sdk-fetch-b/-o`, `l73-keys-build`, `l73-sdk-steady-b/-o`, `l73-sdk-xma-b/-o`, `l74-host-check`, `l74-sdk-xma2-b/-o`, `l75-host-check`, `l75-sdk-steady2-b/-o`, `l75-sdk-xma3-b/-o`, `l76-host-check`, `l76-sdk-steady3-b/-o`, `l76-sdk-xma4-b/-o`, `l77-host-check`, `l77-sdk-fence-b/-o`, `l78-host-check`, `l78-sdk-defaults-b/-o`, `l79-host-check`, `l79-sdk-o`, `l80-host-check`, `l80-sdk-blend-b`, `l81-host-check`.

The record has the bytes, file count and newest write of each one, plus the sha256 of every top-level, `Release\` and `build\` exe/dll. Before each delete, the script copied the dir's `.ninja_log`, `CMakeCache.txt`, `install_manifest.txt` and `base-commit.txt` to `record-round3/buildlogs/<name>/`. Two of these logs matter: [KI-072 evidence](M11-ki072-l71-southcentral-20261106.md) cites the `l72-album-build` and `l71-minidump-build` ninja timestamps as the build-window proof, and KI-072 is treated as open. Both `.ninja_log` files are preserved.

No script or active build references these dirs. The CMake caches of `gate-host-build`, `l82-sdk-b`, `l86/l87-host-build` and `l88-sdk-b` do not point at any of them, and the prepare scripts mention them only in comment headers. The experiment arms in `lNN-arms` contain only `mcla-dev-overrides.txt`. The staged `rexruntime.dll` of each owner build lives in `lNN-internal\build`.

**Kept on purpose:**
- `l80-sdk-o` and `l81-sdk-o` (642 MB). Each holds the only `rexruntime.pdb` / `rexgpu-xenos.pdb` for its shipped `rexruntime.dll`. For l81, that dll is identical to `l81-internal` and the `l81v-volunteer` build (sha256 `FDBC2143...`). For l80, it is identical to `l80-internal` and is the published SDK tag `v0.20.0.10` (`ADDF4492...`). These are the symbols for crash reports.
- `l70-build`: outside the l71-l81 scope.

### SDK worktrees: 17 removed, 8,840,828,982 bytes (8.23 GiB)
Removed: 12 experiment worktrees (`l71-sdk-audio-wt`, `l72-sdk-fetch-wt`, `l73-sdk-steady-wt`, `l73-sdk-xma-wt`, `l74-sdk-xma2-wt`, `l75-sdk-steady2-wt`, `l75-sdk-xma3-wt`, `l76-sdk-steady3-wt`, `l76-sdk-xma4-wt`, `l77-sdk-fence-wt`, `l78-sdk-defaults-wt`, `l80-sdk-blend-wt`), plus the five handed over from round 2 (`l66-sdk-wt`, `l67-sdk-wt`, `l71-sdk-wt`, `l79-sdk-wt`, `l80-sdk-wt`). All belong to `private\l39-sdk-src`.

Checks, re-run on every worktree:
- `status --porcelain --ignore-submodules=all` was empty, with no untracked and no ignored files.
- HEAD is reachable from a branch. The current branch of each worktree is kept, and its HEAD SHA is in the record.

`git worktree remove` without force refused every worktree with "not a git repository: thirdparty/FFmpeg/../../.git/modules/thirdparty/FFmpeg": the copied nested submodules have relative gitlinks. The coordinator approved a fallback: delete the folder by exact path, then run `git worktree prune -v` in `l39-sdk-src`. Prune removed exactly those 17 entries.

`git worktree list` afterwards shows only `l39-sdk-src`, `l81-sdk-wt`, `l82-sdk-wt` and `l88-sdk-wt`. 29 `mcla/*` branches and tags `v0.20.0.0` to `v0.20.0.11` are present. No branch or tag was deleted. The record also keeps the rows of the refused `git worktree remove` attempt, marked as superseded.

**Recovery:** `git worktree add <path> <branch>` restores each tree exactly. Submodule checkouts come from the module store of `l39-sdk-src`.

## Group 2: `C:\BDU\MCLA-Saves` (0 bytes)

**Checked:** 386 F10 packages under `<profile>\diagnostics\<session>\live\` across 13 profiles. Excluding the newest session of each profile leaves 364 packages, 4.60 GB. Each candidate was looked for under `private` (outside `_archive` and the round-2 and build-agent dirs) in two ways:
- a folder with the same name;
- a sha256 match of its `manifest.json` against every same-size file in a full size index of `private` (77,874 files).

**Result:** no non-newest package has an identical copy in `private`. 297 have a manifest and none matched. 67 are `.partial`/`.interrupted` packages without a manifest, and none of them has a same-name copy either.

**Method check:** the only same-name copies are 10 packages of profile `MCLA-5b71…` in its newest session `5d582fcc…`. Their copies are in `private\owner-l86-flicker-20261110\diagnostics`, and all 10 matched file by file (11/11 files each). Those packages stay protected as the newest session.

So nothing in MCLA-Saves was deleted. No save data, `sessions`, `backups`, `metadata`, `cache`, receipts, `save-private` or `save-files-private.json` was opened. Empty `crash`/`runtime` folders were left in place.

| Profile | Packages (all sessions) | Bytes | Deleted | Kept-copy root |
|---|---|---|---|---|
| MCLA-254a4ba8… | 13 | 210,720,148 | 0 | none |
| MCLA-4c4528c5… | 34 | 228,224,167 | 0 | none |
| MCLA-5b71c3e9… | 132 | 1,745,234,940 | 0 | newest session only: `owner-l86-flicker-20261110` |
| MCLA-6ed686e0… | 44 | 610,850,442 | 0 | none |
| MCLA-7d6ff1dd… | 73 | 989,332,110 | 0 | none |
| MCLA-bc1cf77e… | 24 | 342,777,117 | 0 | none |
| MCLA-d5d79c8f… | 6 | 115,672,757 | 0 | none |
| MCLA-d69aab6a… | 9 | 187,121,709 | 0 | none |
| MCLA-f5d835a6… | 49 | 495,211,714 | 0 | none |
| MCLA-f7d807f1… | 2 | 1,047,155 | 0 | none |
| others (2983…, d66b…, 4f6b…, af26…) | 0 non-newest | | 0 | |

## Group 3: `private\evidence` (1.93 GiB)
Size before: 27.58 GB in 194 top-level items. M8-001 alone holds 17.06 GB in 262 run folders.

**Classification:**
- **Issue status:** taken from the status column of `docs/known-issues.md`, plus the explicit open list in the task (KI-074/074b/060/054/026/067/072/075/073 and anything re-test pending, triage or backlog).
- **Citation:** a run folder counts as cited when its exact name appears anywhere in `docs/**`, `README-AI` or `AGENTS.md`.
- **M8-001:** M8 closed on 2026-10-28, and the owner accepted M8-001's deliverable (experimental unlocked mode) in [the closure record](M8-closure-readiness-20261028.md). 164 M8-001 runs are cited (11.32 GB) and stay under rule (b): they include the before/after captures. 98 are cited nowhere (5.74 GB).

**Deleted:** only the driver shader caches inside those 98 uncited M8-001 runs. These are the `profile\AppData\Local\NVIDIA\DXCache`, `...\Local\D3DSCache`, `...\Local\AMD\DxCache` and `...\locallow\NVIDIA\DXCache` dirs of each run's isolated Windows profile. They are driver-generated, rebuild on the next launch, and are not evidence.

152 cache dirs were deleted, 2,070,596,264 bytes (1.93 GiB). Each has a manifest in `record-round3/<run>__<path>.manifest.tsv`, and the manifest's sha256 is in the record.

Everything else in those runs is kept: text, json and log receipts, configs, `user\` saves, and the `user\autostart` / `user\motion-calibration` bmp captures (rule (d): nothing inside a `user` folder was touched).

| Issue / item | Deleted | Kept as before/after |
|---|---|---|
| M8-001 (closed), 98 uncited runs | 1.93 GiB shader caches | all 164 cited M8-001 runs untouched; all receipts and `user\` captures of the 98 runs |
| Everything else | 0 | everything |

**Recovery limit:** the deleted shader caches cannot be replayed; they are regenerated by the driver. No capture, log, receipt or save was removed. The derived reports in `docs/evidence/M8-001-*.md` remain.

### Undecided / kept-large (owner to decide)
| Item | Size | Why kept |
|---|---|---|
| M8-001 uncited runs: bmp captures in `user\autostart`, `user\motion-calibration` | ~3.25 GB | rule (d) "never delete user folders". The bmp files are harness frame captures stored in the run's save root. Deletable if the owner says these `user\` captures are not save lineage. |
| M8-001 cited runs: driver shader caches | ~1.96 GB | rule (b) keeps cited items whole, although the caches have no evidentiary value |
| M8-001 cited runs: everything else | ~9.4 GB | rule (b). Thinning to one before/after per sub-investigation needs a per-run reading of ~40 M8-001 reports. |
| M2-M6 task items (M4-013 1.71 GB, M5-003 0.87, M4-002 0.77, M3-013 0.77, M4-011 0.59, M6-014 0.60, M6-002 0.50, M2-016 0.49, M5-006 0.43, M4-012 0.35, others) | ~8 GB | milestone-acceptance task evidence. Run names are often generic, so they cannot be mapped to before/after without reading each report. M6-014 maps to KI-022/KI-023, which are not clearly closed. |
| M8-vinyl-repro-20261011 (0.65 GB), M8-renderdoc-vinyl-20261011 (0.18 GB) | 0.83 GB | KI-032 is open; KI-033's extra sharpness is deferred |
| M7-002/003/004/012/015 | ~0.7 GB | M7-016 / KI-026 (open) / KI-027 (open) |

## Free space (C:)
| Point | Bytes | GiB |
|---|---|---|
| Before round 3 (at start, round 2 already running) | 59,469,467,648 | 55.39 |
| After round 3 | 114,311,532,544 | 106.46 |

Round 3's own deletions total 22,668,373,886 bytes (21.11 GiB). The rest of the change comes from round 2, which ran at the same time.

## Owner decisions
1. Whether the bmp captures inside M8-001 runs' `user\` folders may go (~3.25 GB in uncited runs, more in cited ones).
2. Whether shader caches in cited M8-001 runs may go (~1.96 GB, no evidentiary value).
3. MCLA-Saves: 4.6 GB of F10 packages have no copy in `private`. Under the approved rule, none can be deleted. Freeing them needs a different rule (for example, copy-then-delete, or deletion for closed issues).
4. Whether M2-M6 task evidence (~8 GB) should get a per-report before/after review.
