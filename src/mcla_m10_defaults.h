#pragma once

// M10 volunteer speed-up defaults (l65;
// docs/evidence/M10-volunteer-speedup-defaults-20261102.md, measured in the
// owner's l63 PC A/B, docs/evidence/M10-l63-pc-ab-20261102.md; Wine/Proton
// entries l66, docs/evidence/M10-proton-defaults-and-ki064-20261103.md,
// measured in the owner's l64 Deck A/B, docs/evidence/M10-l64-deck-ab-20261103.md).
//
// Builds made with the CMake option MCLA_M10_SPEEDUP_DEFAULTS (OFF by
// default) write the SDK switches of kDefaults below, so a volunteer build
// ships the speed-ups that the owner's A/B proved without the owner-internal
// override file:
//
// - at startup (rexruntime cvars, after dev_overrides::OnStartup):
//   memory_watch_census=false (counters nobody reads in volunteer logs);
//   under Wine/Proton only: memory_watch_disarm_bridge_safe_gaps=true;
// - at graphics-plugin load (after dev_overrides::OnGraphicsPluginLoaded,
//   the KI-047 composite sync and the readback_resolve=some default):
//   d3d12_readback_sync_targeted=true, d3d12_readback_elide_unread=true,
//   d3d12_wrm_event_wake=true, d3d12_replay_batched_wake=true,
//   d3d12_readback_gpu_timing=false; under Wine/Proton only:
//   shared_memory_upload_ahead_pages=16; since l94 on both platforms also
//   d3d12_frame_lead_strict=1 (KI-074 fix, see below).
//
// Each phase is the phase of the owner override key for the same cvar.
//
// A value is written only when the cvar is registered, still holds its
// default (the command line or config did not change it), and the owner
// override file (MCLA_DEV_OVERRIDES builds) did not write it: owner A/B arms
// and explicit user values always win. For upload-ahead, an override line
// shared_memory_upload_ahead_16=false or _64=false writes nothing itself but
// is the owner's "off" arm, so it also wins. The two diagnostic switches
// (memory_watch_census, d3d12_readback_gpu_timing) are left at the SDK default
// in MCLA_DEV_OVERRIDES (owner measurement) builds. The host switch
// --mcla_m10_speedup_defaults=false turns the whole set off for a session.
//
// kNeverWrittenCvars lists switches this module must not write: diagnostics,
// switches owned by other host modules (also the KI-035 warp probe and
// watchdog, owner builds only), the Vulkan backend, and the SDK hot
// blocks (M10 watch step 3, docs/evidence/M10-hot-blocks-host-keys-20261103.md)
// until an owner A/B accepts them; until then only the owner override keys
// shared_memory_hot_blocks_shadow/_on set shared_memory_hot_blocks.
//
// Wine-only entries (Platform::kWineOnly): disarm bridging and upload-ahead 16
// were the two largest Deck gains in l64 (upload-ahead CP -8.3 ms/frame,
// bridging -3.3 ms/frame) but net negative on Windows in l63, so on Windows
// they are never written. platform::RunningUnderWine() decides; the summary
// lines say platform=wine|windows. The host init-only switches
// mcla_native_d3d_refcount and mcla_native_xapi_interlocked already default to
// true (mcla_native_refcount.cpp) and are only reported.
//
// l94 KI-074 (owner decision 2026-11-12, "default A, build 94"; owner A/B at
// night, by day and at the 06:00 dawn switch,
// docs/evidence/M11-l93-owner-build-20261112.md): d3d12_frame_lead_strict=1, the
// strict frame lead bound (a guest frame opens only when the previous one has
// completed on the GPU), is written here because the SDK default stays 0 (the
// l92 SDK is not rebuilt). On both platforms: it is a correctness fix for a
// host-side pacing state (the CP two frames ahead of the GPU) that does not
// depend on the OS; the Wine-only rule above is for switches measured net
// negative on Windows, and this one is net positive there; it costs time
// only while the GPU is saturated. Owner controls: frame_lead_strict_off
// (writes 0, the old behaviour) and frame_lead_adaptive (writes 2) win as
// applied override values; a =false line of the three lead keys writes
// nothing and does not turn the default off (silent_false_is_arm false; only
// the upload-ahead =false lines are the owner's off arm).
//
// Semantics kept from the SDK: targeted sync acts only while
// d3d12_readback_sync_composite is on (MCLA_READBACK_SYNC_COMPOSITE_DEFAULT);
// elision acts only in readback_resolve=some (the host default). Both are
// reported in the summary line.
//
// The table and the decision are pure and unit-tested
// (tests/mcla_m10_defaults_test.cpp); cvar calls are in mcla_m10_defaults.cpp.

#include "mcla_dev_overrides.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace mcla::m10_defaults {

inline constexpr std::string_view kSwitchCvar = "mcla_m10_speedup_defaults";

enum class Phase : uint8_t {
  kStartup,
  kGraphicsPlugin,
};

enum class Platform : uint8_t {
  kAll,       // Windows and Wine/Proton
  kWineOnly,  // only under Wine/Proton (net negative on Windows, l63)
};

struct Default {
  std::string_view cvar;
  std::string_view value;  // the text written
  Phase phase;
  // Diagnostics only: left at the SDK default in owner measurement builds.
  bool diagnostic;
  Platform platform = Platform::kAll;
  // A valid file line of this cvar's key whose value writes nothing (=false
  // of upload_ahead_16/_64) is the owner's off arm and wins over the default.
  // false: such a line changes nothing (the l94 frame lead keys).
  bool silent_false_is_arm = false;
};

inline constexpr std::array<Default, 9> kDefaults = {{
    {"memory_watch_census", "false", Phase::kStartup, true},
    {"memory_watch_disarm_bridge_safe_gaps", "true", Phase::kStartup, false, Platform::kWineOnly},
    {"d3d12_readback_sync_targeted", "true", Phase::kGraphicsPlugin, false},
    {"d3d12_readback_elide_unread", "true", Phase::kGraphicsPlugin, false},
    {"d3d12_wrm_event_wake", "true", Phase::kGraphicsPlugin, false},
    {"d3d12_replay_batched_wake", "true", Phase::kGraphicsPlugin, false},
    {"d3d12_readback_gpu_timing", "false", Phase::kGraphicsPlugin, true},
    {"shared_memory_upload_ahead_pages", "16", Phase::kGraphicsPlugin, false,
     Platform::kWineOnly, true},
    // l94 KI-074 fix (both platforms; see above).
    {"d3d12_frame_lead_strict", "1", Phase::kGraphicsPlugin, false},
}};

// Cvars this module writes only under Wine/Proton, never on Windows.
inline constexpr std::array<std::string_view, 2> kWineOnlyCvars = {
    "memory_watch_disarm_bridge_safe_gaps",
    "shared_memory_upload_ahead_pages",
};

// Cvars never written by this module (see above). The unit test checks that
// none is in kDefaults and that every non-launcher override key writes either
// a kDefaults cvar or one of these.
inline constexpr std::array<std::string_view, 93> kNeverWrittenCvars = {
    "memory_watch_probes",
    "mcla_readback_sync_composite",
    "d3d12_readback_sync_composite",
    // SDK default 512; only the M11-015 owner keys readback_kick_256/_128/
    // _1024 change it until an owner A/B accepts another interval.
    "d3d12_readback_sync_kick_draws",
    "mcla_native_d3d_refcount",
    "mcla_native_xapi_interlocked",
    "mcla_gpu_backend_vulkan",
    "shared_memory_hot_blocks",
    "shared_memory_hot_k",
    "shared_memory_hot_n",
    "shared_memory_hot_upload_cap_kb",
    "shared_memory_hot_probe_frames",
    "mcla_warp_probe",
    "mcla_warp_watchdog_seconds",
    // KI-035 finish-race fix: default on in the host itself; only the owner
    // key mcla_warp_finish_fix_off turns it off.
    "mcla_warp_finish_fix",
    // M10-007 / KI-048 soft cap (host mcla_texture_budget_win.cpp; owner key
    // mcla_texture_budget_soft_cap_off).
    "mcla_texture_budget_soft_cap_mb",
    // M11-002 diagnostics (owner builds only; KI-056 album, KI-066 pursuit).
    "mcla_album_probe",
    "mcla_pursuit_probe",
    // M11-001 / KI-068 SDK switches (SDK default true; only the owner keys
    // xam_content_stable_creation_time_off / xam_content_trace_off turn them
    // off).
    "xam_content_stable_creation_time",
    "xam_content_trace",
    // KI-071 SDK audio switches (owner A/B keys audio_speaker_config_stereo /
    // _surround, audio_downmix_itu_off, audio_channel_rms_probe only; the SDK
    // defaults auto / true / false stay for everyone else).
    "audio_speaker_config",
    "audio_downmix_itu",
    "audio_channel_rms_probe",
    // KI-056 fix A (host mcla_album_probe.cpp; default on in the host itself
    // since l81, owner decision 2026-11-08; only the owner key
    // album_preview_restore_off turns it off).
    "mcla_album_preview_restore",
    // l73 owner A/Bs: the SDK defaults (fetch identity cache on, verify off,
    // steady-key exemption on since the l78 SDK) stay for everyone else; only
    // the owner keys fetch_identity_off / fetch_identity_verify /
    // readback_steady_exempt / readback_steady_exempt_off change them.
    "d3d12_fetch_identity_cache",
    "d3d12_fetch_identity_verify",
    "d3d12_readback_sync_steady_exempt",
    // KI-071 XMA doubling fixes (SDK default false; owner keys only until an
    // owner A/B accepts them).
    "xma_swap_carry_packet",
    "xma_valid_only_when_written",
    "xma_work_lock_recheck",
    "xma_swap_trace",
    // KI-071 step 2: sibling trace, D2 stall, D1 start skip, codec reset (SDK
    // default false) and the host guest-consume probe (default false); owner
    // keys only until an owner A/B accepts them.
    "xma_sibling_trace",
    "xma_stall_on_invalid_next",
    "xma_honor_start_skip",
    "xma_reset_codec_on_clear",
    "mcla_xma_consume_probe",
    // KI-071 step 3: frame trim fix and its block-granular variant, the PCM
    // dump directory (string) and its all-rates switch (SDK defaults false /
    // empty); owner keys only until an owner A/B accepts them.
    "xma_honor_frame_trim",
    "xma_frame_trim_block_granular",
    "xma_dump_contexts",
    "xma_dump_all_rates",
    // KI-071 step 4: frame-walk trace, W1 fix and input dump (SDK defaults
    // false; the W1 fix defaults to true in the SDK since l77); owner keys
    // only (xma_frame_walk_fix / xma_frame_walk_fix_off).
    "xma_frame_walk_trace",
    "xma_frame_walk_fix",
    "xma_dump_input",
    // M11-007d: the lock probe and the read-back completion at the lock
    // (host defaults false); owner keys only until an owner A/B accepts them.
    "mcla_lock_probe",
    "mcla_lock_readback_ensure",
    // l77 M11-007e deferred fence writes (SDK default true since l78); owner
    // keys fence_write_deferred / fence_write_deferred_off only.
    "d3d12_fence_write_deferred",
    // l77 KI-074 experiments: the raw timer (host default on in owner builds)
    // and the guest vblank rate (launcher-owned: Locked = default, Unlocked =
    // 240); owner keys raw_timer_off / vblank_hz_72/_80/_88 only.
    "mcla_raw_timer",
    "mcla_guest_vblank_hz",
    // l78 fence deferral minimum gap (SDK int, default 0); owner key
    // fence_defer_gap_2000 only until an owner A/B accepts another value.
    "d3d12_fence_defer_min_gap_us",
    // l79 SDK experiments (default off in l79; since the l80 SDK its platform
    // defaults turn the XMA fixes and the timer wait on, and placed heaps on
    // on Windows: Canary XMA fixes, timer queue blocking wait, placed texture
    // heaps and their block size); owner keys
    // xma_canary_fixes / timer_queue_blocking_wait / texture_placed_heaps /
    // texture_heap_64mb only until an owner A/B accepts them.
    "xma_canary_fixes",
    "timer_queue_blocking_wait",
    "d3d12_texture_placed_heaps",
    "d3d12_texture_heap_block_mb",
    // l80 SDK: the KI-013 blend fix (default off), the KI-060 Wine short
    // sleep mode and audio worker priority (platform defaults) and the placed
    // heap pre-creation count (default 2) stay SDK-owned; owner keys
    // blend_minmax_fix / short_sleep_native / apu_priority_normal only.
    "d3d12_blend_minmax_fix",
    "ke_delay_short_sleep_mode",
    "apu_worker_priority",
    "d3d12_texture_heap_precreate",
    // l81 SDK KI-060: the zero-duration delay mode and its backoff tuning, the
    // guest audio thread priority (platform defaults) and the guest thread
    // priority boost start stay SDK-owned; owner keys zero_delay_native /
    // guest_audio_prio_normal only.
    "ke_delay_zero_mode",
    "ke_delay_zero_backoff_after",
    "ke_delay_zero_backoff_max_us",
    "ke_delay_zero_gap_us",
    "guest_audio_thread_priority",
    "guest_thread_priority_boost_start",
    // l82 SDK: CP bulk register writes (default off) stay SDK-owned; owner
    // key cp_bulk_regs only until an owner A/B accepts them.
    "cp_bulk_register_writes",
    // l83 KI-054 host switches stay host-owned (the probes default off; the
    // hitch hold default on in the host itself since l86, owner decision
    // 2026-11-10); owner keys ambient_dt_probe / ambient_dt_snapshot /
    // raw_timer_hitch_hold / raw_timer_hitch_hold_off only.
    "mcla_ambient_dt_probe",
    "mcla_ambient_dt_snapshot",
    "mcla_raw_timer_hitch_hold",
    // l84 KI-054: the raw timer statistics line (default off) stays
    // host-owned; owner key raw_timer_stats only.
    "mcla_raw_timer_stats",
    // l87 KI-060: the engine-thread filter probe and the native voice filter
    // with its self-check (default off) stay host-owned; owner keys
    // audio_filter_probe / audio_filter_native / audio_filter_native_verify.
    "mcla_audio_filter_probe",
    "mcla_audio_filter_native",
    "mcla_audio_filter_native_verify",
    // l88 KI-074: the fenced read-back delivery (default off) stays
    // SDK-owned; owner key readback_fenced_delivery only.
    "d3d12_readback_fenced_delivery",
    // l89 KI-060: the sub_82147738 native control (default off) stays
    // host-owned; owner key audio_filter_native_ch_off only.
    "mcla_audio_filter_native_ch_off",
    // l90 KI-074: the frame lead limit and the deferred read-back slot
    // release (default off) stay SDK-owned; owner keys frame_lead_limit_1 /
    // readback_deferred_release only.
    "d3d12_frame_lead_limit",
    "d3d12_readback_deferred_release",
    // l91 KI-074: d3d12_frame_lead_strict left this list in l94 (now a
    // default above).
    // KI-026 (host mcla_ki026_probe.cpp): the stall fix (default off until an
    // owner A/B accepts it) and the KI-026 probe; owner keys only.
    "mcla_warp_stall_fix",
    "mcla_ki026_probe",
    // l92 KI-074 / KI-076 probes (SDK, default off); owner keys only.
    "d3d12_readback_read_watch",
    "d3d12_readback_age_inject",
    "d3d12_occlusion_query_bounded_ms",
    "d3d12_present_stats",
    "d3d12_present_sync_interval",
    "mcla_locked_tearing_off",
    // l93 time-of-day controls (host mcla_tod.cpp, default off); owner keys
    // tod_rate_x10 / tod_rate_x30 / tod_freeze / tod_start_0430 /
    // tod_start_0530 only.
    "mcla_tod_rate",
    "mcla_tod_start_minutes",
    // l95 low latency (SDK, default off) and the FMA stage 0 benchmark (host,
    // default off); owner keys low_latency_reflex / _antilag2 / _auto,
    // low_latency_markers, low_latency_boost and fma_bench only.
    "d3d12_low_latency_mode",
    "d3d12_low_latency_markers_only",
    "d3d12_low_latency_boost",
    "mcla_fma_bench",
    // l96 Deck CP lever 1 (SDK, default off); owner keys cp_fence_poll_64,
    // cp_draw_packet_tsc, shader_im_load_memo and shader_im_load_memo_verify
    // only.
    "cp_fence_poll_interval",
    "cp_draw_packet_tsc",
    "shader_im_load_memo",
    "shader_im_load_memo_verify",
};

constexpr bool IsNeverWritten(std::string_view cvar) {
  for (const auto name : kNeverWrittenCvars) {
    if (name == cvar) {
      return true;
    }
  }
  return false;
}

constexpr bool DefaultsAvoidNeverWritten() {
  for (const auto &entry : kDefaults) {
    if (IsNeverWritten(entry.cvar)) {
      return false;
    }
  }
  return true;
}
static_assert(DefaultsAvoidNeverWritten(), "a never-written cvar is in kDefaults");

// True when an owner override key writes the entry's cvar (its line is the
// owner's A/B arm and wins).
constexpr bool HasOverrideKey(const Default &entry) {
  for (const auto &info : dev_overrides::kKeys) {
    if (info.phase != dev_overrides::Phase::kLauncher &&
        dev_overrides::CvarName(info) == entry.cvar) {
      return true;
    }
  }
  return false;
}

// True when one owner override key's record makes its line the entry's A/B
// arm: the key writes the entry's cvar (not a launcher key) and its value was
// applied, or its valid file line holds a value that writes nothing and the
// entry treats such a line as the owner's off arm (silent_false_is_arm).
// applied / file: dev_overrides::AppliedValue / FileValue of the key.
constexpr bool KeyLineWins(const Default &entry, const dev_overrides::KeyInfo &info,
                           std::optional<bool> applied, std::optional<bool> file) {
  if (info.phase == dev_overrides::Phase::kLauncher ||
      dev_overrides::CvarName(info) != entry.cvar) {
    return false;
  }
  if (applied.has_value()) {
    return true;
  }
  return file.has_value() && dev_overrides::CvarText(info, *file).empty() &&
         entry.silent_false_is_arm;
}

constexpr std::string_view ValueText(bool value) {
  return value ? "true" : "false";
}

constexpr std::string_view PlatformName(bool wine) {
  return wine ? "wine" : "windows";
}

enum class Action : uint8_t {
  kDisabledByOption,  // build without MCLA_M10_SPEEDUP_DEFAULTS
  kOffBySwitch,       // --mcla_m10_speedup_defaults=false
  kNotThisPlatform,   // Wine-only entry on Windows
  kMeasurementBuild,  // diagnostic switch in an MCLA_DEV_OVERRIDES build
  kDevOverride,       // the owner override file wrote (or set off) this cvar
  kUnavailable,       // the cvar is not registered (e.g. an older SDK)
  kUserSet,           // the command line or config changed the cvar
  kAlready,           // the cvar already holds the value (SDK default equal)
  kApply,             // write the value
};
inline constexpr size_t kActionCount = 9;

struct Inputs {
  bool option_on = false;          // MCLA_M10_SPEEDUP_DEFAULTS
  bool switch_on = true;           // mcla_m10_speedup_defaults
  bool wine = false;               // platform::RunningUnderWine()
  bool measurement_build = false;  // MCLA_DEV_OVERRIDES
  bool registered = false;         // rex::cvar::GetFlagInfo != nullptr
  bool non_default = false;        // rex::cvar::HasNonDefaultValue
  // An override key of this cvar was applied, or its file line is an explicit
  // "off" that writes nothing (shared_memory_upload_ahead_16/_64=false).
  bool dev_override_applied = false;
  bool holds_value = false;  // the current value already equals the default's
};

constexpr Action Decide(const Default &entry, const Inputs &in) {
  if (!in.option_on) {
    return Action::kDisabledByOption;
  }
  if (!in.switch_on) {
    return Action::kOffBySwitch;
  }
  if (entry.platform == Platform::kWineOnly && !in.wine) {
    return Action::kNotThisPlatform;
  }
  if (entry.diagnostic && in.measurement_build) {
    return Action::kMeasurementBuild;
  }
  // A file value equal to the SDK default is not visible as non-default; the
  // applied record is what keeps such an A/B arm.
  if (HasOverrideKey(entry) && in.dev_override_applied) {
    return Action::kDevOverride;
  }
  if (!in.registered) {
    return Action::kUnavailable;
  }
  if (in.non_default) {
    return Action::kUserSet;
  }
  if (in.holds_value) {
    return Action::kAlready;
  }
  return Action::kApply;
}

constexpr const char *ActionName(Action action) {
  switch (action) {
    case Action::kDisabledByOption:
      return "disabled";
    case Action::kOffBySwitch:
      return "switch_off";
    case Action::kNotThisPlatform:
      return "wine_only";
    case Action::kMeasurementBuild:
      return "measurement_build";
    case Action::kDevOverride:
      return "dev_override";
    case Action::kUnavailable:
      return "unavailable";
    case Action::kUserSet:
      return "user_set";
    case Action::kAlready:
      return "already";
    case Action::kApply:
      return "applied";
  }
  return "unknown";
}

// Per-phase tally for the summary line.
struct Tally {
  std::array<size_t, kActionCount> counts{};
  size_t failed = 0;  // kApply whose write was refused

  constexpr void Add(Action action) { ++counts[static_cast<size_t>(action)]; }
  constexpr size_t Count(Action action) const { return counts[static_cast<size_t>(action)]; }
};

// Runtime (mcla_m10_defaults.cpp). Both calls do nothing in builds without
// MCLA_M10_SPEEDUP_DEFAULTS.

// Phase::kStartup defaults. Call after dev_overrides::OnStartup().
void OnStartup();

// Phase::kGraphicsPlugin defaults. Call once after the graphics plugin has
// been loaded, after dev_overrides::OnGraphicsPluginLoaded() and the
// readback_sync and readback_resolve defaults.
void OnGraphicsPluginLoaded();

}  // namespace mcla::m10_defaults
