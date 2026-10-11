# Current state and handoff

Updated October 11, 2026 Kyiv. README-AI owns the milestone ledger and dated history.

## Active outcome

M7 is CLOSED. Owner approved revised M8 on October10: internal launcher first,
named selectable personal save folders outside builds, baseline Windows/RTX5060
bug fixes and stock/max60. HigherFPS/platforms/release-ready packaging are later.
[Active scope and safety gates](M8-priorities.md). M8-015 committed b67e1be;
M8-016 accepted in separate ledger commit c0a6225. M8-018 remains open for
volunteer/export gates. Owner added M8-019 humane launcher UI/UX refinement;
separate native Play/Profiles/Setup sections and explicit profile operations.
Owner accepted l3 tabs/Ctrl+Tab/path preservation, then requested M8-019 follow-up:
clear START GAME color+text, read-only readiness check and operation-specific
profile dialogs instead of permanent label/destination/source fields. Work active.
Owner accepted l5 automatic startup/green readiness, collection-parent prefill,
Cancel preservation and fullscreen edit/recheck. Then requested fixed window
geometry and removal of unnecessary scrollbars. New private/l6/MCLA-Launcher.exe
SHA256 A665EB7D... is accepted: owner confirms no scrollbars, clipping or Cancel
regression. M8-019 is complete; next is M8-004 actual render targets. Main and all six operation dialogs have no resize
frame/maximize. Overflow-only scrolling remains for constrained work areas, with
coupled axes and offset reset covered by native tests. l5 remains unchanged.
The accepted l5 behavior adds
available collection-parent prefill for Create Empty Profile, automatic background
startup readiness and renewal of unchanged expired checks. Relevant edits require
highlighted CHECK SETUP; navigation, modal Cancel and successful unchanged
Save/Verify do not invalidate. Failed Verify explicitly blocks previous approval.
All backend callbacks are serialized; worker results bind draft generation,
profile root and GPU inventory. Close hides/drains a pending checker before the
controller is destroyed. No detached callbacks or retry loop after failure.
Optional collection-parent.dat remembers successful create/import/copy/restore
parents; old preferences stay v1-compatible. Without a valid hint, the selected
acknowledged managed profile's parent is suggested. No implicit creation/migration.
Primary final fresh l6 build: UI855/0 twice, controller153/0, import31/0,
staging63/0 and production worker-thread preflight4/0. AST scan/3rules and diff
check pass. All17 receipt files verify. Host9195117A, manifest9C6BEA71 and
ticket6C2FA6EA stay identical; l2/l3/l4/l5 launcher hashes unchanged. Nine120DPI
hidden renders inspected; native hidden edit/combo text is not a visible-render proof.
No actual game launch or personal prefs/save writes. Production preflight used
a fresh synthetic TEMP profile, preserved its bytes/inventory and created no
session. Earlier l4 helper gates remain linked evidence, not new gameplay claims.
Existing MAIN and l2/build preferences should stay unchanged; no reimport.
Render-target inspection confirms current integer1/2 paths720p/1440p;1080p needs
separate renderer/guest-mode proof,2160p needs scale3 admission/runtime validation.
Do not represent a1920x1080 window as a1920x1080 render target.
Earlier l3 import-clears-drafts regression was reproduced and fixed; owner now
accepts tabs/Ctrl+Tab/path preservation. l3 launcher138E1CB9 remains unchanged.
Its historical test counts and signed admission are retained in UI evidence.
Existing preferences still point to l2/build and MAIN; keep those, no reimport.
No actual prefs/saves/l2/l3/l4/l5 modifications. Source0.20.2.0 records M8-019
acceptance; accepted l6 binaries are not rebuilt for the source version bump.
[UI evidence](evidence/M8-launcher-ui-20261011.md),
[owner check](internal-launcher-ui-check.md). Hidden renders fit but do not draw
edit/combo text, so no full visible UI/physical keyboard PASS is inferred.
Source VERSION0.20.2.0; accepted l2
remains its original0.19.0.0 and has not been rebuilt/replaced.
Owner confirms all requested controls/audio/AltEnter/F10/reload/name/save and
launcher-close/game-survival tests. Exact sessions, snapshots and image agree.
Native launcher source now includes named profiles, guarded canonical roots,
cross-build writer locks, atomic local settings, immutable build/game admission,
owned child jobs, stock-only GPU/scale/fullscreen controls and complete snapshots.
New managed host mode uses a read-pinned comment-only session config plus sanitized
CLI settings; it neither loads nor saves the build's mcla.toml. VSync Off is
rejected: current SDK changes guest vblank to1000Hz when disabled, so it is not
a validated stock capability. Rename/import/duplicate/restore remain explicit;
restoration creates a NEW independent profile, never overwrites the original.
Rename/recovery/import integration passed native fixtures and scoped review.
Latest native PASS counts: policy117, codec315, preferences138, managed-config50,
filesystem72, UI399, process41, store107, build109, game70, snapshot21,
import-policy72, production-import29, controller69. New controller matrix proves
two valid different builds contend for one profile and independent profiles run
simultaneously without mixing saves/cache/diagnostics. Six native symlink cases
SKIP with Windows1314; no invented PASS.
Current staging63 PASS; exact15 embedded game metadata rows match retained manifest.
New candidate private/l2: launcherBE79E0C3..., host9195117A...,
build manifest9C6BEA71.../game95DA2709... . Exact staged guest-free host14 PASS:
TEMP/mcla-host-probe-527025b9147efbd4dd7c7ef7cc299a0b, empty temporary profile,
explicit roots, lifecycle log markers, immutable sentinel and final snapshots.
The guest-free probe alone is not gameplay evidence; actual owner return below
now supplies gameplay/F10 acceptance. No automatic profile import occurred.
Read-only Verify build/game button reports exact identity without saving/launching.
Hidden UI layout render did not render edit/combo text; native value assertions
pass, and owner successfully completed visible setup and the full requested test.
Spark qwen3.8-27b routine test inventory completed; all7 exact source cases checked.
Private receipt: private/evidence/M8-launcher-spark-20261010-1.
Implementation committed b67e1be; M8-016 acceptance is recorded separately.
Three sessions complete/exit0; six snapshots and F10 artifacts hash-verified.
Successful frame48B04081... shows actual nighttime road/Mazda at1600x900.
The155-sample audit is incomplete, not performance PASS. Separate automatic
unexpected-guest-exit .partial and no-frame package are not failed F10. SDK
hard-exit/report classification remains M8-017; launcher finalization passed.
[Exact owner return and review](evidence/M8-launcher-owner-acceptance-20261011.md).
[First-run instructions](internal-launcher-owner-test.md) use explicit full p1/user
import into the owner's chosen permanent parent. Never progress both branches.
[Exact evidence and limits](evidence/M8-launcher-foundation-20261010.md).
Owner approved test-build expiry overnight: l2 launcher and host enforce a signed
14-day allowance; cacheable signed GitHub grants can extend it. Host compiled
MCLA_TEST_BUILD_EXPIRY=ON, ticket created1791667418/expires1792877018 (Oct25
00:23:38 Kyiv). Policy1417/HTTP26/runtime37/signing66 checks pass; actual copied
host rejects missing/forged/expired tickets before SDK setup. Stricter lifecycle
negative probes use no cache/network/game assets. No ongoing-game expiry timer.
Owner explicitly approved publishing only the signed allowlist on October11.
DebugBundle root commit7878d4877fcae1a9d2baf73414c2fe58aa09b3eb is pushed and
remote-verified; its entire tree is one active-builds.txt (634bytes), no ancestors.
Public HTTPS200 bytes match F8252FEF91FF13C0118681F173AD5B5234065FF06D18D2915BB9EBB7FC7F9872.
Native production admission now returns extended=1/expires1794086618
(Nov7 21:23:38 UTC), and exact signed bytes are in the local lease cache.
No build, game, source, save or diagnostic payload was uploaded. Main/index and
staged l2 artifacts are unchanged. This is live signed-extension PASS, not gameplay.
Nonexportable current-user CNG key MCLA-R-TestBuild-Leases-v1 was created once;
public key embedded in src/mcla_build_lease_public_key.h. Never recreate it during
ordinary builds or assume repository backup contains the private key.
[Expiry evidence](evidence/M8-build-expiry-20261011.md),
[signing/operator policy](test-build-expiry.md). Original l1 remains untouched.
Explicit720p/1080p/1440p/2160p render targets requested for M8-004. l5 names the
existing720p/1440p choices;1080p/2160p remain unavailable, not implemented.
Original p1 is unchanged. Owner explicitly imported MAIN and tested it; no
candidate or personal data was sent to volunteers.
All15 mandatory tasks are committed complete; M7-005 is deferred without PASS.
M7-015 commit3a16edd and independent aggregate review GO; M7 closure source0.19.0.0.
Closure commit d6ca114823dd3af0c3db2fe9b18522a0e8e9f15f was pushed and remote main
verified equal. This follow-up handoff records that completed publication.
Weekly usage3% used/97% remaining after M8-016 commit;
check after each closed task, stop at30% remaining. Never infer physical PASS.

Recent commits: e0ee93f events,8da30b0 profile/source integration,39c294e save
lineage,3a351ef targeted regression,b0872c5 Windows portable acceptance.
Owner completed requested Arcade Landmark; completion/result/return only, not
unreported win/reward/reload. Freeway5+ also accepted. No owner test remains
identified. [Matrix](campaign-coverage-matrix.md), [regression](evidence/M7-013-targeted-regression-20261010.md).

## Accepted gameplay package and saves

Active owner entry is private/l2/MCLA-Launcher.exe, selecting MAIN at
C:\BDU\MCLA-Saves\MCLA-f5d835a679843f1e95de84646d2a8646. Import from p1/user;
profile snapshot164A8DC4..., main save439E2A84..., display-name257870D8... .
All six imported files match both live MAIN and original p1 after acceptance;
no new progression bytes claimed. Keep playing MAIN, never merge/reimport p1.
Owner reports launcher-close survival; logs prove window-close/final snapshots,
not fully graceful SDK worker teardown. No physical replay needed.

Historical Run M7 Profile Candidate.cmd -> private/p1, stock timing,
2x target,VSync/fullscreen,named RTX5060. Do not replace binaries or merge saves.
Host82A69B5E922DE80726F76B2345C9FFE1FE143FFB485E4269FB1586DE9230509A;
launcher67B8F5272016D686EDF2E8B5FBAE43945B8E00E19917637AC8DF29C1DEB5F927;
runtime542DD347B56B0494F42A7B261EC8309897E867C14FE1D2456B1836AE6C12650F;
GPU E2EA29B7F8750619F079F5717198EFC7F180F92E1A9AA1914FD13577155F6939;
helper6E766EDF536C8CA8B65BC5234C37A4A4BDA1463470E203538E71798F882B4675.

MAIN693B31EB -> rename09D0FBF1 -> accepted profile
EFF5249882D8E895AA820199EB800BD33F427555AD205797CA02EE0FC8B45F6A /
saveEE824CF54BBE717783909C62EEBC135B37C29B4516A4D177D70E88A593228DEB.
These are preserved acceptance returns, not a re-read of later live gameplay.
OrangeNoob visible/name/progress reload/pause/audio/AltEnter accepted. Initial
F10 was not pressed; later actual F10 accepted with image19F2FEE7,6artifact
hashes,300audit samples,helper cleanup and session. Three deduplicated preserved
gameplay sessions exit0, snapshots3/2/1,zero watcher errors.
[Rename and returns](evidence/M7-profile-rename-20261010.md).

Clean5D073219/Sandbox5D6CB88B/s2 remain separate; no purchases merge.
s2 is existing Clean-copy, not new profile. Sunset Strip -> Great Palace/Lester,
SUBTITLES ON and captions accepted by owner and independent image review.
Frame0CADE932...,profileEBBDB184...,save482840D9...,session exit0/3snapshots.
No subtitle replay. [Media evidence](evidence/M7-006-subtitle-replay-20261010.md).
Normal portable pointer0.16.0.1 and old branches remain unchanged.

Old p1 launcher lacks the newly fixed combined32-record admission guard.
Preserve/archive and publish a successor before exhausting session/rename
history; never delete history or silently patch p1. The source fix is tested
with synthetic native children, not deployed into that immutable package.
Exit0/automatic unexpected-guest-exit-after-close is not graceful teardown PASS.

## Integrated source and tests

SDK branch mcla/mcla-r-hotfixes HEAD5dd58f894bf34b9ea85e59d3903c7f0e8044d853,
tagv0.20.0.0 and branch pushed; both remote refs and fresh public fetch/fsck verified.
Exactly7profile/API files in the latest SDK commit; unrelated
graphics and tests/unit/CMakeLists WIP remain untouched/excluded. Parent pins0.20.
Dependency-first publication and parent closure/push are complete.
Never stage all SDK files. [Aggregate review](evidence/M7-closure-20261010.md).

Isolated private/sdk20s with25pinned dependency clones;15libmspack120000links
materialized only there after Windows denied native symlinks. Expected dirty
dependency, original pristine. One-shot staging/materializer must not rerun.
Build sdk20b -> sdk20o, install private/sdk20install-unpublished.
Install registered CMake user package; original accepted install untouched.
Explicit prefix/Developer PowerShell required; default original install is0.19.

Tagged tests:401unit cases/3364assertions/41suites/4known skips,1462PPCcases/
5753assertions PASS. Not monolithic Memory fixture PASS.
Receipt private/evidence/M7-002/sdk-unit-isolated-c141c040bb064edabe0e6facb14c0ff6.
Runtime3171D2FA...,GPU E463D15E...,CLI209F87C5... are unpromoted builds.

Fresh WGC ON82/82 host6A0B12A7 and defaultOFF77/77 hostF8E7123B PASS;
both67generatedfiles/133916828bytes/65sources+objects verify manifest34505E79.
New launcherA75B8D8B; earlier historyfixD8A954C7 distinct from p1 67B8.
No game launch on fresh artifacts. Structural scan0.45.0/rule3PASS.
Native tests8hotkey/119protocol/1809audit-rearm/51fakeclient/78adapter/133serializer.
Native profile26calls/21rejects; combinedhistory37calls/3rejects/31children/
32records with capacity recovery bothPS5/7, ordinary recoveryPASS.
DualPS lineage123,bundle5positive/106negative/45source,session1/10,
diagnostics9/76/177,analysis1/16,soak1/44/106,generated1/5,buildmatrixPASS.
Alpha2/40 keeps historical27M6tuples separate from current31issues.
[Full integration](evidence/M7-002-sdk-profile-integration-20261010.md).

## Next work and safety

Primary owns README-AI/current-state/task status/commits. Independent acceptance
and publication reviews returned GO; final docs and dependency/parent push passed.
Scope/native safety and owner return reviews pass. The two-task M8 checkpoint
is GO for UI/UX refinement, not milestone closure. Primary owns final tests,
new-candidate staging and review; all delegated code writers are frozen.
Keep l2/l3/l4/l5/l6 unchanged. M8-019 final owner acceptance and review are complete;
next actual render targets in M8-004 after the task commit and live usage check, then baseline bugs;60FPS,
performance and volunteer distribution remain open.
Use l2 with the existing MAIN; never import p1 again or overwrite the live profile.
Independent architecture/privacy audits found current launcher bundle-root locks
cannot safely protect one external profile across builds. Existing publisher
includes game+seed; never ZIP p1 for volunteers. Legacy profile import must be
explicit, complete and archive-backed, never an implicit folder-switch action.
Sanitized historical link dependencies are committed without experimental code.
No git add -A. Untracked proprietary ISO, private,
generated,out and unrelated M8 scripts/SDK graphics must remain local.

The accepted aggregate review preserves source-vs-physical distinction, unavailable
Payback/SC/observed-menu Unordered deferrals, English-only language claim,
open visual/audio/busy-area limitations and exact temporal harness NO-GO.
Don't rerun consumed native WGC fixture/burst gates or launch interval1.

Spark https://192.168.113.90:4000/v1, normal TLS, short aliases.
Ignored private/config/dgx-spark.env holds LITELLM_API_KEY; never print it.
No PC inference. Owner permits project code on own Spark, not unrelated secrets.
Smoke/real advisory workflow already reviewed; [worker contract](dgx-spark-worker.md).
START-HERE and skills addendum read. write-page and diagnosing-bugs used;
OCR inspected only, not invoked.
