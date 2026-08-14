# M4 closure review

Date: 2026-08-14

Decision: **GO M5 WITH PINNED SAVE**

Release version: `0.4.0.0`

## Scope review

All 13 M4 tasks and all 13 acceptance-register entries are complete. The
supported Complete Edition image now presents a recognizable title frontend,
uses the scoped D3D12 host-RTV path, exposes a stable local profile, accepts one
physical SDL controller at slot 0, sustains frontend audio, remains offline,
handles the tested locales and Unicode paths, and follows a deterministic
pinned post-OOBE save into free roam, pause, and Settings/Options.

No criterion was silently widened. The gate is satisfied by that pinned saved
route, not by a clean-new-game path. M4 does not claim first-run OOBE, a
completed race, general save persistence, multiple physical controllers,
title-driven force feedback, Bink playback, whole-frame parity, every Xenos
backend/gamma/resolve path, or public-release playability. Those boundaries are
recorded in the M4-013 deviation register and owned by M5 and later work.

MCLA has no top-level in-game Exit action. The accepted console-title lifecycle
uses external exact-window `WM_CLOSE`, followed by the bounded execution and
hard-exit records, exit code 0, and no surviving canonical process.

## Final verification

| Gate | Result |
| --- | --- |
| M4 task ledger | 13 complete, 0 open |
| M4 acceptance register | 13 entries |
| M4 task commits | 13 distinct task commits from `aa73ff9` through `807fc9e` |
| Sanitized M4 evidence | 13 task reports plus this closure review |
| Project PowerShell tests | 42/42 passed |
| Closure regression transcript | private SHA-256 `11783B632AAFE5523CB3467B622D968330A0F71FE765772EE4E8D79C356C267E` |
| `ast-grep scan` | clean |
| `ast-grep test --skip-snapshot-tests` | 3 passed, 0 failed |
| Fresh bootstrap | 12 passed, 0 failed |
| ReXGlue fork | v0.9.0.18 / `923c92d1d1cb721cb704ac603fba263a01ba06aa` |
| Fork publication | branch `mcla/mcla-r-hotfixes` and annotated tag `v0.9.0.18` both resolve remotely to the pinned SHA |
| First guest output | 20/20 guest-backed present/readback cycles; owner visual PASS |
| Scoped render path | 10/10 title routes with balanced D3D12 audit and stable title ROIs |
| Intro disposition | 3/3 unpatched title routes with zero post-launch Bink access; playback unclaimed |
| Profile | 3/3 title routes with slot 0 local and distinct absent slots 1-3 |
| Controller | one selected slot-0 pad; split causal digital, analog, focus, and hotplug evidence; multi-pad physical behavior unclaimed |
| Frontend audio | 300-second nonzero XMA/XAudio/SDL route with zero starvation/failure/drop counters |
| Saved frontend route | 20/20 isolated title-to-free-roam, pause, and Settings/Options cycles |
| Process cleanup | exit 0, zero force cleanup, and zero exact-path orphan in all accepted closure cycles |
| Source-data integrity | 15 files, 6,569,586,392 bytes, exact hashes unchanged |
| Save/runtime integrity | pinned two-file seed and four named runtime artifacts unchanged |
| Accepted executable SHA-256 | `9E96FF569E0C9B30CF65CB737EA79BC7741EE978F173E8E3CE7000A74151B135` |
| Prohibited tracked game/generated/private paths | 0; `generated/rexglue.cmake` remains the sole allowed generated bootstrap file |
| Public evidence privacy | bounded hashes, counts, run IDs, and classified metadata only |
| Git integrity | no object-integrity error; harmless dangling local objects only |

The canonical M4-013 closure run is
`20260814-023505-e69d76a2`. Its sanitized `result.json` has SHA-256
`2CF4BE3210C260181F7C2CF9C00D07667ED12D6CA644B4E06ECBF12D7A9B0A28`.
The physical verifier reconstructs and re-hashes all 20 log sets, 80 captures,
per-cycle user/cache trees, the pinned save, the source-game tree, and four
runtime artifacts. Every cycle reaches active saved free roam, pause, and
Settings/Options before external close. Pause correlation is
0.926480-0.932330 and Options correlation is 0.984124-0.999391.

The final aggregate was recovered after all 20 physical routes had completed,
because two valid shutdown-flush tails exposed verifier-only ordering
assumptions. Recovery does not invent timings or artifacts: it binds the exact
pre-result topology, records that elapsed stopwatch values are unavailable,
reconstructs the aggregate only from immutable physical files, and verifies it
twice before acceptance.

The closure matrix also found three stale test contracts and fixed them before
release. The first-analysis policy now binds the current SDK gitlink, the
analysis-config verifier includes the runtime-discovered standalone function
`[0x82554080,0x8255409C)`, and historical M4-002 evidence explicitly retains
30,025 mappings while all current probes require 30,026. The complete 42-test
matrix was restarted from zero after those corrections and passed.

## Exit-criteria assessment

- The supported image reaches the verified title and pinned saved frontend
  route without debugger intervention.
- The reached local user/profile flow does not softlock.
- Guest-backed presentation, scoped render paths, one-pad input, and sustained
  basic frontend audio are operational under bounded physical gates.
- Twenty consecutive saved routes reach free roam, pause, and Settings/Options
  and then close externally without force cleanup, orphan, or data drift.
- All remaining M4 deviations have severity, workaround, and an owning later
  milestone.

Decision: `GO M5 WITH PINNED SAVE`.

## Residual risks entering M5

- The pinned save bypasses first-run OOBE, long opening cutscenes, and starter
  vehicle selection. Those paths remain unverified.
- No race has been selected, completed, or returned from natively; this is the
  primary M5 vertical-slice objective.
- General save/profile persistence is unverified. Each accepted M4 route starts
  from a fresh copy of the pinned seed.
- Broader world rendering, ROV/interlock behavior, PWL gamma, true-direct
  resolves, whole-frame parity, and the minor green vehicle-shadow tint remain
  gameplay/rendering work.
- Controller evidence covers one physical pad. Multi-pad physical policy,
  title-driven force feedback, and full gameplay control behavior remain open.
- Bink decode/playback, event-level audio identity, and user-selected XMP music
  remain outside the M4 claim.
- A saved frontend route is not a playable-release claim; packaging and a
  clean-user launcher remain M9 work.

No residual M4 issue blocks beginning M5-001 with the pinned post-OOBE save.
M5 must first define the exact first-race route and preserve these scope
boundaries until physical gameplay evidence replaces them.
