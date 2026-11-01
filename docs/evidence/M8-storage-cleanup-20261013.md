# Obsolete game payload cleanup

At the owner's request, six obsolete failed M7 preparations no longer retain
duplicate game assets. This frees approximately36.71GiB without deleting unique
test evidence, saves, source, executables or current packages. Their historical
manifests remain intact, but those failed packages are now incomplete until the
game payload is restored. Never rerun consumed diagnostics merely to restore them.

The exact scope is under private/evidence/M7-016:

- failed-bundles/mcla-0.8.2.2-long-session-id/game
- failed-bundles/mcla-0.8.2.2-pointer-publish/game
- failed-bundles/mcla-0.8.2.2-stack-overflow/game
- bundle-20260905-113140-07d1bccd/failed-staging/game
- bundle-20260905-123424-a62b647d/failed-staging/game
- bundle-20260905-143544-618197f0/failed-staging/game

All90 files matched the15 retained canonical files in private/p1/game by exact
relative path, byte count andSHA256 before deletion. The cleanup held canonical
files open against writes/deletion, repeated target hashes immediately before
individual literal-path removals, and performed no recursive directory deletion.
All directories, manifests, logs, dumps, saves and unique capture artifacts remain.
The active l12, MAIN and M8 source-binding experiment were outside the allowlist.
No game or concurrent writer ran during cleanup. Independent safety review GO.

Recovery mapping and per-file deletion journal:
private/cleanup-m7-duplicates-20261013-v2/{plan.json,deletions.jsonl,result.json}.
PlanSHA256989C4523C982345484E6EE161C988C301D3543CB2670FD59DDA998CAA596B180.
Copying the recorded relative files from private/p1/game restores the same bytes;
no new game extraction is needed while that retained copy remains available.

Removed39,417,518,352 logical bytes. C: free space rose from48,336,883,712 to
87,755,722,752 bytes (about45.02 to81.73GiB); small concurrent OS changes mean
the free-space delta is not an exact payload measurement.

The first dry-plan attempt failed in PowerShell's dictionary summation before
writing a plan or deleting anything. Its empty receipt directory is preserved;
the corrected v2 plan and execution succeeded. No milestone acceptance claim
or failed-run status was changed by this maintenance.

## October18: Temp and retired vinyl capture payloads

Owner explicitly authorized recurring Temp cleanup and genuinely obsolete local
build/artifact cleanup, naming private/rdc. Active M8 builds/dependency closures,
MAIN, all save lineage, game assets and open-bug evidence remain protected.
The heartbeat automation `temp` is ACTIVE every48hours, scoped to the operator's
`%LocalAppData%\Temp` and repository/private. General Temp uses a7-day quiet age;
project evidence requires an explicit retention decision, never age alone.
The desktop app/computer must be running for local scheduled work.

First pass removed4157 old empty `.tmpXXXXXX` directories and13 old `.tmp`
files from the operator's `%LocalAppData%\Temp`:148253266 logical file bytes.
No recursive directory removal, reparse traversal, force unlocking, clipboard,
image or investigation-data deletion occurred. Exact receipt:
private/temp-cleanup-20261018-1.json. These generic temporary files are not
recoverable from that metadata-only receipt. The initial dry run exposed a
PowerShell metadata/summation bug before deletion; corrected dry run and execute
both succeeded, with zero skipped targets.

Independent retention and separate cleanup-safety reviews admitted150 exact
leaf files,28477143918bytes. Primary execution completed0 and rehashed all806
retained files after deletion. Plan and journal:
private/cleanup-rdc-retired-20261018-1/{plan.json,deletions.jsonl,result.json}.
PlanSHA B0E3902F90288EDF2A26379C9EB84ACBC4B931377C13075894014310B6E69922;
scriptSHA A6F8396067E52DEB9A5EEB7BEB7BF34460E8C4F7F8659AC112C48ACA7D4DC3C9.
Independent pre-execution audit checked all956 target/keep hashes and exact
inventory partition. No capture/game/build process ran during cleanup.

| Removed class | Files | Bytes | Recovery boundary |
| --- | ---: | ---: | --- |
| rd2-rd6 intermediate raw RDCs | 20 | 21528570705 | Intentionally retired; arbitrary replay of these arms is no longer possible |
| Isolated completed-run driver caches | 126 | 4307000640 | Rebuildable caches, not original raw evidence |
| Duplicate Temp RDCs | 4 | 2641572573 | Byte-identical raw originals retained in vinyl evidence root and pair-v2 |

The twenty raw files are the outside/editor pair under each of these closed
vinyl investigation roots in private/rdc; inner paths and hashes are in the plan:

- rd2 corner c-967b698418f94f7cb4e77b3f429af5ce; center c-20b4ecf091584032b4963186d9d24e3b.
- rd3 OFF c-e8db8e9ecc724b5d9eb667fc1dcf93a3; ON c-430af53df9ba4ffca700596e0432f10f.
- rd4 OFF c-a32f31ae7d8a4a0899279fae00611c57; ON c-da14d8081d454289a3a36f6085b738aa.
- rd5 OFF c-ec51c94461ae478e9f137d4802a857f6; ON c-27624140c10d4bc8a71f7bae84c6846c.
- rd6 OFF c-34831d5692964fadb6e3cf5725a90b1f; ON c-09d0c24bba0a4561940595a76dbf72f4.

They were unique successful diagnostic records, not duplicates or failed garbage.
Sampling alternatives and witness branches have concluded; the accepted packing
fix and deferred extra quality no longer require these intermediate replays.
All derived inventories, textures/raw atlas exports, shader disassembly, source
snapshots, BMPs, complete logs, requests/results, original/after seed copies and
rd4 partial mcla-atlas-upload-g1.bin remain. Hashes do not replace lost raw replay.
Historical manifests are not rewritten: whole-tree/raw checks for retired arms
will now fail by deliberate retention policy, not indicate a newly failed test.

Seven private/rdc raw captures remain: original1440 pair c-232212..., corrected
rd7 pair c-d6d050..., clean rd8 pair c-0c763..., and street c-6fe3... for KI-032.
Original/rejected720p raw copies in private/evidence also remain. Temp f985... and
e2f3... lost only their four duplicate RDC leaves; all other Temp evidence stays.
Failed2160 allocation evidence and resolution regression BMPs remain unchanged.
No .exe/.dll/.pdb was removed; old builds may still supply current dependencies.

Combined logical removal28625397184bytes (about26.66GiB). C: free after the large
pass79870705664bytes (about74.38GiB); concurrent OS activity makes this distinct
from exact removed-byte accounting. No milestone/task acceptance or promotion.
No Spark job for this maintenance: deterministic local inventory/hash checks
need no model and unrelated private Temp content is not sent to Spark.
Independent post-audit GO: all150 journal entries match the plan, every target
is absent, all806 retained pins unchanged, remaining private/rdc membership is
exactly802files including7RDCs. ResultSHA3BA9F4BDAF2F4A75312B7783FD0D323645DC6BD1D011CCE8DC7ADA23A92E94E5;
journalSHA C56DF0AFD6F02C83AC5CB940A1C7D7D74C4AB248EA7AE6CA40F3F71ECC80EC6B.

## October20 scheduled maintenance: empty Temp directories only

Heartbeat `temp`; maintenance only, no M8 runtime resumed. Current-state and
retention policy read first. No game, capture, build or installer conflict was
found by the scoped process checks. No processes stopped, permissions changed,
external model calls, reset redemption or files outside the two scoped roots.

Inventory: 8,105 immediate Temp entries; 1,301 private top-level entries
(metadata only, not a claim of full recursive private coverage); recursive
non-reparse RDC inventory 1,742 entries including 802 files / 8,983,260,808 bytes.
All seven retained RDCs (7,726,151,426 bytes) remain required evidence. No new
project-artifact obsolescence was established: no private payload was deleted.
All file contents, clipboard/screenshots, logs, saves, assets and build closures
were preserved. No private content was sent to Spark: deterministic metadata
inventory and path checks are the appropriate local work.

Removed only 2,072 immediate Temp directories named `.tmpXXXXXX`, each empty
and both created and last modified before the seven-day UTC cutoff recorded in
the plan. Creation range 2026-10-11T07:34:51.1087669Z through
2026-10-13T07:04:48.2646805Z. Every exact path and both timestamps were recorded
before deletion. SHA256 is not applicable to empty directory entries; no file
was a deletion target. All deletions used nonrecursive Directory.Delete(false),
with exact-parent/name, full ancestor/reparse, emptiness, timestamp and process
reference checks. No force or recursive root deletion.

Plan inventory preserves 5,733 fresh entries, three protected clipboard/image
entries and 297 entries without sufficient disposable-content evidence.
The 2,072 planned targets all deleted; zero execution skips. Postcheck confirms
all targets absent, exact journal membership/count, and unchanged size/creation/
write metadata for all 802 RDC files. This is a metadata postcheck, not a new
full-content hash audit of retained files.

Receipts: private/maintenance-temp-20261020-3/{plan.json,deletions.jsonl,result.json}.
Plan SHA256 747F1BD9A7FBF0F7EBD4B2AC91BCD5A8129FF3139E9800BF78C6AACEA2BFF99D.
Journal SHA256 EFBDD6828DA8C7EF2AB34C2772BCFD519C96A1B423FE73AE75273DC111B1E869.
Result SHA256 C60AAE7E7054C6D0DE656D4566F74F73EAB9BF6520329247BB072A7D517979A7.
Executor private/maintenance-temp-20261020.ps1:
2D855B38D741BB7E283DA0E6D20AA05E66D45AC8133DB22273DA01B70D2A2C9F.
Independent pre-execution review checked all 2,072 targets and the corrected
executor; GO limited to these empty directories.

Logical file bytes removed: **0**. C: free at inventory 69,116,416,000 bytes;
immediately before execution 68,846,063,616, after 68,864,745,472. Concurrent
system activity and filesystem metadata mean this is not attributable payload
recovery. No critical shortage observed. No raw replay was lost; empty names
remain in the manifest and can be recreated, but no file restoration is needed.

Preparation failures preserved without deletion: receipt1 is empty after an
unsupported Split-Path parameter combination; receipt2 records no targets due
to a strict-mode null-array Count error. Caller arrays were made explicit.
Independent review then found PowerShell JSON date auto-conversion would skip
unchanged targets; DateKind String fixed it before execution. Receipt3 is the
only executed plan. Earlier zero-target report is not evidence of no eligible
directories. No milestone acceptance, task closure, commit or push.

## October22 scheduled maintenance: empty Temp directories only

Heartbeat `temp`; current-state and retention policy read first. No M8 runtime
resumed, milestone acceptance, task closure, commit or push. Existing dirty
worktree preserved. No game, capture, build or installer conflict identified;
four ResolvePython processes remained untouched. No process stopped or permission
changed. No private content sent to Spark: deterministic metadata/path checks
need no model. No new project-artifact obsolescence proof; all private payloads
and retained raw evidence/build/save dependency closures preserved.

Inventory: 6,758 immediate Temp entries; 1,403 private top-level entries (metadata
only, not full recursive private coverage); 1,742 recursive non-reparse RDC
entries, including 802 files / 8,983,260,808 bytes. Preserved 5,061 fresh Temp
entries, 18 protected clipboard/screenshot entries and 300 entries without
disposability proof. No file contents were deletion candidates.

Removed 1,379 empty immediate `.tmpXXXXXX` directories only. Both creation and
write precede cutoff 2026-10-15T07:25:36.2792865Z; creation range
2026-10-13T07:37:05.1880744Z through 2026-10-15T07:13:04.8531168Z.
Exact paths, timestamps, zero bytes, SHA256 not applicable (empty directories),
and reasons were persisted before deletion. Primary precheck verified all targets;
executor rechecked exact parent/name, all ancestors/no reparse, timestamps,
emptiness and active-process references, then used nonrecursive Directory.Delete.
All 1,379 deleted, zero execution skips. Postcheck verified exact unique journal
membership, all targets absent and unchanged size/creation/write metadata for
all 802 retained RDC files; not a new full-content hash audit.

Receipts: private/maintenance-temp-20261022-1/{plan.json,deletions.jsonl,result.json}.
Plan SHA256 A6D7E4855A85D1FB8087CB732ABF54158699FE6D02D27A6ADE6831C7A0F0713A.
Journal SHA256 591154B04202A4DDEE3BAF6B41F2E985A9DD30FBE084E77D9ACF49F48A47D2A9.
Result SHA256 D422472679D06D34A5D10219FFECDA06646A45E69E5F9439FFF9B16A8B4F226A.
Executor private/maintenance-temp-20261022.ps1:
F709537A5EE2A78AEA0A2E095CBD555B887EBFD7ECD4285399093402C083B6E7.

Deleted files/logical bytes: **0 / 0**. Free C: inventory73,806,417,920 bytes,
immediately before execution73,800,482,816, after73,799,815,168. Background disk
activity means free-space change is not attributable payload recovery. No critical
shortage. No raw replay loss; empty names remain in manifest/journal and can be
recreated, no file restoration needed. The every-two-days `temp` schedule remains.

## October24 active-investigation retention update (not a cleanup run)

Retain the complete O/C/H game collections, H4096-slot raw prefix and its whole
immutable derivative, all paired-SDK/host build dependency closures, and both
new swap-binding leaf/STUB native families. The leaf O3suffix1 truncated-stdout
failure is unique diagnostic evidence; preserve its actual raw4096-byte stdout,
old fixture and result even though fresh suffix2 passes. Preserve the accepted
O3/O2 raw slots, unknown/unavailable snapshots, write-failure prefixes and native
dependency archives for reanalysis. Generated aggregate reports/hashes are NOT
replacement raw data or obsolescence proof. New standalone host/supervisor is
still active, so its future inputs/outputs remain protected too. Normal l12,
MAIN and every save lineage stay untouched. No file deleted by this update.
Latest observed freeC35079307264B before the proposed new host build; not an
exact payload accounting or permission to delete additional project evidence.

Post-S collection update: retain consumed S root
private/evidence/M8-001/autostart-arcade-swap-binding-s-20261024-1, all49native
artifacts/41original images, complete phase/timer/config/swap raw, supervisor
result31DDE724, host/stage/admission dependency closures and all derived review
receipts. Preserve any later offline callback/lexical derivative with its inputs;
neither hashes nor the accepted derivative replace raw replay. All active H/O/C
and native failures remain protected. Latest measured post-S freeC32,075,612,160B
is a different observation from the historical pre-host35,079,307,264B above,
not deletion/recovery accounting. No cleanup or raw replay loss this continuation.
