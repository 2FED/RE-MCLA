# M10 l63: combined owner-internal build for the morning PC and Steam Deck tests (2026-11-01)

Status: **SDK review fixes committed locally and unit-tested; SDK (D3D12 + Vulkan) and host
built and tested; candidate input, prepare script, arm files and the morning checklist ready.
Not signed, not staged, not run in the game.** Built by an Opus subagent for the primary agent.
Nothing was launched (no `mcla.exe`, no launcher, no game); no save, profile, LocalAppData or
registry write; no download; no git write except the one local SDK commit. Spark: not used
(a small source fix with deterministic tests and builds; no log to triage or extraction job
whose validation would be cheaper than doing it directly).

l63 puts every M10 change of 2026-11-01 into one package behind the dev-override file, so the
owner compares arms by switching one text file instead of changing builds:
l57 CPU switches, l58 read-back elision, l59 Vulkan backend switch, l60 store seeding, l61
targeted read-back wait, l62 watch census / disarm bridging / upload-ahead, plus the review
fixes of `M10-watch-review-e57fd88-20261101.md`.

## 1. SDK fix commit (review items S1-S4, N1, N5)

| Item | Value |
|---|---|
| Worktree / branch | `private/l62-sdk-wt`, `mcla/m10-watch` (local, not pushed) |
| Commit | `8081f63f0ab98a03672cbc25f90a436f1c2bbb4f`, parent `e57fd88` |
| Files | `include/rex/graphics/shared_memory.h`, `include/rex/system/watch_census.h`, `src/graphics/shared_memory.cpp`, `src/graphics/d3d12/command_processor.cpp`, `tests/unit/memory/watch_census_test.cpp` (5 files, +218 / -77) |

- **S1** `WatchCensusReport`: under the global critical region only the 2,048-block hotness walk
  (it needs the watch buckets), copies of the lock-side counters and the window reset; every
  `fmt` call and allocation runs after the lock is released. `WatchCensusFrameEnd` no longer walks
  the blocks under the lock: the closed frame's fault bitmap is swapped out under it (O(1)), the
  next history is built outside it by the command processor thread (the only writer of
  `history`; other threads read it only under the lock) with the new pure
  `watch_census::AdvanceHistories`, and swapped in under the lock (O(1)). Two short lock holds
  per frame instead of one 2,048-iteration hold.
- **S2** `SetPages` uses `std::popcount`. With `memory_watch_census=false` no shadow work runs at
  all: feeding was already census-gated; now the invalidation callback, `ClearCache` and
  `SetSystemPageBlocksValidWithGpuDataWritten` skip the shadow clears too. Switching the census
  on already clears the shadow, so no stale shadow survives an off period.
- **S4** (trivial with S2) `SetSystemPageBlocksValidWithGpuDataWritten` is lock-free again when
  the census is off (lock + 32 KiB clear only with the census on).
- **S3** the memexport `ScopedRequestSource` is scoped to the memexport requests (inside
  `if (memexport_used)`), so later requests of the same `IssueDraw` are no longer counted as
  `src_memexport`.
- **N1** `census_thread_id_` and `request_source_` are `std::atomic` (relaxed).
- **N5** `static_assert(kHostGpuMemoryOptimalSparseAllocationLog2 >= 6 + 12, ...)` in
  `shared_memory.h`.
- Behaviour with the census on is unchanged apart from where the lock is held; line format
  unchanged. Changed lines clang-format 20.1.8 clean (`clang-format-diff.py` on the diff: no
  changes), ASCII, `git diff --check` clean.
- New tests (`[watch-census]`): SetPages against a page-by-page reference (5,000 random cases,
  count and resulting bitmap); AdvanceHistories per block against AdvanceHistory over 130 blocks
  (three fault words), in place (out == in), and zero blocks. `[watch-census]`: 19 cases, all
  pass.

## 2. SDK build l63vk (D3D12 + Vulkan)

| Item | Value |
|---|---|
| Script | `private/l63vk-work/l63vk-sdk.ps1` (= `l59vk-sdk.ps1` with source `l62-sdk-wt` and the l63vk paths) |
| Flags | Ninja Multi-Config, clang 20.1.8, VS 2022 Build Tools, `-march=x86-64-v3`, `REXGLUE_USE_D3D12=ON`, `REXGLUE_USE_VULKAN=ON`, FidelityFX OFF, Tracy ON, perf counters ON, tests ON, `CMAKE_FIND_USE_PACKAGE_REGISTRY=OFF`, `PYTHONDONTWRITEBYTECODE=1`, `VULKAN_SDK` unset, prefix map to `./third_party/rexglue-sdk`, `/pdbaltpath` |
| Out-of-source | build `private/l63vk-sdk-b`, output `private/l63vk-sdk-o`, install `private/l63vk-sdk-install` |
| Logs | `private/l63vk-work/` configure.log, build.log, install.log, ctest.log, ctest2.log, reg-before.txt, reg-after.txt |

- Configure: `Graphics: D3D12=ON Vulkan=ON`. Build 1848 steps, rc=0. 912 `warning:` lines; the
  set of (file, message) pairs and their counts is identical to the l59vk build (no new
  warning; all pre-existing SDK/vendored).
- Worktree untouched: `find private/l62-sdk-wt -newer private/l63vk-work/marker-start` (outside
  `.git`) empty after build, install and ctest; no `__pycache__` in the worktree (nothing to
  remove); `git status` clean.
- Install ran with the Windows directories removed from `PATH`: HKCU CMake package registry
  `reg query` before and after identical (no l63vk row). 1,285 files, 195 MB.
  `rexglue.exe --version` = `0.20.0.6-dev.g8081f63`.
- ctest: run 1 2003/2004 (the known `unit.Manifest` `%TEMP%\manifest_test` remove_all race:
  "sdk_version is nullopt when absent"); run 2 on the same build **2004/2004**, 4 skipped (the
  known BitStream Write cases). 2004 = l62's 2002 + the 2 new cases. Tagged suites on
  `l63vk-sdk-o/Release/unit_tests.exe`: `[watch-census]` 19, `[perf-split]` 14,
  `[pipeline-wait]` 10, `[livery-diag2]` 18, `[wrm]` 11, `[replay-wake]` 7,
  `[readback-slots]` 8 cases, all pass. No Vulkan-specific test exists (compile only).

| `l63vk-sdk-install/bin` | Bytes | SHA-256 |
|---|---|---|
| `rexgpu-xenos.dll` | 6777856 | `29CFE7A0DCD789CAAE2F992ED7E2E7030D7F13B3C065419A6130BC670143B21B` |
| `rexruntime.dll` | 11808768 | `2108DAA113F81A8566F6ED2D9F99F2362A11EE887EA3C1F9E2C2FA06F0486F5A` |
| `TracyClient.dll` | 246784 | `752DDD0815F4D9B1B74666385B5F794BA48266E6C40E643C89EA824D1F42E7DA` |
| `rexglue.exe` | - | `E708B0379C1E3149CC79743685B32B50AE1C7546EC52A620841CD44E34988CE7` |

## 3. Host build l63

- Source: the main checkout at `b714e6b` (tracked sources unchanged; no host change was needed;
  `git status` shows only the pre-existing `third_party/rexglue-sdk` modification and the old
  untracked M8 files, which CMake does not list).
- Scripts `private/l63-work/l63_host_build.ps1` (configure / build), `l63_host_tests.ps1`.
  Build `private/l63-build`, `rexglue_DIR` = `l63vk-sdk-install` (`Found ReXGlue SDK
  0.20.0.6-dev.g8081f63`), package registry off.
- Options (CMakeCache confirms): `MCLA_TEST_BUILD_EXPIRY`, `MCLA_RAW_TIMER_DEFAULT_ON`,
  `MCLA_BUILD_IMAGE_CAPTURE_TESTS`, `MCLA_BUILD_DIAGNOSTIC_TESTS` ON; `MCLA_EXPERIMENTAL_F10_WGC`
  OFF; `MCLA_PERF_SPLIT_DEFAULT`, `MCLA_LIVERY_DIAG2_DEFAULT`,
  `MCLA_READBACK_SYNC_COMPOSITE_DEFAULT`, `MCLA_DEV_OVERRIDES`, `MCLA_PREWARM_SEED_DEFAULT` ON;
  `REXSDK_VERSION` empty; prefix maps (checkout to `.`, SDK install to `sdk`);
  `CMAKE_EXE_LINKER_FLAGS=-Xlinker /pdbaltpath:%_PDB%`.
- Targets: the l41 list plus wine path policy, perf split, livery diag, read-back sync, internal
  launcher, fake image helper, native refcount, dev overrides, shader store seed and launcher
  shader seed tests (48 targets); the 43 `*_test` targets of `cmake --target help` are all
  built. 246/246 steps, **0 warnings** (`l63-build.log`).
- Host tests: **43 executables, 43 pass** (`l63-tests.log`): `mcla_dev_overrides_test` 142
  checks, `mcla_shader_store_seed_test` 294, `mcla_launcher_shader_seed_win_test` 46,
  `mcla_diagnostic_export_ui_win_test` 249 / 0 failures, `mcla_wine_path_policy_test` 208.
- ast-grep 0.50.0: scan of the tracked `src`/`tests` sources rc=0, rule tests 3/3
  (`l63-astgrep.log`).

## 4. Candidate

`private/l63-int-input` (`private/l63-work/l63-int-input.sha256`, `l63-pins.json`); the three
DLLs are byte-identical in `l63-build` and `l63vk-sdk-install/bin`.

| File | Bytes | SHA-256 |
|---|---|---|
| MCLA-Launcher.exe | 1445376 | 4DD4EA4511D3A57625352AD5EAE2FE41FA36A37D66005D0328391DF5935B888B |
| mcla.exe | 53243904 | 6C152DAC487DF6509BA7D5B6BB0E92F2D1D7B4BCC6DD8324D4C6B4D4CB588B9C |
| mcla_crash_handler.exe | 211968 | C0AA16A5B5A48A23C0CA6F39257353AA0281C359BF56DD33DBD87CC91449AC4A |
| mcla_diagnostic_image_helper.exe | 60928 | 781E1604A3C108324B0A916419C3D3A3109B8022592676785043F455B4F942B3 |
| rexgpu-xenos.dll | 6777856 | 29CFE7A0DCD789CAAE2F992ED7E2E7030D7F13B3C065419A6130BC670143B21B |
| rexruntime.dll | 11808768 | 2108DAA113F81A8566F6ED2D9F99F2362A11EE887EA3C1F9E2C2FA06F0486F5A |
| TracyClient.dll | 246784 | 752DDD0815F4D9B1B74666385B5F794BA48266E6C40E643C89EA824D1F42E7DA |

- Path scan (`private/l63-work/l63_scan.py`, `l63-scan.log`): no build, user, worktree, temp or
  patch path in any file. Remaining hits are three-byte opcode patterns (`l5x`/`l6x` inside
  instructions in `mcla.exe`, `rexruntime.dll`, `MCLA-Launcher.exe`), the launcher's generic
  `AppData` strings and its `private/save-arc...` archive-entry names, as in l58/l59. KI-045
  admission marker present once in `mcla.exe`.
- Strings: `mcla.exe` holds the override file name, `MCLA dev override`, every one of the 14
  whitelisted names, `shared_memory_upload_ahead_pages`, `conflicting upload-ahead keys` and
  `MCLA gpu backend: vulkan (dev override)` (and not the "no Vulkan backend" message);
  `MCLA-Launcher.exe` holds `prewarm_seed_store`, `store-seed.txt` and `MCLA store seed:
  source=` (the override file name is built from the constexpr name at run time and is not a
  literal there; the launcher's seeding file is compiled with `MCLA_DEV_OVERRIDES=1`);
  `rexgpu-xenos.dll` holds `MCLA_WATCH v=1`, `MCLA_READBACK v={}`, `MCLA_READBACK_SYNC v=1`, the
  targeted fields (`targeted_waits=`, `targeted_wait_ms=`, `fallbacks_to_full=`, `kicks=`),
  `d3d12_readback_sync_targeted`, `d3d12_readback_sync_kick_draws`,
  `shared_memory_upload_ahead_pages`, `ahead_shadow`, `ahead_extended_runs`, the WRM / replay /
  elide / composite cvars, `VulkanCommandProcessor`, `requested backend`; `rexruntime.dll` holds
  the three `memory_watch_*` cvars and `vulkan-1.dll`.
- `private/l63-internal-prepare.ps1` (SHA-256
  `9ACC15A755CDBB62EDDBE0C18430B86EB9A316262BE5B592F99955744B5D2717`, CRLF, parses with 0 errors
  in Windows PowerShell and PowerShell 7) is generated from `l59-internal-prepare.ps1` by
  `private/l63-work/mk_prepare_l63.py`: header, l63 paths, pinned hashes,
  `$expected`/`$expectedRuntime`, an empty carried-row table (only the new `l63-internal` row is
  added; every existing valid row of the per-user file is kept) and the final hint. Plain V1
  ticket, stages `private\l63-internal`, internal audit. Not run; nothing signed or staged.

## 5. Arm files

Generated by `private/l63-work/mk_arms_l63.py` (hashes in `l63-work/l63-arms.sha256`). ASCII,
LF, 14 lines each, every whitelisted key written explicitly (so no arm inherits a value from an
earlier one); PC and Deck copies are byte-identical.

| Key | base | all | all-vulkan |
|---|---|---|---|
| mcla_readback_sync_composite | true | true | true |
| mcla_native_d3d_refcount | false | true | true |
| mcla_native_xapi_interlocked | false | true | true |
| d3d12_wrm_event_wake | false | true | true |
| d3d12_replay_batched_wake | false | true | true |
| d3d12_readback_elide_unread | false | true | true |
| mcla_gpu_backend_vulkan | false | false | true |
| prewarm_seed_store | true | true | true |
| d3d12_readback_sync_targeted | false | true | true |
| memory_watch_census | true | true | true |
| memory_watch_probes | false | false | false |
| memory_watch_disarm_bridge_safe_gaps | false | true | true |
| shared_memory_upload_ahead_16 | false | true | true |
| shared_memory_upload_ahead_64 | false | false | false |

| File | Bytes | SHA-256 |
|---|---|---|
| `private/l63-arms/{pc,deck}/base/mcla-dev-overrides.txt` | 448 | D407D3E7C51786EAED3F9B5085E0810DC3AD233742AF100660E322B3AB438F3B |
| `private/l63-arms/{pc,deck}/all/mcla-dev-overrides.txt` | 440 | 61C387694A7396D4DAC7221F0B263E44FBCFBFE9CB3A62A68E6CCC5EC3A43DAE |
| `private/l63-arms/{pc,deck}/all-vulkan/mcla-dev-overrides.txt` | 439 | E6DD444AB582C81843E93BBB72E4E07FE2719490668C6A7E2C21829228647BC6 |
| `private/l63-arm-base.cmd` | | E080BABD713B7EA9E0E2E8AF12E0FB4A6A1AF2FBE72ADCCD3EF9C3428DF0823E |
| `private/l63-arm-all.cmd` | | F79ECC5C9B20765FF5D0951B277F856BCE95C65D79809BB8652C83E198587329 |
| `private/l63-arm-all-vulkan.cmd` | | DE25967F2B999F21BB05D8E12D6687ADB6666E02412FA5DD889CC86E1AF7863E |
| `private/l63-arm-clear.cmd` | | B65BBD838696C49E4A56F23A8D060B9353C9BD7D70B4347E5AD2D9CF92B89522 |

The PC `.cmd` helpers (ASCII CRLF) follow `l57-ab-*.cmd`: refuse while `mcla.exe` runs, require
`preferences.dat`, write the file and print it, check the write with `||`; `clear` deletes it.
Parser check: `private/l63-work/check_arms.exe` (compiled from `check_arms.cpp` against the
production header `src/mcla_dev_overrides.h`, clang++ 20.1.8 `-std=c++23 -Wall -Wextra`, no
warnings) parsed all six arm files and the `echo` lines of the three writing `.cmd` files:
status parsed, 14 values, 0 ignored lines, every value as in the table, 0 mismatches
(`l63-arms-parse.log`).

## 6. Morning protocol

`private/l63-MORNING.txt` (ASCII CRLF, SHA-256
`FEAD1E2A8FEDFC082CAD03C94617D7BDF5911E840BDB0B8763B4DE259487E00B`) is the owner's short list:
1. sign with `l63-internal-prepare.ps1` (pwsh 7), Build folder `private\l63-internal\build`,
   CHECK SETUP;
2. PC BENCH-C 1440p Unlocked: one warm-up session on base (check `store-seed.txt` says
   `source=seeded`), then base / all / base / all (ABAB) on the usual loop, then 720p base + all;
   F10 at the usual marks and at any broken police livery; `l63-arm-clear.cmd`;
3. PC Vulkan smoke: all-vulkan, title + 30 s driving, quit; a "Failed to load GPU plugin"
   message box means skip Vulkan on the Deck;
4. Deck (if brought): copy `l63-internal/build` and `owner-admitted-builds.txt` (into
   `.../compatdata/<appid>/pfx/drive_c/users/steamuser/AppData/Local/MCLA-R-Launcher-v1/`, the
   l59 README pattern), then title 60/150 s F10 + one loop per arm in the order base, all,
   all-vulkan, all, base, copying the arm file before each session; delete it at the end;
5. send back zipped session and F10 folders (PC BENCH-C profile, Deck profile) and a few words
   per arm.

Each session log should show `MCLA dev override: <name>=<value>` for every applied key; the
Vulkan arm also `MCLA gpu backend: vulkan (dev override)` and `backend='vulkan'`.

## 7. What each counter answers

| Question | Where (base vs all) |
|---|---|
| Is all faster overall? | `MCLA frame pacing/min` p50/p90, fps, `MCLA gpu busy/min`, `MCLA_PERF_SPLIT` CP time, `MCLA_THREAD_CPU` |
| KI-047 sync cost (targeted wait + kicks) | `MCLA_READBACK_SYNC` `forced`, `wait_ms`, `max_wait_ms` (both arms); all only: `targeted_waits`, `targeted_wait_ms`, `targeted_fence_ms`, `targeted_backlog_draws`, `fallbacks_to_full`, `kicks`, `kick_ms` |
| Read-back elision correct and useful | `MCLA_READBACK`: `copies_now`, `bytes_cpu_copied`, `misses`, `forced` equal across arms; `guard_misses=0`; elided share about 0.74; `gpu_readback_ms_per_frame` |
| CPU wait switches | `MCLA_WRM event_wake=` and episode counts; `MCLA_REPLAY_WAKE batched=` |
| Watch path (Deck lever) | `MCLA_WATCH v=1`: base gives `bridge_shadow_saved_calls` and `ahead_shadow16` projections; all gives real `bridged_gaps`, `disarm_calls`, `arm_calls`/`arm_ms`, `ahead_extended_runs`/`ahead_extra_pages`/`ahead_lock_misses`, `src_*=up_kb`, `faults_*`, `arm_hold_ms`/`disarm_hold_ms` |
| Store seeding worked | `store-seed.txt` `source=seeded`; host `MCLA shader store at startup`; minute-1 pipeline waits and skips |
| Police livery | KI-047 diag v2/v3 counters and the F10 at a broken livery, both arms |
| Vulkan vs D3D12 (Deck) | F10 `state.json`: delta `runtime.guest_output_sequence` / seconds (title 60-150 s, loop start-end), `guest_vblank_sequence` clock check, `command_swap_packet_count`; D3D12 per-minute lines are absent in the Vulkan arm by design (KI-059) |

Correctness checks in every arm: `watch_protect_failures=0`, no "Recovered stale physical page
protection", no "Failed to get an upload buffer", no `DEVICE_HUNG`, garage/driving vinyls,
photos, police livery, shadows and reflections as in base.

## 8. Risks

- **First in-game run of most of this code**: none of the l57-l62 switches nor the review fixes
  has run in the game. "all" turns eight changes on at once; if it misbehaves, the single arms
  of l57/l58/l62 (or a narrowed override file) isolate the cause.
- **Census lock changes (8081f63)** are not runtime-measured. Between the two short lock holds
  of a frame end, a fault lands in the next frame's bitmap and a fault-time class lookup sees
  the previous history (hotness attribution shifts by at most one frame; counts unchanged).
- **Upload-ahead / bridging** (steps 1-2) carry the risks of `M10-watch-steps0-2-20261101.md`
  (extra upload bytes; alias-heap read-only exposure equal to today's exact path).
- **Targeted read-back wait** may fall back to the full wait (`fallbacks_to_full`); kicks add
  mid-frame submissions.
- **Vulkan**: never run in this project's builds; a device failure is fatal for the session
  (message box, no D3D12 fallback). The KI-047 fix, read-back counters and photo pipeline wait
  are D3D12-only, and the Vulkan command processor writes no `MCLA_WATCH` line (memory-side
  counters only). The D3D12 arms run a `rexgpu-xenos.dll` that also carries the Vulkan code.
- **Store seeding**: a new fingerprint; if `store-seed.txt` says `cold`, the first session is a
  cold one and the ABAB pairs must start after a warm-up.
- **Forgotten arm file** stays active for every later `MCLA_DEV_OVERRIDES` build; the checklist
  ends with clear (PC) and delete (Deck).
- Dev SDK, unpinned, local branch `mcla/m10-watch` not pushed (now at `8081f63`); ctest
  manifest temp-dir race needed one rerun; volunteer builds unaffected (`MCLA_DEV_OVERRIDES`
  OFF, CMake defaults unchanged).

## Limits

Build and unit tests only. No in-game run; no physical or owner check is claimed.
