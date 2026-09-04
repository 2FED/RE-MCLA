# M7-016 Steam Deck performance diagnostic

## Decision

`native-vulkan-throughput-still-blocked-richer-f10-causal-trace-required`

The first causal trace used MCLA-R `0.10.0.0` with ReXGlue `v0.11.0.0`.
MCLA-R `0.11.0.0` then pinned ReXGlue `v0.12.0.0`, selected the already
compiled native Vulkan backend under Proton/Wine, and added visible F10 status.
The returned Proton 9.0-4 run proves that bypassing D3D12-to-VKD3D did not fix
the severe gameplay cadence or audio breakup.

The current diagnostic build is MCLA-R `0.12.0.0` with ReXGlue `v0.13.0.0`
commit `e66cfd2e2020e71bb001af62fedb37faab4caf68`. It adds read-only monotonic
Vulkan stage counters and native-thread CPU samples to each F10 state package.
The current private child is:

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
asynchronous placeholder skip was logged. Thirty-eight pipeline-creation
messages were present but tapered instead of continuing throughout the
capture. Native Vulkan therefore improved neither the observed order of
magnitude nor the audio behavior enough to disposition KI-027.

## Current causal capture

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

The next physical input is deliberately short: reach moving gameplay under
Proton 9.0-4, press F10 and wait for `READY`, drive for 30--60 seconds while the
slowdown and audio breakup are active, then press F10 again and wait for
`READY`. No forced GPU clock or full compatibility matrix is needed for this
capture.

## Gates

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

No MangoHUD correction, cadence improvement, or audio fix is claimed. The next
returned F10 pair is required to distinguish a fence/submission wait from
guest-side or another native-thread scheduling bottleneck before changing the
runtime path.
