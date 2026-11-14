# Storage cleanup, round 5, 2026-11-14

On 2026-11-14 free space on C: had fallen to about 80 GiB. The owner said: "you can already clean part of it if there is something to delete". An Opus subagent did the work under the AGENTS.md section "Owner-authorized storage maintenance". The primary agent chose the delete set (groups A to F below). Nothing was committed and no exe was launched. Before every target, and before every large inventory scan, the subagent ran `private\tools\wait-for-idle-pc.ps1 -Check`. It reported idle (no `mcla.exe`) every time, so no target had to wait.

The per-target record is untracked because it holds local paths: `private/storage-maintenance-20261114/record-round5.tsv`. It has group, path, bytes, files, newest write, checks, detail (hashes, HEAD, logs copied) and result for every target. Next to it:
- `buildlogs-round5/<name>/`: the build logs copied from each deleted build dir. These are `.ninja_log`, `CMakeCache.txt`, `install_manifest.txt` and top-level `*.log`.
- `groupD-install-binaries.tsv`: the sha256 of every dll/exe in each deleted SDK install.
- `groupF-temp-tests-manifest.tsv`: name, bytes, files, mtime and link count of each deleted Temp test dir.
- `inventory-round5.tsv`: sizes of what is left.

The scripts are in the scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\cleanup5-20261114\` (`lib.ps1`, `groupA.ps1` to `groupE.ps1`, `temp-tests.ps1`, `inventory.ps1`).

## Method (all groups)
- **Tooling:** PowerShell 7 (`private\tools\pwsh\pwsh.exe`), where `-Recurse` does not descend into links.
- **Dry run first:** each group ran once with `-DryRun` and the result was reviewed. Only then did the real run go ahead.
- **Checks on every target:**
  - it is a real directory;
  - neither it nor any ancestor up to `C:\` is a reparse point;
  - it contains 0 reparse points (true for every target in groups A to E);
  - no process command line or executable path names it.
- **Deletion:** `Remove-Item -LiteralPath <exact path> -Recurse -Force`, one target at a time, with no wildcards, then a check that the path is gone.
- **Results:** no partial deletes and no in-use errors.
- **References:** current builds do not refer to any target. The l101/l102 files, the `gate-host-build` CMake cache and the `l101-sdk-b` CMake cache were searched for `l82`..`l100` `-host-build`/`-sdk-*` paths, with no hits. The `lNN-internal-prepare.ps1` scripts stage only from `lNN-int-input`. They name the SDK install only in comments and in an error-message string.

## A. Host build trees l86 to l100: 15 deleted, 3,169,595,603 bytes (2.95 GiB)
`l86-host-build` to `l100-host-build`. For each one, the staged `private\lNN-int-input\mcla.exe` exists and its sha256 equals the build tree's `mcla.exe` (15/15 match):

| Build | mcla.exe sha256 (prefix) | Build | mcla.exe sha256 (prefix) |
|---|---|---|---|
| l86 | `BD40FBAE8919D45A` | l94 | `D5092823C90AD120` |
| l87 | `58EE7CBE67242E3F` | l95 | `D085AC4E45C1CDE9` |
| l88 | `28712C69272294CD` | l96 | `A504D3A3B12032D9` |
| l89 | `1763ADFDE8A82578` | l97 | `1B0D5ED816450D20` |
| l90 | `1BE7C6594FC574AB` | l98 | `C123CEDC5D5DE1CF` |
| l91 | `5ED67A1300476306` | l99 | `858328275AA4C0D5` |
| l92 | `53AE16C9E43E8DA8` | l100 | `FF7AA4A3CDACB9E7` |
| l93 | `6D6B55142F706C80` | | |

The full hashes are in the record. `.ninja_log` and `CMakeCache.txt` of each tree are preserved.

## B. SDK build/object dirs: 17 deleted, 6,559,046,033 bytes (6.11 GiB); 3 kept
- **Deleted:**
  - `-sdk-b` for l82, l88, l90, l91, l92, l95, l96, l97, l99, l100;
  - `-sdk-o` for l82, l88, l90, l91, l95, l96, l99.
- **Checks:** every one has its matching `lNN-sdk-install\bin\rexruntime.dll`, hashed before the delete; its SDK commit is in `l39-sdk-src` (`cat-file -t` = commit).
- **DLL check:** for l88/l90/l91/l95/l96/l99, the install DLL equals the staged `lNN-int-input\rexruntime.dll`.

| SDK | Commit | rexruntime.dll sha256 (prefix) |
|---|---|---|
| l82 | c40db4b | `374ED110DED78CB3` |
| l88 | 7cbfe3b | `A131722571820693` |
| l90 | 4ec7abd | `5CD118E2DB60ED19` |
| l91 | ef72fd1 | `922D32C74B4F35AC` |
| l92 | e6d6bd3 | `440D8435858626DF` |
| l95 | f9f5178 | `2FFE32EAC6B9F65E` |
| l96 | ccac775 | `2CC5143E1A1964F5` |
| l97 | 6c5792d | `947446A5FDEAE6B1` |
| l99 | d3d7b6c | `102E18663A81A6B7` |
| l100 | 1e81149 | `D2941A85E23D848C` |

**Kept (symbols for staged builds):**
- `l92-sdk-o` (324,919,170 bytes; used by l92 to l94);
- `l97-sdk-o` (327,596,660 bytes; used by l97 and l98);
- `l100-sdk-o` (329,141,540 bytes).

Each holds `Release\rexruntime.pdb` and `Release\rexgpu-xenos.pdb`.

**Lost symbols:** the deleted `-sdk-o` dirs held the only `rexruntime.pdb`/`rexgpu-xenos.pdb` for the SDKs of l82 (used by l82 to l87), l88/l89, l90, l91, l95, l96 and l99. Neither the installs nor the `-internal` folders hold pdbs. Crash dumps from those staged builds now symbolize only to export level. A rebuild from the recorded commit is not guaranteed to produce a pdb that matches the shipped DLL.

## C. SDK worktrees: 10 removed, 5,214,474,459 bytes (4.86 GiB)
Removed: `l82`, `l88`, `l90`, `l91`, `l92`, `l95`, `l96`, `l97`, `l99` and `l100-sdk-wt`, all belonging to `private\l39-sdk-src`.

Checks:
- `status --porcelain --ignore-submodules=all` was empty for every worktree, with no untracked and no ignored files.
- HEAD equals the tip of its own `mcla/m11-lNN` branch, and the HEAD SHA is in the record.

Removal used the round-3 fallback: delete the folder by exact path, then `git worktree prune -v`. Prune removed exactly those 10 entries. `git worktree list` now shows only `l39-sdk-src`, `l81-sdk-wt` and `l101-sdk-wt`. 38 `mcla/*` branches and 60 tags are present; no branch or tag was deleted.

**Recovery:** `git worktree add <path> mcla/m11-lNN`.

## D. SDK installs: 6 deleted, 490,567,402 bytes (0.46 GiB)
Deleted: `l88`, `l90`, `l91`, `l95`, `l96` and `l99-sdk-install`.
- Their commits are in the repository.
- Before deletion, all 4 dll/exe files of each install were hashed into `groupD-install-binaries.tsv`.
- The staged `rexruntime.dll` of each matching `lNN-int-input` is the same file.

Kept: the installs for 67, 81, 82, 92, 97, 100 and 101.

## E. Scratch host-source copies: 42 deleted, 1,963,031,951 bytes (1.83 GiB)
Deleted: in `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\`, every `lNN*-host-src` with NN <= 100:
- `l68`, `l69`, `l71`, `l71b`, `l72`, `l73`;
- `l74` to `l81` and their `o` variants, plus `l81v`;
- `l82o` to `l100o`.

Each one was a plain copy, with no `.git` file or directory inside and no reparse point:
- l68 to l73 held only `scripts\bootstrap.ps1`;
- l74 to l87 were partial copies of 44 to 61 files;
- l88o to l100o were full copies of about 149 MB each.

Their source identities are in the matching owner-build evidence files. Every `*-work` and `*-patch` folder is kept, and so are `l101o-host-src` and `l102o-host-src`.

## F. Stale Temp test dirs: 862 deleted, 15,534,693,857 bytes (14.47 GiB)
The round-1 script `temp-tests.ps1` was reused, with the age cut-off changed to 60 minutes and an idle check added. It removed 862 matching top-level dirs in `%LocalAppData%\Temp` with 0 failures. Before each tree delete, it removed the 263 link objects (junction/symlink) inside them as links, without following them.

Nearly all of the space was in `MclaStagingTest-*`: 28 dirs, 13.26 GiB, about 570 MB per run. Each one is a full staged-build copy left behind by a staging test run. The rest:

| Prefix | Dirs | Size |
|---|---|---|
| `mcla-game-verifier-test` | 38 | 0.74 GiB |
| `mcla-controller-test` | 38 | 0.42 GiB |
| `mcla-build-verifier-test` | 38 | 0.04 GiB |
| `mcla-image-client-junction`, `mcla-store-seed-test`, `mcla-lease-runtime`, `MclaStoreTest`, `mcla-snapshot-test`, `mcla-launcher-fs-test`, `mcla-diagnostic-export-test` | 112 to 119 each | about 0 |

## Skipped
`l92-sdk-o`, `l97-sdk-o` and `l100-sdk-o` were kept on purpose (symbols, see B). Every other target passed every check.

## Report only (not touched)
**Largest remaining items in `private\`:**

| Item | GiB |
|---|---|
| `evidence` | 16.40 |
| `_archive` | 6.79 |
| `p1` | 6.26 |
| `timebox-20261025-1` | 2.26 |
| `tools` | 2.21 |
| `l39-sdk-src` | 1.55 |
| `sdk-rb32-s` | 1.51 |
| `bundles` | 1.16 |
| `host-arcade-swap-binding-20261024-1` | 0.56 |
| `l101-sdk-wt` | 0.49 |
| `l81-sdk-wt` | 0.48 |
| `l101-sdk-b` | 0.41 |
| `owner-l100-20261114` | 0.39 |
| `l101-sdk-o` | 0.31 |
| `l100-sdk-o` | 0.31 |
| `l97-sdk-o` | 0.31 |
| `l92-sdk-o` | 0.30 |
| `l81-sdk-o` | 0.30 |
| `l80-sdk-o` | 0.30 |
| `owner-l88-flicker-20261111` | 0.30 |

**`private\owner-*`:** 20 items, 1.91 GiB in total. The largest are:
- `owner-l100-20261114` 0.39 GiB;
- `owner-l88-flicker-20261111` 0.30 GiB;
- `owner-deck-20261103b` 0.25 GiB;
- `owner-deck-20261103` 0.22 GiB;
- `owner-l94-20261112` 0.16 GiB.

**`C:\BDU\MCLA-Saves\<profile>\diagnostics`:** 6.77 GiB in total.

| Profile | GiB |
|---|---|
| MCLA-5b71... | 2.78 |
| MCLA-7d6f... | 0.99 |
| MCLA-68e5... | 0.78 |
| MCLA-6ed6... | 0.60 |
| MCLA-f5d8... | 0.46 |
| MCLA-254a... | 0.32 |
| MCLA-bc1c... | 0.32 |
| MCLA-4c45... | 0.22 |
| MCLA-d69a... | 0.19 |
| MCLA-d5d7... | 0.11 |
| 5 other profiles | about 0 each |

**Scratch root:** 0.74 GiB in total. The largest items are:
- `l102o-host-src` and `l101o-host-src`, 0.14 GiB each;
- `06cd90d3-...`, 0.10 GiB;
- the `*-work` folders, 0.01 to 0.03 GiB each.

## Free space (C:)
| Point | Bytes | GiB |
|---|---|---|
| Before | 86,473,117,696 | 80.53 |
| After | 119,565,176,832 | 111.35 |

The groups freed 32,931,409,305 bytes (30.67 GiB). The rest of the difference is other activity on the disk.

## Lost raw replay / recovery limit
- **A:** object files, incremental ninja state, the test executables and `Launch-MCLA.exe` that existed only in the build trees. The staged `mcla.exe` stays authoritative.
- **B:** SDK object and CMake state, plus the pdbs listed in B.
- **C:** none. Each worktree can be restored exactly from its branch.
- **D:** the install headers and libs needed to rebuild a host against those SDKs. They can be regenerated from the commit, but not byte-identically.
- **E:** none of value. The trees are recorded in evidence.
- **F:** test scratch only.

No evidence, captures or saves were inside any target.

## For the owner / primary
1. **`MclaStagingTest-*` leak:** these dirs come back at about 570 MB per staging test run, roughly 13 GiB since the last round. The staging test should clean up after itself, or the periodic cleanup should keep running.
2. **Next round:** candidates are `p1` (6.26 GiB), `timebox-20261025-1` (2.26 GiB), `sdk-rb32-s` (1.51 GiB), `bundles` (1.16 GiB), `host-arcade-swap-binding-20261024-1` (0.56 GiB), and old non-newest MCLA-Saves diagnostics (round 3 found no duplicates in `private`). Each needs its own check.
3. **Symbols:** symbols for the l82 to l91, l95, l96 and l99 SDKs were deleted as approved. If any of those staged builds may still produce crash dumps worth symbolizing, say so; the symbols cannot be recovered.

Note on the record: the three B `SKIPPED` rows appear twice, because the dry run also logged its skips. No other group's dry run wrote rows.
