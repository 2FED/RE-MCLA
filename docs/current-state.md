# Current state / AI handoff

Updated: 2026-09-05. Read this first, then relevant README-AI sections.
Replace stale facts here; Git, artifact hashes and returned evidence win.
Detailed history: [current experiment](evidence/M7-016-deck-resolve-readback.md).

## Active task and next action

- M6 closed at 0.8.2.2. M7 is **0/16 complete**. Active M7-016/KI-027
  stock-mode Deck performance; early M7-009 save lineage and M7-001 matrix
  preparation remain partial. [Priorities](M7-priorities.md).
- Finish this coherent Deck-lag investigation before a new task; no M7 push.
  Do not repeat accepted tests or require another long soak/physical wheel.
- **0.14.2.0 accepted/archived:** 947 new outputs/65s = **14.57/s**.
  BOTH F10s were gameplay; user pressed pause only afterward. Perceived
  steadier 10-15 FPS and slower pause at 5-8 are not a controlled comparison.
  Traffic light reportedly no longer dithers; KI-015 remains open broadly.
- Published **0.15.0.0 / SDK v0.17.0.0** is diagnostic, NOT a performance fix:
  seven resolve-copy/bytes/wait counters, strict process-lifetime pairing and
  honest GDI frame-quality metadata. Prior cache/protection fixes retained;
  no readback/coherency/scheduling/guest-timing policy changed.
- **Next owner action (about two minutes):** same Proton 9.0-4/Desktop settings.
  F10 READY, drive 30s, F10 READY. Open pause, settle 2s, F10 READY, remain
  paused 30s, F10 READY. Normal exit and full sync. Four F10s yield two
  homogeneous intervals; do not include the pause transition in either.
- Use analyzer -FirstF10/-SecondF10 explicitly per pair. Require matching PID
  AND positive creation_time_100ns. Old packages need -AllowLegacyPidOnly and
  independent same-session proof; returned ProcessLifetimeVerified stays false.

## Exact published artifacts and save lineage

- Root VERSION **0.15.0.0**. SDK local tag/commit **v0.17.0.0** /
  `e11b30af9e20c36d19ef4ee328473b7fb5627478`; local checkpoint only, no push.
- Sync root: `private/bundles/M7-016`; current.txt selects
  `mcla-0.15.0.0-win64-185AB35EEEB7532F-8D6BF5EF8BA689E3-58B3385C89D9E499`.
- Receipt: `private/evidence/M7-016/bundle-20260905-113820-20fbffe4/result.json`.
- Manifest SHA-256:
  `C097FA96E9A38541643C201A96C13C01901B647848478174356DF6949AEA2142`.
  Immutable lock SHA-256:
  `BDAE69ABE0B6DEED5282BE6147825AD766B3FC295E4D086275593093287D6238`.
- Full profile SHA-256 (4 files / 3,815,088 bytes, album included):
  `3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A`.
  Save: `58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585`.
  Header: `2FAEFBC7FF8CDEADBCE025D9BE914436ED05E27D041F72C8C88798612CFB0D60`.
  Album: `8EE49926E1DFE804DB923EF2512219FC7A078FEB50944C3D2BD4E9AC39F81588`.
- Accepted 0.14.2 pair: `live-20260905T104122Z-340-1` and
  `live-20260905T104227Z-340-2`; `104311Z-340-3.partial` is NOT accepted.
  Completed session `session-20260905T104009Z-300`: exit 0, 25 immutable,
  one snapshot, zero watcher errors; complete profile unchanged.
- Full mutable archive outside sync:
  `private/evidence/M7-016/returned-bundles/<0.14.2 bundle id>/7C95E74A68B90D5507C262239ED3FE7A7F9B362DB9296495B13D44063E0A7392`.
  Obsolete 0.14.2 payload pruned only after verified archive/successor. No
  fallback to M6 seed or loss of album/progress.
- Dedicated publication tree `out/build/win-amd64-portable-release`: binaries
  are independent copies; immutable game data alone may be hardlinked. Never
  rebuild legacy developer trees whose DLL hardlinks can mutate old children.

## Known, not assumed

- 0.14.2: 957 closed frames / 3,360,844 draws; 64.132s frame wall / 40.758s
  inclusive draw time. Actual Vulkan fences 5,748 / 0.413s. Arm 501,436 /
  9.192s; disarm 235,499 / 4.064s; faults 60,426 / 5.015s; protect failures 0.
  Arm/disarm about 18.33/17.26 us. GPU Commands 80.06% of one core, one guest
  thread 90.55%. These are nested/aggregate spans, NOT exclusive frame shares.
- Different route/draw load versus 0.14.1 (12.13 outputs/s) prevents a
  controlled 20% gain claim. No pause-specific cap or root cause proven.
- MCLA globally enables fast resolve readback for working Photo Mode. Extra
  pause resolve memcpy/watch faults/waits are a named hypothesis; new counters
  measure it without disabling readback. Copy timer excludes mapped-memory
  invalidation; counts/time/bytes are nontransactional.
- Both accepted Deck F10 BMPs were all-black GDI captures. New frame_signal
  reports source/RGB class, not visibility/freshness. This does NOT fix black
  F10 images or undo the separately accepted Photo Mode fix. READY still
  means package saved. Visible F10 status can raise Steam presentation FPS
  by repainting old output; it adds no new guest-output counts.
- Separate deferred source finding: VulkanPresenter::CaptureGuestOutput lacks
  CPU invalidation for potentially noncoherent readback memory. NOT the F10
  producer. Do not wire its unbounded tracker wait into the dump worker;
  future renderer capture needs bounded waits and output/resource ownership.
- New Windows evidence is guest-free relocation/diagnostics, not saved gameplay
  or actual Vulkan branch execution. No successor Deck/audio/visual PASS.
- Campaign matrix retains eleven historical scenarios/static ABI review, not
  mandatory live nodes/dependencies or clean-campaign credit. No unlock edits.

## Verified gates and operational cautions

- SDK independent scope review + AST scan/1 fixture. Full RelWithDebInfo,
  Release and tagged Release: **1,834 executed / four known BitStream skips /
  zero failures** (1,838 registered). Seven XAM fixtures repeat five times per
  configuration. Initial two fixture failures were cross-process temp-folder
  collision, fixed with atomic owned-directory creation; no production saves.
- Root AST scan/3 fixtures. PS7 and PS5.1: analyzer **12 positive/37 negative**;
  diagnostics **5/20/146 source checks**; bundle 1/13/41; save lineage 46;
  session 1/10; build matrix 1/6; alpha 1/31 and real alpha validation PASS.
  Bootstrap 12/12. Pure nonshipping frame classifier 24 checks.
- Clean publication/relocation/launcher/session/guest-free diagnostic pass,
  exit 0. Real state includes positive creation identity and seven zero-default
  resolve fields. Its GDI frame correctly records all-black, 1920x1200.
- First publication failed at atomic staging rename (access denied, holder
  unknown). Old current remained intact. Exact failed staging retained under
  `private/evidence/M7-016/bundle-20260905-113140-07d1bccd/failed-staging`.
  A fresh clean retry passed without weakening checks or bypassing relocation.
- Initial alpha pin gate caught the escaped regex still naming old SDK;
  corrected and reran both host suites. Initial failure logs retained.
- Logs: `private/evidence/M7-016/deck-return-20260905-0.14.2.0`.
- Finish sync/stop writers before publishing; locks are not distributed. Never
  pick ambiguous save branches. An unrelated source-directory .stfolder marker
  appeared during this turn; preserve it untracked, do not stage private data.
- Sandbox/default native helper setup failed earlier; scoped escalated shell
  works. Edits use Codex apply_patch engine. Native UI needs its supported
  helper; no custom SendInput/UIAutomation fallback. No new GUI control PASS.
