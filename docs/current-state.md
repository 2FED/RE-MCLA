# Current state / AI handoff

Updated: 2026-09-05. Read this first, then relevant README-AI sections.
Replace stale facts here; exact Git/artifacts and accepted evidence win.
Detailed history: [descriptor experiment](evidence/M7-016-deck-descriptor-reuse.md).

## Active task / next action

- M6 closed at 0.8.2.2; M7 **0/16**. Active M7-016/KI-027 stock Deck
  performance; M7-009 save lineage and M7-001 campaign preparation are partial.
- Finish the descriptor comparison before changing investigation/chat. No
  M7 push, physical wheel or new long soak. See [priorities](M7-priorities.md).
- **Accepted 0.15.1.0:** four complete F10s, gameplay 12.78 new outputs/s,
  pause 8.60/s versus previous 12.14/10.02. Pause packets/frame rose 22%;
  per-packet GPU-thread CPU is essentially unchanged. No material spin-hint
  gain or causal regression established. Pause resolve waits remain ZERO.
- Candidate **0.16.0.1 / SDK 0.18.0.1** reuses identical same-epoch Vulkan
  texture/sampler stage sets, retaining requests/barriers/lifetimes and fresh
  allocation on misses. Six fields measure reuse and nested update cost.
  Nanosecond accumulation fixes short-call truncation before exporting us.
- Final clean publication and exact-binary Windows Vulkan/D3D12 controls PASS:
  primary inspected saved gameplay, PAUSE and Settings; exit 0, no force cleanup.
  Native Vulkan cumulative stage-set reuse is 50.03%, NOT a Deck FPS gain.
- Next owner action: same Proton 9.0-4/settings, gameplay F10 READY,
  drive 30s, F10 READY; pause, settle 2s, F10 READY, wait 30s, F10 READY.
  Normal exit/full sync; report motion/audio and progressed profile loading.
  No hour-long test. Do not infer Deck gain from native reuse or unit tests.
- **Separate next research package requested by owner:** Deck CPU scaling /
  matched Xenia baseline, still under M7-016; later M8-011 reuses benchmarks.
  Owner Xenia 15--20 FPS is not controlled A/B. Match scene/settings/cache and
  profile useful work/waits before parallelizing independent work. Two hot
  threads do not mean only two cores are active. No affinity-only promise.

## Identity / save lineage

- Root VERSION 0.16.0.1, local reviewed checkpoint; no push.
- SDK clean tag v0.18.0.1 / 9098e122afa116f4316d6cec47e561ebc5186974.
  Parent gitlink matches; semantic/codegen floor remains 0.18.0.
- Sync root private/bundles/M7-016; current.txt selects
  mcla-0.16.0.1-win64-B6A19E748F517476-0BCEBB4FB89A8529-58B3385C89D9E499.
  Receipt: private/evidence/M7-016/bundle-20260905-144002-92771135/result.json.
  Manifest: 546AF7AA8256738AE37896818338A96A624A6DC73FE9ACE2333229EF04E1FAA2.
  Lock: 0AA62A18D247FEF31D2C37B4D4C09E8C573C6AB598D7A654593406487300DBDD.
  25 immutable files; final launcher/hash verification after probes PASS.
- Full profile4files /3,815,088bytes, including album:
  3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A.
  Save: 58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585.
  Header: 2FAEFBC7FF8CDEADBCE025D9BE914436ED05E27D041F72C8C88798612CFB0D60.
  Album: 8EE49926E1DFE804DB923EF2512219FC7A078FEB50944C3D2BD4E9AC39F81588.
- Accepted F10 pairs live-20260905T130741Z-336-1/130830Z-336-2 and
  130857Z-336-3/130940Z-336-4. PID336 creation134330871933074650. Shutdown
  130950Z-336-5.partial is not a complete capture. Session130633Z-296 exit0,
  25 immutable files, one profile snapshot, zero watcher errors, unchanged save.
- Mutable archive outside sync: private/evidence/M7-016/returned-bundles/
  <0.15.1 bundle id>/4D672717A35E99B9E9EEE5653F72A68AAAD9603AA772D4D7D3DCC33CB2187560.
  Full profiles also archived under private/save-archive/M7-009. No staleM6seed.
- Use dedicated out/build/win-amd64-portable-release only for publication.
  Legacy build-tree DLL hardlinks can mutate published artifacts. Runtime
  binaries are copies; immutable game files alone may be hardlinked.

## Evidence and rejected overclaims

- Gameplay busiest guest thread90.94%, GPU Commands81.61% of one logicalcore;
  others60.24/38.02%. Pause busiest93.07/85.21%. These are threadCPU totals,
  not physical-core placement or proof of parallelizable work.
- Pause9071packets/frame vs4421gameplay. Packets include copy dispatch.
  Resolve copy gameplay19.91GB/3.036s, pause13.76GB/2.058s; waits25/.248s
  versus0. Actual Vulkan fences.253/.021s. Inclusive/cross-thread spans overlap.
- Eight offline instantaneous thread snapshots: two guest PAUSE RIPs, one
  valid own-code unwind. All four GPU Wine stacks fail missing-module unwind;
  none establishes UpdateBindings as hotspot. Not a statistical profile.
- Initial exact0.16.0.0 Windows/Vulkan built-in frontend probe reaches copied
  progressed gameplay, PAUSE, Settings. Primary visually inspected BMPs.
  Exit0/CloseMainWindow/no force/sourceprofile unchanged. About49%stage reuse
  is real avoided work, not Deck FPS proof. Native per-call microsecond
  truncation motivated0.18.0.1; do not use old elapsed totals as cost estimates.
  Final exact controls are native-vulkan-ns-control and native-d3d12-ns-control.
  All six saved gameplay/menu BMPs inspected. Vulkan gameplay cumulative
  6,229,160 binding calls/3.651949s, 6,145,552 native updates/1.427899s;
  nested native wall, not Deck CPU/GPU cost. D3D12 Vulkan-only fields zero.
- F10 Deck GDI frames remain all-black, honestly classified. READY means
  package saved; visible overlay may repaint old output and raise SteamFPS.
  Photo Mode is separately fixed. KI-015 broader dithering/lighting/neon open.
- Campaign seed has11historical scenarios/static ABI boundaries, not mandatory
  live nodes, dependencies or clean-campaign credit. No unlock edits.

## Gates and operational pitfalls

- SDK descriptor/helper review, independent timer review and source/AST pass.
  Descriptor5cases/37assertions; focused graphics diagnostic5/53. Full final
  all-target RelWithDebInfo/Release:1845executed+4knownBitStreamskips=1849,
  zero failures. Explicit post-tag configure/fullRelease/install also pass.
- Actual non-force codegen67files/133,916,451bytes byte-identical to prior
  0.17.1 corpus. Only SDK compatibility metadata advances.
- Final root AST/3fixtures and PS7/5.1 gates pass: analyzer14/48,
  diagnostics5/20/164source checks, bundle1/13/41, save-lineage46, session1/10,
  buildmatrix1/6, alpha1/31 plus real alpha verification, bootstrap12/12.
- Independent review caught permissive optional-counter type coercion, fixed
  with malformed/null/legacy/zero fixtures; caught alpha escaped-version and
  early-init result-label drift, corrected before final gates.
- Retag alone does not rerun SDK CMake configure. Verify installed package
  version after explicit configure/build/install, not only source tag.
- Publication rename access denial recurred; holder unknown. Failed staging
  retained outside sync under bundle-20260905-143544-618197f0/failed-staging;
  fresh full clean retry PASSED. Holder/race still unknown; do not erase
  diagnostics or weaken verification. Intermediate0.16 payload pruned only
  after verified successor and recoverable archive F4D24E3B... outside sync.
- Logs under private/evidence/M7-016/deck-return-20260905-0.15.1.0. First
  0.16 compile interface error and stale-install configure failure are retained.
- Serialize native/private/build mutations. Stop writers/finish sync before
  publishing; locks are not distributed. Never choose ambiguous save branches.
- Default sandbox/native UI helper unavailable; scoped escalated shell and
  Codex apply_patch work. No custom SendInput/UIAutomation fallback. Built-in
  deterministic engine probes do not replace owner/controller evidence.
- Preserve unrelated untracked source-directory .stfolder; never stage private
  payload. Recheck actual status/identity before committing the checkpoint.
