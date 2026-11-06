# M11-015: read-back sync kick-interval A/B on l72 (PC, 2026-11-06)

Read-only analysis by an Opus subagent on 2026-11-06 of the owner l72 kick A/B (Arcade "South Central Tour", MAIN 1080p, sessions e7e5fa2b/6b77644c/28f65deb, HWiNFO + proc-sampler); accepted by the primary agent. Decision: keep `d3d12_readback_sync_kick_draws=512`; M11-015 closed as no gain.

Read-only analysis (Opus subagent) of the owner l72 kick A/B (PC, MAIN profile, 1080p render target, 3840x2160
fullscreen, Unlocked, build 0.20.37.0 `8D47B930...`), Arcade race "South Central Tour", one run per arm, F10 once per
run. Sources: session `mcla*.log`, F10 package `report.txt`/`state.json`/`manifest.json` (answers empty), owner HWiNFO
CSV `HWiNFO_LOG_104818562.CSV` (387 samples, 18:35:21-18:48:13, 2 s; the other CSV `..104024593` ends 18:28:04 and
covers only the pre-race album/audio checks), proc-sampler `proc-sampler-20261106-183526.csv` (381 samples,
18:35:29-18:48:09). Parsed with an isolated Python outside the source folders. Field formulas as in the l69/l71 docs
(per-frame = field / frames of the same window).

## 1. Validity

| arm | session | override line | `targeted_start kick_draws` | race window (menu state running -> finish pause) | F10 (s after start) |
|---|---|---|---|---|---|
| 512 | e7e5fa2b... | none (all three `readback_kick_*=false leaves ... unchanged`) | 512 | 18:35:36.3-18:38:54.8 (198.4 s) | 18:36:09.4 (+33.1) |
| 256 | 6b77644c... | `readback_kick_256=true (d3d12_readback_sync_kick_draws=256)` | 256 | 18:40:29.1-18:43:37.9 (188.8 s) | 18:41:03.3 (+34.1) |
| 128 | 28f65deb... | `readback_kick_128=true (d3d12_readback_sync_kick_draws=128)` | 128 | 18:45:01.6-18:47:59.1 (177.4 s) | 18:45:37.0 (+35.3) |

- Arms confirmed. F10 at the same checkpoint (+33 to +35 s). Race length differs (198 / 189 / 177 s; driving
  variance); the 512 run is ~21 s longer than the 128 run.
- 512 caveat: same game process as the earlier album/audio checks (started 18:22:04, `mcla.1.log` + `mcla.log`); the
  window was unfocused 18:33:36-18:35:33 (several focus flips) just before the race start; its minute windows end at
  :06, so the analysed race span is +30..+150 s (the last ~48 s before the finish have no closing window; the process
  exited at 18:39:06). 256: +7..+187 s; 128: +16..+136 s (no closing window for the last ~41 s).
- Like-for-like set **B** = from the F10 checkpoint onward: PERF_SPLIT post-F10 windows (512 seq16+17, +33..+150 s,
  117 s; 256 seq3+4, +34..+127 s, 93 s; 128 seq3+4, +35..+136 s, 100 s); READBACK_SYNC = minute lines minus the F10
  mark. Pacing uses the per-minute lines that contain set B (2 minutes per arm).
- Foreign load: no build tools (no ninja/clang/cmake/msbuild/link in any top-8 sample). **But a `find` process
  (pid 21888) pinned one logical core (6.1 % of 16 threads = 1 core) in every proc-sampler sample from 18:21:19 to
  18:48:09**, i.e. during all three arms equally. other_cpu_sum 13.8 / 13.9 / 13.7 % avg (max 20 / 24 / 23), game
  20.7 / 22.2 / 22.0 %; MCLA_CPU sys-proc gap ~15 % in every arm. Equal across arms, so the A/B stays comparable, but
  it is a foreign load (and draws from the shared 35 W STAPM budget); it should be identified and stopped.
- Power/thermal (HWiNFO race windows 512 / 256 / 128): package 35.0 / 35.3 / 35.3 W, STAPM limit 99.9 / 98.4 /
  98.8 % (all at the 35 W cap), Tctl 90.3 / 90.1 / 90.4 C, Thermal Limit 89.5 / 89.3 / 89.6 %, HTC/PROCHOT/PROCHOT
  EXT = No in all samples; core clock avg 4051 / 4009 / 4033 MHz; MCLA_CPU eff 4.17 / 4.10-4.16 / 4.15 GHz. NVIDIA
  64.3 / 63.6 / 64.6 W, load 82 / 80 / 82 %, power-limiter flag in 37 / 28 / 31 % of samples. On AC (battery 100 %,
  17.4 V, charge rate 0 W). Total CPU 36.4 / 38.5 / 37.9 %. PresentMon fps 37.4 / 37.1 / 37.6. Comparable.
- Warnings: 256 and 128 show only the usual boot-time set (t:\mc4\art\city VFS misses, read-only default.xex, one
  texture-fetch-constant warning, XeKeys, SDL DB; "guest frame readback failed; using window capture" at F10 in 256).
  512's restarted log has none. No new warnings; `forced_wait_failed=0`, `fallbacks_to_full=0`, `full_waits=0`,
  `gpu_blocks_dropped=0` in all arms. KI-047 livery correctness is not visible in logs.

## 2. Results (set B, from the F10 checkpoint; per frame unless noted)

| metric | 512 | 256 | 128 | d256 | d128 |
|---|---|---|---|---|---|
| frames / wall | 4215 / 117.0 s | 3338 / 92.9 s | 3593 / 100.4 s | | |
| fps (PERF_SPLIT) | 36.02 | 35.94 | 35.77 | -0.08 | -0.25 |
| fps (pacing minutes) | 35.9 | 36.2 | 35.8 | | |
| p50 / p90 / p99 interval ms (frame-weighted) | 26.1 / 33.7 / 61.4 | 26.6 / 34.1 / 52.9 | 26.1 / 35.7 / 52.5 | | |
| max interval ms | 117.0 | 96.9 | 121.3 | | |
| frames >50 / >100 ms (2 min) | 73 / 8 | 51 / 0 | 59 / 3 | -22 / -8 | -14 / -5 |
| draws/frame | 6492 | 6667 | 6639 | +2.7 % | +2.3 % |
| CP busy ms | 26.72 | 26.77 | 26.88 | +0.05 | +0.16 |
| CP CPU ms (user / kernel) | 18.10 (16.38 / 1.73) | 19.01 (16.60 / 2.42) | 18.94 (15.40 / 3.54) | +0.91 | +0.84 |
| CP CPU us/draw | 2.789 | 2.852 | 2.853 | +2.3 % | +2.3 % |
| CP busy minus waits (rb + stream finish) | 19.40 | 19.63 | 19.72 | +0.23 | +0.32 |
| readback_wait ms | 6.53 | 6.10 | 5.93 | -0.43 | -0.60 |
| fence_wait ms | 6.31 | 5.88 | 5.73 | -0.43 | -0.58 |
| stream_finish_wait ms | 0.80 | 1.04 | 1.22 | +0.24 | +0.42 |
| rb + stream finish wait | 7.33 | 7.14 | 7.15 | -0.19 | -0.18 |
| RS forced waits / frame | 1.038 | 1.046 | 1.036 | | |
| RS wait ms / frame (avg) | 6.46 | 6.05 | 5.88 | -0.41 | -0.58 |
| RS wait per forced wait ms | 6.22 | 5.78 | 5.68 | -0.44 | -0.54 |
| RS max wait ms (per-minute max) | 12.9 | 12.6 | 11.8 | | |
| RS targeted fence ms / submit ms | 6.26 / 0.196 | 5.85 / 0.199 | 5.69 / 0.190 | | |
| GPU still busy at the forced wait | 97 % | 96 % | 96 % | | |
| backlog draws at the forced wait | 43.6 | 40.1 | 38.4 | | |
| kicks / frame | 4.90 | 9.98 | 15.77 | x2.0 | x3.2 |
| kick ms / frame (ms per kick) | 0.686 (0.140) | 1.176 (0.118) | 1.533 (0.097) | +0.49 | +0.85 |
| submissions / frame | 11.26 | 16.42 | 22.25 | +5.2 | +11.0 |
| submit + execute ms | 0.916 | 1.155 | 1.266 | +0.24 | +0.35 |
| GPU busy ms (PERF_SPLIT) | 22.37 | 21.99 | 22.15 | -0.38 | -0.22 |
| render thread CPU / wait ms | 27.47 / 11.80 | 27.49 / 10.93 | 27.50 / 11.36 | | |
| proc-sampler game CPU % (whole race) | 20.7 | 22.2 | 22.0 | +1.5 | +1.3 |

All race windows (set A: 512 +30..+150 s, 256 +7..+187 s, 128 +16..+136 s) give the same picture: RS wait 6.46 /
6.00 / 5.89 ms/f, kicks 4.9 / 9.9 / 15.7 per frame, CP busy 26.72 / 27.30 / 26.85, fps 36.0 / 35.3 / 35.8; >50/>100
per minute 512 [37/2, 36/6], 256 [32/0, 19/0, 45/2], 128 [26/2, 33/1].

F10 package windows (set C; PERF_SPLIT window ending at F10): 512 = 3.0 s only (+30..+33 s, 95 frames: 31.3 fps, CP
busy 30.84, rb_wait 6.28, 13.4 subs/f; too short to use); 256 = +7..+34 s, 27.2 s: 37.1 fps, 6785 draws/f, CP busy
25.86, CP CPU 17.95, rb_wait 6.28, sfw 0.89, 13.6 subs/f, GPU 22.36; 128 = +16..+35 s, 19.6 s: 36.0 fps, 6968
draws/f, CP busy 26.72, CP CPU 18.94 (kernel 4.03), rb_wait 5.93, sfw 1.16, 21.9 subs/f, GPU 22.20.

## 3. Interpretation

- The CP wait fell much less than planned: read-back wait -0.4 ms/f (256) and -0.6 ms/f (128), not -1 to -3. Part
  of it moved into `stream_finish_wait` (+0.24 / +0.42), so total CP waiting fell only ~0.2 ms/f.
- Why: at the forced composite wait the GPU is still busy in 96-97 % of cases and the un-kicked backlog is already
  small (38-44 draws per wait in all arms). The ~5.7-6.2 ms per forced wait is the GPU draining work that was already
  submitted (GPU latency; GPU 22 ms/f, 80-82 % load), not submission latency, so earlier kicks cannot shorten it.
- Cost: kicks x2 / x3.2, submissions +5 / +11 per frame, kick time +0.49 / +0.85 ms/f, CP kernel time +0.7 / +1.8
  ms/f, CP CPU +0.9 ms/f (+2.3 % per draw), game CPU +1.3-1.5 % of the machine. Net CP busy +0.05 / +0.16 ms/f (no
  gain). Per-kick cost fell (0.140 -> 0.097 ms) but the count grew faster.
- fps unchanged within noise (36.0 / 35.9 / 35.8). The >100 ms count was lower in 256 (0) and 128 (3) than 512 (8),
  p99 61 -> 53 ms, but the 512 run is the confounded one (13-minute-old process after album/audio checks, focus flips
  just before the start, partly different route segment) and one run per arm cannot separate this from noise; the
  wait structure that kicks act on is unchanged, so a kick-driven hitch effect is not plausible.
- No side effects in logs (no wait failures, no full-wait fallbacks, no dropped read-back blocks, no new warnings).

## 4. Recommendation

- **Keep 512 as the PC default.** 256/128 buy -0.4/-0.6 ms of read-back wait but pay it back in stream finish wait,
  kick and kernel time; CP busy and fps do not move. Do not try 64 (cost rises, wait is flat; 64 would roughly
  double kicks again for at most ~0.2 ms more wait). A time-based kick does not address the cause either: the GPU is
  busy with already-submitted work at the wait.
- The remaining ~6 ms/f is a GPU-latency wait, so the lever is not to block on it: make the KI-047 composite read
  asynchronous / one frame late, or limit the forced wait to frames where the composite actually changed, or cut GPU
  time ahead of the composite in the frame. Plan items that cut CP CPU per draw remain the other path.
- If the tail difference (>100 ms 8 -> 0/3) matters, repeat 512 vs 256 in fresh processes, two runs each, with the
  `find` process stopped, before any change. Low priority.
- Deck: keep 512 (or larger). Kicks cost ~0.23 ms each under vkd3d (plan), so 256 would add roughly +1.1 ms/f and
  128 +2.5 ms/f of CP time for the same small wait gain; no Deck run is needed for this decision.
