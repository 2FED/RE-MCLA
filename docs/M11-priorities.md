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
- l73 (next): SDK `ae35ae3` (M11-016 fetch identity cache, default on, + `d3d12_fetch_identity_verify`) cherry-picked onto `mcla/m11-content-unmount`, with dev keys `fetch_identity_off` / `fetch_identity_verify`; owner A/B off / on / on+verify on the same route; watch liveries/vinyls/album for stale textures; `tex_memo_mismatch` must stay 0. Then M11-017 ThinLTO+PGO.
