# Owner F10 paint / slowdown triage — capture freshness failure

2026-10-04, stock local pilot EXE2D7A949F... / RTX5060. Owner labels the five
completed F10 packages in the already preserved Time Trial/Series return:

| Package suffix (PID42620) | Kyiv request time | Owner intent |
|---|---|---|
| 150227Z-42620-1 | 18:02:27 | Excessively sparkly/shimmering vehicle paint |
| 150244Z-42620-2 | 18:02:44 | Same paint symptom |
| 150304Z-42620-3 | 18:03:04 | Perceived FPS drop to approximately5–8 |
| 150647Z-42620-4 | 18:06:47 | Perceived FPS drop |
| 152012Z-42620-5 | 18:20:12 | Actual sparkling paint selected in garage |

Root: `private/evidence/M7-003/time-trial-series-owner-20261004-1/diagnostics-stock/live`.
Owner additionally confirms fullscreen and explicitly denies seeing the empty
black titled window that appears in the captures. Do not reinterpret the owner
report as a real overlay or game freeze. No new capture/game launch performed.

## Verified failure: images do not track the reported scenes

All30 artifacts listed across five manifests pass length/SHA verification. Main
visually inspected all five BMP files individually. Every3840x2160 BMP is the
same33177654B, SHA
`79C465E2CDB71EF895917305F9382CB6D663D13EDF652260E868933DA844B70A`.
All show Kenneth Hahn State Park Series prompt, a visibly grainy gold car and a
large black titled rectangle. None shows the owner-reported garage. The old scene
label is readable, but cannot safely identify which later event the owner won.

Read-only replay command: hash first and fifth frame.bmp, assert different for
the owner-reported hangout-to-garage scene change. Executed; exit1,
`SCENE_FRESHNESS_FAIL`. This is a deterministic contradiction in the retained
capture evidence, not a live reproduction or root-cause proof. Hash integrity
and manifest `nonuniform` classification do not establish scene freshness.

State snapshots share PID/birth and report focused/fullscreen=true; guest output
sequences advance15395,15831,16294,21340,39928. Hence identical screenshots do not
establish a frozen game. All images use `win32-client-bitblt`. Inspected source
CaptureWindowFrame uses HWND GetDC/BitBlt/GetDIBits, not renderer output readback;
its comment explicitly excludes freshness/visibility guarantees. Exact stale
surface/black-rectangle origin remains unproven. Reject this set for temporal
shimmer, contemporaneous slowdown scene and selected garage-paint comparison.

Manifest SHA pins in order:

1. `563057D3980F106CDE4DD6D3379EEEB8C5CA504D21F35330E6706BA4A7A067C6`
2. `16AEDB7A90D1981979D24191655FDACDC4B80CF08170E26CBC1C038B1589281D`
3. `E3E821FE203251810560E581C22B0D2A5D7F541B5DF45E001DC3F9EF558BD6D1`
4. `3541669910E4298F64A7DF0A72DDC67E300BFBEC1748E095FB3AA57D2CF7528A`
5. `A1E89C7A48BA432F4BB8A95A2E36EFF5FA8F95130F1D9EF7279D420A6DD0D097`

## Performance evidence boundary

Only first F10 arms the300-frame audit:18:02:27.379–18:02:39.291, samples0..299,
guest15397..15696. It ends before snapshot2 and both slowdown requests3/4.
src/mcla_app.cpp's diagnostic_performance_audit_requested_ prevents rearming
once successfully armed in that process. Later F10 packages are not new300-frame
audit windows. Summary PASS indicates audit completion, not acceptable FPS.

Cumulative guest-output differences across rounded manifest timestamps give
approximately25.65,23.15,22.53,23.12 outputs/s over17,20,224,804seconds. They are
neither local minimum FPS nor matched camera/workload measurements, and cannot
confirm or refute transient5–8FPS. Capture and audit also overlap diagnostic
overhead. Keep5–8FPS as owner perception; cause and precise depth unmeasured.

## Disposition / next useful evidence

Track paint under KI-015/M7-004/007 symptom triage without merging its cause with
KI-013 lighting or KI-020 white clipping. Deliberately selected sparkling paint
is an owner comparison reference, not itself proof of a renderer defect.
Windows slowdown is separate from historical Deck KI-027 performance evidence.
Log/state/dumps retain their individual value; no blanket package invalidation.
Owner subsequently localizes the slowdown to busy areas, such as an intersection
under the approach to a highway bridge, improving on leaving that area. This
narrows a future reproduction route but does not identify traffic, streaming,
CPU or GPU as the cause; the subjective5–8FPS estimate remains unmeasured.

Diagnosing-bugs remains at feedback-loop construction for paint and slowdown:
no temporally valid image sequence or timed audit covering the slowdown exists.
Need a fresh user-visible short clip (actual shimmer/garage) and a properly
bounded performance window at recurrence before renderer/performance changes.
No shader/gamma/LOD patch, no native rerun, no revival of deferred M8 exact WGC
timing work. This evidence supports fixing diagnostic capture separately, not
claiming a game-rendering cause or requesting repeated blind F10 captures.

Owner follow-up: garage reference screenshots can be provided later; priority
concern is F10 capture appearing broken despite earlier successful use. Record
this as current capture failure, not a proven code-regression interval: no matched
last-good/first-bad runtime/display-mode comparison has yet been made. Do not ask
the owner to replay races for lost capture context. No renderer fix authorized
by these observations; current work is diagnosis/evidence preservation.

Independent read-only performance review confirms audit0..299 ended before
slowdown3/4. Request-to-package-completion times576/277/297/324/307ms are not
measured game stalls. First audit CPU-scope median31.437ms and GPU-span
median34.0205ms overlap and are not additive; neither establishes later5–8FPS.
State backend0 is the base unknown enum, not wrong-GPU evidence (runtime logs
bind D3D12/RTX5060). Source interpretation checked in frame_audit and D3D12/base
command processor; no performance root cause established. Diagnosing-bugs blocks
speculative renderer changes pending a valid symptom-level feedback loop.

## Oct4 diagnostic engineering — not an image/performance repair claim

The actual F10 call-site policy was extracted into `DiagnosticHotkey`. Before
changing its original successful-arm latch, the compiled deterministic test
failed with `later F10 after audit completion must rearm in the same process`.
After removing the lifetime latch, every accepted snapshot request attempts SDK
audit admission. Busy/rejected snapshots do not arm an unrelated audit; SDK
retains active-audit rejection. Mock completion, active, unavailable/recovery and
busy-to-accepted paths are covered. This fixes caller admission, not measured FPS.

Package completion now displays `F10: PACKAGE READY (IMAGE UNVERIFIED)`;
`frame_signal.freshness` explicitly records `not-established`. GDI capture is
unchanged and may still be stale. The verifier accepts historical missing markers
but rejects supplied fresh/boolean claims. Test first went RED because a false
freshness claim passed, then GREEN after validator containment.

Independent source review: bounded GO. Preserve these limits:

- Queueing wakes the worker before audit admission. Early state/log-tail may
  precede the trigger; future300-frame samples belong to the process log and are
  not guaranteed to be inside that same package.
- 300 frames is a sample bound, not a wall-clock timeout during stalls.
- Caller tests use a mock SDK and do not prove physical rearm, accurate FPS,
  fullscreen overlay layout or capture freshness.

Both PowerShell editions pass diagnostic fixtures:6 positive,22 negative,
170 source checks. Frame signal pure test:24 checks PASS. Fresh CMake Release
configuration compiles both changed application translation units against the
installed Noob SDK. Scoped ast-grep scan clean;3 rule tests PASS. No SDK/source
renderer patch, game launch, owner profile writes or normal-bundle replacement.

Spark routine job `private/evidence/M7-016/f10-admission-spark-20261004-1`
(`qwen3.8-27b`,24.25s,2048-token cap, no retry): receipt artifacts and two-item
JSON schema verified. Adopted direct busy-rejection→accepted-after-completion
test suggestion. Its other proposal requires a real SDK transition rather than
the mock seam; retained as an integration-test gap, not reported as tested.
No secrets in the source packet; result is advice, not acceptance authority.

Fresh image replacement still needs a separately scoped image-only owned-window
WGC helper and physical changing-content/fullscreen proof. Do not simply invoke
Presenter::CaptureGuestOutput in the F10 worker: reviewed D3D12 implementation
holds its mailbox lock and waits indefinitely on a fence; Manager::Stop joins
the worker. An async timeout does not safely cancel that GPU work. Process-
isolated deadline/owned cleanup is the design lead, not an implemented fix.
Deferred M8 exact timestamp/cadence gates remain unchanged.

Final compiled candidate: `out/build/win-amd64-f10-check-20261004/mcla.exe`,
52,653,056B, SHA256
`47F321E80228EB0D9E6BD0EC4D3490F3DE0C8B3C65ADA85AC2645E9146C4C507`.
Fresh Release build finished all73 remaining steps and linked successfully.
Final hotkey test8 checks PASS; RGB24 PASS. SDK DLLs rehashed unchanged from pilot:
runtime570A88C7..., XenosE2EA29B7.... No VFS timing overlay configured.
No game process launched or left running. Do not launch bare build output against
implicit/default user roots. Subsequently staged the isolated package below;
this does not replace the currently working stock pilot.

### Isolated owner-ready candidate, not physical PASS

Entry: `Run F10 STOCK Candidate.cmd`, folder
`private/bundles/M7-Windows-Stock-F10-20261004-1`. Fixed stock2x/VSync/fullscreen,
fresh DXGI RTX5060 name selection, existing reviewed session mutex/five CLI roots.
One-time private staging script is retained under
`private/evidence/M7-016/prepare-f10-stock-20261004.ps1`; rerun refuses an existing
destination. Independent pre-execution/final source safety reviews GO.

Staging receipt SHA256
`F8DFD711041202850A55F92A0B84DC7261C58CA6A3057ABBC1CFD3947EEECCA6`
pins29inputs. Full profile is3763C20A... (11entries3815088B), an independent copy
of the explicit archive, equal to the original pilot before/after. No native
portable session/ancestry fabricated.15immutable game files alone hardlinked;
fsutil checks confirm each of14non-game files has one link. Old pilot, its
runtime/profile and normal0.16.0.1/B6A19E74 current pointer stay unchanged.

Launcher first rejected the deliberately unadmitted receipt; final exact receipt
pin then passed actual CheckOnly in PS5.1/7.6.5. Both shells pass23pure config
checks including candidate normalization, original/foreign/mixed-path and
forbidden-setting rejection. CheckOnly returns before config writes/game launch;
the fresh wrapper/roots are not physical startup or successor-load evidence.

Next owner action: open the new CMD, confirm progressed profile loads, press F10
twice about90seconds apart while the game advances, wait another30seconds, quit
normally and report. The300-sample audit is not wall-clock bounded; `armed=0`
while an earlier audit is active is correct, and primary must inspect logs.
Do not progress both pilot branches in parallel. Real repeated audit, successor
load/exit and fullscreen overlay remain pending. Image capture remains unfixed.

### AFK continuation: actual SDK completion/rearm

`tests/mcla_frame_audit_rearm_test.cpp` links the actual installed runtime DLL
and uses the production `DiagnosticHotkey` policy. Two complete 300-sample
cycles verify active rejection, CPU-only incomplete samples, paired CPU/GPU
completion, then rearm through the same hotkey instance. A halfway arm attempt
in each cycle also verifies that an active audit is not reset. Unavailable
timing and busy snapshot admission remain rejected.

Final result: **1809 checks, 30/30 independent process runs PASS**, with exact
stdout and exit-code acceptance. Availability and samples are synthetic; no GPU,
game, real F10 key, overlay or accurate frame-time claim follows. This closes the
real-SDK completion-state test gap, not the owner gameplay acceptance gate.

An earlier repetition exited zero with empty stdout (trial 9); it was not
accepted as a test receipt. Explicit `fflush(stdout)` makes the final test result
observable before process teardown. The empty-output root cause is not proven.
The owner-ready candidate and its staged runtime remain unchanged.

The independent reached-import audit is recorded in
[M7-014 current stock audit](M7-014-reached-imports-20261004.md).
The new opt-in image-only helper is separate work, documented in
[image isolation preparation](M7-f10-image-isolation-20261004.md). It is not
integrated into F10 or the owner-ready candidate; deferred M8 timing is untouched.

Subsequent same-day work integrates that image path only into a new default-OFF
experimental candidate3380E9E8, preserving earlier47F321E8 unchanged. For the next
owner observation use `Run F10 WGC STOCK Candidate.cmd`, not the earlier admission-
only candidate. See the linked image-isolation report for final tests, pins and
the pending two-scene/restart gate. Neither candidate is physically accepted yet.
