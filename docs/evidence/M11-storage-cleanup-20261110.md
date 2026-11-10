# Storage cleanup, 2026-11-10

The owner asked on 2026-11-10 to "delete old obsolete junk". Free space was about 21 GiB. An Opus subagent did the work under the AGENTS.md section "Owner-authorized storage maintenance". The primary agent chose the delete set: 17 superseded host build trees and 4 SDK build dirs. Another agent was building at the same time, in `l82-sdk-wt`, `l82-sdk-b`, `l88-*` and `gate-host-build`. None of those was touched.

The per-target record is kept untracked because it holds local paths. It lists path, bytes, files, newest write, staged artifact, staged and build sha256, and the sha256 prefix of every top-level exe/dll. It is in `private/storage-maintenance-20261110/record.tsv`, next to the script that produced it (`check.ps1`).

## Pre-checks (per target, scripted)
- **Directory and reparse points:** each target existed and was a real directory. Neither the target nor any ancestor up to `C:\BDU` was a reparse point, and no target contained a reparse point (`Get-ChildItem -Recurse -Attributes ReparsePoint` found 0 in every target).
- **Processes:** no `ninja`, `clang*`, `cmake`, `link`, `lld-link`, `ctest`, `mcla` or `MCLA-Launcher` process had the target's name in its command line or executable path. No exe was launched.
- **Staged copies:** for each `lNN-host-build`, the staged `private\lNN-int-input\mcla.exe` (for l81v: `l81v-volunteer-input`) existed, and its sha256 equalled the sha256 of the build tree's `mcla.exe` (17/17 match). The staged hashes also equal the `mcla.exe` rows in the matching `M11-lNN-owner-build` evidence files. For each `lNN-sdk-b`, `private\lNN-sdk-install\bin\rexruntime.dll` existed. Its sha256 equals the rexruntime.dll in the l71 to l81 owner builds.
- **Script references:** the only script references to the targets were comment headers in `private\lNN-internal-prepare.ps1` and `l81v-volunteer-prepare.ps1`, plus one line in `README-current.txt`. The prepare scripts stage from `lNN-int-input` and take the lease probe from `gate-host-build`, which was kept. This avoids a repeat of the 2026-11-05 lease-probe incident.
- **What was only in the build trees:** `Launch-MCLA.exe` and the ~55 test executables (`mcla_*_test.exe`, `mcla_diagnostic_image_fake_helper.exe`). They were not staged and are now lost (see recovery).
- **Deletion method:** `Remove-Item -LiteralPath <exact path> -Recurse -Force`, one target at a time. After each delete, the script checked that the path was gone. 21 of 21 were deleted, with no partial deletions.

## Deleted
| Path | Bytes | Files | Newest write | Staged copy kept | Staged sha256 (prefix) | Build mcla.exe |
|---|---|---|---|---|---|---|
| `private\l69-host-build` | 203,649,760 | 283 | 2026-11-05 17:01 | `private\l69-int-input\mcla.exe` | `2DC30A8678831C82` | match |
| `private\l71-host-build` | 205,667,041 | 289 | 2026-11-06 15:24 | `private\l71-int-input\mcla.exe` | `A0FFA6C641345524` | match |
| `private\l72-host-build` | 205,930,427 | 289 | 2026-11-06 17:39 | `private\l72-int-input\mcla.exe` | `97F5E83F9521F441` | match |
| `private\l73-host-build` | 206,083,955 | 289 | 2026-11-06 19:49 | `private\l73-int-input\mcla.exe` | `59DF379BDC8474AF` | match |
| `private\l74-host-build` | 206,707,247 | 292 | 2026-11-07 11:04 | `private\l74-int-input\mcla.exe` | `E7753F5EBE1CE3C9` | match |
| `private\l75-host-build` | 207,080,111 | 292 | 2026-11-07 12:55 | `private\l75-int-input\mcla.exe` | `E91CB1D85CCC3D14` | match |
| `private\l76-host-build` | 207,642,158 | 295 | 2026-11-07 15:34 | `private\l76-int-input\mcla.exe` | `C53E6DCB1DF84334` | match |
| `private\l77-host-build` | 207,787,615 | 295 | 2026-11-07 17:53 | `private\l77-int-input\mcla.exe` | `845BBA6E79EA6C4F` | match |
| `private\l78-host-build` | 207,860,362 | 295 | 2026-11-07 19:30 | `private\l78-int-input\mcla.exe` | `3E37F9852636A3FD` | match |
| `private\l79-host-build` | 207,969,271 | 295 | 2026-11-08 12:23 | `private\l79-int-input\mcla.exe` | `1180C3DC46BE2BF1` | match |
| `private\l80-host-build` | 208,119,631 | 295 | 2026-11-08 14:36 | `private\l80-int-input\mcla.exe` | `C3F37CA8E28B9317` | match |
| `private\l81-host-build` | 208,293,463 | 295 | 2026-11-08 16:30 | `private\l81-int-input\mcla.exe` | `3C7EDF3E850A83A2` | match |
| `private\l81v-host-build` | 207,850,415 | 295 | 2026-11-08 16:40 | `private\l81v-volunteer-input\mcla.exe` | `83C8955A35B78B2D` | match |
| `private\l82-host-build` | 208,320,089 | 295 | 2026-11-10 09:11 | `private\l82-int-input\mcla.exe` | `B4CCCE310F6497BA` | match |
| `private\l83-host-build` | 208,922,761 | 300 | 2026-11-10 10:46 | `private\l83-int-input\mcla.exe` | `6ABF0CE4F62605A0` | match |
| `private\l84-host-build` | 209,266,875 | 300 | 2026-11-10 12:14 | `private\l84-int-input\mcla.exe` | `F1F1D61687CFEDB0` | match |
| `private\l85-host-build` | 209,394,734 | 300 | 2026-11-10 14:42 | `private\l85-int-input\mcla.exe` | `FEB43F232571762A` | match |
| `private\l71-sdk-b` | 405,674,148 | 1591 | 2026-11-06 15:14 | `private\l71-sdk-install\bin\rexruntime.dll` | `259145F1436588F9` | n/a |
| `private\l79-sdk-b` | 418,054,531 | 1603 | 2026-11-08 12:03 | `private\l79-sdk-install\bin\rexruntime.dll` | `CD083F8A1D5A8B4C` | n/a |
| `private\l80-sdk-b` | 420,309,544 | 1607 | 2026-11-08 14:16 | `private\l80-sdk-install\bin\rexruntime.dll` | `ADDF4492B10BCCE9` | n/a |
| `private\l81-sdk-b` | 421,570,558 | 1609 | 2026-11-08 16:24 | `private\l81-sdk-install\bin\rexruntime.dll` | `FDBC21435727028A` | n/a |
| **Total** | **5,192,154,696** (4.84 GiB) | **11,404** | | | | |

The full 64-hex staged hashes are in the record. The build trees came from these sources:

| Builds | SDK commit | Host source |
|---|---|---|
| l72 | `rexruntime 259145F1...` | as in `M11-l72-owner-build-20261106.md` |
| l73 | `4a3aea1` | `1dec761` |
| l74 | `8f949f7` | |
| l75 | `4bb8c02` | `60c6b93`-equivalent tree |
| l76 | `a4a5309` | |
| l77 | `590c484` | |
| l78 | `d61c597` | |
| l79 | `0755836` | |
| l80 | `ca63f94` (= tag `v0.20.0.10`) | |
| l81 and l81v | `2aff82e` | host tree `a9de776` |
| l82 to l85 | `c40db4b` | l84: HEAD `cd75f1e` plus the diff in its evidence; l85: HEAD `e41282b` plus the diff in its evidence |

l69 and l71 are recorded in `M10-l69-pc-session-20261105.md` and the l71 probe evidence. Each SDK build dir belonged to the install with the same lNN prefix (l71, l79, l80, l81).

Reason for each deletion: a reproducible build tree for the identity above. The staged binaries are kept in the matching `-int-input`, `-internal` or `-volunteer-input` folder, and the SDK binaries in the matching `-sdk-install`.

## Skipped
None. Every target passed every check.

## Kept (not touched)
- **Build trees:** `gate-host-build` (holds the lease probe), `l86-host-build`, `l87-host-build`, `l82-sdk-b`, every `l88-*`.
- **SDK installs and worktrees:** every `*-sdk-install`, every `*-sdk-wt` git worktree.
- **Staged builds:** every `*-int-input`, `*-internal`, `*-volunteer*`, `*-arms`.
- **Other folders:** `_archive`, `owner-*`, saves, `tools`, keys, `evidence`.
- **Everything else** not in the delete set, including the other `-b`/`-o` and `-host-check` dirs listed below.

## Lost raw replay / recovery limit
Rebuilding from the recorded host commit and the kept SDK install gives the same configuration. Three things are gone:
- Object files and incremental ninja state.
- The test executables and `Launch-MCLA.exe` that existed only in the build trees.
- For the SDK dirs, the SDK object/CMake state.

A rebuild is not guaranteed to be byte-identical. For staged-build identity, the staged `mcla.exe` and `rexruntime.dll` and their published hashes stay authoritative. No evidence, logs, captures or saves were inside the targets.

## Free space (C:)
| Point | Bytes | GiB |
|---|---|---|
| Task start | 22,834,991,104 | 21.27 |
| Just before deletion | 32,776,146,944 | 30.53 |
| Just after deletion | 37,969,133,568 | 35.36 |
| Deletion drive delta | 5,192,986,624 | 4.84 |
| End of task | 35,221,102,592 | 32.80 |

Free space changed outside this task twice:
- It rose by about 9.9 GB between task start and the deletion run.
- It fell by about 2.7 GB after the deletion run.

Both changes came from the concurrent build agent or other activity, not from this task. The deletion delta itself matches the logical bytes.

## Report only: second-round candidates (not deleted)
- **Largest items under `private\` (GB):**

  | Folder | GB |
  |---|---|
  | `_archive` | 32.14 |
  | `evidence` | 27.58 |
  | `p1` | 6.26 |
  | `timebox-20261025-1` | 2.26 |
  | `tools` | 2.21 |
  | `l34-src` | 1.68 |
  | `l70-src` | 1.68 |
  | `l67-src` | 1.67 |
  | `l65-src` | 1.67 |
  | `l39-sdk-src` | 1.55 |
  | `sdk-rb32-s` | 1.51 |
  | `bundles` | 1.16 |
  | `l80-sdk-blend-b` | 0.69 |
  | `host-arcade-swap-binding-20261024-1` | 0.56 |
  | `l80-sdk-blend-wt` | 0.48 |

  `private\` total: 110.9 GB.
- **Remaining l71 to l81 build/object dirs, about 11.55 GB in total:**
  - SDK experiment `-b`/`-o` dirs (sdk-audio, fetch, steady, steady2, steady3, xma to xma4, fence, defaults, blend), about 0.3 to 0.4 GB each.
  - `l71..l81-sdk-o`.
  - `l74..l81-host-check`, about 0.19 GB each.
  - `l71-crashflush-build`, `l71-minidump-build`, `l72-album-build`, `l72-kick-build`, `l73-keys-build`, `l71-audio-keys-build`.

  The matching l71 to l81 `-wt` worktrees (about 0.5 GB each) would need the git-worktree procedure.
- **`%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp` (scratch), 0.5 GB in total:**

  | Folder | GB |
  |---|---|
  | current session folder | 0.18 |
  | `l71-crashflush-src` | 0.14 |
  | `l71-audio-keys-src` | 0.14 |
  | `l86-flicker-work` | 0.03 |
  | `l69-tools` | 0.02 |
  | all others | under 0.01 each |

  The near-empty folders include `l81*`, `l82-canary` and the patch/script dirs. Scratch is not worth a round for space.
