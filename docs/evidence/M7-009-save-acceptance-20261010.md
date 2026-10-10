# Progressed successor save acceptance

October 10, 2026. Primary and independent review accept **GO for the bounded save
and successor-lineage criterion**; final engineering gates below pass. The
M7-009 task commit records this bounded acceptance. Actual progressed Windows successor load/restart is now
accepted on p1, with complete-profile archive and native session/name provenance.
The earlier [portable-lineage report](M7-009-portable-save-lineage.md) retains its
historical pending conclusions; they are not current missing owner tests.

This does not claim universal save durability, arbitrary simultaneous writers,
all interruption points or graceful guest teardown. No destructive owner test,
autosave interruption or replay is required for this acceptance.

## Actual progressed successor

The owner main campaign reached the repaired Pinkslip/Wager/Goal Attack return,
archived as
`693B31EBB2A0F92EE6DD452F60A053DB5086B9A573FCBA479E840E4E9E8865C8`.
Its save is
`49DE59C7F2BFEFE6FE54AE2756D7EF3F58E5110F2A71FFAFF7CB1C137C56E134`.
The publisher explicitly selected that full archive, not an older legacy seed or
the separate Sandbox. An archive import starts a new provenance root with no
invented pre-import native sessions or ancestry. Earlier owner event returns
remain separately bound in the [coverage matrix](../campaign-coverage-matrix.md).

The [profile successor](M7-profile-rename-20261010.md) was built as
`mcla-0.18.0.0-win64-82A69B5E922DE807-67B8F5272016D686-49DE59C7F2BFEFE6`.
Host SHA256:
`82A69B5E922DE80726F76B2345C9FFE1FE143FFB485E4269FB1586DE9230509A`;
launcher:
`67B8F5272016D686EDF2E8B5FBAE43945B8E00E19917637AC8DF29C1DEB5F927`;
runtime:
`542DD347B56B0494F42A7B261EC8309897E867C14FE1D2456B1836AE6C12650F`.
The immutable manifest is
`5BFEF66655B9A9E9C18F080EB3915CE9251534885848E881FB0A43A60C713BCD`;
bundle manifest:
`FD46B60F6CDBE61761D253A72715B00D3FBB3D7105FBA9133055B40D9F7DECD2`.

Completed OrangeNoob rename changes only the name preference, producing profile
`09D0FBF11A4D1B657DBEBCEEE069D3F295B829C8ECA27D8C9F0383308B0F002D`
without changing the save/header. The initial long-path launch failed before
game creation, leaving the rename valid. The complete package and journal were
materialized into short private/p1 with the original preserved. Actual-root
snapshot/probe validation then passed. No reseed, second rename or source-save
overwrite was used to obtain the owner result.

Owner confirms existing progress, visible OrangeNoob and retained name/progress
after full quit and restart. The preserved first return has180 entries/
38,375,514 bytes, receipt
`0BA4EBC51AEBB5BF01DE3AC9284232ECF574C20BF6EE5D1DBAAFA2DBD6E5ACFA`.
The full profile/archive is
`EFF5249882D8E895AA820199EB800BD33F427555AD205797CA02EE0FC8B45F6A`;
save:
`EE824CF54BBE717783909C62EEBC135B37C29B4516A4D177D70E88A593228DEB`;
header:
`1D717009B10A84525F468AD826A7DC016B744FA78274865A7DAB8A1BB65A4957`.

Two native gameplay sessions, 20261010T123005Z-47616 and123631Z-40908,
exit0 with3/2 recoverable snapshots, zero watcher errors and33 immutable files
verified. First final saveCD81B01F... equals the second launcher-start snapshot.
The production full-return resolver accepts the complete seed/name/session/live
profile chain; primary and independent review rehashed the return and archive.
Applying a latest-live-profile session verifier to the older session alone would
correctly reject its now-superseded final save, not establish data corruption.

A later actual F10 follow-up preserves the same full profile/save. Its220-entry/
55,314,850-byte return has receipt
`BE32F88838D44DB0419CDA3E93B393CDCA1712C7E4042D1D0372E60B37C3060F`.
The additional124849Z-26276 session exits0 with one snapshot and zero watcher
errors. Count three cumulative gameplay sessions, not five. Automatic exit
partials remain diagnostics, not alternate saves or new F10 results.

## Source and recovery boundaries

The reviewed [lineage implementation](../../scripts/Portable-SaveLineage.ps1)
requires the complete save/header and full profile tree, bounds paths/counts/
bytes, and rejects sync conflicts, partial saved state, reparse points and hash
drift. Session start/final snapshots are independently verified. Branches,
merges, cycles, disconnected state and a stale live tip fail closed; wall-clock
recency does not select a winning save.

Native rename journals validate before/after trees and prove that only the name
preference changed. Directed rename reachability and ordinary gameplay branch
checks remain separate; preference equivalence cannot collapse divergent game
progress. Repeated names are allowed only through validated administrative edges.

Publication validates and archives every retained branch before selection.
Explicit source/archive selection does not bypass invalid existing branches or
cycles. The chosen archive and its metadata are rechecked across copy/publication
boundaries; pointer replacement precedes any archive-gated pruning. A failed
publication retains the old pointer and original bundle. No real old owner bundle
was pruned to establish this acceptance; fixture coverage is not an executed
destructive-retention demonstration.

The [historical SDK save matrix](M6-005-save-matrix.md) supplies isolated interrupted
overwrite/new-save, restart-idempotent recovery, malformed metadata and storage-full
tests. Its accepted result hash is
`5F46FA6657F39EE5AF990BD505B67D5EDBE6A781E8283D1D51BE07F3543F169E`.
The later profile runtime's focused content suite again passed10 cases/117
assertions, as recorded in the [rename report](M7-profile-rename-20261010.md).
These are controlled SDK tests, not power-loss tests on the owner's campaign.

Final integrated engineering gates, executed by the primary and accepted with
independent source/evidence review:

- Save-lineage fixtures:123 checks in PS5.1 and PS7, including archive drift,
  interrupted returns, ambiguous branches, name-only transitions and divergence.
- Native ordinary name fixture:26 calls/21 rejections, two dummy launches and two journals
  in both shells; production lineage/archive consumed without owner saves.
- Session fixtures:1 positive/10 negative; packaging fixtures:5 positives/
  106 negatives/45 source checks in both shells, including old SDK0.19 v2
  returned-profile acceptance versus rejection as a new SDK0.20 publication.
- Native combined-history boundary in both shells:37 calls/3 rejections,
  31 dummy child launches and32 preserved records. At capacity, new names and
  sessions are rejected before writes/child creation; same-name verification
  and interrupted-session recovery remain available.
- Native launcher recovery fixture: stale lock, interrupted session, unfinished
  diagnostic preservation and follow-up session accepted with the synthetic
  profile unchanged. The dummy child is not game-level interruption coverage.

The [SDK/profile integration report](M7-002-sdk-profile-integration-20261010.md)
binds the history repair and SDK0.20 source pin
`5dd58f894bf34b9ea85e59d3903c7f0e8044d853`. The new launcher is
`D8A954C71C99D334FE372559AD2EA9B75AD91B680CDFD9113947D77066317D30`.
Its32-record guard is verified with synthetic native children only. The accepted
p1 launcher remains `67B8F5272016D686EDF2E8B5FBAE43945B8E00E19917637AC8DF29C1DEB5F927`;
no owner gameplay or deployment of the new launcher is inferred. The old p1
must not be used to claim that guard already protects its future launches.
Preserve/archive a branch at capacity rather than deleting history to continue.

## Retained limits and remaining engineering work

Local locks do not coordinate an external sync writer: stop writers and complete
sync before publication. Photo-only changes and bounded snapshot retention may
leave insufficient provenance for automatic import; fail closed and preserve
originals. The complete profile archive is not a claim that achievements outside
its XUID subtree share identical lineage semantics; return packages retain those
additional files separately.

Individual name-file and completed-journal publications are atomic, but the whole
rename is not a rollback transaction. Partial journals/backups require explicit
recovery. Snapshot collection is not an unlimited atomic capture of arbitrary
concurrent guest writes. Path-budget checks cover known nesting and selected
entries, not arbitrary future path growth or OS-wide long-path support.

Clean5D073219, Sandbox5D6CB88B and subtitle s2 EBBDB184 remain independent of
MAIN/p1. The normal0.16.0.1 pointer and old branches remain intact. Replacing that
pointer is not needed to prove the already observed successor load, and should
not trigger an otherwise unnecessary owner replay.

There is no remaining owner-only M7-009 test. The bounded Windows continuity
disposition is reflected in KI-028 and the task ledger. Source changes require
their relevant regression review before any replacement is promoted.
SDK publication and any replacement gameplay package retain their own gates;
this acceptance does not publish the local SDK tag or alter tested identities.
Preserved exit0 sessions do not prove graceful guest/diagnostic teardown or
unbounded save durability. This report changes no source, live profile or pointer.
