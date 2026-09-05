# Current state / AI handoff

Updated: 2026-09-06. Read this first, then relevant README-AI sections.
Replace stale facts here; actual Git/artifact identities and accepted evidence win.
Detailed reports: [CPU attribution](evidence/M7-016-deck-cpu-attribution.md),
[readback A-B-A](evidence/M7-016-deck-readback-cost.md),
[C++ runtime](evidence/M7-016-proton-cpp-runtime.md),
[disarm fragmentation](evidence/M7-016-disarm-fragmentation.md).

## Active task / next action

- M6 closed at0.8.2.2. M7 **0/16**, active M7-016 / KI-027 Deck performance;
  M7-009 full-profile lineage and M7-001 matrix preparation remain partial.
- Owner's failed automatic START is corrected in isolated candidates. Primary
  inspected actual saved Eclipse/HUD and PAUSE frames again in the SDK19
  control; no owner START/F10 repeat, wheel or long-soak action is needed.
  All profiling/control processes ended; no game/perf attachment remains.
- KI-030 builtin-Proton timed STL mutex fault is contained by exact official
  app-local CRT (same-PE fixture, actual launcher/game/live/crash controls).
  No global override; this is NOT the main FPS fix. Normal publication pending.
- Accepted descriptor gameplay/pause12.63/8.63 outputs/s has no material gain.
  Readback A-B-A fast12.15/some14.50/fast14.25 rejects a robust improvement.
  `some` allows indefinitely stale guest data: NEVER ship/promote its profiles.
- New safe-disarm-gap control:255 outputs in20.000047s (12.75/s),65,942 gaps
  across84,274 disarm calls, zero interval lock misses/protection failures.
  Entire inclusive disarm cost1.129s: high gap ratio is not the FPS explanation.
  Arm gaps77/163,464 are negligible. Timers overlap; do not sum exclusive costs.
  No protection/coherency/clock/readback-policy changes or speedup claimed.
- Next bounded lead: default-off, launch-cached Type-0 contiguous constant
  writes through existing WriteRegisterRangeFromRing. Independent design
  review accepts float4000-47FF,fetch4800-48BF,bool+loop4900-4927 families;
  keep repeated-register, single-word, holes, mixed/side-effect ranges scalar.
  Preserve packet length check, wrapping/endian and both backend invalidation.
  Base/Vulkan writer plus handler occupy148/1419 prior gameplay GPU leaf
  samples (~10.4%), not a recoverable time promise. GPU-free edge fixtures,
  independent review, both backends, matched private A-B-A required. No code
  for this policy yet. Defer coalescing and broad threading speculation.

## Source versus measured / published identity

- Reviewed parent checkpoint VERSION0.18.0.0: diagnostic samplev2, legacyv1
  compatibility, F10 gap counters, exact SDK19 pin and documentation. Base
  parentd44d1b6 contains prior CRT correction. No push; check Git for commit.
- SDK clean localv0.19.0.0 /c2e67161f1207ddcee609507c399efe7a1981b72,
  matching Release install. API counters only, no native protect-call changes.
  Review fixed new metadata race before deployment: nonblocking heap try-lock
  after callbacks; on contention count skipped range, retain legacy predicate.
- Exact Deck gap-control measured PRE-BUMP parent0.17.0.0 plus SDK19:
  host0E90319E41606927BA71E96880355F527F8761FBA917050C639F0EDFD8574FAD;
  runtimeDBFF1BE301B8E83578A601F41C0B641C4E9228EAAC23297D5B7C431661D54A1F;
  GPU2D26216495195318B899CE8CA300499E0A7E43D8FAFFE458DDDD98045341E852.
  Tree out/build/win-amd64-readback-cost; subsequent version builds must not
  relabel this identity. Windowed1280x720 Desktop Vulkan/fast/official CRT.
- Published/profiled bundle STILL0.16.0.1, under private/bundles/M7-016:
  mcla-0.16.0.1-win64-B6A19E748F517476-0BCEBB4FB89A8529-58B3385C89D9E499.
  current.txt and Steam shortcut unchanged. Manifest546AF7AA8256738AE37896818338A96A624A6DC73FE9ACE2333229EF04E1FAA2;
  lock0AA62A18D247FEF31D2C37B4D4C09E8C573C6AB598D7A654593406487300DBDD.
- Prior isolated CRT candidate0.17.0.0 hostAD4D0F58... and32immutablefiles:
  private/evidence/M7-016/cpp-runtime-publication; receipt
  bundle-20260905-203701-1cc3a1a9/result.json. Not selected in normal sync root.
  Official CRT14.44.35211.0 pins config/portable-cpp-runtime.json. No CRT
  binaries in Git. Root loader precedes launcher hash-check (not a sandbox).

## Owner save and private evidence

- Completed owner session20260905T175147Z-296 ended20260905T190534Zexit0,
  one snapshot/zero watcher errors. Verified121files/67,307,014bytes return:
  private/evidence/M7-016/paired-return-20260905T190534Z/transfer.
- Complete four-file profile/album archive private/save-archive/M7-009/:
  3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A.
  Save58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585;
  header2FAEFBC7FF8CDEADBCE025D9BE914436ED05E27D041F72C8C88798612CFB0D60;
  album8EE49926E1DFE804DB923EF2512219FC7A078FEB50944C3D2BD4E9AC39F81588.
  Full mutable archive B673A45B17051658213F9055B67A894CF3E43E53C9DE8070335A2225CEC80951.
  Evolved successor reload still physically unproven. Never promote test copies.
- New return private/evidence/M7-016/disarm-gap-return-20260906:
 24files/22,669,366bytes, all hashes verified; tar9C98890BB1ECA01C313053556BD27627DB1C678DD97506E575C8CC734BFF3B67.
  Raw samples/frames verified/run/fast-gap-control/user/mcla-readback-cost/.
  Original visual_validation_pending receipt is immutable; later primary
  gameplay+both-pause image inspection recorded in the public sanitized report.
  PID101412/start157944737 ended154.075s,exit0,noforce. Source/cache/published
  hashes unchanged. Experiment outside sync: /home/deck/MCLA-Diagnostics-20260905/disarm-gap-20260906.
- Older readback and CRT transfers remain linked in reports. No private
  frames/stacks/maps/saves, immutable game files or generated code in Git.
  Binary-only reviewed SSH transfers; existing Deck profile copied locally.
  Prior full-candidate outbound transfer denied; do not retry/bypass it.

## Verification / operational constraints

- Final all-target SDK RelWithDebInfo and Release:1856 executed +4 known
  BitStream skips,zero failures. Focused11/479. Final tagged Release installed.
- Root ASTscan/3fixtures; serializer133/0; Python28; dualPS diagnostics5/20/170,
  portable3/34/45,lineage48,buildmatrix1/6,alpha1/31 pass. Bootstrap12/12SDK19.
  One escaped stale alpha SDK regex fixed before final dualPS passes.
- Non-force SDK19 generation:all67files/133,916,451bytes unchanged,65CPPobjects.
  Source/pre-bump host built and measured as above. Prior exact CRT candidate
  Windows relocation, Proton launcher/live/session and crash-helper PASS;
  do not claim those were rerun for the version-only new parent label.
- Latest known weekly remaining25%; read live after checkpoint. Stop at20%,
  never redeem credits without permission. No task closed on tooling alone.
- Serialize builds/codegen/runtime/private mutations. Preserve owner full
  profile/album before publication/pruning. Only immutable prepared game files
  may be hardlinked, never mutable build outputs. No normal-bundle update yet.
- Paired SSH/pinned key works. Desktop controls are not GamingMode, motion,
  controller or audio PASS. No sudo/sysctl/clocks/affinity/global prefix edits.
  Omit mcla.toml for CLI-only isolated probes (TOML overrides CLI settings).
- Scoped elevated shell and Codex apply_patch fallback work; standard sandbox
  helper/view_image may fail. Read-only in-memory image preview is available.
  No custom UIAutomation/SendInput. Preserve unrelated source .stfolder.
