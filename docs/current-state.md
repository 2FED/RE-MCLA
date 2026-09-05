# Current state / AI handoff

Updated: 2026-09-05. Read this first, then relevant README-AI sections.
Replace stale facts here; Git, manifest hashes and returned evidence win.
Detailed experiment history belongs in linked reports, not this file.

## Active task and next action

- M6 closed at 0.8.2.2. M7 is **0/16 complete**. Active: M7-016/KI-027
  stock-mode Deck performance; M7-009 save lineage and M7-001 preparation
  remain partial. [Order](M7-priorities.md).
- User wants to finish WHY Deck lags **in this chat**, then use a new thread.
  Do not create a new task yet or repeat already completed owner tests.
- **0.14.0.0 returned and is analyzed/archived.** Both F10s are complete.
  Its 100-second interval measures 9.17 outputs/s, costly native physical-watch
  protection calls and little actual Vulkan fence time.
- Published **MCLA 0.14.1.0 / SDK v0.16.0.0** is a bounded throughput
  candidate, not another counters-only release. Exact Proton source confirms
  SEC_RESERVE committed-range server requests during protect/query. Separate
  SEC_COMMIT backing for already-committed physical RAM avoids that path,
  preserving aliases, guest rights, watch behavior and coherency.
- **Next owner action:** same Proton 9.0-4 / Desktop Mode/settings, load the
  progressed game, F10 READY, about 60 seconds driving, second F10 READY,
  normal exit and completed sync. Report actual motion/FPS and audio breakup.
  No soak. Compare per-call arm/disarm/query cost and output cadence; physical
  improvement is not inferred from source, unit tests or Windows boot.
- [Current evidence and acceptance boundary](evidence/M7-016-deck-physical-backing.md).
  Previous attribution report is historical. If this correction is insufficient,
  inspect residual measured cost before adding another mechanism/counter.

## Exact artifacts and save lineage

- SDK local commit/tag: `c949866149be57585751cbe0eeccaf09743576bb`,
  `v0.16.0.0`. Root VERSION **0.14.1.0**. No milestone push.
- Sync root: `private/bundles/M7-016`; current.txt selects:
  `mcla-0.14.1.0-win64-ED21B43D461B2D00-D7F0EE25CF209E77-58B3385C89D9E499`.
- Publication receipt:
  `private/evidence/M7-016/bundle-20260905-073828-79d51fc3/result.json`.
- Manifest SHA-256:
  `A52C47196C0A29FC586A04E5AAE83699A2CF4B037191368CA6220FB46DFCC9D2`.
  Immutable-lock SHA-256:
  `D6718DF9D7A4B6A0E001331C629F9CC4742F133AFDD92E943CF73D91C3075D76`.
- Complete returned profile SHA-256 (includes album):
  `3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A`.
  Save: `58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585`.
  Header: `2FAEFBC7FF8CDEADBCE025D9BE914436ED05E27D041F72C8C88798612CFB0D60`.
- Accepted 0.14 F10s: `live-20260905T064756Z-336-1` and
  `live-20260905T064936Z-336-2`. Shutdown `.partial` is not accepted.
  Session `session-20260905T064606Z-296`: exit 0, 25 immutable files,
  one snapshot, zero watcher errors; profile unchanged.
- Full old mutable return is archived outside sync:
  `private/evidence/M7-016/returned-bundles/<0.14 bundle id>/87F1C924AAE8198DB3A94FD1170A35CC79588A78B980C43C3D638F4D8A2F0679`.
  Old payload was pruned only after validated archival. No progress discarded.
  The new child uses the complete actual return, not an M6-only save.

## Known, not assumed

- Latest Deck pair: 917 outputs / 925 closed frames / 100 seconds; frame wall
  99.070 seconds, draw-packet wall 77.315 seconds. Native fences: 5,577 calls /
  0.247 seconds. Arm: 473,423 calls / 33.733 seconds; disarm: 224,370 /
  14.380 seconds. Faults: 57,371 / 19.535 seconds (lock 0.639, query 3.846,
  callback 14.973 seconds). Only 54 safe arm gaps; zero protection failures.
- Spans overlap/nest across threads: **do not sum as exclusive frame time**.
  Per-call integer-microsecond truncation understates fast native operations.
  9.17 versus previous 6.91/s is not a matched-route improvement claim.
- Exact Proton 9.0-4 Wine commit: `117daf8d508860a8552f59ebdd6ee16361431136`.
  SEC_RESERVE protect/query uses get_committed_size/server requests even after
  MEM_COMMIT. SEC_COMMIT uses local metadata but still applies protections.
  This does not prove every measured microsecond or all lag came from IPC.
- Windows control on the **published** artifact reached saved car/HUD, pause,
  then resumed/AFK-camera rendering; primary inspected renderer BMPs.
  Native UI and ordinary image helper initialization failed, so existing opt-in
  START/capture requests were used. No physical F10 keypress or gameplay-F10
  pair was collected. Two START pulses were needed to reach saved gameplay.
- Control PID 37896 was deliberately terminated after inspection because native
  UI tooling was unavailable (exit -1). **Not a spontaneous crash, graceful-exit
  PASS or Deck performance PASS.** Published guest-free relocated probe exited 0.
  Control profile remained unchanged; logs/diagnostics were archived from bin/
  into the isolated run and the published 25-file lock was reverified.
  Private control: `private/evidence/M7-016/deck-return-20260905-0.14.0.0/windows-control-gameplay`.
- Dedicated publication build tree: `out/build/win-amd64-portable-release`;
  runtime binaries are independent copies. Do not overwrite legacy developer
  output trees while relying on their old hardlinked artifacts.
- Open: reflections/shimmer/minimap/music/neon/white paint, wheel centering,
  clean new-game/profile, Race Back, successor-save continuity and Deck matrix.
  [Known issues](known-issues.md) owns severity/scope.
- Campaign matrix has eleven historical scenarios and static catalog ABI
  review, not mandatory live nodes/dependencies or clean-campaign credit.
  F10 is not an explicit catalog-memory snapshot; no unlock edits authorized.

## Verified gates

- SDK all-target RelWithDebInfo and tagged Release: **1,822 PASS / four known
  BitStream skips / zero failures** each (1,826 registered). Five new physical
  mapping cases / 79 assertions; SDK AST 1/1. Initial stale PPC runner failure
  was resolved by rebuilding ALL targets, then rerunning BOTH full suites.
- Root AST scan / 3 fixtures. PS7 and PS5.1: analyzer 4 positive / 8 negative;
  diagnostics 1 / 7 / 116 source checks; bundle 1 / 13 / 41 source checks;
  save-lineage 46 checks; session 1 / 10; build matrix 1 / 6. Bootstrap 12/12.
- Clean final publication, relocated launcher/session/guest-free diagnostics
  PASS (exit 0). Final bundle identity rechecked after isolated native control.
- Independent SDK/root review: current alpha-version regex drift found and
  fixed, plus two equivalent IndexOf substitutions for unsupported PS5.1 Contains
  overloads; actual non-Fixture alpha and 1-positive/31-negative suite pass on
  both PowerShell versions.
  No task closed; physical Deck recovery still gates M7-016/KI-027.

## Constraints and commands

- Owner has gamepad only; no physical T300. Prefer short targeted tests.
- Preserve complete owner profiles, including album, separately from clean
  campaign fixtures. Finish sync and stop all writers before publishing/pruning;
  local locks are not distributed locks. Never pick ambiguous branches silently.
- No coherency/readback bypass, guest timing change, brightness clamp or audio
  muting to hide symptoms. Stock performance is M7; dynamic FPS/native Linux/
  macOS/public packaging remain M8/M9. Local commits only until M7 closure.
- README-AI owns architecture/workflow/ledger. [Build](build.md),
  [SDK pin](rexglue-sdk.md), [testing](testing.md).
- `scripts/analyze-portable-deck-performance.ps1 -BundlePath <child>` is
  read-only; `scripts/build-portable-campaign-bundle.ps1` builds/publishes/prunes.
- This turn's default exec/apply_patch, node_repl and view_image helpers failed
  sandbox initialization. Scoped escalated exec with auto-review worked;
  apply_patch used the Codex --codex-run-as-apply-patch entry point. Do not
  bypass unavailable native UI with custom SendInput/UIAutomation scripts.
