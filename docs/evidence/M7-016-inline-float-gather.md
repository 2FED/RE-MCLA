# M7-016: inline Vulkan float-constant gathering

Date: 2026-09-06. Source, tests and exact optimized-code review pass.
The matched Deck control shows no cadence gain; this is not a stock gameplay/audio fix.

## Reason and change

The [fixed-Proton profile](M7-016-proton10-residual-profile.md) contains15/887
leaves in the vertex vec4 packing loop. The exact previous DLL calls imported
rex::bit_scan_forward once per selected vec4 and again to terminate each word.
This is executed work, not a15/887 recoverable frame-time budget. After the
[owner driving NO-GO](M7-016-proton10-owner-driving.md), this is a bounded
byte-preserving optimization to test before introducing a cache or more profiling.

A new inline GatherFloatConstants helper walks four uint64 masks, processing each while nonzero with
std::countr_zero and mask &= mask - 1, copying16 bytes per selected constant.
Both Vulkan call sites retain their original vertex/pixel register banks,
allocation/size, empty-map behavior, failures, dirty flags and descriptor state.
No GPU watch/readback policy, ABI, timing flag or ISA requirement is added.
The existing build already targets x86-64-v3. Type0 remains default OFF;
the performance comparison explicitly enables it in both variants.

Primary and independent reviews accept source addressing, unsigned traversal,
raw-bit preservation, unaligned output and independent scalar/component oracle.
Four tests cover656 gathers: both banks, all256 single bits, empty/full/sparse
and boundary maps, varied bitmaps, NaN/signed-zero/arbitrary words and guards.
Focused tests pass4 cases/1968 assertions. Both complete all-target Release and
RelWithDebInfo builds/suites pass1869 executed plus4 known BitStream skips
(1873 registered). SDK AST scan/one fixture and new-source formatting pass.

## Frozen candidate and binary review

Old package manifest:
FBEB08FBD1756D6A9E2452002455C20D81E4958B56FFD96CB9532004BAFB31FE.
New private package: private/evidence/M7-016/gather-package-20260906.
New manifest:
E5AA3AF42AE9A66BC74C20776FFA572C12BBD4FC2C8F902A3FD2739C42CB9634.
New GPU:
FEA57ADE855A28A00395648F4732EE71E7522F458495D2DC6D669904A2FA68D9.

Exactly one of nine files changes: rexgpu-xenos.dll. The measured host,
crash helper, rexruntime, Tracy and four official CRT files remain byte-identical.
A separate private gather-sdk-install-20260906 records the current full build;
its newly linked runtime is deliberately not substituted into the comparison.
The previous package and clean tagged SDK install remain untouched.
Parent0.18.0.0/201c3a63895a8431cc68c3217c3eb1da5631e93b and SDK19 base
c2e67161f1207ddcee609507c399efe7a1981b72 are unchanged and experimental.

Independent PE review finds identical15 import groups/360 imports/four exports,
ABI2 on both GPUs, and all136 runtime imports resolving in the retained runtime.
Offline relinked symbols have identical code/unwind bytes and layout:
.text8E6A67F2CB4DE470169E26BC26562BAD00078C3F10535AC94D5C48C5741304B4;
.pdataBAB48BA3CFDEDBBBBA9863B5F285591E5061E6278B8813C02E56B5B22431AC49.
MAP5972554A68DDC39C0F47BE4B3E4047DB75A244C8F432F9EE0F03955BCA0A38F4.
Exact UpdateBindings extent is RVA[AF5F0,B111E). All eight inner loops
(four bitmap words per bank) use tzcnt/blsr and16-byte load/store without calls.
Bit-scan imports remain for other callers. Export-only disassembler labels
misidentify this region; the MAP and .pdata provide its actual attribution.

## Runtime protocol and Windows validation

The exact mixed package passes isolated Windows Vulkan frontend smoke: saved gameplay, pause and Settings frames inspected; exit0 through normal window close, no force and complete source profile unchanged.
A separately reviewed old/gather/old protocol keeps Type0 ON, pinned official
Proton10/Sniper/FSync, identical seed/cache, fresh prefixes and no profiler.
Its shared fingerprint includes both complete packages and the ordered variant
mapping; actual mapped GPU identity is checked for every arm. Source and normal
publication preservation checks are retained. The completed Deck result follows below.

Runner4D47210677D9C2EDEBD856B0D04927A5942391CAEAE777304772ABBDFDE63379;
collectorBE2ECF79F5D7B3A475BA76F48AB07532DEA5E0F559CB9AAC57345429A1630EF7.
Primary/independent protocol review and AST parsing pass.

M7 remains0/16; KI-027 and save-successor gates are open. No version bump,
SDK promotion, task closure, commit or push. No experimental profile is promoted.

## Accepted Deck comparison

All three profiler-free Type0-ON runs on fixed Proton10 complete normally.
The shared fingerprint is
6873D0C69FBA339F4A9F6ADAB9C5446DF2D41178E9F1DBBF779EB92E20D6D3A8.
Both packages, selected variants, actual11 mapped images, source prefix,
seed/cache, normal bundle and shortcut identities are checked.

| Arm | Outputs | Seconds | Outputs/s | Draws/output | Binding ms/output |
|---|---:|---:|---:|---:|---:|
| Old |295|20.000006|14.75|5216.43|8.2394|
| Gather |293|20.000004|14.65|5213.91|7.5780|
| Old repeat |296|20.000004|14.80|5175.90|8.0831|

Binding time per output is6.2–8.0% lower, but cadence does not improve.
Draws/output vary by less than1%; resolve-copy timing is approximately
4.49/4.50/4.53ms/output. The target-cost reduction does not establish a total
frame-budget reduction or usable gameplay/audio. No further identical gather
control is justified by this result.

Private return: private/evidence/M7-016/gather-control-return-20260906.
All59 files/64,347,346 bytes pass archive/member/extraction checks.
Archive277C101E5AB84DB2D7375796E10EA7A8249FE2030AD93090DA95DC6669FCA6A3;
inventory275C23E43A477F2512562FD86C19CB85FA47AC522836DD88A6726319CD343CED.
AnalyzerE3CC310E33BD43CE2346D5E0D32476023F38F7B213946FBB08A26B3BFD21D5B6;
output33F2115FFCB86DE28B715C59D738B77DE657BE9A23E1B165FD6829B46554D284.
Independent archive/raw-delta reproduction and exact analyzer-output agreement
pass. Review strengthened the inherited analyzer: require exact11 image names,
unique raw-map headers and selected package/runtime paths. Four synthetic
negative checks reject missing GPU, substituted Tracy, wrong package path and
duplicate map headers. This was a validation gap, not a bad-capture finding.

Primary inspected all9 saved gameplay/pause frames. They show the same Eclipse,
HUD and pause scene with small lighting/scene differences, not pixel identity.
Every run exits0 without forced cleanup, survivors or source drift. Interval
resolve waits, protection failures and metadata-lock misses are zero; counters
overlap and describe completed work, not an exclusive whole-pipeline budget.

The next investigation uses the exact owner-tested old Type0-ON package for a
bounded moving-gameplay CPU profile. Owner-confirmed movement, exact thread
lifetimes and concurrent GPU/two busiest other-thread samples can identify
executed work. It cannot establish a full caller wait predicate, an exclusive
producer bottleneck, a quantitative Vulkan audio underflow count or clean FPS.
The tested gather source remains uncommitted/unpromoted. M7 stays0/16.
