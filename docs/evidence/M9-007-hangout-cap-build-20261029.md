# M9-007 hangout cap and owner-admission fix: l42 owner candidate (2026-10-29)

Implementation subagent report, for review by the primary agent. Nothing was
launched (no `mcla.exe`, no launcher, no game); no save, profile or key was
opened. The l42 prepare script was parse-checked only, not run or signed.
Spark: not used (two small source edits plus deterministic build/test gates;
offloading would cost more than it saves).

Inputs: [M9-007-hangout-analysis-20261029.md](M9-007-hangout-analysis-20261029.md)
(item 1) and its proposal diff; the KI-046 A/B from
[M9-006-ki-046-perf-analysis-20261029.md](M9-006-ki-046-perf-analysis-20261029.md)
section 6; the coordinator's owner-incident note on `owner-admitted-builds.txt`.

## 1. Hangout cap (M9-007)

The proposal diff applied cleanly (`git apply --check`, then `git apply`) and
was then hardened:

- `src/mcla_menu_state.h`: two pure helpers next to the `Controller`, which is
  unchanged.
  - `RacerInputOff(racer_read, racer_state, input_off)` returns the +908 byte
    only for a racer that was read and has state 3.
  - `CapPaused(sim, local, racer_off)` = `sim || local || racer_off`.
- `src/mcla_menu_state.cpp`:
  - New init-only cvar `mcla_ui_cap_racer_inactive`, default true. It is a
    separate kill switch; `mcla_ui_cap_guest_pause=false` still turns the
    whole hook off.
  - `ReadRacerInputOff` reads `*(be32*)0x82874374`, then `players+8`, then
    `racer+656` and `racer+908`. Pointers are null-guarded. Every address is
    checked before it is read: it must lie inside a guest heap and have read
    protection (the same check as the KI-026 probe's `GuestReader`). That
    reader lives in an anonymous namespace in `mcla_ki026_probe.cpp`, so it is
    not exposed and a minimal local copy is used.
  - `ShutdownMenuStateCap` first sets an atomic `racer_reads_stopped`. After
    that the hook never follows the racer pointers.
  - The transition log line is now
    `MCLA menu state: game {paused|running} (sim N local N racer-off N), UI cap {forced|draw-count}, mcla_ui_cap_draws_low V`.
- `tests/mcla_menu_state_test.cpp`: the existing 447 checks are unchanged.
  New tests cover:
  - all 8 `CapPaused` combinations and a non-zero flag;
  - `RacerInputOff`: active, driving, refused read, and states 0/1/2/4/max;
  - a hangout cycle through the `Controller`: racer off forces 100000; a
    pause menu over the hangout is not a new edge; reactivation restores 250.
  - Result: 468 checks.

## 2. Owner-admission fix (owner incident 2026-10-29)

The owner hand-added the l33-sunc row in an editor. The saved file had no
final LF (337 bytes). The strict parser rejected the whole file, and the
launcher then silently refused Unlocked/2160p for every build.

- `src/mcla_launcher_capabilities.h`:
  - New `CheckOwnerAdmission(raw)` returns
    `expected<vector<string>, OwnerAdmissionRejection{reason, line}>`.
  - The 16 KiB limit applies to the raw bytes. Then one leading UTF-8 BOM is
    stripped, CRLF becomes LF, and a missing final LF is added. Every row is
    then checked strictly, as before.
  - Still rejected, with a reason and a 1-based line: unknown or missing
    header, empty line, a fingerprint that is not 64 uppercase hex
    characters, bad label, duplicate, more than 64 builds. A lone CR, a CR-only
    line break and a second BOM are also still rejected.
  - `ParseOwnerAdmission` and `OwnerAdmissionListed` are kept as wrappers.
  - `OwnerAdmissionNotice` formats
    `Owner admission file ignored: <reason> (line N).`
  - The old parser's `remove_prefix(npos + 1)` path cannot occur: the text
    always ends with LF.
- `src/mcla_launcher_controller_win.cpp`:
  - `ReadOwnerAdmission` returns `{listed, notice}`. A missing file stays
    silent (`GetFileAttributesW` NOT_FOUND/PATH_NOT_FOUND).
  - An existing file that cannot be read, or that is rejected, yields the
    notice.
  - `AdmitCapabilities` appends the notice to the error text of a refused
    gated check. On success it hands the notice back, and `CheckLaunch` adds
    it to the CHECK SETUP status. So a stock check on a broken file is still
    Ready, but it says the file was ignored.
- `admission owner-allowlist-invalid` in capabilities.txt: **not done**.
  `capabilities.txt` is re-serialised byte for byte from the receipt by the
  diagnostic export policy (`mcla_diagnostic_export_policy.h`, KI-045
  branch). An extra key would break that check, or need a policy and schema
  change. As instructed, only the status text was added.
- Tests:
  - `tests/mcla_launcher_capabilities_test.cpp`: three inputs that used to be
    rejected are now accepted (no final LF, CRLF, and a header without LF,
    which is an empty list). They moved from the "bad" set to positive checks.
  - New positive checks: CRLF, BOM, BOM+CRLF+no final LF, and mixed endings.
  - New reason/line checks: non-hex row (line 3), lowercase in a CRLF file
    without final LF, unknown header, empty file, duplicate, label, empty
    row, stray CR, CR-only, double BOM, oversized file (no line), the 65th
    build (line 66), and exactly 64 builds accepted. Result: 262 checks
    (l41: 238).
  - `tests/mcla_launcher_controller_win_test.cpp`: the old "malformed" case
    was a CRLF file, which is now valid, so it was replaced.
    - BOM+CRLF+no-final-LF list: Ready at 2160p with "Owner-admitted build"
      and no "ignored" notice.
    - Non-hex row: Blocked, and the status and readiness reason contain
      `Owner admission file ignored: not a 64-character uppercase hex fingerprint (line 3).`
    - Stock check on the same broken file: Ready, with the notice.
    - Valid list: no notice.
    - Result: 375 checks (l41: 367).

## 3. Build

- `private/l39-work/l42_build.ps1`: a copy of `l41_build_merged.ps1`. Only
  the comment and `$Build = private\l42-build` differ. New logs:
  `l42_configure.log`, `l42_build.log` (first pass, 219 steps),
  `l42_build2.log` (after the launcher fix, 23 steps; `mcla.exe` was not
  relinked) and `l42_tests.log`, all in `private/l39-work/`. No existing
  `l39-work` file was changed.
- Cache: the `MCLA_*`, build type, compiler, flags (prefix maps),
  `CMAKE_PREFIX_PATH`, `rexglue_DIR` and `REXSDK_*` entries are identical to
  `private/l41-build/CMakeCache.txt` (diff empty). The SDK is still
  `private/l41-sdk-install`.
- Targets: `mcla`, `mcla_crash_handler`, `mcla_diagnostic_image_helper`,
  `mcla_internal_launcher` (MCLA-Launcher.exe), the 35 test targets of
  `l41_targets.txt` and `mcla_diagnostic_image_fake_helper`.
- Compiler warnings: 0 in both build logs. The configure log shows only the
  same unused `CMAKE_C_COMPILER/CMAKE_C_FLAGS` notice as l41.
- `ast-grep scan`: clean (exit 0, no output). `ast-grep test`: 3 passed.
  `git diff --check`: clean. The 7 source/test files are ASCII and CRLF.

## 4. Tests (35 executables, all exit 0)

The counts match l41 except the three changed suites:

| Suite | l41 | l42 |
| --- | --- | --- |
| `mcla_menu_state_test` | 447 | **468** |
| `mcla_launcher_capabilities_test` | 238 | **262** |
| `mcla_launcher_controller_win_test` | 367 | **375** |

Unchanged suites:

- lease network 26, lease policy 1438, lease runtime 69
- cursor 266
- export policy 585, export UI 249, export native 105 (1 skip: symlink 1314)
- hotkey 8, image client 51, image protocol 119, trigger 102
- frame audit 1809, frame signal 24, guest sampler 9
- build verifier 109, fs 76, game 70
- import policy 72, import 31, library codec 315, preferences 2088
- process 41, profile 118, snapshot 26, store 125 (1 skip), UI 983
- managed config 50, photo 204
- adapter policy 78 (with `config/mcla.toml.example`), path policy 52
- readback 133, session report 39

`mcla_diagnostic_image_client_test` was run with the fake helper and its
SHA-256: 51 checks, fake child only, no capture.

## 5. Binary checks and artifacts

- `mcla.exe`: `SDL.app.metadata` 0, `SDL_VIDEO_DRIVER` 0.
  - SDL imports are the same as l41: `AddSdlEventObserver`,
    `RemoveSdlEventObserver`, `CaptureSdlJoysticks` and `QuerySdlJoystick`,
    from `rexruntime.dll`.
  - The import DLL list matches l41, and so does the set of `SDL*` identifier
    strings.
  - It contains `mcla_ui_cap_racer_inactive` and the `racer-off {}` format.
- `MCLA-Launcher.exe` contains `Owner admission file ignored`.
- Path strings (ASCII and UTF-16LE, case-insensitive): `C:\BDU`, `C:/BDU`,
  `\Users\`, `/Users/`, `mcla-recomp`, `worktrees`, `l41-sdk`, `l42-build`,
  `l34-src`. None were found in any of the seven files.
  - An account-name probe matched 13 ASCII strings in `rexruntime.dll` only.
    That file is byte-identical to l41, where these were already recorded as
    colour-name false positives.

`private/l42-int-input` (seven files, same layout as `l41-int-input`):

| File | Source | Bytes | SHA-256 |
| --- | --- | --- | --- |
| `mcla.exe` | **new**, l42-build | 53073408 | `63B8CE29A3F6153F434A38F5DF892B4B8A639F2D49B52D6E0DAD37D5644D74F5` |
| `MCLA-Launcher.exe` | **new**, l42-build (admission fix) | 1345024 | `1FB8E19A5660832396E7924B845775738A910601C367382C5093220B9DD2CD24` |
| `mcla_crash_handler.exe` | l41-int-input | 211968 | `E45945297AE87F295BFCD4DF77A90FBC10BAFD02AFD209284B94C104C23A29A0` |
| `mcla_diagnostic_image_helper.exe` | l41-int-input | 60928 | `11482F3B0CF9DEFCE03E142145CC6FE7035A957736A32073ECCAB424A4523B6A` |
| `TracyClient.dll` | l41-int-input (= l42-build) | 246784 | `C2CC7F09D1DE4EDB4438CC332EEA3E3EB07750E21451041D4ABDA0E3A25D008C` |
| `rexgpu-xenos.dll` | l41-int-input (= l42-build) | 2997248 | `F3CECECC2962692520E4C5FF5F8BB89E46134C9B707898AE902834295B572E6D` |
| `rexruntime.dll` | l41-int-input (= l42-build) | 10965504 | `3A7A346A06DFBA35EA223CAE41B14DD0258AC50AF09BF6F375C3F59A5304A7C0` |

The rebuilt crash handler (`2B2429F5...`) and image helper (`542E76DA...`)
differ from l41 only in 2 bytes: the COFF `TimeDateStamp` at PE offset +8
(`0x78 + 8`). The l41 bytes were kept, so these two pinned identities do not
change. `mcla.exe` comes from the first pass (18:04): the second pass
rebuilt only launcher and test objects and did not relink it.

## 6. l42 prepare script

`private/l42-internal-prepare.ps1` (ASCII, CRLF, SHA-256
`EA72F67C677525CC213011DEDF8CF5DD4F068F3F0F8027BEAE8C4F8703259C15`) is
modelled on `l41-internal-prepare.ps1`. PowerShell 7 parse: 0 errors. It was
not run.

- Pre-checks:
  - refuses to run if `private/l42-internal` or the ticket already exists;
  - requires `preferences.dat`;
  - pins the `mcla.exe` and `rexruntime.dll` hashes;
  - requires exactly the seven named files in `l42-int-input`, each with its
    pinned SHA-256.
- Then it signs a V1 ticket (no kill switch) and stages to
  `private/l42-internal` (CRT from l12, no guide).
- It then runs the internal staging audit.
- Admission rows:
  - It writes `owner-admitted-builds.txt` with l39, l40, `ABD97918... l41-internal`,
    `738BD6622754A9650A5675C8262127D47ABC632B9C604B5F1C02F4B421AD9C4E l33-sunc`
    and the new `l42-internal` fingerprint.
  - It keeps every valid existing row, so the owner's hand-added l33-sunc row
    is kept. Before reading, it now strips a BOM and CRs from the existing
    file, so CRLF rows are kept, not dropped as malformed.
  - The write is atomic and LF-only.
- `l33-sunc` = SHA-256 of `private/l33-sunc/build/mcla-build.manifest`
  (checked).

## 7. Owner steps on l42 (also in the script header)

1. **Hangout:** open the wager menu at a hangout or meet spot in Unlocked.
   - Expect about 30 fps, and `MCLA menu state: game paused (sim 0 local 0 racer-off 1), UI cap forced`
     plus `MCLA UI cap: on`.
   - After you exit or the race starts, expect `running (... racer-off 0)` and
     `UI cap: off`. Driving should be 55-74 fps.
2. **Newly capped screens:** race intros, arrest/tow, warps and the showroom
   now also cap at about 30, as stock does. Report if that feels wrong.
   `--mcla_ui_cap_racer_inactive=false` restores the l41 behaviour.
3. **Regressions:** the pause menu is still capped (sim 1); check photo mode
   and F10.
4. **Launcher:** CHECK SETUP at 2160p / Unlocked should be Ready with
   "Owner-admitted build" and no "Owner admission file ignored".
5. **KI-046 A/B:** same save and the same downtown drive for 3 or more
   minutes without menus. Unlocked, VSync on, 1440p first, then 2160p.
   - A = `private\l42-internal\build`.
   - B = `private\l33-sunc\build`.
   - Compare frame pacing/min, gpu busy/min, draws p50, near-budget lines and
     the texture cache hard limit.
   - On l42, keep A's route free of hangouts and race intros (now capped), or
     use l41 for A.

## 8. Limits and residual risks

- Nothing physical has been verified. All of section 7 is pending owner
  testing.
- More screens are capped at 30 than in l41 (intros, arrest, warps, showroom).
  This is by design, but it changes the earlier "race-start cinematics stay
  uncapped".
- If some state leaves +908 set while the player drives, driving would be
  capped. That is not expected (the camera input is gated on it), and both
  the kill switch and the `racer-off` log field are available.
- Each game frame now makes up to 8 heap lookup / `QueryProtect` calls. These
  may take the SDK's global memory lock. The cost is expected to be small, but
  it was not measured.
- The check and the read are not atomic. A shutdown that is already in
  progress while a hook is mid-read is not covered by `racer_reads_stopped`.
- Online hangouts are untested; BeginWarp skips the deactivation online.
- Admission: tolerance is limited to the BOM, CRLF and the final LF. Trailing
  blank lines, spaces and lowercase hex are still rejected, but now with a
  visible reason. `capabilities.txt` carries no invalid-admission marker (see
  section 2).
- Vulkan: unchanged; the draw-count fallback is D3D12 only.
