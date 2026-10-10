# Profile source integration and bounded launcher history

October 10, 2026. The owner-tested profile behavior remains accepted on the
immutable p1 package in the [rename report](M7-profile-rename-20261010.md).
This report records reproducible source integration and a newly found launcher
history-boundary repair. New build artifacts are not silently substituted for
p1 or granted its physical gameplay PASS. Primary and independent source review
accept the integration; task closure remains a separate ledger/commit decision.

## Reviewed SDK source

Local SDK commit `5dd58f894bf34b9ea85e59d3903c7f0e8044d853`, tag `v0.20.0.0`,
contains exactly seven reviewed files: root CMakeLists, UserProfile header and
implementation, kernel_state, runtime, content_manager and the profile unit test.
The new public constructor requires the0.20 API floor. Display-name changes keep
the stable XUID and legacy storage key `User`; invalid effective string values
fail before runtime setup. Malformed non-string TOML parser behavior is not a
new validation guarantee. Unrelated dirty graphics experiments are excluded.

The isolated source root `private/sdk20s` starts at the exact old SDK commit
`c2e67161f1207ddcee609507c399efe7a1981b72`, clones25pinned dependencies locally,
and copies only those seven files. Source hashes match the reviewed original.
After commit/tag creation, its Git metadata was aligned to the identical tested
source without replacing working files. The original source, accepted install,
p1 GPU and owner profiles were not overwritten.

Windows Git initially materialized15libmspack symlinks as text placeholders.
Compilation of `cabextract/mspack/lzxd.c` failed at the literal relative pathname.
Native symlink checkout was denied by Windows. The known missing ChangeLog
placeholder was restored; then the reviewed one-shot materializer copied only
the15resolved in-tree ordinary files, checked against original resolved hashes.
Dependency commit remains `305907723a4e7ab2018e58040059ffb5e77db837`.
This is documented source materialization, not a Git-clean dependency claim.
Independent review verifies the exact15-path diff and unchanged original tree.
The only other tracked dependency symlink is noncompiled o1heap documentation.

The minimal compiler witness failed before materialization, then passed with
the production include path. Full runtime/unit/PPC build passed. A preliminary
build at the old exact tag still embedded0.19; it was not mislabelled0.20. The
tagged rebuild reports0.20.0.0, and full SDK build/install succeeds into the
separate `private/sdk20install-unpublished` prefix. Installation also registers
that prefix in the normal CMake user package registry; explicit project prefix
selection remains required. Nothing was publicly published by installation.

Tagged tests:

-401discovered unit cases,3364assertions,41source-isolated suites,4known BitStream
  skips and no failures. Receipt:
  `private/evidence/M7-002/sdk-unit-isolated-c141c040bb064edabe0e6facb14c0ff6`.
  This excludes unrelated graphics WIP tests and is not monolithic-process PASS.
-1462PPC cases/5753assertions PASS.
-Scoped SDK ast-grep scan and all3repository structural-rule tests PASS.

Tagged installed runtime SHA256:
`3171D2FAA7735275C2A663751A475BAEEFADECE946D86C8E14DDD456DB109008`.
Installed CLI:
`209F87C5B1AD5D1E2A53F4520277E54620A2F6F5042F3CBB0DF900EE8ADC3B89`.
New GPU build:
`E463D15E09289EA96B64A95BB7C47D05CDBB3BE6F4277BF2FF8A8E1DE06017C1`.
These are integration artifacts, not promoted owner gameplay binaries.

The preliminary runtime comparison found the same10442export names,715import
symbols/29DLLs, and all114accepted GPU runtime imports resolved. Code-section
length matched but its hash differed, so byte-equivalent execution is unproven.
The accepted p1 runtime remains542DD347... and GPU E2EA29B7....

## Combined history admission repair

Independent final review reproduced an actual producer/reader mismatch: native
retention kept32sessions plus unlimited rename journals, but the production
return reader accepts at most32combined records. One rename plus32launches could
therefore become unpublishable despite successful native operations.

The native regression uses only the bounded synthetic child and dummy saves.
It creates one genuine rename journal and31genuine sessions. The old accepted
launcher incorrectly allows the33rd record; the new launcher rejects both a new
name and a new session before their writes or child creation. Both session-history
pruning calls are removed, preserving the seed-to-tip chain. At capacity the
owner must preserve/archive the branch and publish a successor, not delete
history. Same-name rename and verification remain available. Existing partial
sessions can be recovered before the next new launch is refused.

New launcher `D8A954C71C99D334FE372559AD2EA9B75AD91B680CDFD9113947D77066317D30`
was built outside p1. Independent source review is GO. Native boundary tests in
PowerShell7 and5.1 each pass37calls,3rejections,31dummy child launches,32preserved
records, verification-at-capacity and partial-session recovery-at-capacity.
Ordinary name tests pass26calls/21rejections/two child launches/two journals.
Existing stale-lock/interrupted-session/unfinished-diagnostic recovery passes.
No real owner interruption, game launch or save mutation was used.

Post-migration parent gates pass in both PowerShell5.1 and7:

- Save lineage:123checks; bundle5positive/106negative/45source checks, including
  old0.19v2 return acceptance and rejection for new publication.
- Sessions:1positive/10negative; diagnostics:9positive/76negative/177source.
- Analysis config:1positive/16negative; generated-integration fixtures:1/5.
- Soak fixtures:1positive/44negative/106source; build-matrix fixtures PASS.
- Historical alpha inventory:2positive/40negative. Its27M6 issue tuples remain
  frozen; the31current issue records are validated separately, without granting
  later issues historical coverage. No M6 physical result was rewritten.

Fresh SDK20 WGC-enabled Release builds82/82 actions; default-OFF builds77/77.
Both generated-integration gates verify67files/133916828bytes/65sources and
65objects against manifest34505E79.... They have not run gameplay or replaced p1.
The initial default configure lacked RC in its shell; process-local VS Developer
PowerShell initialization resolves the environment without system changes.
WGC-enabled host:
`6A0B12A7947C6AC5DC1503465859EBD545CF4953E364F4E252F379D16C0F85CE`.
Default-OFF host:
`F8E7123B1B3AA568005226EC08A3DF6528C5D24645E70C045DB58A07C971EAF0`.
Fresh launcher:
`A75B8D8BD45D0DFC06272886AAAEFD790E7ED5670398E23BCF39BAD7DBA55F09`.
Fresh WGC helper:
`5FA890AC41BBE786D86581196FD4C39DBCC1212BFF5E4A33F432D1680A5C0AEA`.

Native diagnostic tests pass8hotkey,119protocol,1809real-SDK audit-rearm and51
fake-helper/client checks. The new launcher passes78adapter policy checks against
the shipped config, ordinary name26calls/21rejections and synthetic recovery.
An initial adapter-test invocation named a nonexistent config and failed; the
correct config/mcla.toml.example invocation passes. No live capture/game was used.
Repository ast-grep0.45.0 source/tests scan and all3rule tests pass.
The existing readback serializer passes133checks. Both previously reviewed
default-OFF unpaused diagnostic hunks are deliberately retained with their
[historical controls](M7-016-unpaused-fma-window.md); neither those controls nor
the current builds establish a new optimization or SDK20 physical performance.

## Acceptance boundary

The rebuilt launcher and SDK do not alter the accepted p1 identity. Source and
native fixture success are engineering evidence, not an unobserved new game run.
Existing complete profile/name/progress/F10/subtitle tests require no repeat merely
for source bookkeeping. A promoted replacement gameplay package still needs a
reviewed integration decision. SDK tag/pins are local until the milestone push.

## M7 profile acceptance

The [isolated clean route](M7-closure-readiness-20261010.md#clean-new-game) and
[renamed main successor](M7-profile-rename-20261010.md) meet the bounded M7-002
criterion: first save/tutorial/continuation, visible local name, persistent rename
and restart. Observed campaign contact/tutorial/unlock transitions are recorded
in the [event set](M7-003-event-acceptance-20261010.md), not extrapolated into
full-story/end-game reachability. Stable XUID/storage identity remains unchanged;
there is no Xbox Live account claim or second-profile support.

Primary and independent review accept this task after the source/native gates
above. No additional owner replay is required while p1 remains unchanged.
The source commit includes shared profile producer/lineage reader, publisher,
SDK API/pins and F10 build dependencies together: splitting them would leave
an inconsistent build or producer/consumer contract. This does not close the
separate M7-009, M7-013 or M7-016 acceptance decisions. The historical alpha
validator maintenance preserves its frozen M6 result rather than enlarging it.
