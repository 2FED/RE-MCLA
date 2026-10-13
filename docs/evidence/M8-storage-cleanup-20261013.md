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
