# Storage cleanup, 2026-11-05

Owner request 2026-11-05 ("free up some space"); inventory and execution by Opus subagents under the AGENTS.md storage-maintenance rules; accepted by the primary agent. Free space 33.6 to 83.5 GiB. The full per-path deletion record (with sizes, reasons and recovery) and the inventory plan are kept under `private/storage-maintenance-20261105/` (not tracked: they contain local paths). Nothing deleted is unrecoverable: build intermediates rebuild from the pinned SDK tags and main; clean clones re-clone from main (l56-src only from local main until the M10 push); removed SDK worktrees had all commits on the published fork branch and their branches remain in the SDK repo. Kept: saves, game, evidence, tester archives, owner Deck exports, tools, l34-src, l39-sdk-src, the hot-blocks worktree (unpublished commits), l67-sdk-wt, l65/l67 clones, current installs, l67/l68 builds and candidates.

Authority: AGENTS.md "Owner-authorized storage maintenance"; target list = `storage-plan-20261105.md` (Tier A in full, selected Tier B groups named by the primary). Per-path record (path, group, class, bytes, files, reason, recovery, result): `storage-delete-record-20261105.tsv` in the agent scratch folder.

## Pre-checks
- Processes: no mcla.exe, cmake, ninja or clang running. The only MCLA process was `private\l67-volunteer\MCLA-Launcher.exe` (pid 30488). It is not a target and was still running from the same path after cleanup.
- For every target: the parent is `C:\BDU\MCLA-Recomp\private`, `%LocalAppData%\Temp` or the agent scratch folder. No target is a reparse point. No ancestor up to the drive root is a reparse point. Sizes were measured before deletion without following links. The only reparse points inside targets were the 14 `source-junction` entries in the staging fixtures.
- First run aborted safely: a PowerShell 5.1 `Split-Path -LiteralPath -Parent` parameter-set error made every parent check fail. The script therefore skipped all targets and deleted nothing. It was stopped, its record was archived as `cleanup/aborted-run1-record.tsv`, the parent lookup was fixed to `[IO.Path]::GetDirectoryName`, and the script was rerun.

## Free space (C:)
| | bytes | GiB |
|---|---|---|
| before | 36,050,145,280 | 33.57 |
| after | 89,628,364,800 | 83.47 |
| freed (drive delta) | 53,578,219,520 | 49.90 |

## Per-group totals (logical bytes measured before deletion)
| Group | Items | GiB | Result |
|---|---|---|---|
| A: private SDK build/object dirs + host build dirs (l34..l66, l66/l66c, Vulkan -b/-o, l58vk-host-b, l67/l67b-host-build, etc.) | 93 | 25.82 | all deleted |
| A: agent scratch older than 1 day (patch/tree/work dirs) | 46 | 1.70 measured, ~1.63 freed | deleted except 5,125 `*.md`/`*.py` files (70.3 MiB) left in place in 31 dirs |
| A: %LocalAppData%\Temp entries older than 7 days (incl. 2 old empty MclaStagingTest dirs from Oct 10-12) | 5,873 | 0.13 | 5,859 deleted, 14 skipped |
| B: MclaStagingTest-* fixtures (listed 20) | 20 | 5.85 | all deleted; for the 14 with a junction, the junction was removed first with `cmd /c rmdir` and its target was confirmed present afterwards |
| B: clean clones l43/l45/l46/l48/l49/l52/l56-src | 7 | 11.67 | all deleted |
| B: l67/l67v sdk-b/-o object dirs | 4 | 1.32 | all deleted (installs kept) |
| B: Vulkan installs l58vk/l59vk/l63vk-sdk-install | 3 | 0.55 | all deleted |
| B: superseded dev SDK installs (l34,l38,l39,l41,l43,l47,l48,l50,l52..l58,l61,l62) | 17 | 1.27 | all deleted |
| B: pre-M9 lanes (host-*-b/-s/-o, sdk-*-b, sdk-swap-b2, sdk20b) | 34 | 3.87 | all deleted |
| B: SDK linked worktrees | 8 | 3.54 | removed with `git worktree remove --force` + `git worktree prune` |

The logical total is about 55.7 GiB. The drive delta is 49.9 GiB because of the files left in place and differences in cluster allocation and compression.

## Worktree verification (fork/mcla/mcla-r-hotfixes = c63895c)
| Worktree | Branch | HEAD | Ancestor of fork branch | `status --porcelain --ignore-submodules=all` | Result |
|---|---|---|---|---|---|
| l47-sdk-wt | detached | daa1ae5 | yes | empty | removed |
| l50-sdk-wt | mcla/ki-047-diag | 851f03b | yes | empty | removed |
| l53-sdk-wt | mcla/m10-perf-split | 483458d | yes | empty | removed |
| l57-sdk-wt | mcla/m10-cpu-waits | 62cd66d | yes | empty | removed |
| l58-sdk-wt | mcla/m10-readback | 141e13f | yes | empty | removed |
| l60-sdk-wt | mcla/m10-prewarm | 9d8ede4 | yes | empty | removed |
| l61-sdk-wt | mcla/m10-ki047-targeted | 561a737 | yes | empty | removed |
| l62-sdk-wt | mcla/m10-watch | 43577c8 | yes | empty | removed |

All branch refs remain in `private\l39-sdk-src`. Remaining worktrees: l39-sdk-src (main), l66-sdk-wt (hotblocks, unpublished), l67-sdk-wt.

## Skipped / left in place
- `%LocalAppData%\Temp\PowerToysMSIInstaller_2026-10-28`: in use by another process.
- `%LocalAppData%\Temp\WinSAT`, `%LocalAppData%\Temp\msdtadmin`: access denied. Not retried with elevation.
- 11 `%LocalAppData%\Temp\mcla-snapshot-test-*` dirs (0 bytes): each contains a file named `trailing.` (trailing dot) that Win32 path APIs cannot delete. They were left in place. Removing them later needs `\?\`-prefixed deletion, which was not attempted.
- Inside the 31 agent-scratch dirs, `*.md` and `*.py` files were kept per the "never *.md notes or *.py" rule. Most of them are copies of the repository tree or edit scripts. All other files and the now-empty directories were removed.

## Kept (verified present after cleanup)
l67-build, l68-build, l68-host-build, l68-vram-build, l65-src, l67-src, l34-src, l39-sdk-src, l66-sdk-wt, l67-sdk-wt, l65/l65v/l66/l66c/l67/l67v-sdk-install, sdk-rb32-s, l67-volunteer, evidence, p1, the agent scratch l68-* dirs and the current session folder. Not touched: MCLA-Saves, MCLA-Play, the game folder, generated, out, tools, bundles, tester-reports, owner-deck-*, l*-vol-input, l*-volunteer, l*-internal, l*-int-input.

## Recovery notes
- Build/object dirs and SDK installs: rebuild from the host or SDK commit recorded in the build evidence. The published SDK commits are reachable from fork mcla/mcla-r-hotfixes. The l66/l66c hot-blocks objects need local branch mcla/m10-hotblocks in l39-sdk-src, which is not published.
- Clean clones: re-clone at the recorded HEAD. l43, l45, l46, l48, l49 and l52 are on origin/main. **l56-src (85128b6) is recoverable only from local main until main is pushed.**
- Worktrees: `git worktree add` from the kept branch refs (or daa1ae5 for l47).
- Staging fixtures: rerun `scripts\Test-MclaInternalLauncherStaging.ps1`. Temp and scratch entries: no recovery needed (temporary). Raw content was not hashed or archived. That was acceptable because none of it was evidence.

## Correction (2026-11-05 evening)

- One live dependency was removed with the pre-M9 lanes: `private/l35-launcher-build/mcla_build_lease_probe_win.exe`, which every `l*-volunteer-prepare.ps1` calls in its step 5 (signed-list verification). The owner's l70 signing run stopped there after ticket, entries, signed list and staging were already written. Recovery: the probe is a CMake target (`mcla_build_lease_probe_win`, EXCLUDE_FROM_ALL); it was rebuilt into `private/gate-host-build/` and steps 5 and 6 were run by hand (`SIGNED_LEASES_VERIFIED entries=7`; volunteer audit 195 checks, 0 findings; receipt capabilities 1080p,1440p,unlocked). Future prepare scripts must reference `private/gate-host-build/mcla_build_lease_probe_win.exe` (or build the target first). Lesson for the maintenance rule: scripts under `private/` are part of a build's dependency closure; grep them for paths before deleting a folder.
