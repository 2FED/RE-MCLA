# l82 Steam Deck A/B (D:\82, 2026-11-10): CP bulk register writes

Read-only analysis by an Opus subagent (logs/json only, python -I; no snapshots/dumps opened; both sessions exit 0, crash folders empty), accepted by the primary agent. Owner report: audio still crackles, cars still jump (expected: l82 changes neither). Spark not used: deterministic log parsing.

**Verdict: no measurable effect.** default vs bulkregs (driving windows): fps 16.09 / 16.16, CP busy 51.6 / 51.0 ms/f, packets_other 10.97 / 10.98 ms/f (1838 / 1872 ns per draw). Counters confirm the arm: regs_bulk 40.0 k/f, bulk_runs 19.9 k/f, i.e. runs average 2.01 dwords, so bulk mode trades ~40 k per-register calls for ~20 k bulk calls; ceiling ~0.2-0.8 ms/f, far below the 1-2.5 ms/f estimate in M11-l82-sdk-bulk-registers (which was too high). The regs_* path does not cover most of packets_other (type-0 single-register packets, type-1, direct writes, constant blocks ~75 k dwords/f). One run per arm: savings >= 1 ms/f unlikely (moderate confidence); < 0.5 ms/f neither seen nor excluded. `cp_bulk_register_writes` stays default off; not a Deck lever. Audio (KI-060) unchanged: underruns 63-71/s in the driving audit, XAudio engine thread F8000534 at 67-89 % of a core. Cadence unchanged: 3/4/5+ vblank mix ~70/195/35 of 298 frames, sd 13.6-14.4 ms.

---

# l82 Steam Deck results (D:\82), read-only analysis, 2026-11-10

Inputs: D:\82\sessions\<id>\{mcla.log,launch-receipt.txt,state.txt,managed.toml,capabilities.txt,store-seed.txt},
D:\82\diags\<id>\{report.txt,latest-live.txt}, live\*\{state.json,manifest.json} (names only for the rest).
Not opened: before-snapshot.txt / after-snapshot.txt, save-private, save-files-private.json, log-tail.txt,
frame.bmp, runtime\*.log (duplicate of mcla.log). No *.dmp exists. Parsers (python -I) in
%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\d82-work\: ab.py (new: per-window + regs_* counters),
parse.py / agg.py / audit.py / tags.py (copied from d81-work), states.py (adapted). Outputs ab_out.txt,
p_default.txt, p_bulkregs.txt, agg_out.txt, audit_out.txt, states_out.txt. Spark not used: deterministic
parse of two logs with existing parsers; checking a model answer would cost more than the parse.
Method as l81: driving = MCLA_PERF_SPLIT windows seq >= 2 (minute 1 = menus/load excluded). Local clock UTC+2.

## 1. Sessions
| arm | session | local start-end | length | F10 (local) | proof |
|---|---|---|---|---|---|
| deck\default | b5ca5328 | 10:36:19-10:41:39 | 322 s | 10:37:44, 10:39:20, 10:41:36 | 9 override values, no cp_bulk_regs |
| deck\bulkregs | 0dc4d158 | 10:44:12-10:49:49 | 338 s | 10:45:34, 10:47:10, 10:49:46 | 10 values; `MCLA dev override: cp_bulk_regs=true (cp_bulk_register_writes=true)` |
- Both `complete` / `exit 0`, `Execution complete`, 3 general F10 packages each, crash folders empty.
- Build 6E4ECD30...74830E37 in launch-receipt, capabilities and report = build_manifest_sha256 of
  private/l82-internal/candidate-receipt.json. Package mcla_version 0.20.37.0 (same string as l81).
- Same base config both arms: wine m10 defaults (bridge, ahead16), KI-060 backoff after=32, F8000534 above_normal
  (bp 1), apu workers above_normal, short delays hires_timer. Default session seeded the shader store (first run,
  667 shaders added); bulkregs found it existing.

## 2. A/B (driving, seq >= 2)
| per frame | l82 default | l82 bulkregs | l81 default |
|---|---|---|---|
| frames / wall | 3986 / 248 s | 4277 / 265 s | - |
| fps | 16.09 | 16.16 | 16.24 |
| draws/f | 5968 | 5867 | 5742 |
| CP busy ms/f (% wall) | 51.6 (83%) | 51.0 (82%) | 50.4 (82%) |
| CP busy per draw | 8.64 us | 8.69 us | 8.77 us |
| CP CPU (kernel) | 44.7 (7.3) | 44.3 (7.1) | 43.7 (7.3) |
| packets_other ms/f (ns/draw) | 10.97 (1838) | 10.98 (1872) | 10.4 (~1810) |
| draw_packet / draw / regions / upload | 37.8/22.2/13.2/11.3 | 37.4/21.9/13.1/11.1 | -/21.4/13.4/11.5 |
| watch arm/disarm/wfault | 6.60/3.04/5.38 | 6.39/2.83/5.18 | 6.61/2.89/5.23 |
| readback / fence wait | 0.62/0.54 | 0.53/0.46 | 0.65/0.56 |
| GPU busy (% wall) | 26.3 (42%) | 26.1 (42%) | 26.2 (43%) |
| render CPU / wait | 53.9/8.6 | 53.5/7.8 | 53.1/8.4 |
| regs_bulk / regs_single / bulk_runs per f | 0 / 40639 / 0 | 39972 / 137 / 19930 | n/a |
- Counters: default regs_bulk=0, bulk_runs=0 every window; regs_single 38.6-42.4 k/f (cvar-off path counts all
  range dwords as single, as designed). bulkregs: 99.7% of range dwords via bulk, **2.01 dwords per run**,
  6.8 range dwords per draw (same total as default: 6.81 single/draw).
- Per window spread (us/draw CP): default 8.41-8.86, bulkregs 8.21-9.09; packets_other ns/draw default 1691-2020,
  bulkregs 1735-1934. Matched by scene-ish minute 3: pk_other 11.48 vs 12.20 ms/f, us/draw 8.41 vs 8.21.
- Verdict lever 2: no measurable effect. A -1 ms/f saving (-9% packets_other) would put the aggregate near
  1670 ns/draw; observed +34 ns/draw (+2%). One run per arm, different routes: a saving >= 1 ms/f is unlikely
  (moderate confidence); a saving < ~0.5 ms/f cannot be seen or excluded. Mechanism: runs average 2 dwords and
  only ~40 k range dwords/f exist, so the virtual WriteRegister calls removed are ~40 k/f minus 20 k bulk calls;
  at ~10-20 ns each the ceiling is roughly 0.2-0.8 ms/f, well under the 1-2.5 ms/f estimate. packets_other is
  dominated by something else (type-0 single/type-1 packets, direct WriteRegister, constant blocks
  fetch_dw_written ~74-76 k/f not counted in regs_*).

## 3. Audio (unchanged vs l81)
| audit (300 frames after F10) | default | bulkregs | l81 default |
|---|---|---|---|
| #1 early | 102 (5.6/s) | 257 (13.8/s) | 79 (3.7/s) |
| #2 driving | 1386 (70.7/s) | 1248 (62.6/s) | 911 (46.9/s); l81 arms 47-62/s |
| #3 (cut by exit) | 33 in 59 (8.8/s) | 1 in 50 (0.3/s) | 273 in 57 (pursuit) |
- F8000534 (XAudio engine) 67-89% core driving (kernel 2-3%), 85-89% in late minutes, both arms (l81 74-90%).
- F800068C (sound manager) 10-17% (kernel 2-3%) both arms (l81 10-18%).
- MCLA_SLEEP backoff both arms: zero delays 0.11-0.20 M/min, backoffs 25-114 k/min (12-66 s), streak_max 346-705,
  top_site 82136738 every minute, alertable 0, fallbacks 0, max_us 5.7-9.5 ms. Same as l81.
- Audit #2 default 70.7/s is above the l81 range but within the scene-driven spread (l80 audits ~2000 per 300).
  Owner "still crackles" fits: 60-70 underruns/s = 32-38% silence. Not touched by l82.
- XMA fault counters 0; stream_stalls 0-2.

## 4. Cadence (KI-054 context)
| | default | bulkregs | l81 default |
|---|---|---|---|
| pacing fps per minute 1..5 | 22.8/16.9/17.8/14.5/15.1 | 22.6/16.1/16.8/16.8/15.3 | 23.4/16.0/18.4/16.2/15.0 |
| p50 ms min 2..5 | 51.9/50.4/66.6/66.4 | 66.3/65.8/50.5/66.1 | 66.3/50.4/66.1/66.3 |
| >100 ms per minute 2..5 | 7/6/48/49 (mean 27.5) | 5/14/36/57 (mean 28.0) | 17/6/18/58 |
| audit #2 3/4/5+ vblanks | 74/194/32 | 62/197/41 | 65/213/21 |
| audit #2 dt sd, cadence changes | 14.4 ms, 122/298 | 13.6 ms, 126/298 | ~9-11 ms, 107/298 |
| audit #1 3/4 vblanks, changes | 169/130, 162/298 | 127/167, 169/298 | - |
- Same flip-flop 3/4-vblank cadence; late-minute hitch rise (36-57/min) in both arms, as l81. Audit #2 dt sd is
  higher than l81 (more 5+-vblank frames). Not arm-dependent.
- Snapshot timer_step 0.0525-0.0672 s, pursuit dt 0.03-0.07 s (no pursuit active this time; warper 29).

## 5. Warnings / counters
- Same benign set as l81 (VFS city probes, self-test rejections, XeKeys, RtlLeaveCriticalSection, texture fetch
  constant invalid type, SDL gamecontrollerdb). No new class.
- One extra warning in default F10 #2 (10:39:20): `MCLA diagnostics: guest frame readback failed; using window
  capture` - known class (seen in l256, M11-015). That package's frame.bmp is 4,096,054 B (window size) instead
  of 3,686,454; manifest still `frame: true`. Not opened.
- memory_watch_protect_failures 0 in all 6 state.json; READBACK guard_misses=0 (16 lines); MCLA_LOG dropped=0
  (26 lines). Watch protect/min arm calls 293-409 k (15.8-17.1 us/call) both arms, as l81.

Nothing here is a physical PASS.
