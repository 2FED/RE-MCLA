# Reverse-engineering notes

This document records address-level conclusions that affect ReXGlue configuration. Raw binaries, disassembly windows, generated guest code, and full analysis logs remain private under the source-data policy. Every manual override must cite a tracked evidence report and pass a non-force codegen rerun before it is accepted.

## M2-011 helper and control-flow audit

The supported Complete Edition image contains a valid `.pdata` directory with 20,071 runtime-function records, including 34 records with exception metadata. ReXGlue consumed the directory without exception or discontinuity diagnostics. The exact exception-function register and private-audit identity are published in `docs/evidence/M2-011-control-flow-metadata.md`.

Detected save/restore helper bases:

| Family | Save base | Restore base |
| --- | --- | --- |
| GPR 14-31 | `0x823D91C0` | `0x823D9210` |
| FPR 14-31 | `0x823DB9A0` | `0x823DB9EC` |
| VMX 14-31 | `0x823DD2C0` | `0x823DD558` |
| VMX 64-127 | `0x823DD354` | `0x823DD5EC` |

No standard Xenon setjmp/longjmp FPR save/restore body was found. The sole direct `RtlUnwind` caller is an unwind wrapper without jump-buffer restoration, so `setjmp_address` and `longjmp_address` remain unset.

The seven M2-008 `UnresolvedCall` targets are executable entries in PDATA gaps. M2-012 owns the incremental configuration experiment. `0x824B0DE8` is the only current function-chunk candidate (parent PDATA function begins at `0x824B0CC0`); `0x8220C018`, `0x8220BF08`, and `0x822C98B8` are computed-dispatch entries; the other three are bounded leaf/tail helpers. No switch table, invalid-data range, or exception hint is justified yet.

## M2-012 accepted manual configuration

The incremental non-force sequence reduced the original seven blockers to zero. Bounding `0x822C98B8` at its first `bctr` exposed one adjacent entry, `0x822C9948`; a separate private Ghidra audit bounded that follow-up through `0x822C9A2C`. This is retained as evidence that exact function ends are preferable to swallowing an entire PDATA gap.

| Address | Exclusive end | Parent | Reason |
| --- | --- | --- | --- |
| `0x8220BF08` | `0x8220C018` | none | computed-dispatch entry |
| `0x8220C018` | `0x8220C0D0` | none | second shared-gap dispatch entry |
| `0x822B88C8` | `0x822B88DC` | none | tail-branch thunk |
| `0x822C98B8` | `0x822C9948` | none | first dispatch entry |
| `0x822C9948` | `0x822C9A2C` | none | discovered follow-up dispatch entry |
| `0x823F32E8` | `0x823F3300` | none | leaf helper |
| `0x823FD718` | `0x823FD720` | none | tail-branch thunk |
| `0x824B0DE8` | `0x824B0DF8` | `0x824B0CC0` | verified function chunk at parent PDATA end |

`config/mcla_functions.toml` is the only analysis include. Manual switch-table, invalid-region, exception-handler, and setjmp/longjmp entries remain absent because M2-009/M2-011 found no corresponding failure. Two clean successful runs produced the same private generated manifest; see `docs/evidence/M2-012-manual-analysis-config.md`.

## Import map handoff

M2-013 maps all 503 XEX import records to 257 known exports: 95 `xam.xex` functions and 162 `xboxkrnl.exe` symbols (151 functions, 11 variables). ReXGlue's accepted generated dispatcher contains all 246 callable thunk addresses. Static generated-code scanning finds 1,517 direct call sites across 240 functions; `__C_specific_handler`, `StfsControlDevice`, `StfsCreateDevice`, `IoInvalidDeviceRequest`, `NtQueryDirectoryFile`, and `NtReadFileScatter` are retained as indirect-only rather than mislabeled unreachable. Variable imports have no callable thunk by design. M2-014 must narrow this complete static inventory to entry-point startup reachability; see `docs/evidence/M2-013-import-coverage.md`.

M2-014 defines two startup layers. Before `xstart` (`0x821322B8`), the runtime must map all 11 imported variables; the pinned SDK does. From `xstart` through seven initializer roots to the first title-main call (`sub_821305E8`), the conservative static closure contains 59 internal functions and 26 imports, all registered by the SDK. `DbgPrint` is after the title-main return and is not part of this minimum. No new project stub is required before title main; `XamShowMessageBoxUIEx` remains an explicit warning/zero-return semantic stub on the retained error-dialog branch. See `docs/evidence/M2-014-startup-import-set.md`.

## Public Xenia patch compatibility

M2-015 pins the Complete Edition upstream patch file at `xenia-canary/game-patches` commit `84d6682caf1b75b2fdb7adcd197c6559c09b2ed4`. Its module hash and Media ID exactly match the local baseline. All eight public writes are byte-compatible with the loaded image: seven cover 60 FPS/delta-time, skip intro, motion blur, imposter shadows, and MSAA; one redirects DbgPrint. Partial `be8`/`be16` writes were checked in their full containing instructions. No patch is enabled or copied into runtime configuration; enhancement behavior belongs to M8. See `docs/evidence/M2-015-xenia-patch-audit.md`.

## ReXGlue FLOAT16_4 validation

The twenty M2-010 `vpkd3d128` findings are valid format-5 instructions with mask 3 and shift 0. Project-fork ReXGlue v0.9.0.1 accepts mask 3 alongside mask 2 while preserving the existing shift bound. The added PPC case produces the same four half-float outputs for masks 2 and 3, the full PPC suite passes, and a clean MCLA codegen run removes all twenty warnings without changing any generated byte. No MCLA-side opcode hook or patch is required. See `docs/evidence/M2-016-rexglue-vector-regression.md`.
