# KI-066 (fined while standing still near police): first l71 pursuit-probe session

Read-only analysis by an Opus subagent on 2026-11-06 of the first l71 police session (720p profile, pursuit probe on, 10 F10 packages); accepted by the primary agent.

Session 473b720b... (PC, l71 arm triad, pursuit probe on), 2026-11-06 15:54-16:03 local
(13:54-14:03Z). Sources: `MCLA_PURSUIT v=1` lines in mcla.1.log + mcla.log (561 lines in total:
272 fn=score, 285 fn=state, 4 fn=tactic), and the F10 packages in `live/`. Only the `pursuit` block
of state.json, manifest/report, log-tail and frame.bmp were read. The session folder holds **10**
packages (live-...-5 to -14), not 14; the report.txt package list matches. Times below are local
(log clock), mm:ss after 15:00 or 16:00.

## Verdict (short)

Fine 2 ("a fine out of nowhere while standing at a light next to a cop") is the game's own
**block-traffic** rule with a police witness. It is not a host artefact:

- Events: 16:03:05.934 `block traffic start` +50 (0->50), then 8 x `block traffic/sec` +25 at
  06.989, 07.991, 09.018, 09.889, 10.963, 11.962, 12.966, 13.931 (score 250). That triggers the
  pull-over (StartTactic type 1) at 13.931, the ticket at 16.672 and a $100 fine.
- There is no decay in between. Decay ("no law broken", -10/s, lr 827085CC) runs only on frames
  with no infraction, and the block-traffic bit (0x4000) stayed set for the whole 8.0 s.
- dt on every line was 0.017-0.026 s. The 8 per-second ticks span 7.997 s from the start event
  (1 Hz in guest time, which equals wall time). No dt spike or catch-up.
- The latch cleared on resolution (latched=none at 03:16.672). busy56 went 1 -> 0 at the end of
  the cut scene (03:35.093). grace 4.98 s counted down normally.
- Frames: package -9 (03:12, score 200) and -10 (03:16, "PULL OVER") show the car at **0 mph**
  in the right lane at a **red** light, with a police car in the left lane beside it. The pull-over
  car pointer (M+84) changed from BF51B4E0 (fines 1 and pursuit) to BF51D980, so this was the
  "other cop".

**No fix is needed.** One probe logging gap is listed in section 6.

## 1. Timeline

| Time (local) | Event | F10 |
|---|---|---|
| 54:50.955 | Manager present, score 0, thresholds 250/500/600/1000 | |
| 55:56.778 | `block traffic start` +50 (first, short stop); decays to 0 by 56:02 | |
| 56:10-56:49 | Single `block traffic/sec` +25 (56:10.678, 56:46.303; the timer +148 carries over between stops), `off-road start` +50 at 56:12.356; all decay to 0 | |
| **58:09.843** | **`red-light` +250 (0->250) -> pull-over tactic (type 1, hud 1)** | |
| 58:10.8-58:17.8 | `pull-over/sec` +10 x8 (250->330); FSM 0->1->2->3 | |
| 58:18.161 | Resolved: score 0, latch cleared, grace 4.978 s, busy 1 | |
| 58:18.966 / 58:25.974 / 58:37.150 | CopCam cut scene / Ticket cut scene / end (busy 0). **Fine 1 = $100** | |
| 58:59.461 | Block-traffic bit set (standing after the ticket) but **no score** (no witnessing cop, see section 4) | |
| 59:15.019 | `block traffic start` +50, +25/s x4 to 150 (59:19.000), then decay -10/s to 30 | |
| 59:42.240 | `mid-speed start` +250 (30->280) -> **second pull-over tactic** | |
| 59:43.783 | `hi-speed start` +600: clamped to **599** while the pull-over tactic is active | **-5** (59:44, 599, pull-over) |
| 59:46-59:59 | red-light x3, wrong-way start +130, per-second hi-speed/pull-over/wrong-way; score held at 599; fine field 100->133 | |
| **00:00.206** | `pull-over/sec` 599->609 -> **pursuit tactic (type 2)**, 18.0 s after the pull-over start (the owner ignored it) | **-6** (00:02, 659) |
| 00:05.903 | Score at max (999) | |
| 00:32.0 / 00:35.0 / 00:37.97 | Pursuit FSM 1->6->1->6 (6 until the end) | **-7** (00:51, 999, FSM 6) |
| 00:55.100 | `other vehicle` +130 (collision) | |
| ~01:22.7 | **Escape** (last score line 01:22.718; the transition into grace was paced out of the log, see section 6) | |
| 01:27.706 | Grace ended: score 0, latch none, tactic none, last_result52 = 2 (escaped), no cut scene; fine field frozen at 436 | **-8** (01:25, grace 2.66 s) |
| 02:46.628 | Last speeding bit clears; no infraction for 19.3 s (approach to the light) | |
| **03:05.934** | **`block traffic start` +50 (witnessed)** | |
| 03:06.989-03:13.931 | `block traffic/sec` +25 x8 -> 250 | **-9** (03:12, 200, 0 mph, red) |
| 03:13.931 | Pull-over tactic (type 1), FSM 2 -> 3 (03:14.947) | **-10** (03:16, 270, "PULL OVER") |
| 03:16.672 | Resolved: grace 4.98, busy 1 | |
| 03:17.420 / 03:24.465 / 03:35.093 | CopCam / Ticket / end. **Fine 2 = $100** | **-11** (copcam), **-12, -13** (ticket) |
| 03:35.918 | Block-traffic bit again (still standing; the light is green in -14), no score | **-14** (03:39) |

There are 4 tactic starts: pull-over 58:09, pull-over 59:42, pursuit 00:00, pull-over 03:13. There
were no arrests and no arrow traps or double pursuits.

## 2. Fine 1: straightforward

A single `red-light` event (lr 82708168) adds 250, which equals the pull-over threshold exactly,
so the pull-over starts on the same frame. Then there are 8 x `pull-over/sec` +10 at 1 Hz while
the owner stops. The tactic resolves in 8.3 s, followed by CopCam, Ticket and $100. dt was normal
(0.020-0.027). Nothing odd.

## 3. The pursuit

The score did **not** reach 600 by accumulation in free roam:

1. Witnessed block traffic after the ticket left the score at 30.
2. `mid-speed start` +250 started pull-over #2.
3. `hi-speed start` +600 went 300 -> 599. While the pull-over tactic is running, the score is held
   one below the pursuit threshold.
4. The owner kept driving (speeding, red lights, wrong way). After about 18 s, `pull-over/sec`
   pushed the score to 609, which started the pursuit (lr 827087F8).
5. The score reached the 999 cap in 6 s and stayed there. The FSM moved between states 1 and 6.
6. The pursuit ended in an escape at about 01:22.7: 5 s of grace, then score 0, latch cleared,
   last_result 2, no cut scene.

The pending-fine field M+76 rose by about $1 every 0.3 s during the pursuit (133 -> 436). It was
not reset on escape, and the next pull-over overwrote it with 100. Two frames had dt = 0.1000
(the game's clamp) at 00:09.719 and 00:12.734. These are hitches during the pursuit and do not
affect any fine.

## 4. Fine 2: the block-traffic rule as implemented by the game code

The per-target infraction bits are computed by `sub_82704F88` (55.cpp:46891), which clears M+96
every frame. The block-traffic part (55.cpp around 47290) works like this:

- If `sub_82702C08(manager, target)` is true:
  - When the latched bit 0x4000 is **not** set, the timer at +148 accumulates dt until it reaches
    T[276]. Only then is bit 0x4000 set in M+96. This is a pre-delay, so the real stop began
    T[276] seconds before 03:05.934.
  - When the bit is already latched, it is set at once.
- If the condition is false and the bit is not latched, the timer resets to 0.

The detector `sub_82702C08` (55.cpp:41443, read statically; medium confidence) returns true only
when all of these hold:

- The police are present (`*0x8286D8E4`).
- The player's current tactic is not a pull-over.
- The vehicle is on a road (`sub_826FD0A8`) and not in the three excluded road states
  (`sub_826FD0E8/088/0C8`).
- The vehicle speed (vehicle+8 -> +224) is below a fixed constant when no road segment is found.
  Otherwise it must be below T[280] x a constant x the segment's speed limit (`sub_822447B8`; the
  fallback when the limit is 0 is T[236]).
- If the lane record has a stop point (`sub_82700230/82700290`, nonzero xyz) and the car is within
  a radius of it, the direction test (`sub_8216C900` against the constant at 0x820C5630) must pass.
  When it fails, the function returns false. This looks like the exemption for a car waiting
  behind a stop line, but the meaning is inferred.
- A physics query (`sub_82574418`, mask 7) along the vehicle's longitudinal axis must hit
  something. In other words, another vehicle is in the probed lane segment.

Scoring is a separate step. In the Update loop `sub_827078E8`, the infraction scoring block
(loc_82707C30 onward, including the 0x4000 block at 54912/54945) runs only when there is a
**witness**. That is either:

- the player's active tactic target,
- `sub_82702A28`, or
- a cop in M+4[] whose flags +872/+1072 are set, that is not busy (`sub_82701DE0` returns 0),
  and for which `sub_822AAFC8` (cop, player) passes.

Otherwise the code jumps to 827083F4 and nothing is scored. This explains why the bit at 58:59
scored nothing until 59:15, and nothing after 03:35.

For 0x4000 itself: on the first frame (latch clear), the code sets the latch, zeroes the timer and
adds T[108] (live: 50, "block traffic start", lr 827083F4). After that, the timer +148 adds dt and
pays T[112] (live: 25, "block traffic/sec", lr 827083C0) per whole second.

**Summary:** a stationary car in a lane with traffic in the probed segment and a witnessing police
car earns 50 + 25/s, so it reaches 250 in **8.0 s** after the pre-delay.

Every input on this path is guest state: vehicle speed, lane, stop point, physics query, cop flags
and dt. The logged dt was normal and the cadence was exactly 1 Hz, so per-second accrual was not
inflated. The car was really at 0 mph (frames -9 and -10). The latch cleared on resolution. Fine 2
is therefore **console-accurate behaviour**.

The one open question is whether the stop-line exemption should have spared the owner at a red
light. Frame -10 shows the car's nose at or just over the transverse stop line. If the car's
reference point was past the lane stop point, the game's direction test does not exempt it, and
console behaviour would be the same. I checked the host translation of the exemption's vector
compare: `vcmpeqfp128.` -> `cr6.setFromMask` (lt = all true) -> `mfocrf r9,2` -> mask 0x80
(cr6.lt). It is correct. There is no evidence of a host defect.

## 5. The l69 "fined after pause at 0 mph" case

This case is consistent with block-traffic accrual. At the live tuning, the score reaches 250
after T[276] + 8.0 s of witnessed stationary blocking (the RE prediction of 100 + 15/s was wrong;
the live values are 50 + 25/s). That matches "about 9 s after resume". Update returns
while paused, so the pre-delay timer and the latch freeze and resume. A unit was directly behind
the player (minimap) in l69, which supplies both the witness and possibly the query hit. A
large-dt explanation is not supported here. dt never exceeded the clamp outside two pursuit
hitches.

## 6. Odd items in the state lines

- Busy, grace and cut-scene flags behaved correctly at both tickets (grace 4.98 -> CopCam ->
  Ticket -> busy 0, about 19 s each). Nothing stuck.
- The score was held at 599 during pull-over #2 for 18 s. This is the game's escalation design,
  not a fault.
- The latch persists until resolution. Combined with the shared +148 timer, this gives early
  per-second ticks (for example 56:10.678, 0.39 s after the bit returned). This is game logic.
- Pending fine M+76 stays at 436 after the escape. It is a stale field, not a charge.
- **Probe gap (diagnostic only):** the state pacer (burst 20, refill 500 ms) was saturated during
  the pursuit by `fine`-only changes, about 3 per second. 91 state lines carry suppressed >= 1.
  The pursuit -> grace transition at about 01:22.7 was lost: the 01:27.706 line reads
  `changed=grace from_phase=grace suppressed=1`. No score line was ever suppressed.
  - Optional probe tweak: do not count `fine`-only changes against the pacer (or log them at most
    once per second), so that phase transitions always get through.
  - Optional detector hook on `sub_82702C08`, logging which branch fired (no segment,
    speed/limit, stop-point direction, query hit) and the player speed. This would settle the
    red-light exemption question on the next occurrence.
- Package -9 has dt 0.0115 (the F10 capture frame). This is benign.
