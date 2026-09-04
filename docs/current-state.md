# Current state / AI handoff

Updated: 2026-09-05. Read this first, then relevant `README-AI` sections.
Replace stale facts here; do not append a running transcript. Actual Git state,
manifest hashes, and returned evidence take precedence over this snapshot.

## Active work and next action

- M6 closed at `0.8.2.2`; M7 is **0/16 complete**. M7-016 is active.
- Priority: usable stock-timing Deck gameplay and cross-build save continuity.
  See [the reviewed M7 order](M7-priorities.md). M7-009's early portable tooling
  subset is implemented/tested; the full task is in progress, not complete.
- Published runtime: **MCLA 0.13.0.0 / ReXGlue v0.14.0.0**. Repository revision
  **0.13.2.0** changes developer-side save handoff only; it does not
  publish a replacement binary or invalidate the pending physical comparison.
- Waiting for one same-process F10 pair on that build: gameplay, F10 `READY`,
  30--60 seconds driving, F10 `READY`, normal exit, completed sync. Keep
  Proton 9.0-4 and Desktop Mode fixed. Compare real movement/audio and the
  presentation overlay. No long soak or wheel required.
- Autonomous next work: campaign-matrix preparation (M7-001). Save-lineage
  tooling is reviewed/tested; actual successor reload remains pending alongside
  the Deck pair. Neither establishes a Deck PASS.

## Exact artifact and evidence

- Root runtime-change commit: `6d9bec7` (local, not pushed).
- SDK commit/tag: `5cc7134845504dd60ba66090880be766a58ab2bb`, `v0.14.0.0`.
- Stable sync root: `private/bundles/M7-016`; `current.txt` selects its child.
- Pending child:
  `mcla-0.13.0.0-win64-2E813D203DF298CF-626A6D06CD5B9347-58B3385C89D9E499`.
- Publication: `private/evidence/M7-016/bundle-20260904-195940-318397c2/result.json`.
- Manifest SHA-256:
  `2F0F0D970DE19D153AC107E33507DD2A88E8206202344E2E4A82E14F6651A086`.
- Seed save SHA-256:
  `58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585`.
- Latest analyzed return: **0.12.0.0**, archived under
  `private/evidence/M7-016/deck-return-20260904-desktop-vulkan-0.12.0.0`.
  Complete F10s: `live-20260904T190632Z-340-1` and
  `live-20260904T190745Z-340-2`. The returned profile was unchanged.
- Full interpretation/tests: [Deck evidence](evidence/M7-016-deck-performance-diagnostic.md).

## Known, not assumed

- Windows: accepted one-hour mixed gameplay, prior frontend soak, delivery
  transition, non-black Photo Mode, input/fullscreen evidence. These are
  bounded artifact/route claims, not a completed campaign.
- Deck: gameplay, garage, Steam Input, fullscreen startup, F10 and sync return
  work. Playable performance/audio and the full recovery matrix are unverified.
- Measured new-output cadence: D3D12 7.34/s, Vulkan 8.86/s, Desktop Vulkan
  7.59/s. These are not simulation ticks or MangoHUD FPS. GPU-clock forcing
  did not resolve the symptom.
- Confirmed defect: idle F10/achievement dialogs repeatedly repainted old
  output. 0.13.0.0 detaches empty overlays; native tests pass, physical
  speed/audio improvement remains unknown.
- New F10 counters separate command-ring idle, WAIT_REG_MEM and successful
  non-MMIO write-fault count/time. Durations can overlap across threads; do
  not subtract their sum from frame wall. OS/Wine dispatch is not fully timed.
- KI-028: developer-side containment implemented. The builder validates complete
  return/session graphs, archives all branches/evidence, preserves ancestry
  across generations, and gates publication/pruning with local locks and
  rechecks. See [lineage evidence](evidence/M7-009-portable-save-lineage.md).
  Physical successor reload is still pending; no observed lost progress.
  Old two-file-only seeds cannot prove whole-profile ancestry automatically.
  Missing retained starts, photo-only drift or incomplete sync stop publication
  without replacing anything. Explicit branch choice needs owner authority.
- Open: KI-001 clean new-game/profile flow; KI-013 reflections; KI-015
  coverage shimmer; KI-016 minimap; KI-017 music; KI-019 neon; KI-020 white
  paint; KI-022 wheel centering; KI-026 Race Back; KI-027 Deck performance.
  [Known issues](known-issues.md) owns severity, scope and disposition.

## Constraints to preserve across chats

- Owner tests via gamepad; physical T300 access is unavailable. Do not ask
  for wheel tests or claim an unverified centering fix.
- Races/story/deliveries may supply normal coverage. Prefer short targeted
  regressions; no automatic two-hour repeat for each patch. Future full
  campaign passes are resumable, not single-sitting marathons.
- Keep the owner's progressed profile separate from isolated clean campaigns.
  Never overwrite it with a baseline. Archive verified complete returns
  before pruning; keep private mutable state in the synchronized bundle.
- One writer per bundle/worktree; wait for sync completion. Serialize builds,
  native probes and private evidence writes.
- Do not disable correctness/coherency/readback, change guest timing, clamp
  global brightness or mute audio to hide symptoms. Stock-mode optimization
  is M7; dynamic FPS/native Linux/macOS/public packaging remain M8/M9.
- Local commits only until milestone closure; no push now. Follow AGENTS.md
  usage-limit and scoped-agent rules.

## Entry points and verification

- Architecture: [architecture.md](architecture.md), README-AI sections 2--6.
- Commands: [build.md](build.md); pin: [rexglue-sdk.md](rexglue-sdk.md).
- Tests: [testing.md](testing.md); ledger/criteria: README-AI sections 15/18/21.
- Pair analyzer: `scripts/analyze-portable-deck-performance.ps1`; read its
  parameters and use same-process returned packages, never mixed builds.
- `scripts/build-portable-campaign-bundle.ps1` rebuilds/publishes/prunes;
  it is not read-only. Its lineage gate also applies to `-PruneOnly`. Finish
  sync and stop writers on every host first; local locks are not distributed.
- Last runtime gates: SDK Release and RelWithDebInfo each 1,809 PASS / four
  known skips; AST 3/3; analyzer 3 positive / 4 negative; diagnostics
  1 positive / 7 negative / 53 source checks; bundle 1 positive / 13 negative /
  36 source checks; bootstrap 12/12; clean 0.13.0.0 relocation PASS.
- Save-lineage gates: 46 isolated checks on PowerShell 7 and Windows PowerShell
  5.1; 47 with the existing native launcher's verify-only fixture; bundle
  1 positive / 13 negative / 36 source checks, session 1 positive / 10 negative,
  AST scan clean / 3 fixtures. No actual publication, pruning or owner save edit.
- Prior documentation review: independent review has no blockers; AST scan
  clean / 3 fixtures pass; 151/151 task/criterion coverage, 16 unchanged M7
  tasks, 28 unique issue IDs and local links checked. Runtime was not rebuilt.

## New-chat starter

“Read AGENTS.md, docs/current-state.md, docs/M7-priorities.md and the relevant
README-AI/evidence sections. Verify the working tree and current.txt. Continue
the next bounded M7 task; distinguish the published 0.13.0.0 Deck build from
later documentation changes. Preserve the progressed save, do not push, and
do not claim physical results that have not returned.”

Refresh this handoff at task/experiment boundaries: goal, changes, tests,
pending/returned artifact, save identity, rejected hypotheses, blocker and
next action. Link detailed history instead of copying the old chat.
