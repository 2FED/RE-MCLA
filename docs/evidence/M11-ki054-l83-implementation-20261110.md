# KI-054 traffic / camera jitter: l83 host implementation (Arms 0, 1, 2; Arm 3 not done)

Implementation subagent (Opus), 2026-11-10, from the design
`docs/evidence/M11-ki054-traffic-jitter-design-20261110.md`. Implemented from the design's
descriptions and the guest addresses in our own generated code only; no code from LARecomp,
holdmysocks or RenanQueiroz. Everything is default off. Nothing was run in the game; no physical
result is claimed. Spark not used (bounded edit of known files; validation against generated code
and unit tests was cheaper done directly).

Base: main `c164309`. The subagent could not write to the shared checkout (session guard), so the
change was authored as a copy-only patch (`orig/`, `new/`, `SHA256SUMS.txt`, `apply.ps1`) and
verified outside the checkout; the primary applies it and runs the official gates.

## 1. Keys (dev overrides, 76 -> 79 keys; never-written 64 -> 67)

| key | cvar (host init-only bool, default false) | phase | arm |
|---|---|---|---|
| `ambient_dt_probe` | `mcla_ambient_dt_probe` | startup | 0: probe (read-only) |
| `ambient_dt_snapshot` | `mcla_ambient_dt_snapshot` | startup | 1: kick-time delta at sites A and B |
| `raw_timer_hitch_hold` | `mcla_raw_timer_hitch_hold` | startup | 2: hitch step held to 1.1x average |

`=true` writes `true`, `=false` writes nothing; no conflict pairs. The three cvars are in
`kNeverWrittenCvars` (host-owned, never an M10 default). `raw_timer_hitch_hold` is logged as
unavailable in a build without `mcla_raw_timer.cpp` (timer configuration probe build).

## 2. Arm 0, ambient delta probe (done)

New `src/mcla_ambient_dt_probe.{h,cpp}` (pure parts in the header, unit-tested). Seven
function-granularity strong overrides; with both switches off each is one static bool test and the
original call. Guest memory is never written; the only guest reads are the main timer fields and
words the guest itself reads at the same point (racer +656/+48, pose +140).

Hook sites (generated/default at the M11 codegen):

| hook | role | evidence |
|---|---|---|
| `sub_826D9578` (kick), lr `0x822C26BC` | snapshot +8 (kick dt), +73 split flag; open window (release) before the original signals the worker | `mcla_recomp.13.cpp:60275` (call from 0x822C26B8), body `mcla_recomp.54.cpp:4738` |
| `sub_826D95B0` (join), lr `0x822C2D08` | after the original waited: close window, count, log per 300 windows | `mcla_recomp.13.cpp:61264`, body `mcla_recomp.54.cpp:4775` |
| `sub_826D75D8` (ambient step), lr `0x826D9500` | marks the worker call inside the window (thread-local), merges its counts before the worker signals done | body `mcla_recomp.54.cpp:3`, worker call `mcla_recomp.54.cpp:4658` (inline path lr 0x822D3D20 is excluded) |
| `sub_822A7E28` (racer splitter) | counts racers with +656 == 3 and +48 != 0, inside / outside a window | `mcla_recomp.12.cpp:68964` |
| `sub_82387A18` (predicate), lr `0x826D7958` / `0x826D7994` | site A: the gate call returns 0 -> the next call sees f30 as loaded by `lfs f30,8(r30)` at 0x826D796C | `mcla_recomp.21.cpp:38608`; site `mcla_recomp.54.cpp:533`, `:545`, `:573` |
| `sub_826FBA60` (suspension update), lr `0x826FA378`, r20 == 0 | site B: f1 as loaded by `lfs f1,8(r19)` at 0x826FA300, passed unchanged | `mcla_recomp.55.cpp:23768`; site `mcla_recomp.55.cpp:20130`, call `:20205` |
| `sub_826FA8A8` (pose countdown) | site C: +8 sampled at entry when +140 bit 0x40 (load 0x826FA8E0 is 14 instructions in, no call before it) | `mcla_recomp.55.cpp:21031`, load `:21069` |

Additional static fact found while implementing: `sub_821BD910` (the split) saves the full delta in
timer +80, sets +73 = 1, +76 = N, +8 = dt/N; the restore pass copies +80 back and clears +73
(`mcla_recomp.5.cpp:2782`). The game loop restores +8 = f28 just before the kick
(`stfs f28,8(r27)`), so the kick value is the full frame delta.

Sites A and B are exact (the register the guest loaded); site C is sampled at entry
(approximation, a few instructions before the guest load). Reads by anything other than the worker
step inside a window are counted as `outside` (thread-membership check for the design's question).

Log (probe only), once per 300 completed kick/join windows (one per game update with ambient
traffic, so frames = windows), plus one `kind=start` line at the first kick:

```
MCLA_AMBDT v=1 kind=start probe=1 snapshot=0 kick_dt=0.038000 rate=26.316 scale=1.000 unscaled=0.038000 split=0
MCLA_AMBDT v=1 probe=1 snapshot=0 window=300 frames=300 windows=300 overlaps=4 racer_splits=9
  racer_outside=0 worker_steps=300 split_reads=1 reads=1/1/0 split=1/0/0 outside=0/0/0 served=0/0
  max_abs_dt_delta=0.020000 min_ratio=0.5000 kick_dt=0.040000 join_changed=0 kick_split=0
  last=[41:a:0.040000:0.020000]
```

(one line in the log; wrapped here; values from the unit test.) Fields: `overlaps` windows with at
least one racer split; `racer_splits` racer-splitter calls in state 3 inside windows (a call with a
zero pass count only restores, so this is an upper bound of real splits); `reads`/`split` per site
a/b/c; `served` Arm 1 replacements at a/b; `last` up to three most recent mismatches as
`window:site:kick_dt:seen_dt`; `join_changed` +8 at the join differs from the kick value;
`kick_split` the split flag was already set at the kick.

F10: `state.json` gets an `"ambient_dt"` object with the session totals (same counters) when either
switch is on (`MclaAmbientDtStateJson`, host counters only; `mcla_app.cpp` -> `RuntimeState` ->
`mcla_diagnostics.cpp`); absent when both are off.

## 3. Arm 1, kick-time delta (done for sites A and B; site C not served)

With `mcla_ambient_dt_snapshot`, on the worker inside an open window, when the loaded value
differs from the kick value by more than 1e-6 s:
- site A: after the predicate at lr 0x826D7994 returns, f30 := double(float kick dt) (f30 is
  non-volatile; the guest then uses it for the ambient clock and countdown);
- site B: before the suspension update, f1 := kick dt.
Equal values are untouched (pause: kick 0 -> read 0; network path `min(+92, 0.05)` is excluded by
the gate predicate; the non-threaded path is not the worker). Racers and the main thread are never
changed.

Not served: site C (`sub_826FA8A8`, 0.5 s pose re-validation countdown). The guest loads +8 from
memory and uses it inline with no call in between, so the value can only be changed by patching
guest code at instruction level or by writing the shared timer global (which racers read); neither
was done. The fallback in the design (join before the racer splitter) was not implemented; it is
only needed if the probe shows site C or other +8 readers on the worker matter.

## 4. Arm 2, raw timer hitch hold (done)

`StepSmoother` moved unchanged from `mcla_raw_timer.cpp` to `src/mcla_step_smoother.h`
(namespace `mcla::raw_timer`) and given a hitch-hold flag. A raw step > 0.1 s applies
`min(0.1, 1.1 x average)` (0.1 s if there is no average yet) instead of 0.1 s; `raw - applied`
goes to the existing debt (same 1 s cap), the average is untouched, and the debt is repaid by the
existing 2 %/update, +-10 % mechanism. Race clock: every second not applied at the hitch is in the
debt and is applied later, so applied + debt == measured exactly while the debt is under the cap
(unit-tested over 600 steps with three stutters, both modes); with the hold the 1 s cap is reached
about 0.06 s of stall sooner per hitch. The `MCLA timer stats` line gains `; hitches N hold 0|1`
at its end (cumulative hitches; appended only).

## 5. Arm 3, continuous-time camera filters (not done)

`camBoomCS::Update` `sub_82320298` (`mcla_recomp.17.cpp:26900`) loads the position factor with
`lwz r11,240(r31)` / `lfs f13,224(r11)`, halves it below 60 fps (`fmuls f13,f13,f30`) and uses it at
0x82320468 in the same basic-block chain; the look-at factor (`lfs f0,228(r11)`, 0x823204F4) the
same way. There is no call between the loads and their uses, so the factors are not reachable at
function granularity. The only function-granular alternative is to rewrite the tune fields
(+224/+228 of the tune object shared by the six camBoomCS instances) around each call and restore
them afterwards, i.e. a temporary guest-data mutation of shared tuning; not done (task rule:
report instead of instruction-level or risky patching). Needs an SDK mid-asm hook or owner approval
of the tune rewrite.

## 6. Tests and checks (outside the main checkout)

Gate-equivalent compile of the four changed host TUs (`mcla_ambient_dt_probe.cpp`,
`mcla_raw_timer.cpp`, `mcla_diagnostics.cpp`, `mcla_app.cpp`) with the gate build's mcla defines,
includes and flags: 0 errors, 0 warnings (also 0 with `-Wall -Wextra`). Unit tests built and run:

| suite | result |
|---|---|
| `mcla_ambient_dt_probe_test` (new; checks addresses, return addresses, registers and instruction counts against generated code) | 44 checks passed |
| `mcla_step_smoother_test` (new) | 1423 checks passed |
| `mcla_dev_overrides_test` | 1222 checks passed |
| `mcla_m10_defaults_test` | 4748 checks passed |

The two new suites are registered in the CMake `foreach` with the other hook tests (53 -> 55
executables). `ast-grep scan` (repo rules) on all new/changed C++ files: no findings. No new line
over 100 columns, no trailing whitespace, no tabs; each file keeps its line endings.
Pending for the primary after applying: `gate_host_build.ps1 build`, `l57_host_tests.ps1`,
`git diff --check`.

## 7. Arms (`private/l83-arms`, sparse, LF)

`pc/default` and `deck/default` are byte copies of the l82 defaults with the first line replaced by
`# l83 arm <dev>/default`; `ambprobe` adds `ambient_dt_probe=true`; `hitchhold` adds
`ambient_dt_probe=true` and `raw_timer_hitch_hold=true`; `snapshot` adds `ambient_dt_probe=true`
and `ambient_dt_snapshot=true`. All eight parse with the production parser (l82 sparse checker
rebuilt against the new header): 0 ignored lines, 0 mismatches.

## 8. Risks / open questions

- Site A/B identification relies on return addresses and r20 (checked against generated code by
  the new test); a regenerated image must rerun that test.
- `sub_82387A18` is a widely used predicate; the hook adds one static-bool test (and, when on, a
  return-address compare) per call.
- Thread membership of sites B and C is assumed by the design; the `outside` counters answer it.
- Arm 1 changes only the delta used by ambient traffic during a racer split; whether that removes
  the visible shake is an owner A/B (design test plan), not claimed here.

## 9. Addendum 2026-11-10 (l84): Arm 1 is inert in retail

The owner's l83 PC run had the F10 `ambient_dt` block with `windows=0`, `worker_steps=0` and
`outside=[17887,26164,0]` (two packages). Static reason, pinned in
`tests/mcla_ambient_dt_probe_test.cpp` since l84: the kick at 0x822C26B8 is skipped unless the
ambient manager (0x8286D8E4) exists and its byte mgr+0x64259 is set; the manager constructor
`sub_826D8E70` sets that byte to (word +4 of the command-line parameter object 0x8288E8F4 != 0),
and the parameter's static registration (`sub_827AE6B0` -> `sub_821C06C8`) stores 0 at +4. On the
retail command line the "[MC] Ambient Traffic" worker is never kicked and the ambient step runs
inline on the main thread (`sub_822D3C20`, return 0x822D3D20). H1 as written (the worker reads a
split delta) is refuted for retail, and `ambient_dt_snapshot` (served only inside a kick/join
window) is a no-op there. The key is kept so that arm files stay valid; it is in no l84 arm.
Probe v2 (l84) measures the inline path: `docs/evidence/M11-l84-owner-build-20261110.md`.
