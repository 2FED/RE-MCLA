# M3-006 XAM startup import matrix

Date: 2026-08-11
Result: PASS — NO PROJECT-OWNED XAM STUB REQUIRED

## Boundary and method

The reviewed boundary is module entry `xstart` at `0x821322B8` through the first call to title main at `sub_821305E8`. The conservative M2-014 envelope contains four XAM imports: `XamLoaderTerminateTitle`, `XamShowMessageBoxUIEx`, `XGetAVPack`, and `XGetLanguage`.

The final RelWithDebInfo executable was launched with the exact Complete Edition XEX, isolated private user/cache roots, and CDB breakpoints on all four bridges. A temporary return breakpoint on the reached normal import reads `PPCContext::r3` after the bridge completes. The verifier requires:

- exactly one title-main boundary;
- the exact stock-route XAM import set;
- the expected guest return value for every reached normal import;
- no conditional UI/termination import before the boundary;
- no fatal, invalid-function, `PPC_UNIMPLEMENTED`, or message-box stub marker.

Reproduction:

```powershell
.\scripts\test-xam-startup-import-trace.ps1
.\scripts\run-xam-startup-import-trace.ps1
```

Final private run: `private/evidence/M3-006/20260811-120652-065cd588-cdb`

## Runtime behavior matrix

| Import | Static classification | Runtime result | Startup requirement |
| --- | --- | --- | --- |
| `XamLoaderTerminateTitle` | post-title-return termination | 0 hits | not required before title main |
| `XamShowMessageBoxUIEx` | conditional error-dialog semantic stub | 0 hits; no stub warning | not required on the stock route |
| `XGetAVPack` | normal platform query | 1 hit; guest `r3 = 6` | required and behavior-verified |
| `XGetLanguage` | conditional platform query | 0 hits | not required on the observed stock route |

The minimum observed pre-main XAM set is therefore one function, not all four members of the conservative static envelope. The result `6` is the pinned SDK's VGA-compatible AV-pack value and satisfies the title's early PAL/platform check. No project override or semantic stub is needed to reach title main.

The zero-hit functions remain in the reviewed envelope because their static branches are real. The verifier intentionally rejects any future pre-main hit from those functions, forcing a behavior review instead of silently expanding the accepted route. `XamShowMessageBoxUIEx` remains an SDK warning/zero-return stub for later error-route work; M3-006 does not claim that unobserved UI behavior is complete.

## Verifier coverage

`scripts/test-xam-startup-import-trace.ps1` executes one positive and six negative fixtures. It proves rejection of:

- a missing title-main boundary;
- a missing required `XGetAVPack` call;
- a wrong AV-pack return value;
- an unexpected pre-main `XGetLanguage` call;
- a reached `XamShowMessageBoxUIEx` stub;
- a fatal or invalid-function runtime marker.

## Evidence identity

For final private run `20260811-120652-065cd588-cdb`:

- executable SHA-256: `BFDB60385D889A350038E618B2CB134A2BC49632DEB9C0C2AACAF2848243B106`
- CDB transcript SHA-256: `C87311BD00F6D912EDE606FBE543B3A7E2263554E675E9FC6746607384D3EBF1`
- runtime log SHA-256: `355EF1791F49A8FE239B19F7F087C803C9F072EC65574CF7C94E7D319E4D7A44`

M3-006 acceptance: PASS. Every XAM import in the conservative startup matrix has an explicit runtime classification, the only stock-route import has a guest-visible behavior comparison, and no missing XAM implementation is hidden by the successful title-main transition.
