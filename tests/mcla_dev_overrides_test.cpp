#include "mcla_dev_overrides.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {
int checks = 0;
void Check(bool value, const char *message) {
  if (!value) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
  ++checks;
}

using mcla::dev_overrides::ApplyAction;
using mcla::dev_overrides::DecideApply;
using mcla::dev_overrides::FileStatus;
using mcla::dev_overrides::FindKey;
using mcla::dev_overrides::Key;
using mcla::dev_overrides::LineIssue;
using mcla::dev_overrides::Parse;
using mcla::dev_overrides::Parsed;
using mcla::dev_overrides::Phase;

bool NoValues(const Parsed &parsed) {
  return parsed.ValueCount() == 0;
}

void TestWhitelist() {
  Check(mcla::dev_overrides::kKeyCount == 59, "fifty-nine whitelisted names");
  {
    // l77 KI-074 keys: the raw timer switch (startup) and the three vblank
    // caps (graphics plugin, a conflict group, replacing the launcher value).
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const auto &raw = Info(Key::kRawTimerOff);
    Check(FindKey("raw_timer_off") == Key::kRawTimerOff && raw.phase == Phase::kStartup &&
              CvarName(raw) == "mcla_raw_timer" && CvarText(raw, true) == "false" &&
              CvarText(raw, false).empty() && raw.true_source == TextSource::kLiteral &&
              !raw.overrides_user,
          "raw_timer_off=true writes false to mcla_raw_timer at startup, false writes nothing");
    const Key cap_keys[] = {Key::kVblankHz72, Key::kVblankHz80, Key::kVblankHz88};
    const char *const cap_names[] = {"vblank_hz_72", "vblank_hz_80", "vblank_hz_88"};
    const char *const cap_values[] = {"72", "80", "88"};
    for (size_t k = 0; k < 3; ++k) {
      const auto &cap = Info(cap_keys[k]);
      Check(FindKey(cap_names[k]) == cap_keys[k] && cap.phase == Phase::kGraphicsPlugin &&
                CvarName(cap) == "mcla_guest_vblank_hz" && CvarText(cap, true) == cap_values[k] &&
                CvarText(cap, false).empty() && cap.true_source == TextSource::kLiteral &&
                cap.overrides_user,
            "vblank_hz_N=true writes N after the plugin loaded (replacing the launcher value), "
            "false writes nothing");
    }
    size_t overriding = 0;
    for (const auto &info : mcla::dev_overrides::kKeys) {
      overriding += info.overrides_user ? 1 : 0;
    }
    Check(overriding == 3, "only the three vblank keys replace a command-line value");
    size_t cap_pairs = 0;
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      Check(pair.first != Key::kRawTimerOff && pair.second != Key::kRawTimerOff,
            "the raw timer key is in no conflict pair");
      const bool first_cap = pair.first == Key::kVblankHz72 || pair.first == Key::kVblankHz80 ||
                             pair.first == Key::kVblankHz88;
      const bool second_cap = pair.second == Key::kVblankHz72 ||
                              pair.second == Key::kVblankHz80 || pair.second == Key::kVblankHz88;
      Check(first_cap == second_cap, "vblank keys pair only with each other");
      cap_pairs += first_cap ? 1 : 0;
    }
    Check(cap_pairs == 3, "the vblank group is listed as its three pairs");
    Check(!FindKey("raw_timer") && !FindKey("raw_timer_on") && !FindKey("mcla_raw_timer") &&
              !FindKey("mcla_raw_timer_off") && !FindKey("RAW_TIMER_OFF") &&
              !FindKey("vblank_hz_60") && !FindKey("vblank_hz_120") &&
              !FindKey("vblank_hz_240") && !FindKey("vblank_hz_7") && !FindKey("vblank_hz") &&
              !FindKey("mcla_guest_vblank_hz") && !FindKey("vblank_hz_72_on") &&
              !FindKey("VBLANK_HZ_72") && !FindKey("vblank_72"),
          "no prefix, suffix, cvar-name, case or other numeric variants of the KI-074 keys");
  }
  {
    // l77 keys: M11-007e deferred fence writes (graphics plugin) and the
    // KI-071 walk fix control (startup); =false writes nothing for both.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const auto &fence = Info(Key::kFenceWriteDeferred);
    Check(FindKey("fence_write_deferred") == Key::kFenceWriteDeferred &&
              fence.phase == Phase::kGraphicsPlugin &&
              CvarName(fence) == "d3d12_fence_write_deferred" &&
              CvarText(fence, true) == "true" && CvarText(fence, false).empty() &&
              fence.true_source == TextSource::kLiteral,
          "fence_write_deferred=true writes true to d3d12_fence_write_deferred after the "
          "plugin loaded, false writes nothing");
    const auto &walk_off = Info(Key::kXmaFrameWalkFixOff);
    Check(FindKey("xma_frame_walk_fix_off") == Key::kXmaFrameWalkFixOff &&
              walk_off.phase == Phase::kStartup && CvarName(walk_off) == "xma_frame_walk_fix" &&
              CvarText(walk_off, true) == "false" && CvarText(walk_off, false).empty() &&
              walk_off.true_source == TextSource::kLiteral,
          "xma_frame_walk_fix_off=true writes false to xma_frame_walk_fix at startup, false "
          "writes nothing");
    Check(Info(Key::kXmaFrameWalkFix).phase == walk_off.phase &&
              CvarName(Info(Key::kXmaFrameWalkFix)) == CvarName(walk_off),
          "the walk fix key and its control write one cvar in one phase");
    size_t walk_pairs = 0;
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      Check(pair.first != Key::kFenceWriteDeferred && pair.second != Key::kFenceWriteDeferred,
            "the fence key is in no conflict pair");
      if (pair.first == Key::kXmaFrameWalkFixOff || pair.second == Key::kXmaFrameWalkFixOff) {
        Check(pair.first == Key::kXmaFrameWalkFix && pair.second == Key::kXmaFrameWalkFixOff,
              "the walk fix control pairs only with the walk fix key");
        ++walk_pairs;
      }
    }
    Check(walk_pairs == 1, "one walk fix conflict pair");
    Check(!FindKey("fence_write_deferred_on") && !FindKey("fence_write_deferred_off") &&
              !FindKey("fence_deferred") && !FindKey("fence_write") &&
              !FindKey("d3d12_fence_write_deferred") && !FindKey("FENCE_WRITE_DEFERRED") &&
              !FindKey("fence_writes_deferred") && !FindKey("xma_frame_walk_fix_of") &&
              !FindKey("xma_frame_walk_fix_off_on") && !FindKey("xma_walk_fix_off") &&
              !FindKey("XMA_FRAME_WALK_FIX_OFF") && !FindKey("xma_frame_walk_off"),
          "no prefix, suffix, cvar-name or case variants of the l77 keys");
  }
  {
    // M11-007d keys: host init-only bools at startup, true/false written.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    const Key lock_keys[] = {Key::kLockProbe, Key::kLockReadbackEnsure};
    const char *const lock_names[] = {"mcla_lock_probe", "mcla_lock_readback_ensure"};
    for (size_t k = 0; k < 2; ++k) {
      Check(FindKey(lock_names[k]) == lock_keys[k], "M11-007d key lookup");
      Check(Info(lock_keys[k]).phase == Phase::kStartup,
            "M11-007d keys at startup (host init-only cvars, before the guest)");
      Check(CvarName(Info(lock_keys[k])) == lock_names[k] &&
                CvarText(Info(lock_keys[k]), true) == "true" &&
                CvarText(Info(lock_keys[k]), false) == "false",
            "M11-007d key writes its own cvar true/false");
    }
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      for (const Key key : lock_keys) {
        Check(pair.first != key && pair.second != key, "M11-007d keys are in no conflict pair");
      }
    }
    Check(!FindKey("mcla_lock") && !FindKey("lock_probe") && !FindKey("mcla_lock_probe_on") &&
              !FindKey("mcla_lock_ensure") && !FindKey("mcla_lock_readback") &&
              !FindKey("lock_readback_ensure") && !FindKey("MCLA_LOCK_PROBE") &&
              !FindKey("mcla_lock_readback_ensure_on") && !FindKey("readback_ensure"),
          "no prefix, suffix or case variants of the M11-007d keys");
  }
  {
    // KI-071 step 4 keys: three SDK bools at startup (=false writes nothing).
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const Key walk_keys[] = {Key::kXmaFrameWalkTrace, Key::kXmaFrameWalkFix,
                             Key::kXmaDumpInput};
    const char *const walk_names[] = {"xma_frame_walk_trace", "xma_frame_walk_fix",
                                      "xma_dump_input"};
    for (size_t k = 0; k < 3; ++k) {
      Check(FindKey(walk_names[k]) == walk_keys[k], "KI-071 step 4 key lookup");
      Check(Info(walk_keys[k]).phase == Phase::kStartup,
            "step 4 SDK keys at startup (rexruntime cvars)");
      Check(CvarName(Info(walk_keys[k])) == walk_names[k] &&
                CvarText(Info(walk_keys[k]), true) == "true" &&
                CvarText(Info(walk_keys[k]), false).empty() &&
                Info(walk_keys[k]).true_source == TextSource::kLiteral,
            "step 4 SDK key writes its own cvar true, false writes nothing");
    }
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      for (const Key key : {Key::kXmaFrameWalkTrace, Key::kXmaDumpInput}) {
        Check(pair.first != key && pair.second != key,
              "step 4 trace / input dump keys are in no conflict pair");
      }
      Check((pair.first != Key::kXmaFrameWalkFix && pair.second != Key::kXmaFrameWalkFix) ||
                (pair.first == Key::kXmaFrameWalkFix && pair.second == Key::kXmaFrameWalkFixOff),
            "the walk fix key pairs only with its l77 control");
    }
    Check(!FindKey("xma_frame_walk") && !FindKey("xma_frame_walk_trace_on") &&
              !FindKey("xma_frame_walk_fix_on") && !FindKey("xma_walk_trace") &&
              !FindKey("xma_walk_fix") && !FindKey("XMA_FRAME_WALK_FIX") &&
              !FindKey("xma_dump_input_on") && !FindKey("xma_dump_inputs") &&
              !FindKey("xma_input_dump") && !FindKey("xma_frame_walk_tracer"),
          "no prefix, suffix or case variants of the step 4 keys");
  }
  {
    // KI-071 step 3 keys: three SDK bools at startup (=false writes nothing)
    // and the dump directory key for the SDK string cvar xma_dump_contexts.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const Key sdk_keys[] = {Key::kXmaHonorFrameTrim, Key::kXmaFrameTrimBlockGranular,
                            Key::kXmaDumpAllRates};
    const char *const sdk_names[] = {"xma_honor_frame_trim", "xma_frame_trim_block_granular",
                                     "xma_dump_all_rates"};
    for (size_t k = 0; k < 3; ++k) {
      Check(FindKey(sdk_names[k]) == sdk_keys[k], "KI-071 step 3 key lookup");
      Check(Info(sdk_keys[k]).phase == Phase::kStartup,
            "step 3 SDK keys at startup (rexruntime cvars)");
      Check(CvarName(Info(sdk_keys[k])) == sdk_names[k] &&
                CvarText(Info(sdk_keys[k]), true) == "true" &&
                CvarText(Info(sdk_keys[k]), false).empty() &&
                Info(sdk_keys[k]).true_source == TextSource::kLiteral,
            "step 3 SDK key writes its own cvar true, false writes nothing");
    }
    const auto &dump = Info(Key::kXmaDumpContextsOn);
    Check(FindKey("xma_dump_contexts_on") == Key::kXmaDumpContextsOn &&
              dump.phase == Phase::kStartup && CvarName(dump) == "xma_dump_contexts" &&
              dump.true_source == TextSource::kXmaDumpDirectory &&
              !CvarText(dump, true).empty() && CvarText(dump, false).empty(),
          "dump key: writes the composed directory to xma_dump_contexts, false writes nothing");
    size_t composed = 0;
    for (const auto &info : mcla::dev_overrides::kKeys) {
      composed += info.true_source == TextSource::kXmaDumpDirectory ? 1 : 0;
    }
    Check(composed == 1, "only the dump key has a composed text");
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      for (const Key key : {Key::kXmaHonorFrameTrim, Key::kXmaFrameTrimBlockGranular,
                            Key::kXmaDumpAllRates, Key::kXmaDumpContextsOn}) {
        Check(pair.first != key && pair.second != key, "step 3 keys are in no conflict pair");
      }
    }
    Check(!FindKey("xma_dump_contexts") && !FindKey("xma_dump") && !FindKey("xma_dump_on") &&
              !FindKey("xma_dump_contexts_off") && !FindKey("xma_dump_context_on") &&
              !FindKey("XMA_DUMP_CONTEXTS_ON") && !FindKey("xma_dump_all_rates_on") &&
              !FindKey("xma_dump_all") && !FindKey("xma_honor_frame_trim_on") &&
              !FindKey("xma_frame_trim") && !FindKey("xma_honor_trim") &&
              !FindKey("xma_frame_trim_block") && !FindKey("xma_frame_trim_block_granular_on"),
          "no prefix, suffix or case variants of the step 3 keys; the string cvar stays out");
  }
  {
    // KI-071 step 3 dump directory composition (pure).
    using mcla::dev_overrides::FormatDumpStamp;
    using mcla::dev_overrides::XmaDumpDirectory;
    Check(FormatDumpStamp(2026, 11, 7, 9, 5, 3) == "20261107-090503", "stamp format");
    Check(FormatDumpStamp(1999, 1, 31, 23, 59, 60) == "19990131-235960", "stamp upper bounds");
    Check(FormatDumpStamp(2026, 13, 7, 9, 5, 3).empty() &&
              FormatDumpStamp(2026, 0, 7, 9, 5, 3).empty() &&
              FormatDumpStamp(2026, 11, 0, 9, 5, 3).empty() &&
              FormatDumpStamp(2026, 11, 7, 24, 5, 3).empty() &&
              FormatDumpStamp(2026, 11, 7, 9, 60, 3).empty() &&
              FormatDumpStamp(1969, 11, 7, 9, 5, 3).empty() &&
              FormatDumpStamp(10000, 11, 7, 9, 5, 3).empty(),
          "stamp out of range is empty");
    const std::filesystem::path user("C:/profile/user");
    const std::filesystem::path session("C:/profile/diagnostics/S-0001");
    Check(XmaDumpDirectory(user, {}, "20261107-090503").generic_string() ==
              "C:/profile/user/diagnostics/xma-dump/20261107-090503",
          "direct start: <user-data>/diagnostics/xma-dump/<stamp>");
    Check(XmaDumpDirectory(user, session, "20261107-090503").generic_string() ==
              "C:/profile/diagnostics/S-0001/xma-dump/20261107-090503",
          "launcher session: <diagnostics root>/xma-dump/<stamp>");
    Check(XmaDumpDirectory({}, session, "20261107-090503").generic_string() ==
              "C:/profile/diagnostics/S-0001/xma-dump/20261107-090503",
          "diagnostics root alone is enough");
    Check(XmaDumpDirectory(user, session, "").empty() && XmaDumpDirectory({}, {}, "x").empty(),
          "no stamp or no root: empty");
  }
  {
    // KI-071 step 2 keys: the four SDK bools at startup (=false writes
    // nothing) and the host consume probe (a plain init-only bool).
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    const Key sdk_keys[] = {Key::kXmaSiblingTrace, Key::kXmaStallOnInvalidNext,
                            Key::kXmaHonorStartSkip, Key::kXmaResetCodecOnClear};
    const char *const sdk_names[] = {"xma_sibling_trace", "xma_stall_on_invalid_next",
                                     "xma_honor_start_skip", "xma_reset_codec_on_clear"};
    for (size_t k = 0; k < 4; ++k) {
      Check(FindKey(sdk_names[k]) == sdk_keys[k], "KI-071 step 2 key lookup");
      Check(Info(sdk_keys[k]).phase == Phase::kStartup,
            "step 2 SDK keys at startup (rexruntime cvars)");
      Check(CvarName(Info(sdk_keys[k])) == sdk_names[k] &&
                CvarText(Info(sdk_keys[k]), true) == "true" &&
                CvarText(Info(sdk_keys[k]), false).empty(),
            "step 2 SDK key writes its own cvar true, false writes nothing");
    }
    Check(FindKey("mcla_xma_consume_probe") == Key::kXmaConsumeProbe &&
              Info(Key::kXmaConsumeProbe).phase == Phase::kStartup &&
              CvarName(Info(Key::kXmaConsumeProbe)) == "mcla_xma_consume_probe" &&
              CvarText(Info(Key::kXmaConsumeProbe), true) == "true" &&
              CvarText(Info(Key::kXmaConsumeProbe), false) == "false",
          "consume probe: host init-only bool at startup, writes true/false");
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      for (const Key key : {Key::kXmaSiblingTrace, Key::kXmaStallOnInvalidNext,
                            Key::kXmaHonorStartSkip, Key::kXmaResetCodecOnClear,
                            Key::kXmaConsumeProbe}) {
        Check(pair.first != key && pair.second != key, "step 2 keys are in no conflict pair");
      }
    }
    Check(!FindKey("xma_sibling") && !FindKey("xma_sibling_trace_on") &&
              !FindKey("xma_stall") && !FindKey("xma_stall_on_invalid_next_off") &&
              !FindKey("xma_start_skip") && !FindKey("xma_honor_start_skip_on") &&
              !FindKey("xma_reset_codec") && !FindKey("xma_codec_reset") &&
              !FindKey("xma_consume_probe") && !FindKey("mcla_xma_consume") &&
              !FindKey("MCLA_XMA_CONSUME_PROBE") && !FindKey("mcla_xma_consume_probe_on"),
          "no prefix, suffix or case variants of the step 2 keys");
  }
  {
    // KI-071 XMA doubling fix keys: plain SDK bools at startup, =false writes
    // nothing.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    const Key xma_keys[] = {Key::kXmaSwapCarryPacket, Key::kXmaValidOnlyWhenWritten,
                            Key::kXmaWorkLockRecheck, Key::kXmaSwapTrace};
    const char *const xma_names[] = {"xma_swap_carry_packet", "xma_valid_only_when_written",
                                     "xma_work_lock_recheck", "xma_swap_trace"};
    for (size_t k = 0; k < 4; ++k) {
      Check(FindKey(xma_names[k]) == xma_keys[k], "KI-071 XMA key lookup");
      Check(Info(xma_keys[k]).phase == Phase::kStartup, "XMA keys at startup (rexruntime cvars)");
      Check(CvarName(Info(xma_keys[k])) == xma_names[k] &&
                CvarText(Info(xma_keys[k]), true) == "true" &&
                CvarText(Info(xma_keys[k]), false).empty(),
            "XMA key writes its own cvar true, false writes nothing");
      for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
        Check(pair.first != xma_keys[k] && pair.second != xma_keys[k],
              "XMA keys are in no conflict pair");
      }
    }
    Check(!FindKey("xma_swap_carry") && !FindKey("xma_swap_carry_packet_on") &&
              !FindKey("xma_swap_carry_packet_off") && !FindKey("xma_valid_only") &&
              !FindKey("xma_valid_only_when_written_off") && !FindKey("xma_work_lock") &&
              !FindKey("xma_work_lock_recheck_on") && !FindKey("xma_swap") &&
              !FindKey("xma_swap_trace_on") && !FindKey("xma_trace") &&
              !FindKey("XMA_SWAP_TRACE") && !FindKey("xma_decoder") && !FindKey("apu"),
          "no prefix, suffix, case or other XMA/APU keys");
  }
  {
    // l73 owner A/B keys (M11-016 fetch identity cache, M11-007a steady key).
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    Check(FindKey("fetch_identity_off") == Key::kFetchIdentityOff &&
              FindKey("fetch_identity_verify") == Key::kFetchIdentityVerify &&
              FindKey("readback_steady_exempt") == Key::kReadbackSteadyExempt,
          "l73 fetch identity and steady-exempt keys");
    Check(!FindKey("fetch_identity") && !FindKey("fetch_identity_on") &&
              !FindKey("fetch_identity_cache") && !FindKey("fetch_identity_cache_off") &&
              !FindKey("fetch_identity_verif") && !FindKey("fetch_identity_verify_on") &&
              !FindKey("d3d12_fetch_identity_cache") && !FindKey("d3d12_fetch_identity_verify") &&
              !FindKey("readback_steady") && !FindKey("readback_steady_exempt_on") &&
              !FindKey("readback_steady_exempt_off") && !FindKey("steady_exempt") &&
              !FindKey("d3d12_readback_sync_steady_exempt") && !FindKey("Fetch_identity_off"),
          "no prefix, suffix, cvar-name or case variants of the l73 keys");
    Check(Info(Key::kFetchIdentityOff).phase == Phase::kGraphicsPlugin &&
              Info(Key::kFetchIdentityVerify).phase == Phase::kGraphicsPlugin &&
              Info(Key::kReadbackSteadyExempt).phase == Phase::kGraphicsPlugin,
          "l73 keys after the plugin registered their cvars");
    Check(CvarName(Info(Key::kFetchIdentityOff)) == "d3d12_fetch_identity_cache" &&
              CvarText(Info(Key::kFetchIdentityOff), true) == "false" &&
              CvarText(Info(Key::kFetchIdentityOff), false).empty(),
          "fetch_identity_off=true writes false, false writes nothing");
    Check(CvarName(Info(Key::kFetchIdentityVerify)) == "d3d12_fetch_identity_verify" &&
              CvarText(Info(Key::kFetchIdentityVerify), true) == "true" &&
              CvarText(Info(Key::kFetchIdentityVerify), false).empty(),
          "fetch_identity_verify=true writes true, false writes nothing");
    Check(CvarName(Info(Key::kReadbackSteadyExempt)) == "d3d12_readback_sync_steady_exempt" &&
              CvarText(Info(Key::kReadbackSteadyExempt), true) == "true" &&
              CvarText(Info(Key::kReadbackSteadyExempt), false).empty(),
          "readback_steady_exempt=true writes true, false writes nothing");
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      for (const Key key :
           {Key::kFetchIdentityOff, Key::kFetchIdentityVerify, Key::kReadbackSteadyExempt}) {
        Check(pair.first != key && pair.second != key, "l73 keys are in no conflict pair");
      }
    }
  }
  Check(FindKey("readback_kick_256") == Key::kReadbackKick256 &&
            FindKey("readback_kick_128") == Key::kReadbackKick128 &&
            FindKey("readback_kick_1024") == Key::kReadbackKick1024,
        "M11-015 kick-interval keys");
  Check(!FindKey("readback_kick_512") && !FindKey("readback_kick") &&
            !FindKey("readback_kick_0") && !FindKey("readback_kick_64") &&
            !FindKey("readback_kick_102") && !FindKey("readback_kick_1024_on"),
        "no default (512), prefix, off or other numeric kick keys");
  Check(mcla::dev_overrides::Info(Key::kReadbackKick256).phase == Phase::kGraphicsPlugin &&
            mcla::dev_overrides::Info(Key::kReadbackKick128).phase == Phase::kGraphicsPlugin &&
            mcla::dev_overrides::Info(Key::kReadbackKick1024).phase == Phase::kGraphicsPlugin,
        "kick keys after the plugin registered d3d12_readback_sync_kick_draws");
  Check(FindKey("mcla_album_preview_restore") == Key::kAlbumPreviewRestore,
        "KI-056 fix A restore key");
  Check(mcla::dev_overrides::Info(Key::kAlbumPreviewRestore).phase == Phase::kStartup &&
            mcla::dev_overrides::CvarName(mcla::dev_overrides::Info(Key::kAlbumPreviewRestore)) ==
                "mcla_album_preview_restore" &&
            mcla::dev_overrides::CvarText(mcla::dev_overrides::Info(Key::kAlbumPreviewRestore),
                                          true) == "true" &&
            mcla::dev_overrides::CvarText(mcla::dev_overrides::Info(Key::kAlbumPreviewRestore),
                                          false) == "false",
        "KI-056 fix A key: host init-only bool at startup, its own cvar");
  Check(!FindKey("mcla_album_preview") && !FindKey("mcla_album_preview_restore_on") &&
            !FindKey("mcla_album_restore"),
        "no prefix or variant keys for the restore");
  Check(FindKey("audio_speaker_config_stereo") == Key::kAudioSpeakerStereo &&
            FindKey("audio_speaker_config_surround") == Key::kAudioSpeakerSurround &&
            FindKey("audio_downmix_itu_off") == Key::kAudioDownmixItuOff &&
            FindKey("audio_channel_rms_probe") == Key::kAudioChannelRmsProbe,
        "KI-071 audio keys");
  Check(!FindKey("audio_speaker_config") && !FindKey("audio_downmix_itu") &&
            !FindKey("audio_speaker_config_auto") && !FindKey("audio_downmix_itu_on") &&
            !FindKey("audio_speaker_config_stere") && !FindKey("audio_channel_rms") &&
            !FindKey("audio_device") && !FindKey("audio_volume") && !FindKey("audio_mute"),
        "the audio cvar names, auto, prefixes and other audio cvars are not keys");
  Check(mcla::dev_overrides::Info(Key::kAudioSpeakerStereo).phase == Phase::kStartup &&
            mcla::dev_overrides::Info(Key::kAudioSpeakerSurround).phase == Phase::kStartup &&
            mcla::dev_overrides::Info(Key::kAudioDownmixItuOff).phase == Phase::kStartup &&
            mcla::dev_overrides::Info(Key::kAudioChannelRmsProbe).phase == Phase::kStartup,
        "KI-071 keys at startup (rexruntime cvars, before the guest)");
  Check(FindKey("xam_content_stable_creation_time_off") == Key::kStableCreationTimeOff,
        "KI-068 stable creation time off key");
  Check(FindKey("xam_content_trace_off") == Key::kContentTraceOff, "KI-068 trace off key");
  Check(!FindKey("xam_content_stable_creation_time") && !FindKey("xam_content_trace") &&
            !FindKey("xam_content_close_host_handles_on_unmount") &&
            !FindKey("xam_content_move_aside_on_delete_failure"),
        "the SDK cvar names themselves are not keys");
  Check(mcla::dev_overrides::Info(Key::kStableCreationTimeOff).phase == Phase::kStartup &&
            mcla::dev_overrides::Info(Key::kContentTraceOff).phase == Phase::kStartup,
        "KI-068 keys at startup (rexruntime cvars, before the guest)");
  Check(FindKey("mcla_album_probe") == Key::kAlbumProbe, "KI-056 album probe key");
  Check(FindKey("mcla_pursuit_probe") == Key::kPursuitProbe, "KI-066 pursuit probe key");
  Check(!FindKey("mcla_album") && !FindKey("mcla_pursuit") && !FindKey("mcla_album_probe_on") &&
            !FindKey("mcla_cpu_census") && !FindKey("mcla_vram_census"),
        "no prefix keys; the census switches stay command-line only");
  Check(mcla::dev_overrides::Info(Key::kAlbumProbe).phase == Phase::kStartup &&
            mcla::dev_overrides::Info(Key::kPursuitProbe).phase == Phase::kStartup,
        "M11-002 probes at startup (host init-only cvars, before the guest)");
  Check(FindKey("mcla_warp_finish_fix_off") == Key::kWarpFinishFixOff,
        "KI-035 finish-race fix off key");
  Check(!FindKey("mcla_warp_finish_fix") && !FindKey("mcla_warp_finish_fix_on"),
        "only the off key for the finish-race fix (default on)");
  Check(FindKey("mcla_texture_budget_soft_cap_off") == Key::kTextureSoftCapOff,
        "texture soft cap off key");
  Check(!FindKey("mcla_texture_budget_soft_cap_mb"), "integer soft cap cvar not whitelisted");
  Check(!FindKey("mcla_texture_budget_soft_cap_on") && !FindKey("mcla_texture_budget_soft_cap"),
        "only the off key");
  Check(FindKey("mcla_warp_probe") == Key::kWarpProbe, "warp probe key");
  Check(FindKey("mcla_warp_watchdog") == Key::kWarpWatchdog, "warp watchdog key");
  Check(!FindKey("mcla_warp_watchdog_seconds"), "integer watchdog cvar not whitelisted");
  Check(!FindKey("mcla_warp_watchdog_15") && !FindKey("mcla_warp"),
        "no numeric or prefix warp keys");
  Check(!FindKey("mcla_ki026_probe") && !FindKey("mcla_ki026_watchdog"),
        "KI-026 switches stay command-line only");
  Check(FindKey("mcla_readback_sync_composite") == Key::kReadbackSyncComposite, "readback key");
  Check(FindKey("mcla_native_d3d_refcount") == Key::kNativeD3dRefcount, "refcount key");
  Check(FindKey("mcla_native_xapi_interlocked") == Key::kNativeXapiInterlocked, "xapi key");
  Check(FindKey("d3d12_wrm_event_wake") == Key::kWrmEventWake, "wrm key");
  Check(FindKey("d3d12_replay_batched_wake") == Key::kReplayBatchedWake, "replay key");
  Check(FindKey("d3d12_readback_elide_unread") == Key::kReadbackElideUnread, "elide key");
  Check(FindKey("mcla_gpu_backend_vulkan") == Key::kGpuBackendVulkan, "vulkan backend key");
  Check(FindKey("prewarm_seed_store") == Key::kPrewarmSeedStore, "store seeding key");
  Check(FindKey("d3d12_readback_sync_targeted") == Key::kReadbackSyncTargeted,
        "targeted read-back wait key");
  Check(!FindKey("d3d12_readback_sync_kick_draws"), "kick draw count (integer) not whitelisted");
  Check(FindKey("memory_watch_census") == Key::kWatchCensus, "watch census key");
  Check(FindKey("memory_watch_probes") == Key::kWatchProbes, "watch probes key");
  Check(FindKey("memory_watch_disarm_bridge_safe_gaps") == Key::kWatchDisarmBridge,
        "disarm bridge key");
  Check(FindKey("shared_memory_upload_ahead_16") == Key::kUploadAhead16, "upload-ahead 16 key");
  Check(FindKey("shared_memory_upload_ahead_64") == Key::kUploadAhead64, "upload-ahead 64 key");
  Check(!FindKey("shared_memory_upload_ahead_pages"), "integer upload-ahead cvar not whitelisted");
  Check(!FindKey("shared_memory_upload_ahead_32"), "only 16 and 64");
  Check(FindKey("shared_memory_hot_blocks_shadow") == Key::kHotBlocksShadow,
        "hot blocks shadow key");
  Check(FindKey("shared_memory_hot_blocks_on") == Key::kHotBlocksOn, "hot blocks on key");
  Check(!FindKey("shared_memory_hot_blocks"), "integer hot blocks cvar not whitelisted");
  Check(!FindKey("shared_memory_hot_blocks_off"), "no off key (=false writes nothing)");
  Check(!FindKey("shared_memory_hot_blocks_1") && !FindKey("shared_memory_hot_blocks_2"),
        "no numeric hot blocks keys");
  Check(!FindKey("shared_memory_hot_k") && !FindKey("shared_memory_hot_n") &&
            !FindKey("shared_memory_hot_upload_cap_kb") &&
            !FindKey("shared_memory_hot_probe_frames"),
        "hot block K, N, cap and probe stay command-line only");
  Check(!FindKey("shared_memory_hot_blocks_sha"), "hot blocks shadow prefix");
  Check(!FindKey("memory_watch_disarm_bridge"), "bridge key prefix");
  Check(!FindKey("d3d12_readback_sync_scope"), "read-back sync scope not whitelisted");
  Check(!FindKey("d3d12_readback_sync"), "targeted key prefix");
  Check(!FindKey("prewarm_seed"), "store seeding prefix");
  for (size_t i = 0; i < mcla::dev_overrides::kKeyCount; ++i) {
    Check(FindKey(mcla::dev_overrides::kKeys[i].name) == static_cast<Key>(i),
          "kKeys is indexed by Key");
  }
  // Names that must stay out of the whitelist (other cvars, near misses).
  Check(!FindKey("d3d12_readback_sync_composite"), "SDK readback cvar not whitelisted");
  Check(!FindKey("readback_resolve"), "readback_resolve not whitelisted");
  Check(!FindKey("d3d12_perf_split"), "perf split not whitelisted");
  Check(!FindKey("d3d12_readback_gpu_timing"), "read-back GPU timing not whitelisted");
  Check(!FindKey("d3d12_readback_elide"), "elide prefix");
  Check(!FindKey("vsync"), "vsync not whitelisted");
  Check(!FindKey("gpu_backend"), "SDK gpu_backend string cvar not whitelisted");
  Check(!FindKey("mcla_gpu_backend"), "vulkan key prefix");
  Check(!FindKey("vulkan_device"), "vulkan_device not whitelisted");
  Check(!FindKey("mcla_native_d3d_refcountX"), "suffix");
  Check(!FindKey("MCLA_NATIVE_D3D_REFCOUNT"), "case sensitive");
  Check(!FindKey(""), "empty name");
  Check(mcla::dev_overrides::Info(Key::kReadbackSyncComposite).phase == Phase::kStartup,
        "readback host switch at startup");
  Check(mcla::dev_overrides::Info(Key::kNativeD3dRefcount).phase == Phase::kStartup,
        "refcount at startup (init-only, before the guest)");
  Check(mcla::dev_overrides::Info(Key::kNativeXapiInterlocked).phase == Phase::kStartup,
        "xapi at startup");
  Check(mcla::dev_overrides::Info(Key::kWrmEventWake).phase == Phase::kGraphicsPlugin,
        "wrm after the plugin registered it");
  Check(mcla::dev_overrides::Info(Key::kReplayBatchedWake).phase == Phase::kGraphicsPlugin,
        "replay after the plugin registered it");
  Check(mcla::dev_overrides::Info(Key::kReadbackElideUnread).phase == Phase::kGraphicsPlugin,
        "elide after the plugin registered it");
  Check(mcla::dev_overrides::Info(Key::kGpuBackendVulkan).phase == Phase::kStartup,
        "vulkan backend at startup (OnPostInitLogging precedes OnPreSetup)");
  Check(mcla::dev_overrides::Info(Key::kPrewarmSeedStore).phase == Phase::kLauncher,
        "store seeding is a launcher key (no host cvar)");
  Check(mcla::dev_overrides::Info(Key::kReadbackSyncTargeted).phase == Phase::kGraphicsPlugin,
        "targeted read-back wait after the plugin registered it");
  Check(mcla::dev_overrides::Info(Key::kWatchCensus).phase == Phase::kStartup &&
            mcla::dev_overrides::Info(Key::kWatchProbes).phase == Phase::kStartup &&
            mcla::dev_overrides::Info(Key::kWatchDisarmBridge).phase == Phase::kStartup,
        "rexruntime memory cvars at startup");
  Check(mcla::dev_overrides::Info(Key::kUploadAhead16).phase == Phase::kGraphicsPlugin &&
            mcla::dev_overrides::Info(Key::kUploadAhead64).phase == Phase::kGraphicsPlugin,
        "upload-ahead after the plugin registered shared_memory_upload_ahead_pages");
  Check(mcla::dev_overrides::Info(Key::kHotBlocksShadow).phase == Phase::kGraphicsPlugin &&
            mcla::dev_overrides::Info(Key::kHotBlocksOn).phase == Phase::kGraphicsPlugin,
        "hot blocks after the plugin registered shared_memory_hot_blocks");
  Check(mcla::dev_overrides::Info(Key::kWarpProbe).phase == Phase::kStartup &&
            mcla::dev_overrides::Info(Key::kWarpWatchdog).phase == Phase::kStartup &&
            mcla::dev_overrides::Info(Key::kWarpFinishFixOff).phase == Phase::kStartup,
        "warp keys at startup (host init-only cvars, before the guest)");
  {
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    for (size_t i = 0; i < mcla::dev_overrides::kKeyCount; ++i) {
      const auto &info = mcla::dev_overrides::kKeys[i];
      const Key key = static_cast<Key>(i);
      const bool mapped = key == Key::kUploadAhead16 || key == Key::kUploadAhead64 ||
                          key == Key::kHotBlocksShadow || key == Key::kHotBlocksOn ||
                          key == Key::kWarpWatchdog || key == Key::kTextureSoftCapOff ||
                          key == Key::kWarpFinishFixOff || key == Key::kStableCreationTimeOff ||
                          key == Key::kContentTraceOff || key == Key::kAudioSpeakerStereo ||
                          key == Key::kAudioSpeakerSurround || key == Key::kAudioDownmixItuOff ||
                          key == Key::kReadbackKick256 || key == Key::kReadbackKick128 ||
                          key == Key::kReadbackKick1024 || key == Key::kFetchIdentityOff ||
                          key == Key::kFetchIdentityVerify || key == Key::kReadbackSteadyExempt ||
                          key == Key::kXmaSwapCarryPacket || key == Key::kXmaValidOnlyWhenWritten ||
                          key == Key::kXmaWorkLockRecheck || key == Key::kXmaSwapTrace ||
                          key == Key::kXmaSiblingTrace || key == Key::kXmaStallOnInvalidNext ||
                          key == Key::kXmaHonorStartSkip || key == Key::kXmaResetCodecOnClear ||
                          key == Key::kXmaHonorFrameTrim ||
                          key == Key::kXmaFrameTrimBlockGranular ||
                          key == Key::kXmaDumpAllRates || key == Key::kXmaDumpContextsOn ||
                          key == Key::kXmaFrameWalkTrace || key == Key::kXmaFrameWalkFix ||
                          key == Key::kXmaDumpInput || key == Key::kFenceWriteDeferred ||
                          key == Key::kXmaFrameWalkFixOff || key == Key::kRawTimerOff ||
                          key == Key::kVblankHz72 || key == Key::kVblankHz80 ||
                          key == Key::kVblankHz88;
      if (!mapped) {
        Check(CvarName(info) == info.name && CvarText(info, true) == "true" &&
                  CvarText(info, false) == "false",
              "bool keys write their own cvar as true/false");
      }
    }
    Check(CvarName(Info(Key::kUploadAhead16)) == "shared_memory_upload_ahead_pages" &&
              CvarText(Info(Key::kUploadAhead16), true) == "16" &&
              CvarText(Info(Key::kUploadAhead16), false).empty(),
          "ahead_16=true writes 16, false writes nothing");
    Check(CvarName(Info(Key::kUploadAhead64)) == "shared_memory_upload_ahead_pages" &&
              CvarText(Info(Key::kUploadAhead64), true) == "64" &&
              CvarText(Info(Key::kUploadAhead64), false).empty(),
          "ahead_64=true writes 64, false writes nothing");
    Check(CvarName(Info(Key::kHotBlocksShadow)) == "shared_memory_hot_blocks" &&
              CvarText(Info(Key::kHotBlocksShadow), true) == "1" &&
              CvarText(Info(Key::kHotBlocksShadow), false).empty(),
          "hot_blocks_shadow=true writes 1, false writes nothing");
    Check(CvarName(Info(Key::kHotBlocksOn)) == "shared_memory_hot_blocks" &&
              CvarText(Info(Key::kHotBlocksOn), true) == "2" &&
              CvarText(Info(Key::kHotBlocksOn), false).empty(),
          "hot_blocks_on=true writes 2, false writes nothing");
    Check(CvarName(Info(Key::kWarpWatchdog)) == "mcla_warp_watchdog_seconds" &&
              CvarText(Info(Key::kWarpWatchdog), true) == "15" &&
              CvarText(Info(Key::kWarpWatchdog), false).empty(),
          "warp_watchdog=true writes 15 s, false writes nothing");
    Check(Info(Key::kTextureSoftCapOff).phase == Phase::kStartup &&
              CvarName(Info(Key::kTextureSoftCapOff)) == "mcla_texture_budget_soft_cap_mb" &&
              CvarText(Info(Key::kTextureSoftCapOff), true) == "0" &&
              CvarText(Info(Key::kTextureSoftCapOff), false).empty(),
          "soft_cap_off=true writes 0 at startup, false writes nothing");
    Check(CvarName(Info(Key::kWarpFinishFixOff)) == "mcla_warp_finish_fix" &&
              CvarText(Info(Key::kWarpFinishFixOff), true) == "false" &&
              CvarText(Info(Key::kWarpFinishFixOff), false).empty(),
          "warp_finish_fix_off=true writes false, false writes nothing");
    Check(CvarName(Info(Key::kStableCreationTimeOff)) == "xam_content_stable_creation_time" &&
              CvarText(Info(Key::kStableCreationTimeOff), true) == "false" &&
              CvarText(Info(Key::kStableCreationTimeOff), false).empty(),
          "stable_creation_time_off=true writes false, false writes nothing");
    Check(CvarName(Info(Key::kContentTraceOff)) == "xam_content_trace" &&
              CvarText(Info(Key::kContentTraceOff), true) == "false" &&
              CvarText(Info(Key::kContentTraceOff), false).empty(),
          "trace_off=true writes false, false writes nothing");
    Check(CvarName(Info(Key::kAudioSpeakerStereo)) == "audio_speaker_config" &&
              CvarText(Info(Key::kAudioSpeakerStereo), true) == "stereo" &&
              CvarText(Info(Key::kAudioSpeakerStereo), false).empty(),
          "speaker_config_stereo=true writes stereo, false writes nothing");
    Check(CvarName(Info(Key::kAudioSpeakerSurround)) == "audio_speaker_config" &&
              CvarText(Info(Key::kAudioSpeakerSurround), true) == "surround" &&
              CvarText(Info(Key::kAudioSpeakerSurround), false).empty(),
          "speaker_config_surround=true writes surround, false writes nothing");
    Check(CvarName(Info(Key::kAudioDownmixItuOff)) == "audio_downmix_itu" &&
              CvarText(Info(Key::kAudioDownmixItuOff), true) == "false" &&
              CvarText(Info(Key::kAudioDownmixItuOff), false).empty(),
          "downmix_itu_off=true writes false, false writes nothing");
    Check(CvarName(Info(Key::kAudioChannelRmsProbe)) == "audio_channel_rms_probe" &&
              CvarText(Info(Key::kAudioChannelRmsProbe), true) == "true" &&
              CvarText(Info(Key::kAudioChannelRmsProbe), false) == "false",
          "rms probe writes its own bool cvar");
    {
      const Key kick_keys[] = {Key::kReadbackKick256, Key::kReadbackKick128,
                               Key::kReadbackKick1024};
      const char *const kick_values[] = {"256", "128", "1024"};
      for (size_t k = 0; k < 3; ++k) {
        Check(CvarName(Info(kick_keys[k])) == "d3d12_readback_sync_kick_draws" &&
                  CvarText(Info(kick_keys[k]), true) == kick_values[k] &&
                  CvarText(Info(kick_keys[k]), false).empty(),
              "readback_kick_N=true writes N, false writes nothing");
      }
    }
    Check(CvarName(Info(Key::kWarpProbe)) == "mcla_warp_probe" &&
              CvarText(Info(Key::kWarpProbe), true) == "true",
          "warp probe writes its own bool cvar");
    // Keys sharing a cvar are exactly the conflict pairs.
    size_t shared = 0;
    for (size_t i = 0; i < mcla::dev_overrides::kKeyCount; ++i) {
      for (size_t j = i + 1; j < mcla::dev_overrides::kKeyCount; ++j) {
        if (CvarName(mcla::dev_overrides::kKeys[i]) != CvarName(mcla::dev_overrides::kKeys[j])) {
          continue;
        }
        ++shared;
        bool paired = false;
        for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
          paired = paired || (static_cast<size_t>(pair.first) == i &&
                              static_cast<size_t>(pair.second) == j);
        }
        Check(paired, "keys writing one cvar form a conflict pair");
      }
    }
    // Four pairs (l77: the walk fix and its control) plus the kick and the
    // vblank groups of three (each listed as its three pairs).
    Check(shared == mcla::dev_overrides::kConflictPairs.size() && shared == 10,
          "ten shared-cvar pairs");
  }
  {
    // The step 1/2 arm files.
    const Parsed bridge = Parse("memory_watch_disarm_bridge_safe_gaps=true\n");
    Check(bridge.ValueCount() == 1 && bridge.Value(Key::kWatchDisarmBridge) == true,
          "bridge arm");
    const Parsed ahead = Parse("shared_memory_upload_ahead_64=true\r\n"
                               "memory_watch_probes=true\r\n");
    Check(ahead.ValueCount() == 2 && ahead.Value(Key::kUploadAhead64) == true &&
              ahead.Value(Key::kWatchProbes) == true && ahead.ignored_lines == 0,
          "ahead 64 arm with probes");
    const Parsed census_off = Parse("memory_watch_census=false\n");
    Check(census_off.Value(Key::kWatchCensus) == false, "census off (overhead A/B)");
    const Parsed both = Parse("shared_memory_upload_ahead_16=true\n"
                              "memory_watch_census=true\n"
                              "shared_memory_upload_ahead_64=true\n");
    Check(both.ValueCount() == 1 && !both.Value(Key::kUploadAhead16) &&
              !both.Value(Key::kUploadAhead64) && both.Value(Key::kWatchCensus) == true,
          "both upload-ahead keys true: neither applies");
    Check(both.ignored_lines == 1 && both.issue_count == 1 && both.issues[0].line == 3 &&
              both.issues[0].issue == LineIssue::kConflict,
          "conflict logged on the later line");
    const Parsed one_off = Parse("shared_memory_upload_ahead_16=false\n"
                                 "shared_memory_upload_ahead_64=true\n");
    Check(one_off.ValueCount() == 2 && one_off.Value(Key::kUploadAhead64) == true &&
              one_off.ignored_lines == 0,
          "16=false with 64=true is not a conflict");
    const Parsed integer = Parse("shared_memory_upload_ahead_pages=16\n"
                                 "shared_memory_upload_ahead_16=16\n");
    Check(integer.ValueCount() == 0 && integer.ignored_lines == 2 &&
              integer.issues[0].issue == LineIssue::kMalformed &&
              integer.issues[1].issue == LineIssue::kMalformed,
          "integer values are not accepted");
  }
  {
    // Hot blocks (M10 step 3): the same pair rules as upload-ahead.
    const Parsed shadow = Parse("shared_memory_hot_blocks_shadow=true\n"
                                "shared_memory_hot_blocks_on=false\n");
    Check(shadow.ValueCount() == 2 && shadow.Value(Key::kHotBlocksShadow) == true &&
              shadow.Value(Key::kHotBlocksOn) == false && shadow.ignored_lines == 0,
          "shadow arm");
    const Parsed on = Parse("shared_memory_hot_blocks_shadow=false\r\n"
                            "shared_memory_hot_blocks_on=true\r\n");
    Check(on.ValueCount() == 2 && on.Value(Key::kHotBlocksOn) == true && on.ignored_lines == 0,
          "on arm; shadow=false with on=true is not a conflict");
    const Parsed both = Parse("shared_memory_hot_blocks_on=true\n"
                              "memory_watch_census=true\n"
                              "shared_memory_hot_blocks_shadow=true\n"
                              "d3d12_wrm_event_wake=true\n");
    Check(both.ValueCount() == 2 && !both.Value(Key::kHotBlocksShadow) &&
              !both.Value(Key::kHotBlocksOn) && both.Value(Key::kWatchCensus) == true &&
              both.Value(Key::kWrmEventWake) == true,
          "both hot blocks keys true: neither applies");
    Check(both.ignored_lines == 1 && both.issue_count == 1 && both.issues[0].line == 3 &&
              both.issues[0].issue == LineIssue::kConflict,
          "hot blocks conflict logged on the later line");
    const Parsed cross = Parse("shared_memory_upload_ahead_16=true\n"
                               "shared_memory_hot_blocks_on=true\n");
    Check(cross.ValueCount() == 2 && cross.Value(Key::kUploadAhead16) == true &&
              cross.Value(Key::kHotBlocksOn) == true && cross.ignored_lines == 0,
          "upload-ahead 16 with hot blocks on is not a conflict");
    const Parsed two = Parse("shared_memory_hot_blocks_shadow=true\n"
                             "shared_memory_upload_ahead_64=true\n"
                             "shared_memory_upload_ahead_16=true\n"
                             "shared_memory_hot_blocks_on=true\n");
    Check(two.ValueCount() == 0 && two.ignored_lines == 2 && two.issue_count == 2,
          "both pairs in conflict: four lines, no value");
    Check(two.issues[0].line == 3 && two.issues[0].issue == LineIssue::kConflict &&
              two.issues[1].line == 4 && two.issues[1].issue == LineIssue::kConflict,
          "each conflict logged on its later line");
    const Parsed dup = Parse("shared_memory_hot_blocks_on=true\n"
                             "shared_memory_hot_blocks_on=false\n");
    Check(dup.ValueCount() == 1 && dup.Value(Key::kHotBlocksOn) == true &&
              dup.issues[0].issue == LineIssue::kDuplicate,
          "repeated hot blocks key: first wins");
    const Parsed integer = Parse("shared_memory_hot_blocks=2\n"
                                 "shared_memory_hot_blocks_on=2\n");
    Check(integer.ValueCount() == 0 && integer.ignored_lines == 2 &&
              integer.issues[0].issue == LineIssue::kMalformed &&
              integer.issues[1].issue == LineIssue::kMalformed,
          "integer hot blocks values are not accepted");
  }
  {
    // The l66 owner arm files (private/l66-arms/<pc|deck>/<arm>/, ASCII LF):
    // every whitelisted key written, base = the l63 "all" arm.
    const std::string common = "mcla_readback_sync_composite=true\n"
                               "mcla_native_d3d_refcount=true\n"
                               "mcla_native_xapi_interlocked=true\n"
                               "d3d12_wrm_event_wake=true\n"
                               "d3d12_replay_batched_wake=true\n"
                               "d3d12_readback_elide_unread=true\n"
                               "mcla_gpu_backend_vulkan=false\n"
                               "prewarm_seed_store=true\n"
                               "d3d12_readback_sync_targeted=true\n"
                               "memory_watch_census=true\n"
                               "memory_watch_probes=false\n"
                               "memory_watch_disarm_bridge_safe_gaps=true\n"
                               "shared_memory_upload_ahead_16=true\n"
                               "shared_memory_upload_ahead_64=false\n";
    const char *const arms[3][2] = {{"false", "false"}, {"true", "false"}, {"false", "true"}};
    for (const auto &arm : arms) {
      const std::string text = common + "shared_memory_hot_blocks_shadow=" + arm[0] + "\n" +
                               "shared_memory_hot_blocks_on=" + arm[1] + "\n";
      const Parsed p = Parse(text);
      Check(text.size() <= mcla::dev_overrides::kMaxBytes && p.status == FileStatus::kParsed &&
                p.ValueCount() == 16 && p.ignored_lines == 0,
            "l66 arm: every key of l66 (16), nothing ignored");
      // l68: the same arm plus the two KI-035 keys still fits and parses.
      const std::string l68 = text + "mcla_warp_probe=true\nmcla_warp_watchdog=true\n" +
                              "mcla_texture_budget_soft_cap_off=true\n";
      const Parsed q = Parse(l68);
      Check(l68.size() <= mcla::dev_overrides::kMaxBytes && q.status == FileStatus::kParsed &&
                q.ValueCount() == 19 && q.ignored_lines == 0 &&
                q.Value(Key::kWarpProbe) == true && q.Value(Key::kWarpWatchdog) == true,
            "l68 arm: every l68 key (19), nothing ignored");
      // l69: the l68 arm plus the finish-race fix off key: every key, still fits.
      const std::string l69 = l68 + "mcla_warp_finish_fix_off=true\n";
      const Parsed r = Parse(l69);
      Check(l69.size() <= mcla::dev_overrides::kMaxBytes && r.status == FileStatus::kParsed &&
                r.ValueCount() == 20 && r.ignored_lines == 0 &&
                r.Value(Key::kWarpFinishFixOff) == true,
            "l69 arm: every l69 key (20), nothing ignored");
      // l71 (M11-002 triad): the l69 arm plus the album and pursuit probes.
      const std::string l71 = l69 + "mcla_album_probe=true\nmcla_pursuit_probe=true\n";
      const std::string l71_all = l71 + "xam_content_stable_creation_time_off=true\n" +
                                  "xam_content_trace_off=false\n";
      const Parsed s = Parse(l71_all);
      Check(l71_all.size() <= mcla::dev_overrides::kMaxBytes && s.status == FileStatus::kParsed &&
                s.ValueCount() == 24 && s.ignored_lines == 0 &&
                s.Value(Key::kAlbumProbe) == true && s.Value(Key::kPursuitProbe) == true &&
                s.Value(Key::kStableCreationTimeOff) == true &&
                s.Value(Key::kContentTraceOff) == false,
            "l71 arm: every l71 triad key (24), nothing ignored, fits");
      // l71 KI-071 audio arms: the triad arm plus the four audio keys: every
      // key (28), still within 1 KiB.
      const char *const audio_arms[3][4] = {{"false", "true", "true", "true"},     // old
                                            {"false", "true", "false", "true"},    // itu
                                            {"true", "false", "false", "true"}};  // stereo
      for (const auto &audio : audio_arms) {
        const std::string text_audio = l71_all + "audio_speaker_config_stereo=" + audio[0] + "\n" +
                                       "audio_speaker_config_surround=" + audio[1] + "\n" +
                                       "audio_downmix_itu_off=" + audio[2] + "\n" +
                                       "audio_channel_rms_probe=" + audio[3] + "\n";
        const Parsed a = Parse(text_audio);
        Check(text_audio.size() <= mcla::dev_overrides::kMaxBytes &&
                  a.status == FileStatus::kParsed &&
                  a.ValueCount() == 28 && a.ignored_lines == 0,
              "l71 audio arm: every l71 key (28), nothing ignored, fits in 1 KiB");
        Check(a.Value(Key::kAudioSpeakerStereo) == (audio[0][0] == 't') &&
                  a.Value(Key::kAudioSpeakerSurround) == (audio[1][0] == 't') &&
                  a.Value(Key::kAudioDownmixItuOff) == (audio[2][0] == 't') &&
                  a.Value(Key::kAudioChannelRmsProbe) == true,
              "l71 audio arm values");
      }
      Check(p.Value(Key::kHotBlocksShadow) == (arm[0][0] == 't') &&
                p.Value(Key::kHotBlocksOn) == (arm[1][0] == 't') &&
                p.Value(Key::kUploadAhead16) == true && p.Value(Key::kWatchCensus) == true,
            "l66 arm values");
    }
  }
  {
    const Parsed p = Parse("mcla_readback_sync_composite=true\r\n"
                           "d3d12_readback_sync_targeted=true\r\n");
    Check(p.status == FileStatus::kParsed && p.ValueCount() == 2 &&
              p.Value(Key::kReadbackSyncComposite) == true &&
              p.Value(Key::kReadbackSyncTargeted) == true,
          "targeted arm file: sync on, targeted on");
    const Parsed q = Parse("d3d12_readback_sync_targeted=false\n");
    Check(q.status == FileStatus::kParsed && q.ValueCount() == 1 &&
              q.Value(Key::kReadbackSyncTargeted) == false,
          "full arm file: targeted off");
  }
  {
    const Parsed p = Parse("mcla_warp_probe=true\r\nmcla_warp_watchdog=false\r\n");
    Check(p.status == FileStatus::kParsed && p.ValueCount() == 2 &&
              p.Value(Key::kWarpProbe) == true && p.Value(Key::kWarpWatchdog) == false,
          "warp probe on, watchdog off parses");
    const Parsed q = Parse("mcla_warp_watchdog=15\nmcla_warp_watchdog_seconds=15\n");
    Check(NoValues(q) && q.ignored_lines == 2 && q.issues[0].issue == LineIssue::kMalformed &&
              q.issues[1].issue == LineIssue::kMalformed,
          "integer watchdog values are not accepted");
  }
  {
    // The KI-035 finish-race fix A/B: fix (default) versus fix-off.
    const Parsed off = Parse("mcla_warp_finish_fix_off=true\r\nmcla_warp_probe=true\r\n");
    Check(off.status == FileStatus::kParsed && off.ValueCount() == 2 &&
              off.Value(Key::kWarpFinishFixOff) == true && off.Value(Key::kWarpProbe) == true,
          "fix-off arm parses");
    const Parsed on = Parse("mcla_warp_finish_fix_off=false\n");
    Check(on.ValueCount() == 1 && on.Value(Key::kWarpFinishFixOff) == false,
          "fix arm (explicit default) parses");
    const Parsed cvar = Parse("mcla_warp_finish_fix=false\n");
    Check(cvar.ValueCount() == 0 && cvar.issues[0].issue == LineIssue::kUnknownName,
          "the cvar name itself is not a key");
  }
  {
    // The M11-002 triad probes: plain bools, integer values refused.
    const Parsed on = Parse("mcla_album_probe=true\r\nmcla_pursuit_probe=false\r\n");
    Check(on.status == FileStatus::kParsed && on.ValueCount() == 2 &&
              on.Value(Key::kAlbumProbe) == true && on.Value(Key::kPursuitProbe) == false,
          "album on, pursuit off parses");
    const Parsed integer = Parse("mcla_album_probe=1\nmcla_pursuit_probe=on\n");
    Check(integer.ValueCount() == 0 && integer.ignored_lines == 2 &&
              integer.issues[0].issue == LineIssue::kMalformed &&
              integer.issues[1].issue == LineIssue::kMalformed,
          "probe values must be true or false");
  }
  {
    // l72 KI-056 fix A arm: the l71 triad arm file (every l71 key, in file
    // order) plus the restore key: every key of that build (29), nothing
    // ignored, fits.
    const std::string triad =
        "mcla_readback_sync_composite=true\n"
        "mcla_native_d3d_refcount=true\n"
        "mcla_native_xapi_interlocked=true\n"
        "d3d12_wrm_event_wake=true\n"
        "d3d12_replay_batched_wake=true\n"
        "d3d12_readback_elide_unread=true\n"
        "mcla_gpu_backend_vulkan=false\n"
        "prewarm_seed_store=true\n"
        "d3d12_readback_sync_targeted=true\n"
        "memory_watch_census=false\n"
        "memory_watch_probes=false\n"
        "memory_watch_disarm_bridge_safe_gaps=false\n"
        "shared_memory_upload_ahead_16=false\n"
        "shared_memory_upload_ahead_64=false\n"
        "shared_memory_hot_blocks_shadow=false\n"
        "shared_memory_hot_blocks_on=false\n"
        "mcla_warp_probe=true\n"
        "mcla_warp_watchdog=false\n"
        "mcla_texture_budget_soft_cap_off=false\n"
        "mcla_warp_finish_fix_off=false\n"
        "mcla_album_probe=true\n"
        "mcla_pursuit_probe=true\n"
        "xam_content_stable_creation_time_off=false\n"
        "xam_content_trace_off=false\n"
        "audio_speaker_config_stereo=false\n"
        "audio_speaker_config_surround=false\n"
        "audio_downmix_itu_off=false\n"
        "audio_channel_rms_probe=false\n";
    const Parsed l71 = Parse(triad);
    Check(l71.ValueCount() == 28 && l71.ignored_lines == 0 &&
              !l71.Value(Key::kAlbumPreviewRestore).has_value(),
          "the l71 triad arm has no restore value (the cvar keeps its default off)");
    const std::string l72 = triad + "mcla_album_preview_restore=true\n";
    const Parsed fix = Parse(l72);
    Check(l72.size() <= mcla::dev_overrides::kMaxBytes && fix.status == FileStatus::kParsed &&
              fix.ValueCount() == 29 && fix.ignored_lines == 0 &&
              fix.Value(Key::kAlbumPreviewRestore) == true &&
              fix.Value(Key::kAlbumProbe) == true,
          "l72 album fix arm: every key of l72 album fix (29), nothing ignored, fits in 1 KiB");
    const Parsed integer = Parse("mcla_album_preview_restore=1\n");
    Check(integer.ValueCount() == 0 && integer.issues[0].issue == LineIssue::kMalformed,
          "restore value must be true or false");
  }
  {
    // M11-015 kick-interval A/B: one key true applies; two or three true is a
    // conflict for the whole group (none applied), logged on every true line
    // but the earliest.
    const Parsed k256 = Parse("readback_kick_256=true\r\n"
                              "readback_kick_128=false\r\n"
                              "readback_kick_1024=false\r\n");
    Check(k256.ValueCount() == 3 && k256.Value(Key::kReadbackKick256) == true &&
              k256.Value(Key::kReadbackKick128) == false &&
              k256.Value(Key::kReadbackKick1024) == false && k256.ignored_lines == 0,
          "kick 256 arm; the other two false is not a conflict");
    const Parsed k1024 = Parse("readback_kick_1024=true\n");
    Check(k1024.ValueCount() == 1 && k1024.Value(Key::kReadbackKick1024) == true,
          "kick 1024 control arm");
    const Parsed two = Parse("readback_kick_128=true\n"
                             "memory_watch_census=true\n"
                             "readback_kick_1024=true\n"
                             "readback_kick_256=false\n");
    Check(two.ValueCount() == 2 && !two.Value(Key::kReadbackKick128) &&
              !two.Value(Key::kReadbackKick1024) && two.Value(Key::kReadbackKick256) == false &&
              two.Value(Key::kWatchCensus) == true,
          "two kick keys true: neither applies, the false one and other keys do");
    Check(two.ignored_lines == 1 && two.issue_count == 1 && two.issues[0].line == 3 &&
              two.issues[0].issue == LineIssue::kConflict,
          "kick conflict logged on the later line");
    const Parsed three = Parse("readback_kick_1024=true\n"
                               "readback_kick_256=true\n"
                               "d3d12_wrm_event_wake=true\n"
                               "readback_kick_128=true\n");
    Check(three.ValueCount() == 1 && !three.Value(Key::kReadbackKick256) &&
              !three.Value(Key::kReadbackKick128) && !three.Value(Key::kReadbackKick1024) &&
              three.Value(Key::kWrmEventWake) == true,
          "all three kick keys true: none applies (resolved as a group)");
    Check(three.ignored_lines == 2 && three.issue_count == 2 && three.issues[0].line == 2 &&
              three.issues[0].issue == LineIssue::kConflict && three.issues[1].line == 4 &&
              three.issues[1].issue == LineIssue::kConflict,
          "three-way kick conflict logged on the two later lines, in line order");
    const Parsed mixed = Parse("readback_kick_128=true\n"
                               "audio_speaker_config_surround=true\n"
                               "readback_kick_256=true\n"
                               "audio_speaker_config_stereo=true\n");
    Check(mixed.ValueCount() == 0 && mixed.issue_count == 2 && mixed.issues[0].line == 3 &&
              mixed.issues[1].line == 4,
          "kick and speaker conflicts in one file: each on its later line, in line order");
    const Parsed integer = Parse("d3d12_readback_sync_kick_draws=256\n"
                                 "readback_kick_256=256\n"
                                 "d3d12_readback_sync_kick_draws=true\n");
    Check(integer.ValueCount() == 0 && integer.ignored_lines == 3 &&
              integer.issues[0].issue == LineIssue::kMalformed &&
              integer.issues[1].issue == LineIssue::kMalformed &&
              integer.issues[2].issue == LineIssue::kUnknownName,
          "integer kick values and the cvar name itself are not accepted");
    // l72 kick arms: the l71 triad arm file (every l71 key, in file order)
    // plus the restore key false and the three kick keys explicit: every key
    // (32), nothing ignored, within 1 KiB. kick-512 is all three false.
    const std::string triad =
        "mcla_readback_sync_composite=true\n"
        "mcla_native_d3d_refcount=true\n"
        "mcla_native_xapi_interlocked=true\n"
        "d3d12_wrm_event_wake=true\n"
        "d3d12_replay_batched_wake=true\n"
        "d3d12_readback_elide_unread=true\n"
        "mcla_gpu_backend_vulkan=false\n"
        "prewarm_seed_store=true\n"
        "d3d12_readback_sync_targeted=true\n"
        "memory_watch_census=false\n"
        "memory_watch_probes=false\n"
        "memory_watch_disarm_bridge_safe_gaps=false\n"
        "shared_memory_upload_ahead_16=false\n"
        "shared_memory_upload_ahead_64=false\n"
        "shared_memory_hot_blocks_shadow=false\n"
        "shared_memory_hot_blocks_on=false\n"
        "mcla_warp_probe=true\n"
        "mcla_warp_watchdog=false\n"
        "mcla_texture_budget_soft_cap_off=false\n"
        "mcla_warp_finish_fix_off=false\n"
        "mcla_album_probe=true\n"
        "mcla_pursuit_probe=true\n"
        "xam_content_stable_creation_time_off=false\n"
        "xam_content_trace_off=false\n"
        "audio_speaker_config_stereo=false\n"
        "audio_speaker_config_surround=false\n"
        "audio_downmix_itu_off=false\n"
        "audio_channel_rms_probe=false\n"
        "mcla_album_preview_restore=false\n";
    const char *const kick_arms[3][3] = {{"false", "false", "false"},   // 512 (default)
                                         {"true", "false", "false"},    // 256
                                         {"false", "true", "false"}};   // 128
    for (const auto &arm : kick_arms) {
      const std::string text = triad + "readback_kick_256=" + arm[0] + "\n" +
                               "readback_kick_128=" + arm[1] + "\n" +
                               "readback_kick_1024=" + arm[2] + "\n";
      const Parsed a = Parse(text);
      Check(text.size() <= mcla::dev_overrides::kMaxBytes && a.status == FileStatus::kParsed &&
                a.ValueCount() == 32 && a.ignored_lines == 0,
            "l72 kick arm: every key (32), nothing ignored, fits in 1 KiB");
      Check(a.Value(Key::kReadbackKick256) == (arm[0][0] == 't') &&
                a.Value(Key::kReadbackKick128) == (arm[1][0] == 't') &&
                a.Value(Key::kReadbackKick1024) == false &&
                a.Value(Key::kAlbumPreviewRestore) == false &&
                a.Value(Key::kReadbackSyncTargeted) == true &&
                a.Value(Key::kReadbackSyncComposite) == true,
            "l72 kick arm values (targeted and composite on, so the kick acts)");
    }
    // l73 arms: the l72 triad arm (kick-512: all three kick keys false) plus
    // the three l73 keys explicit: every key (35), nothing ignored, within
    // kMaxBytes (2 KiB; the full file is beyond the former 1 KiB). Verify is
    // combined with the cache on, and both l73 A/B levers with each other.
    const std::string triad32 = triad +
                                "readback_kick_256=false\n"
                                "readback_kick_128=false\n"
                                "readback_kick_1024=false\n";
    const char *const l73_arms[5][3] = {{"false", "false", "false"},   // base
                                        {"true", "false", "false"},    // fetch-off
                                        {"false", "true", "false"},    // fetch-verify
                                        {"false", "false", "true"},    // steady
                                        {"true", "false", "true"}};    // steady-fetch-off
    for (const auto &arm : l73_arms) {
      const std::string text = triad32 + "fetch_identity_off=" + arm[0] + "\n" +
                               "fetch_identity_verify=" + arm[1] + "\n" +
                               "readback_steady_exempt=" + arm[2] + "\n";
      const Parsed a = Parse(text);
      Check(text.size() > 1024 && text.size() <= mcla::dev_overrides::kMaxBytes &&
                a.status == FileStatus::kParsed &&
                a.ValueCount() == 35 && a.ignored_lines == 0 && a.issue_count == 0,
            "l73 arm: every key (35), nothing ignored, fits in kMaxBytes");
      Check(a.Value(Key::kFetchIdentityOff) == (arm[0][0] == 't') &&
                a.Value(Key::kFetchIdentityVerify) == (arm[1][0] == 't') &&
                a.Value(Key::kReadbackSteadyExempt) == (arm[2][0] == 't') &&
                a.Value(Key::kReadbackKick256) == false &&
                a.Value(Key::kReadbackSyncComposite) == true,
            "l73 arm values (composite on, so the steady exemption can act)");
    }
    const Parsed all_three = Parse("readback_steady_exempt=true\r\n"
                                   "fetch_identity_verify=true\r\n"
                                   "fetch_identity_off=true\r\n");
    Check(all_three.ValueCount() == 3 && all_three.ignored_lines == 0 &&
              all_three.Value(Key::kFetchIdentityOff) == true &&
              all_three.Value(Key::kFetchIdentityVerify) == true &&
              all_three.Value(Key::kReadbackSteadyExempt) == true,
          "the l73 keys never conflict (three distinct cvars)");
    const Parsed l73_bad = Parse("d3d12_fetch_identity_cache=false\n"
                                 "fetch_identity_off=1\n"
                                 "readback_steady_exempt=on\n"
                                 "d3d12_readback_sync_steady_exempt=true\n"
                                 "fetch_identity_verify=true\n"
                                 "fetch_identity_verify=false\n");
    Check(l73_bad.ValueCount() == 1 && l73_bad.Value(Key::kFetchIdentityVerify) == true &&
              l73_bad.ignored_lines == 5 && l73_bad.issues[0].issue == LineIssue::kUnknownName &&
              l73_bad.issues[1].issue == LineIssue::kMalformed &&
              l73_bad.issues[2].issue == LineIssue::kMalformed &&
              l73_bad.issues[3].issue == LineIssue::kUnknownName &&
              l73_bad.issues[4].issue == LineIssue::kDuplicate,
          "l73 near misses: cvar names, non-bool values and repeats are ignored");
    // l73 KI-071 voice arms: every 35-key l73 arm plus the four XMA keys
    // explicit: every key (39), nothing ignored, within kMaxBytes. voice-fix =
    // all four true; voice-c1 = C1 + the swap trace only.
    const char *const xma_arms[3][4] = {{"false", "false", "false", "false"},  // base
                                        {"true", "true", "true", "true"},      // voice-fix
                                        {"true", "false", "false", "true"}};   // voice-c1
    for (const auto &arm : xma_arms) {
      const std::string text = triad32 + "fetch_identity_off=false\n"
                               "fetch_identity_verify=false\n"
                               "readback_steady_exempt=false\n"
                               "xma_swap_carry_packet=" + arm[0] + "\n" +
                               "xma_valid_only_when_written=" + arm[1] + "\n" +
                               "xma_work_lock_recheck=" + arm[2] + "\n" +
                               "xma_swap_trace=" + arm[3] + "\n";
      const Parsed a = Parse(text);
      Check(text.size() <= mcla::dev_overrides::kMaxBytes && a.status == FileStatus::kParsed &&
                a.ValueCount() == 39 && a.ignored_lines == 0 && a.issue_count == 0,
            "l73 voice arm: every key (39), nothing ignored, fits in kMaxBytes");
      Check(a.Value(Key::kXmaSwapCarryPacket) == (arm[0][0] == 't') &&
                a.Value(Key::kXmaValidOnlyWhenWritten) == (arm[1][0] == 't') &&
                a.Value(Key::kXmaWorkLockRecheck) == (arm[2][0] == 't') &&
                a.Value(Key::kXmaSwapTrace) == (arm[3][0] == 't') &&
                a.Value(Key::kReadbackSteadyExempt) == false,
            "l73 voice arm values");
    }
    const Parsed xma_bad = Parse("xma_swap_carry_packet=1\n"
                                 "xma_swap_trace=on\n"
                                 "xma_swap_carry=true\n"
                                 "xma_work_lock_recheck=true\n"
                                 "xma_work_lock_recheck=false\n");
    Check(xma_bad.ValueCount() == 1 && xma_bad.Value(Key::kXmaWorkLockRecheck) == true &&
              xma_bad.ignored_lines == 4 && xma_bad.issues[0].issue == LineIssue::kMalformed &&
              xma_bad.issues[1].issue == LineIssue::kMalformed &&
              xma_bad.issues[2].issue == LineIssue::kUnknownName &&
              xma_bad.issues[3].issue == LineIssue::kDuplicate,
          "XMA near misses: non-bool values, prefixes and repeats are ignored");
    // l74 KI-071 step 2 arms: the l73 base arm (39 keys, XMA fixes off) plus
    // the five step 2 keys explicit: every key (44), nothing ignored, within
    // kMaxBytes. trace = sibling trace + consume probe; d2, d1 = trace + one
    // fix; all = trace + both fixes + the codec reset.
    const std::string base39 = triad32 + "fetch_identity_off=false\n"
                               "fetch_identity_verify=false\n"
                               "readback_steady_exempt=false\n"
                               "xma_swap_carry_packet=false\n"
                               "xma_valid_only_when_written=false\n"
                               "xma_work_lock_recheck=false\n"
                               "xma_swap_trace=false\n";
    const char *const l74_arms[5][5] = {
        {"false", "false", "false", "false", "false"},  // base
        {"true", "false", "false", "false", "true"},    // trace
        {"true", "true", "false", "false", "true"},     // d2
        {"true", "false", "true", "false", "true"},     // d1
        {"true", "true", "true", "true", "true"}};      // all
    for (const auto &arm : l74_arms) {
      const std::string text = base39 + "xma_sibling_trace=" + arm[0] + "\n" +
                               "xma_stall_on_invalid_next=" + arm[1] + "\n" +
                               "xma_honor_start_skip=" + arm[2] + "\n" +
                               "xma_reset_codec_on_clear=" + arm[3] + "\n" +
                               "mcla_xma_consume_probe=" + arm[4] + "\n";
      const Parsed a = Parse(text);
      Check(text.size() <= mcla::dev_overrides::kMaxBytes && a.status == FileStatus::kParsed &&
                a.ValueCount() == 44 && a.ignored_lines == 0 && a.issue_count == 0,
            "l74 arm: every l74 key (44), nothing ignored, fits in kMaxBytes");
      Check(a.Value(Key::kXmaSiblingTrace) == (arm[0][0] == 't') &&
                a.Value(Key::kXmaStallOnInvalidNext) == (arm[1][0] == 't') &&
                a.Value(Key::kXmaHonorStartSkip) == (arm[2][0] == 't') &&
                a.Value(Key::kXmaResetCodecOnClear) == (arm[3][0] == 't') &&
                a.Value(Key::kXmaConsumeProbe) == (arm[4][0] == 't') &&
                a.Value(Key::kXmaSwapCarryPacket) == false &&
                a.Value(Key::kXmaSwapTrace) == false,
            "l74 arm values (the 4a3aea1 XMA fixes off in every arm)");
    }
    const Parsed l74_bad = Parse("xma_sibling_trace=1\n"
                                 "mcla_xma_consume_probe=yes\n"
                                 "xma_consume_probe=true\n"
                                 "xma_honor_start_skip=true\n"
                                 "xma_honor_start_skip=false\n"
                                 "mcla_xma_consume_probe=true\n");
    Check(l74_bad.ValueCount() == 2 && l74_bad.Value(Key::kXmaHonorStartSkip) == true &&
              l74_bad.Value(Key::kXmaConsumeProbe) == true && l74_bad.ignored_lines == 4 &&
              l74_bad.issues[0].issue == LineIssue::kMalformed &&
              l74_bad.issues[1].issue == LineIssue::kMalformed &&
              l74_bad.issues[2].issue == LineIssue::kUnknownName &&
              l74_bad.issues[3].issue == LineIssue::kDuplicate,
          "l74 near misses: non-bool values, prefixes and repeats are ignored");
    // l75 KI-071 step 3 arms: the l74 base (44 keys) plus the four step 3
    // keys explicit: every key (48), nothing ignored, within kMaxBytes.
    // trim-trace = sibling trace + consume probe + dump; trim-fix = + trim
    // fix; trim-block = + block-granular trim.
    const char *const l75_arms[4][6] = {
        // sibling, consume, honor_trim, block, all_rates, dump_on
        {"false", "false", "false", "false", "false", "false"},  // base
        {"true", "true", "false", "false", "false", "true"},     // trim-trace
        {"true", "true", "true", "false", "false", "true"},      // trim-fix
        {"true", "true", "true", "true", "false", "true"}};      // trim-block
    for (const auto &arm : l75_arms) {
      const std::string text = base39 + "xma_sibling_trace=" + arm[0] + "\n" +
                               "xma_stall_on_invalid_next=false\n"
                               "xma_honor_start_skip=false\n"
                               "xma_reset_codec_on_clear=false\n"
                               "mcla_xma_consume_probe=" + arm[1] + "\n" +
                               "xma_honor_frame_trim=" + arm[2] + "\n" +
                               "xma_frame_trim_block_granular=" + arm[3] + "\n" +
                               "xma_dump_all_rates=" + arm[4] + "\n" +
                               "xma_dump_contexts_on=" + arm[5] + "\n";
      const Parsed a = Parse(text);
      Check(text.size() <= mcla::dev_overrides::kMaxBytes && a.status == FileStatus::kParsed &&
                a.ValueCount() == 48 && a.ignored_lines == 0 && a.issue_count == 0,
            "l75 arm: every l75 key (48), nothing ignored, fits in kMaxBytes");
      Check(a.Value(Key::kXmaSiblingTrace) == (arm[0][0] == 't') &&
                a.Value(Key::kXmaConsumeProbe) == (arm[1][0] == 't') &&
                a.Value(Key::kXmaHonorFrameTrim) == (arm[2][0] == 't') &&
                a.Value(Key::kXmaFrameTrimBlockGranular) == (arm[3][0] == 't') &&
                a.Value(Key::kXmaDumpAllRates) == (arm[4][0] == 't') &&
                a.Value(Key::kXmaDumpContextsOn) == (arm[5][0] == 't') &&
                a.Value(Key::kXmaHonorStartSkip) == false &&
                a.Value(Key::kXmaStallOnInvalidNext) == false,
            "l75 arm values (D1/D2 off in every arm)");
    }
    const Parsed l75_bad = Parse("xma_dump_contexts=C:/dump\n"
                                 "xma_dump_contexts_on=1\n"
                                 "xma_honor_frame_trim=on\n"
                                 "xma_frame_trim=true\n"
                                 "xma_dump_contexts_on=true\n"
                                 "xma_dump_contexts_on=false\n"
                                 "xma_dump_all_rates=true\n");
    Check(l75_bad.ValueCount() == 2 && l75_bad.Value(Key::kXmaDumpContextsOn) == true &&
              l75_bad.Value(Key::kXmaDumpAllRates) == true && l75_bad.ignored_lines == 5 &&
              l75_bad.issues[0].issue == LineIssue::kMalformed &&
              l75_bad.issues[1].issue == LineIssue::kMalformed &&
              l75_bad.issues[2].issue == LineIssue::kMalformed &&
              l75_bad.issues[3].issue == LineIssue::kUnknownName &&
              l75_bad.issues[4].issue == LineIssue::kDuplicate,
          "l75 near misses: a path value, non-bool values, prefixes and repeats are ignored");
    // l76 arms: the l75 base (48 keys) plus the three KI-071 step 4 keys and
    // the two M11-007d keys explicit: every key (53), nothing ignored, within
    // kMaxBytes. walk-trace = sibling trace + consume probe + dump + input
    // dump + walk trace; walk-fix = + walk fix; steady-probe = lock probe;
    // steady-ensure = + read-back completion at the lock.
    const char *const l76_arms[5][8] = {
        // sibling, consume, dump_on, walk_trace, walk_fix, dump_input, lock_probe, ensure
        {"false", "false", "false", "false", "false", "false", "false", "false"},  // base
        {"true", "true", "true", "true", "false", "true", "false", "false"},  // walk-trace
        {"true", "true", "true", "true", "true", "true", "false", "false"},   // walk-fix
        {"false", "false", "false", "false", "false", "false", "true", "false"},  // probe
        {"false", "false", "false", "false", "false", "false", "true", "true"}};  // ensure
    for (const auto &arm : l76_arms) {
      const std::string text = base39 + "xma_sibling_trace=" + arm[0] + "\n" +
                               "xma_stall_on_invalid_next=false\n"
                               "xma_honor_start_skip=false\n"
                               "xma_reset_codec_on_clear=false\n"
                               "mcla_xma_consume_probe=" + arm[1] + "\n" +
                               "xma_honor_frame_trim=false\n"
                               "xma_frame_trim_block_granular=false\n"
                               "xma_dump_all_rates=false\n"
                               "xma_dump_contexts_on=" + arm[2] + "\n" +
                               "xma_frame_walk_trace=" + arm[3] + "\n" +
                               "xma_frame_walk_fix=" + arm[4] + "\n" +
                               "xma_dump_input=" + arm[5] + "\n" +
                               "mcla_lock_probe=" + arm[6] + "\n" +
                               "mcla_lock_readback_ensure=" + arm[7] + "\n";
      const Parsed a = Parse(text);
      Check(text.size() <= mcla::dev_overrides::kMaxBytes && a.status == FileStatus::kParsed &&
                a.ValueCount() == 53 && a.ignored_lines == 0 && a.issue_count == 0,
            "l76 arm: every l76 key (53), nothing ignored, fits in kMaxBytes");
      Check(a.Value(Key::kLockProbe) == (arm[6][0] == 't') &&
                a.Value(Key::kLockReadbackEnsure) == (arm[7][0] == 't'),
            "l76 arm lock values");
      Check(a.Value(Key::kXmaSiblingTrace) == (arm[0][0] == 't') &&
                a.Value(Key::kXmaConsumeProbe) == (arm[1][0] == 't') &&
                a.Value(Key::kXmaDumpContextsOn) == (arm[2][0] == 't') &&
                a.Value(Key::kXmaFrameWalkTrace) == (arm[3][0] == 't') &&
                a.Value(Key::kXmaFrameWalkFix) == (arm[4][0] == 't') &&
                a.Value(Key::kXmaDumpInput) == (arm[5][0] == 't') &&
                a.Value(Key::kXmaHonorFrameTrim) == false &&
                a.Value(Key::kXmaFrameTrimBlockGranular) == false,
            "l76 arm values (frame trim off in every arm)");
    }
    const Parsed l76_bad = Parse("xma_frame_walk_trace=1\n"
                                 "xma_frame_walk_fix=on\n"
                                 "xma_walk_fix=true\n"
                                 "xma_dump_input=true\n"
                                 "xma_dump_input=false\n"
                                 "xma_frame_walk_fix=true\n");
    Check(l76_bad.ValueCount() == 2 && l76_bad.Value(Key::kXmaDumpInput) == true &&
              l76_bad.Value(Key::kXmaFrameWalkFix) == true &&
              !l76_bad.Value(Key::kXmaFrameWalkTrace) && l76_bad.ignored_lines == 4 &&
              l76_bad.issues[0].issue == LineIssue::kMalformed &&
              l76_bad.issues[1].issue == LineIssue::kMalformed &&
              l76_bad.issues[2].issue == LineIssue::kUnknownName &&
              l76_bad.issues[3].issue == LineIssue::kDuplicate,
          "l76 near misses: non-bool values, prefixes and repeats are ignored");
    // l77 arms: the l76 base / steady arm plus the two l77 keys explicit:
    // every key (55), nothing ignored, within kMaxBytes. walk-off = base +
    // xma_frame_walk_fix_off (KI-071 control); *-fence = + fence_write_deferred.
    // Second l77 patch: + raw_timer_off and the three vblank caps (59 keys).
    const char *const l77_arms[10][7] = {
        // steady_exempt, fence_deferred, walk_fix_off, raw_off, hz72, hz80, hz88
        {"false", "false", "false", "false", "false", "false", "false"},  // base
        {"false", "false", "true", "false", "false", "false", "false"},   // walk-off
        {"true", "false", "false", "false", "false", "false", "false"},   // steady
        {"true", "true", "false", "false", "false", "false", "false"},    // steady-fence
        {"false", "true", "false", "false", "false", "false", "false"},   // base-fence
        {"true", "false", "false", "true", "false", "false", "false"},    // steady-rawoff
        {"true", "false", "false", "false", "true", "false", "false"},    // steady-cap36
        {"true", "false", "false", "false", "false", "true", "false"},    // steady-cap40
        {"true", "false", "false", "false", "false", "false", "true"},    // steady-cap44
        {"false", "false", "false", "false", "true", "false", "false"}};  // base-cap36
    for (const auto &arm : l77_arms) {
      std::string l76_text = base39 +
                             "xma_sibling_trace=false\n"
                             "xma_stall_on_invalid_next=false\n"
                             "xma_honor_start_skip=false\n"
                             "xma_reset_codec_on_clear=false\n"
                             "mcla_xma_consume_probe=false\n"
                             "xma_honor_frame_trim=false\n"
                             "xma_frame_trim_block_granular=false\n"
                             "xma_dump_all_rates=false\n"
                             "xma_dump_contexts_on=false\n"
                             "xma_frame_walk_trace=false\n"
                             "xma_frame_walk_fix=false\n"
                             "xma_dump_input=false\n"
                             "mcla_lock_probe=false\n"
                             "mcla_lock_readback_ensure=false\n";
      const std::string_view exempt_false = "readback_steady_exempt=false\n";
      const size_t at = l76_text.find(exempt_false);
      Check(at != std::string::npos, "the l76 base text holds the steady key");
      if (at == std::string::npos) {
        continue;
      }
      l76_text.replace(at, exempt_false.size(),
                       std::string("readback_steady_exempt=") + arm[0] + "\n");
      const std::string text = l76_text + "fence_write_deferred=" + arm[1] + "\n" +
                               "xma_frame_walk_fix_off=" + arm[2] + "\n" +
                               "raw_timer_off=" + arm[3] + "\n" +
                               "vblank_hz_72=" + arm[4] + "\n" +
                               "vblank_hz_80=" + arm[5] + "\n" +
                               "vblank_hz_88=" + arm[6] + "\n";
      const Parsed a = Parse(text);
      Check(text.size() <= mcla::dev_overrides::kMaxBytes && a.status == FileStatus::kParsed &&
                a.ValueCount() == mcla::dev_overrides::kKeyCount && a.ignored_lines == 0 &&
                a.issue_count == 0,
            "l77 arm: every key (59), nothing ignored, fits in kMaxBytes");
      Check(a.Value(Key::kRawTimerOff) == (arm[3][0] == 't') &&
                a.Value(Key::kVblankHz72) == (arm[4][0] == 't') &&
                a.Value(Key::kVblankHz80) == (arm[5][0] == 't') &&
                a.Value(Key::kVblankHz88) == (arm[6][0] == 't'),
            "l77 arm KI-074 values");
      Check(a.Value(Key::kReadbackSteadyExempt) == (arm[0][0] == 't') &&
                a.Value(Key::kFenceWriteDeferred) == (arm[1][0] == 't') &&
                a.Value(Key::kXmaFrameWalkFixOff) == (arm[2][0] == 't') &&
                a.Value(Key::kXmaFrameWalkFix) == false && a.Value(Key::kLockProbe) == false,
            "l77 arm values (walk fix key false: the SDK default applies the fix)");
    }
    // The walk fix key and its control: both true is a conflict (neither
    // applies, logged on the later line); one true with the other false is not.
    const Parsed walk_both = Parse("xma_frame_walk_fix_off=true\n"
                                   "fence_write_deferred=true\n"
                                   "xma_frame_walk_fix=true\n");
    Check(walk_both.ValueCount() == 1 && !walk_both.Value(Key::kXmaFrameWalkFix) &&
              !walk_both.Value(Key::kXmaFrameWalkFixOff) &&
              walk_both.Value(Key::kFenceWriteDeferred) == true && walk_both.issue_count == 1 &&
              walk_both.issues[0].line == 3 && walk_both.issues[0].issue == LineIssue::kConflict,
          "walk fix with its control both true: neither applies, conflict on the later line");
    const Parsed walk_off = Parse("xma_frame_walk_fix=false\nxma_frame_walk_fix_off=true\n");
    Check(walk_off.ValueCount() == 2 && walk_off.Value(Key::kXmaFrameWalkFixOff) == true &&
              walk_off.Value(Key::kXmaFrameWalkFix) == false && walk_off.issue_count == 0,
          "walk-off arm: fix=false with fix_off=true is not a conflict");
    const Parsed walk_on = Parse("xma_frame_walk_fix=true\nxma_frame_walk_fix_off=false\n");
    Check(walk_on.ValueCount() == 2 && walk_on.Value(Key::kXmaFrameWalkFix) == true &&
              walk_on.Value(Key::kXmaFrameWalkFixOff) == false && walk_on.issue_count == 0,
          "fix=true with fix_off=false is not a conflict");
    const Parsed l77_bad = Parse("fence_write_deferred=1\n"
                                 "d3d12_fence_write_deferred=true\n"
                                 "xma_frame_walk_fix_off=off\n"
                                 "fence_write_deferred=true\n"
                                 "fence_write_deferred=false\n"
                                 "xma_frame_walk_fix_off=true\n");
    Check(l77_bad.ValueCount() == 2 && l77_bad.Value(Key::kFenceWriteDeferred) == true &&
              l77_bad.Value(Key::kXmaFrameWalkFixOff) == true && l77_bad.ignored_lines == 4 &&
              l77_bad.issues[0].issue == LineIssue::kMalformed &&
              l77_bad.issues[1].issue == LineIssue::kUnknownName &&
              l77_bad.issues[2].issue == LineIssue::kMalformed &&
              l77_bad.issues[3].issue == LineIssue::kDuplicate,
          "l77 near misses: non-bool values, the cvar name and repeats are ignored");
    // The vblank caps: one true applies; two or three true is a conflict for
    // the whole group (none applied), logged on every later true line.
    const Parsed cap = Parse("vblank_hz_72=false\nvblank_hz_80=true\nvblank_hz_88=false\n"
                             "raw_timer_off=true\n");
    Check(cap.ValueCount() == 4 && cap.Value(Key::kVblankHz80) == true &&
              cap.Value(Key::kRawTimerOff) == true && cap.issue_count == 0,
          "cap 40 arm; the other two false is not a conflict; raw_timer_off combines");
    const Parsed caps = Parse("vblank_hz_88=true\n"
                              "raw_timer_off=false\n"
                              "vblank_hz_72=true\n"
                              "vblank_hz_80=true\n");
    Check(caps.ValueCount() == 1 && !caps.Value(Key::kVblankHz72) &&
              !caps.Value(Key::kVblankHz80) && !caps.Value(Key::kVblankHz88) &&
              caps.Value(Key::kRawTimerOff) == false && caps.issue_count == 2 &&
              caps.issues[0].line == 3 && caps.issues[0].issue == LineIssue::kConflict &&
              caps.issues[1].line == 4 && caps.issues[1].issue == LineIssue::kConflict,
          "three vblank caps true: none applies, conflicts on the two later lines");
    const Parsed cap_bad = Parse("vblank_hz_72=72\n"
                                 "mcla_guest_vblank_hz=true\n"
                                 "vblank_hz_76=true\n"
                                 "raw_timer_off=off\n"
                                 "vblank_hz_72=true\n"
                                 "vblank_hz_72=false\n");
    Check(cap_bad.ValueCount() == 1 && cap_bad.Value(Key::kVblankHz72) == true &&
              cap_bad.ignored_lines == 5 && cap_bad.issues[0].issue == LineIssue::kMalformed &&
              cap_bad.issues[1].issue == LineIssue::kUnknownName &&
              cap_bad.issues[2].issue == LineIssue::kUnknownName &&
              cap_bad.issues[3].issue == LineIssue::kMalformed &&
              cap_bad.issues[4].issue == LineIssue::kDuplicate,
          "KI-074 near misses: integer values, the cvar name, other rates and repeats");
  }
  {
    // The M10-007 / KI-048 owner A/B: cap off versus the build default.
    const Parsed off = Parse("mcla_texture_budget_soft_cap_off=true\r\n");
    Check(off.status == FileStatus::kParsed && off.ValueCount() == 1 &&
              off.Value(Key::kTextureSoftCapOff) == true && off.ignored_lines == 0,
          "soft cap off arm parses");
    const Parsed on = Parse("mcla_texture_budget_soft_cap_off=false\n");
    Check(on.ValueCount() == 1 && on.Value(Key::kTextureSoftCapOff) == false,
          "soft cap default arm parses");
    const Parsed integer = Parse("mcla_texture_budget_soft_cap_mb=0\n"
                                 "mcla_texture_budget_soft_cap_off=0\n");
    Check(integer.ValueCount() == 0 && integer.ignored_lines == 2 &&
              integer.issues[0].issue == LineIssue::kMalformed &&
              integer.issues[1].issue == LineIssue::kMalformed,
          "integer soft cap values are not accepted");
    const Parsed name = Parse("mcla_texture_budget_soft_cap_mb=true\n");
    Check(name.ValueCount() == 0 && name.issues[0].issue == LineIssue::kUnknownName,
          "the integer cvar name is not a key");
  }
  {
    // KI-071 speaker config: one key true applies; both true is a conflict
    // (neither applied, logged on the later line), like the integer pairs.
    const Parsed stereo = Parse("audio_speaker_config_stereo=true\r\n"
                                "audio_speaker_config_surround=false\r\n");
    Check(stereo.ValueCount() == 2 && stereo.Value(Key::kAudioSpeakerStereo) == true &&
              stereo.Value(Key::kAudioSpeakerSurround) == false && stereo.ignored_lines == 0,
          "stereo arm; surround=false with stereo=true is not a conflict");
    const Parsed both = Parse("audio_speaker_config_surround=true\n"
                              "audio_channel_rms_probe=true\n"
                              "audio_speaker_config_stereo=true\n"
                              "audio_downmix_itu_off=true\n");
    Check(both.ValueCount() == 2 && !both.Value(Key::kAudioSpeakerStereo) &&
              !both.Value(Key::kAudioSpeakerSurround) &&
              both.Value(Key::kAudioChannelRmsProbe) == true &&
              both.Value(Key::kAudioDownmixItuOff) == true,
          "both speaker config keys true: neither applies, the other keys do");
    Check(both.ignored_lines == 1 && both.issue_count == 1 && both.issues[0].line == 3 &&
              both.issues[0].issue == LineIssue::kConflict,
          "speaker config conflict logged on the later line");
    const Parsed text = Parse("audio_speaker_config=stereo\n"
                              "audio_speaker_config_stereo=stereo\n"
                              "audio_downmix_itu=false\n");
    Check(text.ValueCount() == 0 && text.ignored_lines == 3 &&
              text.issues[0].issue == LineIssue::kMalformed &&
              text.issues[1].issue == LineIssue::kMalformed &&
              text.issues[2].issue == LineIssue::kUnknownName,
          "string values and the cvar names themselves are not accepted");
  }
  {
    const Parsed p = Parse("prewarm_seed_store=false\n");
    Check(p.status == FileStatus::kParsed && p.Value(Key::kPrewarmSeedStore) == false,
          "store seeding off parses");
  }
  Check(mcla::dev_overrides::kFile == "mcla-dev-overrides.txt", "file name (same as l55)");
  Check(mcla::dev_overrides::kDirectory == "MCLA-R-Launcher-v1", "launcher per-user directory");
  Check(mcla::dev_overrides::kMaxBytes == 2048, "2 KiB limit (l73)");
}

void TestValidFiles() {
  {
    const Parsed p = Parse("");
    Check(p.status == FileStatus::kParsed && NoValues(p) && p.ignored_lines == 0, "empty file");
  }
  {
    // The l55 single-line format keeps working.
    const Parsed p = Parse("mcla_readback_sync_composite=false\r\n");
    Check(p.status == FileStatus::kParsed, "l55 line parsed");
    Check(p.Value(Key::kReadbackSyncComposite) == false, "l55 line value");
    Check(p.ValueCount() == 1 && p.ignored_lines == 0, "l55 line only");
  }
  {
    const Parsed p = Parse("mcla_readback_sync_composite=true");
    Check(p.Value(Key::kReadbackSyncComposite) == true, "no final line end");
  }
  {
    const Parsed p = Parse(
        "mcla_readback_sync_composite=true\r\n"
        "mcla_native_d3d_refcount=false\r\n"
        "mcla_native_xapi_interlocked=false\r\n"
        "d3d12_wrm_event_wake=true\r\n"
        "d3d12_replay_batched_wake=false\r\n"
        "d3d12_readback_elide_unread=true\r\n");
    Check(p.status == FileStatus::kParsed && p.ValueCount() == 6 && p.ignored_lines == 0,
          "six keys");
    Check(p.Value(Key::kReadbackSyncComposite) == true, "all six: readback");
    Check(p.Value(Key::kNativeD3dRefcount) == false, "all six: refcount");
    Check(p.Value(Key::kNativeXapiInterlocked) == false, "all six: xapi");
    Check(p.Value(Key::kWrmEventWake) == true, "all six: wrm");
    Check(p.Value(Key::kReplayBatchedWake) == false, "all six: replay");
    Check(p.Value(Key::kReadbackElideUnread) == true, "all six: elide");
  }
  {
    // The l58 arm files: six lines of at most 37 bytes fit the 1 KiB limit.
    const Parsed p = Parse(
        "mcla_readback_sync_composite=true\r\n"
        "mcla_native_d3d_refcount=false\r\n"
        "mcla_native_xapi_interlocked=false\r\n"
        "d3d12_wrm_event_wake=false\r\n"
        "d3d12_replay_batched_wake=false\r\n"
        "d3d12_readback_elide_unread=false\r\n");
    Check(p.ValueCount() == 6 && p.Value(Key::kReadbackElideUnread) == false &&
              p.Value(Key::kReadbackSyncComposite) == true,
          "l58 baseline arm");
  }
  {
    // The l59 Deck arm files (private/l59-deck-arms/<arm>/mcla-dev-overrides.txt,
    // ASCII LF): all seven keys, read-back sync on, Vulkan on or off.
    constexpr const char *kVulkanArm =
        "mcla_readback_sync_composite=true\n"
        "mcla_native_d3d_refcount=false\n"
        "mcla_native_xapi_interlocked=false\n"
        "d3d12_wrm_event_wake=false\n"
        "d3d12_replay_batched_wake=false\n"
        "d3d12_readback_elide_unread=false\n"
        "mcla_gpu_backend_vulkan=true\n";
    const Parsed v = Parse(kVulkanArm);
    Check(v.status == FileStatus::kParsed && v.ValueCount() == 7 && v.ignored_lines == 0,
          "l59 vulkan arm: seven keys");
    Check(v.Value(Key::kGpuBackendVulkan) == true &&
              v.Value(Key::kReadbackSyncComposite) == true &&
              v.Value(Key::kReadbackElideUnread) == false &&
              v.Value(Key::kNativeD3dRefcount) == false,
          "l59 vulkan arm values");
    const Parsed d = Parse(
        "mcla_readback_sync_composite=true\n"
        "mcla_native_d3d_refcount=false\n"
        "mcla_native_xapi_interlocked=false\n"
        "d3d12_wrm_event_wake=false\n"
        "d3d12_replay_batched_wake=false\n"
        "d3d12_readback_elide_unread=false\n"
        "mcla_gpu_backend_vulkan=false\n");
    Check(d.ValueCount() == 7 && d.ignored_lines == 0 &&
              d.Value(Key::kGpuBackendVulkan) == false &&
              d.Value(Key::kReadbackSyncComposite) == true,
          "l59 d3d12 arm");
    const Parsed bad = Parse("mcla_gpu_backend_vulkan=vulkan\ngpu_backend=true\n");
    Check(bad.ValueCount() == 0 && bad.ignored_lines == 2 &&
              bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName,
          "no string backend values, gpu_backend unknown");
  }
  {
    const Parsed p = Parse("d3d12_wrm_event_wake=true\nd3d12_replay_batched_wake=true\n");
    Check(p.ValueCount() == 2 && p.Value(Key::kWrmEventWake) == true &&
              p.Value(Key::kReplayBatchedWake) == true && !p.Value(Key::kNativeD3dRefcount),
          "LF line ends, absent keys stay unset");
  }
  {
    const Parsed p = Parse("# l57 arm: wrm\r\n\r\n\nd3d12_wrm_event_wake=true\r\n\r\n");
    Check(p.ValueCount() == 1 && p.ignored_lines == 0 && p.Value(Key::kWrmEventWake) == true,
          "comments and empty lines are skipped silently");
  }
}

void TestIgnoredLines() {
  {
    const Parsed p = Parse(
        "d3d12_wrm_event_wake=true\n"
        "d3d12_wrm_event_wake = false\n"  // spaces
        "bogus_name=true\n"               // unknown
        "d3d12_replay_batched_wake=1\n"   // value
        "d3d12_wrm_event_wake=false\n");  // duplicate
    Check(p.status == FileStatus::kParsed, "mixed file parsed");
    Check(p.ValueCount() == 1 && p.Value(Key::kWrmEventWake) == true,
          "only the valid first line applies");
    Check(p.ignored_lines == 4 && p.issue_count == 4, "four lines ignored");
    Check(p.issues[0].line == 2 && p.issues[0].issue == LineIssue::kMalformed, "line 2 spaces");
    Check(p.issues[1].line == 3 && p.issues[1].issue == LineIssue::kUnknownName, "line 3 name");
    Check(p.issues[2].line == 4 && p.issues[2].issue == LineIssue::kMalformed, "line 4 value");
    Check(p.issues[3].line == 5 && p.issues[3].issue == LineIssue::kDuplicate, "line 5 repeat");
  }
  const char *malformed[] = {
      "d3d12_wrm_event_wake",           "d3d12_wrm_event_wake=",
      "=true",                          "d3d12_wrm_event_wake=True",
      "d3d12_wrm_event_wake=TRUE",      "d3d12_wrm_event_wake=yes",
      "d3d12_wrm_event_wake=0",         "d3d12_wrm_event_wake= true",
      "d3d12_wrm_event_wake=true ",     " d3d12_wrm_event_wake=true",
      "\td3d12_wrm_event_wake=true",    "d3d12_wrm_event_wake=true=true",
      "d3d12_wrm_event_wake==true",     "d3d12_wrm_event_wake=true\r\r",
      "d3d12_wrm_event_wake=true;",     "d3d12_wrm_event_wake:true",
  };
  for (const char *line : malformed) {
    const Parsed p = Parse(line);
    Check(p.status == FileStatus::kParsed && NoValues(p) && p.ignored_lines == 1 &&
              p.issues[0].issue == LineIssue::kMalformed,
          line);
  }
  {
    const Parsed p = Parse(std::string("d3d12_wrm_event_wake=true\0", 26));
    Check(NoValues(p) && p.issues[0].issue == LineIssue::kMalformed, "NUL in a line");
  }
  {
    const Parsed p = Parse("d3d12_readback_sync_composite=true\nreadback_resolve=true\n");
    Check(NoValues(p) && p.ignored_lines == 2 && p.issues[0].issue == LineIssue::kUnknownName &&
              p.issues[1].issue == LineIssue::kUnknownName,
          "cvars outside the whitelist are not applied");
  }
  {
    std::string many;
    for (int i = 0; i < 40; ++i) {
      many += "x=true\n";
    }
    const Parsed p = Parse(many);
    Check(p.ignored_lines == 40 && p.issue_count == mcla::dev_overrides::kMaxIssues,
          "issue list is bounded, count is not");
    Check(p.issues[15].line == 16, "issue line numbers");
  }
}

void TestRejectedFiles() {
  {
    const Parsed p = Parse("\xEF\xBB\xBFmcla_readback_sync_composite=false");
    Check(p.status == FileStatus::kRejected && NoValues(p), "BOM rejects the file");
  }
  {
    const Parsed p = Parse("d3d12_wrm_event_wake=true\n# caf\xC3\xA9\n");
    Check(p.status == FileStatus::kRejected && NoValues(p),
          "non-ASCII anywhere (also in a comment) rejects the whole file");
  }
  {
    std::string padded = "d3d12_wrm_event_wake=true\n";
    padded += std::string(mcla::dev_overrides::kMaxBytes - padded.size(), '\n');
    Check(padded.size() == 2048, "padded size");
    const Parsed ok = Parse(padded);
    Check(ok.status == FileStatus::kParsed && ok.Value(Key::kWrmEventWake) == true,
          "exactly 2 KiB accepted");
    padded += '\n';
    const Parsed big = Parse(padded);
    Check(big.status == FileStatus::kRejected && NoValues(big), "2049 bytes rejected");
  }
}

void TestApplyDecision() {
  Check(DecideApply(false, false) == ApplyAction::kUnavailable, "cvar missing");
  Check(DecideApply(false, true) == ApplyAction::kUnavailable, "cvar missing wins");
  Check(DecideApply(true, true) == ApplyAction::kUserSet, "command line wins over the file");
  Check(DecideApply(true, false) == ApplyAction::kApply, "default value: apply");
  static_assert(DecideApply(true, false) == ApplyAction::kApply);
  // overrides_user keys (the vblank caps) replace a command-line value.
  Check(DecideApply(true, true, true) == ApplyAction::kReplaceUserSet,
        "an overrides_user key replaces the command-line value");
  Check(DecideApply(true, false, true) == ApplyAction::kApply,
        "an overrides_user key at the default: plain apply");
  Check(DecideApply(false, true, true) == ApplyAction::kUnavailable,
        "an overrides_user key with the cvar missing: unavailable");
  Check(DecideApply(true, true, false) == ApplyAction::kUserSet,
        "other keys still yield to the command line");
  static_assert(DecideApply(true, true, true) == ApplyAction::kReplaceUserSet);
}

void TestConstexpr() {
  constexpr Parsed p = Parse("mcla_native_d3d_refcount=false\nd3d12_wrm_event_wake=true\n");
  static_assert(p.status == FileStatus::kParsed);
  static_assert(p.ValueCount() == 2);
  static_assert(p.Value(Key::kNativeD3dRefcount) == false);
  static_assert(p.Value(Key::kWrmEventWake) == true);
  Check(std::string(mcla::dev_overrides::LineIssueName(LineIssue::kUnknownName)) ==
            "unknown name",
        "issue names");
  constexpr Parsed conflict =
      Parse("shared_memory_upload_ahead_64=true\nshared_memory_upload_ahead_16=true\n");
  static_assert(conflict.ValueCount() == 0);
  static_assert(conflict.issues[0].line == 2);
  constexpr Parsed hot_conflict =
      Parse("shared_memory_hot_blocks_shadow=true\nshared_memory_hot_blocks_on=true\n");
  static_assert(hot_conflict.ValueCount() == 0);
  static_assert(hot_conflict.issues[0].line == 2);
  static_assert(hot_conflict.issues[0].issue == LineIssue::kConflict);
  constexpr Parsed audio_conflict =
      Parse("audio_speaker_config_stereo=true\naudio_speaker_config_surround=true\n");
  static_assert(audio_conflict.ValueCount() == 0);
  static_assert(audio_conflict.issues[0].line == 2);
  static_assert(audio_conflict.issues[0].issue == LineIssue::kConflict);
  constexpr Parsed kick_conflict =
      Parse("readback_kick_256=true\nreadback_kick_128=true\nreadback_kick_1024=true\n");
  static_assert(kick_conflict.ValueCount() == 0);
  static_assert(kick_conflict.issue_count == 2);
  static_assert(kick_conflict.issues[0].line == 2 && kick_conflict.issues[1].line == 3);
  static_assert(kick_conflict.issues[1].issue == LineIssue::kConflict);
  constexpr Parsed walk_conflict =
      Parse("xma_frame_walk_fix=true\nxma_frame_walk_fix_off=true\n");
  static_assert(walk_conflict.ValueCount() == 0);
  static_assert(walk_conflict.issues[0].line == 2);
  static_assert(walk_conflict.issues[0].issue == LineIssue::kConflict);
  constexpr Parsed cap_conflict = Parse("vblank_hz_80=true\nvblank_hz_72=true\n");
  static_assert(cap_conflict.ValueCount() == 0);
  static_assert(cap_conflict.issues[0].line == 2);
  static_assert(cap_conflict.issues[0].issue == LineIssue::kConflict);
  Check(std::string(mcla::dev_overrides::LineIssueName(LineIssue::kConflict)) ==
            "conflicting keys for one cvar (neither is applied)",
        "conflict issue name covers every pair");
}
}  // namespace

int main() {
  TestWhitelist();
  TestValidFiles();
  TestIgnoredLines();
  TestRejectedFiles();
  TestApplyDecision();
  TestConstexpr();
  std::printf("mcla_dev_overrides_test: %d checks passed\n", checks);
  return 0;
}
