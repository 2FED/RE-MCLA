# M9-010 diagnostic triggers without a keyboard (2026-10-29)

Owner specification 2026-10-29: [M9 priorities](../M9-priorities.md) row 10.
State: **implemented, unit-tested and built; not physically verified.** No
game, launcher or helper binary was started for this work.

| Trigger | Category | Source (`reason`) | Extra data |
|---|---|---|---|
| Keyboard F10 (unchanged) | `general` | `f10` | none |
| Wheel R2 (raw joystick button) | `general` | `wheel-r2` | none |
| Wheel L2 (raw joystick button) | `wheel` | `wheel-l2` | `wheel` section in `state.json` |
| Gamepad Start+Back held 1 s | `general` | `pad` | none |

All four go through the same `RequestLiveSnapshot` path as F10: the same
`DiagnosticHotkey` admission (busy rejection, SDK frame-audit arming), the same
on-screen status (now prefixed with the trigger, for example
`WHEEL L2 (WHEEL BUG): RECORDING...`), the same fresh guest-frame capture
(KI-042 path) and the same `live` package folder.

## M9-009 correction: two SDL instances

The primary found (2026-10-29) that `rexruntime.dll` exports no `SDL_*`
symbol and, since M9-009, `mcla.exe` linked its own copy of `SDL3-static.lib`
(about 2.4 MB larger, with SDL strings). The M9-009 cursor-autohide adapter
called `SDL_AddEventWatch` directly, so it registered its watch on a second,
never-initialised SDL instance inside `mcla.exe`. That instance never receives
window, mouse, joystick or gamepad events: **the M9-009 adapter, as built in
l38/l39 before this change, could not have worked.** The SDK's SDL instance
(inside `rexruntime.dll`) owns the window, the input driver and all events.

Fix (this task):

- **SDK** (`private/l39-sdk-src`, branch `mcla/ki-040-xinput-rescan`, commit
  `9ad1c119ec41d35a63333df3bf7ac6d2f47f3f62` on top of KI-040 `259d0b2` and the
  pin `a239b83`; local only, not pushed): new public header
  `rex/ui/sdl_event_observer.h`, implemented in `src/ui/sdl_event_observer.cpp`
  (part of `rexui`, linked into `rexruntime.dll`, exported by
  `WINDOWS_EXPORT_ALL_SYMBOLS`):
  - `SdlEventObserverToken AddSdlEventObserver(std::function<void(const SDL_Event&)>)`
    and `bool RemoveSdlEventObserver(token)`. One `SDL_AddEventWatch` inside
    rexruntime fans out to the registered observers. Observers are read-only
    (an event watch cannot consume or modify an event; its return value is
    ignored), run on whichever thread SDL pushes the event, must not block.
    Removal takes the registry lock exclusively, so no call is in flight once
    it returns. The SDL watch is installed once, outside the registry lock, and
    never removed (avoids a lock-order inversion with SDL's watch-list lock).
  - `CaptureSdlJoysticks(include_values)` / `QuerySdlJoystick(id, include_values)`:
    name, GUID string, VID/PID, SDL joystick type, gamepad flag, the SDK wheel
    admission rule (`IsSupportedWheel`), opened/haptic flags, and raw
    axis/button/hat values for joysticks the SDK driver already opened
    (`SDL_GetJoystickFromID`, no reference taken). Never opens or closes a
    device and never touches haptic state. Bounded to 16 devices, 32 axes,
    128 buttons, 8 hats.
  - `SDL_Event` comes from the SDL3 headers the SDK install already ships; the
    host uses it as a data type only.
  - Test: `tests/unit/ui/sdl_event_observer_test.cpp`, 2 Catch2 cases
    (20 assertions): dispatch/removal/token semantics and removal waiting for an
    in-flight dispatch.
- **Host:** `src/mcla_cursor_autohide_sdl.cpp` now registers through
  `rex::ui::AddSdlEventObserver`; its decision logic and
  `mcla_cursor_autohide_test` are unchanged. No host source calls an SDL
  function any more (checked below), so the linker no longer pulls
  `SDL3-static` objects into `mcla.exe` although the imported
  `rex::runtime` target still lists `SDL3::SDL3-static` in its
  `INTERFACE_LINK_LIBRARIES` (an SDK packaging choice; harmless while the host
  references no SDL symbol).

## Design

Pure policy, unit-tested: `src/mcla_diagnostic_trigger.h`.

- `ParseWheelButtons("L2,R2")`: two raw SDL joystick button indices, each `-1`
  (disabled) or `0..255`, no duplicates, no leading zeros or signs.
- `ButtonEdgeTrigger`: fires on the first press; repeated downs and other
  devices are ignored until the pressing device releases (one package per
  press, re-arm on release; device removal releases).
- `HoldComboTrigger`: per gamepad, Start and Back both held arms a deadline of
  `hold_ms`; `Poll(now)` fires once at the deadline; releasing either button
  disarms or re-arms. Start on one pad plus Back on another never combines; a
  second pad cannot steal an armed combo.
- `ActiveDeviceTracker`: host-observed latest meaningful input device
  (button press, hat, axis delta >= 4096 with the M9-009 `PadActivityFilter`)
  and a 16-entry ring of transitions (timestamp from the SDL event, from, to,
  joystick/gamepad source) plus a total count.
- `BuildTriggerJson` / `BuildWheelStateJson` and SDK log-line classification.

SDL adapter: `src/mcla_diagnostic_triggers_sdl.{h,cpp}`.

- The observer (any SDL thread) only reads the event, updates the activity
  tracker under a host mutex, and posts relevant button events (L2/R2 indices,
  every raw button in probe mode, gamepad Start/Back, device removal) to the UI
  thread with `CallInUIThreadDeferred`. It never calls SDL or the window.
- UI thread: device classification through `rex::ui::QuerySdlJoystick`
  (cached per instance ID, dropped on removal). L2/R2 fire only for devices the
  SDK admits as wheels. Start+Back are ignored for wheels (a T300 that SDL
  also maps as a gamepad must not produce `pad`).
- A small timer thread wakes at the armed hold deadline and posts `Poll` to the
  UI thread, so the pad trigger fires while the buttons are still held.
- `MclaApp::OnDiagnosticTrigger` builds the trigger; for wheel L2 it captures
  the wheel state on the UI thread at press time and calls the shared
  `RequestLiveSnapshot`.

Package contents:

- `manifest.json` and `state.json` gain `"trigger": {"category": ..., "source": ...}`.
  `reason` stays equal to the source, so F10 packages keep `reason: "f10"`.
  Non-trigger packages (`probe`, `unexpected-guest-exit`) record their reason
  as the source and category `general`.
- Wheel packages add `"wheel"` (schema `mcla-wheel-state-v1`) to `state.json`:
  configured L2/R2 indices, trigger device instance ID, every SDL joystick
  (name, GUID, VID/PID, type, gamepad/wheel/opened/haptic flags, raw axes,
  buttons, hats), host latest-active device and transition ring, and three SDK
  log sections taken by the package worker from the captured log: active
  controller (`SDL_ACTIVE_CONTROLLER`), force feedback (`SDL_WHEEL_FFB_*`,
  `SDL wheel FFB ... failed`, unsupported-effect and rumble lines) and device
  (`SDL_WHEEL_CONNECTED`, removals, gamepad add, hotplug/XInput markers);
  newest 32 lines each, 512 bytes per line, sanitised like the log tail.
- The diagnostics export (`mcla_diagnostic_export_policy.h`) now maps the
  reasons `wheel-r2`, `wheel-l2` and `pad` to themselves instead of `other`.
  The export does not copy the wheel section.

Launcher (minimal change): `report::Package` gains `category`; after the
session `LauncherController::State::Poll` reads each completed live package's
`manifest.json` (bounded 64 KiB) through `report::ManifestCategory`, which
accepts only `wheel` inside the `trigger` object and otherwise reports
`general` (packages from before M9-010 have no trigger). `report.txt` lists
each package as `live/<name>  general|wheel` or `crash/<name>  crash` under a
one-line legend; the prompt adds the wheel count, for example
`3 reports were recorded in this session (1 crash, 1 wheel report).`
Package discovery (`PackageNames(..., L"live")`) is unchanged.

## Cvars (`MCLA Diagnostics`, init-only)

| Cvar | Default | Meaning |
|---|---|---|
| `mcla_controller_diagnostic_triggers` | `true` | Enable wheel/pad triggers (requires `mcla_diagnostics_enabled`) |
| `mcla_wheel_diagnostic_buttons` | `"8,9"` | Raw L2,R2 button indices of the wheel; `-1` disables one; invalid text logs a line and falls back to `8,9` |
| `mcla_wheel_button_probe` | `false` | Log `MCLA_WHEEL_BUTTON_PROBE v=1 instance= wheel= button= name=` for every raw joystick button press |
| `mcla_pad_diagnostic_hold_ms` | `1000` | Start+Back hold time, range 500..5000 |

**T300 L2/R2 indices 8 and 9 are unverified.** Basis: M6-015 records that the
T300 reports 13 buttons and that L2/R2 are on the rim but unmapped; the SDK
T300 layout (`wheel_mapping.h`, cvars `wheel_button_*`) maps 0-7 and 10-12 and
was physically verified for every mapped control, which leaves 8 and 9; the
usual Thrustmaster DirectInput order puts L2 before R2. Other wheels: start
with `mcla_wheel_button_probe=true`, press L2 and R2, read the indices from the
log, then set `mcla_wheel_diagnostic_buttons`.

## Not done, and why

- **Start/Back are not suppressed to the guest.** The SDK input driver keeps
  its controller state private and reads gamepad buttons from its own SDL
  event watch; the only SDL-level way to hide them is a global
  `SDL_SetEventFilter`, which would replace any existing filter, would have to
  delay every Start/Back press before knowing whether a combo follows, and
  would not change SDL's internal gamepad state. Not clean, so not done. The
  game's pause menu may therefore open while Start+Back is held; the tester
  guide says so. Clean alternative (needs an SDK change): a host-provided
  button mask consulted in `SDLInputDriver::GetState` while the host reports a
  combo in progress.
- **SDK force-feedback effect table and active-controller index are not read
  directly.** They are private members of `SDLInputDriver` with no accessor.
  The wheel section records the SDK's own telemetry lines instead
  (`SDL_ACTIVE_CONTROLLER`, `SDL_WHEEL_FFB_EFFECT` create/update/start/stop/
  resume/defer/drop, device-control, device-info, summaries). Opening a second
  SDL haptic handle to query effect status was rejected: it could disturb the
  very FFB lifecycle KI-022 is about. Exposing the driver state would be a
  further SDK change.
- Captured-log bound: the SDK lines come from the in-memory SDK log capture
  (`rex::LogCaptureSink`, the newest 2048 entries of all categories); in a
  long session older transitions are only in the rotating runtime journal.

## Files

SDK (`private/l39-sdk-src`, commit `9ad1c11`): `include/rex/ui/sdl_event_observer.h`,
`src/ui/sdl_event_observer.cpp`, `src/ui/CMakeLists.txt`,
`tests/unit/ui/sdl_event_observer_test.cpp`, `tests/unit/CMakeLists.txt`.

Host: `src/mcla_diagnostic_trigger.h` (new), `src/mcla_diagnostic_triggers_sdl.{h,cpp}`
(new), `src/mcla_cursor_autohide_sdl.cpp`, `src/mcla_app.{h,cpp}`,
`src/mcla_diagnostics.{h,cpp}`, `src/mcla_session_report.h`,
`src/mcla_launcher_controller_win.cpp`, `src/mcla_diagnostic_export_policy.h`,
`tests/mcla_diagnostic_trigger_test.cpp` (new), `tests/mcla_session_report_test.cpp`,
`tests/mcla_diagnostic_export_policy_test.cpp`, `CMakeLists.txt`,
`docs/volunteer-tester-guide.txt`.

## Build and test results (2026-10-29)

Toolchain: VS 2022 Build Tools (CMake/Ninja/MSVC libraries) + LLVM 20.1.8
clang++ and llvm-rc, as l38-build and l39-sdk-b.

SDK (`private/l39-work/l39_sdk_build.ps1 build`, incremental, Release, installed
to `private/l39-sdk-install`): rc 0; no warning from the new files (the log's
10 warning lines are the pre-existing `-gcodeview` / `rex_app.h` unused-parameter
/ `init_command.cpp` ones). `l39_sdk_test.ps1`: CTest **1869/1869 passed**
(baseline 1867 + the 2 new observer cases); `[hotplug]` 35 assertions / 3
cases, `[input]` 404 / 36, `[sdl_event_observer]` 20 / 2, all passed. The
installed package still reports version `0.20.0.3-dev.g259d0b2` (computed at
SDK configure time); the binaries contain commit `9ad1c11`.
`rexruntime.dll` 10,964,480 bytes, SHA-256
`F761FB6BD5ECDAD4E0518915AC2FA31838B1F7443A7C1616BD3934AB9606F045`.

Host: new build directory `private/l39-triggers-build`, configured like
`private/l38-build` (Ninja, Release, clang++, llvm-rc, `MCLA_TEST_BUILD_EXPIRY=ON`,
`MCLA_RAW_TIMER_DEFAULT_ON=ON`, `MCLA_BUILD_IMAGE_CAPTURE_TESTS=ON`,
`MCLA_CPPWINRT_INCLUDE_DIR` = Windows SDK 10.0.26100.0 cppwinrt,
`-ffile-prefix-map` for the worktree and the SDK install) plus
`MCLA_BUILD_DIAGNOSTIC_TESTS=ON`, `CMAKE_PREFIX_PATH`/`rexglue_DIR` =
`private/l39-sdk-install`, `REXSDK_VERSION` empty (the SDK is past the
0.20.0.2 pin). `generated/default` (67 files) was copied from the M9 primary
worktree and `third_party/rexglue-sdk/thirdparty/inja/third_party/include`
(nlohmann/json) from `private/l34-src`; both are untracked inputs. Build of
`mcla`, `mcla_crash_handler`, `mcla_internal_launcher` and 30 test targets:
rc 0, **0 compiler warnings**.

| Artifact | Bytes | SHA-256 |
|---|---|---|
| `mcla.exe` | 52,939,776 | `C0B5BDFE4F4DD1A2C953F6ADA0BE6714F14894BBC82BF438B317D3383050A5BF` |
| `MCLA-Launcher.exe` | 1,303,552 | `774A8A0DF7DD9E30722830D79B5D4C128A497BA2C28593666F90D9298EE2C01D` |
| `mcla_crash_handler.exe` | 194,560 | `80074262FFE753223A87ABF7A5E60060B4A96097D0D2DCD7D9355E1AF598B08F` |

SDL footprint check (M9-009 correction):

| `mcla.exe` | Bytes | `SDL.app.metadata` | `SDL_VIDEO_DRIVER` |
|---|---|---|---|
| l38-build (before M9-009) | 52,828,160 | absent | absent |
| l39-ki040-build (M9-009 direct SDL calls) | 55,214,080 | present | present |
| l39-triggers-build (this task) | 52,939,776 | absent | absent |

The new `mcla.exe` imports `AddSdlEventObserver`, `RemoveSdlEventObserver`,
`CaptureSdlJoysticks` and `QuerySdlJoystick` from `rexruntime.dll`. **It needs
the l39 SDK `rexruntime.dll`**; it will not start against an older one.

Unit tests (`m9010_build.ps1 test`, all rc 0, 30/30 executables):

| Test | Before (2dfd6b1) | After |
|---|---|---|
| `mcla_diagnostic_trigger_test` (new) | - | 87 checks |
| `mcla_session_report_test` | 17 | 30 |
| `mcla_diagnostic_export_policy_test` | 538 | 562 |
| `mcla_launcher_controller_win_test` | 343 | 343 (fixture now has a wheel package and a manifest-less package; prompt and report lines asserted) |
| `mcla_cursor_autohide_test` | 266 | 266 (logic unchanged) |
| `mcla_diagnostic_hotkey_test` | 8 | 8 |

Unchanged suites, all 0 failures: frame_signal 24, diagnostic_image_protocol
119, readback_cost_sample 133, portable_path_policy 52, launcher_profile_policy
118, launcher_library_codec 315, launcher_preferences 2088,
managed_config_policy 50, portable_adapter_policy 78 (with
`config/mcla.toml.example`), diagnostic_export_win 105 (1 skip),
diagnostic_export_ui_win 249, launcher_import_win 31, launcher_fs_win 76,
launcher_ui_win 971, launcher_process_win 41, launcher_store_win 125 (1 skip),
launcher_build_win 109, launcher_snapshot_win 26, launcher_game_win 70,
launcher_import_policy 72, launcher_capabilities 200, build_lease_policy 1438,
build_lease_network_win 26 (no network request), build_lease_runtime_win 69.
"Before" counts of the two changed pure tests were taken by compiling the
2dfd6b1 versions with the same clang++. `ast-grep scan` (host and the new SDK
files) clean; `ast-grep test` 3/3.

Spark: not used. The work was a bounded C++ implementation whose checks are
the compiler and the unit tests above; no extraction or log-triage step was
large enough to offload.

## Owner physical verification (pending)

Use a non-MAIN profile with a build that ships the l39 SDK `rexruntime.dll`.

1. **M9-009 again** (it never worked before this fix): pad/wheel input hides
   the cursor while focused; mouse movement shows it.
2. Wheel R2: one press = one package, `RECORDING...` status prefixed
   `WHEEL R2`; holding R2 does not create more; release and press again = a
   second package. Guest input unchanged (R2 does nothing in game).
3. Wheel L2: package with `"trigger": {"category": "wheel", "source": "wheel-l2"}`
   and a `wheel` section in `state.json` naming the T300 with plausible raw
   axes/buttons (button 8 = 1 while pressed). If L2/R2 are swapped or dead,
   run once with `mcla_wheel_button_probe=true`, read
   `MCLA_WHEEL_BUTTON_PROBE` lines and set `mcla_wheel_diagnostic_buttons`.
4. KI-022 attempt: when centering disappears, press L2 and check that the
   wheel section contains the SDK `SDL_ACTIVE_CONTROLLER` and
   `SDL_WHEEL_FFB_EFFECT` lines around it.
5. Gamepad: tap Start+Back (no package); hold about 1 s (one package,
   `PAD START+BACK`); keep holding (no second); release and hold again (second).
   Record whether the pause menu opened.
6. After the session: the launcher prompt counts the wheel report and
   `report.txt` lists each package with `general`, `wheel` or `crash`.
7. F10 still works and still records `reason: "f10"`.

## Limits and risks

- Nothing here is physically verified; T300 indices 8/9 are inferred.
- The SDK force-feedback table and active-controller index are recorded only
  through the SDK's log lines (newest 2048 captured log entries).
- Start/Back reach the guest while the combo is held (pause menu may open).
- Joystick events of every opened device reach the observer; L2/R2 are honoured
  only for devices the SDK admits as wheels, so a Steam Input mirror of the
  wheel that SDL does not classify as a wheel will not trigger.
- The host-observed transition ring counts the host's own activity rule; it can
  differ from the SDK's routing (that is why both are recorded).
- The SDK API is a new local SDK patch on top of KI-040 and is not upstream.
