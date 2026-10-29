# M9 l39 internal owner-test candidate (2026-10-29)

Status: **built and unit-tested; not signed, not staged, never launched.**
This is an internal candidate for owner checks only, not a volunteer package.
No game, launcher, helper or probe binary was started for this work.

## Contents

The merged host tree `3239dd5` (M9-009 cursor auto-hide, M9-007 paused-menu
cap, M9-010 wheel/pad diagnostic triggers, KI-035 guest sampler, KI-026 probe)
built against the l39 SDK, which adds the KI-040 XInput re-scan, the SDL event
observer API and, new here, a KI-035 allocation-failure log.

## SDK (isolated copy `private/l39-sdk-src`, branch `mcla/ki-040-xinput-rescan`)

| Commit | Content |
|---|---|
| `a239b83` | pin (M8 command-processor optimizations) |
| `259d0b2` | KI-040: XInput re-scan on hotplug, instance/backend log, RawInput opt-in |
| `9ad1c11` | M9-010: read-only SDL event observer and joystick snapshot API |
| `d968f59308a7d966b827c01d821fcc10bc569c90` | **new**: KI-035 rate-limited log of failed guest allocations |

`d968f59` applies only the SDK hunk of
[M9-006-ki-035-diagnostic-proposal-20261029.diff](M9-006-ki-035-diagnostic-proposal-20261029.diff)
to `src/kernel/xboxkrnl/xboxkrnl_memory.cpp` (+26 lines). Variable names at this
pin match the proposal. On a failed `NtAllocateVirtualMemory` (returns
`X_STATUS_NO_MEMORY`) or `MmAllocatePhysicalMemoryEx` (returns 0) it logs, for
failure ordinals 1-32 and then every 256th:

```
[NtAllocateVirtualMemory] FAILED: #<n> base=... size=... page=... type=... protect=...
[MmAllocatePhysicalMemoryEx] FAILED: #<n> size=... page=... align=... min=... max=... protect=...
```

(`REXKRNL_IMPORT_FAIL` is a `REXKRNL_WARN`.) Return values and allocation paths
are unchanged. The counter is one shared relaxed atomic. No unit test was added:
the kernel tests have no fixture that drives these `_entry` functions into an
allocation failure (`tests/unit/memory/heap_allocation_test.cpp` covers
`BaseHeap` only). The commit is local; nothing was pushed.

Build: `private/l39-work/l39_sdk_build.ps1 build` (VS 2022 Build Tools, clang
20.1.8, llvm-rc). Incremental: one object (`xboxkrnl_memory.cpp`) compiled with
no diagnostics; the only warnings in the log are the existing link-step
`-gcodeview` "argument unused" driver notes. The build was installed to
`private/l39-sdk-install`.

| Test | Result |
|---|---|
| SDK ctest (`l39_sdk_test.ps1`) | **100% passed, 0 failed out of 1869** (baseline 1869/1869) |
| `unit_tests.exe [hotplug]` | 35 assertions in 3 test cases, pass |
| `unit_tests.exe [input]` | 404 assertions in 36 test cases, pass |

| `l39-sdk-install/bin/rexruntime.dll` | Bytes | SHA-256 |
|---|---|---|
| before (`9ad1c11`) | 10,964,480 | `F761FB6BD5ECDAD4E0518915AC2FA31838B1F7443A7C1616BD3934AB9606F045` |
| after (`d968f59`) | 10,965,504 | `9E7449DD22E6582C35B6ABC388E2D60B5F9BC467B26AC3E3D0B939BF71D49666` |

## Host build (`private/l39-merge-build`, worktree `3239dd5`, sources unchanged)

Incremental rebuild against the updated install: `mcla.exe` relinked, and
`mcla_diagnostic_image_helper.exe` was built (separate target). No compile
diagnostics, 0 warnings. All 32 test executables passed (exit 0, 0 failures;
`mcla_diagnostic_export_win_test` and `mcla_launcher_store_win_test` report one
skip each, as before).

## Input folder `private/l39-int-input`

Mirrors `private/l38-vol-input` (same seven binaries; the ticket is added by the
prepare script). Binaries come from `l39-merge-build`; the three SDK DLLs come
from `l39-sdk-install/bin` and are byte-identical to the copies in the build
directory.

| File | Bytes | SHA-256 |
|---|---|---|
| `mcla.exe` | 53,038,592 | `6D073E4DA6972D37138A47A717EDF1E26A1DF458867690A72718DC9293F8F10D` |
| `mcla_crash_handler.exe` | 194,560 | `E1AED0AD8C4BFDDABA9595A23AB54693F7F0BC48A6F7EBC0656FA47E95DBEE5F` |
| `mcla_diagnostic_image_helper.exe` | 60,928 | `BC6E649F89CE93473BC76AAA2DCC2C7AB93C17F5046647C6E75CB50CE4CA539B` |
| `MCLA-Launcher.exe` | 1,303,552 | `EC869F4521A0DF7565324B2CCD9C37E358C3998C95BF57C55A895C3EFBA6D985` |
| `rexruntime.dll` | 10,965,504 | `9E7449DD22E6582C35B6ABC388E2D60B5F9BC467B26AC3E3D0B939BF71D49666` |
| `rexgpu-xenos.dll` | 2,990,080 | `E6A9E348FB5AAFD4DA64F6A99F886FF4AC897C97B2DBCF1654F52C57148C3084` |
| `TracyClient.dll` | 246,784 | `622ADC9CE45CD3D3F0106054FB0681E54F6E174797FBCF68B611B2EB6B963EF9` |

Path-string check (`private/l39-work/pathcheck.py`, ASCII and UTF-16LE,
case-insensitive): no `c:\bdu`, `c:/bdu`, `\users\`, `/users/`, `mcla-recomp`,
`worktrees`, `l39-sdk` or `l34-src` in any file. The profile-name token hits 13
times in ASCII in `rexruntime.dll`, all colour or controller names
(`Darkorange`, `OrangeRed`, an `OrangeFox86` pad mapping). The l38
`rexruntime.dll` has the same 13 hits.

`mcla.exe` uses the l39 SDK observer API. It needs the l39 `rexruntime.dll`
next to it and does not run with l38 or earlier SDK DLLs.

## Owner steps

1. In PowerShell 7, run
   `private\tools\pwsh\pwsh.exe -NoProfile -File private\l39-internal-prepare.ps1`
   (use the full path). The script:
   - refuses to start if `private\l39-internal`, `private\l39-entries.json`,
     `private\l39-active-builds.txt` or `l39-int-input\mcla-test-build.ticket`
     already exists;
   - checks that `mcla.exe` is `6D073E4D...F8F10D` and `rexruntime.dll` is
     `9E7449DD...D49666`;
   - signs a V2 kill-switch ticket (`New-MclaTestBuildTicket.ps1 -KillSwitch`);
   - writes `l39-entries.json`: both `l38-entries.json` entries unchanged, plus
     the l39 host with `valid_until` = now + 30 days;
   - signs `l39-active-builds.txt`;
   - stages `private\l39-internal` with `Stage-MclaInternalLauncher.ps1`, using
     `-CrtDirectory private\l12` and no tester guide;
   - checks the list with `mcla_build_lease_probe_win.exe --verify-leases`;
   - runs `Test-MclaInternalLauncherStaging.ps1` on the input folder. This is
     the internal audit, which uses a synthetic fixture under `%TEMP%`.

   It publishes nothing.
2. **Kill-switch caution.** A V2 ticket treats the online list as authoritative.
   If the launcher downloads the published `active-builds.txt` and the l39 host
   is not in it, the build is refused as withdrawn and a local `revoked` record
   is cached. A later list that names the host clears it. Before launching
   online, publish `l39-active-builds.txt` (owner approval needed). Otherwise,
   start it only while offline and within 3 days of signing.
3. Point the launcher's Build folder at `private\l39-internal` and start from
   the launcher.

### What to check

Search the session log under `user\diagnostics\runtime\` for these lines.

| Feature | Action | Proof |
|---|---|---|
| M9-009 cursor auto-hide | With the window focused, press a pad button or turn the wheel well past a small angle, then move the mouse. Leave the pad idle with the cursor visible. Alt+Tab while the cursor is hidden. | The cursor hides on deliberate pad/wheel input and returns on mouse movement. It stays visible while the pad is idle and over other windows. `--mcla_cursor_autohide=false` disables the feature. Visual check only. |
| KI-040 pad reconnect | In a menu, switch the 8BitDo pad off for 10 s, then on. Repeat with 30 s off. Unplug the dongle for 5 s. Press F10 after each reconnect. | `SDL gamepad removed from physical slot 0`, then `OnControllerDeviceAdded: ... Instance(<id>), Backend(xinput), Path("XInput#M")`, then `SDL_ACTIVE_CONTROLLER ... reason=first-connected`, and input works. The F10 controller-matrix/input-slot audit shows guest `GetState` returning success with live buttons. |
| M9-007 paused-menu cap (unlocked mode) | Pause the game, open the map, go to the Arcade/race setup menus, then resume. | `MCLA menu state: game paused (...), UI cap forced, <low cvar> <value>` on entry and `game running ... draw-count` on exit. F10 in a paused menu shows about 30 fps. `mcla_ui_cap_guest_pause` (default `true`) controls it. |
| M9-010 triggers | Press wheel R2, wheel L2, and hold pad Start+Back for 1 s. | Status text `WHEEL L2 (WHEEL BUG): RECORDING...` (and the R2/pad equivalents). Live packages with `reason` `wheel-r2`, `wheel-l2` (with a `wheel` section in `state.json`) and `pad`. A log line `MCLA diagnostics: <reason> performance audit armed=1`. |
| KI-035 diagnostics | Press F10 (or any trigger) during normal driving and again in the garage-transition freeze. | `MCLA_GUEST_SAMPLE v=1 start threads=...`, per-thread sample lines, `MCLA_GUEST_SAMPLE v=1 end stopped=0`. During a freeze, any `[NtAllocateVirtualMemory] FAILED: #n ...` or `[MmAllocatePhysicalMemoryEx] FAILED: #n ...` lines tie it to guest heap exhaustion. No such lines is also a result. |
| KI-026 diagnostics | Set `mcla_ki026_probe = true` (init-only, default off) and play a race-back that softlocks. Leave `mcla_ki026_watchdog` off unless you want its behaviour change. | `MCLA_KI026_WARPER_STATE`, `MCLA_KI026_PAUSE_REQUEST`, `MCLA_KI026_GAME_CAMERA`, `MCLA_KI026_EDGE` lines. F10 `state.json` includes the guest KI-026 block. |

## Limits

- Nothing was launched. No physical result exists for any feature, and this
  document records no PASS.
- The ticket is not signed and the candidate is not staged. Both wait for the
  owner's PowerShell 7 run, which needs the signing key.
- The T300 L2/R2 raw button indices (default `8,9`) are unverified. Use
  `mcla_wheel_button_probe=true` to read them, then set
  `mcla_wheel_diagnostic_buttons`.
- Start and Back still reach the guest while held for the pad trigger.
- KI-040 only helps pads that get an XInput slot. DirectInput/HIDAPI devices
  do not benefit, and RawInput (`hid_sdl_rawinput`) stays an experimental
  opt-in.
- The KI-035 log shows only allocation failures. A freeze that is not caused
  by heap exhaustion will show none.
- The SDK commit `d968f59` exists only on the local isolated branch. It is not
  in the project's pinned SDK.
