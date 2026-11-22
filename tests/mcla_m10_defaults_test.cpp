#include "mcla_m10_defaults.h"

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
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
using mcla::m10_defaults::kWindowsOnlyCvars;
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

bool WindowsOnly(const Default &entry) {
  return entry.platform == Platform::kWindowsOnly;
}

// The three l111 defaults (TestL111Defaults); the arms of earlier builds that
// name force_hidef / xfer_elide / xfer_extent are those defaults' own arms.
bool L111Default(const Default &entry) {
  return entry.cvar == "video_mode_force_hidef" ||
         entry.cvar == "d3d12_transfer_elide_full_overwrite" ||
         entry.cvar == "d3d12_draw_extent_from_rect";
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
      {"cp_draw_packet_tsc", "true", Phase::kGraphicsPlugin, false, Platform::kAll, true},
      {"cp_fence_poll_interval", "64", Phase::kGraphicsPlugin, false, Platform::kAll, true},
      {"d3d12_fetch_identity_v2", "true", Phase::kGraphicsPlugin, false, Platform::kAll, true},
      {"d3d12_rt_update_memo", "true", Phase::kGraphicsPlugin, false, Platform::kAll, true},
      {"cp_reg_write_skip_unchanged", "true", Phase::kGraphicsPlugin, false, Platform::kAll,
       true},
      {"d3d12_low_latency_mode", "auto", Phase::kStartup, false, Platform::kAll, true},
      {"d3d12_low_latency_auto_antilag2", "true", Phase::kStartup, false, Platform::kAll, true},
      {"d3d12_frame_lead_strict_low_latency", "1", Phase::kGraphicsPlugin, false, Platform::kAll,
       true},
      {"video_mode_force_hidef", "true", Phase::kStartup, false, Platform::kAll, true},
      {"d3d12_transfer_elide_full_overwrite", "true", Phase::kGraphicsPlugin, false,
       Platform::kWindowsOnly, true},
      // l115: the bound's entry holds true again (false in l112-l114, KI-084).
      {"d3d12_draw_extent_from_rect", "true", Phase::kGraphicsPlugin, false,
       Platform::kWindowsOnly, true},
  };
  Check(std::size(expected) == kDefaults.size(), "twenty defaults");
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
  // since l94 the three frame lead keys (strict, adaptive, strict_off);
  // since l98 the four lever-1 keys (cp_draw_packet_tsc / _off,
  // cp_fence_poll_64 / _off); since l104 the fifteen keys of the six l104
  // defaults (tex_memo_v2 / _off, rt_memo / _off, cp_reg_skip / _off, the
  // three l95 mode keys and low_latency_off, low_latency_auto_antilag2 /
  // _off, the two l97 keys and lead_ll_replace_off); since l111 the six keys
  // of the three l111 defaults (force_hidef / _off, xfer_elide / _off,
  // xfer_extent / _off).
  Check(linked == 36, "thirty-six override keys write default cvars");
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
            kNeverWrittenCvars.size() == 133,
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
            kNeverWrittenCvars.size() == 133,
        "M11-007d cvar names in the never-written set");
  for (const auto key : {dev::Key::kFenceWriteDeferred, dev::Key::kXmaFrameWalkFixOff}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l77 switches keep their SDK defaults (owner keys only)");
  }
  Check(IsNeverWritten("d3d12_fence_write_deferred") && IsNeverWritten("xma_frame_walk_fix") &&
            kNeverWrittenCvars.size() == 133,
        "l77 cvar names in the never-written set");
  for (const auto key : {dev::Key::kRawTimerOff, dev::Key::kVblankHz72, dev::Key::kVblankHz80,
                         dev::Key::kVblankHz88}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the KI-074 switches are never M10 defaults (owner keys only)");
  }
  Check(IsNeverWritten("mcla_raw_timer") && IsNeverWritten("mcla_guest_vblank_hz") &&
            Find("mcla_raw_timer") == nullptr && Find("mcla_guest_vblank_hz") == nullptr &&
            kNeverWrittenCvars.size() == 133,
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
            IsNeverWritten("d3d12_fence_write_deferred") && kNeverWrittenCvars.size() == 133,
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
            Find("d3d12_texture_heap_block_mb") == nullptr && kNeverWrittenCvars.size() == 133,
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
            Find("d3d12_texture_heap_precreate") == nullptr && kNeverWrittenCvars.size() == 133,
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
            kNeverWrittenCvars.size() == 133,
        "l81 cvar names in the never-written set (76)");
  Check(IsNeverWritten("mcla_album_preview_restore") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kAlbumPreviewRestore))) &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kAlbumPreviewRestoreOff))) &&
            Find("mcla_album_preview_restore") == nullptr,
        "the KI-056 fix A restore is host-owned (default on since l81), never an M10 default");
  Check(IsNeverWritten("cp_bulk_register_writes") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kCpBulkRegs))) &&
            Find("cp_bulk_register_writes") == nullptr && kNeverWrittenCvars.size() == 133,
        "the l82 CP bulk register writes stay SDK-owned (owner key cp_bulk_regs only)");
  for (const auto key : {dev::Key::kAmbientDtProbe, dev::Key::kAmbientDtSnapshot,
                         dev::Key::kRawTimerHitchHold}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l83 KI-054 switches keep their defaults (owner keys only)");
  }
  Check(IsNeverWritten("mcla_ambient_dt_probe") && IsNeverWritten("mcla_ambient_dt_snapshot") &&
            IsNeverWritten("mcla_raw_timer_hitch_hold") && kNeverWrittenCvars.size() == 133,
        "l83 cvar names in the never-written set (76)");
  Check(IsNeverWritten("mcla_raw_timer_stats") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kRawTimerStats))) &&
            Find("mcla_raw_timer_stats") == nullptr && kNeverWrittenCvars.size() == 133,
        "the l84 raw timer statistics switch is host-owned (owner key raw_timer_stats only)");
  Check(IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kRawTimerHitchHoldOff))) &&
            dev::CvarName(dev::Info(dev::Key::kRawTimerHitchHoldOff)) ==
                "mcla_raw_timer_hitch_hold" &&
            Find("mcla_raw_timer_hitch_hold") == nullptr && kNeverWrittenCvars.size() == 133,
        "the KI-054 hitch hold is host-owned (default on since l86), never an M10 default");
  for (const auto key : {dev::Key::kAudioFilterProbe, dev::Key::kAudioFilterNative,
                         dev::Key::kAudioFilterNativeVerify, dev::Key::kAudioFilterNativeOff}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l87 KI-060 switches keep their defaults (owner keys only)");
  }
  Check(IsNeverWritten("mcla_audio_filter_probe") && IsNeverWritten("mcla_audio_filter_native") &&
            IsNeverWritten("mcla_audio_filter_native_verify") && kNeverWrittenCvars.size() == 133,
        "l87 cvar names in the never-written set (76)");
  Check(IsNeverWritten("d3d12_readback_fenced_delivery") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kReadbackFencedDelivery))) &&
            Find("d3d12_readback_fenced_delivery") == nullptr &&
            kNeverWrittenCvars.size() == 133,
        "the l88 fenced read-back delivery stays SDK-owned (owner key only)");
  Check(IsNeverWritten("mcla_audio_filter_native_ch_off") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kAudioFilterNativeChOff))) &&
            Find("mcla_audio_filter_native_ch_off") == nullptr &&
            kNeverWrittenCvars.size() == 133,
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
            kNeverWrittenCvars.size() == 133,
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
            !Find("d3d12_frame_lead_strict")->diagnostic && kNeverWrittenCvars.size() == 133,
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
            kNeverWrittenCvars.size() == 133,
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
            IsNeverWritten("mcla_locked_tearing_off") && kNeverWrittenCvars.size() == 133,
        "l92 cvar names in the never-written set (84)");
  for (const auto key : {dev::Key::kTodRateX10, dev::Key::kTodRateX30, dev::Key::kTodFreeze,
                         dev::Key::kTodStart0430, dev::Key::kTodStart0530}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l93 time-of-day controls keep their defaults (owner keys only)");
  }
  Check(IsNeverWritten("mcla_tod_rate") && IsNeverWritten("mcla_tod_start_minutes") &&
            Find("mcla_tod_rate") == nullptr && Find("mcla_tod_start_minutes") == nullptr &&
            kNeverWrittenCvars.size() == 133,
        "l93 cvar names in the never-written set (86)");
  for (const auto key : {dev::Key::kLowLatencyMarkers, dev::Key::kLowLatencyBoost,
                         dev::Key::kFmaBench}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l95 markers, boost and fma keys keep their defaults (owner keys only)");
  }
  // l104: the l95 mode keys write the low-latency mode default's cvar.
  for (const auto key : {dev::Key::kLowLatencyReflex, dev::Key::kLowLatencyAntiLag2,
                         dev::Key::kLowLatencyAuto, dev::Key::kLowLatencyOff}) {
    Check(!IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) != nullptr,
          "l104: the l95 mode keys and low_latency_off write the mode default's cvar");
  }
  Check(!IsNeverWritten("d3d12_low_latency_mode") &&
            IsNeverWritten("d3d12_low_latency_markers_only") &&
            IsNeverWritten("d3d12_low_latency_boost") && IsNeverWritten("mcla_fma_bench") &&
            !IsNeverWritten("d3d12_low_latency_presleep_margin_us") &&
            Find("d3d12_low_latency_mode") != nullptr && kNeverWrittenCvars.size() == 133,
        "l95 cvar names in the never-written set; the mode is a default since l104");
  for (const auto key : {dev::Key::kShaderImLoadMemo, dev::Key::kShaderImLoadMemoVerify}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l96 IM_LOAD memo keys keep their SDK defaults (owner keys only)");
  }
  Check(IsNeverWritten("shader_im_load_memo") && IsNeverWritten("shader_im_load_memo_verify") &&
            Find("shader_im_load_memo") == nullptr && kNeverWrittenCvars.size() == 133,
        "l96 memo cvar names in the never-written set");
  // l98: the fence poll interval and the draw-packet clock left the
  // never-written set (now defaults on both platforms); their l96 keys and
  // the l98 controls write those cvars.
  for (const auto key : {dev::Key::kCpFencePoll64, dev::Key::kCpDrawPacketTsc,
                         dev::Key::kCpFencePollOff, dev::Key::kCpDrawPacketTscOff}) {
    Check(!IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) != nullptr,
          "the l98 lever-1 keys write the lever-1 defaults' cvars");
  }
  Check(!IsNeverWritten("cp_fence_poll_interval") && !IsNeverWritten("cp_draw_packet_tsc") &&
            !WineOnly(*Find("cp_fence_poll_interval")) &&
            !WineOnly(*Find("cp_draw_packet_tsc")) &&
            !Find("cp_fence_poll_interval")->diagnostic &&
            !Find("cp_draw_packet_tsc")->diagnostic && kNeverWrittenCvars.size() == 133,
        "l98: lever 1 is a default on both platforms, not a diagnostic");
  // l97: the low-latency strict replacement is an m10 default since l104 (1);
  // the two l97 keys and lead_ll_replace_off write it; the strict bound itself
  // stays an m10 default.
  for (const auto key : {dev::Key::kLeadLlOffWhenReflex, dev::Key::kLeadLlAdaptiveWhenReflex,
                         dev::Key::kLeadLlReplaceOff}) {
    Check(!IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) != nullptr &&
              dev::CvarName(dev::Info(key)) == "d3d12_frame_lead_strict_low_latency",
          "the l97 keys and lead_ll_replace_off write the l104 replacement default's cvar");
  }
  Check(!IsNeverWritten("d3d12_frame_lead_strict_low_latency") &&
            Find("d3d12_frame_lead_strict_low_latency") != nullptr &&
            !IsNeverWritten("d3d12_frame_lead_strict") &&
            Find("d3d12_frame_lead_strict") != nullptr && kNeverWrittenCvars.size() == 133,
        "l97 cvar name is a default since l104; strict stays a default");
  // l99: the texture churn and guard-action keys keep their defaults (owner
  // keys only).
  for (const auto key : {dev::Key::kTextureHardMargin256, dev::Key::kTextureMinAge2000,
                         dev::Key::kLeadLlGuardAdaptive, dev::Key::kLeadLlGuardRearm,
                         dev::Key::kLeadLlGuardOff}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l99 keys keep their defaults (owner keys only)");
  }
  Check(IsNeverWritten("mcla_texture_hard_margin_mb") &&
            IsNeverWritten("texture_cache_min_age_ms") &&
            IsNeverWritten("d3d12_frame_lead_strict_low_latency_guard_action") &&
            Find("texture_cache_min_age_ms") == nullptr && kNeverWrittenCvars.size() == 133,
        "l99 cvar names in the never-written set");
  // l100: the verifies and the spin backoff keep their defaults (owner keys
  // only); the register-write skip is a default since l104 (cp_reg_skip and
  // cp_reg_skip_off write it).
  for (const auto key : {dev::Key::kCpRegSkipVerify, dev::Key::kCpDrawFastVerify,
                         dev::Key::kGuestSpinBackoff64}) {
    Check(IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) == nullptr,
          "the l100 verify and spin keys keep their defaults (owner keys only)");
  }
  Check(!IsNeverWritten("cp_reg_write_skip_unchanged") &&
            IsNeverWritten("cp_reg_write_skip_verify") &&
            IsNeverWritten("cp_draw_const_only_verify") &&
            IsNeverWritten("mcla_guest_spin_backoff") &&
            Find("cp_reg_write_skip_unchanged") != nullptr &&
            Find("mcla_guest_spin_backoff") == nullptr && kNeverWrittenCvars.size() == 133,
        "l100 cvar names: the verifies never written, the skip a default since l104");
  // l101: the render-target memo verify keeps its default (owner key only);
  // the texture memo v2 and the render-target memo are defaults since l104.
  Check(IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kRtMemoVerify))) &&
            Find(dev::CvarName(dev::Info(dev::Key::kRtMemoVerify))) == nullptr,
        "the l101 verify key keeps its default (owner key only)");
  for (const auto key : {dev::Key::kTexMemoV2, dev::Key::kRtMemo, dev::Key::kTexMemoV2Off,
                         dev::Key::kRtMemoOff, dev::Key::kCpRegSkip, dev::Key::kCpRegSkipOff}) {
    Check(!IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) != nullptr,
          "l104: the lever keys and their controls write the lever defaults' cvars");
  }
  Check(!IsNeverWritten("d3d12_fetch_identity_v2") && !IsNeverWritten("d3d12_rt_update_memo") &&
            IsNeverWritten("d3d12_rt_update_memo_verify") &&
            IsNeverWritten("d3d12_fetch_identity_verify") &&
            Find("d3d12_fetch_identity_v2") != nullptr &&
            Find("d3d12_rt_update_memo") != nullptr && kNeverWrittenCvars.size() == 133,
        "l101 cvar names: the verifies never written, the levers defaults since l104");
  // l103: the Anti-Lag 2 replacement count keeps its SDK default (owner key
  // only: AMD keeps the strict bound); the auto opt-in is a default since
  // l104 (low_latency_auto_antilag2 and its control write it).
  Check(IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kLeadLlCountAntiLag2))) &&
            Find(dev::CvarName(dev::Info(dev::Key::kLeadLlCountAntiLag2))) == nullptr,
        "the l103 replacement-count key keeps its default (owner key only)");
  for (const auto key : {dev::Key::kLowLatencyAutoAntiLag2, dev::Key::kLowLatencyAutoAntiLag2Off}) {
    Check(!IsNeverWritten(dev::CvarName(dev::Info(key))) &&
              Find(dev::CvarName(dev::Info(key))) != nullptr,
          "l104: the auto Anti-Lag 2 keys write the auto opt-in default's cvar");
  }
  Check(IsNeverWritten("d3d12_frame_lead_strict_low_latency_antilag2") &&
            !IsNeverWritten("d3d12_low_latency_auto_antilag2") &&
            Find("d3d12_frame_lead_strict_low_latency_antilag2") == nullptr &&
            Find("d3d12_low_latency_auto_antilag2") != nullptr &&
            !IsNeverWritten("d3d12_frame_lead_strict_low_latency") &&
            !IsNeverWritten("d3d12_low_latency_mode") &&
            IsNeverWritten("d3d12_frame_lead_strict_low_latency_guard_action") &&
            kNeverWrittenCvars.size() == 133,
        "l104: six cvars left the never-written set (98; l105: 104; l111: 117); the Anti-Lag 2 "
        "count and the guard action stay");
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
    if (WindowsOnly(entry)) {
      Check(wine == Action::kWindowsOnly, "Wine/Proton: Windows-only default not applied");
      Check(windows == Action::kApply, "Windows: Windows-only default applied");
      continue;
    }
    Check(wine == Action::kApply, "fresh Wine/Proton start applies every other default");
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
      if ((WineOnly(entry) && !wine) || (WindowsOnly(entry) && wine)) {
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
  size_t wine_windows_only_applies = 0;
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
                                (!in.wine || !WindowsOnly(entry)) &&
                                !(HasOverrideKey(entry) && in.dev_override_applied) &&
                                !(entry.diagnostic && in.measurement_build);
      const Action action = Decide(entry, in);
      Check((action == Action::kApply) == expect_apply, "apply only in the exact case");
      applies += action == Action::kApply ? 1 : 0;
      if (WineOnly(entry) && !in.wine && action == Action::kApply) {
        ++windows_wine_only_applies;
      }
      if (WindowsOnly(entry) && in.wine && action == Action::kApply) {
        ++wine_windows_only_applies;
      }
    }
  }
  Check(windows_wine_only_applies == 0, "Wine-only defaults never apply on Windows");
  Check(wine_windows_only_applies == 0, "Windows-only defaults never apply under Wine/Proton");
  // Free inputs in the apply case: measurement_build, dev_override_applied and
  // wine. Fourteen keyed all-platform entries (four speed-ups and, since l94,
  // the strict frame lead, since l98 the two lever-1 entries, since l104 the
  // three per-draw levers and the three low-latency entries, since l111 the
  // forced HiDef mode): 2 x 2 each (applied false); census (keyed,
  // diagnostic): 1 x 2; GPU timing (no key, diagnostic): 2 x 2; the two keyed
  // Wine-only speed-ups: 2 each (wine true, applied false); the two keyed
  // Windows-only l111 entries: 2 each (wine false, applied false).
  Check(applies == 14 * 4 + 2 + 4 + 2 * 2 + 2 * 2, "apply case count");
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

// l98 CP lever 1 defaults (cp_draw_packet_tsc=true, cp_fence_poll_interval=64)
// and their controls, as TestFrameLeadStrictDefault.
void TestLever1Defaults() {
  const Default *tsc = Find("cp_draw_packet_tsc");
  const Default *poll = Find("cp_fence_poll_interval");
  Check(tsc != nullptr && tsc->value == "true" && tsc->phase == Phase::kGraphicsPlugin &&
            !tsc->diagnostic && tsc->platform == Platform::kAll && !tsc->silent_false_is_arm,
        "l98: draw-packet clock true at graphics-plugin load, both platforms, =false not an arm");
  Check(poll != nullptr && poll->value == "64" && poll->phase == Phase::kGraphicsPlugin &&
            !poll->diagnostic && poll->platform == Platform::kAll && !poll->silent_false_is_arm,
        "l98: fence poll interval 64 at graphics-plugin load, both platforms, =false not an arm");
  Check(dev::CvarText(dev::Info(dev::Key::kCpDrawPacketTsc), true) == tsc->value &&
            dev::CvarText(dev::Info(dev::Key::kCpDrawPacketTscOff), true) == "false" &&
            dev::CvarText(dev::Info(dev::Key::kCpFencePoll64), true) == poll->value &&
            dev::CvarText(dev::Info(dev::Key::kCpFencePollOff), true) == "0",
        "the l96 keys write the defaults, the l98 controls the SDK defaults (false, 0)");
  Check(Find("shader_im_load_memo") == nullptr && Find("shader_im_load_memo_verify") == nullptr &&
            IsNeverWritten("shader_im_load_memo"),
        "l98: the IM_LOAD memo stays off (never written)");
  const struct {
    const Default *entry;
    dev::Key on;
    dev::Key off;
  } groups[] = {{tsc, dev::Key::kCpDrawPacketTsc, dev::Key::kCpDrawPacketTscOff},
                {poll, dev::Key::kCpFencePoll64, dev::Key::kCpFencePollOff}};
  for (const auto &g : groups) {
    for (const auto key : {g.on, g.off}) {
      const auto &info = dev::Info(key);
      Check(KeyLineWins(*g.entry, info, true, true), "an applied lever-1 key wins");
      Check(!KeyLineWins(*g.entry, info, std::nullopt, false),
            "a lever-1 key =false line does not win");
      Check(!KeyLineWins(*g.entry, info, std::nullopt, true),
            "a lever-1 key =true line that was not applied (cvar unavailable) does not win");
      Check(!KeyLineWins(*g.entry, info, std::nullopt, std::nullopt), "no line: no win");
    }
  }
  Check(!KeyLineWins(*tsc, dev::Info(dev::Key::kCpFencePollOff), true, true) &&
            !KeyLineWins(*poll, dev::Info(dev::Key::kCpDrawPacketTscOff), true, true) &&
            !KeyLineWins(*tsc, dev::Info(dev::Key::kShaderImLoadMemo), true, true),
        "a key of another cvar never wins");
  // Owner build (measurement build) on both platforms: the l98 arms.
  struct Arm {
    const char *file;
    Action tsc_expected;
    Action poll_expected;
    const char *message;
  };
  const Arm arms[] = {
      {"", Action::kApply, Action::kApply, "no file: both lever-1 defaults apply"},
      {"memory_watch_census=false\nmcla_warp_probe=true\n", Action::kApply, Action::kApply,
       "the sparse l98 default arm: both defaults apply"},
      {"cp_draw_packet_tsc_off=true\ncp_fence_poll_off=true\n", Action::kDevOverride,
       Action::kDevOverride, "the l98 lever1-off arm: both controls win"},
      {"cp_draw_packet_tsc_off=true\n", Action::kDevOverride, Action::kApply,
       "the clock control alone: the poll default still applies"},
      {"cp_fence_poll_off=true\n", Action::kApply, Action::kDevOverride,
       "the poll control alone: the clock default still applies"},
      {"cp_draw_packet_tsc=true\ncp_fence_poll_64=true\n", Action::kDevOverride,
       Action::kDevOverride, "the l96 keys are their own arm (they write the default values)"},
      {"cp_draw_packet_tsc=false\ncp_draw_packet_tsc_off=false\ncp_fence_poll_64=false\n"
       "cp_fence_poll_off=false\n",
       Action::kApply, Action::kApply, "=false lines of the four keys change nothing"},
      {"cp_draw_packet_tsc=true\ncp_draw_packet_tsc_off=true\ncp_fence_poll_64=true\n"
       "cp_fence_poll_off=true\n",
       Action::kApply, Action::kApply,
       "conflicting lever-1 keys: neither applied, the build defaults stay"},
      {"shader_im_load_memo=true\nshader_im_load_memo_verify=true\n", Action::kApply,
       Action::kApply, "the memo keys do not touch the lever-1 defaults"},
  };
  for (const bool wine : {false, true}) {
    for (const Arm &arm : arms) {
      Inputs in = Fresh(wine);
      in.measurement_build = true;
      in.dev_override_applied = SimulatedOverrideApplied(*tsc, arm.file);
      Check(Decide(*tsc, in) == arm.tsc_expected, arm.message);
      in.dev_override_applied = SimulatedOverrideApplied(*poll, arm.file);
      Check(Decide(*poll, in) == arm.poll_expected, arm.message);
    }
    for (const Default *entry : {tsc, poll}) {
      Inputs in = Fresh(wine);
      in.non_default = true;
      Check(Decide(*entry, in) == Action::kUserSet, "a command-line lever-1 value is kept");
      in = Fresh(wine);
      in.registered = false;
      Check(Decide(*entry, in) == Action::kUnavailable,
            "SDK without the l96 cvars (e.g. the gate SDK): unavailable");
      in = Fresh(wine);
      in.holds_value = true;
      Check(Decide(*entry, in) == Action::kAlready, "an SDK whose default equals: already");
      in = Fresh(wine);
      in.switch_on = false;
      Check(Decide(*entry, in) == Action::kOffBySwitch,
            "--mcla_m10_speedup_defaults=false leaves lever 1 at the SDK default");
    }
  }
}

// l104 defaults (owner decisions 2026-11-15): the three per-draw levers and
// low latency (auto with the AMD opt-in, the strict replacement 1), and their
// controls, as TestLever1Defaults.
void TestL104Defaults() {
  const Default *v2 = Find("d3d12_fetch_identity_v2");
  const Default *rt = Find("d3d12_rt_update_memo");
  const Default *reg = Find("cp_reg_write_skip_unchanged");
  const Default *mode = Find("d3d12_low_latency_mode");
  const Default *aa2 = Find("d3d12_low_latency_auto_antilag2");
  const Default *repl = Find("d3d12_frame_lead_strict_low_latency");
  const Default *const entries[] = {v2, rt, reg, mode, aa2, repl};
  for (const Default *entry : entries) {
    Check(entry != nullptr && !entry->diagnostic && entry->platform == Platform::kAll &&
              !entry->silent_false_is_arm,
          "l104: each new default on both platforms, not a diagnostic, =false not an arm");
  }
  Check(v2->value == "true" && rt->value == "true" && reg->value == "true" &&
            v2->phase == Phase::kGraphicsPlugin && rt->phase == Phase::kGraphicsPlugin &&
            reg->phase == Phase::kGraphicsPlugin,
        "l104 A: the three levers true at graphics-plugin load");
  Check(mode->value == "auto" && mode->phase == Phase::kStartup && aa2->value == "true" &&
            aa2->phase == Phase::kStartup && repl->value == "1" &&
            repl->phase == Phase::kGraphicsPlugin,
        "l104 B: low latency auto and the AMD opt-in at startup, replacement 1 at plugin load");
  // B keeps AMD strict: the replacement does not count an Anti-Lag 2 sleep, and
  // the guard keeps its SDK action (latch strict for the session on a trip).
  Check(Find("d3d12_frame_lead_strict_low_latency_antilag2") == nullptr &&
            IsNeverWritten("d3d12_frame_lead_strict_low_latency_antilag2") &&
            Find("d3d12_frame_lead_strict_low_latency_guard_action") == nullptr &&
            IsNeverWritten("d3d12_frame_lead_strict_low_latency_guard_action") &&
            Find("d3d12_low_latency_allow_wine") == nullptr &&
            Find("d3d12_low_latency_markers_only") == nullptr,
        "l104 B: Anti-Lag 2 count, guard action, Wine allowance and markers stay SDK defaults");
  // The on keys write the default values, the controls the SDK defaults.
  const struct {
    const Default *entry;
    dev::Key key;
    const char *text;
  } texts[] = {
      {v2, dev::Key::kTexMemoV2, "true"},
      {v2, dev::Key::kTexMemoV2Off, "false"},
      {rt, dev::Key::kRtMemo, "true"},
      {rt, dev::Key::kRtMemoOff, "false"},
      {reg, dev::Key::kCpRegSkip, "true"},
      {reg, dev::Key::kCpRegSkipOff, "false"},
      {mode, dev::Key::kLowLatencyReflex, "reflex"},
      {mode, dev::Key::kLowLatencyAntiLag2, "antilag2"},
      {mode, dev::Key::kLowLatencyAuto, "auto"},
      {mode, dev::Key::kLowLatencyOff, "off"},
      {aa2, dev::Key::kLowLatencyAutoAntiLag2, "true"},
      {aa2, dev::Key::kLowLatencyAutoAntiLag2Off, "false"},
      {repl, dev::Key::kLeadLlOffWhenReflex, "1"},
      {repl, dev::Key::kLeadLlAdaptiveWhenReflex, "2"},
      {repl, dev::Key::kLeadLlReplaceOff, "0"},
  };
  for (const auto &t : texts) {
    const auto &info = dev::Info(t.key);
    Check(dev::CvarName(info) == t.entry->cvar && dev::CvarText(info, true) == t.text &&
              dev::CvarText(info, false).empty(),
          "l104: each key of a new default writes its text to that cvar; =false nothing");
    Check(KeyLineWins(*t.entry, info, true, true), "an applied l104 key wins");
    Check(!KeyLineWins(*t.entry, info, std::nullopt, false), "an l104 key =false line does not win");
    Check(!KeyLineWins(*t.entry, info, std::nullopt, true),
          "an l104 key =true line that was not applied (cvar unavailable) does not win");
    Check(!KeyLineWins(*t.entry, info, std::nullopt, std::nullopt), "no line: no win");
  }
  Check(dev::CvarText(dev::Info(dev::Key::kTexMemoV2), true) == v2->value &&
            dev::CvarText(dev::Info(dev::Key::kLowLatencyAuto), true) == mode->value &&
            dev::CvarText(dev::Info(dev::Key::kLeadLlOffWhenReflex), true) == repl->value,
        "the l101 / l95 / l97 on keys write the l104 default values");
  // The verify keys and the other low-latency keys write other cvars: they
  // never win over a default (the SDK lets each verify win over its lever).
  Check(!KeyLineWins(*v2, dev::Info(dev::Key::kFetchIdentityVerify), true, true) &&
            !KeyLineWins(*rt, dev::Info(dev::Key::kRtMemoVerify), true, true) &&
            !KeyLineWins(*reg, dev::Info(dev::Key::kCpRegSkipVerify), true, true) &&
            !KeyLineWins(*reg, dev::Info(dev::Key::kCpDrawFastVerify), true, true) &&
            !KeyLineWins(*mode, dev::Info(dev::Key::kLowLatencyMarkers), true, true) &&
            !KeyLineWins(*repl, dev::Info(dev::Key::kLeadLlCountAntiLag2), true, true) &&
            !KeyLineWins(*repl, dev::Info(dev::Key::kLeadLlGuardOff), true, true) &&
            !KeyLineWins(*repl, dev::Info(dev::Key::kFrameLeadStrictOff), true, true) &&
            !KeyLineWins(*aa2, dev::Info(dev::Key::kLowLatencyOff), true, true),
        "a key of another cvar never wins over an l104 default");
  // Owner build (measurement build) on both platforms: the l104 arms. Order of
  // the expected actions: v2, rt, reg, mode, aa2, repl.
  const std::string base =
      "mcla_readback_sync_composite=true\nmcla_native_d3d_refcount=true\n"
      "mcla_native_xapi_interlocked=true\nd3d12_wrm_event_wake=true\n"
      "d3d12_replay_batched_wake=true\nd3d12_readback_elide_unread=true\n"
      "prewarm_seed_store=true\nd3d12_readback_sync_targeted=true\n"
      "memory_watch_census=false\nmcla_warp_probe=true\nmcla_album_probe=true\n"
      "mcla_pursuit_probe=true\n";
  const Action ap = Action::kApply;
  const Action dv = Action::kDevOverride;
  struct Arm {
    std::string file;
    Action expected[6];
    const char *message;
  };
  const Arm arms[] = {
      {"", {ap, ap, ap, ap, ap, ap}, "no file: every l104 default applies"},
      {base, {ap, ap, ap, ap, ap, ap}, "the sparse l104 pc/default arm: every default"},
      {base + "tex_memo_v2_off=true\nrt_memo_off=true\ncp_reg_skip_off=true\n",
       {dv, dv, dv, ap, ap, ap},
       "pc/levers-off: the three lever controls win, low latency stays"},
      {base + "low_latency_off=true\nlead_ll_replace_off=true\n",
       {ap, ap, ap, dv, ap, dv},
       "pc/lowlat-off: mode off and no replacement win, the levers stay"},
      {base + "tex_memo_v2_off=true\nrt_memo_off=true\ncp_reg_skip_off=true\n"
              "low_latency_off=true\nlead_ll_replace_off=true\n",
       {dv, dv, dv, dv, ap, dv},
       "pc/old-default: the l103 default behaviour (the AMD opt-in is inert with mode off)"},
      {base + "low_latency_reflex=true\nframe_lead_strict_off=true\n"
              "cp_reg_skip_verify=true\ncp_draw_fast_verify=true\ntex_memo_v2=true\n"
              "fetch_identity_verify=true\nrt_memo_verify=true\n",
       {dv, ap, ap, dv, ap, ap},
       "pc/verify-all: reflex and tex_memo_v2 are own arms, the rt / reg defaults stay (their "
       "verify wins in the SDK)"},
      {base + "low_latency_reflex=true\nframe_lead_strict_off=true\n"
              "tex_memo_v2=true\nrt_memo=true\ncp_reg_skip=true\n",
       {dv, dv, dv, dv, ap, ap},
       "pc/fast2: its keys write the default values (own arms)"},
      {base + "low_latency_auto=true\nlow_latency_auto_antilag2=true\n"
              "lead_ll_off_when_reflex=true\nlead_ll_count_antilag2=true\n",
       {ap, ap, ap, dv, dv, dv},
       "amd/auto: the l103 candidate keys are own arms (the count key is another cvar)"},
      {base + "low_latency_antilag2=true\nframe_lead_strict_off=true\n",
       {ap, ap, ap, dv, ap, ap},
       "amd/antilag2-nostrict: the explicit mode wins, the replacement default stays"},
      {"tex_memo_v2=false\ntex_memo_v2_off=false\nrt_memo=false\nrt_memo_off=false\n"
       "cp_reg_skip=false\ncp_reg_skip_off=false\nlow_latency_auto=false\n"
       "low_latency_off=false\nlow_latency_auto_antilag2=false\n"
       "low_latency_auto_antilag2_off=false\nlead_ll_off_when_reflex=false\n"
       "lead_ll_replace_off=false\n",
       {ap, ap, ap, ap, ap, ap},
       "=false lines of the l104 keys change nothing"},
      {"tex_memo_v2=true\ntex_memo_v2_off=true\nrt_memo=true\nrt_memo_off=true\n"
       "cp_reg_skip=true\ncp_reg_skip_off=true\nlow_latency_auto=true\nlow_latency_off=true\n"
       "low_latency_auto_antilag2=true\nlow_latency_auto_antilag2_off=true\n"
       "lead_ll_off_when_reflex=true\nlead_ll_replace_off=true\n",
       {ap, ap, ap, ap, ap, ap},
       "conflicting l104 keys: neither applied, the build defaults stay"},
      {"low_latency_auto_antilag2_off=true\n", {ap, ap, ap, ap, dv, ap},
       "the AMD opt-in control alone"},
      {"lead_ll_adaptive_when_reflex=true\n", {ap, ap, ap, ap, ap, dv},
       "the l97 adaptive key is its own arm of the replacement"},
  };
  for (const bool wine : {false, true}) {
    for (const Arm &arm : arms) {
      for (size_t i = 0; i < 6; ++i) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(*entries[i], arm.file);
        Check(Decide(*entries[i], in) == arm.expected[i], arm.message);
      }
    }
    for (const Default *entry : entries) {
      Inputs in = Fresh(wine);
      Check(Decide(*entry, in) == Action::kApply, "a volunteer build applies every l104 default");
      in.non_default = true;
      Check(Decide(*entry, in) == Action::kUserSet, "a command-line l104 value is kept");
      in = Fresh(wine);
      in.registered = false;
      Check(Decide(*entry, in) == Action::kUnavailable,
            "SDK without the cvar (e.g. the gate SDK): unavailable");
      in = Fresh(wine);
      in.holds_value = true;
      Check(Decide(*entry, in) == Action::kAlready, "an SDK whose default equals: already");
      in = Fresh(wine);
      in.switch_on = false;
      Check(Decide(*entry, in) == Action::kOffBySwitch,
            "--mcla_m10_speedup_defaults=false leaves the l104 switches at the SDK defaults");
    }
  }
}


void TestL105Keys() {
  // l105: the measurement and per-pixel / sharpness keys write never-written
  // cvars: no m10 default changes, and none of them ever wins over a default.
  const struct {
    dev::Key key;
    const char *cvar;
    const char *text;
    dev::Phase phase;
  } keys[] = {
      {dev::Key::kGpuCat, "d3d12_gpu_category_timing", "true", dev::Phase::kGraphicsPlugin},
      {dev::Key::kPresentFxaa, "swap_post_effect", "fxaa", dev::Phase::kGraphicsPlugin},
      {dev::Key::kPresentFsr, "present_effect", "fsr", dev::Phase::kStartup},
      {dev::Key::kPresentCas, "present_effect", "cas", dev::Phase::kStartup},
      {dev::Key::kCasSharpMax, "present_cas_additional_sharpness", "1.0", dev::Phase::kStartup},
      {dev::Key::kFsrSharpMax, "present_fsr_sharpness_reduction", "0.0", dev::Phase::kStartup},
      {dev::Key::kAnisoDefault, "anisotropic_override", "-1", dev::Phase::kGraphicsPlugin},
  };
  for (const auto &k : keys) {
    const auto &info = dev::Info(k.key);
    Check(dev::CvarName(info) == k.cvar && dev::CvarText(info, true) == k.text &&
              dev::CvarText(info, false).empty() && info.phase == k.phase,
          "l105: each key writes its text to its cvar in its phase; =false nothing");
    Check(IsNeverWritten(k.cvar) && Find(k.cvar) == nullptr,
          "l105: the cvar is never written by the m10 defaults");
    for (const auto &entry : kDefaults) {
      Check(!KeyLineWins(entry, info, true, true) && !KeyLineWins(entry, info, std::nullopt, false),
            "l105: no l105 line wins over any m10 default");
    }
  }
  Check(kDefaults.size() == 20 && kNeverWrittenCvars.size() == 133,
        "l105: seventeen defaults (l111: twenty), 115 never-written cvars (98 + 6 + l106 1 + "
        "l107 8 + l108 2; l111: 117)");
  // The l105 arms leave every m10 default as it is (owner build, both
  // platforms).
  const std::string arms[] = {
      "gpu_cat=true\n",
      "present_fxaa=true\n",
      "present_fsr=true\nfsr_sharp_max=true\n",
      "present_cas=true\ncas_sharp_max=true\n",
      "aniso_default=true\n",
      "shared_memory_upload_ahead_64=true\n",
      "gpu_cat=true\npresent_fxaa=true\npresent_fsr=true\naniso_default=true\n",
  };
  for (const bool wine : {false, true}) {
    for (const std::string &arm : arms) {
      for (const auto &entry : kDefaults) {
        if (entry.cvar == "shared_memory_upload_ahead_pages") {
          continue;  // its own key is the upload-ahead arm
        }
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(entry, arm);
        Check(!in.dev_override_applied, "l105 arms touch no m10 default");
      }
    }
  }
  // upload_ahead_64 (pc/upload-ahead): the upload-ahead default is Wine only;
  // on Windows the key's 64 is applied by the override module alone, on Wine
  // it wins over the default 16.
  const Default *upload = Find("shared_memory_upload_ahead_pages");
  Check(upload != nullptr && upload->platform == Platform::kWineOnly, "upload-ahead is Wine only");
  Check(SimulatedOverrideApplied(*upload, "shared_memory_upload_ahead_64=true\n"),
        "shared_memory_upload_ahead_64 is the upload-ahead arm");
  {
    Inputs in = Fresh(true);
    in.measurement_build = true;
    in.dev_override_applied = true;
    Check(Decide(*upload, in) == Action::kDevOverride, "on Wine the 64 arm wins over 16");
    in = Fresh(false);
    in.measurement_build = true;
    in.dev_override_applied = true;
    Check(Decide(*upload, in) == Action::kNotThisPlatform,
          "on Windows the default never writes upload-ahead");
  }
}

void TestL106Keys() {
  // l106: guest_msaa_off writes the never-written host cvar
  // mcla_guest_msaa_off at startup: no m10 default changes, and the key never
  // wins over a default.
  const auto &info = dev::Info(dev::Key::kGuestMsaaOff);
  Check(dev::CvarName(info) == "mcla_guest_msaa_off" && dev::CvarText(info, true) == "true" &&
            dev::CvarText(info, false).empty() && info.phase == dev::Phase::kStartup,
        "l106: the key writes true to mcla_guest_msaa_off at startup; =false nothing");
  Check(IsNeverWritten("mcla_guest_msaa_off") && Find("mcla_guest_msaa_off") == nullptr,
        "l106: the cvar is never written by the m10 defaults");
  for (const auto &entry : kDefaults) {
    Check(!KeyLineWins(entry, info, true, true) && !KeyLineWins(entry, info, std::nullopt, false),
          "l106: the l106 line wins over no m10 default");
  }
  Check(kDefaults.size() == 20 && kNeverWrittenCvars.size() == 133,
        "l106: seventeen defaults (l111: twenty), 115 never-written cvars (104 + 1 + l107 8 + "
        "l108 2; l111: 117)");
  // The l106 arms (pc/msaa-off, pc/msaa-off-fxaa, pc/msaa-off-gpu-cat and the
  // AMD / Deck msaa-off arms) leave every m10 default as it is.
  const std::string arms[] = {
      "guest_msaa_off=true\n",
      "guest_msaa_off=true\npresent_fxaa=true\n",
      "guest_msaa_off=true\ngpu_cat=true\n",
      "guest_msaa_off=true\r\npresent_fxaa=true\r\ngpu_cat=true\r\n",
  };
  for (const bool wine : {false, true}) {
    for (const std::string &arm : arms) {
      for (const auto &entry : kDefaults) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(entry, arm);
        Check(!in.dev_override_applied, "l106 arms touch no m10 default");
      }
    }
  }
}

void TestL107Keys() {
  // l107: the scene sample-count pair, the KI-082 traffic motion probe and
  // the KI-083 resolve fill A/B write never-written cvars: no m10 default
  // changes, and none of them ever wins over a default.
  const struct {
    dev::Key key;
    const char *cvar;
    const char *text;
    dev::Phase phase;
  } keys[] = {
      {dev::Key::kGuestMsaaScene1x, "mcla_guest_msaa_scene", "1", dev::Phase::kStartup},
      {dev::Key::kGuestMsaaScene2x, "mcla_guest_msaa_scene", "2", dev::Phase::kStartup},
      {dev::Key::kTrafficMotionProbe, "mcla_traffic_motion_probe", "true", dev::Phase::kStartup},
      {dev::Key::kResolveFillOff, "resolve_resolution_scale_fill_half_pixel_offset", "false",
       dev::Phase::kGraphicsPlugin},
  };
  for (const auto &k : keys) {
    const auto &info = dev::Info(k.key);
    Check(dev::CvarName(info) == k.cvar && dev::CvarText(info, true) == k.text &&
              dev::CvarText(info, false).empty() && info.phase == k.phase,
          "l107: each key writes its text to its cvar in its phase; =false nothing");
    Check(IsNeverWritten(k.cvar) && Find(k.cvar) == nullptr,
          "l107: the cvar is never written by the m10 defaults");
    for (const auto &entry : kDefaults) {
      Check(!KeyLineWins(entry, info, true, true) && !KeyLineWins(entry, info, std::nullopt, false),
            "l107: no l107 line wins over any m10 default");
    }
  }
  Check(kDefaults.size() == 20 && kNeverWrittenCvars.size() == 133,
        "l107: seventeen defaults (l111: twenty), 115 never-written cvars (105 + 3 + 5 SDK + "
        "l108 2; l111: 117)");
  // The l107 SDK keys write never-written SDK cvars and win over no default.
  const struct {
    dev::Key key;
    const char *cvar;
    const char *text;
    dev::Phase phase;
  } sdk_keys[] = {
      // force_hidef: an l111 default since l111 (TestL111Defaults).
      {dev::Key::kResolveFillOriginOnly, "resolve_fill_half_pixel_only_at_origin", "true",
       dev::Phase::kGraphicsPlugin},
      {dev::Key::kStencilUniform, "d3d12_stencil_transfer_uniform", "true",
       dev::Phase::kGraphicsPlugin},
      {dev::Key::kStencilUniformVerify, "d3d12_stencil_transfer_uniform_verify", "true",
       dev::Phase::kGraphicsPlugin},
      {dev::Key::kGpuCatEvery4, "d3d12_gpu_category_timing_every", "4",
       dev::Phase::kGraphicsPlugin},
  };
  for (const auto &k : sdk_keys) {
    const auto &info = dev::Info(k.key);
    Check(dev::CvarName(info) == k.cvar && dev::CvarText(info, true) == k.text &&
              dev::CvarText(info, false).empty() && info.phase == k.phase,
          "l107 SDK: each key writes its text to its cvar in its phase; =false nothing");
    Check(IsNeverWritten(k.cvar) && Find(k.cvar) == nullptr,
          "l107 SDK: the cvar is never written by the m10 defaults");
    for (const auto &entry : kDefaults) {
      Check(!KeyLineWins(entry, info, true, true) && !KeyLineWins(entry, info, std::nullopt, false),
            "l107 SDK: no line wins over any m10 default");
    }
  }
  // The l107 arms (pc/scene-1x, pc/scene-2x and their FXAA / GPU-category
  // variants, pc/traffic-probe, pc/resolve-fill-off) leave every m10 default
  // as it is, on both platforms.
  const std::string arms[] = {
      "guest_msaa_scene_1x=true\n",
      "guest_msaa_scene_2x=true\n",
      "guest_msaa_scene_1x=true\npresent_fxaa=true\n",
      "guest_msaa_scene_2x=true\ngpu_cat=true\n",
      "guest_msaa_scene_1x=true\r\ngpu_cat=true\r\n",
      "traffic_motion_probe=true\n",
      "resolve_fill_off=true\n",
      "force_hidef=true\n",
      "force_hidef=true\ngpu_cat=true\ngpu_cat_every4=true\n",
      "force_hidef=true\npresent_fxaa=true\n",
      "resolve_fill_origin_only=true\n",
      "stencil_uniform_verify=true\n",
      "stencil_uniform=true\nstencil_uniform_verify=true\n",
      "force_hidef=true\nstencil_uniform=true\n",
  };
  for (const bool wine : {false, true}) {
    for (const std::string &arm : arms) {
      for (const auto &entry : kDefaults) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(entry, arm);
        Check(!in.dev_override_applied || L111Default(entry),
              "l107 arms touch no m10 default (l111: force_hidef is its default's own arm)");
      }
    }
  }
}

void TestL108Keys() {
  // l108: the transfer elision and its verify write never-written SDK cvars:
  // no m10 default changes, and neither ever wins over a default.
  const struct {
    dev::Key key;
    const char *cvar;
  } keys[] = {
      // xfer_elide: an l111 Windows default since l111 (TestL111Defaults).
      {dev::Key::kXferElideVerify, "d3d12_transfer_elide_full_overwrite_verify"},
  };
  for (const auto &k : keys) {
    const auto &info = dev::Info(k.key);
    Check(dev::CvarName(info) == k.cvar && dev::CvarText(info, true) == "true" &&
              dev::CvarText(info, false).empty() && info.phase == dev::Phase::kGraphicsPlugin,
          "l108: each key writes true to its cvar after the plugin loads; =false nothing");
    Check(IsNeverWritten(k.cvar) && Find(k.cvar) == nullptr,
          "l108: the cvar is never written by the m10 defaults");
    for (const auto &entry : kDefaults) {
      Check(!KeyLineWins(entry, info, true, true) && !KeyLineWins(entry, info, std::nullopt, false),
            "l108: no l108 line wins over any m10 default");
    }
  }
  Check(kDefaults.size() == 20 && kNeverWrittenCvars.size() == 133,
        "l108: seventeen defaults (l111: twenty), 115 never-written cvars (113 + 2 SDK; l111: "
        "117)");
  // The l108 arms leave every m10 default as it is, on both platforms.
  const std::string arms[] = {
      "force_hidef=true\nxfer_elide=true\n",
      "force_hidef=true\nxfer_elide_verify=true\ngpu_cat=true\ngpu_cat_every4=true\n",
      "force_hidef=true\nxfer_elide=true\ngpu_cat=true\ngpu_cat_every4=true\n",
      "xfer_elide=true\n",
      "xfer_elide=true\r\ngpu_cat=true\r\ngpu_cat_every4=true\r\n",
      "xfer_elide_verify=true\n",
  };
  for (const bool wine : {false, true}) {
    for (const std::string &arm : arms) {
      for (const auto &entry : kDefaults) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(entry, arm);
        Check(!in.dev_override_applied || L111Default(entry),
              "l108 arms touch no m10 default (l111: their keys are the defaults' own arms)");
      }
    }
  }
}

void TestL109Keys() {
  // l109: the collector key and the two SDK diagnostics write never-written
  // cvars; the fps caps write mcla_guest_vblank_hz, which the m10 defaults
  // never write either. No l109 line wins over any m10 default.
  const struct {
    dev::Key key;
    const char *cvar;
  } keys[] = {
      {dev::Key::kFpsCap30, "mcla_guest_vblank_hz"},
      {dev::Key::kFpsCap40, "mcla_guest_vblank_hz"},
      {dev::Key::kFpsCap60, "mcla_guest_vblank_hz"},
      {dev::Key::kCollector2x, "mcla_guest_collector_2x"},
      {dev::Key::kXferTrigProfile, "d3d12_transfer_trigger_profile"},
      {dev::Key::kStencilLazyVerify, "d3d12_stencil_transfer_lazy_verify"},
  };
  for (const auto &k : keys) {
    const auto &info = dev::Info(k.key);
    Check(dev::CvarName(info) == k.cvar && dev::CvarText(info, false).empty(),
          "l109: each key writes its cvar; =false nothing");
    Check(IsNeverWritten(k.cvar) && Find(k.cvar) == nullptr,
          "l109: the cvar is never written by the m10 defaults");
    for (const auto &entry : kDefaults) {
      Check(!KeyLineWins(entry, info, true, true) && !KeyLineWins(entry, info, std::nullopt, false),
            "l109: no l109 line wins over any m10 default");
    }
  }
  Check(kDefaults.size() == 20 && kNeverWrittenCvars.size() == 133,
        "l109: seventeen defaults (l111: twenty), 118 never-written cvars (115 + 1 host + 2 SDK; "
        "l110: 120; l111: 117)");
  const std::string arms[] = {
      "force_hidef=true\nxfer_elide_verify=true\ngpu_cat=true\ngpu_cat_every4=true\n"
      "xfer_trig_profile=true\nstencil_lazy_verify=true\n",
      "force_hidef=true\nxfer_elide=true\n",
      "force_hidef=true\ncollector_2x=true\n",
      "force_hidef=true\ncollector_2x=true\ngpu_cat=true\ngpu_cat_every4=true\n",
      "fps_cap_40=true\n",
      "fps_cap_60=true\r\n",
      "force_hidef=true\nfps_cap_40=true\n",
      "xfer_elide_verify=true\ngpu_cat=true\ngpu_cat_every4=true\nxfer_trig_profile=true\n"
      "stencil_lazy_verify=true\n",
  };
  for (const bool wine : {false, true}) {
    for (const std::string &arm : arms) {
      for (const auto &entry : kDefaults) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(entry, arm);
        Check(!in.dev_override_applied || L111Default(entry),
              "l109 arms touch no m10 default (l111: their keys are the defaults' own arms)");
      }
    }
  }
}

void TestL110Keys() {
  // l110: the two SDK draw extent keys write never-written cvars; no l110 line
  // wins over any m10 default.
  const struct {
    dev::Key key;
    const char *cvar;
  } keys[] = {
      // xfer_extent: an l111 Windows default since l111 (TestL111Defaults).
      {dev::Key::kXferExtentVerify, "d3d12_draw_extent_from_rect_verify"},
  };
  for (const auto &k : keys) {
    const auto &info = dev::Info(k.key);
    Check(dev::CvarName(info) == k.cvar && dev::CvarText(info, false).empty(),
          "l110: each key writes its cvar; =false nothing");
    Check(IsNeverWritten(k.cvar) && Find(k.cvar) == nullptr,
          "l110: the cvar is never written by the m10 defaults");
    for (const auto &entry : kDefaults) {
      Check(!KeyLineWins(entry, info, true, true) && !KeyLineWins(entry, info, std::nullopt, false),
            "l110: no l110 line wins over any m10 default");
    }
  }
  Check(kDefaults.size() == 20 && kNeverWrittenCvars.size() == 133,
        "l110: seventeen defaults (l111: twenty), 120 never-written cvars (118 + 2 SDK; l111: "
        "117)");
  const std::string arms[] = {
      "force_hidef=true\nxfer_elide_verify=true\nxfer_extent_verify=true\ngpu_cat=true\n"
      "gpu_cat_every4=true\nxfer_trig_profile=true\n",
      "force_hidef=true\nxfer_elide=true\nxfer_extent=true\n",
      "force_hidef=true\nxfer_elide=true\nxfer_extent=true\ngpu_cat=true\ngpu_cat_every4=true\n",
      "force_hidef=true\nxfer_elide=true\nxfer_extent=true\ncollector_2x=true\n",
      "xfer_elide=true\nxfer_extent=true\n",
      "xfer_elide_verify=true\nxfer_extent_verify=true\ngpu_cat=true\ngpu_cat_every4=true\n"
      "xfer_trig_profile=true\n",
  };
  for (const bool wine : {false, true}) {
    for (const std::string &arm : arms) {
      for (const auto &entry : kDefaults) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(entry, arm);
        Check(!in.dev_override_applied || L111Default(entry),
              "l110 arms touch no m10 default (l111: their keys are the defaults' own arms)");
      }
    }
  }
}

// l111 defaults (owner decision 2026-11-18): the forced HiDef mode on both
// platforms and the ownership-transfer elision and extent bound on Windows
// only, and their controls, as TestL104Defaults.
void TestL111Defaults() {
  const Default *hidef = Find("video_mode_force_hidef");
  const Default *elide = Find("d3d12_transfer_elide_full_overwrite");
  const Default *extent = Find("d3d12_draw_extent_from_rect");
  const Default *const entries[] = {hidef, elide, extent};
  for (const Default *entry : entries) {
    // l115: all three hold true again (the bound was false in l112-l114,
    // KI-084).
    Check(entry != nullptr && entry->value == "true" && !entry->diagnostic &&
              !entry->silent_false_is_arm && !IsNeverWritten(entry->cvar) &&
              HasOverrideKey(*entry),
          "l111/l115: hidef, elide and the bound true; not diagnostics, =false not an arm, "
          "keyed");
  }
  // l115: the bound's on key writes the entry's value (pc/extent-new equals the
  // defaults), the off key the other value (the fallback arm pc/extent-off).
  Check(dev::CvarText(dev::Info(dev::Key::kXferExtent), true) == extent->value &&
            dev::CvarText(dev::Info(dev::Key::kXferExtentOff), true) == "false",
        "l115: xfer_extent writes the default true, xfer_extent_off false (the fallback)");
  Check(hidef->phase == Phase::kStartup && hidef->platform == Platform::kAll,
        "l111 A: forced HiDef at startup (init-only, as force_hidef), both platforms");
  Check(elide->phase == Phase::kGraphicsPlugin && extent->phase == Phase::kGraphicsPlugin &&
            elide->platform == Platform::kWindowsOnly &&
            extent->platform == Platform::kWindowsOnly,
        "l111 B/C: elision and extent bound at graphics-plugin load, Windows only");
  Check(kWindowsOnlyCvars.size() == 2 && kWindowsOnlyCvars[0] == elide->cvar &&
            kWindowsOnlyCvars[1] == extent->cvar,
        "l111: the Windows-only list names exactly the elision and the bound");
  size_t windows_only = 0;
  for (const auto &entry : kDefaults) {
    windows_only += WindowsOnly(entry) ? 1 : 0;
  }
  Check(windows_only == kWindowsOnlyCvars.size(), "exactly the listed cvars are Windows-only");
  // The verifies stay never written: owner keys only (they win in the SDK).
  Check(IsNeverWritten("d3d12_transfer_elide_full_overwrite_verify") &&
            IsNeverWritten("d3d12_draw_extent_from_rect_verify") &&
            Find("d3d12_transfer_elide_full_overwrite_verify") == nullptr &&
            Find("d3d12_draw_extent_from_rect_verify") == nullptr &&
            IsNeverWritten("d3d12_stencil_transfer_uniform") &&
            IsNeverWritten("mcla_guest_collector_2x") && kNeverWrittenCvars.size() == 133,
        "l111: the verifies, the stencil skip and the collector stay never written (117)");
  const struct {
    const Default *entry;
    dev::Key key;
    const char *text;
    dev::Phase phase;
  } texts[] = {
      {hidef, dev::Key::kForceHidef, "true", dev::Phase::kStartup},
      {hidef, dev::Key::kForceHidefOff, "false", dev::Phase::kStartup},
      {elide, dev::Key::kXferElide, "true", dev::Phase::kGraphicsPlugin},
      {elide, dev::Key::kXferElideOff, "false", dev::Phase::kGraphicsPlugin},
      {extent, dev::Key::kXferExtent, "true", dev::Phase::kGraphicsPlugin},
      {extent, dev::Key::kXferExtentOff, "false", dev::Phase::kGraphicsPlugin},
  };
  for (const auto &t : texts) {
    const auto &info = dev::Info(t.key);
    Check(dev::CvarName(info) == t.entry->cvar && dev::CvarText(info, true) == t.text &&
              dev::CvarText(info, false).empty() && info.phase == t.phase,
          "l111: each key of a new default writes its text to that cvar in its phase; =false "
          "nothing");
    Check(KeyLineWins(*t.entry, info, true, true), "an applied l111 key wins");
    Check(!KeyLineWins(*t.entry, info, std::nullopt, false), "an l111 key =false line does not win");
    Check(!KeyLineWins(*t.entry, info, std::nullopt, true),
          "an l111 key =true line that was not applied (cvar unavailable) does not win");
    Check(!KeyLineWins(*t.entry, info, std::nullopt, std::nullopt), "no line: no win");
  }
  Check(!KeyLineWins(*elide, dev::Info(dev::Key::kXferElideVerify), true, true) &&
            !KeyLineWins(*extent, dev::Info(dev::Key::kXferExtentVerify), true, true) &&
            !KeyLineWins(*extent, dev::Info(dev::Key::kXferElide), true, true) &&
            !KeyLineWins(*elide, dev::Info(dev::Key::kXferExtentOff), true, true) &&
            !KeyLineWins(*hidef, dev::Info(dev::Key::kGuestMsaaScene2x), true, true) &&
            !KeyLineWins(*extent, dev::Info(dev::Key::kStencilUniform), true, true),
        "a key of another cvar (the verifies among them) never wins over an l111 default");
  // Owner build (measurement build): the l111 arms. Order of the expected
  // actions: hidef, elide, extent; on Windows and under Wine/Proton (the Deck).
  const std::string base =
      "mcla_readback_sync_composite=true\nmcla_native_d3d_refcount=true\n"
      "mcla_native_xapi_interlocked=true\nd3d12_wrm_event_wake=true\n"
      "d3d12_replay_batched_wake=true\nd3d12_readback_elide_unread=true\n"
      "prewarm_seed_store=true\nd3d12_readback_sync_targeted=true\n"
      "memory_watch_census=false\nmcla_warp_probe=true\nmcla_album_probe=true\n"
      "mcla_pursuit_probe=true\n";
  const std::string deck =
      "mcla_readback_sync_composite=true\nmcla_native_d3d_refcount=true\n"
      "mcla_native_xapi_interlocked=true\nd3d12_wrm_event_wake=true\n"
      "d3d12_replay_batched_wake=true\nd3d12_readback_elide_unread=true\n"
      "prewarm_seed_store=true\nd3d12_readback_sync_targeted=true\n"
      "memory_watch_census=false\n";
  const Action ap = Action::kApply;
  const Action dv = Action::kDevOverride;
  const Action wo = Action::kWindowsOnly;
  struct Arm {
    std::string file;
    Action windows[3];
    Action wine[3];
    const char *message;
  };
  const Arm arms[] = {
      {"", {ap, ap, ap}, {ap, wo, wo}, "no file: the l111 defaults apply (Deck: HiDef only)"},
      {base, {ap, ap, ap}, {ap, wo, wo}, "the sparse l111 pc/default (clear) arm"},
      {deck, {ap, ap, ap}, {ap, wo, wo}, "the sparse l111 deck/default arm"},
      {base + "force_hidef_off=true\nxfer_elide_off=true\nxfer_extent_off=true\n",
       {dv, dv, dv}, {dv, wo, wo}, "pc/gpu-old: the three controls win (the l110 default)"},
      {base + "xfer_elide_off=true\nxfer_extent_off=true\n", {ap, dv, dv}, {ap, wo, wo},
       "pc/hidef-only: HiDef stays, the elision and the bound are off"},
      {base + "xfer_extent_off=true\n", {ap, ap, dv}, {ap, wo, wo},
       "pc/extent-off: only the bound is off"},
      {base + "xfer_elide_verify=true\nxfer_extent_verify=true\ngpu_cat=true\n"
              "gpu_cat_every4=true\nxfer_trig_profile=true\n",
       {ap, ap, ap}, {ap, wo, wo},
       "pc/verify-gpu: the defaults stay written; the verifies win in the SDK"},
      {base + "gpu_cat=true\ngpu_cat_every4=true\n", {ap, ap, ap}, {ap, wo, wo},
       "pc/gpu-cat: the defaults with category timing"},
      {deck + "xfer_elide_verify=true\nxfer_extent_verify=true\ngpu_cat=true\n"
              "gpu_cat_every4=true\n",
       {ap, ap, ap}, {ap, wo, wo},
       "deck/verify-gpu: on the Deck the elision and the bound stay off, the verifies audit"},
      {deck + "xfer_elide=true\nxfer_extent=true\n", {ap, dv, dv}, {ap, wo, wo},
       "deck/gpu-new: the on keys turn the two on under Wine (the module writes nothing)"},
      {base + "force_hidef=true\nxfer_elide=true\nxfer_extent=true\n", {dv, dv, dv},
       {dv, wo, wo}, "the l110 pc/extent body: its keys write the default values (own arms)"},
      {base + "force_hidef=true\n", {dv, ap, ap}, {dv, wo, wo},
       "the l107 pc/hidef body: force_hidef is its own arm"},
      {"force_hidef=false\nforce_hidef_off=false\nxfer_elide=false\nxfer_elide_off=false\n"
       "xfer_extent=false\nxfer_extent_off=false\n",
       {ap, ap, ap}, {ap, wo, wo}, "=false lines of the l111 keys change nothing"},
      {"force_hidef=true\nforce_hidef_off=true\nxfer_elide=true\nxfer_elide_off=true\n"
       "xfer_extent=true\nxfer_extent_off=true\n",
       {ap, ap, ap}, {ap, wo, wo},
       "conflicting l111 keys: neither applied, the build defaults stay"},
      {"collector_2x=true\nstencil_uniform=true\nstencil_lazy_verify=true\n", {ap, ap, ap},
       {ap, wo, wo}, "other transfer keys do not touch the l111 defaults"},
  };
  for (const bool wine : {false, true}) {
    for (const Arm &arm : arms) {
      for (size_t i = 0; i < 3; ++i) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(*entries[i], arm.file);
        Check(Decide(*entries[i], in) == (wine ? arm.wine[i] : arm.windows[i]), arm.message);
      }
    }
    for (const Default *entry : entries) {
      const bool off_platform = wine && WindowsOnly(*entry);
      Inputs in = Fresh(wine);
      Check(Decide(*entry, in) == (off_platform ? Action::kWindowsOnly : Action::kApply),
            "a volunteer build applies every l111 default on its platform");
      in.non_default = true;
      Check(Decide(*entry, in) == (off_platform ? Action::kWindowsOnly : Action::kUserSet),
            "a command-line l111 value is kept");
      in = Fresh(wine);
      in.registered = false;
      Check(Decide(*entry, in) == (off_platform ? Action::kWindowsOnly : Action::kUnavailable),
            "SDK without the cvar (e.g. the gate SDK): unavailable");
      in = Fresh(wine);
      in.holds_value = true;
      Check(Decide(*entry, in) == (off_platform ? Action::kWindowsOnly : Action::kAlready),
            "an SDK whose default equals: already");
      in = Fresh(wine);
      in.switch_on = false;
      Check(Decide(*entry, in) == Action::kOffBySwitch,
            "--mcla_m10_speedup_defaults=false leaves the l111 switches at the SDK defaults");
    }
  }
  // Under Wine/Proton the Windows-only entries are never written, whatever
  // else holds; on Windows they follow the ordinary rules.
  for (const auto cvar : kWindowsOnlyCvars) {
    const Default &entry = *Find(cvar);
    Inputs in = Fresh(true);
    in.dev_override_applied = true;
    in.non_default = true;
    in.measurement_build = true;
    Check(Decide(entry, in) == Action::kWindowsOnly, "Wine: reported as Windows-only");
    in = Fresh(false);
    in.measurement_build = true;
    in.dev_override_applied = true;
    Check(Decide(entry, in) == Action::kDevOverride, "Windows: the owner arm wins");
  }
}

void TestL113Keys() {
  // l113: the two SDK keys write never-written cvars; no l113 line wins over
  // any m10 default (the bound's default stays false, l112).
  const struct {
    dev::Key key;
    const char *cvar;
    const char *text;
  } keys[] = {
      {dev::Key::kXferExtentDeferredOff, "d3d12_draw_extent_deferred_claims", "false"},
      {dev::Key::kVerifyEvery4, "d3d12_verify_query_every", "4"},
  };
  for (const auto &k : keys) {
    const auto &info = dev::Info(k.key);
    Check(dev::CvarName(info) == k.cvar && dev::CvarText(info, true) == k.text &&
              dev::CvarText(info, false).empty(),
          "l113: each key writes its cvar; =false nothing");
    Check(IsNeverWritten(k.cvar) && Find(k.cvar) == nullptr,
          "l113: the cvar is never written by the m10 defaults");
    for (const auto &entry : kDefaults) {
      Check(!KeyLineWins(entry, info, true, true) && !KeyLineWins(entry, info, std::nullopt, false),
            "l113: no l113 line wins over any m10 default");
    }
  }
  const Default *extent = Find("d3d12_draw_extent_from_rect");
  Check(kDefaults.size() == 20 && kNeverWrittenCvars.size() == 133 && extent != nullptr &&
            extent->value == "true",
        "l113/l115: twenty defaults (the bound true again since l115), 120 never-written cvars "
        "(117 + 2 l113 SDK + 1 l114 SDK)");
  const std::string arms[] = {
      "xfer_extent=true\nxfer_extent_deferred_off=true\n",
      "xfer_extent_verify=true\nxfer_trig_profile=true\nverify_every4=true\n",
      "xfer_extent=true\n",
      "xfer_extent_deferred_off=true\nverify_every4=true\n",
  };
  for (const bool wine : {false, true}) {
    for (const std::string &arm : arms) {
      for (const auto &entry : kDefaults) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(entry, arm);
        const bool bound_on = &entry == extent && arm.find("xfer_extent=true") != std::string::npos;
        Check(in.dev_override_applied == bound_on,
              "l113 arms touch no m10 default except the bound's on key");
      }
    }
  }
}

void TestL114Keys() {
  // l114: the exact deferred mode key writes a never-written cvar; no l114 line
  // wins over any m10 default (since l115 the bound's default is true).
  const auto &info = dev::Info(dev::Key::kXferExtentModeExact);
  Check(dev::CvarName(info) == "d3d12_draw_extent_deferred_mode" &&
            dev::CvarText(info, true) == "2" && dev::CvarText(info, false).empty(),
        "l114: the key writes its cvar; =false nothing");
  Check(IsNeverWritten("d3d12_draw_extent_deferred_mode") &&
            Find("d3d12_draw_extent_deferred_mode") == nullptr,
        "l114: the cvar is never written by the m10 defaults");
  for (const auto &entry : kDefaults) {
    Check(!KeyLineWins(entry, info, true, true) && !KeyLineWins(entry, info, std::nullopt, false),
          "l114: no l114 line wins over any m10 default");
  }
  const Default *extent = Find("d3d12_draw_extent_from_rect");
  Check(kDefaults.size() == 20 && kNeverWrittenCvars.size() == 133 && extent != nullptr &&
            extent->value == "true",
        "l114/l115: twenty defaults (the bound true since l115), 120 never-written cvars "
        "(119 + 1 SDK)");
  const std::string arms[] = {
      "xfer_extent=true\nxfer_extent_mode_exact=true\n",
      "xfer_extent_mode_exact=true\n",
  };
  for (const bool wine : {false, true}) {
    for (const std::string &arm : arms) {
      for (const auto &entry : kDefaults) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(entry, arm);
        const bool bound_on = &entry == extent && arm.find("xfer_extent=true") != std::string::npos;
        Check(in.dev_override_applied == bound_on,
              "l114 arms touch no m10 default except the bound's on key");
      }
    }
  }
}

// l115 (owner decision 2026-11-18, the l114 pc/extent-new run without
// flicker): the rectangle draw-extent bound is a Windows default again; the
// resolve-only fix is the SDK default of the never-written deferred mode.
void TestL115Defaults() {
  const Default *hidef = Find("video_mode_force_hidef");
  const Default *elide = Find("d3d12_transfer_elide_full_overwrite");
  const Default *extent = Find("d3d12_draw_extent_from_rect");
  Check(extent != nullptr && extent->value == "true" && extent->phase == Phase::kGraphicsPlugin &&
            extent->platform == Platform::kWindowsOnly && !extent->diagnostic &&
            !extent->silent_false_is_arm && HasOverrideKey(*extent),
        "l115: the bound true, graphics plugin, Windows only (the l111 scope)");
  Check(kDefaults.size() == 20 && kWindowsOnlyCvars.size() == 2 &&
            kNeverWrittenCvars.size() == 133,
        "l115: twenty defaults, two Windows-only cvars, 120 never-written cvars (unchanged)");
  for (const auto cvar : {"d3d12_draw_extent_deferred_mode", "d3d12_draw_extent_deferred_claims",
                          "d3d12_verify_query_every", "d3d12_draw_extent_from_rect_verify"}) {
    Check(IsNeverWritten(cvar) && Find(cvar) == nullptr,
          "l115: the fix's mode, its switch and the verifies stay never written (SDK defaults)");
  }
  // The keys: off is the fallback, the others write other cvars (no win).
  Check(KeyLineWins(*extent, dev::Info(dev::Key::kXferExtentOff), true, true) &&
            KeyLineWins(*extent, dev::Info(dev::Key::kXferExtent), true, true) &&
            !KeyLineWins(*extent, dev::Info(dev::Key::kXferExtentDeferredOff), true, true) &&
            !KeyLineWins(*extent, dev::Info(dev::Key::kXferExtentModeExact), true, true) &&
            !KeyLineWins(*extent, dev::Info(dev::Key::kXferExtentVerify), true, true) &&
            !KeyLineWins(*extent, dev::Info(dev::Key::kVerifyEvery4), true, true),
        "l115: xfer_extent_off and xfer_extent win, the mode / switch / verify keys do not");
  const std::string base =
      "mcla_readback_sync_composite=true\nmcla_native_d3d_refcount=true\n"
      "mcla_native_xapi_interlocked=true\nd3d12_wrm_event_wake=true\n"
      "d3d12_replay_batched_wake=true\nd3d12_readback_elide_unread=true\n"
      "prewarm_seed_store=true\nd3d12_readback_sync_targeted=true\n"
      "memory_watch_census=false\nmcla_warp_probe=true\nmcla_album_probe=true\n"
      "mcla_pursuit_probe=true\n";
  const std::string cat = "gpu_cat=true\ngpu_cat_every4=true\n";
  const Action ap = Action::kApply;
  const Action dv = Action::kDevOverride;
  const Action wo = Action::kWindowsOnly;
  const Default *const entries[] = {hidef, elide, extent};
  struct Arm {
    std::string file;
    Action windows[3];
    Action wine[3];
    const char *message;
  };
  const Arm arms[] = {
      {"", {ap, ap, ap}, {ap, wo, wo}, "l115 clear: the bound applied on Windows, not under Wine"},
      {base, {ap, ap, ap}, {ap, wo, wo}, "l115 pc/default: the defaults apply"},
      {base + cat, {ap, ap, ap}, {ap, wo, wo},
       "l115 pc/gpu-cat-race: the defaults with category timing, no verify key"},
      {base + "xfer_extent=true\n", {ap, ap, dv}, {ap, wo, wo},
       "l115 pc/extent-new: its key writes the default value (own arm)"},
      {base + "xfer_extent_off=true\n", {ap, ap, dv}, {ap, wo, wo},
       "l115 pc/extent-off: the fallback, only the bound off"},
      {base + "xfer_extent=true\nxfer_extent_deferred_off=true\n", {ap, ap, dv}, {ap, wo, wo},
       "l115 pc/extent-l111: the bound on, the fix off in the SDK (mode 0)"},
      {base + "xfer_extent=true\nxfer_extent_mode_exact=true\n", {ap, ap, dv}, {ap, wo, wo},
       "l115 pc/extent-exact: the bound on, the exact rule in the SDK (mode 2)"},
      {base + "xfer_extent_deferred_off=true\n", {ap, ap, ap}, {ap, wo, wo},
       "l115: the deferred switch alone touches no m10 default"},
      {base + "xfer_extent_mode_exact=true\n", {ap, ap, ap}, {ap, wo, wo},
       "l115: the exact mode alone touches no m10 default"},
      {base + "xfer_extent_verify=true\nxfer_trig_profile=true\nverify_every4=true\n",
       {ap, ap, ap}, {ap, wo, wo}, "l115 pc/extent-audit: the verify wins in the SDK"},
      {base + "force_hidef_off=true\nxfer_elide_off=true\nxfer_extent_off=true\n",
       {dv, dv, dv}, {dv, wo, wo}, "l115 pc/gpu-old: the three controls win (the l110 default)"},
      {base + "xfer_elide_off=true\nxfer_extent_off=true\n", {ap, dv, dv}, {ap, wo, wo},
       "l115 pc/hidef-only: HiDef stays, the elision and the bound off"},
  };
  for (const bool wine : {false, true}) {
    for (const Arm &arm : arms) {
      for (size_t i = 0; i < 3; ++i) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(*entries[i], arm.file);
        Check(Decide(*entries[i], in) == (wine ? arm.wine[i] : arm.windows[i]), arm.message);
      }
    }
  }
  // The m10 summaries: on Windows the bound is written (applied, was already
  // in l112-l114 because false equals the SDK default); under Wine windows_only.
  Inputs in = Fresh(false);
  Check(Decide(*extent, in) == Action::kApply, "l115 Windows: the bound applied (l114: already)");
  in.holds_value = true;
  Check(Decide(*extent, in) == Action::kAlready, "an SDK whose default is true: already");
  in = Fresh(true);
  Check(Decide(*extent, in) == Action::kWindowsOnly, "l115 Wine: the bound windows_only");
}

// l116 (KI-087): the two guard keys write SDK cvars the m10 defaults never
// write (the guard action since l99, the guard skip new); no l116 line wins
// over any m10 default.
void TestL116Keys() {
  const dev::Key keys[] = {dev::Key::kLeadGuardLatchSession, dev::Key::kLeadGuardSkipOff};
  for (const dev::Key key : keys) {
    const auto &info = dev::Info(key);
    Check(IsNeverWritten(dev::CvarName(info)) && Find(dev::CvarName(info)) == nullptr,
          "l116: the key's cvar is never written by the m10 defaults");
    Check(dev::CvarText(info, false).empty(), "l116: =false writes nothing");
    for (const auto &entry : kDefaults) {
      Check(!KeyLineWins(entry, info, true, true) &&
                !KeyLineWins(entry, info, std::nullopt, false),
            "l116: no l116 line wins over any m10 default");
    }
  }
  Check(IsNeverWritten("d3d12_frame_lead_strict_low_latency_guard_skip") &&
            IsNeverWritten("d3d12_frame_lead_strict_low_latency_guard_action"),
        "l116: the guard skip joins the guard action as never written");
  Check(kDefaults.size() == 20 && kNeverWrittenCvars.size() == 133,
        "l116: twenty defaults (unchanged), 121 never-written cvars (120 + 1 SDK)");
  const std::string arms[] = {
      "lead_guard_latch_session=true\nlead_guard_skip_off=true\n",
      "lead_guard_latch_session=true\n",
      "lead_guard_skip_off=true\n",
  };
  for (const bool wine : {false, true}) {
    for (const std::string &arm : arms) {
      for (const auto &entry : kDefaults) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(entry, arm);
        Check(!in.dev_override_applied, "l116 arms touch no m10 default");
      }
    }
  }
}

// l117: the six per-draw CP lever keys write SDK cvars the m10 defaults never
// write; no l117 line wins over any m10 default.
void TestL117Keys() {
  const dev::Key keys[] = {dev::Key::kCbReuse,       dev::Key::kCbReuseVerify,
                           dev::Key::kIbCache,       dev::Key::kIbCacheVerify,
                           dev::Key::kTexNarrow,     dev::Key::kTexNarrowVerify};
  for (const dev::Key key : keys) {
    const auto &info = dev::Info(key);
    Check(IsNeverWritten(dev::CvarName(info)) && Find(dev::CvarName(info)) == nullptr,
          "l117: the key's cvar is never written by the m10 defaults");
    Check(dev::CvarText(info, false).empty(), "l117: =false writes nothing");
    for (const auto &entry : kDefaults) {
      Check(!KeyLineWins(entry, info, true, true) &&
                !KeyLineWins(entry, info, std::nullopt, false),
            "l117: no l117 line wins over any m10 default");
    }
  }
  Check(kDefaults.size() == 20 && kNeverWrittenCvars.size() == 133,
        "l117: twenty defaults (unchanged), 127 never-written cvars (121 + 6 SDK)");
  const std::string arms[] = {
      "cb_reuse_verify=true\nib_cache_verify=true\ntex_narrow_verify=true\n",
      "cb_reuse=true\nib_cache=true\ntex_narrow=true\n",
      "cb_reuse=true\n",
      "ib_cache=true\n",
      "tex_narrow=true\n",
  };
  for (const bool wine : {false, true}) {
    for (const std::string &arm : arms) {
      for (const auto &entry : kDefaults) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(entry, arm);
        Check(!in.dev_override_applied, "l117 arms touch no m10 default");
      }
    }
  }
}

// l118: the rt_memo2 key writes an SDK cvar the m10 defaults never write; no
// l118 line wins over any m10 default (d3d12_rt_update_memo stays a default).
void TestL118Keys() {
  for (const dev::Key key : {dev::Key::kRtMemo2, dev::Key::kVbSkipValidateOff}) {
    const auto &k = dev::Info(key);
    Check(IsNeverWritten(dev::CvarName(k)) && Find(dev::CvarName(k)) == nullptr,
          "l118: the key's cvar is never written by the m10 defaults");
    Check(dev::CvarText(k, false).empty(), "l118: =false writes nothing");
    for (const auto &entry : kDefaults) {
      Check(!KeyLineWins(entry, k, true, true) && !KeyLineWins(entry, k, std::nullopt, false),
            "l118: no l118 line wins over any m10 default");
    }
  }
  const auto &info = dev::Info(dev::Key::kRtMemo2);
  Check(Find("d3d12_rt_update_memo") != nullptr && !IsNeverWritten("d3d12_rt_update_memo"),
        "l118: the l101 memo is still an m10 default");
  for (const auto &entry : kDefaults) {
    Check(!KeyLineWins(entry, info, true, true) && !KeyLineWins(entry, info, std::nullopt, false),
          "l118: no l118 line wins over any m10 default");
  }
  Check(kDefaults.size() == 20 && kNeverWrittenCvars.size() == 133,
        "l118: twenty defaults (unchanged), 133 never-written cvars (since l120)");
  const std::string arms[] = {
      "rt_memo2=true\nrt_memo_verify=true\n",
      "rt_memo2=true\n",
      "guest_msaa_scene_1x=true\n",
      "guest_msaa_scene_1x=true\npresent_fxaa=true\n",
      "vb_skip_validate_off=true\n",
  };
  for (const bool wine : {false, true}) {
    for (const std::string &arm : arms) {
      for (const auto &entry : kDefaults) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(entry, arm);
        Check(!in.dev_override_applied, "l118 arms touch no m10 default");
      }
    }
  }
}

// l119: the shm_census key and the inval_unwatched_off control write SDK cvars
// the m10 defaults never write; no l119 line wins over any m10 default (the
// census stays off in volunteer builds: nothing writes it there).
void TestL119Keys() {
  for (const dev::Key key : {dev::Key::kShmCensus, dev::Key::kInvalUnwatchedOff}) {
    const auto &k = dev::Info(key);
    Check(IsNeverWritten(dev::CvarName(k)) && Find(dev::CvarName(k)) == nullptr,
          "l119: the key's cvar is never written by the m10 defaults");
    Check(dev::CvarText(k, false).empty(), "l119: =false writes nothing");
    for (const auto &entry : kDefaults) {
      Check(!KeyLineWins(entry, k, true, true) && !KeyLineWins(entry, k, std::nullopt, false),
            "l119: no l119 line wins over any m10 default");
    }
  }
  Check(Find("memory_watch_census") != nullptr && Find("memory_watch_census")->diagnostic,
        "l119: the M10 watch census stays a diagnostic default (left at the SDK default in "
        "owner builds)");
  Check(kDefaults.size() == 20 && kNeverWrittenCvars.size() == 133,
        "l119: twenty defaults (unchanged), 133 never-written cvars (since l120)");
  const std::string arms[] = {
      "shm_census=true\n",
      "shm_census=true\nvb_skip_validate_off=true\n",
      "inval_unwatched_off=true\n",
  };
  for (const bool wine : {false, true}) {
    for (const std::string &arm : arms) {
      for (const auto &entry : kDefaults) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(entry, arm);
        Check(!in.dev_override_applied, "l119 arms touch no m10 default");
      }
    }
  }
}

// l120: the shm_hot and shm_hot_verify keys write SDK cvars the m10 defaults
// never write; no l120 line wins over any m10 default (the lever stays off in
// volunteer builds: nothing writes it there).
void TestL120Keys() {
  for (const dev::Key key : {dev::Key::kShmHot, dev::Key::kShmHotVerify}) {
    const auto &k = dev::Info(key);
    Check(IsNeverWritten(dev::CvarName(k)) && Find(dev::CvarName(k)) == nullptr,
          "l120: the key's cvar is never written by the m10 defaults");
    Check(dev::CvarText(k, false).empty(), "l120: =false writes nothing");
    for (const auto &entry : kDefaults) {
      Check(!KeyLineWins(entry, k, true, true) && !KeyLineWins(entry, k, std::nullopt, false),
            "l120: no l120 line wins over any m10 default");
    }
  }
  Check(kDefaults.size() == 20 && kNeverWrittenCvars.size() == 133,
        "l120: twenty defaults (unchanged), 133 never-written cvars (131 + 2 SDK)");
  const std::string arms[] = {
      "shm_hot_verify=true\nshm_census=true\n",
      "shm_hot=true\n",
      "shm_hot=true\nguest_msaa_scene_1x=true\n",
  };
  for (const bool wine : {false, true}) {
    for (const std::string &arm : arms) {
      for (const auto &entry : kDefaults) {
        Inputs in = Fresh(wine);
        in.measurement_build = true;
        in.dev_override_applied = SimulatedOverrideApplied(entry, arm);
        Check(!in.dev_override_applied, "l120 arms touch no m10 default");
      }
    }
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
  tally.Add(Action::kWindowsOnly);
  Check(tally.Count(Action::kWindowsOnly) == 1 && tally.Count(Action::kNotThisPlatform) == 1,
        "Windows-only count");
  Check(std::string_view(mcla::m10_defaults::ActionName(Action::kWindowsOnly)) == "windows_only",
        "Windows-only action name");
  Check(mcla::m10_defaults::kActionCount == 10, "ten actions");
  Check(mcla::m10_defaults::ValueText(false) == "false", "value text");
}
}  // namespace

int main() {
  TestTable();
  TestPlatform();
  TestDecision();
  TestExhaustive();
  TestFrameLeadStrictDefault();
  TestLever1Defaults();
  TestL104Defaults();
  TestL105Keys();
  TestL106Keys();
  TestL107Keys();
  TestL108Keys();
  TestL109Keys();
  TestL110Keys();
  TestL111Defaults();
  TestL113Keys();
  TestL114Keys();
  TestL115Defaults();
  TestL116Keys();
  TestL117Keys();
  TestL118Keys();
  TestL119Keys();
  TestL120Keys();
  TestTally();
  std::printf("mcla_m10_defaults_test: %d checks passed\n", checks);
  return 0;
}
