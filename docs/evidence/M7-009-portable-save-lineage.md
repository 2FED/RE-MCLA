# M7-009 early subset: portable save-lineage protection

Date: 2026-09-05. Repository revision: **0.13.2.0**.
Disposition: developer-side KI-028 containment implemented and reviewed;
**physical successor reload and full M7-009 remain open**. M7 stays 0/16.

## Defect and boundary

The old publisher seeded every new child from M6-014's latest desktop archive.
It did not consider progressed saves returned from the Deck and could prune
their parent bundle after publishing an older seed. This was a static risk;
the analyzed Deck returns had unchanged profiles. No lost progress is claimed.

This change touches PowerShell publishing/verification only, not guest save
formats, runtime writes, the SDK or the pending 0.13.0.0 Deck child. No owner
profile was imported, replaced or pruned during implementation/testing.

## Implemented contract

- Inventory and SHA-256 the complete `B13EBABEBABEBABE` tree, including photos,
  settings and empty directories. Require the save/header pair. Bound paths,
  sizes and counts; reject conflicts, incomplete profile/session state,
  case collisions, reparse points and unreadable entries.
- Validate every retained session and full-profile start/final snapshot.
  Normal, nonzero-exit and already-recovered interrupted sessions are distinct:
  valid persisted data can be preserved without calling a crashed run healthy.
  Resolve a unique connected terminal profile by hashes, not wall-clock order.
  The live profile must equal that terminal. Known native verification-only
  records are validated separately; differing filename/start clock readings
  are legal because the native launcher obtains them independently.
- Bind new seeds to a complete-profile hash. Carry proven ancestor hashes in
  the next immutable manifest, so later pruning does not disconnect the
  surviving campaign from the original desktop baseline. Validate cycles over
  all graph vertices, including carried ancestors.
- Archive every validated candidate before branch selection. Content-addressed
  full profiles live in `private/save-archive/M7-009/<full-profile-sha256>`;
  metadata, inventories and reused archives are revalidated. Candidate trees
  are materialized, never hardlinked mutable files.
- Preserve the old bundle's profile, sessions/snapshots, logs, diagnostics,
  metadata, update root and identity/hash manifests below
  `private/evidence/M7-016/returned-bundles/<bundle-id>/<tree-sha256>`.
  Cache and immutable runtime/game payloads are disposable; they are not copied
  into the return archive. Incomplete diagnostic captures are preserved, not
  confused with incomplete saves. Conflicted diagnostics still require review.
- Refuse divergent/unproven candidates without explicit `-SaveSource legacy`
  or an exact validated bundle ID. This is an owner choice, not an automatic
  recovery heuristic. Missing/invalid data cannot be bypassed by this option.
- Hold an exclusive publication lock and each child's local launcher lock.
  Do not clear pre-existing stale locks. Rehash returned state before copying,
  after copying, before publication and before pruning. Keep an unadvertised
  successor if relocation/proof fails; do not replace the old `current.txt`.
- Atomically replace the pointer only after staging/relocation and evidence
  preservation. Pruning requires that the pointer advertises the successor,
  the exact old child has a verified archive and its writer lock is owned.
  Remove payloads before releasing that lock. `-PruneOnly` uses the same gates
  and additionally requires the current child to carry the selected profile.

## Verification

Independent session/provenance and publication/retention reviews found no
remaining blockers after fixes for timestamp assumptions, native verification
records, carried-ancestor cycles, multigeneration ancestry and pointer binding.

Commands and observed results:

| Gate | Result |
| --- | --- |
| `ast-grep scan` / `ast-grep test` | Clean / 3 passed |
| `test-portable-save-lineage.ps1` (PowerShell 7) | 46 checks passed |
| Same fixture under Windows PowerShell 5.1 | 46 checks passed |
| `test-portable-save-lineage.ps1 -NativeLauncher` | 47 passed; existing native launcher accepts the new manifest in an isolated verify-only fixture |
| `test-portable-campaign-bundle.ps1` | 1 positive / 13 negative / 36 source checks |
| `test-portable-campaign-session.ps1` | 1 positive / 10 negative |

Fixtures cover full-tree preservation, clock rollback, complete/interrupted/
crashed sessions, stale synchronized live profiles, missing start snapshots,
legacy unproven ancestry, cycles, archive drift, byte bounds, conflicts, local
active/stale locks, source mutation, multigeneration succession after pruning,
atomic pointer-replace failure, and a disposable-cache junction whose external
sentinel must survive. Fixture cleanup affects only unique synthetic roots.

Windows directory rename transiently failed during early fixture runs; bounded
same-parent rename retries retain the staging tree on failure and never replace
an existing archive. Subsequent full suites passed on both PowerShell editions.

## Limits and next evidence

Local file locks do not coordinate remote Syncthing writers. Stop all writers
and complete synchronization before running the publisher. Unknown children,
unfinished staging, stale locks and ambiguous lineage need recovery review,
not automatic deletion. Profile inventory is bounded to 512 MiB / 16,384 entries;
each retained return archive is bounded to 4 GiB. Exceeding bounds stops safely.

The native v1 snapshot watcher keys on save/header changes and retains bounded
history. Photo-only changes, a missing retained start, or truncated ancestry
can therefore block automatic import. Originals stay in place; this patch
does not silently invent missing history or change the runtime watcher.

The pristine bundle verifier remains strict about the current SDK and seed.
Read-only `-ReturnedProfile` accepts separately hash-bound historical runtimes;
it cannot run the native launcher or grant a gameplay-health PASS.

Pending physical artifact remains MCLA **0.13.0.0**, SDK **v0.14.0.0**;
manifest SHA-256 remains
`2F0F0D970DE19D153AC107E33507DD2A88E8206202344E2E4A82E14F6651A086`.
After its short performance return, a later explicitly published successor
must load the actual progressed profile before cross-build continuity can close.
