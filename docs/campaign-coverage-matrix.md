# M7-001 campaign evidence seed

Reviewed: 2026-09-05. **Preparation only; M7-001 remains in progress.**
The mandatory retail node catalog, dependency graph and two clean-campaign
passes are not complete. Eleven rows below are historical test scenarios,
not eleven distinct campaign nodes or a completion percentage.

Machine-readable inventory: [campaign-coverage-seed.json](../config/campaign-coverage-seed.json).
Task criterion: README-AI, M7-001. Execution order: [M7 priorities](M7-priorities.md).

## Reusable evidence and its limits

| Scenario | Existing evidence | What it does not establish |
|---|---|---|
| Ian series | Two-car start, all NEXT RACE events, final standings, free-roam and fresh-process save reload. [M5-012](evidence/M5-012-race-results.md) | Retail IDs, course names, number of constituent events, mandatory prerequisites/unlocks. |
| Martin race | Head-to-head, Race_Finish wrapper, 100 REP / $340 reward, owner reported no police. [M6-003](evidence/M6-003-race-system-matrix.md) | Every police path, event identity or healthy lifecycle of the original timed-out process. |
| Five-race resources | Five completion checkpoints in one process and bounded resources. [M5-013](evidence/M5-013-race-resource-growth.md) | Five unique named nodes, progression writes or non-overlap with other rows. |
| Red Light -> Race Back | Earlier camera/pause softlock; three later healthy retries. [KI-026](evidence/KI-026-race-back-camera-softlock.md) | Root cause, reliable reproduction or a verified fix. KI-026 stays open. |
| Delivery entry | Controllable active objective, 300 seconds stable, preserved save. [Delivery regression](evidence/M6-014-delivery-constructor-registry.md) | Delivery completion, reward, identity or every delivery transition. |
| Garage transactions | Representative vehicle/item/exhaust/paint/switch and reload. [M6-002](evidence/M6-002-garage-lifecycle.md) | All vehicles, categories, garages or unlock thresholds. |
| Arcade Sunset and Vine | Ordered Race with RAINY / DAWN, moving gameplay/particles. [M6-004](evidence/M6-004-environment-effects.md) | Race completion or its campaign counterpart. This is not campaign credit. |
| City streaming | Eight project-defined areas plus return, nine stopped GPS confirmations. [M6-001](evidence/M6-001-city-streaming.md) | Official district borders, chapters or South Central story completion. |
| Mixed owner racing | 2026-08-27 README-AI checkpoint: two freeway events, at least two series and a best-of-three pink-slip win; aggregate save delta +15 races / +14 wins. | Named per-event telemetry or disjointness. Categories are owner-attested, not reconstructed from aggregate counters. |
| Offline policy | Local achievement/service boundaries and title-local Driving Test policy. [M6-008](evidence/M6-008-offline-progression.md) | Physical invocation of every export or entitlement/unlock correctness. |
| Proposed Trevor route | Normative [route definition](../config/first-race-route.json) and [M5-001 report](evidence/M5-001-first-race-route.md). | Native event identity, current-save availability or any completion. Remains unverified. |

Old acceptance is useful historical coverage, not proof for an arbitrary new
SDK/build. Preserve its exact artifact/scope. Do not ask the owner to repeat
already accepted observations solely because this inventory was introduced.

## Official source anchors, not a retail dependency graph

- The 19 March 2009 South Central release describes four added neighborhoods
  and a premium pack with nine cars, 26 races, twelve battle maps, ten delivery
  missions and over 100 red-light races, integrated with career progression.
  These are **historical expansion-package counts**, not a verified Complete
  Edition mandatory-node total. Battle maps must not be counted as required
  offline campaign nodes. [Rockstar / Take-Two release](https://ir.take2games.com/static-files/f1fb45e3-b7dc-44ff-8d8d-e54987eb4fee)
- Xbox's Complete Edition description explicitly includes South Central and
  additional vehicles, parts, music and races. It does not enumerate event IDs,
  mandatory nodes or prerequisite edges. [Xbox Wire, 7 June 2018](https://news.xbox.com/en-us/2018/06/07/rockstar-classics-now-available-via-xbox-one-backward-compatibility/)
- Rockstar documents an original-game Goal Attack case where completed
  objectives were not recorded. Therefore test persisted progress separately
  from an on-screen result. This is a **test-design precaution**, not a claim
  that our build has that original issue. [Rockstar Support, updated 6 February 2024](https://support.rockstargames.com/articles/4GiRX6Dq2SskCVH8FGAlcr/not-getting-credit-for-goal-attacks-in-midnight-club-los-angeles)

The [official manuals index](https://www.rockstargames.com/manuals) is a source
lead; an official-host Complete Edition PDF was not retrieved in this review.
A mirrored manual is not treated as a verified official-host catalog. These
sources do not justify guessing chapter order, REP thresholds, opponent chains,
the South Central entry gate or the campaign end condition.

## Completing the actual matrix

Each future retail node needs the following fields, with unknowns explicitly
null rather than inferred from a familiar opponent or geographic label:

- Stable native event/script identifier and displayed name; mode/category,
  region and base-game/expansion provenance.
- Mandatory/optional/unknown classification and its source, prerequisite
  node IDs, REP/vehicle/mission requirements, and unlock/end conditions.
- Seed or restart profile identity, clean-campaign lineage and exact runtime
  artifact. The owner's progressed profile stays separate from clean runs.
- Entry, objective/completion, reward/unlock and persisted reload observations;
  failure/retry/abandon/recovery paths where applicable.
- Evidence references and disposition for each observation: unverified,
  historical, current-artifact pass, failure or explicit unavailable policy.

Only source-bound native identities may deduplicate rows or create dependency
edges. Aggregate races/wins cannot identify which missions were completed.
Do not turn the current eight test zones into eight official campaign chapters.
Do not turn access to a location into proof of story unlock progression.

Next source work: inspect available on-disc/script metadata tooling and verify
its native identities, then reconcile with Mission Log/save observations as
normal play reaches them. No guessed mandatory-node count is a substitute for
that source work. A future complete catalog needs a separately reviewed schema;
this historical seed is intentionally unable to grant full-campaign credit.

The bounded [native-source investigation](evidence/M7-001-campaign-source-feasibility.md)
now resolves catalog/unlock registration wrappers and identifies RPF3 headers.
It does not yet enumerate instances or dependencies, and does not authorize
unlock mutations or broad asset extraction.

## Validation and review

```powershell
scripts/verify-campaign-coverage-seed.ps1
scripts/test-campaign-coverage-seed.ps1
```

The validator enforces explicit gaps, safe existing repository references,
known issue IDs, unique local scenario IDs and false campaign-credit/completion
flags. The known uncalibrated Trevor ID and route provenance cannot be promoted
by changing classification fields. It cannot prove the semantics of free-text
observations; source review is still required.

Independent read-only review checked all eleven entries against their local
references. A combined classification-edit bypass was found and corrected,
with regression negatives for both the existing and a renamed local ID.
The fixture performs no game launch or owner-profile operation.

Gates: one positive / thirteen negatives on PowerShell 7 and Windows
PowerShell 5.1; structural scan clean; all three ast-grep fixtures pass;
`git diff --check` clean. Developer-side revision: 0.13.3.0. The published
0.13.0.0 / SDK v0.14.0.0 Deck candidate was not rebuilt or replaced.
