# M11 (ACTIVE; approved by the owner 2026-11-06) Steam Deck saves, owner diagnostics, and Deck and video-memory performance round 3

Draft for the owner, 2026-11-06. Once approved, [README-AI](../README-AI) section 17c
owns the task ledger. Inputs: the M10 closure in README-AI section 17b (task table,
checkpoint log, "M11 inputs" 1-14), [M10 priorities](M10-priorities.md) (gate decisions,
harness rules), [current state](current-state.md), [known issues](known-issues.md) (KI-068
added 2026-11-06), and the M10 design and analysis notes linked in each row below.

## Decision summary

M10 closed on 2026-11-05 with the closure regression done 2026-11-05/06. Testers run l70
only (`DebugBundle` `31c806d`). The SDK pin is `v0.20.0.8` = `c63895c`. Main is pushed
(`9e541e8`, handoff `7e835d0`, KI-068 `8c2172c`).

1. **Goal: saves that work on the Steam Deck, answers to the owner's open observations, and
   a measurably faster Deck and steadier PC memory behaviour for the same closed tester
   group.** "Measurably" keeps the M10 meaning: per-minute counters on the same save and
   route, before and after.
2. **Correctness first.** KI-068 (Deck saves under Proton, S1) comes before any performance
   work. It is the only S1 row open.
3. **Answer the open observations cheaply with one owner build.** Three items go into one
   owner-internal build: KI-056 (album preview), KI-066 (pursuit state) and KI-067 (CPU
   clock, temperature and load). Each is either a host diagnostic or a small fix with an
   A/B.
4. **Deck performance follows the M10 verdict.** On the Deck the CP thread is first and the
   CPU is oversubscribed. The order is: free cores (and KI-060 audio with them), hot blocks
   v2 (a new copy shape), then the M10-006 gate again, but only if a core is actually free.
5. **Not in M11:** Vulkan (shelved; KI-062/063), decompilation, public release hardening
   (section 17a, REL-019), new platforms, new graphics options or an in-game overlay.

The strategic direction does not change. The recomp stays the base, and subsystems are
replaced through hooks only where that is cheaper.

## What the numbers say today (from the M10 evidence)

- **Deck (l64/l66/l70 defaults, D3D12 via vkd3d, stock 30, 1280x720):** driving is about
  14.7-14.9 fps. The CP is about 89-99% occupied. The GPU is about 40% busy (27 ms/frame).
  The top eight threads use 4.3-4.4 of 8 logical CPUs, and the CP is off-CPU about 10% of
  wall time. Two threads together use about 1.6 cores:
  - the guest polling thread `F800068C`, about 0.8 core while driving, with
    `KeDelayExecutionThread` at 60% of its last imports;
  - the audio-side thread `F8000534`, about 0.84 core, with `KfLowerIrql` at 80%.

  Audio underruns remain in every arm (KI-060: about 1,467 per driving audit with "all").
  Kicks cost 0.23 ms each under vkd3d, and a targeted wait costs about 4.5 ms. CP draw
  phases take 24.6 ms/frame at 4.15 us per draw, about 3.3x the PC.
- **Hot blocks (l66):** arm calls fell 42%, about -9 ms/frame of CP watch work. The cost
  came back as about 2,050 copies between draws: GPU +10.4 ms/frame, and fps unchanged
  (14.9 vs 14.9). Safety counters were clean.
- **PC video memory (8 GB RTX 5060, 1080p Unlocked):** with the 2048 MB soft cap, usage
  stays at about 5.25-5.40 GB, in budget for 55 minutes (l69). Without the cap it goes over
  budget around minute 10, and the 0.3-1.0 s hitches equal the KI-047 targeted sync wait
  under paging. 2160p has not been measured on a 12/16 GB card.
- **Read-back:** step 1 elision removes 73-74% of GPU read-back copies (-3 ms GPU per frame
  at 1440p). CPU bytes copied are unchanged. Steps 2-5 (read watch, skip untouched, touch
  fallback, batching) are not built.
- **Pre-warm:** on the PC, the seeded store gives 0 pipeline waits. Deck seeding is
  unverified: the l64 Deck store never held the PC union, and no seed line is recorded for
  the l67 Deck loop (verify).
- **KI-067:** a 3-minute host-wide CPU slowdown on l68 (every host CPU cost 1.6-2x), with no
  recurrence on l69. The per-minute census has no CPU clock, temperature or load to
  confirm it.

## Ranked tasks

Estimates are agent-days. Owner tests use the M10 harness rules (below).

| Rank | Task | Source | Scope | Closure criterion | Owner tests | Estimate | Risk |
|---|---|---|---|---|---|---|---|
| 1 | **M11-001** KI-068 Steam Deck saves under Proton | KI-068 (S1, owner 2026-11-06); KI-065; [KI-065 content logs and F10 guard](evidence/M10-ki065-content-logs-and-f10-guard-20261103.md) | (a) The owner details the symptoms: device dialog or silent loss, pause-menu save or autosave, which profile. (b) Deck repro on l70: save from the pause menu, quit, restart; also one autosave after a fine or purchase. (c) Read the session folder: the `[content]` WARN lines (stage, X_RESULT, Win32 error), the launcher before/after save-tree snapshots, the F10 `save copy start/end/skipped` lines, and the `rex_xam_content_transactions_active` state. (d) Root cause and fix: SDK content layer, host or launcher path handling under Wine, as the lines dictate. If the lines are not enough, add an owner-internal Deck diagnostic build first. (e) KI-065 is resolved by the same evidence or folded into this task. | On the Deck, two sessions in a row: a pause-menu save and an autosave survive quit and restart, the launcher snapshot shows the save tree changed, and there are 0 `[content]` failure lines. A PC save/reload shows no regression. KI-068 is Fixed with evidence; KI-065 is closed or re-dispositioned. | Deck: the repro session on l70 (session folder sent); then the fix build with the same steps twice. PC: one save/reload smoke. | 2-5 (analysis 1; fix 1-3; open until the symptoms are known) | High: an S1 save path; save lineage (the Deck profile copy has the MAIN id; the owner names the canonical copy and the profile used for the repro before any sync); a fix in the SDK content layer needs an SDK version and publication |
| 2 | **M11-002** Owner diagnostics triad (one owner-internal build) | KI-056 ([album analysis](evidence/M10-ki056-album-preview-analysis-20261106.md)); KI-066 ([l69 session](evidence/M10-l69-pc-session-20261105.md)); KI-067 ([l68 watchdog arm](evidence/M10-l68-watchdog-arm-ki067-20261105.md)) | **(a) KI-056 album preview.** First the owner's 720p vs 1080p check (no build). Then the host `MCLA_ALBUM v=1` diagnostic on `sub_8263CB78`, `sub_8263C8B0`, `sub_8263C9C8` and `sub_8263DCF8`: state 4, SOI, a CRC of `+1144`, and the `+1132` texture header. Then fix A (re-run the new-photo decode once when the chooser opens) or B (fire event 4 after the state-4 rebind) as an A/B. C (exact invalidation) only if the diagnostic shows a host miss. **(b) KI-066 pursuit state.** Find the police/pursuit manager in generated code. Add heat level, pursuit phase, responding-unit timer and the HUD indicator flag to the F10 guest block (range-checked reads), and log each transition. **(c) KI-067 CPU telemetry.** Add CPU effective MHz, CPU temperature and system load to the per-minute census. | (a) The four owner test steps of the album analysis section 4.3 pass on the PC: the new photo shows before any cursor move and again after hovering, and the stored album image is correct. KI-056 is Fixed, or re-dispositioned with the diagnostic evidence. (b) An F10 during a pursuit and a pull-over shows the state, and the log shows the transitions; KI-066 gets a data-based disposition. (c) The census line carries the three fields on the PC; on the Deck the fields are present or marked unavailable; KI-067 is closed or kept with data. | PC: the 720p/1080p album check (no build); one session on the triad build covering the album steps, one pursuit with F10 at the pull-over, and the normal per-minute lines; fix A/B arms for KI-056. Deck: one short loop for the census fields. | 4-7 (KI-056 diagnostic 1, fix 1-2; KI-066 1-3 depending on how quickly the manager is found; KI-067 0.5-1; build 0.5) | Medium: the KI-056 fix A/B touches guest UI state (bound it to the save chooser, at most once per snapshot); the KI-066 manager layout is unknown; CPU temperature may not be readable under Proton or without vendor APIs (verify) |
| 3 | **M11-003** Deck core freeing and KI-060 | 17b M11 inputs 2; [l64 Deck A/B](evidence/M10-l64-deck-ab-20261103.md) sections 2 and 6; KI-060 | Per-thread counters for `F800068C` and `F8000534` first: wake rate, `KeDelayExecutionThread` and IRQL call costs under Wine. Then cheaper `KeDelayExecutionThread`/IRQL paths, behind cvars. Also a kick-interval A/B (`kick_draws` 1024 / 2048 / off). Whether the kick interval is a dev-override key or needs a build is to be verified (verify). | On the Deck, the top-eight-thread core sum and the CP off-CPU share fall beyond the run spread. Driving fps is not worse (within 10% at matched draws). KI-060 driving underruns per audit fall measurably, with audio checked by ear. PC 720p shows no regression. | Deck: title 60 s plus one driving loop per arm, two runs per arm. PC: 720p BENCH regression. | 4-7 | Medium: guest timing semantics (sleep and IRQL behaviour) and audio regressions; gains measured under Proton may not transfer between Proton versions |
| 4 | **M11-004** Hot blocks v2: copy shape | 17b M11 inputs 1; [l66 Deck A/B](evidence/M10-l66-deck-ab-20261103.md); [watch/upload design](evidence/M10-deck-watch-upload-design-20261101.md) step 3 | On SDK branch `mcla/m10-hotblocks` `cdf469a` (local), batch the hot copies: one copy per hot block per submission, or a per-submission snapshot of the hot blocks before the first draw. Copies then stop splitting render passes. Re-measure against "all". | On the Deck: hot copies after draws fall from about 2,050/frame to a small count, GPU busy returns near the "all" level (27 ms/frame), and driving fps rises at least 5% beyond the run spread. Safety counters stay at 0 (protect failures, guard misses, `hot_gpu_whole`). KI-047 livery and KI-033 vinyl checks pass. Otherwise the task closes negative with the numbers. | Deck: "all" vs hot-v2, two runs each, with police and garage visual checks. PC: regression arm. | 3-5 | Medium: correctness of snapshot timing against guest writes inside a submission; the SDK branch needs a version and publication if it ships |
| 5 | **M11-005** M10-006 multi-threaded CP: gate re-check | 17b M11 inputs 3; M10 gate decision 2026-11-03 | Re-run the gate on the M11-003/004 Deck numbers. Proceed only if a core is free (top-eight threads clearly below the logical CPU count, and the CP off-CPU share down) while the CP is still first. If so: a design note (ordering across threads for the register file, write-watch, EDRAM/texture cache and deferred command list), owner approval, then implementation behind a default-off cvar. | Gate closed with the numbers, or (if opened) driving fps on the Deck rises by a stated margin with no stock-30 or volunteer-path regression. | Deck: the M11-003/004 runs (gate). If opened, Deck and PC A/Bs. | Gate 0.5; design 3-4; implementation 10-15 if approved (M10 estimate) | High if opened: intermittent ordering bugs; the PC gate stays closed while the GPU and the render thread lead |
| 6 | **M11-006** Video-memory budget steering (M10-007 remainder, KI-048) | 17b M11 inputs 4; [budget design](evidence/M10-007-budget-design-20261104.md) section 5; KI-048 | Steps from the design: S0 census with SDK size accessors (cache, scaled-resolve heaps, render targets); S1 soft-limit control against the live budget (`mcla_texture_budget_mode=soft`, floor 1024 MB, headroom 512 MB); S2 residency priority; S3 render-target trim; S4 the bounded KI-047 wait under paging (H4: lazy completion behind a read trap), only if waits over 250 ms remain after S1-S3; S5 the 2160p census on the 3080 Ti or 6800 XT, with an 8 GB recommendation. | From the design: a 30-minute 1080p session on the 8 GB 5060 stays under budget with no frames over 50 ms from budget control. 2160p shows the true footprint, and the 8 GB recommendation is documented. KI-047 composite counters are unchanged. KI-048 closes. | PC 5060: 30+ min 1080p Unlocked per step arm (same pause-menu and freeway segments). 3080 Ti or 6800 XT: one 2160p census session. 10 cold sessions only if S4 is built (KI-047 livery). | 5-8 (S4 adds 3-5 if needed) | Medium: S4 sits on the KI-047 failure surface (reject the timeout-only variants H2/H3); render-target trim risks re-create hitches; 2160p depends on card availability |
| 7 | **M11-007** Read-back steps 2-5 (M10-002 remainder) | 17b M11 inputs 5; [read-back design](evidence/M10-002-readback-design-20261101.md) sections 3.2-3.6 and 6 | Step 2: read-watch census (`d3d12_readback_read_watch`, NoAccess on resolved pages, per-key touch state). Step 3: skip untouched steady keys (`d3d12_readback_skip_untouched`; composite family excluded; photo window suspends skipping; the first resolve after leaving SKIP is synchronous). Step 4 (conditional): touch fallback, previous-slot fence, and photo read-back sync as a KI-056 candidate. Step 5 batching, closed as not justified if `rb_gpu_recorded` stays below about 5 per frame. Deck measurement of the same steps. | `skip_violation=0`. CP -0.5 ms/frame or better at 720p, GPU further down at 1440p. Named checks pass: first photo, police livery (KI-047), garage and driving vinyl (KI-033), album preview (KI-056), signs (KI-038/032), tunnel exposure, minimap. Deck frame time down, or recorded as neutral. | PC: 1440p and 720p BENCH A/B per step, plus a census tour (photo, garage vinyl, police, signs, tunnel, map, menus). Deck: one A/B for step 3. | 4-6 | Medium: a steady family the CPU does read would freeze (the census comes before the skip); read-fault cost under Wine (about 80 us per fault on the Deck) |
| 8 | **M11-008** Pre-warm: Deck seeding check, a1, a2 spike | 17b M11 inputs 6; [pre-warm design](evidence/M10-prewarm-design-20261101.md) section 3 steps 3-4; [l64 Deck](evidence/M10-l64-deck-ab-20261103.md) section 5 | (i) Deck seeding check on the current launcher: read the store line and `store-seed.txt` from the M11-001 Deck sessions; fix the seed source or target if it does not take. (ii) a1: a shipped derived-hint `.xpso` (hashes and state only), after an owner-approved source-data-policy amendment. (iii) a2: a 2-day spike on the engine effect-loader hook (match rate of the loaded-effect ucode hash to the CP hash; counts only). | (i) A fresh Deck BENCH copy starts `source=seeded`, with minute-1 pipeline waits near the warm level. (ii) A fresh, install-like profile with an empty union store shows fewer cold minute-1 waits with hints than without, with 0 skips. (iii) A go/no-go note with the match rate. | Deck: seeding read from the M11-001 sessions, plus one cold vs seeded pair if a fix is needed. PC: an a1 cold vs hint pair on an empty store. | 3-6 (i 0.5-2; ii 2-3; iii timeboxed 2) | Low for (i); (ii) needs an owner policy decision (no `.xsh`, no driver blobs ever ship); (iii) medium research risk |
| 9 | **M11-009** KI-054 dt-smoothing study | KI-054; 17b M11 inputs 9; [l68 analysis](evidence/M10-l68-watchdog-arm-ki067-20261105.md) section 5 | Correlate the `mcla_raw_timer` step/debt lines with the frame-to-frame swings over 30 ms. Run the same freeway route with the raw timer smoothing variants (off vs on) under Unlocked timing. Deck: the same intersection at stock 30 vs about 15 fps. Output: a frame-pacing or dt-smoothing proposal, or a disposition. | A written verdict: varying dt causes the traffic shake (with a proposed change and its A/B), or it does not (KI-054 re-dispositioned). | PC: two freeway runs per timer arm, F10 at the shakes. Deck: one intersection pair. | 2-4 (study only; a fix is a new row) | Low for the study; any fix touches guest timing in both modes |
| 10 | **M11-010** KI-059 D3D12 F10 graphics block | KI-059; 17b M11 inputs 8 | Fill `state.json` `graphics` under D3D12 in the SDK: backend, `guest_frames_closed`, `guest_frame_wall_us`, `issue_swap_calls`, `submission_count`, fence and queue fields. | F10 on D3D12 shows non-zero fields matching the per-minute lines; SDK ctest passes; KI-059 is closed. | PC: one F10 smoke (can ride on any owner build). | 1-2 | Low; needs an SDK version |
| 11 | **M11-011** KI-052 prefilled launcher names | KI-052; 17b M11 inputs 7 | Prefill the profile name and the in-game name so the Deck flow needs no typing. Every folder field already has Browse. | On the Deck, first-run setup to START works without a keyboard; launcher suites pass. KI-052 is closed, or kept for the Steam keyboard itself. | Deck: one first-run setup without a keyboard (can share the M11-001 session). | 1 | Low |
| 12 | **M11-012** KI-035 refinement (LOD pop) | 17b M11 inputs 11; [finish-race fix](evidence/M10-ki035-finish-race-fix-20261105.md) | Exit the state-20 handler one frame earlier, so the finish hook (and its brief LOD pop) is rarely needed. Keep the hook as the backstop. | Over 5+ garage trips after races, hook firings drop (l69: 3 of 5), there is no freeze, and the LOD pop is not seen. Warp-probe unit tests pass. | PC: one 1080p Unlocked session with 5+ garage trips after races. | 1-2 | Medium: touches the guest warper state machine; the hook must stay as the fallback |
| 13 | **M11-013** Volunteer cadence, build scripts, carried confirmations, closure regression | M10-009 pattern; 17b M11 inputs 12 and 14 | One volunteer candidate per closed group of tasks, built from a clean clone at a published SDK pin, with a short changelog. The signed allowance list is trimmed to the current build. Fix the host build script's `*_test` target check (matches 0 help lines), and point the prepare scripts at `private/gate-host-build/mcla_build_lease_probe_win.exe`. Carried confirmations: KI-057 (Deck first-run fullscreen) and KI-064 (owner); KI-022 and KI-047 (testers); repro-only KI-026, KI-038/032, KI-041/043 when packages arrive. At closure: bounded owner regression (PC both modes, Deck), ledger, push. | Each candidate: clean-clone record, owner pre-share check PASS, published list. Closure: owner regression on the last candidate passes on PC and Deck. | Per candidate: PC pre-share session and a Deck loop. Closure: PC 30 FPS and Unlocked, Deck loop. | 1 per candidate + 2 at closure | Low-medium: with a one-entry allowance a broken candidate leaves testers without a build until republished, so the list is published only after the pre-share check |
| 14 (stretch) | **M11-014** CPU codegen items | [CPU targets](evidence/M10-cpu-targets-20261101.md) tasks C, D, E | Promote the M7 published function scope (no `uncaught_exceptions`). Elide the interrupt-disable idiom in codegen for the 45 pure reservation regions. The write-fault hot-block policy is now covered by M11-004. Pull this forward into the Deck group if M11-003 shows guest-thread CPU, not the two named threads, is what holds the cores. | Render work ms/frame down beyond the spread at 720p (target -0.8 ms or better for C). Deck guest-thread CPU down. M7 ABI/vector witnesses, crash-report test and forced-fault crash report pass. All 45 emitted regions are reviewed. | PC 720p BENCH A/B; Deck A/B. | 4-8 | Medium: all generated code changes (unwinding semantics for C) |

Totals without the M10-006 implementation and the stretch item: about 34-64 agent-days
(13 rows; derived by adding the estimates above, assuming two to four volunteer candidates and M11-005 as gate plus design). With the stretch item: about 38-72. If
the M11-005 gate opens, add 10-15. Owner time is mostly Deck sessions (M11-001, -003,
-004), one triad PC session, 30-minute PC budget sessions, and one 2160p session on a
loaner card.

## Execution order

1. **M11-001 first**, and alone, until the Deck repro session's lines are read. It is
   S1, and every later Deck build depends on saves working.
2. **M11-002 next, as one owner-internal build.** Its code (album diagnostic, census
   fields, pursuit-state reads) is written while the owner runs the M11-001 Deck repro;
   these are different code paths, so they can proceed in parallel. Runtime probes stay
   serialised.
3. **First volunteer candidate** once M11-001 is fixed and the KI-056 fix is chosen
   (M11-013). M11-010 and M11-011 ride on it if they are ready.
4. **Deck group:** M11-003, then M11-004, then the M11-005 gate.
5. **In parallel with the Deck group** (different paths, PC-side tests): M11-006 and
   M11-007, then M11-008 (i)-(iii).
6. **Then:** M11-009, M11-012, and the stretch item if time remains. M11-013 runs per
   candidate and at closure.

## First week

| Day | Agent | Owner |
|---|---|---|
| 1 | Prepare the M11-001 Deck repro protocol (exact steps, which lines to send). Start the M11-002 code: KI-067 census fields, the KI-056 `MCLA_ALBUM` wrappers, the static search for the KI-066 pursuit manager. Spark candidate: triage of the Deck session logs for `[content]`/save-copy lines (advisory). | Detail the KI-068 symptoms. Deck repro on l70: pause-menu save, quit, restart, plus one autosave; send the session folder. Name the profile used. The KI-056 720p vs 1080p album check on the PC (no build). |
| 2 | Analyse the Deck session: `[content]` lines, launcher snapshots, F10 save-copy lines. From the same session, read the store-at-startup/seed line (M11-008 (i)) and the fullscreen state (KI-057). Root-cause note for KI-068, or a Deck diagnostic build if the lines are silent. | Answer follow-up questions; run a second Deck repro if the diagnostic build is needed. |
| 3 | KI-068 fix (SDK or host, as dictated), with tests. Finish the M11-002 code; assemble the owner-internal triad build (l71, verify the number) with a V1 ticket. | — |
| 4 | Stage the KI-068 fix for the Deck (combined with the triad build if they are independent and the arms stay attributable; otherwise a separate build). | Deck: the fix build, save and restart twice. PC: the triad session (album steps, a pursuit with F10 at the pull-over, normal lines). |
| 5 | Analyse both. Choose the KI-056 fix arm (A or B) and build the A/B. Start the M11-003 per-thread counters for `F800068C`/`F8000534`. If any SDK delta exists, prepare its version and ask the owner to approve publication. | KI-056 fix A/B on the PC. |

## Owner-facing test plan

**PC (RTX 5060 8 GB; a bench profile copied from MAIN through the launcher, never MAIN
itself):**
- M11-002: the 720p vs 1080p album check (no build). Then on the triad build: photo,
  chooser, F10 before moving the cursor, hover an occupied slot and return, F10, save,
  reopen the album. One pursuit with F10 at the pull-over. Keep the normal session running
  so the census lines cover it.
- M11-006: 30+ minute 1080p Unlocked sessions per step arm, on the same route with pause
  menu and freeway. One 2160p session on the 3080 Ti or 6800 XT.
- M11-007: 1440p and 720p BENCH A/B per step, plus one census tour (photo, garage vinyl,
  police, signs, tunnel, map, menus).
- M11-009 / M11-012: freeway timer arms; 5+ garage trips after races.
- Per volunteer candidate: the pre-share session. At closure: 30 FPS and Unlocked passes.

**Steam Deck (Proton, stock 30, its own bench profile copy; the MAIN-id copy is not
synced):**
- M11-001: the l70 repro (pause-menu save, quit, restart; an autosave after a fine or
  purchase), then the fix build twice. Send the session folders.
- From the same sessions: the seeding line, fullscreen at first run (KI-057), first-run
  setup without a keyboard (KI-052 once built), and an F10 at the start of a pull-over
  (KI-065).
- M11-003 / M11-004: title 60 s plus one driving loop per arm, two runs per arm, with police
  and garage visual checks.
- M11-002: one short loop for the CPU telemetry fields.
- Per candidate and at closure: one loop.

## Measurement and acceptance rules

The M10 rules apply unchanged ([M10 priorities](M10-priorities.md), "Measurement harness
and acceptance rules"):
- No PASS without counters. State the cache, the mode, the build hash and the platform.
- Discard the first minute. Use at least two clean windows per run and two runs per arm.
- A win must beat the run-to-run spread and gain at least 5%. "No regression" means within
  10% at matched draws.
- Read-back and pipeline changes always repeat the first photo, vinyl/livery and album
  preview checks.
- Deck numbers are taken on the Deck, never inferred from the PC.

For M11-001 a save PASS needs the session folder: launcher snapshots plus `[content]`
lines. The owner's word that "it saved" is recorded but does not replace them.

## Risks

- **Save integrity (M11-001).** A fix in the content layer can damage saves.
  Mitigations: test on a named Deck test profile; never sync bench or Deck copies; the
  owner names the canonical copy first; on the PC run a save/reload before any candidate
  ships.
- **Guest-state hooks.** The KI-056 fix A/B, the KI-066 reads and the KI-035 refinement
  touch game UI or state machines. Mitigations: range-checked reads, default-off or
  one-shot behaviour, and kill-switch cvars, as with the M9-007 and KI-035 hooks.
- **SDK publication path.** Every SDK delta (KI-068 if SDK-side, KI-059, hot blocks v2,
  read-back steps, budget accessors) needs a numbered version on `2FED/rexglue-sdk`
  `mcla/mcla-r-hotfixes` and explicit owner approval to push before a candidate pins it.
- **Deck transfer.** Proton/vkd3d versions and the 4-core APU behave differently from the
  PC. Core freeing may not turn into fps under stock-30 vsync quantisation (l66 p50
  66.7 ms = 4 vblanks in every arm).
- **Telemetry availability.** CPU temperature and effective clock may be unavailable under
  Proton or without vendor APIs (verify). Plan the fields as optional, and report
  unavailable values as unavailable, never as zero.
- **Card availability.** The 2160p census needs the 3080 Ti or the 6800 XT.
- **One-build allowance.** Publish a trimmed list only after the pre-share check passes.

## Explicitly out

- **Vulkan** stays shelved (KI-062 black player car, KI-063 Unlocked never activates, 4-7%
  slower than D3D12 on the Deck, KI-047 sync not ported). It is reopened only if a concrete
  reason appears, for example a vkd3d regression that Vulkan avoids.
- **No decompilation.** Fixes go through hooks, host code and the SDK.
- Public release hardening and distribution (section 17a, REL-019).
- Linux native, macOS, ultrawide.
- Native-refresh presentation, VRR, and an in-game settings overlay.
- Rendering defects KI-013, KI-015, KI-016, KI-019, KI-020 and KI-036 pop-in, unless M11
  work touches the same path.
- KI-033 extra sharpness stays deferred.
- Shipping `.xsh`, driver PSO blobs or Fossilize data.

## Spark routing (AGENTS.md)

Suitable bounded jobs:
- triage of the Deck session logs for `[content]`, save-copy and seed lines (M11-001,
  M11-008 (i));
- per-minute census extraction for the A/B tables (M11-003, -004, -006, -007);
- the test-inventory cross-check for the build-script fix (M11-013).

Each job is advisory, with its receipt recorded in the task evidence. Generated-code
reading of the private game image stays with the primary agent, as in the KI-035 fix.

---

## Owner priority update (2026-11-06)

The owner reached the mid/late game and reports constant stutter and frame-rate collapse at 100+ mph (KI-072). Within M11 this becomes the top performance item after the l71 owner checks (KI-068 Deck saves, KI-069/070 F10 pauses, KI-071 audio A/B, M11-002 diagnostics). Plan: attribution first (existing l70 session bursts, then an l71 high-speed run with cheap F10s), then a targeted fix; KI-067 host slowdown is analysed together with it.

## CP-thread cost plan (2026-11-06)

KI-072 attribution: the lag episodes were our own builds on the play PC (KI-067 cause found); the project-side line is headroom: the CP thread runs 95-96 % busy even when healthy (17-19 ms CPU + ~6.5 ms waits per frame at 6.5k draws, 1080p). Plan from [the CP cost plan](evidence/M11-cp-cost-plan-20261106.md), ranked by ms saved per day: (1) M11-015 read-back kick-interval A/B (`d3d12_readback_sync_kick_draws` 512 -> 256/128, hot-reload cvar; -1 to -3 ms CP busy; one owner run with three arms); (2) M11-016 fetch-constant identity cache in the SDK (compare before invalidating; per-slot memo keyed by the 6 raw dwords; lock-free scaled-resolve check; -2 to -3.5 ms PC, -3 to -5 ms Deck; 3-4 days); (3) M11-017 ThinLTO + PGO for the SDK runtime and host (-5 to -15 % CP user time); then bulk context-register writes, bindings fingerprint, M11-007 read-back steps 2-3. Measurement first: perf-split v=2 sub-fields (fetch-constant dwords changed, texture key changes/creations, packet counts, forced-wait backlog) in the next owner-internal build, and one 60 s WPR trace of the GPU Commands thread (owner, elevated shell). Gain ceiling on PC: the GPU sits at ~22 ms/frame, so CP cuts beyond 4-6 ms buy robustness, not fps.

## Build train (2026-11-06 evening)

- l72 (staging): host main `060dbcf` (KI-056 Fix A default off + key; M11-015 kick keys 256/128/1024; 32 keys) + SDK `d08813b`. Owner: album Fix A check, kick A/B (3 arms), carry-over l71 items.
- l73 (staging): host main `1dec761` (39 keys: fetch_identity_off/verify, readback_steady_exempt, 4 XMA keys; arm limit 2 KiB) + SDK `4a3aea1` (`mcla/m11-xma-doubling` = d08813b + ae35ae3 M11-016 fetch identity cache [default on] + e412c5c M11-007a steady-key exemption [default off] + 4a3aea1 KI-071 XMA fixes [default off]); install `private/l73-sdk-install`. Owner A/Bs: steady (base vs steady on South Central Tour + KI-047 livery routine), fetch (fetch-off vs base, fetch-verify), voice (base vs voice-c1 vs voice-fix on a cut-scene + radio), Deck steady. Then M11-017 ThinLTO+PGO.

## Perf benchmark route (owner, 2026-11-06)

Owner proposal, adopted: performance A/B drives use a fixed Arcade race instead of free-roam loops: a long route from the east side through South Central to the west (Arcade race **"South Central Tour"**), with weather, time of day and traffic density set explicitly in the Arcade setup, the same car, one run per arm, a restart between arms, F10 once per arm at the same checkpoint, HWiNFO + proc-sampler on, no builds running. Notes per run: race name, car, weather, time, traffic. First use: the l72 kick A/B (512/256/128).

**M11-015 result (2026-11-06, l72, South Central Tour, 1080p):** 512 / 256 / 128 give 36.0 / 35.9 / 35.8 fps; the read-back wait falls only 0.43 / 0.60 ms per frame (plan expected 1-3 ms) while CP CPU rises 0.9 ms (2-3x kicks, +5-11 submissions per frame, kernel time +0.7-1.8 ms): CP busy flat at 26.7-26.9 ms. Cause: at the forced wait the GPU is still 96-97 % busy with ALREADY submitted work and only ~40 draws are unsubmitted, so earlier kicks cannot shorten it. Decision: keep 512 on PC and Deck; closed. The remaining ~6 ms/frame needs a non-blocking composite read (asynchronous or one-frame-late KI-047 livery composite, or waiting only on frames where the composite changed) = M11-007 direction. Runs were clean of builds; a stray `find` process (one core) ran under all three arms equally. [Analysis](evidence/M11-015-kick-interval-ab-20261106.md).

## M11-007a steady-key exemption (2026-11-06)

Finding: the ~1 forced read-back wait per frame (6.3-6.5 ms PC, 7.1 ms Deck) is NOT the livery composite. KI-047 sync rule c (consecutive-frame resolve) catches a steady 256x256 k_8_8_8_8 resolve to guest 03702000 (256 KB, about draw 26 of every frame, consumed by the GPU as a texture in the same frame, no known CPU reader; never forced before l55, no defect ever tied to it). The real livery keys (050A0000 chain, 054A0000, 055A0000, 055E0000, 055F0000) match rules a/b and run at most 3 consecutive frames. Plan: rule c exempts a key resolved in each of the last 30 frames unless rules a/b match (10x margin); hot-reload cvar `d3d12_readback_sync_steady_exempt` (default false), dev key `readback_steady_exempt`, counters steady_exempt/keys/max_run and a `kind=steady_key` line; optional NoAccess trap on the exempt key's pages that makes it sticky-forced on a CPU touch. Expected: CP busy 26.7 -> ~20.4 ms/frame on PC (GPU-limited at 22.4 ms -> ~42-44 fps on South Central Tour), -7.1 ms/frame on the Deck (~14 %). Owner A/B: 2 fresh runs per arm + the KI-047 livery routine (police close-up cold/warm, garage vinyl apply, traffic spawns, car switch), then a `kick_draws=0` arm and a Deck A/B. [Design](evidence/M11-007-steady-key-design-20261106.md). Ships in l73 with M11-016.

**M11-007a first A/B (2026-11-07, l74, South Central Tour, profile 254a4ba8..., sessions base 00c0cb3c/2a20bf46, steady 2431c474/70c47fe2):** per-minute fps base 31.8-37.2 vs steady 40.2-41.5 (the predicted +6 ms/frame). Regression: in the second half of both steady runs textures of buildings, billboards and road signs flickered (liveries fine during the races). The log shows FIVE keys exempt, not one: a mip chain 05368000 512x512 / 05468000 256 / 054A8000 128 / 054B8000 64 (fmt 6, src_base 0; the lower three later ended by rule_b, i.e. they are CPU-read composites after all) plus 039CA000 256x256 (the PC counterpart of the Deck 03702000 key). The run>=30 rule is too broad; analysis of which key(s) cause the flicker and a narrowed rule (sticky a/b exclusion, no-mip-chain shape filter, or an allowlist of the single GPU-only key) in progress; the owner runs a separate steady livery session next.

**M11-007a analysis (2026-11-07):** clean runs (no builds, 35 W cap, 88-90 C). Gain: fps 34.8/34.4 -> 40.2/40.3 (PresentMon 36-37 -> 43-44), read-back wait 6.9/6.5 -> 0.7/0.8 ms, forced waits 1.19 -> 0.03 per frame, CP busy 27.7 -> 23.7 ms; the GPU is now the limit (23.2 of 24.9 ms, 93 %; NVIDIA power limiter on 66-77 % of samples). From race start to finish only `039CA000` was exempt (steady_exempt_keys=1); the four livery-chain keys (player-car composite, KI-047 family; its 1024 level is composited every frame for 30-80+ frames in the pre-race showcase, which breaks the "max 3 consecutive frames" assumption) were exempt for ONE resolve each before rule b ended them, hence no livery defect. The remaining forced waits are rule-b keys that now take 15-19 ms each because the GPU queue is full. Flicker: no steady_key event in the second half; the four flicker-time F10 frames show normal facades; the exemption changes only the guest-RAM copy of 039CA000, which under `some` delivery is refreshed every 4th resolve from a previous slot (0-3 frames old): a guest CPU reader (streaming/visibility/LOD feedback; cf. KI-038 billboard placeholders) would see stale data. Plan M11-007b: sticky a/b exclusion + no-chain shape filter + single-key cap, AND fast delivery for exempt keys (copy the previous slot on every exempt resolve, <=1 frame old, ~256 KB memcpy/frame); owner A/B flicker yes/no; if it still flickers, 039CA000 cannot be exempt without the read trap. [Analysis](evidence/M11-007a-steady-ab-l74-20261107.md).

**M11-007b implemented (2026-11-07, SDK `4bb8c02`):** sticky exclusion of any key that ever matched rule a/b (session-wide set, max 4096), chain-neighbour shape filter (next smaller level at base+len or next larger level ending at base, same format and src_base, or any overlap; direction chosen so 039CA000 stays exempt while 039BA000 ending at it is not its chain), single-key cap per frame, and `d3d12_readback_sync_steady_fast` (default true): every exempt resolve copies the previous completed slot into guest RAM without waiting (<=1 frame old), counters steady_fast_copies/kb/skipped. Ships in l75 for the owner flicker A/B. [SDK](evidence/M11-007b-steady-narrowing-20261107.md).

**M11-007b l75 A/B (2026-11-07, profile 7d6ff1dd..., sessions steady c8a6754d / 7352bf89, base b632bc55; owner pressed F10 at every flicker):** steady still flickers (1 event in run 1, more in run 2); fps steady 41-42 vs base 33-36. Counters: sticky/shape/cap exclusions 0 during the race (only 039CA000 exempt, as designed) but `steady_fast_skipped` runs in the hundreds per minute, i.e. the previous slot was often not complete when the exempt resolve came, so fast delivery fell back to the every-4th cadence much of the time. Conclusion so far: 039CA000 has a guest CPU reader that needs fresh data; a <=1-frame-old copy is not enough or was not delivered often enough. Next: (1) RE the guest consumer of that resolve (then a wait placed at the consumer instead of at draw 26 would cost little, since by then the GPU is usually done); (2) analysis of fast-copy vs skipped timing against the flicker F10s; (3) until then the steady default stays OFF.

**M11-007b l75 analysis (2026-11-07):** fast delivery was bimodal and sticky: 56.8 % / 48.4 % of exempt resolves fast (1 frame old), the rest fell back to the unfenced every-4th copy, which reads the previous slot while its GPU copy may still run: 3-6 frames stale or torn (this hazard predates l75). Cause: with the GPU at 91-94 % the CP runs >1 frame ahead and the queue never drains until a rule-b wait. All flicker F10s (l74 and l75) fall in the +130..+172 s stretch of the route; 5 of 6 flicker rings contain a skip episode, which is what chance predicts at a 74-82 % skip rate, so the data cannot separate "needs same-frame data" from "stale >1 frame". No flickered object was captured in any F10 frame. Performance: steady 41.0/41.5 fps vs base 34.5; read-back wait 0.58 vs 6.37 ms; GPU 91-94 % busy. Decision: M11-007c = for an exempt resolve whose previous slot is not complete, do a bounded (~20 ms) wait on that slot's submission fence instead of the unfenced copy (delivery always exactly 1 frame old, never torn; caps the CP lead at ~1 frame, which the GPU-bound frame does not need anyway), with counters; drop the unfenced 4th copy for exempt keys; lengthen the F10 flight ring to ~120 frames. Owner test: 2-3 steady runs through the +130..+172 s stretch; no flicker -> ship with this wait; flicker -> the reader needs same-frame data, keep forcing 039CA000 unless the consumer RE yields a consumer-side wait. [l75 analysis](evidence/M11-007b-steady-l75-analysis-20261107.md).

## M11-007d: complete the read-back at the lock (2026-11-07)

RE (M11-ki047-lock-fence-re): the game's CPU readers wait on the EVENT_WRITE_SHD fence, which the SDK CP writes before the host GPU finishes; the KI-047 forced waits mask that. Plan: SDK export `ReadbackSyncEnsureRange(phys, len)` (end the open submission if needed, wait on the copy's fence, memcpy the slot into guest RAM; counters) + a query export (is the range a read-back key, exempt, slot age); host `mcla_lock_probe` (REX_EXTERN on sub_82421CA0 logging resource/flags/base/start/len/lr when the range overlaps a read-back key) and `mcla_lock_readback_ensure` (call the export before the original lock). With the ensure hook, 039CA000 can stay exempt and later the rule a/b waits may go too (own A/B). Alternative measured arm: defer the fence write until earlier exempt copies landed (hardware-exact; pacing risk).

**M11-007c implemented (2026-11-07, SDK `ca2e332` + `1e65cc8`):** exempt resolves now deliver a fenced 1-frame-old copy (fast, or after a bounded fence-only wait of up to 20 ms on the previous slot) or force; the unfenced every-4th copy is gone for exempt keys; F10 resolve ring 200 lines (~120 frames); skip-episode counters. Host range API for M11-007d: `ReadbackSyncQueryRange` and `ReadbackSyncEnsureRange(phys, len, timeout_ms)` (asks the CP to end the open submission via a flag, waits on the copy's fence on the caller thread, copies under a generation check; counters `ensure_*`). [SDK](evidence/M11-007c-steady-bounded-wait-and-range-api-20261107.md). l76 = xma-walk b6d361e + steady3 1e65cc8 merged.

**M11-007c/d l76 runs (2026-11-07, profile 7d6ff1dd..., sessions steady 31689c39 / c959447b, steady-probe 93eb2404, steady-ensure cd91c47a / 4c6a2e31):** flicker in ALL steady arms; with steady-ensure whole objects (cars) despawned at times; in the first run two traffic cars hung in the air on the freeway. fps 40-41 vs ~33 base minutes. The lock probe (44k locks per minute) shows ZERO locks overlapping 039CA000 during the race (`keys=[]`, `overlapping=0`); the only read-back keys ever locked are the player-car livery chain (05368000/05468000/054B8000/054BF000) from lr 82410500 (the vinyl path), all pending=0, so `ensure` never fired. The "CPU reader of 039CA000" hypothesis is refuted. Remaining explanation, consistent with the KI-047 RE: the per-frame forced wait kept the CP within a frame of the GPU; exempt, the CP runs 1-2 frames ahead while the SDK writes EVENT_WRITE_SHD fences before the GPU reaches them, so everything the game synchronises through fences (visibility/culling data, memexport results, which the lock hook does not cover) comes from the wrong frame: flicker, despawns, floating cars. Next (M11-007e): hardware-exact deferred fence writes behind a cvar (write the fence value to guest RAM only when the GPU has completed the submission containing the event; the CP polls completion between packets; the guest spins on memory, the CP never waits on the guest), owner A/B on South Central Tour; until then the steady default stays OFF and 039CA000 stays forced.

**M11-007d l76 analysis (2026-11-07):** in l76 every exempt resolve was delivered fenced and 1 frame old (bounded wait on 41-57 % of frames, mean 3-4 ms, max 17 ms, no timeouts), CP lead capped at ~0.8-1.4 frames, yet the flicker rate did not drop vs l75 (unbounded lead) and the captured floating traffic car (steady 1, +91 s, 405 Freeway) came at the LOWEST lead; no flicker press ever fell in the highest-lead stretches; the game uses no memexport, its single occlusion query is non-blocking and the SDK answers "visible"; no consumer of an early fence was found (the fence word gates only the render thread's ring-space/frame-throttle waits). So the CP-lead/early-fence theory has no support; by elimination the likely mechanism is the game reacting to the changed pacing itself (40-42 fps GPU-bound vs 33-36 CP-bound: traffic/LOD/streaming placement at a different frame step; cf. KI-041, KI-043). Cheap control that needs no build: run steady and base in the launcher's Locked timing mode (equal frame rate): flicker gone with steady at equal fps = frame-rate dependence of the game, not the exemption. M11-007e deferred fences stay an experiment arm (default off) with the logging the analysis lists (BlockOnFence / throttle hooks, frames in flight at each swap). Steady default stays OFF for now. [l76 analysis](evidence/M11-007d-l76-analysis-20261107.md).

**SDK for l77 (2026-11-07):** `f6fc975` KI-071 W1 fix default ON; `590c484` M11-007e deferred fence writes behind `d3d12_fence_write_deferred` (default off; hardware-exact fence timing; expect to give back some fps; counters fence_*). Given the l76 analysis (no fence consumer found) this is an experiment arm, not the expected fix for KI-074; the Locked-mode control on l76 comes first. [SDK](evidence/M11-007e-deferred-fences-sdk-20261107.md).

**Locked-mode control (2026-11-07):** steady x2 and base at ~29 fps on South Central Tour: no flicker anywhere while the exemption ran every frame. Conclusion: the M11-007a/b/c exemption is data-safe; KI-074 is frame-rate dependent game behaviour above ~36 fps. Decisions: (1) the steady exemption can ship (it is the +6 ms/frame) but in Unlocked mode on the owner PC it lifts fps into the KI-074 range, so the default is tied to the KI-074 mitigation; (2) M11-007e deferred fences are no longer expected to matter for KI-074 (experiment arm only); (3) new task M11-018: KI-074 RE + mitigation (what breaks above ~36 fps; candidate host mitigations: a frame-rate cap option in the launcher at 36 for Unlocked, or a game-side dt fix behind a hook).

**Owner direction (2026-11-07):** "36 fps is still rubbish, we should fix it properly: playing at 60+ fps would be great." M11-018 goal = correct game behaviour in Unlocked mode at 60+ fps (traffic/LOD/streaming/visibility independent of the frame rate) via game-side fixes behind host hooks; a frame cap is at most a temporary experiment arm, not the deliverable.

**M11-018 plan (2026-11-07):** l77 gets dev keys for the flag experiments (`raw_timer_off`, `vblank_hz_72/80/88`) and arms steady-rawoff, steady-cap36/40/44; E4 probe in l78 after E1/E2 name the subsystem; the real fix = game-side dt/step handling behind a hook (goal 60+ fps correct).

**M11-018 after the l77 experiments (2026-11-07):** flicker = uncapped GPU-saturated pacing (>=90 % GPU), not dt; E3 is the decisive split (720p CP-bound 45+ fps vs 1440p GPU-saturated <36 fps); then a texture-cache arm and eviction/creation counters (l78); the floating car / despawn branch keeps E4.

**M11-007e first owner result (2026-11-07):** steady-fence 40-41 fps WITHOUT flicker (1 of 1; previous uncapped steady: flicker 7 of 7), base-fence 36.9 fps clean; deferred fences cost no fps here (max 1 frame deferred, no stalls). Pending: 2 more steady-fence runs; if clean, candidate defaults = steady exemption ON + deferred fences ON (then re-check KI-047 livery routine, Deck, and the E3 split becomes optional).

**M11-007e analysis (2026-11-07):** deferred fences bound the CP lead (fast-wait share 35-48 % -> 5-14 %) at no fps cost; flicker gone in 1 of 1. Pending: two more steady-fence runs; then l78 with the defaults flipped (exemption + fences on; W1 on) and the regression routine (KI-047 livery, KI-035 garage, Locked run, Deck loop, cold shader cache, photo mode), plus SDK instrumentation (fence-lateness histogram, CP-lead gauge, submissions by end reason). [Analysis](evidence/M11-007e-l77-fence-analysis-20261107.md).

**E3 (2026-11-07):** 720p steady 52-55 fps clean, 1440p base 34 fps clean; confirms the CP-lead discriminator. Next build l78: SDK defaults flipped (steady exemption ON + deferred fences ON + W1 ON) with off-keys for A/B, plus fence/CP-lead instrumentation; regression routine on PC and Deck.

**M11-007e confirmed (2026-11-07):** steady-fence 3 of 3 clean at ~40 fps vs 7 of 7 flickering without the fence. Process note: one of the confirmation runs was contaminated for two minutes by an SDK configure/build my agent started between the owner's runs (the idle guard checks only at start); rule tightened: during an owner test session all builds are held by explicit coordinator "hold/resume", not by the guard alone.

**l78 SDK (2026-11-07):** `d61c597` on `mcla/m11-defaults` = l77 SDK + defaults ON (steady exemption, deferred fences; W1 already on) + fence-lateness histogram + CP-lead gauge + submissions by end reason (`subm_primary_end` suspected for the extra submissions) + `d3d12_fence_defer_min_gap_us` (Deck arm 2000). [SDK](evidence/M11-l78-sdk-defaults-20261107.md).

## External repos survey (2026-11-08)

[Survey](evidence/M11-external-repos-survey-20261108.md). Adoption candidates for M11, in order: (1) four more Xenia Canary/Edge XMA fixes (6bef1fbed partial-frame loss at Work end, 7015473a7 frameless-packet skip count, 5dd1cdbbf loop start one bit short, ade7e610b no-progress loop end) taken from Canary (BSD-3), plus a diff of our W1 against Canary aee0871dd; (2) placed texture heaps (idea; Renan measured texture creation 0.19 ms -> 8-11 us, race 1 % low 50 -> 96 fps): directly targets the KI-072 drops; (3) blocking timer-queue wait instead of spin (idea; +17-21 % fps at 4 logical CPUs: Deck); (4) KI-026 warper finish for the never-called / never-started transition cases (re-implement from guest facts); (5) CP fast paths: bulk register writes (Canary), threaded submission (FH1-ReImagined MIT), hot pages out of watched ranges; (6) unlocked-fps smoothing/UI pacing/countdown hooks (KI-054); (7) save backup around CREATE_ALWAYS; (8) DXBC relative constant clamp (DEVICE_HUNG) and alpha-to-mask tolerance; (9) Reflex/Anti-Lag 2 via system nvapi64 + MIT headers (low value while CP-bound); (10) vblank clamp; (11) in-game pause-menu settings re-implemented from the published hook map, after asking LARecomp's author.

**l78 regression, part 1 (2026-11-08, owner):** two South Central Tour runs on the default arm (steady exemption + deferred fences + W1 on), Unlocked 1080p: no flicker, liveries look fine. Remaining: Locked run, garage trips, cold shader cache, cut-scene by ear, Deck (default, gap2000, both-off). Code for l79 (Canary XMA fixes, placed texture heaps, blocking timer wait) is being written without builds meanwhile.

**l78 regression, part 2 (2026-11-08, owner):** Locked run clean (nothing new, no flicker, no floating cars); cut-scene by ear on the default "all great" (KI-071 W1 default confirmed). Remaining: cold shader cache (in progress), garage trips, Deck.

**l78 PC regression PASS (2026-11-08, owner):** default arm Unlocked x2, Locked, cut-scene, cold shader cache (other profile + default-cold arm): "passed without errors or new problems". The Deck part (default / gap2000 / both-off) is not yet reported. l79 SDK code written on `mcla/m11-l79` (unbuilt at the time of writing): 58b6ddf Canary XMA fixes (`xma_canary_fixes`, default off; our W1 already covers Canary aee0871dd and is stricter), 4f3cbb7 timer-queue blocking wait (`timer_queue_blocking_wait`, default off), 8aabee4 placed texture heaps (`d3d12_texture_placed_heaps`, default off); provenance: Canary BSD-3 diffs; B and C from the idea only.

**l79 staged (2026-11-08):** host main `7c349e6` (66 keys) + SDK `0755836` (l78 defaults + Canary XMA fixes / timer blocking wait / placed texture heaps, all default off); candidate `private/l79-int-input` (mcla.exe 1180C3DC...), sign `private/l79-internal-prepare.ps1`, arms `private/l79-arms` (pc default/heaps/timer/xma/all3; deck default/timer/heaps64/all3), steps `private/l79-CHECKLIST.txt`. [Build](evidence/M11-l79-owner-build-20261108.md).

**l79 perf A/B result (2026-11-08, owner):** stutters at traversal/intersections much lower with the new switches (freeway exit 28-24 -> 32 fps on all3), peak fps unchanged. Analysis of which switch carries it (MCLA_TEXHEAP create time, MCLA_TIMERQ lateness, frame-interval p99 at the known spots) in progress.

**l79 xma arm (2026-11-08, owner):** cut-scene + radio by ear "fine": the Canary XMA fixes cause no audible regression. Remaining l79 owner items: Deck loops (default, timer, heaps64, all3) and the l78 Deck regression.

**l79 A/B decision (2026-11-08):** default ON next build: `d3d12_texture_placed_heaps` (256 MB; pre-create 1-2 heaps during loading to move the 6-9 ms heap-creation hitches out of races; verify once on the RX 6800 XT before tester defaults) and `timer_queue_blocking_wait` (neutral fps, better ticks, less CPU); `xma_canary_fixes` ON too now that the by-ear arm passed (no trigger seen in logs, no regression). Deck must confirm: high-resolution waitable timer available under Proton, timer lateness, heaps64 reserved vs budget, fallbacks, heap failures 0, p99 vs deck/default.

**l79 Deck A/B (2026-11-08, owner; sessions default cc29bd06, timer 942fa8b0, heaps64 e09a5bd0, all3 f6e174fc, usual city loop):** "slightly better", still ~12-15 fps when driving fast in the city, audio crackles because the CPU struggles (KI-060), repeated saves fine. Per-minute fps 13-23 in every arm. Analysis in progress.

**Deck plan after l79 (2026-11-08):** l78+l79 gave the Deck nothing measurable (~15 fps, 10 us/draw, = l64/l66): the fence/read-back savings were eaten by the memory watch (arm 11 + disarm 4.4 + write faults 6.6 = ~22 ms/f, 39 % of CP busy 57-60 ms/f); GPU only 43 %; render scale is irrelevant. Ranked Deck levers: (1) memory-watch cost under Wine (hot blocks v2 with copies batched per submission, coalesced protect calls; ~15 -> 16.5-17.5 fps); (2) CP fast paths: fetch identity verify/A-B (63 % of fetch dwords rewritten unchanged), bulk register writes (other packets 10.8 ms/f), bind/texture; (3) M11-003/KI-060 audio: polling-loop sleep cost + audio worker priority/affinity; (4) the guest render thread (~46 ms/f) caps the Deck at ~21 fps after 1-2; (5) small: threaded submission, min-gap, kick interval. Deck switches: heaps OFF on Proton (committed creation already ~17 us under vkd3d; heaps add 10-16 ms creation hitches unless pre-created at load), timer ON (try spin tail 50 us), xma neutral. PC: heaps + timer + xma ON.

**Deck memory-watch design (2026-11-08):** the CP-side cost is arm (~8-11 ms/f, 479 calls/f at 16.7 us each under Wine vs 0.7 us on Windows) plus upload; write faults (176/f at 36.6 us, ~6.4 ms/f) land on the guest render thread; 65 dynamic VB/IB ring blocks (16 MB) faulting every frame carry ~92 % of arms, faults and uploads. Plan "hot pages v2": per-page hotness, hot pages unwatched with GPU copies batched in one per-submission list executed first (avoids M10 v1's +10 ms GPU from copies between draws), texture-watched pages stay armed; expected Deck CP -4..-9 ms/f, render thread -2.7..-5.5 ms/f (~15 -> 16-17 fps; 20 fps also needs the CP draw/packet fast paths). Stage 0a: owner Deck run with `memory_watch_census=true`; stage 0b counters; stage 1 implementation on top of v1 `cdf469a`, Wine-only default; acceptance CP >= -4 ms/f. [Design](evidence/M11-deck-memwatch-design-20261108.md).

**l80 staged (2026-11-08):** host main `6b99e5f` (71 keys) + SDK `ca63f94` (platform defaults: PC heaps + timer + Canary XMA on with heap pre-creation; Deck heaps off, timer on, KI-060 short-sleep hires_timer + audio worker above_normal; KI-013 blend fix behind `blend_minmax_fix`); candidate `private/l80-int-input` (mcla.exe C3F37CA8...), sign `private/l80-internal-prepare.ps1`, arms `private/l80-arms` (pc default/blend/heaps-off; deck default/census/audio-off), steps `private/l80-CHECKLIST.txt`. Note: arm files are now 2019-2023 bytes against the 2048-byte limit (helpers copy them byte-exact; an editor re-save with CRLF would push them over and the whole file would be rejected): raise the limit to 4 KiB in the next host change (host + launcher). [Build](evidence/M11-l80-owner-build-20261108.md).

**KI-013 owner PASS (2026-11-08, l80 blend arm):** reflections fixed, no HUD/glow regression. Next SDK: `d3d12_blend_minmax_fix` default ON (all platforms; the Deck runs vkd3d D3D12 so the same path applies).

**Owner decisions (2026-11-08):** KI-056 Fix A default ON in the next builds; prepare the next volunteer build with the new defaults after the Deck analysis (publish only on the owner's "publish lNN"); publish the SDK with the new defaults to the fork as v0.20.0.10 once gates are green and the Deck shows no regression; the in-game menu (LARecomp-style) is deferred, no message to its author for now. Plan for l81: SDK = l80 + KI-013 blend fix ON + Deck zero-delay backoff (KI-060) + hot pages v2 if the census passes its gate; host = Fix A ON + arm limit 4 KiB.

**Process defect found (2026-11-08):** the Deck arm files generated since l71 (copied from the l69 fix arm with every key explicit) set `shared_memory_upload_ahead_16/_64=false` and `memory_watch_disarm_bridge_safe_gaps=false`, which by design counts as the owner's "off" arm and DISABLES the M10 Wine memory-watch defaults (upload-ahead 16, disarm bridging) that l66 "all" had: hence the l79/l80 Deck arm-call rise (917 calls/frame at 2.1 pages vs 479 at 14 pages on l66). l81 Deck arms must leave those keys out (or set them true); census on the corrected baseline next, then decide hot pages v2 (projected 1-4.8 ms/f CP on top of the corrected baseline; the render-thread fault saving is the stronger argument).

**SDK v0.20.0.10 published (2026-11-08):** ca63f94 (l80 SDK) on fork branch mcla/mcla-r-hotfixes. Next tag candidate: l81 SDK 2aff82e after the owner runs.

**l81 staged (2026-11-08):** host `a9de776` (75 keys, Fix A on, 4 KiB arms) + SDK `2aff82e` (blend fix on, zero-delay backoff + game audio thread priority under Wine); candidate `private/l81-int-input` (mcla.exe 3C7EDF3E...), sign `private/l81-internal-prepare.ps1`, sparse arms `private/l81-arms` (pc default/blend-off; deck default/census/zero-native; the M10 Wine defaults now apply on the Deck), steps `private/l81-CHECKLIST.txt`. [Build](evidence/M11-l81-owner-build-20261108.md).

**l81v volunteer candidate prepared (2026-11-08, unsigned):** same sources as l81 (host a9de776, SDK 2aff82e), volunteer options as l70 (dev-override reader compiled out, verified), inputs `private/l81v-volunteer-input` (mcla.exe 83C8955A...), `private/l81v-volunteer-prepare.ps1` (V2, allowance list = l81v only; older builds refused after publication), `private/l81v-CHECKLIST.txt`. Before "publish l81v": (1) publish SDK 2aff82e to the fork as v0.20.0.11 for source parity (it is unpublished; v0.20.0.10 = ca63f94), (2) refresh docs/volunteer-tester-guide.txt KNOWN PROBLEMS for the M11 fixes, (3) owner pre-share check offline. [Candidate](evidence/M11-l81v-volunteer-candidate-20261108.md).

**l82 prep (2026-11-08):** SDK `c40db4b` CP bulk register writes behind `cp_bulk_register_writes` (default off; side-effect registers keep the per-register path; counters regs_bulk/regs_single/bulk_runs). Needs a host key + arms for the owner A/B; candidate for the build after the l81 owner runs, together with hot pages v2 if the corrected-baseline census passes. [Notes](evidence/M11-l82-sdk-bulk-registers-20261108.md).

**l81 owner PC run PASS (2026-11-10)**; SDK v0.20.0.11 = 2aff82e published (source parity for l81v). Pending: l81 Deck loops (default / zero-native / census), l81v signing + offline pre-share check + "publish l81v".

**l81v published (2026-11-10):** DebugBundle `138b0ef`, allowance list = l81v only. Testers can use it now; l70 is withdrawn.

**KI-026 design ready (2026-11-10):** warp stall fix (state 19 exit + finished/never-started detector, `mcla_warp_stall_fix` default off, log MCLA_KI026_CAMFIX). Implementation candidate for l83 after the l82 Deck A/B. Open: which warper state KI-026 actually sits in (no sighting captured state; the l33 sighting predates the l69 state-20 fix). [Design](evidence/M11-ki026-warp-stall-fix-design-20261110.md).

**l82 owner build staged (2026-11-10), not signed:** host 9168872 + SDK c40db4b (CP bulk register writes behind `cp_bulk_regs`). `private/l82-internal-prepare.ps1`, `private/l82-CHECKLIST.txt`, arms `private/l82-arms` (pc/deck default + bulkregs). Purpose: Deck lever 2 A/B. [Evidence](evidence/M11-l82-owner-build-20261110.md).

**l81 Deck results (2026-11-10, D:\81):** fps 14.3 -> 16.4 (owner: PRESS START 24-27), CP busy 62.8 -> 50-54 ms/f, watch arm cost 12.5 -> 6.6 ms/f (M10 Wine defaults back). **Hot pages v2: NO-GO** (census: projected CP -1.2..-2.2 ms/f < -4 threshold; always-hot set unstable). **Audio (KI-060): backoff works on CPU, crackle unchanged**; the starved thread is the XAudio engine thread F8000534 (74-90 % of a core in title sound code). Next: (1) l82 Deck A/B for lever 2 (bulk register writes); (2) audio: profile 82146BE0/82147738, consider pinning F8000534 to its own core / Wine priority check; (3) KI-054 smoothing design. [Analysis](evidence/M11-l81-deck-analysis-20261110.md).

**KI-054 design ready (2026-11-10):** H1 = ambient-traffic thread reads the global dt while the racer splitter has it set to dt/N (our finding; no colleague repo fixes NPC traffic); H2 = hitch frames get a full 0.1 s step. Arms for l83 (default off): `ambient_dt_probe`, `ambient_dt_snapshot`, `raw_timer_hitch_hold`, `camera_ct_filters`. [Design](evidence/M11-ki054-traffic-jitter-design-20261110.md).

**l83 owner build staged (2026-11-10), not signed:** host d14621a + SDK c40db4b (same SDK as l82). KI-054 arms `ambprobe` / `snapshot` / `hitchhold` (all default off; probe on in every KI-054 arm). `private/l83-internal-prepare.ps1`, `private/l83-CHECKLIST.txt` (PC first: race + pursuit + free roam; then Deck). Not implemented: camera_ct_filters (needs a mid-function hook). [Evidence](evidence/M11-l83-owner-build-20261110.md).

**l82 Deck A/B result (2026-11-10, D:\82): lever 2 (CP bulk register writes) = no measurable effect** (packets_other 10.97 vs 10.98 ms/f; runs average 2 dwords). Rejected as a Deck gain; key stays for reference. Deck CP budget (51 ms/f) now needs a different lever: where packets_other actually goes (type-0 single-register packets, constant blocks ~75 k dwords/f, direct writes) must be profiled before the next SDK attempt. [Analysis](evidence/M11-l82-deck-analysis-20261110.md).

**l83 PC result and l84 (2026-11-10):** KI-054 H1 refuted (ambient worker never started in retail; inline main-thread path). l84 staged, unsigned: probe v2 for the inline path, `raw_timer_stats` key (80 keys), KI-075 launcher prefill. Test at 1440p Locked (jitter scales with frame time). [Evidence](evidence/M11-l84-owner-build-20261110.md).

**l84 PC run (2026-11-10):** 1440p Locked is a steady 30 fps; no jumps; probe v2 clean (1 step per timer update, no split). KI-054 now needs the Deck run of l84 (ambprobe, hitchhold).

**l84 Deck result (2026-11-10):** KI-054 cause confirmed (H2). Hitch hold v1 fixes the jumps (owner + instruments) but drops 2.2 % of game time at the debt cap. Next: hitch hold v2 (slew-rate limited step that still repays measured time), replay-tested against the Deck sequences, then l85. [Analysis](evidence/M11-l84-deck-analysis-20261110.md).

**l85 staged (2026-11-10), not signed:** hitch hold v2 (simulation on Deck-like dense hitches: max step ratio 1.06-1.10, lost time 0, vs v1 8-9 % lost and off 1.7-2.0 ratio). Deck first: arm hitchhold. [Evidence](evidence/M11-l85-owner-build-20261110.md).

**l85 Deck PASS (2026-11-10):** KI-054 hitch hold v2: no traffic jumps (owner), lost time 0. Pending: owner PC 1080p Unlocked run; then default-on in l86.

**l86 staged (2026-11-10), not signed:** KI-054 hitch hold v2 default on (owner PASS Deck + PC on l85). Arms: default, hold-off (control). `private/l86-CHECKLIST.txt`. [Evidence](evidence/M11-l86-owner-build-20261110.md).

**KI-060 plan (2026-11-10):** engine thread is compute-bound in two biquad filter effects. l87: `audio_filter_probe` + default-off native replacement of sub_82146BE0 (`audio_filter_native`), Deck A/B on underruns/s and F8000534 CPU %. [Analysis](evidence/M11-ki060-audio-filter-analysis-20261110.md).

**KI-074 reopened (2026-11-10):** flicker back on l86 at 1080p Unlocked ~50 fps (CP 2 frames ahead on steady key 039CA000). Owner A/B with existing keys on l86: `pc/steady-off` (readback_steady_exempt_off) and `pc/cap44` (vblank_hz_88). Fix direction: bound the CP lead to 1 frame (wait for the fence of frame N-1) or deliver read-back copies before writing the fence (SDK). This blocks a volunteer build from l86. [Evidence](evidence/M11-ki074-l86-recurrence-20261110.md).

**l87 staged (2026-11-10), not signed:** KI-060 audio filter probe + native sub_82146BE0 (84 keys; 56 suites green). `private/l87-CHECKLIST.txt`: PC afnative by ear, Deck afprobe then afnative. **KI-074 design:** fenced read-back delivery (`d3d12_readback_fenced_delivery`), counters first; SDK work for l88. [l87](evidence/M11-l87-owner-build-20261110.md), [KI-074 design](evidence/M11-ki074-fenced-delivery-design-20261110.md).

**l88 staged (2026-11-10), not signed:** host + SDK 7cbfe3b (local branch mcla/m11-l88, unpublished). KI-074 A/B: pc/default vs pc/fenced at 1080p Unlocked night race; also pc/afnative (KI-060 native filter by ear), deck/fenced. `private/l88-CHECKLIST.txt`. [SDK](evidence/M11-l88-sdk-20261110.md), [build](evidence/M11-l88-owner-build-20261110.md).

**l87 Deck result (2026-11-10):** native filter works (-30 % underruns) but is only 1.45x faster because std::fma is a CRT call (no -mfma), slow under Wine. l89 plan: fma-free native sub_82146BE0 + native sub_82147738 (expected ~4.7 ms/frame vs 5.33 needed = real time), then the 4.3 ms remainder; separately measure the fma library-call cost across the whole recompiled game on the Deck (possible large CPU lever). [Analysis](evidence/M11-l87-deck-analysis-20261110.md).

**l89 staged (2026-11-10), not signed:** KI-060 fma-free native filters (both), leaf probe; SDK 7cbfe3b as l88, so the l88 KI-074 arms are included. Deck first: afprobe then afnative. Open lever recorded: 9118 std::fma CRT call sites in the recompiled game (software under Wine): candidate for a CPUID-dispatched hardware FMA path, needs a Deck measurement first. [Implementation](evidence/M11-ki060-l89-implementation-20261110.md), [build](evidence/M11-l89-owner-build-20261110.md).

**KI-074 status (2026-11-11):** fenced delivery rejected (owner A/B on l88: fix active, still flickers). Bisection on l88 with existing keys: R1 steady-off (owner running), then 720p defaults / texture_placed_heaps_off / fetch_identity_off. If inconclusive: F10 ring of the last ~30 presented frames with per-frame counters. **KI-076** (60 Hz pacing) open for analysis. [Analysis](evidence/M11-l88-pc-ab-analysis-20261111.md).

**KI-074 after steady-off (2026-11-11):** lit-surface flicker gone with the exemption off (GPU 83-90 %, lead 1); exposure-latency idea refuted by code. Next A/B: exemption ON + `vblank_hz_88` (arm pc/cap44): if clean, the trigger is GPU saturation / 2-frame lead and the fix is a cheap lead bound; then 720p defaults. KI-077: owner protocol F10 at the same spot on laps 1-6, both arms; probe of the adapted-luminance inputs if it persists. [Analysis](evidence/M11-l88-steadyoff-analysis-20261111.md).

**KI-074 (2026-11-11, after cap44):** flicker <=> CP lead 2 under GPU saturation. l90: SDK `d3d12_frame_lead_limit` (default 0), counters lead_cap_waits / wait ms, deferred release of read-back buffers with a counter; owner A/B default vs limit=1. KI-077 not reproduced per lap. [Analysis](evidence/M11-l88-cap44-analysis-20261111.md).

**l90 staged (2026-11-11), not signed:** host (88 keys) + SDK 4ec7abd (local, unpublished). KI-074 A/B: pc/default vs pc/lead1 (then lead1-rel); includes l89 audio arms for the Deck. `private/l90-CHECKLIST.txt`. [SDK](evidence/M11-l90-sdk-20261111.md), [build](evidence/M11-l90-owner-build-20261111.md).

**l90 A/B invalid (2026-11-11):** the launcher still pointed at l88; lead limit untested. Separator across all runs: frame-end lead 2 / fences 2+ frames late (f2p), 0 in clean runs. Next: owner re-run l90 pc/lead1 (check receipt build D6AF6995 and the "MCLA dev override: frame_lead_limit_1=true" line); strict lead variant for l91. [Analysis](evidence/M11-l90-ab-invalid-and-all-runs-20261111.md).

**l91 staged (2026-11-11), not signed:** host (90 keys) + SDK ef72fd1 (strict / adaptive frame lead). Owner A/B: pc/lead-strict first, then lead-adaptive. Valid l90 lead1 run: no-op, still flickers. `private/l91-CHECKLIST.txt`. [SDK](evidence/M11-l91-sdk-20261111.md), [build](evidence/M11-l91-owner-build-20261111.md).

**l91 owner result (2026-11-11):** KI-074 surface flicker gone on lead-strict (40.6 fps avg) and lead-adaptive (44.8 fps avg; default 46-52). Next: 30+ min soak on the MAIN save with pc/lead-adaptive, then adaptive default-on in l92; then find what breaks at lead 2 to recover the last 2-3 fps.

**l91 lead-adaptive long run (2026-11-11):** dawn sidewalk flicker with lead2/f2p = 0; better separator = mid-frame "F-1 not finished" (fast waits); adaptive dropped as a candidate default. Owner next: l91 pc/lead-strict on the same route through dawn (no build needed). KI-077: bloom on characters at the hangout catalogued. [Analysis](evidence/M11-l91-adaptive-long-run-analysis-20261111.md).
