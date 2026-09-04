# M7-016 Steam Deck performance diagnostic

## Decision

`desktop-return-analyzed-idle-ui-repaint-defect-confirmed-performance-unverified`

The first causal trace used MCLA-R `0.10.0.0` with ReXGlue `v0.11.0.0`.
MCLA-R `0.11.0.0` then pinned ReXGlue `v0.12.0.0`, selected the already
compiled native Vulkan backend under Proton/Wine, and added visible F10 status.
The returned Proton 9.0-4 run proves that bypassing D3D12-to-VKD3D did not fix
the severe gameplay cadence or audio breakup.

The returned diagnostic build is MCLA-R `0.12.0.0` with ReXGlue `v0.13.0.0`
commit `e66cfd2e2020e71bb001af62fedb37faab4caf68`. It adds read-only monotonic
Vulkan stage counters and native-thread CPU samples to each F10 state package.
The returned private child is:

`mcla-0.12.0.0-win64-80826022C0CEC134-8824D0B85BBA533F-58B3385C89D9E499`

Its immutable manifest SHA-256 is
`AFAB6F7EB512037A5FE4DFCBB34B29CE5D8BEA507463B13C9D9D0CF8EF672A6D`;
the 25-file hash lock SHA-256 is
`926A9223E46C486CD30D301B28B50448304614264B70B0305283F9EC906C8BB6`.
Clean build, publication, relocation, launcher verification, and guest-free
diagnostics passed in private run `bundle-20260904-185630-b120dcc6`. This is
diagnostic instrumentation, not a performance-fix claim.

## Returned Proton 9.0-4 evidence

The owner observed MangoHUD reporting 86--90 presentations per second while
static and roughly 50 while driving, while perceived gameplay was mostly 1--5
FPS and at best roughly 10. Driving also caused audio breakup. MangoHUD showed
moderate CPU/GPU utilization and initially let the GPU fall to roughly
600--900 MHz. Forcing a 1,600-MHz GPU clock did not improve perceived cadence,
so the low clock is evidence of idle GPU capacity, not the cause of the stall.

The original D3D12 same-process active-driving F10 pair measured 7.34 unique
guest outputs/s against 59.93 guest vblanks/s, or 12.24 percent. Its
guest-frame interval was 116.86 ms median and 134.46 ms p95. The bounded audit
recorded 1,152 audio underruns, two streaming stalls, five shader translations,
and six pipeline creations.

The subsequent native-Vulkan pair is:

- `live-20260904T174147Z-340-1`
- `live-20260904T174244Z-340-2`

Across its 57-second interval, the title produced 505 unique guest outputs
(8.86/s) while advancing 3,442 guest vblanks (60.39/s), or 14.67 percent.
The returned runtime selected Vulkan on RADV VANGOGH with Mesa 26. One
asynchronous placeholder skip marker was logged (the once-only message is
not a count of all skipped frames). Thirty-eight pipeline-creation
messages were present but tapered instead of continuing throughout the
capture. Native Vulkan therefore improved neither the observed order of
magnitude nor the audio behavior enough to disposition KI-027.

## Diagnostic surface introduced in 0.12.0.0

ReXGlue v0.13.0.0 exposes a read-only
`GraphicsPerformanceDiagnosticSnapshot` from the active command processor.
The Vulkan implementation accumulates, without per-frame file I/O:

- guest frames opened and closed, issue-swap calls, successful and failed
  output refreshes, asynchronous placeholder skips, and missing swap sources;
- submission count and cumulative time in frame-fence wait, pipeline-end,
  deferred command execution, queue submission, and total guest-frame wall
  span.

MCLA F10 state also records the Windows native thread ID, name when available,
base priority, and cumulative user/kernel CPU time for every process thread.
The analyzer compares two same-process packages, calculates per-frame or
per-submission Vulkan stage averages, identifies the largest measured stage,
and reports the twelve native threads with the greatest CPU-time delta. Older
F10 packages remain accepted with these new sections reported as unavailable.

## Returned Desktop Mode pair

The owner ran this child in Desktop Mode, reporting GPU utilization alternating
between 100 and 0 percent. Both F10 packages are complete and hash-valid:

- `live-20260904T190632Z-340-1`, manifest time `20260904T190633Z`;
- `live-20260904T190745Z-340-2`, manifest time `20260904T190746Z`.

The 73-second interval contains 554 successful new guest outputs (7.59/s)
and 4,355 guest vblanks (59.66/s), a 12.72 percent ratio. This is new-output
cadence, not proof of an equal number of distinct simulation updates.
The selected backend is native Vulkan on RADV VANGOGH; the window is focused
and fullscreen at 1280x800 during both snapshots.

The Vulkan delta contains 567 completed frames, 1,729 submissions, 13
placeholder skips, no failed refresh or missing source, and 72.47 seconds of
guest-frame wall spans. Recorded averages are 0.015 ms of opening-frame fence
wait per frame, 0.026 ms pipeline-end, 2.391 ms deferred execution and 0.138 ms
queue submission per submission. These measured submission stages account
for only 6.1 percent of frame wall. The opening-frame fence counter does NOT
measure all possible GPU fence waits, so the data does not exclude every
GPU-side stall.

Top threads consume 73.07, 67.73, 67.44 and 54.75 percent of one CPU core;
the 67.73-percent thread is `GPU Commands`. Another UI/DXGI thread consumes
50.62 percent of one core and is sampled inside DXGI in the second dump.
One guest thread accounts for 30.04 seconds of kernel CPU time. These samples
justify investigating idle/polling, native-call and memory-protection costs;
they do not by themselves identify one complete CPU bottleneck.

Release lacks PDBs. Private offline relinking from the exact retained object
files produced linker maps with byte-identical `.text` sections. Selected
addresses can therefore be mapped without modifying the tested binaries.
Wine stack unwinding is incomplete: raw candidate return addresses are not
treated as a reliable complete call stack. Dumps/maps remain private.

The session `session-20260904T190455Z-300` exited 0 at `19:07:57Z`, verified
all 25 immutable files, and completed one save snapshot without watcher
errors. Save SHA-256 remains
`58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585`.
The extra shutdown `.partial` is not a third accepted F10. Before any bundle
replacement, 47 returned mutable files (28,512,939 bytes) were copied and
individually hash-verified under ignored
`private/evidence/M7-016/deck-return-20260904-desktop-vulkan-0.12.0.0`.

## Confirmed idle overlay defect

Both the application F10 status dialog and the SDK achievement toast were
permanently registered with `ImGuiDrawer`, even when their draw callbacks
produced no visible content. A nonempty dialog registry attaches the drawer
to the presenter and continuously requests UI repaints, which can present
the previous guest image again. This explains an independent source of
presentation inflation and UI work; it is not evidence that correcting it
alone recovers gameplay or audio performance.

The correction detaches idle persistent dialogs, explicitly wakes accepted
F10/busy and achievement events, and detaches after the visible interval.
Worker-thread wakeups are queued to the owning UI thread with a weak lifetime
token. Pending callbacks after destruction do nothing; callers still must
retain the object while invoking its methods. Self-removal during drawing
preserves the next dialog's dispatch.

Four additional counters report completed empty-command-ring waits,
`WAIT_REG_MEM` waits, and successfully handled non-MMIO write-fault count/time.
They change no waiting, memory protection, or coherency decisions. Wait
episodes may straddle snapshots; memory fault time aggregates threads and
excludes OS exception delivery before the SDK handler. These values are
reported separately, never subtracted from frame wall.

## Prior 0.12.0.0 gates

- ReXGlue v0.13.0.0 clean dual-backend Release build/install: PASS.
- ReXGlue full RelWithDebInfo suite: 1,802 executed PASS, four known skips,
  zero failures out of 1,806 registered tests.
- Deck analyzer fixture: 2 positives / 2 fail-closed negatives PASS.
- MCLA Release compile with the new SDK diagnostic surface: PASS.
- Direct guest-free diagnostic state probe: graphics and native-thread
  sections PASS.
- Diagnostics source fixture: 1 positive / 7 fail-closed negatives / 45 source
  checks PASS.
- Aggregate bootstrap: 12/12 PASS, including the exact SDK tag/commit/install
  and all 6.57-GB game hashes.
- Real Release diagnostics/crash package run
  `20260904-185258-3ebdc3e2`: PASS.
- Clean current bundle/relocation run `bundle-20260904-185630-b120dcc6`: PASS.
- Portable bundle fixture: 1 positive / 13 fail-closed negatives / 36 source
  checks PASS.

No physical MangoHUD correction, cadence improvement, or audio fix is yet
claimed for the idle-overlay correction. M7-016 and KI-027 remain open.

## Published correction and gates

MCLA `0.13.0.0` pins ReXGlue v0.14.0.0 at
`5cc7134845504dd60ba66090880be766a58ab2bb`. Clean publication and relocation
run `bundle-20260904-195940-318397c2` passed and atomically selected:

`mcla-0.13.0.0-win64-2E813D203DF298CF-626A6D06CD5B9347-58B3385C89D9E499`

Manifest SHA-256:
`2F0F0D970DE19D153AC107E33507DD2A88E8206202344E2E4A82E14F6651A086`.
The 25-file lock SHA-256:
`B513CA66AD49F8F04F2B159C57C367CCAE9AABF48881F86995A459F1AC809B42`.
The selected save and header match the completed returned session. The old
0.12 child was pruned only after the verified evidence/profile archive.

- Independent code review: no blockers; no performance recovery claimed.
- SDK full RelWithDebInfo and tagged Release suites: each 1,809 executed
  PASS, four known skips, zero failures (1,813 registered).
- Seven new UI/memory cases: PASS; five real-drawer tests are headless and
  require no SDL window or GPU. Memory tests cover getter safety/stability,
  not generated faults or Wine exception dispatch cost.
- Parent AST scan and 3/3 fixtures; SDK scoped scan and 1/1 fixture: PASS.
- Analyzer: 3 positives / 4 fail-closed negatives, including old packages,
  incomplete/regressing wait counters and overlapping aggregate time: PASS.
- Diagnostics: 1 positive / 7 negatives / 53 source checks: PASS.
- Bundle: 1 positive / 13 negatives / 36 source checks: PASS.
- Build-matrix verifier fixtures: 1 positive / 6 negatives: PASS.
- Real live/crash packages before the application version bump:
  `20260904-195635-5ad23e9c`, live 71,500 bytes, crash 225,970 bytes, bounded
  dump flags `0x201020`, no automatic upload: PASS.
- Clean final 0.13.0.0 Release compile, hash lock, isolated relocation,
  launcher/session verification and exit-0 diagnostics: PASS.
- Bootstrap exact SDK/tag/install/tool/data checks: 12/12 PASS.
- Generated integration: unchanged 67 files / 65 C++ sources / 133,916,204
  bytes against the accepted manifest: PASS.

Next physical input is only a 30--60-second driving interval between two F10
`READY` captures on the same Proton/Desktop Mode route. Compare actual motion,
audio and the presentation overlay. Do not force GPU frequency or start a
long session to evaluate this correction.
