# M7-016: shared watchdog scheduling investigation

Date: 2026-09-07. M7 remains 0/16; no scheduling change is implemented.

## Scope and semantic limits

The current [published-scope profile](M7-016-published-scope-profile.md) has612
leaves in `sub_82412F98`, including499 PAUSE instructions. This is active waiting,
not proof that the whole sampled CPU budget can be recovered or that a wait can
be removed. Six callers own different completion predicates and stack-local
initializer/finalizer state.

| Caller | Guest call PC | Continuing condition |
|---|---:|---|
|sub_82411180|824111D4|Tagged/aligned position incomplete|
|sub_82411218|82411278|Wrapped cursor interval incomplete|
|sub_82411E98|82411F2C|Modular completion sequence incomplete|
|sub_82419E98|8241A41C|Queue delta at least15|
|sub_8242EEA0|8242F000|Selected slot word differs|
|sub_8242F0A0|8242F0EC|Selected slot word differs|

The helper performs32 PAUSEs, then abort/progress checks, thread-specific refresh
and a watchdog callback/trap path. Preserve both progress reads, all register and
memory effects, callbacks and returns. The threshold5000 reads KTHREAD+0x58,
declared `unk_58`. One SDK comment calls it kernel time, but no named writer/unit
was established. Initial zeroing and a direct-reference census do not exclude
alias writes/restoration. Do not reinterpret the threshold as milliseconds.

A plain host yield after the existing32 PAUSEs and before the first fresh load is
supportable only as a private experiment. Existing Windows `MaybeYield` calls
`SwitchToThread` and `MemoryBarrier`; it may return without another thread running.
No timer, Sleep delay, adaptive counter, TLS-lifetime assumption or wait bypass is
selected. A differential exact-helper witness and actual-runtime controls are
still required before any conclusion about behavior or performance.

## Exact first-hop stack proof

The E54 host and map remain the exact [candidate](M7-016-guest-scope-candidate.md).
The helper's own `.pdata` range is `[12990B0,129957B)`, unwind RVA29F6140,
version1/flags0/no frame register,15-byte prologue. Five pushes plus48-byte local
allocation make88 bytes. At its eight PAUSE RVAs129916A/16C/16E/170/172/174/176/178,
the first return occupies captured stack bytes `[0x58,0x60)`; the next RSP is
sampled RSP+0x60. Require at least96 actual valid stack bytes.

Exact first-return RVAs128EAF7,128EF00,129375E,12C3056,133720E,1337777 follow
decoded E8 calls from the six callers above. No intermediate BodyTag wrapper is
needed for these first hops. Only the eight proven instruction states permit
this fixed-offset interpretation. Bind sampled IP and return−1 to stable
executable mappings and the exact load base; report unknown states/returns.

- Helper body: `85083D7283A095B374D0E32255E2BACEE44CC557980E3879D4DB4E16402EE1DB`
- Unwind header/codes: `F12E01B6206FC21FEB6A165255DE94DF80A2DB8DE8C9AE9D28B1FAC2F1F30C53`

This is a bounded first hop, not automatic DWARF or a complete-stack guarantee.

## First capture: failed deadline, retained

H=`private/evidence/M7-016/watchdog-caller-profile-20260907`.
The reviewed3s scout/8s stack protocol selects the exact helper range in E54,
requiring a unique thread with at least two hits, stable birth/executable maps and
matching XThread names. It uses the existing exact Proton10, current package,
full seed and neutral20s unpaused window. No new game binary is transferred.

Exec31165 fails before starting the stack recorder. Scout raw/text/header654
samples verify150 helper hits on TID148866/start166476050. Relative to observed
BEGIN, record starts2.537254s and ends5.618301s; last decoder ends6.090494s;
pre-stack snapshot is8.277112s, already0.277112s beyond the8s start guard.
The2.186618s gap includes validation, selection and snapshot work; no separate
selector or exception timestamp exists. Do not attribute the entire gap to one
operation.

Title148783/start166475850 is forcibly cleaned up by the failed harness, with
no survivors/errors and unchanged source identity/terminal full seed. Three
scout children finish0; there is no stack child/data, completed counter pair or
final phase frame. Root inspected the available gameplay/unpaused-before frames:
rear view and HUD0mph. This is not a successful capture or normal title exit.

The failure-only collector verifies and retains35 files/17,709,277bytes.
Independent failure-preservation review passes.

- Controller: `B319F41903F448E687943A3C5B9FCBB065C396D546B10FD4025B5330CE4109B0`
- Failure collector: `BF46BB8D1E9FC18BE060906357180E98AAF68B407A5C57B39566816BE730CE5E`
- Archive: `972345B0A4122FE0398D35C5FDED92396F4F48960FD4A3A71C09D99B611A6190`
- Inventory: `99D2B00E01893865F24D830BDD1B0999DB721F07C6268CD086BCE26501C3AA4C`

## Revised selector and second capture

H2=`private/evidence/M7-016/watchdog-caller-profile-v2-20260907`.
The original selector parses both large map texts for every eligible hit. The
revised selector parses immutable before/middle maps once into numeric rows,
retaining unique containment, executable/path checks and complete row equality.
Every timing gate, sampling command and game/runtime/seed identity is unchanged.

Both selectors reproduce the150-hit failure scout. Seven negative equivalence
fixtures reject missing/changed thread, image, mapping, permission and duplicate
mapping cases. Existing556-sample header/four-negative tests and prior profile
612-hit/first3s155-hit/six-negative selector tests pass. Independent replay also
checks unrelated paths and whitespace normalization.

Three local-host runs take0.675–0.748s originally and0.065–0.071s with cached
numeric rows over4676/4778 map entries. This motivates the correction but is not
an exact measurement of the Deck delay. Original failed artifacts remain intact.

- H2 controller: `0B17A9EAA27588F23F2861E18E77865FEF74BEDA29B21F498118CB23C6F87D38`
- Equivalence receipt: `C9EDCA4C91A4909BFF8E7BF6AF24A45521E944947A43FD0336DD9D8AB18D0902`
- Stage manifest: `E3C67635AF63A500F9CAD0D21CE0B350E05344A2B217C09955168B48289D1AA8`
- Collector: `84D8736739541D775F0BF2270E55CDE2480A8541ABC0C507B36EA562361B4A4D`

Second exec11701 completes normally, exit0/no force/survivors/source drift,
177.881519357s controller elapsed; title149328/start166532594. It records644
stack samples. Root verifies/extracts48 files/39,984,694bytes and inspects five
gameplay/unpaused/pause frames: rearHUD0mph at unpaused endpoints, with changing
lighting/traffic. Full capture analysis and primary reproduction now pass:
613 scout samples select152 helper hits on TID149411/start166532807;644 stack
samples have1368–4096 valid bytes. Twelve selected images and thread birth/name
match across four snapshots. Six commands, raw/text/header records, full seed,
counter/phase references and normal lifecycle verify. Stack recording starts
6.575098s after observed BEGIN, lasts8.220245s and ends5.264627s before observed
END. Last scout decoder to pre-stack snapshot is0.362383s; this remains a combined
validation/selection/snapshot interval. Clock and complete-lifetime limits remain.

- Archive: `53C073057B0A8522D1C31B9C358675DA4254B49849196A7C5E50AC089EBA8DE3`
- Inventory: `132250DB821D7A3F8441B8C13C32D84F0274F62CCF63221E85FBCD1569C8FB28`

- Capture analyzer: `EA3100B55B0E9873638099775947D2928F48AB68AF7293E7B486D7B57D68A0AC`
- Capture analysis: `9BB76EEE694728F254EA5E950F93492035F41CB2195ADD4AAE98F69E92B0BDF4`
- Capture verification: `348CDD6701656A15B0EED4F1F751C51EBE06B29A0D9D65E07D6AAFC2B799D104`

## Accepted caller result and narrower next experiment

All644 sampled IP mappings are stable across four snapshots. The helper has396
leaves:325 exact PAUSE sites and71 other states. All325 PAUSE samples establish
the same first return129375E to `sub_82411E98`. Its exact unwind requires104 bytes;
the second captured return at offset0xC8 is12C2F88 in `sub_82419E98`, following
decoded call12C2F83→1293380. The first call is1293759→12990B0. Both returns and
sampled IPs are bound to the current host and stable executable mappings.

There are no missing/unknown first or second hops within the325-sample cohort.
The71 other helper states and248 other leaves remain outside this fixed-offset
proof. Independent raw reconstruction and final analyzer review/reproduction
agree. No arbitrary stack scan, automatic DWARF or further-hop inference is used.

- Raw stack: `E25F56243B6730DF4F12E93D1C71E4A5774B0766B897CE106886BB99784D47D3`
- Caller analyzer: `5FBD12BFDD77C63C441B2BF3EB6E3375DF5641DC382F204871055EE657E74D31`
- Caller result: `F707C22CDE083C3982229E57CE3A17DF10A7FD03E3C0D27C3FDBF68E6FC34BBA`

This narrows the proposed experiment to `sub_82411E98`: yield only after the
helper returns nonzero and the abort branch is passed, before the next fresh
completion-predicate reads at82411F38. The initial post-initializer jump directly
to that label and zero helper-return jump to finalization both skip the insertion.
The shared helper, all32 PAUSEs, watchdog/callback logic and other five callers
remain unchanged. The observed outer path calls through8241A3F8/return8241A3FC.

Next is a source-only exact-caller differential witness under controlled memory
events. Test normal/abort/exception paths, progress changes, modular boundaries
and actual initializer/finalizer effects; only two external callees may be scripted.
This is a narrower experiment, not proof of equivalent real scheduling, fairness,
timing-dependent behavior or a performance gain. No title implementation yet.

## Completion producer: bounded source chain

Independent source/dataflow review identifies the normal producer as the host
GPU-command worker processing a guest EVENT_WRITE_SHD packet. `sub_82412A60`
allocates/zeros a96-byte writeback block and stores its pointer at owner+0x2A90.
Block+0 is the completion sequence; block+60 is separately registered for ring
read-pointer writeback. Initialization uses the current sequence minus2.

`sub_82411CD0` emits packet0xC0025800, initiator3, physical block address with
endian selector2 and the sequence from owner+10908, then increments that sequence
by2. The normal submission path through82412710/82412588/82411DC0/82411928 emits
an indirect-buffer packet and writes MMIO0x7FC80714. The SDK routes this to
CP_RB_WPTR and signals the command worker's event. WorkerThreadMain consumes
the ring; EVENT_WRITE_SHD endian-converts and stores through TranslatePhysical.
The preceding0xC0003B00 packet is INVALIDATE_STATE, not a GPU-idle wait.

Exact GPU18A3 handler range[1F380,1F46A) has the completion store at1F42D. This
handler's register path adds no Vulkan fence wait. The source chain therefore
identifies command-worker progress, not physical GPU completion or proof that
the worker was runnable/starved at every captured PAUSE. CPU initialization,
fallback and watchdog-recovery writers also exist. No live owner fields or
packet stream were captured. Do not suppress synchronization on this evidence.

The exact runtime2953 also exports `rex::thread::MaybeYield`. The isolated
semantic witness may use its unchanged source body, explicitly distinguished
from an actual loaded-DLL identity; a later title experiment must bind its real
implementation and emitted call.

No clean cadence comparison, owner driving/audio PASS, save promotion, scheduling
implementation, version bump, task closure, commit or push occurred.
