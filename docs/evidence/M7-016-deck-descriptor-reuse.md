# M7-016: returned delay-hint build and Vulkan descriptor reuse

Date: 2026-09-05. Primary owner/reviewer: Codex, with independent read-only
GPU lifetime and root schema reviews. M7 remains 0/16; KI-027 is OPEN.

## Accepted 0.15.1.0 return

Four complete F10s share PID 336 and positive creation ticks
134330871933074650. Explicit pairs are live-20260905T130741Z-336-1 /
130830Z-336-2 (gameplay, 49 seconds), then 130857Z-336-3 / 130940Z-336-4
(pause, 43 seconds). Shutdown residue 130950Z-336-5.partial is not an F10.
Both same-lifetime analyses pass. Session-20260905T130633Z-296 completed
exit 0, verified 25 immutable files, one full-profile snapshot and zero
watcher errors. Original profile/album remains unchanged (four files).

| Measurement | Gameplay | Pause |
|---|---:|---:|
| New outputs | 626 / 12.78 per second | 370 / 8.60 per second |
| Previous 0.15.0 output rate | 12.14/s | 10.02/s |
| Draw packets / closed frame | 4,421 | 9,071 |
| Guest-frame wall / closed frame | 75.96 ms | 114.93 ms |
| GPU Commands CPU, one-core basis | 81.61% | 85.21% |
| Busy guest thread, one-core basis | 90.94% | 93.07% |
| Actual Vulkan fence wall | 0.253s | 0.021s |
| Resolve readback waits | 25 / 0.248s | 0 / 0s |
| Resolve memcpy bytes / inclusive wall | 19.91 GB / 3.036s | 13.76 GB / 2.058s |

The owner-reported pause slowdown is measured. However pause packet density
rose from 7,426 to 9,071 per frame (+22%), while GPU-command CPU per packet
stayed about 10.94 vs 10.92 microseconds. This is not the same workload;
neither a causal regression nor material speedup from PAUSE is established.
Inclusive timers overlap and must not be added/subtracted as exclusive costs.
Packets can include copy dispatch. Native clocks do not measure pure GPU time.

Independent exact-bin offline inspection found two actual guest RIPs on PAUSE;
only one has a valid own-code unwind through the previously identified poll.
Four GPU-thread stacks cross missing Wine unwind metadata; none is evidence
of UpdateBindings or descriptor overhead. Eight instantaneous thread snapshots
are not a sampling profile. Do not label all guest CPU as polling.

Full mutable return archived outside Syncthing:
`private/evidence/M7-016/returned-bundles/<0.15.1 bundle id>/4D672717A35E99B9E9EEE5653F72A68AAAD9603AA772D4D7D3DCC33CB2187560`.
Analysis, profile/session validation and all new gate logs:
`private/evidence/M7-016/deck-return-20260905-0.15.1.0`.
Profile SHA-256: `3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A`.
Save: `58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585`.
Album: `8EE49926E1DFE804DB923EF2512219FC7A078FEB50944C3D2BD4E9AC39F81588`.

## Bounded candidate, not a root-cause claim

Initial MCLA 0.16.0.0 pinned SDK v0.18.0.0 /
`20b88f6625790c3679445e0358c12ac321b9c6ae`; final candidate 0.16.0.1 pins
SDK v0.18.0.1 / `9098e122afa116f4316d6cec47e561ebc5186974` for the precise
timer follow-up below. Vulkan UpdateBindings previously
allocated and wrote texture/sampler stage sets on every draw. The candidate
retains only the immediately remembered successfully written set per stage.
It compares actual post-request image views/layouts and sampler handles,
ordered counts, current descriptor handle/value validity, pipeline/stage layout,
frame and submission. A miss still allocates a fresh transient set; no issued
set is overwritten. Partial VS/PS allocation failure cannot publish snapshots.
Layout A-B-A, guest/external, frame/submission and pool-reset boundaries
invalidate reuse. Layout changes now conservatively rebind every set instead
of leaving the previous unused compatible-prefix calculation unapplied.

RequestTextures, UseSampler, uploads/barriers, residency/coherency and lifetime
marks remain unconditional. This does not bypass validation or pool completion.
No change to guest timing, PAUSE semantics, resolution, readback policy or save
handling. No cache is retained across submissions/frames.

Six new address-free counters report completed UpdateBindings calls/inclusive
time, successfully reused/written texture STAGE sets, and actual native
vkUpdateDescriptorSets calls/time (including constant descriptors). They are
relaxed, nontransactional nested totals with bounded clock/atomic overhead.
F10 serializes them; ordinary gameplay adds no per-draw file I/O. The default-
off frontend smoke emits three bounded phase samples for autonomous control.
The analyzer keeps missing legacy groups null, rejects partial/regressed/
negative and malformed types, and gives null reuse percentage for zero work.
An independent review caught pre-existing permissive integer coercion in the
shared helper; it is corrected and covered by null/bool/string/fraction/array/
oversized fixtures. This is analysis hardening, not changed old evidence.

## Review and tests

- Independent SDK lifetime/invalidation/failure review: no blockers.
- Five CPU-only snapshot cases / 37 assertions: ordered values, count/type
  distinction, null/empty, epochs, invalidation, partial publication and deep
  copy of scratch values. They do not execute renderer callbacks or inject
  actual Vulkan allocation/eviction/barrier failures; integration review matters.
- Both full all-target RelWithDebInfo/Release suites: 1,843 executed, four
  known BitStream skips (1,847 registered), zero failures. Tagged Release
  install and complete CTest also pass. Both graphics backends compile.
- SDK AST scan/one fixture and root scan/three fixtures pass.
- Independent root mapping/privacy/fixture review: coercion finding resolved.
  PowerShell 7 and 5.1 analyzer: 14 positive / 48 fail-closed; diagnostics:
  5 positive / 20 negative / 164 source checks.
- Actual non-force SDK regeneration: all 67 guest files remain byte-identical
  (133,916,451 bytes). Only tracked SDK compatibility metadata advances.

## Initial native control and timing follow-up

Isolated native Windows/Vulkan control of the exact published 0.16.0.0 binary
used a copied complete profile and the built-in default-off START/pause/Settings
probe. Primary inspection of renderer BMPs confirms saved car/HUD, PAUSE and
Settings. Exit 0, CloseMainWindow accepted, no force cleanup, source profile
unchanged. Known neon/paint/dither defects remain visible, not fixed by this
change. No physical controller, Deck, driving, audio or campaign PASS is implied.
Receipt: `native-vulkan-control/result.json` under the return evidence root.

At the gameplay snapshot 3,385,733 stage sets were reused and 3,529,340 written
(about 49% reuse); constants still caused 6,796,697 native update calls. These
are cumulative counts, not a before/after performance benchmark. There is
real avoided descriptor work, but no claim it explains the Deck CPU bottleneck.

This control exposed truncation in the new elapsed counters: rounding each
short native call down to whole microseconds undercounts millions of calls.
SDK 0.18.0.1 accumulates nanoseconds and divides the total only on snapshot;
public names/types/units and descriptor behavior are unchanged. Initial 0.16.0.0
time totals are not reliable cost estimates. Two new timing cases cover
independently measured enclosed work, short calls, early returns and unwinding;
the focused graphics-diagnostic suite passes 5 cases/53 assertions. Full
RelWithDebInfo and Release now pass 1,845 executed/four known BitStream skips
(1,849 registered), zero failures. Independent follow-up review found no blocker.

Build failures were retained, not waived: the first 0.16.0.0 compile used the
IGraphicsSystem interface for a concrete command-processor method; the existing
concrete graphics-system cast corrected it before the successful clean build.
The first 0.16.0.1 configure rejected stale installed 0.18.0.0 CMake metadata:
retagging alone did not rerun SDK configure. Explicit configure, all-target
build, full tests and install precede the clean publication retry. Failed
receipts: `bundle-20260905-135527-917c5550` and
`bundle-20260905-142938-d29bf5bd` under private/evidence/M7-016.

## Final publication and native acceptance

Clean final receipt:
`private/evidence/M7-016/bundle-20260905-144002-92771135/result.json`.
Child: `mcla-0.16.0.1-win64-B6A19E748F517476-0BCEBB4FB89A8529-58B3385C89D9E499`.
Manifest SHA-256:
`546AF7AA8256738AE37896818338A96A624A6DC73FE9ACE2333229EF04E1FAA2`.
Immutable lock SHA-256:
`0AA62A18D247FEF31D2C37B4D4C09E8C573C6AB598D7A654593406487300DBDD`.
All 25 immutable files, 15 game files / 6,569,586,392 payload bytes verify.
Relocation, native launcher, session and guest-free diagnostic gates pass,
exit 0. Source full profile remains unchanged. Intermediate 0.16.0.0 mutable
state was archived as F4D24E3B... before its obsolete payload was pruned.

The first final staging move again failed with access denied; holder unknown.
Its exact tree is retained outside sync in
`bundle-20260905-143544-618197f0/failed-staging`. Fresh full clean retry passed;
this is not a fixed publication-race claim or permission to weaken safety gates.

Exact final executable SHA-256:
`B6A19E748F5174766A61A58C3C6E3076CF262F0CB157125321BBD9776E8C6A8D`.
Separate `native-vulkan-ns-control/result.json` and
`native-d3d12-ns-control/result.json` under the return root both pass built-in
saved-gameplay/PAUSE/Settings, CloseMainWindow, exit 0, no force cleanup and
source-profile immutability. Primary visually inspected all six actual renderer
BMPs: saved Eclipse/HUD and both menus are present, not a stuck sign-in screen.
D3D12 initialization/present logs identify the backend; its new Vulkan-only
fields are default zero and its command diagnostic backend is unavailable.
Do not mistake those zeros for measured D3D12 cost. Known vehicle lighting,
neon geometry and backend-specific dithering remain visible; no visual parity
or physical controller/Deck/driving/audio PASS is inferred.

Final Vulkan cumulative gameplay sample: 6,229,160 binding calls / 3.651949s,
3,201,803 reused and 3,198,501 written stage sets (50.03% reuse),
6,145,552 native update calls / 1.427899s. At Settings: 7,257,192 binding
calls / 4.252404s and 7,155,138 native updates / 1.674085s. These nested native
wall totals now include short calls, not pure CPU/GPU time or a Deck benefit.
The number of native calls can remain high because constant descriptors still
update; texture reuse does not imply removing every vkUpdateDescriptorSets call.

Final gates after review fixes: root AST clean/3 fixtures; PowerShell 7 and
5.1 analyzer 14 positive/48 fail-closed, diagnostics 5/20/164 source checks,
bundle 1/13/41, save-lineage 46, session 1/10, build matrix 1/6, alpha 1/31
plus real non-fixture alpha validation, bootstrap 12/12. Real alpha checking
caught a stale escaped version regex, and early-init's active result producer
had an old version label; both are corrected, retaining historical tested
alpha identity. Exact published bundle/launcher re-verifies after native probes.
Logs are named `root-final-*`; post-tag SDK configure/all-target test/install
logs are `sdk-binding-ns-retag-*`. No milestone task closed or pushed.

The requested short successor comparison has now returned and is accepted
below. Do not request another identical four-F10 run.

## Accepted 0.16.0.1 Deck return: avoided work, no material cadence gain

All four completed F10s arrived after the initially incomplete sync. Gameplay
pair: live-20260905T151346Z-336-1 / 151424Z-336-2. Pause pair:
151437Z-336-3 / 151515Z-336-4. Both intervals are 38 seconds, same PID 336,
positive creation ticks 134330947656625730. Both explicit analyzer pairs pass.
Shutdown 151532Z-336-5.partial is excluded, not counted as another F10.

| Measurement | Gameplay | Pause |
|---|---:|---:|
| New outputs | 480 / 12.63 per second | 328 / 8.63 per second |
| Previous 0.15.1 output rate | 12.78/s | 8.60/s |
| Vblank cadence | 59.97/s | 60.13/s |
| Closed frames / async placeholders | 495 / 15 | 328 / 0 |
| Draw packets / closed frame | 4,090 | 9,225 |
| Guest-frame wall / closed frame | 76.014 ms | 115.457 ms |
| GPU Commands CPU, one-core basis | 81.00% | 87.08% |
| Busiest guest CPU, one-core basis | 90.61% | 94.63% |
| Completed binding calls / inclusive wall | 1,993,711 / 3.661381s | 3,003,519 / 5.210364s |
| Reused / written texture stage sets | 773,803 / 1,217,155 | 1,209,760 / 2,069,278 |
| Stage-set reuse | 38.87% | 36.89% |
| Native descriptor updates / nested wall | 1,970,941 / 1.348343s | 2,973,343 / 1.800974s |
| Actual Vulkan fence wall | 0.294923s | 0.018520s |
| Resolve readback waits / wall | 26 / 0.300423s | 0 / 0s |

The owner noticed no particular improvement. This agrees with essentially
unchanged output cadence in both pairs. Reuse is real, but this experiment
does NOT establish a material FPS/audio fix or close KI-027. Packet density
differs (previous gameplay 4,421; pause 9,071), so this is not a controlled
causal speedup/regression estimate. No pre-reuse binding-time counter exists;
the actual amount of time saved cannot be calculated from these totals.

Native updates are still numerous because constant descriptors still update.
That fact is not sufficient evidence to optimize constants next. Binding wall
includes nested operations; native-update wall is a subset, not an additional
CPU budget. Neither can be subtracted from thread CPU or frame wall to invent
an exclusive residual hotspot. GPU Commands uses 62.18 ms CPU/closed gameplay
frame versus 62.58 previously, and 100.88 ms in pause; useful work, polling and
driver work remain unattributed. Other gameplay guest threads use 67.76% and
45.24% of a logical core; this is not an application using only two cores.

Session session-20260905T151245Z-296 finished at 15:15:33Z, exit 0,
state complete, 25 verified immutable files, one profile snapshot and zero
watcher errors. Full four-file profile/album hashes remain as above. This
unchanged short return is not proof of evolved successor-save continuity.
Analysis: private/evidence/M7-016/deck-return-20260905-0.16.0.1/analysis.json.
Full mutable return is archived outside Syncthing at
private/evidence/M7-016/returned-bundles/<0.16.0.1 bundle id>/
3EAEDE327B845C041138C52D578C6A7569B630F302AA73F1CF6F8F62A492CCBF.
The current published executable, SDK, bundle pointer and source profile are
unchanged. No rebuild, pruning, new runtime probe or milestone PASS is implied.

Disposition: retain the reviewed conservative reuse policy as avoided work,
not a proven FPS optimization. Complete this bounded comparison and move to
[CPU profiling](M7-016-deck-cpu-profile-plan.md). Direct Deck capability,
pause/gameplay recordings and narrow GPU copy callers have since been verified;
see [current attribution](M7-016-deck-cpu-attribution.md). A new hardware sample
must answer a new attribution question, not repeat this completed owner test.

## Next research boundary

The owner reports Xenia at roughly 15--20 gameplay FPS on the Deck and asks
for a separate CPU/multicore optimization investigation. This is not a matched
benchmark. [The work package](../M7-priorities.md#separate-follow-up-deck-cpu-scaling--matched-xenia-baseline)
records matching scene/settings/cache, supported sampling and independent-work
design before any parallelization. Other guest threads already use CPU; two
hot threads do not mean only two cores are active. The descriptor comparison
is complete without a demonstrated gain; no speculative affinity change or
FPS promise follows. KI-027 remains open.
