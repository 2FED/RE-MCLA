#include "mcla_m10_defaults.h"

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string_view>

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

using mcla::m10_defaults::Action;
using mcla::m10_defaults::Decide;
using mcla::m10_defaults::Default;
using mcla::m10_defaults::HasOverrideKey;
using mcla::m10_defaults::Inputs;
using mcla::m10_defaults::IsNeverWritten;
using mcla::m10_defaults::KeyLineWins;
using mcla::m10_defaults::kDefaults;
using mcla::m10_defaults::kNeverWrittenCvars;
using mcla::m10_defaults::kWineOnlyCvars;
using mcla::m10_defaults::Phase;
using mcla::m10_defaults::Platform;
using mcla::m10_defaults::Tally;
namespace dev = mcla::dev_overrides;

const Default *Find(std::string_view cvar) {
  for (const auto &entry : kDefaults) {
    if (entry.cvar == cvar) {
      return &entry;
    }
  }
  return nullptr;
}

bool WineOnly(const Default &entry) {
  return entry.platform == Platform::kWineOnly;
}

// A volunteer build at a fresh start: option on, switch on, cvar registered
// and at its default, no override file; on Wine/Proton or on Windows.
constexpr Inputs Fresh(bool wine) {
  Inputs in;
  in.option_on = true;
  in.switch_on = true;
  in.wine = wine;
  in.measurement_build = false;
  in.registered = true;
  in.non_default = false;
  in.dev_override_applied = false;
  in.holds_value = false;
  return in;
}

void TestTable() {
  struct Expected {
    std::string_view cvar;
    std::string_view value;
    Phase phase;
    bool diagnostic;
    Platform platform;
    bool keyed;
  };
  constexpr Expected expected[] = {
      {"memory_watch_census", "false", Phase::kStartup, true, Platform::kAll, true},
      {"memory_watch_disarm_bridge_safe_gaps", "true", Phase::kStartup, false,
       Platform::kWineOnly, true},
      {"d3d12_readback_sync_targeted", "true", Phase::kGraphicsPlugin, false, Platform::kAll, true},
      {"d3d12_readback_elide_unread", "true", Phase::kGraphicsPlugin, false, Platform::kAll, true},
      {"d3d12_wrm_event_wake", "true", Phase::kGraphicsPlugin, false, Platform::kAll, true},
      {"d3d12_replay_batched_wake", "true", Phase::kGraphicsPlugin, false, Platform::kAll, true},
      {"d3d12_readback_gpu_timing", "false", Phase::kGraphicsPlugin, true, Platform::kAll, false},
      {"shared_memory_upload_ahead_pages", "16", Phase::kGraphicsPlugin, false,
       Platform::kWineOnly, true},
      {"d3d12_frame_lead_strict", "1", Phase::kGraphicsPlugin, false, Platform::kAll, true},
  };
  Check(std::size(expected) == kDefaults.size(), "nine defaults");
  for (const auto &e : expected) {
    const Default *entry = Find(e.cvar);
    Check(entry != nullptr, "every expected cvar is in the table");
    Check(entry->value == e.value, "value");
    Check(entry->phase == e.phase, "phase");
    Check(entry->diagnostic == e.diagnostic, "diagnostic flag");
    Check(entry->platform == e.platform, "platform");
    Check(HasOverrideKey(*entry) == e.keyed, "override key presence");
  }
  // Every override key writing a default cvar must be applied in the same
  // phase, so its applied record (or file line) is this cvar's arm by then.
  size_t linked = 0;
  for (size_t i = 0; i < dev::kKeyCount; ++i) {
    const auto &info = dev::kKeys[i];
    const Default *entry = Find(dev::CvarName(info));
    if (!entry) {
      continue;
    }
    ++linked;
    const bool same_phase =
        (info.phase == dev::Phase::kStartup && entry->phase == Phase::kStartup) ||
        (info.phase == dev::Phase::kGraphicsPlugin && entry->phase == Phase::kGraphicsPlugin);
    Check(same_phase, "override key is applied in the same phase");
  }
  // census, disarm bridge, targeted, elide, wrm, batched, upload 16 and 64;
  // since l94 the three frame lead keys (strict, adaptive, strict_off).
  Check(linked == 11, "eleven override keys write default cvars");
  // Upload-ahead: both keys write the same integer cvar; =true writes 16/64,
  // =false writes nothing (the runtime treats that line as the owner's arm).
  for (const auto key : {dev::Key::kUploadAhead16, dev::Key::kUploadAhead64}) {
    const auto &info = dev::Info(key);
    Check(dev::CvarName(info) == "shared_memory_upload_ahead_pages", "upload key cvar");
    Check(dev::CvarText(info, false).empty(), "upload key =false writes nothing");
  }
  Check(dev::CvarText(dev::Info(dev::Key::kUploadAhead16), true) ==
            Find("shared_memory_upload_ahead_pages")->value,
        "the Wine default equals the owner's upload_ahead_16 arm");
  // The SDK validator accepts only 0, 16 and 64.
  const std::string_view upload = Find("shared_memory_upload_ahead_pages")->value;
  Check(upload == "0" || upload == "16" || upload == "64", "upload-ahead value accepted by the SDK");
  // Net negative on Windows (l63): Wine-only, and nothing else is.
  for (const auto cvar : kWineOnlyCvars) {
    const Default *entry = Find(cvar);
    Check(entry != nullptr && WineOnly(*entry), "net-negative switches are Wine-only");
  }
  size_t wine_only = 0;
  for (const auto &entry : kDefaults) {
    wine_only += WineOnly(entry) ? 1 : 0;
  }
  Check(wine_only == kWineOnlyCvars.size(), "exactly the listed cvars are Wine-only");
  Check(Find("memory_watch_probes") == nullptr, "probes not written");
  Check(Find("d3d12_readback_sync_composite") == nullptr,
        "composite stays with mcla_readback_sync");
  Check(Find("d3d12_readback_sync_kick_draws") == nullptr, "kick draws keeps its SDK default");
  Check(Find("mcla_native_d3d_refcount") == nullptr && Find("mcla_native_xapi_interlocked") == nullptr,
        "host native switches already default true");
  Check(Find("mcla_gpu_backend_vulkan") == nullptr, "Vulkan is not a default");
  // Never-written set: none is a default; hot blocks (M10 step 3) stay owner
  // A/B only until an A/B accepts them.
  for (const auto cvar : kNeverWrittenCvars) {
    Check(Find(cvar) == nullptr, "never-written cvar is not in the table");
  }
  for (size_t i = 0; i < kNeverWrittenCvars.size(); ++i) {
    for (size_t j = i + 1; j < kNeverWrittenCvars.size(); ++j) {
      Check(kNeverWrittenCvars[i] != kNeverWrittenCvars[j], "never-written: no cvar twice");
    }
  }
  Check(IsNeverWritten("shared_memory_hot_blocks"), "hot blocks never written");
  Check(IsNeverWritten("shared_memory_hot_k") && IsNeverWritten("shared_memory_hot_n") &&
            IsNeverWritten("shared_memory_hot_upload_cap_kb") &&
            IsNeverWritten("shared_memory_hot_probe_frames"),
        "hot block tuning never written");
  Check(Find("shared_memory_hot_blocks") == nullptr, "hot blocks not a default");
  Check(IsNeverWritten("mcla_warp_probe") && IsNeverWritten("mcla_warp_watchdog_seconds") &&
            Find("mcla_warp_probe") == nullptr && Find("mcla_warp_watchdog_seconds") == nullptr,
        "KI-035 warp probe and watchdog are never volunteer defaults");
  Check(IsNeverWritten("xam_content_stable_creation_time") &&
            IsNeverWritten("xam_content_trace") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kStableCreationTimeOff))) &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kContentTraceOff))) &&
            Find("xam_content_stable_creation_time") == nullptr &&
            Find("xam_content_trace") == nullptr,
        "the KI-068 SDK switches are never written by the M10 defaults");
  Check(IsNeverWritten("audio_speaker_config") && IsNeverWritten("audio_downmix_itu") &&
            IsNeverWritten("audio_channel_rms_probe") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kAudioSpeakerStereo))) &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kAudioSpeakerSurround))) &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kAudioDownmixItuOff))) &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kAudioChannelRmsProbe))) &&
            Find("audio_speaker_config") == nullptr && Find("audio_downmix_itu") == nullptr &&
            Find("audio_channel_rms_probe") == nullptr,
        "the KI-071 SDK audio switches are never written by the M10 defaults");
  Check(IsNeverWritten("mcla_album_probe") && IsNeverWritten("mcla_pursuit_probe") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kAlbumProbe))) &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kPursuitProbe))) &&
            Find("mcla_album_probe") == nullptr && Find("mcla_pursuit_probe") == nullptr,
        "the M11-002 album and pursuit probes are never volunteer defaults");
  Check(IsNeverWritten("d3d12_readback_sync_kick_draws") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kReadbackKick256))) &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kReadbackKick128))) &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kReadbackKick1024))) &&
            Find("d3d12_readback_sync_kick_draws") == nullptr,
        "the M11-015 kick interval is never written by the M10 defaults (owner keys only)");
  Check(IsNeverWritten("d3d12_fetch_identity_cache") &&
            IsNeverWritten("d3d12_fetch_identity_verify") &&
            IsNeverWritten("d3d12_readback_sync_steady_exempt") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kFetchIdentityOff))) &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kFetchIdentityVerify))) &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kReadbackSteadyExempt))) &&
            Find("d3d12_fetch_identity_cache") == nullptr &&
            Find("d3d12_fetch_identity_verify") == nullptr &&
            Find("d3d12_readback_sync_steady_exempt") == nullptr,
        "l73 fetch identity / steady-exempt switches keep their SDK defaults (owner keys only)");
  for (const auto key : {dev::Key::kXmaSwapCarryPacket, dev::Key::kXmaValidOnlyWhenWritten,
                         dev::Key::kXmaWorkLockRecheck, dev::Key::kXmaSwapTrace}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the KI-071 XMA fixes keep their SDK defaults (owner keys only)");
  }
  Check(IsNeverWritten("xma_swap_carry_packet") && IsNeverWritten("xma_valid_only_when_written") &&
            IsNeverWritten("xma_work_lock_recheck") && IsNeverWritten("xma_swap_trace"),
        "XMA cvar names in the never-written set");
  for (const auto key : {dev::Key::kXmaSiblingTrace, dev::Key::kXmaStallOnInvalidNext,
                         dev::Key::kXmaHonorStartSkip, dev::Key::kXmaResetCodecOnClear,
                         dev::Key::kXmaConsumeProbe}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the KI-071 step 2 switches keep their defaults (owner keys only)");
  }
  Check(IsNeverWritten("xma_sibling_trace") && IsNeverWritten("xma_stall_on_invalid_next") &&
            IsNeverWritten("xma_honor_start_skip") && IsNeverWritten("xma_reset_codec_on_clear") &&
            IsNeverWritten("mcla_xma_consume_probe"),
        "KI-071 step 2 cvar names in the never-written set");
  for (const auto key : {dev::Key::kXmaHonorFrameTrim, dev::Key::kXmaFrameTrimBlockGranular,
                         dev::Key::kXmaDumpAllRates, dev::Key::kXmaDumpContextsOn}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the KI-071 step 3 switches keep their defaults (owner keys only)");
  }
  Check(IsNeverWritten("xma_honor_frame_trim") && IsNeverWritten("xma_frame_trim_block_granular") &&
            IsNeverWritten("xma_dump_contexts") && IsNeverWritten("xma_dump_all_rates") &&
            kNeverWrittenCvars.size() == 94,
        "KI-071 step 3 cvar names in the never-written set");
  for (const auto key : {dev::Key::kXmaFrameWalkTrace, dev::Key::kXmaFrameWalkFix,
                         dev::Key::kXmaDumpInput}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the KI-071 step 4 switches keep their defaults (owner keys only)");
  }
  Check(IsNeverWritten("xma_frame_walk_trace") && IsNeverWritten("xma_frame_walk_fix") &&
            IsNeverWritten("xma_dump_input"),
        "KI-071 step 4 cvar names in the never-written set");
  for (const auto key : {dev::Key::kLockProbe, dev::Key::kLockReadbackEnsure}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the M11-007d lock switches keep their defaults (owner keys only)");
  }
  Check(IsNeverWritten("mcla_lock_probe") && IsNeverWritten("mcla_lock_readback_ensure") &&
            kNeverWrittenCvars.size() == 94,
        "M11-007d cvar names in the never-written set");
  for (const auto key : {dev::Key::kFenceWriteDeferred, dev::Key::kXmaFrameWalkFixOff}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l77 switches keep their SDK defaults (owner keys only)");
  }
  Check(IsNeverWritten("d3d12_fence_write_deferred") && IsNeverWritten("xma_frame_walk_fix") &&
            kNeverWrittenCvars.size() == 94,
        "l77 cvar names in the never-written set");
  for (const auto key : {dev::Key::kRawTimerOff, dev::Key::kVblankHz72, dev::Key::kVblankHz80,
                         dev::Key::kVblankHz88}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the KI-074 switches are never M10 defaults (owner keys only)");
  }
  Check(IsNeverWritten("mcla_raw_timer") && IsNeverWritten("mcla_guest_vblank_hz") &&
            Find("mcla_raw_timer") == nullptr && Find("mcla_guest_vblank_hz") == nullptr &&
            kNeverWrittenCvars.size() == 94,
        "KI-074 cvar names in the never-written set (63)");
  for (const auto key : {dev::Key::kReadbackSteadyExemptOff, dev::Key::kFenceWriteDeferredOff,
                         dev::Key::kFenceDeferGap2000}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l78 controls and the fence gap keep their SDK defaults (owner keys only)");
  }
  Check(IsNeverWritten("d3d12_fence_defer_min_gap_us") &&
            Find("d3d12_fence_defer_min_gap_us") == nullptr &&
            IsNeverWritten("d3d12_readback_sync_steady_exempt") &&
            IsNeverWritten("d3d12_fence_write_deferred") && kNeverWrittenCvars.size() == 94,
        "l78 cvar names in the never-written set (63)");
  for (const auto key : {dev::Key::kXmaCanaryFixes, dev::Key::kTimerQueueBlockingWait,
                         dev::Key::kTexturePlacedHeaps, dev::Key::kTextureHeap64Mb}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l79 SDK experiments keep their SDK defaults (owner keys only)");
  }
  Check(IsNeverWritten("xma_canary_fixes") && IsNeverWritten("timer_queue_blocking_wait") &&
            IsNeverWritten("d3d12_texture_placed_heaps") &&
            IsNeverWritten("d3d12_texture_heap_block_mb") &&
            Find("d3d12_texture_placed_heaps") == nullptr &&
            Find("d3d12_texture_heap_block_mb") == nullptr && kNeverWrittenCvars.size() == 94,
        "l79 cvar names in the never-written set (63)");
  for (const auto key : {dev::Key::kBlendMinmaxFix, dev::Key::kShortSleepNative,
                         dev::Key::kApuPriorityNormal, dev::Key::kTexturePlacedHeapsOff,
                         dev::Key::kTimerBlockingOff}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l80 SDK switches keep their SDK / platform defaults (owner keys only)");
  }
  Check(IsNeverWritten("d3d12_blend_minmax_fix") && IsNeverWritten("ke_delay_short_sleep_mode") &&
            IsNeverWritten("apu_worker_priority") &&
            IsNeverWritten("d3d12_texture_heap_precreate") &&
            Find("d3d12_blend_minmax_fix") == nullptr &&
            Find("ke_delay_short_sleep_mode") == nullptr &&
            Find("apu_worker_priority") == nullptr &&
            Find("d3d12_texture_heap_precreate") == nullptr && kNeverWrittenCvars.size() == 94,
        "l80 cvar names in the never-written set (63)");
  for (const auto key : {dev::Key::kZeroDelayNative, dev::Key::kGuestAudioPrioNormal,
                         dev::Key::kBlendMinmaxFixOff, dev::Key::kAlbumPreviewRestoreOff}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l81 controls write SDK / host-owned defaults only (owner keys only)");
  }
  Check(IsNeverWritten("ke_delay_zero_mode") && IsNeverWritten("ke_delay_zero_backoff_after") &&
            IsNeverWritten("ke_delay_zero_backoff_max_us") &&
            IsNeverWritten("ke_delay_zero_gap_us") &&
            IsNeverWritten("guest_audio_thread_priority") &&
            IsNeverWritten("guest_thread_priority_boost_start") &&
            Find("ke_delay_zero_mode") == nullptr &&
            Find("ke_delay_zero_backoff_after") == nullptr &&
            Find("ke_delay_zero_backoff_max_us") == nullptr &&
            Find("ke_delay_zero_gap_us") == nullptr &&
            Find("guest_audio_thread_priority") == nullptr &&
            Find("guest_thread_priority_boost_start") == nullptr &&
            kNeverWrittenCvars.size() == 94,
        "l81 cvar names in the never-written set (76)");
  Check(IsNeverWritten("mcla_album_preview_restore") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kAlbumPreviewRestore))) &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kAlbumPreviewRestoreOff))) &&
            Find("mcla_album_preview_restore") == nullptr,
        "the KI-056 fix A restore is host-owned (default on since l81), never an M10 default");
  Check(IsNeverWritten("cp_bulk_register_writes") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kCpBulkRegs))) &&
            Find("cp_bulk_register_writes") == nullptr && kNeverWrittenCvars.size() == 94,
        "the l82 CP bulk register writes stay SDK-owned (owner key cp_bulk_regs only)");
  for (const auto key : {dev::Key::kAmbientDtProbe, dev::Key::kAmbientDtSnapshot,
                         dev::Key::kRawTimerHitchHold}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l83 KI-054 switches keep their defaults (owner keys only)");
  }
  Check(IsNeverWritten("mcla_ambient_dt_probe") && IsNeverWritten("mcla_ambient_dt_snapshot") &&
            IsNeverWritten("mcla_raw_timer_hitch_hold") && kNeverWrittenCvars.size() == 94,
        "l83 cvar names in the never-written set (76)");
  Check(IsNeverWritten("mcla_raw_timer_stats") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kRawTimerStats))) &&
            Find("mcla_raw_timer_stats") == nullptr && kNeverWrittenCvars.size() == 94,
        "the l84 raw timer statistics switch is host-owned (owner key raw_timer_stats only)");
  Check(IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kRawTimerHitchHoldOff))) &&
            dev::CvarName(dev::Info(dev::Key::kRawTimerHitchHoldOff)) ==
                "mcla_raw_timer_hitch_hold" &&
            Find("mcla_raw_timer_hitch_hold") == nullptr && kNeverWrittenCvars.size() == 94,
        "the KI-054 hitch hold is host-owned (default on since l86), never an M10 default");
  for (const auto key : {dev::Key::kAudioFilterProbe, dev::Key::kAudioFilterNative,
                         dev::Key::kAudioFilterNativeVerify}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l87 KI-060 switches keep their defaults (owner keys only)");
  }
  Check(IsNeverWritten("mcla_audio_filter_probe") && IsNeverWritten("mcla_audio_filter_native") &&
            IsNeverWritten("mcla_audio_filter_native_verify") && kNeverWrittenCvars.size() == 94,
        "l87 cvar names in the never-written set (76)");
  Check(IsNeverWritten("d3d12_readback_fenced_delivery") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kReadbackFencedDelivery))) &&
            Find("d3d12_readback_fenced_delivery") == nullptr &&
            kNeverWrittenCvars.size() == 94,
        "the l88 fenced read-back delivery stays SDK-owned (owner key only)");
  Check(IsNeverWritten("mcla_audio_filter_native_ch_off") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kAudioFilterNativeChOff))) &&
            Find("mcla_audio_filter_native_ch_off") == nullptr &&
            kNeverWrittenCvars.size() == 94,
        "the l89 sub_82147738 native control is host-owned (owner key only)");
  for (const auto key : {dev::Key::kFrameLeadLimit1, dev::Key::kReadbackDeferredRelease}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l90 KI-074 switches keep their SDK defaults (owner keys only)");
  }
  Check(IsNeverWritten("d3d12_frame_lead_limit") &&
            IsNeverWritten("d3d12_readback_deferred_release") &&
            Find("d3d12_frame_lead_limit") == nullptr &&
            Find("d3d12_readback_deferred_release") == nullptr &&
            kNeverWrittenCvars.size() == 94,
        "l90 cvar names in the never-written set (76)");
  // l94: the strict frame lead bound left the never-written set and is a
  // default on both platforms; its three owner keys write that cvar.
  for (const auto key : {dev::Key::kFrameLeadStrict, dev::Key::kFrameLeadAdaptive,
                         dev::Key::kFrameLeadStrictOff}) {
    Check(!IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) != nullptr,
          "the l94 KI-074 lead keys write the strict default's cvar");
  }
  Check(!IsNeverWritten("d3d12_frame_lead_strict") && Find("d3d12_frame_lead_strict") != nullptr &&
            !WineOnly(*Find("d3d12_frame_lead_strict")) &&
            !Find("d3d12_frame_lead_strict")->diagnostic && kNeverWrittenCvars.size() == 94,
        "l94: the strict lead cvar is a default on both platforms, not a diagnostic (85)");
  Check(IsNeverWritten("d3d12_frame_lead_limit") && Find("d3d12_frame_lead_limit") == nullptr,
        "the l90 open-lead limit stays SDK-owned");
  for (const auto key : {dev::Key::kWarpStallFix, dev::Key::kKi026Probe}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the KI-026 stall fix and probe keep their host defaults (owner keys only)");
  }
  Check(IsNeverWritten("mcla_warp_stall_fix") && IsNeverWritten("mcla_ki026_probe") &&
            Find("mcla_warp_stall_fix") == nullptr && Find("mcla_ki026_probe") == nullptr &&
            kNeverWrittenCvars.size() == 94,
        "KI-026 cvar names in the never-written set (78)");
  for (const auto key : {dev::Key::kReadbackReadWatch, dev::Key::kReadbackAgeInject2,
                         dev::Key::kOcclusionQueryBounded4, dev::Key::kPresentStats,
                         dev::Key::kPresentInterval1, dev::Key::kLockedTearingOff}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l92 probes keep their SDK defaults (owner keys only)");
  }
  Check(IsNeverWritten("d3d12_readback_read_watch") &&
            IsNeverWritten("d3d12_readback_age_inject") &&
            IsNeverWritten("d3d12_occlusion_query_bounded_ms") &&
            IsNeverWritten("d3d12_present_stats") &&
            IsNeverWritten("d3d12_present_sync_interval") &&
            IsNeverWritten("mcla_locked_tearing_off") && kNeverWrittenCvars.size() == 94,
        "l92 cvar names in the never-written set (84)");
  for (const auto key : {dev::Key::kTodRateX10, dev::Key::kTodRateX30, dev::Key::kTodFreeze,
                         dev::Key::kTodStart0430, dev::Key::kTodStart0530}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l93 time-of-day controls keep their defaults (owner keys only)");
  }
  Check(IsNeverWritten("mcla_tod_rate") && IsNeverWritten("mcla_tod_start_minutes") &&
            Find("mcla_tod_rate") == nullptr && Find("mcla_tod_start_minutes") == nullptr &&
            kNeverWrittenCvars.size() == 94,
        "l93 cvar names in the never-written set (86)");
  for (const auto key : {dev::Key::kLowLatencyReflex, dev::Key::kLowLatencyAntiLag2,
                         dev::Key::kLowLatencyAuto, dev::Key::kLowLatencyMarkers,
                         dev::Key::kLowLatencyBoost, dev::Key::kFmaBench}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l95 low-latency and fma keys keep their defaults (owner keys only)");
  }
  Check(IsNeverWritten("d3d12_low_latency_mode") &&
            IsNeverWritten("d3d12_low_latency_markers_only") &&
            IsNeverWritten("d3d12_low_latency_boost") && IsNeverWritten("mcla_fma_bench") &&
            !IsNeverWritten("d3d12_low_latency_presleep_margin_us") &&
            Find("d3d12_low_latency_mode") == nullptr && kNeverWrittenCvars.size() == 94,
        "l95 cvar names in the never-written set (89)");
  for (const auto key : {dev::Key::kCpFencePoll64, dev::Key::kCpDrawPacketTsc,
                         dev::Key::kShaderImLoadMemo, dev::Key::kShaderImLoadMemoVerify}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l96 lever-1 keys keep their SDK defaults (owner keys only)");
  }
  Check(IsNeverWritten("cp_fence_poll_interval") && IsNeverWritten("cp_draw_packet_tsc") &&
            IsNeverWritten("shader_im_load_memo") &&
            IsNeverWritten("shader_im_load_memo_verify") &&
            Find("cp_fence_poll_interval") == nullptr && kNeverWrittenCvars.size() == 94,
        "l96 cvar names in the never-written set (93)");
  // l97: the low-latency strict replacement stays SDK-owned (default 0, both
  // owner keys write it); the strict bound itself stays an m10 default.
  for (const auto key : {dev::Key::kLeadLlOffWhenReflex, dev::Key::kLeadLlAdaptiveWhenReflex}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr &&
              dev::CvarName(dev::Info(key)) == "d3d12_frame_lead_strict_low_latency",
          "the l97 keys keep their SDK default (owner keys only)");
  }
  Check(IsNeverWritten("d3d12_frame_lead_strict_low_latency") &&
            Find("d3d12_frame_lead_strict_low_latency") == nullptr &&
            !IsNeverWritten("d3d12_frame_lead_strict") &&
            Find("d3d12_frame_lead_strict") != nullptr && kNeverWrittenCvars.size() == 94,
        "l97 cvar name in the never-written set (94); strict stays a default");
  // l81 arm rule: an arm without upload-ahead and bridge lines leaves the Wine
  // defaults to this module (decision applied on Wine, not on Windows); an
  // explicit =false line of those keys is the owner's off arm (the l71-l80
  // Deck template defect, docs/evidence/M11-l80-deck-analysis-20261108.md
  // section 4.1).
  {
    const dev::Parsed sparse = dev::Parse("memory_watch_census=false\nmcla_warp_probe=true\n");
    const dev::Parsed l80 = dev::Parse("memory_watch_disarm_bridge_safe_gaps=false\n"
                                       "shared_memory_upload_ahead_16=false\n"
                                       "shared_memory_upload_ahead_64=false\n");
    for (const auto *cvar : {"memory_watch_disarm_bridge_safe_gaps",
                             "shared_memory_upload_ahead_pages"}) {
      const Default *entry = Find(cvar);
      Check(entry != nullptr && WineOnly(*entry), "Wine-only entry present");
      if (entry == nullptr) {
        continue;
      }
      bool sparse_line = false;
      bool l80_line = false;
      for (size_t i = 0; i < dev::kKeyCount; ++i) {
        if (dev::CvarName(dev::kKeys[i]) != entry->cvar) {
          continue;
        }
        sparse_line = sparse_line || sparse.values[i].has_value();
        l80_line = l80_line || l80.values[i].has_value();
      }
      Inputs in = Fresh(true);
      in.measurement_build = true;
      in.dev_override_applied = sparse_line;
      Check(!sparse_line && Decide(*entry, in) == Action::kApply,
            "sparse l81 Deck arm: the Wine default applies in an owner build");
      in.dev_override_applied = l80_line;
      Check(l80_line && Decide(*entry, in) == Action::kDevOverride,
            "explicit =false line: the owner's off arm wins (the l80 Deck defect)");
      Inputs win = Fresh(false);
      win.measurement_build = true;
      Check(Decide(*entry, win) == Action::kNotThisPlatform, "never on Windows");
    }
  }
  Check(IsNeverWritten("mcla_warp_finish_fix") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kWarpFinishFixOff))) &&
            Find("mcla_warp_finish_fix") == nullptr,
        "the KI-035 finish-race fix cvar is never written by the M10 defaults");
  Check(IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kTextureSoftCapOff))) &&
            Find("mcla_texture_budget_soft_cap_mb") == nullptr,
        "the M10-007 soft cap is never written by the M10 defaults");
  Check(!IsNeverWritten("shared_memory_upload_ahead_pages") &&
            !IsNeverWritten("memory_watch_census"),
        "defaults are not in the never-written set");
  for (const auto key : {dev::Key::kHotBlocksShadow, dev::Key::kHotBlocksOn}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))),
          "hot blocks keys write a never-written cvar");
  }
  // Every non-launcher override key writes a default cvar or a never-written
  // one, so a new key forces a decision here.
  for (const auto &info : dev::kKeys) {
    if (info.phase == dev::Phase::kLauncher) {
      continue;
    }
    const bool is_default = Find(dev::CvarName(info)) != nullptr;
    Check(is_default != IsNeverWritten(dev::CvarName(info)),
          "override key cvar: exactly one of default or never-written");
  }
  for (size_t i = 0; i < kDefaults.size(); ++i) {
    for (size_t j = i + 1; j < kDefaults.size(); ++j) {
      Check(kDefaults[i].cvar != kDefaults[j].cvar, "no cvar twice");
    }
  }
  Check(mcla::m10_defaults::PlatformName(true) == "wine" &&
            mcla::m10_defaults::PlatformName(false) == "windows",
        "platform names");
}

void TestPlatform() {
  for (const auto &entry : kDefaults) {
    const Action windows = Decide(entry, Fresh(false));
    const Action wine = Decide(entry, Fresh(true));
    Check(wine == Action::kApply, "fresh Wine/Proton start applies every default");
    if (WineOnly(entry)) {
      Check(windows == Action::kNotThisPlatform, "Windows: Wine-only default not applied");
    } else {
      Check(windows == Action::kApply, "Windows: common default applied");
    }
  }
  // Wine-only entries on Wine: the owner file and explicit values still win.
  for (const auto cvar : kWineOnlyCvars) {
    const Default &entry = *Find(cvar);
    Inputs in = Fresh(true);
    in.measurement_build = true;
    in.dev_override_applied = true;
    Check(Decide(entry, in) == Action::kDevOverride, "Wine: dev override wins");
    in = Fresh(true);
    in.non_default = true;
    Check(Decide(entry, in) == Action::kUserSet, "Wine: user value wins");
    in = Fresh(true);
    in.registered = false;
    Check(Decide(entry, in) == Action::kUnavailable, "Wine: older SDK without the cvar");
    in = Fresh(true);
    in.holds_value = true;
    Check(Decide(entry, in) == Action::kAlready, "Wine: already holds the value");
    in = Fresh(true);
    in.measurement_build = true;
    Check(Decide(entry, in) == Action::kApply, "Wine owner build without a file line applies");
    // Windows: never, whatever else holds.
    in = Fresh(false);
    in.dev_override_applied = true;
    in.non_default = true;
    Check(Decide(entry, in) == Action::kNotThisPlatform, "Windows: reported as Wine-only");
  }
}

void TestDecision() {
  for (const bool wine : {false, true}) {
    for (const auto &entry : kDefaults) {
      if (WineOnly(entry) && !wine) {
        continue;  // TestPlatform
      }
      Check(Decide(entry, Fresh(wine)) == Action::kApply, "fresh volunteer start applies");

      Inputs in = Fresh(wine);
      in.option_on = false;
      in.switch_on = false;
      in.dev_override_applied = true;
      Check(Decide(entry, in) == Action::kDisabledByOption, "option off wins over everything");

      in = Fresh(wine);
      in.switch_on = false;
      Check(Decide(entry, in) == Action::kOffBySwitch, "host switch off");

      in = Fresh(wine);
      in.registered = false;
      Check(Decide(entry, in) == Action::kUnavailable, "older SDK without the cvar");

      in = Fresh(wine);
      in.non_default = true;
      Check(Decide(entry, in) == Action::kUserSet, "command line or config value kept");

      in = Fresh(wine);
      in.holds_value = true;
      Check(Decide(entry, in) == Action::kAlready, "SDK default already equal");

      // Owner A/B arm: the override file wrote the cvar, maybe to its SDK
      // default (non_default false). It must win.
      in = Fresh(wine);
      in.dev_override_applied = true;
      in.measurement_build = true;
      const Action owner = Decide(entry, in);
      if (entry.diagnostic) {
        Check(owner == Action::kMeasurementBuild, "diagnostics untouched in owner builds");
      } else {
        Check(owner == Action::kDevOverride, "dev override arm wins");
      }
      in.non_default = true;
      Check(Decide(entry, in) != Action::kApply, "never overwrite an arm set to a non-default");

      // Measurement build without a file value.
      in = Fresh(wine);
      in.measurement_build = true;
      Check(Decide(entry, in) ==
                (entry.diagnostic ? Action::kMeasurementBuild : Action::kApply),
            "owner build without the file: speed-ups on, diagnostics at SDK default");
    }
  }
  // A cvar with no override key ignores the applied flag (it cannot be set).
  const Default *timing = Find("d3d12_readback_gpu_timing");
  Inputs in = Fresh(false);
  in.dev_override_applied = true;
  Check(Decide(*timing, in) == Action::kApply, "applied flag without a key is ignored");
}

void TestExhaustive() {
  // Over all input combinations: kApply only for option on, switch on,
  // Wine for Wine-only entries, registered, default value, no applied file
  // value (for keyed entries), and not a diagnostic switch in a measurement
  // build.
  size_t applies = 0;
  size_t windows_wine_only_applies = 0;
  for (const auto &entry : kDefaults) {
    for (unsigned bits = 0; bits < 256; ++bits) {
      Inputs in;
      in.option_on = bits & 1;
      in.switch_on = bits & 2;
      in.measurement_build = bits & 4;
      in.registered = bits & 8;
      in.non_default = bits & 16;
      in.dev_override_applied = bits & 32;
      in.holds_value = bits & 64;
      in.wine = bits & 128;
      const bool expect_apply = in.option_on && in.switch_on && in.registered &&
                                !in.non_default && !in.holds_value &&
                                (in.wine || !WineOnly(entry)) &&
                                !(HasOverrideKey(entry) && in.dev_override_applied) &&
                                !(entry.diagnostic && in.measurement_build);
      const Action action = Decide(entry, in);
      Check((action == Action::kApply) == expect_apply, "apply only in the exact case");
      applies += action == Action::kApply ? 1 : 0;
      if (WineOnly(entry) && !in.wine && action == Action::kApply) {
        ++windows_wine_only_applies;
      }
    }
  }
  Check(windows_wine_only_applies == 0, "Wine-only defaults never apply on Windows");
  // Free inputs in the apply case: measurement_build, dev_override_applied and
  // wine. Five keyed all-platform entries (four speed-ups and, since l94, the
  // strict frame lead): 2 x 2 each (applied false); census (keyed,
  // diagnostic): 1 x 2; GPU timing (no key, diagnostic): 2 x 2; the two keyed
  // Wine-only speed-ups: 2 each (wine true, applied false).
  Check(applies == 5 * 4 + 2 + 4 + 2 * 2, "apply case count");
}

// The runtime's dev_override_applied for an entry (mcla_m10_defaults.cpp
// DevOverrideApplied) for an owner file, assuming every cvar is registered and
// at its default: the dev-override phase (run first, mcla_app.cpp) applies
// each parsed value whose text is non-empty, so AppliedValue is that value and
// FileValue the parsed one.
bool SimulatedOverrideApplied(const Default &entry, std::string_view file) {
  const dev::Parsed parsed = dev::Parse(file);
  bool wins = false;
  for (size_t i = 0; i < dev::kKeyCount; ++i) {
    const auto &info = dev::kKeys[i];
    const std::optional<bool> value = parsed.values[i];
    std::optional<bool> applied;
    if (value.has_value() && info.phase != dev::Phase::kLauncher &&
        !dev::CvarText(info, *value).empty()) {
      applied = value;
    }
    wins = wins || KeyLineWins(entry, info, applied, value);
  }
  return wins;
}

void TestFrameLeadStrictDefault() {
  const Default *strict = Find("d3d12_frame_lead_strict");
  Check(strict != nullptr && strict->value == "1" && strict->phase == Phase::kGraphicsPlugin &&
            !strict->diagnostic && strict->platform == Platform::kAll &&
            !strict->silent_false_is_arm,
        "l94: strict frame lead 1 at graphics-plugin load, both platforms, =false not an arm");
  Check(dev::CvarText(dev::Info(dev::Key::kFrameLeadStrict), true) == strict->value &&
            dev::CvarText(dev::Info(dev::Key::kFrameLeadStrictOff), true) == "0" &&
            dev::CvarText(dev::Info(dev::Key::kFrameLeadAdaptive), true) == "2",
        "frame_lead_strict writes the default, strict_off 0 (SDK default), adaptive 2");
  // Only the upload-ahead entry treats a silent =false line as the owner's arm.
  for (const auto &entry : kDefaults) {
    Check(entry.silent_false_is_arm == (entry.cvar == "shared_memory_upload_ahead_pages"),
          "silent =false arm only for upload-ahead");
  }
  // KeyLineWins per key record.
  for (const auto key : {dev::Key::kFrameLeadStrict, dev::Key::kFrameLeadAdaptive,
                         dev::Key::kFrameLeadStrictOff}) {
    const auto &info = dev::Info(key);
    Check(KeyLineWins(*strict, info, true, true), "an applied lead key wins");
    Check(!KeyLineWins(*strict, info, std::nullopt, false), "a lead key =false line does not win");
    Check(!KeyLineWins(*strict, info, std::nullopt, true),
          "a lead key =true line that was not applied (cvar unavailable) does not win");
    Check(!KeyLineWins(*strict, info, std::nullopt, std::nullopt), "no line: no win");
  }
  Check(!KeyLineWins(*strict, dev::Info(dev::Key::kFrameLeadLimit1), true, true),
        "a key of another cvar never wins");
  Check(!KeyLineWins(*strict, dev::Info(dev::Key::kPrewarmSeedStore), true, true),
        "a launcher key never wins");
  const Default *upload = Find("shared_memory_upload_ahead_pages");
  Check(KeyLineWins(*upload, dev::Info(dev::Key::kUploadAhead16), std::nullopt, false) &&
            KeyLineWins(*upload, dev::Info(dev::Key::kUploadAhead64), std::nullopt, false),
        "the upload-ahead =false line stays the owner's off arm");
  // Owner build (measurement build) on both platforms: the arms of l94.
  struct Arm {
    const char *file;
    Action expected;
    const char *message;
  };
  const Arm arms[] = {
      {"", Action::kApply, "no file: the strict default applies"},
      {"memory_watch_census=false\nmcla_warp_probe=true\ntod_start_0530=true\n"
       "tod_rate_x10=true\n",
       Action::kApply, "the sparse l94 dawn arm: the strict default applies"},
      {"frame_lead_strict_off=true\n", Action::kDevOverride,
       "frame_lead_strict_off=true wins (writes 0 = the SDK default)"},
      {"tod_start_0530=true\ntod_rate_x10=true\nframe_lead_strict_off=true\n",
       Action::kDevOverride, "the l94 dawn-off arm: strict-off wins"},
      {"frame_lead_adaptive=true\n", Action::kDevOverride, "frame_lead_adaptive=true wins (2)"},
      {"frame_lead_strict=true\n", Action::kDevOverride,
       "frame_lead_strict=true is its own arm (writes the default value)"},
      {"frame_lead_strict=false\nframe_lead_adaptive=false\nframe_lead_strict_off=false\n",
       Action::kApply, "=false lines of the three lead keys change nothing"},
      {"frame_lead_strict_off=true\nframe_lead_adaptive=true\n", Action::kApply,
       "conflicting lead keys: neither applied, the build default stays"},
  };
  for (const bool wine : {false, true}) {
    for (const Arm &arm : arms) {
      Inputs in = Fresh(wine);
      in.measurement_build = true;
      in.dev_override_applied = SimulatedOverrideApplied(*strict, arm.file);
      Check(Decide(*strict, in) == arm.expected, arm.message);
    }
    // A command-line value (non-default, e.g. 2) is kept; an SDK without the
    // cvar (the volunteer SDK before l91) is reported as unavailable.
    Inputs in = Fresh(wine);
    in.non_default = true;
    Check(Decide(*strict, in) == Action::kUserSet, "a command-line lead value is kept");
    in = Fresh(wine);
    in.registered = false;
    Check(Decide(*strict, in) == Action::kUnavailable, "SDK without the cvar: unavailable");
    in = Fresh(wine);
    in.switch_on = false;
    Check(Decide(*strict, in) == Action::kOffBySwitch,
          "--mcla_m10_speedup_defaults=false also leaves the lead bound at the SDK default");
  }
}

void TestTally() {
  Tally tally;
  tally.Add(Action::kApply);
  tally.Add(Action::kApply);
  tally.Add(Action::kUserSet);
  tally.Add(Action::kNotThisPlatform);
  Check(tally.Count(Action::kApply) == 2, "apply count");
  Check(tally.Count(Action::kUserSet) == 1, "user set count");
  Check(tally.Count(Action::kNotThisPlatform) == 1, "Wine-only count");
  Check(tally.Count(Action::kDevOverride) == 0, "dev override count");
  Check(std::string_view(mcla::m10_defaults::ActionName(Action::kApply)) == "applied",
        "action name");
  Check(std::string_view(mcla::m10_defaults::ActionName(Action::kNotThisPlatform)) == "wine_only",
        "Wine-only action name");
  Check(mcla::m10_defaults::ValueText(false) == "false", "value text");
}
}  // namespace

int main() {
  TestTable();
  TestPlatform();
  TestDecision();
  TestExhaustive();
  TestFrameLeadStrictDefault();
  TestTally();
  std::printf("mcla_m10_defaults_test: %d checks passed\n", checks);
  return 0;
}
