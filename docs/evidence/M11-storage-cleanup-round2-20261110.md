# M11 storage cleanup, round 2 (2026-11-10)

Owner approval, 2026-11-10, for the exact round-2 list (groups A-D below): "так, видаляй за списком"
("yes, delete according to the list"). This is maintenance under the AGENTS.md rule "Owner-authorized storage
maintenance". It makes no milestone or task acceptance claim and was not committed by the cleanup agent.
Round 1 (`l69..l85-host-build`, `l71/l79/l80/l81-sdk-b`) ran at the same time in a separate agent and has its
own report, [M11-storage-cleanup-20261110.md](M11-storage-cleanup-20261110.md). Nothing in this round touched
round-1 targets, `l88-*`, `l88-sdk-wt`, `gate-host-build`, `l82-sdk-wt`, `l82-sdk-install`,
`l67-sdk-install`, `l81-sdk-install` or `l81-sdk-wt`. No exe was launched and no process was stopped.

Method: PowerShell 7.4 (`private/tools/pwsh`). For each target we checked the target and all of its ancestors
for reparse points. No target contained a reparse point; the scripts would have removed only the link object,
without following it. Each deletion used one exact `-LiteralPath`, with no wildcards and no parent deletion,
and each target was confirmed gone afterwards. Group A used whole-directory removal, one target at a time.
Group C removed exact files, one at a time, and hashed each one first. Group D used whole-directory removal
after the git verification described below.

Receipts are in `private/storage-cleanup-round2-20261110/`:
- plans: `invA.json`, `planC-items.csv`, `planC-files.csv`
- journals: `journalA.jsonl`, `journalC.jsonl`, `journalD.jsonl`, `resB.json`
- per-item C summary: `deletedC-items.csv`
- D verification: `verifyD-*.json`
- free space and sizes: `free.txt`, `top.csv`
- scripts: `del.ps1`, `delC.ps1`, `doB.ps1`, `verifyD.ps1`, `verifyCopy.ps1`, `invA.ps1`

Journal SHA-256s:
- `journalA.jsonl`: `694C424F...7939495`
- `journalC.jsonl`: `3C4CF172...1173498FD`
- `journalD.jsonl`: `7B39B196...3EE20D59`
- `planC-files.csv`: `9C9C2396...D48DB90E1`

## Summary

| Group | Result | Files | Logical bytes |
|---|---|---:|---:|
| A old SDK installs | 14 of 14 deleted | 16,909 | 1,131,656,495 (1.05 GiB) |
| B old SDK worktrees | 0 of 5 removed: `git worktree remove` refused, not forced | 0 | 0 (2,600,022,119 still held) |
| C `_archive` builds/candidates/sdk/misc | binary leaves of 628 items deleted | 5,520 | 27,222,736,854 (25.35 GiB) |
| D source copies | 4 of 6 deleted (2 skipped) | 67,265 | 5,533,853,852 (5.15 GiB) |
| **Total** | | **89,694** | **33,888,247,201 (31.56 GiB)** |

C: free space was 48,299,573,248 bytes before this round and 103,314,862,080 bytes after it (about 44.98 to
96.22 GiB). Round 1 was deleting at the same time, so the free-space change is not an exact payload
measurement for this round.

## A. Old SDK installs (deleted)

Each source commit was taken from `include/rex/version.h` (`REXGLUE_VERSION_STRING ...gXXXXXXX`) or from the
cited evidence for the tagged installs. Every commit is present in the local SDK repository (`cat-file -t` =
commit). No active script (gate, l86/l87/l88 prepare) references these installs. Old prepare scripts that do
reference them are listed in the last column. The SHA-256s of all `bin/*.dll` and `bin/*.exe` files
(`rexglue.exe`, `rexgpu-xenos.dll`, `rexruntime.dll`, `TracyClient.dll`) are in `invA.json`; the table shows
`rexruntime.dll` only. Recovery for every row: rebuild from the source commit with the recorded l65-style SDK
configuration. Bit-identical output is not guaranteed; the recorded hashes identify the original bytes.

| Install | Bytes | Files | Version string / build stamp | Source commit | rexruntime.dll SHA-256 | Old referencing scripts |
|---|---:|---:|---|---|---|---|
| l65-sdk-install | 80,222,989 | 1200 | 0.20.0.6-dev.g8081f63 @20261102_2032 | 8081f63f | DE6CC48A...91E1B83 | none |
| l65v-sdk-install | 80,286,525 | 1200 | 0.20.0.7 @20261102_2119 | tag v0.20.0.7 = 43577c89 (M9-001 clean clone v0.20.0.7) | 991AEED2...C5FA41D | l65-clean-clone, l65-volunteer-prepare |
| l66-sdk-install | 80,258,914 | 1201 | 0.20.0.7 @20261103_1322 | ff30a86d (`mcla/m10-hotblocks`; M10-hot-blocks-sdk, DLL hashes match) | E4E18A81...7f2e385 | none |
| l66c-sdk-install | 80,270,517 | 1201 | 0.20.0.1-dev.gff30a86 @20261103_1346 | ff30a86d | 18E38180...224B6AFB | l66-internal-prepare |
| l67v-sdk-install | 80,373,184 | 1201 | 0.20.0.8 @20261103_1935 | tag v0.20.0.8 = c63895c8 (M9-001 clean clone l70; l70 volunteer DLLs) | 7695301A...7748892B | l67/l70 clean-clone and volunteer-prepare |
| l71-sdk-install | 80,677,977 | 1205 | 0.20.0.3-dev.gd08813b @20261106_1512 | d08813b4 (v0.20.0.9) | 259145F1...787AF1CC | l71/l72-internal-prepare |
| l73-sdk-install | 80,771,521 | 1207 | 0.20.0.3-dev.g4a3aea1 @20261106_1942 | 4a3aea15 | EE1173E1...5822328B6 | l73-internal-prepare |
| l74-sdk-install | 80,914,747 | 1208 | 0.20.0.4-dev.g8f949f7 @20261107_1033 | 8f949f70 | 3C4FEC5A...89BBCD8DD | l74-internal-prepare |
| l75-sdk-install | 81,153,916 | 1209 | 0.20.0.7-dev.gbd9314f @20261107_1411 | bd9314f4 | 30A31CC4...96FF60377 | l75-internal-prepare |
| l76-sdk-install | 81,288,647 | 1212 | 0.20.0.10-dev.ga4a5309 @20261107_1508 | a4a53095 | A7E5A4C4...D37C402E1B7 | l76-internal-prepare |
| l77-sdk-install | 81,302,716 | 1213 | 0.20.0.12-dev.g590c484 @20261107_1644 | 590c484f | 4E1D2CBF...E8735BB084 | l77-internal-prepare |
| l78-sdk-install | 81,335,906 | 1214 | 0.20.0.14-dev.gd61c597 @20261107_1923 | d61c5975 | 367E194D...F712DA730 | l78-internal-prepare |
| l79-sdk-install | 81,344,812 | 1217 | 0.20.0.18-dev.g0755836 @20261108_1201 | 0755836f | CD083F8A...8EBFE08204B | l79-internal-prepare |
| l80-sdk-install | 81,454,124 | 1221 | 0.20.0.23-dev.gca63f94 @20261108_1415 | ca63f94d (v0.20.0.10) | ADDF4492...6951B | l80-internal-prepare |

Kept: `l67-sdk-install`, `l81-sdk-install`, `l82-sdk-install`, `l88-sdk-install` (not touched).

## B. Old SDK worktrees (all skipped: git refused)

The main SDK repository is `private/l39-sdk-src` (`rev-parse --git-common-dir`). All five worktrees passed the
pre-checks:
- no reparse points;
- `status --porcelain --ignored --ignore-submodules=all` empty: no modified, untracked or ignored files;
- HEAD reachable from a branch, and for four of them also from a tag.

`git worktree remove <path>` then refused each one with
`fatal: not a git repository: thirdparty/FFmpeg/../../.git/modules/thirdparty/FFmpeg`. The nested
submodule's `.git` file points to a relative `../../.git/modules/...`, which does not exist in a linked
worktree. Git therefore cannot run the internal status check it needs before removing. As instructed, nothing
was forced. No branch or tag was deleted.

| Worktree | Branch | HEAD | Also in | Bytes | Files |
|---|---|---|---|---:|---:|
| l66-sdk-wt | mcla/m10-hotblocks | cdf469a8 | branch only (unpublished) | 519,751,697 | 20,169 |
| l67-sdk-wt | mcla/m10-content-logs | c63895c8 | tag v0.20.0.8+, 18 M11 branches | 519,721,370 | 20,170 |
| l71-sdk-wt | mcla/m11-content-unmount | d08813b4 | tag v0.20.0.9+ | 519,868,350 | 20,180 |
| l79-sdk-wt | mcla/m11-l79 | 0755836f | tag v0.20.0.10+ | 520,311,174 | 20,204 |
| l80-sdk-wt | mcla/m11-l80 | ca63f94d | tag v0.20.0.10+ | 520,369,528 | 20,212 |

Owner decision needed: allow `git worktree remove --force` for these five. The trees are verified clean, and
`--force` only overrides git's own refusal. The alternative is to delete each directory by literal path and
then run `git worktree prune`. Kept as instructed: l81, l82 and l88 worktrees.

## C. `private/_archive` (binary leaves deleted; records kept)

`MOVES-20261106.tsv` (2,108 rows) classifies the archived items as follows:
- 632 pre-M9/M8-era artifacts;
- 276 superseded candidate folders;
- 129 superseded build dirs;
- 52 superseded SDK lanes.

`scripts/`, `work-logs/` and `MOVES-20261106.tsv` were not touched.

What was deleted:
- **Rule.** Inside every approved item we deleted only rebuildable binary leaves, one exact file at a time.
  The extensions were `.exe .dll .lib .obj .pch .ilk .exp`. Each file's SHA-256, size and mtime were
  journalled before its removal.
- **Records kept in place.** Every other file stayed where it was, so the item folders, the MOVES paths and
  the historical manifests still resolve. This includes:
  - 253 V1 test-build tickets (`mcla-test-build.ticket`) in builds/candidates and the 82 in misc;
  - `candidate-receipt.json` and `mcla-build.manifest` files;
  - logs, JSON receipts, sources, `.map` files, disassembly text, `.bin` raw data, BMPs, `.sav` and `.dmp`.
- **Loose single-file items.** Files lying directly in the group directories were not touched, for example
  `rd4-off-shared-memory.obj`, `sdk-atlas-off-texture-cache.obj` and `ppc-fault-artifact-*.exe`.
- **Planning.** 855 item directories were planned: 628 had binaries deleted and 227 were kept whole. There
  were 5,520 planned files and 0 failures.

| Group / class | Items | Files | Bytes | Newest mtime |
|---|---:|---:|---:|---|
| builds: M8 guest/autostart `*-bin` drops | 89 | 801 | 6,096,932,640 | 2026-10-25 |
| builds: M8 `host-*` overlay build trees | 32 | 212 | 1,819,677,361 | 2026-10-24 |
| builds: l68-host-build, l68-vram-build | 2 | 792 | 605,512,973 | 2026-11-04 |
| builds: l13-out, l14-out | 2 | 4 | 210,731,520 | 2026-10-26 |
| candidates: `*-input` (l9-l64, rd1-rd8, camera1) | 79 | 553 | 5,428,010,496 | 2026-11-03 |
| candidates: staged l1-l37, rd, camera | 64 | 919 | 4,315,875,660 | 2026-11-01 |
| candidates: l38-l64 internal/volunteer packages | 17 | 255 | 1,208,415,296 | 2026-11-03 |
| misc: `*-expiry-*` package copies | 41 | 369 | 2,803,395,360 | 2026-10-17 |
| misc: vector/work/virtual native and differential builds (include `clang++.exe` copies) | 29 | 134 | 3,174,737,888 | 2026-10-15 |
| misc: other M8 test/admission/route builds | 246 | 583 | 921,292,769 | 2026-10-24 |
| sdk: `*-o`, `*-b`, clock-witness, sem-sdk fixture outputs | 27 | 898 | 638,154,891 | 2026-10-23 |
| **Total** | **628** | **5,520** | **27,222,736,854** | |

The full per-item record is `deletedC-items.csv`. Each row gives the path, binary bytes and file count, the
newest mtime, the `mcla.exe` SHA-256, the `candidate-receipt.json` SHA-256 and the SHA-256s of top-level
`*.exe`/`*.dll`. The per-file record is `journalC.jsonl`.

Identity:
- Candidates are identified by the kept `mcla-build.manifest` and `candidate-receipt.json` hash lists, and by
  the build-name evidence reports (`docs/evidence/M9-*`, `M10-*`).
- Per-item host commits were not resolved one by one in this pass. Recovery stays possible through those
  reports.

Kept whole in the archive:
- **Builds named in open issues.** Candidates the known-issues table names for an open issue, kept for
  reproduction or bisect: l2 and l47-int-input (KI-022), l7 (KI-032), l19 and l19-input (KI-036),
  l24-sunc and its input (KI-038), l33-sunc and its input (KI-041/KI-026), and l34-s30 and l34-sunc with
  their inputs (KI-043/KI-041).
- **Source copies.** `candidates/l13-src`, plus the SDK source trees `sdk/sdk-fork-m8` (with `.git`) and
  `sdk/sdk-l18-s`; none of these was verified against git.
- **Unestablished identity.** `sdk/sdk20install-unpublished` (0.20.0.0 "unpublished": its source commit is
  not established).
- **misc items with dumps, saves or KI evidence:** `ki047-ab`, the four `ppc-fault-package-*`,
  `rexglue-consumer-smoke`, `rexglue-smoke`, `ki033-accepted-20261012`, `ki034-commit-20261012`.
- **misc capture tools:** the four `wgc-*` capture tools and `renderdoc-source-1.45`.
- **No binaries:** 196 items with no binaries, which are preparation receipts and records.

The three reference builds are not in the archive and were not touched:
- l66: owner/internal build, `private/l66-internal` and `l66-int-input`.
- l70: volunteer build and M10 closing build, `private/l70-volunteer`, `l70-vol-input` and `l70-build`.
- l69: the other build of the M10 closure regression (README-AI 17b), `private/l69-internal` and
  `l69-int-input`.

None of the three is under `_archive`.

## D. Source-copy folders

For each copy we compared every file's git blob id against the commit tree, both raw and after CRLF to LF.

| Folder | Verified against | Tracked files | Differ | Extra (not in commit) | Result |
|---|---|---|---|---|---|
| l65-src (git clone) | host 1ff00dce (main) + SDK submodule 43577c89 (v0.20.0.7) | 951 + 1364 identical | 0 | 67 ignored `generated/default/*` | deleted, 1,792,755,242 B, 21,967 files |
| l67-src (git clone) | host e551fb6e (main) + SDK c63895c8 (v0.20.0.8) | 968 + 1367 identical | 0 | 67 ignored `generated/default/*` | deleted, 1,794,332,548 B, 22,153 files |
| l70-src (git clone) | host 4f69018f (main; l70 source) + SDK c63895c8 | 985 + 1367 identical | 0 | 67 ignored `generated/default/*` | deleted, 1,798,605,086 B, 22,302 files |
| l67-host-src (export, no .git) | host c66bcf7 (= evidence "267ae4b + KI-065 patch") | 587 identical (461 raw, 126 after LF) | 0 | 67 `generated/default/*`; 189 SDK files `thirdparty/inja` (=7d1b4600) and `thirdparty/xxHash` (=e626a72b), identical to their gitlinks | deleted, 148,160,976 B, 843 files |
| l34-src | host 370632df (`claude/admiring-jang-24d485`) | — | 32+ modified tracked files | 14 untracked hand-written sources/docs | **SKIPPED** |
| l39-sdk-src | — | — | — | — | **SKIPPED** |

Why the deletions are safe:
- **Generated files.** The `generated/default/*` files are ReXGlue codegen output from the game image. They
  are git-ignored and are regenerated by the clean-clone scripts.
- **Nested submodule differences.** Nested submodules matched their gitlinks (`submodule status
  --recursive`, no `+` or `-`). The only status difference was 15 files in `thirdparty/libmspack/cabextract/mspack`.
  These are git symlinks (mode 120000) materialised by Windows as byte-identical copies of their targets; all
  15 of 15 were verified, so none is a hand edit.
- **No unique git state.** The clones had no stash and no commits missing from the main repository.

Why the two folders were skipped:
- **l34-src** has uncommitted modifications to about 32 tracked files, among them `CMakeLists.txt`,
  `README-AI`, `src/mcla_app.cpp`, `src/mcla_build_lease_*` and `scripts/*Signing*`. It also has untracked
  files: `src/mcla_diagnostic_export_{main_win,policy,ui_win,win}.*`, `src/mcla_session_report.h`, the
  matching tests, two M9 evidence docs and `mcla_manifest.l34-local.toml`. The full list is in
  `git -C private/l34-src status`.
- **l39-sdk-src** is the common git directory of every SDK worktree, including `l82-sdk-wt` and
  `l88-sdk-wt`, which the active build uses. Deleting it would destroy all SDK branches and worktrees.

## Lost raw replay and unreproducible binaries

- **Lost raw replay: none.** No raw evidence (RDC, dumps, saves, `.bin` slot data, logs, receipts, tickets) was
  deleted.
- **Original binary bytes are gone** for the 5,520 group-C files and the 14 group-A installs. Only the
  SHA-256s remain.
  - **SDK installs.** Rebuildable from the recorded SDK commits. Every commit is in the local repository, and
    the tags v0.20.0.7/8/9/10 are also on the fork. Bit-identical output is not guaranteed.
  - **Candidates and builds.** Rebuildable from the host commit plus SDK named in their evidence reports.
    Bit-identity is not guaranteed, and old V1 tickets bind to the original `mcla.exe` hash. A rebuilt binary
    therefore needs a new ticket; it cannot re-use the kept one.
  - **M8 native and differential test executables** (misc). Built from sources kept in the same folders, but
    against toolchain/SDK states of Oct 13-25. Their bytes should be treated as **lost: practically
    unreproducible bit-for-bit**. Their results, receipts and sources remain.
  - **Third-party copies** (`clang++.exe`, CRT DLLs). Redistributable and can be downloaded again.
- **Group D.** Nothing unreproducible was lost: every tracked byte is in git, and the extras are regenerable
  codegen output.

## Addendum 2026-11-11: l34-src

A file-by-file review (Opus subagent, read-only) found that `private/l34-src` was a standalone clone at 370632d (an ancestor of main HEAD, 387 commits back) and that every uncommitted or untracked hand-written file in it is already in HEAD in the same or a later form (diagnostic-export sources byte-identical; session-report, lease policy, launcher and M9 docs superseded). The only file not in git in any form, `mcla_manifest.l34-local.toml` (550 bytes, sha256 0AB8DA90...), was copied to `private/storage-maintenance-20261110/l34-src-kept/`. The folder (1,808,421,466 bytes, 21,913 files, no reparse points) was then deleted by the primary under the owner approval of 2026-11-10 for verified source copies. Lost: nothing unique.
