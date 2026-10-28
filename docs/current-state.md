# Current state and handoff

Updated October 28, 2026 Kyiv. README-AI owns the milestone ledger and checkpoint history.

## Active outcome

M8 is closed by owner decisions and the bounded M8-012 regression (2026-10-28): 10 complete, 9 deferred, 19 total, GREEN. M8-006, M8-007 and M8-018 move to the start of M9. Details are in the [closure record](evidence/M8-closure-readiness-20261028.md) and the [performance and timing timebox](evidence/M8-001-timebox-20261025.md).

## Builds (private, owner PC)

- **Daily: `private/l31-s30`.** Stock 30 FPS. It adds the raw measured-time timer (smoothing, whole-millisecond steps, hitch debt), streaming replay, `clear_memory_page_state=false`, bulk Type-0 register writes, light readback (`some`) and the DXGI-budget texture cache. The launcher Build folder selects it; `C:\BDU\MCLA-Play` holds the shortcuts and README.
- **Experimental unlocked: `private/l33-sunc`.** Guest vblank 240 Hz, draw-count UI cap, 45 s start delay, tear-free present.
- **Fallbacks:** `l27-s30`, `l20`, `l12` (stock).
- **Launcher:** the owner still uses the l12 launcher binary. The repository launcher source has diverged, so no launcher toggle was shipped.

## Where the work lives

- **RE-MCLA source** (this repository): the raw timer (`src/mcla_raw_timer.cpp`), the texture budget, the timer resolution and `mcla_app` readback `some`.
- **SDK work:** the GPU plugin and command-processor changes were made in the private copy `private/sdk-l18-s` (base rexglue-sdk `5dd58f8` plus the resolve-downscale-32bpp patch) through the patch scripts in `private/timebox-20261025-1/`. They are being committed to a rexglue-sdk feature branch; see the closure record for the branch and PR.
- **Logged statistics** (once a minute, in the GPU plugin): frame pacing, real GPU busy time, replay and write-watch statistics, and draws per frame.

## Known issues to carry

KI-013, KI-026, KI-032, KI-033, KI-035, KI-036, KI-037 (first photo and save preview), KI-038 and KI-040 (gamepad drop with a 2.4 GHz dongle, first seen 28.10). Unlocked-mode limits:

- Menus drawn over the world run fast.
- Heavy intersections drop to about 30-40 FPS.
- One `DEVICE_HUNG` in eight l29 title runs.

## Next (M9, approved 2026-10-28)

- Plan: [M9 priorities](M9-priorities.md). Order: M9-001 reproducible build on SDK `a239b83`, M9-002 launcher reconciliation, M9-003/004 mode option and Setup (ex M8-006/007), M9-005 volunteer build with kill switch and report prompt (ex M8-018), M9-006 KI triage, M9-007 menu state hook, M9-008 regression.
- M10: multi-threaded command processor. Public release hardening comes after that.

## Rules that still apply

- Never touch MAIN saves except through the owner's launcher.
- Announce every game run to the owner.
- Commit locally; push only with explicit owner approval.
