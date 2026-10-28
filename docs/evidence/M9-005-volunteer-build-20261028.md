# M9-005 volunteer build (in progress, started 2026-10-28)

Carries M8-018. Distribution option A (owner decision 2026-10-28): a closed
group of trusted testers, shared by the owner through a private link. Plan and
rules: [M9 priorities](../M9-priorities.md).

| Part | State |
|---|---|
| Kill switch (V2 tickets) | Implemented and unit-tested; not published |
| Post-session report prompt | Not started |
| Fresh F10 image or explicit stale mark (KI-042) | Not started |
| Diagnostics export with frame-pacing/CP/GPU summaries | Export exists (M9-002); summaries not started |
| Allowlisted staging and sentinel audit | Not started |
| Tester guide, one tester smoke | Not started |

## Kill switch (2026-10-28)

Policy text: [test-build expiry](../test-build-expiry.md#kill-switch-builds-v2-tickets-m9-005).

- `mcla_build_lease_policy`: `MCLA-TEST-BUILD-V2` tickets (`kill_switch`)
  and a pure `EvaluateKillSwitch` that returns admit/refuse plus a cache action
  (keep / refresh / revoke). V1 `Evaluate` refuses V2 tickets, so a V2 build
  never gets the 14-day allowance.
- `mcla_build_lease_runtime_win`: V2 tickets go to `CheckKillSwitch`. It reads
  `<host>.killswitch` (check time plus the signed list, or `revoked`), fetches
  the list unless the managed host can follow a check younger than 24 hours,
  applies the decision and writes the record atomically under the existing
  cache writer lock. V1 behaviour is unchanged.
- Owner rules as implemented: offline start for 3 days from creation or
  24 hours after a listing check; a verified list without the host refuses and
  records a withdrawal (also refused offline); network errors, timeouts and
  untrusted lists neither revoke nor extend.
- **One stricter choice than the literal rule.** A withdrawn build stays refused
  offline even inside its first 3 days. Without this, a tester could defeat a
  withdrawal by going offline. It is reversed as soon as a list names the
  host again.
- Tooling: `New-MclaTestBuildTicket.ps1 -KillSwitch` writes V2.
  `Mcla-TestBuildSigning.ps1` accepts V2 payloads. The staging check accepts
  V1 or V2 tickets.

Tests:

| Suite | Before | Now | Failures |
|---|---|---|---|
| Build lease policy | 1417 | 1438 | 0 |
| Build lease runtime (signed fixtures, real cache dir, injected network) | 47 | 69 | 0 |
| Launcher controller | 338 | 338 | 0 |
| Staging script | 63 | 63 | 0 |

ast-grep is clean.

Not yet run: `Test-MclaTestBuildSigning.ps1` (PowerShell 7). This agent session
may not execute the downloaded PowerShell 7 binary; the owner can run it, or
allow it.

Not done: nothing was published to `DebugBundle`. Every volunteer host hash
must be listed there before testers go online, and each publication needs the
owner's explicit yes. The volunteer `mcla.exe` must also be built with
`MCLA_TEST_BUILD_EXPIRY=ON`, so the host checks as well as the launcher.
