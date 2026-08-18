# M6-003 representative race-system matrix

Status: accepted. Physical result
`private/evidence/M6-003/20260818-180605-e3b74fb5/result.json`, SHA-256
`AD3573B56412D7DBC10650BE0D250AFD4AACA22EB8EEEB6F9816E94EC3007E3A`.

## Contract

The current route starts from the exact completed HANGOUT save, explicitly
selects guest sign-in compatibility state 2, and asks the owner to enter one
available two-car Ian or Martin event. A default-off InitOnly audit captures
the two-car start and stable reward screen and records a bounded invocation of
the reached `Race_Finish` guest wrapper. The current run is combined with
immutable accepted evidence for traffic rendering, a complete multi-event Ian
series, and five consecutive reward/resource checkpoints.

The optional race-description, checkpoint-list/hit, and UI-result wrappers are
kept in the schema but were zero-hit on the retail Martin route. Their zero
counters are therefore preserved as a telemetry limitation rather than
misrepresented as evidence of missing gameplay. Current event completion and
police absence are explicit external owner observations; they are not marked
as machine-verified or embedded in the runtime log. The owner previously
observed police during the Ian route, also as external testimony.

## Commands

```powershell
scripts/test-race-system-smoke.ps1
scripts/run-race-system-smoke.ps1
scripts/verify-race-system-smoke.ps1 `
  -ResultPath <private-result.json>
```

The accepted physical event completed before the original strict summary timed
out. It was recovered without replaying the race:

```powershell
scripts/run-race-system-smoke.ps1 `
  -FinalizeRun private/evidence/M6-003/20260818-180605-e3b74fb5 `
  -RecoverOpponent MARTIN -RecoverPolice not-seen
```

Recovery is fail-closed and unique to the pinned run. It binds the original
log, clean-build log, both BMPs, physical artifact manifest, and missing event
lifecycle tail. A separate cycle using that physical binary reached a
nontrivial title frame and exited 0 through external `WM_CLOSE`; it proves the
binary's controlled lifecycle, not a controlled exit for the force-cleaned
event process.

## Accepted result

The current event visibly contains two cars at the Martin start and a stable
reward screen showing 100 REP and $340 earned. The machine audit records one
completed `Race_Finish` wrapper invocation between those distinct frames, no
fatal/assertion/guest-crash/device-removal marker, and no dropped records. The
owner completed the race and reported that police were not seen.

The result also rehashes the accepted Ian multi-event series, the five-race
reward/resource run, and the traffic-category result. The final evidence tree
contains 17 files, 23 directories, and 16,239,105 bytes with tree SHA-256
`C2F1637B42442F24A1F830D4C8641DFCFAC7EA072E603D74AB574DA41E0DD03B`.
The fixture suite passes three positives, twenty-eight fail-closed negatives,
and twenty-four source-contract checks. PowerShell parsing, a clean Release
build, ast-grep scan, and all three ast-grep rule tests pass.

## Scope

Acceptance binds one current Martin head-to-head event, one prior complete Ian
multi-event series, five prior race rewards, and prior traffic activity. It
proves the reached finish/reward route and representative race-system breadth;
it does not prove every race type, opponent, checkpoint implementation, police
behavior, reward value, difficulty, or campaign branch. The zero-hit optional
wrappers are not claimed as current machine telemetry, and the original event
process is explicitly recorded as force-cleaned after its post-reward timeout.
