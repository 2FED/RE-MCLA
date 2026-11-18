# M11 housekeeping: retire M7-era scripts, archive M7 data folders (2026-11-18)

Owner request (2026-11-18): retire the old M7-era scripts, archive `private/bundles`,
`private/s1` and `private/s2`, and refresh `private/README-current.txt`. This follows storage tidy
round 7 ([M11-storage-cleanup-round7-20261117.md](M11-storage-cleanup-round7-20261117.md)),
which left these folders in place because scripts under `scripts/` still named them.
No builds, tests or game launches were run. `private/tools/wait-for-idle-pc.ps1` returned 0
(no game session) before the moves. Spark was not used: the work is a deterministic reference
search plus file moves, which is cheaper to do and check directly than to check a model's answer.

## Main finding: the M7 runners were never tracked

Round 7 counted "about 13 tracked scripts". A per-file `git ls-files` check shows that only
four of the candidates are tracked. The M7 stock, profile and subtitle runners were untracked
working-tree files that were never committed. Untracked files cannot be staged as deletions
and Git holds no copy to recover them from, so they were **moved** to
`private/_archive/scripts/scripts-m7-20261118/` (not deleted). **No tracked file was deleted.**

## Candidate list and proof

The candidates are every file whose code names `private\bundles`, `private\s1` or `private\s2`
as a path. Comments, severity labels such as `S1`/`S2`, and docs were left out. The reference
search covered:

- all tracked files outside `docs/evidence`: CMake, tests, `.ps1`/`.py`/`.cmd`/`.toml`/`.json`,
  README-AI, `docs/current-state.md`, AGENTS.md and the workflow docs;
- every untracked script in `scripts/`;
- the live private scripts: `l57-work`, `l39-work`, `tools`, `gate-host-build`, and the top-level
  `private/*.ps1`/`*.cmd`/`*.txt` files, including the l103-l111 prepare scripts and arms and
  the volunteer prepare scripts.

### Retired (untracked, archived)

| Script | Data it named | Who referenced it | Verdict |
|---|---|---|---|
| `Invoke-MclaCleanProfileStock.ps1` | bundles `M7-Windows-Stock-Clean`, `-Local` | nothing | retired |
| `Invoke-MclaF10StockCandidate.ps1` | bundles `M7-Windows-Stock-F10`, `-Local` | nothing | retired |
| `Invoke-MclaF10WgcStockCandidate.ps1` | bundles `M7-Windows-Stock-F10-WGC`, `-Local` | nothing | retired |
| `Invoke-MclaM7LocalStock.ps1` | bundles `M7-Windows-Stock-Local` | nothing | retired |
| `Invoke-MclaPinkslipStockCandidate.ps1` | bundles `M7-Windows-Stock-Pinkslip`, `-Local` | nothing | retired |
| `Invoke-MclaSandboxStock.ps1` | bundles `M7-Windows-Stock-Sandbox`, `-Local` | nothing | retired |
| `Test-MclaM7LocalContract.ps1` | bundles `M7-Windows-Stock-Local`, `-F10` | nothing | retired |
| `stage-short-profile-candidate.ps1` | bundles `M7-Profile-Portable`, `M7-016/current.txt` | nothing | retired |
| `stage-subtitle-candidate.ps1` | `private/s1`, `private/s2`, bundles `M7-Windows-Stock-Clean` | only `test-subtitle-candidate.ps1` (retired) | retired |
| `Invoke-MclaSubtitleCandidate.ps1` | `private/s2` | only `test-subtitle-candidate.ps1` (retired) | retired |
| `Mcla-M7LocalContract.ps1` (second level) | none directly | dot-sourced only by the retired M7 local runners | retired |
| `test-subtitle-candidate.ps1` (second level) | none directly | nothing; it is an AST test of the two subtitle scripts and is not in any gate | retired |

`private/storage-maintenance-20261118/retired-scripts.tsv` holds the hash, Git blob id, size,
reason and historical references for each file. The only historical references are the round 7
report and `M7-profile-rename-20261010.md`.

### Kept

| Script | Data it names | Why kept |
|---|---|---|
| `scripts/build-portable-campaign-bundle.ps1` (tracked) | default `-OutputRoot` `private\bundles\M7-016` | `scripts/test-portable-campaign-bundle.ps1` reads its source. It is documented in `docs/build.md` and `docs/testing.md` and belongs to the portable-launcher family (`verify-portable-campaign-bundle.ps1`, `Portable-SaveLineage.ps1`, the `mcla_portable_launcher` CMake target) |
| `scripts/analyze-portable-deck-performance.ps1` (tracked) | default bundle root `private\bundles\M7-016` | `scripts/test-portable-deck-performance-analysis.ps1` calls it more than 40 times. It is documented in `docs/testing.md` |
| `scripts/test-portable-campaign-bundle.ps1` (tracked) | `git check-ignore` of a `private/bundles/M7-016` path | it is a test and does not need the folder |
| `scripts/test-portable-save-lineage.ps1` (tracked) | fake-repo `private/bundles/test` fixtures | it is a test and never touches the real folder |
| `scripts/mcla_stock_startup_probe.py` (untracked) | `private/bundles/M8-001-Windows-B256-Uncapped-Scale2-20261002/Get-MclaAdapters.ps1` | `scripts/test_mcla_win32_capture.py` imports `reject_blank_stock_pixels` from it. It also has its own test and its runner `Invoke-MclaStockStartup.ps1` |
| `scripts/mcla_arm_stage.py` (untracked) | `private/bundles/M7-016/MCLA-Deck-Stock-TU5-Control/bin` | the kept `mcla_stock_startup_probe.py` imports it at module level. It has its own test `test_mcla_arm_stage.py` |

No gate, CMake file or `docs/current-state.md` uses any of the kept scripts. They are kept only
because tests and docs name them, as the task rules require.

## Archived data

All moves stayed on the same volume, and the file count and bytes were checked before and after
each one. Every move is a row in `private/_archive/MOVES-20261118.tsv`. No moved tree or ancestor
contained a reparse point.

| From | To | Files | Bytes |
|---|---|---:|---:|
| `private/bundles/M7-Profile-Portable-20261010-1` | `private/_archive/m7-m8/bundles/` | 37 | 80,439,209 |
| `private/bundles/M7-Windows-Stock-Clean-20261010-1` | same | 36 | 70,074,934 |
| `private/bundles/M7-Windows-Stock-F10-20261004-1` | same | 15 | 70,812,389 |
| `private/bundles/M7-Windows-Stock-F10-WGC-20261004-1` | same | 67 | 106,392,113 |
| `private/bundles/M7-Windows-Stock-Local-20261004-1` | same | 92 | 264,380,615 |
| `private/bundles/M7-Windows-Stock-Pinkslip-20261010-1` | same | 25 | 71,466,875 |
| `private/bundles/M7-Windows-Stock-Sandbox-20261010-1` | same | 41 | 79,954,950 |
| `private/s1` | `private/_archive/m7-m8/s1` | 56 | 80,905,659 |
| `private/s2` | `private/_archive/m7-m8/s2` | 71 | 83,610,482 |

In total 908,037,226 bytes (0.85 GiB) were archived. Once the scripts were retired, nothing left
named `private/s1` or `private/s2`, so both folders moved in full. Kept scripts still name two
children of `private/bundles`, so those two stayed:

- `M7-016` (348,733,436 bytes, 206 files), named by the tracked portable-bundle publisher and
  analyzer and by the kept `mcla_arm_stage.py`.
- `M8-001-Windows-B256-Uncapped-Scale2-20261002` (154,768,353 bytes, 78 files), named by the kept
  `mcla_stock_startup_probe.py`.

The old `private/README-current.txt` (from the l66-l70 era) moved to
`private/_archive/misc/README-current-20261118-old.txt`. A short current map replaced it.

Free space on the volume was 115,384,696,832 bytes before and 115,383,652,352 bytes after. Moves
on the same volume free no space. The small difference comes from unrelated background activity.

## Open owner decision

To archive the rest of `private/bundles` (0.47 GiB), one of these would have to happen first:

- retire the tracked M7-016 portable-bundle family: `build-`, `verify-` and
  `analyze-portable-*`, `Portable-SaveLineage.ps1`, their four `test-portable-*.ps1` tests and
  the matching `docs/build.md`/`docs/testing.md` sections;
- or at least change their default roots, and also retire the untracked stock-startup probe chain.

Either one is a code change rather than housekeeping, so it is left for an owner or primary
decision.
