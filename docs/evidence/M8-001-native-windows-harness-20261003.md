# M8-001 native Windows harness checkpoint, 2026-10-03

Current status: **owner8 full no-game WGC fixture accepted**, including primary
integrity/oracle and independent visual review:13 exact frames/all8 states,
all14 inputs, normal helper/fixture cleanup. Build4 uses strict completion-time
freshness without tolerance/retry. Cohort3 stock GDI remains a black-image NO-GO.
Fresh cohort4 stock A1 now passes startup/passive ACK/owned WGC legal-splash pixel
and normal-close/seed integrity review.384 mocks/1528 CPU checks PASS. Timing-series
cadence is NO-GO; isolated persistent burst implementation/build now passes
1921 CPU checks and411 full Python mocks. Owner burst1 then FAILED at stage8
after1 warmup+16 timed frames; exact guard unknown, no pixels/cadence admitted.
Owner burst2 identifies LateSchedule before18th timed sample; no pixels/cadence.
Owner burst3 fails LateSchedule; wait-only build5 intervention in owner burst4
then rejects first timed frame at GpuFreshness (source2.3384ms ahead of GPU-ready).
The consumed burst gate is disabled pending temporal-contract redesign, not retry.
M7 remains0/16 closed; M8-001 open. No game rebuild/version/commit/push or product
promotion. Historical checkpoints below retain their original scope and dates.

## Oct4 owner burst4: strict temporal-contract counterexample

Result11650B SHA `0FB129BC3BEEEAC57F22F7195FB5709A54D65DE2189F7CE4292EB5155488AB5B`;
launch358B SHA `F34A8E9725BD0F8AE5BCF89545133F92E8E1960F1763934DF046ADAC1E5B8BDF`.
Primary and independent18-input rehash PASS before gate disable. Build5B39D1BC1...
helper19664/birth134355748981494769; fixture39364/birth134355748969134327,
HWND2427316/thread41484, physical1280x720/PMv2/DPI120, original geometry.
Helper exit1/Job0/mapping closed; fixture normal WM_CLOSE/exit0/Job0; no force,
errors, retention or survivors. Only launch/result files; no pixels/game/saves.

One completed warmup, zero completed timed samples. First timed slotindex1/
one-based2 fails Guard26 GpuFreshness at GPU stage9, not LateSchedule:

- Arm2254655567975; Available2254656536714; Submit2254656538702.
- GPU-ready2254656550812; compositor2254656574196; catch2254656551305.
- Frequency10000000, so native qpc_100ns conversion is exact identity here.
- Source leads GPU-ready23384 units=2.3384ms and catch22891=2.2891ms.
- OwnerBefore->Available96.8709ms; Submit->GpuReady1.211ms; arm lateness5.5us.
- RowPitch/End/FrameClosed/OwnerAfter zero: rejection occurs BEFORE Map/copy.

Warmup whole14.6344ms, Submit->GPU-ready3.3737ms. The new wait's CPU-only A/B/A
proof remains valid; this single warmup/failed sample does not establish a full
series speedup or isolate GPU execution cost. Previous15ms polling bucket is
absent in this observed timed sample. No long-schedule behavior measured here.

Actual-tuple replay of the original strict compositor predicate raises
AssertionError (RED); durable regression retains the counterexample using real
q100 conversion, never adds tolerance. All8 schedule/model regressions PASS.
This refutes GPU-completion-as-universal-source-upper-bound in these recorded
observations. Driver bug, future planned-present time and WGC implementation
cause remain unproved; cannot infer from numeric lead alone. Moving the check
later, sleeping until stamp, clamp/jitter allowances or rebranding failure PASS
would not independently establish a pixel-production-time bound.

Gate disabled/consumed. No owner attempt5 prepared. Independent timestamp-contract
and architecture reviews agree no demonstrated replacement error bound exists.
Primary read the [clock-contract research](M8-001-wgc-clock-contract-20261004.md).
A condition-driven wait to the reported stamp was considered, but not adopted:
even honest added wait does not establish the lower-bound post-arm production
proof, and cannot fix unknown event-time semantics. A distinct observational
transport/unique producer-paint oracle remains a possible separately reviewed
future design; it cannot replace interval1 elapsed-bounds evidence silently.

OWNER DECISION Oct4: defer exact stock/interval1/stock experiment and temporal
harness work to M8; resume representative M7 on stock. This supersedes the
harness-first order, not any result. Full432 Python tests PASS23.380s. No further
native fixture staged/launched; original helpers/ABI/extractor remain strict.
M7 remains0/16; no candidate/game launch or task/commit/push. Weekly93% remaining.

## Oct4 owner burst3 timing-prefix diagnosis

Result61223B SHA `2D0E71F793E895436912613A4A21A89559F4AD8A2DA3AF1B2570D417EF94C6E6`;
launch358B SHA `1DE6944EF06688A40652615A7687E6BE9FF1E9243F1F8D8D588F3DB037CD28DB`.
Primary and independent reviewers rehashed all17 inputs BEFORE disabling consumed
attempt3. Same build3 C0AD7A01...; fixture15956/birth134355733070188729/HWND33887170/
thread13116, helper29268/birth134355733075633104. Prefix21=1warmup+20timed validates
the same successful-header guards, not pixels. Helper exit1/Job0/mapping closed;
fixture normal WM_CLOSE/exit0/Job0, no force/errors/retention. Only launch/result
files; no game, save, input, desktop capture or image persistence.

Guard15 LateSchedule before timed21/slotindex21/one-based22 arm. Due2238769809747,
previous slot End2238770858837, failure catch2238770859606, frequency10000000.
Previous End was already104.909ms late; catch follows76.9us later and is104.9859ms
late. Backlog precedes the final guard: not an isolated long final owner check.
Failed acquisition fields zero; all completed off-slot counts zero.

For late completed slots10..20, whole brackets107.481..110.749ms (median109.2223),
OwnerBefore->Available90.670..93.449ms (median92.3776), Submit->GpuReady14.839..
16.441ms (median15.2833), GPUready->End0.850..1.658ms. PreviousEnd->Arm5.7..13.6us.
Across all20 selected timed source observations, successive compositor deltas
104.1604..118.0562ms (mean109.65021). These are observed selected-source timestamps,
not independent producer paint times or a guarantee of WGC delivery cadence.

All20 timed host brackets exceed20ms; first adjacent widths sum63.5957ms exceeds
necessary40ms. Unchanged `_series` rejects exact relative timestamps (tiny
synthetic payload solely for timing seam);12/19 upper gaps exceed/equal200ms.
Two durable regressions bind this rejection and pre-guard backlog to the saved
tuple; all7 schedule/model tests PASS. NO cadence/content/timing/M7 acceptance.

Ranked leads: producer/fresh-frame wait, coarse Sleep1 polling, real copy latency.
Source has Sleep1 in all three schedule/frame/GPU polling loops. A separate
CPU-only direct kernel32 Sleep1 probe (32 samples, no timer-resolution change,
frequency10MHz) measures14.7852..16.0777ms, median15.5373ms. This corroborates a
polling-latency explanation, not proof all recorded GPU-ready latency was sleep.
[Microsoft Sleep contract](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-sleep)
does not promise immediate dispatch after the requested delay.

Next controlled change: helper-owned unnamed/noninheritable auto-reset high-
resolution waitable timer, one-shot1ms waits, bounded20ms API wait, fail closed,
owner/deadline checks around waits, no timeBeginPeriod/priority/spin/fallback.
Same fixture producer100ms, frame selection, native ABI and strict gates remain.
Independent design review supports CPU-only A/B/A before native admission; a
source-cadence bottleneck may remain. No claim this alone will pass `_series`.
[CreateWaitableTimerExW contract](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw).

Implemented new isolated `mcla_poll_wait.h` with injected operations seam and
checked ownership lifecycle. Primary integrated it at the three former Sleep1
sites; original owner/deadline checks retained plus checks surrounding each wait.
WaitError Win32 code maps to HRESULT; timeout/unexpected result fails closed.
Source reviewer required two clarifications/corrections before measurement:
CPU-only child uses held subprocess timeout/kill+wait, NOT a capture Job0 claim;
all three console CPU/probe invocations now explicitly CREATE_NO_WINDOW.
No other accepted single-shot helper, producer, protocol, reader or extractor
changed. All previous evidence remains immutable and no failed result is promoted.

Discovery build4 not admitted. Frozen build5:
`private/evidence/M8-001/wgc-burst-build-20261004-5`.
Helper172544B SHA `B39D1BC11D961AA53FC1D23E1843E4BC64BE8BD38E1BFB51CC5BA1FA3964297A`;
receipt318632B SHA `4B764DF44FCF84F4D48AD3F0E6FCE622767B94CE9BD362A92CC95A89E06189D3`.
Primary and independent492-input/21-output hashes and AMD64/PE32+/GUI2 PASS.
All3 `/O2 /W4 /WX` compiles exit0,2314 protocol and270 injected wait checks PASS.
Header6F1F1ABB...; probe45E82825...; helper-sourceFCA7B9E3.... Parent's initial
manual verification used a relative output path and was correctly rejected by
safe_path; absolute-path read-only recheck passes. No build/evidence rerun.

Explicit CPU-only `--measure-waits` runs exact utility A1(Sleep1)/B(highres1ms)/
A2(Sleep1),32 samples each. Raw96 QPC pairs positive/nonoverlapping/inside the
1.0352815s observation; frequency10MHz. Median waits15.94015/1.53965/15.6746ms.
B range1.5191..1.6067ms; A1 range7.1424..16.1331ms and A2 range12.0972..16.0799ms.
Reported process CPU deltas all0 at API accounting resolution, NOT zero CPU cost.
No foreground/UI/capture/game/registry/timer-resolution changes. Source and
independent saved-artifact review GO only for this transport intervention;
not GPU execution, source availability, 10Hz cadence or game timing acceptance.

Full431 Python tests PASS25.329s; final39 focused reader/fixture tests PASS6.620s.
Fresh owner attempt4 repins build5 and additionally pins the new wait header
(18 launch inputs rather than17). Wrapper and source gate agree on output4;
PowerShell parser PASS. Actual native fixture has NOT run.
Final admitted-gate `preflight()` PASS without launch (helper/receipt/PE, fresh
output4 absent); diff check PASS, LF/CRLF notices only. No capture/game processes
found after CPU test completion. Final admitted-gate9 fixture mocks PASS1.928s.
Expected discriminator: GPU-ready observation and polling delays may shrink; first-eligible source wait
may still cause LateSchedule or strict cadence NO-GO. Same100ms producer, fixed
epoch,91 slots, all time/content/privacy checks; failure prefix retained, no retry.
Next owner action is one updated `Run WGC Burst Harness Test.cmd`, keep owned
window foreground/unobscured until closure, report result regardless of status.

## Oct4 owner burst2 LateSchedule / temporal-model checkpoint

Result8607B SHA `46C759BD2CE3018AFCF2C00372A61E1B0AF83A775DB4C6AA68644C0AE61BB892`;
launch SHA `CAE64C60611CA3D3953E006FE535C74E104003FE0E42A1835AA39EF02F8D1BEC`.
Primary rehashed all17 original inputs before disabling source gate. Independent
review matched16/17 afterwards, with only the deliberately changed gate differing.
Owner27252/birth134355719860858496/HWND1706420/thread9028,PMv2/DPI120/client1280x720
at320,194; helper40444/birth134355719951495111, exact C0AD7A01...build3. Initial
foreground/unobscured check passes. Helper exit1/Job0/mapping closed/no force/errors;
fixture normal WM_CLOSE/exit0/Job0/no force/errors/retention. No images persisted.

Guard15 LateSchedule, failed slotindex18 (one-based19), preceding completed counts
1warmup+17timed=18, offslots0. ScheduleOrigin2225625990003, due2225642990003 equals
origin+1.7s; failure catch2225644068576. Frequency10000000: catch107.8573ms afterdue,
NOT exact comparison time. Source guard proves comparison at least100ms late.
Arm/available/GPU/copy/close fields allzero: no request for this failed sample.
Helper begin2225621802719/end2225644766828 ->2.2964109s, ready after0.4187284s.
No actual source cadence, GPU cost, driver fault or connection to burst1's generic
failure can be inferred. Both attempts remain FAILED; no M7/capture/timing credit.

Read-only actual-tuple replay RED on the late-slot condition. Independent source
and algebra review finds `_series`10Hz whole-bracket gaps80..120ms require adjacent
capture widths sum<=40ms; ideal100ms arms require each<=20ms. Pure real-extractor
replay passes20ms, rejects20.001ms. Synthetic106ms-source/5ms-copy model completes
17timed then fails18th with102ms lateness; this is a falsifiable counterexample,
not a fit proving the actual producer rate. Initial model epoch misplaced the
warmup-copy offset and failed its expectation; corrected explicitly to start
the schedule after warmup. Five model regressions PASS. No captured pixels used.

Research skill delegated primary-source reading into
[WGC temporal-model note](M8-001-wgc-temporal-model-20261004.md), reviewed by primary.
Documented compositor timestamp and host acquisition are distinct; no WGC-specific
error bound or guaranteed delivery frequency adopted. WM_TIMER100ms is not an
exact clock standard. Source timestamps are NOT substituted into existing brackets;
no separate timing schema adopted yet. Current M70/16/YELLOW, scope unchanged,
estimate remains provisional. Next two items: stage-cost evidence, then evidence-
selected transport/temporal design before any stock/candidate experiment.

To identify the cost without changing native execution, parent now exports the
already-stored completed-prefix headers after exact timely exit1/Job0. At most
91 fixed384B slot headers plus384B global, no payload/unused-slot reads. Shared
per-slot success validator preserves identity/crop/visibility/clock/queue guards;
global failure is never turned into success. Prefix counts must match1warmup+
remainingtimed; independent reviewer caught a contradictory-kind-count case,
regression RED before correction/GREEN after. Durations arm-to-owner,owner-to-
available,available-to-submit,submit-to-GPUready,GPUready-to-end plus lateness
are mixed API/wait/attestation costs, not isolated GPU timing. Failure remains
terminal; no selected-frame replacement or runtime retry.

Independent source/privacy GO and44 focused tests PASS. Helper/native/protocol/
fixture-producer/extractor hashes unchanged from burst2; no rebuild/native run.
Reader281BDAF3..., fresh attempt3 uses build3 and original strict schedule.
Final full429 Python tests PASS26.288s. After independent GO, primary admitted
one-shot attempt3 source gate; `preflight()` only (not `run()`) passes helper/
receipt/PE/fresh-output checks. PowerShell parser PASS; no MCLA or WGC helper
processes found. Native/protocol/producer/extractor hashes above unchanged;
`git diff --check` PASS (existing LF/CRLF notices only). No native test launched.
One owner-started updated CMD is the next required action; FAILED is expected
to remain possible and will now preserve cost witnesses. Prior attempts immutable.
Weekly94% remains; no task/milestone closure, version, commit or push.

## Oct4 owner burst1 failure / diagnostic preparation

Immutable result6612B SHA `69A1657507D68158EEA4259B25682DB664E736CB466C983334EBC9612E758AEE`;
launch SHA `0D8C6FFD9BEE77E8538A2667A9F0BCF12D63DC12A856B8879936AFE4E4B85F19`.
All17 recorded input pins rechecked before source edits. Fixture19076/birth
134355711421200963/HWND3017364/thread38928,PMv2/DPI120,physical1280x720 at320,194.
Single initial visibility check foregroundtrue/occludedfalse; readiness1 and
square-corner setter/flush ACK. This does not prove visibility stayed valid.
Helper33104/birth134355711433040027, pinned build2 F663BE7B..., returned1 unforced,
Job0/no cleanup errors, mapping closed. Fixture WM_CLOSE delivered/normal exit0,
Job0/no force/errors/retention. No MCLA, saves, input or desktop capture.

Helper reports Status2/Stage8/0x80070057 E_INVALIDARG, TotalCompleted17 =1warmup+
16timed,OffslotTotal0. Begin2217103332036/end2217124879529/frequency10000000 gives
2.1547493s helper lifetime, NOT FPS or validated cadence. All payloads rejected,
no saved frames. Counts are helper metadata, not independent pixel acceptance.
The failure has no precise guard/timing witness; no cause can be inferred from
the generic code. Original record remains FAILED; source gate consumed/disabled.

Primary read-only replay rehashes17 inputs and explicitly fails assertion that
Status==3 (RED actual-result replay). A targeted mock regression first failed
KeyError FailureGuard, exposing lost diagnostics, then passes after ABIv2 reader
changes. This fixes observability only, not the original native failure. Ranked
leads: fixed-slot late schedule; nonpositive/backwards/out-of-window compositor
timestamp; changed owner/geometry/foreground/occlusion during sampling. Exact
guard and failed-slot timestamps discriminate; no thresholds or retries changed.
ABIv2 uses the existing3 reserved global words for FailureGuard/FailureSlot/
FailureQpc; same mapping/slot/pixel sizes. Parent may read only global plus exact
one384B failed-slot header after timely unforced exit1/Job0, identity/pin checks.
No failure pixels, completed-slot sampling, scene or cadence admission. Failed
slot word45 also records the preceding source timestamp, allowing numerical
duplicate/backwards comparison if eligibility was reached; zero after an earlier
failure means unavailable, not proof there was no previous frame. Successful
slots require zero in that field. Slot words46..47 remain reserved; sizes unchanged.

Reviewed diagnostic build3: helper170496B SHA
`C0AD7A01FE03BC4BD410BD2D2422CF0BBC178EF9E09D8D5E7DE7A707EF7B3A9D`;
receipt256018B SHA `36DD9FC137C9E9D309169E63BBBC150626BE3730C88FD5CA99F02BBEE2F61967`.
Native source/protocol/test hashes respectively6D43EBC8.../9B1D29BF.../9C81D78A....
Frozen build2 dependency census reused with unchanged header sets; `/O2 /W4 /WX`,
2314 CPU checks PASS (including53 stable IDs, duplicate/backwards witnesses and
320-value eligibility equivalence census). Primary490-input/12-output rehash and
AMD64/PE32+/GUI-subsystem2 checks PASS. Final Python full420 PASS30.879s including
failure-previous-witness extension;35 focused burst/runner tests PASS after pinning.
Independent source/privacy/build review GO;490-input/12-output/GUI-PE rechecks
and35 focused mocks PASS. No actual burst2/native/game run. Exact helper/receipt
pinned; new output/launcher points at attempt2, gate admits one owner-started
no-game diagnostic. Previous capture_bracket fractional-frequency formula was
already in original frozen C15B25 build2, not changed by this diagnostic work.
Read-only ABI2/build3/attempt2 preflight PASS; output still absent, no launch.
Need owner to run the updated CMD once and keep its colored fixture unobscured
until it closes. This is the current user blocker. Do not rerun on failure.
Weekly allowance observed95% remaining; no task/milestone closure/commit/push.

## Oct4 bounded burst implementation (previous preparation)

New files `mcla_wgc_burst.py`, native `mcla_wgc_burst_capture.cpp` /
`mcla_wgc_burst_protocol.h`, CPU/mock tests, build script and separate colored
fixture runner. Old accepted one-shot CPP/protocol hashes remain AE32C1AD... /
292F184C..., no binary/game/save changes. One persistent D3D11 device/session/
pool/staging/query,1 stored full warmup then90 fixed10Hz target slots. Exactly
335,497,728-byte anonymous mapping:48-word global and91 slots, each48-word header
plus1280x720 BGRA. No helper file/network/input/focus/desktop capture or retries.
Default notification border remains enabled; own-window metadata-only Z-order,
DWM cloak, monitor,PMv2/DPI,PID/birth/HWND/thread guards persist through waits.

Each timed bracket is actual ArmQpc before all drain/wait/attestation through
EndQpc after GPU copy/post-owner/frame close. Separate available/submit/GPU-ready
witnesses distinguish costs. Fixed origin follows completed warmup; no schedule
reset/catch-up or thin compositor/midpoint substitution. Only positive strictly
monotonic pre-arm timestamps aged<=.2s can be declared off-slot metadata,<=16
per sample/1440 total, without pixel access. First eligible frame is irrevocable;
warmup cannot discard/replace. Unknown/stale/backwards/shape/owner/time/copy errors
fail the whole burst and clear payloads. Native soft15s,parent hard20s; individual
native calls still depend on OS responsiveness, with exact owned-Job cleanup.

Parent checks all91 headers before any payload read and reads failed-exit1 global
header only. Unresolved retained helper ownership keeps mapping alive. Transport
success does not imply cadence: unchanged `_series` runs on all90 original timed
brackets. All91 full RGB grids must pass before any pixels persist; a valid-content
cadence NO-GO is recorded honestly. Fixture default safety20s unchanged; explicit
square-corner burst-only lifetime40s covers visibility/capture/normal close.

Discovery build1 then frozen-dependency build2 under
`private/evidence/M8-001/wgc-burst-build-20261004-{1,2}`. Build2:
helper167936B SHA `F663BE7B166907E4DF9AEF6E78C4EF1E60B028FB8F6EB8840161A678686B3497`;
receipt256018B SHA `5630DEB63A86821750050E70608604BEF8935E87E80215900684FEDEC210E6E6`.
`/O2 /W4 /WX` compile/link and1921 CPU-only protocol checks PASS; primary rehash
490 inputs/12 outputs PASS. Full411 Python mock suite PASS21.660s; wrapper parser
PASS. No native capture/game executed by those tests/builds.

Independent review identified Python/native equality mismatch for same-tick
owner/availability witnesses, incomplete warmup-end exclusion, and helper-launch
OwnershipError potentially replacing the fixture cleanup owner. All corrected
with regression tests. Ownership rollback test drives the real runner with
mocked APIs, proves original fixture normal close and FAILED result; no GUI.
Final independent source/build/runner review GO:51 focused tests PASS,490 build
inputs/12 outputs rehashed, AMD64 PE32+ GUI subsystem2 verified. Constants pinned
to exact build2 above; native source gate now admits one no-game owner-started
fixture. New `Run WGC Burst Harness Test.cmd` is separate from consumed old
launchers. Owner-started visibility is required
because prior automatic foreground contexts failed before capture; no repeat
of those known failures. Memory/observer overhead and actual cadence remain
unmeasured. Fresh output `native-wgc-burst-fixture-owner-20261004-1`; click only
colored fixture if needed, keep unobscured until closed, report console and do
not rerun. No scene/timing/gameplay/M7 credit, version/commit/push. Weekly95% remains.
After admission constants were set, read-only pinned preflight PASS (no launch),
26 burst/runner mocks PASS3.846s. The console FAILED inside the suite is the
deliberate mocked helper-ownership rollback negative, not a physical fixture run.

## Oct4 stock4 acceptance / cadence feasibility checkpoint

Stock4 result `814CE4459B0F0914EE46CD3F8264DBB5E026CAEA554FAB79B5BAA3E6B016C2AF`;
launch `061DBF3BA5B6A477F43B1B2246837DBADC008EBD5CEEAC5B31CC809A0A610B11`;
WGC BMP `377708EEF6592C0A89784E1310691D844D2E9D59D5280208D763FFCB6FA90E8B`.
Primary and independent acceptance: all20 input pins before disabling consumed
source gate, actual readiness prefix/VFS+DEV proofs, DXGI index1 RTX5060 and
logged D3D12 vendor/device10DE/2D19,7 loaded package-module pins, exact held
owner/helper identity and cropped1280x720 protocol. BMP shows legal splash text
and Guest FPS30.0 overlay,93731 nonzero RGB pixels. No PressSTART/gameplay or
measured FPS claim. One FRAME ACK present93 took0.0928058s; terminal drain healthy,
no control ACK. Guest publication and WGC image are not proven present-equivalent.

Stock PID7156/birth134355687607999654; helper6024/birth134355687666414723.
Both normal exit0/Job0, no force/errors/retention; mapping closed. Original9
binary/4seed pins and copied4seed pins unchanged. Actual isolated user tree matches
receipt and adds only mcla-first-frame.bmp and mcla-garage-lifecycle-garage-main-before.bmp.
Primary initial overly strict replay expected the entire user tree to equal the
four-file seed and rejected those two intended diagnostic BMPs; corrected replay
checks exact receipt tree plus unchanged four seeds and exact two added names.
No artifact was modified. Original pending-review result remains immutable.

This accepts startup/passive ACK/owned-client legal-splash capture and cleanup
only. Both stock/fixture source gates are disabled/consumed. B/A2 never staged/
launched, interval1 remains unrun. No additional owner action is needed for review.

**Feasibility checkpoint, primary + independent reviewer, Oct4:** M70/16,
M8-001 open/YELLOW; no task closure/commit. The current single-frame pipeline is
not suitable for the cycle extractor's existing timing gates. Replaying actual
fixture8 against _series at5/10/60Hz rejects: outer capture brackets0.1909798..
0.9490328s (median0.2015904,7/13 >=.2s), conservative adjacent upper gaps
0.4202061..1.2002338s. Midpoint rate3.734Hz is descriptive, not accepted timing.
Stock capture bracket0.2317129s also fails and contains only one sample.
Inner helper spans0.160708..0.1821194s (stock0.1915279) cannot replace outer
bounds or be converted into a timing PASS. Exact overhead attribution unmeasured.

Ranked falsifiable leads: repeated helper/device/session setup dominates (a
persistent session should reduce subsequent spans); GPU readback dominates
(post-setup spans remain slow); parent processing dominates (inner spans pass
while outer do not). Capture-stage witnesses in a bounded series prototype are
needed to discriminate, not another unchanged stock run. Next two work items:
(1) independent design/review of a persistent bounded WGC burst preserving owned
window/normal-exit pixel publication and honest time bounds;
(2) CPU/mock regression and a separately admitted fixture cadence test before
game scene/cycle integration. No tolerance/cadence relaxation or implicit scope
credit. Previous assumption that one-shot capture could directly feed cycle
extraction is rejected; additional backend work is required, estimate remains
unbounded until the design/cadence proof. Owner milestone scope unchanged.

## Oct4 owner8 acceptance / fresh stock WGC preparation (previous checkpoint)

Consumed `native-wgc-fixture-owner-20261004-8` original result remains unchanged:
`NATIVE_NO_GAME_GATE_PASS_VISUAL_REVIEW_PENDING`. Primary accepts the subsequent
independent review as disposition, without rewriting historical evidence.
Result82623B SHA `8B64756977CD379DBCC75462024E264BE7DD65A6FD20594B9C6E0C2881E4EA3B`;
launch `451A1F227979C19DC0E4F43C4F2D030E46A0B9EAB11D620A97450CBB35807C9D`.
Primary/independent checks: all14 inputs before disabling consumed fixture gate;
all13 BMP/individual/result receipts and exact full-grid921600 RGB pixels;
decoded owner/crop/QPC protocol,13 distinct helper PID/birth associations.
Phases0,2,5,7,1,3,5,7,1,4,6,0,2 cover all8 states. Independent reviewer viewed
each unique state: complete8x4 grid, no missing corners/foreign content.
All13 helpers exit0/Job0/mapping closed/no force/errors. Fixture PID36692,
birth134355667719272942, HWND3216528/thread41400, physical1280x720/DPI120,
normal exit0/Job0/no force/errors/retention. Game/scene/timing false.

Accepted scope is this fixture/configuration on this machine. Success receipts
contain final QPC, not the early acquisition sample, so they do not establish
that the exact14.8us skew recurred. Actual13-frame capture now passes; no claim
of an OS/driver diagnosis, universal reliability, game pixels or present identity.
Fixture source gate disabled after acceptance; no additional colored test needed.

Stock integration replaces only NativeGraphics pixel backend with the accepted
WgcGraphics and exact helper/build4 pins. Before output/game launch, preflight
also pins the accepted fixture8 receipt. Sources include WGC module/CPP/protocol,
stock wrapper/CMD and frozen artifacts; both success and failed cleanup recheck
them. Failed helper diagnostics are retained through graphics.reader.attempts;
no pixels/frame receipt or retry from failed capture. Existing blank-image guard,
owned geometry, exact readiness proofs, module inventory, passive FRAME ACK,
normal-close/terminal ACK and seed preservation remain. No game corner preference
is changed. Captured client/guest ACK are not claimed to identify the same present.

Fresh `windows-native-integration-20261004-4` copies the unchanged planCC070148...
and stages only A1 through the reviewed copier.9 exact stock binaries,4 independent
seed files and5 empty auxiliary roots verified. No TOML/log/probe output exists;
B/A2 are not staged or launched. Isolation receipt SHA
`24887D0BD5DCAF7C80F843419434A91C33E537C45739EE32A28DC83F836E124C`.
Original normal bundle/seed and cohorts1..3 remain untouched. StockABF722...,
normal mc4 seed58B3385...; interval1 remains unrun.

26 stock integration tests PASS, including all frozen-input prelaunch rejects,
exact WGC factory binding, helper-failure diagnostic retention/no frame/no retry.
Full384 Python harness tests PASS17.547s; PS5 wrapper parse/diff-check and primary
nonexecuting fresh A1/WGC preflight PASS. Independent integration/staging review
reran26 tests,verified plan/source/staged pins,onlyA1/no old outputs and WGC pins;
GO accepted by primary,then only stock A1 source gate enabled. No game launch
performed this turn. Reviewed wrapper SHA
`C4EE49D55387B66CF0672797C43865E3DE1AF8F6394A30BF81619C36B294B90C`.
Next action is one owner-started
`Run Stock Harness Startup Test.cmd`: passive startup window capture/normal close,
no keys/controller, keep game unobscured. No automated attempt in the known-bad
foreground launch context. Live weekly96% remaining, no reset used.

## Oct4 primary-started7 / fresh owner8 (previous checkpoint)

One primary-started7, after reviewed build4 admission and advance owner notice,
FAILED `fixture visibility readiness deadline`.478 samples over9.9986359s all
foregroundfalse/occludedtrue; zero helpers/frames/frame files. Setup/readiness
acknowledgments are metadata only. Normal fixture exit0/Job0, no force/errors/
retention; source preservation performed/unchanged true. Primary and independent
review verified all14 inputs before fresh8 entry/wrapper preparation.
Result `D32DE900FF22A27596F354AA04A163204553CB18FB9158A405BBF715FBF81AB2`;
launch `59C9AE0532D034F30BFD89A9F8B45B79F55FFE929D90F443A0B2DB0AEA30DAFE`.
No information about the freshness correction was collected. This automated
launch context differs from owner6, whose visibility passed; do not relabel6
as an occlusion failure or claim owner error. No automatic retry/focus attempt.

Fresh8 changes only output identity and wrapper notice, retaining reviewed
build4 and all capture/visibility/pixel/cleanup rules. Independent review gives
conditional GO for one owner-started8 after primary fresh-output/helper/entry
preflight. Primary verified all those conditions, wrapper/entry output agreement,
GUI PE/helper pin, disabled stock, PS5 wrapper parse and diff-check, then enabled
only fixture8 admission. Owner runs root WGC CMD once and keeps the fixture visible until it
closes. No new permission required; the dependency is physical foreground in
the owner-started launch context. Stock disabled, M7 still0/16, no task closure.

## Oct4 attempt6 / premature timestamp rejection and build4

Attempt6 result `70916BFD66473EB7B1B543E1E8055DE4DE96A092BB8CF26A965D96C27364FD1F`,
launch `9E86F9974C3C693E30B566E21FC3DF760E798762B7A71FDCD6DC4E6A4F9E58D0`.
Independent review verified all14 source inputs before edits, all4 saved BMPs and
individual receipts, held identity/DWM crop/freshness and exact full RGB grid;
phases7,1,3,6. All4 visual reviews show complete colored grids only. Full run
remains FAILED (4/13), not physical reliability PASS. Fixture normal exit0/Job0,
no force/errors/retention. Helpers1..4 exit0; helper5 exit1/Job0/mapping closed,
no failed pixels admitted. Foreground true/unoccluded; owner confirms the fixture
visibly opens above other windows. Visibility is not the cause of this failure.

Helper5 customer HRESULT0xA0040002/stage7 identifies the timestamp guard.
Actual values: begin2164350872287, acquisition-QPC2164352138379,
compositor2164352138527,frequency10000000. Positive/nonstale lower bound passes;
early upper bound fails by148ticks =14.8us. Deterministic replay reproduces that
predicate rejection. This identifies our failing check, not an OS/driver cause.
[Microsoft documents the timestamp as compositor QPC](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframe.systemrelativetime).
It is not proof of game-present identity; that non-claim remains.

Correction retains the same selected first frame. Acquisition now validates
positive/nonstale timestamp, monotonic sample and existing absolute5s deadline.
Strict unchanged fresh_frame runs after GPU-event S_OK and same_owner, before
Map/pixel reads; existing postcopy and parent strict freshness remain. No clock
tolerance, timestamp rewrite, extra sleep, new frame, retry or geometry change.
This uses readback completion as the upper endpoint, rather than requiring it
at initial acquisition. Still-future timestamps at readback are rejected.

Nine CPU regressions use the actual failing tuple plus explicitly synthetic
completion samples; unknown/stale/future/rollback/deadline remain negatives.
The old behavior-preserving forwarding predicate failed the new positive at
runtime (`FAILED: acquired frame may precede timestamp until GPU completion`).
Two initial /O2 red builds diagnosed the contradictory old checks as unreachable
code (C4702 under /WX); the CPU-only /Od red build exercised the failure at runtime.
Fresh production /O2 build4 is warning-clean and passes1528 CPU checks;
381 Python harness tests PASS19.299s. No physical fix is inferred from those tests.

Build4 helper159232B SHA `5B9BC86FA130A813AA8A69168D7FD2F1D5FA6ECB40FB40340736ECDA6F9AB2D4`;
receipt `B8EDA32AAE507197AC436D7269ACD28E100100D4C30DF2D6D8E7B6895F5F0123`.
Primary rehashed all build inputs/outputs; independent source review GO.
Independent build review also verified488 inputs/12 outputs/GUI PE and89 focused
mocks PASS. No unresolved source/build finding remained before primary7 launch.
Fresh `native-wgc-fixture-owner-20261004-7` is the next bounded runtime check.
Owner is present; primary may start it once with advance notice for attention.
No automatic focus/input or repeat; exact visibility remains mandatory. Original
attempt6 is preserved. Stock stays disabled; no game/save/candidate run or M7 credit.

## Oct4 attempt5 disposition / diagnostic build3 (previous checkpoint)

Consumed `private/evidence/M8-001/native-wgc-fixture-owner-20261003-5` remains
FAILED. Primary replay and independent integrity/visual review accept its single
phase1 full-grid frame (all921600 RGB pixels), not the required13-frame run.
Result SHA `65ECFE31CDE85BFDC4D2F1A95611F46FFCB9D4CE089B859ABB1922933D96CBC5`;
launch `1391C03A314D5807D47F18D34E22A8B02BE4E46D33E84F73728CB3EEF5150B93`;
BMP `A9736468BDA52C31A358917493A3B59D7CEA806E212A7614334D828A75868BF9`.
Fixture PID31980/birth134355646922857030 closed normally exit0/Job0, no force,
errors or retained ownership. Helper1 exit0; helper2 PID39272/birth134355646926506957
exit1, Job0, unforced, mapping closed. No rejected pixels were read or saved.
Runtime source check performed/unchanged true. Independent review rehashed11
unchanged inputs and reconstructed exact old CPP/protocol pins in memory; the
entrypoint has since been deliberately disabled/repinned. Do not claim all14
current source files match the old receipt after intentional edits.

Retained failure header: status2,stage7/frame,HRESULT0x80070057;
QPC begin2152578427540,end0,frequency10000000;
compositor timestamp2152579706458. Timestamp is after start by0.1278918s.
The old helper did not retain the freshness comparison QPC, so end0 does not
prove a zero clock or future frame. Nonzero stored timestamp and stage7 narrow
the old source interval to freshness, Surface/QI/GetInterface or texture guards.
Ranked falsifiable hypotheses: freshness rejection yields customer code0002;
texture invariant rejection yields0003; native API failure retains native HRESULT.
No cause or physical fix is established by this receipt replay.

Build3 changes only diagnostics: retain actual QPC at freshness validation;
map exact source-owned invalid_argument strings to customer HRESULTs:
`0xA0040001` item-size drift, `0xA0040002` stale/unknown compositor timestamp,
`0xA0040003` exact SDR BGRA8 texture invariant. Unknown guards still E_INVALIDARG;
native WinRT HRESULT remains unchanged. No strings/pixels/new header fields,
retry, altered crop, visibility or freshness tolerance. Success still requires
final completion QPC and the existing full protocol. Source independently GO.

Five real guard/catch CPU cases first failed compilation (missing mapper), then
passed with the implementation. Fresh `wgc-window-capture-build-20261004-3`
passed warning-clean helper/CPU compilation and1519 CPU protocol checks.
Helper158720B SHA `06E95E0235AFE7B9658DD4D8C1951212DD4B53A5791983E6AAFC537784AB0528`;
receipt `78D95A8BEC4FCB51D15B0463442394AE90274751210BFAF7F116E170207235AA`.
Build2 and failed evidence remain intact. Full381 Python mocks PASS14.046s;
new integration case preserves customer codes/future comparison metadata only
as a failure diagnostic and still rejects frame admission. No native capture
was executed by the build or these tests.

Independent build review rehashed all488 inputs/12 outputs, checked AMD64 PE32+
GUI subsystem2, and reran89 focused mocks PASS. Primary rehash/preflight agrees;
PS7/PS5.1 wrapper parsing and diff-check PASS. Primary enables only fixture
admission after accepting this review; no runtime has occurred.
Fresh `native-wgc-fixture-owner-20261004-6` uses the same square fixture and
strict13-frame oracle. ONE owner-started WGC CMD is now admitted;
click and keep the fixture foreground/unobscured until it closes. No automatic
retry. Full fixture visual/integrity acceptance still precedes new stock capture.
Stock admission remains false, interval1 unrun, M7 remains0/16. Live allowance
Oct4 is97% remaining; physical owner visibility, not quota, is the next dependency.

## Oct4 owner-return / fresh attempt5 (previous handoff)

Owner asks next action. Primary rehashed unchanged reviewed producer/probe/reader
and helper572FEA26.../PE; fresh output5 absent,stock gate disabled. Enables only
ONE owner-started attempt5,unchanged capture/shape/diagnostic contract.88 focused
offline mocks PASS5.906s,diff-check PASS. No native launch/capture performed.
Owner runs root WGC CMD once,clicks colored fixture immediately,keeps it foreground/
unobscured until closure,then reports console output without rerun. No MCLA/save/
keyboard/controller injection/desktop capture; pending visual/integrity review.

## Autonomous attempt4 / owner visibility blocker (previous handoff)

Owner AFK until tomorrow explicitly requested maximal autonomous continuation.
Primary made exactly one already-reviewed fresh attempt4, no focus/input action.
Result64375B SHA `9A79CD48F86469B51A550487BCDE27B07E6BBAAEFF0703A4D93D8096AA14CCE1`;
launch358B SHA `41348CF5D55DB993320F177EFC14525B58B97B1BA5FE0344BC53ED6C9294CA18`.
FAILED `ValueError: fixture visibility readiness deadline`;480 samples across
9.9938502seconds,all foregroundfalse/occludedtrue. Initial metadata records a
noncloaked overlapping window covering the client. Producer paint readiness and
square-setup acknowledgment passed; no pixel shape proof. Zero frames/helper
attempts; only launch/result files. No data about original helper6 failure.

Fixture PID43200/birth134355198356577324,HWND10490004/thread35704,physical1280x720
at320,194/DPI120. Normal WM_CLOSE exit0/Job0/no force/errors/retention; source
checks performed/unchanged true,all14 original inputs independently verified
before subsequent intentional source-gate/output edits. No game/save/input/
desktop capture. Primary accepts independent actual disposition, not PASS.
Preserve output; no automatic rerun/focus bypass. Fixture admission disabled;
fresh5 fixed output prepared but not admitted or run. On owner return, review
unchanged capture contract/fresh5 identity,admit one owner-started visibility test.
Legal/logo/PressSTART and stock/candidate/scene/timing gates remain pending.

Owner subsequently explicitly granted full unrestricted access and continuation.
Primary read computer-use skill/guidance/confirmations/API and tried supported
`node_repl` initialization,one reset-triggered retry,explicit kernel reset/final
initialization. All three returned `trusted Node process exited unexpectedly`.
No window selection/activation/input/screenshot executed. Stop GUI actions per
skill recovery rule; no custom terminal/UI workaround or system/security changes.
This is a tool/runtime blocker,not missing test permission. Fresh5 remains disabled/
unrun,physical foreground/unobscured and full fixture gate still pending.

## WGC attempt3 partial pixels / helper failure diagnostics (previous handoff)

Consumed output `native-wgc-fixture-owner-20261003-3`: original FAILED result36038B
SHA `FC1037C88AE56AC8BF2134B3984B62ADC016EED7F7FFA874FA0FEF5B649FDB7B`;
launch358B SHA `D99B027D9A7477FDFE750B1C1B95AAA2B5BFF4E40F359E5693E7B0B646E57FAA`.
Primary oracle/hash replay and independent integrity/visual review accept five
saved full-grid1280x720 frames, phases2,4,6,0,2 (four distinct states). All32
blocks and every RGB pixel match; no clipping/foreign content seen. Four BMP pins:
phase2 `A4266B2A299E9BC7DE317E776AC9CD445900DADC55E94F96F1626A6F2EBE0075`,
phase4 `5CEF065CF1B0D33FC1718C11360D516EB573FBEDBD6CFB51637CF0CA2A22EEEC`,
phase6 `FFE37D8943379CEA356362E8DE2CC4FD21DDA642B282CA9E362A555040E00006`,
phase0 `C8CF4857C6C3C2F4903BEF258FF126E3058AA7F9F66DA1AAB9F04A6E4732FC6E`.
Repeated phase2 has identical pin; all five individual JSON receipts verified.
All14 original inputs independently matched before intentional diagnostic edits.
Source check performed/unchanged both true. No agent-run native call or new capture.

Fixture PID12684/birth134355141362513642,HWND8325794/thread17500,client at320,194,
DPI120. Owned producer acknowledged setter33/value1/S_OK+flush. First five helpers
exit0/Job0/no force/errors; exact protocol/identity/current-attempt associations,
compositor QPC and crop1,38 from1282x759 item/content PASS. This supports exact
pixels under this square-fixture configuration, not reconstruction of attempt2's
discarded54 mismatches, arbitrary rounded windows or full13-frame reliability.

Helper6 PID43268/birth134355141375842492 returned1, unforced Job0/mapping closed.
Error `ValueError: WGC helper normal timely exit required`. No sixth frame or
failure header retained; its failure stage/HRESULT cannot now be recovered.
Fixture normal WM_CLOSE exit0/Job0/no force/errors/retention; game false, scene/
timing false. Original full FAILED verdict preserved, not retrospectively PASS.

Diagnosing-bugs actual receipt/5 BMP replay deterministically reproduces partial
pixels plus full failure. Ranked predictions shared: stale compositor timestamp
would fail at frame/freshness; owner/window drift would fail attestation; WGC/GPU
exception or soft deadline would identify its stage/HRESULT. None is proven by
exit1. Correct-seam offline tests (success then exit1, header-only read spy) RED
before delta: missing failure_diagnostic KeyError/assertion. They now pass; this
fixes missing diagnostics only, not physical helper failure.

Reader now reads exactly320B header ONLY after timely exact exit1, known drained
Job0/unforced/no errors/released helper, held owner/window and helper pin recheck.
Validate exact protocol/reserved/FAILED2, signed negative32-bit HRESULT/stage2..11,
owner/client/flags/payload declaration and bounded start-QPC identity. Whitelist
seven integers plus stage label and explicit content/pixel/scene/timing false.
Failed timestamp may be stale or end-QPC absent: diagnostic, never freshness PASS.
No payload slicing/decoder/persistence, previous last_receipt cleared, no retry.
Missing/contradictory header gives exception type only/unavailable, still FAILED.
Crash/hard deadline/late exit/force/unknown cleanup/release/owner/window/pin drift
cannot read failure header. Mapping-close failure remains terminal/no last_receipt.
Native helper/build2 unchanged; no rebuild or new native execution.

Initial87 focused tests/full379 PASS19.980s. Independent review requested explicit
child.closed True after release before mapping read; actual NativeOwner contract
already sets it or raises, but injected no-op release reached diagnostics. New
correct-seam negative RED then GREEN after guard. Final88 focused tests PASS4.206s;
full380 offline harness tests PASS13.190s and PS7/PS5.1 wrapper parse/diff-check PASS.
Independent final88 tests/source/privacy review GO; fresh-output4 preflight PASS.
Primary admits fixture attempt4 only. Reviewed reader
`A07090D177EE25BDA957C45C830A541A24FAD046F9F1BED8B09BA318B5325459`,
wrapper `9E81E03BB46C116FE36E1017AA0F17B7A9CAEFAFAC338DB9CF009A3F2FAD48F3`,
tests `DFE4D128DA4FCF8C537539F8F1AF1867E1DB3579112436E8416182BEF0D2B859`.
Only entry admission bool changes after this review. Latest live weekly2% used/
98% remaining, no reset used; owner interaction is the next blocker, not quota.
ONE fresh owner-started attempt4 only; foreground/unobscured
until fixture closes. Same13-frame requirement, square fixture/helper/crop/oracle/
notification border; only failure metadata differs. Stock/cohort remain disabled,
interval1 candidate unrun, M70/16/M8-001 open, no task commit/push/version/save change.

## WGC attempt2 diagnostics / square-corner experiment (previous handoff)

Owner diagnostic output `native-wgc-fixture-owner-20261003-2` is consumed; keep
original FAILED receipt10532B SHA
`8010DDE4A6327A6CBB2C83B66870F5A3D76FE24C4F98E5BA6D5B60365799B9B7`;
launch358B SHA `219641F11F1A7719183F60963BD4C4434D46794C2E7891A81BD50FDDE8530602`.
Primary and independent integrity review accept all14 recorded input pins,
held/current-attempt identities,exact current backend protocol metadata and normal
helper/fixture exit0/Job0/no force/errors/retention,completed source preservation.
Frames empty,game false. No new native call/window/helper run by the agent.
Fixture PID16128/birth134355129086864232,HWND1838004/thread1828,client1280x720
at320,194,DPI120. Helper PID4824/birth134355129088095180,unchanged572FEA26...pin.
Window/item/content1282x759,DWM[319,156,1601,915],crop[1,38,1280,720]. QPC
1634740106859..1634742301354,freq10MHz,compositor1634741959946 inside bracket.
Current metadata was associated with rejected capture1/helper attempt1; no scene/
present-equivalence/timer/capture acceptance from transport success.

Diagnostic phase2:54/921600 unique RGB pixels differ,maximum channel delta192,
bbox[0,711,1280,720].48 in2px client/grid edge bands,6 outside those bands.
Zero-shift interior20/917604; vertical +/-1=3854,horizontal +/-1=5046,diagonals8859.
This rejects a coherent +/-1 shift as explanation; not every possible crop bug.
All32 block centers match. Planar RGB hash
`E396678D454CDE19632E885B96071FACACA7891E779C18DE707764B7E489687F`.
No raw pixels/per-corner mask/symmetry captured; bbox across both horizontal
extremes does not by itself prove a circular corner mask.

Diagnosing-bugs actual-receipt mismatch assertion reproduced RED54; old receipt
does not turn green from a source edit. Ranked falsifiable predictions shared:
rounded corners -> fixture-only DONOTROUND yields exact full-grid pixels;
bottom-border artifact -> preference alone does not fix it; producer incomplete
edge -> preference alone does not fix publication mismatch. Leading inference is
Windows11 corner composition,not physically proven cause. Existing caption+
system-menu fixture has no thick resize border; changing only its own appearance
is bounded and discarded when it closes.

Primary added opt-in `--square-corners`,defaultFalse. Immediately after own HWND
creation,before ShowWindow,call DwmSetWindowAttribute33 with DWORD1/size4 and
require S_OK,then DwmFlush S_OK. No getter: SDK labels corner preference [set].
Exact owned WM_FIXTURE_SHAPE acknowledgment is available only after those calls/
no producer error; parent brackets PID/birth/window and requires exact cookie
before graphics/readiness/capture. Receipt says actual_pixel_shape_proven=false;
only unchanged strict full-grid oracle can establish pixels. No mask,tolerance,
translation,retry,style/size/input/notification-border/game/helper changes.
Default old GDI fixture route never requests the new preference.
Primary reference: [Microsoft corner preference](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-rounded-corners),
[attribute33](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute).
No Windows10 support inferred; unsupported setter fails before ShowWindow/capture.

Red new producer tests:3 unexpected-keyword errors; WGC caller missing shape
requirement also RED. After implementation:full371 mocks PASS16.444s,PS7/PS5.1
parse,diff-check and nonexecuting helper/PE/pin/fresh-output3 preflight PASS.
New negatives include setter/flush errors or positive non-S_OK -> no Show/normal
cleanup,no foreign ack,unknown/failed/drift ack,no graphics/frame after missing
setup,actual-bool requirement,default route unchanged. Setup ack before first
paint deliberately grants no pixel readiness. Independent79 focused mocks and
source/privacy/cleanup review GO; unchanged reader/helper/protocol/capture pins.
Primary accepted and admits ONE owner-started fresh
`native-wgc-fixture-owner-20261003-3` experiment only; absent at handoff.
Do not rerun attempts1/2,launch stock or infer arbitrary rounded-window support.
Actual strict pixels/changing states/visual review/cleanup remain required. A
success supports the preference hypothesis under this fixture configuration,
not exact reconstruction of old54 pixels or stock/timing/M7 PASS.
Weekly98% remaining/no reset; M70/16/M8-001 open,save/binary/SDK/version/Git
lineage unchanged. Next requires owner's foreground/run; no automatic native retry.

Final attempt3 pins: producer
`6D7372C7B5B21B637A15BD5E7A3EFE61CD376EF1D2F139F5CBD39AA31703BF18`,
probe `B3D8F694EF680C4C74DC0BD5632FDDB98857CF64083A6A4C5BA0230382EA3DA1`,
entry `A3BF20A1F9144C2ADC73710EBAF16B292FF03EC272525735C44CAEDD9C05B0F7`,
wrapper `148B6DC13212626EABBFAD0C7867D59207476488D7100B5BAA01B38FD828FF22`.
After primary fixture-only admission toggle:371 tests PASS23.613s,diff-check and
final nonexecuting helperPE/pin/fixture-true/stock-false/fresh-output3 preflight PASS.
Attempt2 result hash unchanged; HEAD201c3a6/SDKc2e6716 deliberately dirty unchanged.

## WGC attempt1 failure / aggregate diagnostic follow-up (previous handoff)

Owner ran `native-wgc-fixture-owner-20261003-1`: receipt5819B SHA
`4B1790DDA06B808B607FACDDA1BD0474D956AACEB7E8DEBED473034DF72EE35E`;
launch358B SHA `91DC5CA169634C480EE1570C27E41FB497CD6F3D4CEE52E6AE61CBAE53E662FE`.
Only these two files exist; frames empty. Independent review matched all14 recorded
inputs before intentional diagnostic edits; helper/build inputs remain unchanged.
Fixture PID21188/birth134355092250614617,HWND5117098/thread4748,physical1280x720
at320,194,DPI120. Visibility preflight foreground/unobscured. Helper PID9876/
birth134355092258435388,creation-Job/expected572FEA26...pin,normal exit0/Job0,
no force/errors,mapping closed. Fixture normal WM_CLOSE exit0/Job0/no force/errors/
retention. Game false. No new native operation was performed by the agent.

Actual error `ValueError: owned GDI full-grid readback` binds old oracle source.
All32 center samples necessarily matched ONE phase; at least one other RGB pixel
did not. Protocol validation is source-inferred from reached oracle,not separately
exported evidence. No rejected pixels retained,so phase value/mismatch region/
delta/exact crop cannot be reconstructed. Do not claim proven border/translation/
HDR/interpolation/cycle cause. `source_files_unchanged=false` was an untouched
success-path default,NOT a measured failed source check. Preserve receipt intact.

Diagnosing-bugs actual-receipt replay asserted non-FAILED and reproduced the
exact full-grid assertion; this does not reconstruct discarded pixels. Ranked
falsifiable hypotheses shared: one-pixel crop offset -> coherent translation
score; compositor boundary effect -> edge-localized mismatch; mixed repaint ->
interior/multiblock mismatch not explained by translation. Actual pixel loop is
unavailable on this invocation; next run is explicit diagnostic instrumentation,
not a declared root-cause fix or automatic repeat.

Primary added strict-oracle FixtureContentError with aggregate-only metadata:
phase32 centers,RGB-comparison pixel count/hash/bbox,max RGB channel delta,
client and grid edge bands,interior counts,nine fixed +/-1 translations.
RGB hash is B-plane+G-plane+R-plane,top-down row-major; alpha ignored. Edges are
first/last2 pixels of each client/cell,client subset of grid. Translation scores
compare SAME1278x718 interior denominator (917604 pixels) for all9 candidates;
positive dx/dy compares actual(x,y) with expected(x+dx,y+dy). No raw samples,
pixels,mask or corrected frame exported. Every original mismatch STILL FAILS;
diagnostic error falls back to exception type metadata and original oracle FAIL.
No tolerance,crop correction,content retry,game input or rejected BMP persistence.
Misleading GDI-only oracle message changed to method-neutral fixture wording.

Validated WGC metadata is copied only on THIS FixtureContentError after current
capture ownership/visibility/QPC checks,tagged capture_id/helper_attempt_index/
content_admitted=false. No stale last_receipt association on pre-reader failure.
Success-path-only source default replaced by explicit finally preservation check,
including failure paths; unknown/mutated sources force FAILED,not PASS.

Red new diagnostics tests (2 missing-diagnostic errors) preceded implementation.
Then22 fixture tests PASS: strict shifted-grid rejection/zero score correct dx,
single-pixel maxdelta/bbox,RGB-not-channel corner count,alpha independence and
interior partition,diagnostic-error fallback,no-save/no-retry/normal cleanup,
current backend association and stale pre-reader exclusion. Full361 offline
harness tests PASS in17.734s; PS7/PS5.1 wrapper parse PASS. No C++/helper rebuild.
Fresh fixed `native-wgc-fixture-owner-20261003-2` absent; exact old helper PE/pin
preflight PASS. Updated CMD wrapper explicitly calls it diagnostic attempt2.
Independent patch/privacy review GO:22 fixture tests plus independent hash/
vertical-translation/alpha arithmetic checks and two source-check failure
injections PASS. Primary accepted review and enabled ONE attempt2 diagnostic only;
physical full-grid WGC/actual cause and game capture remain unaccepted.
M70/16,M8-001 open,candidate never run,stock/cohort flags false. Live weekly98%
remaining,no reset. Next needs owner foreground/run; preserve normal save lineage.

Final attempt2 handoff: entry SHA
`A9A67742AEFEEE12273F76E9D5306C37C5B32E7D619FE4B3B00CFF775C972B04`,
probe `8749EB87F321B916AF33F05E5507EAA4EA68CFCAE6918F6700FD40DFC1B65554`,
wrapper `43B9C5FC9054CF9409015BD534AA27CE22161EF4945C5852CBB04730B99C8D97`.
Post-toggle361 tests PASS19.080s,diff-check PASS,nonexecuting exact PE/helperpin/
fresh-output2/fixture-true/stock-false preflight PASS. Failed attempt1 result hash
unchanged; no mcla.exe/helper process observed. No native launch/rebuild,game/save/
SDK/Git-index/HEAD/version/commit/push changes. Owner action remains necessary.

## Cohort3 review and window-only replacement (first WGC handoff, historical)

Owner16:01 stock A1 `windows-native-integration-20261003-3/probe-a1` was independently
reviewed for immutable result/launch/frame/log,12 recorded source inputs,9 staged
binaries,original/copy seed trees and7 observed package modules. All matched.
Stock host ABF722683E201DE241D3ABAE934BE7BC8AB592DB327E51A29CBE41CDC180EC62,
PID40416/birth134355061172942804,HWND2362172/thread38632,RTX5060(10DE/2D19).
One owned async nonactivating resize:1600x900->1280x720 physical,DPI120,stable
origin/style/PMv2/monitor. Successful-present log also records1280x720; this is
separate extent evidence,not exact GPU/native-frame equivalence.
Startup proof: exact VFS9/dev21 allowlists; nontrivial guest FRAME83,config logger,
then ONE passive FRAME ACK present90,92.3794ms. No START/A/control request.
Normal WM_CLOSE exit0/Job0,no force/errors/retention; terminal ACK drain/no unexpected
requests. Original4 seed files and copied seeds unchanged. Isolated user root adds
guest BMPs only. No normal-bundle write,interval1/gameplay/cycle/timer claim.

Primary and independent image review: guest ACK BMP contains legal text/89133
nonzero RGB pixels; saved native client BMP has all921600 RGB pixels black (all
BGRA bytes zero). Native bracket6.6169ms follows ACK; not atomic same-present proof.
Owner saw legal splash and clarified legal -> Rockstar logo -> title/Press START.
Neither early readiness nor opaque `garage-main-before` filename establishes
garage/menu identity. Accept ownership/startup/ACK/geometry/cleanup ONLY; reject
client pixel admission. Preserve original pending-review status/receipt verbatim,
do not replace it with PASS/FAILED or retry the consumed arm.

Immutable cohort3 SHA256 pins:

- result30904B: `2447ABA402938AF04AECECE3C7FF747FF4E06AF655913C7E910BCEDAB938E990`
- launch381B: `03E1C3F499159510B5CB0A12A786F9291688139B09400A72990175336AE60431`
- client BMP3686454B: `6DA67C22DD09928604DE2B8A803376C26F2439307F380F7F85290B990905265E`
- frame JSON1434B: `4690DE14B4D8CFDC5329ECEE781AB049D7A2E11A9BA74E4FC92CE5B42A488DEE`
- log12096B: `83AEAEDCED331FF66A2899692906F60A30E4C9C59055E6CEFE940FC61C0844D8`
- guest BMP: `5F55F7CEAD3ACE323517F1A745238FFBA0A847D4901529086AA227116D1038EA`

Diagnosing-bugs loop: actual BMP RGB-nonzero assertion reproduced the black failure.
Ranked predictions shared before testing: (1) GDI misses D3D12 flip content;
(2) a real black transition between independent frames;(3) readback decoder defect.
Mocked actual GDI API reader preserves channel/order/top-down layout, but720
reported scanlines can leave an all-zero buffer. Stock guard now rejects black
RGB,including alpha255 black, before any frame file; one failure is terminal.
These regressions protect rejection,not successful game capture. Flip-model
interop limitation is a source-backed leading inference,not proven temporal cause:
[Microsoft flip-model guidance](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model).
The original image remains black by design; no physical replacement repro run yet.

Replacement: C++/WinRT `CreateForWindow` for exact held HWND only,never display,
desktop,picker/fallback,focus/input. Hardware D3D11 BGRA8 one-buffer pool,first
available frame only,cursor off/default notification border. Requires exact
DWM/item/content extents,physical1280x720 client crop,unchanged owner/DPI/bounds,
fresh compositor QPC inside helper bracket. Crop-only staging texture,EVENT query,
nonblocking map and bounded pitched-row copy. No guest-present equivalence.
[HWND interop](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow),
[free-threaded pool](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframepool.createfreethreaded?view=winrt-26100),
[compositor timestamp](https://learn.microsoft.com/en-us/uwp/api/windows.graphics.capture.direct3d11captureframe.systemrelativetime?view=winrt-26100).

Python supervises each hidden helper in a separate creation-Job (game owner is
untouched),10s parent deadline/5s cleanup/retained ownership on unknown cleanup.
Strict40-int64/3686720B nonce-named anonymous mapping; no pixels read until normal
exit0/Job0. Failed protocol/content/cleanup cannot publish a frame or retry.
Fixture full-grid oracle checks every RGB pixel before persistence; WGC receipts
are explicitly named,not mislabeled BitBlt. Metadata backend/attempt records remain
available for failed-frame review. No auth/model/REX/Python environment inherited.

Initial build1 is preserved but NOT admitted: independent review found `wmain`
alone does not suppress a console under unchanged owner flags. Fresh build2 uses
`/SUBSYSTEM:WINDOWS /ENTRY:wmainCRTStartup`; entrypoint checks exact hash twice
and pure AMD64/PE32+/subsystem2 before any fixture launch. No global game flags changed.
Build2 helper158720B SHA `572FEA26D6684E1CB5B129A8F77DECE0736766C13E970D7EBC4EFCDB8C5F1AAD`;
receipt SHA `E91E46F02CD66E7F8AA119BCDD75FFC76C743A6AE1B0C228D36AD08C52548DC1`.
MSVC14.44.35207/SDK26100 `/std:c++20 /O2 /MT /W4 /WX` builds exit0; CPU1514
checks PASS. Build script freezes488 discovered inputs and verifies identical
header sets/postbuild preservation; independent review rehashed every input/output.
Receipt covers listed inputs,not an exhaustive toolchain/dynamic-loader attestation.
No WGC helper execution occurred; CPU test exe has no native Windows APIs.

Current gates:354 offline harness tests PASS (including29 WGC protocol/entry
tests and2 fixture-adapter integration mocks),PS7+PS5.1 wrapper parse,diff-check.
Independent source/privacy/cleanup/build/preflight review GO; requested additional
launch-rollback/release/window-drift/bad-mapping/mapper/recovery negatives all PASS.
Primary permits ONE no-game fixture only at fixed fresh output
`private/evidence/M8-001/native-wgc-fixture-owner-20261003-1` (absent at handoff).
13 captures/full-grid/at least3 distinct states and normal cleanup still require
actual owner foreground visibility and independent visual/integrity review.
Do not promise total runtime from per-helper deadlines; keep window unobscured
until it closes. System capture border remains enabled. No old fixture rerun.

Next: owner runs `Run WGC Harness Test.cmd` once,reports console output. On any
failure preserve evidence and stop,no auto-retry. After physical WGC acceptance,
prepare/review a fresh stock gate; title/menu and cycles require separate actual
scene evidence. Candidate48E8...never run,frozen LIVE/cohort flags false.
Live weekly2% used/98% remaining (10080min window),no reset used; stopping needs
owner foreground/run action,not quota. Dirty source/SDK/game/save lineage preserved.

Final owner-handoff pins after primary accepted review,toggled only fixture
admission and removed the unsupported total-duration promise:

- fixture entry `9805FF2F2A8A6339A05B1D6731FA96710C28A52BCC5ECD45E7283E57E463B69F`
- PS wrapper `71E6165F0272097540EE8892E9B0794DD797B4B4C56F2D3395E545846E227D49`
- root CMD `2009EA660DD9401143DE0CFDC07969CC68598BB8C3F908021C9F7FF0F0276E0B`

Post-toggle354 tests PASS in12.623s,PS7+PS5.1 final parse PASS; nonexecuting PE/
helper pin/fresh-output/fixture-true/stock-false preflight PASS. No current mcla.exe
observed. Actual HEAD201c3a63895a8431cc68c3217c3eb1da5631e93b and SDK
c2e67161f1207ddcee609507c399efe7a1981b72 unchanged,deliberately dirty.

## Reference scope and implementation

Re-read migration START-HERE and its Oct3 skills addendum; main read the selected
CppStudio control/native/readback references and xbox360-decomp Track A material.
259 CppStudio / 37 Xbox reference files rehash PASS. They remain reference-only:
no rollout/SDK patch/Track D/parallel ledger. [Adoption](M8-001-cppstudio-reference-trial.md).

Public engineering components, not a live game adapter admission:

- `scripts/mcla_win32_owner.py`: creation-time Job-list assignment, suspended
  attestation/receipt before resume, no-breakaway kill-on-close, executable
  write/delete-denying lock, held PID/birth/path identity, unique HWND and pinned
  GUI thread, scoped physical PMv2 coordinates, normal WM_CLOSE and Job accounting.
  Unknown close/cleanup retains handles and blocks new launches; forced exit is
  never normal PASS. Parent supervisor retains ownership on recovery uncertainty.
- `scripts/mcla_win32_capture.py`: owned client DC only, SRCCOPY (no CAPTUREBLT),
  deselect bitmap before GetDIBits, exact1280x720 immutable top-down BGRA/BMP,
  before/after process/window/geometry/DPI/visibility and QPC, exclusive receipts.
  No desktop DC, input or focus change. HWND-only DC/visibility does not itself
  prove initial paint completion (see failed manual run below). Failed sessions
  cannot retry; fixture content is now validated before any pixel persistence.
- `scripts/mcla_harness_io.py`: contained/exclusive regular evidence, no reparse/
  aliased inputs, exact copy/hash. Read-induced atime is excluded from mutation
  detection, while identity/size/mtime/ctime/mode/links/attributes remain checked.
  Deterministic atime and content-mutation regressions pass.
- `scripts/mcla_request_bridge.py`: frozen Requests parser, two START/six A/seven
  opaque frame slots. Fully flushed same-user-directory temp then Windows
  no-replace rename; no partial control publication. Per-request log boundary,
  discard lines begun before publication, held/named log identity, bounded full
  batches, exact INFO/input envelope, duplicate/order/consumption/output/QPC and
  deadline checks. Terminal no-unsolicited-ACK drain follows proved logger exit.
  Bare ACKs are explicit offline-only test input. Consumed128-byte tail witness
  detects typical truncation/rewrite, not adversarial full-prefix reconstruction.
- `scripts/mcla_cycle_extract.py`: pure ROI recurrence candidates or actual
  observed-landmark/QPC-neighbor bounds; complete regular samples, <.2s conservative
  gap, <=120 samples/12s, >=3 cycles. Scene/cycle verification, timing correctness
  and performance admission remain false. Recurrence is not game-clock proof.
- `scripts/mcla_capture_fixture.py` / `mcla_native_fixture_probe.py`: isolated
  no-game colored-window gate with exact palette oracle, changing pixels, source
  pins, isolated child `-I -S -B`, environment allowlist and bounded cleanup.
  Fixture safety timer exit3 cannot become parent normal-close exit0 evidence.
  [Owner-assisted launcher](../../scripts/Invoke-MclaNativeFixture.ps1) and
  [CMD](<../../Run Native Harness Test.cmd>) give a10s click/readiness window.

Independent reviews covered ownership/rollback/privacy, transport/source semantics,
cycle non-claims and no-game probe safety. Main integrated and reran gates.
159 new offline tests PASS: owner37, fixture18, capture/IO40, probe13, bridge25,
cycle26. Earlier137 count preceded the initial-paint regression work.
Spark client34 tests and ast-grep scan/three fixtures PASS. Native/physical tests
are separately listed below; mocks are not capture or game PASS.

Final primary reruns: campaign seed1 positive/13 negative and alpha matrix1/31
PASS on PowerShell7 and Windows PowerShell5.1; owner-wrapper parser PASS; diff
check clean (LF/CRLF warnings only). Frozen artifact preflight revalidated9 binaries,
four seed files, maps/helpers and interval1 candidate48E8; native remains not ready.
Owner bug-hunt hosts rehash to recorded stock2D7A/unlocked1F18 identities. No matching
MCLA/Python process remained after probes. No new accepted game artifact or save.

## Native observations and diagnosis

Private evidence directories under `private/evidence/M8-001`:

| Attempt | Observation | Closure / credit |
|---|---|---|
| `native-fixture-20261003-1` | Exact1280x720, DPI120, creation Job/birth/HWND established; immediate visibility rejected, zero frames. | Exact Job forced cleanup; FAILED, no normal-close credit. |
| `native-fixture-20261003-2` |97 observations/~2s: foreground false, occluded true; zero frames. | Normal close exit0, Job active0, no retained handles; capture FAILED. |
| `native-fixture-20261003-3` |482 observations/~10s after owner offered click: foreground true, occluded true; zero frames. | Normal close exit0/Job0; capture FAILED. |
| `native-occlusion-metadata-20261003-1` | Metadata only: one fullscreen IslandWindow, DWM HRESULT0/cloak1. No titles or other-window pixels. | Normal close exit0/Job0, sources unchanged; diagnostic only. |
| `native-fixture-20261003-4` | One independently reviewed post-fix run; own fixture later became DWM-cloaked. Zero frames. | Normal close exit0/Job0, no retained handles; capture FAILED. |
| `native-fixture-owner-20261003-124941-1fd3f7c1562747df9b6e8665dbfb6c7d` | Owner manual run; foreground true/occluded false on first observation. One saved BMP: first160x180 block correct, rest stale underlying content; palette oracle FAILED. | Normal cleanup exit0/Job0/no retained handles. Original receipt and rejected BMP preserved privately; no pixel acceptance. |
| `native-fixture-owner-20261003-140237-a111c0f22e1e4e52b50f7dc1cbcfc1ca` | Corrected owner run;13 full-grid frames,8 unique states,all6 source/exe pins and13 BMP/JSON hashes verified. | Normal WM_CLOSE exit0/Job0/no force/errors/retention. Primary integrity plus independent visual review: ACCEPT no-game fixture gate only. |

The first occlusion policy incorrectly equated WS_VISIBLE with actual display.
DWM cloak1 means application-cloaked; this hidden window is not evidence of a
displayed overlay. After a red native loop, source/API research and six direct
visibility regression cases, policy now excludes an overlapping window ONLY on
successful documented DWM cloak bits1/2/4. Own HWND must be uncloaked; unknown
HRESULT/bits and actual visible overlaps still fail closed. No class allowlist.
[Microsoft DWM contract](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute).

The diagnosing-bugs skill supplied the red-capable native/offline loops, bounded
hypotheses and preserved failed receipts. No temporary debug logging was left;
occlusion metadata is an explicit private diagnostic mode, not capture admission.

After repeated focused failures, CppStudio guidance paused blind GUI attempts for
research/owner assistance. Owner offered to click the fixture; new metadata supplied
a concrete state delta, not permission to weaken visibility. Attempt4 keeps the
remaining physical blocker explicit. No bitmap exists from attempts1–4; the later
owner manual run below produced one rejected bitmap, not native visual PASS.

### Owner manual initial-paint failure and fix

The later12:49 manual run is a new state delta: visibility passed, but
`ValueError: owned GDI palette/shape readback` immediately triggered normal
parent cleanup. That explains the brief flash; the owner did not click too late.
Although result.json has an empty accepted `frames` list, CaptureSession had
already written frame-0001.bmp/JSON before the old palette oracle failed.
BMP is3686454 bytes,1280x720 top-down, SHA256
`7E796FEBFC94171DDA2EE4D49F6C306058AEF70BC1C6ABAED169C2C91C1E433E`.
Offline replay of the exact saved BMP reproduced the exact ValueError; first
center is RGB32/64/192, remaining centers mostly61/61/61 or12/12/12. Visual
inspection confirms first block plus stale underlying content, not a full grid.
Preserve original failed evidence; do not circulate its pixels as a fixture sample.

Source cause: ShowWindow makes the owned HWND visible before initial UpdateWindow
finishes; the old paint method issued32 direct client FillRect calls and had no
producer-completion barrier. HWND/foreground/occlusion checks cannot establish
that the full client has painted. GDI/DWM completion remains a separate boundary.

Regression loop before fix:
`python -B -m unittest discover -s scripts -p test_mcla_capture_fixture.py`.
The new complete-grid publication test failed because FillRect targeted the live
paint DC. Readiness/flush tests also failed against the absent readiness contract.
Now all rectangles render to an owned offscreen bitmap, then one SRCCOPY publishes
the complete grid, with producer-thread GdiFlush and complete resource cleanup.
The exact owned HWND responds to a bounded metadata-only WM_APP readiness query
only after completed paint and producer GDI/DWM flush. Parent attests before/after,
rejects timeout/malformed/late/drift replies, and reads no pixels before readiness.
The fixture's full-grid RGB oracle ignores only BI_RGB's unused alpha byte and runs
before persistence; rejected content terminates the session without a new BMP.

Tests cover parent queries between rectangle writes, failed allocations/paint/
publication/flush/cleanup, readiness timeout/drift, zero reads/files on readiness
failure, all eight phases and corruption outside center samples.159 offline tests,
Spark34 and AST scan/3 fixtures PASS. Independent read-only review GO for one
owner-assisted verification; **no native rerun yet**, per failed-run instruction.
Double buffering is not independent proof of cross-process atomic capture: any
mixed phase must still reject rather than weaken the oracle. Readiness is fixture
synchronization, not game scene/timing admission. Sequential deadlines may reach
the20s safety timer and conservatively fail; exit3 cannot become normal PASS.
Producer flush semantics: [GdiFlush](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-gdiflush),
[DwmFlush](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmflush).
The diagnosing-bugs skill supplied exact-frame replay and producer-seam red/green
regressions; no new game/artifact/save/index/version/commit/push was performed.

### Owner successful run / scoped native acceptance

The14:02 result remains immutable `NATIVE_NO_GAME_GATE_PASS_VISUAL_REVIEW_PENDING`;
the following review is a separate primary disposition, not overwriting its status.
Result SHA256 `4A72F37FA1E10532F2B7C88DB89B3916EB1133396531C72027028149460E11A6`.
Primary verified13 BMPs/individual JSON receipts/result rows,all6 current input pins,
exact full-grid RGB,headers/dimensions/byte sizes,stable identity and fresh ordered
QPC. Independent read-only reviewer independently constructed expected grids and
visually inspected all8 distinct states; no stale/foreign content or clipping.
Primary also viewed first and phase6 frames. **ACCEPT no-game fixture pixel gate.**

Held identity: PID3992,birth134354989574282502,HWND4394936,GUI thread6788,
creation Job,1280x720 physical client/DPI120/PMv2,foreground/unobscured throughout.
13 phases:0,1,2,3,4,5,5,6,7,0,1,2,3. QPC frequency10MHz,overall bracket span
1.2104083s,individual brackets3.6402–12.1296ms. These are fixture sample brackets,
not game frame-time/performance measurements or a precise animation clock.
Ready acknowledgment1 preceded capture. Normal WM_CLOSE delivery acknowledged,
exit0,active Job processes0,no force/cleanup errors/retention. Sources unchanged.
No new probe was launched to accept these existing owner receipts.

This acceptance does not admit game readiness/scene/cycle identity,timing correctness,
performance,interval1 runtime or M7 closure. Next: integrate isolated real arm staging,
actual retained runtime logger/request bridge and reviewed native game adapter.
Live/cohort remains disabled. Live usage rechecked:1% used/99% weekly remaining;
no reset credit used,30%-remaining stop unchanged.

## Remaining bridge/live gates

No further fixture click is required: owner14:02 evidence passed the bounded
fixture gate. Preserve failed evidence and never retry captures automatically or
capture a cloaked/covered client. This is not permission to bypass readiness on
other applications; the game adapter needs its own readiness/scene review.

Exact real arm staging is now prepared/reviewed. Integrate retained-runtime logger behavior,
all explicit roots/TU5/GPU configuration and optional collector normal closure.
Fresh bin must have no mcla.toml: current SDK TOML overrides earlier CLI/environment.
Old mock argv has no actual game root; it is not directly runnable as live code.
Use ordinary canonical `private/game`, not bug-hunt bundle hardlinked payload.
Verify RTX5060 adapter identity again; default selection is not high-performance
proof. Each arm has its own four-file seed and empty cache; never carry arm progress.

Find a physically observable same animation and establish its clock/source relevance
for title/parked/menu before any cycle flags are admitted. ACK/publication growth is
not scene or normal-speed proof. Full A/B/A and timing interpretation remain pending;
frozen LIVE/staging flags remain disabled. Stock is fallback; B256 remains rejected.
[Original exact artifacts and contracts](M8-001-stock-interval1-20260928.md).

Same-thread HWND destruction/reuse and adversarial filesystem races are not fully
tracked; this is a trusted serialized owned-test harness, not a security sandbox.

## Real arm preparation and first stock startup

Two fresh non-executing sets were staged under
`private/evidence/M8-001/windows-native-integration-20261003-{1,2}`. Each plan has
SHA256 `CC0701482AFDCACAF61F710F64643CED66223CE0B4623B9C9AC0984D5659E578`.
The separately reviewed public staging API is admitted; frozen private staging/
LIVE flags are unchanged. Independent reviewers verified53 pins per set:27 staged
binaries,12 seed files and14 preserved sources. A1=A2; only B's host differs to
48E8. Four distinct source/A1/B/A2 filesystem identities per seed,all links1;
15 empty cache/metadata/update/diagnostics/tmp roots,no TOML. Successful isolation
receipts publish only after source/root/final real-plan checks. Failed partial
preparation never receives a successful receipt and is never reseeded/adopted.

The non-executing launch contract preserves frozen stock controls, explicitly
mirrors all game/user/cache/metadata/update/diagnostics/log roots into CLI and REX
environment, selects freshly enumerated RTX5060 raw DXGI index1,info input logger,
100MiB/one-file rotation cap and contained TEMP/TMP. Auth/Python/REX overrides are
not inherited. TOML is absent. Loaded-module checks use the created held process
handle,require host/runtime/GPU and pin each observed package-name module at its
exact bin path. Lazy modules stay unobserved; OS/driver paths are inventory,not
hash proof. Two equal snapshots are not atomic mapped-memory integrity; Microsoft
documents [enumeration loader races](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-enumprocessmodulesex)
and [module-name truncation](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getmodulefilenameexw).

Live-log constructor regression reproduced `file changed while hashing` on a
legitimate append. Replaced immutable hashing with held/named regular unaliased
identity,initial-size/cursor no-shrink checks and existing consumed-tail witness.
Independent review caught an initial lost shrink guard;31 bridge tests now pass,
including append-positive,alias/shrink/replacement negatives and reader cleanup.
This is trusted append-writer protection,not adversarial full-prefix immutability.

Exactly one real stock A1 was launched at14:50 (cohort1). PID37392,
birth134355018046112612,host ABF. Actual log proves explicit isolated roots and
D3D12 initialization on RTX5060/vendor10DE/device2D19. It stopped before readiness,
module inventory,ACK or client capture: `RuntimeError: window does not match bound HWND`.
The second title-binding operation re-enumerated another same-process top-level
HWND. Its real-owner offline regression was red,then40 owner tests and independent
review accepted a bound-only title-tightening branch. Existing HWND/thread/PID/
liveness checks remain; no loosening,rediscovery or implicit rebind. The14:02
fixture gate remains accepted for its recorded source version; this source delta
has not been physically re-probed.

Failed stock run closed its attested window normally:exit0,Job active0,no force,
cleanup errors or retained handles. Source pins,game XEX,binaries and source/copied
seed unchanged. Original FAILED receipt is preserved:

- result `9D20BFD53A281DE20F6CB748751E76B6B42096DDFA66A4430741B3194EEFC087`
- launch `3E0FB004B81D94C22AA8D6E0043B879A96AA772EE7B2044BEEA926A5A29892DE`
- log `732BDF5519D5C0D844C5E38AA3AF4601F6ECDB5DDB5297E2E3F0768F8F403574`

No automatic native repeat. Replay also found stock's intentional VFS negative
checks were falsely rejected as generic errors. Frozen host1439–1483 and actual
log49–57 prove a nine-record ordered self-test (4info/2warning/3error). The parser
recognizes only exact category/severity/payload/order/same module-validation
thread before first frame/config; incomplete proof is not-ready,no error credit.
Other errors/crash/unimplemented/failure markers still reject. Actual replay now
returns not-ready,missing frame/config,with exactly expected error lines51/52/55.
Native stock caller additionally requires complete VFS proof and all3 expected
rejections. Independent45 parser/probe tests and exact frozen host/SDK source
review accepted this exception for owner handoff. This changes no game protection
or error severity; old native result is still FAILED.

Latest gates: **274 current harness tests PASS**,Spark34 PASS,PS7/PS5.1 wrapper
parser PASS,Git diff-check PASS. Test command:
`python -E -s -B -m unittest discover -s scripts -p 'test_mcla_*.py'`.
Ownership40/staging42/launch9/modules10/readiness25/stock-probe20/bridge31 are
within274,not extra totals. Earlier159 counts remain historical checkpoints.

At the pre-cohort2 checkpoint,it was staged,not launched. Owner action then:
`Run Stock Harness Startup Test.cmd` once. This is REAL STOCK MCLA,not colored
fixture; close any existing game first,click only the owned game client if needed,
leave foreground/unobscured and do not use game keys/controller. Startup bound90s,
one passive frame request/ACK (<=1s counter bracket),one owned-client capture,
normal close30s; failure stops and retains evidence. No START/A/keyboard injection,
desktop screenshot or normal-save writes. PS wrapper uses Python-E/-s/-B and
checks for an existing mcla.exe without touching it. Exclusive output/fresh arm
checks prevent repeat/reuse. Run-level mocks cover pre-bind/parser/capture/close/
preservation failures and cleanup ordering,not physical PASS.

Owner-run physical capture/integrity review remains next. Interval1 never ran;
full scene/cycle/cohort/timer/performance gates and M7 remain pending. Used
diagnosing-bugs red/green+actual-log replay and START-HERE reference-only control
guidance. Normal bundle/SDK/save lineage/index/version/commit/push unchanged.

## Second owner stock failure / exact developer-probe diagnosis

Owner cohort2 A1 at15:26 showed splash and then closed. Preserve its original
FAILED receipt; no automatic repeat or native launch was made to diagnose it.
PID26548,birth134355039767031002,ABF host: early held-window binding and bridge
construction succeeded. Readiness rejected `startup failure marker` on log78,
then the harness delivered WM_CLOSE normally:exit0,Job active0,no force/errors/
retention. No frame request,ACK,capture,START/A or candidate launch occurred.
Source/game/binary/source-seed checks and the copied four-file seed remained
unchanged in that receipt. This is harness-triggered shutdown,not crash evidence.

Immutable identities:

- result `3D376BF6FBC11059A4B2DAAADDB93BBDA8F1FDF49B35F158B6072585D749F615`
- launch `72FA7A8A6D52EAB4F7FBBBF0191E7BC8055CFFED8A9474FF4F61F463DBE8321E`
- log `78896CE6E970C0A97261A9944C3EF7E90452DACF8F030B4610C0E87A5571B9B5`

The red-capable original-log parser replay raised at78. Ranked hypotheses were
optional developer assets,missing required content and broken mount. Exact
source/log/history distinguishes the first: lines77..97 have seven triples,
five `t:\mc4\art\city\test_dt_railyard.loc` then two
`t:\mc4\art\city\test_sc_exposition_park.loc`. Each is warning/fs no-device,
error/fs ResolvePath parent missing,warning/krnl NtCreateFile `0xc000000f`.
One guest thread29720 differs from module-validation25260; complete module/VFS
proof precedes it. SDK virtual_file_system.cpp147..151,253..258 and
xboxkrnl_io.cpp152..177 explain the exact log/status propagation. Historical
`scripts/verify-world-streaming-smoke.ps1:186..198` already required the same5+2 misses.
No complete generated guest-callsite derivation is claimed.

Same retained ABF/AEE historical logs continue beyond all seven misses to a
nontrivial frame,gameplay config and normal close:
`stock-tu5-backend-ab-20260920/return-a1` (log79..99,frame117,config118),log SHA
`0DF779A2D5C6CD1BD826530088F437121B1D61B57BEA89B701CCAD90E59A03D0`;
`stock-gpu-clock-ab-20260920/return-a1-v3/arms-v3/a1`
(log82..102,frame123,config124),log SHA
`6748BB04DA995D0B880C4D646C6F9246C8931066A41DB70FA13A5DD226D7C37B`.
Those are Deck logs,not RTX native admission. Original foreign GPU replay rejects;
a one-payload in-memory adapter substitution exercises only classifier behavior,
still not-ready for missing GARAGE config. Files are not rewritten.

Regression first rejected the exact21-record sequence; narrow parser correction
then passes34 pure tests,including all26 prior cases. Only complete exact ordered
payload/severity/category/status/path/count/thread/context proof earns seven
canonical miss records. Partial proof is not-ready and earns no accepted misses;
other FAILED/errors/crash/device-loss/unimplemented markers still reject.
Actual cohort2 replay is now not-ready,missing frame/config,with miss lines
79/82/85/88/91/94/97; no retroactive native PASS. Actual stock caller now separately
requires complete developer proof and exactly7 misses before publishing a frame;
five call-site negative subcases were red,then21 probe tests pass. Parser/source
exception independently reviewed GO; native retry remains pending.

Separate next-gate risk: retained log shows guest1280x720 but host swapchain
1600x900,consistent with Windows125% scaling. SDK window controls are logical
pixels (window.cpp24..31,window_sdl.cpp130..134,224..230); strict capture requires
physical1280x720. Do not reduce CLI window to1024x576: HasNonDefaultValue compares
default values (cvar.cpp468..475),so xboxkrnl_video.cpp42..73 can also reduce guest
mode despite explicit default1280/720 flags. Keep frozen controls; review a
separate one-shot physical resize of the already held owned HWND,no activation,
position/style/awareness changes,or input. Target/caller DPI distinction and
metadata-only bounded convergence must precede any frame request.
Microsoft primary references: [DPI-aware outer sizing](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-adjustwindowrectexfordpi),
[position/async/no-activation flags](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos),
[physical rectangle virtualization](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowrect).
Physical extent is not proof of resized GPU presentation or performance. Current
dirty SDK source is explanatory; no exact retained-binary internals claim.

Used diagnosing-bugs original-log replay,source/historical discrimination and
red/green regression. No game/save/SDK/binary/index/version/commit/push changes.
Both failed native attempts remain preserved; new source pins do not relabel
their recorded source versions. Source gate held disabled during correction.

### Corrected physical preparation / fresh owner handoff

`mcla_owned_geometry.py` uses a separate one-shot permit,consumed even on failure/
no-op. Exact owned process/Job/birth/HWND/thread metadata brackets every sample.
Requires visible nonminimized/nonmaximized standard framed top-level PMv2 target,
no menu/scrollbar/unsupported style; unchanged DPI/monitor/client and outer origin.
Already1280x720 is a no-op. Otherwise computes outer extent with actual style/DPI,
requires client and entire target outer frame fit the same monitor,then exactly
one async SetWindowPos with flags0x4216 (NOMOVE/NOZORDER/NOACTIVATE/NOOWNERZORDER/
ASYNCWINDOWPOS). Metadata-only convergence bounded5s,no correction retry or pixels.
Caller PMv2 context is restored; target awareness/styles and frozen flags are not
changed. Failed geometry cannot reach readiness/frame and still normally closes
the held window. Current SDL_windowsvideo.c580..589 defaults to PMv2,no proof of
retained actual target yet; a different actual awareness fails before resize.

Geometry1600x900 regression was red before fix,then31 mocks green. Integrated
ordering/source-pin/failure tests were red,then23 probe cases green. Primary
reviewed helper/API signatures/all31 tests and full parser/call-site source.
Current full harness317 PASS,Spark34 PASS,PS7+PS5.1 wrapper parse PASS,diff-check
PASS; all tests use injected native backends or TEMP,not a native game rerun.
Independent source/privacy/cleanup review reran88 geometry/probe/parser cases GO.
Helper SHA `329016ABD6C5FE768D53FB2A0A13CCDF0A588C38844FC14FC1D8A7973BE62287`;
parser SHA `212CD82E2286AA5658FCFC080D8AC8E53EBA935D377DE4F2361C05EF788D7427`.
If geometry fails after async submission,receipt has attempted/startup-window/error
but not every postrequest detail; assume it may have queued,never claim no resize
or resized presentation. Cleanup must still prove normal exit/Job0.

Fresh `windows-native-integration-20261003-3` A1/B/A2 preparation passed independent
53-pin verification,exact planCC070...,four distinct identities/link1 for each seed,
15 empty auxiliary roots,no TOML/log. All3 receipts staged-not-launched,native false.
Old cohorts1/2 remain immutable; no reuse/reseed. Primary accepted final independent
review and repinned the stock probe to cohort3,admitting exactly one owner-started
A1. Frozen LIVE/cohort flags remain false. No automatic game launch performed.
Final probe SHA `0559326AE268661585BA8AE0609B38A34F8D86F7539EFA71D4B17A626017BB61`;
wrapper SHA `4D669309EE29ED5015622C3EB676A775BE7F5797F4FBE2B1D1AF1CE20016CF73`.
Post-repin317 tests PASS; primary non-executing preflight confirms cohort3/A1,
nine binaries/four seed files and absent probe output. No current mcla.exe process
was observed. Latest live weekly allowance98% remaining; stop30%,no reset used.
No native ACK/capture/scene/cycle/timer/performance/interval1 or M7 credit.
Owner starts `Run Stock Harness Startup Test.cmd` once,keeps game unobscured,
clicks only its window if needed,and does not use game keys/controller. At most
90s startup,one passive frame,normal close. Report failure; do not rerun.

## Spark routine work (continued)

Auto-review initially refused uploading this new module; no request was sent then.
Owner explicitly approved repository source transfer to their Spark in this chat.
No bearer/third-party credentials were put in the packet. Short alias
`qwen3.8-27b`, non-thinking, max1024 tokens/1200s: one request completed stop in
161.453s. Private job `spark-bridge-cases-20261003-qwen`; primary verified five
artifact hashes/source hash and four exact guard substrings, literal bearer absent.

Partial acceptance only: four extracted guards guided bridge negatives. Packet
incorrectly requested five public methods while excluding underscores; only four
existed. Returned constructor as fifth is not that compliance. Explanation also
misstated QPC units: the guard is one second for any valid frequency, not only1.
Primary corrected/rejected those portions. Live source evolved after the packet;
receipt binds the supplied version. No independent approval, game evidence or
model-quality claim. [Workflow](../dgx-spark-worker.md).

Latest weekly allowance observed98% remaining; no reset used, owner stop30%.
Task/milestone counts and save/binary lineage are unchanged. Fresh owner-started
stock A1 gate is next; no automatic retry or cohort starts merely from fixture
acceptance. Stopping here requires the owner's real visibility/run action, not
the usage threshold. Full M7 review/closure/commit/push remains pending.
