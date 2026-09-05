# Current state / AI handoff

Updated: 2026-09-05. Read this first, then relevant README-AI sections.
Replace stale facts here; Git, artifact hashes and returned evidence win.
Detailed history: [current correction](evidence/M7-016-deck-delay-hint.md).

## Active task and next action

- M6 closed at 0.8.2.2. M7 is **0/16 complete**. Active M7-016/KI-027
  stock-mode Deck performance; early M7-009 save lineage and M7-001 matrix
  preparation remain partial. [Priorities](M7-priorities.md).
- Finish this coherent Deck-lag investigation before a new task; no M7 push.
  Do not repeat accepted tests or require another long soak/physical wheel.
- **0.15.0.0 accepted/archived:** five complete F10s. FIRST two are gameplay;
  FINAL two are pause. Extra intermediate pause F10 is harmless, retained but
  not paired. Gameplay 886 outputs/73s = **12.14/s**; pause 561/56s = **10.02/s**.
- Published **0.15.1.0 / SDK v0.17.1.0** is a bounded codegen correction:
  exact db16cyc now emits simde_mm_pause(), not just a comment. Thirteen added
  statements in actual regenerated game; no other generated-code change.
  Prior cache/protection/diagnostics fixes retained. No renderer/coherency
  bypass, OS yield/Sleep or guest-clock scaling. Deck benefit is UNPROVEN.
- **Next owner action (about two minutes):** same Proton 9.0-4/Desktop settings.
  F10 READY, drive 30s, F10 READY. Open pause, settle 2s, F10 READY, remain
  paused 30s, F10 READY. Normal exit and full sync. Note motion/audio and
  whether the existing progressed car/profile loaded. No wheel or hour-long run.
- Analyze each explicit pair with matching PID AND positive creation_time_100ns.
  Do not splice modes/process lifetimes or compare different route loads as
  a controlled speedup. If no benefit, investigate measured descriptor-work
  reuse or a named sampling question, not repeat the rejected pause-wait theory.

## Exact published artifacts and save lineage

- Root VERSION **0.15.1.0**. SDK local tag/commit **v0.17.1.0** /
  `2c7c99c8f7683279295c45b0278cf91b1f88815e`; local checkpoint only, no push.
- Sync root: `private/bundles/M7-016`; current.txt selects
  `mcla-0.15.1.0-win64-7E11C5B992C824AE-C78486BB42C195E3-58B3385C89D9E499`.
- Receipt: `private/evidence/M7-016/bundle-20260905-123823-19c684f2/result.json`.
- Manifest SHA-256:
  `1BBFA7FD89BA3255DF55C5D8E737EF8FE55BDFF01E363084FB444B262589E250`.
  Immutable lock SHA-256:
  `F7376C6BB51F797174EF9909287059BC0822C2DC82215845A1267F43A9613878`.
- Full profile SHA-256 (4 files / 3,815,088 bytes, album included):
  `3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A`.
  Save: `58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585`.
  Header: `2FAEFBC7FF8CDEADBCE025D9BE914436ED05E27D041F72C8C88798612CFB0D60`.
  Album: `8EE49926E1DFE804DB923EF2512219FC7A078FEB50944C3D2BD4E9AC39F81588`.
- Accepted pairs: live-20260905T114748Z-340-1 / 114900Z-340-2, then
  114925Z-340-4 / 115021Z-340-5. Creation ticks 134330823743760390.
  114917Z-340-3 is not paired; 115027Z-340-6.partial is NOT a complete F10.
  Completed session session-20260905T114614Z-300: exit 0, 25 immutable,
  one snapshot, zero watcher errors, complete profile unchanged.
- Full mutable archive outside sync:
  `private/evidence/M7-016/returned-bundles/<0.15.0 bundle id>/33B9020756F0106C9BA71394AFADAED9CF7A3A853D01C4FD9F91CAA185B876BD`.
  Obsolete 0.15.0 payload pruned only after verified archive/successor. No
  fallback to M6 seed or loss of album/progress.
- Dedicated publication tree out/build/win-amd64-portable-release: binaries
  are independent copies; immutable game data alone may be hardlinked. Never
  rebuild legacy developer trees whose DLL hardlinks can mutate old children.

## Known, not assumed

- Gameplay/pause: GPU Commands uses 83.71/81.38% of one core; guest thread
  508 uses 89.42/91.32%. Pause has 7,426 draw packets/frame versus 4,661
  gameplay, about 59% more. Packets can include copy dispatch, not just draws.
- Resolve waits: gameplay 32/0.101s; pause ZERO. Resolve memcpy wall is
  4.403/2.703s, about 28.0/20.7 GB copied. Nested faults and inclusive spans
  overlap; do not add/subtract them as exclusive costs. Actual Vulkan fence
  wall 0.113/0.037s. Readback waits do not explain this pause slowdown.
- One exact-bin PE-unwind chain for thread 508 in F10 #2 reaches guest
  0x82412F98 via 0x82411E98: a bounded poll contains four iterations of eight
  delay hints. Prior codegen omitted their host statements. The captured
  thread's semantic role and fraction spent polling are not proven. Other
  Wine-module unwinds are rejected without matching images. Five F10s are
  not a sampling profile; 91% CPU is not proven to be all spin.
- x86 PAUSE is not an exact sixteen-cycle Xenon delay, nor OS descheduling.
  Poll responsiveness may change and FPS need not improve. ARM uses a
  different local SIMDe implementation; no macOS/ARM physical claim.
- All five accepted Deck F10 BMPs are all-black GDI captures. frame_signal
  labels this honestly, not a repaired capture. Photo Mode is separately
  fixed. READY means package saved. Visible F10 status can raise Steam FPS
  by repainting old output without new guest-output counts.
- Prior owner report that a traffic light no longer dithers is narrow;
  KI-015 and wider geometry/temporal parity remain open.
- Deferred Vulkan descriptor reuse is a source lead, NOT in this candidate.
  It must retain texture/sampler requests and handle layout/submission/frame/
  error lifetimes before reusing actual descriptor values.
- Campaign matrix retains eleven historical scenarios/static ABI review, not
  mandatory live nodes/dependencies or clean-campaign credit. No unlock edits.

## Verified gates and operational cautions

- SDK independent exact-opcode/semantics/source/test review plus AST scan/
  one fixture. Full RelWithDebInfo, Release and tagged Release:
  **1,838 executed / four known BitStream skips / zero failures**
  (1,842 registered). Red old emission is 0/9; three PPC state cases pass.
- Actual non-force regeneration: 67 files / 133,916,451 bytes / 65 C++.
  Exactly 13 added hints across four files; remove those lines and all
  prior 67 files are byte-identical. Dispatcher unchanged. Optimized native
  object contains eight consecutive F3 90 PAUSE instructions at the poll.
- Root AST scan/3 fixtures and independent pin/privacy/document review pass.
  PS7 + PS5.1: analyzer 12 positive/37 negative; diagnostics 5/20/146 source
  checks; bundle 1/13/41; save lineage 46; session 1/10; build matrix 1/6;
  alpha 1/31 plus real alpha validation; bootstrap 12/12.
- Clean publication/relocation/launcher/session/guest-free diagnostics pass,
  exit 0. No successor Deck cadence/audio PASS.
- Isolated exact-published-binary built-in Windows/D3D12 frontend control
  loads the copied progressed save, opens PAUSE and Settings; primary visually
  verifies the car/HUD and both menus. Exit 0 through CloseMainWindow, no
  force cleanup, source profile unchanged. Evidence: native-successor-control-
  shared-log below the return evidence root. This is not Deck/audio/driving
  or physical-controller proof. Initial private harness failed on log sharing
  and needed cleanup; retained separately as native-successor-control.
- First staging rename failed with access denied; holder unknown. Complete
  failed staging retained outside sync at
  private/evidence/M7-016/bundle-20260905-123424-a62b647d/failed-staging.
  Move and fresh full clean retry passed without weakening gates. Recurring
  transient publication failure remains a follow-up, not a proven ACL fix.
- Logs/receipts:
  private/evidence/M7-016/deck-return-20260905-0.15.0.0.
- Finish sync/stop writers before publishing; locks are not distributed.
  Never pick ambiguous save branches. Unrelated source-directory .stfolder
  remains untracked; do not stage private payload.
- Default sandbox/native UI helper setup failed earlier; scoped escalated
  shell works. Edits use Codex apply_patch. Native UI needs its supported
  helper; no custom SendInput/UIAutomation fallback. Built-in deterministic
  engine probes are separate from physical controller/F10 evidence.
