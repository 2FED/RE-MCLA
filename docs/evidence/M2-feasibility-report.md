# M2 feasibility report

Date: 2026-08-11

Decision: **GO WITH SDK FORK**

Reviewer: Codex technical milestone review

## Executive conclusion

The supported Midnight Club: Los Angeles Complete Edition image is feasible for a ReXGlue static-recompilation port. ReXGlue completes non-force analysis and emission deterministically, every discovered validation finding has a finite tested resolution, the complete import table is symbolic, and no evidence indicates self-modifying code or another execution model that invalidates static recompilation.

M3 may start using MCLA-R ReXGlue fork v0.9.0.1 at `583c8ff35cde3818992fc78d936c635bca092a6b`. The fork is mandatory at this gate because upstream v0.9.0 and the checked upstream `main` reject a valid FLOAT16_4 mask shape diagnostically. The fork fix has direct PPC coverage and does not alter any generated MCLA byte.

## Supported input and baseline

| Property | Verified value |
| --- | --- |
| Title ID | `545407F8` |
| Media ID | `5940C9DB` |
| XEX SHA-256 | `C386F4001FA569E6AD4B982F441F67412F00B3F47C166134555CD4B59854A432` |
| Xenia module hash | `1984A3354B78CE19` |
| Image base / entry | `0x82000000` / `0x821322B8` |
| Stock oracle | Xenia Canary `canary_experimental@7d8db5a2c` |

The stock oracle completed intro, frontend, garage, day/night races, HUD, pause, save creation, clean reload, and the full Xbox-compatible controller surface. Raw screenshots, logs, save data, and game files remain private and ignored.

## Finding disposition

| Class | Initial | Final open | Owner / resolution |
| --- | ---: | ---: | --- |
| S0 safety or private-data defect | 0 | 0 | No finding |
| S1 unresolved direct control flow | 7 | 0 | MCLA-R config: eight evidence-bounded function entries, including one follow-up discovered during iteration |
| S2 FLOAT16_4 diagnostic/coverage defect | 20 | 0 | ReXGlue fork v0.9.0.1 plus PPC regression |
| Unknown codegen finding | 0 | 0 | Complete classification maintained |
| Unsupported PPC instruction | 0 | 0 | No finding in generated corpus |
| Jump-table failure | 0 | 0 | No finding |
| Invalid data region | 0 | 0 | No finding |
| Exception-handler generation failure | 0 | 0 | No finding |
| Oversized output/function warning | 0 | 0 | No finding |

The final non-force run completes Register, Scan, Discover, GapFill, Merge, Validate, and Write with exit code 0 and no diagnostic after the fork fix.

## Determinism and scale

- Generated files: 64
- Generated size: 128,031,984 bytes
- Clean generated-manifest SHA-256: `F8A97618215848CA3B2279725DE77D8AF556E6B2E02AF1CA66D51A812E60F933`
- M2-012 clean runs: byte-identical
- M2-016 fork run: byte-identical to M2-012
- Manual function entries: 8
- Manual switch tables: 0
- Invalid-data regions: 0
- Exception hints: 0
- setjmp/longjmp overrides: 0

The generated corpus is large enough that compile time and linker memory remain an M3 measurement, but its size is finite, deterministic, and within the intended Windows x86-64 toolchain model.

## Import and startup boundary

All 503 XEX import records map to 257 known symbols: 246 callable imports and 11 variables. The generated dispatcher contains every callable thunk. Static generated-code coverage finds 1,517 direct call sites across 240 imports; six callable imports are explicitly indirect-only rather than unknown.

Before guest entry, all 11 variables have pinned SDK mappings. The conservative closure from `xstart` through seven initializer roots to the first title-main call contains 59 internal functions and 26 function imports. Every one has an SDK registration. No project-owned import or variable stub is currently required to reach title main. `XamShowMessageBoxUIEx` remains a known warning/zero-return semantic stub on a conservative error path and must be observed during M3/M4 runtime testing.

## Control flow and exceptions

The image contains 20,071 PDATA records, including 34 exception-marked functions. ReXGlue consumes this metadata without an exception diagnostic. Standard GPR/FPR/VMX helpers are mapped; no standard setjmp/longjmp body is present. The original seven cross-gap targets and one follow-up entry have address-level bounds and rationales. No broad force flag, speculative switch table, or unsupported exception override is retained.

## Public patch compatibility

All eight writes in the public Complete Edition Xenia patch file match the local loaded image at the byte and containing-instruction level. They cover five requested enhancement groups plus one DbgPrint diagnostic group. Every group remains disabled. This proves address compatibility only; skip-intro is available as an off-by-default M3 fallback, while 60 FPS, motion blur, imposter, and MSAA changes remain M8 work.

## Required SDK fork

| Property | Value |
| --- | --- |
| Repository | `https://github.com/2FED/rexglue-sdk.git` |
| Branch | `mcla/float16-mask3` |
| Tag | `v0.9.0.1` |
| Commit | `583c8ff35cde3818992fc78d936c635bca092a6b` |
| Upstream base | v0.9.0 / `3eb9b511b4140d2769e27be63eae57d41bfa2afa` |

Focused `vpkd3d128` tests pass 17 cases / 136 assertions. The complete ReXGlue PPC suite passes 1,459 cases / 5,733 assertions. MCLA FLOAT16_4 warnings fall from 20 to 0 while all 64 generated files remain byte-identical. The project may return to upstream only after an equivalent fix and regression are merged and the normal SDK-upgrade review passes.

## Residual M3 risks

| Risk | Severity at handoff | Owner | M3 action / closure condition |
| --- | --- | --- | --- |
| 128 MB generated corpus has not been compiled or linked | S1 if build fails | M3-001, M3-012, M3-013 | Clean Debug, RelWithDebInfo, and Release builds; measure time and peak memory |
| Native process has not entered guest startup | S1 until demonstrated | M3-002 through M3-014 | Bounded smoke test reaches entry and localizes the first deterministic guest failure |
| Runtime semantics of imports are not proven by static registration | S1/S2 depending reachability | M3-005 through M3-008 | Implement or harden only imports reached by startup traces; reject state-sensitive fake success |
| Exception-marked functions are statically accepted but not exercised natively | S1 if reached and broken | M3-013, M3-014 | Capture deterministic guest PC/function and test the first reachable path |
| `XamShowMessageBoxUIEx` is a semantic stub | S2 on error route | M3-006 / M4 | Log invocation and implement observable behavior if the route is reachable |
| Graphics, audio, input, save, and campaign behavior are native-untested | S1/S2 beyond M3 scope | M4–M7 | Progress through their milestone gates against the Xenia baseline |
| Project depends on a fork | Maintenance risk | SDK owner | Keep exact tag/SHA pin; periodically check upstream; never silently track `main` |

These risks are expected work for the next milestones, not evidence against feasibility. No current risk requires user-only input before M3-001.

## Stop-condition evaluation

| M2 stop condition | Result |
| --- | --- |
| Analysis is nondeterministic | False — clean manifests are identical |
| Essential control flow remains unreconstructed | False — all discovered cross-gap targets are bounded and non-force codegen passes |
| Startup broadly depends on unsupported exception/MMIO behavior | Not observed — static startup set is fully registered; runtime proof moves to M3 |
| Generated size/build requirements are already impractical | Not observed — 128 MB output is finite; native compile measurement moves to M3 |

## Effort and gate recommendation

M3 retains the planned estimate of 1–4 weeks. Its first two tasks can begin immediately: integrate the ignored generated source graph, then implement the project `ReXApp` lifecycle. The estimate should be re-evaluated after the first complete compiler/linker attempt and again after the first native startup trace.

Recommendation: approve `GO WITH SDK FORK`, close M2 after the milestone-wide review/tests/version/documentation sequence, and start M3 without waiting for additional manual game interaction.
