# M11: KI-071 fix in the SDK (speaker config follows the host, ITU stereo downmix, channel RMS probe)

Implementation by an Opus subagent on 2026-11-06 as SDK commit `dbe6d54` on `mcla/m11-audio` (parent `109116b`), to be cherry-picked onto `mcla/m11-content-unmount` for l71; accepted by the primary agent as code: ctest 2034/2034. Not run in the game; acceptance is the owner listening A/B on the Pier conversation (old = surround + ITU off; itu = surround + ITU; stereo = game stereo mix) plus a check that engine/radio/music balance is unchanged.

---

# KI-071 SDK audio change: speaker config, ITU downmix, channel RMS probe (draft)

Draft by an Opus implementation subagent, 2026-11-06. The primary agent still has to review and accept it.
Build and unit tests only. The game was not launched, and nothing here is a physical PASS.

## Identity
- SDK repo `private/l39-sdk-src`, worktree `private/l71-sdk-audio-wt`, branch `mcla/m11-audio`,
  commit **dbe6d54** (parent 109116b). Not pushed. No install step.
- Build: `private/l71-sdk-audio-b` (output `private/l71-sdk-audio-o`). The script is
  `%TEMP%\claude\C--BDU-MCLA-Recomp\l71-audio-scripts\l71_sdk_audio_build.ps1`, copied from
  `l71-scripts/l71_sdk_build.ps1` with the same options (Ninja Multi-Config, Release, clang 20.1.8,
  x86-64-v3, D3D12 only, tests, Tracy, perf counters). The install command was removed from it.
- The worktree's thirdparty submodule trees were copied from `l39-sdk-src` (the gitlinks are identical
  at 109116b), including their `.git` gitfiles because thirdparty/CMakeLists.txt checks for them.

## What changed (citations at dbe6d54)
1. `src/kernel/xboxkrnl/xboxkrnl_audio.cpp` XAudioGetSpeakerConfig_entry. The answer is now
   resolved once (function-local static) by `ResolveGuestSpeakerConfig()`:
   - cvar `audio_speaker_config` (defined in `src/audio/speaker_config.cpp`; values auto|stereo|surround;
     default auto; `.allowed()`; RequiresRestart).
   - stereo -> 0x00000001 (bit 16 clear). surround -> 0x00010001 (the old constant).
   - auto: calls the new `AudioSystem::QueryHostOutputChannels()`
     (`include/rex/audio/audio_system.h`, default 0 = unknown). The SDL backend overrides it with
     `SDLAudioDriver::QueryPlaybackDeviceChannels()`, which runs SDL_InitSubSystem(AUDIO), resolves
     the same device that `Initialize` would use (the `audio_device` cvar, through the shared
     `ResolvePlaybackDevice`), calls SDL_GetAudioDeviceFormat, then SDL_QuitSubSystem.
     1-2 channels -> stereo; more than 2 -> surround; 0/failure -> surround plus a WARN.
   - Logs once at INFO: `[audio] speaker config: cvar=<mode> device_channels=<n> answer=0x<value>`.
     When the answer falls back, it also logs at WARN:
     `[audio] speaker config: host device channel count unknown; auto keeps surround (0x00010001)`.
   - Call order: the SDL stream opens per render client in `AudioSystem::RegisterClient ->
     SDLAudioSystem::CreateDriver -> SDLAudioDriver::Initialize`, not at "Audio system initialized"
     (that line is AudioSystem::Setup). The game calls XAudioGetSpeakerConfig from sub_8213AB78
     (`generated/default/mcla_recomp.0.cpp` ~26252) and sub_82169C70 (`mcla_recomp.2.cpp` ~2408).
     The register call is in `mcla_recomp.25.cpp` ~54925. Whether the speaker-config call runs before
     or after registration was not established statically. The direct device probe makes the answer
     independent of that order, so no driver state is shared.
   - Game decoding (verified in the generated code): sub_8213AB78 does `rlwinm r7,r8,17,30,30; ori 1`,
     so mode = bit16 ? 3 : 1. sub_82169C70 tests `& 0x10000` and stores a flag byte at +659. Only
     bit 16 is consumed, so the low word (1) does not matter to this title. 0x00000001 as "analog
     stereo" follows the XDK low-word convention; it comes from memory and was not checked against
     an XDK header.
2. `include/rex/audio/conversion.h`: new `StereoDownmixWeights`, with `kItuStereoDownmix`
   {0.41421356, 0.29289322, 0.29289322} and `kLegacyStereoDownmix` {0.4, 0.2, 0.4}.
   `sequential_6_BE_to_interleaved_2_LE` (SSE path) now takes weights:
   L = front*FL + surround*BL + center*FC. LFE is still dropped. The scalar versions are now always
   compiled (`*_scalar`) so they can be tested on x64, and on non-AMD64 the dispatch names forward to
   them. **The scalar BL/BR swap is fixed**: index 4 is BL and goes to L, index 5 is BR and goes to R.
   With full-scale FL+FC+BL = 1.0 each, the output is exactly 1.0 (no clip).
3. `src/audio/sdl/sdl_audio_driver.cpp`: new cvar `audio_downmix_itu` (bool, default true; false
   selects the legacy weights). It is read per SDL callback, so it can be toggled live. The SDL hints
   and device lookup were factored into `ApplySdlAudioHints` / `ResolvePlaybackDevice`, with no
   behaviour change. `SubmitFrame` calls `RecordChannelRmsProbeFrame`.
4. `src/audio/channel_rms_probe.cpp` / `include/rex/audio/channel_rms_probe.h`: cvar
   `audio_channel_rms_probe` (default false). It accumulates the per-channel RMS of the guest planar
   big-endian frame (256 samples per channel) and logs once per 48000 samples per channel, i.e. one
   line per second of audio:
   `[audio] rms: fl=0.0000 fr=0.0000 fc=0.0000 lfe=0.0000 bl=0.0000 br=0.0000` (numbers only;
   non-finite samples count as 0). When the cvar is off, the cost is one cvar read per frame.

## Cvars (all category Audio)
| cvar | default | effect |
| --- | --- | --- |
| audio_speaker_config | auto | auto/stereo/surround answer to XAudioGetSpeakerConfig (restart) |
| audio_downmix_itu | true | ITU Lo/Ro 6-to-2 weights; false = legacy 0.4/0.2/0.4 |
| audio_channel_rms_probe | false | 1 Hz per-channel RMS log line |

Owner A/B examples: `audio_speaker_config=surround` + `audio_downmix_itu=false` reproduces the old
behaviour exactly. `surround` + ITU isolates the effect of the downmix. `stereo` (or auto on a stereo
device) tests the game's own stereo mix.

## Tests
- New `tests/unit/audio/audio_output_mix_test.cpp` (8 cases). It checks the ITU ratios and
  normalisation and the legacy values. It checks the weights and channel order on the scalar and
  dispatched (SSE) paths for both weight sets, with BL->L, BR->R and LFE excluded. It checks that
  full-scale input does not clip and that SSE matches scalar, that the 6-channel interleave order is
  the same on both paths, the speaker-config parse and answer for every mode x {0,1,2,6,8} channels
  plus the bit-16 semantics, the cvar defaults and validation (`quad` is rejected), and the RMS
  accumulator window, values, NaN handling and format.
- ctest Release in l71-sdk-audio-b: **2034 tests, 0 failed, 4 skipped** (the BitStream Write
  skips). The 109116b baseline was 2026, so these are the 8 new cases.
- The full build produces no warnings in any touched file (the remaining warnings are in
  untouched headers and sources: xenos.h, d3d12_util.h, texture cache, SDL, and so on).
  clang-format 20.1.8 reports no changes on all touched files. The diff is ASCII only and
  `git diff --check` is clean.

## Risks / open points
- **The game's mode-1 (stereo) matrix is not fully reversed.** sub_8213A600 (flag byte 0x827D180C)
  builds different 6-channel output levels for mode 1. It is not yet confirmed that mode 1 puts
  dialogue in FL/FR at a sensible level, or that it leaves FC/BL/BR empty. With auto on a stereo
  device, the default now **changes the game's mix path** (mode 3 -> mode 1). If mode 1 is broken,
  `audio_speaker_config=surround` restores mode 3 (and the ITU downmix still applies). Running the
  RMS probe in both modes on the Pier cut-scene will show where dialogue energy goes.
- Loudness: the ITU normalisation gives front-only content x0.414 (old: x0.4), so the level is
  essentially unchanged and there is no clipping. Centre content goes up about 3.3 dB and
  surround content goes down about 2.7 dB relative to the old weights. If mode 1 leaves only
  FL/FR, the stereo result is about -7.7 dB below the guest full scale, the same as before.
- Windows Spatial sound (Sonic/Atmos) and 7.1/virtual-surround endpoints are outside our control.
  With 6 or 8 channels, auto answers surround and the 6 channels pass through to Windows, so the H2
  colouring remains there. The owner can force `audio_speaker_config=stereo` (6-channel stream, game
  stereo mix).
- Mono (1-channel) endpoints: auto answers stereo, but the driver still opens a 6-channel stream and
  SDL downmixes (unchanged driver behaviour).
- The device probe briefly initialises and shuts down the SDL audio subsystem on the guest thread at
  the first call. SDL3 reference-counts subsystem init, so if the driver already holds the subsystem
  the quit only decrements the count. A concurrent first-time driver Initialize on another thread was
  not exercised. A probe failure logs a WARN and keeps the legacy answer.
- If the default output device changes later, the answer does not follow, because it is resolved once
  per process (the game also reads it at init).
- Acceptance needs the owner's listening A/B on the Pier conversation with Karol, plus a check that
  engine, radio and music balance did not change. No physical PASS is claimed.
