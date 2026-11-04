# M10-007 / KI-048: MCLA_VRAM census (S0, host part) and the soft-cap mitigation

Date: 2026-11-04. Status: implemented and unit-tested in a scratch patch (l68-vram-patch, base main
`dc886a3`, i.e. after the KI-035 warp patch); built twice against `private/l67-sdk-install` (volunteer-like options, and the same with
`MCLA_DEV_OVERRIDES=ON`). **Not run in the game**: no physical result exists yet. Design source:
[M10-007 budget design](M10-007-budget-design-20261104.md) section 4 (census) and "Short-term
mitigation". The SDK accessors of the census (texture cache total, scaled-resolve heaps, render
targets, shared-memory residency) are M11 and not part of this change.

## 1. What changed (host only)

| File | Change |
|---|---|
| `src/mcla_vram_census_win.h` (new) | Pure helpers: `EffectiveDrawScale`, `DecideStartLimits` (legacy sizing plus the soft cap), `DecideBudgetAction`, the `KmtApi` interface and `GpuPerfReader` policy, `CensusWindow` (the `MCLA_VRAM v=1` line). |
| `src/mcla_vram_census_win.cpp` (new) | The Windows `KmtApi`: `D3DKMTOpenAdapterFromLuid`, `D3DKMTQueryAdapterInfo` (`KMTQAITYPE_NODEMETADATA`, `KMTQAITYPE_NODEPERFDATA`, `KMTQAITYPE_ADAPTERPERFDATA`), `D3DKMTCloseAdapter`, resolved from gdi32 at run time (missing entry points = `kmt=off`, never a failed start). |
| `src/mcla_texture_budget_win.cpp` | Start-up limits through `DecideStartLimits`; cvars `mcla_texture_budget_soft_cap_mb` (int, init-only, default 2048, 0 = off) and `mcla_vram_census` (bool, init-only, default true); the 1 s monitor also samples NON_LOCAL memory and D3DKMT perf data and writes the census line; one soft-cap decision line at start-up; a census-only monitor (no limit changes) when the user set the texture-cache limits; `MarkTextureCacheBudgetCensus`. |
| `src/mcla_texture_budget.h` | Declares `MarkTextureCacheBudgetCensus(std::string_view)` (inline no-op off Windows). |
| `src/mcla_perf_split.cpp` | `perf_split::Mark` (called for every accepted F10 / wheel / pad live snapshot, next to the MCLA_THREAD_CPU window cut) also calls `MarkTextureCacheBudgetCensus(source)`. Chosen instead of `src/mcla_app.cpp` to stay clear of the parallel KI-035 patch. |
| `src/mcla_dev_overrides.h` | One key appended at the end of the table: `mcla_texture_budget_soft_cap_off` (startup phase, writes `mcla_texture_budget_soft_cap_mb=0` for `=true`, nothing for `=false`); enum entry `kTextureSoftCapOff`, `kKeyCount` 18 -> 19 (after the two KI-035 warp keys), one comment bullet. |
| `src/mcla_m10_defaults.h` | `mcla_texture_budget_soft_cap_mb` added to `kNeverWrittenCvars` (14 -> 15): the M10 defaults module never writes it; required by the existing "every override key writes a default or a never-written cvar" test. |
| `tests/mcla_vram_census_win_test.cpp` (new), `tests/mcla_dev_overrides_test.cpp`, `tests/mcla_m10_defaults_test.cpp`, `CMakeLists.txt` | New suite and the key/whitelist checks; CMake: one test block and one source line. |

Behaviour-neutral parts: the census only reads DXGI and D3DKMT and logs. The existing lines
("video-memory budget ... texture cache limits", the 60 s "video memory X MB (peak) of Y MB budget",
"near budget ... hard limit now", the exit peak line) are unchanged, so existing log parsers keep
working. The behaviour change is the soft cap (section 3), which applies only at scale >= 2 on a LOCAL
budget under 10 GB.

## 2. The `MCLA_VRAM v=1` line

Written by the monitor thread (category GPU): once per minute (`reason=minute`, right after the
existing 60 s line), once for every accepted live snapshot (`reason=f10`, `wheel-r2`, `wheel-l2`,
`pad`), and once at exit (`reason=exit`). Each line closes a window: min/avg/max, sample counts and
deltas refer to the time since the previous `MCLA_VRAM` line (any reason). Example (unit-test values):

```
MCLA_VRAM v=1 reason=minute seq=1 window_s=60.0 samples=2 local_mb=6900 local_max_mb=6900
  local_budget_mb=6721 local_budget_delta_mb=-410 local_budget_min_mb=6721 local_budget_max_mb=7131
  local_avail_mb=3360 nonlocal_mb=1200 nonlocal_max_mb=1200 nonlocal_budget_mb=16000
  nonlocal_budget_delta_mb=0 peak_mb=6916 tex_soft_mb=2048 tex_hard_mb=5595 tex_floor_mb=2048
  steps=on near_budget_steps=3 near_budget_s=42 scale=2 kmt=ok gpu_mhz=1200/1500/1800
  gpu_max_mhz=2500 mem_mhz=7001/7001/7001 temp_c=70.0/78.0/86.0 power_pct=40.0/50.0/60.0 fan_rpm=n/a
```

(one line in the log; wrapped here).

| Field | Source / meaning |
|---|---|
| `seq`, `window_s`, `samples` | line number, seconds since the previous line, DXGI samples in the window (1 Hz, plus one at a mark) |
| `local_mb`, `local_max_mb` | `QueryVideoMemoryInfo(LOCAL).CurrentUsage` now and the window maximum |
| `local_budget_mb`, `local_budget_min_mb`, `local_budget_max_mb` | LOCAL `Budget` now and its range in the window |
| `local_budget_delta_mb` | LOCAL budget now minus the budget at the previous line (the first line: minus the monitor's first sample); signed, `+N`/`-N`/`0` |
| `local_avail_mb` | LOCAL `AvailableForReservation` |
| `nonlocal_mb`, `nonlocal_max_mb`, `nonlocal_budget_mb`, `nonlocal_budget_delta_mb` | the same for `DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL` (our system-memory allocations: READBACK/UPLOAD heaps, plus anything VidMm demoted); `n/a` when the query fails |
| `peak_mb` | session peak LOCAL usage (as the exit line) |
| `tex_soft_mb`, `tex_hard_mb`, `tex_floor_mb` | the texture-cache limits as the monitor last set them (user-set limits: as the cvars held them at start-up; `tex_floor_mb` = hard there) |
| `steps` | `on` = the monitor may lower the hard limit; `off` = census only (user set the limits) |
| `near_budget_steps` | 256 MB hard-limit steps taken so far (the "near budget" lines) |
| `near_budget_s` | 1 Hz samples so far with usage + 512 MB above the budget (time spent near/over budget, also after the floor is reached) |
| `scale` | effective draw resolution scale (as the SDK's `GetConfigDrawResolutionScale`, larger axis) |
| `kmt` | D3DKMT reader state: `ok`, `open_failed`, `failed` (5 consecutive samples with both queries failing: closed and not retried), `off` (entry points missing or census off), `wine` |
| `gpu_mhz` | `D3DKMT_NODE_PERFDATA.Frequency` of the first node whose `KMTQAITYPE_NODEMETADATA` engine type is 3D (node 0 if none is reported), min/avg/max |
| `gpu_max_mhz` | `MaxFrequency` of that node (last non-zero value) |
| `mem_mhz` | `D3DKMT_ADAPTER_PERFDATA.MemoryFrequency`, min/avg/max |
| `temp_c` | `Temperature` (deci-degrees C), min/avg/max |
| `power_pct` | `Power`; the WDK header documents it as **tenths of a percent** (of the driver's power limit), not watts |
| `fan_rpm` | `FanRPM` (0 on laptops without a reported fan: `n/a`) |

`n/a`: a driver that leaves a field at 0, a query that fails, or a window without a D3DKMT sample. The
D3DKMT values are min/avg/max of the non-zero 1 Hz samples. Under Wine/Proton (Steam Deck) nothing
changes: `ApplyTextureCacheBudget` still returns before any monitor starts, so there are no
`MCLA_VRAM` lines and no D3DKMT calls (the reader also refuses to open under Wine, unit-tested). With
`--mcla_vram_census=false` there are no census lines and no D3DKMT calls; the budget monitor works as
before.

Start-up lines (once):

```
MCLA graphics: texture cache soft cap capped: scale 2, budget 7131 MB, cap 2048 MB; soft 2048 MB (uncapped 3565 MB), floor 2048 MB, hard 5595 MB
MCLA graphics: video-memory budget 7131 MB (in use ... MB); texture cache limits 2048/5595 MB
MCLA graphics: video-memory census on; D3DKMT perf data ok (3D node 0)
```

### Reading thermal versus paging

Compare the per-minute lines against `MCLA_PERF_SPLIT` GPU ms per frame and the
`MCLA_READBACK_SYNC` max wait of the same minute:

- **Thermal / power limit**: GPU ms per frame up while `gpu_mhz` avg falls well below
  `gpu_max_mhz` (and below the early-session avg), `temp_c` max at a plateau, `power_pct` at its
  plateau; `local_mb` below `local_budget_mb` or flat; `nonlocal_mb` flat.
- **Paging (over-commit)**: GPU ms per frame up with `gpu_mhz` flat; `local_max_mb` at or above
  `local_budget_mb` (often with `local_budget_delta_mb` negative: the OS lowered the budget),
  `near_budget_s` growing by about 60 per minute, and **`nonlocal_mb` rising at a constant total
  footprint** (VidMm demoting our allocations into system memory). The 0.3-1.0 s hitches should
  coincide with those minutes.
- Both can occur in one session; the static pause menu (constant content) is the cleanest window,
  as in the design.

`local_budget_min_mb`/`max_mb` show budget moves inside a minute (l67: 7,131 down to 6,466-6,721 MB).
What the line still cannot show (M11 SDK accessors): how much of `local_mb` is texture cache, render
targets, scaled-resolve heaps or shared memory.

## 3. Mitigation rule (`mcla_texture_budget_soft_cap_mb`)

In `ApplyTextureCacheBudget` (Windows, not Wine, the user did not set
`texture_cache_memory_limit_soft/hard`, LOCAL budget available):

```
hard   = clamp(min(budget * 4/5, budget - 1536), 768, 8192)          (unchanged)
legacy = min(clamp(budget / 2, 384, 4096), hard)                      (unchanged)
if cap > 0 and scale >= 2 and budget < 10240 MB:
    soft = max(min(budget / 2, cap), 384), only if that is below legacy
else:
    soft = legacy
floor  = soft            (the monitor never lowers hard below the floor)
```

- 720p (scale 1): unchanged. Budget >= 10 GB (12/16 GB cards): unchanged.
- 1080p/1440p (scale 2) and 2160p (scale 3) on the 8 GB laptop (budget 7,131 MB): soft 2,048 MB,
  floor 2,048 MB, hard 5,595 MB (was 3,565 / 3,565 / 5,595).
- `reason` in the start-up line: `capped`, `scale_1`, `budget_10gb_or_more`,
  `half_budget_not_above_cap` (small cards whose budget/2 is already under the cap), `off`.
- Overrides: user-set texture-cache limits win (nothing changed, census only);
  `--mcla_texture_budget_soft_cap_mb=<n>` (0..8192) on the command line; in `MCLA_DEV_OVERRIDES`
  builds the file key `mcla_texture_budget_soft_cap_off=true` writes 0 (startup phase, before the
  graphics plugin loads; left alone when the command line set the cvar). Volunteer builds
  (`MCLA_DEV_OVERRIDES=OFF`) never read the file.
- Soft eviction removes only textures unused for 30 s whose last submission completed, so the l17
  hard-limit failure class is not reintroduced. The lower floor also lets the monitor's hard steps
  bite (down to 2,048 + SDK's 72 MB render-to-texture allowance at scale 2) instead of being no-ops.
  Risk: more texture re-creation when revisiting areas (still about 5x the SDK default soft 384 MB).

## 4. Tests

- `mcla_vram_census_win_test` (new): draw-scale resolution like the SDK; `DecideStartLimits`:
  scale 1 unaffected (3,565/5,595), budget >= 10 GB unaffected (11,674 and exactly 10,240 MB),
  10,239 MB capped, scale 2 and 3 with a 7,131 MB budget -> soft/floor 2,048 and hard 5,595,
  5,936 MB budget, cap 0 (the override) restores the legacy values, a user cap of 3,000 wins over the
  default, cap below 384 clamps, cap above budget/2 is a no-op, small budgets; `DecideBudgetAction`
  (Wine first, user-set limits win, no budget); `GpuPerfReader` with a fake `KmtApi` (Wine: no call;
  no API; open failure; 3D node from metadata, node 0 fallback; LUID pass-through; partial data;
  disabled and closed once after 5 consecutive failures; closed by the destructor); the line (all
  fields, budget deltas against the previous line, window reset, zero driver fields as `n/a`,
  non-local `n/a`, census-only `steps=off`, reason sanitising, ASCII single line).
- `mcla_dev_overrides_test`: 19 keys; the new key's name, phase, cvar, `true` -> `0`, `false` ->
  nothing; integer values and the integer cvar name rejected; the l68 "every key" arm string now also
  carries `mcla_texture_budget_soft_cap_off=true` and still fits the 1,024-byte limit.
- `mcla_m10_defaults_test`: the soft cap is a never-written cvar for the M10 defaults.
- Build results, test counts, warning count and ast-grep: section 6.

## 5. Owner A/B (needs an `MCLA_DEV_OVERRIDES=ON` owner build; not run)

Same 1080p Unlocked route on the 8 GB RTX 5060 laptop as l67 (30+ min, the same pause-menu and
freeway segments), two sessions on a warm shader store:

1. **cap on** (build default): no `mcla_texture_budget_soft_cap_off` line (or `=false`) in
   `%LocalAppData%\MCLA-R-Launcher-v1\mcla-dev-overrides.txt`. Expected start-up line
   `texture cache soft cap capped: scale 2, budget ~7131 MB, cap 2048 MB; soft 2048 MB (uncapped 3565 MB), floor 2048 MB, hard 5595 MB`.
2. **cap off**: the file contains `mcla_texture_budget_soft_cap_off=true`. Expected
   `MCLA dev override: mcla_texture_budget_soft_cap_off=true (mcla_texture_budget_soft_cap_mb=0)` and
   `texture cache soft cap off: ... soft 3565 MB (uncapped 3565 MB), floor 3565 MB, hard 5595 MB`
   (the l67 behaviour).

Compare per minute: `local_max_mb` against `local_budget_mb`, `near_budget_s`, `nonlocal_mb`,
`gpu_mhz`/`temp_c`, the existing frames over 50 ms and the `MCLA_READBACK_SYNC` max wait. Accept the
mitigation for a volunteer build when the cap-on session stays under budget (or clearly closer) after
10+ minutes without new texture re-creation hitch clusters; if the cap-on session is still over budget
by a similar amount, the non-texture part dominates and the M11 steps (RT trim, residency) are
needed. One F10 in each session (same spot) also gives a `reason=f10` census line next to the package.

## 6. Build and test record

Filled in from the scratch builds of this patch (see the patch README and the primary's handoff for the
exact hashes):

- Tree: `l68-vram-tree` = main `dc886a3` tracked inputs + ignored `generated/` + this patch.
- SDK: `private/l67-sdk-install` (v0.20.0.8 c63895c, D3D12 only).
- `private/l68-vram-build/vol`: `MCLA_M10_SPEEDUP_DEFAULTS=ON`, `MCLA_PREWARM_SEED_DEFAULT=ON`,
  `MCLA_DEV_OVERRIDES=OFF`, expiry, raw timer, perf split, livery diag2, read-back sync composite ON.
- `private/l68-vram-build/dev`: the same with `MCLA_DEV_OVERRIDES=ON`.

## 7. Proposed KI-048 row additions (for the primary; known-issues.md not edited here)

Status/notes addition: "2026-11-04 l68 patch: per-minute `MCLA_VRAM v=1` census (LOCAL and NON_LOCAL
usage/budget with deltas, limits, near-budget steps/seconds, D3DKMT GPU/memory clock, temperature,
power) and the scale-aware soft cap `mcla_texture_budget_soft_cap_mb` (default 2048 MB at scale >= 2 on
budgets under 10 GB; owner A/B key `mcla_texture_budget_soft_cap_off`). Built and unit-tested; owner
1080p A/B pending. [Evidence](evidence/M10-007-vram-census-and-soft-cap-20261104.md)."

Launcher / tester-guide note (workaround column and tester guide): "on 8 GB cards prefer 720p for long
sessions; 1080p and 1440p cost the same video memory; 2160p is not recommended on 8 GB".

## 8. Risks

- D3DKMT perf queries are WDDM 2.4+ and driver-dependent; a driver may report zeros (`n/a`) or fail
  (`kmt=failed` after 5 s). The calls run on the monitor thread at 1 Hz, never on the CP or render
  thread; their cost was not measured in the game.
- `Power` is a percentage of the driver's limit, not watts; comparisons are within one GPU only.
- The node is the first metadata-reported 3D engine; on hybrid laptops the DXGI adapter LUID is the
  game's adapter (same selection as the budget), not the display adapter.
- The soft cap can raise texture re-creation in revisited areas; the census does not yet count
  re-creations (SDK accessor, M11).
