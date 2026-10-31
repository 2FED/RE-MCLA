# M10 performance, Steam Deck and stabilisation round 2

Draft for the owner, 2026-10-31. [README-AI](../README-AI) will own the task
ledger once approved. Inputs: the [M9 priorities](M9-priorities.md) (M10 notes,
deferred list), the M9 closure checkpoint in README-AI section 17,
[current state](current-state.md), [known issues](known-issues.md), the
[performance timebox](evidence/M8-001-timebox-20261025.md), the KI-046
[analysis](evidence/M9-006-ki-046-perf-analysis-20261029.md) and
[A/B](evidence/M9-006-ki-046-ab-result-20261029.md), the KI-047
[diagnostic runs](evidence/M9-006-ki-047-diag-runs-20261031.md) and the
[Deck l48 analysis](evidence/M9-006-deck-l48-analysis-20261030.md).

## Decision summary

M9 closed on 2026-10-31 (9 of 10 complete, M9-004 deferred to REL-019, GREEN).
Testers run l52 (allowance l48/l49/l52), SDK pin `v0.20.0.5` = `851f03b`.

1. **Goal: a measurably faster and smoother build for the same closed tester
   group, with the Steam Deck as a first-class target.** "Measurably" means
   per-minute counters on the same save and route before and after, not feel
   alone.
2. **Measure first.** Every performance change in M10 is justified by the
   M10-001 numbers. The multi-threaded command processor (CP), the original M10
   headline, is now a gated task (M10-006): it goes ahead only if M10-001 shows
   that the CP thread, not the guest render thread or the GPU, is the wall.
3. **Cheaper wins come first.** The read-back path (about 1,100 resolves per
   second, all on the read-back path), native Vulkan on the Deck and a
   shader/pipeline pre-warm are smaller, local changes with a large expected
   gain, measured with the same harness.
4. **Not in M10:** public release hardening (dump validation, installer,
   clean-machine install, release tags; section 17a, REL-019), new platforms
   beyond the Deck (Linux native, macOS, ultrawide), new graphics options and an
   in-game settings overlay. Volunteer distribution stays option A.

Strategic direction is unchanged: the recomp is the base, selected subsystems
are replaced through hooks when cheaper; no full decompilation.

## What the numbers say today

- **PC, stock 30 (recommended):** vsync-limited at 30 fps at 720p-1440p; the
  read-back cost is hidden inside the 33 ms budget (KI-047 runs: 30.9/31.4 fps).
- **PC, unlocked:** heavy intersections draw about 6-10 k objects per frame at
  about 2.5 us of CP work each (15-25 ms of CP per frame); replay alone is
  0.53-0.65 us per draw. The guest render thread builds a frame in 13-17 ms and
  spins on the GPU command serial for the rest (timebox; title and stock
  scenes, not yet at a heavy intersection). Unlocked 1440p averages about 37 fps.
- **PC, 2160p:** GPU-bound (59.5 s GPU busy per minute, about 49 ms per frame,
  about 20 fps) and over the 8 GB DXGI budget (KI-048). GPU cost per megapixel is
  quoted as 4.6-6.0 ms (A/B windows) and 7.2-7.5 ms (analysis); M10-001 fixes one
  definition (see the harness rules).
- **Read-back:** about 35-37 resolves per frame, 1,090-1,170 per second at
  1440p `some`, every one through `IssueCopy_ReadbackResolvePath`: main-scene
  tiles, four shadow cascades, the HDR bloom chain. About 16-17.6 k CPU copies
  per minute (270-290 per second, up to 5 MB each) plus a GPU downscale and copy
  for every resolve even when the CPU copy is deferred. The CPU needs almost
  none of the per-frame ranges.
- **Deck (l48/l49, D3D12 through vkd3d-proton on RADV):** title 14-24 outputs/s,
  play 10-14 fps with audio tearing. CP time is dominated by WAIT_REG_MEM
  (23.45 s of 36.35 s); write-watch costs 18.8 k faults / 1.08 s and 225 k arms /
  2.29 s. Older M7-016 data: Vulkan 8.86 against D3D12 7.34 outputs/s, so the
  backend alone is not expected to reach 30.

## Execution order

| Order | Task | Carries | What closes it |
|---|---|---|---|
| 1 | **M10-001** Measurement at one heavy intersection, plus a Deck baseline | M9 proposed M10-001; Deck note | One repeatable heavy-intersection route on a bench profile, unlocked mode, PC 5060 at 1440p. In the same window: guest render thread CPU, CP thread CPU, real GPU busy, draws per frame, and the per-draw CP cost split (state/bindings, shared-memory requests, primitive processing, replay), plus read-back resolves/copies per second. The same harness on the Deck at the title and on one short drive (stock mode). Output: a written verdict naming the wall per platform (render thread, CP, GPU, read-back, wait), which sets the order of 002-006. | Plan change 2026-10-31 (harness design): measure a 720p arm next to 1440p at the same place, since the 5060 is GPU-bound at 1440p; the 720p arm decides CP versus render thread for M10-006. [Harness design](evidence/M10-001-harness-design-20261031.md). |
| 2 | **M10-002** Read-back path scoping | KI-047 diagnostic note; Deck read-back cost | Restrict the CPU read-back to ranges that are not resolved every frame (for example: skip keys resolved in each of the last N frames, keep one-shot and rare ranges on the current path). Before/after with the harness on PC and Deck. Correctness-sensitive: Photo Mode (KI-037 fix), the vinyl/livery composite (KI-033, KI-047) and the album preview (KI-056) depend on this path, so each gets a named check. Closes when CPU copies and GPU read-back copies per second drop measurably, frame time does not regress, and the three checks pass on the owner PC. |
| 3 | **M10-003** Shader/pipeline pre-warm before the first launch | owner request 2026-10-31; ex M8-008 | Three steps: (a) portability study: which parts of the per-build store (translated shaders, PSO keys, driver PSO blobs) are portable across GPUs and drivers, and what must compile locally; (b) a launcher pre-warm pass with progress UI before the first START of a new build fingerprint, cancellable and resumable; (c) replay of recorded pipeline keys from owner/volunteer sessions (keys only, no game data, within the volunteer boundary). Closes when a cold first launch shows minute-1 pipeline waits near the warm-store level (l51 reference: cold 406 waits / 2.09 s, warm 122 / 1.2 s) on the PC and the Deck, with 0 skips. |
| 4 | **M10-004** Vulkan-enabled SDK and Deck A/B | KI-053 | SDK built with `REXGLUE_USE_VULKAN=ON` on the fork, published and pinned through a clean clone; ctest and host suites on both backends. Deck A/B of D3D12/vkd3d against native Vulkan with the harness (same route, cold and warm stated). Only if Vulkan wins or ties without new defects: the launcher passes `--gpu_backend=vulkan` under Wine and the host detects Proton with `RunningUnderWine()`. Windows stays D3D12. |
| 5 | **M10-005** Deck performance work | Deck note; KI-027 | After the 001/002/004 numbers. Target: a stable 30 in play on the Deck in stock mode at 1280x800 (or 1280x720), audio without tearing. Work items chosen from the numbers: WAIT_REG_MEM handling, write-watch cost, Deck defaults (scale, read-back mode, budget). Time-boxed; if 30 is not reached, the best stable rate and the remaining wall are recorded honestly. |
| 6 | **M10-006** Multi-threaded command processor | M9 decision; unlocked limit | Gated on M10-001: go only if the CP thread is the wall at the heavy intersection and the render thread has headroom below 16.7 ms. Design note first (ordering across threads for the register file, write-watch, EDRAM/texture cache and deferred command list; which stage splits first), owner approval of the design, then implementation behind a default-off cvar. Closes when unlocked heavy-intersection fps rises by a stated margin with no regression in stock 30 or on the volunteer paths. If the gate stays closed, the task closes as "not justified" with the numbers. |
| 7 | **M10-007** Video-memory budget behaviour | KI-048; KI-054 trigger; KI-035 context | Budget-aware texture residency instead of stepping the hard limit down mid-scene; measure the scaled-resolve and render-target footprint at scale 3. Closes when a 30-minute 1080p session on the 8 GB 5060 stays under budget without frames over 50 ms from budget steps, and 2160p on the 3080 Ti or 6800 XT shows the true footprint (with a documented 2160p recommendation for 8 GB). |
| 8 | **M10-008** Known-issue triage round 2 | M9 dispositions; owner reports 2026-10-31 | KI-055 (partial packages at exit) fixed early and cheaply; KI-056 (album preview) with M10-002; KI-054 (NPC jitter on frame drops) after M10-007 shows whether it is only a frame-drop symptom; KI-035 (garage freeze) / KI-026 (stuck camera) when a repro package arrives (diagnostics already in the build); KI-038/032 (signs); KI-041/043 (unlocked-only rendering); KI-052 (Steam keyboard); Deck fullscreen (see inputs below); confirm KI-022 (wheel centering) and KI-047 (police livery) from tester packages. Every row gets a fix with before/after evidence or an updated disposition. |
| 9 | **M10-009** Volunteer release cadence and closure regression | M9-005/M9-008 pattern | Each candidate from a clean clone at a published SDK pin, a short tester changelog, a signed list with the allowance trimmed to the current and previous build, tester feedback collected into KI rows. At closure: bounded owner regression on the PC (both modes) and the Deck, ledger and current-state update, milestone push. |

Order 1 starts immediately. 2 and 3 can run in parallel after 1 (different code
paths; runtime probes stay serialised). 4 can start alongside 1 (SDK build
work); its Deck A/B needs the 001 harness. 5 waits for 1, 2 and 4. 6 waits for
the 001 verdict and the owner's design approval. 7 and 8 run alongside; 9 runs
per candidate and at closure.

## Estimates and owner involvement

| Task | Agent-days | Owner and machines | Evidence it must produce |
|---|---|---|---|
| M10-001 | 3-4 | 5060 PC: 2 cold + 2 warm unlocked runs on the bench route; Deck: title + drive, stock | Harness document, raw per-minute counters, thread CPU table, CP cost split, verdict note |
| M10-002 | 4-6 | 5060 PC before/after runs and photo/vinyl/livery/album checks; Deck before/after | Before/after counter table, three named correctness checks with F10 packages, SDK commit and tests |
| M10-003 | 6-9 | 5060 PC and Deck cold first launch; 3080 Ti / 6800 XT for store portability | Portability study, launcher tests, cold/warm minute-1 waits and skips per machine |
| M10-004 | 4-6 | Deck A/B (two runs per backend); PC smoke that Windows stays D3D12 | Clean-clone record with Vulkan on, ctest on both backends, Deck A/B table, launcher change tests |
| M10-005 | 5-10 (time-boxed) | Deck session per candidate change | Per-change Deck counters, final stable-rate statement, audio check |
| M10-006 | design 3-4; implementation 10-15 if approved | 5060 PC unlocked heavy route; regression in stock 30 | Design note, gate decision, before/after, ordering test suite, regression list |
| M10-007 | 4-6 | 5060 PC 30-minute 1080p session; 2160p on the 3080 Ti or 6800 XT | Budget lines per minute, footprint inventory at scale 2/3, before/after |
| M10-008 | 6-10, spread | Owner/tester packages on request; Deck for KI-052 and fullscreen | Per-KI evidence and updated rows in known-issues |
| M10-009 | 1 per candidate + 2 at closure | Owner signs and publishes each list; regression on PC and Deck | Candidate records, published list commits, changelog, closure checkpoint |

Total: about 6-8 weeks of agent work without the M10-006 implementation, 2-3
weeks more if the gate opens it. Owner time is mostly short timed runs on the
bench route plus Deck sessions.

## Measurement harness and acceptance rules

- **No PASS without counters.** A performance claim needs the per-minute log
  lines (frame pacing, GPU busy, draws per frame, replay, write-watch, read-back
  resolves/copies, pipeline waits/skips) and the thread CPU table for the same
  window. Feel is recorded but never replaces counters.
- **Same save, same route.** A bench profile copied from MAIN through the
  launcher (MAIN itself is never used), one named route to the heavy
  intersection, driven the same way; the Deck uses its own bench profile copy.
- **State the cache.** Every run records a cold or warm shader/pipeline store
  and the number of pipelines loaded from storage.
- **State the mode.** Build hash, stock 30 or unlocked, resolution and draw
  scale, read-back mode, backend, vsync, adapter and driver.
- **Windows.** Discard the first minute (boot, menus, 45 s of 60 Hz vblank in
  unlocked mode); at least two clean per-minute windows per run; at least two
  runs per arm. GPU cost per megapixel = GPU busy ms per frame divided by the
  render-target megapixels, taken from a minute that is not vsync-limited.
- **Decision threshold.** A change is a win only if it beats the run-to-run
  spread and gains at least 5% on the target metric; within 10% at matched
  draws counts as "no regression" (the KI-046 rule).
- **Correctness checks travel with performance changes.** Read-back and
  pipeline changes always repeat first photo, vinyl/livery and album preview.

## Risks

- **Regressions on the paths volunteers test.** Read-back scoping, pre-warm and
  a split CP all touch rendering that testers use daily. Mitigation: new paths
  behind cvars, default-off until the owner regression passes, one change per
  volunteer candidate so reports stay attributable.
- **SDK publication path.** Every SDK delta must reach the fork
  (`2FED/rexglue-sdk` `mcla/mcla-r-hotfixes`, SDL changes on `2FED/SDL`) as a
  numbered version at a reachable commit before a candidate pins it; pushes need
  explicit owner approval. A Vulkan-enabled build changes the SDK build
  configuration and the published DLL set, so it gets its own clean-clone record.
- **Deck differences.** Proton/vkd3d versions, Steam Input, Z: paths, a 1280x800
  panel and a shared-memory APU behave differently from the PC; a PC win may not
  transfer. Deck numbers are taken on the Deck, never inferred.
- **Pre-warm portability.** Driver PSO caches are not portable; shipping them
  may give no gain or break on driver updates. The study (M10-003a) decides what
  ships; local compilation stays the fallback.
- **Multi-threaded CP scope.** Ordering bugs are intermittent and hard to
  attribute; the gate and the design approval exist so it does not start
  without a measured reason.
- **Save lineage.** The Deck uses a profile copy with the MAIN id; the owner
  must name the canonical copy before any sync, and bench profiles never sync.

## Inputs to fix before approval

- KI-025 is Closed (SDK `v0.10.0.2` Alt+Enter on Windows), but the l48 Deck run
  was windowed 1280x720 on the 1280x800 panel; no open KI covers Deck
  fullscreen. Proposal: a new KI for Deck presentation, handled in M10-008.
- KI-053's target column says "None" and KI-048's says "investigate later";
  both become M10 tasks (M10-004, M10-007). KI-051 is owner-confirmed on the
  Deck (GPU preselected) but its row is still Open.
- The current-state header says October 30 while carrying October 31 content,
  and its "Next (M9...)" section still describes M9 as approved and pending.
- The M9 deferred list puts the Steam Deck and shader-cache distribution
  (M8-008) in later platform milestones; this plan pulls both into M10.

## Deferred, not dropped

- Public release hardening and distribution: the later release milestone
  (section 17a), including REL-019 Setup categories.
- Present at native monitor refresh (144 Hz / 4K120, no VRR) and an in-game
  settings overlay: after M10.
- Stable 60 at heavy intersections in unlocked mode if the M10-006 gate stays
  closed; unlocked stays experimental.
- Ultrawide (M8-005), Linux native (M8-009/010), macOS (M8-013/014): later
  platform milestones, without PASS.
- KI-013, KI-015, KI-016, KI-019, KI-020 rendering defects and KI-036 pop-in:
  a later rendering milestone unless M10 work touches the same path.
