# M7-016: physical backing and Proton reserved-section overhead

2026-09-05. KI-027 / M7-016 remain open. This follows the
[command/memory experiment](M7-016-deck-command-memory-cost.md).

## Accepted 0.14.0.0 return

SDK v0.15.0.0; bundle
`mcla-0.14.0.0-win64-92EA61AE9EFC1DF8-BEF9742FB0FFF9D4-58B3385C89D9E499`.
Complete F10s `live-20260905T064756Z-336-1` and
`live-20260905T064936Z-336-2` pass artifact hashes and same-process analysis.
The shutdown `.partial` is not accepted as a third capture.
Session `session-20260905T064606Z-296` passes: exit 0, 25 immutable files,
one profile snapshot, zero watcher errors. Full profile remains
`3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A`;
save remains `58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585`.
No progress or photo album was replaced. Complete mutable return archive:
`private/evidence/M7-016/returned-bundles/<bundle>/87F1C924AAE8198DB3A94FD1170A35CC79588A78B980C43C3D638F4D8A2F0679`.

| Measured 100-second interval | Value |
|---|---:|
| New outputs / closed frames | 917 (9.17/s) / 925 |
| Frame wall / draw-packet wall | 99.070 s / 77.315 s |
| Actual native fence calls | 5,577 / 0.247 s |
| Watch arming native calls | 473,423 / 33.733 s |
| Watch disarming native calls | 224,370 / 14.380 s |
| Handled write faults | 57,371 / 19.535 s |
| Fault lock / query / callback | 0.639 / 3.846 / 14.973 s |
| Watch arming lock | 1.251 s |
| Safe arm-gap splits / protection failures | 54 / 0 |

These spans overlap/nest and aggregate different threads; do not add them
into exclusive frame percentages. Integer-microsecond per-call truncation
understates very short native calls. This is not a matched-route speedup
versus the previous 6.91/s run. Only 54 gaps among 473,423 calls rejects
arm-gap coalescing as the main fix. Native fence waits are not dominant here.

## Source-supported mechanism and bounded correction

[Proton 9.0-4](https://github.com/ValveSoftware/Proton/tree/proton-9.0-4)
pins Wine `117daf8d508860a8552f59ebdd6ee16361431136` (verified through its
public submodule metadata). In that exact
[virtual-memory implementation](https://github.com/ValveSoftware/wine/blob/117daf8d508860a8552f59ebdd6ee16361431136/dlls/ntdll/unix/virtual.c#L2581),
`get_committed_size` asks wineserver about every SEC_RESERVE range. Protection
and basic-information queries call it under the virtual-memory mutex;
MEM_COMMIT does not remove the section's SEC_RESERVE flag. A pagefile-backed
SEC_COMMIT section instead uses local committed-page metadata while still
applying page protection. This establishes an avoidable synchronous request,
not proof that it accounts for every measured microsecond or all low FPS.

SDK **v0.16.0.0**, commit `c949866149be57585751cbe0eeccaf09743576bb`, uses
one separate Windows SEC_COMMIT section for the already-precommitted 512 MiB
physical RAM. All five views (7F writeback, A/C/E aliases, raw physical)
share it; offsets are normalized before the existing granularity adjustment.
Virtual backing remains SEC_RESERVE, including the 8/9 XEX alias. POSIX keeps
its previous mapping layout. Final committed capacity is unchanged.
Guest allocation metadata, permissions, native watch calls, fault handling,
GPU invalidation/readback and guest timing are not bypassed or relaxed.
Partial mapping retry now clears unmapped pointers and stops on cleanup failure.

## Review and tests

Primary review plus independent mapping/coherency review found no blockers.
Primary independently checked exact Wine source/gitlink; stale line anchors
from the research handoff were corrected before this report. No upstream
post or patch was sent.

- Native committed-section recommit/protection fixture: 12 assertions passed
  before changing runtime. Three old-runtime baseline cases: 50 assertions.
- Final five native mapping cases: 79 assertions, covering every alias,
  E bounds, virtual reservation/XEX alias, independent host protections,
  guest RO/NoAccess, actual protected-write/rearm, partial collision cleanup
  and repeated initialization/destruction. Existing concurrent fault tests remain.
- RelWithDebInfo and Release all-target builds and CTest: **1,822 PASS,
  four known BitStream skips, zero failures** (1,826 registered) each.
  Tagged Release install/CTest repeats the same result. SDK AST 1/1.
- Initial Release CTest used a stale PPC runner after a unit-only build and
  failed. Rebuilding all targets corrected the binary/header ABI mismatch;
  both full suites were rerun. SDK README-AI now requires all-target builds
  before full CTest. Actual allocation/unmap API failure is not injected;
  the collision test exercises successful partial cleanup.

Detailed generated logs remain under
`private/evidence/M7-016/deck-return-20260905-0.14.0.0`.

## Acceptance boundary

MCLA **0.14.1.0** consumes the exact SDK correction. Publication/native control
is recorded below. Its Deck return now confirms reduced per-call physical
memory overhead, **not recovered playable throughput or stable audio**.
Do not repeat this completed owner test. No long soak or wheel work. Stay in
this chat until this investigation reaches a clean outcome. M7 remains 0/16;
no milestone push.

## Accepted 0.14.1.0 return and residual bottleneck

Complete F10s `live-20260905T083803Z-336-1` and
`live-20260905T083943Z-336-2` pass hashes and same-process analysis (100 seconds).
`live-20260905T084007Z-336-3.partial` is a shutdown artifact, not a third capture.
Session `session-20260905T083540Z-296` passes: exit 0, 25 immutable files,
one profile snapshot, zero watcher errors. The complete profile, save, header
and photo album are unchanged. Full mutable return archive state:
`57DD284A930E75452F0073F8F7DA79AA33C18591D2DFC4B253C678658F776EFB`, under
`private/evidence/M7-016/returned-bundles/<0.14.1 bundle>/`.
Analysis: `private/evidence/M7-016/deck-return-20260905-0.14.1.0/analysis.json`.

| Same-duration returned intervals | 0.14.0.0 | 0.14.1.0 |
|---|---:|---:|
| New guest outputs / second | 9.17 | 12.13 |
| Closed frames | 925 | 1,219 |
| Draw packets / inclusive wall | 2,962,008 / 77.315 s | 6,199,135 / 63.474 s |
| Native watch arm calls / wall | 473,423 / 33.733 s | 896,601 / 14.833 s |
| Mean native arm / disarm | 71.25 / 64.09 us | 16.54 / 16.95 us |
| Native disarm calls / wall | 224,370 / 14.380 s | 336,432 / 5.703 s |
| Handled faults / inclusive wall | 57,371 / 19.535 s | 92,659 / 7.117 s |
| Fault query wall / mean | 3.846 s / 67.04 us | 0.152 s / 1.64 us |
| Actual native Vulkan fences | 5,577 / 0.247 s | 7,313 / 0.339 s |
| Safe arm gaps / protection failures | 54 / 0 | 148 / 0 |

Owner reports possibly 10-15 rather than 5-7 FPS, still poor performance.
Measured output cadence is consistent with that observation. The routes and
draw workload differ substantially; **12.13/9.17 is not a controlled 32% A/B
speedup claim**. The reduction in per-call arm/disarm/query cost supports the
targeted backing correction. Nested/multithread spans must not be added as
exclusive frame costs, and per-call integer-us truncation understates fast calls.

Residual GPUCommands CPU is 84.78 CPU-seconds, versus 90.88 for the busiest
guest thread in this 100-second interval. Native protection still totals many
calls, but counters combine all watch users; they do not identify the primitive
cache's share. Next bounded investigation: cache correctness and eligibility
for small converted index buffers. No coherency/protection bypass is authorized.

The owner's temporary Steam FPS spike while the F10 notification is visible
is explained by `ImGuiDrawer::Render` requesting another UI paint while a dialog
is attached. This re-presents an existing guest image; it is not new game work.
`DiagnosticStatusDialog` hides/detaches outside CAPTURING/READY/FAILED, retaining
the earlier idle-overlay correction. Guest output sequence/refresh-success
counters exclude these UI-only presents. This cosmetic spike is not a new
performance regression or evidence that the game is rendering at 60 FPS.

## Publication and native control

Clean publication/Windows relocation **PASS**:
`private/evidence/M7-016/bundle-20260905-073828-79d51fc3/result.json`.
Child: `mcla-0.14.1.0-win64-ED21B43D461B2D00-D7F0EE25CF209E77-58B3385C89D9E499`.
Manifest SHA-256 `A52C47196C0A29FC586A04E5AAE83699A2CF4B037191368CA6220FB46DFCC9D2`;
lock SHA-256 `D6718DF9D7A4B6A0E001331C629F9CC4742F133AFDD92E943CF73D91C3075D76`.
All 25 immutable files and the actual complete returned profile were verified;
the relocated guest-free diagnostic/session probe exited 0. The old payload
was removed from sync only after its full mutable return was hash-archived.

The exact published binary ran separately with an isolated archived profile.
Native UI tooling failed sandbox initialization, so existing opt-in controller
START/capture requests were used. Primary visually inspected the actual renderer
artifacts: title after first START; saved car/HUD after second START; working
pause; then resumed/AFK-camera rendering. The reused garage checkpoint names
do **not** claim purchases, garage traversal or that lifecycle suite's PASS.
No physical F10 keypress, native gameplay F10 pair or Deck throughput claim.
The normal frame viewer was unavailable too; bounded previews were decoded
from those existing BMP artifacts, not captured through alternate desktop APIs.

Control PID 37896 was explicitly terminated after inspection because the native
UI close route was unavailable. Exit -1 is deliberate test cleanup, **not** a
spontaneous game crash or graceful-exit PASS. Profile hash remained unchanged.
The standalone bin launch emitted its configured relative logs/diagnostics in
bin/; these control-only outputs were moved to the isolated evidence directory,
and the published bundle/launcher hashes were reverified afterwards. The normal
portable launcher and relocated diagnostics route retain sibling-root evidence.
Control logs include expected VFS negative probes/unmapped optional paths;
do not claim an entirely error-free log.

Parent gates: AST scan/3 fixtures; both PS7 and PS5.1 analyzer 4 positive/8
negative, diagnostics 1/7/116 source checks, bundle 1/13/41 source checks,
save-lineage 46 checks, session 1/10, build matrix 1/6; bootstrap 12/12.
Independent root review found an old current-SDK regex in the historical alpha
verifier; corrected to 0.16.0.0 without changing the historical tested alpha.
The actual PS5.1 path then exposed two unsupported ordinal Contains overloads;
equivalent ordinal IndexOf checks preserve fail-closed matching on .NET Framework.
Actual non-Fixture alpha verification and 1-positive/31-negative fixtures now
pass on both PS7 and PS5.1. These developer-script fixes do not change binaries.
