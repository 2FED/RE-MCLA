# M7-016: residual GPU work on fixed Proton 10

Date: 2026-09-06. Capture, return and exact function attribution reviewed and reproduced.

The [runtime comparison](M7-016-proton-runtime-comparison.md) selects installed
official Proton 10.0-4b for the next diagnostic baseline. This recording asks
what GPU-thread work remains after the older Wine commitment-scan defect is
fixed. It changes no SDK policy, clocks, affinity, synchronization setting or
normal publication. Type-0 batching remains OFF and readback remains `fast`.

## Capture and identity

The exact experimental parent 0.18.0.0 / dirty SDK19 candidate and app-local
official CRTs are unchanged. The same complete four-file owner profile and
cache are copied into a fresh private prefix, outside Syncthing. Normal
Proton `run` initializes that prefix under the installed Sniper container.
The original prefix, profile, cache, shortcut and bundle remain hash-bound
sources and pass final preservation checks.

Helper32C0B1FDB17DB82C3052B8FDF3004B8A7D9AF774959582F73CEBF9B50E8E7E15;
runtime manifest8222680D3B6ABB3422B1EFB32CFBB5122FE17F178B9E370A8F784B5EA0F746C3.
Private remote root: proton10-residual-20260906 under the existing diagnostic
root. FingerprintCF34511A93BBA2ABA11A599751429E378431311A49D6820F005ACFEE9DF0F65F.

- Title PID124321, birth161725807; GPU TID124356, birth161725868.
- Three-second all-thread scout: 625 samples; six exact Type-0/Type-3 packet
  hits uniquely select the GPU thread. Original candidate PE/pdata bounds are
  rechecked before launch; no thread-name-only selection.
- Twelve-second single-TID recording: 887 userspace `task-clock` leaves at
  99 Hz, without stacks, registers, call chains, inheritance or kernel samples.
- Scout runs at elapsed135.239–138.615 seconds; leaf recording at
  138.629–150.754, completing before the observed pause-after frame.
- Module snapshots occur at gameplay-before-pause and pause-before. Lightweight
  process/thread/maps snapshots bracket the scout and leaf recording. Text and
  perf-header rendering happen only after the title exits; raw/text tuples
  agree. No lost or throttled records were reported.
- Exit0 at158.880 seconds; no force, cleanup errors or surviving owned
  processes. Primary inspected the saved Eclipse/HUD and both PAUSE images.

This is a paused-scene CPU proxy. Exact containment in the separate counter
window is not claimed, nor is the profiled cadence a clean runtime comparison.
The two full loaded-module checks do not claim an end-of-counter-window
snapshot. Audio, motion/controller usability and GamingMode remain untested.

## Return and analysis

Collector18A2C1DAAF7EDA556340B5575C14E8919715D6EFD486480FA83CE7E477784953
excludes copied caches and the fresh prefix before traversal and verifies
actual archive-member hashes. The primary rechecks all 43 bounded regular
files, 27,221,121 bytes, before accepting extraction.

Archive9CB06ACDCE47ABF76169FF14318BA2CD463359238FF8861FAAD51E8B764F4D74;
inventory107B976A24A062158070C6A88B66A59757E1CBE07384A1762191711EC8D04675.
Local evidence: private/evidence/M7-016/proton10-residual-return-20260906.

Attribution must use the candidate's own PE/pdata and code-identical MAP
evidence, and the actual Proton10 ELF symbol table plus executable PT_LOAD
mapping. Older Proton9 offsets and profiler-reported symbols are insufficient.
Exact Unix ntdll SHA256 is
FD63E771B1C4BBE65CEA6BB5A0C672C0C686CCF1EFA5AC01CFF40319651FD7A3.
An independent ELF-table/llvm-nm check identifies its positive-size local scan
functions; zero-size data labels must not become code intervals. A separate
live read-only `getconf CLK_TCK` returns100 for any /proc tick interpretation.

## Independently checked attribution

An independent raw parser reproduces all887 raw/text tuples, exclusively the
recorded title/GPU thread, with a fixed10,101,010ns sample period and no lost
or throttled records. The scout independently reproduces625 samples across
seven TIDs and the six unique packet-thread hits. Actual ELF header/device/
inode and executable mappings agree across all four snapshots.

| Exact Proton10 Unix function | Leaf samples |
|---|---:|
| get_vprot_range_size |0|
| get_committed_size |0|
| NtProtectVirtualMemory |1|
| __wine_syscall_dispatcher |55|
| __wine_unix_call_dispatcher |9|
| NtQueryPerformanceCounter |2|
| set_vprot / set_page_vprot |1 / 1|

Unix ntdll totals69 samples. Zero sampled scan leaves does not prove zero
calls; it does remove the prior visible scan hotspot from this recording.
The dispatcher leaf alone does not identify which Windows API called it.

Module counts are GPU410, official vcruntime147, RADV129, Unix ntdll69,
rexruntime45, Windows ntdll33, vDSO23, libc13, Unix winevulkan7, msvcp5,
ucrt4 and kernel32=2, summing to887. RADV internal function names remain
unverified. Of the147 vcruntime leaves,142 belong to the exact exported
memmove range; leaf-only evidence does not recover copy callers or sizes.

The independent original-versus-recovered code/pdata/MAP check gives these
largest SDK leaves:

| Exact function | Samples |
|---|---:|
| VulkanCommandProcessor::UpdateBindings |51|
| CommandProcessor::WriteRegister |37|
| TextureCache::FindOrCreateTexture |27|
| VulkanCommandProcessor::WriteRegister |24|
| CommandProcessor::ExecutePacket(RingBuffer*) |14|
| CommandProcessor::ExecutePacketType0 |14|
| VulkanTexture::GetView |13|
| RenderTargetCache::Update |11|
| VulkanCommandProcessor::IssueDraw |11|
| cvar::HasNonDefaultValue |10|

GPU375/410 leaves and runtime37/45 resolve through own pdata; the remaining
35/8 stay unnamed. The three scalar-write/Type-0 targets total75/887 (8.46%).
That share is not a recoverable frame-time promise.

Private `analyze-proton10-residual.py` SHA256
AA4B8C410C7B96045339522D0993BBB8FAD876DEE5E9DFB6516D56268E8FF8E0
reproduces the independent counts above, with archive/member, raw/text/header,
thread lifetime, stable mapping, exact PE/pdata/MAP and ELF checks. Primary
review and reproduction pass; AST, eight pure regression checks and an
independent implementation review pass. Retained output
`proton10-residual-analysis.json` SHA256
AA3CD19B17E4AD5902A198EE43AA5F5B8F9C3650EFACB69901026AD32B6D0579.

The GPU snapshot bracket spans12.224036176 seconds with913 user and127 kernel
ticks. The separate live CLK_TCK100 check implies9.13/1.27 seconds, but it is
not archived in the capture receipt. Perf sample timestamps span11.966674
seconds in a separate clock domain; no exact counter-window containment,
exclusive pipeline time, core affinity or wait attribution follows.

The justified follow-up [Proton10 Type-0 OFF/ON/OFF](M7-016-proton10-type0-control.md)
is now complete:13.80/14.60/13.60 outputs/s, with1.6–1.9% fewer draws/output
in ON. This is a positive bounded pause signal, not full attribution or a
gameplay/audio PASS. Source defaultOFF remains; do not repeat identical pause
controls. Actual driving/audio is the next acceptance gate.

## Exact instruction follow-up

Read-only independent reviews reproduce all51 UpdateBindings and27
FindOrCreateTexture leaves from the same retained capture. Exact original
code/pdata and recovered MAP agree; every PC aligns with a decoded instruction.
Source correspondence uses control flow/constants/exact callees, not PDB lines.

- UpdateBindings vertex vec4 packing:15 leaves at GPU RVAs AFDAB(1), AFDBE(11)
  and AFDC4(3), respectively the bit-scan result test, vector store and16-byte
  destination advance.
- Nine leaves at B0AD1 follow a descriptor diagnostic `lock inc` and land on
  a values-validity OR. Sampling placement cannot assign their cost to that
  OR or quantify atomic overhead.
- FindOrCreateTexture:26/27 leaves are in the normalized-key unordered-map
  search, at4A7D3(15),4A7E1(8),4A7F9(1) and4A802(2); one at4A8CC returns an
  existing texture. Three samples lie in subsequent-node traversal, without
  proving full-hash collisions, chain lengths, lookup count or hit rate.

Existing descriptor reuse already checks ordered handles plus frame/submission/
layout context. Texture binding resets preserve upload/invalidation semantics;
do not bypass them. A recent normalized-key/view cache is only a possible
follow-up: repeated-key frequency/cost is unmeasured, so no new cache or SDK
change was selected. No task closure, version bump, promotion or push.
