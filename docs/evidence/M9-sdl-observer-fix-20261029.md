# M9 l39 owner result: SDL observer investigation (2026-10-29)

Status: **no code defect found; the owner's game sessions ran the l38 volunteer
build, not l39.** No SDK or host source was changed. No SDK commit was made.
A re-ticketed owner candidate `l40` (the l39 binaries, byte-identical, with a
plain V1 ticket) is prepared for a re-test. No game, launcher or helper binary
was started for this work.

## Owner result (2026-10-29)

Reported on the l39 internal candidate: prepare OK; offline start OK; refused
at date +4 days as designed; KI-040 reconnect worked (pad power-cycle, dongle
unplug, Bluetooth switch); F10 works; M9-009 cursor auto-hide did not hide the
cursor on pad input; M9-010 pad Start+Back hold (any duration) produced no
diagnostic package (the pause menu opened, as expected).

## Log evidence

Only one game session ran after `private/l39-internal` was staged (15:51
local): runtime log `runtime-20261029T135608Z-45104.log` (15:56 to 16:00 local)
in the owner's MAIN-backup profile. Two earlier sessions today (13:03, 13:26
local) predate the l39 SDK commits.

1. **No M9-009 or M9-010 line at all.** The 15:56 log has none of
   `MCLA cursor autohide: enabled`, `... SDL event watch unavailable`,
   `MCLA diagnostics: controller triggers enabled ...` or
   `... unavailable (SDK event observer)`. In the l39 host every path through
   `MclaApp::OnPostSetup` logs one of each pair (`src/mcla_app.cpp:1289-1296`,
   `3634-3667`). `[app] MCLA diagnostics: post-runtime crash handlers refreshed`
   (same function, line 1282) is followed directly by
   `[ppc] MCLA module identity ...` from `LaunchModule`.
2. **The device-added line has the pre-KI-040 format.** The log shows
   `SDL OnControllerDeviceAdded: "XInput Controller #1", JoystickType(1),
   GameControllerType(3), VendorID(0x2DC8), ProductID(0x310B)` with no
   `Instance(...)`, `Backend(...)`, `Path(...)` fields. SDK commit `259d0b2`
   (in l39) added those fields; the format string is present in the l39
   `rexruntime.dll` (1 hit) and absent from the l38 one (0 hits).
3. **The session's build fingerprint is the l38 volunteer manifest.**
   `launch-receipt.txt` and `capabilities.txt` record
   `build 460EE32F22B288FD...C2DED06126E2C3DA9E584E6`; the launcher uses the
   SHA-256 of `mcla-build.manifest` as the build fingerprint (and cache folder
   name). `private/l38-volunteer/build/mcla-build.manifest` hashes to
   `460EE32F22B288FD...`; the l39 manifest hashes to `D0F1B45F4EECE536...`.
   No session receipt and no profile cache folder for `D0F1B45F...` exists in
   any profile.
4. **The launcher preference still points at l38.** The launcher preferences
   file (saved 15:56) names `private\l38-volunteer\build` as the Build folder.
   The l38 volunteer `mcla.exe` contains no `cursor autohide` string: it has no
   M9-009 or M9-010 code.
5. The pad was seen and re-acquired by the SDK driver in that session:
   `SDL OnControllerDeviceAdded` five times (XInput Controller #1 and, after
   the Bluetooth switch, `8BitDo Ultimate 2 Wireless`), each with
   `SDL_ACTIVE_CONTROLLER ... reason=first-connected`, and matching
   `SDL gamepad removed from physical slot 0` lines. No
   `MCLA_WHEEL_BUTTON_PROBE` or trigger line (the l38 host has none).

Conclusion: the M9-009 and M9-010 owner checks, and the KI-040 check, were run
against l38. The l39 features were never exercised. **The KI-040 "reconnect
worked" observation is therefore l38 SDK behaviour (no re-scan fix) and is not
evidence for the KI-040 fix.**

## Source review (no defect found)

The original hypothesis was that the SDK disables joystick/gamepad events or
installs the observer watch on the wrong SDL instance. The l39 source shows
neither:

- `src/input/sdl/sdl_input_driver.cpp:193-260` (`OnWindowAvailable`, on the UI
  thread): `SDL_InitSubSystem(SDL_INIT_EVENTS)`, then the driver's own
  `SDL_AddEventWatch` (it consumes joystick/gamepad events through a watch,
  `HandleEvent` at 813), then `SDL_INIT_GAMEPAD`. There is no
  `SDL_SetJoystickEventsEnabled`, `SDL_SetGamepadEventsEnabled` or
  `SDL_SetEventEnabled` call anywhere in the SDK. `HandleEvent` flushes the
  queue every 64 events (820-824); watches run before queueing, so the flush
  does not hide events from other watches.
- `src/ui/sdl_event_observer.cpp:109-123`: `AddSdlEventObserver` installs one
  `SDL_AddEventWatch` on first use. It is compiled into `rexruntime.dll`;
  `mcla.exe` imports `AddSdlEventObserver`/`RemoveSdlEventObserver` from
  `rexruntime.dll` and contains no SDL function (only the `SDL_Event` type
  name in mangled symbols and log tokens). Same SDL instance as the window and
  the input driver.
- Ordering: `ReXApp::OnInitialize` (`src/ui/rex_app.cpp:88-104`) runs
  `SetupPresentation` (window) before `ConstructRuntime`, whose last step is
  `OnPostSetup` (306). The input driver logged
  `SDL input driver initialized successfully` (events and gamepad subsystems
  up) before `OnPostSetup` in the owner log. In the vendored SDL 3.5.0,
  `SDL_AddEventWatch` only appends to the watch list
  (`thirdparty/sdl3/src/events/SDL_eventwatch.c:94-119`) and does not fail
  after init.

Residual SDK risk, not a cause here: the observer watch is installed once
(`std::call_once`). If the SDL events subsystem were fully quit and
re-initialised during a session, `SDL_QuitEventWatchList` would drop it and it
would not be re-added. The SDK only quits events at shutdown.

## Fix

None in code. The owner must re-test with the launcher Build folder set to the
new candidate. SDK branch `mcla/ki-040-xinput-rescan` stays at
`d968f59308a7d966b827c01d821fcc10bc569c90`; no new SDK commit; host worktree
sources unchanged. No rebuild was needed, so no new SDK ctest or host test run
was made for this task (l39 results: SDK ctest 1869/1869; 32 host test
executables pass, 0 warnings; see
[M9-l39-owner-candidate-20261029.md](M9-l39-owner-candidate-20261029.md)).

## l40 owner candidate

Owner rule (2026-10-29): owner builds use a plain 14-day V1 ticket, without the
kill switch and without entries/active-builds signing. Volunteer builds keep
the V2 flow.

`private/l40-int-input`: the same seven files as `l39-int-input`, copied from
`private/l39-merge-build` (host binaries, launcher) and
`private/l39-sdk-install/bin` (SDK DLLs). Every hash equals the l39 input.

| File | Bytes | SHA-256 |
|---|---|---|
| `mcla.exe` | 53,038,592 | `6D073E4DA6972D37138A47A717EDF1E26A1DF458867690A72718DC9293F8F10D` |
| `mcla_crash_handler.exe` | 194,560 | `E1AED0AD8C4BFDDABA9595A23AB54693F7F0BC48A6F7EBC0656FA47E95DBEE5F` |
| `mcla_diagnostic_image_helper.exe` | 60,928 | `BC6E649F89CE93473BC76AAA2DCC2C7AB93C17F5046647C6E75CB50CE4CA539B` |
| `MCLA-Launcher.exe` | 1,303,552 | `EC869F4521A0DF7565324B2CCD9C37E358C3998C95BF57C55A895C3EFBA6D985` |
| `rexruntime.dll` | 10,965,504 | `9E7449DD22E6582C35B6ABC388E2D60B5F9BC467B26AC3E3D0B939BF71D49666` |
| `rexgpu-xenos.dll` | 2,990,080 | `E6A9E348FB5AAFD4DA64F6A99F886FF4AC897C97B2DBCF1654F52C57148C3084` |
| `TracyClient.dll` | 246,784 | `622ADC9CE45CD3D3F0106054FB0681E54F6E174797FBCF68B611B2EB6B963EF9` |

Path-string check (`private/l39-work/pathcheck.py`, ASCII and UTF-16LE): six
files clean; `rexruntime.dll` has the same 13 ASCII profile-token hits as l38
and l39 (colour and controller names), no path hits.

Prepare script: `private/l40-internal-prepare.ps1` (SHA-256
`A17D800A040E5B1E23F1FB8BABD20944501E3FFC47F4B828BBCFCD8444050EB1`; parses with
0 errors; not run). It refuses to overwrite `private/l40-internal` or the
ticket, pins the `mcla.exe` and `rexruntime.dll` hashes above, signs a V1 ticket
(`New-MclaTestBuildTicket.ps1` without `-KillSwitch`) and checks its first line
is `MCLA-TEST-BUILD-V1`, stages with `Stage-MclaInternalLauncher.ps1`
(`-CrtDirectory private\l12`, no guide), and runs
`Test-MclaInternalLauncherStaging.ps1`. No entries, active-builds or lease-probe
step. Nothing was signed by the agent.

### Owner steps

1. `C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l40-internal-prepare.ps1`
2. Start `private\l40-internal\MCLA-Launcher.exe`. In its settings set the
   Build folder to `private\l40-internal\build` and confirm it no longer says
   `private\l38-volunteer\build`.
3. Start the game. After the session, the profile's new `sessions\<id>\launch-receipt.txt`
   must show a build fingerprint other than `460EE32F...`, and the runtime log
   must contain `MCLA cursor autohide: enabled` and
   `MCLA diagnostics: controller triggers enabled ...`, and device-added lines
   with `Instance(...)`, `Backend(...)`, `Path(...)`.
4. Re-check: cursor hides on pad/wheel input and returns on mouse movement;
   Start+Back hold for the configured time writes a `live` package; wheel
   L2/R2 (if available); KI-040 pad power-cycle, dongle unplug and Bluetooth
   switch.

## M9-009 story, updated

1. M9-009 (l38/l39 cursor build) called `SDL_AddEventWatch` from `mcla.exe`
   on a second, uninitialised SDL copy; it could not have worked.
2. M9-010 moved both host adapters to the SDK observer API in
   `rexruntime.dll` (SDK `9ad1c11`); l39 is the first build with that fix.
3. The owner's 2026-10-29 "does not work" result came from l38 (launcher Build
   folder preference), which has no M9-009 or M9-010 code. M9-009 and M9-010
   remain **physically untested** on l39 binaries; l40 is the re-test vehicle.
