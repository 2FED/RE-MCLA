# M7-016: parked H100 CPU leaf profile

The clean-environment retry produced an independently accepted3303-sample
userspace profile. The main leads are a shared guest wait and GPU-thread memory
copies; callers remain unproven. The first attempt failed before recording due
to Steam library contamination and is preserved separately below. No clean FPS
or physical acceptance is claimed for this profiling run.

The [optional setter control](M7-016-pacing-setter-cvar.md) passed full evidence
review with17.05–21.82% parked cadence improvement. A separate single-arm run
profiles remaining CPU work using the same ABF host/U8 GPU/AEE runtime,
full copied profile/cache,1x/pre-masktrue, HUD/MangoOFF and flagtrue.

Only h100-profile is admitted. The existing automatic capture starts2–3seconds
after observed unpaused BEGIN, records12seconds of99Hz task-clock:u leaf samples,
then renders text/header through three normally reaped children. No stacks,
registers, kernel sampling, inheritance or physical movement attestation is added.
Before/after thread births/maps and COMM/FORK/EXIT metadata define actual coverage;
newly created threads are not presumed captured. SC_CLK_TCK is read and recorded.
Two outer120-byte observations require H100/I2/rate60/override0, but do not establish
continuous H during capture. FPS under profiling is not a clean comparison.

Private root: private/evidence/M7-016/pacing-setter-profile-20260909.
Runner32A835ED7FC2801DEDD2374151C50326E7A085DD48A6EAFD48A0CDDEFC492832;
supervisor13D2E55D440F5C6629D1BFDBA4B45D29F5181B2F43F620A1AD44F8BCB6F877E4;
plan34433D4FC1D2A7FC0C18EFD04C458FE560FFCA85078563E42AFCAFDEEAE5A755;
mappingCC53BE241B133A2ACECAF7F1EF4B7FFC34382A825B93AB0AD60F286ADDAB5E73.
Root reviewed full runner/supervisor diff; independent source/map review PASS.
Three changed functions/36 unchanged function ASTs; original capture algorithm
retained with tick evidence and explicit parked/noncontinuous/clean-FPS-false scope.

Actual stage67files/82063917B, archive
3484D1E06E2A0B04C94A988073ADB35119CB6BC67EE88EE71029A3C146A24DF2,
manifestE8DD4C3FF9C9F10C9F6A7BE4D456AAD00E2F351FA31F84D79CA7BC3640432998.
Strict archive/member/hash verification and fresh manual unpack passed on Deck.
Remote /home/deck/MCLA-Diagnostics-20260905/pacing-setter-profile-20260909/title-profile.
Current SDK launcherFC0AE69F9833B678B5322CEAD576607666694A151FFD33B733F5DD104C1C03E3
preserves prior launcher0BDF in previous-launcher.sh.
Runner215185/wrapper215208 started through official SDK, first H100 snapshot passed.
Do not relaunch this active root. Wait for sequence-result and normal Mango restore,
then return/verify/analyze. M7-016, stable30 and physical acceptance remain open.

## Failed first attempt

The record child failed with Steam pinned_libs_64/libcurl.so.4 missing
CURL_OPENSSL_4 required by system libdebuginfod. The inherited LD_PRELOAD also
emitted the32-bit overlay warning. The original perf_env copied game environment;
that was incompatible with native system perf inside the SDK session.
The controller marked capture failed and force-cleaned only its owned title
PID215390/start186748604 at119.879s. No owned survivors or cleanup errors;
Mango restored. Do not classify this as normal title acceptance or valid sampling.

Returned104payloads99366831B verified. Archive
D447530BD25424BD135D06741517004EC15BB67BCF0C66269FCA24DBA0BF2C9C;
manifestDE7069B098D7E4F9039D8C7C9EAEF0E221F13557B783793C069A85853C36E077.
Independent failure audit PASS104payloads/full identities and rejected capture.
Perf child215632 exited1 after0.133s; no profile.data. CPU sidecar was terminated
with partial data/no COMPLETE footer, not accepted as a full telemetry run. This root is CLOSED and must not be rerun.
Fresh pacing-setter-profile-cleanenv-20260909 authoring changes only profiler-child
environment to a small explicit system-tool environment, retaining game environment
and all capture/cleanup gates. Root will smoke-test perf loading before a title.


## Clean profiler-environment retry

Fresh private root pacing-setter-profile-cleanenv-20260909.
Runner6CD390316993E28D3AFB22CDB18D18A6810A082D1EB04195097E8C99D2DF5398;
supervisor20A17B0DB101CD64CCED9471136BBF6FC86FCDCAE222B01BA8F1D602F87B6A70.
Independent exact source/map review PASS. Game environment and capture/cleanup
guards remain unchanged. Only perf children receive an explicit eight-key
environment: system PATH, verified Deck HOME, C locale, three /dev/null perf
configuration settings and empty DEBUGINFOD_URLS. Loader paths/preloads are absent.
The same dictionary is included in identity/profile receipts.

Root AST-extracted the actual literal for a guest-free perf --version smoke:
PASS, perf6.15-3, stderr empty,0.026s. Receipt
73127D50C42EC4EA6F6FD19D6F474D03710271315FEA34B5259AA483265DAC21.
This proves loading only, not attachment permission. Read-only kernel values were
perf_event_paranoid2 and ptrace_scope1; no system settings changed.

Actual stage67files82062463B, archive
E64F562CC790DA96DFBE13A9DC9ACA5F0476C8B114B19290DA945C917D18A3B7;
manifest588DADE17A6A2B044D65F9875E5063B2D634420B158FB936F7D0C39D166D1FB6.
Fresh remote .../pacing-setter-profile-cleanenv-20260909/title-profile verified.
SDK launcherAE3B7E7B8FA7344BBD34A3344066DE1B409C65881599D53AF28DBCF224AFB139
preserves prior failed-profile launcher. Runner215917/wrapper215926 active;
first H100/I2/rate60/override0 observation passed. Recording/closure still pending.


## Accepted clean-environment profile

Retry completed normally: title216116/start186843831,178.89975157s, exit0,
no force/survivors/cleanup errors and Mango restored. Result
E26226D55EB571750254ABFB99334670FEC20E85B142DE854B41664C6E064926;
profile receipt16C03093A065FFB75F1DD091FD6DFB7DED5AF23879F685FAA1028C2577DC5552;
rawC8ADC9C0DFF318D1715DEE255E7A503AE4AE39A3D26068F2504E9DA0C2650059.

Archive15E8E8695EFC59665CB09FFA0AE1C9B24B309AE76A99D6DE478E3A261D0211D9;
manifest9CFAFFDDAAD84F7F152F47F2BFDE040EB235F7104DBFCA64F6FA467CF5C8A62E.
Root strict extraction and independent full audit passed130payloads/116791809B,
67staged identities, nine profile payloads plus receipt, three normal profiler
commands, raw/text/header agreement, two120B H100 checks, two13B GPU checks,
24 phase mappings,94 counters, six frames, full profile,420CPU samples and focus.

3303 samples span11.99711168s; recording wall time12.111786061s.
No lost/throttled records. All20 sampled TIDs retain their births across68-thread
snapshots; no observed additions/departures/reuse. Full maps change while module
identities stay stable. CLK_TCK100; CPU bracket12.384205432s.
Root replay summaryC111A2C2E66707947BAFF886C4E9C4B80414A038C22FE4F473C885424A23C402.

| Recorded thread / TID | User seconds | Kernel seconds | One-CPU share |
|---|---:|---:|---:|
| XThread0220 /216183 |10.27|0.95|90.60%|
| GPU Commands /216146 |9.68|1.28|88.50%|
| Main XThread /216160 |4.03|0.10|33.35%|
| XThread0240 /216191 |3.38|0.17|28.67%|

These are short-bracket shares of one logical CPU, not machine utilization or
exclusive frame budgets. The record under profiling observes25.499994/21.849996
outputs/s; no clean FPS claim. Endpoint images show idle-camera rotation from rear
to front three-quarter view. Accept neutral parked input, not identical camera/
scene, physical driving/audio, continuous H or exact cross-clock containment.

## Exact leaf attribution and next constraint

Independent raw/IP/MMAP2/executable-map and current PE/pdata/MAP review found:

| Function | Samples |
|---|---:|
| Guest82412F98 shared watchdog/wait |481|
| Official CRT memmove |167|
| TimerQueue::TimerThreadMain |99|
| Guest body82169D38 |65|
| UCRT fma/fmal alias body |64|
| Guest823F4450 |55|
| VulkanCommandProcessor::UpdateBindings |45|
| Guest82462BD0 |41|

Every sample has period10101010ns; summed sample period is residency weight,
not exclusive wall time, invocation count or recoverable frame cost.
Module totals: host1908/GPU368/runtime264/RADV185/vcruntime176/Unixntdll107/
Windowsntdll91/UCRT81/remaining123. Unavailable ELF internals remain path-only.
191 of2808 inspected PE leaves have no exact named assignment.

GPU216146 is independently selected by12ExecutePacket,9Type0 and2DRAW_INDX
leaves; the second similarly named216155 thread has zero ticks/samples.
165memmove leaves occur on that GPU thread (17.80% of its samples), but this
leaf-only record establishes neither callers nor copy sizes.
All481 shared-wait leaves occur on216183;396 hitPAUSE and68 the following
comparison in current range129A6B0–129AB7B. Caller/predicate remains unknown.

Root cross-checked source/MAP anchors. Earlier caller-specific yielding showed
no cadence gain, generic copy replacements had no broad gain, and cached readback
was already preferred. Do not repeat those changes merely because their leaves
remain visible. Next bounded investigation derives current first-hop caller/
size evidence for the wait and copies, preferably with a small proven stack
budget in a separate controlled capture. No new optimization is selected yet.
