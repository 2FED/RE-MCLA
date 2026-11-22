# Storage cleanup, round 9, 2026-11-22

Owner approval on 2026-11-22: "yes, delete; and in general you can delete the stale diagnostics, as we planned earlier". This round takes the two round-9 candidates left by [round 8](M11-storage-cleanup-round8-20261122.md), and applies the F10 diagnostics pruning rule proposed in its section F. An Opus subagent did the work under the AGENTS.md section "Owner-authorized storage maintenance", with the method, checks and record format of round 8. Nothing was committed and no exe was launched.

`private\tools\wait-for-idle-pc.ps1 -Check` ran before every target, and every 25 packages during the hashing pass. Every check reported idle, so nothing had to wait.

**Record:** `private/storage-maintenance-20261122/record-round9.tsv`. It is untracked, because it holds local paths and full profile ids.
- Group 1 uses the round-8 columns.
- Group 2 has one row per F10 package, 726 in all, kept or deleted. Each row gives the profile, session, package, bytes, files, the rule letter and its reason, the sha256 of every non-private file, the size-only entries for private items, the checks and the result.

Next to it are `saves-diagnostics-round9-before.tsv` and `-after.tsv`, the per-profile sizes. The scripts and the plan (`plan.ps1`, `plan-diag.tsv`, `group1.ps1`, `group2.ps1`, `diagsize.ps1`, `after.ps1`, `sesstext.ps1`) are in the scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\cleanup9-20261122\`. They use a copy of the round-8 `lib.ps1` that points at the round-9 record.

## Method
- **Tooling:** PowerShell 7 (`private\tools\pwsh\pwsh.exe`). Every group ran once with `-DryRun`, and the result was reviewed before the real run.
- **Checks on every target:**
  - it is a real directory;
  - neither it nor any ancestor up to `C:\` is a reparse point;
  - it contains no reparse points (0 found);
  - for Group 1, no process names it, and `MCLA-Launcher.exe` is not running.
- **Group 2 also re-checked, just before each delete:**
  - the path has the form `<profile>\diagnostics\<session>\live\live-<date>...`;
  - the package date is before 2026-11-08;
  - the byte count equals the plan.
- **Deletes:** each delete was an exact `Remove-Item -LiteralPath -Recurse -Force` with no wildcards, followed by a check that the path was gone.
- **Results:** no failures, no skips and no partial deletes.

## 1. Build trees and unshipped SDK symbols: 5 dirs deleted, 1,191,826,538 bytes (1.11 GiB)
In each tree, `mcla.exe` has the same sha256 as the archived `_archive\candidates\lNN-int-input\mcla.exe` (4 of 4 match). Each tree's `.ninja_log` and `CMakeCache.txt` are kept in `buildlogs/lNN-host-build/`.

| Target | Bytes | mcla.exe sha256 (prefix) |
|---|---|---|
| `l107-host-build` | 214,998,182 | `94C1B8336D7E8E2C` |
| `l108-host-build` | 215,035,629 | `0D6CE767029AFC23` |
| `l109-host-build` | 215,180,737 | `D93F690CA032C576` |
| `l110-host-build` | 215,217,426 | `150527C13ACCCE93` |
| `l106-sdk-o` | 331,394,564 | (SDK symbols, see below) |

**`l106-sdk-o` (SDK `6a4b82e`, 7 pdbs).** The brief allowed a delete only if no staged or archived build uses this SDK. That was confirmed in two ways:
- **Docs:** README-AI, `M11-priorities.md` and the l106 evidence all say that `6a4b82e` was "not installed": l106 shipped the l105 DLLs.
- **Hashes:** its `rexruntime.dll` (`F53F354B026FF8D8...`) matches none of the 107 `rexruntime.dll` files in the staged and archived `lNN-internal`, `-int-input` and `-volunteer-input` folders. It also matches none of the 162 under `private\bundles` and `private\_archive`. `l106-int-input` carries `74663325AFD8A447...`, the same file as `l105-sdk-install`.

Because no build ships this SDK, the dir was deleted, not archived. Commit `6a4b82e` stays in `l39-sdk-src` (branch `mcla/m11-l106`), so the pdbs can be rebuilt from it, though not bit for bit.

## 2. F10 diagnostics pruned: 56 packages deleted, 540,188,576 bytes (0.50 GiB)
The scope was `C:\BDU\MCLA-Saves\<profile>\diagnostics\<session>\live\<package>` only. Nothing in `sessions\`, saves, snapshots, `runtime\` logs, `xma-dump\` or the session-level `report.txt` and `latest-live.txt` was touched.

**Private items:** `save-private` folders, `save-files-private.json` and `process-private.dmp` were never opened or hashed. The record lists only their existence and byte size. The other package files were hashed: `frame.bmp`, `log-tail.txt`, `manifest.json`, `report.txt`, `state.json` and `save-metadata.json`.

**Keep rules.** A package was kept if any of these held, checked in this order:
- **(a)** it is named in `docs/known-issues.md`, `docs/M11-priorities.md`, `docs/current-state.md`, README-AI or any text file under `docs/evidence` (623 files). A package counts as named when any of these appears:
  - its package id;
  - its `YYYYMMDDTHHMMSSZ-pid` form;
  - its bare timestamp;
  - its session id, either in full or as any hex prefix of 8 or more characters. A session match keeps every package of that session.
- **(b)** its package date is 2026-11-08 or later;
- **(c)** it is the newest package of its profile;
- **(d)** it is a crash package. All of these are kept, because KI-085 is open. There is 1 crash package, outside `live\`, and it was not touched;
- **(e)** it could not be classified, or it holds owner-typed text in its `report.txt`. 0 packages fell under (e).

None of the deleted packages, and none of their sessions' report files, holds owner-typed answers.

| Decision | Packages | Bytes |
|---|---|---|
| keep (a): named in the docs | 614 | 8,222,635,346 |
| keep (b): newest 14 days | 53 | 732,519,737 |
| keep (c): newest of profile | 3 | 29,088,732 |
| keep (d): crash | 1 (outside `live`) | not counted |
| keep (e) | 0 | 0 |
| **deleted** | **56** | **540,188,576** |

**Why far fewer were deleted than the round-8 dry count (137 packages, 4.38 GiB).** The round-8 rule matched package ids only. The round-9 rule also keeps every package whose session id the docs mention. Many evidence files cite sessions only by an 8-hex prefix (for example the KI-074, l63, l79 and l103 analyses, and the M9-008 owner-report inventory). Of the 325 packages dated before 2026-11-08:
- 208 are kept only by a session-id mention;
- 58 are named by package id or timestamp (the round-8 count found 11 of these). All 58, including the 11, are kept;
- 3 are kept by rule (c), as the newest package of their profile;
- 56 had no keep rule, and were deleted.

No package appeared since round 8: the scan found the same 726 packages, and the same 9.83 GiB total. The difference makes the delete set smaller, not larger, so the run went ahead.

Sessions cited only by an earlier storage report, for example `34b8579d…` and `5d582fcc…` in round 4, were kept under (a) as written.

**Deleted by profile:**

| Profile | Deleted | Bytes | Dates |
|---|---|---|---|
| 4c45 | 8 | 33,878,468 | 2026-10-25 |
| 5b71 | 20 | 201,436,191 | 2026-10-28..11-05 |
| 6ed6 | 10 | 146,842,524 | 2026-11-01..11-03 |
| f5d8 | 18 | 158,031,393 | 2026-10-25..10-28 |

The deletes emptied 30 `live\` folders, and those were removed. No session folder became empty, because each still holds `runtime\` logs or report files, so all session folders stay.

## 3. Report only
**Diagnostics size per profile:**

| Profile | Before (bytes) | After (bytes) | Live packages before -> after |
|---|---|---|---|
| 5b71 | 4,041,979,895 | 3,840,543,704 | 266 -> 246 |
| 68e5 | 3,014,645,419 | 3,014,645,419 | 226 |
| 7d6f | 1,064,981,471 | 1,064,981,471 | 73 |
| 6ed6 | 639,366,298 | 492,523,774 | 44 -> 34 |
| f5d8 | 496,913,170 | 338,881,777 | 35 -> 17 |
| bc1c | 346,384,669 | 346,384,669 | 24 |
| 254a | 338,762,258 | 338,762,258 | 11 |
| 4c45 | 236,671,490 | 202,793,022 | 27 -> 19 |
| d69a | 202,217,734 | 202,217,734 | 9 |
| d5d7 | 116,644,283 | 116,644,283 | 6 |
| d66b, 2983, f7d8, 4f6b, af26 | 58,779,110 | 58,779,110 | 5 |
| **Total** | **10,557,345,797 (9.83 GiB)** | **10,017,157,221 (9.33 GiB)** | **726 -> 670** |

**The 10 largest remaining packages** are all rule (a), and all about 38 MB each:

| Profile | Session | Package | Bytes |
|---|---|---|---|
| bc1c | `fa7fdf70` | `live-20261103T082218Z-27016-1` | 38,004,861 |
| 5b71 | `6031364a` | `live-20261106T102252Z-37272-3` | 37,969,249 |
| 5b71 | `fce31190` | `live-20261029T152633Z-20800-3` | 37,880,361 |
| d5d7 | `9bedbbaa` | `live-20261101T135741Z-41508-3` | 37,879,974 |
| 5b71 | `fce31190` | `live-20261029T152552Z-20800-2` | 37,850,336 |
| 5b71 | `e614faa6` | `live-20261031T092230Z-42608-2` | 37,843,436 |
| f5d8 | `a7710173` | `live-20261011T081410Z-41152-3` | 37,793,919 |
| f5d8 | `a7710173` | `live-20261011T081400Z-41152-2` | 37,792,173 |
| 5b71 | `fce31190` | `live-20261029T152507Z-20800-1` | 37,743,217 |
| 5b71 | `e614faa6` | `live-20261031T092156Z-42608-1` | 37,739,197 |

**`private\owner-*`:** the rule cannot delete anything there, because its scope is only `C:\BDU\MCLA-Saves\<profile>\diagnostics\<session>\live`, and the scripts never enumerate `private\`. These owner folders are separate evidence copies, so they stay outside the rule.

A read-only count found 25 `owner-*` folders holding 190 `live-*` package copies, 44 of them dated before 2026-11-08. Applying the rule there by mistake would put some of those copies at risk. Any future version of the rule must keep its fixed `MCLA-Saves` root.

## Free space (C:)
| Point | Bytes | GiB |
|---|---|---|
| Before | 61,253,967,872 | 57.05 |
| After | 62,990,753,792 | 58.66 |

The deletes freed 1,732,015,114 bytes (1.61 GiB): Group 1 1.11 GiB and Group 2 0.50 GiB. The measured gain is 1,736,785,920 bytes.

## Lost raw replay / recovery limit
- **Group 1:**
  - **Host trees:** the object files, ninja state, test executables and build-local launchers of the four host trees are gone. The archived int-inputs stay authoritative.
  - **`l106-sdk-o`:** its pdbs and objects are gone. They belong to an SDK that no package ships, and the source commit stays.
- **Group 2:** the 56 deleted packages cannot be recovered. They held frame captures, log tails, state, save metadata, F10 minidumps and save copies. None of them is cited by package id, session or timestamp in any tracked document. The record keeps their file list, sizes and hashes.

## For the owner / primary
1. The diagnostics are still 9.33 GiB, because most old packages belong to sessions cited by an 8-hex prefix in evidence files. A narrower rule would free more:
   - one option is to keep a cited session's packages only when the citation names a package, or the session belongs to an open KI;
   - another is to move cited sessions to an archive outside C:.

   Either needs an owner decision.
2. Profile 68e5 (3.01 GiB, 226 packages, all from 2026-11-08 or later) will become prunable under rule (b) as it ages, unless its sessions are cited.
