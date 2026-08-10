# M1 closure review

Date: 2026-08-11
Decision: GO M2
Release version: 0.1.0.0

## Scope review

All 15 M1 tasks and all 15 corresponding acceptance-register entries are complete. The resulting environment has deterministic compiler/build discovery, a recursively pinned and built ReXGlue SDK, verified read-only source ingestion, an exact private extraction manifest, pinned emulator/reverse-engineering/GPU-debug tools, and a one-command aggregate gate.

No M1 criterion was weakened during closure. ReXGlue code generation and target-game feasibility remain explicitly owned by M2.

## Final verification

| Gate | Result |
| --- | --- |
| M1 task ledger | 15 complete, 0 open |
| M1 acceptance register | 15 entries |
| Fresh-shell `scripts/bootstrap.ps1` | 12 passed, 0 failed |
| Bootstrap missing-tool test | 11 passed, 1 intentional failure, exit code 1 |
| `ast-grep scan` | clean |
| `ast-grep test` | 3 passed, 0 failed |
| Supported ISO/XEX verification | pass; source hash unchanged |
| Extracted payload | 15 files, 6,569,586,392 bytes, all hashes pass |
| ReXGlue recursive state | v0.9.0, commit `3eb9b511b414`, 26 initialized status entries |
| `rexglue init` sample scaffold | pass |
| Minimal installed-SDK consumer | configure/build/link/run pass; prints `0.9.0` |
| Prohibited tracked paths | 0 |
| `git diff --check` | clean |
| `git fsck --full` | no integrity errors; one harmless unreachable tree from a local amended commit |

The initialized sample lives only in ignored `private/rexglue-smoke/` and points at the supported local XEX/game root. As designed by ReXGlue, its generated application header does not exist until codegen. Closure therefore also built a separate ignored minimal C++23 consumer that used `find_package(rexglue 0.9.0 EXACT)`, linked `rex::runtime`, and executed successfully. This proves the installed SDK/compiler integration without running M2 codegen early or fabricating generated game code.

## Gate assessment

M1 exit criteria are satisfied:

- the supported dump is identified before codegen
- extraction and all manual path assumptions are documented and validated
- the ReXGlue CLI initializes a project
- the installed SDK and compiler build and run a minimal consumer
- tool versions, paths, hashes, and upgrade triggers are recorded
- aggregate validation is reproducible from a fresh PowerShell process

Decision: `GO M2`.

## Residual risks entering M2

- No claim is made that ReXGlue can yet analyze or generate valid code for the complete MCLA XEX.
- The initialized target scaffold cannot build until codegen creates its generated headers and sources; this is the expected M2 dependency.
- No real Xenia gameplay baseline, GPU capture, import-coverage result, or codegen determinism result exists yet.
- Actual codegen volume, unsupported PPC behavior, exception/control-flow reconstruction, and SDK-fork needs remain unknown.
- Private artifacts and generated proprietary output must remain ignored throughout feasibility work.

The first M2 work is the stock Xenia baseline (M2-001 through M2-004), followed by the controlled non-force/force codegen audit and feasibility report.
