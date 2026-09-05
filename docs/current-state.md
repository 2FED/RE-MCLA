# Current state / AI handoff

Updated: 2026-09-05. Read this first, then relevant README-AI sections.
Replace stale facts here; actual Git, manifest hashes and returned evidence
take precedence. Keep detailed history in linked reports, not this file.

## Active task and next action

- M6 closed at 0.8.2.2. M7 is **0/16 complete**; M7-016/KI-027 is the active
  performance investigation. M7-009 portable lineage and M7-001 preparation
  remain partial, not closed. [Order](M7-priorities.md).
- User explicitly wants to finish WHY Deck lags **in this chat**, then move to
  a new thread at a clean outcome. Do not create a new task yet.
- The requested **0.13.0.0 pair has returned, been analyzed and archived**.
  Do not ask for it again. Owner confirms truthful 2--10 FPS overlay after
  idle-dialog fix; real gameplay/audio remain unchanged.
- Published **MCLA 0.14.0.0 / SDK v0.15.0.0** is a targeted diagnostic
  candidate, NOT a throughput fix. Await its same-process F10 pair: progressed
  gameplay, F10 READY, about 60 seconds driving, F10 READY, normal exit and
  completed sync. Keep Proton 9.0-4 / Desktop Mode/settings fixed. No soak.
- Decision rules: high watch time plus safe splits -> bounded protection-run
  coalescing; high fault-lock or arming-lock time -> lock ownership; high actual fence time
  -> specific wait/readback; high draw preparation with low others -> narrow
  preparation path. [Full current experiment](evidence/M7-016-deck-command-memory-cost.md).
- No safe, evidenced throughput correction is identified yet. Do not bypass
  coherency/readback, guest rights or timing based on a raw stack lead.

## Exact artifacts, save and evidence

- SDK local commit/tag:
  `a88eefcec058975bc01be54ab7a8ee424ce73630`, `v0.15.0.0`.
  Root VERSION 0.14.0.0. Local checkpoint only; no milestone push.
- Sync root: `private/bundles/M7-016`; current.txt selects:
  `mcla-0.14.0.0-win64-92EA61AE9EFC1DF8-BEF9742FB0FFF9D4-58B3385C89D9E499`.
- Publication:
  `private/evidence/M7-016/bundle-20260905-054033-dae9f0b0/result.json`.
- Manifest SHA-256:
  `CA5C1FEEBC4BE7ACE5956489BD606167438F008CE8DCCC0262E016242D787CA3`.
- Immutable-lock SHA-256:
  `AFD02379252AB1A490C224C36E8D78018EECAC56983E0F4D28722288173F5DB0`.
- Selected **complete returned** profile SHA-256:
  `3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A`.
  It includes the photo album, not just the save/header.
- Save SHA-256:
  `58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585`.
  Header:
  `2FAEFBC7FF8CDEADBCE025D9BE914436ED05E27D041F72C8C88798612CFB0D60`.
- Accepted 0.13 F10s: `live-20260905T044355Z-332-1` and
  `live-20260905T044532Z-332-2`. The shutdown .partial is not accepted.
  Session `session-20260905T044215Z-292`: exit 0, 25 immutable files verified,
  one save snapshot, zero watcher errors; save unchanged.
- Full old mutable return is outside sync under
  `private/evidence/M7-016/returned-bundles/<0.13 bundle id>/33C82BF70DF845F487A63B3E362909146D0712E7C71A903D48203914E5466EC4`.
  The obsolete payload was pruned only after validated archival. No progress
  discarded; current sync root retains one child.
- Native control and private maps:
  `private/evidence/M7-016/deck-return-20260905-0.13.0.0`.
  Dumps/raw maps stay private. Reconstructed maps match exact executable
  .text hashes, but raw stack words are NOT validated unwind/profiling data.

## Known, not assumed

- Latest Deck interval: 663 new outputs/96 s = **6.91/s**; 683 closed frames,
  140.764-ms average frame wall, 60.34 vblanks/s. No output refresh failures.
  Old measured submission spans cover 5.72% of frame wall; opening fence and
  ring-idle spans are tiny, WAIT_REG_MEM totals 5.105 s.
- 52,909 handled non-MMIO write faults aggregate 17.879 s; this includes
  lock/query/callback cost across threads and excludes earlier OS/Wine fault
  dispatch. It is not exclusive frame time or a complete root-cause proof.
- New counters split primary/draw/swap/interrupt, actual Vulkan fence calls,
  protection arm/disarm and fault lock/query/callback cost. Includes 21
  fixed-size scalars, no per-page/packet journal, no scheduling/coherency edit.
  Per-span integer-microsecond truncation undercounts short native calls;
  compare counts too, not just approximate cumulative time.
- Isolated Windows Vulkan control reached saved gameplay visually, then
  collected two F10s: all 21 fields monotonic, zero protection failures,
  29.33 new outputs/s. Stationary including AFK camera, NOT Deck driving
  parity. Pre-bump host 0.13.4.0 + final SDK 0.15.0.0. Closed via Alt+F4.
  Keyboard START pulses did not work; existing opt-in garage START request did.
- New publication uses a **dedicated portable build tree** and independently
  copied runtime binaries (all six verified single-link). Legacy developer
  outputs may still have old hardlinks: do not overwrite them while relying
  on a hardlinked artifact. Game hardlinks are unchanged/optional.
- KI-028 developer containment passes full-tree lineage/archive tests and this
  publication selected the actual Deck return. Physical successor reload is
  still pending. Never silently select an ambiguous/older unrelated branch.
- Open graphics/audio/input: KI-013 reflections, KI-015 shimmer, KI-016
  minimap, KI-017 music, KI-019 neon, KI-020 white paint, KI-022 centering.
  KI-001 clean new-game/profile and KI-026 Race Back remain open.
  [Known issues](known-issues.md) owns scope/severity.
- Campaign preparation: eleven historical scenarios and statically reviewed
  catalog wrapper/count/descriptor ABI. Mandatory live nodes, prerequisite
  graph and clean-campaign credit remain unknown. Existing F10 is not a
  catalog-memory snapshot; no unlock mutation or archive extraction authorized.

## Gates completed for this candidate

- SDK RelWithDebInfo and tagged Release: **1,817 PASS / four known skips /
  zero failures** each (1,821 registered); eight new cases. SDK AST 1/1.
- Parent AST scan clean / 3 fixtures. Analyzer 4 positive / 8 negative;
  diagnostics 1 positive / 7 negative / 116 source checks; portable bundle
  1 positive / 13 negative / 41 source checks on PS7 and Windows PS5.1.
- Save-lineage 46 isolated checks; session 1 positive / 10 negative; build
  matrix 1 positive / 6 negative; bootstrap 12/12.
- Native gameplay F10 manifests: 12/12 artifacts hash-verified; all 21 new
  scalar fields present/monotonic. Final clean 0.14.0.0 publication/Windows
  relocation passes with exit 0; its guest-free F10 has 21/21 zero defaults.
- Primary integration and independent memory/command/publisher reviews:
  no blockers. These do not close physical M7-016 or KI-027.

## Constraints and entry points

- Owner has gamepad only; physical T300 unavailable. No wheel reruns.
- Prefer short targeted tests. Races/story/deliveries may count as ordinary
  play; never automatically repeat two hours for a small change.
- Preserve complete owner profiles separately from isolated clean campaigns.
  Finish sync and stop writers on every host before publication/pruning.
  Local file locks are not distributed locks.
- No coherency/readback disablement, guest timing changes, global brightness
  clamp or muting sound to hide symptoms. Stock performance is M7;
  dynamic FPS/native Linux/macOS/public packaging remain M8/M9.
- Local commits only until milestone closure; no push. Follow AGENTS.md
  usage threshold and bounded independent-agent rules.
- Architecture/workflow/ledger: README-AI. Commands: [build.md](build.md).
  SDK pin: [rexglue-sdk.md](rexglue-sdk.md). Gates: [testing.md](testing.md).
- `scripts/analyze-portable-deck-performance.ps1 -BundlePath <child>` is
  read-only analysis of the returned pair; old all-absent counter sets work,
  partial/negative/regressing sets fail.
- `scripts/build-portable-campaign-bundle.ps1` mutates/builds/publishes/prunes.
  Dedicated output: `out/build/win-amd64-portable-release`. PruneOnly still
  requires lineage/archive safety. Do not use it to merely inspect returns.
