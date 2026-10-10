# M7-016: Type-0 control on fixed Proton 10

Date: 2026-09-06. Primary return/visual and independent analyzer reviews pass.
This is a paused-scene control, not gameplay/audio acceptance.

## Question and protocol

The earlier [Proton9 control](M7-016-type0-constant-batching.md) reduced sampled
Type-0 command work without establishing a repeatable cadence gain. The
[runtime comparison](M7-016-proton-runtime-comparison.md) subsequently selected
fixed official Proton10; its [residual profile](M7-016-proton10-residual-profile.md)
still assigns75/887 leaves to the scalar-write/Type-0 target group. This justifies
one fresh OFF/ON/OFF, without rebuilding, adding a profiler or changing policy.

All three arms use the exact parent0.18.0.0/dirty-SDK19 experimental candidate,
official app-local CRT, installed Proton10.0-4b, Sniper, FSync,1280x720 windowed
Vulkan and readback `fast`. Each normal Proton `run` initializes a fresh private
prefix and complete copied four-file seed/cache. Only the launch-cached
`gpu_type0_constant_batch` flag changes. No experimental save is promoted.
Normal bundle/current.txt, shortcut, owner prefix/profile/cache and installed
runtimes pass preservation checks. No clocks, affinity or global setting change.

Primary and independent prelaunch review accept the observer/collector diff.
The observer retains gameplay-before-pause and pause-before module snapshots;
it does not race title teardown with an end-window snapshot. No profiler runs.
The shared fingerprint excludes the deliberately varied flag; exact ordered
arms, launch arguments, individual flag receipts and actual log markers bind
OFF/ON/OFF. All other launch arguments agree after normalizing per-arm roots.

Helper6099E7B245F500E2F2E1B89BF79AAC1FE0B959ECF3AD5A708483BC596113DEFC;
collector5822442AA16C70CAB192FE011D47AC5E3418638D40E8EF22563C7844BC7388C9;
runtime manifest8222680D3B6ABB3422B1EFB32CFBB5122FE17F178B9E370A8F784B5EA0F746C3;
fingerprint8A3A4ECDF6F062736BC2993441256FCB69EB23AA0B891D6C297CF5FAAF9A9AA4.

## Returned measurements

| Arm | Outputs | Interval seconds | Outputs/s | Draws/output | Readback MB/output | Binding ms/output |
|---|---:|---:|---:|---:|---:|---:|
| OFF |276|20.000047|13.80|5302.95|37.039|8.641|
| ON |292|20.000004|14.60|5218.53|37.044|8.310|
| OFF repeat |272|20.000006|13.60|5320.04|37.068|8.675|

Readback MB uses decimal bytes/1,000,000. Arm/disarm cost is6.512/2.810,
6.482/2.887 and6.531/2.826ms/output; resolve-copy cost4.756/4.513/4.769.
Resolve waits, protection failures and metadata-lock misses are zero. Nested
timers overlap and must not be added as an exclusive frame budget.

All arms log the same RADV VANGOGH/Vulkan1.4.330/Mesa26.0.0-devel git9cc9241790
and FSync. Exact11-image loaded checks agree at both observer phases. Title
PID/birth pairs are125020/161863348,125366/161879353 and125697/161895289.
Normal exit0 at158.165/158.616/158.100 seconds; no force, cleanup errors,
survivors or source drift. All runtime/SSH sessions ended.

Primary inspected all nine saved-gameplay/pause frames: the same Eclipse,
location, HUD and expected PAUSE menu are present. Body highlights, traffic
and small scene details vary; images are not pixel-identical. These frames
do not establish temporal rendering, motion, controller or audio correctness.

ON is5.8–7.4% above the two OFF rates; the OFF spread is about1.5%. However,
ON also has1.6–1.9% fewer draws per output. The result supports a modest
candidate benefit on fixed Proton10, with a workload qualification; it does
not isolate the entire delta to batching or establish repeatable gameplay
speedup. Keep the source defaultOFF and normal publication unchanged. The
next acceptance evidence must include actual driving/audio and persisted
progress, rather than another identical paused-scene series.

## Reproduction and limits

Private return: private/evidence/M7-016/proton10-type0-return-20260906,
67 bounded regular files/68,195,910bytes. Collector excludes copied caches
and fresh prefixes before traversal; archive member hashes are checked before
and after extraction. Original visual_validation_pending receipts remain
immutable; primary review is recorded above.

ArchiveB646B4AA4002B96E2554580ED94DECC25320751B9018E868F5269BD301142156;
inventory7CB9F73C9A76074A83D8625C4CA6EF74F502BF36CB9D2EDC184C1CB10700A923.
Private analyzer `analyze-proton10-type0-control.py`:
A54DAEFF2FFA4F0392725931F240C15091702FB98E1D88729577314A147FEB87.
Output `proton10-type0-analysis.json`:
23FBBB239B2C340DB9705A17B02E7D5919F7FE284E0F851FC3487A8B44BD11D6.
The analyzer rechecks archive/inventory/all files, lifecycle/pins/flags,
normalized launch equivalence, samples and all frame hashes before emitting
counter deltas. It cannot turn a pause proxy into gameplay or GamingMode PASS.
Independent archive/member review and raw counter subtraction reproduce all
rates/deltas, and the analyzer output exactly matches the retained JSON.

No source/build/version change, SDK promotion, task closure or push. M7 remains
0/16; stock cadence/audio, successor save lineage and the remaining physical
platform matrix stay open.

## Completed owner driving follow-up

The isolated Proton10/Type0-ON session has now completed. The owner reports a
slight subjective improvement, still approximately10–17 FPS and intermittent
audio/music stutter and crackle. Normal exit0,11 exact loaded images and the
16-file private return are verified; the full terminal seed is unchanged.
See [owner driving evidence](M7-016-proton10-owner-driving.md) for exact helper,
launch, archive/inventory identities, observation limits and next disposition.
This is a stock gameplay/audio NO-GO; no M7-009 successor gate was exercised.
