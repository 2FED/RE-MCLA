# M6-002 representative garage-lifecycle gate

Status: accepted. Physical result
`private/evidence/M6-002/20260817-155005-1dd57bd3/result.json`, SHA-256
`21FCBE38695B45D22AE516E33E51AD0A292286985549673811E0FE3E33900644`.

## Contract

The canonical gate uses one clean Release build and two isolated processes.
Both start from a bounded private profile tree and explicitly select guest
sign-in compatibility state 2. This enables MCLA's offline save-permission
path; it does not implement or claim Xbox Live or any network service.

A deterministic synthetic slot-0 controller executes the complete route with
no operator interaction. Every button pulse is accepted only after the host
records source-down, guest-visible down, source-up, and guest-visible up in
that order. Cycle 1 uses 53 pulses to enter saved gameplay, choose `GO TO
GARAGE`, purchase the unlocked 1983 Golf/GTI, buy and apply one representative
exterior item and the 5Zigen exhaust item, unlock the paint shop, apply a
dark-blue paint, switch back to the 1998 240SX, and return to free roam. Cycle
2 uses 16 pulses in a fresh process to re-enter the garage, show both vehicles,
select the persisted blue Golf, switch back to the 240SX, and return to free
roam.

The gate captures seven cycle-1 and five cycle-2 frames at explicit lifecycle
boundaries. It also requires exact save/header shapes, a changed cycle-1 save,
byte-exact profile handoff into cycle 2, no fatal/assertion/guest-crash/device
removal marker, and controlled external `WM_CLOSE` in both processes. Raw
frames, save content, logs, item text, identifiers, and host paths remain
private.

## Commands

```powershell
scripts/test-garage-lifecycle-smoke.ps1
scripts/run-garage-lifecycle-smoke.ps1
scripts/verify-garage-lifecycle-smoke.ps1 `
  -ResultPath <private-result.json>
```

## Accepted result

The physical run completed all 69 causal control steps and all twelve ordered
captures without user input. Visual review confirms:

- the purchased Golf is active after the showroom transaction;
- the exterior item visibly changes its front bumper;
- the performance screen shows the 5Zigen exhaust installed;
- the Golf changes from black to dark blue;
- the fresh process lists both the 240SX and Golf;
- the fresh-process Golf retains the blue paint and exterior item;
- both cycles return to free roam and close externally.

The cycle-1 save differs from the seed, cycle 2 begins from the exact cycle-1
profile handoff, and the second process produces another valid save. The
537,428-byte save and 328-byte header shapes remain valid; their raw content
digests stay private. The final evidence tree contains 26 files, 23
directories, and 53,427,573 bytes with tree SHA-256
`BABE1C92883BC01A1AF228680AFCFC8B80DA9B5636CB89787A35084A0EB0FFE1`.

The fixture suite passes two positives, twenty-six fail-closed negatives, and
thirty-nine source-contract checks. The final verifier rehashes both process
logs, all captures, the save chain, clean build, runtime artifacts, prior
evidence, and complete evidence tree.

## Scope

Acceptance proves one affordable, unlocked representative lifecycle on the
current completed save: second-vehicle purchase, one exterior item, one
performance item, paint-shop access and color change, vehicle switching,
free-roam return, and fresh-process persistence. It does not claim exhaustive
vehicle, motorcycle, garage, item, price, economy, unlock, campaign, or network
coverage. M7-004 owns the complete category matrix.
