# M10 l66b host: hot-blocks dev-override keys and l66 A/B arms (2026-11-03)

Status: implemented, unit-tested and built in a scratch owner build. Not run on a Deck or a PC,
no physical PASS. Base: main `e61d897` (contains `88fdc01`, the l66 host patch). SDK: the
hot-blocks SDK of [M10-hot-blocks-sdk-20261103.md](M10-hot-blocks-sdk-20261103.md)
(`private/l66-sdk-install`, ReXGlue 0.20.0.7, branch `mcla/m10-hotblocks` `ff30a86`, local).

## 1. Keys (`src/mcla_dev_overrides.h`)

Two bool convenience keys for the integer SDK cvar `shared_memory_hot_blocks` (0 off, 1 shadow,
2 on), exactly like the upload-ahead pair; phase graphics plugin (the cvar is registered by
`rexgpu-xenos.dll`):

| Key | `=true` writes | `=false` |
|---|---|---|
| `shared_memory_hot_blocks_shadow` | `shared_memory_hot_blocks=1` | writes nothing (logged "leaves ... unchanged") |
| `shared_memory_hot_blocks_on` | `shared_memory_hot_blocks=2` | writes nothing |

- Both `true` in one file is a conflict: neither is applied, the later line is logged
  (`conflicting keys for one cvar (neither is applied)`). The conflict rule is now a table
  `kConflictPairs` (upload-ahead 16/64, hot blocks shadow/on) instead of the hard-coded
  upload-ahead pair; behaviour of the upload-ahead pair is unchanged. The log text changed from
  `conflicting upload-ahead keys ...` to the generic text above.
- Integer values stay rejected in the file (`shared_memory_hot_blocks=2` and
  `shared_memory_hot_blocks_on=2` are malformed lines; the bare cvar name is not whitelisted).
  K, N, upload cap and probe frames stay command-line only.
- Whitelist 14 -> 16 keys (`kKeyCount`). An arm file with every key is 511-512 bytes (limit
  1024).
- The runtime (`mcla_dev_overrides.cpp`) is unchanged: it already maps `cvar`/`true_text`/
  `false_text` generically and records `FileValue` for `=false` lines.

## 2. M10 defaults never-written set (`src/mcla_m10_defaults.h`)

New `kNeverWrittenCvars` (12 entries) with `IsNeverWritten()` and a `static_assert` that no
`kDefaults` entry is in it: `memory_watch_probes`, `mcla_readback_sync_composite`,
`d3d12_readback_sync_composite`, `d3d12_readback_sync_kick_draws`, `mcla_native_d3d_refcount`,
`mcla_native_xapi_interlocked`, `mcla_gpu_backend_vulkan`, and the SDK hot-block cvars
`shared_memory_hot_blocks`, `shared_memory_hot_k`, `shared_memory_hot_n`,
`shared_memory_hot_upload_cap_kb`, `shared_memory_hot_probe_frames`. Hot blocks must not become a
volunteer default until an owner A/B accepts them; only the owner keys above set the mode.
`kDefaults` and `mcla_m10_defaults.cpp` are unchanged (the volunteer default set is the l66 one).

## 3. Tests

- `tests/mcla_dev_overrides_test.cpp` (173 checks, was 142 in l63): 16 keys; both new names found, the integer
  cvar, an `_off` key, numeric keys, K/N/cap/probe and a prefix not whitelisted; graphics-plugin
  phase; `_shadow=true` writes `1`, `_on=true` writes `2`, `=false` writes nothing; keys that share
  a cvar are exactly the `kConflictPairs`; shadow arm, on arm (`shadow=false` + `on=true` is no
  conflict), both true (neither, conflict on the later line 3), upload-ahead 16 + hot on (no
  conflict), both pairs in conflict in one file (two conflicts on their later lines), repeated key
  (first wins), integer values rejected; the three l66 arm contents parse to 16 values with 0
  ignored lines; constexpr hot-blocks conflict; the generic conflict log text.
- `tests/mcla_m10_defaults_test.cpp` (2412 checks, was 2313): no never-written cvar is in `kDefaults`, no
  duplicates, `shared_memory_hot_blocks` and the four tuning cvars are never written, defaults
  (`shared_memory_upload_ahead_pages`, `memory_watch_census`) are not in the set, both hot-block
  keys write a never-written cvar, and every non-launcher override key writes either a default
  cvar or a never-written one (exactly one), so a future key forces a decision there. The
  existing table/decision/exhaustive checks are unchanged (still 8 keyed defaults, 26 apply cases).

## 4. Owner A/B arm files (`private/l66-arms`, not in Git)

Every whitelisted key written explicitly, ASCII, LF. Base of all three = the l63 "all" arm
(`private/l63-arms/deck/all`, identical to `pc/all`; the generator checks its 14 lines) plus the
two new keys `false`. PC and Deck files are identical.

| Arm | Extra vs l63 all | SHA-256 (pc and deck) |
|---|---|---|
| `all` | `_shadow=false`, `_on=false` | `D4A5396E25DC7114DEBD2124BB66FBFB850CA07B4042933C61FA53423D243CE5` |
| `hot-shadow` | `_shadow=true`, `_on=false` | `77F5A55DE3199A29389DC8B687DA99A36499CBB4C81BA0216F1F989C3E59AD6F` |
| `hot-on` | `_shadow=false`, `_on=true` | `ED8140871276D095EC0377DCB4F023BB6940177DC06301E8015B3A05457748DA` |

PC helpers (ASCII, CRLF, pattern of `private/l63-arm-*.cmd`: refuse while `mcla.exe` runs, write
only into an initialised `%LocalAppData%\MCLA-R-Launcher-v1`, every key echoed):

| File | SHA-256 |
|---|---|
| `private/l66-arm-all.cmd` | `B329E7759986DCDB0B8FCFBB93AB86352C7CFFD026598674C87562B817F7388E` |
| `private/l66-arm-hot-shadow.cmd` | `34A523E48E5092D495684128E3AE0BDA70B4A96F445CA37372F41435A76157D0` |
| `private/l66-arm-hot-on.cmd` | `73F09A4491429486BD75B118865AE07D318159C9A2792CC63B094F8F19A3AA52` |
| `private/l66-arm-clear.cmd` | `58F69A136DD40D2A8DC8FB3891FEBBF58B614F72DA2F2ED68D5EB943D709020E` |

Parse check: a small program (`check_arms.cpp`, the l63 checker) compiled against the patched
`src/mcla_dev_overrides.h` parsed the six arm files and the three `.cmd` echo blocks as the file
`cmd` writes (CRLF): every file status parsed, 16 values, 0 ignored lines, each value equal to
the arm's expected set; 0 mismatches.

The `all` arm also equals the l64 Deck "all" set, so D-all should reproduce l64 "all". The owner
build has `MCLA_M10_SPEEDUP_DEFAULTS=OFF`, so the arm file alone defines the state (no Wine-only
default can add upload-ahead or bridging behind an arm's back).

## 5. Build (owner-internal l66b)

Scratch tree = main `e61d897` (the 236 tracked files of `src`, `tests`, `CMakeLists.txt`,
`cmake`, `config`, `rules`, `rule-tests`, `sgconfig.yml` byte-identical to the checkout) + this
patch, against `private/l66-sdk-install` (`Found ReXGlue SDK 0.20.0.7`), clang 20.1.8, Ninja,
Release, into `private/l66b-host-build`. Options as the l63 owner build:
`MCLA_TEST_BUILD_EXPIRY`, `MCLA_RAW_TIMER_DEFAULT_ON`, `MCLA_BUILD_IMAGE_CAPTURE_TESTS`,
`MCLA_BUILD_DIAGNOSTIC_TESTS`, `MCLA_PERF_SPLIT_DEFAULT`, `MCLA_LIVERY_DIAG2_DEFAULT`,
`MCLA_READBACK_SYNC_COMPOSITE_DEFAULT`, `MCLA_DEV_OVERRIDES`, `MCLA_PREWARM_SEED_DEFAULT` ON;
`MCLA_M10_SPEEDUP_DEFAULTS`, `MCLA_EXPERIMENTAL_F10_WGC` OFF; prefix maps, `/pdbaltpath`,
`REXSDK_VERSION` empty.

| Item | Result |
|---|---|
| configure / build | rc 0 / rc 0 (252 steps, the 50 targets of the l66 list incl. all 45 test executables), 0 warnings |
| unit test executables | 45 of 45 pass (`mcla_dev_overrides_test` 173 checks, `mcla_m10_defaults_test` 2412) |
| ast-grep 0.50.0 | `scan src tests` rc 0, `test` rc 0 (3 rule tests) |
| `mcla.exe` | `291e5fc87dc6cbfb6bb4fb429e137d62bdb150685d2a702633b2f5a6ced1ae85` |
| `MCLA-Launcher.exe` | `92688ff8efd5af9fc4913b373be2efbe7bdf417375674bb7283cfa7fb54f541e` |
| `mcla_crash_handler.exe` | `713825a2e287b3c8e3a908e10a114469d2a85aa84cdd3fabc7d66dd494996113` |
| `rexgpu-xenos.dll` / `rexruntime.dll` | byte-identical to `l66-sdk-install/bin`: `3752a78d...` / `e4e18a81...` |

String check: `mcla.exe` holds `shared_memory_hot_blocks_shadow`, `shared_memory_hot_blocks_on`,
`conflicting keys for one cvar`, `MCLA dev override`, `mcla-dev-overrides.txt` and
`shared_memory_upload_ahead_16` (not the old `conflicting upload-ahead` text);
`rexgpu-xenos.dll` holds `shared_memory_hot_blocks`. CMakeCache confirms the options above. Sources
ASCII; line endings as in the checkout (`mcla_dev_overrides.h` and its test CRLF,
`mcla_m10_defaults.h` and its test LF).

## 6. Risks and open points

- Not run: no game session, no Deck, no PC run. The keys are verified by unit tests, the parse
  check and the strings in `mcla.exe`; the SDK side of hot blocks is covered only by the SDK
  evidence (model and unit tests).
- `memory_watch_census` must stay on for the `hot_*` counters (all three arms have it on).
- A forgotten arm file stays active for every later `MCLA_DEV_OVERRIDES` build; run
  `private/l66-arm-clear.cmd` (or delete the file on the Deck) after the A/B.
- The conflict log text changed (generic); log readers that grep `conflicting upload-ahead keys`
  need the new text.
