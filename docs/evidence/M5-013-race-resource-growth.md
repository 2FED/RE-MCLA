# M5-013 repeated-race resource-growth gate

Status: accepted. Physical result
`private/evidence/M5-013/20260817-015958-36eec226/result.json`, SHA-256
`D890A903775A3B53262B9957E7B7DC1D4B76B49B8735360BECD8233842446298`.

## Contract

The canonical gate uses one clean optimized Release process and the exact
completed M5-012 save. It records one stable title/gameplay baseline followed
by five operator-confirmed completed-race checkpoints in the same process.
Each checkpoint is delayed for five seconds, captures a private 1280x720 guest
frame, and samples the host process three times. The stored median contains
only numeric measurements:

- private bytes and working set;
- handle and host-thread counts;
- Windows `GPU Process Memory` dedicated and shared usage for the exact MCLA
  PID.

The baseline and each completed checkpoint are persisted immediately to the
same `resource-samples.json`; a later guest blocker therefore leaves the
already collected diagnostic measurements intact. Only the final six-sample
file is accepted as a result.

Windows may transiently publish a performance-counter sample whose `Status`
is invalid while a new GPU-process instance is appearing. The runner never
reads `CookedValue` from such a sample: it requires a valid dedicated/shared
pair for the exact PID and retries for up to ten seconds. A focused runner
self-test covers valid multi-instance aggregation, invalid-status rejection,
wrong-PID rejection, and the incomplete-pair retry condition.

The comparison deliberately starts at post-race checkpoint 1 rather than the
title baseline so normal shader/cache warm-up is not mislabeled as a leak.
Both the final value and every sampled peak from checkpoints 1 through 5 may
grow by at most 512 MiB private memory, 512 MiB working set, 128 handles, 16
threads, 512 MiB dedicated GPU memory, and 256 MiB shared GPU memory relative
to checkpoint 1. These are bounded-regression limits, not a claim of lifetime
leak freedom.

The route also requires five ordered present sequences, at least two distinct
capture hashes, the exact completed M5-012 save and header as its isolated
seed, a hash-bound clean Release build log, no fatal/assertion/unregistered-
function/guest-crash/device-loss marker, and external `WM_CLOSE` with exit 0.
The working copy may legitimately update race statistics or progression; its
final save and header must retain their expected shapes and are hash-bound in
the accepted result rather than being forced to remain byte-identical.
Raw logs, frames, saves, process IDs, and host paths remain private.

## Commands

```powershell
scripts/test-race-resource-smoke.ps1
scripts/run-race-resource-smoke.ps1
scripts/verify-race-resource-smoke.ps1 `
  -ResultPath <private-result.json>
```

The accepted optimized process completed five owner-confirmed race events over
approximately eighteen minutes. All five ordered 1280x720 captures have
different hashes. Relative to race checkpoint 1, final/peak growth was:

- private bytes: 206,409,728 / 206,409,728;
- working set: 196,239,360 / 196,239,360;
- handles: 6 / 14;
- host threads: 0 / 2;
- GPU dedicated bytes: 50,446,336 / 50,446,336;
- GPU shared bytes: 17,563,648 / 17,563,648.

All values remain below their declared bounds. The process emitted the exact
five-frame summary and exited 0 by external `WM_CLOSE`; source game, seed/save
shape, runtime artifacts, logs, samples, frames, and evidence tree are bound by
the persisted verifier. The working save remained byte-identical in this route,
which is permitted and not interpreted as five new progression writes.

The first attempt captured checkpoint 1 and then exposed the independently
bounded series-transition leaf `[0x8220B810, 0x8220B834)`. Non-force generation,
exact dispatcher registration, Release compilation, and the accepted rerun
through that transition all pass. The harness also gained invalid-performance-
counter retry handling and a `-FinalizeRun` recovery path after the successful
physical run initially hit a verifier-only timing false negative.

The fixture suite now has five positives (counter self-test, unchanged and
shape-preserving updated working-save probes, persisted-result mode, and
completed-run finalization) and fourteen fail-closed negatives.

## Scope

Acceptance covers five race completions in one Release process on the current
Windows/D3D12 host. It does not cover every race, first-run/OOBE, every GPU
driver/backend, multi-controller play, indefinite soak duration, or attribution
of any future growth to a particular guest or host allocator.
