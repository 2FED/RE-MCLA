# M7-016: software FMA found in the gameplay profile

2026-09-06. Offline attribution and compiler experiment; no game binary,
package, SDK source, generated corpus or published bundle changed.

## Exact sampled cost

The accepted [all-thread gameplay capture](M7-016-moving-allthreads-profile.md)
contains4094 userspace leaf samples. The owner drove normally, including stops
in traffic, without pause/menu/loading. This does not imply constant movement.

XThread0240 has994 samples. Its393 Proton UCRT samples resolve to365 in
fma,23 in scalbn and5 in _dclass. Across all sampled threads the respective
counts are409,25 and8. Thread names encode Windows IDs; these leaves do not
prove an audio or physics subsystem, a dynamic caller chain, invocation counts,
exclusive CPU budgets, frame rate or the cause of all audio stutter.

The exact mapped Proton10 UCRT is retained privately:
CEC72E71FCCBB545EBD1FE96F92E9A849E726AFA76E092A4128FC193893DE16C,
1,434,926bytes. Its device259:8/inode7486209 matches both capture snapshots
and the post-session source. Remote before/after hash and returned bytes agree.

Own PE runtime-function ranges, exports and COFF symbols establish:
- fma: RVA[0x53C50,0x5433A), aliases _o_fma/_o_fmal/fma/fmal.
- scalbn: RVA[0x6AE90,0x6AF44).
- _dclass: RVA[0x228E0,0x22926).

The exact fma body has no hardware vfm instructions. It contains four integer
multiply instructions, calls _dclass and tail-jumps to scalbn at0x54107.
This is a concrete software-math cost in the measured runtime.

The exact measured host is974392DD9C7D60578AC2BCA50E24317FD4F1F2E2EE19CD989219B72888B64164.
Its guest sub_82146BE0 has own PE range[0x1932C0,0x199BD0),96 static
call sites to thunk0x2DF5990, and no hardware vfm instructions. The thunk
uses IAT0x2E1F000 importing fma from api-ms-win-crt-math-l1-1-0.dll.
These are static callers, not proof that all sampled fma invocations came
from this function. All30,034 current guest mapping keys were checked;
current mapping SHA256 isF9E50347F925761E647CE1154E7BC9CF6DC33572F93315C31287224BD3B4470B.

## Host versus SDK flags

The measured host build tree still contains the exact host above.
Its generated-source commands in out/build/win-amd64-readback-cost/build.ninja
use -O3 -DNDEBUG -std=gnu++23 -D_DLL -D_MT, the dynamic CRT dependency,
and -msse4.1. They contain no -mfma, -march or -ffp-model=strict.

The SDK Windows preset and actual SDK CMakeCache already use
-march=x86-64-v3. The installed rexglue_helpers.cmake adds only -msse4.1
to this host. SDK directory-level -ffp-model=strict is not inherited by
the separately built generated host. Thus the host misses FMA while the
current SDK already targets v3. This does not establish runtime dispatch
or prove a complete supported-machine feature check.

## First compiler proof

Private fma-codegen-probe-20260906 records exact commands, assembly and LLVM IR.
Compiler clang++20.1.8 SHA256:
57E32B1EA2456F12F97BF802453171BBFF12D56B2C486B4ECA0C75343C0CDA2C.

Under the actual host flags ordinary std::fma wrappers call the CRT.
Adding only -mfma lowers them to vfmadd213sd. The single-precision guest
form retains double FMA, then vcvtsd2ss, then vcvtss2sd. Function-local
target("fma") also lowers these wrappers, but provides no runtime dispatch.
Separate strict-mode controls retain constrained FMA with dynamic rounding
and strict exception metadata. No -fno-math-errno or fast-math is required.

This is compiler-lowering evidence, not numerical/runtime/gameplay acceptance.
The SDK floating-point builders deliberately use std::fma and retain
double(float(...)) for single forms. Replacing these with a*b+c or fmaf
would change the operation and is not selected.

## Semantic gate before a candidate

Actual host driver options include -ffp-contract=on and -fno-rounding-math.
Global -mfma/v3 can enable implicit contractions elsewhere. The baseline
already represents std::fma as unconstrained/speculatable llvm.fma in IR;
that permissiveness is not newly introduced by -mfma.

The scalar PPC builders request flush mode off. FPSCRPlatform maps this to
MXCSR FTZ and DAZ together; vector builders enable flushing. Guest rounding
changes also update MXCSR. Tests must cover operation ordering across those
changes, four rounding modes, subnormals, signed zero, overflow/underflow,
NaN classification/payloads, signaling NaNs and relevant exception flags.
A standalone noinline one-expression probe does not settle those contracts.
Strict IR records compiler intent, not proof against the exact Proton UCRT.

The bounded state/control compiler probe and a small native numerical
witness are now complete (below). Broader numerical/runtime review still
precedes any selected target flag or production code change. No physical rerun is needed for offline preparation. A later exact
candidate still requires measured gameplay/audio validation; stock gameplay
remains10–17 FPS with stutter/crackle and M7-016/KI-027 stays open.

## Completed state and native numerical controls

The first probe's frozen verification passes28 assembly/IR function pairs,
22 compiler-tool invocations (14 compilations, seven driver reports, one
version query). Receipt hashes:
- CF264A7FD85B5E82C1E1BA64BE9CD8518E2CC37DB211E93F5EAB2CE63647E99F
- V3 extension A91AB501F43A668CFBBB7F0521E4A0212C0BC3FF2EC88E6A22008324D70FD9A0
- Contraction extension7D82A9DC9BE653C605C58C684D394D794426BAF9737B48BF5023B718066D1B88

The separate private fma-state-probe-20260906 passes12 assembly/IR wrappers
and12 compiler-tool commands. Receipt:
6DB28C3FC51839D576DAA86CD9A3A2069E91581E79693CEBF38C50E619601C90.
Source:
95FC810457EE53DBC380C82645B95501CD3F264E693B98469D31465F4A081823.

| Additional flags | Ordinary a*b+c | Same explicit FMA across down/up MXCSR changes |
|---|---|---|
| None | Separate multiply/add | One CRT result reused |
| -mfma | Implicitly fused | One hardware result reused |
| -mfma -ffp-contract=off | Separate multiply/add | One hardware result reused |
| -mfma -ffp-model=strict | Separate multiply/add | Two ordered hardware operations |

Primary independently reran the read-only frozen verifiers and reviewed IR,
assembly and source. Strict mode retains dynamic rounding/strict exception
metadata. The state collapse exists in the baseline toy program; it is not
evidence that a particular game function currently fails this way.

Primary then prepared and independently reviewed a guest-free numerical
witness. Review caught incomplete stdout validation and a missing precompile
source pin; both were fixed before the first execution. It requires exact
source identity before/after, exactly five strict-output lines and eight fields
per rounding row. CPUID checks FMA/AVX/OSXSAVE before XGETBV; the baseline-built
harness skips unsupported machines. All tests mask exceptions and disable
FTZ/DAZ; MXCSR is restored before normal exit.

Private fma-numerical-witness-20260906 records five compilations, four links
and four native Windows executions, all exit0. Its21 scoped strict witness
checks pass: down/up pair plus positive/negative halfway rounding, smallest
subnormal, negative zero and fused cancellation under all four rounding modes.
For a=1,b=1,c=2^-53, strict returns down0x3FF0000000000000 and
up0x3FF0000000000001. The other three compiled controls return the down
result twice. The ordinary cancellation control also confirms that plain
-mfma changes its result, while contract-off and strict preserve separate
operations. State restoration reports true in every arm.

Native receipt SHA256:
08BF1CA4853D63F200CEB47DB35E42E8C2CAB1A04CBF08BBA78EC43F42C568D4.

These are native Windows witnesses, not an exact-Proton numerical comparison,
complete PPC/FPSCR conformance or gameplay acceptance. The native CRT loaded
by the toy baseline was not pinned as a new Proton-style runtime evidence set.
NaN payload/order, signaling NaNs, infinities, overflow/underflow boundaries,
outside-negation and narrow forms at runtime, VMX-to-FPU transitions, exception
flags and errno still need the appropriate tests. No shipping flags changed.

At this checkpoint the live weekly allowance reaches3% (97% used), the owner's
explicit stop threshold. All probes and agents are finished; no new build or
physical session is started. Resume from the focused numerical/state gate,
then review an isolated candidate and its exact binary before any Deck test.
No version bump, task closure, promotion, commit or push.

Owner renewed limits and resumed. The [expanded runtime matrix and cached-CSR controls](M7-016-fma-runtime-matrix.md) are now complete on native Windows and exact Proton; the isolated generated-TU candidate is the next gate.
