# M10-003 step 1: local union shader store (launcher seeding) and the SDK creation-thread fix (l60 patch, 2026-11-01)

Implementation of steps 1 and 2 of
[the pre-warm design](M10-prewarm-design-20261101.md) by an Opus subagent;
build and unit tests only. Nothing was launched (no `mcla.exe`, no launcher),
no owner profile, save or store was modified, and no shader bytes of the
owner's stores were read into anything kept: the owner's stores were scanned
for header words, file sizes and record counts only (`tools/header_scan.py` of
the patch folder, ucode skipped with seeks). All test stores are synthetic
(counter data, not game data). Spark: not used (deterministic source reading,
build and test; no log to triage).

**Status: implemented, unit-tested; owner A/B pending. No physical PASS.**

## 1. What changed

| Part | Where | What |
| --- | --- | --- |
| SDK fix | local SDK branch `mcla/m10-prewarm`, commit `9d8ede4` (from `mcla/m10-readback` `141e13f`), worktree `private/l60-sdk-wt`, not pushed | Startup creation of stored pipelines starts the extra creation threads it always meant to (`creation_thread_needed_count`, upstream rule), and a blocking initialisation waits for every busy creation thread, so `Created N graphics pipelines ... in X milliseconds` covers the whole creation. The line adds `(T creation threads, P persistent, blocking B)`. |
| Store format | `src/mcla_shader_store_seed.h` (new, pure) | Exact parser of the SDK store files, family table, merger (dedup, bounds), summary line. |
| Seeding pass | `src/mcla_launcher_shader_seed_win.{h,cpp}` (new) | Reads the other stores of the installation, writes the build's store atomically. |
| Launcher | `src/mcla_launcher_controller_win.cpp` | Calls the pass in `Start()` right after the build cache folder is opened (before the host starts); writes `store-seed.txt` in the session folder; `OutputDebugString` of the same line. |
| Switch | `src/mcla_dev_overrides.h` | New launcher-only key `prewarm_seed_store` (`Phase::kLauncher`; the host parses and ignores it). |
| Host | `src/mcla_shader_store_log.{h,cpp}` (new), `src/mcla_app.cpp` | One log line at startup with the valid record counts of the store the session starts with. |
| Build | `CMakeLists.txt` | Sources, header-only XXH3 include (`third_party/rexglue-sdk/thirdparty/xxHash`, SYSTEM), option `MCLA_PREWARM_SEED_DEFAULT` (OFF; see section 4), two new test executables. |
| Policy | `docs/source-data-policy.md` | Shader-store paragraph (union store is local derived data + game data, never leaves the machine). |
| Tests | `tests/mcla_shader_store_seed_test.cpp`, `tests/mcla_launcher_shader_seed_win_test.cpp` (new), `tests/mcla_dev_overrides_test.cpp` | See section 5. |

## 2. Store formats (SDK `141e13f`, v0.20.0.6 base)

Read from `src/graphics/d3d12/pipeline_cache.cpp` (`InitializeShaderStorage`,
lines 256-755), `src/graphics/vulkan/pipeline_cache.cpp` (400-575) and the
headers; record sizes confirmed by compiling `static_assert`s against the SDK
headers (d3d12 `PipelineStoredDescription` 72 bytes, Vulkan 68 bytes,
`ShaderStoredHeader` 12 bytes; a deliberately wrong size failed).

| File (`shaders/shareable/`) | Header (as stored) | Records |
| --- | --- | --- |
| `545407F8.xsh` (shared by both backends) | `XESH`, `20 20 12 19` | 12-byte header: XXH3-64 of the ucode (LE), dword count (31 bits) + type (1 bit); then the ucode |
| `545407F8.rtv.d3d12.xpso` | `XEPS`, `DXRT`, `20 26 02 26` | 72 bytes: XXH3-64 of the 64-byte description, description |
| `545407F8.rov.d3d12.xpso` | `XEPS`, `DXRO`, `20 26 02 26` | 72 bytes |
| `545407F8.fbo.vk.xpso` | `XEPS`, `00 00 00 00`, `20 26 02 28` | 68 bytes (60-byte description) |
| `545407F8.fsi.vk.xpso` | `XEPS`, `01 00 00 00`, `20 26 02 28` | 68 bytes |

Versions: `.xsh` `ShaderStoredHeader::kVersion` 0x20201219; d3d12 `.xpso`
max(`PipelineDescription::kVersion` 0x20210425, `DxbcShaderTranslator::Modification::kVersion`
0x20260226); Vulkan max(0x20260228, SPIR-V `Modification::kVersion` 12). The
title id is in the file names only. The SDK reads records until the first
truncated or hash-mismatching one and discards a file whose header differs
from its constants (truncate to 0, new header).

Owner stores (header scan, counts only, 2026-11-01): 52 store folders in 10
profiles, each with `545407F8.xsh` and `545407F8.rtv.d3d12.xpso` only; every
header equals the table above; every `.xpso` is a whole number of 72-byte
records; 26,919 shader records in total (largest file 669), no partial tail,
largest shader 681 dwords. The design's union estimate (758 shaders, 6,822
descriptions) was not recomputed (that would need reading the records).

## 3. Merge rules as implemented

- **Sources.** Every managed profile root in the launcher library (the
  selected profile included), its `cache` folder, and only children whose name
  is a 64-hex build fingerprint, other than the session's own fingerprint
  folder; in each only `shaders/shareable/` and only the five file names
  above (other titles' stores, saves, sessions, diagnostics are never opened).
  At most 1,024 entries per profile cache, 256 source folders (newest `.xsh`
  first), 16 MiB per file and 256 MiB read per pass.
- **Path safety.** Folders through `fs::OpenDirectory` (every ancestor held,
  no reparse ancestor on Windows; KI-049 Wine/Proton rules unchanged), source
  identity compared with `fs::CompareDisjoint` against the target; files
  through `fs::ReadFile` (regular, single-link, non-reparse, bounded, size
  stable during the read). A junctioned build folder or profile `cache`
  folder and a hard-linked store file are refused (tested).
- **Validation.** Exact SDK rules: header equal to the family's (magic, api
  word, version), records hash-checked with XXH3-64 (header-only xxHash 0.8
  from the SDK submodule), the valid prefix used, the rest dropped; a ucode
  count above 65,536 dwords ends the prefix. A source with any other header
  (other format version, other family, short, damaged) adds nothing and is
  counted in `skipped_files`.
- **Separation.** One merger per file name; the header check also rejects a
  file whose content belongs to another family, so ROV/RTV and d3d12/vk are
  never mixed. The `.xsh` is shared by both backends, as in the SDK.
- **Deduplication.** Shaders by ucode hash (the SDK's own key), descriptions
  by their full 72/68 bytes (the SDK compares the full description).
- **Order.** The build's own valid records first, then new records of the
  sources in source order.
- **Target rules.** A missing or empty target file is created when anything
  is added; a valid target is rewritten only when a source adds a record
  (otherwise untouched; a bad tail is left for the SDK to truncate). A target
  file with another header (for example a newer SDK format) or one that cannot
  be read is never replaced (`foreign_files`).
- **Bound.** Each written file stays within 16 MiB; records beyond are left
  out (`over_bound`).
- **Write.** `fs::AtomicWrite` (same-directory `.mcla-tmp-*.partial`, flush,
  `MoveFileEx` replace) under the profile writer lock `Start()` already holds;
  descriptions first, shaders last. Source files are only read.
- **Failure.** Seeding never blocks a launch; errors give `source=error`.
- **Idempotence.** A second pass with the same sources adds and writes
  nothing (`source=existing`, tested).

## 4. Switch and counters

- Default OFF (primary decision 2026-11-01: `MCLA_PREWARM_SEED_DEFAULT` was
  changed to OFF so volunteer builds keep the old cold behaviour until the owner
  A/B passes; owner-internal builds configure it ON). In launchers built with `MCLA_DEV_OVERRIDES` (the
  owner-internal builds) the launcher reads
  `%LocalAppData%\MCLA-R-Launcher-v1\mcla-dev-overrides.txt` (the same file and
  parser as the host, regular single-link file only) before each session:
  `prewarm_seed_store=false` turns seeding off (cold baselines),
  `prewarm_seed_store=true` on; an absent, rejected or malformed file keeps
  the default. With seeding off nothing is read from other folders and
  nothing is created or written.
- Session folder `store-seed.txt` (also in the launcher debug output), for
  example:
  `MCLA store seed: source=seeded shaders=758 descriptions=6822 from_dirs=51 ms=40 bytes=780000 added_shaders=... added_descriptions=... scanned_dirs=... skipped_files=0 foreign_files=0 over_bound=0 read_bytes=... switch=on`
  (illustrative numbers). `source` is `cold` (no records before, none added;
  also every switch-off run with an empty store), `seeded` (records added),
  `existing` (records before, none added) or `error`. `shaders` and
  `descriptions` are the valid records after the pass (descriptions summed
  over the four `.xpso` families), `from_dirs` the source folders that added
  at least one record, `ms` the pass wall time, `bytes` the bytes written.
  `launch-receipt.txt` is unchanged (the metadata export accepts only its
  known keys).
- Host log (`mcla.log`, App category, right after the dev-override lines):
  `MCLA shader store at startup: xsh=N rtv.d3d12=M rov.d3d12=absent fbo.vk=absent fsi.vk=absent bytes=B`
  (valid records per family, read-only, before the SDK opens the store).
- SDK lines as before: `Translated N shaders from the storage in X
  milliseconds`, and `Created N graphics pipelines ... in X milliseconds (T
  creation threads, P persistent, blocking true)` with the l60 SDK.

## 5. Builds and tests

SDK (`private/l60-sdk-work`, l58 configuration: Ninja Multi-Config, clang
20.1.8, `-march=x86-64-v3`, D3D12 only, Tracy and perf counters on; submodule
trees copied from `private/l58-sdk-wt`, gitlinks unchanged):

- build `private/l60-sdk-b`, output `private/l60-sdk-o`, 1,440 steps, rc=0,
  503 `warning:` lines (the l58 count; none in the changed lines). No install
  (the host build uses `l58-sdk-install`).
- ctest **1982/1982** (the 1978 baseline + 4 new `[storage-creation]`
  cases), run twice (before and after clang-format of the changed lines);
  tagged suites `[perf-split]`, `[pipeline-wait]`, `[livery-diag2]`, `[wrm]`,
  `[replay-wake]`, `[readback-slots]`, `[storage-creation]` (17 assertions)
  pass.
- clang-format 20.1.8 clean on the new files and changed lines; ASCII, LF.

Host (`private/l60-host-work`, scratch copy of main `15213ff` + this patch,
build `private/l60-host-build` against `private/l58-sdk-install`, l58 options +
`MCLA_DEV_OVERRIDES`): `Found ReXGlue SDK 0.20.0.1-dev.g62cd66d` (the l58 install's version
string). First attempt stopped at 134/238 on a string-literal error of this
patch (fixed); the remaining 105 steps built with **0 warnings**
(`l60-build2.log`). Host tests: **43 executables, 43 pass**
(`l60-tests.log`; again after the final rebuild, `l60-build3.log` 0 warnings and `l60-tests2.log` 43/43; the 41 of l59 plus the two new ones):
`mcla_shader_store_seed_test` 294 checks, `mcla_launcher_shader_seed_win_test`
46 checks (junction and hard-link fixtures ran, none skipped),
`mcla_dev_overrides_test` 98 checks, `mcla_launcher_controller_win_test` 411
checks (its synthetic sessions now carry `store-seed.txt`, `source=cold`),
`mcla_diagnostic_export_win_test` and `mcla_diagnostic_export_policy_test`
unchanged (the export does not read `store-seed.txt` or `cache/`). Mutation
check: disabling the ucode hash check in `Parse` makes
`mcla_shader_store_seed_test` fail ("bad ucode hash stops"); restored and
rebuilt (`l60-mutation.log`). ast-grep 0.50.0 on the scratch tree: scan rc=0,
rule tests 3/3 (`l60-astgrep.log`). New and changed lines are ASCII. Logs and
scripts: `private/l60-host-work/`. No candidate staged.

New tests:

- `mcla_shader_store_seed_test` (pure): header bytes of all five families,
  record sizes, file names; valid parse; empty/short/bad magic/bad version;
  rtv vs rov, d3d12 vs vk, xsh vs xpso separation; truncated last shader,
  partial record header, bad hash in the middle, oversized count, partial and
  corrupted descriptions; dedup in target, across sources and within a file;
  version-mismatch and cross-family sources add nothing; foreign and
  unreadable targets never written; truncated source prefix merged; bad target
  tail dropped only on write; size bound; idempotence; summary classification
  and line format; fingerprint names; host startup line.
- `mcla_launcher_shader_seed_win_test` (owned temp fixture, two library
  profiles plus an outside folder): switch off creates and reads nothing;
  seeding gives the exact union per family, keeps families separate, ignores
  another title's store, a non-fingerprint folder, a store-named file among the
  saves, an older-format store, a junctioned build folder and a hard-linked
  file; sources byte-identical afterwards; no temporary file left; second pass
  writes nothing; a later new source record grows only that family; a foreign
  build store file is left byte-identical while the other families seed; a
  profile whose `cache` folder is a junction is refused; no profiles gives
  `cold`; the switch parser and the override-file read (dev-override builds
  only).
- `mcla_dev_overrides_test`: the eighth key and its launcher phase.

## 6. Owner A/B (to run later; not run)

Build: an l60 candidate (primary's decision) with the l60 SDK DLLs and this
launcher. PC, 1440p, Unlocked, the bench route of
[M10-001-route-protocol](../M10-001-route-protocol.md).

1. From MAIN make four fresh BENCH copies through the launcher: A1, A2
   (seeding off) and B1, B2 (seeding on). Each run starts on its own fresh
   copy, because a run fills its copy's store (a second run on the same copy
   is warm by construction). Alternate the arms (A1, B1, A2, B2) so the
   driver's own shader cache warms both arms alike.
2. Arm A (seeding off): write `prewarm_seed_store=false` in
   `%LocalAppData%\MCLA-R-Launcher-v1\mcla-dev-overrides.txt` before START;
   `store-seed.txt` in the session folder must say `source=cold ...
   switch=off`, and the host line `xsh=0` or `xsh=absent`.
3. Arm B (seeding on): remove the line (or write `=true`) before START;
   `store-seed.txt` must say `source=seeded` with `shaders` and
   `descriptions` near the union (design estimate 758 / 6,822; B2 may also
   pick up the records of the earlier runs' copies).
4. Record per run: the `store-seed.txt` line, the host `MCLA shader store at
   startup` line, `Translated N shaders ... X ms`, `Created N ... X ms (T
   creation threads ...)`, time to title, and for minute 1 and minutes 2-5
   waits, total wait ms, longest wait, skips, run-time PSOs.
5. Pass (from the design): arm B minute-1 waits and wait ms at or below the
   warm reference (l51 warm 122 / 1.2 s; l53 warm 55 / 0.3 s) with 0 skips;
   startup growth under 1 s; no regression in first photo, police livery and
   album preview. KI-047 frequency is not a criterion.
6. Deck (later): copy the owner's own seeded store to the Deck profile and
   run cold vs seeded the same way (Deck numbers on the Deck).

## 7. Risks and limits

- Format constants are compiled into the launcher; an SDK format change must
  bump them (fail-safe: the SDK discards an unrecognised seeded file and runs
  cold; a target written by a newer SDK is never replaced).
- Startup grows with the union (all stored PSOs are created before the guest
  starts, now with all cores and fully awaited): measure the `Created ...`
  line; thousands of PSOs on a driver-cold machine (Deck, driver update) may
  take seconds (design step 3).
- Seeding reads other profiles' stores; a store a running game holds open for
  writing is skipped (sharing violation, `skipped_files`), never waited for.
- Two runs of the same arm on the same copy are not independent (run 1 fills
  the store).
- The union carries descriptions for every scale/MSAA combination seen on
  this machine; extra PSOs cost driver memory (KI-048 budget) - watch commit
  and VRAM in the A/B.
- Volunteer builds do not seed (CMake default OFF); the override file is read
  only in dev-override launchers. Turning seeding on for volunteers waits for
  the owner A/B.
- Not covered: no in-game run, no owner check, no Wine/Proton run of the
  seeding pass (KI-049 path rules are reused, not re-tested here).

## Primary verification (2026-11-01)

- Patch applied byte-exact to main; CMake default changed to OFF by the primary; `private/l57-host-build` rebuilt from main with 0 warnings; all 43 test executables pass (`mcla_shader_store_seed_test`, `mcla_launcher_shader_seed_win_test`, `mcla_dev_overrides_test` 98, `mcla_launcher_controller_win_test`); `git diff --check` and ast-grep clean. SDK fix `9d8ede4` (branch `mcla/m10-prewarm`, local) ctest 1982/1982. No candidate built; nothing run in the game.
