# Current state / AI handoff

Updated: 2026-09-05. Read this first, then relevant README-AI sections.
Replace stale facts here; Git, manifest hashes and returned evidence win.
Detailed history belongs in linked reports, not this file.

## Active task and next action

- M6 closed at 0.8.2.2. M7 is **0/16 complete**. Active: M7-016/KI-027
  stock-mode Deck performance; M7-009 save lineage and M7-001 preparation
  remain partial. [Order](M7-priorities.md).
- Finish the active Deck-lag investigation in this chat before a clean handoff.
  Do not create a new task or repeat accepted owner tests.
- **0.14.1.0 returned and is analyzed/archived.** The 100-second pair has
  12.13 new guest outputs/s; native watch protect calls average about 17 us,
  versus 64-71 us in 0.14.0.0. Different routes/draw load preclude a controlled
  FPS speedup claim. Owner perceives 10-15 FPS, still poor performance.
- Published **MCLA 0.14.2.0 / SDK v0.16.1.0** repairs regression-proven CPU
  primitive-cache invalidation and restores its 4096-index cache threshold.
  This is a bounded throughput candidate, not a measured cache-share claim.
  GPU coherency, guest rights, physical backing and timing remain intact.
- **Next owner action:** same Proton 9.0-4 / Desktop Mode/settings and route
  if possible: load progressed gameplay, F10 READY, about 60 seconds driving,
  second F10 READY, normal exit and completed sync. Report motion/FPS, audio
  breakup and any changed geometry. No long soak or physical wheel test.
- [Current correction/tests/publication](evidence/M7-016-deck-primitive-cache.md).
  [Accepted physical-backing return](evidence/M7-016-deck-physical-backing.md).
  Performance and stable audio remain unverified on the successor Deck build.

## Exact artifacts and save lineage

- SDK local commit/tag: `2fdf4ea4de5ff3c0a686cd627bd865a03d28640a`,
  `v0.16.1.0`. Root VERSION **0.14.2.0**. Local checkpoint only; no M7 push.
- Sync root: `private/bundles/M7-016`; current.txt selects:
  `mcla-0.14.2.0-win64-39F3F5564492EB26-CC3A3BF697F95FE2-58B3385C89D9E499`.
- Publication receipt:
  `private/evidence/M7-016/bundle-20260905-092703-c432b824/result.json`.
- Manifest SHA-256:
  `03DDB7B01419550D9C7EF30456ECE4955F4F80E417E08A150FEC74F1AC9E29F6`.
  Immutable-lock SHA-256:
  `1903B9865FED87E4636C0CF54C0C2779035852C2036C1C6C69F27DEAA7BB7170`.
- Complete returned profile SHA-256 (includes album):
  `3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A`.
  Save: `58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585`.
  Header: `2FAEFBC7FF8CDEADBCE025D9BE914436ED05E27D041F72C8C88798612CFB0D60`.
  Album: `8EE49926E1DFE804DB923EF2512219FC7A078FEB50944C3D2BD4E9AC39F81588`.
- Accepted 0.14.1 F10s: `live-20260905T083803Z-336-1` and
  `live-20260905T083943Z-336-2`; shutdown `084007Z-336-3.partial` is NOT
  a third accepted capture. Session `session-20260905T083540Z-296`: exit 0,
  25 immutable files, one snapshot, zero watcher errors; profile unchanged.
- Full old mutable return is archived outside sync:
  `private/evidence/M7-016/returned-bundles/<0.14.1 bundle id>/57DD284A930E75452F0073F8F7DA79AA33C18591D2DFC4B253C678658F776EFB`.
  The old payload was pruned only after verified archival. Successor uses this
  full return (4 files / 3,815,088 bytes), not an M6-only seed. No progress lost.
- Dedicated publication tree: `out/build/win-amd64-portable-release`.
  Runtime binaries are independent copies; only immutable game data may be
  hardlinked. Do not rebuild legacy trees that share older published DLL links.

## Known, not assumed

- Latest interval: 1,213 outputs / 1,219 closed frames / 100 seconds; frame
  wall 99.643 seconds, 6,199,135 draw packets / 63.474 seconds. Actual Vulkan
  fences: 7,313 / 0.339 seconds. Arm: 896,601 / 14.833 seconds; disarm:
  336,432 / 5.703 seconds. Faults: 92,659 / 7.117 seconds (lock 0.317,
  query 0.152, callback 6.536 seconds). Safe arm gaps 148; protection failures 0.
- Spans overlap/nest across threads: **do not sum as exclusive frame time**.
  Integer-us timings understate fast calls. Different workload means no matched
  32% FPS gain claim. Residual counters do not isolate primitive-cache cost.
- F10 ImGui actively repaints the old guest image while CAPTURING/READY; Steam
  may show 60+ then. This is not new game output. Idle-dialog repaint bug stays
  fixed, and guest-output counters exclude these UI-only presents.
- SDK cache repair covers half-open overlap, invalidation during conversion,
  bounded no-hit unwatch replies and small-input recomputation. No claim that
  a neon/reflection/minimap symptom is fixed. Current-draw atomicity is not
  promised; subsequent stale reuse after invalidation is prevented.
- New publication's clean relocation/launcher/session/guest-free diagnostic
  proof passes with exit 0. **No new Windows saved-gameplay/UI control**:
  native helper initialization failed before launch, including reset/retry.
  The preceding 0.14.1 saved-gameplay control remains historical only.
- Campaign matrix has eleven historical scenarios and static catalog ABI
  review, not live mandatory nodes/dependencies or clean-campaign credit.
  F10 is not an explicit catalog-memory snapshot; no unlock edits authorized.
- Open rendering/audio/camera/wheel/campaign/platform issues remain in
  [known issues](known-issues.md); no owner PASS promoted from automated tests.

## Verified gates

- SDK red baseline: 6/9 fail. Final real CPU/Memory cache fixture: **12 cases /
  800 assertions** pass on RelWithDebInfo and Release. Crossing-fixture bug
  found by independent review and corrected before final acceptance.
- Full all-target RelWithDebInfo, Release and tagged Release: **1,834 PASS /
  four known BitStream skips / zero failures** (1,838 registered). SDK AST 1/1.
- Root AST scan / 3 fixtures. PS7 and PS5.1: analyzer 4 positive / 8 negative;
  diagnostics 1 / 7 / 116 source checks; bundle 1 / 13 / 41 source checks;
  save-lineage 46 checks; session 1 / 10; build matrix 1 / 6; alpha 1 / 31.
  Bootstrap 12/12. Production non-Fixture alpha verification passes on both.
- Source/range/lock/test and parent-pin reviews accepted; final current-document
  identities corrected without rewriting historical evidence. No task closed.
- Final published payload/launcher rehash, documentation links, diff whitespace
  and 151-task/151-criterion ledger check pass; independent handoff review clean.
- Logs: `private/evidence/M7-016/deck-return-20260905-0.14.1.0`.

## Constraints and commands

- Owner has gamepad only; no physical T300. Prefer short targeted tests.
- Preserve complete profiles/albums separately from clean campaign fixtures.
  Finish sync and stop writers before publishing/pruning; locks are not
  distributed. Never pick ambiguous save branches silently.
- No coherency/readback bypass, guest timing change, brightness clamp or audio
  muting to hide symptoms. Stock performance is M7; dynamic FPS/native Linux/
  macOS/public packaging remain M8/M9. Local commits only until M7 closure.
- README-AI owns the workflow/ledger. [Build](build.md), [SDK pin](rexglue-sdk.md),
  [testing](testing.md). Analyzer: `scripts/analyze-portable-deck-performance.ps1
  -BundlePath <child>`; publisher: `scripts/build-portable-campaign-bundle.ps1`.
- Default execution/helpers failed sandbox setup. Scoped escalated exec with
  auto-review works; edits use Codex --codex-run-as-apply-patch. computer-use
  requires its native helper: no custom SendInput/UIAutomation fallback.
