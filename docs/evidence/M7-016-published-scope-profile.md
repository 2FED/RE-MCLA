# M7-016 residual profile after published scopes

2026-09-06. M7 remains0/16. This is a separate sampled CPU run of the
[published-scope candidate](M7-016-guest-scope-candidate.md), not another clean
cadence measurement. The bracketing clean series remains18.45/18.80/18.55 outputs/s.

## Capture and provenance

Private root: `private/evidence/M7-016/guest-scope-profile-20260906`.
The reviewed helper uses the existing E54C42E5... host/package09BAE976... in place,
exact Proton10, Type0ON, fast readback,1280×720 and the unchanged full seed/cache.
Only six small helper/manifest files,118,402bytes, were staged. Preparation
10A5519C... launched no title. Capture76441 subsequently completed normally in
178.3766seconds, with no forced cleanup, survivors or source-identity drift.

The99Hz userspace task-clock recording requests12seconds and records2,599 samples.
Raw/text/header records agree. Recording wall time is12.107224s; raw sample span
11.984077s; CPU snapshot bracket12.323476s. Recording launched2.581935s after
observed BEGIN and completed5.410951s before observed END, in controller time.
These observations do not bridge perf, Python and Windows-counter clock domains.

All18 sampled TIDs have the same birth identifiers in both68-thread snapshots;
no added/departed/reused TIDs were observed. The parser deliberately retains
`sample_lifetimes_fully_bound=false`. Full process maps changed, while twelve
selected image records and every sampled executable mapping match both snapshots.

All46 files /32,996,633bytes are verified and safely extracted. Both counter
phases, six frame hashes, full terminal seed and lifecycle checks pass. Primary
inspected the five gameplay/unpaused/pause frames; unpaused endpoints show the
parked Eclipse, rear camera and HUD0mph, with changing scene details.

- Archive: `ADAF4D6AEFB812C60D96682DDE80F35A52519A55FA0E98CBA272F643DC6E9729`
- Inventory: `0950689765AC61025806308BBA69A3CFFB592BAAC58FCBB96E26B4A70CE80B29`
- Return analyzer: `7F8E2BBD88AA0658CEBC1E5C995E0242D8713F91B95D537D48D304AFF68965B9`
- Analysis: `99DF81B887D78753423E05B30B5AB7D64CCB82D192687D7A0AE28139714D73B4`
- Verification: `E3EA20337128EFC5E0CF67CD0CC3DEF6CB2028F3A346EE96D32612963E51F509`

## Bounded attribution

Local package PEs are pinned to the observed file hashes. Anonymous executable
pages are assigned only through their pinned PE executable sections and stable
mapping bounds. Function names use exact own unwind-range starts; missing ranges
remain unassigned. Other system-module labels are stable mapped paths, without
an added claim that their exact local binary bytes were obtained.

| Pinned module | Leaves | Without own unwind range |
|---|---:|---:|
| Host |1485|82|
| GPU |275|26|
| Runtime |213|20|
| VCRUNTIME140 |165|0|

The VCRUNTIME leaves are159 in the exact exported `memmove` body and6 in
`memset`;156 memmove and all memset leaves occur on the GPU Commands-labelled
thread. The previous disjoint exception-count/helper group has zero samples in
this recording. Zero sampled leaves is not zero calls or an exclusive saved-time
measurement. The global exception-count import still exists in the host.

The shared guest progress-watchdog function has612 leaves,499 at exact PAUSE
instructions, all on one guest thread. Its32 pauses per invocation are followed
by progress, abort and watchdog logic; repeated polling occurs in six callers
with distinct conditions. It is not a single verified GPU-fence wait, and its
5000 counter threshold has no verified time unit. Do not bypass its return,
callbacks, fresh progress reads or caller predicates.

Other leading host ranges are the private body of `sub_82169D38` with54 leaves
and `sub_823F4450` with40. SDK symbol images have identical section bytes/layouts
and unwind ranges to the package despite timestamp differences. Exact SDK starts
identify `UpdateBindings`33, `FindOrCreateTexture`20, `VulkanTexture::GetView`13,
`RenderTargetCache::Update`13, `TimerQueue::TimerThreadMain`95,
`cvar::HasNonDefaultValue`12, `PhysicalHeap::EnableAccessCallbacks`8 and
`copy_and_swap_32_unaligned`7. These leaves are not dynamic call chains.

Independent raw-based attribution and primary reproduction agree. The leaf
validator now rejects Python optimization mode; its original report/source are
retained as v1, with unchanged results except the new analyzer hash.

- Leaf analyzer: `8BD655536BB343A0D796715E9A778226474454CFC1EDE509F2C79BE5CD9D60C5`
- Leaf analysis: `879F398E9E5C9A1292A28B41B885BC52BAC13AE8D56F97B8818DA8C173460E71`
- SDK attribution: `FC576D364792EE44D7650A3FDB579D1B9A31DBAD0E7645836A61EB3B65562A0F`

## Next bounded investigation

Attribute current GPU-thread copy callers and valid copy-size observations with
a short targeted register/stack recording after a fresh thread selector. Retain
existing waits and coherency behavior. Leaf counts alone cannot distinguish
upload from readback or justify suppressing copies. A possible future scheduler
hint is deferred pending caller-specific wait semantics and lifecycle evidence.

The owner is asleep and authorized autonomous work until20% weekly remaining or
a necessary owner gate. No new owner drive, production scope promotion, version
bump, task closure, commit or push has occurred.
