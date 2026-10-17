# Windows internal START-only navigation port

Automatic navigation and one stock short31 collection are verified below.
This is a diagnostic prerequisite, not timing/FPS/simulation or milestone PASS.
The public checkpoint includes only the generic START latch and its tests;
private integration, title masks, images and binaries are not release inputs.

The owner explicitly requested porting the existing Deck automatic navigation
into the Windows harness, with routine Spark use and a5,000 existing-credit floor.
The owner also clarified that Escape was pressed AFTER the prior build closed.
That does not explain the preceding missed Return/START attempts. The unrelated
earlier agent12 stop remains tool-reported/disputed; no new cause is asserted.

## Reuse and boundaries

Historical accepted source:
`private/evidence/M8-001/stationary-hold55-20260928/revision-1/hold55-mcla_app.cpp`.
It uses a synthetic input driver, a two-observation PRESS START glyph gate and
a5-second START hold. It also sends six A dismissals and later route/pause inputs;
those actions are deliberately excluded from the new navigation-only calibration.

The old SetGameplayGamepad wait could treat cancellation as an observation and
could leave START latched on a failed short-circuit. Its5-second hold began after
an up-to20-second ACK wait. The new pure `StartLatch` starts the deadline at
publication, checks time on every guest read, always becomes neutral at expiry,
requires separate down/neutral ACKs, and cannot rearm. Analog channels stay zero.
Only the diagnostic build has this option; it is disabled by default. A dedicated
InputSystem contains only this driver, excluding physical SDL/MnK merging.

Navigation-only phase: inherited title masks at1280x720, paired image/provenance
API, START, neutral ACK, five fixed post-input observations. These images inventory
the outcome; they do not automatically prove gameplay or freshness. Supervisor
normal owned-process closure and independent image/integrity review remain gates.
At the initial navigation-only stage no scout READY or measurement was enabled.
Later automatic stock/interval1/
stock controls must match the input actor; manual owner9 is not an identical actor.

## Initial verification (historical preparation)

- Native pure input test:123checks,0failures, optimized clang-cl build.
  `private/autostart-tests-20261017-1` retains build and run output.
- Independent source review GO for navigation-only isolation, bounded input,
  paired capture, exclusive output and worker teardown. Native runtime remains
  pending. Review does not approve future scout/READY integration.
- Historical title masks copied exactly, SHA256
  `0A5F0B3C9A137054F53B6F023D58A586AF79EE22A3039A25F1201D08197D9DB5`.
- Separate stock overlay build/preparation and fresh isolated run are pending;
  normal l12 and MAIN are not modified or promoted.

Later same-turn preflight: historical title gate569checks/42pinned fixturesPASS;
independent build audit20,256pinsPASS,84reused/1replaced/1newobjects. Host53,579,776
bytes SHA256`E6CF68D508D824769BBE990DFE8535151D606E33E11A206F8C38543A01966025`.
Missing/malformed/expired-ticket rejection controlsPASS without title/profile/cache.
Supervisor35mocktestsPASS. Independent review identified neutralACK exactdeadline
inequality mismatch: regression RED first, then corrected `<` and GREEN35tests.
First preliminarymanifest is preserved stale/unlaunched. Finalmanifest
`private/autostart-stock-inputs-20261017-2.json`,853pins/184046bytes,
SHA256`7603E18AB1B27C3E3843ECB8D9D34FCC68AAF546D9E1C0516DABF514A46E9DA5`:
primary/independent immutable trees, sourcepins, fresh stage and9MAIN/l12 baseline
checksPASS. Run `private/evidence/M8-001/autostart-stock-20261017-1` launched once.
The mask point set is private game-image-derived data, explicitly ignored; do
not stage it or the historical screenshot-fixture inventory for public release.

## First native return

Both primary and independent reviewer verified all853pins/threeimmutabletrees,
nine protected MAIN/l12 files, seven native artifact hashes and all six images.
START was read77times; downACK+3ms, release observed+5007ms, neutralACK+5091ms.
Actual guest reads enforce5000ms before returning buttons; release is not backdated.
Images: PRESS START; two loading-map; two loading-zoom; RX8street with LOADING.
Automatic entry into save loading is proven; completed free roam is NOT accepted.
Normal WM_CLOSE ACK/exit0/Job0, no forcedcleanup/pending/errors. Result SHA256
`92427AB5D3F1689787B7827F86DB8AB55E037B6E94EEBDA8C6119AEA269B5A25`.
No OS-level input, desktop capture or owner navigation. Run is CONSUMED.
Next diagnostic changes only post-neutral spacing5→8seconds for the same five
observations, retaining the95second native budget, one START and strict review.
Source changes intentionally stale the consumed manifest; frozen build source
copies and accepted return remain preserved. Never rerun this identity.

## Second native return (navigation only)

Fresh build2 changes only the fixed observation spacing5→8seconds; native95s
budget and one5s START remain. Independent20,256buildpins/source/link reviewGO.
Host SHA256`759A5B90071095BFEE4CF2AF1AF1F172B7FF7DB6CE35A16854CC0B803AB906C1`.
Fresh stage and missing/malformed/expired ticket gatesPASS. Finalmanifest
`private/autostart-stock2-inputs-20261017-1.json`,853pins/184063bytes,
SHA256`936A981254559B00F154FDEC22A78BFEAB10DF2E55ECFF4512B44D7C7D9CA3B0`.
Primary/independent admissionGO. One run at
`private/evidence/M8-001/autostart-stock2-20261017-1` is CONSUMED.
Primary viewed all6frames: title;loadingmap;loadingzoom;parkedRX8nightstreet;
same parkedscene with KarolSMS inpost4/5. Completed loading is visible, not an
identical clean measurement scene or FPS proof. Independent all6image/853pin/
3tree/9protected/7artifact/receipt/cleanup review agrees; navigation-only GO.
Native64downreads/downACK+22ms/releaseobserved+5005ms/neutralACK+5038ms.
NormalWM_CLOSE ACK/exit0/Job0/no forcedcleanup, immutableinputsunchanged=true.
Supervisor result SHA256
`276F5D86D681EA83AAF88E491D1D74DD852E053491A1FB471783465D42729054`.
Next integration keeps two fixed paired post4/5images and primary scene assertion;
new source provider23mocktestsPASS, explicit prelaunch110sdeadline and noCUclaim.

## Combined scout integration and first failed transport

Independent native source/buildGO: same input plus journal installation before
navigation, closed success receipt before scoutWAITING, same token and restricted
flag allowlist. Build `private/host-autostart-scout-overlay-20261017-1`,20,259pins,
83reuse/2replace/1new; app/scout reproduce except COFFtimestamps. Host53,580,288
bytes SHA256`063CC0CC7D7715F9AC9A4A1FBA1F112889ABA1807752D0CA39C953BE909DF304`.
Native tests scout1087/autostart1088/swap133+73/glue194 PASS, two existing macro
redefinition warnings understood. Python35navigation/23sourceREADY/56combinedPASS;
actual SetEvent-bracket deadline bug RED7/GREEN fixed before native admission.
ASTscan and3rulesPASS. Missing/invalid/expiredticket gatesPASS, no runtime dirs.
Full-run input provenance explicitly qualifies legacy scout input=false field.

Combined stock attempt1 is CONSUMED, FAILED, never rerun. Finalmanifest1073pins/
221821bytes SHA256`42F5CC5EDDFB085C2FE47F1025226DABECAE1CEC9D978B82D9BF7A088A730C04`.
Run `private/evidence/M8-001/game-work-autostart-20261017-1`; armed source archive
SHA256`18168CA3B354E2FB2FCC6C6E5D68AF98F22B38CBEEF084548C8175E9D1081873`.
Navigation completed; primary saw fixed post4/5 parkedRX8nightstreet, post5SMS.
Request appeared83.7743s after prelaunch anchor, leaving26.2257s. Tool delivery/
review/publication exceeded that bound: publisher rejected at its first freshness
check before writing any assertion. NoREADY signal; no accepted measurement.
NormalACK/exit0/Job0/noforcedcleanup; supervisor immutableinputsunchanged=true.
Result SHA256`EDBA84BC94B3EFD552F26C9EFDBBFBA9EB2ACD661D06960F8ABA62088870A043`.
Next one separately identified transport diagnostic may emit the two fixed images
and hashes immediately in one read-only tool response. No native110/180s budgets,
capture schedule, seed or EXE change; late delivery stillfails. This is not an
automatic retry or acceptance of the consumed run. Credit59233.0618 at admission.

## Combined attempt2: automatic stock collection

One fresh identity retained the same EXE, seed, native budgets and capture
schedule. A read-only watcher delivered the fixed post4/post5 pair in one tool
response; primary actually viewed both before publishing the bound assertion.
No image selection/search, automatic scene verdict or deadline extension.
Manifest1074pins/222013bytes SHA256
`42CDAC0A4F2A65CBF30F3EFE37E5E841B5E1F2B41F329F880D5E9846EEF303C3`.
Armed source archive SHA256
`7D33D090D4DA58852A6D6A83D534734B7DE9F3153F82F5384256DC34A3717E4F`.
Run `private/evidence/M8-001/game-work-autostart-20261017-2` is CONSUMED.

The watcher observed the request with23.7824426s remaining. Its timestamp is
not the model's image-delivery timestamp and cannot measure transport latency.
Actual READY SetEvent bracket2801036065422..2801036065469 at10MHz ended
13.5790675s before the original deadline; assertion age0.0974195s.
The first failed attempt remains FAILED and is not replaced by this success.

Native43downreads/downACK+15ms/releaseobserved+5001ms/neutralACK+5034ms.
Primary viewed all6 navigation images: title, three loading frames, parkedRX8,
parkedRX8 with KarolSMS. All31 later measurement BMPs were individually viewed
at full source size: parkedRX8nightstreet, hidden idle HUD, traffic passing,
no loading/menu/pause/phone overlay. This establishes the sampled scene only,
not every intermediate frame or external-display presentation.

Primary and independent1074pins/three immutable trees/nine protected MAIN-l12
files/seven navigation artifacts/request-assertion bindings PASS. Fullread_pair
equals the stored result after standard JSON tuple/list normalization; all31
image and13sidecar pins match. NormalWM_CLOSE ACK/exit0/Job0, no forcedcleanup.
Supervisor144135005bytes SHA256
`6FEAE78256D194A1D9D733C149CA0F8F0B214806A5706B25F6015FC91F73CC57`;
source metadata20454bytes SHA256
`E6FABBB47E583A04F0ABF2F513847DF93FCF934DEC6ABF21B96D993B46A7E0FB`.

Reviewed one-shot exact-ID derivative at
`private/evidence/M8-001/game-frame-store-autostart2-20261017-1` records
31sources/31Updates/62work/248targets/248stores,0blockers/exclusions,69retained
context issues and760raw stores. All existing nested analyses agree with the
supervisor. Status remains VECTOR_STORE_JOIN_DIAGNOSTIC_INCONCLUSIVE and all
proof booleans remain false; arithmetic, state units and simulation are not
established. Analysis SHA256
`A0FF626EE0EA64366ADDFD5EB42615BF263D4DBC6F87B59358F70C2AA869C980`.
Independent fullJSON replay and36code/15raw pins match before and after.
Analysis94408420bytes; receipt SHA256
`84BCFF21A635C51DE585E0FFA5B5C8F64B66870369278041F496F1B5FD13E7DC`.
Consumed supervisor disarmed only after return/derivative acceptance. Its frozen
armed source remains preserved; intentional disarm stales the live-source pin.

The generic latch can be built standalone with a C++20 compiler, include path
`src`, and `tests/mcla_autostart_input_test.cpp`; its123checks cover missing
ACKs, exact deadlines, delayed timers, rollback, cancellation and rearm.
Native integration remains default-off/private; normal launcher/host and saves
were not changed. Repository checkpoint version is separate from the inherited
private executable version. M8 remains4complete/6deferred/19total YELLOW,
M8-001 open. Next: matched automatic interval1 B and stock A2, then review their
captured source-to-state evidence; no60FPS promotion from this collection alone.

## Spark contribution details

Job `private/evidence/M8-autostart-port-spark-20261017-1`, alias `qwen3.8-27b`,
completed in178.719seconds. Primary checked all six exact source quotations and
all receipt artifact hashes. Accepted as an ordered source index: title gate,
START helper, configured loading wait, six A dismissals and following capture.
The fifth quote is only an incomplete call site and is not implementation proof.
The response cannot establish ACK internals, default loading duration or gameplay;
these limits were retained. No model-generated commands executed, no percentage
or savings claim. Final text SHA256
`1BF016D9D9672EB87BCF30AAD0A87800C8ECCF28014A7794F412A0A6744234C4`.

Live credit balance during implementation60586.5483; no purchase/reset/push.
