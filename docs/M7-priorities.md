# M7 current-state review and execution order

Date: 2026-09-04. Owner/reviewer: primary Codex agent, with independent
read-only save/dependency/evidence review. Scope: planning/documentation;
no runtime fix, physical test or new platform-support claim in that review.

2026-09-05 performance update: four complete 0.15.1.0 F10s and the complete
profile/album are accepted and hash-archived. Explicit gameplay/pause pairs
yield 12.78/8.60 outputs/s versus prior 12.14/10.02. Pause packet density rose
22%; per-packet GPU-thread CPU is essentially unchanged. PAUSE executes but
no material gain or causal regression is established; pause resolve waits
are still zero. Broken Wine unwinds do not identify descriptor cost.
MCLA 0.16.0.1 / SDK v0.18.0.1 conservatively reuses unchanged same-epoch
texture descriptor sets with actual-value comparisons and intact resource
requests/barriers/lifetimes. Six counters measure actual reuse and update cost.
Exact final native Vulkan and D3D12 controls pass saved gameplay/PAUSE/Settings;
Vulkan shows about 50% cumulative stage-set reuse. This is avoided work, not
a Deck speedup claim. Next: short same-settings Deck cost/cadence/audio evidence.
See [the current experiment](evidence/M7-016-deck-descriptor-reuse.md).
Old pending-pair wording below is historical only where explicitly tied to
the 2026-09-04 review.

## Decision

**Prioritize stock-mode Deck performance and cross-build save continuity
before broad campaign play.** This is compatibility work needed for usable
local testing, not an early 60/120-FPS enhancement project. M7 stays 0/16
complete. Stable IDs and two full-campaign passes remain; execution order
and the M7-016/M7-009 criteria are refined.

## Findings

| Priority | Evidence and problem | Disposition |
|---|---|---|
| P0 | KI-027: latest gameplay/pause pairs yield 12.78/8.60 new outputs/s; pause workload differs, and the spin hint shows no material gain. | Keep throughput/pacing/audio the M7-016 front gate; validate conservative descriptor reuse and measure avoided work before broad campaign coverage. |
| P0 | Review found an M6-only seed-selection risk in the publisher. | KI-028: developer-side full-tree lineage/archive/publication protection implemented and fixture-tested in 0.13.2.0; actual progressed successor reload remains the next save gate. See [evidence](evidence/M7-009-portable-save-lineage.md). |
| P1 | Boot/garage/F10 is proven; usable performance and save evolution/reload are not. | Separate tooling, accepted historical returns and pending physical correction; current candidate is 0.16.0.1, not a boot-only platform PASS. |
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
| 1 | **M7-016 performance**, KI-027 | Accepted 0.15.1 pairs show no material hint gain and still zero pause resolve waits. Validate 0.16.0 descriptor reuse/cost and short driving/audio evidence; retain only evidence-backed optimizations. |
| 1, separate follow-up investigation | **M7-016 Deck CPU scaling / matched Xenia baseline** | Profile work versus waits and synchronization; compare identical scenes/settings before deciding which independent work can safely run in parallel. Produce an evidence-backed optimization backlog, not a core-affinity-only change or promised FPS gain. |
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

2026-09-05 preparation update: M7-001 now has a reviewed
[historical evidence seed](campaign-coverage-matrix.md), with eleven scenarios
and explicit unknowns. This preserves prior work without claiming a mandatory
retail catalog, dependency graph, clean-campaign credit or current-artifact
playability. The bounded native ABI/layout review now establishes how paired
name/descriptor arrays are indexed, but not populated mandatory events or
dependencies. Current F10 is not an explicit catalog-memory snapshot; any
future opt-in collector must use isolated scope and validated live ranges.
The performance and successor-save front gates retain priority; no runtime
collector or pending-bundle replacement was introduced for matrix preparation.

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

1. Use 0.16.0.1 / SDK 0.18.0.1 for descriptor reuse and precise cumulative
   update timing. The 0.15.1 gameplay/pause pairs are accepted; do not repeat
   their attribution as a new test of unchanged code. Record build/SDK/hash,
   Proton/backend/mode, resolution/save/route and cold/warm cache. Do not
   change clocks, renderer and scheduling simultaneously.
2. Capture two F10s 30 seconds apart during driving. Open pause, settle two
   seconds, then capture two F10s 30 seconds apart while paused. Analyze each
   pair explicitly with process-lifetime identity. Record owner motion/audio
   observation; note capture overhead and the brief visible status overlay.
3. Compare new-output rate, available frame-time percentiles, underrun deltas,
   thread CPU, command waits, memory faults, shader/PSO/stream activity.
   Unavailable is unknown, not zero. Submission spans are not active GPU time;
   one fence metric does not cover all fences; multithread durations overlap.
4. The UI fix is physically confirmed to correct the inflated overlay count.
   Pause readback waits are now measured at zero, not a remaining theory to
   retest. The current candidate preserves the earlier spin hints, whose
   material benefit is unproven, and now measures conservative descriptor
   reuse. Require actual cost/output/audio evidence before accepting a gain.
   Only add counters for a named unanswered question, never as a substitute
   for a defined experiment or indefinitely repeat a completed owner test.
5. Review/unit-test each correction and run the relevant Windows regression.
   Do not disable coherency, needed readback, checks, sound or stock timing
   to improve an overlay number.
6. Once short driving is usable, use a targeted 5--10-minute race/garage/reload
   check. Long sessions require a specific lifetime/resource hypothesis or an
   agreed campaign gate, not every small patch.

The existing 2--6-month campaign estimate remains provisional, not a promise.
The feasibility front gate has no reliable date yet; re-estimate after the
descriptor comparison and successor-save reload instead of inventing a deadline.

## Separate follow-up: Deck CPU scaling / matched Xenia baseline

Owner requested this bounded research package on 2026-09-05. Finish the
current descriptor comparison first, then investigate remaining stock-mode
headroom separately; M7-016 remains its owner and M8-011 later consumes the
reproducible benchmark method for optional enhancements. No task ID is closed
or added, and no new conversation or speculative threading rewrite is started.

The owner reports roughly 15--20 gameplay FPS in Xenia on this Deck. This is
an uncontrolled comparator, not proof of a fixed overhead gap: record the
exact Xenia build, Proton/backend, resolution, patches, save/scene, cache
warmth and presentation versus actual simulation cadence before comparison.
Retain stock timing and comparable graphics; do not count repeated presents.

Our latest gameplay pair has the busiest guest thread at 90.94% and GPU
Commands at 81.61% of one logical core, but other guest threads also consume
60.24% and 38.02%. Thus "only two cores are used" is incorrect. These thread
totals neither identify fixed physical cores nor prove that the hot work is
parallelizable. Affinity can move a thread without shortening its serial work.

Deliverables and decision gates:

- A same-scene, same-settings baseline with repeated short samples, frame-time
  tails, motion/audio observations and attributable per-thread work/waits.
- A supported sampling/call-stack route with matching module/unwind identities;
  F10 snapshots alone are not a statistical CPU profile. Separate guest work,
  busy polling, command translation/driver work, memory protection, streaming,
  audio and synchronization. Inclusive spans must not be summed as CPU budgets.
- Ranked serial-overhead fixes and genuinely independent batching/worker
  candidates. Each candidate needs an ordering, lifetime, contention and
  determinism design plus a before/after regression test. Do not split guest
  or GPU command streams blindly, bypass coherency, or alter the guest clock.
- Explicit stop/continue conclusions: useful parallel headroom, cheaper serial
  work, or a hardware/compatibility limit. No automatic 30-FPS guarantee or
  two-hour owner test; request only the shortest missing hardware measurement.

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

No milestone closed, extra physical session was scheduled or commit pushed
by the 2026-09-04 planning review. The 2026-09-05 experiment above now supersedes
its pending artifact; the complete 0.15.1.0 return is archived outside sync.

## Historical review verification (2026-09-04)

- Independent read-only review: no blockers; task IDs/criteria, save-risk
  scope and pending-artifact distinction reconciled by the primary agent.
- Structural scan clean; ast-grep 3/3 fixtures pass; `git diff --check` passes.
- Documentation audit: 151 unique tasks / 151 criteria, M7 0/16 unchanged,
  28 unique known issues, local links resolve, SDK commit and current bundle
  pointer unchanged.
- After review/tests, repository version advances to **0.13.1.0** for this
  documentation-only release. No runtime rebuild, new SDK tag, portable
  publication, save mutation or repeated physical test is required.
