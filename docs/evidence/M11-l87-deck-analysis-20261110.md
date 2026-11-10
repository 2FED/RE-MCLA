# l87 Steam Deck analysis (2026-11-10): KI-060 audio filter probe and native filter

Read-only analysis by an Opus subagent (logs/json only, python -I), accepted by the primary agent. Spark not used: deterministic log parsing.

**Headline.** Owner Deck run on l87 (2026-11-10, D:/87): audio still crackles in both arms; on afnative the music plays in slow motion while driving and normally when standing (owner: it may always have been slow, hidden by the tearing). Probe: the engine renders 111.5 frames/s (afprobe) and 127.9 (afnative) of the 187.5 needed, so music advances at 59.5 % / 68 % of real time; underruns 75-86/s vs 53-60/s (-30 %). Per engine frame while driving: sub_82146BE0 3.45 ms (74 calls, 61 us each) -> 2.30 ms native (42 us, only 1.45x), sub_82147738 1.10 ms, remainder 4.27 ms; real time needs 5.33 ms. The native path served 1,338,903 calls with 0 fallbacks; 1 verify mismatch out of 15,138 is a self-check race (coefficient refresh by another thread), not wrong arithmetic. Why only 1.45x: mcla.exe is built -O3 -msse4.1 without -mfma, so every std::fma is a CRT library call (96 sites in sub_82146BE0, 72 in sub_82147738), which under Proton lands in the Wine CRT fma (believed software; unmeasured). Next: native kernel without fma (bit-identical for float operands in double), native sub_82147738, then the remainder; check the fma cost for the whole recompiled game.

---

# l87 Steam Deck results (D:\87), KI-060 audio filter probe / native, read-only analysis, 2026-11-10

Inputs read: sessions\<id>\{mcla.log, launch-receipt.txt, state.txt, capabilities.txt}, diags\<id>\{report.txt,
latest-live.txt}, live\*\state.json. Not opened: before/after-user, snapshots, save-private, frame.bmp, log-tail,
runtime\*.log; no *.dmp. Parsers (python -I) here: afilt.py (per-second MCLA_AFILT, afilt_out.txt), afagg.py (state
buckets + timeline, afagg_out.txt, tl_native.txt), f10.py (state.json audio_filter + thread CPU + REX_PERF_AUDIT,
f10_out.txt), imports.py (PE import check of the staged l87 mcla.exe), fmacount.py (std::fma sites per generated
function). Spark not used: two logs, deterministic parse. Local clock = UTC+2.

## Sessions
| arm | session | local | length | F10 | exit |
|---|---|---|---|---|---|
| afprobe (audio_filter_probe) | 80aaf6de | 18:49:35-18:52:37 | 182 s | 18:50:43, 51:14, 51:35, 52:33 | complete, exit 0, Execution complete, crash empty, log dropped 0 |
| afnative (probe + native + verify) | 6087d8a8 | 18:53:55-18:57:13 | 198 s | 18:55:09, 55:42, 56:09, 57:08 | same |
Build AE5FFF5E...EA51669 = build_manifest_sha256 of private/l87-internal/candidate-receipt.json (both). Staged
mcla.exe sha256 58EE7CBE... = receipt.

## Probe (seconds bucketed: driving = busy >= 0.95 and fps < 175; standing/unsat = filters active, not saturated)
| | afprobe driving (121 s) | afnative driving (120 s) | standing (both) |
|---|---|---|---|
| engine fps (need 187.5) | 111.5 (0.595) | 127.9 (0.682) | 186-188 |
| frame ms avg / max | 8.82 / 23.2 | 7.67 / 33.8 | 4.4-4.7 (late standing), 1.2-3 (early) |
| 82146BE0 calls/frame, us/active call | 74.5, 61.2 | 73.7, 42.1 | ~40 calls/frame |
| 82146BE0 ms/frame (share) | 3.45 (39 %) | 2.30 (30 %) | 1.2-1.3 |
| 82147738 calls/frame, us/call, ms/frame (share) | 16.9, 64.3, 1.09 (12 %) | 16.9, 65.1, 1.10 (14 %) | 11 calls, 0.85 |
| remainder ms/frame (share) | 4.28 (49 %) | 4.27 (56 %) | 2.4-2.6 |
| mix a/b/ab/none % | 63.3/4.5/7.8/24.3 | 61.7/3.9/8.5/26.0 | |
| zero-input / denormal-input | 1.2 % / 0 | 1.0 % / 0 | |
| MXCSR DE=UE (bq) | 0.17 % of calls | 0.15 % | |
Filter-call max per frame 99/100; other_frames 0; outside 0 (no over-rendering, no foreign calls).

## Native
nat 1,338,903 of 1,360,156 calls (rest = verify originals), fallback 0, refresh 17,358, verified 15,138,
mismatch 1, verify_skipped 6,115. Only line: `MCLA_AFILT_VERIFY v=1 n=1 call=1 a=1 b=0 diffs=266 field=a.c0
want=0x3E5E1538 got=0x3E6477F9` (18:55:20). 266 = 5 coefficients + 4 history + cache + 256 samples: the original
refreshed bank A coefficients that were not due when the host copy was taken, i.e. +80 changed between the copy and
the original's read (concurrent write by another guest thread); a self-check race, not a kernel arithmetic error.
Speedup 61.2 -> 42.1 us per active call (1.45x; same mix). Saved 1.15 ms/frame; channel and remainder unchanged per
frame; time went into more frames (fps +15 %, 111.5 -> 127.9).

## Why native is only 1.45x: std::fma is a CRT call on the Deck
- l87 mcla.exe imports `fma` from api-ms-win-crt-math-l1-1-0.dll; gate build flags are `-O3 -msse4.1` (no -mfma)
  for both src/ and generated/ TUs, so every std::fma is a call. Bundle ships no ucrtbase -> Proton/Wine builtin
  fma (software, by Wine source knowledge; not measured here).
- std::fma sites: 82146BE0 96, 82147738 72, 823F4450/823EFD30/823EE050/823EC248 0.
- Native: ~1713 fma per active call (256 x 6 x 1.115 banks) -> 42.1 us = ~25 ns per step; original ~36 ns.
- Exact fix for the native kernel: every fma operand is a float-exact double, so x*c is exact in double and
  fma(x,c,h) == x*c + h in double (one rounding either way). Drop std::fma -> bit-identical, no call.
- 82147738 needs operand checks; otherwise hardware FMA via target("fma") with CPUID dispatch (correctly rounded,
  identical).
- M7-016-audio-mixer-cost.md's "96 hardware FMA" does not hold for this build.

## Audio
| | audit1 | audit2 | audit3 | audit4 (standing at end) |
|---|---|---|---|---|
| afprobe | 74.7/s | 85.7/s | 75.3/s | 3.8/s (52 frames) |
| afnative | 52.7/s | 59.6/s | 52.5/s | 0.0/s (54 frames) |
Underruns ~= 187.5 - producer fps (afprobe F10 windows 110.9/102.6/108.7 fps -> predicted 77/85/79).
F8000534 CPU 87-90 % (k 2 %) afprobe, 85-86 % (k 3 %) afnative; F800068C 10-12 % (k 3 %) both. Earlier: l81
47-62/s, l82 63-71/s, l84 69-83/s.

## Slow-motion music
SDL inserts 256 silent samples when no frame is queued and drops nothing. Driving, the producer delivers 59.5 %
(afprobe) or 68 % (afnative) of real time, so music advances at that rate with no pitch change. Standing: ~40
filter voices, 4.5 ms per frame, fps 187.5, 0-4 underruns/s, so it sounds normal. afnative has fewer gaps
(~55/s against ~80/s, one gap per ~2.3 frames against ~1.4), so it sounds like continuous slow playback more than
chopping. Inference: owner audio is not recorded. The sound itself was not changed: 0 fallbacks, 0 denormals, and
the only mismatch is the race above.

## Next levers (driving, afnative baseline 7.67 ms/frame, real time needs 5.33)
1. Remove the fma calls from 82146BE0 native (exact mul+add): 2.30 -> ~0.3 ms, about -2.0 ms -> ~5.7 ms
   (~175 fps, ~13 underruns/s).
2. Same for 82147738 (native or FMA dispatch): 1.10 -> ~0.15 ms, -0.95 ms. 1+2 -> ~4.7 ms, real time with ~12 %
   headroom at 79 voices. Cheap confirmation: afprobe on the Windows PC (UCRT fma uses FMA3) should show far
   lower us/call.
3. Remainder 4.27 ms (56 %), about 46 us per voice (2.5 ms at 40 voices, 4.3 ms at 79): the next native target
   once 1+2 are in.
4. Dedicated core / priority: wall busy 0.98 against CPU 0.86 -> about 12 % of wall is lost, up to ~0.9 ms
   equivalent.
5. Skip silent voices: 1 % zero-input -> < 0.05 ms. Neither-bank calls already cost ~0.6 us. Not worth it.
6. Voice cap: ~0.1 ms per voice; it changes the mix, so last resort.
