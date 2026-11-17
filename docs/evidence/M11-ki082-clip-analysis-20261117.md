# M11 KI-082 clip analysis: traffic "tremble" measured from the owner's recording (2026-11-17)

Read-only analysis by an Opus subagent, 2026-11-17. No builds, no tests, no game launches, no repo
edits. Spark not used: a single video, measured with deterministic ffmpeg and standard-library
Python; there was no text packet to offload and checking a model answer would have cost more than
the measurement itself.

KI-082 (traffic cars tremble when they are close to the player). The owner says "only moving ones:
while they drive they shake, when they stand still everything is fine" and that this clip shows it.
Context and hypotheses: `docs/evidence/M11-ki082-traffic-probe-analysis-20261117.md`.

## 0. Input

| item | value |
|---|---|
| file | owner Steam recording `clip_1,794,928,997,627.mp4`, copied unchanged to `private/owner-l107-20261117/` |
| sha256 | `ee54da7cd37dbc5abf3900e92a1fbe48cd4ef804c2573cda82a29e8b4c63c6c4` (source and copy identical) |
| size | 156,740,076 bytes |
| video | HEVC 3840x2160, 2142 frames, 35.85 s, time base 1 us, variable frame timing (avg 59.75 fps) |
| scene | night, Broadway / downtown, Unlocked, about 45 fps game, 120 Hz output without VRR |

All pixel values below are in 3840x2160 (4K) output pixels. The game renders 1920x1080, so 1 render
pixel = 2 output pixels.

## 1. Method

1. Timestamps: `ffprobe` packet `pts_time` for all 2142 frames.
2. Duplicate detection: the whole clip was decoded once to 320x180 gray (`-threads 2`), and each
   frame's mean absolute difference (MAD) to the previous frame was computed. The MAD histogram has a
   clean gap between 0.08 and 0.12 gray levels; 0-0.08 is encoder noise on an identical frame. A frame
   counts as a duplicate when MAD < 0.1. This works while the scene moves (t < 30.5 s). In the last
   5 s the player stands and the scene is almost static, so duplicates cannot be told from static
   content there, and that tail is excluded from the timing statistics.
3. Segments: 2 fps contact sheets and single frames were used to pick the segments. Each segment was
   decoded at 1920x1080 rgb24 (scale `area`), and only the unique game frames were tracked.
4. Tracking: a weighted centroid inside a moving search window (predicted from the previous
   velocity). The weights are:
   - red lights: `R - (G+B)/2 - 40` where `R > 120`;
   - white lamps and headlights: luma - 200;
   - yellow ceiling lamps: `(R+G)/2 - B - 60`.

   A car is the mean of its left and right light clusters. `sep` is the right x minus the left x, a
   size and distance proxy that grows as the car gets closer. The player car is the mean of its two
   tail-light clusters, which are fixed to the chase camera.
5. Residual: a local quadratic fit over 7 consecutive unique frames, with the residual taken at the
   centre frame. It was computed against three time axes:
   - `_t`: capture time (the recorder's pts of the unique frame);
   - `_k`: uniform frame index (one rendered frame = one step);
   - `_c`: "world time", a per-frame time corrected (least squares) so that the static world features
     move smoothly. It tests whether the car moves consistently with the world inside each rendered
     frame.

   Baseline: for white noise, the 7-point quadratic residual has a lag-1 autocorrelation of about
   -0.4 (measured on the static segment S3: -0.38 .. -0.49). A real period-2 alternation would push
   it towards -1; a value near -0.4 means no alternation.
6. Step ratio: the per-frame displacement, projected on the local direction of motion and divided by
   the mean displacement of the surrounding 7 frames. A one-step pose lag that comes and goes, or a
   lower-rate traffic update, would show ratios near 0 and 2 (a staircase). A uniform update gives a
   narrow peak at 1.0.

Scripts, track tables and contact sheets are in the local work folder `ki082-clip-work` (Temp). The
large raw decodes were deleted after use; they can be rebuilt from the clip and the scripts.

## 2. Frame timing

| item | value |
|---|---|
| recorder intervals | 1075 of 2141 intervals are exactly 16.67 ms; the rest are scattered 8.0 - 33.5 ms |
| duplicate frames (MAD < 0.1, t < 30.5 s) | 480, **all at exactly 16.67 ms** after the previous frame; 0 duplicates at any other interval |
| duplicate runs | 478 single duplicates, 1 double, never longer |
| unique game frames (t < 30.5 s) | 1341 in 30.5 s = **44.0 fps** |
| unique-frame capture interval | mean 22.75 ms, median 22.1, p5 9.2, p95 33.3, max 42.7 ms |
| interval histogram peaks (1 ms bins) | 17 ms (168), 20-21 ms (172), 25 ms (118), 33 ms (299), 9 ms (58), 12-13 ms (96) |
| lag-1 autocorrelation of intervals | -0.30 (short and long intervals alternate) |

Reading: the recorder runs a 60 Hz clock and repeats the last frame when no new game frame arrived
within one tick, which is why duplicates appear only at exactly 16.67 ms. New frames are stamped at
arrival time. Those arrival times are irregular: the unique-frame interval scatters by about +-35 %
around 22.7 ms.

The rendered content does not scatter like that. The per-frame step of every tracked object is
uniform (s.4.2): the step-ratio sd is 0.07-0.11 including tracking noise, against 0.30-0.35 for the
capture intervals, and the steps are uncorrelated with the capture intervals (r = -0.10 .. +0.24). So
the game advances its world by an almost constant amount per rendered frame (about 22.7 ms of motion
at 44 fps), while the frames reach the recorder (and, we infer, the 120 Hz display) at irregular
times. No step ratio near 2 was seen in any segment, so the recorder did not drop game frames in the
measured segments.

## 3. Segments

| id | time (s) | unique frames | scene | tracked features |
|---|---|---|---|---|
| S4 | 0.60 - 1.30 | 31 | player at ~10-22 mph, turning; red car crossing just ahead from left to right (moving, close, about 830 px/s on screen) | red car headlights (pair), player tail-lights, T-Mobile sign, street lamp, red traffic light |
| S1 | 13.30 - 15.60 | 102 of 137 | under the overpass at ~13-16 mph; white car ahead-right in the next lane (moving, roughly 10-15 m) | white car tail-light clusters, player, three yellow ceiling lamps |
| S2 | 19.30 - 21.80 | 110 of 147 | 40 mph braking to 11 mph; blue car ahead-right being overtaken (moving, close, 530 px/s) | blue car tail-lights, player, street lamp |
| S3 | 25.40 - 28.00 | 113 | player stopped (0 mph) at a red light; white car standing next to the player; distant cross traffic | white car tail-light, player, traffic light, street lamp |

Not used:
- The black car passing on the right at 14-15 s and 21-22 s: its lights leave the frame too fast for
  a 7-frame fit.
- The crossing traffic in S3: no lights face the camera.
- The right tail-light of the S3 white car: its glow saturated the search window.

## 4. Results

The tables give residual RMS and peak-to-peak in 4K pixels. `px/s` is the mean screen speed along
the axis. `rms_t/speed` is the capture-time residual divided by the screen speed, expressed as a
timing error.

### 4.1 Against capture time (what the recording shows)

| segment | feature | px/s | rms_t | p2p_t | rms_t / speed |
|---|---|---|---|---|---|
| S4 | red car x (moving traffic) | 834 | 3.97 | 16.3 | 4.8 ms |
| S4 | street lamp x (world) | 285 | 1.60 | 8.1 | 5.6 ms |
| S4 | traffic light x (world) | 285 | 1.31 | 6.8 | 4.6 ms |
| S4 | player car x | 44 | 0.37 | 1.6 | - |
| S2 | blue car x (moving traffic) | 530 | 4.40 | 32.8 | 8.3 ms |
| S2 | blue car sep (distance) | 149 | 1.23 | 9.3 | 8.3 ms |
| S2 | street lamp x (world) | 507 | 3.04 | 22.6 | 6.0 ms |
| S2 | street lamp y (world) | 234 | 1.48 | 12.7 | 6.3 ms |
| S2 | player car y | 70 | 0.44 | 3.0 | 6.3 ms |
| S1 | white car x (moving traffic) | 198 | 1.05 | 5.9 | 5.3 ms |
| S1 | ceiling lamp B x (world) | 165 | 0.83 | 3.8 | 5.1 ms |
| S1 | ceiling lamp A y (world) | 122 | 0.65 | 3.2 | 5.4 ms |
| S1 | player car x | 6 | 0.17 | 1.4 | - |
| S3 | white car x / y (standing traffic) | 0 | 0.01 / 0.02 | 0.11 / 0.27 | - |
| S3 | traffic light, lamp, player (static) | 0-1 | 0.01-0.08 | 0.08-0.49 | - |

- The residual is the same timing error, about 5-8 ms RMS, for traffic cars, world features and the
  player car where it moves. Its amplitude is proportional to screen speed, not to being a traffic
  car. Moving traffic close to the camera simply has the highest screen speed (S4 830 px/s, S2
  530 px/s), so it shows the largest displacement error: 4 px RMS and 16-33 px p2p.
- The residual correlates with the capture interval (|r| = 0.45-0.75; the sign depends on the axis).
  It vanishes when the time axis is corrected from the world features (s.4.2), so it is a timing
  effect, not a geometry effect.
- Standing traffic with a standing player (S3) shows 0.01-0.02 px RMS, i.e. perfectly still. This
  matches the owner's "standing still everything is fine".

### 4.2 Per rendered frame (uniform index) and against world time

| segment | feature | rms_k | p2p_k | ac1_k | rms_c | step ratio sd | step ratio p5-p95 |
|---|---|---|---|---|---|---|---|
| S4 | red car x | 0.48 | 2.1 | -0.41 | (2.3, see note) | 0.070 | 0.85 - 1.07 |
| S4 | traffic light x | 0.27 | 1.3 | -0.11 | 0.30 | 0.144 | 0.72 - 1.22 |
| S2 | blue car x | 0.21 | 1.5 | -0.09 | 0.40 | 0.082 | 0.84 - 1.14 |
| S2 | blue car sep | 0.22 | 1.1 | -0.10 | 0.23 | - | - |
| S2 | street lamp x | 0.24 | 1.5 | -0.34 | (used for correction) | 0.078 | 0.88 - 1.17 |
| S1 | white car x | 0.19 | 1.4 | -0.12 | 0.23 | 0.096 | 0.87 - 1.12 |
| S1 | white car y | 0.69 | 4.3 | -0.36 | 0.69 | - | - |
| S1 | white car sep | 0.25 | 1.4 | -0.13 | 0.27 | - | - |
| S1 | ceiling lamps x/y | 0.12-0.23 | 0.6-1.2 | -0.36 .. -0.51 | 0.11-0.18 | 0.097-0.111 | 0.84 - 1.19 |
| S3 | all (static) | 0.01-0.10 | 0.08-0.49 | -0.38 .. -0.49 | - | - | - |

Note on S4: the world features there move slowly (30-290 px/s) and partly leave the frame, so the
world-time estimate is noisy. The red car's own per-frame steps are smoother (step-ratio sd 0.07)
than that estimate, and its uniform-index residual is 0.48 px. The 2.3 px `rms_c` reflects the noisy
time axis, not the car.

- Inside each rendered frame, moving traffic is where the world says it should be to about 0.2-0.5 px
  RMS (0.1-0.25 render pixels). That is the same as for static world features of similar size.
- Direction (S1, world-time 2D decomposition): the white car's residual is 0.27 px along its motion
  and 0.68 px perpendicular (vertical).
  - The vertical part (white car y: 0.69 px RMS, 4.3 px p2p) is the same on all three time axes, so it
    is not timing. It is the only car-specific term found.
  - It is small (0.35 render px RMS) and does not alternate (ac1 -0.36, the noise baseline).
  - It is most likely body motion (suspension or pitch at 13 mph), or blob-shape noise from the glow
    around the lights.
- Per-frame step ratios are unimodal at 1.0, with sd 0.07-0.10 for cars, the same as for the world.
  - No ratio near 0 or 2 in any segment: no staircase, no repeated pose and no skipped pose.
  - The lag-1 autocorrelation of the ratios is 0.0 to 0.4, so there is no period-2 alternation.

### 4.3 Bound on a one-step pose lag (hypothesis c)

If the pose were taken from the other slot of the two-slot render buffer on some frames, the car
would be displaced, relative to the world, by one simulation step of its own motion.
- S4 red car: one step is 17-25 px on screen (its measured per-frame displacement). The observed
  per-frame residual is 0.48 px RMS and 2.1 px p2p, so any intermittent stale pose is at most about
  3-10 % of a step. Excluded.
- S2 blue car: one step is about 0.4 m of travel at 40 mph. Along the view axis that would change
  `sep` by roughly 2-3 %, i.e. 3-10 px. The observed `sep` residual is 0.22 px RMS and 1.1 px p2p.
  Excluded.
- A constant (always stale) one-step lag would be smooth and invisible. It cannot be measured from
  the video, and it would not produce a tremble.

## 5. Verdict

| hypothesis | prediction | measured | status |
|---|---|---|---|
| (c) traffic pose and camera from different slots, intermittent | car-only residual along the motion of about one step (3-25 px), alternating or tied to frame time | car moves consistently with the world to 0.2-0.5 px RMS; step ratios 0.84-1.14 | **not supported** (a stale pose larger than about 10 % of a step is excluded) |
| lower-rate traffic update | staircase: repeated poses, then double steps | no ratio near 0 or 2; car steps as uniform as the world | **not supported** |
| camera-only judder (shaky camera pose) | world and player shake in the rendered frames | world features smooth per frame (0.1-0.3 px) | **not supported** |
| presentation-timing judder (uniform content steps shown at irregular times; 120 Hz without VRR) | everything that moves on screen shakes by speed x timing error; nothing static shakes | 5-8 ms RMS timing error (against capture time) for cars, world and player alike; amplitude proportional to screen speed; zero for standing traffic with a standing player | **supported** |

Conclusion: the clip shows no traffic-specific geometry error. The tremble the owner sees is best
explained by presentation-time judder.
- The game produces frames whose content advances uniformly (about 22.7 ms per frame at 44 fps), but
  the frames are delivered at irregular times.
- On a 120 Hz display without VRR, a 44 fps stream is shown with a mixed 2/3-vblank cadence
  (16.7/25 ms). That is at least +-4 ms of error, and more with the irregular arrival seen here.
- The displacement error is screen speed x timing error. It is therefore largest for close traffic
  that moves relative to the camera, whose high-contrast tail-lights the eye follows: about 4 px RMS
  and 16-33 px p2p at 4K for the S2 and S4 cars. The player car, fixed on screen, shows 0.4 px.
- Standing traffic with a standing player has zero screen speed and is perfectly still
  (0.01-0.02 px), which matches the report.

Confidence:
- High (about 85 %) that the rendered frames contain no car-specific tremble above about 0.5 px RMS
  (no intermittent pose lag or slot mix-up, no staircase).
- Moderate (about 60 %) that the visible tremble is display-cadence judder. The recorder's arrival
  timestamps are not the display's scanout times, so the exact on-screen timing error is inferred,
  not measured. It lies somewhere between the 2.4 ms RMS of pure 120 Hz quantisation and the 5-8 ms
  measured against capture time.

Testable prediction: standing (or parked) traffic that the moving player drives past, and the world
itself, should judder by the same amount at the same screen speed. At night the world is dark and
low in contrast, so the effect is mostly noticed on lit cars.

What this clip cannot show:
- The actual display times. The recording is 60 Hz with duplicate filling and arrival-time stamps,
  of a 44 fps game on a 120 Hz screen.
- Whether the irregular delivery comes from the game's present pacing (Reflex sleep, vsync), from the
  compositor, or from the recorder.
- Whether the 0.7 px vertical term is body motion or tracking noise.
- Anything in the last 5 s, where the scene is static.

## 6. Next steps

1. Owner A/B, the cheapest test and decisive for the cadence hypothesis. Use the same street and the
   same speed, following and then passing a traffic car, with:
   - (a) Unlocked, as now;
   - (b) a frame limit at a divisor of 120 Hz (60 or 40 fps), or Stock 30 fps;
   - (c) VRR on, if the panel supports it.

   If (b) or (c) removes the tremble, KI-082 is presentation pacing, not traffic.
2. Host side, which is better than video: log for every presented frame the host present timestamp,
   the present statistics or vblank count, and the game step dt. That gives the display time against
   the content time directly.
3. If a second clip is wanted:
   - Record at 120 fps at 4K (or with OBS at the display rate, constant frame rate, no duplicate
     filling).
   - Hold a constant speed behind one lit car for 10 s, drive alongside it for 5 s, pass a parked
     car, then stand while a car drives past.
   - Record the same route with a 60 fps cap as the control.

## 7. Receipts

- Commands:
  - `ffprobe -show_entries packet=pts_time` for the timestamps;
  - `ffmpeg -threads 2 ... scale=320:180,format=gray -f rawvideo` for the MAD pass;
  - `ffmpeg -threads 2 -copyts -ss .. select='between(t,a,b)',scale=1920:1080,showinfo -pix_fmt rgb24`
    for each segment;
  - the owner Python, standard library only (numpy and PIL are not installed there).
- Unique frames per segment: S4 31, S1 102 of 137 captured, S2 110 of 147, S3 113 of 179. In the
  static S3 segment, frames below the MAD threshold were skipped as duplicates.
- Owned processes: only short-lived ffmpeg, ffprobe and python, at `-threads 2` or single-threaded.
- The evidence file could not be written to `docs/evidence/` from this subagent (the shared checkout
  was write-blocked). It is left in the work folder for the primary agent to copy.
