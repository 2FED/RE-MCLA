# Current state and handoff

Updated October 29, 2026 Kyiv. README-AI owns the milestone ledger and checkpoint history.

## Active outcome

**M9 at 2026-10-29: M9-001/002/003 closed, M9-004 deferred to REL-019, M9-005 staged and waiting on the owner, M9-006 and M9-009 in progress.** Ledger: README-AI section 17; plan: [M9 priorities](M9-priorities.md).

- **M9-005 volunteer candidate l38** (`private/l38-volunteer`, host `9777D5BE...`, launcher `009BC3B1...`, V2 kill-switch ticket created 2026-10-28 19:19 Kyiv) is staged and audited. 2026-10-29: a clean 18-file copy was made at `C:\BDU\MCLA-Test\MCLA-R-l38` (all receipt hashes and sizes match; volunteer audit 183 checks, 0 findings; no `C:\BDU` or profile path strings). Offline window ends 2026-10-31 19:19 Kyiv; online start is refused until `l38-active-builds.txt` is published to `DebugBundle` (owner yes required). **Owner-only next steps:** in-game smoke (two F10 presses in different scenes for KI-042 freshness, report prompt, kill switch), the publication decision, one tester smoke. [Evidence](evidence/M9-005-volunteer-build-20261028.md).
- **M9-009 (hide cursor on pad/wheel input)**: implemented host-only and committed (b96ce01): SDL event watch, pure state machine (unit test 266 checks), CVar `mcla_cursor_autohide`; `mcla.exe` compiled in `private/l39-cursor-build` (`f4d568e6...`), not staged or launched. Owner physical check pending. [Evidence](evidence/M9-009-cursor-hide-20261029.md).
- **M9-006 KI triage (2026-10-29, analyses accepted, no fix claimed)**:
  - KI-040 pad re-acquire: SDK slot re-binding is correct; vendored SDL 3.5.0 re-scans XInput only after HID arrival notifications. Fix attempt in progress by an Opus subagent: patched SDL poll in an isolated SDK copy `private/l39-sdk-src` (local commit only), SDK build/tests, host build in `private/l39-ki040-build` for owner testing. [Analysis](evidence/M9-006-ki-040-sdl-rescan-20261029.md).
  - KI-035 garage freeze: livelock theory withdrawn; frames present at a steady 30 fps during the freeze, so the garage logic waits on a condition that never comes. Next: diagnostic (SDK allocation-failure warning, F10 guest-thread sampler), not yet implemented. [Analysis](evidence/M9-006-ki-035-freeze-analysis-20261029.md).
  - KI-026 camera softlock: the UILogic camera-paused flag set by the warp object explains dead camera input with a drivable car; two stuck patterns ranked. Next: always-on `guest_ki026` F10 block plus an off-by-default probe; watchdog only after a confirmed occurrence. [Analysis](evidence/M9-006-ki-026-camera-analysis-20261029.md).
- **M9-007 menu state**: the game's own pause object (`*(be32*)0x828CCEA0`, byte flags +4/+6/+10, applied each frame by `sub_822D29E8`) found by static analysis; host-only hook proposal recorded (`mcla_ui_cap_guest_pause`), not yet implemented. [Analysis](evidence/M9-007-menu-state-analysis-20261029.md).
- **Serialisation**: host edits for M9-007/KI-035/KI-026 wait until the KI-040 host build from this worktree finishes; then one Opus implementer builds them into a new private directory. No game run in this continuation.
- Working rule from the owner (2026-10-29): the primary (Fable) coordinates; Opus subagents execute tasks; Fable executes only when an Opus attempt visibly fails.
- Direct `mcla.exe` launches must pass `--d3d12_adapter=1` (RTX 5060 is not the first DXGI adapter at 4K). No game run in this continuation.

M8 is closed by owner decisions and the bounded M8-012 regression (2026-10-28): 10 complete, 9 deferred, 19 total, GREEN. M8-006, M8-007 and M8-018 move to the start of M9. Details are in the [closure record](evidence/M8-closure-readiness-20261028.md) and the [performance and timing timebox](evidence/M8-001-timebox-20261025.md).

## Builds (private, owner PC)

- **Daily: `private/l31-s30`.** Stock 30 FPS. It adds the raw measured-time timer (smoothing, whole-millisecond steps, hitch debt), streaming replay, `clear_memory_page_state=false`, bulk Type-0 register writes, light readback (`some`) and the DXGI-budget texture cache. The launcher Build folder selects it; `C:\BDU\MCLA-Play` holds the shortcuts and README.
- **Experimental unlocked: `private/l33-sunc`.** Guest vblank 240 Hz, draw-count UI cap, 45 s start delay, tear-free present.
- **Fallbacks:** `l27-s30`, `l20`, `l12` (stock).
- **Launcher:** the owner uses the l12 launcher binary. M9-002 (done, owner smoke passed): the repository now builds it. The l12 source was uncommitted work in the main working tree, now ported. The rebuilt launcher has byte-identical code, and `private/l35-s30` / `l35-sunc` carry it. M9-003 (done, owner-checked): the launcher has a Frame rate choice (30 FPS / Unlocked, experimental), so one build (`private/l36`) serves both modes and replaces the two shortcut folders.

## Where the work lives

- **RE-MCLA source** (this repository): the raw timer (`src/mcla_raw_timer.cpp`), the texture budget, the timer resolution and `mcla_app` readback `some`.
- **SDK work:** published on the fork branch `mcla/mcla-r-hotfixes` (`678adc4` 32bpp, `a239b83` M8 CP work) and pinned in the submodule since M9-001. The private `sdk-l18-s` copy and the timebox patch scripts are history, not build inputs.
- **Logged statistics** (once a minute, in the GPU plugin): frame pacing, real GPU busy time, replay and write-watch statistics, and draws per frame.

## Known issues to carry

KI-013, KI-026, KI-032, KI-033, KI-035, KI-036, KI-037 (first photo and save preview), KI-038, KI-041 (night light flicker on unlocked) and KI-040 (gamepad drop with a 2.4 GHz dongle, first seen 28.10). Unlocked-mode limits:

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
