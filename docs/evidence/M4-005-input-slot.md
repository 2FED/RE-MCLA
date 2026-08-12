# M4-005 SDL input-slot evidence

Date: 2026-08-12

Decision: PASS as `single-sdl-controller-slot0`.

## Scope and claim boundary

One selected physical SDL controller is deterministically assigned to Xbox 360
user slot 0. After the verified Complete Edition title checkpoint, an operator
held and released the SDL South/A face button. The bounded audit causally bound
both physical edges to matching guest-visible `XamInputGetState` slot-0 edges.
The always-connected idle NOP fallback cannot satisfy this challenge because it
cannot create either A edge or its SDL source sequence.

This task does not claim the complete button/axis surface, dead zones, focus
loss, disconnect/reconnect, multi-controller behavior, or vibration. Those
remain M4-006 scope.

## Automated result

- private run ID: `20260812-140305-f0bd8e1b`
- private result SHA-256:
  `3C388CC37986D40606FA9B458129230F669D23825D69C082678AC3B765ED585F`
- isolated physical title/input cycles: 1/1
- title/input READY: 44,076 ms
- operator A hold-to-guest-down: 14,365 ms
- A release-to-PASS: 2,018 ms
- exact audit chain: CONFIG, device slot 0, A-up title arm, SDL A down,
  guest A down, SDL A up, guest A up, PASS summary
- guest query/success/disconnected masks: `F` / `1` / `E`
- removals, rejected devices, unexpected events, failures, dropped records: 0
- focused SDK mapping tests: 5 cases / 12 assertions
- title logo/tight `PRESS` edge correlations: 0.919537 / 0.920386
- controlled exit: 561 ms; exit code 0
- process cleanup: no force cleanup or surviving exact-path process
- source-game identity: unchanged 15 files and 6,569,586,392 bytes
- game, runtime, capture, rotated-log, and evidence-tree hashes: physically
  reverified after exit

The accepted runtime artifact hashes are:

- `mcla.exe`: `6450E9B22D8499C7F590134781E411F6CCEA11A8C02C3149BF57C5DF4821B468`
- `rexruntimerd.dll`: `3874A8F27451EB1141E8C54718BB635A4A5F8E10748AF2BE589C1DAD278DE32B`
- `TracyClientrd.dll`: `375ED4F9193A754B8C6A324A17EE4D8410C77F7314940353241A8ED069EC18B8`
- `rexgpu-xenosrd.dll`: `5670C7B2EC06DB91A571AC2FF66D973694F3F6F320B6F6ABD8DD1CAF5B228F13`

Raw logs, BMP, controller-identifying host logs, build logs, unit output, and
result JSON remain ignored and private.

## Verification

```powershell
scripts\test-input-slot-smoke.ps1
scripts\run-input-slot-smoke.ps1
scripts\verify-input-slot-smoke.ps1 -ResultPath <private-result.json>
```

The fixture suite passes one physically bound title baseline and one compact
positive, and rejects 48 fail-closed negatives covering the backend/config,
device count and slot, A-up arm, edge/source ordering, guest user/result,
summary counters, malformed or private markers, topology, and source contract.
The canonical runner now prints progress for all seven long-running phases.

Independent review requested and verified two SDK corrections before the
accepted run: only the first controller is forced to slot 0 while later pads
retain valid free SDL player indices, and invalid audit phases are sanitized.
Direct negative/concurrency tests for the internal reducer remain non-blocking
SDK hardening; the accepted physical route exercises its positive causal chain.
