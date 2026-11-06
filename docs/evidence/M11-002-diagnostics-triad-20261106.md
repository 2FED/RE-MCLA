# M11-002 diagnostics triad: KI-056 album, KI-066 pursuit, KI-067 CPU census (l71, 2026-11-06)

Host-only patch and owner-internal candidate l71 by an Opus subagent for the primary agent.
**Built and unit-tested only. Not run in the game. No physical PASS.** Nothing was launched
(no `mcla.exe`, no launcher). No save, profile, LocalAppData or registry was written. There
were no git writes.

The police-manager reverse engineering (section 3) was done by a second, read-only Opus
subagent. It used the in-memory image reader (`l68-tools/img.ps1`, copied and extended in
scratch `l71-pursuit-re/`) and the generated code. The decrypted image was not persisted. This
author checked the key citations against the generated code, and `tests/mcla_pursuit_probe_test.cpp`
now checks them too.

Spark was not used. The work needed private game-image reads that must not leave the PC, and
the build, test and hash steps are deterministic.

## 1. Inputs

| Item | Identity |
|---|---|
| Host source | main `72f7e17`, scratch copy `l71-host-src`. main later moved to `e0797c6` with docs-only commits: `src`, `tests` and `CMakeLists.txt` are identical between `72f7e17` and `e0797c6`. |
| SDK | `private/l71-sdk-install`: ReXGlue commit `109116b` on v0.20.0.8, the M11-001 / KI-068 fix. It adds the `xam_content_stable_creation_time` and `xam_content_trace` cvars (both default true), `xam_content_close_host_handles_on_unmount` (default true) and `xam_content_move_aside_on_delete_failure` (Wine-only). CMake prints `Found ReXGlue SDK 0.20.0.8`. |
| SDK DLLs | `rexruntime.dll` `1EB4B1C9...2420A`, `rexgpu-xenos.dll` `A79557DC...14970`, `TracyClient.dll` `30E8AA91...BD37A`. These are not the l67/l69 bytes. The copies in the build directory are byte-identical. |

## 2. KI-056 album diagnostic (`MCLA_ALBUM v=1`)

The analysis is in [M10-ki056-album-preview-analysis-20261106.md](M10-ki056-album-preview-analysis-20261106.md).
The code is in `src/mcla_album_probe.{h,cpp}`. The pure parts are tested in
`tests/mcla_album_probe_test.cpp` (99 checks), including a check against the generated code.

### 2.1 Switch and hooks

- **Switch:** init-only cvar `mcla_album_probe`, default off. Dev-override key
  `mcla_album_probe=true`, startup phase.
- **When the switch is off:** each of the four strong overrides calls `__imp__` and returns. No
  guest memory is read.
- **When the switch is on:** each override reads state before and after the original call, then
  calls the original unchanged. All guest reads go through the bounds- and protection-checked
  `mcla::guest::Reader` (`src/mcla_guest_reader.h`). That class moved unchanged out of
  `mcla_ki026_probe.cpp`; the only addition is `Bytes()`.
- **No guest writes and no guest calls** are made other than the original function.
- **Rate limit:** lines are limited per kind by `album::Pacer`: a burst of 20, then 1 line per
  500 ms, at most 1000 lines per session. The number of suppressed calls is reported on the next
  line.

### 2.2 Generated lines mirrored

All citations are lines of `generated/default/mcla_recomp.49.cpp`.

| Function | Wrapper logs | Generated lines mirrored |
|---|---|---|
| `sub_8263CB78`: tick, r3 = album+456, r5 = phase | `fn=tick` only when the state at +132 or the sub-state at +136 changes. Logs from/to, frames at +732, and the image at +1140. | State lwz :14519. Sub-state :14598. State 4 calls `sub_8263C8B0` at :14700, then state 0 at :14705. Event 9 at :14685, then state 4 at :14692. |
| `sub_8263C8B0`: new photo, r3 = album | `fn=new_photo`, see 2.3 | SOI test at :14127-:14136, with a skip to `0x8263c9b0` on failure. Decode at :14148. Store of +1140 at :14157. Width and height compare through vtable +24/+28 at :14163/:14184. Recreate through vtable +92 at :14209. Bind `sub_8218F8D0` at :14222. No event is fired. |
| `sub_8263C9C8`: hover, r3 = album, r4 = slot | `fn=hover`: slot, mask +860, occupied bit, mode +1225 and count +1208, the slot JPEG (`*(+428)+slot*0x32000+204`) and its marker. The predicted `path=decode\|not-valid\|bad-marker\|unreadable` (a mirror of the guest test), `success=`/`event4=`, the image after the call, and CRC before and after. | :14260, :14266, :14282-:14300 (`slw`/`cntlzw`; slw gives 0 for slot & 0x20), :14317-:14322, marker :14343, decode :14361, bind :14435, `li r4,4`/`bl 0x8268ee10` :14446/:14448, failure path :14453 |
| `sub_8263DCF8`: save, r3 = album, r4 = slot | `fn=save`: slot, marker of +1148, mask before and after, image after, CRC before and after, slot texture `*(album+608+slot*16)` | Copy :17092. Decode :17124. Thumbnail :17135. `addi r11,r29,38` :17138. Material +604 :17191. Mask OR/store :17316/:17318. |

### 2.3 The `fn=new_photo` line

- **JPEG start marker:** the pointer at +1148, its first two bytes (`marker=FFD8`), and
  `path=decode-bind|bad-marker-skip|unreadable`. When `bind=1`, the rebind path ran.
- **Image +1140:** before and after, as `WxH@data/stride`. The image layout is from
  `sub_8218DE38` in `mcla_recomp.3.cpp`: u16 width at +0, height at +2, stride at +12, data
  pointer at +16.
- **`data_is_pixels`:** whether the decoded image still points at the +1144 allocation. That is
  the memory behind texture +1132. A decode into a new image buffer would explain a preview that
  never changes.
- **CRC:** `crc4k_before` and `crc4k_after` are CRC-32s of the first 4 KiB of `*(+1144)` (raw
  guest bytes), with `crc_changed`.
- **Texture +1132:**
  - its pointer before and after, and its first 16 words (`tex_words`, the "texture header
    words");
  - `fetch_before` and `fetch_after`: a k_8_8_8_8 Xenos fetch constant found heuristically,
    either inside the object (`obj+N`) or behind one of its pointer words (`wK->+N`). The decoded
    fields are base, WxH, format, pitch and linear/tiled;
  - `fetch_on_pixels`: whether the fetch base equals +1144 (physical);
  - `recreate92=yes|no|unknown`: inferred from the fetch size before the call against the image
    size after it.
- **Material and counter:** `material1136`, and `snapshot=N` (the count of new-photo decodes).

### 2.4 F10 block and the fix-A seam

The F10 `state.json` gets a block `"album"`:
- With the probe off it is `{"probe": false}`.
- With the probe on, it holds a live read of the album fields (state and sub-state, mask,
  marker, image, CRC, texture words and fetch), plus `last_tick`, `last_new_photo`, `last_hover`
  and `last_save` with their ages, the call counts, and `tracker`.

**Fix A is not implemented.** Its seam is `album::NewPhotoTracker::PendingRedecode()`, which is
pure and tested: a snapshot was decoded, then not saved and not re-decoded. That is the "at most
once per snapshot, chooser only" precondition. A later build can call `sub_8263C8B0` once from
the chooser-open point behind its own init-only cvar. The F10 block reports the precondition as
`fix_a_precondition`.

**What the owner run decides** (analysis 4.1):
- the state-4 decode never runs, or the marker fails: fix A;
- it runs and the CRC changes, but the panel stays empty: guest UI, fix B;
- it runs and `data_is_pixels=0` or `fetch_on_pixels=0`: the decode does not reach the texture
  memory. This is a new lead, a guest buffer swap, not a host miss;
- the CRC changes but nothing redraws: compare with the SDK `tex_*` lines (fix C).

## 3. KI-066 police pursuit (`MCLA_PURSUIT v=1`, F10 `"pursuit"`)

The code is in `src/mcla_pursuit_probe.{h,cpp}`. The tests are in
`tests/mcla_pursuit_probe_test.cpp` (48 checks, including checks against generated files 0, 4,
14, 23, 55, 56 and 61). MCLA has **no discrete heat level**. Each target has a law-breaking
score.

### 3.1 Where the manager and its fields are

- **The manager:** `mcPoliceManager` M = `*(0x8286D8E0)`.
  - It is created by `sub_822CB9C8` (228 bytes; `stw r31,-10016(r10)`, mcla_recomp.14.cpp:15308).
  - It is 0 when absent.
  - RTTI: `.?AVmcPoliceManager@@`.
- **Update:** `sub_8270AE38` (55.cpp:61215, r3 = M), called from `sub_822CB350`
  (14.cpp:14330).
  - It returns at once while the simulation is paused.
  - dt is `*(float*)0x827D7508`.
- **Score:** the only writer is AddLawBreakScore `sub_82705DA0` (55.cpp:49108). The record for
  target index i is at M + 68*i. The score field is at +104 (`stw r10,104(r28)` 55.cpp:49258),
  clamped to [0, T[0]].
- **Tuning table:** T = 0x8282CAC8. The image defaults are max 1000, pull-over 250, bullhorn
  500, pursuit 600, and 10/s decay. The probe reads the live values.

| Field | Offset / source | Confidence |
|---|---|---|
| Score (the "heat") | M+104 (player record, index 0) | high |
| Infractions this frame / latched | M+96 / M+100 (bit names in `InfractionText`; 0x4000 = block traffic) | high |
| Post-resolution grace | M+116, float seconds; Update decrements it, 55.cpp around 61434 | high |
| Cut scene on screen | M+60: 0 none, 1 CopCam, 2 Ticket, 3 Arrest (setters `sub_82703428/90/510`) | high |
| Busy (resolution or cut scene) | M+56, u8 | medium-high |
| Fine in $ | M+76 (`stw r3,76(r19)` 55.cpp:59725) | high |
| Last result / cops / pull-over car | M+52 / M+80 / M+84 | high (52/80/84 per the RE report) |
| Tactics | M+44 array, M+48 u16 count. The player tactic is the active one (+20) whose record pointer (+36) is M+96. | high |
| Pursuit phase | Tactic type from vtable slot +56: one of five `li r3,N` getters (0 responding, 1 pull-over, 2 pursuit, 3 pursuit-dbl, 4 arrow trap). The derived `phase=` also covers cut scenes, grace and none. | high |
| Responding / FSM state | Tactic +160 Fsm: +12 is the current index; the state class is named through RTTI (for example `StateTicket@FsmPullOver@...`) | medium (the index order per tactic is unmapped; the RTTI name is logged) |
| HUD indicator | **No persistent flag.** StartTactic sends a one-shot HUD message (`sub_82650FF0`). The probe reports the player's active tactic type as `hud`, and the hook logs StartTactic's r6 (`hud_flag`). | derived |
| Responding-unit timer | **Not identified.** Tactic +104 is a float of unknown meaning; the dispatch FSM timers are not mapped. | missing |

### 3.2 F10 block and log lines

**F10 `"pursuit"` block:**
- It is always present and read-only, like `guest_ki026`.
- It holds the manager, thresholds, dt, score, band, phase, bits, grace, cut scene, busy, fine,
  last result, cops, the pull-over car, and the player tactic (type, FSM index, class).
- `"verified"` says these values come from static analysis only. When the probe is on, it also
  holds the log counters and last values.

**Log lines:** behind the init-only cvar `mcla_pursuit_probe` (default off; dev key
`mcla_pursuit_probe=true`). Each hook is a pass-through.

| Line | Hook | Contents |
|---|---|---|
| `fn=score` | AddLawBreakScore | target, delta, the reason string (for example "block traffic/sec"), score before and after, band, infraction bits, dt |
| `fn=tactic` | StartTactic `sub_82704228` (r4 = type, r6 = HUD flag) | the returned tactic and a full snapshot |
| `fn=state` | after each Update | Written when anything but the score changes (`Changes()`: manager, latched, grace, cutscene, busy, fine, tactic, fsm, breaking start or stop), with from_phase and the full snapshot |

**Hypothesis for KI-066 (from the RE subagent, unproven):** a parked car that blocks AI traffic
sets 0x4000 ("block traffic"). After 1 s it scores 100, then 15 per second, so it reaches the
250 pull-over threshold in about 10-11 s. The category is "traffic violation". That matches the
l69 case: about 9 s after resume, at 0 mph.

The alternative is a large first dt after the pause, which inflates the per-second loops. The
`fn=score` lines carry dt and the reason, which separates the two.

## 4. KI-067 CPU census (`MCLA_CPU v=1`)

The code extends `src/mcla_vram_census_win.{h,cpp}`: pure `CpuReader` / `CpuWindow` /
`ComputeCpuLoad` in the header (tested in `tests/mcla_vram_census_win_test.cpp`, now 978 checks,
with a fake `CpuApi`), and the Windows `SystemCpuApi` in the .cpp. The texture-budget monitor
thread (`src/mcla_texture_budget_win.cpp`) writes one `MCLA_CPU` line next to each `MCLA_VRAM`
line, for the same reasons: minute, F10 mark and exit.

**Switch:** cvar `mcla_cpu_census`, init-only, default on. It only acts while `mcla_vram_census`
runs.

**Sources** (each optional, never fatal, disabled after 5 consecutive failures):

| Source | Fields |
|---|---|
| GetSystemTimes / GetProcessTimes at 1 Hz | `sys_pct`, `proc_pct` (percent of all logical processors), `proc_cores` |
| PDH (pdh.dll resolved at run time): "\Processor Information(_Total)\% Processor Performance" x "...\Processor Frequency" | `eff_mhz`, `perf_pct`, `nominal_mhz` |
| CallNtPowerInformation(ProcessorInformation) (powrprof.dll at run time) | `ntpi_mhz`, `ntpi_max_mhz`, `ntpi_limit_mhz` (MhzLimit drops under a power or thermal limit) |
| WMI `ROOT\WMI MSAcpi_ThermalZoneTemperature` | `temp_c` with `temp_src=acpi_tz`: tried once at start on the monitor thread (COM MTA on that thread only; no CoInitializeSecurity), then every 10 s if it worked. Usually it needs administrator rights, and the start line reports `temp=unavailable` with the HRESULT. An ACPI zone is not the package sensor. |

**Format:** each field is min/avg/max, or `n/a`.

**Wine/Proton:** there is no monitor and nothing is called. One line at start,
`MCLA_CPU v=1 reason=start ... cpu=wine ...`, with every field n/a. That gives the Deck
"present or marked unavailable".

## 5. Dev overrides and M10 defaults

`kKeyCount` is now 24, with four new startup keys:
- `mcla_album_probe` and `mcla_pursuit_probe`: plain bools;
- `xam_content_stable_creation_time_off=true`, which writes `xam_content_stable_creation_time=false`
  (the KI-068 Deck control);
- `xam_content_trace_off=true`, which writes `xam_content_trace=false`.

`=false` changes nothing. The never-written set (`kNeverWrittenCvars`, now 20 entries) adds
`mcla_album_probe`, `mcla_pursuit_probe`, `xam_content_stable_creation_time` and
`xam_content_trace`. The tests cover:
- key lookup, phase, cvar mapping, the conflict-pair invariant and the 1 KiB limit for an arm
  with every key (`tests/mcla_dev_overrides_test.cpp`, 229 checks);
- the never-written checks (`tests/mcla_m10_defaults_test.cpp`, 2557 checks).

The F10 export policy test checks that `album` and `pursuit` never reach exported metadata (622
checks).

## 6. Build and tests

- **Build:** `private/l71-host-build` was configured fresh against `l71-sdk-install`, from
  scratch tree `l71-host-src`.
  - Options: expiry, raw timer, perf split, livery diag2, read-back sync composite, dev
    overrides, prewarm seed, and the M10 speed-up defaults ON. Image-capture and diagnostic tests
    ON. WGC OFF. `REXSDK_VERSION` empty. Prefix maps and `/pdbaltpath`.
  - Result: 267/267 steps, rc 0, **0 warnings**. The 56 targets include all 51 `*_test` targets
    in `build.ninja`, and the test executables built match those targets exactly.
- **Tests:** 51 executables, **0 failed** (`l71-tests.log`).
- **ast-grep 0.50.0:** `scan src tests` rc 0; `test` 3/3 rc 0.

## 7. Candidate `private/l71-int-input` (final, 2026-11-06)

**Superseded:** the first l71 build (triad patch on `72f7e17`, SDK `109116b`; `mcla.exe`
`4A0B0673...D634`) was rebuilt and is no longer staged.

**Final build:**
- **Source:** a scratch copy (`l71b-host-src`) of main `3e78c2c54bede0545c6ff03740ab746d4870101b`
  ("host: dev-override keys for the KI-071 audio A/B"). That tree contains the triad and, since
  then:
  - the KI-070 F10 without live minidump (`src/mcla_diagnostic_trigger.h`,
    `mcla_diagnostics.cpp`);
  - the crash/F10 async log flush (`mcla_native_crash_win.cpp`, `mcla_diagnostics.cpp`);
  - the four KI-071 audio dev keys (28 keys).
- **SDK:** the final `private/l71-sdk-install`, d08813b (`0.20.0.3-dev.gd08813b`: KI-068 +
  KI-069 + KI-071).
- **Build:** `private/l71-host-build` was configured fresh with the same l69 owner options:
  267/267 steps, rc 0, **0 warnings**. The test targets in `build.ninja` (51) equal the test
  executables built (51).
- **Tests:** 51 of 51 pass, 0 failed.
- **ast-grep 0.50.0:** scan rc 0, rule tests 3/3.

| File | Bytes | SHA-256 |
|---|---|---|
| MCLA-Launcher.exe | 1456128 | 9E2C1492E47F2150C407A1EB0C3627A08FE2AB57DFFF01F87BF53606E30DCE4E |
| mcla.exe | 53581824 | A0FFA6C641345524C48F97273E2F128D45D1DDB554379F7863B999A64094F2D1 |
| mcla_crash_handler.exe | 211968 | 62E9D38BA6A58FEC0A22A3B993EBDFE99E9F3511B5779C45CFD7805D31A15101 |
| mcla_diagnostic_image_helper.exe | 60928 | AE8A0B85CBBE864425E94595A73309BE14C0FD7ECC1DF9F4ED685C7EF90456E7 |
| rexgpu-xenos.dll | 3232768 | 3655D910427AB0643FB7785E138306A889EDF607AA4FA249CC0EE987787AF1CC |
| rexruntime.dll | 11255808 | 259145F1436588F94530B74487353759A168CC3FB22182AFFE51BE9ECA933EFD |
| TracyClient.dll | 246784 | 30E8AA912F64D638CCC3E67C25EF6BE03D36151901D9A72BD6F21472C2BFD37A |

The three DLLs are the `l71-sdk-install/bin` files. The copies in the build directory are
byte-identical.

**Path scan** (`l71b_scan.py`):
- No build, user, temp or patch paths.
- The remaining hits are three-byte opcode patterns (`l4x`-`l7x` inside instructions of
  `mcla.exe`, `MCLA-Launcher.exe` and `rexruntime.dll`), the launcher's generic `AppData`
  strings and the `private/save-arc` entry names.
- The admission marker is present once.

**String checks:** 0 failures.
- `mcla.exe` holds:
  - all 28 whitelisted keys, read from the tree's `kKeys`;
  - the audio cvar names `audio_speaker_config`, `audio_downmix_itu` and `audio_channel_rms_probe`
    (key targets);
  - `minidump_policy`, `skipped-by-default`, and the `FlushLoggingFor` import;
  - `MCLA_ALBUM v=1`, `MCLA_PURSUIT v=1`, `MCLA_CPU v=1` and the CPU-census texts.
- `rexruntime.dll` holds `FlushLoggingFor`, `MCLA_LOG v=1`, the three audio cvars and their
  `[audio] speaker config: cvar=` text, `xam_content_stable_creation_time` and
  `xam_content_trace`.
- `rexgpu-xenos.dll` holds `MCLA_CP_FREEZE`.

## 8. Owner staging (final)

| File | SHA-256 |
|---|---|
| `private/l71-internal-prepare.ps1` (generated by `mk_prepare_l71b.py` with the new pins; V1 ticket, stage `private\l71-internal`, internal audit, KI-045 admission row `l71-internal`; 0 parse errors in PowerShell 7.4.22 and 5.1). V1 has no leases, so the lease probe is not needed. | F220DE66F515015ECF8E89A74052B850EA26BF3116D9F91E91579950683DDE51 |
| `private/l71-arms/pc/triad/mcla-dev-overrides.txt`: the l69 fix arm, every key explicit, + album/pursuit/warp probes true, xam `_off` false, the four audio keys false (876 B) | F0A53ED420F7BB01ED6ABFBF33BF69CC65F491CBC54C3811DBC8C6A276D84DAA |
| `private/l71-arms/pc/audio-old/mcla-dev-overrides.txt` (coordinator; 873 B) | 6D66C0AC956D6DB0BA3D57C9839EA5F2B45CD25C5528C81DD9FE627B23E447A1 |
| `private/l71-arms/pc/audio-itu/mcla-dev-overrides.txt` (coordinator; 874 B) | 9DF46E1FA525DD18883B2E7745E9D87926D00744797FF8A45A91E8934A8AB380 |
| `private/l71-arms/pc/audio-stereo/mcla-dev-overrides.txt` (coordinator; 874 B) | 2006C3328A54B1ABAEF11183870C954C058F277DF69680B7584E98FD67732552 |
| `private/l71-arms/deck/saves-fix/mcla-dev-overrides.txt`: l69 fix keys + album/pursuit/warp probes false, xam `_off` false, audio keys false (879 B) | 0ED09F73502742176D218CEC5804095151CDC8C695514AC3156F3D4A3753C349 |
| `private/l71-arms/deck/saves-control/mcla-dev-overrides.txt`: saves-fix + `xam_content_stable_creation_time_off=true` (878 B) | 082C8ED8AE3CDC0847E8D7EDE7A88A94B618F9047DFE2AF802CE233C6DA39BFF |
| `private/l71-arm-triad.cmd` | 369B04757BF8E0B8149CBCE59C67CD84BFF4DFB198F1CCBBAD826AD50767E4C2 |
| `private/l71-arm-clear.cmd` | 36AFDC6F33283E35A743C3D8D76DC6E07C4D380568E448F4C99E1585BA3BB7BF |
| `private/l71-CHECKLIST.txt` | E8545ADE7961369231C27E31587162357DE323EE3A6D7D356A600A2EED27FB18 |

- **Arm check:** all six arm files (pc triad, audio-old, audio-itu, audio-stereo; deck
  saves-fix, saves-control) were parsed with the production parser of the final tree. Each is
  status parsed, 28 values, 0 ignored, 0 mismatches against the expected true-sets.
- **Checklist, PC:**
  - at 1080p: photo; F10 in the chooser before moving the cursor; hover an occupied slot and
    back; F10; save, reopen and check;
  - the same at 720p;
  - a pull-over with F10 at "PULL OVER" and at the fine;
  - one 20-minute drive for the CPU census;
  - KI-071 audio A/B on the Pier conversation with Karol (or any cut-scene): audio-old, then
    audio-itu, then audio-stereo, one restart each. Note which sounds right and whether the
    engine, radio or music changed, plus the Windows output format and the Spatial sound state.
  - KI-069/KI-070: F10 must no longer pause the picture. Expected log lines:
    `MCLA diagnostics: live minidump policy=skipped-by-default`, `MCLA_LOG v=1 kind=minute` with
    `dropped=0`, and `MCLA_CP_FREEZE` only on real stalls.
- **Checklist, Deck:** saves-fix with two garage saves, quit, restart and check the load
  (expected: no dialog, `save ok` twice, identical `query times class=34 creation=`); then
  saves-control (expected: the dialog on save 2).

## 9. Risks and limits

- **Album probe:** it only reads, but the hooks run on the guest UI thread. When on, the
  new-photo, hover and save hooks read about 4 KiB + 2x128 B twice per call. These calls are
  event-driven. The tick hook reads 4 words per frame.
- **Fetch-constant search:** it is a heuristic, and `fetch=none` is possible. The raw texture
  words are logged either way.
- **Pursuit:** the field meanings come from static analysis. The FSM index order, the
  responding-unit timer, M+212/+220 and the exact block-traffic test (`sub_82702C08`) are not
  decoded. The F10 block reads tactic and FSM pointers through the checked reader, so a stale
  pointer gives `complete: false`, not a crash.
- **CPU census:** WMI and COM on the monitor thread. `ConnectServer` uses
  `WBEM_FLAG_CONNECT_USE_MAX_WAIT`, so in the worst case a slow WMI start delays the first
  census lines; the game is not affected. PDH and powrprof are loaded at run time. Under Wine
  nothing is called.
- **`MclaKi026StateJson` / `GuestReader`:** the move to `mcla_guest_reader.h` is
  behaviour-identical (diffed). `mcla_warp_probe_test` still passes 5936 checks.
