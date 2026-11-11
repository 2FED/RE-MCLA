# l88 steady-off run (2026-11-11): KI-074 and KI-077

Read-only analysis by an Opus subagent, accepted by the primary agent; proven and inferred points are marked in the body. Spark not used: log parsing and source reading.

**Headline.** l88 arm steady-off (session ffc34e51): 39-46 fps (default 46-52), GPU 83-90 % busy (default 92-96 %), CP lead max 1 frame (default 2), 5.5-7.4 ms/frame of targeted waits; the exemption buys about 5 fps. The exposure-latency hypothesis is refuted by code: the luminance/adaptation chain (rage grpostfx; targets 03098000/03099000 and 07459000..0746E000) is GPU-side with a fixed 1-frame latency in both arms, and adaptation is per second (ElapsedTime = game dt), so it cannot oscillate from read-back timing nor accumulate over minutes. 039CA000 is a 256x256 colour resolve sampled GPU-side in the same frame; no CPU reader. Leading hypothesis for the lit-surface flicker: GPU saturation with a 2-frame CP lead (same split as l77: flicker at >= 90 % GPU, none at <= 87 %). Residual light-source flicker matches the older KI-041(a). KI-077 bloom drift could not be quantified: only 4 F10 packages, all from the last 2.5 minutes; candidates: time of day not really reset on restart, the scale-2 handling of the 1x1 adapted-luminance target (rendered 2x2 inside an 8x8 tile), or game-side integration.

---

# l88 steady-off run (2026-11-11), read-only analysis

Evidence copy: private/owner-l88-flicker-20261111/{diagnostics,sessions}/ffc34e518df042309da5519f96eb9a4f
(robocopy /XD save-private /XF save-files-private.json; logs, receipts, 4 F10 packages, runtime log).

## Identity
- Session ffc34e51, 10:15:00-10:30:47 local (~15.8 min; driving 10:16-10:30). Build D17C1E6B... (l88), unlocked,
  RT 1920x1080 scale 2, guest 960x540, vsync 1, guest_vblank 240. 13 dev values = run A's 12 +
  `readback_steady_exempt_off=true (d3d12_readback_sync_steady_exempt=false)`.
- Logs complete (mcla.1.log 10:15:01 -> mcla.log 10:30:46), concatenated as ffc.all.log.
- F10: only 4 presses/packages: 10:28:05, 10:28:29, 10:28:54, 10:29:44 (all in the last 2.5 min).
  Race timers 01:09.60, 01:34.62, 01:58.99, 00:08.59 (next loop). Same /4 Arcade race, Midnight (black sky).

## Per minute (pm.py, iv.py)
- steady-off: fps 39.0-46.3 (median ~43), GPU 83-90 %, CP 85-96 %, targeted waits 1.03-1.07/frame = 039CA000
  forced own slot every frame, 5.5-7.4 ms/frame (max single 11-19 ms), copy_ms 6.6-8.8 ms/f,
  cp_lead_frames max 1, lead2_frames 0, fence_late_f2p 0.
- default 5a50364b driving minutes: fps 46.4-51.7 (median ~48), GPU 92-96 %, CP 76-92 %, read-back wait
  0.6-1.4 ms/f (targeted 0.2-0.5 + steady fast 0.4-0.8), lead max 2.
- Exemption buys ~5 fps (~10-11 %), ~5.5-6 ms/frame CP wait; costs GPU saturation (+6-8 points busy).

## Frames (lum.py on full-res BMPs, region rows 10-62 %; lum.txt)
- steady-off 4 frames: mean 25-42, p99 164-187, bright(>=180) 0.66-1.17 %, block p99 149-173, ring 73-89.
- default A (09:28, 09:50-52): p99 78-128, bright 0.01-0.25 %, block p99 74-111.
- fenced B: p99 100-151, bright 0.01-0.63 %. l86: p99 94-175 (f15 Carney's 01:44 same race: 175 / 0.80 %).
- No early steady-off frame -> in-session drift not measurable. Cross-arm differences confounded by place/ToD.
- Crops cS4.png (steady-off 00:08 Mulholland) vs cB4.png (fenced 00:21 same road): S4 lamps have large
  volumetric halos/cones; B4 lamps small; B sky reddish clouds vs S black (ToD differs).

## Code
- XEX expanded in temp for strings only (xex.ps1; image deleted). rage grpostfx: SampleAverageLuminance ->
  ReSampleAverageLuminance(Exp) -> CalculateAdaptedLuminance (samplers AdaptedLuminanceMapSampler +
  ToneMapSampler; constants ElapsedTime, Frames, Lambda, TexelSize) -> ToneMap; "Current Adapted Lum 1x1 RT".
- sub_8260C8E0 (mcla_recomp.47.cpp:34229-34333) looks up ElapsedTime -> +1500, Frames -> +1516,
  Lambda -> +1504, Gamma -> +1556, BrightPassValues -> +1508.
- sub_8260DA68 (47.cpp:37872-37901): ElapsedTime = float at 0x827D7508 (game timer effective dt);
  37906-37936: Frames = (float)int @+284; 37937-37974: Lambda = sub_823DB788(2.0) / float @+288 (log -> ln2/timeframe).
- PC keys: 03098000 (8x8 resolve, sampled 1x1) and 03099000 (8x8, sampled 4x4), chain 07459000/07469000/0746D000/0746E000;
  all tex_load scaled_resolve=1, all tex_inval by_gpu=1 -> GPU-side feedback, independent of guest RAM read-back.
- ToD tunables: m_Luminance, m_LightAdaption, m_LampGlows, m_RoadReflection, m_BrightPass (parser at 16.cpp:12464-12504).
