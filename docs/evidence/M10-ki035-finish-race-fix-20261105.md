# KI-035 finish-race fix: host hook on the warper's camera-finished callback (l69, 2026-11-05)

Host-only patch by an Opus subagent for KI-035 (garage freeze: "the game never enters the garage;
the transition shot stays on screen"). Implements the fix proposed in part 2 of
[M10-l68-pc-ab-ki035-root-cause-20261105.md](M10-l68-pc-ab-ki035-root-cause-20261105.md).
Built and unit-tested only; **not run in game, no physical PASS**. Offsets and the state-20 wait
are from [M10-ki035-warp-probe-20261104.md](M10-ki035-warp-probe-20261104.md).

Base: main `a83baaa` (tracked `src`, `tests`, `CMakeLists.txt` unchanged against HEAD). SDK:
`private/l67-sdk-install` (v0.20.0.8, the l67/l68 SDK). No SDK change.

## 1. Mechanism (recap)

Each UI frame (`sub_821FC588`, `mcla_recomp.7.cpp`) first updates the UI transition list
(`sub_822247A8` -> `sub_82221DB0`) and then the warper (`sub_8221BE98`), both with the same dt.
State 20 (`loc_8221C120`) leaves only while the warp camera is ready (vfunc +92, bit 0x80 of +16)
and `float(float(time + dt) + 0.0001) >= duration` with the **current** dt. When dt grows on the
last frame of the 3 s zoom-in, the list pass advances the camera past its end and finishes it
first (SetFinished `sub_82228858` clears the ready bit; the stop path resets time to 0). The
warper's finish callback `sub_8221B8C8` then runs while the warper is still in state 20 with
+276 = 0: it clears +300, sets +273, but skips the finalise. Ready never comes back, and state 20
has no other exit: the freeze. On the console (fixed dt) the window is empty; on PC with Unlocked
timing it is a few percent per cinematic warp.

## 2. What the original callback does (generated code)

All citations `generated/default/mcla_recomp.8.cpp` unless noted. `sub_8221B8C8` (line 9510) is
reached only through the function table (`mcla_init.cpp` 4349 `{ 0x8221B8C8, sub_8221B8C8 }`,
`mcla_register.cpp` 4341); there is no direct generated call. Registers: `r3` = warper
(`mr r31,r3`), `r4` = event source, `r5` = the finished transition.

| address | generated code | meaning |
|---|---|---|
| `0x8221B8DC..E4` | `lwz r11,256(r31)`; `cmplw cr6,r11,r4`; `bne` -> `loc_8221B9A0` | event source must be warper+256 |
| `0x8221B8E8..F0` | `lwz r11,68(r31)`; `cmplwi cr6,r11,0`; `beq` -> exit | a request must be present |
| `0x8221B8F4..FC` | `lwz r11,108(r11)`; `cmplw cr6,r5,r11`; `bne` -> exit | `r5` must be request+108 (the request's camera) |
| `0x8221B900..924` | `lwz r11,300(r31)`; `cmplw cr6,r5,r11`; `bne loc_8221B928`; `bl 0x82224A28`; `stw 0,300(r31)` | only when `r5 == warper+300`: release and clear +300 |
| `0x8221B928..968` | `lbz r11,7(req)`; states 17/12/18 -> `sub_82204558`, else `sub_82204480` | camera pause at end of warp, only when req+7 (0 in both freezes) |
| `0x8221B984..994` | `lbz r11,276(r31)`; `beq`; `bl 0x8221B790` | finalise only when +276 != 0 |
| `0x8221B998..99C` | `li r11,1`; `stb r11,273(r31)` | always +273 = 1 |

State-20 exit in `sub_8221BE98` (prologue `fmr f31,f1`, `li r28,1`, `li r26,0`), after the wait
compare `fcmpu cr6,f12,f30; blt` at `0x8221C1C0..C4`:

| address | generated code |
|---|---|
| `0x8221C1C8` | `lis r11,-32121` |
| `0x8221C1CC` | `li r10,29` |
| `0x8221C1D0` | `lwz r11,-10000(r11)` (pointer at `0x8286D8F0`) |
| `0x8221C1D4` | `stb r26,293(r11)` (= 0) |
| `0x8221C1D8` | `stb r28,276(r31)` (= 1) |
| `0x8221C1DC` | `stw r10,288(r31)` (= 29) |

The wait itself first requires req+3 and req+0 (`0x8221C15C..0x8221C180`) before it looks at the
camera.

## 3. The hook

`REX_EXTERN(sub_8221B8C8)` in `src/mcla_ki026_probe.cpp` (the former `MCLA_KI026_EDGE` line for
this function is replaced; the hook prints the same `MCLA_KI026_EDGE ... fn=Warper_OnCameraFinished`
line under `--mcla_ki026_probe`). Pure decision `mcla::warp::DecideFinish` in
`src/mcla_warp_probe.h`:

| inputs (all checked guest reads) | rule |
|---|---|
| `mcla_warp_finish_fix` (init-only bool, **default true**) | off -> original unchanged |
| every read succeeded; `r3 == *(0x8286D8D4) + 0x51240` (the UILogic warper) | else pass |
| `warper+288 == 20` | else pass (a healthy trip arrives in 29) |
| request (+68) != 0, `r5` != 0, `warper+256 == r4`, `r5 == request+108`, `r5 == warper+300` | all three original compares plus the +300 match, else pass |
| `request+0` and `request+3` non-zero | else `skip-request-pending` (pass; the handler would still wait for them) |
| `*(0x8286D8F0)+293`, `warper+276`, `warper+288` in a guest heap with read+write protection | else `refused-unwritable` (pass) |
| otherwise | **`finish-state20`** |

`finish-state20`: the hook stores exactly the handler's exit (`*(0x8286D8F0)+293 = 0`,
`warper+276 = 1`, `warper+288 = 29`), then calls `__imp__sub_8221B8C8`. The original then clears
+300, finds +276 = 1 and runs the finalise `sub_8221B790` (state 29, request recycled, flags
cleared), then +273 = 1: the same sequence as a healthy trip (handler exit, then finalise on the
camera's finish notification), without the dt look-ahead. In every other case only the original
runs. No guest code is called besides the original.

Logs:

- `MCLA_KI035_FINISH_RACE v=1 action=<finish-state20|skip-request-pending|refused-unwritable|observed-fix-off> fix= state= request= type= req0= req3= req7= camera= class= ready= time= duration= flags16= active_index= active_count= exit276= global= dt_update= last20_time= last20_dt= last20_duration= last20_ready= last20_due= last20_age_ms=`
  (WARN) whenever the race condition holds, also with the fix off (`observed-fix-off`, the control
  arm); at most 200 per process. After a `finish-state20`,
  `MCLA_KI035_FINISH_RACE_DONE v=1 state= request= exit276= notified273= camera300= complete=`
  shows the warper after the original (expected: state 29, request 0, +273 1, +300 0).
- `MCLA_KI035_FINISH_NOTIFY v=1 action= ...` (same fields plus the match flags, `source`,
  `wait_camera`) on **every** callback entry while `--mcla_warp_probe` is on, at most 2,000 per
  process: the field measurement the analysis asked for.
- `dt_update` is the dt of the last `sub_8221BE98` call (recorded on every call). `last20_*` is
  the last state-20 sample after the stock update (recorded while the warp probe or the watchdog is
  on): its time, dt, duration, ready, `last20_due` = the generated compare
  (`mcla::warp::State20ExitDue`), and its age. The race shows `last20_due=0` with `last20_time`
  close to `last20_duration` and a notification a frame later; a menu cancel (candidate b') would
  show `last20_time` well below the duration.
- F10 `state.json` `guest_ki026`: `warp_finish_fix`, `warp_finish_races`, `warp_finish_fixes`.

Owner key `mcla_warp_finish_fix_off=true` (startup phase) writes `false` to `mcla_warp_finish_fix`;
`=false` writes nothing. The cvar is in `kNeverWrittenCvars` (`src/mcla_m10_defaults.h`). The
state-20 watchdog (`mcla_warp_watchdog_seconds`, default 0) is unchanged and stays a backstop.

## 4. Tests

- `tests/mcla_warp_probe_test.cpp` (5,936 checks; was 5,846):
  - `TestFinishDecision`: the l68 race sample (request `BED351C0`, Parallel camera `BF1F0BF0`,
    state 20, req0/req3 1) gives `finish-state20`; fix off gives pass; every state 0..40 other
    than 20 passes (the healthy trip arrives in 29); all eight combinations of the three matches
    (r4 / request+108 / warper+300) finish only when all match; no request, null camera,
    incomplete reads, a non-UILogic warper pass; req+0 or req+3 clear gives
    `skip-request-pending` (also when unwritable; fix off passes); unwritable gives
    `refused-unwritable`; a default-constructed sample passes (static_assert).
  - `TestState20ExitDue`: the generated compare with float rounding: healthy l68 exit (3.0210 +
    dt vs 3.0260), one frame short waits, epsilon, NaN exits (unordered compare, as the guest),
    and the race inequality `T + dt_N + K < D <= T + dt_(N+1)`.
  - `TestGeneratedMirror`: reads `generated/default/mcla_recomp.8.cpp` (path from CMake,
    `MCLA_WARP_GENERATED_RECOMP8`) and finds, in order, the lines the hook relies on, each built
    from the header constants: in `sub_8221B8C8` the prologue address, `mr r31,r3`, the +256/r4,
    +68, request+108/r5 and +300/r5 compares with their branches, the +300 clear, the +276 test,
    the `sub_8221B790` call and the +273 store; in `sub_8221BE98` the prologue `fmr f31,f1`,
    `li r28,1`, `li r26,0` before any state label, and in `loc_8221C120` (up to `loc_8221C1F0`) the
    +68/+296/req+3/req+0 loads, the +92/+592/+608 slots, the `fadds`/`lfs 18612`/`fadds`/`fcmpu`/
    `blt` compare, then `lis -32121` (checked to give `0x8286D8F0` with `-10000`), `li r10,29`,
    `lwz -10000`, `stb r26,293`, `stb r28,276`, `stw r10,288`. Mutation check: changing
    `kWarperFinishFlagField` to 277 or `kExitGlobalByteField` to 292 in a scratch copy of the
    header makes the test fail on the corresponding line.
- `tests/mcla_dev_overrides_test.cpp` (208 checks): 20 keys; `mcla_warp_finish_fix_off` found,
  `mcla_warp_finish_fix` / `_on` are not keys; startup phase; `=true` writes `false` to
  `mcla_warp_finish_fix`, `=false` writes nothing; the l68 arm (19 keys) still parses; the l69 arm
  (all 20 keys) parses within 1 KiB; the fix-off arm lines parse; the cvar name is an unknown key.
- `tests/mcla_m10_defaults_test.cpp` (2,477 checks): `mcla_warp_finish_fix` is never written and
  is not a default; the "every override key writes a default or a never-written cvar" rule holds
  for the new key.

## 5. Build and link check

Scripts and logs: `l69_host_build.ps1`, `l69_host_tests.ps1`, `l69-configure.log`,
`l69-build.log`, `l69-help.txt`, `l69-tests.log`, `l69-astgrep.log`, `l69-scan.log`,
`l69-arms-check.log` in the patch directory `build/` (copies in `private/l69-work`).

- Scratch tree `l69-host-src`: main `a83baaa` tracked build inputs (591 files, none dirty)
  + `generated/` + the SDK inja/xxHash headers, the 8 patched files overlaid (`make_src.py`).
- Configure rc 0, `rexglue_DIR` = `private/l67-sdk-install` (CMake prints the stale
  `Found ReXGlue SDK 0.20.0.7` metadata, as for l67/l68); the l68 owner options:
  `MCLA_TEST_BUILD_EXPIRY`, `MCLA_RAW_TIMER_DEFAULT_ON`, `MCLA_PERF_SPLIT_DEFAULT`,
  `MCLA_LIVERY_DIAG2_DEFAULT`, `MCLA_READBACK_SYNC_COMPOSITE_DEFAULT`, `MCLA_DEV_OVERRIDES`,
  `MCLA_PREWARM_SEED_DEFAULT`, `MCLA_M10_SPEEDUP_DEFAULTS`, image/diagnostic tests ON;
  `MCLA_EXPERIMENTAL_F10_WGC` OFF; `REXSDK_VERSION` empty; prefix maps; `/pdbaltpath`. Build dir
  `private/l69-host-build`.
- Build: 54 targets, 261/261 steps rc 0, **0 warnings**, 0 errors.
- Host tests: **49 executables, 49 pass** (first run, no rerun needed); the `cmake --target help`
  `*_test` list (49) equals the built `*_test.exe` set (49). The image-client and controller
  tests leave their owned fixtures under `%TEMP%` ("preserved"), as usual.
- ast-grep 0.50.0: `scan src tests` rc 0, `test` 3/3 rc 0.
- **Link check.** `mcla.exe` was relinked from the same objects with the identical ninja link
  command (VS 2022 dev shell, `CMakeFiles/mcla.rsp` rebuilt from `build.ninja`) plus `/MAP`, into
  a scratch directory: all seven sections are byte-identical to the built `mcla.exe` (only the
  header timestamp/debug-directory differ). The map has one definition named `sub_8221B8C8`, at
  `0x1400A3A30` from `mcla_ki026_probe.cpp.obj` (the override); the generated body
  `__imp__sub_8221B8C8` is at `0x14076D270` (`mcla_recomp.8.cpp.obj`). The generated code has no
  direct call of `sub_8221B8C8`; its callers reach it through `PPCFuncMappings`
  (`mcla_init.cpp`) and the registrar (`mcla_register.cpp`), both naming the symbol: the
  `PPCFuncMappings` entry for guest `0x8221B8C8` in the **built** `mcla.exe` holds `0x1400A3A30`
  (30,035 entries scanned), i.e. the override. `sub_8221BE98` likewise resolves to the
  `mcla_ki026_probe.cpp.obj` override.
- `mcla.exe` SHA-256 `2DC30A8678831C82ADBE615D00142F2D092F563BF5764A76373018CB882B1FA8`
  (53,416,448 bytes); strings `MCLA_KI035_FINISH_RACE v=1`, `MCLA_KI035_FINISH_RACE_DONE v=1`,
  `MCLA_KI035_FINISH_NOTIFY v=1`, `mcla_warp_finish_fix`, `mcla_warp_finish_fix_off`,
  `warp_finish_fixes` present, plus every l68 string; path scan: no build, user, temp or patch
  path (only the known opcode-byte and launcher `AppData`/`private/save-arc` hits). Not run.

Owner candidate input `private/l69-int-input` (four files from `l69-host-build`, three DLLs from
`l67-sdk-install/bin`, byte-identical to the build-dir copies and to l68):

| File | Bytes | SHA-256 |
|---|---|---|
| MCLA-Launcher.exe | 1454592 | 312175FED1BA9CCE8B1563CE0483DFC97BE2337483AF859C0A7166E44DA3EB57 |
| mcla.exe | 53416448 | 2DC30A8678831C82ADBE615D00142F2D092F563BF5764A76373018CB882B1FA8 |
| mcla_crash_handler.exe | 211968 | 95302FDFF609194E3B18D47FAC434C2BD9DCAE4BB2A8496E910C6B0714FD0368 |
| mcla_diagnostic_image_helper.exe | 60928 | BB056B522CE8AF6185AA9E24DD9735FB95F4A6552638C79C31BCC061E11042FC |
| rexgpu-xenos.dll | 3210240 | D812C51BD07B2235FD1DAC12BF0FFA27E850B657859AB5B7A7268FBD01487771 |
| rexruntime.dll | 11064320 | DC0950F1F84184C4F66DDBCF6CF48DAC8CA10612530A3E22B9C39D371103DCE6 |
| TracyClient.dll | 246784 | 70CA43BA133310E0DFA5A42E0222F24EBB5573D2205D3AED13DD2F2CFF0BC876 |

Prepare script `private/l69-internal-prepare.ps1` (SHA-256 `A057A5C0CCC120D9B8D63102156C0D93EE36C77E8B7B6CDEFB6D18446EE2D309`, ASCII CRLF, 0 parse
errors in Windows PowerShell 5.1 and PowerShell 7.4.22), generated from
`l68-internal-prepare.ps1` by `mk_prepare_l69.py` (every replacement count-checked): l69 pins,
plain V1 ticket, stage `private/l69-internal`, internal staging audit, KI-045 admission row
`l69-internal` (existing rows kept). Not run.

Arms (ASCII LF, all 20 keys explicit; base = the l68 PROBE arm read back from its file, plus
`mcla_warp_finish_fix_off=false`):

| File | Bytes | SHA-256 |
|---|---|---|
| `l69-arms/pc/fix/mcla-dev-overrides.txt` | 631 | BCB494E389407E59D9E093A9813105FE063050318ECB328A4442186BDA1A2DDE |
| `l69-arms/pc/fix-off/mcla-dev-overrides.txt` | 630 | 5B710467FBA2A386D70BA46426B45F631E7A3E16D9F515AE398A07FDF113080C |
| `l69-arm-fix.cmd` | 2686 | A1347C998185F45D140322B084C1899AA0AB6DDC670BAE35D57672EB62195A45 |
| `l69-arm-fix-off.cmd` | 2623 | 7B7ED44ABC0E04D6C615AF5565FB177C6FA7AC6D52BF097A544C03D1998664B2 |
| `l69-arm-clear.cmd` | 1719 | 84F8B95CAE8BBE516D709A28B0EBED0D4C12D44EC7A1FFF14D78A0761538B0ED |

The two arms differ only in `mcla_warp_finish_fix_off`; both keep `mcla_warp_probe=true` and
`mcla_warp_watchdog=false`. Parse check (the l68 checker compiled against the patched
`src/mcla_dev_overrides.h`): both LF files and their CRLF variants parse, 20 values, 0 ignored
lines, 0 mismatches, 651 bytes max (limit 1024). Each `.cmd` echo block is byte-equal to its arm
file; the helpers are ASCII CRLF (pattern of `l68-arm-*.cmd`: refuse while `mcla.exe` runs, write
only into an initialised `%LocalAppData%\MCLA-R-Launcher-v1`).

## 6. Owner repro (l69, PC)

Arms (`private\l69-arm-fix.cmd`, `private\l69-arm-fix-off.cmd`, `private\l69-arm-clear.cmd`;
files `private\l69-arms\pc\{fix,fix-off}\mcla-dev-overrides.txt`, every key explicit, base = the
l68 PROBE arm: warp probe on, watchdog off, soft cap on). Prepare once with
`private\l69-internal-prepare.ps1` (PowerShell 7), set the launcher Build folder to
`private\l69-internal\build`, CHECK SETUP.

1. Run `l69-arm-fix.cmd`. MAIN profile, 1080p Unlocked as on l67/l68.
2. At least **5 garage trips after a race**: finish a race (free roam event or freeway race),
   continue to free roam, open the pause menu, choose "go to garage", let the garage load, leave
   the garage, repeat. Alt-Tab on the result screen is optional. Normal driving between trips is
   fine; the race is more likely with jittery frame pacing (heavy scenes, long sessions).
3. Expected: **no freeze**. Whenever the race would have frozen the game, the log has a WARN
   `MCLA_KI035_FINISH_RACE v=1 action=finish-state20` followed by `MCLA_KI035_FINISH_RACE_DONE ...
   state=29 request=00000000` and the garage loads as usual (zoom-in shot ends, garage appears).
   Press F10 once inside the garage after such a line.
4. Then run `l69-arm-fix-off.cmd` and do one or two trips as the control. A freeze there is
   expected to be rare; if it happens, the log shows `action=observed-fix-off` at the moment the
   shot ended. Press F10, wait 10 s, F10 again, close the game.
5. Run `l69-arm-clear.cmd` at the end.

What to send back: the session folders (full runtime log `mcla.1.log`/`mcla.log`, F10 packages)
of both arms, zipped outside the saves folder, plus a note of how many trips were made per arm and
whether any froze. What is looked at: every `MCLA_KI035_FINISH_NOTIFY` line (action, state at
the notification, `last20_*`), any `MCLA_KI035_FINISH_RACE` WARN and its `_DONE` line, the
`MCLA_KI035_WARP_STATE` sequence 26 -> 20 -> 29 per trip, the F10 counters
`warp_finish_races`/`warp_finish_fixes`, and that later warps (next trips, events) still work.
No race in 5 trips is not evidence against the mechanism (a few percent per trip); the
`last20_*` fields of the healthy trips still measure the margin.

## 7. Risks

- Not run in game. First in-game run of the hook; only unit tests and the generated-code mirror
  test cover it.
- It acts only on a notification that the original callback itself accepts, for the camera the
  warper still holds, in state 20, with the request's own wait flags set. If (b') a menu cancel is
  the real trigger, the shot is cut short instead of freezing.
- `skip-request-pending` (req+0 or req+3 clear at the notification) is logged but not fixed; the
  watchdog arm remains the backstop for that case.
- The fix changes behaviour by default in this build (cvar default true); the control arm needs
  the explicit key.
- `last20_*` is filled only while the warp probe or watchdog is on (both l69 arms have the probe
  on); without it the WARN shows -1 there.
- The state-20 handler can also exit on the same frame; that is not a race (state is then 29 at
  the notification and the hook passes through).

## 8. Files

New: this report. Changed: `src/mcla_ki026_probe.cpp` (hook, cvar, dt/last-sample capture, F10
fields), `src/mcla_warp_probe.h` (constants, `FinishSample`, `DecideFinish`, `State20ExitDue`),
`src/mcla_dev_overrides.h` (key 20 `mcla_warp_finish_fix_off`), `src/mcla_m10_defaults.h`
(never-written), `tests/mcla_warp_probe_test.cpp`, `tests/mcla_dev_overrides_test.cpp`,
`tests/mcla_m10_defaults_test.cpp`, `CMakeLists.txt` (`MCLA_WARP_GENERATED_RECOMP8` for the warp
test).

## 9. Spark

Not used: generated-code reading of a private game image and a small host edit whose validation
is the build, the unit tests and the generated-code mirror test.

## Owner confirmation (2026-11-05)

- l69 fix arm, PC 1080p Unlocked, session 37a5ea6b: five garage trips after races, no freeze. Hook fires: 17:47:21 (duration 3.040), 18:01:18 (duration 5.244), 18:17:11 (duration 3.045), each with `action=finish-state20` and a `_DONE` line showing state 29 and the request cleared; healthy exits at 17:40:28 (time 2.978 of 2.992) and 18:12:36 (3.024 of 3.026). Three of five trips hit the race in this session. Control arm (fix-off) pending/optional.
