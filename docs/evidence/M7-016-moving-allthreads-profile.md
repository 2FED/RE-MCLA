# M7-016: moving gameplay, all-thread CPU profile

Date: 2026-09-06. **Capture and normal exit complete; full private return independently verified. Exact hotspot investigation continues.**

The preceding [moving-profile attempt](M7-016-moving-gameplay-profile.md)
ended normally but stopped after a three-second scout with only one exact
GPU packet-function hit. Its failed evidence is frozen and verified.
The partial data corroborates an already known shared wait; it does not
identify every caller or justify a synchronization change.

This follow-up records one12-second99Hz task-clock:u leaf window for all
title threads present at attachment. It has no sampling-dependent GPU
selection prerequisite. Before/after maps and CPU/thread snapshots are saved
before classification, and raw COMM/FORK/EXIT metadata remains available.
New, departed and reused TIDs are reported explicitly. No-inherit means
newly created threads are not assumed to be recorded.

The exact old nine-file Type0-ON candidate, Proton10/Sniper/FSync, Vulkan
readback fast,1280x720 windowed SDL and complete copied seed/cache remain
the same. Use a new isolated prefix outside the entire Syncthing root.
The prior failed session and its helpers remain immutable.

After reviewed preparation and fresh owner readiness, launch the manual
runner. Only a CURRENT owner driving confirmation triggers capture.
The owner keeps driving until recording complete, then exits normally.
No automatic input, pause, retry, title termination or save promotion.

A valid mechanical result requires ten hash-bound artifacts, the exact
record/script/header child commands and normal reaping, raw/text/header
agreement, capture policy, bracket snapshots and truthful thread coverage.
GPU/subsystem classification is performed offline. Generic Windows thread
names do not prove subsystem roles.

The record is CPU residency with profiler overhead. It does not establish
clean FPS, exclusive wall/CPU wait budgets, caller or producer attribution
from a shared wait leaf, physical continuous movement, audio underrun
counts, GamingMode acceptance or progressed-successor save lineage.

Remote preparation root:
/home/deck/MCLA-Diagnostics-20260905/moving-allthreads-20260906.

Final private helper SHA256 values:

- run-moving-allthreads.py:
  E7ADC617B9D28EC0A283C30233ADC3C85AA7E92EF0748EFDE05B4F4B4C433B4B
- capture-moving-allthreads.py:
  EEDFB15718FE208B871A6B7B0D0C5064B06810D01B7665FF3121A253E28E5D9C
- collect-moving-allthreads.py:
  B613CAE3F23A1624067FCCCEC6A17F35D749E43B0FCCCD3D2847840BE289534E

Primary and independent full capture review plus runner/collector diff
review pass. AST3 and28 completed-receipt cases/three parser negatives pass.
Actual prior822 samples validate raw/text/header/record metadata and
69 COMM records including one auxiliary record. Independent checks cover
all five sampled-TID snapshot statuses and reject missing bracket/header
artifacts. No additional synthetic runtime or game was needed for review.

Only these three helpers and the unchanged runtime manifest were deployed.
The Deck repeats28+3 pure checks, then prepare completes without a title.
A separate postcheck confirms no mcla.exe and an empty private prefix.
Three returned preparation receipts bind all9 binaries, full4-file seed,
two cache files, runtime pins and the manual single12-second contract.

- Comparison fingerprint:
  8F78298FF500205289CCA5CE563FBBC4165F8C84ACE22D46CBCBA0710E713923
- comparison-baseline.json SHA256:
  C7F413F07BD27633C3353A66E75E64340004BF9328162B23BC1F298C2C34F0A0
- preparation.json SHA256:
  6FF6846AF211ECAA2B963388A968616696D4CFA9082D689963B9B070F6A73E07
- launch.json SHA256:
  98F7633FD8C0CE15F3B59FAC90FD94A2AA95160E91F412CA5E106005B3EFCEA0

Local receipts: private/evidence/M7-016/moving-allthreads-preparation-return-20260906.
Primary independently recomputes all hashes and the semantic fingerprint.
Protected source identities remain unchanged. Fresh owner readiness was confirmed; session6444 launched wrapper130520
and reached exact runtime/active-receipt readiness. Latest live weekly allowance5% exceeds this
run's user3% stop threshold, with no reset redeemed.

After readiness, run python3 -B <root>/run-moving-allthreads.py run.
After active receipt and CURRENT driving confirmation, invoke
python3 -B <root>/capture-moving-allthreads.py --owner-confirmed-moving.
Owner keeps driving until RECORDING COMPLETE, then exits normally.
Only after normal exit run python3 -B <root>/collect-moving-allthreads.py
and return/verify/analyze before the next decision.
No new game/SDK binary is built or promoted for this capture.


## Completed session and return

Fresh readiness and current-driving confirmations preceded this capture.
Owner clarified that gameplay included stops at intersections/in traffic,
with no pause, menu or loading screen. This is owner-attested gameplay,
not continuous vehicle motion or a machine-recorded visual acceptance.

Title130698/birth162875875 exits0 normally; all manual/capture/collector
sessions are closed. No force, cleanup errors, surviving owned processes
or protected-source drift. Complete terminal4-file seed is unchanged.
Harness elapsed227.691263578s is not exact title lifetime.

- Capture receipt SHA256:
  8CBF2D8B63F5D18D9F4CAF66D902D30002DFD451F8707E02FDDF568EC98DD1D4
- Manual receipt SHA256:
  BF5981C2F57068D9B1851F2934EECEAFAF5B332F5790AFE08C5E1E85710B21FC
- Raw cpu.data SHA256:
  BADA2ED52156B214BCC70117726F48095B2CC10E83E68B43E0E01D84CF08865E
- Return archive SHA256:
  A04C78B64500C825729E492D72F2E3F7BB4D9E5DE19941CFEFD57E5D6BA7207D
- Inventory SHA256:
  8FB9CDDCA22E2C95DED732DBBA3A05BF9B1F59017A47D03D2DD5DA3444450A58

All30 regular files/6,675,980 bytes, exact helpers, preparation bindings,
capture validator and lifecycle/preservation receipts independently pass.
Primary safely retains exact members under
private/evidence/M7-016/moving-allthreads-return-20260906/verified.

## CPU findings and limits

Raw/text/header agree on4094 samples. The perf sample span is11.991234646s,
the recording child takes12.128722917s and snapshot bracket12.171116276s.
These are distinct measurements. All19 sampled TIDs have the same birth
at both snapshots;67 threads occur at each end with no observed added,
departed or reused TIDs. Raw metadata has68 COMM records, one auxiliary.
No-inherit/attachment enumeration and unseen transient threads remain
coverage limits. All4094 IPs have stable executable mappings at both
snapshots and one matching initial raw MMAP2;141 initial maps precede
the first sample. The overall map text changes, but sampled mappings do not.

| Recorded thread | Samples | User/kernel CPU seconds across snapshot bracket |
|---|---:|---:|
| XThread0220 |1025|10.28 /0.81|
| XThread0240 |994|9.87 /0.37|
| GPU Commands |817|8.09 /1.49|
| XThread0260 |437|4.47 /3.73|
| Main XThread |383|3.86 /0.06|

All stable threads total41.16 user and8.74 kernel seconds at CLK_TCK100.
Those are not exclusive wait, pipeline, GPU-execution or per-frame budgets.

The known shared guest wait has355 samples on XThread0220, including340
in its loop/comparison and289 on PAUSE instructions. This corroborates the
earlier six-caller investigation; leaf samples still do not select every
calling predicate. No wait removal or scheduler replacement is justified.

Exact GPU work includes UpdateBindings48, FindOrCreateTexture19,
official CRT memmove124, RADV153 and Unix ntdll70 (dispatcher59).
Three GPU-DLL samples also appear on XThread0220, so module membership
alone is not GPU-thread identity.

The follow-up emphasis is XThread0240:393 of994 samples in Proton10
UCRT. Generic names encode Windows IDs, not audio/physics roles. Nearby
rexruntime decode_frame19 and guest vector loops do not establish a callchain.
The exact mapped UCRT is now hash-copied privately for offline attribution:
CEC72E71FCCBB545EBD1FE96F92E9A849E726AFA76E092A4128FC193893DE16C,
1,434,926bytes, device259:8/inode7486209 matches both snapshots and the
post-session file. No new drive or source patch is selected for this step.

Exact follow-up attribution resolves these393 leaves to365 software fma,
23scalbn and5_dclass. See [software FMA and compiler gate](M7-016-software-fma.md).
No new runtime/gameplay result follows from this offline analysis.
