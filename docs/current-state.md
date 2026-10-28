# Current state and handoff

Updated October 28, 2026 Kyiv. README-AI owns the milestone ledger and checkpoint history.

## Active outcome

**M9-001 in progress (2026-10-28).** The SDK pin is `a239b83` (package 0.20.0.2). A fresh clone built the SDK and `mcla.exe` into `private/l34-build/out-s30` and `out-sunc`. The unlocked build uses the CMake option `MCLA_UNLOCKED_VBLANK_HZ=240`, not an SDK edit. Codegen is byte-identical, SDK tests pass 1864/1864, and the sources match l31-s30/l33-sunc. Still to do: tickets and staging into `private/l34-{s30,sunc}`, which need PowerShell 7 (owner decision pending), then the owner check. Not tested in game yet. [Evidence](evidence/M9-001-reproducible-build-20261028.md).

M8 is closed by owner decisions and the bounded M8-012 regression (2026-10-28): 10 complete, 9 deferred, 19 total, GREEN. M8-006, M8-007 and M8-018 move to the start of M9. Details are in the [closure record](evidence/M8-closure-readiness-20261028.md) and the [performance and timing timebox](evidence/M8-001-timebox-20261025.md).

## Builds (private, owner PC)

- **Daily: `private/l31-s30`.** Stock 30 FPS. It adds the raw measured-time timer (smoothing, whole-millisecond steps, hitch debt), streaming replay, `clear_memory_page_state=false`, bulk Type-0 register writes, light readback (`some`) and the DXGI-budget texture cache. The launcher Build folder selects it; `C:\BDU\MCLA-Play` holds the shortcuts and README.
- **Experimental unlocked: `private/l33-sunc`.** Guest vblank 240 Hz, draw-count UI cap, 45 s start delay, tear-free present.
- **Fallbacks:** `l27-s30`, `l20`, `l12` (stock).
- **Launcher:** the owner still uses the l12 launcher binary. The repository launcher source has diverged, so no launcher toggle was shipped.

## Where the work lives

- **RE-MCLA source** (this repository): the raw timer (`src/mcla_raw_timer.cpp`), the texture budget, the timer resolution and `mcla_app` readback `some`.
- **SDK work:** published on the fork branch `mcla/mcla-r-hotfixes` (`678adc4` 32bpp, `a239b83` M8 CP work) and pinned in the submodule since M9-001. The private `sdk-l18-s` copy and the timebox patch scripts are history, not build inputs.
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
