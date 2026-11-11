# l88 PC run 1 (2026-11-11): tearing report and cadence counters

Read-only analysis by an Opus subagent, accepted by the primary agent. Spark not used: deterministic log parsing and source reading.

**Headline.** Owner, l88 pc/default, 1080p Unlocked, VSync on, MAIN save (2026-11-11, session ffa60ded): (1) KI-074 flicker again near the end of the run; (2) new: tearing from the start of gameplay. Tearing: the presenter cannot tear (tearing cvar forced off, FLIP_DISCARD, Present(0) without ALLOW_TEARING; both logs print "presentation tearing disabled"), SDK 7cbfe3b does not touch presentation, no captured guest frame has a tear line; the only difference from l86 and ~25 earlier PC sessions is the display mode recorded by the game: window 2560x1440 fullscreen instead of 3840x2160. Likely external (driver vsync override / VRR / display path on that mode); owner check pending. Flicker: the new cadence counters do NOT support the fenced-delivery hypothesis: unfenced cadence copies are a steady ~0.5 per frame in quiet minutes too, the flicker cluster at 08:33-08:34 is no different, and the spikes (08:27-08:28, 08:32) do not coincide with F10 presses.

---

# l88 PC session ffa60ded (2026-11-11 08:20-08:34 local), read-only analysis

- Build D17C1E6B... = l88 candidate-receipt build_manifest_sha256 (match). Dev overrides: same 12 values as the l86 session (arm pc/default; no fenced key).
- capabilities.txt / launch-receipt.txt / managed.toml identical to l86 except the build hash (unlocked, vsync 1, 1920x1080 RT, guest 960x540, scale 2, fullscreen 0 at launch then double-click fullscreen).
- DIFFERENCE: every F10 report.txt says `window 2560x1440 fullscreen`; l86 and all ~25 earlier PC sessions with packages say 3840x2160. Initial windowed swap chain 1440x810 (l86: 1200x675) -> desktop/display was 2560x1440, i.e. a different display or desktop mode.
- Startup identical: "MCLA unlocked frame rate: presentation tearing disabled" in both (d3d12_allow_variable_refresh_rate_and_tearing forced false, graphics_system.cpp:186-193).
- Presenter: FLIP_DISCARD, ALLOW_TEARING flag only if that cvar (d3d12_presenter.cpp:419-426); Present(0, DXGI_PRESENT_RESTART [+ALLOW_TEARING only if flag]) (d3d12_presenter.cpp:1176-1178). Sync interval is always 0; without ALLOW_TEARING DXGI flip model does not tear (queues to next vblank, drops stale). The `vsync` cvar only drives the guest vblank timer (graphics_system.cpp:211-212).
- SDK 7cbfe3b touches only command_processor.{h,cpp}, readback_pending.h, readback_slots.h + tests; no presenter / swap / src/ui. Default arm: planner identical (fenced_cadence=false), extra GetCompletedValue poll (local var only, submission_completed_ untouched), FIFO TrackCompletion + OldestUndeliveredSubmission under readback_pending_ mutex, counters. No change to Present, swap, submission ordering or fence stores.
- Perf identical to l86: ~2600-2750 frames/min (~44 fps), gpu_busy 54-58 s/min, present_ms ~0.8/frame.
- frame.bmp = guest output capture ("D3D12 guest capture: success sequence=N last_presented_sequence=N"); 10 packages viewed (frames/): no horizontal discontinuity in any. One earlier press (08:27:03) fell back to window capture; that package was retention-deleted (packages 1-5 gone).
- Cadence counters: see cad.py output. k8 unfenced ~0.5/frame every minute (steady), cad_unfenced_served == cad_unfenced always; f36/depth/other spikes in 08:27-08:28 and 08:32 with lead2_frames 68-221 / f2p up to 1158; flicker cluster 08:33:37-08:34:24 had unf 0.51-0.96/frame, f36 0-19, depth 0-6, lead2 0-2 -> no correlation. fence_before_data ~10.2/frame constant (saturated, uninformative in default arm).
