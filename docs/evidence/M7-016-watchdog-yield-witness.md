# M7-016: narrow caller-yield semantic witness

Date: 2026-09-07. M7 remains0/16. This establishes bounded fixture behavior;
it is not a title performance result.

## Tested change

[Caller profiling](M7-016-watchdog-scheduling.md) identifies the observed chain
sub_82419E98 → sub_82411E98 → sub_82412F98. The proposed host yield is inserted
only in sub_82411E98, after the helper returns nonzero and its abort branch is
passed, before the next fresh predicate reads at loc_82411F38. Initial entry
through the explicit goto-label and zero helper result skip the insertion.

Private evidence Q=`private/evidence/M7-016/watchdog-caller-yield-witness-20260907`.
Two exact extracted caller bodies differ only by entry names for coexistence and
one candidate HostYield call. Six shared extracts preserve the actual helper,
initializer, finalizer, save/restore29 and thread-ID implementations. Source
normalization verifies all seven extracts. Only preparation82412710 and timeout
callee82425C18 are scripted boundaries. The finalizer reporting pointer is0;
its indirect-report branch and preparation's recursive subtree remain uncovered.

Both arms use the actual accepted3157 scope header and PPCContext. Original
memory accessors and PAUSE execute under bounded observation wrappers. External
memory events occur at the candidate yield or the corresponding baseline fresh
read following a helper invocation; the initial predicate does not receive that
event. No globally aliased context pointer bypasses the restrict-qualified body.

HostYield executes the exact copied Windows SDK-source MaybeYield body:
SwitchToThread followed by MemoryBarrier. This is a source copy, not a bound call
to the game runtime DLL. A title candidate must separately bind its actual export.

## Review, corrections and build

Root and independent source review corrected two fixture defects before runtime:
count finalizer entries at the unique prologue stack store, rather than counting
both reason reads; publish the readable arena pointer only after memory commit
succeeds. Original handoff BDFE7FA8 remains in source-v1. Two unsuccessful build
attempts are retained: missing installed-SDK external-spdlog defines, then Werror
on omitted aggregate members. Explicit default initializers preserve case values.

The final build uses pinned Clang20.1.8/O3/NDEBUG/dynamic CRT, separate TUs,
SSE4.1 and the existing installed SDK include tree. All1,207 frozen inputs and
21 compile/assembly/IR/link/inspection commands verify. Independent linked-code
review confirms all10 alias pairs; baseline17 direct calls versus candidate's
same17 plus HostYield; initial/abort branches skip yield; eight PAUSE-wrapper
calls remain in a four-count loop and the wrapper emits F3 90. The yield path
contains imported SwitchToThread and MFENCE. Win64 unwind/preserved registers,
normal breadcrumb restoration and typed timeout-exception escape are reviewed.

- Source manifest: `06C8BB7448045E0A14A0B3199AAED866B83136D94B3A313EB28614CC4E926D4A`
- Frozen build manifest: `90A07E2EAF4341B73D6BBFE33874A5391B58260859A944D662583DB4AE36D94D`
- PE: `200D4FC755813486B31DFA027D28D09238E82AFA5E7A8458B6017688E934C2B0`
- Native runner: `5B38D793EE6D415D2887FEA25692281BB2783C21E07551EE8C4218C6C3ADE0ED`
- Proton runner: `4FD085E7718C032B39A643B33578CE49CB0CB0452F57B6D6D4F32E6F88E00B91`

## Runtime results

All32 scenarios pass on native Windows and exact Proton10. Each compares the
complete context representation, complete256KiB arena including stack residue,
and ordered access/callee/event traces, plus explicit state/count oracles.
Guard pages, four-helper and4096-trace-event caps bound each arm.

Coverage includes early/preparation/initial-goto skips, ordinary and+2 progress,
second progress read and A→B→A, thread-refresh guards,4999/5000 and unsigned wrap,
timeout return/side effects/exception escape, two/three iterations, reason0/3
finalization and high-half register/LR canaries. Both arms execute30 helper calls
across the matrix, each with32 PAUSEs. Candidate yield count is22; baseline0.
The36 normalized output lines match exactly across runtimes, including32 case
rows, memory/trace digests and the final PASS. These digests supplement full
in-memory comparisons; they are not the sole oracle.

| Runtime | Process | Controller elapsed | Result |
|---|---:|---:|---|
|Native Windows|42816|2.091s including identity hold|exit0,32/32|
|Exact Proton10|150245, wrapper150098|11.709s including prefix setup|exit0,32/32|

Both controls leave no survivors, forced cleanup, unexpected native stderr or
source drift. Proton retains only the known optional32-bit MangoHud dependency
warning and normal fsync messages. Exec86899 is closed. Both map the exact
witness, MSVCP140, VCRUNTIME140, VCRUNTIME140_1 and platform UCRT; atomic-wait is
absent. Preflight correctly makes MSVCP mandatory because Q imports it.
The two-second flushed identity hold precedes fixture execution.

Eleven returned files/56,297bytes match the fixed ten-entry inventory plus the
inventory itself. Root verification binds source/build inputs, maps/preambles,
lifecycle/output receipts and all case rows; six malformed-output tests reject.
Independent final runtime/verifier review is recorded in the handoff.

- Native receipt: `E9DD0B7ACB0C5F5140E67416E8926B8E4B75706B5BDF533AA3E0D48228797B70`
- Proton receipt: `797BC8046624BA8A87668A47CA569A6F47E380793DD407B1A8CF4FEEC646A7B7`
- Native stdout: `22050ACBEF6D52CAD43088B07A5AFB7C751120F4A405CDEAE7FECFB20FB1353F`
- Proton stdout: `D71EAA74FBEFDC895E1567AAD45B0D12C3A8EC5A813B338405B72B1DC451CAE0`
- Return inventory: `D47E1005EB9D7C3992D81AACA1FE115BB3AF2F0D24C7F94FEE74B57A36F8D8B9`
- Verifier: `FA71B3DE1BA8A68B464C3D662D64E58960ADFDBB4B5A3FF3CB4D568A8C0A474E`
- Verification: `4C066321A6CC1BB85C1E94ACF71389BBCD67FA85F864BF91025329B66D8BBDD8`

## Next gate and limitations

Build an isolated uninstrumented title candidate, preserve original generated
files/SDK/companions, and review the real yield import and control flow. Then run
native visual/lifecycle controls and clean Deck baseline/candidate/baseline
cadence checks. Fixture instrumentation changes compiler behavior; its pass does
not substitute for that integration review.

No real scheduler fairness, producer starvation, unchanged timing-dependent
behavior, full preparation/timeout subtree, arbitrary fault-PC contract, FPS or
audio improvement is established. No game implementation, bundle/save promotion,
version bump, task closure, commit or push has occurred.
