# l93 time of day: always-on read-only log and default-off dev controls (2026-11-12)

Status: implemented, built and unit-tested (gates green, owner build l93 staged:
[M11-l93-owner-build-20261112.md](M11-l93-owner-build-20261112.md)); not run in the game, so
no physical PASS.
Design and RE: [M11-time-of-day-re-20261112.md](M11-time-of-day-re-20261112.md). Implemented
from our own RE notes and the generated code only; no code from LARecomp or GPL forks. Spark
not used (private generated-code reading and a small host implementation; validation is the
deterministic unit and generated-code test).

## Design decisions (accepted by the coordinator, 2026-11-12)

- One host int cvar per group, not one per key: `mcla_tod_rate` (1 = the game's own rate,
  10, 30, 0 = freeze) and `mcla_tod_start_minutes` (0 = off, 270, 330). The dev-override test
  invariant is that keys writing one cvar are exactly the conflict pairs, so the three rate
  keys are a group of three and the two start keys a pair.
- The gate is session-wide: the rate controls act after the first Arcade race preset warp of the
  process and stay enabled until the game is closed, including free roam or career afterwards.
  Hence the owner checklist runs the dawn arms on a profile copy, never MAIN.
- The start keys replace the target of every preset of the Arcade race preset call (Midnight,
  Dawn, Noon and Dusk alike), not only Midnight / Dawn.

## Context: the l92 lead-strict run (owner, 2026-11-12)

The owner's l92 pc/lead-strict run on 2026-11-12 (session 568cdc6b, build 103B9D6F confirmed,
key applied) showed no flicker: lead2_frames 0, fence_late_f2p 0, 0 timeouts over the
session, and oq_drains = 0 in every minute (the game issued no occlusion queries in that run,
so the occlusion-query drain is not a factor there). l93 adds the dawn arms to reach the 06:00
switch quickly with and without the strict lead.

## What is on by default and what is not

| Part | Default | Guest writes |
| --- | --- | --- |
| Stage 0: MCLA_TOD lines and the F10 `time_of_day` block | always on | none |
| `mcla_tod_rate` (keys `tod_rate_x10`, `tod_rate_x30`, `tod_freeze`) | 1 = off | hour + its render slot |
| `mcla_tod_start_minutes` (keys `tod_start_0430`, `tod_start_0530`) | 0 = off | warp target register f1 |

Both cvars are host init-only ints (src/mcla_tod.cpp). With both at their defaults no guest
memory or register is written; the two overrides only call the original and read.

## Hook sites (generated/default)

- `REX_EXTERN(sub_822F1DB0)` (mcla_recomp.15.cpp), the time-of-day update. Its only caller in
  the whole generated tree is sub_823045B0 (mcla_recomp.16.cpp: `lwz r3,-7572(r11)` with
  `lis r11,-32120`, `bl 0x822f1db0`, return 0x823045D0), so r3 = *0x8287E26C and the hook runs
  once per frame on the update task. The test pins, in order: the auto byte `lbz r8,14545(r31)`,
  the warp flag `lbz r11,14552(r31)`, the pause object `lwz r11,-12640(r9)` (0x828CCEA0) with
  `lbz r10,4(r11)` / `lbz r10,10(r11)`, the band rate `stfs f13,14548(r31)`, the advance
  `lfs f13,8(r8)` (0x827D7508) / `fmadds f0,f12,f13,f0` / `stfs f0,14540(r31)`, the wrap at f3
  (`lfs f3,29504(r10)` = 0x82007340, 24.0); the warp branch (`lfs f0,14556(r31)`,
  `lfs f0,88(r8)` unscaled delta, `stb r7,14552(r31)` with r7 = 0 throughout); and the publish
  `lwz r11,-17936(r11)` (0x8288B9F0), `addi r10,r11,3633`, `rlwinm r9,r10,2,0,29`,
  `stfsx f0,r9,r31`, with no call and no other store after it. The function's only call is the
  warp-path wrap helper (return 0x822F1FAC).
- `REX_EXTERN(sub_822F1518)` (mcla_recomp.15.cpp), the warp start: `lfs f0,14540(r3)`,
  `fcmpu cr6,f1,f0`, `beqlr cr6`, then `stfs f2,14548(r3)`, `stfs f1,14556(r3)`,
  `stb r11,14552(r3)`. Seven direct callers in the generated tree; exactly one returns to
  0x8265D61C: the Arcade race preset sub_8265D2B8 (mcla_recomp.50.cpp: `lwz r31,-7572(r11)`,
  `bl 0x8238eb90` (preset table, return 0x8265D60C), `mr r3,r31`, `lfs f2,30712(r10)`
  (0x820077F8, 3.0 h/s), `bl 0x822f1518`, return 0x8265D61C; f1 is not written between the
  table call and the warp call).
- Night state (read only, not hooked): sub_822F3BD8 compares the getter hour (sub_822F10D0)
  with `lfs f0,14568(r31)` (`bgt` -> night) and `lfs f0,14564(r31)` (`bge` -> day). The hook
  computes `night` with the same thresholds read from the manager (6.0 / 18.0 at boot): night
  when hour > high or hour < low, unknown (-1) when the thresholds are not sane.

## Stage 0 (always on, read-only)

After the original update the hook reads *0x8287E26C (non-null, 4-byte aligned, room for
+0x3900 bytes; otherwise nothing is read), then hour +0x38CC, rate +0x38D4, target +0x38DC,
night thresholds +0x38E4 / +0x38E8, auto byte +0x38D1, warp flag +0x38D8 and the pause state
(*0x828CCEA0 checked the same way; paused = byte +4 set and byte +10 clear). The steady clock is
read on the first update and every 16th; the shared state is published for F10 under a mutex on
those updates and on every line. Cost per frame: about ten guest loads, the compare logic and
(1 in 16) a clock read.

Line (one per real minute, plus immediately on every night flag change and warp start / end;
`why` joins coincident reasons with `+`):

```
MCLA_TOD v=1 why=<first|minute|night|warp_start|warp_end> hour=HH.hhh rate_hps=R auto=0|1
  warp=0|1 target=HH.hhh paused=0|1 night=-1|0|1 ctl=<off|rate10|rate30|freeze>
  start=<off|0430|0530> gate=0|1 warp_lr=0xXXXXXXXX acts=N
```

(one line in the log; wrapped here). `gate` is the Arcade marker, `warp_lr` the return address
of the latest sub_822F1518 call, `acts` the control write count. F10 `time_of_day`: version,
valid, hour, rate_hps, auto, warp, target, paused, night, night_low, night_high, ctl, start,
gate, warp_lr, and the session counters updates, lines, night_changes, warp_starts,
warp_calls, preset_warps, start_replaced, acts, blocked.

## Controls (default off)

- Rate / freeze (`mcla_tod_rate`: 10, 30 or 0): before the original the hook reads the hour and
  the warp flag; after it, it acts only when the gate is open, the auto byte is set, no warp is
  pending before or after the update, the simulation is not paused, the manager is the one the
  game updated, the render slot index is 0 or 1, and the step is an ordinary advance (forward
  delta across 24:00 in (0, 0.05] h). Rate: hour = wrap(after + (k - 1) x delta) into [0, 24);
  freeze: the pre-call hour is written back. The value goes to +0x38CC and to
  mgr + 4 x (3633 + *0x8288B9F0), exactly the slot the game just published.
- Start (`mcla_tod_start_minutes`: 270 or 330): only at return address 0x8265D61C, f1 is set to
  4.5 / 5.5 before the original runs (any preset of that call; the original then starts the
  warp unless the hour already equals it). Every other caller is untouched.
- Gate: the marker "Arcade preset warp seen in this session" is set by the warp hook on any
  call returning to 0x8265D61C (with or without a start key). Until then the rate controls do
  not act (counter `blocked`, one `MCLA_TOD_CTL v=1 kind=gate_blocked` line). The first write
  logs one `MCLA_TOD_CTL v=1 kind=act ctl=... before= after= written= slot=` line; every start
  replacement logs `MCLA_TOD_CTL v=1 kind=start from= to= start= lr=0x8265D61C`.
- Dev keys (kKeyCount 98 -> 103, startup phase, =false writes nothing): `tod_rate_x10` (10),
  `tod_rate_x30` (30), `tod_freeze` (0) write mcla_tod_rate and form a group of three (three
  conflict pairs); `tod_start_0430` (270) / `tod_start_0530` (330) write mcla_tod_start_minutes
  and form a pair. A rate key and a start key combine. Both cvars are in the M10 never-written
  list (84 -> 86). Conflict pairs 18 -> 22.

## Tests

Gate build rc 0, 0 compiler warnings; gate tests 57 executables, `failed executables: 0`
(`mcla_tod_test: 59 checks passed`, `mcla_dev_overrides_test: 1834 checks passed`,
`mcla_m10_defaults_test: 6265 checks passed`); the l93 owner build repeated the same counts.

tests/mcla_tod_test.cpp: layout static_asserts (every offset and address, the lis/addi pairs),
pointer and slot checks, hour wrap / forward delta / step plausibility, rate x10 / x30 /
freeze fix-up including across 24:00 and the jump / backward / invalid cases, the night
predicate with the manager's thresholds, the pause predicate, MayAct and the gate, start
minutes and the preset-only replacement, the minute / edge scheduler, the line and JSON
formats, and the generated-code checks above (whole tree: one update caller, seven warp
callers, one return to 0x8265D61C). tests/mcla_dev_overrides_test.cpp and
tests/mcla_m10_defaults_test.cpp updated per convention.

## Risks and not done

- The gate is session-wide (see the design decisions): use a copy of the profile for the dawn
  arms (not MAIN); whether the hour reaches the save is not proven (RE section "risks").
- Not proven that sub_8265D2B8 is Arcade-only; the RE names it the race preset caller.
- Ambient density by hour (mcAmbientTod) follows the faster clock; traffic and pedestrians may
  change faster than in a normal session.
- Not done: no in-game menu, no hot reload, no other warp callers, no garage-byte control.

## Save behaviour (owner, 2026-11-12)

Owner fact (2026-11-12): the save DOES remember the hour of day (the owner uses it to "teleport": start a race where needed and quit, and the game keeps that time and place). So an altered clock would be written into the save: the dawn arms must only be used on a COPY of the profile, never on MAIN.
