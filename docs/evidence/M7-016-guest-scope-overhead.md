# M7-016: guest-function diagnostic scope overhead

Status2026-09-06: exact profile/source/linked-code attribution complete;
replacement semantics and compiler behavior are under investigation. No
diagnostic functionality has been removed or game candidate changed.

## Observed work

The [unpaused hardware-FMA profile](M7-016-unpaused-fma-window.md) contains2933
samples. Exact image/section and own-function-boundary attribution yields:

| Module | Leaves |
|---|---:|
| Host | 1256 |
| VCRUNTIME140 | 348 |
| GPU SDK | 278 |
| Windows NTDLL | 217 |
| Runtime SDK | 212 |
| RADV | 143 |
| KERNELBASE | 116 |
| Unix NTDLL | 110 |
| KERNEL32 | 73 |
| UCRT | 58 |
| libc | 40 |
| MSVCP140 | 25 |
| vDSO | 22 |
| Other mapped modules | 35 |

Wine maps many PE code pages anonymously. The host attribution uses its pinned
header/load base, executable sections and exact .pdata ranges; it does not
treat anonymous pages as unrelated code or use the nearest preceding symbol.
All sampled IP mappings agree at both snapshots, although the complete process
maps change.19 sampled TIDs retain their birth identity in68-thread snapshots.

VCRUNTIME contains43 leaves in its exported `__uncaught_exceptions`, plus65,
81 and31 in three bounded helper ranges. Exact disassembly/imports establish
a static route from exception-count lookup through per-thread data access and
cached `FlsGetValue` resolution, with `GetLastError`/`SetLastError` preservation
and a `TlsGetValue` fallback. All81 resolver-range sampled PCs fall in entry,
cache-check or exit code, not loader/GetProcAddress slow paths. They do not
establish repeated DLL loading. Three other leaves hit guard dispatch.

Installed MSVC14.44.35207 sources corroborate `__vcrt_getptd_noinit`,
`__vcrt_FlsGetValue` and `try_get_function`; a matching DLL PDB was not found.
Those source names are semantic correspondence, distinct from exact PDB names.
The220 exception-count/helper leaves occur across eight guest-thread TIDs;
116 are on the busiest139270. These are leaf counts, not a dynamic call chain,
invocation count or recoverable frame-time budget.

VCRUNTIME also has118 exact `memmove` leaves,117 on GPU Commands139221 and one
on an audio-labelled thread, plus7 `memset` leaves. These are separate work.
Remaining UCRT math is47fma/3scalbn/1_dclass across four TIDs; it is not
attributed to particular host callers by co-occurrence.

## Concrete source and linked mechanism

SDK `include/rex/ppc/context.h` defines `GuestFunctionScope`. Its constructor
samples `std::uncaught_exceptions`, saves the previous guest PC and function,
then records the current function. Its destructor queries the exception count
again. Equal counts restore both independently saved words; an increased count
retains the faulting breadcrumbs during unwinding.

Every generated prologue variant includes this scope, including ordinary
Release without Tracy. The authoritative template is
`resources/templates/codegen/init_h.inja`; generated `mcla_init.h` is a consumer.
Source and installed context headers match SHA05CE5B45.... Existing
`tests/unit/system/crash_report_test.cpp` covers normal nested restoration and
retention of the faulting inner scope.

The current exact44E82B46... host imports MSVCP140's
`?uncaught_exceptions@std@@YAHXZ`. Its linked thunk has83559 static direct call
sites, including normal and separate cleanup paths. The frozen generated corpus
contains29788 prologue occurrences across63 recompilation units. These are
different static counts, not runtime invocation estimates.

Three sampled functions in TU2, TU27 and TU0 each contain two main-body calls
and one separate cleanup call: entry saves the count, normal exit compares it,
and restoration is skipped on inequality. Both context words at0x150/0x154
are restored on equality. This is a concrete cost mechanism consistent with
the profile, not a measured standalone optimization opportunity.

## Candidate direction and required proof

A standard-C++ normal-completion continuation may save the previous fields,
invoke a body and restore only after the body returns. An escaping exception
skips the restoration naturally. This requires a code-generation/wrapper change;
merely making the old destructor restore unconditionally is incorrect.

A guest-free witness now passes natively under
`guest-scope-witness-20260906`. Nine cases compare actual installed SDK behavior
with the proposed continuation for nested calls, early returns, caught/escaping
exceptions, callbacks entered during existing unwinding and local destructors.
All78 observation rows agree with independent expected values and between arms;
nested unwinding explicitly observes1→2→1. Previous PC and previous function
are deliberately different in relevant cases and restore independently.

Reviewed source43E8CA31... stays frozen. A separate runtime preamble produces
module paths and a2s mapping-observation hold.1195 inputs/210,678,573bytes are
frozen; four compile/assembly/IR/link commands exit0 without diagnostics.
PE CEC25CBC8EBAEBF278A764166CE54B6FB574EEEC1F89E93DDDFA093538E5DE05
imports MSVC/CRT but no ReXGlue runtime. Native exit0 takes2.0763663seconds,
with five exact loaded images, no force/survivors/errors and all source/staged
files unchanged. Native stdoutB6B0074B... contains96 validated lines.

The address-taken `std::uncaught_exceptions` preamble resolves to the local
import thunk in this PE, not the final MSVCP implementation. Static import
review and actual app-local C++ DLL mappings are retained separately; no
resolved-IAT implementation-pointer claim is made. Observation calls intentionally
affect code generation, so this is not a performance or asynchronous-fault test.
The same PE also passes exact Proton10 with the matching four app-local C++
runtime files copied on Deck, no DLL overrides and a fresh isolated prefix.
Wrapper and child exit0, no force/survivors/cleanup errors or input drift.
Five mapped images are verified; all78 semantic observations match native
Windows exactly. This remains a console control-flow witness, not the normal
game launcher or generated-wrapper integration.

- Proton runner:17BE8D1CFCA32894A47A4A016369CA183ECA1061ADD86B8DB51556F40FA7C331
- Proton result:26092B7E442BC0E2F29893C63898F23C07CD2B2E85DD27972410B0ECEAA7AAF0
- Proton analysis:35B75BB3F723DBD90CEEB090F16D3DBB21618BCCAAFCC38B957793887CF72463
- Return verification:CD5E9F8291F04EE4DF4C4D93532674B8D5F626F8DFD7A72DC09700962B411A2E

Independent final return review passes:11 copied evidence/helper files plus the
local analysis are checked, alongside all1195 frozen inputs and16 build artifacts.
Generated ABI/alias and setjmp/longjmp fixtures remain separate from these nine
standard-C++ cases.

## Optimized owned-read-fault characterization

Separate body and handler translation units compare four synthetic cases per
arm: entry-only, later block PC, nested inner body and a same-PC loop whose first
inaccessible word is iteration16. Runtime-selected two-page storage is initialized
before making its second page PAGE_NOACCESS. The handler accepts only the first
continuable read AV at the exact owned word on the armed thread, records the
breadcrumbs, changes only that page to RW and resumes once. Other faults continue
searching. No game memory, instruction-pointer adjustment or observer call in the
tested body is used.

Five compile/assembly/IR/link commands pass on1197 frozen inputs/210,681,722bytes.
The original PE019D31B6... remains unchanged; an offline symbol relink has identical
layout and bytes in all five sections. Map46C7C276... binds all eight observed
fault IPs to their exact body loads, including leaf functions without .pdata.

Native Windows and exact Proton both complete eight reads/four comparisons with
the expected and equal PC/function values, correct results and caller restoration.
Both exit0 without force, survivors or recorded identity drift. All five loaded
images are verified on each runtime. The29-line outputs include the mapping
preamble; normalized fault observations agree across runtimes.

No barriers, observer calls or volatile breadcrumb fields were added. Both loops
already retain one PC store before each volatile load: four unrolled sites and
one remainder site. The Windows compiler lowers these guest reads as
**atomic volatile acquire**; that existing behavior is part of this result.
The continuation bodies have zero exception-count calls; SDK bodies have ten.
This does not extend to unhandled faults, vector/nonvolatile accesses, other
compiler models or all generated control flow, and establishes no FPS gain.

- PE:019D31B62956CA0586F1F0E93A587058F91069356479A91B202315E01DC68B31
- Map:46C7C27674E09CEE1ECD88AFCD43B181C8A8C50233EDFB46E6A8C709304EF53A
- Native stdout:6BA4B0DB24D128D1848A04F88393DEEF12661C278DCC8F879F19DEA40AD217D7
- Proton result:66CB578C86296192FF507B1B7531F0E279D47F6948619F796FA21B4D70E8956A
- Proton analysis:4F9AD77685A0CE850AFF3249151BED9C3FA66B653B43ADFB446C6B59A3B6B1CF
- Return verification:7E9CF62D8309BD2293A3B1BA28A5E17F2314D85833025EC808F1F660F816155D

Primary and independent native/source/code/Proton return reviews pass, including
11 copied files plus local analysis,1197 inputs and20 original build artifacts.
Next source preparation is the separate generated-ABI,
weak-alias, destructor-order and platform-longjmp fixture. No game candidate is
implied by these synthetic successes.

A subsequent exploratory compile of the actual SIMDe aligned vector-load/store
category exposes a distinct gap: its nonvolatile accesses do not have the scalar
acquire behavior. The SDK retains breadcrumb stores; a plain continuation can
eliminate them completely. This is why the scalar fault result is not a general
generated-code guarantee. Compiler-only publication candidates retain the stores
in initial probes, but their lifetime/ordering and loop-cost effects need review
before selecting a mechanism. These exploratory stdin compilations are separate
from the frozen eight-fault witness.

The existing [M3 breadcrumb contract](M3-009-crash-reporting.md) covers C++
guest exceptions and the last tracked entry/block/unimplemented site. It does
not promise instruction-exact reporting or generic native-fault recovery.
Compiler observability is a separate compatibility check: removing external CRT
calls may allow diagnostic stores to be removed or reordered across a faulting
memory instruction. Representative native access-violation/resume witnesses and
exact assembly review must characterize baseline versus candidate behavior
before a game experiment, without inventing a stronger legacy guarantee.
Blindly adding volatile stores
to every hot spin-loop iteration could create new overhead; no such change is
selected. Private CRT offsets/cached internal TLS pointers are not a solution.

Weak aliases, original `__imp__` entry points, callbacks, generated SEH paths,
setjmp/longjmp, Tracy naming and all return forms must retain their behavior.
An emitter-owned normal-completion label outside the body-local lifetime is
the preferred production direction; a private wrapper can first test the idea.
The [nonvolatile vector control](M7-016-guest-scope-vector.md) now demonstrates
why compiler publication is necessary: plain continuation loses both diagnostic
fields, while signal-fence publication preserves the five tested sites without
CPU-fence instructions. Its additional stores and changed loop optimization
make an actual game comparison necessary.
No production scope/template change, SDK promotion, version bump, task closure, commit or
push has occurred. M7-016/KI-027 remains open.
