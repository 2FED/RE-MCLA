# Current state and handoff

Updated 2026-11-20, evening (Kyiv). Replace in place; do not append notes. History lives in README-AI 17c
(ledger, owner builds, SDK lineage, checkpoint log), docs/M11-priorities.md (dated entries) and the
evidence files named below. Per-issue status is owned by docs/known-issues.md.

## 1. Active milestone and task

**M11 ACTIVE since 2026-11-06** (plan: docs/M11-priorities.md; ledger: README-AI 17c). main =
origin/main = `6c7b6c1` (pushed). Host submodule pin unchanged at `c63895c` (`v0.20.0.8`); owner and
volunteer builds use private SDK worktrees and installs (`private/l39-sdk-src` and its worktrees).

Owner decision 2026-11-20: **PC performance work pauses at l117**; next are the Steam Deck and the
volunteer build. Active: (1) l117v is published and may be sent to testers; (2) Steam Deck run of the
signed l117 (`private/l120-DECK-CHECKLIST.txt (the l117 one is archived under private/_archive/misc)`), not run yet; (3) owner checks of l117v in the coming
days (Deck, other hardware).

## 2. What is published

- SDK `v0.20.0.12` = `d889a16` on fork `2FED/rexglue-sdk` branch `mcla/mcla-r-hotfixes` (published
  2026-11-20 on the owner's word; fast-forward from `v0.20.0.11` = `2aff82e` over 22 commits; tag object
  `3cd0ad0`). Source of the l117v SDK DLLs (FidelityFX presenter shaders compiled out).
- Volunteer build **l117v** = host `ecfc182` + SDK `d889a16`, V2 ticket (30-day lease to 2026-12-20),
  build manifest `8BCFDD65`, mcla.exe `A65B6E7E...`. Allowance list on `DebugBundle` of `2FED/RE-MCLA`
  commit `c020638` = l117v only (l81v withdrawn; re-publishable from `138b0ef` if ever needed).
  Package `private/l117v-volunteer` (re-staged with the updated guide; old staging kept as
  `l117v-volunteer-oldguide`). Changelog `docs/volunteer-changelog-l117v.txt`; guide `docs/volunteer-tester-guide.txt`.

## 3. Current owner build and defaults

Owner builds are owner-internal (V1 tickets): candidate `private/lNN-int-input`, signed folder
`private/lNN-internal` (launcher Build folder `private\lNN-internal\build`), arms `private/lNN-arms`,
steps `private/lNN-CHECKLIST.txt`. "manifest" = first 8 hex of `build_manifest_sha256` in the receipt.

| Build | Host | SDK | Purpose | Status |
|---|---|---|---|---|
| l115 | `3228204` | `389d3bd` | Draw-extent bound back on (resolve-only claims) | Signed (5BFD7346); 92-min MAIN session clean (67 fps) |
| l116 | `6c39a1b` | `d889a16` | KI-087 guard fix; thin/black frame probes (174 keys) | Signed (DA88D447); alt-tab test verified; gpu-cat day/night |
| l117 | `2ef5dc3` | `e6eeeb9` | **Current owner build**: CP levers L1-L3 + verify, default off (180 keys) | Signed (285BA6FC); cp-verify2 done: levers unusable |
| l117v | `ecfc182` | `d889a16` | Volunteer build (V2, dev overrides compiled out) | Published (see section 2); PC pre-share check clean |

**Defaults of l117 (= l116; l117v identical except two diagnostics off). PC and Deck unless noted:**
- KI-054 raw-timer hitch hold v2 (l86). KI-013 blend fix and KI-056 Fix A (l81).
- Frame lead: strict bound (l94). Low latency `auto` (l104): on NVIDIA Reflex replaces strict while its
  sleep is fresh; the lead-2 guard skips paused/unfocused/outlier frames and re-arms Reflex after a
  cool-down (l116, KI-087 fix). On AMD Anti-Lag 2 with strict kept on. Nothing on Intel or Wine/Deck.
- Per-draw levers (l104): texture memo v2, RT Update memo, register-write skip. CP lever 1 (l98):
  draw-packet TSC, fence poll interval 64. KI-060 native audio filters (l98).
- Forced HiDef for the 960x540 guest mode (l111, all platforms; only the 1080p target uses 960x540).
- Transfer elision and rectangle draw-extent bound with resolve-only deferred claims (l111/l115,
  **Windows only**; Wine/Deck keeps them off until a Deck run).
- F10 retention: unlimited in internal builds (2 GiB free-space floor), 50 packages / 768 MiB in
  volunteer builds (l102). Volunteer builds also turn off `memory_watch_census` and
  `d3d12_readback_gpu_timing`.

**Measured (RTX 5060 Laptop, owner sessions):** 1080p target play about 67 fps (l115, 92 min), dips to
about 46 in dense daylight scenes, CP-bound by draw count (6.2 k draws, l116); GPU 12.5-12.9 ms (scene
5.9-6.2, transfers 2.5, resolves 2.1, loads/uploads 1.9). 1440p target about 53 fps (l117v, 39 min):
at guest 1280x720 the scene is drawn in two tiles, so draws are doubled (dips 34-36 fps at about 10 k).

## 4. Tested versus pending

**RTX 5060, 1080p target:** pure defaults on MAIN on l111 (1 h), l115 (92 min, nine races, minimap
intact) and l116 (alt-tab test: 0 guard trips, fps returns). Transfer proofs clean (l108-l110 verify
arms, 0 bad). KI-084 minimap fix verified in races on l114 and as default on l115. Pursuits on the
1080p defaults: not recorded.
**RTX 5060, 1440p target:** l117v pre-share check, 39 play min incl. pursuits and races: clean (0 guard
trips, 0 freezes, black 0, repeat presents 0); owner: "can be published".
**Radeon 860M (laptop iGPU):** Anti-Lag 2 A-B-A positive by feel (l103); l111 amd/verify-gpu clean
(elision and extent 0 bad); l111 defaults only a 36 s unlocked sample (32-34 fps by eye). The l115
resolve-only claims and the l116 guard have not run on AMD. KI-081 seen here.
**RX 6800 XT (VM, numbers only):** no run recorded; AMD discrete is unverified on the current defaults.
**Steam Deck:** last runs l96 (native audio filters fixed KI-060: music perfect, about 1 % crackle at
peak voices; lever 1 -2.7 ms/frame; strict-off felt worse). The l98-l116 defaults, the l104 levers and
l117v have NOT run on the Deck. Deck results come back as `D:O` archives: untrusted-archive rules apply.
**Not tested anywhere:** hybrid-GPU laptops (GPU choice and Reflex/Anti-Lag 2 on the chosen card);
KI-026 stall fix (arm `pc/stall-fix`); KI-076 present-stats arm at 60 Hz; KI-075 launcher prefill.
**Rejected by measurement (keep off):** l117 CP levers (L1 safe but small, L2 negligible, **L3
`tex_narrow` UNSAFE: 140 k mismatches, never enable**); `guest_msaa_off` and `collector_2x` (no
effect); lazy stencil; IM_LOAD memo (131 mismatches on the Deck); earlier: fenced delivery, adaptive
lead, bulk register writes, hot pages v2.

## 5. Open issues (status owned by docs/known-issues.md)

- KI-085 DEVICE_HUNG (TDR) at a race start in a heavy verify arm (l112 pc/extent-verify2): unexplained;
  never seen on defaults. Heavy GPU verify arms stay out of races.
- KI-086 possible black frame flash: unconfirmed; l116 probes show black 0 so far; display path leads.
- KI-081 white texture flashes on the Radeon 860M: low priority (driver suspected). KI-082 traffic
  tremble while moving: deferred (also at a true 30 fps). KI-079 tree blink at 06:00: game behaviour,
  low priority. KI-080 ocean shimmer: triage.
- KI-026 Race Back camera never returns: fix in arm, untested. KI-076 60 Hz judder: open (Reflex
  default since l104; present-stats arm not run). KI-077 session bloom: open. KI-041, KI-043: need a repro.
- KI-072 high-speed stutter: traversal stutter fixed; remaining dips are CP-bound by draw count.
- KI-074 night surface flicker: fixed by strict / Reflex-without-strict; mechanism unproven.
- KI-060 Deck audio: fixed by native filters; residual peak-voice dips; current defaults unheard on Deck.
- Fixed since l107: KI-083 tile seam (forced HiDef, l111), KI-084 minimap white square (resolve-only
  claims, default l115), KI-087 guard latch (l116, verified once). Before: KI-013, KI-054, KI-056, KI-068, KI-071.

## 6. Blockers and next actions

No engineering blocker. **Owner:**
1. Send the l117v package to testers when ready (owner 2026-11-20: it may be sent; not recorded as sent).
2. Steam Deck on l117 per `private/l120-DECK-CHECKLIST.txt (the l117 one is archived under private/_archive/misc)`: Run 1 deck/default, Run 2 deck/gpu-new
   (elision + extent under Wine), optional Run 3 deck/levers-off. verify-gpu only in free roam.
3. Deck run of l117v; optional AMD discrete (RX 6800 XT) and a hybrid-GPU laptop run.
4. Open decisions: unbind F4/console for direct starts in volunteer builds (proposal in
   `M11-l117v-volunteer-prep-20261120.md` section a); whether to purge the 135 test files once pushed
   by mistake (commits `9d73984`/`6663100`) from history.

**Agent (after the owner's Deck results):** analyse the Deck runs (cp_busy against l96 55.9 ms, audio,
`MCLA_LOWLAT backend=none result=wine`); decide the Deck defaults for elision/extent. Leads, in order:
(1) one-tile scene at guest 1280x720 for the Deck and the 720p/1440p/4K targets (`guest_msaa_scene_1x`
arm or extended EDRAM; roughly halves draws where the CP limits); (2) GPU floor: resolves 2.0 ms,
stencil transfers 1.3 ms, texture loads 1.0 ms; (3) CP: shared-memory uploads + memory watches (about
1 us per draw), RT memo inputs. Refresh `private/README-current.txt` (stale: says l111). Signing:
`private\tools\pwsh\pwsh.exe -NoProfile -File private\lNN-internal-prepare.ps1` (ends "DONE").

## 7. Standing rules a new session must not miss

- **No builds, tests, worktree checkouts, large log parsing or other file work while the owner plays**:
  `private/tools/wait-for-idle-pc.ps1` plus the coordinator's "hold" / "resume" (KI-067/KI-072 cause).
- **Heavy GPU verify arms never in races** (KI-085). **Never enable `tex_narrow`** (l117 L3).
- `private\p1` (live game source), `private\bundles` and `private\l12` are live: never move, archive or
  delete them in cleanups.
- `tests/` holds 135 untracked files: **never `git add tests/`**.
- Publish (SDK tag, allowance list, package) only on the owner's explicit word. A V2 volunteer build
  cannot run until its allowance list is published.
- Agents never launch the game; every game run is the owner's. Time-of-day arms only on a COPY profile.
- Commit after each closed task; push with green gates. Never infer a physical PASS from implementation.
- Save lineage: MAIN profile `5b71` only through the owner's launcher; copy profiles `68e5` (time-of-day
  and most arm runs) and `d66b`; the Deck copy has the MAIN id and never syncs with bench copies. Never
  open save folders, `save-files-private.json` or `*.dmp`.

## 8. Pointers (docs/evidence)

- Volunteer, publication, Deck plan: `M11-l117v-volunteer-prep-20261120.md`, `M11-l117-deck-plan-20261120.md`.
- Performance: `M11-l107-guest-tiling-and-msaa-20261117.md` (SD path), `M11-l110-pc-extent-20261118.md`,
  `M11-l115-main-session-20261119.md`, `M11-l116-pc-gpu-cat-day-night-20261119.md`, `M11-l117-sdk-cp-per-draw-20261119.md`.
- Low latency: `M11-l96-pc-analysis-20261113.md`, `M11-l103-amd-860m-antilag2-20261115.md`,
  `M11-l116-sdk-guard-and-thin-frames-20261119.md`. KI-084: `M11-l114-sdk-resolve-only-mode-20261119.md`.
- Deck: `M11-l96-deck-analysis-20261113.md`, `M11-deck-cp-budget-and-levers-20261112.md`. Builds:
  `M11-lNN-owner-build-*.md` and the SDK reports (l95-l117); older pointers in README-AI 17c.

## Addendum 2026-11-20 night (owner away until 2026-11-22)

- Current staged owner build: **l119** (unsigned; supersedes l118; SDK c503e32 local). New since l117: KI-088 fix default on (vertex request skips validated; counters `MCLA_VB`), `rt_memo2` (off, verify first), scene-1x arms for the 1280x720 guest (one tile), `MCLA_SHM` census and `MCLA_FAULT_PROBE`, Canary invalidation fix (default on; control key `inval_unwatched_off`). Unsafe l117 arms removed.
- Findings filed: KI-088 (stale vertex data possible; in the published SDK and l117v; not yet observed), shared-memory study, predicated-tiling RE, one-tile 2x scene not feasible without Canary's extended EDRAM and shader sources.
- Owner next (in order): Deck on l119 per `private/_archive/misc/l119-DECK-CHECKLIST.txt` (deck/default, then deck/scene-1x: the main new run, then deck/gpu-new); PC `pc/shm-census` 10 minutes daylight incl. a race at 1080p then 5 minutes at 1440p; read `MCLA_VB vb_*_skip_invalid`: non-zero means KI-088 is live and a volunteer update should follow; KI-082 A/B clear against pc/vb-old next to moving traffic.
- Agent next after those runs: build (a2)/(e1) if the census supports them; extent estimator only if 2x MSAA must be kept at 1280x720; upstream SDK report for KI-088 only with the owner's consent.

## Addendum 2026-11-22

- Deck ran signed l119 (archive D:	9, copy private/owner-deck-20261122): defaults fine; scene-1x = one tile, 16 -> 18.5-20.8 fps, CP 55.5 -> 41.9 ms, fences flicker (KI-089), first-use freezes; gpu-new no fps change; KI-088 LIVE (0.18-0.32 stale skips per frame prevented, slot 95); guest-memory protection costs 16-20 us per call under Wine (8.1 ms CP + 6.5 ms render thread per frame).
- Current staged owner build: **l120** (unsigned; SDK c9c579c local): l119 + `shm_hot` compare-before-copy (off; verify first). Owner next on the Deck: `private/l120-DECK-CHECKLIST.txt` Run 1 deck/shm-hot-verify (need cmp_diff_valid=0), Run 2 deck/shm-hot, Run 3 deck/shm-hot-scene-1x. PC: `private/l120-CHECKLIST.txt` (pc/shm-hot-verify doubles as the owed census run; KI-082 A/B clear against pc/vb-old).
- Volunteer hotfix l120v staged unsigned (KI-088 fix only on the published SDK; 7-day overlap list); owner: not urgent; publish only on the owner's word. l117v is with the testers.
- 2026-11-22 later: PC l120: shm-hot verify and ON clean (neutral on the PC); KI-082 A/B: the KI-088 fix does not change the traffic tremble (KI-082 = traffic body pose, deferred). Storage round 8: l103, l106-l118 archived, build trees and old SDK worktrees deleted, C: 41 -> 57 GiB free; private root now holds l119, l120, l117v, l120v. Deck runs on l120 are the next owner step.
- Storage round 9 (2026-11-22): l107-l110 host build trees and l106-sdk-o deleted (1.11 GiB); stale F10 packages pruned by the owner-approved keep rule: 56 packages / 0.50 GiB deleted, 670 kept (614 because their session id is cited in the docs). C: 58.7 GiB free. Diagnostics still 9.3 GiB: a tighter rule (keep cited sessions only for open KI rows) awaits the owner.
- 2026-11-22 evening: KI-090 (white minimap square with pc/shm-hot ON on l120; cause unproven; lead: span-only compare left other bytes of hot pages stale). Current staged owner build: **l121** (unsigned; SDK b7cc959 local): shm-hot whole-page compare + fault emulation + extended verify. Owner next: sign l121; PC Run 1 pc/shm-hot-verify 15 min with races (counters), Run 2 pc/shm-hot 20+ min with races and a screen recording (F10 while the square is white), optional Run 3 pc/shm-hot-span. Deck shm-hot only after a clean PC Run 2. shm-hot ON arms of l120 must not be used.
- Storage round 10 (2026-11-28): l121 build by-products deleted (0.63 GiB), l119 set archived (0.53 GiB) and its worktree removed (0.49 GiB), 21 stale F10 packages deleted (0.31 GiB; rule unchanged). C: 38.4 GiB free; about 21.5 GiB was consumed outside the project since round 9. A tighter diagnostics rule would free 3.4-3.5 GiB more: awaits the owner. KI-090: not reproduced in 61 min of the span arm (2026-11-24); open, cause unproven. Next owner run: Deck on l121, deck/shm-hot-verify first.
