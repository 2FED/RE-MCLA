# l96 Steam Deck analysis (2026-11-13): audio fixed, census, lever 1, strict control

Read-only analysis by an Opus subagent (logs/json only, python -I), accepted by the primary agent. One run of about two minutes of driving per arm: fps differences under 0.7 are noise. Spark not used: deterministic log parsing.

**Audio (KI-060).** Owner Deck runs on l96 (2026-11-13, D:/96, build 277DB5EB): afprobe crackles as always; afnative: MUSIC PERFECT, other sounds crackle about 1 % of the time. Instruments: engine 128.9 frames/s on afprobe (69 % of real time) vs 186.7 on afnative (99.6 %; 187.5 needed); sub_82146BE0 62.1 -> 5.2 us per call, sub_82147738 61.6 -> 6.4 us; underruns 75.7/s -> 0.3-2.5/s; native served 1.91 M + 447 k calls, verified 28 k, fallback 0, mismatch 0, raced 0. The two crackle presses fall in seconds where the producer dipped to 176-182 frames/s at peak voice count (about 7 % headroom at peak). PC by-ear check on l96 pc/afnative: sound unchanged, mismatch 0, 187.9 frames/s. Verdict: audio_filter_native default on; remaining gap = the three leaf functions (kick, mix, add, about 3 ms of the 4.95 ms engine frame).

**Command processor.** Deck census (l96 default, per frame, 5890 draws): CP busy 55.9 ms; packets_other 10.22 ms fully attributed: type-0 register writes 3.22, draw-packet clock calls 2.45 (QPC costs 106-117 ns under Proton vs 18-27 on the PC), unattributed 2.00, fence polls 0.95, IM_LOAD 0.83, constants 0.34. Lever 1 (cp-lever1): packets_other 10.22 -> 7.52 ms (clock calls -2.15, fence polls -0.88); owner: a bit smoother, no artefacts; BUT the IM_LOAD memo had 131 mismatches in 3.4 M hits (would have used stale shaders without verify): memo stays off. strict-off on the Deck: no measured gain (15.0 vs 14.0-14.9 fps, within noise), fence polls double; owner felt it worse: strict stays on. The Deck frame interval is quantised to multiples of 16.7 ms (p50 66.6 ms = the 15 fps bucket), so CP savings show as fps only once frames drop under 50 ms. 66 % of constant dwords rewrite the same value; 53 % of draws change constants only. FMA bench on the Deck: CRT 24.9 ns vs hardware 1.59 ns (15.7x), 0 mismatches. New lead for KI-077 on the Deck: the adapted-luminance read-back is 0 in every line (bytes only 0x00/0xFF), unlike the PC.

---

# D:\96 l96 Steam Deck owner runs: read-only analysis notes (2026-11-13)

Inputs: sessions\<id>\{mcla.log, launch-receipt.txt, state.txt, managed.toml}, diags\<id> folder listing (crash empty).
Not opened: before/after-user, save-private, *.dmp, vkd3d cache. Parsers (python -I, this folder): parse.py -> all.json;
win.py (per-window); agg.py -> agg.json (driving windows = PERF_SPLIT windows after the first, draws/frame > 5000);
census.py -> census_out.txt; schedchk.py; af.py / afwin.py (MCLA_AFILT); aud.py (REX_PERF_AUDIT underruns, thread CPU).
Spark not used: deterministic parse. Local clock UTC+2.

## Arms (all build 277DB5EB... = l96 candidate-receipt build_manifest_sha256; state complete, exit 0; crash folders empty)
| arm | session | local | F10 (audits) | driving s / frames |
|---|---|---|---|---|
| default | e00d8b5c | 14:28:44-14:32:05 | 14:29:50, 14:31:58 | 128.5 / 1801 |
| cp-lever1 (fence_poll 64, draw_packet_tsc, im_memo + verify) | 997fb060 | 14:35:08-14:38:08 | 14:36:18, 14:38:04 | 107.2 / 1579 |
| strict-off | d931fa77 | 14:40:15-14:43:48 | 14:41:25, 14:43:43 | 138.6 / 2084 |
| afprobe | fd7ded0a | 14:45:37-14:49:06 | 14:46:43, 14:46:54, 14:49:00 | 136.6 / 2033 |
| afnative (probe+native+verify) | 3d39f42f | 14:50:04-14:53:19 | 14:51:11, 14:52:17, 14:52:30, 14:53:15 | 123.7 / 1834 |
| fma-bench | ad3c07e2 | 14:54:21-14:55:30 | none | menu only |

MCLA_CLOCK (all six): qpc 106-109 ns, steady 111-117 ns, rdtsc 10.7-10.8 ns, tsc 2800 MHz invariant, wine=1
(PC: qpc 18-25, steady 19-27, rdtsc 8-11). MCLA_FMA_BENCH: crt_ns 24.91, hw_ns 1.59 (15.7x), cpu_fma3=1, os_ymm=1,
mismatch 0, nan_mismatch 0, hw=ran. MCLA_LOWLAT: only `host kind=entry sdk_entry=found`; no SDK kind=init/minute
line in any arm -> low-latency layer never started (off). cp_mcycles = 0 under Wine (QueryThreadCycleTime unusable).

## Census (default, per frame, 14.02 fps, 5890 draws)
cp_busy 55.9 (cp_cpu 44.2, kernel 8.0), ring_idle 8.1, wrm_mem 7.3, fence_wait 5.3 (= strict wait 4.84),
phases 21.8, regions 19.0, upload 12.1 (arm 7.0), write_fault 5.7, stream_finish 2.6, packets_other 10.22.
packets_other reconciles exactly: t0 3.218 + imload 0.828 + const 0.336 + event 0.208 + swap 0.002 + other 0.189
+ draw_outside 2.454 + wrm excess 0.039 + fence_poll_pkt 0.945 + unattributed 2.004 = 10.223.
Fence polls 4618/f (PC 40236) at 228 ns each (PC 43); IM_LOAD 2064 + IMM 1273, 3337 hashes 0.663 ms (199 ns, PC 85);
ALU dw 334.9k written, 66 % unchanged; cb_uploads 11.9k / 12.0 MB; draws value_same 54 %, const_only 53 %,
pso_same 66 %, rt_same 99.5 %, tex_same 57 %, state_same 0.8 %.
sched= worked (no sched_unavailable) but run_ms is ~0.47x cp_cpu_ms (GetThreadTimes) in every driving window
and 0.65x in menus, ~20k slices/s regardless of load: thread identity / units unverified -> do not use as CP
preemption yet (wait/run ~0.3 on whatever thread it reads).
Frame intervals are 16.7 ms multiples (p50 66.6 ms = 4 vblanks, p90 83.7): Deck at 14-15 fps is pinned to the
66.7 ms bucket; CP savings show as fps only when frames fall below 50 ms (or with a 40/50 Hz panel / vsync off).

## Lever 1 (one run; draws 6200/f vs 5890)
Engaged: MCLA_CP_LEVER fence_poll_interval=64 draw_packet_tsc=1 im_memo=1 verify=1; fence_polls_skipped 7795/f,
polls 360/f; draw_packet_tsc_packets = all draw packets; memo hit 3,356,474, miss 26,892, inval 1, MISMATCH 131.
packets_other 10.22 -> 7.52 (per draw 1.735 -> 1.213 us; other strict-on arms 1.77-1.78): draw_outside -2.15,
fence_poll_pkt -0.88, imload +0.16 (verify hashes all), unattributed -0.05. cp_busy per draw 9.49 -> 8.67 us.
fps 14.02 -> 14.73 (other arms 14.0-15.0: noise). Strict wait 4.84 -> 2.80 ms/f; fence_late_f1 0.10 -> 0.67/frame,
f2p 0. Pacing >100 ms: 51/min (default 43-61).

## Strict off (one run)
fps 15.04 (strict-on arms 14.0-14.9), cp_busy 54.3, fence_wait 0.98, ring_idle 5.1, fence polls 9860/f
(2.1x; packets_other 11.73), GPU 40.0 %, GPU Commands thread 70 % of a core (62 % default), render 84 % (80 %).
lead_open_frames_max 1 (0 in strict arms), lead2 0, f2p 0, cp_lead_frames avg 1.0 max 1 in every arm.
Pacing p99 132/151 ms, >100 ms 30/40 per min (default 166/183, 43/61). Instruments do not show "worse".

## Audio
afprobe driving 136 s: 128.9 fps (0.688), frame 7.44 ms, busy 0.96; bq 62.1 us/active call 2.83 ms/f;
ch 61.6 us 1.13 ms/f; kick 75 us x10.2 = 0.77 ms/f; underruns audit1 75.7/s.
afnative driving 123 s: 186.7 fps (0.996), frame 4.52 ms, busy 0.845 (max 0.963); bq 5.23 us (11.9x) 0.28 ms/f;
ch 6.42 us (9.6x) 0.11 ms/f; kick 70 us x12.9 = 0.91 ms/f; mix ~0.49 us x1914, add ~2.3 us x387 (timed only
before the 50k/s guard; estimates ~0.9 ms/f each). Verify: bq nat 1,909,111 verified 21,633 mismatch 0 raced 0
fallback 0; ch nat 447,170 verified 6,561 mismatch 0 raced 0; no MCLA_AFILT_VERIFY / _CH_VERIFY lines.
Underruns: audit1 0.3/s, audit2 (14:52:17, 20 s, covers both crackle presses) 2.5/s, audit3 0.
Crackle window 14:52:16-38: 185.2 fps, 8 of 23 s below 185 (min 176), busy to 0.963, bq 79/f, kick 1.05 ms/f;
underruns cluster at 14:52:17-19, :26-32, :36-37 = the dips. Headroom ~7 % at peak voices; leaves
(kick+mix+add ~3 ms of 4.95) are the limit now. F8000534 89 -> 70 % of a core, F800068C 10 -> 23 %.

## TOD
MCLA_TOD present in all arms with brightness fields; adapt_lum=0 / bits 0 in EVERY Deck line while lum4_raw holds
only 0x00/0xFF bytes (PC: adapt_lum 0.004-0.03, lum4_raw = floats 0x3Bxx/0x3Cxx). Deck adaptation luminance
looks byte-saturated or wrong-format -> new KI-077 Deck-side lead (auto-exposure input broken on RADV path?).

## Warnings
Same classes in all arms (t:\mc4\art\city device, test_*.loc, XeKeys, invalid fetch constant once). No new class.
