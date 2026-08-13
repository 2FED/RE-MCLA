# M4-009 explicit offline-service boundary

Status: accepted on 2026-08-13.

## Decision

The supported autonomous frontend route reaches the Complete Edition title
with guest socket creation explicitly blocked and without relying on retired
online services. ReXGlue v0.9.0.16 classifies the ten XLiveBase messages found
in the MCLA generated corpus as either local offline operations or unavailable
online services, supplies deterministic return codes, and prevents the
network-disabled validation route from creating a guest socket.

This closes M4-009 as `offline-service-title-route-pass`. The canonical title
run observed zero XLiveBase message dispatches and zero guest socket attempts,
so it proves that the explicit offline/network-block policy does not block the
frontend—not runtime reachability of every classified message. Exact semantics
for all ten known message IDs are covered by focused unit tests. This does not
implement Xbox Live, friends, presence, matchmaking, invitations, or a general
host firewall.

## Canonical evidence

- Private run: `20260813-193514-2bb7e7ca`
- Result SHA-256: `52AC3A713038DC4EF9A6236CFB76462006B5AAF8D3856D2422A5410AA9847539`
- SDK: ReXGlue `v0.9.0.16`, commit `218b7750d4c848c9b35d371bf78dd6c26ab93398`
- Focused SDK tests: 3 cases / 32 assertions
- Adjacent offline/XAM regression subset: 6 cases / 42 assertions
- Runtime XLiveBase dispatches / socket attempts: 0 / 0
- Network block: enabled before guest launch; host socket attempts: 0
- Frontend: verified title capture, SHA-256 `0AAF57E538027C114C46B321E308721A51CC0923125C6753E1B2C36FEF2F7631`
- Lifecycle: exact-PID/title `WM_CLOSE`, exit 0 in 337 ms, no force cleanup or survivor
- Integrity: 15-file source-game tree and four runtime artifacts match before/after

The canonical runner performed a clean SDK install and clean host build. Its
runtime-log manifest, capture, cycle tree, three build/test logs, source-game
identity, and four runtime artifacts are hash-bound in the ignored private
aggregate.

The broader SDK unit executable still has pre-existing unrelated codegen
template fixture failures (`sdk_version_full` / `has_dll_modules`) followed by
the known global XMemory teardown assertion. M4-009 therefore records only the
clean focused and adjacent regression subsets above; it does not claim a clean
whole-SDK suite.

## Gate

```powershell
scripts/test-offline-service-smoke.ps1
scripts/run-offline-service-smoke.ps1
scripts/verify-offline-service-smoke.ps1 -ResultPath <private-result.json>
```

The compact fixture suite passes one positive and 12 fail-closed negatives.
Raw logs, build output, and the BMP remain private under
`private/evidence/M4-009/`.
