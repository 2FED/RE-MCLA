# M7 remaining owner tests and engineering gates

2026-10-10. M7 is not closed. Accepted racing observations do not need replay.
The two isolated owner tests passed on the accepted stock host. Local profile
rename was required by the owner's Oct10 decision and now passes owner gameplay
and reload, including visible OrangeNoob in the interface. Pause/audio/Alt+Enter
also pass. The owner corrected their initial F10 report, then completed a separate
F10 test. Its image,300sample audit,helper cleanup,session and preserved profile
now pass primary/independent review; no repeated smoke test is needed.

## Owner scope and accepted coverage

Latest October10 follow-up: owner recognizes the race-to-start sequence and
confirms it worked; no repeat is needed. The static `beatMeThereRace` label is
still a source-binding lead, not independently proven by that recognition.
Owner also confirms the startup promotional video works and every encountered
cutscene can be skipped with controller Y. These are representative observations,
not a claim about unseen videos, decoded Bink identity or subtitle coverage.
Owner's subsequent Arcade screenshot lists Ordered, Circuit and Landmark only.
Unordered is unavailable in that observed menu and deferred without PASS under
the existing no-grind decision, not a claim of global absence or Goal Attack access.

Owner then reports not noticing subtitles despite explicitly enabling them when
starting the main save. This is an unresolved subtitle observation, not a PASS
or a reproduced defect. Current host config and preserved return logs/metadata/
diagnostics expose no decoded subtitle setting; the saved title TOML records
achievements only. No save mutation or speculative caption-export fix was made.
Owner screenshot subsequently confirms the visible SUBTITLES ON option.
The subsequent controlled s2 replay passes representative subtitle rendering:
owner confirms the Lester scene with subtitles ON and visible; primary and
independent F10 review show a readable English caption at Great Palace.
No all-scene or telephone-subtitle claim; media scope has not been waived.

Owner subsequently identifies a specific expected-subtitle reference: the early
cutscene after Sunset Strip Race at the Great Palace parking lot with Lester.
This is an owner-named reference scene, not a current-build failure observation;
no reference clip or line-by-line comparison has been collected. Telephone and
incidental dialogue remain unsuitable controls until their expected behavior is
established. The historical M5-001 post-OOBE seed is still present: read-only
hashes match the route contract (save E8B559E1..., header1DAEF55F...,537756B).
That contract targets Sunset Strip/Trevor, not the subsequent cutscene trigger.
Owner requested the isolated diagnostic replay, then found Sunset Strip already
completed in s1. That is a wrong-seed result, not a subtitle failure. Owner now
explicitly selects the existing Clean Profile Test save rather than a new game.
The same subtitle CMD targets an independent copy in s2; original Clean and main
profiles remain unchanged by preparation. Owner subsequently completes Sunset
Strip and the subtitle observation; no repeat needed. F10/session integrity pass.
A separate unexpected-guest-exit package follows window closing despite exit0;
preserve the teardown limitation separately from accepted subtitle rendering.
[Exact identities and instructions](M7-006-subtitle-replay-20261010.md).

The owner explicitly identifies the earlier championship as Tournament, reports
a Wager loss, and completes Goal Attack with its damage-goal mark preserved in
the event list after restart. Do not infer a Wager win/stake amount, all-goals
credit, or exact course IDs. Delivery, Red Light, first Time Trial, Kenneth Hahn
Series, Tournament, repaired tutorial and Pinkslip observations retain their
individual scope in the [coverage matrix](../campaign-coverage-matrix.md).

The owner sees an available motorcycle for purchase, but cannot confirm Payback
or South Central event availability; reports Hollywood Central and Doc's Garage
as the two open garages. A garage's location alone does not prove expansion
event completion. Owner explicitly approves deferring currently unavailable
content without PASS, until naturally unlocked or M8. No content-unlock grind
is an M7 gate; this does not exclude already available distinct mechanisms.

## Verified latest return

`private/evidence/M7-003/wager-goal-attack-owner-20261010-1` contains 11 payload
files / 3,917,297 bytes plus receipt. Collection held the source launcher's
root-derived mutex, required no MCLA process, verified source/copy hashes and
full-profile equality, and archived the complete profile without modifying it.
Primary independently rehashed all 11 returned payloads after collection.

Return receipt SHA256: `E39145AD1D96491E659B2F28AB70788982FC89197425D2705A1460E4245127B8`.
Cumulative log: 48,789 bytes, `91986E72A026FF634FB0CADAFE51EB22BBD8404B17DA483B97C346E4EB77E67F`.

- Full profile/archive: `693B31EBB2A0F92EE6DD452F60A053DB5086B9A573FCBA479E840E4E9E8865C8`.
- Main save: `49DE59C7F2BFEFE6FE54AE2756D7EF3F58E5110F2A71FFAFF7CB1C137C56E134`.
- Header: `1D717009B10A84525F468AD826A7DC016B744FA78274865A7DAB8A1BB65A4957`.

The cumulative candidate log contains four sessions, including the two new
13:03–13:21 and 13:26–13:27 Kyiv sessions. All select RTX5060; latest sessions
reach window-close and the existing hard-exit path. They corroborate process
restart, not decoded goal or money values. Earlier two sessions must not be
counted again. INFO logging does not establish absence of DEBUG-only stubs.

## Accepted isolated owner tests

Both use host `DCDE2F12014C29277BB76B919ED539119C88BC6D49B816F12F28290EF97E2F70`
and image helper `D4894275DC0311B905C8E666E8F585FC2D81883A638194537674C7208AA0C98D`,
stock timing, named RTX5060, 2x, VSync and fullscreen. Only immutable game files
are hardlinked; executable/helper/config/launcher files are independently copied.
No game has been launched by the agent for these tests.

### Clean new game

Entry: `Run M7 Clean Profile Test.cmd`.
Bundle: `private/bundles/M7-Windows-Stock-Clean-20261010-1`.
Receipt: `FB0F7988EADE9818A3914A71BA7F883217827BC14769A962DC34FE6FC9512722`.

All six mutable roots were empty at preparation. Owner confirms the clean test
completed with all requested checks PASS, including new save and fresh-process
continuation. The initial missing-save/create-new-save prompt was expected for
this empty profile, not evidence of lost main-campaign progress.

The current SDK hardcodes local display name `User` and offers no rename setter
or setting. Rename is an engineering gap, not an owner action. Content/profile
settings paths use that name, so a future rename must preserve stable storage.
This test retained local signin state1, not state2. KI-001's bounded clean-start
route now passes; multiple accounts and renamed-profile gameplay remain untested.
Owner explicitly chose to implement persistent rename in M7, not defer it.

Return `private/evidence/M7-002/clean-owner-return-20261010-1` contains 22 payload
files / 2,617,948 bytes. Receipt SHA256:
`E219133A6AB69FCDE6AA12AADCC3EFBD2C03637227C777FBCC71994D7E561C40`.
Full profile/archive:
`5D07321954B57ED0D4383464B073587F7AD70DA87DFA81263659CE9F900457F0`;
save `D8E9AE500E5F9AC38B26D8A7749A7CE8F78C5D6ABDC4B82A586674A997B6273F`.
Two logged sessions, 13:47:10–14:11:10 and 14:11:26–14:12:41 Kyiv, corroborate
restart on RTX5060 with local state1 and 30,035 guest mappings.

### Disposable gameplay sandbox

Entry: `Run M7 Sandbox Test.cmd`.
Bundle: `private/bundles/M7-Windows-Stock-Sandbox-20261010-1`.
Receipt: `862443FE142F6AFA8775F4A7266EF8DAAFEC05D3884C77B75E6C94925673E635`.

Independent full copy of the explicit archive693B31EB above. Purchases, losses
and progress remain in this sandbox; never promote it or continue the owner's
main campaign here automatically.

Owner confirms motorcycle use, tuning purchase, Restart/Abandon, police arrest
and release after a fine, working driving/pause, and purchases preserved after
restart. This accepts the reported recovery route; no separate police escape,
damage reset, exact motorcycle/item identity, or every customization category
is inferred. These successful checks do not need replay.

Return `private/evidence/M7-004/sandbox-owner-return-20261010-1` contains 27 payload
files / 12,467,392 bytes. Receipt SHA256:
`ECD50AD091A1A311DECCCC69200E885A4C06C43A0A93B410D0D153DA85A5E3F9`.
Full profile/archive:
`5D6CB88BEA5A4867783E6CE2FD861D8FE2836E7EB6599D395E5821C7A9F5B69E`;
save `8D489F0FF20C5C6B15D448ADDC40F1012B297BE30DFFDDA1B08E0C5A80EF3E38`.
Two sessions, 14:13:12–14:31:19 and 14:31:41–14:32:24 Kyiv, corroborate restart
with the same RTX5060/local-state1/30,035-mapping runtime.

Primary and independent review rehashed all 49 payload files and full-profile
archives. Both collections held the corresponding root-derived launcher mutex,
required no MCLA process, and copied to absent destinations. Main profile remains
693B31EB above; neither test profile is promoted. Clean achievements outside the
Xbox-profile subtree are preserved in the return, not included in its archive hash.
Window-close/hard-exit and four partial automatic exit packages do not establish
graceful teardown or new gameplay crashes. Reload invalid-fetch warnings remain
a limitation; no bypass was enabled.

Historical garage/streaming/AI evidence guides this combined sample, but is not
silently promoted to current-host PASS. No all-vehicle/all-category purchase tour.

## Autonomous engineering completed and remaining

Completed preparation: safe return/archive, independent content/release audits,
isolated clean/sandbox staging and launch preflights, source safety review, and
opt-in WGC packaging support. The initial sandbox staging review caught a
precreated-profile-directory conflict before execution; corrected before its
single successful stage. A new package fixture initially failed due to PowerShell
dynamic variable shadowing; fixed without changing acceptance checks.

Publisher now accepts `-EnableF10Wgc -CppWinRtIncludeDir <installed cppwinrt>`.
It requires clean Release, checks feature cache/header/embedded host pin, copies
the helper, and binds host/helper in optional strict v2 `image_capture` metadata.
Verifier enforces exact topology/hash/pin checks; default and historical v1 return
behavior remain distinct. Independent source review passed. The separate profile
candidate is now clean-built and relocation-verified; the normal root is untouched.
[Exact artifacts and tests](M7-profile-rename-20261010.md).

Remaining agent-owned gates:

- Collect and review the prepared candidate's owner renamed-profile/gameplay/
  reload return. Name/storage separation, full-profile journaling, named GPU,
  WGC/2x settings, clean build and relocation already pass their engineering
  gates; clean new-game PASS does not establish new-runtime physical acceptance.
- Reached-import audit closure and bounded compatibility claims
  and residual defects, relevant regression tests, independent review, task
  commits and milestone commit/push. Proprietary data must not enter Git.

Owner rename is complete. After the actual-root path-length failure, the unchanged
package/profile/journal were copied to short `private/p1`; actual-destination
snapshot/session probe passes. Use only `Run M7 Profile Candidate.cmd`, already
redirected there; no second rename. Original long-root package remains preserved.
[Correction and tests](M7-profile-rename-20261010.md#owner-launch-path-failure-and-correction).

The short physical check is complete, including the subsequent F10. The
owner accepted main progress/name (including visible UI name), driving,
audio/pause, Alt+Enter and same-entry reload. Full return/archive EFF52498... and
two complete gameplay sessions are preserved in the
[rename report](M7-profile-rename-20261010.md#accepted-renamed-profile-return).
No deliberate autosave interruption/power cut or repeated name test.
Independent final artifact/entry review GO. The candidate starts from MAIN693B;
Clean/Sandbox purchases are not merged. Preserve old branches and avoid playing
both as competing main profiles.

## Checks and limits

- Pure local configuration: 23 checks PASS.
- Save lineage: 123 isolated checks PASS in PS5/7; no owner save/runtime touched.
- WGC/default/legacy package fixtures: PS5 and PS7 each PASS, 5 positives,
  105 fail-closed negatives, 45 source checks, 2 cache and 3 path-budget positives.
- Session fixture: 1 positive / 10 negatives PASS.
- C++ runtime closure: 3 positives / 8 negatives, seven placements and signatures PASS.
- Campaign seed: 1 positive / 13 negatives PASS; no invented gameplay credit.
- Native launcher recovery with accepted candidate launcher and dummy executable:
  stale lock, interrupted session, unfinished diagnostic preservation and follow-up
  session PASS, synthetic profile unchanged. This does not launch MCLA.
- Clean/Sandbox independent safety review and PS5/PS7 CheckOnly PASS; bounded
  owner gameplay/restart acceptance and 49-file return integrity review PASS.
- Scoped ast-grep source scan clean; all three rule fixtures PASS.
- Final launcher profile fixture: 26 calls/21 rejections, two dummy launches and
  two rename journals PASS in both shells; actual lineage/archive consumed.
- Clean Release 82/82 and relocated diagnostics/session PASS. Final native
  recovery fixture and PS5/7 wrapper CheckOnly PASS; 33 immutable files verified,
  exact named RTX5060 index1. The disposable relocation copy was cleaned after
  evidence preservation; candidate and original profiles remain available.
- SDK profile/content/CVar focused suites PASS. Source-isolated complete inventory
  414 cases/17,300 assertions/four known skips PASS; existing monolithic memory
  fixture collisions are documented separately, not passed over as success.

Normal pointer, original campaign and historical branches remain unchanged.
Latest primary-reviewed F10 return:220entries/55314850B,receiptBE32F888,
profileEFF52498/saveEE824CF5 unchanged; see the rename report. M7-001/008/010/012
are locally committed; M7-005 is deferred without PASS. Source0.18.4.0 does not
relabel the accepted0.18.0.0 binaries. Milestone integration/review/push remain;
latest live allowance87% weekly remaining, no reset used.
