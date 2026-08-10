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
