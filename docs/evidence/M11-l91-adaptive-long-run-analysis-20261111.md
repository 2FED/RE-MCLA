# l91 lead-adaptive long run (2026-11-11): dawn sidewalk flicker and bloom on characters

Read-only analysis by an Opus subagent, accepted by the primary agent. Spark not used: log parsing and image statistics.

**KI-074.** Owner long run on l91 arm lead-adaptive (2026-11-11, session 3b8faee3, MAIN save, ~40 min): flicker again, but at in-game dawn and on the whole sidewalk/ground (packages 2-4, Sunset Blvd to the Santa Monica Promenade). In the minutes around those presses the frame-end gauge and late fences were 0 (last lead-2 frame >= 70 s earlier) and adaptive was disarmed, yet frame F-1 was still on the GPU at frame open in 80-98 % of frames, and steady fast waits (464-468/min; session median 27) and unfenced-served cadence copies peaked. So frame-end lead 2 is NOT the separator for this event; the quantity that fits every run so far is the mid-frame state "F-1 not finished" (0 in steady-off, cap44 and strict; 350-725/min in every flicker run). Alternative: a dawn lighting transition (the resolve key set switches 20 s after the last press). Strict has only been run at night. Adaptive is therefore not adoptable; next owner run: l91 pc/lead-strict on the same route through dawn.

**KI-077.** Owner F10 on l91 (2026-11-11, package live-20261111T171640Z-22036-1, lead-adaptive long run, 10 min uptime): far too much bloom on the characters at the Santa Monica racers hangout (gameplay, on foot, night): white cap, polo, sneakers and licence plate have halos, the T-shirt print looks emissive; character area luma p99 255 with 2.5 % of pixels >= 250. No comparable earlier hangout frame exists. Closest older issue: KI-020 (white surfaces glow). Engine-rendered cut-scenes should show the same and are a good controlled comparison.

---

# l91 adaptive long run (session 3b8faee3), read-only notes

Evidence copy: private/owner-l91-flicker-20261111/{diagnostics,sessions}/<id> for 667e7aff (strict), 0f40ed8d (adaptive short),
3b8faee3 (adaptive long); robocopy /XD save-private /XF save-files-private.json *.dmp. Logs here: 3b8f.all.log (mcla.3,.2,.1,mcla.log
oldest first), 667e.all.log, 0f40.all.log. Scripts: pm2.py, win2.py, keys.py, bmp2png.py (copied), cpu.py, cad.py, sep.py, rkeys.py, fam.py, st.py.

Session 3b8f: 19:06:44-19:47:26 local, build 5485D487 (l91), arm frame_lead_adaptive (d3d12_frame_lead_strict=2), unlocked, RT 1920x1080.

## Packages
- 1 19:16:40 (uptime 9:56) hangout (racers' meeting spot), night, on foot; owner: too much bloom on characters (not flicker).
- 2 19:26:16 race lap 1/2 00:49.56 Sunset Blvd, dark dawn sky, yellow smoke checkpoint. Same race as 3/4, 40 s earlier.
- 3 19:26:56 lap 1/2 01:30.15 Arizona Ave (Santa Monica, 3rd St Promenade plaza), dawn/early morning, pedestrians. Sidewalk press.
- 4 19:26:59 lap 1/2 01:32.53 promenade sidewalk beside road, morning sky. Sidewalk press.
- Captured frames: plaza/sidewalk paving looks normal (grey tiles, soft headlight pool); no visible wrong patch in the captured frames.

## Lead counters around the presses
- Minute lines 19:26:46 (covers 19:25:46-19:26:46), 19:27:46, 19:28:46: lead2 0, f2p 0, triggers 0. Marks 19:26:16/56/59: lead2 0, f2p 0.
- Last lead-2 frame before the sidewalk presses: inside 19:24:46-19:25:46 (1 frame) -> >= 70 s before press 3.
- Adaptive was disarmed: strict waits 543 (19:26:46 min) then 0 in 19:26:46-56, 56-59 and the next two minutes; skips 2061-2522/min
  (F-1 still on GPU at open in 80-98 % of frames, no wait).
- Session-highest steady fast waits (exempt 039CA000 previous slot still on GPU): 464/min and 468/min in the press minutes;
  marks 110 in 10 s, 148 in 13 s. Flight ring composite wait share 34 / 44 / 29 % vs 0.5 % at the bloom press.
- cad_unf_other (unfenced previous slot served, non-k8 keys): 258 and 349/min, session peak, ~6/s at presses vs ~1.5/s before.
- fps/s before presses: 33-42 (press 2), 39-50 (press 3), 39-45 (press 4).

## Across runs (driving minutes >= 1500 frames; sep.py)
fast waits: steady-off 0, cap44 0 (max 1), strict 667e 0; every flicker run median 350-725/min; 3b8f median 27, max 468 at presses.
cad_unf_other: clean runs 0 (cap44 max 8); flicker runs high; 3b8f peak at presses. lead2: 3b8f press window 0.
=> frame-end lead 2 is not necessary for this report; "F-1 still running mid-frame F" (fast waits / skips) is the condition common to all
reported flicker windows and absent in all clean runs.

## Morning resolve set
- 19:27:19 (20 s after press 4): a new family of 28 single-channel (fmt 2) 128x128/256x256 resolve keys at 0317C000..03554000 appears
  and the night family (fmt 3 128x128 paired with fmt 6) stops; CPU read-back 5.6 -> 9.3 MB/frame; unfenced-served bytes 4-19 -> 77-176 MB/min.
- At the presses the night family was still active; fmt 3 tex_load sample count rose 19:26:20-19:27:00; all are scaled_resolve=1 loads.
- No time-of-day value is logged.

## Bloom package 1
Character region luma p99 255, >=250 2.5 %; white cap/polo/sneakers/plate have halos, T-shirt print looks emissive.
No earlier hangout frame found (report.txt search in owner-l86..l91 and private/evidence). Related: KI-077, KI-020 (white surfaces glow).
