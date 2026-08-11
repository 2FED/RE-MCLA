# M2 closure review

Date: 2026-08-11

Decision: **GO WITH SDK FORK M3**

Release version: `0.2.0.0`

## Scope review

All 17 M2 tasks and all 17 acceptance-register entries are complete. The exact Complete Edition image has a stock behavioral oracle, deterministic non-force generated output, fully classified and closed analysis findings, a complete symbolic import map, a bounded pre-main dependency set, byte-verified public patch compatibility, and a publicly fetchable tested SDK fork.

No criterion was weakened. Native compilation, linking, process startup, and runtime-semantic import behavior remain explicitly owned by M3 rather than being claimed from static evidence.

## Final verification

| Gate | Result |
| --- | --- |
| M2 task ledger | 17 complete, 0 open |
| M2 acceptance register | 17 entries |
| M2 task commits | 17 unique IDs through `f0be783` |
| Sanitized M2 evidence | 17 task reports plus this closure review |
| Project PowerShell tests | 10/10 passed |
| ReXGlue focused PPC tests | 17 cases, 136 assertions passed |
| ReXGlue complete PPC suite | 1,459 cases, 5,733 assertions passed |
| `ast-grep scan` | clean |
| `ast-grep test --skip-snapshot-tests` | 3 passed, 0 failed |
| Fresh bootstrap | 12 passed, 0 failed |
| Final non-force codegen | exit 0, zero diagnostics |
| Generated output | 64 files, 128,031,984 bytes, byte-identical manifest |
| Generated-manifest SHA-256 | `F8A97618215848CA3B2279725DE77D8AF556E6B2E02AF1CA66D51A812E60F933` |
| Import coverage | 503/503 records symbolic; 246 function thunks and 11 variables |
| Startup dependency coverage | 11/11 variables and 26/26 pre-main imports registered |
| ReXGlue fork | v0.9.0.1 / `583c8ff35cde3818992fc78d936c635bca092a6b`; 25 nested nodes clean |
| Fork publication | branch `mcla/float16-mask3` and tag `v0.9.0.1` resolve to the pinned SHA |
| Prohibited tracked paths | 0 |
| `git diff --check origin/main..HEAD` | clean |
| `git fsck --full` | no integrity errors; three dangling trees and two dangling blobs from local rewritten work |

The source ISO/XEX identity and all 15 extracted payload hashes were revalidated by the final bootstrap. Raw game content, generated guest sources, Xenia captures/logs, and saves remain ignored.

## Exit-criteria assessment

- ReXGlue analysis and emission complete without crashing.
- All 7 original S1 control-flow findings are closed by 8 evidence-bounded function entries.
- All 20 original S2 vector-pack findings are closed by the tested ReXGlue fork.
- No unknown, unsupported-instruction, jump-table, invalid-data, exception-generation, or oversized-output finding remains.
- Clean output is deterministic across the two M2-012 runs and the M2-016 fork run.
- No evidence suggests pervasive self-modifying code or an unsupported execution model.
- The feasibility report assigns every material M3 risk an owner and closure condition.

Decision: `GO WITH SDK FORK M3`.

## Residual risks entering M3

- The 128 MB generated corpus has not yet been compiled or linked; M3 must measure build time and peak memory.
- No native executable has entered guest startup code; the first failure must be bounded and diagnosed rather than bypassed.
- Static import registration does not prove runtime semantics. M3 must implement only trace-reachable gaps and must not silently fake state-sensitive success.
- Exception-marked paths, graphics, audio, input, save, and campaign behavior remain native-untested.
- The exact SDK fork/tag must remain pinned until an equivalent upstream fix passes the project upgrade gate.

No residual risk requires manual user action before M3-001. The next work is generated-source CMake integration followed by the project `ReXApp` lifecycle.
