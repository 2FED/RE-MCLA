# KI-067 / KI-072: HWiNFO sensor log vs l71 police session (2026-11-06)

Read-only analysis by an Opus subagent on 2026-11-06 of the owner HWiNFO64 CSV (private/owner-hwinfo-20261106) against the l71 police session 473b720b...; accepted by the primary agent.

Source: owner HWiNFO64 CSV `private/owner-hwinfo-20261106/HWiNFO_LOG_95170687.CSV` (kept in place, read-only,
parsed with an isolated Python outside that folder). Game logs: session `473b720b...` `mcla.1.log` + `mcla.log`
(complete from launch 15:54:22 to window close 16:03:44 local). Only logs were read.

## 1. CSV identity, time range, columns

- 290 samples, 385 columns (Ukrainian headers), **15:54:31.7 - 16:04:09.7 local**, interval **2.0 s**
  (1.90-2.09 s). Machine: HP Victus 15-fb3xxx (Zen5 + Zen5c APU, 4+4 cores / 16 threads; NVIDIA dGPU 60/80 W).
- It covers **only the police session 473b720b** (launch 15:54:22, close 16:03:44, plus 25 s after exit).
  The album session 234aebc6 (15:43-15:46) is **not** covered.
- No per-process columns (HWiNFO does not log processes).

| col | header (translated) | note |
|---|---|---|
| 131 | CPU (Tctl/Tdie) [C] | main package temperature |
| 132 | "CPU core" [C] | constant 100.0: a limit value, not a reading |
| 134 | CPU Skin temperature [C] | input of AMD STAPM/skin tracking |
| 135 | CPU CCD1 (Tdie) | constant 22.0: invalid sensor |
| 136 / 137-144 | Core temperatures avg / Core0-7 | |
| 20 / 21-28 | Core clocks avg / per core (Zen5 = 0,2,4,6; Zen5c = 1,3,5,7) | |
| 30 (=47) / 31-46 | Effective clock avg / per thread | avg includes idle threads (~1.3 GHz in game) |
| 65 / 66 | Max CPU/thread usage / Total CPU usage [%] | 66 is comparable to MCLA_CPU sys_pct |
| 84 | Total CPU utility [%] | normalised to base clock, can exceed 100 |
| 95 / 185 | Core C0 residency avg / Average active cores | |
| 155 | CPU package power [W] | |
| 165 / 166 | CPU core power / SoC power (SVI3) [W] | |
| 167 / 177 | APU STAPM [W] / APU STAPM Limit [%] | **the binding limit** |
| 175 / 176 | CPU PPT FAST / PPT SLOW Limit [%] | |
| 173 / 174 | CPU TDC / EDC Limit [%] | |
| 178 | Thermal Limit [%] | Tctl / Tjmax (implied Tjmax ~100-101 C) |
| 172 | Frequency limit - global [MHz] | |
| 180 / 181 / 182 | Thermal throttling HTC / PROCHOT CPU / PROCHOT EXT [Yes/No] | "Ні" = No, "Так" = Yes |
| 2 / 6 / 7 | Virtual memory committed / Physical available [MB] / Physical memory load [%] | |
| 226 / 227 | System drive read / write rate [MB/s] | |
| 237-252 | integrated GPU block (idle, ~5 %) | |
| 253 / 266 / 277 / 286 | NVIDIA GPU temp / power / clock / core load | |
| 297 / 298 | NVIDIA performance limiter: power / thermal [Yes/No] | |
| 348 / 361 / 366 / 368 | PresentMon: presented fps / frame time 99 % / GPU busy / CPU busy (foreground app) | |

## 2. Per window (game PERF_SPLIT windows; HWiNFO aggregated over the same window)

Unit costs: CP CPU us/draw = cp_cpu_ms/draws; kernel ms/f = cp_kernel_ms/frames; VirtualProtect us/call from
`MCLA watch protect/min` (arm us / arm calls; per minute only); memcpy GB/s = upload_kb*1024/upload_ms;
render own us/draw = (render_cpu_ms - render_wait_ms)/draws. fps and >50/>100 ms from `MCLA frame pacing/min`.
HWiNFO: Tctl min/avg/max, Zen5 thread effective clock avg, package power avg/max, STAPM Limit % avg, Thermal Limit
% max, total usage avg/max. Throttling flags HTC/PROCHOT/PROCHOT EXT were **No in all 290 samples**.

| end | win s | fps | >50/>100 | draws/f | CP us/draw | kern ms/f | VP us/call | memcpy GB/s | own us/draw | Tctl C | Zen5 eff MHz | pkg W | STAPM % | Th.lim % max | usage % avg/max |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 15:55:37 | 60.0 | 32.9 | 30/10 | 3517 | 3.04 | 1.73 | 0.69 | 3.93 | 2.38 | 71.8/82.0/88.1 | 1995 | 33.0/45.1 | 73.7 | 88.0 | 29.1/99.5 (boot) |
| 15:56:37 | 60.0 | 49.1 | 5/0 | 4196 | 2.92 | 1.69 | 0.75 | 3.96 | 3.03 | 87.9/90.8/93.2 | 2136 | 34.2/35.5 | 97.6 | 92.7 | 27.6/30.8 |
| 15:57:37 | 60.0 | 48.6 | 11/0 | 4499 | 2.86 | 1.89 | 0.64 | 3.83 | 2.92 | 91.5/92.8/94.0 | 2158 | 34.5/35.5 | 98.4 | 93.5 | 27.9/34.8 |
| 15:58:16 F10 | 38.6 | - | - | 5517 | 2.75 | 2.37 | - | 3.99 | 2.59 | 90.9/92.8/94.0 | 2176 | 34.4/35.2 | 98.2 | 93.2 | 27.9/38.2 |
| 15:58:21-36 F10 x3 | 5-8 | 43 | 4/0 | 2.6-5.0k | 2.66-3.10 | 1.2-2.7 | 0.65 | 3.99-4.72 | 2.6-3.4 | 86.8-92.5 | 1.6-2.1k | 30.7-33.8 | 89-97 | 86-92 | 20-27 |
| 15:59:37 | 60.0 | 46.9 | 2/0 | 3962 | 2.91 | 1.63 | 0.69 | 4.26 | 2.96 | 86.5/90.2/93.0 | 2009 | 32.9/35.7 | 94.1 | 92.5 | 26.6/38.1 |
| 16:00:37 (split) | 34.7 | 40.0 | 24/1 | 6673 | 2.88 | 3.07 | 0.64 | 4.24 | 2.39 | 91.1/92.2/93.1 | 2228 | 34.7/35.4 | 99.2 | 92.3 | 28.9/33.8 |
| 16:01:37 (split) | 12.5 | 42.2 | 26/3 | 4613 | 2.98 | 2.18 | 0.65 | 4.20 | 2.70 | 90.4/91.2/92.4 | 2170 | 34.2/35.5 | 97.8 | 91.7 | 29.7/35.2 |
| 16:02:37 | 60.0 | 43.5 | 16/1 | 4983 | 2.98 | 2.23 | 0.75 | 4.20 | 2.70 | 87.5/92.1/94.1 | 2141 | 34.1/35.3 | 97.2 | 93.3 | 29.2/36.5 |
| 16:03:12 F10 | 35.2 | - | - | 4493 | **3.31** | 2.20 | - | 3.93 | **3.11** | 86.6/91.1/94.2 | 2553 | 34.7/35.2 | 99.0 | 93.1 | **42.1/91.1** |
| 16:03:16-39 (short) | 2-9 | 43.3 | 17/0 | 3.0-5.3k | 2.75-3.16 | 0.7-3.7 | 0.79 | 3.93-4.77 | 2.6-3.4 | 86.5-93.0 | 1.6-2.2k | 30-35 | 87-99.5 | 87-93 | 21-30 |

(F10 windows 16:00:02 / 16:00:51 / 16:01:25 sit inside the split minutes and give the same picture: CP 2.93-3.06,
memcpy 4.02-4.18, Tctl 92-93 C, 35 W, STAPM 99-100 %.)

MCLA_CPU per window (all 23 lines): eff_mhz avg 4189-4425 (min 3736 only in the 16:03:12 window), perf_pct
~215, ntpi_mhz / limit 2000/2000 throughout (no OS-visible limit), temp n/a; sys_pct avg 26-29 % and proc_pct
avg 18-22 % in gameplay minutes; HWiNFO total usage over the same windows 27-30 % (matches sys_pct within 1-2 pp).

## 3. KI-067 episode? No. One 10-second foreign-load dip instead.

**No 2-3 minute KI-067 episode in this session** (9 min, 720p police / free roam). All unit costs stay at the
healthy l70 baseline across 2.6-6.7k draws/frame: CP CPU 2.66-3.31 us/draw (l70 healthy 2.7-3.0), VirtualProtect
0.64-0.79 us/call (0.64-0.76), upload memcpy 3.83-4.77 GB/s (3.4-3.8), render own 2.39-3.42 us/draw (2.5-3.0).

**Healthy baseline (for comparing the next episode):**

- Tctl 90-94 C while driving (session max 94.2 in game, 96.0 just after exit); hottest core 91-93 C; Thermal
  Limit 90-93 % (implied Tjmax ~100 C), i.e. **only ~6-8 C of headroom**, but HTC / PROCHOT never asserted.
- CPU package 33-35 W with **APU STAPM Limit 95-100 %: the CPU is power-capped by STAPM at ~35 W the whole time it
  plays.** The STAPM limit itself moves: 45 W while the dGPU is idle (boot, after exit) and 35 W while the dGPU
  draws 60-75 W (shared laptop power/thermal budget). PPT SLOW 61-65 %, PPT FAST 50-54 %, TDC ~34 %, EDC ~42 %:
  not binding.
- Zen5 core clock 4741 MHz (x47.5) held under the cap (global frequency limit 4740-4750 MHz; 5.0 GHz when lightly
  loaded); Zen5c cores 3284 MHz. MCLA_CPU eff_mhz avg ~4300-4340.
- CPU skin temperature **rose monotonically 47.5 -> 59.9 C over the 10 minutes and had not plateaued.** Skin
  temperature feeds AMD STAPM / skin-temperature tracking; on a longer session (the l70 episodes came 30-45 min
  in, in races) a further cut of the STAPM limit below 35 W is a plausible KI-067 mechanism. Hypothesis only; not
  observed here.
- NVIDIA GPU: 59 W avg (max 76.5) against 60/80 W limits; "power" performance limiter Yes in 132 of 290 samples,
  thermal limiter never; 71 C avg / 75.8 C max; core load ~73 %.
- Memory: physical load 84-86 % (only 4.3-4.8 GB of ~32 GB free), commit 55-56.5 GB, page-file use 8.6-9 %.

**Foreign-load dip 16:02:36-16:02:46 (seen in both logs):**

- HWiNFO total usage 30 % -> 79 / 86 / 91 / 77 % (samples 16:02:39-16:02:45), all 16 threads in C0 (7.8 active
  cores), commit +1.6 GB, physical available -1.1 GB, system drive read 31 MB/s then write 25-41 MB/s.
  The game process did not rise: MCLA_CPU window ending 16:03:12 has proc_pct max 23.9 % vs sys_pct max 93.5 %
  (eff_mhz min 3736). A **non-game process** (process start / scan / update; identity unknown) used ~10 logical
  CPUs for ~8 s.
- Package power stayed pinned at 35.0 W (STAPM), so the foreign load took power budget from the game: Zen5 clock
  4741 -> 4187-4347 MHz; Tctl actually fell (91.6 -> 86.6 C); no throttling flag.
- Game effect: guest frames per second 45 -> 22-27 for ~8 s (16:02:37-16:02:43), PresentMon fps 21.8-27.0,
  NVIDIA load 76 -> 41 %. The F10 window covering it has the session-high CP 3.31 us/draw and own 3.11 us/draw.
- Not a KI-067 episode by duration (seconds, not minutes), but the same signature in miniature: host costs up
  while the game's own load is flat, package power capped, no thermal flag.
- A second burst ran right after the game closed (16:03:51-16:04:09: 58-90 % usage, disk write up to 65 MB/s,
  STAPM back to 45 W); likely the launcher's after-session work or the same background consumer.

## 4. Foreign consumer (sys_pct vs proc_pct)

- Steady state: HWiNFO total 27-30 % = MCLA_CPU sys_pct 26-29 % vs game proc_pct 18-22 %: a constant
  ~7-9 pp (about 1.2-1.4 logical CPUs) of non-game load (HWiNFO with its PresentMon sampling, launcher, DWM,
  others). HWiNFO validates the MCLA_CPU line.
- Burst 16:02:37-46 (above): 70+ pp of non-game load. The CSV has no per-process data, so no candidate can be
  named.

## 5. Recommendations for the next HWiNFO run

Keep (about 40 columns): Date, Time; Tctl/Tdie, core temperatures avg + per core, CPU skin temperature, Thermal
Limit %; CPU package power, core power, APU STAPM [W] and STAPM Limit %, PPT FAST/SLOW %, TDC/EDC %; global
frequency limit, per-core clocks (Zen5 cores 0/2/4/6), core effective clock avg and per-thread effective clocks;
total CPU usage, max thread usage, C0 residency, active cores; HTC / PROCHOT CPU / PROCHOT EXT; memory commit,
physical available, memory load; system drive read/write rate; NVIDIA temp, power, clock, core load,
power/thermal limiter flags; PresentMon presented fps, frame time 99 %, CPU busy, GPU busy. Drop: PCIe error
counters, DRAM SPD/PMIC, NPU, battery, network, drive SMART, iGPU block, VIDs, per-core power.

Interval: 2 s is enough for 2-3 min episodes (STAPM / skin trends are slow). **1 s is worth it** with the trimmed
column set (roughly 130 KB per 10 min), because the only event seen here lasted 8 s and its onset order
(foreign load vs clock vs fps) is just 3-4 samples at 2 s. Record from before launch through a long session
(episodes appeared after 30+ min). To name a foreign consumer, add an owner-side per-process log (for example a
Performance Monitor counter log of `Process(*)\% Processor Time` and `Process(*)\IO Data Bytes/sec` at 1-2 s),
which HWiNFO cannot provide.

Decision rule for the next episode: STAPM % at 100 with the STAPM W value below 35 (or skin temperature still
rising) and Zen5 clock down with usage flat -> platform power / skin limit; total usage up with proc_pct flat ->
foreign process; Tctl 95-100 C with HTC / PROCHOT Yes -> thermal throttling; none of these -> look inside the
process.
