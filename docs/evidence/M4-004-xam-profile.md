# M4-004 XAM profile evidence

Date: 2026-08-12

Decision: PASS as `single-local-offline-profile`.

## Scope and claim boundary

The autonomous Complete Edition title route uses the existing deterministic
ReXGlue local user contract: slot 0 is locally signed in, exposes one stable
nonzero XUID and consistent name/sign-in information, while slots 1, 2, and 3
are independently signed out and return no-such-user information.

This task does not claim an account picker, Xbox Live sign-in, multiple local
profiles, generalized profile writes, or persistence. `ReadProfileSettings`,
privilege masks 251/252, and `SigninUI` were not reached in any accepted title
cycle. Their records remain fail-closed if reached, but physical runtime
coverage is not claimed. The deterministic voice defaults are unit-tested;
general profile persistence remains M6-006 scope.

## Automated result

- private run ID: `20260812-123316-db0f1cf4`
- private result SHA-256:
  `388F2FC350C43149CA6E5AD4E24A1B743F18CFE3A450C6D80962099F73C89AE0`
- isolated title/profile cycles: 3/3
- title capture time: 41,931-42,044 ms
- controlled exit: 319-588 ms; exit code 0 in all cycles
- slot-0 sign-in records: exactly 1 bounded record per cycle
- signed-out slot records: exactly users 1, 2, and 3 per cycle
- sign-in-state and sign-in-info absent masks: `E` / `E` in every cycle
- XUID records: exactly 1 per cycle; mask 7, success, nonzero
- name records: exactly 1 consistent record per cycle
- sign-in-info records: slot 0 plus distinct absent slots 1-3 per cycle
- profile-read, privilege, and SigninUI records: 0 in every cycle
- focused SDK profile defaults: 2 cases / 15 assertions
- title logo edge correlation: 0.965660-0.998975
- tight `PRESS` edge correlation: 0.993607-0.999956
- process cleanup: 3/3; no force cleanup or surviving exact-path process
- source identity: unchanged 15 files and 6,569,586,392 bytes
- prior-cycle and complete evidence trees: physically immutable

The accepted runtime artifact hashes are:

- `mcla.exe`: `184D208E64ECA81C0753EE36ADC0DA71EBA863B2912FBA6D12CE061E78C69FC3`
- `rexruntimerd.dll`: `C30F9751615357C189BB99C317047BEA0C0363F689AB0D37FEBC1BBE61498A10`
- `TracyClientrd.dll`: `6C04333BCE5DFEF75D3E9586918C04C7CBA70A1C2240779DE79A03118CA0BB49`
- `rexgpu-xenosrd.dll`: `A339B7DDB5035F37C48CBF7806D74C67857D5FB66EA2648BD74450FF87D438D1`

Raw logs, BMPs, unit output, build logs, XUID values, and result JSON remain
ignored and private.

## Verification

```powershell
scripts\test-xam-profile-smoke.ps1
scripts\run-xam-profile-smoke.ps1
scripts\verify-xam-profile-smoke.ps1 -ResultPath <private-result.json>
```

The fixture suite passes two positives and rejects 55 fail-closed negatives,
including duplicate or missing absent slots, incorrect presence masks, identity
mismatch, malformed optional profile/privilege records, topology, containment,
privacy, and summary drift. The canonical runner clean-installs the SDK, runs
the focused profile-default tests, clean-builds RelWithDebInfo, then binds all
three rotated-log sets and physical title captures to the sanitized aggregate.

Independent review found two scope/evidence P2 issues and one bounded-record
P2 follow-up. Documentation now excludes the three zero-hit routes; distinct
per-slot records and masks replaced aggregate-only absence proof; repeated
absent records were fixed to remain bounded before final release validation.
