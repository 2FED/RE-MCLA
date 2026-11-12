# KI-077 / KI-020 excess brightness and bloom: research (2026-11-12)

Read-only research by an Opus subagent, accepted by the primary agent; proven and inferred points are marked in the body. Spark not used: source reading and image statistics.

**Headline.** The scale-2 handling of the 1x1 adapted-luminance target cannot explain a brightness difference BETWEEN sessions: every PC capture from l86 to l94 uses the same scale (2) and output size, the texel bias it introduces is deterministic and scene-only, and adaptation takes seconds. Image statistics confirm bright sessions (l88 steady-off p99 164-187, l90 7585 p99 140-156, l91 hangout frame p99 245 with 1.6 % saturated) versus normal ones (l88 cap44 125-138, l88 default 78-128) with no separating host setting. Likeliest cause (inferred): game state: time of day plus a script-switched bright-pass preset (byte +340 of the post-fx object at [0x829054A0], written only by two script natives; BrightPassValues built in sub_8260DA68 from one of two presets), and ToD tunables for bright pass, lamp glows, luminance and adaptation. Nothing logs these yet. Proposed: an always-on tiny log of the adapted luminance (03098000) and the bright-pass preset fields next to MCLA_TOD and in F10; owner A/B "720" (1280x720 x1) vs "1440" (1280x720 x2) on the same static night scene would rule the scale in or out.

---

# KI-077 / KI-020 bloom research (read-only), 2026-11-12

- xs.py: cross-session F10 frame stats (rows 10-62 %), output xs.txt. All PC sessions l86..l94 are scale 2,
  window physical 3840 wide, frame 1920x1080 -> scale/window cannot explain between-session variance.
- Bright night sessions: l88 ffc3 (p99 164-187, brt 0.66-1.17 %), l90 7585 (p99 140-156), l91 3b8f 171640 hangout
  (p99 245, sat 1.59 %). cap44 12ac p99 125-138. l94 frames are 06:00 dawn (day, not comparable).
- Chain (l91 3b8f log, fmt 36 = k_32_FLOAT): 07459000 128x128 -> 07469000 64x64 -> 0746D000 16x16 -> 0746E000 4x4 (8x8 resolve)
  -> 03099000 4x4 (8x8 resolve) / 03098000 1x1 (8x8 resolve); all scale=2x2, all read-back keys (resolve2 decision=now
  copied), tex_load scaled_resolve=1; chain memory overlaps the 640x640 depth read-back range 072B3000+3194880 (game aliasing).
- SDK: fill_half_pixel_offset at scale>1 (draw.cpp:31,1031-1035), half_pixel_offset (draw.cpp:29,325-328), texture-offset
  scaling (dxbc_translator_fetch.cpp:27,884-887), CPU-write -> scaled-resolve pages dropped (texture/cache.cpp:1284-1337),
  read-back downscale picks top-left host pixel (resolve_downscale.cs.hlsl:13-28).
- Game: sub_8260DA68 (47.cpp:36404..): BrightPassValues(+1508) = one of two float4 presets {+256,+260,+264,+308^2} or
  {+268,+272,+276,+280^2} selected by byte +340; +1512 = (+304)^2; +1560 = +348. +340 setter sub_823076E8 (16.cpp:29138)
  on object [0x829054A0], called from script-native wrappers sub_82304680 (16.cpp:21657) and sub_82305080 (16.cpp:23235).
- Launcher: mcla_render_target_policy.h:16-19: 720 = 1280x720 x1, 1080 = 960x540 x2, 1440 = 1280x720 x2, 2160 = 1280x720 x3.
