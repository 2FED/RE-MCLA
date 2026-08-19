# M6-008 offline progression and retired-service matrix

Status: PASS

Accepted private result: `20260819-164950-4b322fe1`, SHA-256
`7DC8946ABD4B1BB78D52E527655D713EB4FD7037421997F4BA209689BC21903B`.

## Decision

MCLA now has an explicit offline-safe policy for the six progression and
retired-service groups in this task:

| Feature | Offline behavior | Progress source |
|---|---|---|
| Achievements | local state; guide UI reports not signed in | local achievement manager |
| Presence | unavailable | none |
| Leaderboards | valid empty enumeration | none |
| Rate My Ride | unavailable | none |
| Driving Test | title-local save | title save |
| Voice | unavailable; packet submission returns `800700AA` | none |

The implementation never fabricates leaderboard rows, Xbox Live identity,
presence, voice transport, progression, or vehicle unlocks. M6-009 still owns
the separate policy decision for vehicles formerly tied to retired services.

## Build and test evidence

ReXGlue `v0.9.0.26`, commit
`51f18fab1a5c11d50a380a30fbe592b93fd98248`, was clean-built in both
RelWithDebInfo and Release configurations before the Release MCLA host was
clean-built. The focused suites passed:

- offline XAM: 6 cases / 25 assertions;
- offline-service routes: 3 cases / 32 assertions;
- achievement manager: 6 cases / 50 assertions.

The result rehashes the SDK build and test logs, focused unit executable,
Release build log, four runtime artifacts, eleven source/generated files, and
five immutable accepted results: M4-004 local profile, M4-009 offline frontend,
M5-012 series progression, M6-003 race-system progression, and M6-005 save
restart behavior. The physical verifier passed after the schema-order defect in
the initial aggregate writer was corrected; no native runtime replay was
needed or claimed.

The compatibility choices were compared with the corresponding Xenia Canary
implementations of [XAM user/statistics](https://raw.githubusercontent.com/AdrianCassar/xenia-canary/master/src/xenia/kernel/xam/xam_user.cc),
[XAM UI](https://raw.githubusercontent.com/AdrianCassar/xenia-canary/master/src/xenia/kernel/xam/xam_ui.cc),
and [XAM voice](https://raw.githubusercontent.com/AdrianCassar/xenia-canary/master/src/xenia/kernel/xam/xam_voice.cc).

## Scope boundary

This build-only gate does not claim that the current physical route called each
service export. It does not prove an achievement guide UI, Xbox Live presence,
real leaderboard rows, a Rate My Ride backend, voice transport, or the exact
Driving Test unlock point. Those would require separate reached-route evidence.
It proves deterministic return-bearing SDK behavior, local state boundaries,
bounded buffer construction, clean integration, and consistency with already
accepted profile/progression/save routes.

Commands:

```powershell
scripts/test-offline-progression-report.ps1
scripts/run-offline-progression-report.ps1
scripts/verify-offline-progression-report.ps1 `
  -ResultPath private/evidence/M6-008/20260819-164950-4b322fe1/result.json
```
