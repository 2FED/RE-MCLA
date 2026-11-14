# Current state and handoff

Updated 2026-11-12, end of day (Kyiv). Replace in place. History lives in README-AI 17c (ledger,
owner builds, SDK lineage, checkpoint log), docs/M11-priorities.md (dated entries) and the evidence
files linked below. Per-issue status is owned by docs/known-issues.md.

## 1. Active milestone and task

**M11 ACTIVE since 2026-11-06** (owner-approved plan: docs/M11-priorities.md; ledger: README-AI 17c).
main = origin/main = `0d4ca1e` (pushed). Host submodule pin unchanged at `c63895c` (`v0.20.0.8`);
owner builds use private SDK worktrees and installs.

In progress overnight 2026-11-12/13 (results NOT known; nothing below depends on them):
- **l95 low-latency build** (KI-076 follow-up: Reflex / Anti-Lag 2 per the design). No l95
  candidate, receipt or SDK branch existed when this file was written.
- **Deck CP analysis** (where the Deck CP budget of ~51 ms/frame goes; `packets_other` profiling).
- **KI-077 research** (Bloom/brightness differs between sessions).

Closed on 2026-11-12: the KI-074 strict frame lead is default on in l94 and the MAIN regression run
was clean; the dawn hunt ended with KI-079 classified as game behaviour, and the read-back
freshness theory was withdrawn.

## 2. Current builds

Owner builds are owner-internal (V1 tickets). The candidate is `private/lNN-int-input`, the signed
folder is `private/lNN-internal` (launcher Build folder `private/lNN-internal\build`), the arms are
`private/lNN-arms` and the steps are `private/lNN-CHECKLIST.txt`. "Signed" means that
`candidate-receipt.json` is present; "manifest" is the first 8 hex digits of its `build_manifest_sha256`.

| Build | Host | SDK | Purpose | Status / owner result |
|---|---|---|---|---|
| l81v | `a9de776` | `2aff82e` (v0.20.0.11) | Volunteer build (V2, dev overrides compiled out) | Published 2026-11-10: `DebugBundle` `138b0ef`, allowance list = l81v only (l70 withdrawn) |
| l86 | `21cea1e` | `c40db4b` | KI-054 hitch hold v2 default on (81 keys) | Signed (8423639B); KI-074 flicker recurred at 1080p Unlocked ~50 fps |
| l87 | `8aa6ae9` | `c40db4b` | KI-060 filter probe + native `sub_82146BE0` (84 keys) | Signed (AE5FFF5E); Deck: -30 % underruns, still crackles |
| l88 | `79dc1e6` | `7cbfe3b` | KI-074 cadence counters + fenced delivery (85 keys) | Signed (D17C1E6B); fenced delivery rejected; steady-off and cap44 clean |
| l89 | `a8a488c` | `7cbfe3b` | KI-060 fma-free native filters, leaf probe (86 keys) | Signed (1694F36C); Deck afprobe/afnative NOT run |
| l90 | `c829269` | `4ec7abd` | KI-074 frame lead limit, deferred release (88 keys) | Signed (D6AF6995); first A/B invalid (launcher on l88); valid lead1 = no-op |
| l91 | `062c318` | `ef72fd1` | KI-074 strict / adaptive lead (90 keys) | Signed (5485D487); strict and adaptive clean; adaptive long run: dawn sidewalk flicker -> rejected |
| l92 | `3693e01` | `e6d6bd3` | Probes (read watch, age inject, occlusion, present stats); KI-026 arm (98 keys) | Signed (103B9D6F); lead-strict run clean; stall-fix and present-stats NOT run |
| l93 | `5584799` | `e6d6bd3` | Always-on time-of-day log; Arcade clock controls (103 keys) | Signed (0F3E2F19); dawn A/B: flicker at 06:00 on default, none with strict |
| l94 | `98cea8b` | `e6d6bd3` | **Current owner build**: strict lead default on; dawn hunt arms (104 keys) | Signed (D82D1F3C); MAIN regression clean (~30 min); dawn hunt done |

l79-l85 are listed in README-AI 17c (owner builds table).

**Current default behaviour (l94; PC and Deck unless noted):**
- KI-074 strict frame lead bound (`d3d12_frame_lead_strict=1`), written by the host m10 defaults on
  Windows and Wine/Proton since **l94**. Control key: `frame_lead_strict_off`.
- Time-of-day logging (`MCLA_TOD` line, F10 `time_of_day` block) always on since **l93**. The clock
  controls (`tod_*`) work only through Arcade arm keys.
- Read-back cadence counters always on since **l88** (SDK `7cbfe3b`).
- KI-054 raw-timer hitch hold v2 (`mcla_raw_timer_hitch_hold`) since **l86**. Control key: `raw_timer_hitch_hold_off`.
- KI-013 MIN/MAX blend fix (`d3d12_blend_minmax_fix`) and KI-056 Fix A (`mcla_album_preview_restore`)
  since **l81**. Under Wine also the KI-060 zero-delay backoff and the game audio-thread priority.
- l80 platform defaults: placed texture heaps with pre-creation (Windows only), timer blocking wait,
  Canary XMA fixes. l78: steady exemption and deferred fence writes. l77: KI-071 W1 frame walk.

## 3. Tested versus pending

**Physically confirmed by the owner (build, date):**
- KI-013 traffic-light reflections: arm PASS on l80 (11-08); default-on PC run PASS on l81 (11-10).
- KI-072 traversal stutter: placed heaps, l79 A/B (11-08); freeway-exit stutter gone on the l80 default (11-08).
- KI-054 traffic jumps: hitch hold v2 PASS on the Deck and the PC, l85 (11-10).
- KI-074 night surface flicker: removed by strict on l91 (11-11) and l92 (11-11), at dawn on l93
  (11-12), and as the default on l94 on MAIN for ~30 min (11-12).
- KI-071 default by ear: PASS on l78 (11-08). KI-068 Deck saves: still fine on l79 (11-08).
- Rejected by owner measurement: fenced read-back delivery (l88), CP bulk register writes (l82
  Deck), frame lead limit 1 (l90, no-op), adaptive lead (l91 long run), hot pages v2 (l81 census).

**Implemented, not yet tested by the owner:**
- KI-060 l89 fma-free native filters on the Deck (`deck/afprobe`, `deck/afnative`; not run since l89).
- KI-026 warp stall fix (`mcla_warp_stall_fix`, arm `pc/stall-fix`; Race Back x10-15).
- KI-076 present stats, sync interval and the Locked tearing switch (arm `pc/present-stats` at 60 Hz).
- Strict frame lead on the Deck (`deck/default` vs `deck/strict-off`; fps cost unmeasured).
- KI-075 launcher destination prefill (l84+; no owner check recorded).
- Fix A as a default (on since l81; the fix itself passed as an arm on l72).
- KI-054 hitch hold as a default (on since l86): no dedicated regression result recorded.
- l94 `pc/dawn-off-fenced`: not reported. Read watch and age inject did run on l94 (dawn hunt).

**Designed only:** Reflex / Anti-Lag 2 (until l95 lands and is tested); hardware FMA dispatch
(bounded at ~2 % of Deck CPU, low priority); KI-073 album export. **Rejected:** fenced delivery,
adaptive lead, bulk register writes, hot pages v2 (measurements above).

## 4. Open issues

- KI-074 Unlocked surface flicker: fixed by the strict default (l94); mechanism unproven; costs a few fps when GPU-bound.
- KI-074b Object despawns: open; no new data since 2026-11-07.
- KI-079 Tree blink at the 06:00 night/day switch: treated as game behaviour (about 23 of 37 switches across all arms); low priority.
- KI-078 White whole-frame flash: probably external (same flash seen in the browser); watch only.
- KI-077 Bloom/brightness differs between sessions: research in progress; not reproducible per lap.
- KI-076 60 Hz judder / tearing: open; next the present-stats arm at 60 Hz, then l95 low latency.
- KI-060 Deck audio crackle (XAudio engine thread starved): open; next the Deck afprobe/afnative arms on l94.
- KI-026 Race Back camera never returns: fix in arm `pc/stall-fix`, untested.
- KI-035 Garage transition freeze: fixed; field data: 11 forced finishes (type-17 transitions); side effect a visible LOD blink; M11-012 refinement open.
- KI-041 Unlocked light flicker: open, needs a repro (the l93+ time-of-day controls can help).
- KI-075 Launcher destination prefill: fix in l84+; owner check pending.
- KI-073 Album delete/export: backlog (design ready).
- KI-072 High-speed stutter: largely fixed (placed heaps); the remaining drops are CP/GPU-bound (Deck CP analysis).
- Closed in M11: KI-013 traffic-light reflections, KI-054 traffic jumps, KI-056 album preview,
  KI-068 Deck saves, KI-071 doubled cut-scene voice (see README-AI 17c).

## 5. Blockers and owner actions

No engineering blocker. Owner actions, in order (on l94 unless noted):
1. Before every run: set the launcher Build folder to `private\l94-internal\build` and press CHECK
   SETUP. After the run the game log must show that arm's `MCLA dev override: ...` lines (the first
   l90 A/B was invalid because of this).
2. Deck (`private/l94-CHECKLIST.txt`, optional item c): `deck/default` (strict on) vs
   `deck/strict-off`, then `deck/afprobe` and `deck/afnative` (KI-060).
3. PC `pc/stall-fix` (KI-026: Red Light Driver, lose, Race Back 10-15 times).
4. PC `pc/present-stats` (KI-076: display at 60 Hz, 3 min Unlocked, restart, 3 min Locked).
5. Optional: `pc/dawn-off-fenced` (not reported). Afterwards return to `l94-arm-default.cmd`.
6. l95 once staged: sign it with its prepare script and follow its checklist.

Helpers: `private\l94-arm-<arm>.cmd` (they refuse while `mcla.exe` runs) and `private\l94-arm-clear.cmd`.
Signing a build: `private\tools\pwsh\pwsh.exe -NoProfile -File private\lNN-internal-prepare.ps1`
(ends with "DONE").

**Time-of-day rule:** the save stores the hour of day. Every `dawn*` / `tod_*` arm runs only on a
COPY profile (launcher "Copy selected profile"), never on MAIN.

Publication (the owner's word is needed each time): an SDK release after v0.20.0.11 (all KI-074
SDK work is local), and a new volunteer build (l81v is current; the strict default is not yet in
any volunteer build).

## 6. Save lineage and safety

- MAIN profile id prefix `5b71`: only through the owner's launcher; agents never touch it.
- Copy profile `68e50352` (`68e5`) is used for all dawn / time-of-day tests (l93, l94).
- The Deck profile copy has the MAIN id; bench and Deck copies never sync.
- Agents never launch the game; every game run is the owner's.
- No builds, tests or sustained CPU work while the owner plays: `private/tools/wait-for-idle-pc.ps1`
  plus the coordinator's explicit "hold" / "resume" (cause of KI-067 host slowdown / KI-072).
- Owner measurement tools: `private/tools/proc-sampler.ps1` and the HWiNFO64 CSV (copy it to
  `private/owner-hwinfo-<date>` before analysis).
- Commit after each closed task; push with green gates. SDK and signed-list publication need the
  owner's yes; volunteer builds come from a clean clone at a published SDK pin.
- Storage: four cleanup rounds on 2026-11-10 (C: free ~21 -> ~107 GB) and the l34-src addendum
  (~116 GB free). Kept: MAIN and all save lineage, tickets, receipts, staged binaries, open-issue
  candidates, sessions with owner text, cited F10 packages. Reports:
  `docs/evidence/M11-storage-cleanup*-20261110.md`.

## 7. Pointers (docs/evidence)

- KI-074: `M11-ki074-l86-recurrence-20261110.md`, `M11-l88-pc-ab-analysis-20261111.md`,
  `M11-l88-steadyoff-analysis-20261111.md`, `M11-l88-cap44-analysis-20261111.md`,
  `M11-l90-ab-invalid-and-all-runs-20261111.md`, `M11-l91-adaptive-long-run-analysis-20261111.md`,
  `M11-ki074-rootcause-research-20261111.md`, `M11-l94-dawn-hunt-read-watch-20261112.md`.
- KI-079 / dawn: `M11-l94-dawn-hunt-tree-impostors-20261112.md`, `M11-time-of-day-re-20261112.md`,
  `M11-l93-time-of-day-control-20261112.md`. KI-078: `M11-l94-white-flash-analysis-20261112.md`.
- KI-076 / low latency: `M11-ki076-pacing-and-low-latency-design-20261111.md`.
- KI-060 Deck audio: `M11-ki060-audio-filter-analysis-20261110.md`, `M11-l87-deck-analysis-20261110.md`,
  `M11-ki060-l89-implementation-20261110.md`, `M11-fma-dispatch-design-20261111.md`.
- Deck CP: `M11-l81-deck-analysis-20261110.md`, `M11-l82-deck-analysis-20261110.md`,
  `M11-deck-memwatch-design-20261108.md`.
- KI-054: `M11-ki054-traffic-jitter-design-20261110.md`, `M11-l84-deck-analysis-20261110.md`,
  `M11-l85-owner-build-20261110.md`. KI-026: `M11-ki026-implementation-20261111.md`.
- KI-013: `M11-ki013-blend-minmax-20261108.md`. KI-072: `M11-l79-perf-ab-20261108.md`.
- Builds: `M11-lNN-owner-build-*.md` and `M11-lNN-sdk-*.md` (l79-l94); the volunteer build
  `M11-l81v-volunteer-candidate-20261108.md`; tester guide `docs/volunteer-tester-guide.txt`.

> Update 2026-11-13 early: the overnight work landed. l95 is STAGED, unsigned (host 110 keys, SDK f9f5178 local: Reflex / Anti-Lag 2 default off; brightness fields always on); checklist `private/l95-CHECKLIST.txt`. KI-077 research: scale path ruled out between sessions, likely game state. Deck CP budget: incremental ceiling 18.5-20 fps; 30 fps needs a native renderer (owner decision topic). Evidence: M11-l95-*.md, M11-ki077-brightness-research-20261112.md, M11-deck-cp-budget-and-levers-20261112.md.
> l96 is STAGED, unsigned, and is a superset of l95 (host 114 keys, SDK ccac775 local): sign l96 instead of l95. Checklist `private/l96-CHECKLIST.txt`: Deck part first (measurement run, lever-1 arm, strict-off control, audio arms, fma bench), then the PC part (Reflex arms, KI-026 stall fix, present stats at 60 Hz).
> 2026-11-13: l96 owner PC runs done (Reflex works; Reflex without strict: latency 36 -> 12.6 ms, +1.9 fps, no flicker in 13 min); l96 Deck runs done (owner: afnative music perfect, other sounds crackle ~1 %; cp-lever1 a bit smoother; strict-off feels worse), analysis in progress. l97 STAGED, unsigned (superset of l96; host 116 keys, SDK 6c5792d): Reflex-replaces-strict with automatic fallback, default unchanged. Next owner: PC afnative by-ear check on l96 (in progress), then sign l97 and run `private/l97-CHECKLIST.txt` (soak on pc/auto first).
> 2026-11-13 later: l96 Deck analysis done (KI-060 audio fixed by the native filters; lever 1 = -2.7 ms/frame; IM_LOAD memo unsafe). l98 STAGED, unsigned, superset of l97 (host 119 keys, SDK 6c5792d): native audio filters, draw-packet TSC and fence poll interval 64 are default on. Sign l98 (not l97). Next owner: Deck default by ear; PC pc/auto soak and dawn-auto (COPY profile) per `private/l98-CHECKLIST.txt`.
> 2026-11-13 night: l98 owner runs: Deck not yet reported; PC pc/auto soak 44 min (no artefacts, but the Reflex guard tripped after 5 min: strict for 90 % of the run) and dawn-auto x10 (clean; guard tripped once). Owner priority: hard frame drops on PC. l99 STAGED, unsigned, superset of l98 (host 124 keys, SDK d3d7b6c): per-hitch attribution line, texture churn fix (off), VRAM census, unguarded Reflex arms. Sign l99 and run `private/l99-CHECKLIST.txt` (close the animated wallpaper first: it holds GPU memory).
> 2026-11-14 early: l100 STAGED, unsigned, superset of l99 (host 128 keys, SDK 1e81149): Deck register-write skip (off, with verify), constants-only draw path verify-only, spin census and backoff. Sign l100 (not l99). PC first: default hitch run, churn, auto-noguard soak, dawn-auto-noguard (COPY profile); Deck: default by ear, cp-verify, cp-reg, spin-backoff. `private/l100-CHECKLIST.txt`.
> 2026-11-14: PC drops diagnosed (sustained CP-bound dips by draw count at night; GPU-bound by day); Reflex without strict: three clean owner runs incl. a soak (default decision pending with the owner); cp-reg blocked (verifier false positives suspected). l101 STAGED, unsigned, superset of l100 (host 131 keys, SDK a49e511): texture memo v2 and RT memo default off with verify, MCLA_DIP always on. Sign l101; PC Run A pc/verify-all then Run B pc/fast per `private/l101-CHECKLIST.txt`. Rule tightened: no agent file work at all while the owner plays.
> 2026-11-14 later: l102 STAGED, unsigned (= l101 + F10 retention: unlimited in internal builds, 50 in volunteer builds; SDK a49e511 unchanged). Sign l102 (not l101); PC Run A pc/verify-all then Run B pc/fast per `private/l102-CHECKLIST.txt`. Old session diagnostics now accumulate in internal builds: prune by hand or in the next storage round.
> 2026-11-14 evening: AMD Anti-Lag 2 test prepared on l102 (`private/l102-AMD-CHECKLIST.txt`: 860M by feel, 6800 XT VM for numbers); l103 host keys proposed for the AMD guard/auto path. Test Temp leaks fixed (tests + staging script; the modified staging script has run only with a synthetic build: watch the first real `lNN-internal-prepare.ps1`). C: free about 112 GB after cleanup round 5.
