# l84 Steam Deck analysis (2026-11-10): KI-054 hitch hold

Read-only analysis by an Opus subagent (logs/json only, python -I), accepted by the primary agent. Spark not used: deterministic log parsing.

**Headline.** KI-054 cause confirmed on the Deck (l84, D:/84, 2026-11-10): owner saw many traffic jumps on arm ambprobe and none on arm hitchhold. Instruments: every >100 ms frame applied a full 0.1 s step (1.4-2.7x average): 243 hitches -> 414 ambient step jumps; all owner F10 presses fall in windows with 13-38 hitches. With `raw_timer_hitch_hold` the applied max is 1.08-1.23x average and step jumps are 3 (first minute only). BUT the debt sits at the 1 s cap and 5.6 s of 256 s (2.2 %, up to 6 % in hitch-dense minutes) of game time is dropped, because the repayment ceiling is 1.1x an average that excludes hitch steps. Not default-on as written; v2 = slew-rate limiter that can climb towards measured time.

---

# l84 Steam Deck results (D:\84), read-only analysis, 2026-11-10

Inputs: sessions\<id>\{mcla.log,launch-receipt.txt,state.txt}, diags\<id>\{report.txt,latest-live.txt},
live\*\state.json (folder names only otherwise). Not opened: before/after-user, snapshots, save-private,
log-tail, frame.bmp, runtime\*.log; no *.dmp present. Parsers (python -I) in this folder: timer.py (timer
stats + AMBDT + F10 mapping, timer_out.txt), drift.py (time lost at the debt cap, drift_out.txt), f10.py
(state.json ambient_dt per package + REX_PERF_AUDIT per F10, f10_out.txt), agg.py/parse.py/audit.py/tags.py
copied from d82-work (agg_out.txt, p_ambprobe.txt, p_hitchhold.txt). Spark not used: two logs, deterministic parse.
Local clock = UTC+2. "hitches N" in the timer stats line is cumulative; per-window = difference.

## Sessions
| arm | session | local | length | F10 | proof |
|---|---|---|---|---|---|
| deck\ambprobe | f05fe64e | 14:07:34-14:13:07 | 334 s | 11 (14:08:56 pkg -1 not in D:\84; 14:10:31, 11:06, 11:16, 11:24, 11:40, 11:41, 12:03, 12:34, 12:39, 13:04) | 11 values, ambient_dt_probe + raw_timer_stats, no hitch hold |
| deck\hitchhold | f3763a82 | 14:14:12-14:19:13 | 302 s | 3 (14:15:29, 17:04, 19:11) | 12 values, + raw_timer_hitch_hold=true |
Both: build EEAC0416...3E027D0F = build_manifest_sha256 of private/l84-internal/candidate-receipt.json;
complete / exit 0 / Execution complete; crash folders empty; log dropped=0.

## Mechanism
- Ambient step = timer step: threaded=0, steps_per_update 1 per update (4799/4799 and 4199/4199 after the
  first), timer_mismatch 0, split_at_step 0 in both arms.
- ambprobe: applied max 100.0 ms in all 16 windows (max/avg 1.44-2.68), applied sd 3.5-14.5, step change
  0.9-7.7 ms; step_jumps 414 in 16 windows (2-62 per window), max_step_ratio 1.59-2.94; jumps/hitches 1.70
  (243 hitches) = up+down per hitch.
- hitchhold: applied max 54-76 ms (max/avg 1.08-1.23), sd 2.1-6.6, step change 0.6-0.9 ms; step_jumps 3 total,
  all before step 969 (start / first minute); 0 in the last 12 windows despite 32-37 hitches per window.
- F10s (ambprobe): all 10 in D:\84 fall in windows with 13-38 hitches (mean 24.9) and 24-62 jumps (mean 41);
  the 8 pre-F10 windows had 1-12 hitches (mean 5.3), 2-22 jumps (mean 9.9). Jump rate between F10s 7.6-25 per
  100 steps. Window granularity only (~20 s); no per-update trace before each press.

## Time accuracy
- ambprobe: measured 295.496 s, applied 294.869 s (+0.627 s, 0.21 %), final debt 0.520, max debt 0.787,
  0 windows at >= 0.9; time lost at the cap 0.107 s (0.04 %), all in one window.
- hitchhold: measured 256.473 s, applied 249.866 s (+6.607 s, 2.58 %); debt 0.79-0.98 in the last 5 windows;
  lost at the cap 5.628 s (2.19 % of session; 2.9 %, then 5.8-6.3 % per window in the dense windows).
- Cause: hold ceiling 1.1 x average, and the average excludes hitches (but includes the short partner steps:
  measured min 18-25 ms while presentation is 66 ms p50) -> 1.1 x avg (64-73 ms) < measured mean (66-71 ms);
  repayment can never catch up, debt pins at the cap, the excess is dropped.

## Side effects
fps 15.45 / 15.56; CP busy 52.6 / 52.3 ms/f (81 %); audio driving audits 69-83/s vs 69.9/s (l82 63-71/s);
cadence 5+ vblank 20-77 vs 44 per 300 audit frames; warnings identical; protect failures 0; readback
guard_misses 0; gap lock misses 18 / 26 (cumulative); log dropped 0.
Timer hitches (32-38 per 300 updates) are 2-4x the presented frames > 100 ms (audit 5-18 per 300 frames).
