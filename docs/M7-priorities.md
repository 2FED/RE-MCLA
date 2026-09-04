# M7 current-state review and execution order

Date: 2026-09-04. Owner/reviewer: primary Codex agent, with independent
read-only save/dependency/evidence review. Scope: planning/documentation;
no runtime fix, physical test or new platform-support claim.

## Decision

**Prioritize stock-mode Deck performance and cross-build save continuity
before broad campaign play.** This is compatibility work needed for usable
local testing, not an early 60/120-FPS enhancement project. M7 stays 0/16
complete. Stable IDs and two full-campaign passes remain; execution order
and the M7-016/M7-009 criteria are refined.

## Findings

| Priority | Evidence and problem | Disposition |
|---|---|---|
| P0 | KI-027: Deck comparisons produce about 7--9 new outputs/s with audio breakup; native Vulkan and forced clocks did not resolve it. | Make throughput/pacing/audio an explicit M7-016 sub-gate before campaign coverage. |
| P0 | Review found an M6-only seed-selection risk in the publisher. | KI-028: developer-side full-tree lineage/archive/publication protection implemented and fixture-tested in 0.13.2.0; actual progressed successor reload remains the next save gate. See [evidence](evidence/M7-009-portable-save-lineage.md). |
| P1 | Boot/garage/F10 is proven; usable performance and save evolution/reload are not. The task paragraph stopped at the prior 0.12 diagnostic candidate. | Separate implemented tooling from physical gates; identify 0.13.0.0 as the pending binary. Do not rebuild just for this documentation release. |
| P1 | M7-001 matrix preparation waits for all of M7-016, while M7 prerequisites require a campaign checklist; reached-import auditing appears last. | Allow planning/save fixtures while awaiting hardware; audit reached imports incrementally, not only after two campaigns. |
| P2 | Rendering/music defects have generic follow-up owners; wheel centering lacks available hardware. | Assign existing M7 tasks below; keep separate hypotheses and defer physical wheel work. |
| P2 | README-AI is roughly 278 KB of guidance plus history; current SDK references drifted in architecture/issues. | Add a short replace-in-place handoff and read order; correct current pins without rewriting old evidence. |

Sources: [Deck analysis](evidence/M7-016-deck-performance-diagnostic.md),
[portable boundary](evidence/M7-016-portable-campaign-bundle.md),
[M6 closure](evidence/M6-closure-review.md), [issues](known-issues.md), and
the builder's inspected save-selection/staging code.

## Ordered work packages (existing IDs)

| Order | Work package | Completion signal |
|---|---|---|
| 1 | **M7-016 performance**, KI-027 | Analyze pending 0.13.0.0 pair, attribute remaining cost, make one bounded causal correction at a time, verify actual driving/audio recovery. |
| 1, independent | **M7-009 portable save lineage**, KI-028; prerequisite subset of M7-016 | Validated complete return selection/import, provenance/backups, rejection of conflict/incomplete data, successor-build reload without regressing progress. |
| 2, preparation now | **M7-001 campaign matrix** | Mandatory nodes/categories/dependencies/restart saves and reusable evidence; unknown coverage stays unverified. |
| 3 | **M7-016 remaining platform matrix**, then **M7-002** | Short input/audio/window/save/recovery checks; isolated clean-profile new-game/tutorial/unlock/name flow without touching owner progress. |
| 4 | **M7-003/007/008/014**, **M7-012** interrupts for blockers | Event/retry/Race Back, garage/delivery/recovery transitions and newly reached imports. F10 on recurrence instead of endless non-repro retries. |
| 5, interleaved with progression | **M7-004/007/003 render triage**, **M7-006 audio/media**, remaining content tasks | Controlled evidence for vehicle lighting/neon/white paint (004), world coverage/shimmer (007), HUD flicker (003), and music continuity (006); no blanket parity claim. |
| 6 | **M7-011/013/015** | Two full clean-campaign evidence sets, save continuity and compatibility review. Normal resumable sessions, not two-hour sitting requirements. |

This order does not waive other dependencies or close full M7-009 when its
portable subset passes. M7-012 blocker fixes and M7-014 reached-import audit
run throughout progression. M7-015 documentation stays current throughout;
its final acceptance still requires the full campaign evidence.

Optional resolution/60+/VRR, native Linux/macOS and public packaging remain
M8/M9. Shader/cache optimization may enter M7-016 only when evidence proves
it blocks stock gameplay, not as speculative renderer rewrites or permission
to distribute guest-derived shader content.

## M7-016 sub-gates

- **Tooling:** self-rooting launcher, immutable verification, path containment,
  F10 return and Windows relocation have evidence. Retain their tests; partial
  Deck boot is not platform support.
- **Performance:** usable stock-paced gameplay. Engineering target: sustained
  30 new game frames/s (about 33.3 ms) during representative driving, with
  frame-time tails reported and no persistent audio starvation. Output
  counters do not alone prove simulation speed/visible unique content:
  require owner motion/audio observation. A lower stable candidate needs
  explicit owner acceptance of the limitation, not automatic full Deck PASS.
  Today's severe cadence remains a blocker.
- **Save lineage:** prove progression from a returned build reloads in the
  next. Clocks/filenames alone are insufficient; validate full profile trees,
  package/header identity, hashes and session/bundle provenance. Archive both
  sides before replacement. Divergent branches require owner selection;
  never silently pick the newest unrelated profile or discard another branch.
- **Physical matrix:** pin Proton/mode, controls, sustained audio, fullscreen
  toggles, save/reload, interrupted-session recovery, F10 and synced return.
  Keep Desktop and Game Mode/gamescope evidence separate. An exact no-go may
  retain Windows, but cannot merely avoid investigating the stock-mode blocker.

## Performance experiment protocol

1. Use the already published 0.13.0.0 candidate first. Record build/SDK/hash,
   Proton/backend/mode, resolution/save/route and cold/warm cache. Do not
   change clocks, renderer and scheduling simultaneously.
2. Initial check: the requested 30--60-second driving interval between two
   F10 READY captures, plus owner motion/audio observation. Note capture
   overhead and the brief visible overlay as measurement caveats.
3. Compare new-output rate, available frame-time percentiles, underrun deltas,
   thread CPU, command waits, memory faults, shader/PSO/stream activity.
   Unavailable is unknown, not zero. Submission spans are not active GPU time;
   one fence metric does not cover all fences; multithread durations overlap.
4. If the UI fix only corrects MangoHUD, say so. Investigate measured remaining
   CPU/command/memory cost next. Add counters only for a named unanswered
   question; do not ship diagnostic-only iterations without a decision rule.
5. Review/unit-test each correction and run the relevant Windows regression.
   Do not disable coherency, needed readback, checks, sound or stock timing
   to improve an overlay number.
6. Once short driving is usable, use a targeted 5--10-minute race/garage/reload
   check. Long sessions require a specific lifetime/resource hypothesis or an
   agreed campaign gate, not every small patch.

The existing 2--6-month campaign estimate remains provisional, not a promise.
The feasibility front gate has no reliable date yet; re-estimate after the
pending pair and save-lineage implementation instead of inventing a deadline.

## Context and review cadence

AGENTS.md is durable process; README-AI owns architecture/workflow and the
milestone ledger; current-state.md is the short handoff; evidence reports
preserve detailed history. Update the handoff at bounded task/experiment
boundaries, without duplicating chronological logs. Existing history stays.

One chat per coherent outcome is the practical default: keep an active Deck
investigation together; use a fresh chat for a separate save/campaign outcome
after updating the handoff. Compaction is supported, not a reason to restart
after each message. Official guidance recommends focused chats and concise
reusable instructions: [Codex best practices](https://learn.chatgpt.com/guides/best-practices).

No milestone closes, extra physical session is scheduled or commit is pushed
in this review. The pending 0.13.0.0 bundle and saves/evidence stay unchanged.

## Review verification

- Independent read-only review: no blockers; task IDs/criteria, save-risk
  scope and pending-artifact distinction reconciled by the primary agent.
- Structural scan clean; ast-grep 3/3 fixtures pass; `git diff --check` passes.
- Documentation audit: 151 unique tasks / 151 criteria, M7 0/16 unchanged,
  28 unique known issues, local links resolve, SDK commit and current bundle
  pointer unchanged.
- After review/tests, repository version advances to **0.13.1.0** for this
  documentation-only release. No runtime rebuild, new SDK tag, portable
  publication, save mutation or repeated physical test is required.
