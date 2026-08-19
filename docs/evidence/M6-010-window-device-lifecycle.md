# M6-010 window and device lifecycle matrix

M6-010 closes the platform-lifecycle checklist with a split physical matrix.
The accepted private result is
`private/evidence/M6-010/20260819-180446-730942f2/result.json`, SHA-256
`746D7DAECD475E48EEA9EE8342319A84CC2E059992A635579AB55AF7A719C71B`.
It uses exact ReXGlue v0.9.0.26 commit
`51f18fab1a5c11d50a380a30fbe592b93fd98248` and a clean Release host build.

## Physical matrix

The six rows deliberately reuse immutable evidence where that is stronger than
replaying an already accepted interactive route:

| Row | Physical source |
| --- | --- |
| gameplay pause | M5-006 `20260814-130533-0b95f6b6` |
| focus-loss neutralization and recovery | M4-006 split controller matrix |
| controller disconnect and reconnect | recovered M4-006 hotplug run `20260813-144406-2c1974da` |
| audio pause and default-device recovery | M6-007 `20260819-122150-2ad3b961` |
| repeated controlled external close | M4-013 `20260814-023505-e69d76a2` |
| minimize, restore, rendered output, and close while minimized | current M6-010 run |

The current autonomous cycle reached the captured Complete Edition title,
minimized the exact game window, kept the process alive for 5,025 ms, restored
the window, and captured a new 1280x720 physical desktop image. The restored
frame has 234 occupied sampled RGB555 bins and a 180-level p05-p95 luma range.
It then minimized the same window again, posted external `WM_CLOSE`, and exited
0 in 571 ms without force cleanup. The 10,532-byte runtime log contains the
bound nontrivial guest-frame marker and the ordered window-close/hard-exit tail,
with no guest crash, unimplemented PPC, device removal, or D3D12 device-loss
marker. The complete five-file, 7,472,224-byte cycle tree is rebound by
SHA-256 `BF3F82D4A0842D94184E3BEDA2B84B3E9684ADAA4749817287B0A8A3749A3AF0`.

The initial frame SHA-256 is
`B2BC7AF7856AA8991EBB580BF52DBE146835479202308C92E8645688C75A6909`;
the post-restore frame SHA-256 is
`A37863E3750C15A547C26D4753B0A4E105A1ACA6E4143D5852A412D837780745`.
The differing hashes are supporting evidence that the restored presentation
continued to update, not a whole-frame parity assertion. Source-game content,
the isolated non-capture user tree, and all four runtime artifacts remained
unchanged.

## Verification

```powershell
scripts/test-window-device-lifecycle-smoke.ps1
scripts/run-window-device-lifecycle-smoke.ps1
scripts/verify-window-device-lifecycle-smoke.ps1 `
  -ResultPath <private-result.json>
```

The source contract checks SDL minimize/restore dispatch, focus propagation,
input focus hooks, the project close hook, and the console-style hard-exit
boundary. The runner rehashes the three prior results and the recovered hotplug
tree, clean-builds Release, uses contained user/cache/log roots, captures the
restored client at a normalized 1280x720, and closes the exact process-owned
window while it is minimized. The final verifier rehashes both frames, the
complete runtime log manifest, the build log, and all four runtime artifacts.
One positive and thirty-eight fail-closed fixture mutations plus twenty source
checks pass.

## Scope

External `WM_CLOSE` is the supported desktop exit for this console-style title;
MCLA has no in-game desktop Exit command. The normal close path intentionally
hard-exits after title termination, so this result does not claim graceful
subsystem teardown. The matrix also does not claim OS suspend/hibernate,
display-adapter removal, multiple-controller identity, or one monolithic
process containing all six rows. Exact music selection or continuous music is
not part of this lifecycle gate; M6-007 owns the accepted long-session audio
pause/default-device recovery evidence.
