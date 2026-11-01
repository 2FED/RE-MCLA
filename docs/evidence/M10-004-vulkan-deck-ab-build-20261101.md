# M10-004: Vulkan versus D3D12 owner A/B build for the Steam Deck (l59, 2026-11-01)

Status: **SDK and host built and unit-tested; candidate input and arm files
prepared; not run in the game.** Built by an Opus subagent for the primary
agent. Nothing was launched (no `mcla.exe`, no launcher, no game); no git
write; no download or install; no registry write. No physical result exists.
Spark: not used (deterministic build, test and source lookups; no log to
triage).

Relates to KI-053 (Proton runs D3D12 through vkd3d-proton instead of the
plugin's native Vulkan backend) and continues
[M10-004-vulkan-sdk-build-20261101.md](M10-004-vulkan-sdk-build-20261101.md)
(l58vk preparation; its host switch proposal is re-based here onto the l58
dev-override whitelist).

## 1. What l59 is

l59 = l58 (main `aeb3250`, which holds the l58 host commit `021087e`) + one
host change: the init-only host cvar `mcla_gpu_backend_vulkan` (bool, default
false), whitelisted for the owner override file. When it is true and the host
was built against an SDK with `REX_HAS_VULKAN`, `MclaApp::OnPreSetup` sets
`config.gpu_backend = "vulkan"`, **overriding the launcher's
`--gpu_backend=d3d12`** (the launcher cannot pass `vulkan`), and logs

```
MCLA gpu backend: vulkan (dev override); launcher backend was 'd3d12'
```

followed by the existing `MCLA graphics: selected GPU plugin 'xenos'
backend='vulkan'`. Built against a D3D12-only SDK it logs `MCLA gpu backend:
mcla_gpu_backend_vulkan ignored: this build's SDK has no Vulkan backend;
keeping 'd3d12'` and changes nothing (that branch was compiled with
`-fsyntax-only` and `REX_HAS_VULKAN` undefined: clean). The SDK is the l58 SDK
commit `141e13f` compiled with `REXGLUE_USE_VULKAN=ON` (D3D12 kept), so one
package and one DLL set serve both arms.

Ordering: the dev-override file is read in `OnPostInitLogging` (inside
`SetupEnvironment`), before `SetupPresentation` copies `gpu_backend` into the
runtime config and calls `OnPreSetup`, so the startup-phase key is in place in
time. The launcher never sets `mcla_gpu_backend_vulkan`, so the "set by the
command line" rule does not block the file value. A Vulkan device that fails to
initialize fails setup with the SDK message box `Failed to load GPU plugin
'xenos' with backend 'vulkan'` (no D3D12 fallback, by SDK design).

## 2. SDK (l59vk)

| Item | Value |
| --- | --- |
| Source | read-only worktree `private/l58-sdk-wt`, branch `mcla/m10-readback`, HEAD `141e13f72580066513db36efdd956da1245df4b2` (v0.20.0.6 + l57 CPU waits `62cd66d` + l58 read-back work); submodules `volk`, `vulkan-headers`, `vulkan-memory-allocator`, `glslang`, `spirv-tools`, `spirv-headers` present |
| Script | `private/l59vk-work/l59vk-sdk.ps1` (= `l58vk-sdk.ps1` with the source, build, output and install paths changed; `-Step configure, build, install, ctest`) |
| Flags | the l58vk set: Ninja Multi-Config, clang 20.1.8, `-march=x86-64-v3`, `REXGLUE_USE_D3D12=ON`, `REXGLUE_USE_VULKAN=ON`, FidelityFX OFF, Tracy ON, perf counters ON, tests ON, `CMAKE_FIND_USE_PACKAGE_REGISTRY=OFF`; `PYTHONDONTWRITEBYTECODE=1`; `VULKAN_SDK` unset |
| Outputs | build `private/l59vk-sdk-b`, output `private/l59vk-sdk-o`, install `private/l59vk-sdk-install` |
| Logs | `private/l59vk-work/` configure.log, build.log, install.log, ctest.log, ctest2.log, reg-before.txt, reg-after.txt |

- Configure 114.5 s, `Graphics: D3D12=ON Vulkan=ON`, Python 3.15.1 found
  (`C:/Python315`, SPIRV-Tools grammar tables). Build 1845 steps, rc=0;
  912 `warning:` lines, the identical message set of the l58vk build (all
  SDK/vendored code).
- Source tree untouched: `find private/l58-sdk-wt -newer
  private/l59vk-work/marker-start` is empty after build, install and ctest
  (no `__pycache__` this time; nothing to remove).
- Install: 1283 files, 190 MB; `rexglue.exe --version` =
  `0.20.0.2-dev.g141e13f` (dev SDK, as l58). Installed targets export
  `REX_HAS_D3D12=1;REX_HAS_VULKAN=1`. Install ran with the Windows directories
  removed from `PATH`: the HKCU CMake package registry is unchanged (`reg
  query` before and after identical, l39..l58 rows only; no l59vk row).
- DLL imports of `rexgpu-xenos.dll` and `rexruntime.dll` are identical to the
  D3D12-only l58 SDK (llvm-readobj): no new runtime or system DLL;
  `vulkan-1.dll` is loaded at run time by volk (string present in
  `rexruntime.dll`; on the Deck Proton's winevulkan supplies it).

| File (`l59vk-sdk-install/bin`) | Bytes | SHA-256 | l58 (D3D12 only) bytes |
| --- | --- | --- | --- |
| `rexgpu-xenos.dll` | 6735360 | `ADA375A9D23E516448A26A3DC85CCC8D00EC2B66121F2FA5522B35619EC1181D` | 3168256 |
| `rexruntime.dll` | 11786240 | `3433F93881EB017B1E86316CD12B4C0613288F89ED2933267A3FC45E24C2F694` | 10995712 |
| `TracyClient.dll` | 246784 | `CBC7D70100FE694EDEAA0DC5789391CFD1AF6CE8F8C687389DE3D9AFA79F655E` | 246784 |
| `rexglue.exe` | - | `6B0B465F583365C4AE51498389F44685D69326149D89D3D63C9CB46187405BA1` | - |

### ctest

| Run | Result |
| --- | --- |
| 1 (`ctest.log`) | 1977/1978, 4 skipped (the known BitStream Write cases), 1 failed: `unit.Manifest: missing project name fails`, `remove_all: The process cannot access the file because it is being used by another process` (the shared `%TEMP%\manifest_test` race under `-j 8`, as in l58vk) |
| 2 (`ctest2.log`, same build) | **100% tests passed, 0 failed out of 1978**, same 4 skipped |

Tagged suites run separately on `l59vk-sdk-o/Release/unit_tests.exe`, all
passed: `[perf-split]` 14 cases, `[pipeline-wait]` 10, `[livery-diag2]` 18,
`[wrm]` 11, `[replay-wake]` 7, `[readback-slots]` 8. No Vulkan-specific test
exists; the Vulkan backend is covered by compilation only.

## 3. Host patch

Three files (plus this document), re-based onto main `aeb3250` (after l58's
`kReadbackElideUnread`):

1. `src/mcla_app.cpp`: `REXCVAR_DEFINE_BOOL(mcla_gpu_backend_vulkan, false,
   "MCLA", ...)`, init-only; the `OnPreSetup` block of section 1, placed
   before the unchanged KI-053 `any` branch (which therefore never fires in
   the Vulkan arm either).
2. `src/mcla_dev_overrides.h`: `Key::kGpuBackendVulkan` =
   `{"mcla_gpu_backend_vulkan", Phase::kStartup}`, `kKeyCount` 6 to 7; header
   comment names this document.
3. `tests/mcla_dev_overrides_test.cpp`: seven keys; the new key's lookup and
   startup phase; `gpu_backend`, `mcla_gpu_backend` and `vulkan_device` stay
   out of the whitelist; the two l59 Deck arm files parse byte for byte (seven
   values, no ignored line; checked identical to the files in
   `private/l59-deck-arms`); `mcla_gpu_backend_vulkan=vulkan` is malformed and
   `gpu_backend=true` unknown. 93 checks (l58: 83).

Apply (primary, main checkout, copy-only with hash checks; refuses if any
target changed since `aeb3250`):
`powershell -NoProfile -ExecutionPolicy Bypass -File C:\Users\orange\AppData\Local\Temp\claude\C--BDU-MCLA-Recomp\l59-patch\apply.ps1`

## 4. Host build and tests

- Scratch tree `%TEMP%\claude\C--BDU-MCLA-Recomp\l59-patch\tree` = the main
  checkout's `src`, `tests`, `cmake`, `config`, `rules`, `rule-tests`,
  `utils`, `scripts`, `generated`, `docs`, `CMakeLists.txt`, `VERSION`,
  `sgconfig.yml` at `aeb3250` (tracked sources clean) plus the three patched
  files; the `third_party/rexglue-sdk/thirdparty/inja` test headers as in the
  l58 tree.
- `private/l59-build` (scripts `private/l59-ab-work/l59_ab_build.ps1`,
  `l59_ab_targets.ps1`, `l59_ab_tests.ps1`): the l58 options
  (`MCLA_TEST_BUILD_EXPIRY`, `MCLA_RAW_TIMER_DEFAULT_ON`,
  `MCLA_PERF_SPLIT_DEFAULT`, `MCLA_LIVERY_DIAG2_DEFAULT`,
  `MCLA_READBACK_SYNC_COMPOSITE_DEFAULT`, `MCLA_DEV_OVERRIDES` ON, image
  capture and diagnostic tests ON, WGC OFF), `rexglue_DIR` =
  `l59vk-sdk-install`, package registry off; `Found ReXGlue SDK
  0.20.0.2-dev.g141e13f`. The `mcla_app.cpp` compile line carries
  `REX_HAS_D3D12=1 REX_HAS_VULKAN=1`.
- 236/236 steps (first attempt stopped at a test-file string literal error
  of this patch, fixed; then 210 remaining steps), **0 warnings**.
- Host tests: **41 executables, 41 pass** (`l59-tests.log`);
  `mcla_dev_overrides_test` 93 checks; `mcla_diagnostic_export_ui_win_test`
  249 checks, 0 failures this time (it failed 1 check in the l58 run because
  of the desktop state).
- ast-grep 0.50.0 on the scratch tree: scan rc=0, rule tests 3/3
  (`l59-astgrep.log`). Added lines ASCII, at most 79/73/92 columns.

## 5. Candidate

`private/l59-int-input` (`private/l59-ab-work/l59-int-input.sha256`,
`l59-pins.json`):

| File | Bytes | SHA-256 |
|---|---|---|
| MCLA-Launcher.exe | 1373696 | 0A88A4ADB6DAD83D6786901B89F0FAAAFDCE7EF8246BD65D6180494C80BEC8C8 |
| mcla.exe | 53218304 | D86A233DAE6DF4A3BB9C7D364BBDCE227913853F4F99F6BF2CDFA832D52EF43F |
| mcla_crash_handler.exe | 211968 | A94057ACA2EB6C822FAA6DD88F54AC8C3006EA8B3372650FB13512B80DC1BBAB |
| mcla_diagnostic_image_helper.exe | 60928 | 6B44233B7F57D9D4AA7CA76652C983D18DD54AA1CF8EBAC7FDB2A47F7CDC987A |
| rexgpu-xenos.dll | 6735360 | ADA375A9D23E516448A26A3DC85CCC8D00EC2B66121F2FA5522B35619EC1181D |
| rexruntime.dll | 11786240 | 3433F93881EB017B1E86316CD12B4C0613288F89ED2933267A3FC45E24C2F694 |
| TracyClient.dll | 246784 | CBC7D70100FE694EDEAA0DC5789391CFD1AF6CE8F8C687389DE3D9AFA79F655E |

The three DLLs are byte-identical in `l59-build` and `l59vk-sdk-install/bin`.
Path scan (`l59-pathscan.log`): no build, user, worktree or patch path in any
file; the only hits are three-byte opcode patterns (`l5x` in `mcla.exe` and
`rexruntime.dll`) and the launcher's generic `AppData` strings, as in l58;
the KI-045 admission marker is present once. Strings (`l59-strings.log`):
`mcla.exe` holds `MCLA gpu backend: vulkan (dev override)`,
`mcla_gpu_backend_vulkan`, the override file name and
`d3d12_readback_elide_unread`, and not the "ignored" message;
`rexgpu-xenos.dll` holds `MCLA_READBACK v={}`, the read-back cvars and the
Vulkan command processor.

`private/l59-internal-prepare.ps1` (SHA-256
`10BBCE86A06CF9288DFC1C5243B2218D445D4674FC23859BE988EC7788BF5E91`, parses
with 0 errors) differs from `l58-internal-prepare.ps1` only in its header,
the l59 paths, the pinned hashes, `$expected`/`$expectedRuntime`, the
admission label `l59-internal` and the final hint. Run (owner, PowerShell 7):
`C:\BDU\MCLA-Recomp\private\tools\pwsh\pwsh.exe -NoProfile -File C:\BDU\MCLA-Recomp\private\l59-internal-prepare.ps1`.
Plain V1 ticket, stages `private\l59-internal`, internal audit, KI-045
admission row for l59 (existing rows kept). Not run; nothing staged or signed.

## 6. Arm files

Deck (inside Proton; a `.cmd` cannot be run there easily), ASCII, LF, seven
lines each, generated by `l59-patch/tools/mk_arms_l59.py`:

| File | SHA-256 | Content |
| --- | --- | --- |
| `private/l59-deck-arms/d3d12/mcla-dev-overrides.txt` | D7A997A75D03133ABA697434F2CB1962DCE73AEDDC49A666379429710E0E83EA | read-back sync true, every other key false (`mcla_gpu_backend_vulkan=false`) |
| `private/l59-deck-arms/vulkan/mcla-dev-overrides.txt` | 662C900341629211AD88A70A34452F40887DE8B0E65F31C2A9E58B7852CA5D78 | read-back sync true, `mcla_gpu_backend_vulkan=true`, the rest false |
| `private/l59-deck-arms/README.txt` | 6CFD83F5B5C6B513716AD15E6F102B8E6C0747301E92E444F4B272CA3B43987B | Deck path and steps |

Deck path: `~/.local/share/Steam/steamapps/compatdata/<appid>/pfx/drive_c/users/steamuser/AppData/Local/MCLA-R-Launcher-v1/mcla-dev-overrides.txt`
(the `<appid>` folder whose `MCLA-R-Launcher-v1` already holds
`preferences.dat`).

PC smoke (ASCII CRLF, same pattern as l58): `private/l59-ab-vulkan.cmd`
(7C16FC1B359889816EABE14043C021822B69591D2CDBAA3E9C45EC44A8D532AA),
`private/l59-ab-d3d12.cmd`
(E29104D2EAE003C58E0C40CECC56B6A22A89224B789EE59D51874F5E0DC1C1AB),
`private/l59-ab-clear.cmd`
(E6CC1366142BFB9040949C7D09C44A5B4133E1BBBA56CC1D6587B3E5297A60BF).

## 7. Protocol

1. PC (5060): `l59-internal-prepare.ps1`, CHECK SETUP. `l59-ab-d3d12.cmd`, one
   session (log: `backend='d3d12'`, the M10 per-minute lines as in l58); then
   `l59-ab-vulkan.cmd`, one session (log: `MCLA gpu backend: vulkan (dev
   override)`, `backend='vulkan'`; title, one lap, F10; look for obvious
   Vulkan defects); `l59-ab-clear.cmd`.
2. Deck, stock, 1280x800, through the launcher, the M10-001 route; per run:
   copy the arm file, start, check the log lines, F10 at about 60 s and 150 s
   on the title screen, F10 at the start and at the end of one lap, quit.
   Order A B B A (d3d12, vulkan, vulkan, d3d12). Note cold/warm (the first
   Vulkan run builds its own pipeline cache), crash/hang, audio tearing and
   visual checks (photo/vinyl/livery, police livery KI-047, reflections,
   shadows). Delete the file at the end.
3. Decision per M10 priorities: switch the launcher to Vulkan under Wine only
   if Vulkan wins or ties without new defects; Windows stays D3D12.

## 8. What each arm can measure

D3D12-only (absent in the Vulkan arm, by construction): `d3d12_perf_split`
(`MCLA_PERF_SPLIT`, and the host's `MCLA_THREAD_CPU`, which is emitted per
perf-split window), the per-minute block of the D3D12 command processor
(`MCLA frame pacing/min`, `MCLA draws per frame/min`, `MCLA gpu busy/min`,
`MCLA replay/min`, `MCLA_READBACK_SYNC`, `MCLA_READBACK`, the `MCLA_WRM`
report), the read-back sync fix `d3d12_readback_sync_composite` (KI-047), the
read-back elision and GPU read-back timing, `d3d12_livery_diag2`, the
pending-pipeline wait `d3d12_async_pipeline_wait(_always)` used by the photo
capture (KI-037/KI-047) and the batched replay wake
(`d3d12_replay_batched_wake`, `MCLA_REPLAY_WAKE`). The WAIT_REG_MEM event wake
itself lives in the shared command processor, but its report line is printed
only by D3D12 (and the switch is off in both arm files). The shared
`readback_resolve` cvar is honoured by both.

The Vulkan backend has **no per-minute line at all** (no fps, pacing or GPU
busy log); its only telemetry is the cumulative diagnostic snapshot.

Shared by both arms (code outside the backends), in the F10 diagnostics
`state.json`:
- `runtime.guest_output_sequence` and `runtime.guest_vblank_sequence` (the
  presenter's published guest outputs and guest vblanks);
- the base command-processor counters `command_swap_packet_count`,
  `command_draw_packet_count`, `command_ring_idle_us`,
  `command_wait_reg_mem_us`, `command_primary_buffer_*` and the memory
  write-watch costs (`memory_non_mmio_write_fault_*`, `memory_watch_*`).

Not comparable: `graphics.backend`, `guest_frames_opened/closed`,
`guest_frame_wall_us`, `issue_swap_calls`, `submission_count`, fence and
queue times are filled only by the Vulkan command processor's snapshot
override; D3D12 does not override it, so in the D3D12 arm they read 0
(`backend` 0 = unknown, Vulkan 2). The host's `mcla_guest_fps_hud` (M7-016)
is not in the current source.

**Recommended comparison metric:** guest outputs per second from two F10
snapshots of the same run: delta `runtime.guest_output_sequence` / delta
seconds (title: the 60 s and 150 s F10; driving: the lap start and lap end
F10), seconds taken from the game-log timestamps of the two F10 lines, with
delta `guest_vblank_sequence` / seconds (about 60 in stock) as the clock
cross-check and delta `command_swap_packet_count` / seconds (guest swaps) as a
second, independent rate. Report the median of the two runs per arm; this is
the M7-016 "outputs/s" unit (older Deck data: Vulkan 8.86 against D3D12 7.34
outputs/s at the title). Secondary, both arms: delta `command_wait_reg_mem_us`
and write-watch costs per output (CPU-side waits); owner smoothness and
defects. Frame-time distribution (p50/p95, 1% lows) is available only in the
D3D12 arm and cannot decide the A/B.

## 9. Risks

- **Vulkan backend never run in this project's builds.** No Vulkan ctest;
  a Vulkan device or pipeline failure is fatal for the session (message box,
  no fallback). PC smoke first.
- **KI-047 fix and the photo pipeline wait are D3D12-only:** in the Vulkan arm
  the police-livery/read-back defect and photo/vinyl issues may reappear; that
  is a known gap of the backend, not of this switch, and it must be recorded
  as such in the A/B verdict.
- **Asymmetric telemetry:** only the F10 counters of section 8 compare; the
  owner must press F10 at the stated points or the Vulkan arm has no number.
- **D3D12 regression risk:** the D3D12 arm runs a `rexgpu-xenos.dll` that also
  carries the Vulkan code (shared code compiled with `REX_HAS_VULKAN`). The
  D3D12 arm is therefore not byte-identical to l58; compare D3D12 l59 against
  l58 numbers before blaming either backend.
- **Dev SDK, unpinned, local branch not pushed;** Python 3 at SDK build time;
  the ctest manifest temp-dir race (rerun once).
- **Override file in the wrong prefix** (several compatdata folders): the log
  then shows `mcla-dev-overrides.txt not present; build defaults` (D3D12),
  which the README makes the owner check.
- Volunteer builds are unaffected (`MCLA_DEV_OVERRIDES` OFF; the cvar exists
  but defaults to false; launcher unchanged).

## Limits

Build and unit tests only. No in-game run; no physical or owner check is
claimed.

## Primary verification (2026-11-01)

- Patch applied byte-exact to main; `private/l57-host-build` (D3D12-only SDK, so the switch compiles to its "ignored" branch) rebuilt from main with 0 warnings; all test executables pass, `mcla_dev_overrides_test` 93 checks; the UI layout test that failed earlier in the day passes again (desktop state); `git diff --check` and ast-grep clean.
- Deck admission: an owner V1 build starts only if the launcher's `owner-admitted-builds.txt` with the l59 row is present in the same Proton prefix folder as the arm file (`.../AppData/Local/MCLA-R-Launcher-v1/`); copy the PC file there after running the prepare script.
- The F10 graphics block asymmetry is catalogued as KI-059.
