# M2-011 control-flow and exception metadata audit

Date: 2026-08-11
Result: HELPERS, PDATA, JUMP CONTEXT, AND GAP ENTRIES IDENTIFIED

## Evidence identity

- Private Ghidra audit SHA-256: `FB13A297983F3A756B0314ABFB1A7E9162E672555D6F6D829155D32F145D0BA2`
- Private generated-manifest SHA-256: `F0A16B36EECD0EAAF49B2272B334C39FB487E58DDA08276E6A6D85CBAB5B5BDC`
- Ghidra: 12.0.4 with XEXLoaderWV 13.0.0, PowerPC big-endian Xenon language
- ReXGlue: pinned v0.9.0 M2-009 force snapshot

## Save/restore helpers

| Family base | Save | Restore | ReXGlue expansion |
| --- | --- | --- | --- |
| GPR 14 | `0x823D91C0` | `0x823D9210` | registers 14-31, stride 4 |
| FPR 14 | `0x823DB9A0` | `0x823DB9EC` | registers 14-31, stride 4 |
| VMX 14 | `0x823DD2C0` | `0x823DD558` | registers 14-31, stride 8 |
| VMX 64 | `0x823DD354` | `0x823DD5EC` | registers 64-127, stride 8 |

All eight base signatures are unique in executable memory and agree with the helper registrations in the immutable generated dispatcher.

## setjmp / longjmp decision

- No standard 18-register `stfd f14..f31` setjmp body targeting `r3` exists.
- No standard 18-register `lfd f14..f31` longjmp body targeting `r3` or copied buffer register `r7` exists.
- The image imports `RtlUnwind`; its only direct generated caller is `sub_8274ABB0`, a small unwind wrapper that does not contain jump-buffer FPR restore semantics.
- Therefore `setjmp_address` and `longjmp_address` remain intentionally unset. Revisit only if runtime evidence exposes a nonstandard indirect implementation.

## Seven unresolved cross-gap branches

| ID | Source / generated owner | Target | Source PDATA gap | Target PDATA gap | First terminal | Classification |
| --- | --- | --- | --- | --- | --- | --- |
| CF-01 | `0x82203F90` / `sub_82203F90` | `0x822B88C8` | `82203F7C-82203F98` | `822B88A4-822B88E0` | 822B88D8 b | tail-branch thunk |
| CF-02 | `0x824AF4D0` / `sub_824AF4D0` | `0x824B0DE8` | `824AF4CC-824AF4D8` | `824B0DE8-824B0DF8` | 824B0DF0 b | function-chunk candidate |
| CF-03 | `0x8220DA7C` / `sub_8220DA70` | `0x8220C018` | `8220DA30-8220DB60` | `8220BDEC-8220C0D0` | 8220C0CC bctr | computed-dispatch entry |
| CF-04 | `0x8220DAF4` / `sub_8220DAE8` | `0x8220BF08` | `8220DA30-8220DB60` | `8220BDEC-8220C0D0` | 8220C014 bctr | computed-dispatch entry |
| CF-05 | `0x822C9E04` / `sub_822C9DF8` | `0x822C98B8` | `822C9D6C-822C9E28` | `822C97B0-822C9B88` | 822C9944 bctr | computed-dispatch entry |
| CF-06 | `0x823FB7F4` / `sub_823FB7F0` | `0x823F32E8` | `823FB7E0-823FB848` | `823F32DC-823F3300` | 823F32FC blr | leaf helper |
| CF-07 | `0x823FDB24` / `sub_823FDB20` | `0x823FD718` | `823FDABC-823FDB30` | `823FD6FC-823FD720` | 823FD71C b | tail-branch thunk |

All fourteen source/target sites are executable but outside every PDATA range. ReXGlue recovered each source during gap fill but did not register the target as a call target. CF-02 is the sole parent-chunk candidate: `0x824B0DE8` begins exactly at the end of PDATA function `0x824B0CC0-0x824B0DE8` and also has a local incoming branch. CF-03 and CF-04 are separate entries in one shared dispatch gap. The remaining targets have standalone terminal behavior. M2-012 must add the minimum explicit entries and verify the classification by a non-force rerun.

The M2-009 analyzer reported zero `DiscontinuousFunction` diagnostics. That zero does not erase the cross-gap evidence above; it means there is no additional analyzer-reported discontinuity outside this finite seven-pair set.

## Exception directory

- `.pdata`: `0x82102A00-0x82129D38` (160,568 bytes)
- Runtime-function records: 20,071 (8 bytes each)
- Records with `ExceptionFlag`: 34

| Function begin | Exclusive end | Prolog instructions | 32-bit flag |
| --- | --- | ---: | --- |
| `0x821322B8` | `0x8213247C` | 4 | true |
| `0x821342B8` | `0x8213484C` | 4 | true |
| `0x82134860` | `0x821350D4` | 4 | true |
| `0x82135150` | `0x821353C8` | 4 | true |
| `0x82135440` | `0x82135C90` | 4 | true |
| `0x821C91C8` | `0x821C927C` | 4 | true |
| `0x823D9210` | `0x823D9264` | 21 | false |
| `0x823D9750` | `0x823D9834` | 11 | true |
| `0x823D9990` | `0x823D9A74` | 4 | true |
| `0x823DAF40` | `0x823DB040` | 4 | true |
| `0x823DB9EC` | `0x823DBA38` | 19 | false |
| `0x823DD558` | `0x823DD7F0` | 166 | false |
| `0x823DD898` | `0x823DDA18` | 4 | true |
| `0x823DE738` | `0x823DE7F4` | 5 | true |
| `0x823DEDD8` | `0x823DEF40` | 4 | true |
| `0x823DF358` | `0x823DF400` | 6 | true |
| `0x823E0380` | `0x823E0524` | 4 | true |
| `0x823E0A78` | `0x823E0AEC` | 6 | true |
| `0x823E0B18` | `0x823E0B74` | 6 | true |
| `0x823E28C8` | `0x823E29BC` | 4 | true |
| `0x823E2A70` | `0x823E2ABC` | 5 | true |
| `0x823E2B30` | `0x823E2BBC` | 5 | true |
| `0x823E6088` | `0x823E6148` | 4 | true |
| `0x823E6288` | `0x823E63AC` | 4 | true |
| `0x823E6740` | `0x823E6878` | 4 | true |
| `0x823E6B28` | `0x823E6C5C` | 4 | true |
| `0x823E90A8` | `0x823E9168` | 6 | true |
| `0x823E91C8` | `0x823E92F0` | 4 | true |
| `0x823E94B0` | `0x823E9570` | 4 | true |
| `0x823E9C10` | `0x823E9D74` | 4 | true |
| `0x823E9EB0` | `0x823E9FC8` | 4 | true |
| `0x82460F08` | `0x82460F64` | 4 | true |
| `0x8274B0F8` | `0x8274B398` | 4 | true |
| `0x827A7460` | `0x827A7658` | 10 | true |

ReXGlue consumed this exception directory without an exception-parser diagnostic. Exception wrapper generation remains disabled by the conservative M2-007 policy; M2-012 must not add exception-handler hints unless a rerun or runtime route demonstrates a missing handler.

## M2-012 handoff

- Add the seven target entries incrementally, not as a bulk speculative range.
- Test CF-02 first as a chunk of `0x824B0CC0`; fall back to a standalone function only if the analyzer rejects or misroutes the parent relation.
- Treat CF-03/CF-04 as explicit multi-entry dispatch functions in the shared PDATA gap.
- Do not add switch tables, invalid regions, exception hints, or setjmp/longjmp overrides without new address-level evidence.
- A successful non-force rerun with zero uncatalogued findings is the configuration acceptance gate.
