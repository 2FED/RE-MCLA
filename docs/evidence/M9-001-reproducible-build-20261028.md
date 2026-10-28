# M9-001 reproducible source build (2026-10-28)

**Status:** built from published source, checked against the sources of
`l31-s30` and `l33-sunc`, and staged as `private/l34-s30` and
`private/l34-sunc`. Owner manual drive at 2K passed on both (KI-041 flicker on sunc, see below). Automated title checks pass on both (below).

## What changed in the repository

- `third_party/rexglue-sdk`: `5dd58f8` -> `a239b83` (fork branch
  `mcla/mcla-r-hotfixes`, published; verified with `git ls-remote`). It adds
  `678adc4` (32bpp resolve downscale) and `a239b83` (M8 command-processor work
  and unlocked-frame-rate support, `mcla_guest_vblank_hz` default 0).
- SDK package version is `0.20.0.2` (`rexglue --version` prints
  `0.20.0.2-dev.ga239b83`). `CMakeLists.txt` and `CMakePresets.json` now pin
  `0.20.0.2`. `bootstrap.ps1` checks commit `a239b83` and skips the tag check,
  because no release tag points at it (only `v0.20.0.0` -> `5dd58f8` exists).
  Tagging the fork needs owner approval and was not done.
- New CMake option `MCLA_UNLOCKED_VBLANK_HZ` (default 0). When it is above 0,
  `mcla_app.cpp` sets `mcla_guest_vblank_hz` in `OnPostLoadGraphicsPlugin`,
  before the GPU plugin's `Setup` reads it and starts the vblank thread, unless
  the user set the cvar explicitly. It follows the existing `readback_resolve`
  pattern in the same function. This replaces the private edit of the SDK
  default (`0` -> `240`) that produced `l33-sunc`; the launcher passes settings
  on the command line and ignores `mcla.toml`, so a config file could not do it.
  M9-003 replaces the option with a launcher mode choice.
- **32bpp patch step:** no tracked build script applied
  `patches/rexglue/resolve-downscale-32bpp.patch`; it was applied by hand to
  private SDK copies (`sdk-rb32-s`, then `sdk-l18-s` via the timebox scripts).
  The fix is now in the SDK (`678adc4`), so nothing applies it any more. The
  patch and manifest stay as history and as inputs of the standalone shader
  regression in `tests/resolve-downscale`.

## What l31-s30 and l33-sunc were made of

| Part | l31-s30 / l33-sunc | Source |
|---|---|---|
| `mcla.exe` (same file in both) | built 28.10 from `private/l13-src` against the `sdk20install-unpublished` headers | l13-src is an older copy (VERSION 0.19.0.0) plus the raw timer |
| `rexgpu-xenos.dll` | built from `private/sdk-l18-s` | s30: vblank default 0 (l29 build); sunc: vblank default 240 |
| `rexruntime.dll`, `TracyClient.dll` | built 11.10 from `private/sdk-rb32-s` | carried since l9 |
| Launcher, image helper | l12 binaries | unchanged |

## Source equivalence

- `a239b83` against `sdk-l18-s`, every tracked file, line endings normalized:
  one difference, `src/graphics/graphics_system.cpp` line 43, vblank default
  `0` (published) against `240` (l33-sunc). So l31-s30's GPU source (vblank 0)
  equals `a239b83`.
- `678adc4` against `sdk-rb32-s`: no differences. `a239b83` changes only
  graphics files, so the runtime source equals l31's runtime source.
- Nested SDK dependencies: all 22 recursive submodules are at the commits
  pinned by `a239b83` (fresh recursive clone from GitHub).
- Repository against `l13-src`, files that go into `mcla.exe`:
  - `mcla_app.cpp`: one log string ("light" instead of "fast") and a comment.
  - `VERSION` (0.20.37.0 against 0.19.0.0): only the embedded version string.
  - `mcla_build_lease_runtime_win.*`: a `read_only` parameter used by the
    launcher. It is not linked into `mcla.exe`, because `MCLA_TEST_BUILD_EXPIRY`
    is OFF in both builds.
  - The other differences are launcher and test sources, which are not in
    `mcla.exe`.
- One more difference: l31's `mcla.exe` was compiled against older SDK headers
  and then run with a newer GPU DLL. l34 is compiled and run against the same
  SDK headers.
- Codegen: a fresh `rexglue codegen` with the published SDK (game path pointed
  at the hash-verified `private/p1/game/default.xex`) produced 67 files,
  133,916,828 bytes, byte-identical to `l13-src/generated/default` and to the
  accepted `generated/default`.

## Build configuration

SDK (`private/l34-sdk-b`, Ninja Multi-Config, Release, installed to
`private/l34-sdk-install`) uses the configuration of the l31/l33 GPU build
(`sdk-l18-b`): clang 20.1.8, `-march=x86-64-v3 -gcodeview -g`, linker
`/DEBUG /OPT:REF /OPT:ICF`, Tracy ON, perf counters ON, Vulkan OFF,
FidelityFX OFF. Tests ON (builds test binaries only). Two deliberate
differences: C sources also get `-march=x86-64-v3` (the preset default), and
`/DEBUG` also applies to the runtime, which `sdk-rb32-b` linked without debug
info. Both are code-generation neutral or limited to third-party C. A first attempt
without the linker flags gave larger DLLs (GPU 3.31 MB) and was discarded.
Windows file links of `libmspack` (15) were materialized from their pinned
targets in the clone only.

`mcla.exe` (`private/l34-build`, Ninja Release): clang 20.1.8,
`MCLA_RAW_TIMER_DEFAULT_ON=ON`, `MCLA_TEST_BUILD_EXPIRY=OFF` (as l13-build),
`MCLA_UNLOCKED_VBLANK_HZ=0` for s30 and `240` for sunc. The unlocked
reconfigure recompiled only `mcla_app.cpp`.

## Results

- SDK build and install: pass. SDK unit tests (Release): 1864 of 1864 passed,
  4 known BitStream skips.
- `mcla.exe` and `mcla_crash_handler.exe`: pass, no compiler warnings.

| File | Bytes | SHA-256 |
|---|---|---|
| `out-s30/mcla.exe` | 52,703,744 | `A50E30C64128F4FD542B3B75DD7A543B7BAE020F803E0BBAA5B23F43505D9FD1` |
| `out-sunc/mcla.exe` | 52,704,256 | `766899067ECF517F7BD022023885069416D6A17A93AA321966E56873C70C7495` |
| `rexgpu-xenos.dll` (both) | 2,991,104 | `13E4AC9A03F1FE4412F007D8A2961EAD0566829F3EC87F1C6DE7A2B8594EC166` |
| `rexruntime.dll` (both) | 10,954,240 | `978189C7C7795E727C35BBE6D7AACCD5A195A05DAB06B99FFDEF65AE9EBB95D1` |
| `TracyClient.dll` (both) | 246,784 | `961A2F1DD3677B87A5184704841519CF949989961778975D7AA430344919ED16` |
| `mcla_crash_handler.exe` | 194,560 | `BF908060B544BC0A36F67E5BD5AF8D32A96FDB3DF1D97E8C58C5491D326A6D7E` |

Sizes against l31/l33: GPU DLL 2,991,104 against 2,990,592, runtime
10,954,240 against 10,951,680, Tracy identical. The few hundred bytes to
2.5 KB match the new version stamp and debug directory.

Binary behaviour switches (string inventory, no launch):

- Both `mcla.exe`: raw timer and `mcla_raw_timer_ms_steps` present, light
  readback `some` message, test-build admission absent (expiry OFF).
- Only the unlocked `mcla.exe` contains the unlocked vblank startup message.
- GPU DLL contains the same switches as l33's: `mcla_guest_vblank_hz`,
  `mcla_guest_vblank_unlock_delay_s` (45), `mcla_ui_cap_draws_low/high`,
  `d3d12_streaming_replay`, `clear_memory_page_state`, the tear-free present
  message and the vblank log line. Their defaults are verified at source level
  above (a239b83 = the l29/l33 source with vblank 0).

## Staging

- PowerShell 7.4.22 (official `win-x64` zip from GitHub) is in `private/tools/pwsh`.
  The zip SHA-256 `5F89F518...6359` matches the release `hashes.sha256` and the
  GitHub asset digest; `pwsh.exe` has a valid Microsoft Corporation signature.
  The agent session may not run downloaded binaries, so the owner ran
  `private/l34-sign-and-stage.ps1`. It checks the `mcla.exe` hashes, signs the
  tickets with the current-user key and calls `Stage-MclaInternalLauncher.ps1`.
- `l34-s30` ticket: host `A50E30C6...`, created 1793196759. `l34-sunc` ticket:
  host `766899067E...`, created 1793196761.
- Both receipts list 17 files and all hashes verify on disk. The launcher (l12
  binary `DB0D84A5...`), CRT and image helper are the same files as in l31-s30.
  Receipts mark no external delivery, no game assets and no personal data.

## Automated title start checks (2026-10-28, owner-approved and announced)

Same procedure as the l31-s30 / l33-sunc baselines: direct `mcla.exe` from the
staged `build` folder, empty isolated profile under `private/m9-001-runs`,
1280x720 windowed (swapchain 1600x900 as in the baselines), about 170 s on the
title and attract scene, then a normal window close. No owner profile or save
was used. An automated run cannot drive on an empty profile, so the drive part
remains for the owner.

**First series (16:14-16:21 Kyiv): invalid for performance.** DXGI picked the
integrated AMD Radeon 860M. The RTX 5060 now drives the 4K display, so it is
no longer the first adapter, and the direct launch used `d3d12_adapter=-1`
(first physical adapter). The l12 launcher pins the adapter by name, so
launcher runs are not affected. Functional results still matched the
baselines.

**Second series (16:22-16:28 Kyiv), `--d3d12_adapter=1`, log confirms RTX 5060:**

| Run | Alive 170 s / normal close / crash dirs | FPS min 1 / 2 | GPU busy ms/frame | frames >50 ms (min 2) |
|---|---|---|---|---|
| l31-s30 baseline | yes / yes / 0 | 29.2 / 29.8 | 10.3 / 15.5 | 11 |
| **l34-s30** | yes / yes / 0 | 29.6 / 30.0 | 11.2 / 17.3 | 0 |
| l33-sunc baseline | yes / yes / 0 | 31.9 / 45.7 | 11.3 / 16.3 | 1 |
| **l34-sunc** | yes / yes / 0 | 37.6 / 63.6 | 10.4 / 14.6 | 0 |

- **Behaviour switches, identical to the baselines:**
  - UI cap on at 3 draws and off on the 3D title, on again at 138 or 236 draws.
  - sunc vblank: 60 Hz, then 240 Hz at about 45 s, then 60 Hz under the UI cap.
  - Tear-free present on sunc only.
  - Raw timer applied.
  - The unlocked startup message appears only in l34-sunc.
- **sunc minute 2:** the title scene drew about 5.1 k draws per frame against
  7.9 k in the baseline run, so the higher frame rate partly reflects scene
  content; it is not a measured speed-up.
- **Error lines:** 10 in every log (l31/l33 baselines and both l34 runs), the
  known `t:\` VFS probes.
- **Not covered:** driving, races, saves and the race timer. These need the
  owner's manual check on a real profile through the launcher.

## Owner manual check (2026-10-28)

The owner drove both staged builds through the launcher at 2K: "all good" on both. One finding on `l34-sunc` only: at night, light flickers slightly and bushes flicker constantly while lit by the headlights when driving past. It is logged as KI-041. l34 matches l33-sunc at source and codegen level, so it is probably a pre-existing unlocked-mode effect; a 720p repro and the same route on `l33-sunc` will classify it. M9-001 is closed on this result; KI-041 goes to M9-006 unless it turns out to be an l34 regression.

## Spark routing

Not used: the work was exact file and hash comparison and builds, which are
cheaper to do directly than to validate a model answer.

## Open

1. Done 2026-10-28 16:12 Kyiv, see Staging below.
2. Owner title-plus-drive check on both: 30 cap on s30; on sunc 60 Hz until
   45 s, then 240 Hz with the UI cap, as in the l33 log.
3. `VERSION` stays 0.20.37.0 because it is embedded in the tested binaries;
   bump it with the next host change.
4. Legacy M7 portable-bundle scripts still require SDK 0.20.0.0 / `5dd58f8`;
   they are not on the M9 build path.
5. Optional: tag `a239b83` on the fork (owner approval needed).
