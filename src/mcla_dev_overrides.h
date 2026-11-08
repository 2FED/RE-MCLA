#pragma once

// Owner-internal developer overrides (M10 l57 CPU A/B,
// docs/evidence/M10-l57-cpu-ab-build-20261101.md; M10-002 l58 read-back
// elision A/B, docs/evidence/M10-002-readback-elision-build-20261101.md;
// M10-004 l59 Vulkan Deck A/B,
// docs/evidence/M10-004-vulkan-deck-ab-build-20261101.md; M10-003 shader
// store seeding switch, docs/evidence/M10-prewarm-union-store-20261101.md;
// KI-047 targeted read-back wait A/B,
// docs/evidence/M10-ki047-targeted-wait-20261101.md; M10 watch census and
// steps 1-2 of the Deck watch/upload path,
// docs/evidence/M10-watch-steps0-2-20261101.md; M10 watch step 3 hot blocks,
// docs/evidence/M10-hot-blocks-sdk-20261103.md and
// docs/evidence/M10-hot-blocks-host-keys-20261103.md).
//
// Builds made with the CMake option MCLA_DEV_OVERRIDES (OFF by default; ON
// only for owner-internal measurement builds, never for volunteer builds) read
// the per-user file %LocalAppData%\MCLA-R-Launcher-v1\mcla-dev-overrides.txt
// once at game start, because the launcher cannot pass extra command-line
// flags. The file holds lines "name=true" or "name=false":
//
// - at most kMaxBytes bytes, ASCII only (no BOM); otherwise the whole file is
//   rejected and nothing is applied;
// - lines end in LF or CRLF; empty lines and lines starting with '#' are
//   skipped;
// - a line must be exactly name=true or name=false (no spaces); the name must
//   be one of kKeys. Malformed lines, unknown names and repeated names (the
//   first occurrence wins) are ignored and logged by line number.
// - shared_memory_upload_ahead_16 and shared_memory_upload_ahead_64 are bool
//   convenience keys for the integer SDK cvar shared_memory_upload_ahead_pages
//   (0 = off, 16 or 64): =true writes 16 or 64, =false changes nothing. Both
//   true in one file is contradictory: neither is applied (logged as a
//   conflict on the later line). Integer values are deliberately not accepted
//   in the file; any other value needs the command line.
// - shared_memory_hot_blocks_shadow and shared_memory_hot_blocks_on are the
//   same kind of pair for the integer SDK cvar shared_memory_hot_blocks
//   (0 = off, 1 = shadow, 2 = on): =true writes 1 or 2, =false changes
//   nothing, both true is a conflict (neither applied). The other hot-block
//   cvars (K, N, upload cap, probe frames) need the command line.
// - mcla_warp_probe (KI-035) is a plain host bool. mcla_warp_watchdog is a
//   bool convenience key for the integer host cvar mcla_warp_watchdog_seconds
//   (0 = off): =true writes 15, =false changes nothing. Other limits need the
//   command line.
// - mcla_warp_finish_fix_off=true writes false to the host bool cvar
//   mcla_warp_finish_fix (KI-035 finish-race fix, default on) for the A/B
//   control arm; =false changes nothing.
// - mcla_texture_budget_soft_cap_off=true writes 0 (off) to the host integer
//   cvar mcla_texture_budget_soft_cap_mb (M10-007 / KI-048 A/B); =false changes
//   nothing.
// - mcla_album_probe (KI-056 MCLA_ALBUM lines) and mcla_pursuit_probe (KI-066
//   MCLA_PURSUIT lines) are plain host init-only bools (M11-002,
//   docs/evidence/M11-002-diagnostics-triad-20261106.md).
// - mcla_album_preview_restore (KI-056 fix A, default off) is a plain host
//   init-only bool (mcla_album_probe.cpp,
//   docs/evidence/M11-ki056-fix-a-20261106.md).
// - xam_content_stable_creation_time_off=true writes false to the SDK bool
//   cvar xam_content_stable_creation_time (M11-001 / KI-068 stable save
//   creation time, SDK default true; the Deck A/B control arm), and
//   xam_content_trace_off=true writes false to the SDK bool cvar
//   xam_content_trace (KI-068 content trace lines, SDK default true); =false
//   changes nothing. Both are rexruntime cvars, registered before startup.
// - KI-071 cut-scene audio A/B (docs/evidence/M11-ki071-cutscene-audio-analysis-20261106.md):
//   audio_speaker_config_stereo=true and audio_speaker_config_surround=true
//   write stereo or surround to the SDK string cvar audio_speaker_config
//   (default auto: follow the host playback device). Both true in one file is a
//   conflict (neither applied), as for the integer pairs. =false changes
//   nothing. audio_downmix_itu_off=true writes false to the SDK bool cvar
//   audio_downmix_itu (ITU Lo/Ro stereo downmix, default true; the A/B control
//   arm restores the legacy weights). audio_channel_rms_probe is the plain SDK
//   bool (1 Hz per-channel RMS log line). All are rexruntime cvars registered
//   before startup; an SDK without them logs the keys as unavailable.
// - M11-015 read-back sync kick-interval A/B
//   (docs/evidence/M11-cp-cost-plan-20261106.md item 1): readback_kick_256,
//   readback_kick_128 and readback_kick_1024 are bool convenience keys for the
//   integer SDK cvar d3d12_readback_sync_kick_draws (graphics plugin,
//   hot-reload; SDK default 512): =true writes 256, 128 or 1024, =false
//   changes nothing (all three false is the 512 default arm). The three form a
//   conflict group of three pairs: two or more true in one file is
//   contradictory and none of the true ones is applied.
// - l73 owner A/Bs (graphics plugin, hot-reload SDK bools):
//   fetch_identity_off=true writes false to d3d12_fetch_identity_cache
//   (M11-016 fetch-constant identity cache, SDK default true; the A/B control
//   arm, docs/evidence/M11-016-fetch-identity-cache-20261106.md);
//   fetch_identity_verify=true writes true to d3d12_fetch_identity_verify (the
//   cache's verify mode, SDK default false; may be combined with the cache on);
//   readback_steady_exempt=true writes true to
//   d3d12_readback_sync_steady_exempt (M11-007a steady-key exemption, SDK
//   default false until l77, true since the l78 SDK, so the key's true now
//   writes the default; docs/evidence/M11-007a-steady-key-exemption-20261106.md).
//   =false changes nothing for all three. They write three different cvars;
//   the steady key pairs only with its l78 control readback_steady_exempt_off.
// - KI-071 XMA doubling fixes (docs/evidence/M11-ki071-xma-doubling-re-20261106.md,
//   SDK branch mcla/m11-xma-doubling): xma_swap_carry_packet (C1: carry the
//   packet over an input-buffer swap for multi-stream waves),
//   xma_valid_only_when_written (C2), xma_work_lock_recheck (C3) and
//   xma_swap_trace (swap trace lines) are plain SDK rexruntime bools (default
//   false), registered before startup: =true writes true, =false changes
//   nothing (so an SDK default stays visible as the default).
// - KI-071 step 2 (docs/evidence/M11-ki071-xma-sibling-trace-sdk-20261107.md,
//   SDK branch mcla/m11-xma-sibling): xma_sibling_trace (sibling group and
//   MCLA_XMA_SIB lines), xma_stall_on_invalid_next (D2),
//   xma_honor_start_skip (D1) and xma_reset_codec_on_clear are plain SDK
//   rexruntime bools (default false) of the same kind: =true writes true,
//   =false changes nothing. mcla_xma_consume_probe is a plain host init-only
//   bool (MCLA_XMA_CONSUME lines, mcla_xma_consume_probe.cpp,
//   docs/evidence/M11-ki071-xma-consume-probe-20261107.md).
// - KI-071 step 3 (docs/evidence/M11-ki071-xma-frame-trim-sdk-20261107.md,
//   SDK branch mcla/m11-xma-trim; host keys
//   docs/evidence/M11-ki071-xma-dump-keys-20261107.md): xma_honor_frame_trim,
//   xma_frame_trim_block_granular and xma_dump_all_rates are plain SDK
//   rexruntime bools (default false) of the same kind: =true writes true,
//   =false changes nothing. The SDK cvar xma_dump_contexts is a string (a
//   directory), which a name=true|false line cannot carry:
//   xma_dump_contexts_on=true makes the host create the directory
//   <diagnostics>/xma-dump/<yyyyMMdd-HHmmss> (XmaDumpDirectory below; UTC
//   start time) and write its path to xma_dump_contexts, logged once;
//   =false changes nothing.
// - KI-071 step 4 (docs/evidence/M11-ki071-xma-frame-walk-sdk-20261107.md,
//   SDK branch mcla/m11-xma-walk): xma_frame_walk_trace (walk anomaly lines),
//   xma_frame_walk_fix (W1 split-header claim and dup guard) and
//   xma_dump_input (input packet / walk dump next to each PCM dump; needs
//   xma_dump_contexts_on) are plain SDK rexruntime bools (default false) of
//   the same kind: =true writes true, =false changes nothing. Since the l77
//   SDK (f6fc975) xma_frame_walk_fix defaults to true: the key's true now
//   writes the default; the control is xma_frame_walk_fix_off below.
// - M11-007d / KI-047 (src/mcla_lock_probe.cpp): mcla_lock_probe (MCLA_LOCK
//   lines for guest locks overlapping a read-back key) and
//   mcla_lock_readback_ensure (complete a pending read-back copy before the
//   lock returns) are plain host init-only bools: true/false written as is.
// - l77 (docs/evidence/M11-007e-deferred-fences-sdk-20261107.md):
//   fence_write_deferred=true writes true to d3d12_fence_write_deferred
//   (M11-007e deferred EVENT_WRITE_SHD fence writes, graphics plugin,
//   hot-reload, SDK default false until l77, true since the l78 SDK; the
//   control is fence_write_deferred_off below); =false changes nothing.
//   xma_frame_walk_fix_off=true writes false to xma_frame_walk_fix (the
//   KI-071 W1 walk fix, SDK default true since l77; the upstream-walk control
//   arm, before the guest); =false changes nothing. xma_frame_walk_fix=true
//   together with xma_frame_walk_fix_off=true is contradictory (a conflict
//   pair: neither is applied).
// - l77 KI-074 experiments (docs/evidence/M11-ki074-framerate-re-20261107.md
//   sections 5 and 6): raw_timer_off=true writes false to the host init-only
//   mcla_raw_timer (mcla_raw_timer.cpp, read on the first guest timer tick,
//   so the startup write is in time): the stock fixed 1/30 s step, E1,
//   diagnostic only (the game runs about 1.35x fast at 40 fps); =false
//   changes nothing. vblank_hz_72, vblank_hz_80 and vblank_hz_88 write 72,
//   80 or 88 to the SDK int mcla_guest_vblank_hz (graphics plugin,
//   graphics_system.cpp, hot-reload, read on every vblank tick): frame caps
//   of 36, 40 and 44 fps (E2); =false changes nothing. The three form a
//   conflict group like the kick keys. The launcher's Unlocked mode passes
//   --mcla_guest_vblank_hz=240 on the command line, so these three keys are
//   marked overrides_user: they replace a command-line or config value
//   (logged with the value replaced) instead of yielding to it.
// - l78 (docs/evidence/M11-007e-l77-fence-analysis-20261107.md section 5): the
//   l78 SDK turns d3d12_readback_sync_steady_exempt and
//   d3d12_fence_write_deferred on by default and adds the int
//   d3d12_fence_defer_min_gap_us (default 0). readback_steady_exempt_off=true
//   writes false to d3d12_readback_sync_steady_exempt and
//   fence_write_deferred_off=true writes false to d3d12_fence_write_deferred
//   (the A/B controls, graphics plugin, hot-reload); each forms a conflict
//   pair with its l73 / l77 on key (both true: neither applied).
//   fence_defer_gap_2000=true writes 2000 to d3d12_fence_defer_min_gap_us
//   (graphics plugin); =false changes nothing for all three.
// - l79 (docs/evidence/M11-l79-sdk-20261108.md; all four SDK cvars default
//   off, so =false changes nothing): xma_canary_fixes=true writes true to the
//   SDK rexruntime bool xma_canary_fixes (Xenia Canary XMA decoder fixes, read
//   on every XMA Work pass) and timer_queue_blocking_wait=true writes true to
//   the SDK rexruntime bool timer_queue_blocking_wait (the timer queue thread
//   blocks on a high-resolution waitable timer instead of spinning; read on
//   every dispatch pass), both at startup. texture_placed_heaps=true writes
//   true to d3d12_texture_placed_heaps and texture_heap_64mb=true writes 64 to
//   the int d3d12_texture_heap_block_mb (256 by default); both are registered
//   by the graphics plugin and are restart-only: D3D12TextureCache::Initialize
//   latches them when the runtime sets up the guest GPU, which follows
//   OnPostLoadGraphicsPlugin (ReXApp::SetupPresentation precedes
//   ConstructRuntime), so the graphics-plugin phase is in time.
//   texture_heap_64mb alone (without texture_placed_heaps) has no effect.
//   Since the l80 SDK the platform defaults turn xma_canary_fixes and
//   timer_queue_blocking_wait on everywhere and d3d12_texture_placed_heaps on
//   on Windows (off under Wine/Proton), so these keys' true then writes the
//   default there; the controls are the l80 keys below.
// - l80 (docs/evidence/M11-l80-sdk-20261108.md): blend_minmax_fix=true writes
//   true to the SDK bool d3d12_blend_minmax_fix (KI-013 MIN/MAX blend source
//   factor, default false; graphics plugin, read for every pipeline's pixel
//   shader modification, so a new modification key compiles new pipelines).
//   short_sleep_native=true writes native to the SDK string
//   ke_delay_short_sleep_mode (KI-060, latched at the first guest delay;
//   platform default native on Windows, hires_timer under Wine/Proton) and
//   apu_priority_normal=true writes normal to the SDK string
//   apu_worker_priority (read when the audio worker and XMA decoder threads
//   are created; platform default normal on Windows, above_normal under
//   Wine/Proton), both at startup: the Deck control arm for the l80 KI-060
//   defaults. texture_placed_heaps_off=true writes false to
//   d3d12_texture_placed_heaps (graphics plugin, restart-only like
//   texture_placed_heaps) and timer_blocking_off=true writes false to
//   timer_queue_blocking_wait (startup): the controls for the l80 platform
//   defaults; each forms a conflict pair with its l79 on key (both true:
//   neither applied). =false changes nothing for all five.
// - Conflict rule (all groups): every key of a true-true kConflictPairs pair
//   (evaluated on the file as read, so a group of three is resolved as a
//   whole) loses its value; one kConflict issue is logged for each of those
//   lines except the earliest of its group.
//
// Each accepted value is applied to its cvar unless that cvar already holds a
// non-default value (set by the command line or the config file; keys marked
// overrides_user replace that value instead), and logged once as
// "MCLA dev override: name=value". Host init-only cvars are applied at
// startup (before the guest runs); SDK cvars of the graphics plugin are applied
// when the plugin has been loaded (they are registered by the plugin DLL).
// Without the option nothing is read or written, and the single-line l55 file
// read of mcla_readback_sync.cpp stays as before.
//
// Phase::kLauncher keys are not cvars: the host parses and ignores them; the
// launcher (MCLA_DEV_OVERRIDES launcher builds) reads the same file before a
// session and applies them itself (prewarm_seed_store=false: no shader store
// seeding, for cold baselines).
//
// The parser and the apply decision are pure and unit-tested
// (tests/mcla_dev_overrides_test.cpp); file and cvar calls are in
// mcla_dev_overrides.cpp.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace mcla::dev_overrides {

inline constexpr std::string_view kDirectory = "MCLA-R-Launcher-v1";
inline constexpr std::string_view kFile = "mcla-dev-overrides.txt";
// 2 KiB since l73: a full 35-key arm file (every key explicit) is about
// 1.07 KB, beyond the former 1 KiB (a full 59-key arm is about 1.7 KB, a full
// 66-key l79 arm about 1.9 KB, a full 71-key l80 arm about 2.02 KB: close to
// the limit; later arms drop redundant =false lines rather than raise it).
inline constexpr size_t kMaxBytes = 2048;

enum class Key : uint8_t {
  kReadbackSyncComposite,
  kNativeD3dRefcount,
  kNativeXapiInterlocked,
  kWrmEventWake,
  kReplayBatchedWake,
  kReadbackElideUnread,
  kGpuBackendVulkan,  // M10-004 Deck A/B: native Vulkan for the session
  kPrewarmSeedStore,  // M10-003: launcher shader store seeding (default on)
  kReadbackSyncTargeted,  // KI-047 cost: targeted read-back wait + early submit
  kWatchCensus,           // M10 watch census counters (SDK default on)
  kWatchProbes,           // M10 watch F10 probes (SDK default off)
  kWatchDisarmBridge,     // M10 step 1: disarm safe-gap bridging
  kUploadAhead16,         // M10 step 2: shared_memory_upload_ahead_pages=16
  kUploadAhead64,         // M10 step 2: shared_memory_upload_ahead_pages=64
  kHotBlocksShadow,       // M10 step 3: shared_memory_hot_blocks=1 (shadow)
  kHotBlocksOn,           // M10 step 3: shared_memory_hot_blocks=2 (on)
  kWarpProbe,             // KI-035: warp state / camera logging
  kWarpWatchdog,          // KI-035: mcla_warp_watchdog_seconds=15
  kTextureSoftCapOff,     // M10-007: mcla_texture_budget_soft_cap_mb=0
  kWarpFinishFixOff,      // KI-035 fix A/B: mcla_warp_finish_fix=false
  kAlbumProbe,            // M11-002 / KI-056: album preview diagnostic
  kPursuitProbe,          // M11-002 / KI-066: police pursuit transition log
  kStableCreationTimeOff, // KI-068 A/B: xam_content_stable_creation_time=false
  kContentTraceOff,       // KI-068: xam_content_trace=false
  kAudioSpeakerStereo,    // KI-071 A/B: audio_speaker_config=stereo
  kAudioSpeakerSurround,  // KI-071 A/B: audio_speaker_config=surround
  kAudioDownmixItuOff,    // KI-071 A/B: audio_downmix_itu=false (legacy weights)
  kAudioChannelRmsProbe,  // KI-071: audio_channel_rms_probe (1 Hz RMS line)
  kAlbumPreviewRestore,   // KI-056 fix A: mcla_album_preview_restore (default off)
  kReadbackKick256,       // M11-015 A/B: d3d12_readback_sync_kick_draws=256
  kReadbackKick128,       // M11-015 A/B: d3d12_readback_sync_kick_draws=128
  kReadbackKick1024,      // M11-015 A/B control: d3d12_readback_sync_kick_draws=1024
  kFetchIdentityOff,      // M11-016 A/B control: d3d12_fetch_identity_cache=false
  kFetchIdentityVerify,   // M11-016: d3d12_fetch_identity_verify=true
  kReadbackSteadyExempt,  // M11-007a A/B: d3d12_readback_sync_steady_exempt=true
  kXmaSwapCarryPacket,    // KI-071 C1: xma_swap_carry_packet=true
  kXmaValidOnlyWhenWritten,  // KI-071 C2: xma_valid_only_when_written=true
  kXmaWorkLockRecheck,    // KI-071 C3: xma_work_lock_recheck=true
  kXmaSwapTrace,          // KI-071: xma_swap_trace=true (swap trace lines)
  kXmaSiblingTrace,       // KI-071 step 2: xma_sibling_trace=true (group lines)
  kXmaStallOnInvalidNext,  // KI-071 D2: xma_stall_on_invalid_next=true
  kXmaHonorStartSkip,     // KI-071 D1: xma_honor_start_skip=true
  kXmaResetCodecOnClear,  // KI-071: xma_reset_codec_on_clear=true
  kXmaConsumeProbe,       // KI-071: mcla_xma_consume_probe (host, MCLA_XMA_CONSUME)
  kXmaHonorFrameTrim,     // KI-071 step 3: xma_honor_frame_trim=true
  kXmaFrameTrimBlockGranular,  // KI-071 step 3: xma_frame_trim_block_granular=true
  kXmaDumpAllRates,       // KI-071 step 3: xma_dump_all_rates=true
  kXmaDumpContextsOn,     // KI-071 step 3: xma_dump_contexts=<diagnostics>/xma-dump/<stamp>
  kXmaFrameWalkTrace,     // KI-071 step 4: xma_frame_walk_trace=true (walk lines)
  kXmaFrameWalkFix,       // KI-071 step 4: xma_frame_walk_fix=true (W1 fix)
  kXmaDumpInput,          // KI-071 step 4: xma_dump_input=true (input/walk dump)
  kLockProbe,             // M11-007d: mcla_lock_probe (host, MCLA_LOCK)
  kLockReadbackEnsure,    // M11-007d: mcla_lock_readback_ensure (host)
  kFenceWriteDeferred,    // M11-007e: d3d12_fence_write_deferred=true
  kXmaFrameWalkFixOff,    // KI-071 control: xma_frame_walk_fix=false (SDK default true)
  kRawTimerOff,           // KI-074 E1: mcla_raw_timer=false (stock fixed 1/30 s step)
  kVblankHz72,            // KI-074 E2: mcla_guest_vblank_hz=72 (36 fps cap)
  kVblankHz80,            // KI-074 E2: mcla_guest_vblank_hz=80 (40 fps cap)
  kVblankHz88,            // KI-074 E2: mcla_guest_vblank_hz=88 (44 fps cap)
  kReadbackSteadyExemptOff,  // l78 control: d3d12_readback_sync_steady_exempt=false
  kFenceWriteDeferredOff,    // l78 control: d3d12_fence_write_deferred=false
  kFenceDeferGap2000,        // l78 A/B: d3d12_fence_defer_min_gap_us=2000
  kXmaCanaryFixes,           // l79 A: xma_canary_fixes=true (Xenia Canary XMA fixes)
  kTimerQueueBlockingWait,   // l79 C: timer_queue_blocking_wait=true
  kTexturePlacedHeaps,       // l79 B: d3d12_texture_placed_heaps=true (restart-only)
  kTextureHeap64Mb,          // l79 B: d3d12_texture_heap_block_mb=64 (restart-only)
  kBlendMinmaxFix,           // l80 KI-013: d3d12_blend_minmax_fix=true
  kShortSleepNative,         // l80 KI-060 control: ke_delay_short_sleep_mode=native
  kApuPriorityNormal,        // l80 KI-060 control: apu_worker_priority=normal
  kTexturePlacedHeapsOff,    // l80 control: d3d12_texture_placed_heaps=false
  kTimerBlockingOff,         // l80 control: timer_queue_blocking_wait=false
};
inline constexpr size_t kKeyCount = 71;

// When the cvar can be written: host init-only cvars before the guest starts,
// graphics-plugin cvars once the plugin has registered them. Launcher keys are
// read by the launcher only (no host cvar).
enum class Phase : uint8_t {
  kStartup,
  kGraphicsPlugin,
  kLauncher,
};

// Where the text written for true comes from: the literal true_text, or (one
// key) the per-session XMA dump directory composed by the runtime
// (XmaDumpDirectory), which a fixed text cannot express.
enum class TextSource : uint8_t {
  kLiteral,
  kXmaDumpDirectory,
};

struct KeyInfo {
  std::string_view name;
  Phase phase;
  // The cvar written (empty: the key's own name) and the text written for
  // true and false (empty: nothing is written for that value).
  std::string_view cvar = {};
  std::string_view true_text = "true";
  std::string_view false_text = "false";
  TextSource true_source = TextSource::kLiteral;
  // The value replaces a command-line or config value of the cvar (the
  // launcher's own --mcla_guest_vblank_hz) instead of yielding to it.
  bool overrides_user = false;
};

// The whitelist, indexed by Key.
inline constexpr std::array<KeyInfo, kKeyCount> kKeys = {{
    {"mcla_readback_sync_composite", Phase::kStartup},
    {"mcla_native_d3d_refcount", Phase::kStartup},
    {"mcla_native_xapi_interlocked", Phase::kStartup},
    {"d3d12_wrm_event_wake", Phase::kGraphicsPlugin},
    {"d3d12_replay_batched_wake", Phase::kGraphicsPlugin},
    {"d3d12_readback_elide_unread", Phase::kGraphicsPlugin},
    {"mcla_gpu_backend_vulkan", Phase::kStartup},
    {"prewarm_seed_store", Phase::kLauncher},
    {"d3d12_readback_sync_targeted", Phase::kGraphicsPlugin},
    // rexruntime cvars, registered before startup.
    {"memory_watch_census", Phase::kStartup},
    {"memory_watch_probes", Phase::kStartup},
    {"memory_watch_disarm_bridge_safe_gaps", Phase::kStartup},
    // Registered by the graphics plugin (shared memory).
    {"shared_memory_upload_ahead_16", Phase::kGraphicsPlugin, "shared_memory_upload_ahead_pages",
     "16", ""},
    {"shared_memory_upload_ahead_64", Phase::kGraphicsPlugin, "shared_memory_upload_ahead_pages",
     "64", ""},
    {"shared_memory_hot_blocks_shadow", Phase::kGraphicsPlugin, "shared_memory_hot_blocks", "1",
     ""},
    {"shared_memory_hot_blocks_on", Phase::kGraphicsPlugin, "shared_memory_hot_blocks", "2", ""},
    // KI-035 host init-only cvars (mcla_ki026_probe.cpp), before the guest.
    {"mcla_warp_probe", Phase::kStartup},
    {"mcla_warp_watchdog", Phase::kStartup, "mcla_warp_watchdog_seconds", "15", ""},
    // M10-007 host init-only cvar (mcla_texture_budget_win.cpp), read at
    // graphics-plugin load, so the startup phase is early enough.
    {"mcla_texture_budget_soft_cap_off", Phase::kStartup, "mcla_texture_budget_soft_cap_mb", "0",
     ""},
    // KI-035 finish-race fix (host init-only bool, default true), before the
    // guest.
    {"mcla_warp_finish_fix_off", Phase::kStartup, "mcla_warp_finish_fix", "false", ""},
    // M11-002 host init-only bools (mcla_album_probe.cpp,
    // mcla_pursuit_probe.cpp), before the guest.
    {"mcla_album_probe", Phase::kStartup},
    {"mcla_pursuit_probe", Phase::kStartup},
    // M11-001 / KI-068 SDK rexruntime bools (default true), before the guest.
    {"xam_content_stable_creation_time_off", Phase::kStartup, "xam_content_stable_creation_time",
     "false", ""},
    {"xam_content_trace_off", Phase::kStartup, "xam_content_trace", "false", ""},
    // KI-071 SDK rexruntime audio cvars, before the guest (the speaker config
    // is read at the title's first XAudioGetSpeakerConfig call).
    {"audio_speaker_config_stereo", Phase::kStartup, "audio_speaker_config", "stereo", ""},
    {"audio_speaker_config_surround", Phase::kStartup, "audio_speaker_config", "surround", ""},
    {"audio_downmix_itu_off", Phase::kStartup, "audio_downmix_itu", "false", ""},
    {"audio_channel_rms_probe", Phase::kStartup},
    // KI-056 fix A host init-only bool (mcla_album_probe.cpp), before the
    // guest.
    {"mcla_album_preview_restore", Phase::kStartup},
    // M11-015 kick-interval A/B: the integer SDK cvar is registered by the
    // graphics plugin (command_processor.cpp, hot-reload), so after the plugin
    // has loaded.
    {"readback_kick_256", Phase::kGraphicsPlugin, "d3d12_readback_sync_kick_draws", "256", ""},
    {"readback_kick_128", Phase::kGraphicsPlugin, "d3d12_readback_sync_kick_draws", "128", ""},
    {"readback_kick_1024", Phase::kGraphicsPlugin, "d3d12_readback_sync_kick_draws", "1024", ""},
    // l73 owner A/Bs: SDK bools registered by the graphics plugin
    // (command_processor.cpp, hot-reload), so after the plugin has loaded.
    {"fetch_identity_off", Phase::kGraphicsPlugin, "d3d12_fetch_identity_cache", "false", ""},
    {"fetch_identity_verify", Phase::kGraphicsPlugin, "d3d12_fetch_identity_verify", "true", ""},
    {"readback_steady_exempt", Phase::kGraphicsPlugin, "d3d12_readback_sync_steady_exempt", "true",
     ""},
    // KI-071 XMA doubling fixes: SDK rexruntime bools (default false), before
    // the guest; =false writes nothing.
    {"xma_swap_carry_packet", Phase::kStartup, "", "true", ""},
    {"xma_valid_only_when_written", Phase::kStartup, "", "true", ""},
    {"xma_work_lock_recheck", Phase::kStartup, "", "true", ""},
    {"xma_swap_trace", Phase::kStartup, "", "true", ""},
    // KI-071 step 2: SDK rexruntime bools (default false), before the guest;
    // =false writes nothing.
    {"xma_sibling_trace", Phase::kStartup, "", "true", ""},
    {"xma_stall_on_invalid_next", Phase::kStartup, "", "true", ""},
    {"xma_honor_start_skip", Phase::kStartup, "", "true", ""},
    {"xma_reset_codec_on_clear", Phase::kStartup, "", "true", ""},
    // KI-071 guest consume probe: host init-only bool
    // (mcla_xma_consume_probe.cpp), before the guest.
    {"mcla_xma_consume_probe", Phase::kStartup},
    // KI-071 step 3: SDK rexruntime bools (default false), before the guest;
    // =false writes nothing.
    {"xma_honor_frame_trim", Phase::kStartup, "", "true", ""},
    {"xma_frame_trim_block_granular", Phase::kStartup, "", "true", ""},
    {"xma_dump_all_rates", Phase::kStartup, "", "true", ""},
    // The SDK string cvar xma_dump_contexts (default empty, rexruntime):
    // =true writes the directory the host composed and created (true_text is
    // only the description); =false writes nothing.
    {"xma_dump_contexts_on", Phase::kStartup, "xma_dump_contexts",
     "<diagnostics>/xma-dump/<yyyyMMdd-HHmmss>", "", TextSource::kXmaDumpDirectory},
    // KI-071 step 4: SDK rexruntime bools (default false), before the guest;
    // =false writes nothing.
    {"xma_frame_walk_trace", Phase::kStartup, "", "true", ""},
    {"xma_frame_walk_fix", Phase::kStartup, "", "true", ""},
    {"xma_dump_input", Phase::kStartup, "", "true", ""},
    // M11-007d lock probe and read-back completion at the lock: host
    // init-only bools (mcla_lock_probe.cpp), before the guest.
    {"mcla_lock_probe", Phase::kStartup},
    {"mcla_lock_readback_ensure", Phase::kStartup},
    // l77: M11-007e deferred fence writes, an SDK bool registered by the
    // graphics plugin (command_processor.cpp, hot-reload), so after the
    // plugin has loaded; =false writes nothing.
    {"fence_write_deferred", Phase::kGraphicsPlugin, "d3d12_fence_write_deferred", "true", ""},
    // l77: the KI-071 walk fix control (SDK rexruntime bool, default true
    // since l77), before the guest; =false writes nothing.
    {"xma_frame_walk_fix_off", Phase::kStartup, "xma_frame_walk_fix", "false", ""},
    // l77 KI-074 E1: the host init-only raw timer switch (default on in owner
    // builds), before the guest; =false writes nothing.
    {"raw_timer_off", Phase::kStartup, "mcla_raw_timer", "false", ""},
    // l77 KI-074 E2: frame caps through the SDK int registered by the graphics
    // plugin (hot-reload), after the plugin has loaded; =false writes nothing.
    // They win over the launcher's Unlocked --mcla_guest_vblank_hz=240.
    {"vblank_hz_72", Phase::kGraphicsPlugin, "mcla_guest_vblank_hz", "72", "",
     TextSource::kLiteral, true},
    {"vblank_hz_80", Phase::kGraphicsPlugin, "mcla_guest_vblank_hz", "80", "",
     TextSource::kLiteral, true},
    {"vblank_hz_88", Phase::kGraphicsPlugin, "mcla_guest_vblank_hz", "88", "",
     TextSource::kLiteral, true},
    // l78: controls for the two SDK defaults turned on in l78 (steady-key
    // exemption, deferred fence writes) and the fence deferral minimum gap,
    // all registered by the graphics plugin (hot-reload), so after the plugin
    // has loaded; =false writes nothing.
    {"readback_steady_exempt_off", Phase::kGraphicsPlugin, "d3d12_readback_sync_steady_exempt",
     "false", ""},
    {"fence_write_deferred_off", Phase::kGraphicsPlugin, "d3d12_fence_write_deferred", "false",
     ""},
    {"fence_defer_gap_2000", Phase::kGraphicsPlugin, "d3d12_fence_defer_min_gap_us", "2000", ""},
    // l79: SDK rexruntime bools (default false), read on every pass, before
    // the guest; =false writes nothing.
    {"xma_canary_fixes", Phase::kStartup, "", "true", ""},
    {"timer_queue_blocking_wait", Phase::kStartup, "", "true", ""},
    // l79: placed texture heaps, restart-only SDK cvars registered by the
    // graphics plugin and latched by D3D12TextureCache::Initialize (guest GPU
    // setup, after the plugin-load phase); =false writes nothing.
    {"texture_placed_heaps", Phase::kGraphicsPlugin, "d3d12_texture_placed_heaps", "true", ""},
    {"texture_heap_64mb", Phase::kGraphicsPlugin, "d3d12_texture_heap_block_mb", "64", ""},
    // l80 KI-013: SDK bool registered by the graphics plugin (pipeline cache),
    // read per pixel shader modification; =false writes nothing.
    {"blend_minmax_fix", Phase::kGraphicsPlugin, "d3d12_blend_minmax_fix", "true", ""},
    // l80 KI-060 controls: SDK rexruntime strings, latched at the first guest
    // delay / when the audio threads are created, so before the guest;
    // =false writes nothing.
    {"short_sleep_native", Phase::kStartup, "ke_delay_short_sleep_mode", "native", ""},
    {"apu_priority_normal", Phase::kStartup, "apu_worker_priority", "normal", ""},
    // l80 controls for the platform defaults: placed heaps (graphics plugin,
    // restart-only) and the timer queue blocking wait (rexruntime, startup);
    // =false writes nothing.
    {"texture_placed_heaps_off", Phase::kGraphicsPlugin, "d3d12_texture_placed_heaps", "false",
     ""},
    {"timer_blocking_off", Phase::kStartup, "timer_queue_blocking_wait", "false", ""},
}};

// Key pairs that write different values to one cvar (integer, the
// audio_speaker_config string, or the xma_frame_walk_fix,
// d3d12_readback_sync_steady_exempt, d3d12_fence_write_deferred,
// d3d12_texture_placed_heaps and timer_queue_blocking_wait bools):
// both true in one file is contradictory and
// neither is applied. A group of three keys for one cvar is listed as its
// three pairs (first < second in Key order); see ResolveConflicts.
struct ConflictPair {
  Key first;
  Key second;
};
inline constexpr std::array<ConflictPair, 14> kConflictPairs = {{
    {Key::kUploadAhead16, Key::kUploadAhead64},
    {Key::kHotBlocksShadow, Key::kHotBlocksOn},
    {Key::kAudioSpeakerStereo, Key::kAudioSpeakerSurround},
    {Key::kReadbackKick256, Key::kReadbackKick128},
    {Key::kReadbackKick256, Key::kReadbackKick1024},
    {Key::kReadbackKick128, Key::kReadbackKick1024},
    {Key::kXmaFrameWalkFix, Key::kXmaFrameWalkFixOff},
    {Key::kVblankHz72, Key::kVblankHz80},
    {Key::kVblankHz72, Key::kVblankHz88},
    {Key::kVblankHz80, Key::kVblankHz88},
    {Key::kReadbackSteadyExempt, Key::kReadbackSteadyExemptOff},
    {Key::kFenceWriteDeferred, Key::kFenceWriteDeferredOff},
    {Key::kTimerQueueBlockingWait, Key::kTimerBlockingOff},
    {Key::kTexturePlacedHeaps, Key::kTexturePlacedHeapsOff},
}};

constexpr const KeyInfo &Info(Key key) {
  return kKeys[static_cast<size_t>(key)];
}

// The cvar a key writes.
constexpr std::string_view CvarName(const KeyInfo &info) {
  return info.cvar.empty() ? info.name : info.cvar;
}

// The text written to the cvar for a value; empty: nothing is written.
constexpr std::string_view CvarText(const KeyInfo &info, bool value) {
  return value ? info.true_text : info.false_text;
}

// KI-071 step 3 dump directory. The diagnostics root is the one the
// diagnostics manager uses (mcla_diagnostics_root; empty = <user-data>/
// diagnostics). The launcher passes <profile>/diagnostics/<session-id> there,
// so a managed session's dumps sit in its own session folder; a direct start
// uses <user-data>/diagnostics. The UTC start stamp keeps sessions that share
// a root apart (the SDK numbers its files per session from 0).
inline constexpr std::string_view kXmaDumpSubdirectory = "xma-dump";

// "yyyyMMdd-HHmmss"; empty when a field is out of range.
inline std::string FormatDumpStamp(int year, int month, int day, int hour, int minute,
                                   int second) {
  if (year < 1970 || year > 9999 || month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 ||
      hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 60) {
    return {};
  }
  std::string out;
  const auto digits = [&out](int value, int width) {
    char buffer[4] = {};
    for (int i = width - 1; i >= 0; --i) {
      buffer[i] = static_cast<char>('0' + value % 10);
      value /= 10;
    }
    out.append(buffer, static_cast<size_t>(width));
  };
  digits(year, 4);
  digits(month, 2);
  digits(day, 2);
  out.push_back('-');
  digits(hour, 2);
  digits(minute, 2);
  digits(second, 2);
  return out;
}

// <diagnostics root>/xma-dump/<stamp>; empty when the stamp is empty or no
// root is known (both roots empty).
inline std::filesystem::path XmaDumpDirectory(const std::filesystem::path &user_data_root,
                                              const std::filesystem::path &diagnostics_root,
                                              std::string_view stamp) {
  if (stamp.empty() || (diagnostics_root.empty() && user_data_root.empty())) {
    return {};
  }
  const std::filesystem::path root =
      diagnostics_root.empty() ? user_data_root / "diagnostics" : diagnostics_root;
  return root / std::filesystem::path(kXmaDumpSubdirectory) / std::filesystem::path(stamp);
}

constexpr std::optional<Key> FindKey(std::string_view name) {
  for (size_t i = 0; i < kKeyCount; ++i) {
    if (kKeys[i].name == name) {
      return static_cast<Key>(i);
    }
  }
  return std::nullopt;
}

enum class FileStatus : uint8_t {
  kAbsent,    // no file
  kRejected,  // too large, not ASCII or unreadable: nothing applied
  kParsed,    // read line by line (individual lines may be ignored)
};

enum class LineIssue : uint8_t {
  kMalformed,    // not exactly name=true or name=false
  kUnknownName,  // well formed, name not in kKeys
  kDuplicate,    // a name already given on an earlier line
  kConflict,     // contradicts an earlier line (two keys of one cvar true, kConflictPairs)
};

struct Issue {
  uint16_t line = 0;  // 1-based
  LineIssue issue = LineIssue::kMalformed;
};

inline constexpr size_t kMaxIssues = 16;

struct Parsed {
  FileStatus status = FileStatus::kAbsent;
  std::array<std::optional<bool>, kKeyCount> values{};
  std::array<uint16_t, kKeyCount> lines{};  // line of each value (1-based)
  std::array<Issue, kMaxIssues> issues{};
  size_t issue_count = 0;    // issues stored (at most kMaxIssues)
  size_t ignored_lines = 0;  // every ignored line, also beyond kMaxIssues

  constexpr std::optional<bool> Value(Key key) const {
    return values[static_cast<size_t>(key)];
  }
  constexpr size_t ValueCount() const {
    size_t count = 0;
    for (const auto &value : values) {
      count += value.has_value() ? 1 : 0;
    }
    return count;
  }
};

namespace detail {

constexpr void AddIssue(Parsed &parsed, uint16_t line, LineIssue issue) {
  if (parsed.issue_count < kMaxIssues) {
    parsed.issues[parsed.issue_count++] = Issue{line, issue};
  }
  ++parsed.ignored_lines;
}

constexpr void ParseLine(Parsed &parsed, uint16_t number, std::string_view line) {
  if (line.ends_with('\r')) {
    line.remove_suffix(1);
  }
  if (line.empty() || line.front() == '#') {
    return;
  }
  size_t equals = std::string_view::npos;
  for (size_t i = 0; i < line.size(); ++i) {
    const auto c = static_cast<unsigned char>(line[i]);
    if (c < 0x21 || c > 0x7E) {
      AddIssue(parsed, number, LineIssue::kMalformed);
      return;
    }
    if (c == '=') {
      if (equals != std::string_view::npos) {
        AddIssue(parsed, number, LineIssue::kMalformed);
        return;
      }
      equals = i;
    }
  }
  if (equals == std::string_view::npos || equals == 0) {
    AddIssue(parsed, number, LineIssue::kMalformed);
    return;
  }
  const std::string_view name = line.substr(0, equals);
  const std::string_view text = line.substr(equals + 1);
  bool value = false;
  if (text == "true") {
    value = true;
  } else if (text != "false") {
    AddIssue(parsed, number, LineIssue::kMalformed);
    return;
  }
  const std::optional<Key> key = FindKey(name);
  if (!key) {
    AddIssue(parsed, number, LineIssue::kUnknownName);
    return;
  }
  auto &slot = parsed.values[static_cast<size_t>(*key)];
  if (slot.has_value()) {
    AddIssue(parsed, number, LineIssue::kDuplicate);
    return;
  }
  slot = value;
  parsed.lines[static_cast<size_t>(*key)] = number;
}

// Two keys of a conflict pair true: none of the keys in a true-true pair
// applies. Pairs are evaluated on the values as parsed (before any reset), so
// a group of three (listed as its three pairs) is dropped as a whole when two
// or more of its keys are true. One conflict is logged on each dropped line
// that has an earlier-line partner in a true-true pair (for a pair: the later
// line), in line order.
constexpr void ResolveConflicts(Parsed &parsed) {
  std::array<bool, kKeyCount> dropped{};
  std::array<bool, kKeyCount> logged{};
  for (const ConflictPair &pair : kConflictPairs) {
    const size_t a = static_cast<size_t>(pair.first);
    const size_t b = static_cast<size_t>(pair.second);
    if (parsed.values[a] == true && parsed.values[b] == true) {
      dropped[a] = true;
      dropped[b] = true;
      logged[parsed.lines[a] > parsed.lines[b] ? a : b] = true;
    }
  }
  uint16_t previous = 0;
  for (;;) {
    // The next logged line after `previous` (lines are unique per key).
    uint16_t next = 0;
    for (size_t i = 0; i < kKeyCount; ++i) {
      if (logged[i] && parsed.lines[i] > previous && (next == 0 || parsed.lines[i] < next)) {
        next = parsed.lines[i];
      }
    }
    if (next == 0) {
      break;
    }
    AddIssue(parsed, next, LineIssue::kConflict);
    previous = next;
  }
  for (size_t i = 0; i < kKeyCount; ++i) {
    if (dropped[i]) {
      parsed.values[i].reset();
    }
  }
}

}  // namespace detail

// Strict parse of the whole file content (see the rules above).
constexpr Parsed Parse(std::string_view content) {
  Parsed parsed;
  if (content.size() > kMaxBytes) {
    parsed.status = FileStatus::kRejected;
    return parsed;
  }
  for (const char c : content) {
    if (static_cast<unsigned char>(c) > 0x7F) {
      parsed.status = FileStatus::kRejected;
      return parsed;
    }
  }
  parsed.status = FileStatus::kParsed;
  uint16_t number = 0;
  while (!content.empty()) {
    ++number;
    const size_t end = content.find('\n');
    if (end == std::string_view::npos) {
      detail::ParseLine(parsed, number, content);
      break;
    }
    detail::ParseLine(parsed, number, content.substr(0, end));
    content.remove_prefix(end + 1);
  }
  detail::ResolveConflicts(parsed);
  return parsed;
}

constexpr const char *LineIssueName(LineIssue issue) {
  switch (issue) {
    case LineIssue::kMalformed:
      return "malformed (expected name=true or name=false)";
    case LineIssue::kUnknownName:
      return "unknown name";
    case LineIssue::kDuplicate:
      return "repeated name (the first line wins)";
    case LineIssue::kConflict:
      return "conflicting keys for one cvar (neither is applied)";
  }
  return "unknown";
}

enum class ApplyAction : uint8_t {
  kApply,          // write the cvar
  kUnavailable,    // the cvar is not registered (e.g. an SDK without it)
  kUserSet,        // the command line or config set the cvar: left alone
  kReplaceUserSet, // the command line or config set the cvar; an
                   // overrides_user key replaces that value
};

constexpr ApplyAction DecideApply(bool cvar_registered, bool cvar_non_default,
                                  bool overrides_user = false) {
  if (!cvar_registered) {
    return ApplyAction::kUnavailable;
  }
  if (cvar_non_default) {
    return overrides_user ? ApplyAction::kReplaceUserSet : ApplyAction::kUserSet;
  }
  return ApplyAction::kApply;
}

// Runtime (mcla_dev_overrides.cpp). Both calls do nothing in builds without
// MCLA_DEV_OVERRIDES.

// True in builds made with MCLA_DEV_OVERRIDES.
bool Enabled();

// Reads and parses the file once and applies the Phase::kStartup keys. Call
// after logging is set up and before the guest starts. The roots are those of
// the diagnostics manager (XmaDumpDirectory; xma_dump_contexts_on only).
void OnStartup(const std::filesystem::path &user_data_root,
               const std::filesystem::path &diagnostics_root);

// Applies the Phase::kGraphicsPlugin keys. Call once after the graphics plugin
// has been loaded.
void OnGraphicsPluginLoaded();

// The value written to the key's cvar by this module (nullopt when the file
// has no valid line for it, the cvar was left alone, or the write failed).
std::optional<bool> AppliedValue(Key key);

// The value of the key's valid file line (nullopt without the option, without
// a parsed file or without a valid line), whether or not anything was
// written. Used for keys whose value writes nothing (upload-ahead, hot blocks,
// read-back kick, the l73 fetch-identity / steady-exempt and the KI-071 XMA
// SDK keys =false, also xma_dump_contexts_on=false).
std::optional<bool> FileValue(Key key);

}  // namespace mcla::dev_overrides
