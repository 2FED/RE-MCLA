# Storage cleanup, round 7, 2026-11-17

Round 6 ended with a proposal list (section F of `M11-storage-cleanup-round6-20261116.md`). The owner answered: "We still take the game from p1, so p1 must NOT be deleted or moved anywhere for now. Everything else can more or less go to the archive and/or be deleted." The primary agent turned that into a target list and an Opus subagent did the work under the AGENTS.md section "Owner-authorized storage maintenance". It used the method, record format and safety rules of round 6. Nothing was committed and no exe was launched.

The owner was running game tests during this round. Before every target the script ran `private\tools\wait-for-idle-pc.ps1 -Check`. On a busy result it waited and checked again every 120 s. The game ran from 12:44 to about 12:56, and the dry run waited through those six checks. No target was handled while `mcla.exe` was running.

The per-target record is untracked because it holds local paths: `private/storage-maintenance-20261117/record-round7.tsv`. It uses the round-6 columns: group, path, bytes, files, newest write, checks, detail and result. Next to it:
- `buildlogs/<name>/`: the `.ninja_log` and `CMakeCache.txt` of each deleted build tree.
- `sdk-rb32-s-uncommitted.diff`: the uncommitted diff of `sdk-rb32-s`, saved before the move (see B).
- `inventory-before-round7.tsv` and `inventory-after-round7.tsv`: every top-level entry of `private\`.

The moves are logged in `private/_archive/MOVES-20261117.tsv`, in the same format as before (from, to, bytes, reason). To restore an item, move it back. The scripts are in the scratch folder `%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\cleanup7-20261117\` (`lib.ps1`, `refs.ps1`, `round7.ps1`, `inventory.ps1`). The reference search results are in `refs.tsv` in the same folder.

## Method
- **Tooling and checks:** the same as round 6. The scripts ran in PowerShell 7 and used a dry run first. Every target was checked for reparse points (itself, its ancestors and its contents) and for processes that name it.
- **Moves:** each move is a `Move-Item -LiteralPath` rename on the same volume, and the destination had to be absent first. Afterwards the source had to be gone, and the destination had to have the same file count and bytes.
- **Deletes:** each delete removed one exact path with `Remove-Item -LiteralPath -Recurse -Force`, then checked that the path was gone.
- **Launcher gate:** `MCLA-Launcher.exe` was running from `l106-internal` between sessions. A target was held back while the launcher ran only if it contained a `mcla.exe` or `MCLA-Launcher.exe`, so it could be a launcher Build folder. Five targets were held back: `timebox-20261025-1`, `host-arcade-swap-binding-20261024-1` and the three build trees. They were done at 13:22, after both the launcher and the game had exited.
- **Reference check:** before anything moved, each candidate name was searched in the scripts and documents that stay:
  - every text file in the `private\` root (prepare scripts, arm helpers, checklists, volunteer and trim files);
  - `private\l57-work` and `private\l39-work`;
  - the top level of `private\tools`;
  - the tracked `scripts\` folder;
  - `docs/current-state.md` and `README-AI`.

  A hit in a script that stays means the item is skipped. Hits in `README-current.txt` were ignored, because that file is stale (see "For the primary").

## A. Moved to `_archive`: 30 items, 5,960,576,760 bytes (5.55 GiB)
| Destination | Items | Bytes | What |
|---|---|---|---|
| `_archive\m7-m8\` (new) | 5 | 3,026,627,344 | `timebox-20261025-1` (2,427,222,378 bytes, 17,410 files) and the four items its scripts name: `host-arcade-swap-binding-20261024-1`, `autostart-motion-config-off-preparation-20261017-1`, `game-work-owner10-preflight-20261017-1.json`, `prepare-arcade-swap-binding-s-20261024-1.py` |
| `_archive\sdk\` | 8 | 2,931,012,854 | `l82-`, `l92-`, `l97-` and `l100-sdk-install`; `l92-`, `l97-` and `l100-sdk-o`, with each one's 7 pdbs, `rexruntime.pdb` and `rexgpu-xenos.pdb` included (moved, not deleted); `sdk-rb32-s` (see B) |
| `_archive\work-logs\` | 13 | 2,872,496 | `l67-work`, `l67v-work`, `l67-sdk-work`, `l68-work`, `l69-work`, `l70-work`, and 7 clean-clone logs (4 from l67, 3 from l70) |
| `_archive\scripts\` | 3 | 47,020 | `l65-`, `l67-` and `l70-clean-clone.ps1` |
| `_archive\misc\` | 1 | 17,046 | `m10-closure-draft.md` |

**Notes on the moves:**
- **Timebox scripts:** they also name `p1`, `sdk-rb32-s` and `evidence`. Their references do not hold anything back, because the timebox scripts are now in the archive. `p1` and `evidence` stayed in place.
- **`host-arcade-swap-binding-20261024-1`:** the tracked `scripts/mcla_arcade_swap_phase_inventory.py` names it, but only in a docstring that pins a dependency file, and the docstring says "No file is read here". That is not a live reference.
- **The four SDK installs:** no staying script and neither the l103 nor the l106 prepare script names them. The record holds their `rexruntime.dll` sha256 (prefixes `374ED110`, `440D8435`, `947446A5`, `D2941A85`).
- **`m10-closure-draft.md`:** a subagent draft from 2026-11-03, written for README-AI section 17b. M10 is recorded as closed in README-AI, and the "M11 inputs" section was placed there. Nothing names the draft as current.

## B. `sdk-rb32-s`: archived, not deleted
- `.git` exists. HEAD is `5dd58f89` (tag `v0.20.0.0`, detached), and `git -C private\l39-sdk-src cat-file -t` gives `commit` for it.
- **`git status` is not clean.** Two files have uncommitted changes:
  - `src/graphics/shaders/resolve_downscale.cs.hlsl` (+19 lines);
  - `src/graphics/shaders/bytecode/d3d12_5_1/resolve_downscale_cs.h` (the regenerated bytecode).
- The rule said to archive in this case, so it moved to `_archive\sdk\sdk-rb32-s` (1,621,993,415 bytes, 20,748 files), and the diff was also saved to the record folder (33,440 bytes).

## C. Deleted: 3 build trees, 609,252,501 bytes (0.57 GiB)
Each tree's `mcla.exe` has the same sha256 as the staged or archived input:

| Tree | Bytes | Compared with | sha256 (prefix) |
|---|---|---|---|
| `l67-build` | 202,506,361 | `l67-vol-input\mcla.exe` | `1B4642487AC6684B` |
| `l68-build` | 203,343,282 | `_archive\candidates\l68-int-input\mcla.exe` | `F4A954191C154BEF` |
| `l70-build` | 203,402,858 | `l70-vol-input\mcla.exe` | `146FBBB5FB9ADC4E` |

The `l67-` and `l70-volunteer-prepare.ps1` scripts name their build tree only in a comment. Their code stages from `lNN-vol-input`.

## Skipped, with reasons
| Item | Reason |
|---|---|
| `bundles` | **Live references in tracked scripts.** `Invoke-MclaM7LocalStock.ps1`, `Invoke-MclaF10StockCandidate.ps1`, `Invoke-MclaF10WgcStockCandidate.ps1`, `Invoke-MclaCleanProfileStock.ps1`, `Invoke-MclaPinkslipStockCandidate.ps1`, `Invoke-MclaSandboxStock.ps1`, `Test-MclaM7LocalContract.ps1`, `stage-short-profile-candidate.ps1`, `stage-subtitle-candidate.ps1`, `build-portable-campaign-bundle.ps1` (default output), `analyze-portable-deck-performance.ps1`, `mcla_arm_stage.py` and `mcla_stock_startup_probe.py` all use `private\bundles\M7-*` / `M8-001-*` paths. The untracked root `Run M7 ... .cmd` launchers call several of them. |
| `s1` | **Live reference:** `scripts/stage-subtitle-candidate.ps1` (destination `private/s1`). |
| `s2` | **Live reference:** `scripts/Invoke-MclaSubtitleCandidate.ps1` (bundle `private/s2`). |
| `l12` | **Live reference:** the CRT source (`-CrtDirectory "$P\l12"`) of `l103-` and `l106-internal-prepare.ps1` and of all four volunteer prepare scripts (`l65`, `l67`, `l70`, `l81v`). |
| `l49-trim-*` (3 files), `l56-entries.json` | The never-touch list says to leave trim-lease and `*-entries.json` lineage files in place this round. `l56-entries.json` is also read by `l65-volunteer-prepare.ps1` (`$oldEntries`). |
| `p1` and everything on the protected list | Not touched. |

The only other hits were in `README-current.txt`, in README-AI (for `l49-trim-leases-prepare.ps1`, in the M9-005 ledger) and in historical evidence docs. The move logs map those.

## Free space (C:)
| Point | Bytes | GiB |
|---|---|---|
| Before | 109,238,042,624 | 101.74 |
| After | 107,013,738,496 | 99.67 |

The deletes freed 0.57 GiB, and the moves free nothing because they stay on the same volume. Free space still went down by 2.07 GiB during the run because of other disk activity. A new SDK worktree, `private\l107-sdk-wt`, appeared while the round ran; this round did not create or touch it, and the owner's game session was also writing to the disk.

## Lost raw replay / recovery limit
- **A and B:** none. Everything is in `_archive` with its original name, and `MOVES-20261117.tsv` maps it back. `sdk-rb32-s` keeps its `.git` and its uncommitted changes.
- **C:** the object files, ninja state, test executables, `Launch-MCLA.exe` and the build-local `MCLA-Launcher.exe` of the three trees are gone. The staged `mcla.exe` stays authoritative, and the ninja log and CMake cache of each tree are kept.

## For the primary / owner
1. **`README-current.txt` is stale.** It still describes l66 to l70 and lists items that are now archived or deleted. It was not edited.
2. **`bundles`, `s1` and `s2` are still referenced by tracked M7-era scripts.** Archiving them needs either a decision that those scripts and the root `Run M7 ... .cmd` launchers are retired, or a path update.
3. **`l12`** is the live CRT source for every prepare script and stays.
4. **Trim-lease and `l56-entries.json` files** remain in place, as asked for this round.
