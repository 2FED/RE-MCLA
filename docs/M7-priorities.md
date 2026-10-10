# M7 current scope and execution order

Updated 2026-10-10. [README-AI](../README-AI) owns task status and the milestone
ledger. M7 is in final review/commit/publication integration; this document does
not declare the milestone closed or pushed.

## Decision

M7 covers representative Windows offline single-player / Complete Edition
compatibility on the owner's RTX 5060 laptop. It does not require a full story,
every retail event or a second playthrough. Steam Deck/Proton and exact
stock/interval1/stock timing remain deferred to M8. The owner explicitly moved
the failed temporal harness out of the M7 critical path.

Payback, unconfirmed South Central event access and Unordered are deferred
without PASS until natural unlock/M8. No grinding is required. Unordered's
absence from the observed Arcade menu is not proof of global absence, and
Landmark is not a substitute for it. [Event acceptance](evidence/M7-003-event-acceptance-20261010.md)
and [unavailable-content disposition](evidence/M7-005-unavailable-content-disposition-20261010.md)
preserve the exact boundaries.

Stock remains the correctness baseline, not measured stable 30 or 60 FPS. B256/V96
remains rejected/diagnostic and interval1 remains unrun. Source-version changes
do not upgrade historical physical results.

## Remaining execution order

| Order | Agent-owned work | Required boundary |
|---|---|---|
| Complete | Targeted regression and relocated Windows reviews are committed as3a351ef andb0872c5. | [M7-013](evidence/M7-013-targeted-regression-20261010.md) and [M7-016](evidence/M7-016-windows-acceptance-20261010.md) keep accepted p1 separate from unrun source builds. No new owner replay for bookkeeping. |
| 2 | Complete M7-015 compatibility-document review and the aggregate milestone review. | Claims match the event, profile/save, media, vehicle, transition and import reports; deferred content never becomes PASS. |
| 3 | Complete reviewed task/milestone commits and dependency/parent push gates. | Check exact source pins, required tests, private-data exclusion and publication results. A successful local build is not a completed push. |
| After M7 closure | Hand off the final state and review M8 priorities with the owner. | Re-rank deferred timing, graphics/performance/platform and launcher work before starting a new experimental campaign. |

Continue until M7 is reviewed, closed, committed and pushed, genuine owner input
is needed, or observable weekly allowance reaches 30%. After each task closure,
check live usage before beginning the next task. Do not invent a usage value
when the environment cannot expose it. Serialize GUI/game/build/private-state
operations; primary owns acceptance, status, commits and pushes.

## Accepted tests and branch control

No owner-only M7 test remains identified for the unchanged accepted packages.
Do not repeat rename, name/progress reload, actual F10, subtitles or Landmark.
Do not ask for another Tournament/Pinkslip replay, a winning Wager, an
intentional Race Back softlock, autosave interruption or full playthrough.

The active owner entry is `Run M7 Profile Candidate.cmd` into `private/p1`.
Its immutable hybrid is host82A69B5E/runtime542DD347/launcher67B8F527/GPU E2EA29B7,
with full hashes in the [profile report](evidence/M7-profile-rename-20261010.md).
It runs stock timing, 2x target, VSync/fullscreen with named RTX 5060 selection.
Visible name/progress reload, pause/audio/Alt+Enter, actual window-only F10 and
the later Landmark completion/result/return observation are accepted.

The explicit MAIN archive693B31EB led to renamed p1 returnEFF52498/saveEE824CF5.
Clean5D073219, Sandbox5D6CB88B and subtitle s2 EBBDB184 remain separate branches;
do not merge purchases or progression. The normal0.16.0.1 pointer and old
packages remain preserved. [Save acceptance](evidence/M7-009-save-acceptance-20261010.md).

The repaired launcher D8A954 and later fresh-build A75B8D8B have a synthetic-native-tested 32-record combined
history guard; unchanged p1 launcher67B8 does not. Preserve/archive the complete
branch and prepare a reviewed successor before capacity requires more records;
never delete history or silently patch p1. This is not a request for a new game test.

Reviewed SDK v0.20.0.0/`5dd58f894bf34b9ea85e59d3903c7f0e8044d853` and fresh
WGC-enabled 82/82 and default-OFF 77/77 Release builds are engineering evidence only.
They have not replaced p1 or inherited its gameplay/F10 acceptance. WGC remains
OFF by default. [Integration](evidence/M7-002-sdk-profile-integration-20261010.md).
Any future replacement follows the changed-artifact review in M7-013.

## Coverage and retained limits

- [Available events](evidence/M7-003-event-acceptance-20261010.md) include current
  owner-attested Freeway completions and the targeted Landmark result/return.
  Unknown names, individual rewards and unreported outcomes stay unknown.
- [Vehicle](evidence/M7-004-vehicle-acceptance-20261010.md) and
  [transition](evidence/M7-007-transition-acceptance-20261010.md) acceptance keeps
  historical garage/city coverage distinct from current motorcycle/recovery tests.
- [Media](evidence/M7-006-subtitle-replay-20261010.md) accepts a controlled s2
  caption case, reported video/Y skip and repaired tutorial; English gameplay
  does not certify other languages or music continuity.
- [Collected evidence](evidence/M7-011-representative-set-20261010.md) and the
  [coverage matrix](campaign-coverage-matrix.md) bind cohorts and save ancestry,
  not one same-artifact campaign.
- KI-001's bounded local-profile route and KI-031's reproduced callback are
  closed; KI-028 Windows continuity is contained. Rendering/music, Race Back,
  wheel centering and deferred Deck deployment retain their known limitations.
- F10 images/audit completion establish neither exact temporal capture alignment
  nor measured FPS. Exit0 and saved snapshots do not prove graceful worker teardown.

Future route records still require artifact/config/GPU and save identity,
observed entry/outcome/lifecycle, evidence links and explicit untested/deferred
cells. Static labels and aggregate counters cannot create physical PASS.

## Deferred M8 timing harness — preserve, do not retry

Original no-game WGC fixture8 and stock startup4 retain their bounded pixel/
ownership/cleanup acceptance. Persistent burst1..4 remain FAILED. Burst3 showed
~109ms late-prefix captures and ~15ms GPU-ready polling. Scoped highres waits
passed CPU A/B/A and lifecycle tests; burst4 then rejected the first timed frame
because its WGC timestamp led GPU-ready by2.3384ms. There is no established
replacement clock-error bound. No tolerance/delay/clamp or source-time substitution
was admitted. All capture launch gates are disabled/consumed; no attempt5 prepared.

Full432 Python tests PASS; protocol2314 and wait270 CPU checks retain their
recorded scope. These are not physical timing/scene/cadence PASS. Candidate
interval1 never ran; Live/B/A2 remain disabled. New temporal transport/oracle
design is deferred together with the exact A/B/A experiment by owner decision.
[Native evidence](evidence/M8-001-native-windows-harness-20261003.md).
[Clock-contract limits](evidence/M8-001-wgc-clock-contract-20261004.md).

## Spark and later M8 review

Use HTTPS LiteLLM on the owner's Spark, short aliases, ignored
`private/config/dgx-spark.env` key. Choose bounded packets, preserve receipts,
verify source facts/numbers; returned suggestions cannot execute tools or confer
physical PASS. Existing useful quote/extraction results and rejected jobs retain
their individual dispositions. [Worker contract](dgx-spark-worker.md).

After reviewed M7 closure and push, stop at the milestone handoff and agree the
M8 plan with the owner. Review correct fixed/dynamic/uncapped timing, measured Windows
performance, Deck frame-time attribution, graphics modes, Linux/macOS/ultrawide/
cache/launcher scope and measurable acceptance. Deck's requested one-third
frame-time reduction is a target, not a measured removable budget. Do not add
overlapping timer costs or revive old experiments without a matched baseline.
