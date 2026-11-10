# l81 Steam Deck analysis (D:\81, 2026-11-10)

Read-only analysis by an Opus subagent (logs/json only, python -I; no before-user/after-user/dumps opened; three clean exits, crash folders empty), accepted by the primary agent. Owner report: PRESS START 20-24 -> 24-27 fps, driving better; audio still crackles and slows down at speed; traffic cars now visibly jump back and forth (KI-054). Spark not used: deterministic log parsing.

**Headline.** fps 16.2-16.5 in the driving windows (l80: 14.3); CP busy 50-54 ms/f (l80: 62.8); memory-watch arm cost 6.4-7.2 ms/f (l80: 12.5) because the M10 Wine defaults are back on. The Sleep(0) backoff removed the sched_yield storm (zero delays 0.2 M/min vs 2-10 M/min native; sound-manager thread 10-18 % of a core vs 42-84 %) but the audio underruns are unchanged (47-62/s, 25-33 % of output time silence; pursuit 40 %): the starved producer is the game XAudio engine thread F8000534 at 74-90 % of a core, 80 % of it in title sound code 82146BE0/82147738; above_normal changed nothing measurable. Census: hot pages v2 is a NO-GO (always-hot blocks carry 66 % of arms, unstable set; projected CP -1.2..-2.2 ms/f vs the -4 ms/f threshold). Work moves to lever 2 (l82 CP bulk register writes).

---

# l81 Steam Deck results (D:\81), read-only analysis, 2026-11-10

Inputs: `D:\81\sessions\<id>\mcla.log` (+ launch-receipt, capabilities, state.txt), `D:\81\diags\<id>\report.txt`,
`live\*\{state.json,manifest.json,report.txt}`. Not opened: before-user / after-user, save-private,
save-files-private.json, log-tail.txt, frame.bmp, any dump (there are none). `runtime\*.log` is a duplicate
of `mcla.log` (same lines, no category tag), not parsed separately. Parsers (python -I) in
`%LocalAppData%\Temp\claude\C--BDU-MCLA-Recomp\d81-work\`: tags.py, parse.py, agg.py, audit.py, census.py,
states.py; outputs p_*.txt, agg_out.txt, audit_out.txt, census_out.txt. Spark not used (deterministic parse
of three logs; checking a model answer would cost more than the parse).

Method as in the l80 analysis: minute 1 (menus/load) excluded; "driving" = every MCLA_PERF_SPLIT window
with seq >= 2 (minute + F10 windows, non-overlapping); per-frame = window sums / frames. Times are the
Deck's local clock (UTC+2).

## 1. Sessions, arms, build

| arm | session | start-end (local) | length | F10 (local) | override proof |
|---|---|---|---|---|---|
| deck\default | aa34eaab | 09:37:37-09:43:17 | 340 s | 09:39:20, 09:40:56, 09:43:13 (pursuit active) | 9 values; `memory_watch_census=false`; no zero/prio keys |
| deck\zero-native | 3925fbe5 | 09:45:32-09:50:33 | 302 s | 09:46:56, 09:48:30, 09:50:30 | 11 values; `zero_delay_native=true (ke_delay_zero_mode=native)`, `guest_audio_prio_normal=true` |
| deck\census | 5968d12c | 09:51:38-09:57:12 | 336 s | 09:52:58, 09:54:33, 09:57:09 | 9 values; `memory_watch_census=true` |

- All three: `complete`, `exit 0`, `Execution complete`, three F10 live packages each, crash folders empty.
- Build: launcher `build 4CB252D3...AA2F1F` = `build_manifest_sha256` of `private/l81-internal/candidate-receipt.json`
  (whose mcla.exe is 3C7EDF3E...253469, launcher 13EA4343...); package `mcla_version 0.20.37.0`. The log has
  no exe-hash line; identity rests on the manifest hash.
- **Wine watch defaults are ON in all three arms** (fixes the l79/l80 caveat): `MCLA m10 default:
  memory_watch_disarm_bridge_safe_gaps=true` and `MCLA m10 default: shared_memory_upload_ahead_pages=16`;
  census lines `bridge=1 ahead=16`; arm calls ~14 pages each (l80: 2.1).
- KI-060 lines: default/census `repeated zero guest delays mode=backoff after=32 max_us=500 gap_us=200`,
  `guest thread F8000534 (... start 823EC8C8) host priority above_normal` (state.json: F8000534
  base_priority 1); zero-native `mode=native`, `boost normal` (base_priority 0). `apu_worker_priority`
  above_normal in all (XMA Decoder / Audio Worker bp 1). Short delays `hires_timer` in all.

## 2. Performance

### Driving aggregates (seq >= 2)

| per frame | default | zero-native | census | l80 default | l80 census |
|---|---|---|---|---|---|
| fps (perf windows) | 16.24 | 16.47 | 16.39 | 14.29 | 14.45 |
| draws / frame | 5742 | 5705 | 5758 | 6263 | 6086 |
| CP busy ms/f (% wall) | 50.4 (82 %) | 53.9 (89 %) | 50.7 (83 %) | 62.8 (90 %) | 61.6 (89 %) |
| CP busy per draw | 8.77 us | 9.45 us | 8.80 us | 10.03 | 10.13 |
| CP thread CPU (kernel) | 43.7 (7.3) | 45.1 (7.6) | 44.0 (7.2) | 51.6 (12.9) | 50.8 (12.6) |
| wrm_mem | 7.1 | 6.4 | 6.5 | 6.7 | 7.2 |
| GPU busy (% wall) | 26.2 (43 %) | 26.1 (43 %) | 25.9 (42 %) | 29.5 (42 %) | 28.9 |
| render CPU / wait | 53.1 / 8.4 | 56.4 / 10.0 | 52.8 / 7.8 | 65.3 / 16.5 | 64.4 / 16.2 |
| read-back / fence wait | 0.65 / 0.56 | 0.82 / 0.70 | 0.67 / 0.58 | 1.17 / 1.01 | 0.81 / 0.70 |
| draw / regions / upload | 21.4 / 13.4 / 11.5 | 23.1 / 14.5 / 12.3 | 22.1 / 12.9 / 11.1 | 25.1 / 20.5 / 18.2 | 24.5 / 20.3 / 18.1 |
| watch arm / disarm / write fault | **6.61** / 2.89 / 5.23 | 7.16 / 3.22 / 5.87 | 6.39 / 2.79 / 5.10 | 12.5 / 4.7 / 7.2 | 12.3 / 4.7 / 7.1 |
| packets_other / copy | 10.4 / 3.8 | 10.9 / 4.0 | 10.5 / 3.9 | 11.4 / 4.4 | 11.3 / 4.1 |
| replay CPU | 15.0 | 15.3 | 15.0 | 16.5 | 16.2 |
| watch protect/min arm calls (us/call) | 368-420 k (15.5-16.3) | 392-405 k (16.7-17.9) | 331-405 k (16.0-16.6) | 790-881 k (11.9-13.3) | 795-823 k |
| TimerQueue late > 1 ms / min (min 2+) | 760-1052 | 1210-1943 | 855-1132 | 1127-1797 | 931-1978 |

Per-minute pacing (minute 1 = menus/load):

| minute | default fps / p50 / p99 / >100 ms | zero-native | census |
|---|---|---|---|
| 1 | 23.4 / 34.1 / 67.4 / 7 | 23.1 / 34.0 / 83.3 / 6 | 23.9 / 33.9 / 84.2 / 12 |
| 2 | 16.0 / 66.3 / 100.7 / 17 | 17.5 / 50.6 / 83.3 / 2 | 17.8 / 50.5 / 83.1 / 4 |
| 3 | 18.4 / 50.4 / 83.5 / 6 | 18.6 / 50.2 / 81.8 / 0 | 18.4 / 50.3 / 67.7 / 2 |
| 4 | 16.2 / 66.1 / 117.0 / 18 | 14.6 / 66.7 / 99.8 / 6 | 14.7 / 66.4 / 166.3 / **67** |
| 5 | 15.0 / 66.3 / 199.7 / **58** | (session ended) | 15.1 / 66.5 / 133.8 / **43** |

Readings:
- **The fps gain is real and comes from the watch defaults.** fps +14 % against l80 at 8 % fewer draws.
  CP per draw fell 10.0 -> 8.8 us. Arm cost halved (12.5 -> 6.6 ms/f): half the calls (~400 k/min against
  ~830 k/min), each a bit dearer (16 us against 13 us, at 14 pages per call). The l80 projection
  ("restore ahead16 + bridge: CP -3 to -4 ms/f") was exceeded: CP busy -12 ms/f. Only part of that is
  the arm; the rest is draw_regions/upload (-7 ms/f) and lower kernel time.
- **The backoff's own CP effect:** zero-native has the same watch config, and its CP per draw is 9.45 us
  against 8.77 (+0.7 us, +8 %). Its arm calls cost 16.7-17.9 us against 15.5-16.3. TimerQueue lateness is
  1.5-2x higher. That is the sched_yield storm taking SMT and kernel time from the CP. fps is equal
  (16.47 against 16.24) because frames are quantised to 3- or 4-vblank intervals and the scenes differ.
- **The CP is still the first wall** (82-89 % of wall) at about 50 ms/f against a 50 ms (3-vblank)
  target. That explains why the cadence now flips between 50 and 67 ms (section 5).
- The owner's PRESS START 24-27 fps fits minute 1's 23.1-23.9 (that minute includes loading).

## 3. Audio

### MCLA_SLEEP (per minute; top_site always 0x82136738)

| minute | default (backoff) | zero-native (native) | census (backoff) |
|---|---|---|---|
| 1 | zero 0.10 M, backoffs 17 k (7.3 s), streak_max 79 | **zero 2.23 M** | zero 0.11 M, backoffs 33 k (17.7 s) |
| 2 | 0.18 M; 82 k (45.4 s); 595 | 4.79 M | 0.20 M; 75 k (40.8 s); 453 |
| 3 | 0.21 M; 74 k (39.8 s); 351 | 5.63 M | 0.19 M; 83 k (46.8 s); 477 |
| 4 | 0.17 M; 92 k (52.9 s); 423 | **10.22 M** | 0.17 M; 117 k (67.3 s); 548 |
| 5 | 0.17 M; 117 k (67.8 s); 720 | - | 0.17 M; 110 k (63.8 s); 507 |

- **The backoff works as designed.** Zero delays drop about 25-60x (10.2 M/min to 0.17-0.21 M/min).
  The sound-manager thread F800068C drops from 42-84 % of a core (15-38 % kernel; zero-native, the same as
  l80) to 10-18 % (2-3 % kernel). That frees about 0.3-0.7 logical CPU. alertable=0 and fallbacks=0 in
  every minute; max_us is 4.7-8.8 ms in all arms.
- **New fact: `top_site` is 0x82136738, the bank-lock spin `while (*(u8*)0x828317A1) Sleep(0)` in
  sub_82136700. It is not the tick end 0x821449D0.** That is 99.9 % of the zero delays in native mode and
  75-97 % in backoff mode. `zero_streak_max` is 351-720, so this thread waits on the flag for up to about
  0.2-0.35 s at a time. The sound tick is gated by a lock or flag that another thread holds, not by its own
  timing. zero_backoff_us exceeds 60 s per minute in later minutes, so more than one thread backs off
  (zero minus top_site_zero is 30-36 k per minute from other sites).
- Guest samples confirm this. In backoff arms F800068C sits 85-87 % in SleepEx (82460270) and 5 % in
  XMsgInProcessCall. Native: 57 % and 16 %. XMP GetStatus polling time fell about 3x, so ticks are about
  3x rarer.

### Underruns (REX_PERF_AUDIT, 300 frames after each F10)

| audit | default | zero-native | census | l80 default / census |
|---|---|---|---|---|
| #1 (early driving) | 79 (3.7/s) | 145 (8.4/s) | 67 (3.6/s) | 467 / 325 |
| #2 (driving) | **911 (46.9/s)**, stalls 1 | **1159 (59.3/s)**, 0 | **1200 (62.3/s)**, 2 | 2038 / 1907 |
| #3 (cut short by exit) | 273 in 57 frames (75.8/s; **police pursuit**) | 53 in 56 (15.3/s) | 2 in 43 | - |

- **The backoff did not fix the crackle.** In audit #2, backoff arms show 47 and 62/s against 59/s native,
  which is within run-to-run noise. Underruns are lower than l80 per audit, but the zero-native control is
  lower too. So the drop belongs to the overall l81 load reduction or to the scene, not to the backoff.
- What an underrun means (SDK `src/audio/sdl/sdl_audio_driver.cpp:300-311`, header `channel_samples_ = 256`):
  - the SDL callback finds no guest audio frame queued and inserts 256 samples of silence, 5.33 ms;
  - **47-62 underruns/s means 25-33 % of output time is silence**, so the guest produces only about 67-75 %
    of real-time audio (187.5 frames/s needed);
  - in the pursuit window (default audit #3) it is 76/s, about 40 % silence.
  - This matches "crackles and slows down at fast driving". The sound is chopped by silence gaps, and the
    game's audio falls behind real time because the producer cannot keep up. Silence insertion does not
    change pitch, so "slows down" is the stretched or late delivery, not a resample. Owner audio is not
    recorded, so this is an inference.
- **Which thread is starved or saturated: F8000534**, the game XAudio engine thread (start 823EC8C8, by the
  new KI-060 line). It uses 74-90 % of a core while driving with 2-3 % kernel, rising in the late and
  pursuit minutes (default min 5 / F10 #3: 87-89 %; census min 4-5: 88-90 %). That is the same as l80
  (79-85 %), so the priority boost (bp 1 confirmed) changed nothing measurable. Its samples are 79-85 %
  "after KfLowerIrql" inside title sound code 82146BE0 / 82147738 (plus the 823EFD30 / 823EE050 / 823F4450
  library). Only 4-9 % are in KeWaitForSingleObject. So it is a **compute-bound producer** on one core, not
  a thread waiting on a lock. The 75-90 % CPU against 100 % wall leaves room for SMT sharing or preemption.
  The data cannot separate the two (there is no run-queue wait counter).
- Correlation with load: in audit #2, frames longer than 70 ms carry more underruns (default 4.2 against
  2.9 per frame; zero-native 6.2 against 3.5; census 5.7 against 3.7). The early-driving audits have
  0.2-0.5 per frame. Underruns follow the audio thread's load (more voices in fast driving or pursuit),
  not F800068C.
- XMA: every fault counter is 0 in all arms (`MCLA_XMA` shows only swaps and `canary_fix=1`). The build
  emits **no MCLA_XMA_SIB / MCLA_XMA_CONSUME / audio-route lines**, so consume gaps cannot be read.
  MCLA_CPU is start-only on Wine (times=off).
- TimerQueue (`MCLA_TIMERQ`): dispatches 60 k/min, late > 1 ms 760-1052/min with backoff against
  1210-1943 native; max_late 3.8-8.9 ms. Not an audio factor.

## 4. Census (deck\census) and the hot pages v2 go/no-go

The design's criterion (`docs/evidence/M11-deck-memwatch-design-20261108.md`, section 3, Acceptance):
"Deck CP busy at least -4 ms/f. Render fault time at least -2.5 ms/f. ... If the census in stage 0a/0b
projects less than -3 ms/f of CP, stop after stage 0 and move to lever 2." Lever 2 is the CP
draw/packet fast paths (design line 260).

Census minutes 2-5 (3964 frames, 240 s, 16.5 fps), ahead 16 + bridge on:

| per frame | l81 census | l80 census (ahead 0, bridge 0) | l66 "all" |
|---|---|---|---|
| arm calls (ms, us/call, pages/call) | **383 (6.38 ms, 16.65 us, 14.1)** | 917 (11.9, 13.0, 2.1) | 479 (8.0, 16.7, 14) |
| arms by pages | 5-16 p: 339/f (5.71 ms); 17-64: 14; 2-4: 23; 1: 7 | 1 p: 632/f | 92 % in 5-16 |
| arms by source | IB 177 (3.01 ms), VB 140 (2.29), tex 65 (1.07), resolve/other 0 | IB 473, VB 386, tex 58 | IB 242, VB 167, tex 70 |
| disarm calls (ms, pages/call) | 142 (2.77, 40.3); bridged gaps 36/f | 364 (4.6, 5.4) | 185 (3.7, 38) |
| render write faults (ms, us each) | 133 (4.66, 35.0 us); other 8.7 (0.38); CP 0 | 94 (6.6, 70 us) | 176 (6.4, 36.6) |
| arm / disarm hold | 6.98 / 2.94 ms | 12.7 / 4.9 | 8.7 / 3.9 |
| VB / IB fast path | 97.2 / 96.4 % (req 4920 / 4783) | 93 / 91 % | 96-97 % |
| h8 / h4-7 / h1-3 / h0 share of arms | **66.5 / 28.5 / 3.8 / 1.2 %** | 88.1 / 7.9 / 3.0 / 1.1 | 92 % h8 |
| h8 share of faults / upload pages | 64.9 / 66.6 % | 81.1 / 81.8 | 91 / 92 |
| h8 blocks per minute window | 49, 32, 15, 2 (h4-7: 7, 45, 51, 88) | 42-48 | 65 |
| h8 blocks with a texture watch | 12, 8, 1, 2 (avg 5.8 of 24.5) | 4-11 of 44 | 22 of 65-71 |

- **The hot set is not stable while driving.** h8 holds 84 % of arms in minute 2, then falls to 49-50 % in
  minutes 4-5 (pursuit / fast driving), when only 2-15 blocks stay hot in 8 of 8 frames and 51-88 are
  4-7-of-8.
- Projection, as in l80 section 4.3:
  - h8 arms 255/f x 16.65 us = 4.24 ms/f, of which 77 % is eligible (h8 blocks with a texture watch
    excluded), so gross arm saving -3.24 ms/f;
  - minus 1-2 ms/f prologue and staging, **net CP -1.2 to -2.2 ms/f**;
  - render-fault side: 64.9 % x 4.66 ms/f x 77 % = **-2.3 ms/f**.
  - Optimistic (hysteresis also catches h4-7): (255 + 109) x 16.65 us x 0.77 = 4.7 ms/f gross, so net
    -2.7 to -3.7 ms/f.
- **Verdict: no-go for hot pages v2 on the Deck by the design's own rule.**
  - The fair-baseline projection (-1.2 to -2.2 ms/f, optimistic about -3.7) is below the -3 ms/f
    stop-after-stage-0 line, and well below the -4 ms/f acceptance.
  - The render-fault saving (-2.3) also misses -2.5.
  - Move to lever 2 (CP draw/packet fast paths; l82's CP bulk-register A/B already goes there).
- The census's own overhead is within noise: census CP 50.7 against default 50.4 ms/f; arm us/call is equal.

## 5. Car jitter (KI-054-like)

What the data shows:
- **The presented cadence now alternates between 3 and 4 vblanks.** l80 default drove at a near-constant
  4-vblank cadence (p50 66.7 ms every minute, 14.3 fps). l81 has driving minutes with p50 50 ms, and in
  every arm at least one minute with p50 66 ms. In the audit windows (300 consecutive frames):
  - frame-to-frame cadence changes in 22-40 % of frames (default #2 107/298, zero-native #2 111/298, census
    #2 112/298; early #1 windows 66-120/298);
  - dt sd 9-11 ms; |dt(n) - dt(n-1)| averages 6.8-7.5 ms;
  - mix 3 / 4 / 5+ vblanks = 65 / 213 / 21 (default #2), 79 / 181 / 39, 87 / 182 / 30.
- Under the raw measured-time timer (`MCLA timer: raw measured time`, scale 1), each frame shows 50 or
  67 ms of motion. If NPC traffic or the camera use a dt or interpolation that is one frame out of phase,
  every cadence flip is a ±16.7 ms position error. At 50-100 km/h that is about 0.23-0.46 m of
  back-and-forth. **At 14-15 fps the l80 cadence hardly flipped. At 16-19 fps it flips constantly.** This
  fits "it appeared because fps is higher".
- Snapshot dt values in the F10 state blocks disagree with each other: `guest_ki026.timer_step` 0.045-0.081
  s across the nine packages (0.081 in the default pursuit F10, where frames were about 67 ms), and the
  pursuit block's `dt` 0.026-0.067 s. This is consistent with a game step that does not track the
  presented interval frame by frame. These are single snapshots, though, not a series.
- **Hitch frames over 100 ms** also rose in the late minutes of default (58/min) and census (67 and
  43/min), against 0-6/min in zero-native (whose session ended a minute earlier). Those minutes show
  F8000534 at its peak (88-90 %), the render thread lower (77-82 %) and the CP at 76-80 % of wall. KI-054
  was first reported "during frame-time drops", so these hitches are a second plausible trigger.
  - One run per arm and different routes (pursuit in default), so the data cannot attribute this to the
    above_normal audio boost.
  - Possible, not shown: the boosted, 90 %-busy audio thread taking SMT or CPU time from the render thread.

What the data cannot say:
- There is no per-frame guest dt or raw-timer step series. The audit sample has host_us, cpu_us, gpu_us
  and underruns only.
- There are no NPC positions and no record of whether the game interpolates or extrapolates traffic.
- It cannot tell game-side interpolation at variable dt from the raw timer's own step and debt logic (KI-054
  lead 1).
- Next evidence: log the raw timer's per-frame step and debt in REX_PERF_AUDIT_SAMPLE or a 300-frame
  burst. A/B on the Deck: raw timer off against on, or a fixed 4-vblank (15 fps) cap if one exists. If
  a constant cadence removes the jitter, it is cadence or dt.

## 6. Warnings, errors, other

- The same benign set in all three logs:
  - 7x `t:\mc4\art\city` VFS no-device (test_dt_railyard / test_sc_exposition_park .loc probes);
  - the VFS self-test rejections;
  - `XeKeysConsolePrivateKeySign` unavailable;
  - one RtlLeaveCriticalSection cross-thread compatibility warning;
  - one "Texture fetch constant invalid type";
  - SDL gamecontrollerdb missing.
  No new warning class. `memory_watch_protect_failures=0` in all nine state.json files; READBACK
  `guard_misses=0`; MCLA_LOG dropped=0.
- Pipeline waits: 1 at startup (19 ms). Texture creates: avg 11-20 us, max 5.6 ms at load.
- Crash: every `diags\<id>\crash` folder is empty, and no *.dmp exists anywhere under D:\81 (live minidump
  policy skipped-by-default). Live packages: 3 per session, each with state.json (15.0-15.3 KB),
  manifest.json (1695 B), report.txt (722-723 B, owner questions blank), save-metadata.json (255 B),
  save-files-private.json (765 B, not opened), log-tail.txt (~262 KB, not opened) and frame.bmp
  (3,686,454 B, not opened).
- KI references:
  - KI-060 (audio): backoff effective on CPU, not on underruns; producer F8000534 saturated; top_site =
    bank-lock spin.
  - KI-054 (traffic jitter): new trigger condition, cadence flips at 16-19 fps.
  - KI-013: blend fix on by default (l81); no Deck observation reported.
  - KI-026 / KI-035 state blocks: warper state 29 in all packages, finish-fix enabled, 0 fires.
- The default F10 #3 was taken during a police pursuit (pursuit score 999, phase pursuit). Its audit
  window has the worst underrun rate (76/s).

## Open questions

1. Who holds the 0x828317A1 bank flag that F800068C spins on, for up to about 0.3 s: F8000534 (title code
   8214xxxx), streaming, or another thread? A writer trace on sub_8213AB48 / sub_8213A0E8 would show it.
2. What are 82146BE0 / 82147738, F8000534's hot title functions? A per-voice mixer or DSP? Is the
   recompiled code inefficient there (VMX128 paths, denormals, FPSCR or mftb in loops)? Making that thread
   faster is the remaining audio lever. Other levers: a dedicated physical core (affinity off the
   render/CP SMT siblings), and a larger SDL or guest frame queue only to mask jitter.
3. Does Wine/Proton on the Deck apply above_normal at all (nice without CAP_SYS_NICE)? state.json shows
   Wine's base_priority 1, not the Linux nice value.
4. KI-054: a raw-timer step series, plus an owner A/B with a constant cadence.
5. Hot pages are no-go. Confirm lever 2 (l82 CP bulk registers) on the Deck with the same three-arm loop.

Nothing here is a physical PASS.
