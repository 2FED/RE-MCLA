# KI-071 cut-scene dialogue sounds muffled and hollow: code-side analysis (read-only)

Read-only code analysis by an Opus subagent on 2026-11-06 (SDK audio path, host, recompiled game code; owner package c652b5cc.../live-20261106T071434Z-36736-1); accepted by the primary agent.

Status: analysis only. Nothing was built or run. The SDK files cited are identical in
`private/l39-sdk-src` and `third_party/rexglue-sdk` (checked with diff).

## What the evidence shows
- The F10 package (live-20261106T071434Z-36736-1, build 2788628F, 0.20.37.0) and its log tail
  contain no audio lines. `state.json` has no audio block.
- Session `mcla.10.log` (09:10-09:22 local, which covers 07:14 UTC): all 301 REX_PERF_AUDIT
  lines show `audio_underruns=0`. The only other audio lines are the XMA Decoder and Audio Worker
  thread starts and "Audio system initialized". No starvation or decoder errors. This rules out
  underrun or stutter artefacts. The SDL device channel count is not logged (`sdl_audio_stability_audit`
  and `sdl_audio_route_audit` are off by default).

## Signal path (code facts)
1. The game imports only XAudioGetSpeakerConfig, XAudioRegister/Unregister/SubmitRenderDriverFrame,
   XAudioGetVoiceCategoryVolume[ChangeMask] and XMACreate/ReleaseContext (`generated/default`).
   Mixing, resampling, reverb, filtering and 3D panning are game code (statically linked
   XAudio2-style library and RAGE audio), recompiled and running as guest code. The host
   receives one finished 6-channel 48 kHz float frame per call (FL FR FC LFE BL BR, planar big-endian).
2. `src/kernel/xboxkrnl/xboxkrnl_audio.cpp:27` reports `*config_ptr = 0x00010001`
   (a constant inherited from Xenia).
3. The game decodes it at sub_8213AB78 (`generated/default/mcla_recomp.0.cpp`, about line 26252).
   `rlwinm r7,r8,17,30,30; ori r11,r7,1` gives mode = ((cfg >> 16) & 1) ? 3 : 1, stored at
   0x827D15D4/0x827D15D8. sub_8213A600 (about line 25164) branches on mode==1 to build different
   6-channel output-matrix levels. With 0x00010001 the game always runs in mode 3 (digital
   surround) and mixes a discrete 5.1 programme. Dialogue normally goes to the centre channel,
   and ambience/reverb returns go to the fronts and surrounds.
4. `src/audio/sdl/sdl_audio_driver.cpp` Initialize: opens a 6-channel stream. If the device
   reports exactly 2 channels, it reopens as stereo and the SDK downmixes. Otherwise the
   6 channels pass through to SDL/Windows (a 7.1 or virtual-surround endpoint stays on the
   6-channel path).
5. Stereo downmix, `include/rex/audio/conversion.h` sequential_6_BE_to_interleaved_2_LE (AMD64):
   L = 0.4*FL + 0.2*FC + 0.4*BL, R = 0.4*FR + 0.2*FC + 0.4*BR, LFE discarded.
   ITU/Dolby Lo/Ro is L = FL + 0.707*FC + 0.707*BL, normalised 0.414/0.293/0.293.
   So the centre is at -6 dB relative to the fronts (standard: -3 dB) and the surrounds are at 0 dB
   (standard: -3 dB). The centre-to-surround balance is off by about 6 dB.
   Side bug: the non-AMD64 scalar fallback swaps BL/BR (`br = input[4]`, `bl = input[5]`).
   This does not affect the x64 PC build, but is relevant to the ARM64 backlog.
6. XMA path (`src/audio/xma_context.cpp`): FFmpeg XMAFRAMES decodes at the context rate index
   (24/32/44.1/48 kHz) into the guest output ring. Sample-rate conversion is done by the game's
   own mixer from the wave format. A wrong rate index would change pitch or speed, not make speech
   muffled. Loop/flush handling exists (flush on context reset, loop-end subframe limit). The host
   side has no resampler or filter.

## Ranked hypotheses
H1 (most likely): wrong 5.1-to-stereo balance for centre-channel dialogue.
The host always tells the game it is on 5.1 digital (0x00010001), so cut-scene voice is mixed
into FC. The SDK stereo downmix then gives FC half the weight of FL/FR and the same weight as the
reverb/ambience in BL/BR. Dry voice drops about 6 dB relative to the room and ambience, which sounds
like distant, hollow speech "from a barrel". Engine, radio and music sit mostly in FL/FR (with engine
panning), so they are unaffected. This matches the report: only dialogue, present since the first build,
no underruns. A console on stereo output reports a stereo config, so the game uses its own
mode-1 stereo mix and FC-only dialogue never appears.

H2: the host endpoint is 7.1/virtual surround (Windows Sonic, Dolby Atmos for Headphones, or a
headset 7.1 driver). The SDK passes 6 channels through, and the spatialiser renders the discrete
centre through an HRTF "room". That colours centre-only content (cut-scene voice) with comb
filtering and reverb, while front-stereo content is barely affected. It has the same root cause as H1
(the game is told it has 5.1); only the component that downmixes differs.

H3 (less likely): a game-side environment effect (a reverb/low-pass interior preset, or an X3DAudio
distance low-pass on cut-scene emitters) computed wrongly by recompiled math, for example a large
listener/emitter distance from a cut-scene camera bug. This would also affect other positional
cut-scene sounds. Nothing in the logs points to it. It stays open only if the probes show the
dialogue energy is not concentrated in FC.

H4 (unlikely): an XMA decode defect (wrong rate index, mono/stereo flag, stale subframes at packet
or loop boundaries). Such defects cause pitch errors, clicks or garbage, not a steady low-pass or
hollow tone, and in-game XMA streams sound fine. Dialogue may not use XMA at all.

## Discriminating probes (cheapest first)
- P0 (no build): ask the owner for the Windows output device format (channels) and whether
  Spatial sound is on. Stereo with spatial off points to H1. 7.1 or spatial on points to H2.
- P1 (existing cvar, no code): one session with `sdl_audio_stability_audit=true`. It logs
  `SDL_AUDIO_STABILITY_DRIVER ... source_channels=2|6`, which shows which downmix is in effect.
- P2 (no build, non-destructive): switch the Windows device to 5.1 (if allowed) or toggle Spatial
  sound, then replay the Pier cut-scene. The change in dialogue clarity identifies the downmixer.
- P3 (small SDK probe): in RecordAudioRouteSubmittedFrame, accumulate per-channel RMS of the 6
  planar channels and log one line per second, numbers only
  (`SDL_AUDIO_CHANNEL_RMS fl= fr= fc= lfe= bl= br=`). In the cut-scene, H1/H2 predict FC
  dominating and BL/BR carrying the room. H3 predicts the voice in FL/FR or no FC energy.
- P4 (decisive A/B, cvar-gated SDK change): `audio_speaker_config=surround|stereo`. Stereo reports
  bit 16 clear, so the game takes mode 1. Replay the same cut-scene in both modes. If stereo removes
  the barrel sound, H1/H2 are confirmed.

## Proposed fix
1. Primary: make XAudioGetSpeakerConfig follow the host. When the SDL device is stereo (or the cvar
   `audio_speaker_config=auto|stereo|surround`, default auto, selects stereo), return a value with
   bit 16 clear (for example 0x00000001) so the game produces its own authored stereo mix. Keep surround
   only for real 6- or 8-channel endpoints. The driver's channel decision must be available before the
   game's first XAudioGetSpeakerConfig call; the driver opens at "Audio system initialized", which is
   before title code. Review the game's mode-1 branch (sub_8213A600, flag byte at 0x827D180C)
   before changing the default.
2. Secondary (still needed whenever surround content reaches a stereo device): change the downmix to
   ITU Lo/Ro weights (FL/FR 1.0, FC 0.7071, BL/BR 0.7071, LFE 0, optionally 0.5), normalised by
   1/(1+0.7071+0.7071) ~= 0.4142. Fix the scalar path's BL/BR swap in the same change. Add a unit
   test next to `tests/unit/audio/sdl_audio_config_test.cpp` asserting per-channel weights and
   channel order for both the SIMD and scalar paths.
3. Acceptance: owner A/B on the Pier conversation with Karol (old vs fixed), plus a check that engine,
   radio and music balance did not change. A physical PASS needs the owner's listening confirmation.
