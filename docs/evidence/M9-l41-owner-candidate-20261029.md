# M9 l41 owner candidate: merged host tree dd7cca7 + l41 SDK (2026-10-29)

**Status:** built, unit tested and binary-checked; owner input folder and
prepare script written. The prepare script was only parse-checked: no ticket
was signed, nothing was staged, `owner-admitted-builds.txt` was not written,
and no `mcla.exe`, launcher or game was started. This is not a physical PASS
of anything.

l41 is the owner's next internal candidate: plain **V1** ticket (owner rule
2026-10-29: no kill switch, no online check), no tester guide, nothing
published.

## Inputs

| Item | Identity |
|---|---|
| Host tree | `dd7cca7` on `claude/admiring-jang-24d485` (merge of KI-037 photo-capture pipeline wait into the M9 tree carrying M9-007, KI-035, KI-026, M9-009, M9-010, KI-044, KI-045 and per-package `report.txt`); no source change was needed for the merge |
| SDK install | `private/l41-sdk-install`, CMake package `ReXGlue SDK 0.20.0.6-dev.g8b10d62` |
| SDK commits (on `a239b83`) | `259d0b2` input(sdl) KI-040 XInput re-scan; `9ad1c11` ui(sdl) SDL event observer and joystick snapshot API; `d968f59` kernel(memory) KI-035 failed-allocation log; `8b10d62` gpu(d3d12) KI-037 bounded pipeline wait (`d3d12_async_pipeline_wait`) |
| Generated code | worktree `generated/default` and the inja/nlohmann include copy (untracked build inputs, as in the earlier builds) |

## Build

Script `private/l39-work/l41_build_merged.ps1` (copy of `l41_host_build.ps1`
with the worktree and build directory changed). Toolchain: VS 2022 Build
Tools (CMake, Ninja, MSVC libraries), LLVM clang++ 20.1.8, llvm-rc, Windows
PowerShell 5.1.

New directory `private/l41-build`, cache options identical to
`private/l41-photo-build`: Ninja, Release, `rexglue_DIR`/`CMAKE_PREFIX_PATH` =
`private/l41-sdk-install`, `REXSDK_DIR`/`REXSDK_VERSION` empty,
`MCLA_TEST_BUILD_EXPIRY=ON`, `MCLA_RAW_TIMER_DEFAULT_ON=ON`,
`MCLA_BUILD_IMAGE_CAPTURE_TESTS=ON`, `MCLA_BUILD_DIAGNOSTIC_TESTS=ON`,
`MCLA_EXPERIMENTAL_F10_WGC=OFF`, Windows SDK 10.0.26100.0 cppwinrt,
`-ffile-prefix-map` for this worktree (`=.`) and the l41 SDK (`=sdk`).

| Step | Result |
|---|---|
| Configure | rc 0, found `ReXGlue SDK 0.20.0.6-dev.g8b10d62` |
| Build `mcla`, `mcla_crash_handler`, `mcla_diagnostic_image_helper`, `mcla_diagnostic_image_fake_helper`, `mcla_internal_launcher` and 35 test targets | 219/219 steps, rc 0, **0 compiler warnings**, 0 errors |
| `ast-grep scan` (0.50.0, whole worktree) | no findings, exit 0 |
| `ast-grep test` | 3 passed, 0 failed |

## Tests (35 executables, all rc 0, 0 failures)

| Suite | Checks | Note |
|---|---:|---|
| `mcla_launcher_capabilities_test` | 238 | expected 238 |
| `mcla_launcher_controller_win_test` | 367 | expected 367; copies itself as a synthetic `mcla.exe` into a temp fixture |
| `mcla_launcher_ui_win_test` | 983 | expected 983 |
| `mcla_session_report_test` | 39 | expected 39 |
| `mcla_diagnostic_trigger_test` | 102 | expected 102 |
| `mcla_diagnostic_export_policy_test` | 585 | expected 585 |
| `mcla_menu_state_test` | 447 | expected 447 |
| `mcla_guest_sampler_test` | 9 | expected 9 |
| `mcla_photo_capture_test` | 204 | expected 204 |
| `mcla_cursor_autohide_test` | 266 | expected 266 |
| `mcla_diagnostic_image_client_test` | 51 | run with `mcla_diagnostic_image_fake_helper.exe` and its SHA-256 (the test's helper modes are fake-child modes; no capture) |
| `mcla_portable_adapter_policy_test` | 78 | run with `config/mcla.toml.example` |
| `mcla_diagnostic_export_win_test` | 105, 1 skip | symlink fixture |
| `mcla_launcher_store_win_test` | 125, 1 skip | |
| `mcla_diagnostic_export_ui_win_test` | 249 | |
| `mcla_diagnostic_hotkey_test` | 8 | |
| `mcla_diagnostic_image_protocol_test` | 119 | |
| `mcla_frame_audit_rearm_test` | 1809 | |
| `mcla_frame_signal_test` | 24 | |
| `mcla_readback_cost_sample_test` | 133 | |
| `mcla_managed_config_policy_test` | 50 | |
| `mcla_portable_path_policy_test` | 52 | |
| `mcla_build_lease_policy_test` | 1438 | |
| `mcla_build_lease_network_win_test` | 26 | no network request executed |
| `mcla_build_lease_runtime_win_test` | 69 | |
| `mcla_launcher_build_win_test` | 109 | |
| `mcla_launcher_fs_win_test` | 76 | |
| `mcla_launcher_game_win_test` | 70 | |
| `mcla_launcher_import_policy_test` | 72 | |
| `mcla_launcher_import_win_test` | 31 | |
| `mcla_launcher_library_codec_test` | 315 | |
| `mcla_launcher_preferences_test` | 2088 | |
| `mcla_launcher_process_win_test` | 41 | |
| `mcla_launcher_profile_policy_test` | 118 | |
| `mcla_launcher_snapshot_win_test` | 26 | |

Logs: `private/l39-work/l41_merged_configure.log`, `l41_merged_build.log`,
`l41_merged_tests.log`.

## Binary checks

- `mcla.exe`: `SDL.app.metadata` 0 occurrences, `SDL_VIDEO_DRIVER` 0; imports
  `AddSdlEventObserver`, `RemoveSdlEventObserver`, `CaptureSdlJoysticks` and
  `QuerySdlJoystick` from `rexruntime.dll` (`llvm-objdump -p`).
- Path strings, ASCII and UTF-16LE, case-insensitive: `C:\BDU`, `C:/BDU`,
  `\Users\`, `/Users/`, `mcla-recomp`, `worktrees`, `l41-sdk`, `l34-src`:
  **none** in any of the seven files. (An extra account-name probe matched
  only colour names and `...Range` symbols in `rexruntime.dll`, false
  positives, also present in the l40 runtime.)

## Artifacts (l41)

| File | Source | Bytes | SHA-256 |
|---|---|---:|---|
| `mcla.exe` | `private/l41-build` | 53,069,824 | `85F69D263AAE942EF60D7B233BFEADFF4AB042D5DBFEE43A2C7EF3C17E6DDB78` |
| `MCLA-Launcher.exe` | `private/l41-build` | 1,335,296 | `F9DC11B33FF8CBD6D1B7F48EF75B5EDF75F73CC1F1AAFCF68A87C2FC1ED8BA5F` |
| `mcla_crash_handler.exe` | `private/l41-build` | 211,968 | `E45945297AE87F295BFCD4DF77A90FBC10BAFD02AFD209284B94C104C23A29A0` |
| `mcla_diagnostic_image_helper.exe` | `private/l41-build` | 60,928 | `11482F3B0CF9DEFCE03E142145CC6FE7035A957736A32073ECCAB424A4523B6A` |
| `rexruntime.dll` | `private/l41-sdk-install/bin` | 10,965,504 | `3A7A346A06DFBA35EA223CAE41B14DD0258AC50AF09BF6F375C3F59A5304A7C0` |
| `rexgpu-xenos.dll` | `private/l41-sdk-install/bin` | 2,997,248 | `F3CECECC2962692520E4C5FF5F8BB89E46134C9B707898AE902834295B572E6D` |
| `TracyClient.dll` | `private/l41-sdk-install/bin` | 246,784 | `C2CC7F09D1DE4EDB4438CC332EEA3E3EB07750E21451041D4ABDA0E3A25D008C` |

The two SDK DLLs equal the `private/l41-photo-build` evidence values. The
launcher link is not bit-reproducible (earlier evidence), so its hash
identifies this binary only.

## Owner input folder

`private/l41-int-input` holds exactly the seven files above, same layout as
`private/l40-int-input` (flat folder). `mcla-test-build.ticket` is absent on
purpose: the prepare script creates it there.

## Prepare script

`private/l41-internal-prepare.ps1` (PowerShell 7; parse check with
`private/tools/pwsh/pwsh.exe` 7.4: 0 errors). Not run. In order it:

1. refuses if `private/l41-internal` or the input ticket already exists, or if
   `%LocalAppData%\MCLA-R-Launcher-v1\preferences.dat` is absent (a settings
   directory with only the admission file is refused by the launcher as a
   nonempty store); checks the pinned `mcla.exe` and `rexruntime.dll` hashes;
2. signs a plain V1 ticket with `scripts/New-MclaTestBuildTicket.ps1` (no
   `-KillSwitch`) and asserts the first line is `MCLA-TEST-BUILD-V1`;
3. stages to `private/l41-internal` with `scripts/Stage-MclaInternalLauncher.ps1`
   (CRT from `private/l12`, no tester guide);
4. runs `scripts/Test-MclaInternalLauncherStaging.ps1` (synthetic fixture in
   `%TEMP%`, launches nothing);
5. KI-045: hashes `private/l41-internal/build/mcla-build.manifest`
   (SHA-256, uppercase hex), checks it equals `build_manifest_sha256` in
   `candidate-receipt.json`, then writes
   `%LocalAppData%\MCLA-R-Launcher-v1\owner-admitted-builds.txt`: header
   `MCLA-OWNER-ADMITTED-BUILDS-V1`, LF-only ASCII with final LF, valid
   existing rows kept (malformed or duplicate rows dropped with a warning; an
   unknown header or >16 KiB file is refused), plus `l39-internal`
   (`D0F1B45F...0265`), `l40-internal` (`C0BB61C8...2094`) and `l41-internal`
   (the new fingerprint) when not already listed; at most 64 rows. Written to
   a unique temp file in the same directory, flushed, renamed over the
   target, then read back and compared.

It publishes nothing and launches no game or launcher binary.

## Owner steps

1. Start any MCLA launcher once on this PC if it never ran (so
   `preferences.dat` exists), then close it.
2. Run
   `private\tools\pwsh\pwsh.exe -NoProfile -File private\l41-internal-prepare.ps1`
   (full path). Note the printed l41 fingerprint and the admission list.
3. Start `private\l41-internal\MCLA-Launcher.exe`, Browse to
   `private\l41-internal\build`, press CHECK SETUP. Use a non-MAIN profile.

## Owner checks on l41

Read the full `runtime-*.log` of each session (not only `log-tail.txt`).

| Item | Do | Expect (exact text) |
|---|---|---|
| KI-044 | Browse + CHECK SETUP; close and reopen the launcher | status `Setup saved.`; the Build folder is still `private\l41-internal\build` after reopening |
| KI-045 | choose 3840x2160, then Unlocked FPS; CHECK SETUP | Ready with `Owner-admitted build (owner list), not a tested build.`; `capabilities.txt` of that session ends with `admission owner-allowlist`. Stock 720p/1440p at 30 FPS: no admission key |
| M9-007 | Unlocked FPS, D3D12: drive 10 s, pause menu 10 s, map/Arcade menu 10 s, resume, race start, traffic light | about 30 fps in the paused menus; log `MCLA menu state: game paused (sim N local N), UI cap forced, mcla_ui_cap_draws_low V` with `MCLA UI cap: on`, then `MCLA menu state: game running ...` and `MCLA UI cap: off` after resume; 55-74 fps driving, traffic light uncapped; after exit `mcla.toml` does not hold `mcla_ui_cap_draws_low = 100000` |
| KI-038/KI-032 | same copied save, Selma/Highland intersection, stationary 60 s: first at 2160p, then at 1440p | F10 at the sign at about 5 s and 60 s in each; note whether signs are legible and whether they change over time |
| KI-037 | on this fresh build (empty pipeline cache), D3D12, defaults: Photo Mode, take the **first** photo, then a second | first photo correct (no black top, no row bands; short shutter pause acceptable); log `MCLA photo capture: window open, pipeline wait on`, then `MCLA photo capture: window closed after N frames (T ms), pipeline wait off` (or `window timed out`); on the first shot one `D3D12 async pipeline wait: episode ended, N waits ...` with N > 0. Optional control: `--mcla_photo_pipeline_wait=false` on another fresh build should reproduce the broken first photo |
| report.txt | record F10, wheel R2, wheel L2, pad Start+Back (hold about 1 s) | each `live\...` package folder has `report.txt` with `package`, `kind`, `created_utc`, `mcla_version`, `category`, `source` (`f10`, `wheel-r2`, `wheel-l2`, `pad`), `wheel`, `window`; the wheel L2 one has `wheel <name> \| VID 0xNNNN PID 0xNNNN \| GUID <guid>`, others `not-recorded (wheel L2 records the wheel)`; after the game closes the pending `launcher_session` line is replaced by `build`, `mode`, `render_target`, `session_started_utc`, `session_ended_utc`, `exit_code`; a file you already answered (pending line removed) is not touched |
| M9-010 | wheel R2 once, hold, release, again; wheel L2; pad tap vs hold Start+Back | R2: one package per press, status `WHEEL R2 ... RECORDING...`; L2: `state.json` `"trigger": {"category": "wheel", "source": "wheel-l2"}` and a `wheel` section naming the T300 (button 8 = 1 while pressed); pad: tap = no package, hold = one `PAD START+BACK` package, keep holding = no second. If L2/R2 are wrong, run once with `--mcla_wheel_button_probe=true` and read `MCLA_WHEEL_BUTTON_PROBE v=1 ...` lines |
| M9-009 | focused window: pad/wheel input, then mouse | pad/wheel input hides the cursor; mouse movement shows it |
| KI-040 | 8BitDo pad off 10 s then on; off 30 s; dongle unplug 5 s | `SDL gamepad removed from physical slot 0`, then `OnControllerDeviceAdded: ... Backend(xinput), Path("XInput#M")`, then `SDL_ACTIVE_CONTROLLER ... reason=first-connected`; input works |
| KI-035/KI-026 F10 data | one F10 while driving; wait 15 s before closing | runtime log `MCLA_GUEST_SAMPLE v=1 start ...`, per-thread `MCLA_GUEST_SAMPLE v=1 thread=...` lines, `MCLA_GUEST_SAMPLE v=1 end stopped=0`; `state.json` has `guest_ki026` with `"complete": true` in gameplay |
| F10 | F10 | still records `reason: "f10"` |

## Limits

- Nothing was launched: no `mcla.exe`, launcher, game or image helper outside
  the unit test (the image client test started only the fake helper).
- The prepare script was not run; no ticket, staging, receipt or admission
  file exists yet. The l41 build fingerprint is unknown until it runs.
- T300 L2/R2 indices 8/9 are unverified.
- Start/Back reach the guest while the combo is held (the pause menu may
  open).
- Everything here is unpinned: the SDK commits are local, the host branch is
  not merged to main, and no physical result exists for any feature in the
  table.
- The admission file is per-user owner convenience, not a security boundary.

Spark: no job was sent. The work was a deterministic build, unit-test run,
hash and string check; validating a model answer would cost more than the
checks themselves.
