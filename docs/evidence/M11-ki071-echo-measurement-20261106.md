# KI-071 (cut-scene dialogue sounds hollow / "voice twice with a small delay"): echo measurement

Measurement by an Opus subagent on 2026-11-06 (owner Game Bar capture of l72 vs an Xbox 360 longplay; ffmpeg/yt-dlp/numpy installed by the owner); accepted by the primary agent. Spectrograms: img/M11-ki071-spec-ours.png, img/M11-ki071-spec-ref.png.

Date: 2026-11-06. Analysis subagent; read-only; no builds, no game launches, no git writes.

## Result in brief

- **Our capture has a discrete, constant echo lag of 557.3-557.7 samples at 48 kHz (11.61 ms), plus a second copy at
  1114.4-1114.7 samples (23.22 ms).** Those values match **512 and 1024 samples at 44.1 kHz** (557.28 and 1114.56 at
  48 kHz), i.e. exactly one and two 512-sample XMA frames of a 44.1 kHz stream (or 256/512 at 22.05 kHz). They do **not** fit
  48 kHz frames (512 = 10.67 ms, 1024 = 21.33 ms), 24 kHz frames (512 = 21.33 ms) or 32 kHz frames (512 = 16.0 ms).
  The lag does not drift across 7 cut-scene lines (refined lag 557.1-557.7 / 1114.4-1114.7).
- **Copy level:** roughly -10 to -15 dB below the direct sound in the strongest lines (R channel, copy at d: -10.1 dB on the
  2:45 line, -12.2 dB on 2:42; L channel, copy at 2d: -13.1 / -14.7 dB), about -20 dB averaged over the whole 2:40-3:16
  scene. Frame-averaged values, so they are lower bounds for the frames that carry the copy (see method).
- **The copies are distributed unevenly between L and R and the pattern changes from line to line.** For example, at
  2:42-2:48 R carries the copy at +11.6 ms and L the one at +23.2 ms; at 3:06-3:15 R carries the +23.2 ms copy. The L-to-R
  transfer function has discrete taps at +/-557 and +1115 samples (up to about -10 dB relative to the lag-0 tap).
- **L/R skew:** no bulk delay between channels (main L/R correlation peak at lag 0, r = 0.85 on dialogue). The problem
  is extra inter-channel copies at +/-11.6 ms and +23.2 ms, not a shifted channel.
- **Not global:** the 11.6 ms lag also appears in the phone call at 0:12-0:16 (mono-mix cepstrum +11.9 sigma) and in
  driving gameplay at 0:17-0:40 (+22 sigma in the mono mix, and an L-to-R tap at +557 samples at 64% of the lag-0 tap, so there R
  carries an 11.6 ms-delayed copy of L). It is weak at 1:06-1:38 (+7.9 sigma) and **absent** at 1:53-2:13 (driving) and at
  2:21-2:33 (garage entry). So it does **not** affect the whole output stage. It follows particular content: dialogue,
  the phone call and part of the driving audio (probably a streamed 44.1 kHz source such as radio music; I could not
  confirm what is playing at 0:17-0:40).
- **Reference (Xbox 360 longplay, 5:50:30-5:51:30):** no peak at 557 or 1115 samples (cepstrum at d = -0.0025, the noise
  floor; the L-to-R transfer is exactly 0.000 at +/-557 and +/-1115). The only cepstral peaks are short and pitch-related
  (2.5 / 4.96 ms). Its L/R correlation is 0.96, and the reference dialogue is nearly mono (side about -20 dB versus mid). Our
  dialogue has side about -10 dB versus mid, which fits the L/R-asymmetric copies.

## Inputs

- OURS: `C:\Users\orange\Videos\Captures\mcla [rexglue-v0.20.0.3-dev.gd08813b-Release] 2026-11-06 19-07-46.mp4`.
  The file is only 197.06 s long, so the requested 4:00-4:10 window does not exist. Audio is AAC-LC, 48 kHz stereo,
  128 kb/s (Game Bar). l72 build, audio_speaker_config=auto, stereo + ITU downmix. The full track was decoded to 16-bit
  WAV for analysis and then deleted.
  Content, from thumbnails (`ki071-context-frames.png`): 0:05 car and phone UI, 0:14 map, 0:28-2:00 driving,
  2:27 garage entry, 2:40-3:16 cut-scene with subtitles ("Listen, you even know what you getting into down here?",
  "Ah see now I heard you were the man to race, Hugo.").
- REFERENCE: `https://youtu.be/RSakprOvdDo`, section `*5:50:30-5:51:30`, fetched by yt-dlp `-f bestaudio
  --force-keyframes-at-cuts`. The result is Opus 48 kHz stereo. YouTube Opus was re-encoded by libopus at 96 kb/s because of the
  keyframe cut, so this audio is transcoded twice. Kept as `media71_ref\ref.webm` (705 KB). Nothing from the media was executed.

## Method

Scripts are in `C:\Users\orange\.claude\jobs\06cd90d3\tmp\audio71\` (`timeline.py`, `echo.py`, `echo2.py`, `cross.py`,
`calib.py`, `spec.py`). They ran with `C:\Python315\python.exe -I` (numpy 2.5.4, scipy 1.18.1; matplotlib is absent, so
`spec.py` writes its PNGs directly). ffmpeg ran with `-threads 1`.

1. **Timeline:** 0.5 s blocks give RMS, side/mid ratio, spectral flatness (100-4000 Hz) and voiced fraction (40 ms
   frames with normalized autocorrelation > 0.6 at 2.5-12.5 ms). Dialogue lines start at 2:41.5. The gaps between lines
   are very quiet (about -55 dBFS), so the dialogue stream is nearly isolated.
2. **Echo detection:** 4096-sample frames (85 ms), hop 1024, Hann window. Only voiced frames are used for speech, and all
   frames above -48 dBFS for non-dialogue. For each frame: log power, minus a 400 Hz-smoothed envelope, limited to the
   150-6000 Hz band, then the real cepstrum. These cepstra are averaged over frames. Also computed: a whitened
   autocorrelation of the averaged spectrum. Channels: M = (L+R)/2, L, R and S = (L-R)/2. Pitch peaks smear out over many
   frames with varying f0; a fixed echo stays sharp. Peak significance is given in sigma of a robust MAD noise estimate over
   1-60 ms. Lags are refined with a parabolic fit.
3. **Level calibration:** copies at gain g (0.1 to 1.0) and delay d = 557.28 were added to the reference speech, and the
   result went through the same pipeline. The cepstrum at d is linear in g up to about 0.5 (slope k = 0.193). Our raw
   values divided by k give g. For multi-tap channels this is a first-order estimate. It is averaged over all voiced frames
   of a segment, so frames without the copy dilute it, and the true copy level in affected frames is the same or higher.
4. **L/R relation:** frame-wise cross-spectra are averaged to a Wiener estimate of the L-to-R transfer, h = E[R L*]/E[|L|^2]
   over 150-6000 Hz, with 2N zero padding. The impulse response is checked at 0, +/-557 and +/-1115 samples. The main
   cross-correlation peak is computed separately.

## Numbers

Cepstral significance at d = 557 and 2d = 1115 samples (sigma; + = echo-like peak), voiced frames:

| Window | frames | M d / 2d | L d / 2d | R d / 2d | refined M lag (d, 2d) |
|---|---|---|---|---|---|
| cut-scene 2:40-3:16 all | 338 | +28.1 / +25.8 | +3.7 / +15.9 | +25.9 / +26.9 | 557.51, 1114.59 |
| line 2:41.5-2:45 | 27 | +13.6 / +14.1 | -1.6 / +15.6 | +22.0 / +3.8 | 557.56, 1114.60 |
| line 2:45-2:48 | 32 | +19.2 / +16.9 | -2.4 / +20.9 | +30.5 / +3.4 | 557.54, 1114.61 |
| line 2:50.5-2:54 | 62 | +13.5 / +8.2 | -3.3 / +10.0 | +14.8 / +4.0 | 557.72, 1114.41 |
| line 2:58-3:02.5 | 54 | +4.7 / +6.3 | +0.6 / -2.0 | +8.7 / -1.0 | 557.11, 1114.68 |
| line 3:06.5-3:10 | 55 | +10.9 / +11.2 | -2.7 / +3.2 | -3.9 / +21.7 | 557.43, 1114.61 |
| line 3:10-3:14.5 | 65 | +11.3 / +14.5 | +4.1 / +1.3 | -2.6 / +33.1 | 557.48, 1114.53 |
| phone call 0:12.3-0:16 | 58 | +11.9 / -2.7 | -4.7 / -1.6 | -3.7 / -1.8 | 557.37, - |
| gameplay voice 0:41-0:44 | 53 | +4.5 / +2.3 | weak | weak | - |
| gameplay voice 0:56.5-1:01 | 30 | -2.0 / +1.3 | none | none | - |

Non-dialogue (all frames above -48 dBFS):

| Window | frames | M d / 2d | L d | R d | L-to-R tap at +557 (relative to lag-0 tap) |
|---|---|---|---|---|---|
| 0:17-0:40 driving | 1075 | **+22.0** / +3.0 | +1.3 | -1.8 | **0.047 / 0.074 (64%, -3.9 dB)** |
| 1:06-1:38 driving | 1496 | +7.9 / -1.8 | -3.3 | -1.6 | 0.013 / 0.133 (-20 dB) |
| 1:53-2:13 driving | 934 | -2.7 / +3.5 | -1.0 | -3.3 | 0 |
| 2:21-2:33 garage | 559 | -1.8 / +4.8 | -3.0 | -2.7 | - |
| REFERENCE 5:50:30-5:51:30 voice | 578 | no peak (top peaks 2.5 / 4.96 ms, pitch) | none | none | 0.000 |

Estimated copy level, g relative to the direct sound (cepstral calibration; n/s = not significant):

| Segment | M at d | M at 2d | L at d | L at 2d | R at d | R at 2d |
|---|---|---|---|---|---|---|
| whole scene | -20.3 dB | -21.0 dB | n/s | -24.7 dB | -21.4 dB | -21.0 dB |
| 2:41.5-2:45 | -16.6 | -16.3 | n/s | -14.7 | **-12.2** | -27.5 |
| 2:45-2:48 | -14.4 | -15.5 | n/s | **-13.1** | **-10.1** | -29.3 |
| 2:50.5-2:54 | -19.6 | -24.0 | n/s | -22.3 | -18.9 | -30.3 |
| 2:58-3:02.5 | -27.2 | -24.7 | n/s | n/s | -22.9 | n/s |
| 3:06.5-3:10 | -20.4 | -20.2 | n/s | -31.0 | n/s | **-14.5** |
| 3:10-3:14.5 | -21.2 | -19.1 | -30.1 | n/s | n/s | **-12.7** |

L-to-R Wiener taps on dialogue (lag-0 tap normalised): +557 / -557 / +1115 take values such as 0.046/0.023/-0.018 at 2:42,
0.061/0.025/-0.030 at 2:45 and 0.006/0.022/0.041 at 3:10. Lag 0 is 0.17-0.19, so these taps are -8 to -18 dB relative.
The L/R cross-correlation peak is at lag 0 in every window (dialogue r = 0.85, reference r = 0.96).

Spectrograms (0-3 kHz, 3 s, mono mix, 4096-point FFT):
`C:\Users\orange\.claude\jobs\06cd90d3\tmp\ki071-spec-ours-165s.png` (our 2:45-2:48) shows harmonics broken into beads and
dashes, the comb pattern at 86 Hz spacing. `C:\Users\orange\.claude\jobs\06cd90d3\tmp\ki071-spec-ref-5h51m00s.png` (reference)
shows smooth, continuous harmonic contours.

## Interpretation

- A peak this sharp at exactly 512 and 1024 samples of a 44.1 kHz timebase, constant across lines and also present in a phone
  call and in some gameplay audio, cannot be pitch, room reverb or a codec artifact. The reference has nothing at those lags.
  It is a whole-frame duplication or offset of 44.1 kHz stream content by one or two 512-sample XMA frames. The strongest
  copy is about 10-13 dB below the direct sound, enough to cause audible comb filtering ("hollow") and a slap-back feel.
- The L-to-R transfer has discrete off-zero taps whose sign and lag change per line. So the copies are not one channel
  delayed relative to the other. They behave like the same content mixed again 1 or 2 frames later into the channels with
  different gains: for example several voices or channel-mapping paths of the same stream that are out of sync by whole
  frames, or a stream whose frames are re-emitted into the downmix. Because part of the driving audio is affected and
  other driving stretches are clean, the stage is the stream or voice path (XMA decode, frame queueing, or voice/channel
  routing for 44.1 kHz streams), not the final output or resampler.
- Note for the RE track (`ki071-doubling-re.md`, line 70): a 512-sample frame at 24 kHz would give 21.33 ms
  (1024 samples at 48 kHz). The measured 2d lag is 1114.6 samples (23.22 ms), so the measurement favours a 44.1 kHz
  stream (or 22.05 kHz with 256-sample units).

## Caveats

- Both sources are lossy: AAC 128k for ours, and Opus encoded twice for the reference. Lossy codecs do not create a
  discrete, sample-exact peak at a fixed lag across many frames, and they do not remove one at -10..-20 dB. The calibration
  confirms the method resolves a -20 dB copy on the reference audio (g = 0.1 gives about +8 sigma).
- The reference is a different line, speaker and mix, so it only shows that the console has no discrete echo. It says
  nothing about matching dialogue level or reverb.
- Copy levels are frame-averaged first-order cepstral estimates. Treat them as about +/-3 dB. Wiener tap magnitudes are
  biased by L/R coherence and band limits, so use them only as relative values.
- I did not identify the content of the affected driving window (0:17-0:40). Radio music is likely but not verified.
- This is a single capture from a single build (l72, stereo + ITU downmix). I have not checked whether 5.1 output or a
  different speaker configuration changes the L/R distribution.

## Housekeeping

The decoded WAVs (38 MB and 11 MB) and the individual frame grabs were deleted. Kept: `media71_ref\ref.webm` (705 KB),
`ki071-context-frames.png`, the two spectrogram PNGs and the scripts in `audio71\`. The `media71_ours\` folder is empty.
