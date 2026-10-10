# M8-001 stock-timer presentation-interval candidate

Oct3 continuation: [native Windows harness checkpoint](M8-001-native-windows-harness-20261003.md).
Original159 component mocks pass; later integration tests are in the linked report.
Owner14:02 no-game fixture13 frames/8 states passes
primary integrity and independent visual review, normal exit0/Job0. Full real
arm/native game adapter integration remains. One isolated stock A1 startup at14:50
failed harness HWND rebinding before ACK/capture, normally closed exit0/Job0/no
force with source/seed/binaries unchanged. Cohort2 then falsely rejected seven
known developer asset probes and normally closed with the same preservation.
Exact parser/caller guards and owned physical sizing are offline fixes,not native
PASS. Owner cohort3 accepts startup/guest ACK/physical sizing/normal cleanup only;
all-black client BMP rejects its pixel gate. Legal splash is not logo or Press
START. Stock gate now disabled,cohort3 consumed; preserve evidence,no auto-retry.
Owner WGC attempt1 normal helper/fixture cleanup but full-grid mismatch rejected
with no frame;32 centers match,exact cause not retained. Later owner
attempt2 diagnostic located54 mismatches at bottom9rows,zero shift best; no
mask/rounded-edge cause proven. Attempt3 saved five exact full-grid frames/four
states,independently reviewed partial pixels; helper6 exit1/Job0,no failure header,
full fixture FAILED. One reviewed autonomous fresh attempt4 while owner AFK failed
visibility before helper/frame,normal cleanup/source preservation. Source fixture
gate admitted owner6: visibility/four exact frames passed, then helper5 rejected
timestamp14.8us ahead of early QPC. Reviewed build4 checks strict upper bound
after GPU completion/before Map, without tolerance/retry.381 mocks/1528 CPU
checks PASS. Primary7 failed visibility before capture; owner8 then passed13
exact frames/all8 states,all14pins/normal cleanup and independent visual review.
No-game WGC fixture accepted. Fresh cohort4 stock A1/WGC integration and staging
review GO,384 offline mocks PASS; ONE owner-started stock passive-frame probe
completed/accepted for startup/passiveACK/legal-splash WGC pixels/normalcleanup
and source/seed integrity. Actual one-shot capture cadence cannot meet current
cycle gates; separate bounded persistent-series engineering now precedes scenes.
B/A2 not staged/launched; stock/fixture source gates disabled after consumption.
Candidate has still never run;
no live cohort admission. Frozen checkpoints below retain their original limits.

The owner-observed correct-speed Deck control uses stock TU5, D3D12,720p,
VSync60 and present interval2. It publishes about23–26 guest frames/s in
retained gameplay measurements; that is not scanout FPS and cannot establish
stable45/60. The earlier B256/V96 `0x821BDB08` unconditional branch is
rejected: it skips timer mode, history and reset handling, and the owner saw
accelerated splash/world and turning jerk on both backends. This experiment
keeps **stock TU5** and changes only the separate selector2 presentation
immediate at0x82419AA3 from02 to01 (`39600002`→`39600001`). It is a private
timing hypothesis, not a product fix or performance result.

The source-only candidate changes two ASCII digits (instruction comment and
literal) in retained H100 TU27. Stock TU5, H100 setter, U8 GPU and all other
generated TUs are pinned. Offline36 source checks and independent scope review
passed. Source `C776887145D69BFDAFAC1C90EFFEA946D106D817F9B180703A48E3D7A6CDA48B`;
baseline source `0614D9BF9E268A8658D6DBD4CC9462D5A9C0460410B19EC67F48CCE017464FA2`.
Private preparation root:
`private/evidence/M8-001/stock-interval1-20260928/`.

## Baseline reproduction

The retained ABF graph has72 objects and426 TU27 dependencies. A private
supervised Job builder freezes1,047 build inputs and2,970 protected current
identities/absences; it never edits the normal bundle or generated/out trees.
The first baseline compile failed before producing an object because the
restricted child environment lacked MSVC include discovery (`<cstring>`),
although the installed header matched its frozen hash. No link ran. A new
discovery-only revision explicitly pinned installed
`VCToolsInstallDir=.../14.44.35207`; owned `-###` and dependency-only `-M`
exited0, selected MSVC14.44/UCRT SDK10.0.26100.0 and reproduced all426
dependencies. Discovery result
`57168C894468E96D321363B005E81B831B01E5845E23A22927B3BB13E1F39D92`.

A fresh baseline TU27 compile then exited0 with empty stderr and exact426
dependencies, but stopped before linking because the raw793,195-byte object
SHA differed. Independent whole-file comparison found only three differing
bytes inside COFF TimeDateStamp `[4,8)`. A separate private normalized copy
equals retained object `827A600543066BDAFE0C34ECDBC235A1D2CA56EBD2C43F4CD9D0B748FAD4538A`
byte-for-byte; the raw object remains preserved. Independent receipt
`0F805C8DAB85EA4AFED0A8ADE565E51F025548D4DBD5A2743665A499782D2A07`.

One reviewed link with that normalized object plus71 unchanged objects exited0
normally. Its raw45,310,976-byte EXE
`DB8DDF5657CEBEF6EADA78A5C62E52AA031FD43D46054161CB64F05D75D2BE4D`
differs from retained ABF only at PE timestamp offsets128–130; a separate
in-memory normalization exactly reproduces ABF
`ABF722683E201DE241D3ABAE934BE7BC8AB592DB327E51A29CBE41CDC180EC62`.
All code, imports, TLS, pdata/unwind and relocations are otherwise identical.
The MAP differs only in its timestamp line. Independent baseline-host review
`ED755770ABE11871FA05190A0EF40CC94AA3BFE7B2F6FDA2D09D4E5DCBD38CD2`.
No baseline game run was inferred from this build.

## Candidate compile stop

The exact one-immediate source candidate compiled with normal Job ownership,
zero warnings and the same426 dependencies. A deliberately strict object
gate stopped **before linking** because Clang optimized the selector tail
instead of emitting a single immediate-byte change. Independent COFF and
disassembly review partitions all172 raw-byte differences: three COFF
timestamp bytes;160 bytes localized to the selector tail of
`__imp__sub_824199B0`; one pdata end offset; eight section-checksum bytes.
The optimizer preserved the guest-PC store for0x82419AA0 and jumped to an
existing `mov eax,1` after that branch's own PC store, shortening the tail by
five bytes. Other selector cases and all other functions, symbols,
relocations and memory operations remain unchanged. This justifies preparing
a **new structural gate**, not retroactively accepting the failed strict
run. Raw candidate object
`CFFCBDE84010B92A0DE34874F9CE3E2F0CA85F4DC5B8B411026D5FDA0379382E`;
independent forensic receipt
`3A201BCC3620A16DF2A01C6B3F7D1E3DE5A96D0E18BF73CFDC37CDB24F67762D`.
No candidate EXE, Windows timing test or Deck test exists at this checkpoint.

## Candidate linked binary

An isolated semantic gate then reconstructed the exact reviewed selector-tail
rewrite rather than whitelisting a changed region. Offline201 tests passed,
including wrong jump target, skipped guest-PC store, altered other selector
case, pdata/checksum corruption and unrelated-byte negatives. A separate
single owned link used the raw candidate object plus71 retained objects and
exited0 normally, with no warnings, forced cleanup or survivors. Inputs and
protected state remained unchanged. Candidate EXE
`48E8F4F63822C7E7F827C05DD8D25BCCF53C280C27E31B748387535BD7595E11`;
MAP `8382C10B5E77B1047A46C323561A02E60967F50098DDA38A48F630A8DF5565A7`.

Independent whole-file review found exactly164 differences from ABF:160 bytes
in the reviewed selector-tail branch sharing, three PE timestamp bytes and
one pdata byte shortening that function by five bytes. The guest-PC82419AA0
store remains before a jump to the existing `mov eax,1`; other selector cases,
code, imports, exports, TLS, relocations and unwind information match.
Independent binary-review receipt
`9EDBEEE0A13E61F096E13DD01C9CC23AE442C0F7B55A4873B4C9CAF2F8CA5257`.
This is **static change-scope PASS only**. No candidate game run, timing
correctness, FPS or Deck claim follows.

Even a later statically admitted candidate must first pass a bounded Windows
correctness check. Existing effective-delta sums do not cover every timer
instance, mode, reset, history or consumer. An interval1 cadence increase
alone cannot prove normal world/splash speed; stock interval2 remains the
rollback. No version bump, commit, push or product promotion follows.

## Next Windows rejection gate

Read-only feasibility review found that the frozen app already offers a
private-file START/A request/ACK seam and requested guest BMPs with
`present_seq`. A bounded A1/B/A2 Windows experiment can compare the same
intact seed/cache under stock interval2 / candidate interval1 / stock
interval2 without physical input. It needs a new reviewed wrapper: older
native launchers hard-pin different binaries/routes, and the retained owned
window-capture helper previously failed an occlusion gate. No such wrapper or
game run exists yet. Focus and visible1280×720 capture must pass a fresh
preflight; F10 must stay off because it arms extra diagnostics.

The prospective test uses identical H100, D3D12, VSync60,720p and fresh
isolated roots. It compares repeated splash/menu/gameplay animation periods,
guest-output sequence and host presents separately, with normal owned close
and save preservation. A conservative B speed bound >25% above both stable
controls can reject the candidate. Inconclusive capture, scene or control
drift cannot be called a pass. Even a visually normal result would only say
gross acceleration was not observed; timer-instance/consumer, turning,
garage and owner physical correctness remain open.

An isolated source-only harness core was then prepared at
`private/evidence/M8-001/stock-interval1-20260928/windows-rejection-prep/`.
Its61 fake-backend/contract tests and AST check pass. It binds exact A/B/A
artifact identities and limits, preserves a conservative `>1.25×` rejection
criterion with stable controls, and distinguishes `INCONCLUSIVE` from any
speed claim. An independent review admitted it **offline only**. `--live`
unconditionally rejects; no native backend, cycle-feature extractor, staged
arm or game run exists. The next implementation must add exact process/Job,
HWND/occlusion/capture, file request/ACK, optional PresentMon, save isolation,
normal closure and UNKNOWN ownership handling, then pass a fresh no-game
preflight and independent review. Do not enable Live by changing a flag.

A subsequent isolated offline revision tightened the harness schema: all
cycle values must be finite/bounded, counts are exact integers, each cycle
phase must match its enclosing phase, and receipt writes reject traversal,
reparse/symlink parents and existing destinations. All172 offline checks
passed and an independent review admitted this **offline only**. Live remains
disabled; it still has no native backend or real cycle extraction. Source
`8E4A8561D0B45E507099B7A6943A28C417C4903A35B96F2D7E082B45CA569435`;
reviewed revision:
`private/evidence/M8-001/stock-interval1-20260928/windows-rejection-prep-v2/`.

A further isolated offline component modeled fresh A1/B/A2 arm creation from
the original four-file seed and empty cache, exact PID/birth ACK only after
Job assignment and before resume, and owned/UNKNOWN cleanup. Its28 mock
checks and AST scan passed, and an independent reviewer confirmed no evolved
save/cache carryover. Real staging and process creation remain disabled; no
arms were made. This component is **not** a functional game backend: module
and HWND attestation, input/capture, animation-cycle extraction, collector,
normal closure and top-level signal wiring remain missing. Root:
`private/evidence/M8-001/stock-interval1-20260928/windows-rejection-prep-v3/`;
component `B998209AEAE34BEE85C2C43BA4119922B318B60448F60FD98657755DCE46404C`.

A final source-only passive capture contract binds the planned A1/B/A2 arm
ACK to exact PID/birth/host hash, HWND/client geometry, nonce, monotonic
capture IDs/QPC and exclusive private outputs. It does not change focus or
send input. Seventy mock checks and AST review passed. No actual Win32 capture
worker, game run or Live enablement exists. The original strict occlusion
gate remains required; a prior retained helper failed it before BitBlt.
Root `private/evidence/M8-001/stock-interval1-20260928/windows-rejection-capture-prep/`;
source `29A620745E81CD869E7A768AAECD915B2B946F906FE0F51C9BCA5C9AA15C1068`.
Work stopped at **25% weekly allowance remaining** per the owner threshold.
