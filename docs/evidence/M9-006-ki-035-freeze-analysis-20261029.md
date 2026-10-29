# KI-035 garage-transition freeze: analysis (M9-006)

Read-only analysis by an Opus subagent on 2026-10-29 from the two F10 packages
and their full runtime logs, reviewed and accepted as evidence by the primary
agent. Resolved cdb stack outputs are retained in
`private/ki035-analysis-20261029/`. The diagnostic in
[M9-006-ki-035-diagnostic-proposal-20261029.diff](M9-006-ki-035-diagnostic-proposal-20261029.diff)
is neither compiled nor applied yet. No fix is claimed.

Date: 2026-10-29. Read-only analysis. No game or helper launched; nothing in
the repository or SDK was modified. Packages are named by id only.

## Inputs

| Input | What was used |
| --- | --- |
| F10 `live-20261025T150147Z-11828-1` (l17, v0.19.0.0) | manifest, state.json, log-tail, frame.bmp (downsampled view), minidump via cdb |
| F10 `live-20261025T153821Z-16284-1` (l18, v0.19.0.0) | same |
| Second F10s `...150201Z-11828-2.partial`, `...153838Z-16284-2.partial` | directory timestamps only (both empty) |
| Full runtime logs for both processes (`runtime-20261025T145326Z-11828.log`, `runtime-20261025T153122Z-16284.log`) in the same diagnostics install folders | Not used in the earlier KI-035 analysis. They continue past the F10 and hold the F10 performance-audit output. |
| Guest code | `generated/default` (sub_82306230, 82305E38, 82131008, 822C0980, 82414078), relinked `mcla-map.map` (l13-build) to resolve dump frames |
| SDK source | `private/l34-src/third_party/rexglue-sdk` (memory imports, `GuestFunctionScope`, `RecordGuestImport`, frame audit) |

Save files and `save-private` were not opened. No existing evidence report
analyses these dumps beyond the KI-035 row and `M8-001-timebox-20261025.md`
(l17 log findings and l17b fix). This report extends that work and does not
repeat it.

## Key finding: the CP and render worker were not stuck

Each F10 armed the SDK frame audit (`REX_PERF_AUDIT_*`). It logs to the full
runtime log, not to the package `log-tail.txt`, which stops before the audit
finishes. Results during the freeze:

| | l17 freeze | l18 freeze |
| --- | ---: | ---: |
| audit samples | 300/300, PASS | 300/300, PASS |
| guest frames | 14197 to 14496, no gaps | 12308 to 12607, no gaps |
| wall span | 10.00 s (30.0 fps) | 10.00 s (30.0 fps) |
| CPU frame p50 / p95 / max | 32.9 / 35.0 / 65.5 ms | 32.7 / 35.3 / 69.2 ms |
| GPU frame p50 / p95 / max | 33.4 / 37.4 / 64.9 ms | 33.2 / 37.2 / 66.9 ms |
| streaming reads in 10 s | 149 (5.18 MB, burst at samples 61-71), 0 stalls | 2 (256 KB), 0 stalls |

In both freezes the game kept producing and presenting frames at the 30 fps
cap, with normal frame costs, for at least 10 s after the F10. Other evidence
agrees:

- **Video memory:** in l18 it kept growing (2966 MB at 17:37:22, 3248 MB at
  17:38:22, after the freeze began).
- **Swap counts:** `guest_output_sequence` equals `command_swap_packet_count`
  in both (14195 and 12306), about 28-29 swaps/s over the whole process life.
- **Second F10 `.partial`:** this is a shutdown artefact, not a hang. Both were
  queued 2-4 ms after `Window closing, shutting down...` (17:02:01.036/.040 and
  17:38:38.968/.970), and the process hard-exited about 20 ms later. The KI
  statement "a second F10 stayed .partial" does not indicate a stuck
  diagnostics path.

The suspected CP/render-worker livelock is refuted. The freeze is a game-level
state that does not advance while the frame loop runs normally.

## Re-reading the minidumps

Frames were resolved with the relinked map (RVA nearest-symbol; ICF can blur
single frames).

- **Render worker F80004E0** (l18):
  `82306230 -> 82305E38 -> 821C3048 -> 82131008 -> 822C0980 -> 822E6408 ->
  823078D8 -> 8260EBB8/8260DA68/8260D678 -> 82194B58 -> 82187BA8 ->
  8218AD48 -> 82189FB8 -> 8218CC70 -> 82414078`.
  - `82305E38` is the render-thread loop. It waits on an event (`821C90C0`),
    renders a frame (`8217C658`/`82131008`) and signals completion.
  - `82131008` is the per-frame scene render (`822C0980`, then `822BF208`).
  - `sub_82414078` is **not a copy routine**. It is a tiny D3D render-state
    setter: it stores the argument at device+0x2E64, merges bits into a shadow
    register at device+0x2934 and sets dirty flags at device+0x10.
  - The top frames (`VCRUNTIME140!__uncaught_exceptions`) are the epilogue of
    the SDK `GuestFunctionScope`, which runs in every recompiled function.
  - This is ordinary scene submission.
  - In l17 the same thread was in a write fault on watched memory during scene
    render. At about 1.9-2.0 k faults/s for the whole session (961 994 in
    500 s; 830 424 in 419 s), that is also routine.
- **Thread CPU** (state.json): the render worker used 95.8 % (l17) and 94.9 %
  (l18) of wall time over the whole process life. "Actively running" is its
  normal state and cannot mark a freeze. GPU Commands used 59 % and 48 %.
- **Main XThread:**
  - l18 waits in `821C90C0 <- 823057E8 <- 82305D00 <- 82131508 <- 82131790 <-
    821BD7C0`. That is the main loop's per-frame handshake with the render
    thread.
  - l17 is running game update code (`826FA8A8 <- ... <- 822C1FA8 <- 82131508`).
  - Both are normal frame-loop states.
- **All other guest threads** sit in their top-level idle waits in both dumps,
  identical between l17 and l18:
  - job workers `821C4528`
  - pool `824E6A80`
  - `821BC910`, `82466940`, `82370760`, `8246C8A0`, `82768740`, `822ECE88`,
    `826D9448`
  - KeWait in `8242FB88` and `823EC8C8`
  - KeDelayExecution in `82460270`

  None is blocked mid-operation in a loader or I/O call.
- **GPU Commands:** the frames are only partly resolvable with the available
  PDB (version mismatch). The recorded `IssueDraw > RequestTextures > ...`
  matches a normal draw.

Per-minute GPU statistics ("frame pacing/min", "draws per frame/min", and so
on) are not emitted by v0.19, so there is no pre-freeze minute to compare.
Session averages: 4 329 draws/frame (l18) and 5 435 (l17).

## Freeze image

The l18 F10 frame (BitBlt, fullscreen) shows the garage exterior at the screen
edges and a large solid-black rectangle covering most of the view, with a small
HUD element top right. The l17 frame (windowed BitBlt) is almost entirely
black. That is consistent with the game holding a transition overlay or panel
while it waits to switch to the garage scene.

## Ranked hypotheses

1. **CP/render-worker livelock (write-watch ping-pong, scratch-buffer retry,
   resolve never completing). Refuted, high confidence.** Frames advanced at
   30.0 fps with normal CPU and GPU cost for 10 s in both freezes. Faults,
   draws and the render worker's CPU share are at whole-session norms. No GPU
   code path needs a fix for KI-035 on this evidence.
2. **Garage-entry state machine polls a condition that never becomes true.
   High confidence as a class.** It explains a static transition with live
   rendering, all workers idle, and no host lock. Sub-cases, unresolved:
   - a. **Silent guest allocation failure. Medium-low.**
     - Mechanism: `MmAllocatePhysicalMemoryEx` returns 0 and
       `NtAllocateVirtualMemory` returns `X_STATUS_NO_MEMORY` with no log line
       (`xboxkrnl_memory.cpp`, failure branches after `AllocRange`/`Alloc`).
       A garage asset load that cannot get memory could retry or wait each
       frame.
     - For: the freeze is intermittent after long free-roam, and the
       emulator's physical heap fragmentation can differ from hardware.
     - Against: the l18 session was only 7 minutes.
     - Status: currently untestable, because the path is silent.
   - b. **Lost or never-issued streaming request. Medium-low.**
     - For: the l18 loader was nearly idle during the freeze (2 reads).
     - Against: no NtCreateFile failures, no stream stalls, and the l17 loader
       still served a 5 MB burst, so the I/O path works.
   - c. **Game polls a GPU-produced value** (fence, query or resolve readback
     for the transition panel) **that stops updating for one resource.
     Low-medium.** The CP is otherwise healthy, and the black panel is
     suggestive only.
   - d. **Time-driven transition stuck on a timer delta. Low.** The owner
     reports the freeze pre-dates the raw timer, but that is from memory.
3. **Texture-budget monitor. Already ruled out** (KI row, l18 with no
   adjustment).

## Decisive next diagnostic

- **Analyse what exists first:** after every KI-035 F10, read the full
  `runtime-*.log` in the diagnostics install folder, not only `log-tail.txt`.
  It shows whether frames advance (`REX_PERF_AUDIT_SAMPLE` /
  `SUMMARY`).
- **Owner procedure, existing build:** press F10 once, wait at least 15 s (the
  audit needs 10 s), then optionally press F10 again and wait for the "snapshot
  complete" log line (well under 1 s) before closing. Closing the window right
  after F10 produces the empty `.partial`.
- **Instrumented build** (`ki-035-proposal.diff`, diagnostic only, uncompiled):
  - **Part 1, SDK `src/kernel/xboxkrnl/xboxkrnl_memory.cpp`:** a rate-limited
    `REXKRNL_IMPORT_FAIL` on `MmAllocatePhysicalMemoryEx` and
    `NtAllocateVirtualMemory` failure (first 32, then every 256th; size, page
    size, alignment, range, protect).
    - A FAILED line during the freeze confirms 2a.
    - None rules it out.
  - **Part 2, `src/mcla_app.cpp`:** the F10 bind (next to the existing
    `ArmFrameAuditForDiagnostics` call) starts a 10 s, ~500 Hz sampler.
    - It reads each guest thread's `PPCContext::current_function` and
      `last_import`, maintained by the SDK's `GuestFunctionScope` and
      `RecordGuestImport`.
    - It logs one `MCLA_GUEST_SAMPLE` line per thread with the top 12 guest
      functions and the import histogram.
    - `OnWindowCloseRequested` stops it.
  - **Owner protocol:** one F10 in normal driving near a garage (control),
    and one during the freeze.
  - **Reading the result:** the Main XThread functions that appear only in the
    freeze sample, plus any import repeated every frame (allocation, file query,
    XAM content), identify the polled condition. It can then be traced in
    `generated/default`.

## Unresolved risks

- **Sampler is a sketch:** it is uncompiled and untested. It reads another
  thread's `PPCContext` without synchronisation (aligned 32-bit and pointer
  reads; diagnostic only). It relies on `object_ref<XThread>` keeping
  `thread_state()` alive for 10 s; check this against the exit path before
  use.
- **Function-level resolution only:** `current_function` is the innermost guest
  function. A polling state machine that does little work per frame may be
  under-sampled. The import histogram mitigates this.
- **Frame resolution is approximate:** dump frames were resolved with
  nearest-symbol RVA lookup on a relinked map, with ICF folding. Single frames
  can be mislabelled; whole chains and thread states are consistent across
  both dumps.
- **No pre-freeze baseline:** the freeze start time is unknown and v0.19 has no
  per-minute GPU statistics.
- **Intermittent repro:** still needs owner reproduction; no 100 % repro is
  known.
- **Documentation corrections the primary should consider** for the KI-035
  row:
  - "copy routine" should read "D3D render-state setter".
  - Add that frames kept advancing during the freeze.
  - The second `.partial` is a shutdown artefact.
