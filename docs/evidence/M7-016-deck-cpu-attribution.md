# M7-016: direct Deck CPU attribution

Date: 2026-09-05. Primary analysis plus independent source, parser and
call-destination review. KI-027 remains open; no FPS/audio fix is claimed.

## Fixed artifact and scope

The accepted 0.16.0.1 host / SDK 0.18.0.1 stayed unchanged. Full identities
and the accepted 12.63 gameplay / 8.63 pause outputs/s comparison are in
[descriptor evidence](M7-016-deck-descriptor-reuse.md). Paired Devkit SSH
enabled unprivileged profiling without installation, sudo, sysctl, affinity,
clock, Proton-setting or save changes. The owner was explicitly freed to play
or close normally after the two phase-confirmed captures; do not leave future
instructions saying the owner must remain paused.

Private recordings under private/evidence/M7-016 and the bundle diagnostics:

| Capture | Scope | Accepted raw samples |
|---|---|---:|
| cpu-20260905T175611Z-93337 | Owner-confirmed settled pause, 20 seconds | 4,249 |
| cpu-20260905T181559Z-93337 | Owner-confirmed moving gameplay, 20 seconds | 6,878 |
| cpu-gpu-stack-20260905T183135Z-93337 | GPU Commands thread only, 8 seconds; phase unconfirmed | 573 |

All use task-clock:u at 99 Hz, no inheritance, kernel samples or build-id
cache writes. The first two record leaf IP/TID/time/period only. Every exported
sample matches the raw perf record multiset; exact image hashes and process /
existing-thread lifetimes match, with no lost/throttled records. Pause has
70 threads at both ends. Gameplay starts with 71 and gains one explicitly
excluded non-inherited thread; this is not complete coverage of that new TID.

The final recording adds bounded private 4096-byte user stacks and registers
for one exact existing GPU thread. These may contain process data and must
never be published. Its original sample_lines field counts 775 nonblank
decoded lines, including callchain lines; **573 is the raw sample count**.
The reusable collector was corrected without rewriting the original evidence.
The [perf manual](https://man7.org/linux/man-pages/man1/perf-record.1.html)
documents per-thread scope and bounded DWARF stack recording. This capture
is not a phase-matched performance comparison or a validated whole callgraph.

## Exact symbol validation

- All 30,034 guest/native mappings in the actual host PE match the generated
  registration table. Its own .pdata bounds determine attribution; gaps are
  not assigned the nearest guest name.
- Existing SDK objects were relinked only into a private offline-symbols
  directory, never over developer/published outputs. Every .text and .pdata
  byte, section RVA/size and preferred base matches each exact shipped DLL.
  Only those verified maps supply SDK function names; older maps are invalid.
- The installed Wine ucrtbase and Unix ntdll files were hash-copied privately,
  matching the recorded device/inode mappings. Exact ucrt disassembly and
  SDK call/thunk/import bytes independently validate the narrow copy callers.

Private helpers and reports: analyze-leaf-profile-20260905.py,
recover-current-sdk-maps.ps1, analyze-gpu-copy-callers.py, each capture's
analysis.json or copy-callers.json, and offline-symbols-0.18.0.1. These are
exact-capture tools, not a general supported profiler or distributable assets.

## Findings, not a single-cause claim

The busiest guest thread has 1,758 pause / 1,700 gameplay samples. Its shared
delay helper accounts for 971 / 768; the actual PAUSE-loop/comparison block
accounts for 921 / 715, or 52.39% / 42.06% of that thread's sampled userspace
CPU. Other gameplay guest threads have 1,694 and 636 samples, and the main
guest has 517. Work is already distributed; aggregate CPU below 100% does
not demonstrate spare capacity on the critical producer/consumer path.

Static producer analysis identifies a command-completion wait whose marker
is written by the GPU Commands CPU worker when it reaches PM4 EVENT_WRITE_SHD.
That handler directly updates physical memory, without a Vulkan fence wait.
However the delay helper has six different callers: completion sequence,
buffer marker, ring read pointer, another queue and per-slot markers. Its
progress/watchdog checks also matter. **Leaf samples do not prove every wait
came through the completion-sequence caller.** No global wait removal,
WaitOnAddress replacement without notifications, or affinity-only fix is safe
on this evidence.

GPU Commands has 1,441 pause / 1,419 gameplay samples. In gameplay, exact
SDK leaf samples include UpdateBindings 77, base WriteRegister 75 and Vulkan
WriteRegister 53; external module samples include ucrt 222, RADV 203 and
Unix ntdll 197. These are leaf CPU residency, not inclusive function budgets,
call counts, blocked wall time or active GPU execution time.

The narrow caller capture resolves all 75 samples in one exact SSE2 forward
copy loop through the proven return slot, own SDK function bounds, direct
call destination, memcpy thunk and import:

| Verified caller | Loop samples | In-flight copy sizes represented |
|---|---:|---:|
| IssueCopy_ReadbackResolvePath | 47 | 64 KiB to 5 MiB |
| VulkanSharedMemory::UploadRanges | 26 | 4 KiB to 444 KiB |
| UpdateBindings | 2 | 768 bytes |

This identifies large resolve readbacks and uploads as real CPU work.
It does not measure invocation frequency or total transferred bytes. Zero
pause resolve **waits** never meant zero resolve **copies**. Existing nested
copy/byte counters should be used in the next matched control rather than
adding another generic counter set.

An uncached-readback allocation bug is **not established**: the inspected SDK
already prefers supported HOST_VISIBLE + HOST_CACHED memory for readbacks.
Actual Deck allocation type/flags were not captured; fallback remains unknown.
Disabling readbacks globally is not a shippable optimization: title-side
consumers, including the physically repaired Photo Mode, need those bytes.

## Review, limits and next bounded experiment

Independent review reproduced the 573/75 result and verified the copy size
reconstruction. Primary hardened the parser to verify the exact memcpy thunk
and IAT target, then reran successfully. Root also reviewed and tested the
separate [unfinished-F10 launcher correction](M7-016-launcher-diagnostic-partial.md).
AST scan / 3 rule tests, 52 path-policy checks, native recovery, bundle
1 positive / 13 negatives / 41 contracts, and 46 lineage checks pass;
PowerShell fixtures pass on both 7 and 5.1. No SDK source change or codegen.

Limitations: no adjacent sampling-overhead control, no raw live code-byte
snapshot, incomplete caller coverage outside the exact copy loop, kernel
work excluded and no matched Xenia baseline. One gameplay thread also has
substantial kernel ticks; their cause is not identified by userspace samples.
Do not promise a projected FPS uplift or claim that one optimization will
recover stock 30 FPS.

Next, after the current game exits normally and its full return is archived,
use a short, same-scene readback-cost A/B as **diagnostic-only**: keep stock
clock, save, resolution, Proton and other policies fixed; measure new outputs,
existing resolve-copy bytes/time, upload/protection activity and audio. An
altered readback policy needs explicit non-shipping labeling and visual/Photo
Mode checks, not a silent default change. Then choose between selective/lazy
readback research and remaining command/protection work based on the result.
The current live session must not be killed or reseeded for that experiment.
No additional unchanged four-F10 run or long soak is required.

### Selected control: separate-process fast versus some

Source review selects the existing valid launch values `readback_resolve=fast`
and `readback_resolve=some`, not a new runtime binary or a global off switch.
In SDK src/graphics/vulkan/command_processor.cpp:4464--4771 both modes retain
the actual resolve, readback allocation, GPU transfer/downscale, barriers and
double-buffer alternation. Once slots are warm, `some` suppresses host mapping
invalidation and the CPU memcpy into guest RAM. That raw physical view bypasses
virtual-alias write watches; it does not directly exercise their fault handlers.
Stale guest contents can indirectly change consumers. This is narrower than disabling
the whole readback path, but still not a pure memcpy-only timing subtraction.

The `some` miss condition tests missing/undersized/unmapped slots, **not content
freshness**. CPU data can remain stale indefinitely. Any profile produced by
the diagnostic arm stays isolated and must never replace owner progress or
be accepted as campaign/Photo Mode coverage. Warm each arm separately, keep
equivalent seed/cache/scene settings, compare counter deltas and repeat A if
scene or thermal drift could explain a change. Treat altered visuals, scene
mismatch, copy counters that do not change, or unresolved cold-cache work as
an invalid or inconclusive control, not an optimization PASS.

Run through a separately rooted developer harness only after normal closure
and full-profile archive. The portable launcher intentionally rejects arbitrary
runtime flags. Direct launch must set game/user/cache/update/metadata/log and
diagnostic roots explicitly, use independent full-profile copies, and retain
the original bundle hashes. Do not modify its immutable config or Steam entry.
Audit the staged mcla.toml: it loads after startup argument parsing, and its
portable log/diagnostic paths are relative to the arm's working directory.
Verify the actually resolved roots and mode rather than trusting arguments.
Direct launch does not get the portable launcher's save watcher; keep the
archived original immutable and every experimental output isolated.
`--readback_resolve=none` is not a reliable current-host control: OnPostLoadGraphicsPlugin
tests value-versus-default and resets the SDK default `none` to `fast`.
`auto` is not an allowed value and must not be used as an accidental bypass.

No live F4/console mutation: although tagged hot-reloadable, the cvar's plain
string storage is read by the GPU worker without the setter's registry lock
or a demonstrated worker handoff (cvar.h:315--324, core/cvar.cpp:271--291).
Separate launches avoid introducing that additional race into the experiment.
This was the reviewed protocol. The later [executed A-B-A control](M7-016-deck-readback-cost.md)
does not establish a robust material gain and is not a shipping fix.
True read-demand/lazy coherence is separate work: PhysicalHeap::TriggerCallbacks
currently explicitly rejects read watches (SDK system/xmemory.cpp:1985--1988).

M7 remains 0/16. KI-027 usable stock cadence/audio, KI-028 evolved successor
reload and the remaining physical platform matrix are unclosed. New chat is
optional at this retained-evidence boundary, not a performance PASS.
