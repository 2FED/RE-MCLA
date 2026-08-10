# M2-010 codegen finding triage

Date: 2026-08-11
Result: ALL 27 UNIQUE FINDINGS CLASSIFIED

## Evidence identity

- M2-009 force stderr SHA-256: `3679E164642BE88B29E549FBE16BEC2A95658F6408994FFEBF88610FF8207CFD`
- Private Ghidra address-audit SHA-256: `0729C5AB173AF9A85514C995434EFF6AC072A8D771F3193EE2B5901559E31CFB`
- Audit scope: 34 unique guest addresses (7 source/target pairs plus 20 vector-pack sites)
- XEX identity is inherited from the verified M2-009 run and remains private.

## Classification summary

| Category | Findings | Severity | Owner |
| --- | ---: | --- | --- |
| Config fix | 7 | S1 | MCLA-R analysis/config |
| ReXGlue defect | 20 | S2 | Upstream ReXGlue; local SDK fork if required |
| Project hook/stub | 0 | N/A | N/A |
| False positive | 0 | N/A | N/A |
| Unknown | 0 | N/A | N/A |

The seven control-flow findings are S1 because normal codegen fails and force-generated call sites terminate with a fatal diagnostic if executed. The 20 pack findings are S2: generation completes, but unverified vector packing can produce a major rendering/data defect.

## Per-finding register

| ID | Guest site | Category | Owner | Severity | Next action |
| --- | --- | --- | --- | --- | --- |
| CF-01 | `0x82203F90` -> `0x822B88C8` | Config fix | MCLA-R analysis/config | S1 | Verify unwind/PData and helper boundaries in M2-011; add the target as a justified manual function in M2-012, then rerun non-force codegen. |
| CF-02 | `0x824AF4D0` -> `0x824B0DE8` | Config fix | MCLA-R analysis/config | S1 | Verify unwind/PData and helper boundaries in M2-011; add the target as a justified manual function in M2-012, then rerun non-force codegen. |
| CF-03 | `0x8220DA7C` -> `0x8220C018` | Config fix | MCLA-R analysis/config | S1 | Verify unwind/PData and helper boundaries in M2-011; add the target as a justified manual function in M2-012, then rerun non-force codegen. |
| CF-04 | `0x8220DAF4` -> `0x8220BF08` | Config fix | MCLA-R analysis/config | S1 | Verify unwind/PData and helper boundaries in M2-011; add the target as a justified manual function in M2-012, then rerun non-force codegen. |
| CF-05 | `0x822C9E04` -> `0x822C98B8` | Config fix | MCLA-R analysis/config | S1 | Verify unwind/PData and helper boundaries in M2-011; add the target as a justified manual function in M2-012, then rerun non-force codegen. |
| CF-06 | `0x823FB7F4` -> `0x823F32E8` | Config fix | MCLA-R analysis/config | S1 | Verify unwind/PData and helper boundaries in M2-011; add the target as a justified manual function in M2-012, then rerun non-force codegen. |
| CF-07 | `0x823FDB24` -> `0x823FD718` | Config fix | MCLA-R analysis/config | S1 | Verify unwind/PData and helper boundaries in M2-011; add the target as a justified manual function in M2-012, then rerun non-force codegen. |
| VP-01 | `0x8243C67C` / `1BD7EE15` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-02 | `0x8243C6F4` / `1BB7F615` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-03 | `0x8243C7FC` / `1B77E615` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-04 | `0x8243C808` / `1B57F615` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-05 | `0x8243C80C` / `1B17CE15` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-06 | `0x8243C810` / `1AD7BE15` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-07 | `0x8243C814` / `1A97AE15` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-08 | `0x8243C818` / `1A379615` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-09 | `0x8243C828` / `19D77E15` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-10 | `0x8243C894` / `1BD7EE15` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-11 | `0x8243CD48` / `1B77EE15` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-12 | `0x8243D010` / `1BB7DE15` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-13 | `0x8243D040` / `1BF79E15` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-14 | `0x8243D050` / `1B975E15` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-15 | `0x8243D064` / `1B775614` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-16 | `0x8243D07C` / `1BB71E15` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-17 | `0x8243D08C` / `1B37F615` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-18 | `0x8243D0B8` / `1BF77615` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-19 | `0x8243D0CC` / `1BD72614` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |
| VP-20 | `0x8243D158` / `1B77EE15` | ReXGlue defect | Upstream ReXGlue; MCLA-R SDK fork if needed | S2 | Add mask=3/shift=0 FLOAT16_4 regression coverage in M2-016; align validation/emission with Xenon semantics before native graphics acceptance. |

## Rationale

### Seven unresolved direct branches

Every source instruction is an unconditional direct branch to mapped executable `.text`, and every target contains an instruction plus an incoming reference. Six targets are immediately preceded by `blr`; in the remaining pair the source branch is immediately preceded by `blr` and the target has another local incoming branch. This is consistent with omitted function/chunk boundaries rather than an import. M2-011 must distinguish save/restore helpers, function chunks, and exception metadata before M2-012 adds manual boundaries.

### Twenty FLOAT16_4 pack warnings

All twenty instruction words decode as `vpkd3d128` format 5 (`FLOAT16_4`), mask 3, shift 0. ReXGlue v0.9.0 warns whenever FLOAT16_4 mask is not 2, while its writer still emits conversion code. Xenia's independent Xenon implementation accepts masks 1 through 3 and treats mask 3 like mask 2 except for the shift-3 special case. Therefore mask 3/shift 0 is a valid operand shape and the ReXGlue diagnostic/coverage gap is an SDK defect, although emitted-value parity still needs M2-016 regression tests.

- ReXGlue source: `third_party/rexglue-sdk/src/codegen/builders/vector.cpp` (`build_vpkd3d128`)
- Independent reference: [Xenia `ppc_emit_altivec.cc` at audited revision](https://github.com/xenia-project/xenia/blob/95a5c3ee250f80c3b9d139658649d9ffb6db3eec/src/xenia/cpu/ppc/ppc_emit_altivec.cc)
- Reference tests: [Xenia `instr_vpkd3d128.s` at audited revision](https://github.com/xenia-project/xenia/blob/95a5c3ee250f80c3b9d139658649d9ffb6db3eec/src/xenia/cpu/ppc/testing/instr_vpkd3d128.s)

## Decision

- No finding remains uncatalogued or unknown.
- No project hook/stub is justified by this codegen evidence.
- Proceed to M2-011 before changing the function list.
- Preserve the 20 pack sites as the M2-016 SDK regression corpus.
