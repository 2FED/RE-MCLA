# M3-007 explicit offline service states

Date: 2026-08-11
Result: PASS — DIRECT XONLINE/SOCIAL/XHV RESULTS ARE DETERMINISTIC

## Scope and failure mode

The accepted M2 import coverage contains nine direct XAM/XONLINE imports that were still registered through `REX_EXPORT_STUB` and one direct XHV-derived headset query that returned false without explaining the offline state. The generic macro logs a warning but does not write `PPCContext::r3`; a return-bearing call can therefore inherit a caller argument or temporary value and misreport it as success, a title ID, or a connection state.

M3-007 changes only those ten direct imports. It does not claim that Xbox Live, retired Rockstar services, multiplayer, voice transport, or their post-boundary call routes are implemented.

## Reviewed contract matrix

| Direct import | Explicit offline result | Diagnostic intent |
| --- | --- | --- |
| `XamShowFriendsUI` | `X_ERROR_NOT_LOGGED_ON` | social UI unavailable offline |
| `XamShowGamerCardUIForXUID` | `X_ERROR_NOT_LOGGED_ON` | gamer-card UI unavailable offline |
| `XamShowPlayerReviewUI` | `X_ERROR_NOT_LOGGED_ON` | player-review UI unavailable offline |
| `NetDll_XNetServerToInAddr` | `0x2743` (`WSAENETUNREACH`) | no service route exists |
| `NetDll_XNetUnregisterInAddr` | `0` | logged, idempotent cleanup no-op |
| `NetDll_XNetConnect` | `0x2743` (`WSAENETUNREACH`) | connection cannot be established |
| `NetDll_XNetGetConnectStatus` | `3` (`LOST`) | connection is explicitly absent |
| `NetDll_XNetQosLookup` | `X_ERROR_FUNCTION_FAILED` | no fabricated QoS result |
| `XNetLogonGetTitleID` | `0` | invalid/no online title ID |
| `XamVoiceHeadsetPresent` | `0` (`false`) | XHV voice/headset service unavailable |

Every path emits a once-only `[OFFLINE] <ImportName>` warning. The cleanup no-op is the sole success result and is not silent; state-bearing queries fail or return an explicit absent/invalid state.

## Regression and review evidence

The SDK regression invokes each exported bridge after setting `ctx.r3.u64 = 0xDEADBEEF`. Its ten assertions prove that no reviewed import preserves stale caller state.

```powershell
third_party\rexglue-sdk\out\win-amd64\RelWithDebInfo\unit_tests.exe "[kernel][xam][offline]"
ctest --test-dir third_party\rexglue-sdk\out\build\win-amd64-tests -C RelWithDebInfo -L unit -E "MigrationScan|TemplateRegistry|Template:"
ctest --test-dir third_party\rexglue-sdk\out\build\win-amd64-tests -C RelWithDebInfo -L ppc
.\scripts\test-offline-service-stubs.ps1
```

Results:

- focused offline regression: 3/3 cases, 10/10 assertions;
- relevant SDK unit suite: 210/210 passed, with four pre-existing BitStream skips;
- PPC suite: 1,459/1,459 passed;
- project verifier: one positive case passed and three negative fixtures were rejected for a generic-stub regression, missing `[OFFLINE]` marker, and wrong connection-state return;
- all 17/17 tracked project test scripts passed;
- ast-grep scan was clean and 3/3 rule tests passed;
- bootstrap passed 12/12 checks, including the recursive SDK source pin, installed CLI version, and full private source hashes;
- MCLA RelWithDebInfo configured against exact installed SDK `0.9.0.5` and linked successfully.

The unfiltered SDK unit run also exposed nine pre-existing template/migration failures involving `MigrationScan`, `TemplateRegistry`, manifest/template SDK-version fields, and CMake reference rewrites. None touches the four XAM sources or the new offline regression; the relevant suite above excludes those categories and is fully green. This baseline debt is not represented as an M3-007 regression.

## Pin and artifact identity

- fork: `https://github.com/2FED/rexglue-sdk.git`
- branch: `mcla/mcla-r-hotfixes`
- tag: `v0.9.0.5`
- commit: `23b55de7d0ac36b67d032eecc2bf8ed00d9d26a6`
- installed RelWithDebInfo runtime SHA-256: `39FC1458FBA79A19C60CC5B3DD1DF2BB1CBC84C2C9EBDCEFADFC7D9199A03A0A`
- linked MCLA RelWithDebInfo executable SHA-256: `C3F98784A0B2252BC1B643A08394E3E2D8016D313A3BA423E29499F80128F0E4`

The SDK CLI and installed CMake package both report `0.9.0.5`; the annotated tag resolves to the same immutable commit as the submodule.

## Upstream duplicate audit

Before publication, all open and closed upstream issues/PRs were searched for `REX_EXPORT_STUB`, `stale r3`, `stub return`, `return value`, `unimplemented export`, and `guest register`. Closed PR #312 was the only directly related result and covers only an unjustified `KeSetPriorityThread` default; it does not track the systemic macro behavior or per-API classification requirement.

The non-duplicate systemic report is [rexglue/rexglue-sdk#407](https://github.com/rexglue/rexglue-sdk/issues/407). It contains no private paths, proprietary addresses, game data, or memory dumps.

M3-007 acceptance: PASS. Every selected direct XONLINE/social/XHV call has an explicit logged offline contract, every guest-visible result is tested through the actual bridge, and no reviewed import can silently reuse stale `r3`.
