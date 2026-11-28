# Storage cleanup, round 10, 2026-11-28

The owner asked on 2026-11-28: "run the disk cleanup now, please". An Opus subagent did the work under the AGENTS.md section "Owner-authorized storage maintenance", with the method, checks and record format of [round 8](M11-storage-cleanup-round8-20261122.md) and [round 9](M11-storage-cleanup-round9-20261122.md). Nothing was committed, nothing was signed, no exe from the project was launched, and no key, ticket, save, snapshot or `sessions\` folder was opened or changed.

`private\tools\wait-for-idle-pc.ps1 -Check` ran before every target, and every 25 packages during the diagnostics hashing pass. Every check reported idle, so nothing had to wait.

**Record:** `private/storage-maintenance-20261128/record-round10.tsv`. It is untracked, because it holds local paths and full profile ids.
- Groups 1 and 2 use the round-8 columns.
- Group 3 has one row per F10 package, 674 in all. Each row gives the rule letter and its reason, the tighter-rule dry verdict, the sha256 of every non-private file, size-only entries for private items, the checks and the result.

Next to it are:
- `buildlogs/l121-host-build/` and `buildlogs/l121-sdk-b/`: `.ninja_log`, `CMakeCache.txt` and `install_manifest.txt`;
- `git-refs-before.txt` and `git-refs-after.txt`: byte-identical;
- `inventory-before-round10.tsv` and `inventory-after-round10.tsv`: the `private\` top level;
- `saves-diagnostics-round10-before.tsv` and `-after.tsv`: per-profile sizes.

The moves are in `private/_archive/MOVES-20261128.tsv`. The scripts and plans are in `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\cleanup10-20261128\`: `lib.ps1`, `plan.ps1`, `plan-diag.tsv`, `plan-moves.tsv`, `group1.ps1`, `moves.ps1`, `worktree.ps1`, `worktree-fallback.ps1`, `group3.ps1`, `diagsize.ps1`, `inventory.ps1`, `temp-survey.ps1` and `after.ps1`. They are copies of the round-8 and round-9 scripts, pointed at the round-10 record.

## Method
- **Tooling:** PowerShell 7 (`private\tools\pwsh\pwsh.exe`). Every group ran once with `-DryRun`, and the result was reviewed before the real run.
- **Checks on every target:**
  - it is a real directory (or a file, for the helper exes);
  - neither it nor any ancestor up to `C:\` is a reparse point;
  - it contains 0 reparse points (true for every target);
  - no process names it;
  - `MCLA-Launcher.exe` is not running where a target holds an exe.
- **Deletes:** each one is an exact `Remove-Item -LiteralPath` with no wildcards, followed by a check that the path is gone. **Moves:** each one is a same-volume `Move-Item -LiteralPath`. The destination had to be absent first, and file count and bytes were compared afterwards.
- **Results:** no failures, no skips by check, no partial deletes and no count mismatches.
- **A dry-run bug that was caught.** In the first group-1 dry run, the hash helper was named `H`, which PowerShell resolves to its alias for `Get-History`. Every hash came back empty, and the comparison "matched" empty against empty. The helper was renamed, and an empty or failed hash now fails the check. Only the corrected run produced the hashes below.

## 1. l121 build by-products: 4 targets deleted, 675,892,682 bytes (0.63 GiB)

| Target | Bytes | Identity check |
|---|---|---|
| `private\l121-host-build` | 215,836,890 | sha256 equal to `l121-int-input` for `mcla.exe` (`B52C866C4A5AD2BD...`), `MCLA-Launcher.exe` (`C18B08F0...`), crash handler (`9A41D164...`), image helper (`664912FA...`) and `TracyClient.dll` (`7B268EB2...`) |
| `private\l121-sdk-b` | 459,483,376 | no `rexruntime.pdb` or `rexgpu-xenos.pdb` inside, and no built DLL copies inside. The `l121-sdk-install\bin` DLLs equal the staged ones: `rexruntime.dll` `B734A799CBEFD174...`, `rexgpu-xenos.dll` `6440093F...`, `TracyClient.dll` |
| `l121-work\check_sparse.exe` (scratch) | 286,208 | compiled helper; `check_sparse.cpp` and `build_check_sparse.ps1` stay |
| `l120-work\check_sparse.exe` (scratch) | 286,208 | the same |

`l121-internal-prepare.ps1` stages only from `l121-int-input`. The host tree is named only in its comments.

**Kept, although named in the brief:**
- **`private\l121-sdk-o`** (342,841,638 bytes): it holds the 7 pdbs of the current owner build (`Release\rexruntime.pdb`, `Release\rexgpu-xenos.pdb` and others). By the round-8 precedent, symbols are never deleted, and the current build keeps its `-sdk-o`. It can be archived once l121 is superseded.
- **`l121o-host-src` and `l120o-host-src`** (about 0.57 MB each): these are the 4-file owner-variant overlays (`mcla_dev_overrides.h`, `mcla_m10_defaults.h` and their tests), not full source trees. They are not in git, and they were last written on 2026-11-26.
- **Everything else in `l121-work` and `l120-work`:** recipe scripts, logs, checklist bodies and doc drafts, all small text.

## 2. Superseded owner build l119: 122 items archived (569,210,520 bytes) and its worktree removed (523,298,554 bytes)
With l121 current and l120 previous, l119 is now two builds back, which is where l118 stood in round 8. It was handled the same way.

| Destination | Items | Bytes | What |
|---|---|---|---|
| `_archive\candidates\` | 4 | 145,177,196 | `l119-internal`, `l119-int-input`, `l119-int-input.SHA256SUMS.txt`, `l119-arms` |
| `_archive\scripts\` | 114 | 455,593 | 113 `l119-arm-*.cmd` and `l119-internal-prepare.ps1` |
| `_archive\misc\` | 2 | 24,164 | `l119-CHECKLIST.txt`, `l119-DECK-CHECKLIST.txt` |
| `_archive\sdk\` | 2 | 423,553,567 | `l119-sdk-install` (`rexruntime.dll` `C09485B396AF5E2F...`) and `l119-sdk-o` (7 pdbs, moved, not deleted) |

- **Reference check:** `l119-` paths occur only in documents: `l120-CHECKLIST.txt` line 76, and `docs/current-state.md` on `l119-DECK-CHECKLIST.txt`, which now lives under `_archive\misc\`. The l120 prepare script names l119 only in comments. No gate script under `l57-work` or `l39-work`, and nothing in `scripts\`, references l119, l120 or l121 paths.
- **`l119-sdk-wt`:** `git worktree remove` (without `--force`) refused. The worktree's `thirdparty/FFmpeg/.git` gitlink points to a modules directory that does not exist, so `git status` fails with code 128. Every worktree round 8 removed was in the same state. Round 8's fallback was used:
  - checks: status clean (`--ignore-submodules=all`), HEAD `c503e32` equal to the tip of `mcla/m11-l119`, no reparse points, and the same bytes and file count as at the check;
  - then an exact-path delete, then `git worktree prune -v`, which removed exactly the one entry.
- **Branches and tags:** 54 `mcla/*` branches and 61 tags, the same before and after, with identical ref files. `git worktree list` now shows `l39-sdk-src`, `l81-sdk-wt`, `l120-sdk-wt`, `l120v-sdk-wt` and `l121-sdk-wt`.
- **l120 (the previous build):** kept as a full set, including `l120-sdk-install`, `l120-sdk-o` and `l120-sdk-wt`. Round 8 kept the SDK install, `-sdk-o` and worktree of the build one before current (then l119), and archived them only once a build fell two behind.

## 3. F10 diagnostics: 21 packages deleted, 335,852,198 bytes (0.31 GiB)
The rule is the round-9 keep rule, unchanged except for the 14-day window, which now starts on 2026-11-14. The scope was `C:\BDU\MCLA-Saves\<profile>\diagnostics\<session>\live\<package>` only.
- **Private items:** `save-private`, `save-files-private.json` and `*.dmp` were never opened or hashed; the record gives their size only.
- **Corpus:** 627 doc files, naming 113 package ids, 162 timestamps and 4,907 hex tokens.
- **Re-checks before each delete:** the path form, a date before 20261114, and a byte count equal to the plan.
- **Owner text:** none in the delete set's session reports.
- **Wider search:** a search of all of `docs/` and README-AI, beyond the rule's corpus, found none of the deleted sessions or package times.
- **Folders:** 8 `live` folders were left empty and removed. Session folders were not touched.

| Decision | Packages | Bytes |
|---|---|---|
| keep (a): named in the docs (package, timestamp or session prefix of 8+ hex) | 618 | 8,272,224,533 |
| keep (b): dated 2026-11-14 or later | 29 | 359,486,987 |
| keep (c): newest of its profile | 6 | 66,269,284 |
| keep (d): crash | 1 (in `crash\`, outside `live`, not touched) | not counted |
| keep (e): unclassifiable | 0 | 0 |
| **deleted** | **21** | **335,852,198** |

**Deleted by profile:**

| Profile | Packages | Dates |
|---|---|---|
| 254a | 1 | 2026-11-08 |
| 68e5 | 8 | 2026-11-12 (one session) |
| 7d6f | 4 | 2026-11-08 |
| d69a | 8 | 2026-11-10 |

**Size:** diagnostics went from 10,116,008,787 bytes (9.42 GiB, 674 live packages) to 9,780,156,589 bytes (9.11 GiB, 653).

**Tighter rule, dry count only (nothing acted on).** Of the 393 packages dated before 2026-11-14:
- 65 are named by package id or timestamp;
- 301 are kept only because their session is cited.

Under the proposed tighter rule, a cited session is kept only when a package is named, or when the citing KI row is Open. Read strictly, the citation must sit in a `docs/known-issues.md` row whose status starts with "Open". On that reading the rule would also free **289 packages, 3,805,608,243 bytes (3.54 GiB)**, and keep:
- 10 packages, cited by Open KI rows;
- 2 packages, as newest of their profile.

A lenient reading also counts citations in the 19 evidence files linked from Open KI rows. It would keep 17 more packages and free 272 packages (3,662,678,660 bytes, 3.41 GiB).

This report deliberately names no kept session ids, so that it cannot itself become a citation that keeps them under (a).

## 4. %LocalAppData%\Temp: nothing deleted
- **Whole folder:** 258 MB; only 3 items, 8 KB in total, have been quiet for 7 days.
- **Claude scratch:** every task folder was last written on 2026-11-22 or later, so none meets the 7-day quiet age. These are `deck119-work`, `l120v-work`, `ki082-clip2-work` (71 MB) and `cleanup8-20261122`, plus the protected `ki090-work`, `l120-work`, `l121-work`, `cleanup9` and `cleanup10`. They become candidates from about 2026-11-29 to 11-30.
- **Not ours:** the `whazza-*` test folders belong to another project, and they and the other recent items are skipped.
- **Test leftovers:** no `Mcla*`, `mcla*` or `mcli*` dir exists.

## 5. Report only
**Free space on C:**

| Point | Bytes | GiB |
|---|---|---|
| Start (first reading, transient) | 13,771,927,552 | 12.83 |
| Before (stable, about 1 min later) | 39,844,646,912 | 37.11 |
| After | 41,279,934,464 | 38.44 |

The deletes freed 1,535,043,434 bytes (1.43 GiB): group 1 0.63, the worktree 0.49 and the diagnostics 0.31. The measured gain is 1,435,287,552 bytes; the difference is other disk activity.

The free space fell from 58.66 GiB after round 9 to 37.1 GiB. That is about 21.5 GiB, and none of it is in the areas this round covers:
- `private\` top-level totals are unchanged (`_archive` 24.13 GiB, `evidence` 16.40 GiB before this round);
- diagnostics grew by 4 packages;
- Temp holds 258 MB.

The 26 GB swing at the start points to a large writer outside the project, such as a WSL disk, the page file or Windows Update.

**The 15 largest items in `private\` after the round:**

| Item | GiB | Item | GiB |
|---|---|---|---|
| `_archive` | 24.66 | `l81-sdk-wt` | 0.48 |
| `evidence` | 16.40 | `bundles` | 0.47 |
| `p1` | 6.26 | `owner-l100-20261114` | 0.39 |
| `tools` | 2.21 | `l121-sdk-o` | 0.32 |
| `l39-sdk-src` | 1.55 | `l120-sdk-o` | 0.32 |
| `owner-l120-20261122` | 0.49 | `l120v-sdk-o` | 0.32 |
| `l121-sdk-wt` | 0.49 | | |
| `l120-sdk-wt` | 0.49 | | |
| `l120v-sdk-wt` | 0.49 | | |

`private\` has 365 top-level entries now; it had 490 before.

**The 15 largest items under `C:\BDU\MCLA-Saves`** (12.43 GiB in total):
- `sessions\` folders, by size only and never opened: profile 5b71 1.26 GiB, 68e5 0.46, 7d6f 0.39, f5d8 0.31, 254a 0.15, 4c45 0.14;
- nine diagnostics sessions, all kept under (a) or (b): six of profile 68e5 (0.34, 0.33, 0.26, 0.26, 0.24 and 0.19 GiB) and three of profile 5b71 (0.30, 0.20 and 0.13 GiB).

## Lost raw replay / recovery limit
- **Group 1:**
  - **Host tree:** the object files, ninja state, test executables, `Launch-MCLA.exe` and the build-local launcher, crash handler and image helper copies of the l121 host tree are gone. `l121-int-input` stays authoritative, with byte-identical exes and DLLs.
  - **`-sdk-b`:** its CMake and object state is gone. The pdbs stay in `l121-sdk-o`, and the build logs are in the record folder.
  - **`check_sparse.exe`:** both copies are gone, and can be rebuilt from their source.
- **Group 2:** nothing is lost. Everything is in `_archive`, and `MOVES-20261128.tsv` maps it back. The worktree can be restored exactly with `git worktree add <path> mcla/m11-l119`; its submodule contents come from the pinned commits.
- **Group 3:** the 21 packages cannot be recovered. They held frame captures, log tails, state, save metadata, F10 minidumps and save copies. None is cited anywhere in `docs/` or README-AI. The record keeps their file lists, sizes and hashes.

## For the owner / primary
1. **The tighter diagnostics rule** would free about 3.5 GiB more (289 packages). It needs an owner yes, and a choice between delete and archive off C:.
2. **The 21.5 GiB of free space lost since 2026-11-22 is outside the project** (`private\`, MCLA-Saves and Temp are flat). It is worth a look with a disk-usage tool, for example at a WSL disk image or Windows Update.
3. **Owner decisions still pending:**
   - `l121-sdk-o`: kept as the current build's symbols; archive it when l121 is superseded;
   - `l80-sdk-o` (0.30 GiB of old symbols);
   - `l117v-volunteer-oldguide`;
   - `_archive` (24.66 GiB) and `evidence` (16.40 GiB) could move off C:.
4. **Scratch work folders** `deck119-work`, `l120v-work`, `ki082-clip2-work` and `cleanup8` reach the 7-day quiet age around 2026-11-29 to 11-30. Their results are in `docs/evidence`.
5. **`docs/current-state.md`** still cites `private/l119-DECK-CHECKLIST.txt`, which is now in `private/_archive/misc/`. `private/README-current.txt` was refreshed to the disk state after this round.
