# M10 l66 host: Wine/Proton-only speed-up defaults and KI-064 (2026-11-03)

Status: implemented and unit-tested in a scratch build. Not run on a Deck or a PC, no
physical PASS. Base: main `c2a0028`.

## 1. Wine/Proton-only defaults (`MCLA_M10_SPEEDUP_DEFAULTS`)

Evidence: the l64 Deck A/B ([M10-l64-deck-ab-20261103.md](M10-l64-deck-ab-20261103.md))
shows upload-ahead 16 as the largest Deck gain (CP -8.3 ms/frame) and disarm bridging as
the second (fault path -3.3 ms/frame). Both were net negative on Windows in l63
([M10-l63-pc-ab-20261102.md](M10-l63-pc-ab-20261102.md)), so they are written only when
the host runs under Wine/Proton (`platform::RunningUnderWine()`, the `wine_get_version`
export of ntdll).

| Cvar | Value | Phase (same as the override key) | Windows | Wine/Proton |
|---|---|---|---|---|
| `memory_watch_census` | false | startup | written | written |
| `memory_watch_disarm_bridge_safe_gaps` | true | startup (rexruntime, hot-reload cvar) | **not written** | written |
| `d3d12_readback_sync_targeted` | true | graphics plugin | written | written |
| `d3d12_readback_elide_unread` | true | graphics plugin | written | written |
| `d3d12_wrm_event_wake` | true | graphics plugin | written | written |
| `d3d12_replay_batched_wake` | true | graphics plugin | written | written |
| `d3d12_readback_gpu_timing` | false | graphics plugin | written | written |
| `shared_memory_upload_ahead_pages` | 16 | graphics plugin (rexgpu shared memory; SDK validator accepts 0/16/64) | **not written** | written |

Rules for every entry (unchanged from l65): written only when the cvar is registered,
still holds its default (no command-line or config value), and the owner override file
did not set it; a cvar that already holds the value is reported as `already`. Each write is
logged `MCLA m10 default: name=value`; every entry that is left alone is logged with its
reason (Wine-only entries on Windows: `... left at <value> (wine_only)`). Both summary lines
now start with `platform=wine|windows` and count `wine_only=`; the startup summary also
reports `memory_watch_disarm_bridge_safe_gaps`.

Owner override file (`MCLA_DEV_OVERRIDES` builds): any applied line for the cvar wins, as
before. New: the two upload-ahead keys write nothing for `=false`, so a line
`shared_memory_upload_ahead_16=false` (or `_64=false`) is now also treated as the owner's
arm (the Deck "off" baseline) and keeps 0. For this the dev-override module gained
`FileValue(Key)` (the parsed value of a valid line; nullopt without the option). The
override key link is now derived from the dev-override whitelist (every non-launcher key
whose cvar is a default cvar), which covers both upload-ahead keys.

`--mcla_m10_speedup_defaults=false` still turns the whole set off, also on Wine.

## 2. KI-064: stray file in `sessions` blocked CHECK SETUP and START

Cause: four loops in `src/mcla_launcher_controller_win.cpp` (CHECK SETUP, START GAME,
duplicate/restore of a source profile, in-game name change) opened every entry of
`<profile>\sessions` with `fs::OpenDirectory`, so a plain file (Dolphin's compressed zip)
failed with "directory ancestor is not a normal non-reparse directory".

Fix: one helper `InspectSessionHistory` used by all four loops.

- The entry's attributes are read with `GetFileAttributesW` (does not follow reparse
  points). An entry that is neither a directory nor a reparse point is a plain file: it is
  skipped, counted and never opened. Directories, every reparse point (also a reparse file)
  and entries whose attributes cannot be read still go through `fs::OpenDirectory`, so the
  KI-049 rules decide them.
- The capacity bound is unchanged and still counts every entry (files too): 1023 for
  CHECK/START, 1024 for duplicate and rename, as before.
- Messages now name the entry: an entry that cannot be opened gives
  `Session entry "<name>" cannot be inspected: <OpenDirectory error>`; an unfinished
  session gives the caller's old text plus `: <name>` (START keeps the full record path).
- CHECK SETUP adds ` (ignored N file(s) in sessions)` to the readiness text when N > 0.

Diagnostics folders: `PackageNames` (the `live`/`crash` package lists) already keeps only
directories that are not symlinks, so plain files there never blocked anything; no change.
No other loop in the launcher opens each `sessions` entry.

## 3. Tests

- `tests/mcla_m10_defaults_test.cpp` (2313 checks, was 891): table with values, phases,
  platform and override-key presence; every override key writing a default cvar is applied
  in the same phase (8 keys, both upload-ahead keys); upload `=false` writes nothing and the
  Wine value equals the owner's `upload_ahead_16` arm; the value is one the SDK validator
  accepts; exactly `kWineOnlyCvars` are Wine-only. Decision on Windows (not applied:
  `wine_only`) and on Wine (applied); on Wine the dev override and a user value win, older
  SDK gives `unavailable`. Exhaustive check of all 256 input combinations per entry (Wine
  flag included): apply exactly in the intended case (26 apply cases) and never a Wine-only
  entry on Windows.
- `tests/mcla_launcher_controller_win_test.cpp` (owned synthetic fixture, 423 checks, was
  411): the unfinished-session refusal names the entry; a plain file `stray-archive.zip` in
  `sessions` gives Ready with `(ignored 1 file(s) in sessions)` and stays unchanged; a
  subdirectory `stray-folder` without `state.txt` is Blocked with "unfinished session" and
  its name; without stray entries Ready has no note. In the support-repair matrix a stray
  file is in `sessions` when START runs: START succeeds, the fixture child completes, the
  file is unchanged and the next check reports it. `OnlySession` skips plain files.

## 4. Build

Scratch tree = main `c2a0028` (236 tracked files of `src`, `tests`, `CMakeLists.txt`,
`cmake`, `config`, `rules`, `rule-tests`, `sgconfig.yml` blob-identical) + this patch,
against `private/l65-sdk-install`, clang 20.1.8, Ninja, Release, into
`private/l66-host-build`. Options as the l65 volunteer-like build: `MCLA_TEST_BUILD_EXPIRY`,
`MCLA_RAW_TIMER_DEFAULT_ON`, `MCLA_BUILD_IMAGE_CAPTURE_TESTS`, `MCLA_BUILD_DIAGNOSTIC_TESTS`,
`MCLA_PERF_SPLIT_DEFAULT`, `MCLA_LIVERY_DIAG2_DEFAULT`, `MCLA_READBACK_SYNC_COMPOSITE_DEFAULT`,
`MCLA_PREWARM_SEED_DEFAULT`, `MCLA_M10_SPEEDUP_DEFAULTS` ON; `MCLA_EXPERIMENTAL_F10_WGC`,
`MCLA_DEV_OVERRIDES` OFF.

| Item | Result |
|---|---|
| configure / build | rc 0 / rc 0 (252 steps), 0 warnings |
| unit test executables | 45 of 45 pass |
| ast-grep 0.50.0 | `scan` rc 0, `test` rc 0 (3 rule tests) |
| `mcla.exe` | `741efc2ccdc3a4b845987171cb28790ff3d53721ff3ce2420e64bc979567d683` |
| `MCLA-Launcher.exe` | `2df34674ac4f07c4f7e0e746b43ff934d623541c3f479cc173b886f15b366575` |
| `rexgpu-xenos.dll` / `rexruntime.dll` | l65 SDK install, `1eb95425...` / `de6cc48a...` |

String check: `mcla.exe` holds `platform=`, `wine_only` and `shared_memory_upload_ahead_pages`;
`MCLA-Launcher.exe` holds the UTF-16 texts `file(s) in sessions` and `Session entry "`.
Sources ASCII; line endings as in the checkout. `MCLA_DEV_OVERRIDES=ON`: not linked as a
configuration; `mcla_dev_overrides.cpp` and `mcla_m10_defaults.cpp` were compiled alone with
the build's own commands plus `-DMCLA_DEV_OVERRIDES=1` (rc 0, no diagnostics), which covers
the new `FileValue` body.

## 5. Risks and open points

- Wine detection is the existing `wine_get_version` probe; a Windows-on-Wine-like layer
  without that export would get the Windows set (safe direction).
- Upload-ahead raises upload bytes about 3x on the Deck (8.8 -> 26.6 MB/frame) and render
  write faults +54%; l64 saw 0 protect failures and no device errors. A volunteer Deck run
  with the shipped defaults is still needed to confirm the gain without the override file.
- A plain file in `sessions` that is really a broken session record (not a directory) is now
  ignored instead of blocking; real session records are always directories created by the
  launcher, so this does not hide a session the launcher wrote.
- KI-064 stays Open until the owner confirms on the Deck with a file in `sessions`.
