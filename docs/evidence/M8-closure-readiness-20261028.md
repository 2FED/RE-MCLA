# M8 closure readiness (draft, 2026-10-28)

**Status:** this is a working draft for the owner decision on closing M8 by about 2026-11-04. It is not a closure record. The ledger state comes from `README-AI` (3 complete, 6 deferred, 19 total, YELLOW at the last ledger update). The performance and timing work since 2026-10-25 is in [the timebox evidence](M8-001-timebox-20261025.md).

## What changed since the last ledger update (timebox 2026-10-25 to 2026-10-28)

All of these changes are private builds and local commits; nothing was pushed.

- **Game speed and timer.**
  - The raw measured-time timer replaces fixed 1/30 s steps, so the game no longer runs in slow motion below 30 updates per second.
  - Step smoothing and `timeBeginPeriod(1)` remove judder and the 24-updates-per-second cap.
  - The race clock no longer drifts. It accumulated whole milliseconds per update and lost the fraction; steps are now whole milliseconds with the remainder carried. Owner phone-stopwatch check: +0.26 s over 3:07 (l31).
- **CP cost** (the GPU command-processor thread; owner-tested, no artifacts):
  - Register lookup table.
  - Per-thread range buffer.
  - Light readback (`some` with periodic refresh).
  - Streaming replay of recorded D3D12 commands on a worker thread, with batched publish.
  - `clear_memory_page_state=false`, which removes per-frame re-upload of CPU pages (the owner checked vinyls, signs, HUD, map and photos).
  - Bulk Type-0 register writes.
  - Texture-cache budget from the DXGI budget.
- **Unlocked mode (experimental).**
  - A guest vblank override (240 Hz) removes the 30 FPS cap.
  - A draw-count UI cap keeps menus, the photo album, logos and loading screens at 30.
  - A 45 s start delay protects the frame-counted legal and logo screens.
  - Presentation has no tearing.
  - Owner gameplay: 55-74 fps in normal driving and about 30-40 at heavy intersections. The owner rated it the smoothest mode.
- **Measurement.** Per-minute frame pacing, real per-submission GPU busy time, replay statistics, and write-watch statistics are logged in the GPU plugin. The sampler's syscall bias (about 2.3×) was quantified.

## Item-by-item (proposal)

| Item | Current | Evidence / gap | Proposed disposition |
| --- | --- | --- | --- |
| M8-001 | open | The original target was a byte-guarded opt-in "60 FPS" route. What exists now is an unlocked mode (vblank override, raw timer, UI cap, ms-step race clock), owner-tested with a correct race timer. Stable 60 is not reached in heavy scenes. | Owner decision: accept "unlocked (experimental) mode" as M8-001's deliverable with explicit limits, or keep it open for a future milestone. |
| M8-002 | open | Verified so far: game speed and race timer in unlocked mode and at 30. Owner-visible residual effects: menus over the world (pause, Arcade over map) run fast, the slow-motion crash camera slows the race clock (likely original behaviour), and KI-037 (first photo). A traffic/AI/audio/cutscene audit at 60 is not done. | Owner decision: accept with residual limits, or defer the full audit. |
| M8-003 | done | Stock-only capability contract. | No change. The launcher still has no unlocked toggle (folder selection instead). |
| M8-004 | open | Render targets accepted functionally; KI-032 (2160p signs) and KI-038 (placeholder sign textures) open. | Owner decision: close with KI-032/038 residuals, or keep open. |
| M8-005, 008, 009, 010, 013, 014 | deferred | Unchanged. | Unchanged. |
| M8-006 | open | No new graphics options were exposed. | Defer or close as "no optional controls exposed in M8". |
| M8-007 | open | Nothing new. | Defer or close as not required. |
| M8-011 | open | Reproducible owner-route measurements with recorded build, config and power state now exist (l24-l31 runs; frame pacing, CP busy, GPU busy). | Candidate to close on the timebox evidence. |
| M8-012 | open | No bounded regression run yet on the final presets (l31-s30, l33-sunc). | Needs a bounded owner regression session before closure. |
| M8-015, 016, 019 | done | Accepted. | No change. |
| M8-017 | in progress | Fixed in this timebox: slow motion and timer, judder, allocation hitches, CP costs, race clock. Open: KI-013, 035, 036, 037, 038, 040. | Owner-reviewed residual dispositions per KI. |
| M8-018 | in progress | Volunteer export and tester acceptance remain. | Owner decision: required for M8 or moved to M9. |

## Deferred to a future milestone (already recorded)

- **Multi-threaded command processor:** the structural fix for heavy-scene drops. Heavy scenes run about 6-10 k draws per frame at about 2.5 µs of CP work per draw.
- **Launcher toggle for unlocked mode:** the repository launcher has diverged from the l12 launcher the owner uses.
- **Better menu detection** for menus drawn over the world.

## Draft tester notes (internal; not for external delivery)

**Builds:**

- **30 FPS (recommended): `l31-s30`.** Stock 30 FPS cap with correct game speed, an exact race clock, smoother frame pacing and fewer heavy drops than the earlier builds.
- **Unlocked FPS (experimental): `l33-sunc`.** No 30 FPS cap in driving, typically 55-70+ FPS, dropping to about 30-40 at heavy intersections. Menus, the photo album, logos and loading screens stay at 30.

**Known limitations, unlocked mode:**

- Pause and Arcade menus drawn over the world may run fast.
- Drops from 60+ to 30-40 feel stronger than at 30 FPS.
- The slow-motion crash camera slows the race clock (likely original behaviour).
- Rare GPU hangs: one `DEVICE_HUNG` in eight l29 title runs.

**Known issues, all builds:**

- First photo per session broken, and the save preview shows the previous photo (KI-037).
- Traffic-light glow on cars (KI-013).
- Garage transition freeze (KI-035).
- Distant pop-in (KI-036).
- Placeholder sign textures, mostly at 4K (KI-038).
- Gamepad drop with a USB 2.4 GHz dongle (KI-040, first seen 28.10).
