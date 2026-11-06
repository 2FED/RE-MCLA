# l70 PC owner session 6031364a: Hills Tournament frame drops and picture freezes (KI-069)

Read-only analysis by an Opus subagent on 2026-11-06 of the owner's l70 Hills Tournament session (PC, 1080p Unlocked, 4 F10 packages); accepted by the primary agent.

Read-only analysis by an Opus subagent on 2026-11-06. Build 2788628F... (0.20.37.0), PC MAIN profile, 1080p Unlocked
(window 3840x2160 fullscreen, guest 960x540 at scale 2), texture soft cap 2048 MB (hard 5595), M10 speed-ups on,
`d3d12_livery_diag2=true` and `d3d12_perf_split=1` by build default. The session ran 12:13:08-12:28:10 local
(10:13-10:28 UTC, 15 min) and exited with code 0; the crash folder is empty. Store seed: existing, 775 shaders /
20,364 descriptions (1,091 ms). Sources: the runtime journal (14,618 lines), session mcla.1.log + mcla.log (with
thread ids), the 4 F10 packages (manifest, state.json, report.txt with empty owner fields, frame.bmp, file
timestamps). Local time = UTC+2; the presses were 12:21:02, 12:21:14, 12:22:52 and 12:27:11 local. Minute
windows below are labelled by their end time (for example "->12:22" = 12:21:10-12:22:10).

## 1. Timeline

| time (local) | menu state / event | notes |
|---|---|---|
| 12:13:08-12:14:02 | boot, title, pause/unpause flicker while loading; fullscreen at 12:13:21 | |
| 12:14:02-12:16:20 | game running (free roam) | 30.7, 43.5, 47.3 fps |
| 12:16:21-12:16:30 | paused (timer-scale ramp 1 -> 0.1 first) | |
| 12:16:30-12:19:11 | game running (free roam) | 41-45 fps |
| 12:19:12-12:20:17 | paused 64 s (tournament menus) | 30 fps UI cap |
| **12:20:17** | game running: **race 1 starts** (race clock 0 at about 12:20:26) | |
| 12:21:02.6 | **F10 #1** (HUD 00:35.71, 1/4) | audit armed |
| 12:21:14.8 | **F10 #2** (HUD 00:47.82) | audit still running from #1 (armed=0) |
| **12:22:50.38-12:22:55.86** | **5.5 s freeze: no guest frame (swap) completes** | see section 2 |
| 12:22:52.8 | **F10 #3**, pressed 2.4 s into the freeze; HUD in the frozen frame 02:23.85 | read-back timed out, window capture |
| 12:22:55.86 | second F10 press ignored ("capture already active") | |
| 12:23:00.6-05.1, 12:24:14-15 | paused | |
| 12:25:27-12:25:38 | paused (race 2 transition) | |
| 12:25:38 | **race 2** (race clock 0 at about 12:25:49) | |
| 12:27:11.7 | **F10 #4** (HUD 01:22.52, 3/4) | |
| 12:27:42-12:28:09 | race end: pauses, fullscreen off 12:27:54, KI-035 hook fired 12:27:58.8 (finish-state20 -> state 29, complete=1) | |
| 12:28:10.87 | window closed, exit 0 | |

Per-minute numbers (CP = GPU-command thread; per-frame values divide the window totals by frames):

| window | fps | interval p50/p99/max ms | >50 / >100 | draws/f p50 (max) | CP busy ms/f | CP CPU us/draw | CP kernel ms/f | GPU ms/f | render thread cpu/wait ms/f | fence ms/f | WRM ms/f (max ms) | targeted read-back max ms | VirtualProtect arm us/call | p_tex us/draw | upload memcpy GB/s | GPU MHz min/avg/max, temp max | VRAM / budget MB |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| ->12:14 | 30.7 | 33/66/800 | 25/6 | 3.6k (9.1k) | 14.0 | 2.76 | 1.9 | 12.8 | 32.0/23.0 | 1.7 | 18.5 (235) | 11.5 | 0.75 | 0.79 | 3.39 | 352/1533/2497, 70.5 | 3875/7131 |
| ->12:15 | 43.5 | 22/38/84 | 5/0 | 4.3k (7.7k) | 21.9 | 3.68 | 2.4 | 18.2 | 22.6/7.9 | 4.6 | 1.0 (5.1) | 10.6 | 0.86 | 0.98 | 3.29 | 2370/2521/2685, 73.7 | 4178/7131 |
| ->12:16 | 47.3 | 20/41/85 | 13/0 | 3.2k (7.4k) | 20.1 | 3.53 | 1.8 | 17.3 | 20.8/8.6 | 6.3 | 1.1 (4.7) | 15.4 | 0.90 | 1.00 | 3.39 | 2077/2435/2580, 75.0 | 4609/7131 |
| ->12:17 | 41.3 | 22/48/118 | 18/2 | 3.4k (8.3k) | 21.3 | 3.50 | 2.0 | 19.1 | 23.8/11.4 | 7.0 | 2.9 (29.5) | 16.7 | 0.84 | 0.94 | 3.40 | 1320/2390/2617, 74.3 | 5050/6674 |
| ->12:18 | 44.9 | 21/33/78 | 2/0 | 3.6k (6.1k) | 21.2 | 3.14 | 1.6 | 19.4 | 21.9/10.6 | 8.6 | 1.0 (4.7) | 15.7 | 0.80 | 0.82 | 3.60 | 2077/2506/2602, 75.6 | 5297/6674 |
| ->12:19 | 41.1 | 23/43/91 | 16/0 | 4.3k (7.5k) | 23.3 | 3.47 | 2.1 | 20.0 | 24.0/10.5 | 7.0 | 1.0 (5.0) | 12.9 | 0.91 | 0.84 | 3.32 | 2025/2491/2632, 75.9 | 5284/6674 |
| ->12:20 (menu) | 30.2 | 33/35/51 | 1/0 | 3.9k (6.9k) | 17.1 | 3.76 | 2.3 | 18.5 | 32.7/17.8 | 1.0 | 16.1 (23.5) | 9.5 | 0.83 | 0.87 | 3.57 | 1770/1947/2610, 74.0 | 5290/6674 |
| **->12:21 race 1** | **25.5** | 37/120/306 | 204/23 | 4.6k (7.8k) | **47.4** | **6.95** | **6.1** | 20.1 | 45.8/18.6 | 6.5 | 33.8 (29.7) | **224** | **1.70** | 2.34 | **1.93** | 1672/2091/2235, 66.2 | 5291/6674 |
| ->12:22 | 22.9 | 42/95/360 | 275/12 | 4.1k (6.8k) | 42.2 | 7.86 | 5.9 | 20.3 | 41.5/11.7 | 4.6 | 1.2 (4.7) | 11.1 | 2.28 | 2.34 | 1.74 | 1215/2005/2302, 68.0 | 5291/6674 |
| ->12:23 | 20.4 | 44/104/**5530** | 259/16 | 4.6k (6.7k) | 44.3 | 6.94 | 5.3 | 20.4 | 45.4/33.7 | 4.2 | 7.4 (15.5) | 10.9 | 2.22 | 1.88 | 1.82 | 982/2038/2287, 66.9 | 5293/6545 |
| ->12:24 | 25.4 | 38/70/125 | 124/3 | 3.9k (7.0k) | 38.3 | 7.14 | 5.4 | 20.1 | 38.0/10.7 | 4.4 | 1.0 (4.8) | 10.4 | 2.22 | 1.96 | 1.83 | 1650/2165/2445, 71.6 | 5293/6545 |
| ->12:25 | 29.7 | 33/62/233 | 53/3 | 3.3k (5.7k) | 32.4 | 7.29 | 4.2 | 19.1 | 32.5/8.1 | 4.4 | 1.3 (14.2) | 10.4 | 2.43 | 1.95 | 1.88 | 1635/2238/2460, 74.2 | 5293/6545 |
| ->12:26 race 2 | 26.2 | 34/99/317 | 208/14 | 3.7k (6.7k) | 35.5 | 7.02 | 4.7 | 19.1 | 35.3/11.5 | 4.2 | 2.3 (23.2) | 10.5 | 2.32 | 2.23 | 1.88 | 547/2073/2445, 72.7 | 5293/6674 |
| ->12:27 | 28.6 | 33/78/166 | 178/6 | 4.2k (7.5k) | 33.9 | 5.63 | 4.0 | 20.2 | 33.7/10.3 | 5.5 | 1.1 (4.9) | 12.1 | 1.86 | 1.57 | 2.14 | 1522/2198/2580, 75.1 | 5293/6674 |
| ->12:28 (race end, menus) | 33.0 | 29/69/390 | 123/9 | 5.0k (8.7k) | 25.0 | 3.65 | 2.7 | 20.4 | 29.4/13.3 | 5.3 | 5.0 (29.7) | 20.9 | n/a | 1.10 | 3.31 | 412/2251/2625, 73.1 | 5293/6120 |

- Frames over 500 ms in the whole session: two, both in ->12:23 (5,530 ms at 12:22:50 and 608 ms at 12:22:58.6).
  Over 2,000 ms: one (the 5.5 s freeze). Every other minute's maximum is 390 ms or less (the pacing line's max).
- Pipeline waits: at most 17 waits / 3 ms total per minute (1 ms longest), 0 timeouts; pso_wait 0-0.7 ms per window.
  Not a factor anywhere.
- Streaming: the three audits had stream_stalls 0 (max single read 1.15 ms); audit 3 had 1 audio underrun.
- VRAM: 5.29 GB against a 6.1-7.1 GB budget, near_budget_steps 0 in all 19 VRAM lines. The budget dipped to 6,545 MB
  (12:23-12:25) and 6,120 MB (after fullscreen off at 12:27:54) with no effect.
- Thread CPU (user s per minute): the render thread XThread733C is pegged at 50-56 in every full minute; GPU Commands
  26-36 (free roam) and 32-36 (race), with kernel 4-5 s (free roam) rising to 7.4-8.2 s (race); XThread88B4 (audio
  imports) 14-20 -> 22-31 and XThread44EC 8-12 -> 12-17 in the race minutes; Main XThread 17-27; D3D12 Replay 8-12.

## 2. The freezes

### 2.1 Event A: 5.5 s freeze at 12:22:50.38-12:22:55.86 (package 3 was pressed during it)

- **Frame stream.** The CP logged its last line at 12:22:50.376 (livery composite, frame 20576, draw 34, i.e. early
  in the frame; sync_wait 8 ms and cp_wait5 27 ms, both normal). Its next line is the F10 mark at 12:22:55.896, still
  frame 20576. The pacing line for ->12:23 reports max 5,530,125 us, the only multi-second interval of the session.
  Guest output stayed at 20575 (`D3D12 guest capture: success sequence=20575 last_presented_sequence=20575` at
  12:22:52.826).
- **Nothing else stopped.** The 240 Hz guest vblank counter advanced normally (93,558 at F10 #2 -> 117,074 at F10 #3,
  98.0 s x 240 = 23,520). Other host threads kept creating log messages during the gap: F10 queued (main thread
  t38624, 12:22:52.775), guest capture (t2692, 12:22:52.826), read-back timeout (diagnostics worker t39356,
  12:22:54.281). So this was not a whole-process suspension and not a device hang.
- **The F10 export did not start it.** The freeze began 2.4 s before F10 #3. The package 2 export had finished at
  12:21:22.238, 88 s earlier. F10 #3 then hit the frozen CP: the guest-frame read-back waited for a new frame and timed
  out after 1.5 s (12:22:54.281), so frame.bmp is a 3840x2160 window capture of the frozen image (a black rectangle
  covers the car area in that capture). The main thread spent 3.08 s between its two log calls (12:22:52.775 ->
  12:22:55.858; for F10 #1 these were in the same millisecond). A second F10 press was then ignored at 12:22:55.864.
- **Where the CP spent the 5.5 s.** The F10 perf-split window (12:22:10-12:22:55.9, 45.7 s, 928 frames) was compared
  with the race-window rate per frame (12:21:10-12:27:10):
  - `copy_ms` is 11,833 against 6,261 expected: **+5,572 ms**, matching the freeze.
  - Everything else matches its per-frame expectation within noise: fence_wait +5 ms, readback_wait +28, resolve +33,
    readback memcpy +15, WRM -404, upload, watch arm/disarm, write faults, present +29, pso 0.
  - gpu_busy is +253 ms, so the GPU sat idle (GPU clock min 180 MHz in that window).
  - Per-event maxima in the same window are small: WRM max 4.8 ms, targeted read-back max 10.8 ms, no pipeline wait.
  - `copy_ms` is the inclusive IssueCopy scope. The livery-diag composite/resolve lines are logged from inside
    IssueCopy on the CP thread.
- **Log ordering points at the synchronous logging path.**
  - Not one line from any thread was written to the files between 12:22:50.376 and the end of the gap, although
    lines were created then.
  - In the runtime journal they appear out of timestamp order (12:22:54.281, then 52.826, then 52.775). That happens
    when threads take their timestamp and then queue on a sink mutex held by another thread.
  - The SDK loggers are synchronous `*_mt` sinks with `flush_on(info)`, so every info line is flushed on the calling
    thread. The diagnostics journal sink is added last.
  - The CP's own 12:22:50.376 line is complete in both files, so the CP most likely stalled in the flush or write of
    that line (or in a later sink) while holding the sink lock.
  - The OS-side reason (disk, antivirus or indexer, OneDrive or another filter on the save folder) cannot be seen from
    this data. No other activity is logged at 12:22:50.
- **Classification: (e) something else.** It was a CP thread stall inside IssueCopy that coincides with a blocked
  synchronous log sink.
  - Not (a): the F10 export came later.
  - Not (b): the fence and WRM waits did not grow and the GPU was idle.
  - Not (c): no pipeline waits.
  - Not (d): the per-call costs, high all race, did not jump further, and the other threads ran.
  - "Simulation kept running" fits: only the CP, and threads that log, were blocked; guest game threads do not log.
- **After-effect.** The package 3 minidump caused a second, 0.62 s stop at 12:22:58.63-59.26 (see 2.3). The owner
  therefore saw about 5.5 s plus 0.6 s within 9 s.

### 2.2 Package 2 (12:21:14.797): no stall found

- **Package 1's export was done.** It finished at 12:21:03.132 (save copy 12:21:02.737-02.803, 65 ms; minidump file
  closed 12:21:03.104), 11.7 s before the press.
- **The frame stream ran normally around the press.**
  - The 300-frame audit armed at F10 #1 covers 12:21:02.76-12:21:17.29 (frames 18119-18418), i.e. both presses.
  - Interval p50 45 ms, p99 132, max 306 ms. The 306 ms frame was at 12:21:03.19, package 1's minidump; the other
    intervals were at most 156 ms.
  - In 12:21:10-14.8 the largest interval was 95 ms (12:21:13.73).
  - The CP frame-tagged log shows no gap over 0.25 s from 12:21:03.2 to 12:21:21.6.
- **The package's capture is fresh.** Package 2 captured guest frame 18370 (last presented 18370), and its HUD clock
  advanced 12.11 s against 12.20 s of wall time since package 1.
- **No present stall.** present_ms was 20-22 ms per second in the windows around the press (a 2 s present stall would
  add about 2,000 ms).
- **Package 2's own effect.** Its minidump stopped frames for 0.42 s at 12:21:21.64-22.06, 7 s after the press. The
  export ran 7.4 s in total: 7 s waiting for the dump helper, and only the last 0.42 s froze the process.
- **Conclusion.** The process data show no multi-second freeze at or before package 2. A display-side freeze outside
  the process (DWM or driver presentation of the window) is not instrumented and cannot be excluded. Or the owner's
  first freeze was another event; the only multi-second one in the session is Event A.

### 2.3 F10 exports: a 0.3-0.6 s whole-process stop per package, category (a)

Each package's minidump end time (helper process, `MiniDumpWriteDump` suspends the game's threads) lines up with the
only CP/audit gap at that time:

| package | dump file closed | CP gap | audit interval |
|---|---|---|---|
| #1 | 12:21:03.104 | 12:21:02.765-03.105 (0.34 s) | 306 ms |
| #2 | 12:21:22.060 | 12:21:21.638-22.061 (0.42 s) | after the audit ended |
| #3 | 12:22:59.248 | 12:22:58.632-59.256 (0.62 s) | 608 ms |
| #4 | 12:27:12.587 | 12:27:12.153-12.598 (0.45 s) | 390 ms |

The save copy (56-142 ms) and guest capture (15-50 ms) caused no visible stall.

## 3. The Hills drop to 21-30 fps: CP-bound from a host per-call slowdown (KI-067 signature)

- **Onset at the race start.** The drop begins in the window that contains the race 1 start (12:20:17). It persists
  through both races and is gone in ->12:28 (race end and menus), with nothing logged at either edge. The audit at
  12:27:11 still shows 7.2 us of CP CPU per draw, so recovery came after 12:27:11.
- **CP-bound.**
  - CP busy is 32-47 ms per frame, 97% of wall time, against a 34-49 ms frame interval.
  - The render thread waits on it (render_wait 8-34 ms/f).
- **Not GPU-bound.** GPU work is 19-20 ms per frame (50% utilisation), the GPU clock averaged 2.0-2.2 GHz (2.4-2.5
  before the race), and the maximum temperature fell to 66-75 C.
- **Not more work.** Draws per frame p50 is 3.3-4.6k, the same as the 41-47 fps free-roam minutes (3.2-4.3k).
- **Every host unit cost roughly doubled at once** (race vs this session's free roam):

  | cost | free roam | race | ratio |
  |---|---|---|---|
  | CP CPU per draw (us) | 3.1-3.8 | 5.6-7.9 | about 2x |
  | CP kernel time (ms/frame) | 1.6-2.4 | 4.0-6.1 | about 2.5x |
  | VirtualProtect arm (us/call) | 0.80-0.91 | 1.70-2.43 | about 2.5x |
  | Texture phase (us/draw) | 0.82-1.00 | 1.57-2.34 | about 2x |
  | Upload memcpy (GB/s) | 3.3-3.6 | 1.74-2.14 | halved |

  - XThread88B4 and 44EC used 1.5-2x the CPU per minute at half the frame rate.
  - Uniform inflation of user code, syscalls and memcpy at unchanged workload is the KI-067 shape (l68: CP CPU per
    draw 2.2-2.8 -> 4.4-5.1 us, arm 0.53-0.65 -> 0.99-1.44 us). This episode is larger, lasted about 7 minutes, and
    again was in a championship/tournament race.
- **Against the l69 freeway minutes** (7.1-7.3k draws/f, CP busy 29 ms/f, CP CPU 2.94-3.10 us/draw, p_tex 0.90-1.06
  us/draw, arm 0.57-0.58 us, kernel 2.8-3.0 ms/f, memcpy 3.8-3.9 GB/s, GPU 22-24 ms/f, 32-36 fps):
  - The Hills draw about 40% fewer calls per frame, yet the CP needs 30-60% more time per frame. Its per-draw cost
    is 2.4x, VirtualProtect 3-4x and memcpy half the freeway values.
  - Even this session's free roam ran about 1.3-1.5x slower than l69's per-call baseline (arm 0.80-0.91 vs
    0.56-0.61), so the host was already slower than in l69 before the race.
- **Cause.** The host CPU probably ran slower: clock or power limit, power mode, battery, or outside contention. It
  cannot be confirmed because there is no CPU clock, temperature or load telemetry; the TSC rate is nominal 1,996 MHz.
  The repeated coincidence with championship/tournament races (more AI cars, audio thread busier) is worth testing,
  but the doubled VirtualProtect and memcpy unit costs are not content-dependent.
- **Spike pattern in the race.** 124-275 frames over 50 ms and 3-23 over 100 ms per minute. The audits' per-frame
  work equals the interval (late frames are also heavy frames), the same as in KI-067.

## 4. Safety counters and warnings

- **Exit and audits.** Exit code 0, crash folder empty, lifecycle "guest exit after window close". All 3 audits PASS
  (stalls 0, shader/pso 0; audit 3 had 1 audio underrun). No warp watchdog (limit 0, fires 0). The KI-035 hook fired
  once at the race end and completed.
- **Read-back, WRM and output.** Read-back aborted 0, full_waits 0, fallbacks_to_full 0, forced_wait_failed 0,
  gpu_blocks_dropped 0, guard_misses 0. WRM aborted 0. guest_output_refresh_failures 0, swap_source_missing 0.
- **Memory watch.** memory_watch_protect_failures 0 and arm_gap_splits 0.
  write_fault_already_cleared rose to 465 by package 4 (l69: 1,479 by the end) and disarm_gap_lock_misses to 44.
- **New warnings, all consequences of Event A:**
  - `MCLA diagnostics: guest frame readback timed out; using window capture` (12:22:54.281)
  - `snapshot request ignored; capture already active` (12:22:55.864)
- **Usual warnings:** t:\mc4\art\city VFS misses, one invalid texture-fetch-constant warning, missing
  gamecontrollerdb, the startup cross-thread RtlLeaveCriticalSection compatibility note.
- **Larger waits.** One 224 ms targeted read-back wait (->12:21, near the race start) and one 235 ms WRM wait
  (->12:14, loading). No others over 30 ms.

## 5. What to change

1. **Take logging off the CP's critical path.**
   - Use an async spdlog logger with a non-blocking overflow policy, or drop `flush_on(info)` to warn with
     `flush_every(1-2 s)`.
   - At minimum, stop CP-thread diagnostics (livery diag2, about 14 lines/s, on by default in this volunteer build)
     from doing synchronous flushed file writes.
   - Add a sink-write timer that logs any single write or flush over 100 ms, with the thread, to confirm Event A's
     mechanism the next time.
2. **F10 minidump.** Make the live minidump optional, or move it off the default F10 path (for example write it only
   for crash or hang reports, or after an explicit second key). Each one freezes the whole process for 0.3-0.6 s, and
   pressing F10 to report a freeze adds another freeze.
3. **Freeze telemetry.** Add a CP watchdog: when no swap completes for over 1 s, log the CP's current packet type,
   scope (IssueCopy, draw, WRM, fence, present) and duration, plus a one-shot host stack sample of the CP thread. A
   guest-swap-age field in the F10 state would also help.
4. **KI-067 / Hills slowdown.**
   - Add CPU effective MHz (CallNtPowerInformation or APERF/MPERF), CPU temperature if available, system CPU load,
     and AC/battery and power-mode state to the minute line.
   - Ask the owner whether the laptop was on AC and in the best-performance mode, and whether anything else ran.
   - Then repeat the Hills Tournament once with the same settings, to see whether the onset is tied to the race start
     (content) or random (host).
