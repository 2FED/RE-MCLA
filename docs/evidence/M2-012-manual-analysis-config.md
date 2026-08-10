# M2-012 manual analysis configuration

Date: 2026-08-11
Result: NON-FORCE CODEGEN PASSES WITH DETERMINISTIC OUTPUT

## Final input identity

- Manifest SHA-256: `3ED7976DCC75085BB235CBA1406F1110C6DF78B7FA6525AD03FBF62D44B3AE90`
- Reviewed function config SHA-256: `FEB14690C0795A6748AD76E751B98D58435A5373847E8BE32D5A54CB6CE53FFF`
- Clean generated-manifest SHA-256: `F8A97618215848CA3B2279725DE77D8AF556E6B2E02AF1CA66D51A812E60F933`
- ReXGlue: pinned v0.9.0; all nine optimization/exception-generation flags remain false

## Incremental evidence

| Iteration | Function entries | Exit | Unresolved | Outcome | Private stderr SHA prefix |
| --- | ---: | ---: | ---: | --- | --- |
| `01-cf02-chunk` | 1 | 1 | 6 | CF-02 chunk accepted | `165C0EB7FF4C...` |
| `02-cf01-tail` | 2 | 1 | 5 | CF-01 accepted | `BADB3671FC6E...` |
| `03-cf03-dispatch` | 3 | 1 | 4 | CF-03 accepted | `5EA1E3FA4466...` |
| `04-cf04-dispatch` | 4 | 1 | 3 | CF-04 accepted | `7C83A4DDB23E...` |
| `05-cf05-dispatch` | 5 | 1 | 3 | CF-05 accepted; exposed 0x822C9948 | `AE7FB9BAB27A...` |
| `06-cf05-followup` | 6 | 1 | 2 | follow-up accepted | `C86312D46968...` |
| `07-cf06-leaf` | 7 | 1 | 1 | CF-06 accepted | `C1ACCDA1C5F6...` |
| `08-cf07-tail-final` | 8 | 0 | 0 | first success | `E4EDEE04351A...` |
| `09-final-clean-a` | 8 | 0 | 0 | clean determinism A | `81C3D18309BB...` |
| `10-final-clean-b` | 8 | 0 | 0 | clean determinism B | `A90277AA4D1C...` |

Iteration 05 is intentionally retained as a failed prediction: adding `0x822C98B8` removed its original blocker but exposed the adjacent entry `0x822C9948`. A separate private Ghidra window proved that entry reaches `bctr` at `0x822C9A28`; iteration 06 added the bounded follow-up and restored the monotonic path to zero.

## Accepted manual functions

| Address | Exclusive end | Relation | Address rationale |
| --- | --- | --- | --- |
| `0x8220BF08` | `0x8220C018` | standalone | computed-dispatch entry; terminates at `bctr` |
| `0x8220C018` | `0x8220C0D0` | standalone | second entry in the shared dispatch gap |
| `0x822B88C8` | `0x822B88DC` | standalone | bounded tail-branch thunk |
| `0x822C98B8` | `0x822C9948` | standalone | first dispatch entry; exact end exposed the next entry |
| `0x822C9948` | `0x822C9A2C` | standalone | follow-up dispatch entry verified privately to `bctr` |
| `0x823F32E8` | `0x823F3300` | standalone | bounded leaf ending in `blr` |
| `0x823FD718` | `0x823FD720` | standalone | two-instruction tail thunk |
| `0x824B0DE8` | `0x824B0DF8` | chunk of `0x824B0CC0` | begins exactly at parent PDATA end; chunk hypothesis passed |

## Explicit zero override classes

- Manual switch tables: 0 - M2-009 reported no missing jump table and the three `bctr` bodies are callable dispatch helpers, not unresolved switch sites.
- Invalid-instruction/data regions: 0 - M2-009 reported no invalid-region or unimplemented-instruction finding.
- Exception-handler hints: 0 - all 34 exception-marked PDATA records parsed without diagnostics.
- setjmp/longjmp overrides: 0 - M2-011 found no matching jump-buffer implementation.

## Determinism and gate result

Two runs started with no `generated/default` directory, used the exact same final manifest/config, exited 0, emitted the same 64 files / 128,031,984 bytes, and produced byte-identical generated manifests. Both completed Register, Scan, Discover, GapFill, Merge, Validate, and Write. The only diagnostics were the same 20 classified FLOAT16_4 warnings owned by M2-016; there were zero analysis errors and zero uncatalogued findings.

M2-012 acceptance: PASS. The generated output remains ignored and private.
