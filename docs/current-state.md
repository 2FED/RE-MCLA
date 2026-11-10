# Current state and handoff

**Update 2026-11-08 (end of day):** l78 PC regression PASS; l79 A/B: placed texture heaps removed the PC traversal stutter (p99 53 -> 37 ms); KI-013 traffic-light reflections FIXED (owner PASS on l80); KI-071 voice confirmed on the default; SDK v0.20.0.10 = ca63f94 published to the fork. Deck still ~15 fps with crackling audio: root causes found (the game's sound-manager thread spins on Sleep(0) ~180k/s; our Deck arms since l71 disabled the M10 Wine memory-watch defaults). Staged for the owner: **l81** (host a9de776 + SDK 2aff82e: blend fix + Fix A on, Deck zero-delay backoff + game audio-thread priority, sparse Deck arms) with `private/l81-CHECKLIST.txt`; **l81v** volunteer candidate (unsigned, allowance list l81v only) with `private/l81v-CHECKLIST.txt`; tester guide refreshed. Before "publish l81v": publish SDK 2aff82e as v0.20.0.11. Next engineering: Deck hot pages v2 after a census on the corrected baseline; CP fast paths. See docs/M11-priorities.md tail.

Updated November 7, 2026 Kyiv (M11 active; l78 defaults build in preparation).

## Active outcome

**M11 ACTIVE since 2026-11-06** (approved by the owner). Plan: `docs/M11-priorities.md`. Ledger,
owner builds l71-l78 and the SDK lineage: README-AI section 17c.

**Now: l78, the defaults build.** Host main `f86da9d` (62 dev keys, including the off-keys
`readback_steady_exempt_off`, `fence_write_deferred_off` and the Deck arm `fence_defer_gap_2000`) on SDK
`d61c597` (`mcla/m11-defaults`, install `private/l78-sdk-install`, `0.20.0.14-dev.gd61c597`, ctest
2137/2137). Staged 2026-11-07 late: candidate `private/l78-int-input` (mcla.exe 3E37F985...), sign `private/l78-internal-prepare.ps1`, arms `private/l78-arms` (9), steps `private/l78-CHECKLIST.txt`; evidence M11-l78-owner-build. Staging note at the time of the rewrite was: arms `private/l78-arms` and steps
`private/l78-CHECKLIST.txt` exist, but no candidate folder yet. Next: finish the candidate, then the owner
regression routine on the PC and the Deck. The routine covers the KI-047 livery check, KI-035 garage trips,
one Locked run, a Deck loop, a cold shader cache and photo mode, with the new fence, CP-lead and
submission counters.

**Decisions already taken (in the l78 SDK defaults):**
- **Steady exemption on.** This is M11-007a, narrowed by 007b/c. It gives +6 ms/frame (about 34 -> 40 fps
  on South Central Tour 1080p). The Locked-mode control showed it is data-safe.
- **Deferred fence writes on (M11-007e).** Steady-fence runs were clean 3 of 3 at about 40 fps, against 7 of
  7 flickering without deferred fences. There was no fps cost.
- **KI-071 frame-walk fix (W1) on.** Since `f6fc975`.

**State of the issues behind those decisions:**
- **KI-074** (flicker, despawns and floating traffic above about 36 fps in Unlocked) is mitigated by the
  deferred fences. The discriminator is the GPU running a frame behind the CP; dt size is not the cause.
  The owner's goal of correct behaviour at 60+ fps stays open.
- **KI-074b** (despawns, also seen at 1440p without the exemption) is open.
- **KI-067/KI-072:** the lag episodes were the project's own builds on the play PC. Rule: builds are held
  during owner sessions (see the rules below). The CP-headroom line continues: M11-016 (unmeasured) and
  M11-017 ThinLTO+PGO.

**After l78:**
1. SDK publication `v0.20.0.10`+ after the owner's approval. Open choices: the fetch identity default and
   the steady/fence defaults after the regression.
2. M11-017.
3. KI-074b.
4. The Deck items (plan M11-003/004: core freeing with KI-060, hot blocks v2).
5. KI-073 stays backlog.

**Open for the owner:**
- The l78 regression on the PC and the Deck.
- The KI-071 by-ear check of the new default (l77 `base` vs `walk-off` is not recorded).
- The M11-016 fetch identity A/B (never run).
- A KI-068 PC save/reload smoke test (not recorded).
- The KI-056 Fix A default (`mcla_album_preview_restore`, off).
- Carried: the KI-067 re-test with no build running; KI-064 stray-file check; tester confirmation of KI-022
  and KI-047; the canonical save copy if the Deck profile copy (MAIN id) is played on.

**M10 CLOSED** (2026-11-05; closure regression done 2026-11-05/06; closure commit `9e541e8`, pushed).
Ledger: README-AI 17b. l70 was shared with the testers on 2026-11-06.

## Builds (private, owner PC)

- **Volunteer: l70 only** (`private/l70-volunteer`, `mcla.exe` `146FBBB5...`, `DebugBundle` `31c806d`, one
  entry; lease valid until about 2026-12-05). Older volunteer builds are withdrawn.
- **Owner-internal builds** use V1 tickets. Candidates are `private/l7x-int-input`, with arms
  `private/l7x-arms` and steps `private/l7x-CHECKLIST.txt`.

| Build | Host | SDK (install) | Purpose |
|---|---|---|---|
| l71 | `3e78c2c` | `d08813b` (`l71-sdk-install`) | KI-068 fix, triad, KI-069/070, KI-071 audio |
| l72 | `060dbcf` | `d08813b` (`l71-sdk-install`) | KI-056 Fix A, kick A/B |
| l73 | `1dec761` | `4a3aea1` (`l73-sdk-install`) | XMA fixes, fetch identity, steady key |
| l74 | `c03a1df` | `8f949f7` (`l74-sdk-install`) | XMA sibling trace, first steady A/B |
| l75 | `60c6b93` | `4bb8c02`, install later `bd9314f` (`l75-sdk-install`) | XMA trim, steady narrowing |
| l76 | `406ee7e` | `a4a5309` (`l76-sdk-install`) | W1 frame walk, lock probe |
| l77 | `afadfa6` | `590c484` (`l77-sdk-install`) | KI-074 experiments, deferred fences |
| l78 | `f86da9d` | `d61c597` (`l78-sdk-install`) | Defaults on, fence/CP-lead counters (in preparation) |

- **SDK:** `v0.20.0.9` = `d08813b` (KI-068, KI-069, KI-071 speaker config) is published on the fork. The
  host submodule pin is unmoved at `c63895c` (`v0.20.0.8`). Everything after `d08813b` is local.
- **Gate build:** `private/gate-host-build` (holds `mcla_build_lease_probe_win.exe` for the prepare
  scripts' signed-list verification).

## Where the work lives

- **Project:** branch `main`. At the time of writing origin/main is `70823b5` and local main is `723dd24`
  (9 commits ahead, unpushed). The pre-M9 dirty tree is kept on local branch
  `wip/main-dirty-m8-20261101` (`f65ea53`).
- **SDK fork:** `2FED/rexglue-sdk` `mcla/mcla-r-hotfixes` at `d08813b` (`v0.20.0.9`); SDL `2FED/SDL`
  `422c186`. The checkout `third_party/rexglue-sdk` still holds an old dirty tree; builds use private
  worktrees of `private/l39-sdk-src`.
- **Local M11 SDK branches** (one linear chain; worktree in brackets):

| Branch | Commit | Worktree |
|---|---|---|
| `mcla/m11-content-unmount` | `d08813b`, published | `private/l71-sdk-wt` |
| `mcla/m11-audio` | `dbe6d54`, picked as `d08813b` | `private/l71-sdk-audio-wt` |
| `mcla/m11-fetch-identity` | `ae35ae3` | `private/l72-sdk-fetch-wt` |
| `mcla/m11-steady-key` | `e412c5c` | `private/l73-sdk-steady-wt` |
| `mcla/m11-xma-doubling` | `4a3aea1` | `private/l73-sdk-xma-wt` |
| `mcla/m11-xma-sibling` | `8f949f7` | `private/l74-sdk-xma2-wt` |
| `mcla/m11-xma-trim` | `c526cb3` | `private/l75-sdk-xma3-wt` |
| `mcla/m11-steady2` | `bd9314f` | `private/l75-sdk-steady2-wt` |
| `mcla/m11-steady3` | `a4a5309` | `private/l76-sdk-steady3-wt` |
| `mcla/m11-xma-walk` | `b6d361e` | `private/l76-sdk-xma4-wt` |
| `mcla/m11-fence` | `590c484` | `private/l77-sdk-fence-wt` |
| `mcla/m11-defaults` | `d61c597` | `private/l78-sdk-defaults-wt` |

  The M10 branches are unchanged (see README-AI 17b). `mcla/m10-hotblocks` `cdf469a` is unpublished.
- **Published allowance:** `DebugBundle` of `2FED/RE-MCLA` at `31c806d` (l70 only).
- **Host source:**

| Area | Files |
|---|---|
| Dev-override keys | `src/mcla_dev_overrides.*` |
| Album probe and Fix A | `src/mcla_album_probe.*` |
| Pursuit probe | `src/mcla_pursuit_probe.*` |
| Texture-lock probe and ensure hook | `src/mcla_lock_probe.*` |
| XMA consume probe | `src/mcla_xma_consume_probe.*` |
| F10 minidump policy | `src/mcla_diagnostic_trigger.h` |
| Raw timer | `src/mcla_raw_timer.cpp` |
| Texture budget and soft cap | `src/mcla_texture_budget_win.cpp` |
| `MCLA_VRAM` census | `src/mcla_vram_census_win.*` |
| M10 and Proton defaults | `src/mcla_m10_defaults.*` |
| KI-035 hook | `src/mcla_warp_probe.h`, `src/mcla_ki026_probe.*` |
| Read-back sync | `src/mcla_readback_sync.*` |

## Known issues to carry

| Issue | Status |
|---|---|
| KI-056 album preview | Fixed by Fix A (owner PASS on l72); default off pending the owner's decision |
| KI-065 Deck save dialog | Folded into KI-068 (fixed) |
| KI-066 police fines | Closed (console-accurate) |
| KI-067 host slowdown | Cause found (own builds); re-test pending |
| KI-068 Deck saves | Fixed (owner PASS on the Deck, l71) |
| KI-069 picture freezes | Fixed (async logging; SDK `v0.20.0.9`) |
| KI-070 F10 minidump | Fixed (no live dump on F10) |
| KI-071 doubled cut-scene voice | Fixed (W1); default on in the local SDK; unpublished |
| KI-072 high-speed stutter | Attributed to KI-067; CP headroom work continues |
| KI-073 album export | Backlog (design ready) |
| KI-074 Unlocked flicker | Mitigated by deferred fences |
| KI-074b despawns | Open |

Still open from earlier milestones: KI-013, KI-026, KI-032, KI-033, KI-036, KI-038, KI-041, KI-043,
KI-048 (mitigated by the soft cap), KI-052, KI-054, KI-059, KI-060, KI-062/063 (shelved with Vulkan).
Confirmation pending: KI-022 and KI-047 (testers), KI-057 (Deck), KI-064 (owner).

## Rules that still apply

- Never touch MAIN saves except through the owner's launcher.
- Announce every game run to the owner.
- **No builds or test runs on the play PC while the owner plays.** The coordinator gives an explicit
  "hold" before an owner session and "resume" after it. The idle guard
  `private/tools/wait-for-idle-pc.ps1` (checked only at the start of a step) is not enough on its own.
- **Owner-side measurement tools:**
  - `private/tools/proc-sampler.ps1` for per-process CPU every 2 s (CSV under the launcher's LocalAppData
    folder). Instructions are in `private/tools/proc-sampler-README.txt`.
  - An HWiNFO64 sensor CSV log for clocks, power and temperature.
  - Both run during performance A/Bs on the fixed Arcade race "South Central Tour".
- Commit after each closed task; review and push after each milestone.
- SDK publication and signed-list publication need the owner's explicit yes. Volunteer builds come from a
  clean clone at a published SDK pin.

**2026-11-10.** l81 owner PC run PASS; SDK v0.20.0.11 (2aff82e) published; l81v signed by the owner and published (DebugBundle 138b0ef, l81v only). Owner is running the l81 Deck loops (default / zero-native / census), results pending. Staged, unsigned: l82 owner build (host 9168872 + SDK c40db4b, CP bulk register writes A/B, `private/l82-CHECKLIST.txt`). Designed, not implemented: KI-026 warp stall fix (l83 candidate). Next: l81 Deck results -> hot pages v2 decision; l82 Deck A/B; then KI-026 implementation.

**2026-11-10, later.** l81 Deck analysed (fps 14.3->16.4; audio unchanged, XAudio engine thread starved; hot pages v2 NO-GO). l82 Deck run done by the owner (D:\82), analysis in progress. KI-026 and KI-054 designs recorded; KI-054 code landed in d14621a (all off). Staged, unsigned: l83 (KI-054 A/B, `private/l83-CHECKLIST.txt`). Next: l82 Deck verdict on bulk register writes; owner signs and runs l83 on PC (ambprobe -> snapshot -> hitchhold), then Deck.
