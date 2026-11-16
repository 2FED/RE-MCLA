# Storage cleanup, round 6, 2026-11-16

The owner asked on 2026-11-16: "move everything that is already superfluous and out of date from private into private\_archive, and delete what can be deleted". An Opus subagent did the work under the AGENTS.md section "Owner-authorized storage maintenance". It used the method of round 5 (`M11-storage-cleanup-round5-20261114.md`) and the `_archive` conventions of the 2026-11-06 tidy (`M10-storage-cleanup-20261105.md`). The primary agent chose the groups (A to F below). Nothing was committed and no exe was launched. Before every target, and before every large inventory scan, the scripts ran `private\tools\wait-for-idle-pc.ps1 -Check`; on a busy result they would wait and re-check every 120 s. Every check reported idle, so no target had to wait. `MCLA-Launcher.exe` was not running at any `lNN-internal` move.

The per-target record is untracked because it holds local paths: `private/storage-maintenance-20261116/record-round6.tsv`. It has group, path, bytes, files, newest write, checks, detail (hashes, HEAD, logs copied, destination) and result for every target. Next to it:
- `buildlogs/<name>/`: the `.ninja_log`, `CMakeCache.txt`, `install_manifest.txt` and top-level `*.log` of each deleted build dir.
- `groupE-temp-tests-manifest.tsv`: name, bytes, files, mtime and link count of each deleted Temp test dir.
- `inventory-before-round6.tsv` and `inventory-after-round6.tsv`: every top-level entry of `private\` with bytes, files, links, newest write and last write.

The moves are also logged in `private/_archive/MOVES-20261116.tsv`, in the same format as `MOVES-20261106.tsv` (from, to, bytes, reason). To restore an item, move it back from there.

The scripts are in the scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\cleanup6-20261116\` (`lib.ps1`, `groupA.ps1`, `groupB.ps1`, `groupC.ps1`, `groupD.ps1`, `temp-tests.ps1`, `inventory.ps1`).

## Method (all groups)
- **Tooling:** PowerShell 7 (`private\tools\pwsh\pwsh.exe`), where `-Recurse` does not descend into links.
- **Dry run first:** groups A, C, D and E each ran once with `-DryRun`. The real run went ahead only after the result was reviewed.
- **Checks on every target:**
  - it is a real directory (or, in group A, a file);
  - neither it nor any ancestor up to `C:\` is a reparse point;
  - it contains 0 reparse points (true for every target);
  - no process command line or executable path names it.
- **Deletion:** `Remove-Item -LiteralPath <exact path> -Recurse -Force`, one target at a time, with no wildcards, then a check that the path is gone.
- **Moves:** `Move-Item -LiteralPath <exact path> -Destination <exact path>` on the same volume, so it is a rename, never a copy and delete. Each move needed the destination to be absent first. Afterwards the source had to be gone, and the destination had to have the same file count and total bytes.
- **Tickets:** signed tickets inside `lNN-int-input` and `lNN-internal` were not opened or changed. Whole directories were moved, and only file metadata was read.
- **Signing key:** it is in the current-user CNG store (`scripts/Mcla-TestBuildSigning.ps1`, key name `MCLA-R-TestBuild-Leases-v1`), not under `private\`. Nothing under `private\` has a key-like name.
- **Results:** no failures, no partial deletes, no in-use errors and no count or byte mismatches.

## Which builds are current
The launcher's Build folder is stored in `preferences.dat`, which was not opened. The fallback rule therefore applied: the two newest signed `lNN-internal` folders by `candidate-receipt.json` time are current.

| Build | Receipt time |
|---|---|
| l106-internal | 2026-11-16T13:29 |
| l103-internal | 2026-11-15T09:05 |

The whole l103 set stayed in place: internal, int-input, SHA256SUMS, arms, 38 arm helpers, both checklists and the prepare script. So did everything named `l106-*` except `l106-host-build` (see C).

**References:** no script that stays in place refers to a moved item.
- The l106 prepare script, checklist and arm helpers name only `l106-*`, `l105-sdk-install` and `l12`.
- The volunteer prepare and clean-clone scripts (`l65`, `l67`, `l70`, `l81v`) and the trim-lease templates name no `lNN-internal`, `-int-input` or `-arms` folder.
- Scratch generators did refer to moved items. `l106-work` (`mk_arms.py`, `mk_helpers.py`, `mk_prepare.py`, `mk_checklist.py`) used the `l105-*` files as templates. Older `lNN-work` generators read their predecessors' files, for example `l68-work\mk_prepare_l68.py` read the l66 files. A future l107 generator should take `l106-*` as its template.

## A. Superseded owner builds moved to `_archive`: 630 items, 4,851,562,520 bytes (4.52 GiB)
**Builds:** l59, l63, l66, l68, l69, l71 to l102, l104 and l105. l103 and l106 stayed in place.

**What moved:**
- each build's `lNN-internal`, `lNN-int-input`, `lNN-int-input.SHA256SUMS.txt`, `lNN-arms`, `lNN-arm-*.cmd`, `lNN-CHECKLIST*.txt`, `lNN-AMD-CHECKLIST.txt` and `lNN-internal-prepare.ps1`;
- `l59-deck-arms` with `l59-internal-prepare.ps1`, since only that script and the old README cite it.

**Layout:** the existing `_archive\<category>\<original name>` layout of 2026-11-06 was kept instead of a per-build folder:

| Destination | Items | What |
|---|---|---|
| `_archive\candidates\` | 130 | 107 directories (internal, int-input, arms, deck-arms) and 23 `*.SHA256SUMS.txt` |
| `_archive\scripts\` | 461 | arm helper `.cmd` files and `*-internal-prepare.ps1` |
| `_archive\misc\` | 39 | checklists |

There were no name collisions with the earlier archive entries.

**Not moved:**
- volunteer lines (`*-volunteer*`, and `l65`, `l67`, `l70` and `l81v` with their entries, pins, active-builds and clean-clone files);
- the trim-lease signing templates (`l49-trim-*`, `l70-trim-*`);
- `l12`, the CRT that the l106 prepare script stages from;
- `lNN-build` and `lNN-work` dirs, and every SDK install and object dir. These are listed in F.

## B. Symbols: all kept in place
Each of these `-sdk-o` dirs holds `Release\rexruntime.pdb` and `Release\rexgpu-xenos.pdb` for an SDK that is inside an owner build. That build is either still in place or now archived.

| -sdk-o | Bytes | SDK install | Builds |
|---|---|---|---|
| l92-sdk-o | 324,919,170 | l92-sdk-install | l92, l93, l94 (archived) |
| l97-sdk-o | 327,596,660 | l97-sdk-install | l97, l98 (archived) |
| l100-sdk-o | 329,141,540 | l100-sdk-install | l100 (archived) |
| l101-sdk-o | 330,264,020 | l101-sdk-install | l101, l102, l104 (archived); l103 (in place) |
| l105-sdk-o | 331,348,506 | l105-sdk-install | l105 (archived); l106 (in place) |

The protected dirs `l80-sdk-o`, `l81-sdk-o` and `l106-sdk-o` were not touched. `l106-sdk-o` is the build of SDK commit 6a4b82e, which no package ships.

## C. Rebuildable build dirs: 10 deleted, 3,116,546,409 bytes (2.90 GiB); 1 kept
**Host build trees:** 6 deleted, 1,279,529,098 bytes. In each one, `mcla.exe` has the same sha256 as the staged `lNN-int-input\mcla.exe` (6/6 match).

| Build | mcla.exe sha256 (prefix) | Bytes |
|---|---|---|
| l101 | `11AF32EC14A8358E` | 212,733,381 |
| l102 | `C7007A0865E48CBB` | 212,780,478 |
| l103 | `5F38E8647DD1584B` | 213,225,587 |
| l104 | `B130B0DA7B61E14F` | 213,328,902 |
| l105 | `1B29F64F6911DB3D` | 213,484,183 |
| l106 | `D569AE5B50775017` | 213,976,567 |

The checks were run before the int-inputs moved. For l106, the code of `l106-internal-prepare.ps1` (with comments excluded) does not mention `l106-host-build`. The script stages only from `l106-int-input`, and `l106-internal` was already signed and staged.

**SDK build dirs:** 3 deleted, 1,314,401,471 bytes.

| Dir | Bytes | Install rexruntime.dll (prefix) |
|---|---|---|
| l101-sdk-b | 437,676,739 | `87B7EBE7F45F6AEA` |
| l105-sdk-b | 437,281,284 | `74663325AFD8A447` |
| l106-sdk-b | 439,443,448 | no install; SDK 6a4b82e is not shipped |

None of them held any `rexruntime.pdb` or `rexgpu-xenos.pdb`, and every `-sdk-o` dir was kept.

**`l101-sdk-wt`:** removed, 522,615,840 bytes.
- `status --porcelain --ignore-submodules=all` was empty, with no ignored files.
- HEAD `a49e5113` equals the tip of `mcla/m11-l101`.
- Removal used the round-3/5 fallback: delete the folder by exact path, then `git worktree prune -v`. Prune removed exactly that entry.
- Branches: 40 `mcla/*` before and after. Tags: 60 before and after.
- `git worktree list` now shows `l39-sdk-src`, `l81-sdk-wt`, `l105-sdk-wt` and `l106-sdk-wt`.
- **Recovery:** `git worktree add <path> mcla/m11-l101`.

**Kept: `l101-sdk-install` (82,178,200 bytes).** l103 stays in place and was staged from it, as were l101, l102 and l104. Its `rexruntime.dll` (`87B7EBE7F45F6AEA...`) equals the staged one in all four int-inputs. The full hashes of all 4 binaries are in the record.

## D. Scratch host-source copies: 5 deleted, 746,230,565 bytes (0.69 GiB)
Deleted: `l101o-host-src` to `l105o-host-src` in `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\`. Each was a plain copy of 878 or 879 files, about 149 MB, with no `.git` and no reparse point. Kept: `l106o-host-src`, and every `*-work` and `*-patch` folder.

## E. Stale Temp test dirs: 113 deleted, 574,714,327 bytes (0.54 GiB)
The round-5 script was reused, with the 60-minute age cut-off and the round-5 name list. Two changes were made: the prefix `mcli-import-` was added, and idle checks now run every 20 dirs. The script removes link objects inside each dir as links, without following them, before it deletes the tree.

| Prefix | Dirs |
|---|---|
| `MclaStagingTest-*` | 1 |
| `mcli-import-*` | 112 |

There were 0 failures and no top-level links.

## Skipped or kept, with reasons
- **l103 build set:** current (second-newest signed internal).
- **`l101-sdk-install`:** l103 stays in place and was staged from it.
- **The five `-sdk-o` dirs:** symbols, see B.

## F. Not lNN-named, or outside A to E: inventory only (owner decides)
Sizes are from `inventory-after-round6.tsv`. None of these was moved or deleted. Purposes are guessed from names and top-level file names only.

| Item | GiB | Last write | Probable purpose | Proposal |
|---|---|---|---|---|
| `p1` | 6.26 | 2026-10-11 | M7-era portable bundle (bin, cache, diagnostics, bundle manifest) | archive, or delete after checking its `diagnostics` for unique evidence |
| `timebox-20261025-1` | 2.26 | 2026-10-28 | 2026-10-25 timebox l24 to l28 variant experiment (evidence/export) | archive as a group with the 4 items it names (below) |
| `sdk-rb32-s` | 1.51 | 2026-10-11 | old SDK source tree copy | delete (no current script names it; check for `.git` and local commits first) |
| `bundles` | 1.16 | 2026-10-10 | M7 Windows stock/profile bundles | archive |
| `host-arcade-swap-binding-20261024-1` | 0.56 | 2026-10-24 | arcade swap-binding host build (named by the timebox scripts) | archive with timebox |
| `autostart-motion-config-off-preparation-20261017-1`, `game-work-owner10-preflight-20261017-1.json`, `prepare-arcade-swap-binding-s-20261024-1.py` | about 0 | 2026-10-17/24 | named by the timebox scripts | archive with timebox |
| `save-archive` | 0.24 | 2026-09-05 | M6-014 / M7-009 save archives | keep (save lineage) unless the owner says otherwise |
| `s1`, `s2` | 0.08 each | 2026-10-25 | small portable bundles | archive |
| `baseline`, `fixtures` | 0.02, about 0 | 2026-08 | M2/M4 baselines and fixture | keep (tiny) or archive |
| `README-current.txt` | about 0 | 2026-11-06 | top-level map of 2026-11-06; now stale (describes l66 to l70) | rewrite to match the new top level |
| `m10-closure-draft.md`, `README.local.md`, `game-manifest.json`, `issue-drafts` | about 0 | | notes and manifests | keep |
| `config` | about 0 | | `dgx-spark.env` (may hold credentials) | keep; not opened |
| `tester-reports`, `owner-*`, `evidence`, `storage-*`, `tools`, `gate-host-build` | | | evidence, tools and gate | protected |

**lNN-named items left in place that the owner may want to clear next:**
- `l68-build` (0.19 GiB, owner build l68 now archived): delete or archive.
- `l68-work`, `l69-work` (about 0): archive.
- `l67-build`, `l70-build` (0.19 GiB each): build output that the volunteer prepare scripts read. Keep while those volunteer lines matter.
- `l82-sdk-install`, `l92-sdk-install`, `l97-sdk-install`, `l100-sdk-install` (0.08 GiB each): only archived builds use them. They are needed only to rebuild a host against those SDKs, so they could be deleted after their DLL hashes are recorded.
- `l92-sdk-o`, `l97-sdk-o`, `l100-sdk-o` (0.30 GiB each): symbols for archived builds. They could move into `_archive\sdk\`.
- `l67-sdk-work`, `l67v-work`, `l67-work`, `l70-work`: volunteer-line work dirs, kept.

## Free space (C:)
| Point | Bytes | GiB |
|---|---|---|
| Before | 98,529,804,288 | 91.76 |
| After | 102,805,196,800 | 95.74 |

The groups freed 4,437,491,301 bytes (4.13 GiB): C 2.90, D 0.69, E 0.54. Group A is a same-volume move, so it frees nothing. Free space also moved with other activity on the disk during the run: it went down by about 3 GiB at one point and came back.

## Lost raw replay / recovery limit
- **A:** none. Everything is in `_archive` with its original name, and `MOVES-20261116.tsv` is the map back. Docs and checklists that cite `private\lNN-*` paths for the moved builds are now stale by one path component.
- **C:** object files, incremental ninja state, test executables and `Launch-MCLA.exe` from the host trees, and the SDK CMake/object state. The staged `mcla.exe` stays authoritative, and the pdbs are kept in `-sdk-o`. The worktree can be restored exactly from its branch.
- **D:** none of value. The source identities are in the owner-build evidence files.
- **E:** test scratch only.

No evidence, captures, saves or tickets were inside any deleted target.

## For the owner / primary
1. **Proposal list F:** decide on `p1` (6.26 GiB), `timebox-20261025-1` with its 4 named items (2.82 GiB), `sdk-rb32-s` (1.51 GiB), `bundles` (1.16 GiB), `s1`/`s2`, `l68-build`, and the old SDK installs and symbols of archived builds.
2. **Current builds:** l103 was treated as current by the receipt-time rule. If the launcher actually points elsewhere (it is set in `preferences.dat`, which was not read), l103 can be archived like the others. Archiving it also releases `l101-sdk-install`.
3. **`README-current.txt`** is stale and should be rewritten for the new top level.
4. **`mcli-import-*` leak:** 112 dirs accumulated in Temp. The test that creates them should clean up after itself.
