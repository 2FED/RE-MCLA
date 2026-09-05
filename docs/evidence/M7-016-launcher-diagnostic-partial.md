# M7-016: unfinished F10 must not block the next launch

Date: 2026-09-05. Narrow launcher fix; no game/SDK performance change.

## Reproduction and recovery

The owner's existing Steam shortcut failed with the launcher's incomplete
bundle/session message. Paired read-only inspection found no live game or
launcher, no writer lock or Syncthing conflict, and one unfinished diagnostic:
diagnostics/live/live-20260905T151532Z-336-5.partial.

The corresponding game session completed normally with exit 0 at 15:15:33Z.
The diagnostic had nine files, including an empty unfinished process dump,
state/log tail and four private profile files. All nine matched the prior
hash-bound return archive; the live full profile/album remained unchanged.
ValidateTree wrongly treated this inert evidence as an executable/session
publication failure before starting the game.

The exact orphan was reversibly relocated on both synchronized hosts to
diagnostics/interrupted/live-20260905T151532Z-336-5.interrupted. Original
bytes remain at private/evidence/M7-016/returned-bundles/<bundle-id>/
3EAEDE327B845C041138C52D578C6A7569B630F302AA73F1CF6F8F62A492CCBF.
No file was deleted or declared a complete diagnostic. No save, album,
immutable binary or Steam/Proton setting changed. The owner confirmed launch
through the existing shortcut after this recovery.

## Permanent source fix

Only a direct directory matching the F10 producer's canonical timestamp,
positive 32-bit PID and positive 64-bit sequence under diagnostics/live is
exempt from the incomplete-publication rejection. Other partials remain
fail-closed. The exemption applies to both normal validation passes and
verify-only; it never promotes/deletes/uses diagnostic save fragments.
Recursive traversal, reparse/Syncthing checks and immutable hashing remain.
Fresh-publication validators stay strict; returned-profile inspection already
permits diagnostic partials. Analysis still excludes incomplete packages.

The source fix does not extend the diagnostic producer's existing 24-hour
partial-retention policy. Archiving the original before restart therefore
matters. Crash-package partial recovery is not added by this narrowly tested fix.

## Review and tests

Independent read-only review identified the exact policy seam and retention
risk; root reviewed the implementation before structural checks and tests.

- Root ast-grep scan passes; rule tests 3/3.
- Separate out/build/win-amd64-launcher-recovery compiles only the launcher
  and nonshipping path-policy test. Unit test: 52 checks, 0 failures.
- Actual native dummy-title recovery fixture passes under PowerShell 7 and 5.1:
  stale lock recovered, interrupted session recorded honestly, follow-up
  session completes, both launch validation passes and verify-only accept the
  valid F10 partial, contents/empty directories preserved, user tree unchanged,
  and no completed diagnostic/latest pointer fabricated.
- First PS5.1 fixture exposed a null ExitCode caused by delayed process-handle
  acquisition. Retaining the handle before WaitForExit corrects the fixture;
  final dual-shell reruns pass. This was not a second launcher failure.
- Matcher negatives cover wrong parents, regular files, malformed/overflow/zero
  identifiers and nested partials. Actual filesystem reparse/conflict checks
  are unchanged and reviewed, not newly claimed as native negative-test passes.
- Final bundle contracts (1 positive, 13 fail-closed negatives, 41 source
  checks) and full-profile lineage (46 checks) pass under both shells.

Source hotfix version is 0.16.0.2 after review and tests.
Published 0.16.0.1 remains unchanged for the CPU experiment. The permanent
launcher correction is source-tested, not yet deployed to that immutable bundle.
No milestone/task closure or FPS benefit follows from this startup recovery.
