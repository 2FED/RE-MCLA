# M7-016: 0.13.0.0 Deck return and command/memory attribution

Date: 2026-09-05. KI-027 remains open. This is a bounded performance
investigation, not a completed Deck or campaign gate.

## Accepted return

The owner reports unchanged 2--10 FPS but the Steam overlay now follows that
actual cadence instead of reporting 60--90 FPS. This physically confirms the
presentation-counter correction, not a throughput or audio correction.

Bundle: `mcla-0.13.0.0-win64-2E813D203DF298CF-626A6D06CD5B9347-58B3385C89D9E499`,
MCLA 0.13.0.0 / SDK v0.14.0.0. Both complete F10 packages are verified:
`live-20260905T044355Z-332-1` and `live-20260905T044532Z-332-2`.
Their manifest timestamps delimit 96 seconds; directory names reflect the
request rather than the final manifest time. The shutdown `.partial` is
not a third accepted F10.

`session-20260905T044215Z-292` ended normally (exit 0), checked all 25
immutable files and recorded one save snapshot with zero watcher errors.
The full mutable return was validated and archived before any replacement:

`private/evidence/M7-016/returned-bundles/<bundle>/33C82BF70DF845F487A63B3E362909146D0712E7C71A903D48203914E5466EC4`.

The final save remains
`58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585`;
header remains
`2FAEFBC7FF8CDEADBCE025D9BE914436ED05E27D041F72C8C88798612CFB0D60`.
No owner progress was replaced. The normal session verifier passes without
the separate `RequireDiagnosticsProbe` mode requirement.

## Measurement

| Counter / interval | Result | Interpretation limit |
|---|---:|---|
| New output sequence | 663 / 96 s = 6.91/s | Not simulation ticks or proof of distinct visible content |
| Guest vblank | 5,793 / 96 s = 60.34/s | Clock remains active; does not establish simulation health |
| Frames opened / closed | 683 / 683 | Includes 20 asynchronous-placeholder skips |
| Output refresh failures / missing source | 0 / 0 | Not a general render-correctness pass |
| Frame wall | 96.142 s | 140.764 ms per closed frame |
| Pipeline-end + replay + queue submission | 5.72% of frame wall | Does not cover most CPU preparation or every GPU wait |
| Opening-frame fence wait | 0.007496 s | Not all native fence waits |
| Command ring idle | 0.007641 s | Completed idle episodes only |
| WAIT_REG_MEM | 5.105293 s | Nested within command execution |
| Handled non-MMIO write faults | 52,909 (551/s) | Includes watches already cleared by another thread |
| Handled fault aggregate time | 17.879002 s | Includes lock/query/callback time across threads; excludes earlier OS/Wine dispatch |

CPU deltas for the busiest threads are 70.20 s (guest thread 500), 67.73 s
(GPU Commands 380), 65.53 s (guest 564), and 53.49 s (guest 532).
Guest 564 has 41.41 s kernel CPU. Aggregate host utilization cannot prove
that GPU frequency or one CPU core is the bottleneck.

## Source and dump corroboration

Private offline relinking reconstructed linker maps for mcla.exe,
rexgpu-xenos.dll and rexruntime.dll. Each accepted map's executable
`.text` section hashes identically to the returned binary. This operation
did not rebuild or replace the tested bundle. Maps/dump payloads remain
private.

In the second instantaneous sample the command-thread stack contains
candidate return addresses in physical watch arming and native protection;
another guest thread contains callback/invalidation candidates. The first
sample also contains shared-memory request/transfer candidates. Wine's
unwind is incomplete: raw stack words are leads, not validated call chains
or a statistical profile. Nearest-symbol matches outside code are excluded.

Source review identifies the following unanswered costs:

- GPU watch arming performs native protection outside the old fault timer.
  Each physical alias is intentional; skipping aliases is not a safe fix.
- The fault timer begins before the global memory mutex; it mixes contention,
  protection queries and invalidation callbacks. These require separate
  counters before choosing a remedy.
- An already-watched gap splits a protection run. Coalescing may reduce
  syscalls, but only if such fragmentation is actually frequent. No patch
  may cross guest read-only/NoAccess/provider boundaries.
- Draw preparation, nested commands and interrupt guest execution are outside
  the narrow replay/submission spans. Native fence calls also occur outside
  the opening-frame wait.
- Guest sub-millisecond delays become zero-duration host yields. Ordinary
  uncontended critical-section release has no unconditional syscall. Neither
  finding authorizes changing guest scheduling without frequency evidence.

An independent primitive-cache overlap predicate also deserves a separate
coherency regression fixture; it is not established as KI-027's cause and is
not changed in this experiment.

## Next experiment and decision rules

The narrow diagnostic addition separates completed primary-buffer, draw,
swap and interrupt spans, actual Vulkan fence-call wall time, and physical
watch arm/disarm calls/time. Fault lock/query/callback and already-cleared
counts distinguish contention from invalidation work. A safe-gap split
counter determines whether protection-run coalescing has useful opportunity.
All are fixed-size, address-free, relaxed cumulative counters. No packet,
page, input or asset journal is added. Overlapping and nested times are
never added together or subtracted as exclusive frame stages.

1. High arm/disarm time plus frequent safe-gap splits: implement and test
   bounded run coalescing, then compare the same route.
2. High fault-lock or watch-arming-lock time with low query/callback time:
   examine lock ownership,
   not exception suppression or expanded unprotection.
3. High native fence-call time: isolate the corresponding draw/readback wait;
   do not disable correctness-required readback.
4. High draw/primary time with low protection/fence time: subdivide only the
   implicated preparation path, using the exact binaries and existing maps.
5. Large guest CPU/yield cost remains a separate scheduling lead. Do not
   shorten waits or change the 30-FPS guest timing to inflate output counts.

## Implemented candidate and verification

MCLA **0.14.0.0** pins SDK **v0.15.0.0**, commit
`a88eefcec058975bc01be54ab7a8ee424ce73630`. The 21 new fields flow from owned
SDK snapshots to F10 JSON and the pair analyzer. Old packages with all new
fields absent remain readable; partial, negative or regressing sets fail
closed. Counts/times are relaxed atomics, not a transactional snapshot.

Independent command, memory/coherency and publisher reviews found no blockers.
The primary agent reviewed integration and ran:

- SDK RelWithDebInfo and tagged Release: **1,817 PASS, four known BitStream
  skips, zero failures** per configuration (1,821 registered). Eight new
  cases cover nested/early-return/unwinding timers and real protected writes,
  physical aliases, concurrent writes and safe/unsafe gap classification.
- Parent AST scan clean / 3 fixtures; SDK AST clean / 1 fixture.
- Analyzer: 4 positive / 8 negative; diagnostics: 1 positive / 7 negative /
  116 source checks, on PowerShell 7 and Windows PowerShell 5.1.
- Portable bundle: 1 positive / 13 negative / 41 source checks on both
  PowerShell hosts, including in-place source mutation after materialized
  runtime copying. Save-lineage: 46 isolated checks; session: 1 positive /
  10 negative; build matrix: 1 positive / 6 negative. Bootstrap: 12/12.

Actual Windows Vulkan control used an isolated complete copy of the returned
profile and a separately built pre-version-bump 0.13.4.0 host with SDK 0.15.0.0.
Keyboard pulses did not leave the title screen; the existing opt-in START
request did. The agent visually observed saved gameplay (car and HUD), then
took `live-20260905T053753Z-33076-1` and `live-20260905T053901Z-33076-2`.
All twelve manifest artifacts hash-check, all 21 counters are nonnegative and
monotonic, and native protection failures stay zero. The car was stationary;
the interval includes the AFK camera, not driving. It yielded 2,024 new
outputs / 69 seconds = 29.33/s. Native watch arming reports approximately
0.593 s for 1,049,091 calls; disarming 0.197 s for 498,416 calls; fault-lock
wait 0.01165 s; arming-lock wait 0.04687 s; native fence calls 0.08973 s.
Each completed span is truncated to integer microseconds before accumulation;
many native protection calls are sub-microsecond, so these sums undercount
wall cost and are not unbiased high-precision estimates. Compare call counts
alongside time and account for per-call quantization. These establish functioning cost attribution,
not comparable Deck hardware/route or a physical performance PASS. The run
was closed with Alt+F4; its log records normal window-close termination.
Private reproducible check/result: `deck-return-20260905-0.13.0.0/verify-windows-control.ps1`
and `windows-control-result.json` below `private/evidence/M7-016`.

## Publication and save preservation

Clean publication and relocated native diagnostics/save-session proof pass:
`private/evidence/M7-016/bundle-20260905-054033-dae9f0b0/result.json`.

- Child: `mcla-0.14.0.0-win64-92EA61AE9EFC1DF8-BEF9742FB0FFF9D4-58B3385C89D9E499`.
- Manifest SHA-256: `CA5C1FEEBC4BE7ACE5956489BD606167438F008CE8DCCC0262E016242D787CA3`.
- Immutable-lock SHA-256: `AFD02379252AB1A490C224C36E8D78018EECAC56983E0F4D28722288173F5DB0`.
- All 25 immutable files verify; final relocated guest-free F10 has all 21
  new scalar fields zero-initialized. Relocated session/diagnostics exit 0.
- Seed source is the accepted 0.13.0.0 return, not an M6 fallback. Complete
  profile SHA-256: `3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A`.
  Save/header retain the identities above; photo-album files are included.

Review found the old publisher could hardlink developer runtime binaries into
the supposedly immutable child. The new publisher builds in the dedicated
`out/build/win-amd64-portable-release` tree and always materializes all six
runtime/launcher files. Read-only NTFS link checks confirm each final binary
has only one name. Game-file hardlinks remain optional. No old developer
output was deleted or rewritten during the isolated builds. The old bundle
payload was pruned only after the full mutable return/profile archive was
validated, retaining just the current child in sync; archived returns remain
recoverable outside that sync folder.

Next owner action: synchronize the new child completely, keep Proton 9.0-4
and Desktop Mode/settings unchanged, verify the progressed save, take F10
READY in gameplay, drive for about 60 seconds, take a second F10 READY, close
normally and let sync finish. No long soak or wheel test. This comparison is
needed to choose between the explicit memory/lock/fence/preparation branches
above; the Windows control cannot measure Wine/RADV overhead.

No throughput fix or new Deck physical PASS is claimed. The active investigation
stays in the existing chat as requested; move to a new task at its clean
outcome, with the durable handoff updated.
