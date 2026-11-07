#include "mcla_m10_defaults.h"

#include <cstdio>
#include <cstdlib>
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
  };
  Check(std::size(expected) == kDefaults.size(), "eight defaults");
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
  // census, disarm bridge, targeted, elide, wrm, batched, upload 16 and 64.
  Check(linked == 8, "eight override keys write default cvars");
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
            kNeverWrittenCvars.size() == 45,
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
            kNeverWrittenCvars.size() == 45,
        "M11-007d cvar names in the never-written set (45)");
  Check(IsNeverWritten("mcla_album_preview_restore") &&
            IsNeverWritten(dev::CvarName(dev::Info(dev::Key::kAlbumPreviewRestore))) &&
            Find("mcla_album_preview_restore") == nullptr,
        "the KI-056 fix A restore is never a volunteer default (owner key only)");
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
  // wine. Four keyed speed-ups: 2 x 2 each (applied false); census (keyed,
  // diagnostic): 1 x 2; GPU timing (no key, diagnostic): 2 x 2; the two keyed
  // Wine-only speed-ups: 2 each (wine true, applied false).
  Check(applies == 4 * 4 + 2 + 4 + 2 * 2, "apply case count");
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
  TestTally();
  std::printf("mcla_m10_defaults_test: %d checks passed\n", checks);
  return 0;
}
