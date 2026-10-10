# M7-016: caller-specific host yield — no cadence gain

Date: 2026-09-07. M7 remains0/16; stable stock Deck performance is open.

## Decision

Do not promote this candidate or repeat the same three-arm experiment. A narrow
host-yield insertion passes semantic, build, binary, native and exact-Proton
runtime gates, but the clean Deck windows show no positive cadence signal.
Normal bundle/shortcut/current.txt and saves remain unchanged.

| Metric | Baseline A | Yield candidate | Baseline C |
|---|---:|---:|---:|
|Unpaused guest outputs/s|19.2500|19.0500|19.2000|
|Unpaused draws/output|3306.844|3268.499|3283.878|
|Paused guest outputs/s|15.0500|14.7500|14.9000|
|Paused draws/output|5294.920|5399.322|5222.631|

Unpaused candidate cadence is1.039%/0.781% below the bracketing controls;
baseline drift is−0.260%. Paused cadence is1.993%/1.007% lower, with baseline
drift−0.997%. Work varies, especially in pause. These results reject a claimed
speedup in the observed windows; they do not prove a universal regression.
Guest-output publication is not a direct display-FPS or simulation-update count.

## Change and prerequisite proof

[Caller profiling](M7-016-watchdog-scheduling.md) resolves325 exact PAUSE samples
through sub_82419E98 → sub_82411E98 → sub_82412F98. The
[32-case witness](M7-016-watchdog-yield-witness.md) passes native and exact Proton
with matching context, memory and traces, including abort/exception paths.

Private evidence I=`private/evidence/M7-016/watchdog-caller-yield-candidate-20260907`.
A private full TU27 copy adds exactly52 bytes: the SDK thread-header include and
one `rex::thread::MaybeYield()` call after nonzero helper return, before the fresh
predicate label. Removing both additions restores the original source exactly.
The original generated tree, SDK and other source files are untouched.

The deferred private CMake hook first validates the retained topology and applies
the existing63 scope options, then replaces exactly one source while preserving
order/count/ownership/options and TU0 strict hardware-FMA flags. The effective
target retains72 sources. All78 CMake and72 Ninja commands match E54 except the
selected source/object and its derived dependency paths. Link options are equal.
Controller review also binds E54's accepted generation receipts, constrains
output paths, and preserves a separate child-reap attempt after cleanup errors.

- Private TU27: `96FFC579C230A0492C117E1FB78976D3501D327CFD6698BDAA48545D3D139435`
- Hook: `C6DC58AFC856BBB1CC13AFFDDEA166F7447267B405CB1759934E6B9CAD078D52`
- Controller: `87396D28E79EDFF8F0A6E9F4FBD34B21B883E3C8AABE2124ACF1F016EF65B9D6`
- Frozen inputs: `557C25EA732F831C69688BEC24AE81B3B9CBF80AD6EF61C27924354642AD08AD`
- Command review: `D3776EBC9F3940E9D63E3095D5A758AA66F42E8259D8386BB9BD66DF7F03BED2`

## Build and linked-code acceptance

Build exec70231 completes77 steps in156.156s, exit0/no forced cleanup or stderr.
All2,759 frozen inputs/898,349,688bytes and seven configured generation files
remain unchanged. Root verifies71 other host objects after excluding only COFF
timestamp bytes4–7. Independent relocation-aware TU27 comparison finds559 of560
function spans identical; only the target function changes. No function
membership change occurs across its188 sections.

Target range[1293380,12937F8) contains the helper-zero branch at1293788, which
skips yield. Call129378A reaches the real runtime function; jump129378F returns
to fresh reads at1293690. Initial entry reaches those reads directly. The shared
helper and target/helper unwind contents are unchanged. All29,788 ordinary/
original aliases and30,034 binary mapping entries verify. Generated entry RVAs
stay fixed;246 imported guest entries move16 bytes with their thunks.

Imports increase665→666, solely `rexruntime.dll!?MaybeYield@thread@rex@@YAXXZ`.
Host thunk297C7E0 uses IAT29A7CB0. Exact runtime2953 exports the function at
[B6FB0,B6FC2), calling SwitchToThread followed by MFENCE and ABI-preserving return.
This is the actual runtime export, separately verified from the witness's copy.

- Host: `7F46068987F1EA7FBE345DCF733EE96C59429B9C90808EBB09E831F184EE74DB`,45,299,712bytes
- Map: `721A1FCE4119DF3505BB254BD2E16CC6DD463DF8C23036B8E44CEC23020809F5`
- Other-object review: `9999EE35F2B2BE453A486E957567157CFF9491F07EDDD9B19E207ECB0E574907`
- Package: `F610DE71F85925CAD8E6F26EB7A2F9674C8D88DE40BE4CCD97805D8572AF6628`

The nine-file package changes only the host. Eight companions are retained from
E54, including the previously tested crash helper; newly relinked auxiliary
executables are not substituted.

## Runtime controls and returned evidence

Native exec59367/PID35932 completes normally in173.7749988s. Seven loaded images,
source/probe full seed, six frame hashes and two counter pairs verify. Root
inspects five gameplay/unpaused/pause frames; the idle camera changes by the
after-window frame as in the earlier native control. The native595/20.000110s
and600/20.000089s counts are not a Deck performance comparison.

The reviewed Deck helper runs exact E54/yield/E54 under the same Proton10,
full four-file seed/cache, settings and20-second neutral unpaused/pause windows.
Each arm has a fresh isolated prefix. No profiler, clock, affinity, sync or
coherency policy is varied. Only the host and small helpers transfer; companions
are copied locally on Deck outside the entire game sync root.

| Arm | Exec | Title PID / start | Controller elapsed | Exit |
|---|---:|---|---:|---:|
|A baseline|72625|150986 /167014783|178.0401s|0|
|B yield|43307|151364 /167038812|178.0450s|0|
|C baseline|90258|151714 /167063800|178.3138s|0|

All are closed without forced cleanup, survivors or source drift. Their shared
comparison fingerprint is42994E27… . Twelve selected images, complete seed,
phase/frame/counter linkage and the exact package/runtime inputs verify.

All84 returned files/88,996,272bytes are hash-checked and safely extracted. Root
inspects15 phase frames: the unpaused endpoints retain rear view/HUD0mph, with
traffic, pedestrians, lights and scene detail varying. This does not establish
equal work or a fixed scene for every moment. Primary source review and raw
counter reproduction match the delegated analysis; independent source/build/
package/runner/native reviews pass.

- Native result: `54ECCF12E3856C1601018D00142A19B1C4036BFC25AF54212AC908B73C4B3BD2`
- Native verification: `69CF4C6146A49948A4DCFF364F3F24DA03CC2FFBE18D5D02B6581B853C93C7AD`
- Deck helper: `4BABB4294E03D8CDA2CEBF0F138DF986F62DD35A8152641A63D9B4A4D582331D`
- Collector: `92195C0F327C33AFF0E75BA1CC4851CA3566FD20C9D861A7FE0703DA04F76AFD`
- Archive: `7B75FD423949DE6BFC68DB673291FF0D7185F03859C4E71ED8B9D2B390BF6823`
- Inventory: `B7A0844AC041D1FBE95F4E7934109242510A022EC564AA6FA62331990D991866`
- Analyzer: `7FACBEBE827552AA152D15E64E4E43C7A24AA11C83D0A6DC11D7ED0244DC7C95`
- Analysis: `23C42BB14B6395AFE97572D0B9DB94BAD3D16E9E3C76D5D7DB6FB9B15B5CAC2A`
- Verification: `36D6A8AD58D9274EB5F756C14B22AF109A0AC905DC749A1A679B5A417B0D6A29`

## Next investigation

The baseline counters report about34.626ms in draw-packet processing per output
over3306.8 draws, versus51.913ms in primary-buffer processing. Binding update is
5.273ms, readback copy3.441ms, watch arm6.180ms, disarm2.933ms and non-MMIO write
fault4.099ms. These timers overlap and are not additive exclusive CPU budgets.
Read-only review will first use existing GPU-stack evidence and IssueDraw source
to identify which draw stages need finer attribution. No new optimization is
selected solely from these totals.

No stable30, owner driving/audio PASS, save progression, normal promotion,
version bump, task closure, commit or push is claimed.
