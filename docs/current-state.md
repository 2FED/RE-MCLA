# Current state and handoff

Updated November 5, 2026 Kyiv (M10 closed in the ledger; l70 published as the only allowed build; owner pre-share check and the push of main pending).

## Active outcome

**M11 ACTIVE (approved 2026-11-06):** **M11 drafted on 2026-11-06; owner approval pending.** The plan is in
`docs/M11-priorities.md`, and the ledger will be README-AI section 17c. M10 is closed and
pushed (`9e541e8`). Testers run l70 only (`DebugBundle` `31c806d`) on SDK `v0.20.0.8`.

The first task is M11-001, KI-068: saves on the Steam Deck under Proton do not work (S1).
The owner details the symptoms, then runs a Deck repro on l70: save from the pause menu,
quit, restart, plus one autosave after a fine or purchase. He sends the session folder. The
agent reads the SDK `[content]` WARN lines, the launcher before/after snapshots and the F10
save-copy lines, then fixes the cause. The same session also yields the Deck seeding line
(M11-008) and the first-run fullscreen state (KI-057). The KI-065 dialog evidence folds
into this task.

Next comes M11-002, one owner-internal build with three items:
- KI-056 album preview: the owner's 720p vs 1080p check first, then the `MCLA_ALBUM`
  diagnostic and a fix A/B;
- KI-066 pursuit/heat state in the F10 guest block;
- KI-067 CPU clock, temperature and load in the census.

Then the Deck performance group (M11-003 core freeing with KI-060, M11-004 hot blocks v2,
the M11-005 gate). Alongside it, the video-memory budget steering (M11-006) and read-back
steps 2-5 (M11-007). No game run happens without announcing it to the owner. Each SDK
delta needs the owner's approval to publish.


**M10 CLOSED on 2026-11-05 (closure regression on l70 pending).** Ledger: README-AI section 17b. Done: M10-001 (PC and Deck verdicts), M10-003 (local union store; PC confirmed, Deck seeding unverified because the Deck sessions used the l56 launcher), M10-008 (triage round 2; KI-035 garage freeze root-caused and fixed, KI-065 mitigated, nine rows closed), M10-009 (l56, l65, l67, l70); partially done: M10-002 (read-back elision shipped; steps 2-5 to M11), M10-005 (Proton defaults: +15% fps and far better pacing on the Deck; hot blocks negative; stable 30 not reached, best about 14.7-14.9 fps driving) and M10-007 (1080p measurement done and the 2048 MB texture-cache soft cap shipped; 2160p and real budget steering to M11); closed as not justified now: M10-006; shelved: M10-004 (Vulkan slower, KI-062/063).

**Volunteer l70 is published as the ONLY allowed build** (`DebugBundle` `31c806d`, one entry, issued 1793897517; l48, l49, l52, l56, l65 and l67 are refused at their next online check, so testers must move to l70 promptly). Clean clone of main `4f69018` (build inputs `dba3e58`) on SDK `v0.20.0.8`; adds the KI-035 fix and the KI-048 soft cap over l67. [Evidence](evidence/M9-005-volunteer-candidate-l70-20261105.md), [changelog](volunteer-changelog-l70.txt).

**Owner steps, in order:**
1. **l70 pre-share check** on the staged `private/l70-volunteer`: PC, one 1080p session with two or three garage trips after races (no freeze; `MCLA_KI035_FINISH_RACE` / `_DONE state=29` when the hook fires; `texture cache soft cap` at startup and `MCLA_VRAM v=1` lines in budget; `platform=windows applied=1` and `applied=5`); Deck, one loop (`platform=wine applied=2` and `applied=6`; the soft cap does not engage under Wine). Details in the l70 evidence, "Owner checks before sharing".
2. Share l70 with the closed group.
3. Done 2026-11-06: the closure line is filled in README-AI 17b, the M10 closure commit is `9e541e8`, and main is pushed to origin.

**M11 inputs** (also in README-AI 17b):
- Hot blocks v2 (Deck): change the copy shape (batch hot copies per block per submission, or snapshot before the first draw of a submission); SDK `mcla/m10-hotblocks` `cdf469a`, local.
- Deck core freeing and KI-060 (audio underruns): polling thread F800068C and audio-side thread F8000534 (about 1.6 cores together); kick-interval A/B; CP draw phases.
- M10-006 (multi-threaded CP) revisit once a Deck core is free.
- M10-007 continued: budget steering of the soft limit against the live budget, residency priorities, render-target trim; the 2160p footprint run on the 3080 Ti or 6800 XT; one long cap-on session to close KI-048.
- Read-back steps 2-5 of the M10-002 design (with first photo, livery and album checks).
- Pre-warm a1 (derived-hint `.xpso` after a source-data-policy amendment) and a2 (engine effect-loader hook spike); Deck seeding check on a current launcher.
- KI-052: prefilled launcher names so the Deck needs no typing.
- KI-059: fill the D3D12 F10 graphics block in the SDK.
- KI-054: frame-pacing or dt-smoothing study (varying time steps under Unlocked timing).
- KI-066 / KI-067: police/pursuit state in the F10 guest block with logged transitions; CPU effective clock, temperature and system load in the per-minute census.
- KI-035 refinement: exit state 20 one frame earlier so the hook (and its brief LOD pop) is rarely needed.
- Build-script quirk: the automatic "every `*_test` target" check of the l66b/l67b host build script matches 0 help lines; use an explicit list with a cross-check (as l70 did) or fix the regex.
- Gate build note: prepare scripts must use `private/gate-host-build/mcla_build_lease_probe_win.exe` (or build that target first); the old `private/l35-launcher-build` probe was removed by the 2026-11-05 cleanup.

**Open for the owner (not blocking):** KI-056 album preview check, KI-064 stray-file check, KI-065 Deck repro (F10 at the start of a pull-over); tester confirmation of KI-022 (wheel centering) and KI-047 (police livery); name the canonical save copy if the Deck profile copy (same id as MAIN) is played on.

## Builds (private, owner PC)

- **Volunteer: l70 only** (`private/l70-volunteer`, `mcla.exe` `146FBBB5...`, V2 kill-switch ticket with `capabilities 1080p,1440p,unlocked`; lease valid until about 2026-12-05). l67 (`private/l67-volunteer`) and older are withdrawn.
- **Owner builds:** l68 (`private/l68-internal`: KI-035 warp probe and watchdog arms, KI-048 census and soft-cap A/B) and l69 (`private/l69-internal`: l68 plus the KI-035 fix; arms `private/l69-arm-fix.cmd`, `l69-arm-fix-off.cmd`, `l69-arm-clear.cmd`). Owner builds use V1 14-day tickets without the online check.
- **SDK:** pin `v0.20.0.8` = `c63895c`. Volunteer install `private/l67v-sdk-install` (l67 clean clone, D3D12 only, ctest 2014/2014; reused read-only for l70); dev install `private/l67-sdk-install` (used by l68/l69).
- **Gate build:** `private/gate-host-build` (holds the rebuilt `mcla_build_lease_probe_win.exe` used by the prepare scripts' signed-list verification).

## Where the work lives

- **Project:** branch `main` in the main checkout; local commits since `99d6e56` are unpushed and are pushed at the M10 closure. The pre-M9 uncommitted main tree is preserved on local branch `wip/main-dirty-m8-20261101` (`f65ea53`); branch `claude/admiring-jang-24d485` (`99d6e56`) is on origin.
- **SDK fork:** `2FED/rexglue-sdk` `mcla/mcla-r-hotfixes` at `c63895c` (`v0.20.0.8`); SDL fork `2FED/SDL` `422c186`. The submodule checkout under `third_party/rexglue-sdk` still holds the old dirty `5dd58f8` tree (gitlink `c63895c`); builds use private clean clones.
- **Local SDK branches** (in `private/l39-sdk-src`): published (reachable from the fork branch): `mcla/m10-perf-split` `483458d`, `mcla/m10-cpu-waits` `62cd66d`, `mcla/m10-readback` `141e13f`, `mcla/m10-prewarm` `9d8ede4`, `mcla/m10-ki047-targeted` `561a737`, `mcla/m10-watch` `43577c8`, `mcla/m10-content-logs` `c63895c` (worktree `private/l67-sdk-wt`), plus the M9 branches `mcla/ki-022-ffb-rearm` `daa1ae5`, `mcla/ki-040-xinput-rescan` `f519dd5`, `mcla/ki-047-diag` `851f03b`. Unpublished: `mcla/m10-hotblocks` `cdf469a` (worktree `private/l66-sdk-wt`).
- **Published allowance:** `DebugBundle` of `2FED/RE-MCLA` at `31c806d` (l70 only).
- **Host source:** raw timer `src/mcla_raw_timer.cpp`; texture budget and soft cap `src/mcla_texture_budget_win.cpp`; `MCLA_VRAM` census `src/mcla_vram_census_win.*`; M10 speed-up and Proton defaults `src/mcla_m10_defaults.*`; KI-035 warp probe and finish hook `src/mcla_warp_probe.h`, `src/mcla_ki026_probe.*`; read-back sync `src/mcla_readback_sync.*`.
- **Logged statistics** (once a minute): frame pacing, GPU busy, perf split, replay and write-watch, read-back, pipeline waits, `MCLA_VRAM`.

## Known issues to carry

Open: KI-013, KI-026, KI-032, KI-033 (extra sharpness), KI-036, KI-038, KI-041, KI-043, KI-048 (mitigated by the soft cap), KI-052, KI-054, KI-056, KI-059, KI-060, KI-062/063 (shelved with Vulkan), KI-065 (mitigated), KI-067. Fixed, confirmation pending: KI-022 and KI-047 (testers), KI-057 (Deck fullscreen; verify on the Deck), KI-064 (owner check). Fixed and owner-confirmed in M10: KI-035, KI-061. Unlocked-mode limits:

- Menus drawn over the world run fast.
- Heavy intersections drop to about 30-40 FPS (1440p GPU-bound on the RTX 5060).
- One `DEVICE_HUNG` in eight l29 title runs.

## Rules that still apply

- Never touch MAIN saves except through the owner's launcher.
- Announce every game run to the owner.
- Commit after each closed task; review and push after each milestone (owner workflow 2026-11-01).
- Volunteer builds come from a clean clone at a published SDK pin; publishing a signed list needs the owner's explicit yes.
