# M2-004 save reload and controller baseline

Date: 2026-08-11
Result: PASS

## Isolated save lifecycle

The first verified stock session began with zero profiles. The operator created a local profile/save, started a new game, completed the opening route, entered free roam and the garage, and exited Xenia normally. The raw profile, save, header, logs, and snapshots remain ignored under the isolated private baseline.

Two private save snapshots establish that autosave content persisted and changed as play progressed:

| Snapshot | Save bytes | Save SHA-256 | Header bytes | Header SHA-256 |
| --- | ---: | --- | ---: | --- |
| Earlier active-session snapshot | 537428 | `0E805392B9F49D736D7C7B2658D620130D133370323A62101A8253C8FFC3D14C` | 328 | `1DAEF55FA78BDD3F1AA75A36772B801C6C714B6D1A115A97904F3D1C957BC4C9` |
| Final snapshot after normal exit | 537428 | `E8B559E1F2D03341B1147CAB4F5A3F3C778E6E9633B0B04B9908360FA2C67D68` | 328 | `1DAEF55FA78BDD3F1AA75A36772B801C6C714B6D1A115A97904F3D1C957BC4C9` |

The differing save hashes with a stable container header show an in-place title-data update rather than a new tracked artifact. No raw save bytes are distributed.

A third snapshot after the resumed game loaded, entered a nighttime race, and exited normally remained byte-identical to the final pre-reload snapshot. This is expected for a load-only route with no later autosave trigger and confirms that the reload test did not corrupt or rewrite the container.

## Clean reload result

`scripts/run-xenia-baseline.ps1 -Resume` reused the same explicit isolated storage/content/cache roots only after the first Xenia process had exited. It revalidated the exact pinned Xenia executable and `default.xex`, retained stock timing with patches/title updates disabled, and selected a new uniquely named log instead of overwriting the first-session log.

The resumed private log reported:

- exact Xenia build `canary_experimental@7d8db5a2c`
- exact module hash `1984A3354B78CE19`
- one existing profile found and loaded into slot 0
- the connected Xbox-compatible controller added to slot 0
- existing `mc4.sav` enumerated for Midnight Club: LA
- zero fatal, assertion, crash, device-lost, or unhandled-exception markers at the reload gate

The operator selected Continue and explicitly confirmed that the previously saved game progress loaded. A frontend capture before Continue and a nighttime race capture after Continue independently bracket the successful reload route.

## Controller matrix

Xenia's SDL mapping exposed the standard Xbox-compatible button/axis set. The operator exercised the complete input surface across frontend navigation, driving, pause, garage, and race states and reported functional responses:

| Input group | Confirmed inputs |
| --- | --- |
| Analog sticks | Left stick and right stick, including their normal in-game functions |
| Directional | D-pad up/down/left/right |
| Analog triggers | LT and RT |
| Shoulder buttons | LB and RB |
| Face buttons | X, Y, A, and B |
| System/game navigation | Start and Back/Select |

The missing optional `gamecontrollerdb.txt` did not prevent detection or use. Force-feedback ordinal warnings remain an M2-010/M2-014 triage item; this task confirms input, not rumble fidelity.

## Resume safety and verification

Resume mode:

- requires an explicit existing baseline below `private/`
- rejects missing storage/content/cache directories
- rejects baseline and required-directory reparse points
- refuses a second resume while the verified Xenia executable is already running
- creates a new `xenia-resume-*.log` and refuses log overwrite
- preserves the original new-baseline behavior and its overwrite rejection

PowerShell syntax parsing, positive new-session and resume `-WhatIf` preflights, explicit-path enforcement, existing-path rejection for non-resume runs, missing-directory rejection, concurrent-resume rejection, private containment, and no-write behavior passed. `ast-grep scan` and all 3/3 project structural rule tests also passed.

This evidence contains no absolute host path, profile name/identifier, save path, controller GUID, or proprietary save payload.
