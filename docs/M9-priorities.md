# M9 volunteer build and stabilisation

Owner approved 2026-10-28. [README-AI](../README-AI) section 17 owns the
task ledger; the earlier release draft is now section 17a there. Inputs: the
[M8 closure record](evidence/M8-closure-readiness-20261028.md), the
[performance timebox](evidence/M8-001-timebox-20261025.md),
[current state](current-state.md) and [known issues](known-issues.md).

## Decision summary

M8 closed on 2026-10-28 (10 complete, 9 deferred of 19, GREEN). The owner moved
M8-006, M8-007 and M8-018 to the start of M9 and agreed that a multi-threaded
command processor (CP) is separate work after M8.

The approved plan:

1. **M9 = a closed volunteer build plus stabilisation**, not a public release.
   M9 starts with the three carried M8 items, makes the build reproducible from
   published source, and triages the known issues a tester will meet first.
2. **M10 = multi-threaded command processor** (evidence below).
3. **Public release hardening** (the earlier M9-001 to M9-018 draft: dump
   validation, installer, clean-machine install, release tags) moves unchanged to
   a later release milestone, renumbered when it starts. It needs the
   asset-free prepare pipeline, which is a large separate piece of work.

Strategic direction stays hybrid: the recomp is the base, and selected
subsystems may be replaced by readable C++ through hooks when that is cheaper
or more reliable than fixing them in place. No full decompilation.

## Why the multi-threaded CP is M10, not M9

- **It only helps the experimental unlocked mode.** Heavy intersections draw
  about 6-10 k objects per frame at about 2.5 us of CP work each, so 15-25 ms of
  CP per frame. That fits the 33 ms budget of stock 30 (`l31-s30`, the
  recommended mode) and breaks only the 16.7 ms budget of unlocked mode.
  Volunteers get stock 30 by default, so M9's goal does not depend on it.
- **It is an architecture change, not a tuning step.** Xenia-style CP state
  (register file, shared-memory write-watch, EDRAM and texture cache, deferred
  command list) is single-threaded by design. Splitting it means ordering
  guarantees across threads, and every regression would land on the same
  rendering paths that volunteers are testing. Mixing the two would make
  volunteer reports hard to attribute.
- **The CP may not be the only wall.** The 2026-10-26 review measured the
  game's own render thread at 13-17 ms of CPU per frame. If it does not shrink
  at heavy intersections, a faster CP still will not give 60 there. This needs
  one measurement before any design work (M10-001 below).
- **Volunteer data makes M10 better.** If the volunteer diagnostics export
  carries per-minute CP-busy, GPU-busy and draws-per-frame (already logged),
  M10 starts with numbers from several CPUs instead of one laptop.

Proposed M10 opening task (recorded here so it is not lost): **M10-001**
measure, at one repeatable heavy intersection on unlocked mode, the guest render
thread, the CP thread and real GPU busy time in the same window, plus the
per-draw CP cost split (state/bindings, shared-memory requests, primitive
processing, replay). Decide the split design only after that.

## Execution order

| Order | Task | Carries | What closes it |
|---|---|---|---|
| 1 | **M9-001** Reproducible source build | SDK pointer | Bump `third_party/rexglue-sdk` from `5dd58f8` to `a239b83` (fork branch `mcla/mcla-r-hotfixes`, already published). Remove the build-time 32bpp patch step, which that commit now contains. A clean clone builds a stock-30 and an unlocked artifact whose behaviour matches `l31-s30` and `l33-sunc` on a short title-plus-drive check. Private `sdk-l18-s` patch scripts become history, not a build input. |
| 2 | **M9-002** Launcher source reconciliation | prerequisite for 006/007 | The repository launcher source and the l12 binary the owner uses have diverged. Rebuild one launcher from source that keeps every l12 behaviour the owner accepted (profiles, Setup, readiness, F10, lock). Owner smoke on a non-MAIN profile. |
| 3 | **M9-003** Mode option in the launcher | M8-006 | One option: "30 FPS (recommended)" or "Unlocked FPS (experimental)", with the limits shown next to it. Switching back restores stock exactly. This replaces the two shortcut folders. It meets the M8-006 criterion: each option toggles independently, restores stock, and stays hidden until verified. No other graphics options in M9. |
| 4 | ~~**M9-004** Setup categories~~ | M8-007 | Deferred by owner 2026-10-28: the rows did nothing and were removed; the requirement moved to the public release (REL-019). |
| 5 | **M9-005** Volunteer build and diagnostics export | M8-018 | Allowlisted staging (no game data, saves, owner paths, logs, keys), sentinel audit, signed 14-day expiry (already implemented), explicit export with a visible contents list that adds frame-pacing/CP/GPU summaries, a one-page tester guide, and one independent tester smoke test. Distribution: option A (owner decision 2026-10-28), see below. Includes the build kill switch and the post-session report prompt below. F10 images must be fresh or marked stale first (KI-042: BitBlt returned the same frame for three presses). |
| 6 | **M9-006** Known-issue triage for testers | M8-017 residuals | Every open KI gets a disposition (fix in M9, tester-visible known issue, or later milestone). Order: hangs and lost input first, then visible defects. See the table below. |
| 7 | **M9-007** Menu state from the game | unlocked limit | Time-boxed to 2 days. Find the game's own pause/menu state by reverse engineering and read it through a hook, instead of the draw-count heuristic. If found, cap pause, Arcade-over-map and the wager/race-start screen at the meet spot (owner F10, 2026-10-28: it ran at about 80 fps) at 30. If not, the limit stays accepted and documented. A static-scene detector stays rejected (misfires at traffic lights). |
| 8 | **M9-008** Bounded regression and closure | M8-012 pattern | Owner regression on the reproducible build (save/reload, profile switch, a race, garage, photo, pause, exit) in both modes, plus volunteer smoke results. Ledger and current-state update, then the milestone push. |
| 9 | **M9-009** Hide the mouse cursor on pad/wheel input | owner request 2026-10-28 | While the game window is focused, gamepad or wheel input hides the mouse cursor and mouse movement shows it again. No effect when unfocused or in the launcher. Small host change (input listener plus window cursor state); it can run alongside M9-003 to M9-006 and must land before the M9-008 regression. |

Order 1 and 2 can start immediately. Order 5 needs the owner's distribution
decision before anything leaves the PC. Order 6 runs in parallel with 3-5 and
continues with volunteer reports.

## Known-issue triage (proposal for M9-006)

| KI | Sev | Symptom | Proposed M9 action |
|---|---|---|---|
| KI-035 | S2 | Garage transition freeze, must close the game | **Fix attempt.** A hang is the worst tester experience. Start from the existing F10 pair (render thread waiting, CP busy in copy/upload), find what the CP loops on. Owner 100% repro would help. |
| KI-026 | S2 | Camera stuck after Red Light Driver / Race Back | **Fix attempt, time-boxed.** Pre-existing since September, reproduced on l33-sunc with three F10 dumps. Compare against the healthy two-site camera-apply trace. Candidate for a hook-level fix if the guest state machine is understood. |
| KI-040 | S2 | Gamepad not re-acquired after a reconnect | **Fix attempt.** Make SDL/XInput re-scan on device arrival and accept a controller at any XInput index. Volunteers will use many pad types. |
| KI-037 | S3 | First photo broken; save-slot preview shows the previous photo | **Investigate, 2 days.** Leads: partial multi-tile EDRAM resolve on first use, or photo draws skipped during async shader compilation. Workaround (discard and retake) goes into the tester guide either way. |
| KI-038 / KI-032 | S3 / S2 | Placeholder-colour signs, all signs at 4K | **Investigate after the above.** Likely one texture-cache/resolution-scaling cause. Until fixed, the tester guide recommends 720p-1440p. |
| KI-013 | S2 | Traffic lights over-light car paint | **Known issue for testers.** Also present in Xenia, long-standing renderer defect. Reconsider in a later rendering milestone. |
| KI-043 | S2 | Lighting and shadows wrong on the race victory screen after the finish slow motion (l34-sunc, 720p) | **Unlocked-only** (owner: s30 victory screen correct, 2026-10-28). Investigate with KI-041: compare `mcla_guest_vblank_hz=120`, then capture a frame. |
| KI-042 | S2 | F10 frame image can be stale (BitBlt of the flip-model window) | **Fix in M9-005.** Fresh capture or an explicit stale mark; volunteer reports depend on it. |
| KI-041 | S3 | Night light and bush flicker under headlights, unlocked mode (l34-sunc, owner 2026-10-28) | **Classify first.** 720p repro (owner) and the same night route on l33-sunc. If l33-sunc shows it too, it is a pre-existing unlocked-mode issue and joins the unlocked limits; otherwise it is an M9-001 regression and is fixed before M9-003. |
| KI-036 | S3 | Distant pop-in | **Compare with stock l12 once**, then decide (may be original LOD). |
| `DEVICE_HUNG` | - | One GPU reset in eight l29 title runs | **Watch.** Count in owner and volunteer runs; act if it repeats. |

## Volunteer boundary (unchanged from M8, still binding)

The volunteer build excludes the owner's profiles, saves, settings, absolute
paths, logs, F10/crash packages, keys and source game data. No upload or
telemetry; diagnostics leave a tester's PC only by an explicit export. The
current portable publisher output is not the volunteer artifact.

[Source-data policy](source-data-policy.md) forbids distributing binaries that
reproduce proprietary game code "unless an explicit legal review approves
distribution". `mcla.exe` is such a binary. M9-005 therefore needs one owner
decision first:

- **A.** A small closed group of trusted testers who own the game, with the
  owner's explicit written approval recorded as the review for that group only.
- **B.** Testers prepare their own build from their own dump. This pulls the
  dump-validation and local codegen work from the release milestone into M9 and
  makes M9 several weeks longer.

Owner decision 2026-10-28: **option A**. The owner hands the build to a closed
group personally through a private file-sharing link.

## Build kill switch (M9-005, owner request 2026-10-28)

Reuses the signed M8-018 expiry mechanism ([policy](test-build-expiry.md)):
the RSA-signed `active-builds.txt` on branch `DebugBundle` and the signed ticket
beside `mcla.exe`. Changes:

- **Offline lifetime 3 days** from build creation instead of 14, plus a 24-hour
  online-check cache (rule below).
- **Online check is authoritative.** If the signed list downloads and verifies,
  a hash that is listed may start; a hash that is missing is refused and its
  cached grant is deleted. Removing a hash from the list therefore revokes the
  build on its next online start (today removal does not revoke).
- **No internet:** only the offline window applies. A network error, timeout or
  bad signature never revokes and never extends.
- Launcher and host both check, as today. Expiry never stops a running game or
  touches saves.
- Limits stay honest: local clock rollback and patched binaries are not
  defended; this is a freshness switch, not DRM.

Offline rule (owner decision 2026-10-28): without internet a build may start if
**either** it is less than 3 days old (signed creation time) **or** the cache
holds a successful online check that listed this hash less than 24 hours ago.
Every successful online check refreshes that 24-hour cache; a check that does
not list the hash deletes it. So an active tester needs to be online about once
a day after day 3, and a revoked build stops within a day even if the tester
stays offline.

## Post-session report prompt (M9-005, owner request 2026-10-28)

The launcher already waits for the game process. After the game exits it lists
F10 packages (keyboard or wheel/pad binding) and crash packages created during
that session. If there are any, it shows: "N reports were recorded in this
session", with **Open folder** and a short form (what you were doing, steps,
what went wrong, expected). The answers are saved as `report.txt` inside each
package, next to build, mode, resolution and time, which the launcher fills in.
Nothing is sent anywhere; the tester shares the folder by hand. A crash or hang
that ended the session is shown first.

## Estimate

About 2-3 weeks of agent work for orders 1-7 under option A, plus owner checks
(launcher smoke, Setup review, regression) and one volunteer's time. Option B
adds the prepare pipeline and is not estimated yet.

## Deferred, not dropped

- Multi-threaded CP and stable 60 at heavy intersections: M10.
- Present at native monitor refresh (owner has 144 Hz / 4K120, no VRR) and an
  in-game settings overlay: candidates for after M10.
- Ultrawide (M8-005), shader-cache distribution (M8-008), Linux (M8-009/010),
  macOS (M8-013/014), Steam Deck: later platform milestones, without PASS.
- Public release hardening and distribution: the later release milestone.
