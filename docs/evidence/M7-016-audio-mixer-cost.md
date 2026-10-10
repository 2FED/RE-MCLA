# M7-016: moving audio DSP cost and safe optimization boundary

Status: read-only investigation. No candidate code, build, numerical witness or
performance acceptance exists. Affinity remains the current runtime experiment.

The symptomatic moving capture has126 leaves in guest82146BE0 and122 in823F4450,
all on titleTID224120. Combined6.72% of3688 user-space leaves is a residency
weight, not exclusive time or evidence that audio causes an intersection dip.
Exact retained ABF PE/MAP and instruction-start matching bind every leaf.

The first function applies two conditionally enabled recursive float-filter banks
to256samples. Five coefficients and four history values per bank support a
second-order filter interpretation. Coefficients and bypasses are already cached.
The second mixes256samples with a gain ramp; its caller uses1024-byte channel
strides. This supports audio DSP/mixing attribution independently of codec leaves
elsewhere on the same thread. Voice/object identity and full callchain remain open.

| Disjoint sampled instruction category | Filter82146BE0 | Mixer823F4450 |
|---|---:|---:|
| Context loads |31|17|
| Context stores |23|30|
| Other context reads |3|2|
| Endian conversion |22|18|
| Direct guest-memory accesses |2|11|
| FP arithmetic/conversion |26|7|
| Calls |0|0|
| Other |19|37|
| Total |126|122|

The filter's compiled body already has96hardware FMA instructions. Its generated
std::fma expressions do not demonstrate software-FMA calls in the loop.
The mixer has zero calls and zero fused FP instructions; preserve its separate
SIMD multiply/add. Sampling skid/dependencies forbid treating context-store leaves
as removable cost. Mixer alone accounts for3.31% of capture leaves.

Exact PE function bounds, half-open RVA:
filter186D60-18CB2B/unwind29C8F9C; mixer11DB430-11DBA80/unwind29F5E34.
HostABF722683E201DE241D3ABAE934BE7BC8AB592DB327E51A29CBE41CDC180EC62,
MAP9E47D2BB74AD2B4F7410E595E3869207F8B54EA26E68A291C3DF8D5C6958015D.
Private retained sample text97180FCB; full capture bindings are in
[the moving evidence](M7-016-manual-h100-profile.md).

## Semantic boundary before any local-register candidate

Computational inputs include r1/r3-r7/r31, xer.so, FPSCR/hostFPmode and guestmemory.
Potential modifications: path-dependent r3-r11/r31, v0-v13/v30/v31/v61-v63,
cr6 and (nonempty paths) cr0/xer.ca, FPSCRflushstate and breadcrumbs.
Finalr31 is the actual guest-stack reload; aliasing can make it differ from entry.

Guest-memory macros preserve32-bit address wrapping,16-byte vector alignment,
physicalalias offsets and originalaccess order. Source/destination/gain/guest-stack
overlap must remain valid. Recoverable write-fault callbacks retry the native
instruction; an eventual destructor cannot expose guest context during a callback.
Keep original PC updates and GuestFunctionScope exceptional-exit breadcrumbs.

The audit found no implemented arbitrary-point full-register observer: running
thread serialization returnsfalse, RtlCaptureContext is a stub, APCs execute at
alertable waits, and crash breadcrumbs do not capture everyregister. This is
not proof that arbitrary debugger/futurecallback observation can be ignored.

A conservative experiment would localize vector arithmetic only between original
memory accesses, synchronize dirtyvectorcontext before each possiblefault, and
avoid stale cachedcontext across callbacks. Keep GPR/CR/XER/FPSCR/PC updates.
That may retain moststores; no useful speedup is predicted from source alone.
Wholefunction exit-only writeback is not currently justified.

A differential witness must compare full defined context, orderedaccesses and
memory for zero/small/256sample counts; in-place/partial/gain/stack overlaps;
signedzero/subnormals/Inf/NaN and supportedFPmodes. Inject observation/failure at
every memoryaccessordinal and compare preaccesscontext/partialmemory. Such mocks
do not prove actual OS fault recovery; a separate runtimegate would remain.

Read-only sources: generated/default/mcla_recomp.0.cpp:55181 and coefficient
helper55014; mcla_recomp.25.cpp:56931 and caller57105; mcla_init.h:107;
SDKsrc/system/mmio_handler.cpp:383, crash_report.cpp:89, xthread.cpp:1313,
include/rex/ppc/context.h:508. No generated/source changes were made.
