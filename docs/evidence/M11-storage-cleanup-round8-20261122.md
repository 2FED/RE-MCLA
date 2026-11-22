# Storage cleanup, round 8, 2026-11-22

Free space on C: had fallen to about 41 GiB, down from 115 GiB two days earlier. Most of it went to owner builds l111 to l120, two volunteer builds (l117v, l120v), SDK worktrees and scratch. The primary agent chose the target groups (A to F below). An Opus subagent did the work under the AGENTS.md section "Owner-authorized storage maintenance", with the method, checks and record format of rounds 5 to 7 (`M11-storage-cleanup-round5-20261114.md`, `-round6-20261116.md`, `-round7-20261117.md`). Nothing was committed and no exe was launched.

Before every target, and before every inventory scan, the scripts ran `private\tools\wait-for-idle-pc.ps1 -Check`. On a busy result they would have waited and checked again every 120 s. Every check reported idle, so nothing waited. `MCLA-Launcher.exe` was not running during any `lNN-internal` move.

The per-target record is untracked because it holds local paths: `private/storage-maintenance-20261122/record-round8.tsv`. It uses the round-6/7 columns: group, path, bytes, files, newest write, checks, detail and result. Next to it:
- `buildlogs/<name>/`: the `.ninja_log`, `CMakeCache.txt` and `install_manifest.txt` of each deleted build dir (26 dirs).
- `git-refs-before.txt` and `git-refs-after.txt`: every `mcla/*` branch and tag of `l39-sdk-src` with its SHA. The two files are byte-identical.
- `inventory-before-round8.tsv` and `inventory-after-round8.tsv`: every top-level entry of `private\`.
- `scratch-inventory-before-round8.tsv` and `scratch-large-files-before-round8.tsv`: the scratch folder before group D.
- `saves-diagnostics-round8.tsv`: section F.

The moves are logged in `private/_archive/MOVES-20261122.tsv` (from, to, bytes, reason). To restore an item, move it back. The scripts are in the scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\cleanup8-20261122\`: `lib.ps1`, `plan.ps1`, `refs.ps1`, `moves.ps1`, `groupC.ps1`, `groupD.ps1`, `temp-tests.ps1`, `scratch-inv.ps1` and `inventory.ps1`. The plan and the reference hits are there too, in `plan-moves.tsv` and `refs.tsv`.

## Method
- **Tooling:** PowerShell 7 (`private\tools\pwsh\pwsh.exe`), where `-Recurse` does not descend into links.
- **Dry run first:** every group ran once with `-DryRun`. The dry run found 0 skips. The real run went ahead only after the result was reviewed.
- **Checks on every target:**
  - it is a real directory (or, for A, a file);
  - neither it nor any ancestor up to `C:\` is a reparse point;
  - it contains 0 reparse points (true for every target);
  - no process command line or executable path names it;
  - for targets that hold an exe, `MCLA-Launcher.exe` is not running.
- **Moves:** each move is a `Move-Item -LiteralPath` rename on the same volume, and the destination had to be absent first. Afterwards the source had to be gone, and the destination had to have the same file count and bytes.
- **Deletes:** each delete removed one exact path with `Remove-Item -LiteralPath -Recurse -Force`, with no wildcards, then checked that the path was gone.
- **Results:** no failures, no partial deletes, no in-use errors and no count or byte mismatches.
- **Reference check (before anything moved):** each candidate name, and each `lNN-arm-` prefix, was searched in the scripts and documents that stay. The search covered:
  - every text file in the `private\` root that stays;
  - `l57-work`, `l39-work`, `l119-arms` and `l120-arms`;
  - the top level of `tools`;
  - the `gate-host-build` CMake cache;
  - the tracked `scripts\` folder;
  - `docs/current-state.md` and `README-AI`.

  The search found no hits in script code. The hits it did find:
  - `l117-DECK-CHECKLIST.txt` appears twice in `docs/current-state.md`, which is a document. It was archived as asked, because the l119/l120 Deck checklists replace it.
  - `l119-`, `l120-`, `l117v-` and `l120v-host-build` are named only in comments of their prepare scripts. The code of those scripts stages only from `lNN-int-input` or `lNN-volunteer-input`, plus `l12` and `gate-host-build`.

## Which builds stay
Per the brief, these stay:
- the current owner build l120 and the previous signed owner build l119, each as a whole set: internal, int-input, SHA256SUMS, arms, arm helpers, checklists, prepare script, and the SDK install, `-sdk-o` and worktree;
- the published volunteer line (`l117v-*`, `l81v-*`, the l65/l67/l70 lineage);
- the unsigned hotfix candidate `l120v-*`.

Only the rebuildable dirs in C were removed from these sets.

## A. Superseded owner builds moved to `_archive`: 1,294 items, 1,883,508,498 bytes (1.75 GiB)
**Builds:** l103 and l106 to l118 (14 builds). l113 and l118 never had an `lNN-internal` folder, because they were not signed.

**What moved:** each build's `lNN-internal`, `lNN-int-input`, `lNN-int-input.SHA256SUMS.txt`, `lNN-arms`, `lNN-arm-*.cmd`, `lNN-CHECKLIST.txt`, `lNN-AMD-CHECKLIST.txt` and `lNN-DECK-CHECKLIST.txt` (l117, l118), and `lNN-internal-prepare.ps1`. l117, the base of `l117-DECK-CHECKLIST.txt`, was archived as asked.

| Destination | Items | Bytes | What |
|---|---|---|---|
| `_archive\candidates\` | 54 | 1,879,736,708 | 12 `-internal`, 14 `-int-input`, 14 `-arms` and 14 `*.SHA256SUMS.txt` |
| `_archive\scripts\` | 1,223 | 3,641,234 | arm helper `.cmd` files and 14 `*-internal-prepare.ps1` |
| `_archive\misc\` | 17 | 130,556 | checklists |

The archive layout of 2026-11-06 was kept. There were no name collisions.

## B. SDK symbols and installs moved to `_archive\sdk`: 24 items, 5,015,344,560 bytes (4.67 GiB)
- **What moved:** `lNN-sdk-o` and `lNN-sdk-install` for l101, l105, l107 to l110, l112 to l114, and l116 to l118. There are 12 of each; l111 and l115 had none of their own.
- **Symbols:** every `-sdk-o` holds 7 pdbs, including `Release\rexruntime.pdb` and `Release\rexgpu-xenos.pdb`. All were moved, not deleted. The full `rexruntime.dll` hashes are in the record.
- **No live users:** no staying script names these dirs. The l119 and l120 prepare scripts pin their own SDK DLL hashes from `l119-sdk-install` (c503e32) and `l120-sdk-install` (c9c579c). The volunteer scripts use `l117v-sdk-install` and `l120v-sdk-install`.
- **`l116-sdk-install`:** this is the published SDK state d889a16. It went with the others. The commit stays in `l39-sdk-src` (branch `mcla/m11-l116`, tag `v0.20.0.12`), and `l117v-sdk-install` holds the same SDK.

| SDK install | rexruntime.dll sha256 (prefix) | SDK install | rexruntime.dll sha256 (prefix) |
|---|---|---|---|
| l101 | `87B7EBE7F45F6AEA` | l112 | `246034EEA505B098` |
| l105 | `74663325AFD8A447` | l113 | `1AC2AC54AA90C9A9` |
| l107 | `2B264954DC466DB1` | l114 | `DBCACC5D5E8FCD23` |
| l108 | `2767E8B71E3A2ECB` | l116 | `8A9F2F9A3E3A1AEF` |
| l109 | `7D5F8B0ACF48782A` | l117 | `D2EBF2721F4F898B` |
| l110 | `1B2C80ED329A612C` | l118 | `7BD349FBDC27E10A` |

## C. Rebuildable trees deleted: 38 dirs, 15,153,235,599 bytes (14.11 GiB)
**Host build trees:** 12 deleted, 2,584,918,436 bytes. In each one, `mcla.exe` has the same sha256 as the staged input (12/12 match; full hashes are in the record).

| Tree | Compared with | sha256 (prefix) |
|---|---|---|
| l111 | `l111-int-input\mcla.exe` | `E2A614BBBD3459AB` |
| l112 | `l112-int-input` | `BE3C6F5D9D3AE3E8` |
| l113 | `l113-int-input` | `723EA94431C010F6` |
| l114 | `l114-int-input` | `8056565B34F0470D` |
| l115 | `l115-int-input` | `86EC4C37803C98B0` |
| l116 | `l116-int-input` | `63EA88473AD6A6FC` |
| l117 | `l117-int-input` | `9906955170AF7AFA` |
| l118 | `l118-int-input` | `BA0D6577A084E616` |
| l119 | `l119-int-input` | `D3EE68F2412303C8` |
| l120 | `l120-int-input` | `3A6A0A36A2F9FC44` |
| l117v | `l117v-volunteer-input` | `A65B6E7E86863758` |
| l120v | `l120v-volunteer-input` | `CD59A0FB4762394A` |

The l111 to l118 hashes were taken from the int-inputs before group A moved them. Each tree is about 215 MB.

**SDK build dirs (`-sdk-b`):** 14 deleted, 6,292,946,815 bytes. These were l107 to l110, l112 to l114, l116 to l120, l117v and l120v, about 441 to 459 MB each. No `-sdk-b` held a `rexruntime.pdb` or `rexgpu-xenos.pdb`, so the symbols are only in the `-sdk-o` dirs, which were kept or archived. The install `rexruntime.dll` hash of each one is in the record.

**SDK worktrees:** 12 removed, 6,275,370,348 bytes. These were l105 to l110, l112 to l114, and l116 to l118, about 523 MB each.
- **Checks:** `status --porcelain --ignore-submodules=all` was empty for each one, with no ignored files, and HEAD equals the tip of its `mcla/m11-lNN` branch.
- **Removal:** the round-3/5/6 fallback was used: delete the folder by exact path, then `git worktree prune -v` in `l39-sdk-src`. Each prune removed exactly the one entry.
- **Branches and tags:** 53 `mcla/*` branches and 61 tags before and after, all with the same SHAs. `v0.20.0.12` is present.
- **`git worktree list`:** now shows `l39-sdk-src`, `l81-sdk-wt`, `l119-sdk-wt`, `l120-sdk-wt` and `l120v-sdk-wt`. No l117v worktree exists.
- **Recovery:** `git worktree add <path> mcla/m11-lNN`.

## D. Scratch: 15 dirs deleted, 2,242,081,654 bytes (2.09 GiB)
- **Deleted:** `l106o-host-src` to `l119o-host-src` and `l117v-host-src` in `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\`. Each one was a plain copy of 882 or 888 files, about 149 MB, with no `.git` and no reparse point.
- **Kept:** `l120o-host-src`, `l120v-host-src` and every `*-work`, `*-patch` and `*-design` folder.
- **Large artefacts in `*-work` folders:** none. No file over 200 MB exists in any of them; the largest work folder is `ki082-clip2-work`, at 71 MB.
- **`ki082-clip-work` and `ki082-clip2-work`:** no raw decode or frame dump exists. The only images are analysis outputs, kept as small images:
  - `ki082-clip-work`: 13 JPG frames and contact sheets, 2.0 MB;
  - `ki082-clip2-work`: 31 PNG and 2 PGM contact sheets, crops and grids, 70 MB.

  Nothing was deleted there.
- **Source clips:** they were verified anyway, and match their recorded sha256:
  - `owner-l107-20261117`: `ee54da7c...` (156,740,076 bytes);
  - `owner-l120-20261122`: `8c6b54b4...` (221,998,913 bytes) and `dd0b7321...` (238,678,155 bytes).

## E. Temp test leftovers: 0
The round-6 `temp-tests.ps1` was reused unchanged, with the 60-minute age cut-off, the round-5 prefix list plus `mcli-import-`, and link-safe deletion. It found 0 candidates and 0 top-level links. No `Mcla*`/`mcla*`/`mcli*` dir exists in `%LocalAppData%\Temp` at all, so the leak fixes of the l103 era hold.

## Skipped or left, with reasons
- **Nothing in A to E was skipped by a check.**
- **Left in the root on purpose:**
  - `l107-host-build` to `l110-host-build` (860,431,974 bytes, about 215 MB each): rebuildable trees of the now-archived builds, but outside the round-8 list, which named host builds 111..120. They are candidates for round 9 (compare with `_archive\candidates\lNN-int-input\mcla.exe`).
  - `l106-sdk-o` (331,394,564 bytes): pdbs of SDK 6a4b82e, built for l106 but never shipped (l106 used `l105-sdk-install`). It was not on the B list. Symbols are never deleted: move it to `_archive\sdk\` or keep it.
  - `l80-sdk-o`, the protected volunteer and SDK items, and everything on the never-touch list.

## F. Report only
**The 20 largest top-level items in `private\` after the round:**

| Item | GiB | Item | GiB |
|---|---|---|---|
| `_archive` | 24.13 | `bundles` | 0.47 |
| `evidence` | 16.40 | `owner-l120-20261122` | 0.43 |
| `p1` | 6.26 | `owner-l100-20261114` | 0.39 |
| `tools` | 2.21 | `l120-sdk-o` | 0.32 |
| `l39-sdk-src` | 1.55 | `l119-sdk-o` | 0.32 |
| `l120-sdk-wt` | 0.49 | `l120v-sdk-o` | 0.32 |
| `l119-sdk-wt` | 0.49 | `l117v-sdk-o` | 0.31 |
| `l120v-sdk-wt` | 0.49 | `l106-sdk-o` | 0.31 |
| `l81-sdk-wt` | 0.48 | `l81-sdk-o` | 0.30 |
| `l80-sdk-o` | 0.30 | `owner-l88-flicker-20261111` | 0.30 |

`private\` now has 365 top-level entries; there were 1,721 before.

**`C:\BDU\MCLA-Saves\<profile>\diagnostics`:** 10,557,345,797 bytes (9.83 GiB) in total. In round 5 it was 6.77 GiB.

| Profile | GiB | Packages | Newest | Older than 14 days (GiB / packages) |
|---|---|---|---|---|
| MCLA-5b71... | 3.76 | 78 | 2026-11-22 | 1.59 / 41 |
| MCLA-68e5... | 2.81 | 41 | 2026-11-18 | 0 / 0 |
| MCLA-7d6f... | 0.99 | 39 | 2026-11-10 | 0.97 / 37 |
| MCLA-6ed6... | 0.60 | 12 | 2026-11-03 | 0.60 / 12 |
| MCLA-f5d8... | 0.46 | 25 | 2026-10-31 | 0.46 / 25 |
| MCLA-bc1c... | 0.32 | 6 | 2026-11-03 | 0.32 / 6 |
| MCLA-254a... | 0.32 | 13 | 2026-11-08 | 0.29 / 11 |
| MCLA-4c45... | 0.22 | 12 | 2026-11-06 | 0.22 / 12 |
| MCLA-d69a... | 0.19 | 3 | 2026-11-10 | 0 / 0 |
| MCLA-d5d7... | 0.11 | 1 | 2026-11-01 | 0.11 / 1 |
| 5 other profiles | 0.05 | 7 | | about 0 |

**Proposed pruning rule (for the owner; nothing was deleted):**
- keep every package whose id, or its 12-character prefix, appears in `docs/known-issues.md`, `docs/evidence/*.md` or `README-AI`;
- keep every package from the newest 14 days;
- keep the newest package of each profile;
- move the rest to an archive outside C:, or delete it, after its file list and hashes are recorded.

A dry count of that rule found 148 packages older than 14 days:
- 11 of them (0.18 GiB) are named in the docs and would stay;
- 137 (4,700,065,800 bytes, 4.38 GiB) are not named anywhere and would go.

The docs name 18 package ids in total.

## Free space (C:)
| Point | Bytes | GiB |
|---|---|---|
| Before | 44,495,466,496 | 41.44 |
| After | 61,611,388,928 | 57.38 |

The deletes freed 17,395,317,253 bytes (16.20 GiB): C 14.11 and D 2.09. A and B are same-volume moves, so they free nothing. The measured gain is 15.94 GiB; the difference is other disk activity during the run.

## Lost raw replay / recovery limit
- **A and B:** none. Everything is in `_archive` with its original name, and `MOVES-20261122.tsv` maps it back. Docs and checklists that cite `private\lNN-*` paths for the moved builds, for example `docs/current-state.md` on `l117-DECK-CHECKLIST.txt`, are now stale by one path component.
- **C:** for each deleted tree, these are gone:
  - object files, incremental ninja state, test executables, `Launch-MCLA.exe` and the build-local `MCLA-Launcher.exe`, crash handler and image helper of the host trees;
  - the CMake and object state of the SDK builds.

  The staged `mcla.exe` and launcher files in each int-input or volunteer-input stay authoritative. The pdbs are kept in `-sdk-o`, and the build logs are kept in the record folder. The worktrees can be restored exactly from their branches.
- **D:** none of value. The source identities are in the owner-build evidence files.
- **E:** nothing deleted.

No evidence, captures, saves, tickets or owner material was inside any deleted target.

## For the owner / primary
1. **Round-9 candidates:** `l107-host-build` to `l110-host-build` (0.80 GiB) and `l106-sdk-o` (0.31 GiB, symbols: archive, never delete).
2. **MCLA-Saves diagnostics** are 9.83 GiB and growing, because packages are unlimited in internal builds. The pruning rule in F would free about 4.4 GiB now. It needs an owner yes, and a choice between archive and delete.
3. **`docs/current-state.md`** still points at `private/l117-DECK-CHECKLIST.txt`. It is now under `private/_archive/misc/`.
4. **`private\README-current.txt`** was updated to the new set of builds on disk (l119 and l120 sets, l117v, l120v, and the list of what remains for a decision).
