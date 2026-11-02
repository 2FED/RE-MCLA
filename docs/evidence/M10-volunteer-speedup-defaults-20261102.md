# M10 l65: volunteer speed-up defaults (MCLA_M10_SPEEDUP_DEFAULTS), 2026-11-02

Host-side change by an Opus subagent on 2026-11-02; built and unit-tested only. Nothing was
launched (no `mcla.exe`, no launcher); no save, profile or LocalAppData file touched.
**Status: implemented, unit-tested, two build configurations green. No physical PASS.**

Spark: not used (one small module plus a decision table; deterministic unit tests are cheaper
than validating a model answer).

## 1. Purpose

The owner's l63 PC A/B (`docs/evidence/M10-l63-pc-ab-20261102.md`) proved the M10 SDK
switches on BENCH-A: "all" vs "base" +15-24% at 1440p, about +10% at 720p, with zero failed or
fallback waits. Those switches are SDK cvars that are off by default (or, for two diagnostics,
on by default); so far only the owner-internal override file of `MCLA_DEV_OVERRIDES` builds
could turn them on. Volunteer builds must not read that file. The new CMake option
`MCLA_M10_SPEEDUP_DEFAULTS` (OFF by default) makes the host write the proven values itself.

## 2. What is set, when, and in which order

`src/mcla_m10_defaults.h` (pure table and decision), `src/mcla_m10_defaults.cpp` (cvar calls),
`src/mcla_app.cpp` (two calls), `CMakeLists.txt` (option, source, test target),
`tests/mcla_m10_defaults_test.cpp`.

| Phase | Cvar | Value | SDK default | Diagnostic | Override-file key |
|---|---|---|---|---|---|
| startup | `memory_watch_census` | false | true | yes | `memory_watch_census` |
| graphics plugin | `d3d12_readback_sync_targeted` | true | false | no | same name |
| graphics plugin | `d3d12_readback_elide_unread` | true | false | no | same name |
| graphics plugin | `d3d12_wrm_event_wake` | true | false | no | same name |
| graphics plugin | `d3d12_replay_batched_wake` | true | false | no | same name |
| graphics plugin | `d3d12_readback_gpu_timing` | false | true | yes | none |

Not written (net negative on PC in l63): `memory_watch_disarm_bridge_safe_gaps` (false),
`shared_memory_upload_ahead_pages` (0); also untouched: `memory_watch_probes`,
`d3d12_readback_sync_kick_draws` (SDK 512), `d3d12_readback_sync_composite` (owned by
`mcla_readback_sync.cpp`). The host init-only switches `mcla_native_d3d_refcount` and
`mcla_native_xapi_interlocked` are already `REXCVAR_DEFINE_BOOL(..., true, ...)` in
`src/mcla_native_refcount.cpp` (verified); they are only reported in the startup summary.

Order in `MclaApp`:

1. `OnPostInitLogging`: `dev_overrides::OnStartup()` (owner file, `MCLA_DEV_OVERRIDES` builds
   only), then **`m10_defaults::OnStartup()`** (census), then the shader-store log line.
2. `OnPostLoadGraphicsPlugin`: `dev_overrides::OnGraphicsPluginLoaded()`, texture budget, perf
   split, livery diag2, `readback_sync` (composite fix), photo-capture pipeline wait, the
   `readback_resolve=some` default (unless user-set), then **`m10_defaults::OnGraphicsPluginLoaded()`**
   last. The `readback_resolve` block was restructured from an early `return` into an `if` so
   the new call always runs; its behaviour is unchanged.

Decision per entry (`Decide`, first match wins):

1. build without the option: nothing (the whole module compiles to two empty functions; the
   switch cvar is not even defined);
2. `--mcla_m10_speedup_defaults=false` (new host init-only switch, default true in ON builds):
   nothing, one line `MCLA m10 defaults: off (...)`;
3. a diagnostic entry (census, GPU timing) in an `MCLA_DEV_OVERRIDES` build: left at the SDK
   default (owner measurement builds keep their counters);
4. the owner override file wrote this cvar (`dev_overrides::AppliedValue(key)` has a value):
   left alone. This is needed because `rex::cvar::HasNonDefaultValue` compares values: an A/B
   arm that writes the SDK default (for example `d3d12_readback_sync_targeted=false`) would
   otherwise look untouched and be overwritten;
5. cvar not registered (an SDK without it, such as the pinned volunteer SDK): logged
   `unavailable`;
6. cvar holds a non-default value (command line or config): left alone;
7. cvar already holds the value: left alone (`already`);
8. otherwise written, logged `MCLA m10 default: name=value`.

Log lines (category app for startup, gpu for the plugin phase): one
`MCLA m10 default: name=value` per value written; one `MCLA m10 defaults: name left at v (reason)`
or `... unavailable in this build` per entry not written; one summary line per phase:

    MCLA m10 defaults (startup): applied=1 failed=0 user_set=0 dev_override=0 measurement_build=0 already=0 unavailable=0 mcla_native_d3d_refcount=true mcla_native_xapi_interlocked=true
    MCLA m10 defaults (graphics plugin): applied=5 failed=0 user_set=0 dev_override=0 measurement_build=0 already=0 unavailable=0 d3d12_readback_sync_composite=true readback_resolve=some memory_watch_disarm_bridge_safe_gaps=false shared_memory_upload_ahead_pages=0

(expected lines for a volunteer build with the l65 SDK; not observed at runtime, nothing was
launched).

Existing semantics kept: targeted sync acts only while `d3d12_readback_sync_composite` is on,
so volunteer builds need `MCLA_READBACK_SYNC_COMPOSITE_DEFAULT=ON` as well (writing targeted
without composite is harmless: the SDK ignores it). Elision acts only in `readback_resolve=some`
(the host default); a user `fast`/`full` makes it inert, and the SDK guard covers a runtime mode
switch. Both values are printed in the summary line.

## 3. Why (l63 numbers, BENCH-A, owner)

| Switch | l63 evidence |
|---|---|
| targeted sync (with composite) | forced wait 720p 5.1 -> 1.7 ms/f, 1440p 14.9 -> 7.7-9.0 ms/f; `forced_wait_failed=0`, `fallbacks_to_full=0` in every window |
| read-back elision | GPU read-back 3.46 -> 0.88 ms/f (720p), 4.02 -> 1.02 (1440p); CPU bytes copied identical; `guard_misses=0`, `full_waits=0`, `gpu_blocks_dropped=0`. The `sporadic_read_prev_unfenced=56` loading-minute artifact is fixed in SDK l65 (`prev_unwritten` class) |
| WRM event wake | wrm wait 1.9-2.4 -> 1.0-1.4 ms/f (1440p), 1.93 -> 0.96 (720p stationary) |
| replay batched wake | replay worker CPU 60 -> 23% (720p), 40 -> 16% (1440p); +0.1-0.8 ms/f replay latency |
| census off | census costs <= 0.05-0.1 ms/f and one `MCLA_WATCH` line per minute; no consumer in volunteer logs |
| GPU timing off | timestamps around every recorded read-back block (~15 blocks/f with elision); only feeds `gpu_readback_ms`; cost not isolated |
| bridging, upload-ahead not set | disarm ms/f up 0.43 -> 0.62-0.70 (720p); upload +~20 MB/frame, render-thread write faults +60% |

Overall "all" vs "base": 1440p stationary 32.5 -> 40.3 fps (+24%), laps +15% / +21%; 720p
stationary +9.7%. The owner reported the police livery correct in all l63 arms (KI-047).

## 4. How the owner can still A/B

- Owner builds (`MCLA_DEV_OVERRIDES=ON`): every override-file line wins over this module, also
  a line that writes the SDK default (rule 4). The two diagnostics stay at SDK defaults (census
  on, GPU timing on) in these builds, so measurement builds are unchanged unless the file says
  otherwise. Building an owner build with `MCLA_M10_SPEEDUP_DEFAULTS=OFF` reproduces the old
  behaviour exactly (verified by the OFF configuration below).
- Any build with the option: `--mcla_m10_speedup_defaults=false` keeps every SDK default; an
  explicit non-default command-line value of a single cvar also wins. Limitation: an explicit
  command-line value equal to the SDK default (`--d3d12_wrm_event_wake=false`) is not
  distinguishable from the default (the SDK compares values) and is overwritten; use the master
  switch or an owner build. The launcher passes none of these cvars.

## 5. Build and tests

Scratch tree = main `b769d09` (233 tracked files of `src`, `tests`, `CMakeLists.txt`, `rules`,
`rule-tests`, `sgconfig.yml`, `cmake`, `config` blob-identical; no extra `src`/`tests` files)
+ this patch, against `private/l65-sdk-install` (SDK `mcla/m10-watch` 43577c8, local), clang
20.1.8, Ninja, Release. Options in both configurations: `MCLA_TEST_BUILD_EXPIRY`,
`MCLA_RAW_TIMER_DEFAULT_ON`, `MCLA_BUILD_IMAGE_CAPTURE_TESTS`, `MCLA_BUILD_DIAGNOSTIC_TESTS`,
`MCLA_PERF_SPLIT_DEFAULT`, `MCLA_LIVERY_DIAG2_DEFAULT`, `MCLA_READBACK_SYNC_COMPOSITE_DEFAULT`,
`MCLA_PREWARM_SEED_DEFAULT` ON; `MCLA_EXPERIMENTAL_F10_WGC`, `MCLA_DEV_OVERRIDES` OFF;
`MCLA_M10_SPEEDUP_DEFAULTS` ON (`private/l65-host-build/on`) and OFF (`private/l65-host-build/off`).

| Configuration | configure / build | warnings | unit test executables | `mcla.exe` sha256 |
|---|---|---|---|---|
| ON (`l65-host-build/on`) | rc 0 / rc 0 | 0 | 45 of 45 pass (`mcla_m10_defaults_test`: 891 checks) | `292d6517b83981d8fd666146055e336bf422c2d90f23b44c10f8ec61aac9b0e9` |
| OFF (`l65-host-build/off`) | rc 0 / rc 0 | 0 | 45 of 45 pass (`mcla_m10_defaults_test`: 891 checks) | `1f9fdc028ed5dbfccb29cdb09af53adc8a93b70268f5a0915e045350eafad6a7` |

- Compile-out check: the ON `mcla.exe` holds the 7 `MCLA m10 default...` format strings and the
  `mcla_m10_speedup_defaults` cvar name; the OFF `mcla.exe` holds neither (0 and 0), and
  `MCLA_M10_SPEEDUP_DEFAULTS=1` appears only in the ON `build.ninja`.
- SDK DLLs copied from the install: `rexgpu-xenos.dll` `1eb954258bac7d7e42c40313652f6187d95614bf2c2c8dc6c276ad6666e3e069`
  (the l65 SDK install hash), `rexruntime.dll` `de6cc48ab53f96e5c2e92ca7a4b3c1b730ff8da4d1f064597bf69b39b91e1b83`.
- ast-grep 0.50.0 `scan` rc 0, `test` rc 0 (3 rule tests pass).
- Unit test (`tests/mcla_m10_defaults_test.cpp`, registered in the M9 host-hook test loop):
  table contents and phases; every override key names the same cvar and phase as the
  dev-override whitelist and every whitelist key of a default cvar is linked; net-negative
  switches, probes, composite, kick draws and the native host switches are never in the
  table; each decision rule; an override arm equal to the SDK default wins; exhaustive check of
  all 128 input combinations per entry (apply exactly in the intended case; 11 apply cases).
- Sources ASCII only.

## 6. Pre-warm seeding for volunteers (`MCLA_PREWARM_SEED_DEFAULT`)

Recommendation: **ON** for the next volunteer build (the option default in CMake is unchanged;
the clean-clone script sets it).

Evidence (l63, BENCH-A): first launch `source=seeded shaders=758 descriptions=6822
from_dirs=36 ms=544`, 6822 pipelines created in 1187 ms; later launches `source=existing`,
68-96 ms; first launch at a new resolution about 1.0 s once; **0 pipeline waits in every minute**
of all six sessions against 2-133 (720p) and 3-448 (1440p) waits per minute in l53. The owner
accepts a few seconds at first launch.

Review of `src/mcla_launcher_shader_seed_win.cpp` / `src/mcla_shader_store_seed.h` for volunteer
machines:

- Scope: only the profiles in the launcher library; in each only
  `cache/<64-hex fingerprint>/shaders/shareable/` and only the five SDK store file names. Saves,
  sessions and diagnostics are never opened; source files are never written.
- Filesystem safety: directories through `fs::OpenDirectory` (no reparse ancestors on Windows,
  KI-049 Wine/Proton rules), files through `fs::ReadFile` (regular, single-link, non-reparse,
  at most 16 MiB), `is_symlink` entries skipped; the build's own files written with
  `fs::AtomicWrite` under the profile writer lock the controller holds; `CompareDisjoint`
  rejects a source that is the target.
- Bounds: 1024 cache entries per profile, 256 source directories (newest first), 256 MiB read
  in total, 16 MiB per written file (records beyond it are counted as `over_bound`).
- Format safety: exact SDK header (magic, API word, version) and per-record XXH3 check; a file
  of another format version or family is skipped; a truncated tail (another profile's session
  writing at the same time) is cut at the last valid record, which is what the SDK itself does.
- Failure: exceptions caught, errors counted in `store-seed.txt`; seeding never blocks a launch.

No blocking concern found. Residual risks: (a) a volunteer with many large profiles pays up to
the 256 MiB read bound once per new build (seconds on a slow disk or a Proton prefix on an SD
card); (b) records from another build of the same store version are trusted as the SDK trusts
its own store (a description from a different SDK translation would at worst create an unused
pipeline at start-up); (c) volunteer builds have no off switch (the override file is read only
by `MCLA_DEV_OVERRIDES` launchers); a single-profile volunteer simply has nothing to seed from.

## 7. Risks and open items

- **No runtime evidence of this build.** Volunteer-default arm = l63 "all" minus bridging and
  upload-ahead (both net negative) minus census and GPU timing; that exact combination was never
  run. Expected at least as fast as "all"; to be confirmed by a single-arm owner run (BENCH-A,
  1440p and 720p) and the KI-047 police-livery visual check on the volunteer build.
- WRM event wake raises CP kernel time (4 -> 6-8% at 1440p, 8 -> 12-13% at 720p); PC evidence
  only, and only with vsync on. Replay batched wake mainly helps the Deck; Deck numbers for this
  combination are not yet measured.
- Targeted sync adds kicks and submissions (+1-3 ms/f CP work in "all"); `kick_draws` stays
  512, tuning is a later A/B.
- With the pinned volunteer SDK (no M10 cvars) every entry is logged `unavailable` and nothing
  changes; the speed-ups need the l65 (or later) SDK in the volunteer package.
- Explicit command-line values equal to the SDK default are not detectable (section 4).
