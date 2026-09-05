# Current state / AI handoff

Updated: 2026-09-06. Read this first, then relevant README-AI sections.
Replace stale facts here; actual Git/artifact identities and accepted evidence win.
Detailed findings: [CPU attribution](evidence/M7-016-deck-cpu-attribution.md),
[readback A-B-A](evidence/M7-016-deck-readback-cost.md),
[C++ runtime](evidence/M7-016-proton-cpp-runtime.md).

## Active task / next action

- M6 closed at 0.8.2.2. M7 **0/16**, active M7-016 / KI-027 Deck performance;
  M7-009 full-profile lineage and M7-001 matrix preparation remain partial.
- Owner closed the profiling game normally. Both requested pause/moving CPU
  recordings are retained; no owner action, unchanged F10 set, long soak or
  wheel test is needed now. No game/perf attachment remains from those runs.
- Descriptor return still measures 12.63/8.63 gameplay/pause outputs/s without
  material gain. Direct profiles prove multiple guest wait predicates and
  large resolve/upload copy callers, not one complete root cause.
- Executed isolated readback A-B-A: fast 12.15, some 14.50, fast-repeat 14.25
  outputs/s. Initial apparent 19% uplift is not robust against baseline drift.
  Some eliminates warm CPU copies but permits indefinitely stale guest data:
  NEVER ship it or import its experimental profiles. Fast copy costs about
  4.7 ms/output. Timings overlap; raw physical copies bypass alias write watches.
- New concrete KI-030: /MD timed STL wait under Proton builtin MSVCP140 holds
  the contender out until timeout. Minimal same-PE fixture: 500-ms failure
  builtin, 3/3 ~21-ms success official app-local CRT; Windows also succeeds.
  Five isolated real-host controls now reach saved gameplay and pause, normal
  exit0. Latest minimal4-DLL control works WITHOUT WINEDLLOVERRIDES, with loaded
  paths captured. This fixes synthetic START observation, NOT the main FPS gap.
- Reviewed app-local CRT packaging passes dualPS contracts and actual isolated
  Windows/Proton launcher, live diagnostics, session/save finalization and
  deliberate native crash-helper verification. Exact new0.17.0.0 host also
  reaches primary-inspected saved gameplay/pause and exits0. No owner action
  needed, no published replacement yet. Fullscreen control12.15outputs/s is
  not a speedup. TOML redirects logs; failed private harness-path assertion is
  retained and separately resolved against actual log/samples, not rerun.
- Next cost lead after the dependency correction: measure safe disarm-range
  fragmentation before coalescing. No SDK edit, global watch/clock/sync bypass,
  affinity-only tuning or promised30FPS. The known GPU/audio waits are Win32
  waits, not this broken timed STL helper.

## Source versus published identity

- Prior committed parent3434ab2, reviewed source VERSION0.17.0.0. Checkpoint: default-off
  readback probe/serializer/analyzer, timed-runtime fixture, CRT bundle contract
  and documentation; review/tests preceded the version bump. No push.
- SDK clean0.18.0.1 /9098e122afa116f4316d6cec47e561ebc5186974; gitlink matches.
  No SDK edits/codegen in this experiment. Prior full SDK1845 +4 known skips.
- Opt-in measured host SHA256:
  E1233390F6B12E362FFD4CA7B17E28FB9396103264AA5C7D618AD1368C1AF96C.
  Build tree out/build/win-amd64-readback-cost. Old identity stays bound to
  these measurements even after a subsequent source/version rebuild.
- Published/profiled bundle is STILL0.16.0.1, under private/bundles/M7-016:
  mcla-0.16.0.1-win64-B6A19E748F517476-0BCEBB4FB89A8529-58B3385C89D9E499.
  current.txt unchanged. Receipt bundle-20260905-144002-92771135/result.json
  under private/evidence/M7-016; manifest546AF7AA8256738AE37896818338A96A624A6DC73FE9ACE2333229EF04E1FAA2,
  lock0AA62A18D247FEF31D2C37B4D4C09E8C573C6AB598D7A654593406487300DBDD.
- Official CRT file version14.44.35211.0 (VS Redist directory14.44.35112).
  Pins in config/portable-cpp-runtime.json; private source only, no Microsoft
  binaries in Git. Root launcher imports load before its integrity check.
- New isolated candidate under private/evidence/M7-016/cpp-runtime-publication:
  mcla-0.17.0.0-win64-AD4D0F5815126536-2FC8B65D466D35F4-58B3385C89D9E499.
  HostAD4D0F5815126536C615B8C036C36E27D78D865AE273A379EF89F9A83A9E7440;
  manifest2CAC1E0FD0535ED63F39A76B10AAAEE1CDCF7B4C1BF528CF56FD7B68D09AF4F4;
  lockCDC8FA4DE2BAAB610DB828604EA68AE12289653E37A2005F94A221C0C611EE60.
  Receipt bundle-20260905-203701-1cc3a1a9/result.json,32immutablefiles.

## Owner save and experimental evidence

- Completed owner session20260905T175147Z-296 ended20260905T190534Z, exit0,
  one snapshot/zero watcher errors. Hash-verified return121files/67,307,014bytes:
  private/evidence/M7-016/paired-return-20260905T190534Z/transfer.
- Full mutable archive under returned-bundles/<bundle-id>/:
  B673A45B17051658213F9055B67A894CF3E43E53C9DE8070335A2225CEC80951.
- Full four-file profile/album unchanged, private/save-archive/M7-009/:
  3BBC44D0E0CBE03633DFA3BFFBC776B0C5E62CFDC5AA34BEFB346DE022322B5A.
  Save58B3385C89D9E4999B6791CCB95B6F7ED7963CE07F9C47618850841236784585;
  header2FAEFBC7FF8CDEADBCE025D9BE914436ED05E27D041F72C8C88798612CFB0D60;
  album8EE49926E1DFE804DB923EF2512219FC7A078FEB50944C3D2BD4E9AC39F81588.
  Evolved successor reload is still physically unproven; never claim progress
  from one of the isolated experimental copies.
- Original A-B-A experiment archive115files/218,804,182bytes, locally checked
  every member: readback-experiment-return-20260905 under private/evidence/M7-016;
  tar SHA25627BC295E72E33E35BCF4362E363BE31719C6B598CCC1AF75618CCF36AAD5CDEC.
  Final remote tree is OUTSIDE entire sync root: /home/deck/MCLA-Diagnostics-20260905.
  Relocation preserved276entries with no deletion; receipt
  experiment-final-relocation-20260905.json. No nested experimental child in
  playable bundle or sync-root inventory; normal Steam shortcut unchanged.
- Minimal no-override control returned24hash-verifiedfiles under
  readback-minimal-crt-return-20260905. Latest candidate return62files/
  38,995,747bytes under cpp-candidate-return-20260905; archive
  BDC9F53F592514E0081D9A07F946448D620AAC596E0FABA2B21BD4405E4BC728.
  All member hashes verified. Candidate gameplayPID99739/start157558636 and
  deliberate crash control ended; no game/perf attachment remains.
- Prior raw CPU/stack records and exact private symbols remain in evidence;
  never publish stacks, maps, proprietary frames or saves.

## Verification and operational constraints

- Serializer127/0; Python24; ASTscan/3. DualPS bundle3/34/45,CRT3/8,lineage48,
  analyzer15/56,session1/10,native recovery8/8 pass. Exact candidate Windows
  relocation + Proton launcher/session + live/crash verifier pass, dumpflags
  0x1020, live48687bytes/crash93479bytes. Primary inspected gameplay/pause.
  Final /MD fixture03A052741F20E1FC8D7679C87F52253C0B4C4CECCBA1B285E61A5D4EFDEDEB18
  passes Windows3/3, fails builtin Proton, passes app-local Proton3/3~21ms.
- Source0.16.0.2 unfinished-F10 recovery correction was previously
  tested but not deployed. Do not weaken recursive conflict/reparse checks.
- Direct paired SSH with pinned hostkey works; keep addresses/key paths private.
  Desktop-mode controls are not GamingMode/gamescope/controller/audio PASS.
  No sudo, installs, sysctl, clocks, affinity, global prefix or Steam edits.
- Serialize game launches/private captures/build outputs. Stop writers, archive
  and validate full profile before publication/pruning. No hardlinks from
  mutable runtime/build outputs; only prepared immutable game files may link.
- Elevated scoped shell and Codex apply_patch fallback work; normal sandbox
  helpers/view_image can fail. Read-only in-memory image preview fallback is
  available; no custom UIAutomation/SendInput. All source edits use apply_patch.
- Preserve unrelated source-directory .stfolder. No private/game/generated
  content or DLLs in Git. Publish only at a reviewed exact-artifact boundary.
- Last observed weekly allowance29%; re-read rather than infer. Continue safe
  milestone work while above20%, never mark KI-027 or M7 complete on tooling.
