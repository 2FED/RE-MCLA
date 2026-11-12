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
  Check(mcla::dev_overrides::kKeyCount == 103, "one hundred and three whitelisted names");
  {
    // l93 time-of-day keys: host init-only ints at startup; the rate keys
    // write mcla_tod_rate (10 / 30 / 0 = freeze), the start keys
    // mcla_tod_start_minutes (270 / 330); =false writes nothing.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    struct TodKey {
      Key key;
      const char *name;
      const char *cvar;
      const char *text;
    };
    const TodKey tod_keys[] = {
        {Key::kTodRateX10, "tod_rate_x10", "mcla_tod_rate", "10"},
        {Key::kTodRateX30, "tod_rate_x30", "mcla_tod_rate", "30"},
        {Key::kTodFreeze, "tod_freeze", "mcla_tod_rate", "0"},
        {Key::kTodStart0430, "tod_start_0430", "mcla_tod_start_minutes", "270"},
        {Key::kTodStart0530, "tod_start_0530", "mcla_tod_start_minutes", "330"},
    };
    for (const TodKey &k : tod_keys) {
      const auto &info = Info(k.key);
      Check(FindKey(k.name) == k.key && info.phase == Phase::kStartup &&
                CvarName(info) == k.cvar && CvarText(info, true) == k.text &&
                CvarText(info, false).empty() && !info.overrides_user,
            "l93 key writes its host int at startup, false writes nothing");
    }
    const auto paired = [](Key a, Key b) {
      for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
        if ((pair.first == a && pair.second == b) || (pair.first == b && pair.second == a)) {
          return true;
        }
      }
      return false;
    };
    Check(paired(Key::kTodRateX10, Key::kTodRateX30) &&
              paired(Key::kTodRateX10, Key::kTodFreeze) &&
              paired(Key::kTodRateX30, Key::kTodFreeze) &&
              paired(Key::kTodStart0430, Key::kTodStart0530),
          "the rate keys and the freeze key are a group of three, the start keys a pair");
    Check(!paired(Key::kTodRateX10, Key::kTodStart0530) &&
              !paired(Key::kTodFreeze, Key::kTodStart0430) &&
              !paired(Key::kTodRateX10, Key::kFrameLeadStrict),
          "a rate key and a start key combine");
    const Parsed dawn = Parse("tod_start_0530=true\r\ntod_rate_x10=true\r\n"
                              "frame_lead_strict=true\r\n");
    Check(dawn.ValueCount() == 3 && dawn.issue_count == 0 && dawn.ignored_lines == 0 &&
              dawn.Value(Key::kTodStart0530) == true && dawn.Value(Key::kTodRateX10) == true &&
              dawn.Value(Key::kFrameLeadStrict) == true,
          "the l93 dawn-strict arm parses");
    const Parsed rates = Parse("tod_rate_x10=true\ntod_rate_x30=true\ntod_freeze=false\n");
    Check(rates.ValueCount() == 1 && !rates.Value(Key::kTodRateX10) &&
              !rates.Value(Key::kTodRateX30) && rates.Value(Key::kTodFreeze) == false &&
              rates.issue_count == 1 && rates.issues[0].line == 2 &&
              rates.issues[0].issue == LineIssue::kConflict,
          "x10 and x30 both true: neither applies, conflict on the later line");
    const Parsed three = Parse("tod_freeze=true\ntod_rate_x10=true\ntod_rate_x30=true\n");
    Check(three.ValueCount() == 0 && three.issue_count == 2 &&
              three.issues[0].issue == LineIssue::kConflict &&
              three.issues[1].issue == LineIssue::kConflict,
          "freeze with both rates: the whole group is dropped");
    const Parsed starts = Parse("tod_start_0430=true\ntod_start_0530=true\n");
    Check(starts.ValueCount() == 0 && starts.issue_count == 1 && starts.issues[0].line == 2,
          "both start keys true: neither applies");
    const Parsed bad = Parse("tod_rate_x10=10\nmcla_tod_rate=true\ntod_rate=true\n"
                             "TOD_FREEZE=true\n");
    Check(bad.ValueCount() == 0 && bad.ignored_lines == 4 &&
              bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName &&
              bad.issues[2].issue == LineIssue::kUnknownName &&
              bad.issues[3].issue == LineIssue::kUnknownName,
          "l93 near misses: an integer, the cvar name, a prefix and the upper case are ignored");
  }
  {
    // l92 probe keys: six SDK cvars of the graphics plugin phase, each its own
    // cvar (no conflict pair); =false writes nothing.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    struct L92Key {
      Key key;
      const char *name;
      const char *cvar;
      const char *text;
    };
    const L92Key l92_keys[] = {
        {Key::kReadbackReadWatch, "readback_read_watch", "d3d12_readback_read_watch", "true"},
        {Key::kReadbackAgeInject2, "readback_age_inject_2", "d3d12_readback_age_inject", "2"},
        {Key::kOcclusionQueryBounded4, "occlusion_query_bounded_4",
         "d3d12_occlusion_query_bounded_ms", "4"},
        {Key::kPresentStats, "present_stats", "d3d12_present_stats", "true"},
        {Key::kPresentInterval1, "present_interval_1", "d3d12_present_sync_interval", "1"},
        {Key::kLockedTearingOff, "locked_tearing_off", "mcla_locked_tearing_off", "true"},
    };
    for (const L92Key &k : l92_keys) {
      const auto &info = Info(k.key);
      Check(FindKey(k.name) == k.key && info.phase == Phase::kGraphicsPlugin &&
                CvarName(info) == k.cvar && CvarText(info, true) == k.text &&
                CvarText(info, false).empty() && !info.overrides_user,
            "l92 key writes its SDK cvar in the graphics plugin phase, false writes nothing");
      for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
        Check(pair.first != k.key && pair.second != k.key, "the l92 keys are in no conflict pair");
      }
    }
    Check(!FindKey("d3d12_readback_read_watch") && !FindKey("readback_age_inject") &&
              !FindKey("readback_age_inject_1") && !FindKey("occlusion_query_bounded") &&
              !FindKey("present_interval") && !FindKey("PRESENT_STATS") &&
              !FindKey("locked_tearing") && !FindKey("readback_read_watch_off"),
          "no cvar, numeric, prefix, case or off variants of the l92 keys");
    const Parsed arm = Parse("readback_read_watch=true\r\nreadback_age_inject_2=true\r\n"
                             "frame_lead_strict=true\r\npresent_stats=true\r\n"
                             "present_interval_1=false\r\nlocked_tearing_off=false\r\n"
                             "occlusion_query_bounded_4=false\r\n");
    Check(arm.ValueCount() == 7 && arm.issue_count == 0 &&
              arm.Value(Key::kReadbackReadWatch) == true &&
              arm.Value(Key::kReadbackAgeInject2) == true &&
              arm.Value(Key::kFrameLeadStrict) == true && arm.Value(Key::kPresentStats) == true &&
              arm.Value(Key::kPresentInterval1) == false &&
              arm.Value(Key::kLockedTearingOff) == false &&
              arm.Value(Key::kOcclusionQueryBounded4) == false,
          "an l92 arm parses (strict lead with age inject, read watch, present stats)");
    const Parsed bad = Parse("readback_age_inject_2=2\n"
                             "d3d12_present_stats=true\n"
                             "present_stats=true\n"
                             "present_stats=false\n");
    Check(bad.ValueCount() == 1 && bad.ignored_lines == 3 &&
              bad.Value(Key::kPresentStats) == true &&
              bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName &&
              bad.issues[2].issue == LineIssue::kDuplicate,
          "l92 near misses: an integer, the cvar name and a repeat are ignored");
  }
  {
    // KI-026 keys: the stall fix (host init-only bool, default off; =false
    // writes nothing) and the KI-026 probe (plain host bool), both at startup;
    // the KI-026 watchdog stays command-line only.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    const auto &stall = Info(Key::kWarpStallFix);
    Check(FindKey("mcla_warp_stall_fix") == Key::kWarpStallFix &&
              stall.phase == Phase::kStartup && CvarName(stall) == "mcla_warp_stall_fix" &&
              CvarText(stall, true) == "true" && CvarText(stall, false).empty() &&
              !stall.overrides_user,
          "mcla_warp_stall_fix=true writes true at startup, false writes nothing");
    const auto &probe = Info(Key::kKi026Probe);
    Check(FindKey("mcla_ki026_probe") == Key::kKi026Probe && probe.phase == Phase::kStartup &&
              CvarName(probe) == "mcla_ki026_probe" && CvarText(probe, true) == "true" &&
              CvarText(probe, false) == "false",
          "mcla_ki026_probe is a plain host bool at startup");
    Check(!FindKey("mcla_ki026_watchdog") && !FindKey("mcla_warp_stall_fix_off") &&
              !FindKey("mcla_warp_stall") && !FindKey("warp_stall_fix") &&
              !FindKey("MCLA_WARP_STALL_FIX") && !FindKey("mcla_ki026") &&
              !FindKey("ki026_probe"),
          "no watchdog, off, prefix, case or short variants of the KI-026 keys");
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      Check(pair.first != Key::kWarpStallFix && pair.second != Key::kWarpStallFix &&
                pair.first != Key::kKi026Probe && pair.second != Key::kKi026Probe,
            "the KI-026 keys are in no conflict pair");
    }
    const Parsed arm = Parse("mcla_warp_stall_fix=true\r\nmcla_ki026_probe=true\r\n"
                             "mcla_warp_probe=true\r\nmcla_warp_watchdog=false\r\n");
    Check(arm.ValueCount() == 4 && arm.issue_count == 0 &&
              arm.Value(Key::kWarpStallFix) == true && arm.Value(Key::kKi026Probe) == true &&
              arm.Value(Key::kWarpProbe) == true && arm.Value(Key::kWarpWatchdog) == false,
          "the KI-026 fix arm parses");
    const Parsed control = Parse("mcla_warp_stall_fix=false\nmcla_ki026_probe=true\n");
    Check(control.ValueCount() == 2 && control.issue_count == 0 &&
              control.Value(Key::kWarpStallFix) == false,
          "the KI-026 control arm parses");
    const Parsed bad = Parse("mcla_warp_stall_fix=1\n"
                             "mcla_ki026_watchdog=true\n"
                             "mcla_ki026_probe=true\n"
                             "mcla_ki026_probe=false\n");
    Check(bad.ValueCount() == 1 && bad.ignored_lines == 3 &&
              bad.Value(Key::kKi026Probe) == true &&
              bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName &&
              bad.issues[2].issue == LineIssue::kDuplicate,
          "KI-026 near misses: an integer, the watchdog and a repeat are ignored");
  }
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
      if (pair.first == Key::kFenceWriteDeferred || pair.second == Key::kFenceWriteDeferred) {
        Check(pair.first == Key::kFenceWriteDeferred &&
                  pair.second == Key::kFenceWriteDeferredOff,
              "the fence key pairs only with its l78 control");
      }
      if (pair.first == Key::kXmaFrameWalkFixOff || pair.second == Key::kXmaFrameWalkFixOff) {
        Check(pair.first == Key::kXmaFrameWalkFix && pair.second == Key::kXmaFrameWalkFixOff,
              "the walk fix control pairs only with the walk fix key");
        ++walk_pairs;
      }
    }
    Check(walk_pairs == 1, "one walk fix conflict pair");
    Check(!FindKey("fence_write_deferred_on") && !FindKey("fence_deferred") &&
              !FindKey("fence_write") &&
              !FindKey("d3d12_fence_write_deferred") && !FindKey("FENCE_WRITE_DEFERRED") &&
              !FindKey("fence_writes_deferred") && !FindKey("xma_frame_walk_fix_of") &&
              !FindKey("xma_frame_walk_fix_off_on") && !FindKey("xma_walk_fix_off") &&
              !FindKey("XMA_FRAME_WALK_FIX_OFF") && !FindKey("xma_frame_walk_off"),
          "no prefix, suffix, cvar-name or case variants of the l77 keys");
  }
  {
    // l78 keys: the controls of the two SDK defaults turned on in l78 (steady
    // exemption, deferred fences) and the fence deferral gap, all graphics
    // plugin; =false writes nothing.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const auto &exempt_off = Info(Key::kReadbackSteadyExemptOff);
    Check(FindKey("readback_steady_exempt_off") == Key::kReadbackSteadyExemptOff &&
              exempt_off.phase == Phase::kGraphicsPlugin &&
              CvarName(exempt_off) == "d3d12_readback_sync_steady_exempt" &&
              CvarText(exempt_off, true) == "false" && CvarText(exempt_off, false).empty() &&
              exempt_off.true_source == TextSource::kLiteral && !exempt_off.overrides_user,
          "readback_steady_exempt_off=true writes false to d3d12_readback_sync_steady_exempt "
          "after the plugin loaded, false writes nothing");
    Check(Info(Key::kReadbackSteadyExempt).phase == exempt_off.phase &&
              CvarName(Info(Key::kReadbackSteadyExempt)) == CvarName(exempt_off),
          "the steady key and its control write one cvar in one phase");
    const auto &fence_off = Info(Key::kFenceWriteDeferredOff);
    Check(FindKey("fence_write_deferred_off") == Key::kFenceWriteDeferredOff &&
              fence_off.phase == Phase::kGraphicsPlugin &&
              CvarName(fence_off) == "d3d12_fence_write_deferred" &&
              CvarText(fence_off, true) == "false" && CvarText(fence_off, false).empty() &&
              fence_off.true_source == TextSource::kLiteral && !fence_off.overrides_user,
          "fence_write_deferred_off=true writes false to d3d12_fence_write_deferred after the "
          "plugin loaded, false writes nothing");
    Check(Info(Key::kFenceWriteDeferred).phase == fence_off.phase &&
              CvarName(Info(Key::kFenceWriteDeferred)) == CvarName(fence_off),
          "the fence key and its control write one cvar in one phase");
    const auto &gap = Info(Key::kFenceDeferGap2000);
    Check(FindKey("fence_defer_gap_2000") == Key::kFenceDeferGap2000 &&
              gap.phase == Phase::kGraphicsPlugin &&
              CvarName(gap) == "d3d12_fence_defer_min_gap_us" && CvarText(gap, true) == "2000" &&
              CvarText(gap, false).empty() && gap.true_source == TextSource::kLiteral &&
              !gap.overrides_user,
          "fence_defer_gap_2000=true writes 2000 to d3d12_fence_defer_min_gap_us after the "
          "plugin loaded, false writes nothing");
    size_t exempt_pairs = 0;
    size_t fence_pairs = 0;
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      Check(pair.first != Key::kFenceDeferGap2000 && pair.second != Key::kFenceDeferGap2000,
            "the fence gap key is in no conflict pair");
      if (pair.first == Key::kReadbackSteadyExemptOff ||
          pair.second == Key::kReadbackSteadyExemptOff) {
        Check(pair.first == Key::kReadbackSteadyExempt &&
                  pair.second == Key::kReadbackSteadyExemptOff,
              "the steady control pairs only with the steady key");
        ++exempt_pairs;
      }
      if (pair.first == Key::kFenceWriteDeferredOff || pair.second == Key::kFenceWriteDeferredOff) {
        Check(pair.first == Key::kFenceWriteDeferred && pair.second == Key::kFenceWriteDeferredOff,
              "the fence control pairs only with the fence key");
        ++fence_pairs;
      }
    }
    Check(exempt_pairs == 1 && fence_pairs == 1, "one conflict pair for each l78 control");
    Check(!FindKey("readback_steady_exempt_of") && !FindKey("readback_steady_exempt_off_on") &&
              !FindKey("READBACK_STEADY_EXEMPT_OFF") && !FindKey("steady_exempt_off") &&
              !FindKey("readback_steady_off") && !FindKey("fence_write_deferred_of") &&
              !FindKey("fence_write_deferred_off_on") && !FindKey("fence_deferred_off") &&
              !FindKey("FENCE_WRITE_DEFERRED_OFF") && !FindKey("fence_write_off") &&
              !FindKey("fence_defer_gap") && !FindKey("fence_defer_gap_1000") &&
              !FindKey("fence_defer_gap_0") && !FindKey("fence_defer_gap_2000_on") &&
              !FindKey("d3d12_fence_defer_min_gap_us") && !FindKey("fence_defer_min_gap_us") &&
              !FindKey("FENCE_DEFER_GAP_2000") && !FindKey("fence_gap_2000"),
          "no prefix, suffix, cvar-name, case or other numeric variants of the l78 keys");
  }
  {
    // l79 keys: two SDK rexruntime bools at startup (Canary XMA fixes, timer
    // queue blocking wait) and the two restart-only placed-heap cvars of the
    // graphics plugin; =false writes nothing; four cvars, no conflict pair.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const auto &canary = Info(Key::kXmaCanaryFixes);
    Check(FindKey("xma_canary_fixes") == Key::kXmaCanaryFixes &&
              canary.phase == Phase::kStartup && CvarName(canary) == "xma_canary_fixes" &&
              CvarText(canary, true) == "true" && CvarText(canary, false).empty() &&
              canary.true_source == TextSource::kLiteral && !canary.overrides_user,
          "xma_canary_fixes=true writes true to its own SDK cvar at startup, false writes "
          "nothing");
    const auto &timer = Info(Key::kTimerQueueBlockingWait);
    Check(FindKey("timer_queue_blocking_wait") == Key::kTimerQueueBlockingWait &&
              timer.phase == Phase::kStartup && CvarName(timer) == "timer_queue_blocking_wait" &&
              CvarText(timer, true) == "true" && CvarText(timer, false).empty() &&
              timer.true_source == TextSource::kLiteral && !timer.overrides_user,
          "timer_queue_blocking_wait=true writes true to its own SDK cvar at startup, false "
          "writes nothing");
    const auto &heaps = Info(Key::kTexturePlacedHeaps);
    Check(FindKey("texture_placed_heaps") == Key::kTexturePlacedHeaps &&
              heaps.phase == Phase::kGraphicsPlugin &&
              CvarName(heaps) == "d3d12_texture_placed_heaps" && CvarText(heaps, true) == "true" &&
              CvarText(heaps, false).empty() && heaps.true_source == TextSource::kLiteral &&
              !heaps.overrides_user,
          "texture_placed_heaps=true writes true to d3d12_texture_placed_heaps after the "
          "plugin loaded, false writes nothing");
    const auto &block = Info(Key::kTextureHeap64Mb);
    Check(FindKey("texture_heap_64mb") == Key::kTextureHeap64Mb &&
              block.phase == Phase::kGraphicsPlugin &&
              CvarName(block) == "d3d12_texture_heap_block_mb" && CvarText(block, true) == "64" &&
              CvarText(block, false).empty() && block.true_source == TextSource::kLiteral &&
              !block.overrides_user,
          "texture_heap_64mb=true writes 64 to d3d12_texture_heap_block_mb after the plugin "
          "loaded, false writes nothing");
    // Since l80 the timer and heaps keys pair with their l80 controls only.
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      for (const Key key : {Key::kXmaCanaryFixes, Key::kTextureHeap64Mb}) {
        Check(pair.first != key && pair.second != key,
              "the l79 XMA and heap-size keys are in no conflict pair");
      }
    }
    Check(!FindKey("xma_canary_fix") && !FindKey("xma_canary_fixes_on") &&
              !FindKey("XMA_CANARY_FIXES") && !FindKey("canary_fixes") &&
              !FindKey("xma_canary") && !FindKey("timer_queue_blocking") &&
              !FindKey("timer_queue_blocking_wait_on") && !FindKey("TIMER_QUEUE_BLOCKING_WAIT") &&
              !FindKey("timer_queue_spin_tail_us") && !FindKey("timer_blocking_wait") &&
              !FindKey("d3d12_texture_placed_heaps") && !FindKey("texture_placed_heap") &&
              !FindKey("texture_placed_heaps_on") && !FindKey("TEXTURE_PLACED_HEAPS") &&
              !FindKey("placed_heaps") && !FindKey("d3d12_texture_heap_block_mb") &&
              !FindKey("texture_heap_block_mb") && !FindKey("texture_heap_64") &&
              !FindKey("texture_heap_128mb") && !FindKey("texture_heap_256mb") &&
              !FindKey("texture_heap_64mb_on") && !FindKey("TEXTURE_HEAP_64MB"),
          "no prefix, suffix, cvar-name, case or other numeric variants of the l79 keys");
  }
  {
    // l80 keys: the KI-013 blend fix (graphics plugin bool), the two KI-060
    // controls (rexruntime strings at startup) and the controls for the l80
    // platform defaults (placed heaps after the plugin loaded, timer wait at
    // startup); =false writes nothing; the two controls pair with their l79
    // on keys.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const auto &blend = Info(Key::kBlendMinmaxFix);
    Check(FindKey("blend_minmax_fix") == Key::kBlendMinmaxFix &&
              blend.phase == Phase::kGraphicsPlugin &&
              CvarName(blend) == "d3d12_blend_minmax_fix" && CvarText(blend, true) == "true" &&
              CvarText(blend, false).empty() && blend.true_source == TextSource::kLiteral &&
              !blend.overrides_user,
          "blend_minmax_fix=true writes true to d3d12_blend_minmax_fix after the plugin "
          "loaded, false writes nothing");
    const auto &sleep = Info(Key::kShortSleepNative);
    Check(FindKey("short_sleep_native") == Key::kShortSleepNative &&
              sleep.phase == Phase::kStartup && CvarName(sleep) == "ke_delay_short_sleep_mode" &&
              CvarText(sleep, true) == "native" && CvarText(sleep, false).empty() &&
              sleep.true_source == TextSource::kLiteral && !sleep.overrides_user,
          "short_sleep_native=true writes native to ke_delay_short_sleep_mode at startup, "
          "false writes nothing");
    const auto &apu = Info(Key::kApuPriorityNormal);
    Check(FindKey("apu_priority_normal") == Key::kApuPriorityNormal &&
              apu.phase == Phase::kStartup && CvarName(apu) == "apu_worker_priority" &&
              CvarText(apu, true) == "normal" && CvarText(apu, false).empty() &&
              apu.true_source == TextSource::kLiteral && !apu.overrides_user,
          "apu_priority_normal=true writes normal to apu_worker_priority at startup, false "
          "writes nothing");
    const auto &heaps_off = Info(Key::kTexturePlacedHeapsOff);
    Check(FindKey("texture_placed_heaps_off") == Key::kTexturePlacedHeapsOff &&
              heaps_off.phase == Phase::kGraphicsPlugin &&
              CvarName(heaps_off) == "d3d12_texture_placed_heaps" &&
              CvarText(heaps_off, true) == "false" && CvarText(heaps_off, false).empty() &&
              heaps_off.true_source == TextSource::kLiteral && !heaps_off.overrides_user,
          "texture_placed_heaps_off=true writes false to d3d12_texture_placed_heaps after the "
          "plugin loaded, false writes nothing");
    const auto &timer_off = Info(Key::kTimerBlockingOff);
    Check(FindKey("timer_blocking_off") == Key::kTimerBlockingOff &&
              timer_off.phase == Phase::kStartup &&
              CvarName(timer_off) == "timer_queue_blocking_wait" &&
              CvarText(timer_off, true) == "false" && CvarText(timer_off, false).empty() &&
              timer_off.true_source == TextSource::kLiteral && !timer_off.overrides_user,
          "timer_blocking_off=true writes false to timer_queue_blocking_wait at startup, false "
          "writes nothing");
    Check(Info(Key::kTexturePlacedHeaps).phase == heaps_off.phase &&
              CvarName(Info(Key::kTexturePlacedHeaps)) == CvarName(heaps_off) &&
              Info(Key::kTimerQueueBlockingWait).phase == timer_off.phase &&
              CvarName(Info(Key::kTimerQueueBlockingWait)) == CvarName(timer_off),
          "each l80 control and its l79 on key write one cvar in one phase");
    size_t heap_pairs = 0;
    size_t timer_pairs = 0;
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      // Since l81 the blend key pairs with its control blend_minmax_fix_off.
      for (const Key key : {Key::kShortSleepNative, Key::kApuPriorityNormal}) {
        Check(pair.first != key && pair.second != key,
              "the KI-060 controls are in no conflict pair");
      }
      if (pair.first == Key::kTexturePlacedHeapsOff || pair.second == Key::kTexturePlacedHeapsOff ||
          pair.first == Key::kTexturePlacedHeaps || pair.second == Key::kTexturePlacedHeaps) {
        Check(pair.first == Key::kTexturePlacedHeaps && pair.second == Key::kTexturePlacedHeapsOff,
              "the heaps control pairs only with the heaps key");
        ++heap_pairs;
      }
      if (pair.first == Key::kTimerBlockingOff || pair.second == Key::kTimerBlockingOff ||
          pair.first == Key::kTimerQueueBlockingWait ||
          pair.second == Key::kTimerQueueBlockingWait) {
        Check(pair.first == Key::kTimerQueueBlockingWait && pair.second == Key::kTimerBlockingOff,
              "the timer control pairs only with the timer key");
        ++timer_pairs;
      }
    }
    Check(heap_pairs == 1 && timer_pairs == 1, "one conflict pair for each l80 control");
    Check(!FindKey("d3d12_blend_minmax_fix") && !FindKey("blend_minmax_fix_on") &&
              !FindKey("BLEND_MINMAX_FIX") && !FindKey("blend_min_max_fix") &&
              !FindKey("blend_minmax") && !FindKey("blend_fix") &&
              !FindKey("ke_delay_short_sleep_mode") && !FindKey("short_sleep_hires_timer") &&
              !FindKey("short_sleep_yield") && !FindKey("short_sleep") &&
              !FindKey("SHORT_SLEEP_NATIVE") && !FindKey("short_sleep_native_on") &&
              !FindKey("apu_worker_priority") && !FindKey("apu_priority_above_normal") &&
              !FindKey("apu_priority_highest") && !FindKey("apu_priority") &&
              !FindKey("APU_PRIORITY_NORMAL") && !FindKey("texture_placed_heaps_of") &&
              !FindKey("texture_placed_heaps_off_on") && !FindKey("TEXTURE_PLACED_HEAPS_OFF") &&
              !FindKey("placed_heaps_off") && !FindKey("timer_blocking") &&
              !FindKey("timer_blocking_of") && !FindKey("timer_queue_blocking_wait_off") &&
              !FindKey("TIMER_BLOCKING_OFF") && !FindKey("timer_blocking_off_on") &&
              !FindKey("d3d12_texture_heap_precreate") && !FindKey("texture_heap_precreate"),
          "no prefix, suffix, cvar-name, case or other value variants of the l80 keys");
    // Both members of a control pair true: neither applies, the later line is
    // logged; with the on key false the control applies.
    const Parsed both = Parse("timer_queue_blocking_wait=true\n"
                              "texture_placed_heaps_off=true\n"
                              "timer_blocking_off=true\n"
                              "texture_placed_heaps=true\n");
    Check(both.ValueCount() == 0 && both.issue_count == 2 && both.issues[0].line == 3 &&
              both.issues[0].issue == LineIssue::kConflict && both.issues[1].line == 4 &&
              both.issues[1].issue == LineIssue::kConflict,
          "l80 controls with their l79 on keys: neither applied, conflicts on the later lines");
    const Parsed controls = Parse("texture_placed_heaps=false\n"
                                  "texture_placed_heaps_off=true\n"
                                  "timer_queue_blocking_wait=false\n"
                                  "timer_blocking_off=true\n"
                                  "short_sleep_native=true\n"
                                  "apu_priority_normal=true\n"
                                  "blend_minmax_fix=true\n");
    Check(controls.ValueCount() == 7 && controls.issue_count == 0 &&
              controls.Value(Key::kTexturePlacedHeapsOff) == true &&
              controls.Value(Key::kTimerBlockingOff) == true &&
              controls.Value(Key::kShortSleepNative) == true &&
              controls.Value(Key::kApuPriorityNormal) == true &&
              controls.Value(Key::kBlendMinmaxFix) == true &&
              controls.Value(Key::kTexturePlacedHeaps) == false &&
              controls.Value(Key::kTimerQueueBlockingWait) == false,
          "l80 controls with the on keys false, and all five l80 keys true: no conflict");
  }
  {
    // l81 keys: the two KI-060 controls (rexruntime strings at startup), the
    // KI-013 blend fix control (graphics plugin) and the KI-056 fix A control
    // (host init-only, startup); =false writes nothing; the two default-on
    // controls pair with their on keys.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const auto &zero = Info(Key::kZeroDelayNative);
    Check(FindKey("zero_delay_native") == Key::kZeroDelayNative &&
              zero.phase == Phase::kStartup && CvarName(zero) == "ke_delay_zero_mode" &&
              CvarText(zero, true) == "native" && CvarText(zero, false).empty() &&
              zero.true_source == TextSource::kLiteral && !zero.overrides_user,
          "zero_delay_native=true writes native to ke_delay_zero_mode at startup, false "
          "writes nothing");
    const auto &prio = Info(Key::kGuestAudioPrioNormal);
    Check(FindKey("guest_audio_prio_normal") == Key::kGuestAudioPrioNormal &&
              prio.phase == Phase::kStartup && CvarName(prio) == "guest_audio_thread_priority" &&
              CvarText(prio, true) == "normal" && CvarText(prio, false).empty() &&
              prio.true_source == TextSource::kLiteral && !prio.overrides_user,
          "guest_audio_prio_normal=true writes normal to guest_audio_thread_priority at "
          "startup, false writes nothing");
    const auto &blend_off = Info(Key::kBlendMinmaxFixOff);
    Check(FindKey("blend_minmax_fix_off") == Key::kBlendMinmaxFixOff &&
              blend_off.phase == Phase::kGraphicsPlugin &&
              CvarName(blend_off) == "d3d12_blend_minmax_fix" &&
              CvarText(blend_off, true) == "false" && CvarText(blend_off, false).empty() &&
              blend_off.true_source == TextSource::kLiteral && !blend_off.overrides_user,
          "blend_minmax_fix_off=true writes false to d3d12_blend_minmax_fix after the plugin "
          "loaded, false writes nothing");
    const auto &album_off = Info(Key::kAlbumPreviewRestoreOff);
    Check(FindKey("album_preview_restore_off") == Key::kAlbumPreviewRestoreOff &&
              album_off.phase == Phase::kStartup &&
              CvarName(album_off) == "mcla_album_preview_restore" &&
              CvarText(album_off, true) == "false" && CvarText(album_off, false).empty() &&
              album_off.true_source == TextSource::kLiteral && !album_off.overrides_user,
          "album_preview_restore_off=true writes false to mcla_album_preview_restore at "
          "startup, false writes nothing");
    const auto &album = Info(Key::kAlbumPreviewRestore);
    Check(CvarName(album) == "mcla_album_preview_restore" && CvarText(album, true) == "true" &&
              CvarText(album, false).empty(),
          "mcla_album_preview_restore=true writes the l81 default, false writes nothing");
    Check(Info(Key::kBlendMinmaxFix).phase == blend_off.phase &&
              CvarName(Info(Key::kBlendMinmaxFix)) == CvarName(blend_off) &&
              album.phase == album_off.phase && CvarName(album) == CvarName(album_off),
          "each l81 control and its on key write one cvar in one phase");
    size_t blend_pairs = 0;
    size_t album_pairs = 0;
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      for (const Key key : {Key::kZeroDelayNative, Key::kGuestAudioPrioNormal}) {
        Check(pair.first != key && pair.second != key,
              "the l81 KI-060 controls are in no conflict pair");
      }
      if (pair.first == Key::kBlendMinmaxFixOff || pair.second == Key::kBlendMinmaxFixOff ||
          pair.first == Key::kBlendMinmaxFix || pair.second == Key::kBlendMinmaxFix) {
        Check(pair.first == Key::kBlendMinmaxFix && pair.second == Key::kBlendMinmaxFixOff,
              "the blend control pairs only with the blend key");
        ++blend_pairs;
      }
      if (pair.first == Key::kAlbumPreviewRestoreOff ||
          pair.second == Key::kAlbumPreviewRestoreOff ||
          pair.first == Key::kAlbumPreviewRestore || pair.second == Key::kAlbumPreviewRestore) {
        Check(pair.first == Key::kAlbumPreviewRestore &&
                  pair.second == Key::kAlbumPreviewRestoreOff,
              "the album control pairs only with the restore key");
        ++album_pairs;
      }
    }
    Check(blend_pairs == 1 && album_pairs == 1, "one conflict pair for each l81 control");
    Check(!FindKey("ke_delay_zero_mode") && !FindKey("zero_delay_backoff") &&
              !FindKey("zero_delay") && !FindKey("ZERO_DELAY_NATIVE") &&
              !FindKey("zero_delay_native_on") && !FindKey("zero_delay_native_off") &&
              !FindKey("ke_delay_zero_backoff_after") && !FindKey("ke_delay_zero_backoff_max_us") &&
              !FindKey("ke_delay_zero_gap_us") && !FindKey("guest_audio_thread_priority") &&
              !FindKey("guest_audio_prio") && !FindKey("guest_audio_prio_above_normal") &&
              !FindKey("GUEST_AUDIO_PRIO_NORMAL") && !FindKey("guest_audio_prio_normal_on") &&
              !FindKey("guest_thread_priority_boost_start") &&
              !FindKey("blend_minmax_fix_of") && !FindKey("blend_minmax_fix_off_on") &&
              !FindKey("BLEND_MINMAX_FIX_OFF") && !FindKey("d3d12_blend_minmax_fix_off") &&
              !FindKey("album_preview_restore") && !FindKey("album_preview_restore_of") &&
              !FindKey("mcla_album_preview_restore_off") &&
              !FindKey("ALBUM_PREVIEW_RESTORE_OFF") && !FindKey("album_preview_restore_off_on"),
          "no prefix, suffix, cvar-name, case or other value variants of the l81 keys");
    // Both members of a control pair true: neither applies, the later line is
    // logged; with the on key false the control applies.
    const Parsed both = Parse("blend_minmax_fix=true\n"
                              "album_preview_restore_off=true\n"
                              "blend_minmax_fix_off=true\n"
                              "mcla_album_preview_restore=true\n");
    Check(both.ValueCount() == 0 && both.issue_count == 2 && both.issues[0].line == 3 &&
              both.issues[0].issue == LineIssue::kConflict && both.issues[1].line == 4 &&
              both.issues[1].issue == LineIssue::kConflict,
          "l81 controls with their on keys: neither applied, conflicts on the later lines");
    const Parsed controls = Parse("blend_minmax_fix=false\n"
                                  "blend_minmax_fix_off=true\n"
                                  "mcla_album_preview_restore=false\n"
                                  "album_preview_restore_off=true\n"
                                  "zero_delay_native=true\n"
                                  "guest_audio_prio_normal=true\n");
    Check(controls.ValueCount() == 6 && controls.issue_count == 0 &&
              controls.Value(Key::kBlendMinmaxFixOff) == true &&
              controls.Value(Key::kAlbumPreviewRestoreOff) == true &&
              controls.Value(Key::kZeroDelayNative) == true &&
              controls.Value(Key::kGuestAudioPrioNormal) == true &&
              controls.Value(Key::kBlendMinmaxFix) == false &&
              controls.Value(Key::kAlbumPreviewRestore) == false,
          "l81 controls with the on keys false: no conflict");
    const Parsed bad = Parse("zero_delay_native=native\n"
                             "ke_delay_zero_mode=true\n"
                             "guest_audio_prio_normal=1\n"
                             "blend_minmax_fix_off=true\n"
                             "blend_minmax_fix_off=false\n"
                             "album_preview_restore_off=true\n");
    Check(bad.ValueCount() == 2 && bad.Value(Key::kBlendMinmaxFixOff) == true &&
              bad.Value(Key::kAlbumPreviewRestoreOff) == true &&
              !bad.Value(Key::kZeroDelayNative) && !bad.Value(Key::kGuestAudioPrioNormal) &&
              bad.ignored_lines == 4 && bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName &&
              bad.issues[2].issue == LineIssue::kMalformed &&
              bad.issues[3].issue == LineIssue::kDuplicate,
          "l81 near misses: string values, the cvar name, integers and repeats are ignored");
  }
  {
    // l82 key: CP bulk register writes (graphics plugin bool, default false
    // in the SDK); =false writes nothing; no conflict pair.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const auto &bulk = Info(Key::kCpBulkRegs);
    Check(FindKey("cp_bulk_regs") == Key::kCpBulkRegs && bulk.phase == Phase::kGraphicsPlugin &&
              CvarName(bulk) == "cp_bulk_register_writes" && CvarText(bulk, true) == "true" &&
              CvarText(bulk, false).empty() && bulk.true_source == TextSource::kLiteral &&
              !bulk.overrides_user,
          "cp_bulk_regs=true writes true to cp_bulk_register_writes after the plugin loaded, "
          "false writes nothing");
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      Check(pair.first != Key::kCpBulkRegs && pair.second != Key::kCpBulkRegs,
            "the l82 key is in no conflict pair");
    }
    Check(!FindKey("cp_bulk_register_writes") && !FindKey("cp_bulk_regs_on") &&
              !FindKey("cp_bulk_regs_off") && !FindKey("CP_BULK_REGS") && !FindKey("bulk_regs") &&
              !FindKey("cp_bulk") && !FindKey("cp_bulk_reg"),
          "no prefix, suffix, cvar-name or case variants of the l82 key");
    const Parsed on = Parse("cp_bulk_regs=true\n");
    Check(on.ValueCount() == 1 && on.issue_count == 0 && on.Value(Key::kCpBulkRegs) == true,
          "cp_bulk_regs=true parses");
    const Parsed off = Parse("cp_bulk_regs=false\n");
    Check(off.ValueCount() == 1 && off.issue_count == 0 && off.Value(Key::kCpBulkRegs) == false,
          "cp_bulk_regs=false parses (writes nothing)");
    const Parsed bad = Parse("cp_bulk_regs=1\n"
                             "cp_bulk_register_writes=true\n");
    Check(bad.ValueCount() == 0 && bad.ignored_lines == 2 &&
              bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName,
          "l82 near misses: an integer value and the cvar name are ignored");
  }
  {
    // l83 KI-054 keys: host init-only bools at startup; =true writes true to
    // the mcla_ cvar, =false writes nothing; no conflict pair except the hitch
    // hold, default on since l86 and paired with its control (l86 block).
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const Key keys[] = {Key::kAmbientDtProbe, Key::kAmbientDtSnapshot, Key::kRawTimerHitchHold};
    const char *const names[] = {"ambient_dt_probe", "ambient_dt_snapshot",
                                 "raw_timer_hitch_hold"};
    const char *const cvars[] = {"mcla_ambient_dt_probe", "mcla_ambient_dt_snapshot",
                                 "mcla_raw_timer_hitch_hold"};
    for (size_t k = 0; k < 3; ++k) {
      const auto &info = Info(keys[k]);
      Check(FindKey(names[k]) == keys[k] && info.phase == Phase::kStartup &&
                CvarName(info) == cvars[k] && CvarText(info, true) == "true" &&
                CvarText(info, false).empty() && info.true_source == TextSource::kLiteral &&
                !info.overrides_user,
            "l83 key: =true writes true to its mcla_ cvar at startup, =false writes nothing");
      Check(!FindKey(cvars[k]), "the l83 cvar name is not a key");
      if (keys[k] == Key::kRawTimerHitchHold) {
        continue;
      }
      for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
        Check(pair.first != keys[k] && pair.second != keys[k], "the l83 probes are in no pair");
      }
    }
    Check(!FindKey("ambient_dt") && !FindKey("ambient_dt_probe_on") &&
              !FindKey("ambient_dt_probe_off") && !FindKey("AMBIENT_DT_PROBE") &&
              !FindKey("ambient_dt_snapshot_off") && !FindKey("ambient_snapshot") &&
              !FindKey("raw_timer_hitch") && !FindKey("hitch_hold") &&
              !FindKey("camera_ct_filters"),
          "no prefix, suffix or case variants of the l83 keys; no camera key");
    const Parsed on = Parse("ambient_dt_probe=true\n"
                            "ambient_dt_snapshot=true\n"
                            "raw_timer_hitch_hold=true\n");
    Check(on.ValueCount() == 3 && on.issue_count == 0 && on.Value(Key::kAmbientDtProbe) == true &&
              on.Value(Key::kAmbientDtSnapshot) == true &&
              on.Value(Key::kRawTimerHitchHold) == true,
          "the l83 keys parse together");
    const Parsed off = Parse("ambient_dt_probe=false\nraw_timer_hitch_hold=false\n");
    Check(off.ValueCount() == 2 && off.issue_count == 0 &&
              off.Value(Key::kAmbientDtProbe) == false &&
              off.Value(Key::kRawTimerHitchHold) == false && !off.Value(Key::kAmbientDtSnapshot),
          "=false parses (writes nothing)");
    const Parsed bad = Parse("ambient_dt_probe=1\n"
                             "mcla_ambient_dt_snapshot=true\n"
                             "raw_timer_hitch_hold=true\n"
                             "raw_timer_hitch_hold=false\n");
    Check(bad.ValueCount() == 1 && bad.ignored_lines == 3 &&
              bad.Value(Key::kRawTimerHitchHold) == true &&
              bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName &&
              bad.issues[2].issue == LineIssue::kDuplicate,
          "l83 near misses: an integer, the cvar name and a repeat are ignored");
  }
  {
    // l84 KI-054 key: the raw timer statistics line, a host init-only bool at
    // startup (default off); =true writes true, =false writes nothing.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const auto &info = Info(Key::kRawTimerStats);
    Check(FindKey("raw_timer_stats") == Key::kRawTimerStats && info.phase == Phase::kStartup &&
              CvarName(info) == "mcla_raw_timer_stats" && CvarText(info, true) == "true" &&
              CvarText(info, false).empty() && info.true_source == TextSource::kLiteral &&
              !info.overrides_user,
          "l84 key: =true writes true to mcla_raw_timer_stats at startup, =false nothing");
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      Check(pair.first != Key::kRawTimerStats && pair.second != Key::kRawTimerStats,
            "the l84 key is in no pair");
    }
    Check(!FindKey("mcla_raw_timer_stats") && !FindKey("raw_timer_stat") &&
              !FindKey("timer_stats") && !FindKey("raw_timer_stats_off") &&
              !FindKey("RAW_TIMER_STATS"),
          "no prefix, suffix or case variants of the l84 key");
    const Parsed on = Parse("ambient_dt_probe=true\nraw_timer_stats=true\n");
    Check(on.ValueCount() == 2 && on.issue_count == 0 && on.Value(Key::kRawTimerStats) == true &&
              on.Value(Key::kAmbientDtProbe) == true,
          "the l84 ambprobe arm keys parse together");
    const Parsed bad = Parse("raw_timer_stats=1\nmcla_raw_timer_stats=true\n"
                             "raw_timer_stats=false\n");
    Check(bad.ValueCount() == 1 && bad.ignored_lines == 2 &&
              bad.Value(Key::kRawTimerStats) == false &&
              bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName,
          "l84 near misses: an integer and the cvar name are ignored");
  }
  {
    // l86 KI-054 control: the hitch hold is default on (owner decision
    // 2026-11-10); raw_timer_hitch_hold_off=true writes false to
    // mcla_raw_timer_hitch_hold at startup, =false writes nothing; a conflict
    // pair with raw_timer_hitch_hold, which writes the default (true).
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const auto &off = Info(Key::kRawTimerHitchHoldOff);
    const auto &hold = Info(Key::kRawTimerHitchHold);
    Check(FindKey("raw_timer_hitch_hold_off") == Key::kRawTimerHitchHoldOff &&
              off.phase == Phase::kStartup && CvarName(off) == "mcla_raw_timer_hitch_hold" &&
              CvarText(off, true) == "false" && CvarText(off, false).empty() &&
              off.true_source == TextSource::kLiteral && !off.overrides_user,
          "raw_timer_hitch_hold_off=true writes false to mcla_raw_timer_hitch_hold at startup, "
          "false writes nothing");
    Check(CvarName(hold) == CvarName(off) && hold.phase == off.phase &&
              CvarText(hold, true) == "true" && CvarText(hold, false).empty(),
          "raw_timer_hitch_hold=true writes the l86 default, false writes nothing");
    size_t hold_pairs = 0;
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      if (pair.first == Key::kRawTimerHitchHoldOff || pair.second == Key::kRawTimerHitchHoldOff ||
          pair.first == Key::kRawTimerHitchHold || pair.second == Key::kRawTimerHitchHold) {
        Check(pair.first == Key::kRawTimerHitchHold &&
                  pair.second == Key::kRawTimerHitchHoldOff,
              "the hitch hold control pairs only with the hitch hold key");
        ++hold_pairs;
      }
    }
    Check(hold_pairs == 1, "one conflict pair for the l86 control");
    Check(!FindKey("raw_timer_hitch_hold_of") && !FindKey("raw_timer_hitch_hold_off_on") &&
              !FindKey("RAW_TIMER_HITCH_HOLD_OFF") && !FindKey("mcla_raw_timer_hitch_hold_off") &&
              !FindKey("hitch_hold_off"),
          "no prefix, suffix, cvar-name or case variants of the l86 key");
    const Parsed both = Parse("raw_timer_stats=true\n"
                              "raw_timer_hitch_hold=true\n"
                              "raw_timer_hitch_hold_off=true\n");
    Check(both.ValueCount() == 1 && both.Value(Key::kRawTimerStats) == true &&
              both.issue_count == 1 && both.issues[0].line == 3 &&
              both.issues[0].issue == LineIssue::kConflict,
          "the l86 control with its on key: neither applied, conflict on the later line");
    const Parsed control = Parse("raw_timer_hitch_hold_off=true\n"
                                 "raw_timer_stats=true\n"
                                 "raw_timer_hitch_hold=false\n");
    Check(control.ValueCount() == 3 && control.issue_count == 0 &&
              control.Value(Key::kRawTimerHitchHoldOff) == true &&
              control.Value(Key::kRawTimerHitchHold) == false &&
              control.Value(Key::kRawTimerStats) == true,
          "the l86 hold-off arm keys parse together; with the on key false no conflict");
    const Parsed bad = Parse("raw_timer_hitch_hold_off=1\n"
                             "mcla_raw_timer_hitch_hold=false\n"
                             "raw_timer_hitch_hold_off=true\n"
                             "raw_timer_hitch_hold_off=false\n");
    Check(bad.ValueCount() == 1 && bad.ignored_lines == 3 &&
              bad.Value(Key::kRawTimerHitchHoldOff) == true &&
              bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName &&
              bad.issues[2].issue == LineIssue::kDuplicate,
          "l86 near misses: an integer, the cvar name and a repeat are ignored");
  }
  {
    // l87 KI-060 keys: host init-only bools at startup (default off); =true
    // writes true to the mcla_ cvar, =false writes nothing; no conflict pair.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const Key keys[] = {Key::kAudioFilterProbe, Key::kAudioFilterNative,
                        Key::kAudioFilterNativeVerify};
    const char *const names[] = {"audio_filter_probe", "audio_filter_native",
                                 "audio_filter_native_verify"};
    const char *const cvars[] = {"mcla_audio_filter_probe", "mcla_audio_filter_native",
                                 "mcla_audio_filter_native_verify"};
    for (size_t k = 0; k < 3; ++k) {
      const auto &info = Info(keys[k]);
      Check(FindKey(names[k]) == keys[k] && info.phase == Phase::kStartup &&
                CvarName(info) == cvars[k] && CvarText(info, true) == "true" &&
                CvarText(info, false).empty() && info.true_source == TextSource::kLiteral &&
                !info.overrides_user,
            "l87 key: =true writes true to its mcla_ cvar at startup, =false writes nothing");
      Check(!FindKey(cvars[k]), "the l87 cvar name is not a key");
      for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
        Check(pair.first != keys[k] && pair.second != keys[k], "the l87 keys are in no pair");
      }
    }
    Check(!FindKey("audio_filter") && !FindKey("audio_filter_probe_on") &&
              !FindKey("audio_filter_native_off") && !FindKey("AUDIO_FILTER_PROBE") &&
              !FindKey("audio_filter_verify") && !FindKey("audio_filter_native_verif"),
          "no prefix, suffix or case variants of the l87 keys");
    const Parsed on = Parse("audio_filter_probe=true\n"
                            "audio_filter_native=true\n"
                            "audio_filter_native_verify=true\n");
    Check(on.ValueCount() == 3 && on.issue_count == 0 &&
              on.Value(Key::kAudioFilterProbe) == true &&
              on.Value(Key::kAudioFilterNative) == true &&
              on.Value(Key::kAudioFilterNativeVerify) == true,
          "the l87 afnative arm keys parse together");
    const Parsed bad = Parse("audio_filter_native=1\n"
                             "mcla_audio_filter_probe=true\n"
                             "audio_filter_probe=true\n"
                             "audio_filter_probe=false\n");
    Check(bad.ValueCount() == 1 && bad.ignored_lines == 3 &&
              bad.Value(Key::kAudioFilterProbe) == true &&
              bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName &&
              bad.issues[2].issue == LineIssue::kDuplicate,
          "l87 near misses: an integer, the cvar name and a repeat are ignored");
  }
  {
    // l88 KI-074 key: fenced read-back delivery (graphics plugin bool,
    // default false in the SDK); =false writes nothing; no conflict pair.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const auto &fenced = Info(Key::kReadbackFencedDelivery);
    Check(FindKey("readback_fenced_delivery") == Key::kReadbackFencedDelivery &&
              fenced.phase == Phase::kGraphicsPlugin &&
              CvarName(fenced) == "d3d12_readback_fenced_delivery" &&
              CvarText(fenced, true) == "true" && CvarText(fenced, false).empty() &&
              fenced.true_source == TextSource::kLiteral && !fenced.overrides_user,
          "readback_fenced_delivery=true writes true to d3d12_readback_fenced_delivery after "
          "the plugin loaded, false writes nothing");
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      Check(pair.first != Key::kReadbackFencedDelivery &&
                pair.second != Key::kReadbackFencedDelivery,
            "the l88 key is in no conflict pair");
    }
    Check(!FindKey("d3d12_readback_fenced_delivery") && !FindKey("readback_fenced") &&
              !FindKey("readback_fenced_delivery_on") && !FindKey("READBACK_FENCED_DELIVERY") &&
              !FindKey("fenced_delivery") && !FindKey("readback_fenced_deliver"),
          "no prefix, suffix, cvar-name or case variants of the l88 key");
    const Parsed on = Parse("readback_fenced_delivery=true\n");
    Check(on.ValueCount() == 1 && on.issue_count == 0 &&
              on.Value(Key::kReadbackFencedDelivery) == true,
          "readback_fenced_delivery=true parses");
    const Parsed bad = Parse("readback_fenced_delivery=1\n"
                             "d3d12_readback_fenced_delivery=true\n"
                             "readback_fenced_delivery=false\n"
                             "readback_fenced_delivery=true\n");
    Check(bad.ValueCount() == 1 && bad.ignored_lines == 3 &&
              bad.Value(Key::kReadbackFencedDelivery) == false &&
              bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName &&
              bad.issues[2].issue == LineIssue::kDuplicate,
          "l88 near misses: an integer, the cvar name and a repeat are ignored");
  }
  {
    // l89 KI-060 control key: the per-channel filter stays recompiled (host
    // init-only bool at startup, default off); =false writes nothing; no
    // conflict pair.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const auto &ch_off = Info(Key::kAudioFilterNativeChOff);
    Check(FindKey("audio_filter_native_ch_off") == Key::kAudioFilterNativeChOff &&
              ch_off.phase == Phase::kStartup &&
              CvarName(ch_off) == "mcla_audio_filter_native_ch_off" &&
              CvarText(ch_off, true) == "true" && CvarText(ch_off, false).empty() &&
              ch_off.true_source == TextSource::kLiteral && !ch_off.overrides_user,
          "audio_filter_native_ch_off=true writes true to mcla_audio_filter_native_ch_off at "
          "startup, false writes nothing");
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      Check(pair.first != Key::kAudioFilterNativeChOff &&
                pair.second != Key::kAudioFilterNativeChOff,
            "the l89 key is in no conflict pair");
    }
    Check(!FindKey("mcla_audio_filter_native_ch_off") && !FindKey("audio_filter_ch_off") &&
              !FindKey("audio_filter_native_ch") && !FindKey("AUDIO_FILTER_NATIVE_CH_OFF") &&
              !FindKey("audio_filter_native_ch_off_on") && !FindKey("audio_filter_native_ch_of"),
          "no prefix, suffix, cvar-name or case variants of the l89 key");
    const Parsed on = Parse("audio_filter_probe=true\n"
                            "audio_filter_native=true\n"
                            "audio_filter_native_verify=true\n"
                            "audio_filter_native_ch_off=true\n");
    Check(on.ValueCount() == 4 && on.issue_count == 0 &&
              on.Value(Key::kAudioFilterNativeChOff) == true &&
              on.Value(Key::kAudioFilterNative) == true,
          "the l89 control arm keys parse together");
    const Parsed bad = Parse("audio_filter_native_ch_off=1\n"
                             "mcla_audio_filter_native_ch_off=true\n"
                             "audio_filter_native_ch_off=false\n"
                             "audio_filter_native_ch_off=true\n");
    Check(bad.ValueCount() == 1 && bad.ignored_lines == 3 &&
              bad.Value(Key::kAudioFilterNativeChOff) == false &&
              bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName &&
              bad.issues[2].issue == LineIssue::kDuplicate,
          "l89 near misses: an integer, the cvar name and a repeat are ignored");
  }
  {
    // l90 KI-074 keys: the frame lead limit (graphics plugin int, SDK default
    // 0) and the deferred read-back slot release (graphics plugin bool, SDK
    // default false); =false writes nothing; no conflict pairs.
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const auto &lead = Info(Key::kFrameLeadLimit1);
    Check(FindKey("frame_lead_limit_1") == Key::kFrameLeadLimit1 &&
              lead.phase == Phase::kGraphicsPlugin &&
              CvarName(lead) == "d3d12_frame_lead_limit" && CvarText(lead, true) == "1" &&
              CvarText(lead, false).empty() && lead.true_source == TextSource::kLiteral &&
              !lead.overrides_user,
          "frame_lead_limit_1=true writes 1 to d3d12_frame_lead_limit after the plugin "
          "loaded, false writes nothing");
    const auto &rel = Info(Key::kReadbackDeferredRelease);
    Check(FindKey("readback_deferred_release") == Key::kReadbackDeferredRelease &&
              rel.phase == Phase::kGraphicsPlugin &&
              CvarName(rel) == "d3d12_readback_deferred_release" &&
              CvarText(rel, true) == "true" && CvarText(rel, false).empty() &&
              rel.true_source == TextSource::kLiteral && !rel.overrides_user,
          "readback_deferred_release=true writes true to d3d12_readback_deferred_release "
          "after the plugin loaded, false writes nothing");
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      Check(pair.first != Key::kFrameLeadLimit1 && pair.second != Key::kFrameLeadLimit1 &&
                pair.first != Key::kReadbackDeferredRelease &&
                pair.second != Key::kReadbackDeferredRelease,
            "the l90 keys are in no conflict pair");
    }
    Check(!FindKey("d3d12_frame_lead_limit") && !FindKey("frame_lead_limit") &&
              !FindKey("frame_lead_limit_2") && !FindKey("frame_lead_limit_0") &&
              !FindKey("FRAME_LEAD_LIMIT_1") && !FindKey("frame_lead_limit_1_on") &&
              !FindKey("lead_limit_1") && !FindKey("d3d12_readback_deferred_release") &&
              !FindKey("readback_deferred") && !FindKey("READBACK_DEFERRED_RELEASE") &&
              !FindKey("readback_deferred_release_on") && !FindKey("deferred_release"),
          "no prefix, suffix, cvar-name, case or other numeric variants of the l90 keys");
    const Parsed on = Parse("frame_lead_limit_1=true\n"
                            "readback_deferred_release=true\n");
    Check(on.ValueCount() == 2 && on.issue_count == 0 &&
              on.Value(Key::kFrameLeadLimit1) == true &&
              on.Value(Key::kReadbackDeferredRelease) == true,
          "the l90 arm keys parse together");
    const Parsed bad = Parse("frame_lead_limit_1=1\n"
                             "d3d12_frame_lead_limit=true\n"
                             "frame_lead_limit_1=false\n"
                             "frame_lead_limit_1=true\n"
                             "readback_deferred_release=yes\n");
    Check(bad.ValueCount() == 1 && bad.ignored_lines == 4 &&
              bad.Value(Key::kFrameLeadLimit1) == false &&
              bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName &&
              bad.issues[2].issue == LineIssue::kDuplicate &&
              bad.issues[3].issue == LineIssue::kMalformed,
          "l90 near misses: an integer, the cvar name, a repeat and a non-bool are ignored");
  }
  {
    // l91 KI-074 keys: the strict (1) and adaptive (2) frame lead bound, one
    // graphics plugin int (SDK default 0); =false writes nothing; the two keys
    // form one conflict pair, none with frame_lead_limit_1 (another cvar).
    using mcla::dev_overrides::CvarName;
    using mcla::dev_overrides::CvarText;
    using mcla::dev_overrides::Info;
    using mcla::dev_overrides::TextSource;
    const auto &strict = Info(Key::kFrameLeadStrict);
    Check(FindKey("frame_lead_strict") == Key::kFrameLeadStrict &&
              strict.phase == Phase::kGraphicsPlugin &&
              CvarName(strict) == "d3d12_frame_lead_strict" && CvarText(strict, true) == "1" &&
              CvarText(strict, false).empty() && strict.true_source == TextSource::kLiteral &&
              !strict.overrides_user,
          "frame_lead_strict=true writes 1 to d3d12_frame_lead_strict after the plugin "
          "loaded, false writes nothing");
    const auto &adaptive = Info(Key::kFrameLeadAdaptive);
    Check(FindKey("frame_lead_adaptive") == Key::kFrameLeadAdaptive &&
              adaptive.phase == Phase::kGraphicsPlugin &&
              CvarName(adaptive) == "d3d12_frame_lead_strict" &&
              CvarText(adaptive, true) == "2" && CvarText(adaptive, false).empty() &&
              adaptive.true_source == TextSource::kLiteral && !adaptive.overrides_user,
          "frame_lead_adaptive=true writes 2 to d3d12_frame_lead_strict after the plugin "
          "loaded, false writes nothing");
    size_t lead_pairs = 0;
    for (const auto &pair : mcla::dev_overrides::kConflictPairs) {
      lead_pairs += (pair.first == Key::kFrameLeadStrict &&
                     pair.second == Key::kFrameLeadAdaptive)
                        ? 1
                        : 0;
      Check(pair.first != Key::kFrameLeadLimit1 && pair.second != Key::kFrameLeadLimit1,
            "frame_lead_limit_1 is in no conflict pair");
    }
    Check(lead_pairs == 1, "one conflict pair for the l91 strict and adaptive keys");
    Check(!FindKey("d3d12_frame_lead_strict") && !FindKey("frame_lead_strict_1") &&
              !FindKey("frame_lead_strict_2") && !FindKey("FRAME_LEAD_STRICT") &&
              !FindKey("lead_strict") && !FindKey("frame_lead_strict_on") &&
              !FindKey("frame_lead_adaptive_on") && !FindKey("FRAME_LEAD_ADAPTIVE") &&
              !FindKey("lead_adaptive") && !FindKey("frame_lead"),
          "no prefix, suffix, cvar-name, case or numeric variants of the l91 keys");
    const Parsed on = Parse("frame_lead_strict=true\n"
                            "frame_lead_adaptive=false\n"
                            "frame_lead_limit_1=true\n");
    Check(on.ValueCount() == 3 && on.issue_count == 0 &&
              on.Value(Key::kFrameLeadStrict) == true &&
              on.Value(Key::kFrameLeadAdaptive) == false &&
              on.Value(Key::kFrameLeadLimit1) == true,
          "strict with adaptive false and the l90 limit key: no conflict");
    const Parsed ad = Parse("frame_lead_adaptive=true\nframe_lead_strict=false\n");
    Check(ad.ValueCount() == 2 && ad.issue_count == 0 &&
              ad.Value(Key::kFrameLeadAdaptive) == true &&
              ad.Value(Key::kFrameLeadStrict) == false,
          "the adaptive arm parses");
    const Parsed both = Parse("frame_lead_strict=true\n"
                              "memory_watch_census=false\n"
                              "frame_lead_adaptive=true\n");
    Check(both.ValueCount() == 1 && !both.Value(Key::kFrameLeadStrict) &&
              !both.Value(Key::kFrameLeadAdaptive) &&
              both.Value(Key::kWatchCensus) == false && both.issue_count == 1 &&
              both.issues[0].line == 3 && both.issues[0].issue == LineIssue::kConflict,
          "strict and adaptive both true: neither applies, conflict on the later line");
    const Parsed bad = Parse("frame_lead_strict=1\n"
                             "d3d12_frame_lead_strict=true\n"
                             "frame_lead_strict=false\n"
                             "frame_lead_strict=true\n"
                             "frame_lead_adaptive=2\n");
    Check(bad.ValueCount() == 1 && bad.ignored_lines == 4 &&
              bad.Value(Key::kFrameLeadStrict) == false &&
              bad.issues[0].issue == LineIssue::kMalformed &&
              bad.issues[1].issue == LineIssue::kUnknownName &&
              bad.issues[2].issue == LineIssue::kDuplicate &&
              bad.issues[3].issue == LineIssue::kMalformed,
          "l91 near misses: an integer, the cvar name, a repeat and a non-bool are ignored");
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
              !FindKey("steady_exempt") &&
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
      for (const Key key : {Key::kFetchIdentityOff, Key::kFetchIdentityVerify}) {
        Check(pair.first != key && pair.second != key,
              "l73 fetch identity keys are in no conflict pair");
      }
      Check((pair.first != Key::kReadbackSteadyExempt &&
             pair.second != Key::kReadbackSteadyExempt) ||
                (pair.first == Key::kReadbackSteadyExempt &&
                 pair.second == Key::kReadbackSteadyExemptOff),
            "the steady key pairs only with its l78 control");
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
                                          false)
                .empty(),
        "KI-056 fix A key: host init-only bool at startup, its own cvar; since l81 (default "
        "on) =false writes nothing");
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
  Check(FindKey("mcla_ki026_probe") == Key::kKi026Probe && !FindKey("mcla_ki026_watchdog"),
        "KI-026 probe key; the KI-026 watchdog stays command-line only");
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
                          key == Key::kVblankHz88 || key == Key::kReadbackSteadyExemptOff ||
                          key == Key::kFenceWriteDeferredOff || key == Key::kFenceDeferGap2000 ||
                          key == Key::kXmaCanaryFixes || key == Key::kTimerQueueBlockingWait ||
                          key == Key::kTexturePlacedHeaps || key == Key::kTextureHeap64Mb ||
                          key == Key::kBlendMinmaxFix || key == Key::kShortSleepNative ||
                          key == Key::kApuPriorityNormal || key == Key::kTexturePlacedHeapsOff ||
                          key == Key::kTimerBlockingOff || key == Key::kAlbumPreviewRestore ||
                          key == Key::kZeroDelayNative || key == Key::kGuestAudioPrioNormal ||
                          key == Key::kBlendMinmaxFixOff || key == Key::kAlbumPreviewRestoreOff ||
                          key == Key::kCpBulkRegs || key == Key::kAmbientDtProbe ||
                          key == Key::kAmbientDtSnapshot || key == Key::kRawTimerHitchHold ||
                          key == Key::kRawTimerStats || key == Key::kRawTimerHitchHoldOff ||
                          key == Key::kAudioFilterProbe || key == Key::kAudioFilterNative ||
                          key == Key::kAudioFilterNativeVerify ||
                          key == Key::kReadbackFencedDelivery ||
                          key == Key::kAudioFilterNativeChOff ||
                          key == Key::kFrameLeadLimit1 || key == Key::kReadbackDeferredRelease ||
                          key == Key::kFrameLeadStrict || key == Key::kFrameLeadAdaptive ||
                          key == Key::kWarpStallFix || key == Key::kReadbackReadWatch ||
                          key == Key::kReadbackAgeInject2 ||
                          key == Key::kOcclusionQueryBounded4 || key == Key::kPresentStats ||
                          key == Key::kPresentInterval1 || key == Key::kLockedTearingOff ||
                          key == Key::kTodRateX10 || key == Key::kTodRateX30 ||
                          key == Key::kTodFreeze || key == Key::kTodStart0430 ||
                          key == Key::kTodStart0530;
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
    // Ten pairs (l77: the walk fix and its control; l78: the steady key and
    // the fence key with their controls; l80: the timer and heaps keys with
    // their controls; l81: the album restore and blend keys with their
    // controls; l86: the hitch hold key with its control) plus the kick and
    // the vblank groups of three (each listed as its three pairs); l91: the
    // strict and adaptive lead keys; l93: the time-of-day rate group of three
    // and the two start keys.
    Check(shared == mcla::dev_overrides::kConflictPairs.size() && shared == 22,
          "twenty-two shared-cvar pairs");
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
          "the l71 triad arm has no restore value (the cvar keeps its default)");
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
    // The l77 arm text for one l77_arms row (empty if the l76 base text lost
    // its steady key line).
    const auto l77_text = [&base39](const char *const *arm) -> std::string {
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
      if (at == std::string::npos) {
        return {};
      }
      l76_text.replace(at, exempt_false.size(),
                       std::string("readback_steady_exempt=") + arm[0] + "\n");
      return l76_text + "fence_write_deferred=" + arm[1] + "\n" + "xma_frame_walk_fix_off=" +
             arm[2] + "\n" + "raw_timer_off=" + arm[3] + "\n" + "vblank_hz_72=" + arm[4] +
             "\n" + "vblank_hz_80=" + arm[5] + "\n" + "vblank_hz_88=" + arm[6] + "\n";
    };
    // l78: the l77 arms with the three l78 keys explicit false (62 keys);
    // l79: plus the four l79 keys explicit false (66 keys); l80: plus the
    // five l80 keys explicit false (71 keys); l81: plus the four l81 keys
    // explicit false (75 keys; beyond the former 2 KiB limit); l82: plus the
    // l82 key explicit false (76 keys); l83: plus the three l83 keys explicit
    // false (79 keys); l84: plus the l84 key explicit false (80 keys); l86:
    // plus the l86 key explicit false (81 keys); l87: plus the three l87 keys
    // explicit false (84 keys); l88: plus the l88 key explicit false (85 keys);
    // l89: plus the l89 key explicit false (86 keys); l90: plus the two l90
    // keys explicit false (88 keys); l91: plus the two l91 keys explicit false
    // (90 keys); KI-026: plus the two KI-026 keys explicit false (92 keys);
    // l92: plus the six l92 keys explicit false (98 keys); l93: plus the five
    // l93 keys explicit false (103 keys).
    const std::string l83_false = "ambient_dt_probe=false\n"
                                  "ambient_dt_snapshot=false\n"
                                  "raw_timer_hitch_hold=false\n"
                                  "raw_timer_stats=false\n"
                                  "raw_timer_hitch_hold_off=false\n"
                                  "audio_filter_probe=false\n"
                                  "audio_filter_native=false\n"
                                  "audio_filter_native_verify=false\n"
                                  "readback_fenced_delivery=false\n"
                                  "audio_filter_native_ch_off=false\n"
                                  "frame_lead_limit_1=false\n"
                                  "readback_deferred_release=false\n"
                                  "frame_lead_strict=false\n"
                                  "frame_lead_adaptive=false\n"
                                  "mcla_warp_stall_fix=false\n"
                                  "mcla_ki026_probe=false\n"
                                  "readback_read_watch=false\n"
                                  "readback_age_inject_2=false\n"
                                  "occlusion_query_bounded_4=false\n"
                                  "present_stats=false\n"
                                  "present_interval_1=false\n"
                                  "locked_tearing_off=false\n"
                                  "tod_rate_x10=false\n"
                                  "tod_rate_x30=false\n"
                                  "tod_freeze=false\n"
                                  "tod_start_0430=false\n"
                                  "tod_start_0530=false\n";
    const std::string l82_false = "cp_bulk_regs=false\n" + l83_false;
    const std::string l81_false = "zero_delay_native=false\n"
                                  "guest_audio_prio_normal=false\n"
                                  "blend_minmax_fix_off=false\n"
                                  "album_preview_restore_off=false\n" +
                                  l82_false;
    const std::string l80_false = "blend_minmax_fix=false\n"
                                  "short_sleep_native=false\n"
                                  "apu_priority_normal=false\n"
                                  "texture_placed_heaps_off=false\n"
                                  "timer_blocking_off=false\n" +
                                  l81_false;
    const std::string l79_false = "xma_canary_fixes=false\n"
                                  "timer_queue_blocking_wait=false\n"
                                  "texture_placed_heaps=false\n"
                                  "texture_heap_64mb=false\n" +
                                  l80_false;
    const std::string l78_false = "readback_steady_exempt_off=false\n"
                                  "fence_write_deferred_off=false\n"
                                  "fence_defer_gap_2000=false\n" +
                                  l79_false;
    for (const auto &arm : l77_arms) {
      const std::string l77 = l77_text(arm);
      Check(!l77.empty(), "the l76 base text holds the steady key");
      const std::string text = l77 + l78_false;
      const Parsed a = Parse(text);
      Check(text.size() <= mcla::dev_overrides::kMaxBytes && a.status == FileStatus::kParsed &&
                a.ValueCount() == mcla::dev_overrides::kKeyCount && a.ignored_lines == 0 &&
                a.issue_count == 0,
            "l77 arm: every key (80), nothing ignored, fits in kMaxBytes");
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
    // l78 arms (private/l78-arms): default = the l77 base arm plus the three
    // l78 keys false (the l78 SDK defaults give the steady exemption, the
    // deferred fences and the W1 walk fix); exempt-off, fence-off and both-off
    // add the controls (both-off = the l70-style behaviour); walk-off adds
    // xma_frame_walk_fix_off; the Deck gap2000 arm adds fence_defer_gap_2000.
    const char *const l78_arms[6][4] = {
        // walk_fix_off, exempt_off, fence_off, gap2000
        {"false", "false", "false", "false"},  // default
        {"false", "true", "false", "false"},   // exempt-off
        {"false", "false", "true", "false"},   // fence-off
        {"false", "true", "true", "false"},    // both-off
        {"true", "false", "false", "false"},   // walk-off
        {"false", "false", "false", "true"}};  // gap2000
    for (const auto &arm : l78_arms) {
      const char *const l77_row[7] = {"false", "false", arm[0], "false",
                                      "false", "false", "false"};
      const std::string l77 = l77_text(l77_row);
      Check(!l77.empty(), "the l76 base text holds the steady key");
      const std::string text = l77 + "readback_steady_exempt_off=" + arm[1] + "\n" +
                               "fence_write_deferred_off=" + arm[2] + "\n" +
                               "fence_defer_gap_2000=" + arm[3] + "\n" + l79_false;
      const Parsed a = Parse(text);
      Check(text.size() <= mcla::dev_overrides::kMaxBytes && a.status == FileStatus::kParsed &&
                a.ValueCount() == mcla::dev_overrides::kKeyCount && a.ignored_lines == 0 &&
                a.issue_count == 0,
            "l78 arm: every key (80), nothing ignored, fits in kMaxBytes");
      Check(a.Value(Key::kReadbackSteadyExempt) == false &&
                a.Value(Key::kFenceWriteDeferred) == false &&
                a.Value(Key::kXmaFrameWalkFix) == false &&
                a.Value(Key::kXmaFrameWalkFixOff) == (arm[0][0] == 't') &&
                a.Value(Key::kReadbackSteadyExemptOff) == (arm[1][0] == 't') &&
                a.Value(Key::kFenceWriteDeferredOff) == (arm[2][0] == 't') &&
                a.Value(Key::kFenceDeferGap2000) == (arm[3][0] == 't'),
            "l78 arm values (on keys false: the l78 SDK defaults apply)");
    }
    // The l78 controls with their on keys: both true is a conflict (neither
    // applies, logged on the later line); one true with the other false is not.
    const Parsed exempt_both = Parse("readback_steady_exempt=true\n"
                                     "fence_defer_gap_2000=true\n"
                                     "readback_steady_exempt_off=true\n");
    Check(exempt_both.ValueCount() == 1 && !exempt_both.Value(Key::kReadbackSteadyExempt) &&
              !exempt_both.Value(Key::kReadbackSteadyExemptOff) &&
              exempt_both.Value(Key::kFenceDeferGap2000) == true &&
              exempt_both.issue_count == 1 && exempt_both.issues[0].line == 3 &&
              exempt_both.issues[0].issue == LineIssue::kConflict,
          "steady key with its control both true: neither applies, conflict on the later line");
    const Parsed fence_both = Parse("fence_write_deferred_off=true\nfence_write_deferred=true\n");
    Check(fence_both.ValueCount() == 0 && fence_both.issue_count == 1 &&
              fence_both.issues[0].line == 2 &&
              fence_both.issues[0].issue == LineIssue::kConflict,
          "fence key with its control both true: neither applies, conflict on the later line");
    const Parsed both_off = Parse("readback_steady_exempt=false\n"
                                  "readback_steady_exempt_off=true\n"
                                  "fence_write_deferred=false\n"
                                  "fence_write_deferred_off=true\n");
    Check(both_off.ValueCount() == 4 && both_off.issue_count == 0 &&
              both_off.Value(Key::kReadbackSteadyExemptOff) == true &&
              both_off.Value(Key::kFenceWriteDeferredOff) == true &&
              both_off.Value(Key::kReadbackSteadyExempt) == false &&
              both_off.Value(Key::kFenceWriteDeferred) == false,
          "both-off: on keys false with the controls true is not a conflict");
    const Parsed l78_bad = Parse("fence_defer_gap_2000=2000\n"
                                 "d3d12_fence_defer_min_gap_us=true\n"
                                 "fence_defer_gap_1000=true\n"
                                 "readback_steady_exempt_off=off\n"
                                 "fence_write_deferred_off=true\n"
                                 "fence_write_deferred_off=false\n"
                                 "fence_defer_gap_2000=true\n");
    Check(l78_bad.ValueCount() == 2 && l78_bad.Value(Key::kFenceWriteDeferredOff) == true &&
              l78_bad.Value(Key::kFenceDeferGap2000) == true &&
              !l78_bad.Value(Key::kReadbackSteadyExemptOff) && l78_bad.ignored_lines == 5 &&
              l78_bad.issues[0].issue == LineIssue::kMalformed &&
              l78_bad.issues[1].issue == LineIssue::kUnknownName &&
              l78_bad.issues[2].issue == LineIssue::kUnknownName &&
              l78_bad.issues[3].issue == LineIssue::kMalformed &&
              l78_bad.issues[4].issue == LineIssue::kDuplicate,
          "l78 near misses: integer values, the cvar name, other gaps and repeats are ignored");
    // l79 arms (private/l79-arms): default = the l78 default arm plus the four
    // l79 keys false; pc xma / timer / heaps add one key each, all3 the three
    // switches; deck heaps64 and deck all3 also set texture_heap_64mb.
    const char *const l79_arms[7][4] = {
        // canary, timer, heaps, heap_64mb
        {"false", "false", "false", "false"},  // default
        {"true", "false", "false", "false"},   // xma
        {"false", "true", "false", "false"},   // timer
        {"false", "false", "true", "false"},   // heaps
        {"true", "true", "true", "false"},     // all3
        {"false", "false", "true", "true"},    // deck heaps64
        {"true", "true", "true", "true"}};     // deck all3
    for (const auto &arm : l79_arms) {
      const char *const l77_row[7] = {"false", "false", "false", "false",
                                      "false", "false", "false"};
      const std::string l77 = l77_text(l77_row);
      Check(!l77.empty(), "the l76 base text holds the steady key");
      const std::string text = l77 +
                               "readback_steady_exempt_off=false\n"
                               "fence_write_deferred_off=false\n"
                               "fence_defer_gap_2000=false\n"
                               "xma_canary_fixes=" +
                               arm[0] + "\n" + "timer_queue_blocking_wait=" + arm[1] + "\n" +
                               "texture_placed_heaps=" + arm[2] + "\n" +
                               "texture_heap_64mb=" + arm[3] + "\n" + l80_false;
      const Parsed a = Parse(text);
      Check(text.size() <= mcla::dev_overrides::kMaxBytes && a.status == FileStatus::kParsed &&
                a.ValueCount() == mcla::dev_overrides::kKeyCount && a.ignored_lines == 0 &&
                a.issue_count == 0,
            "l79 arm: every key (80), nothing ignored, fits in kMaxBytes");
      Check(a.Value(Key::kXmaCanaryFixes) == (arm[0][0] == 't') &&
                a.Value(Key::kTimerQueueBlockingWait) == (arm[1][0] == 't') &&
                a.Value(Key::kTexturePlacedHeaps) == (arm[2][0] == 't') &&
                a.Value(Key::kTextureHeap64Mb) == (arm[3][0] == 't') &&
                a.Value(Key::kReadbackSteadyExemptOff) == false &&
                a.Value(Key::kFenceWriteDeferredOff) == false &&
                a.Value(Key::kFenceDeferGap2000) == false,
            "l79 arm values (l78 controls false: the l78 SDK defaults apply)");
    }
    const Parsed l79_all = Parse("xma_canary_fixes=true\n"
                                 "timer_queue_blocking_wait=true\n"
                                 "texture_placed_heaps=true\n"
                                 "texture_heap_64mb=true\n");
    Check(l79_all.ValueCount() == 4 && l79_all.issue_count == 0 &&
              l79_all.Value(Key::kXmaCanaryFixes) == true &&
              l79_all.Value(Key::kTimerQueueBlockingWait) == true &&
              l79_all.Value(Key::kTexturePlacedHeaps) == true &&
              l79_all.Value(Key::kTextureHeap64Mb) == true,
          "all four l79 keys true together: no conflict");
    const Parsed l79_bad = Parse("texture_heap_64mb=64\n"
                                 "d3d12_texture_placed_heaps=true\n"
                                 "texture_heap_128mb=true\n"
                                 "timer_queue_blocking_wait=on\n"
                                 "xma_canary_fixes=true\n"
                                 "xma_canary_fixes=false\n"
                                 "texture_heap_64mb=true\n");
    Check(l79_bad.ValueCount() == 2 && l79_bad.Value(Key::kXmaCanaryFixes) == true &&
              l79_bad.Value(Key::kTextureHeap64Mb) == true &&
              !l79_bad.Value(Key::kTimerQueueBlockingWait) &&
              !l79_bad.Value(Key::kTexturePlacedHeaps) && l79_bad.ignored_lines == 5 &&
              l79_bad.issues[0].issue == LineIssue::kMalformed &&
              l79_bad.issues[1].issue == LineIssue::kUnknownName &&
              l79_bad.issues[2].issue == LineIssue::kUnknownName &&
              l79_bad.issues[3].issue == LineIssue::kMalformed &&
              l79_bad.issues[4].issue == LineIssue::kDuplicate,
          "l79 near misses: integer values, the cvar name, other sizes and repeats are ignored");
    // l80 arms (private/l80-arms): default = the l79 default arm (l79 switches
    // false: the l80 platform defaults apply) plus the five l80 keys false;
    // pc blend adds blend_minmax_fix, pc heaps-off texture_placed_heaps_off;
    // deck census adds memory_watch_census, deck audio-off the two KI-060
    // controls.
    const char *const l80_arms[5][6] = {
        // census, blend, short_sleep, apu, heaps_off, timer_off
        {"false", "false", "false", "false", "false", "false"},  // default
        {"false", "true", "false", "false", "false", "false"},   // pc blend
        {"false", "false", "false", "false", "true", "false"},   // pc heaps-off
        {"true", "false", "false", "false", "false", "false"},   // deck census
        {"false", "false", "true", "true", "false", "false"}};   // deck audio-off
    for (const auto &arm : l80_arms) {
      const char *const l77_row[7] = {"false", "false", "false", "false",
                                      "false", "false", "false"};
      std::string l77 = l77_text(l77_row);
      const std::string_view census_false = "memory_watch_census=false\n";
      const size_t at = l77.find(census_false);
      Check(!l77.empty() && at != std::string::npos,
            "the l76 base text holds the steady and census keys");
      if (at == std::string::npos) {
        continue;
      }
      l77.replace(at, census_false.size(), std::string("memory_watch_census=") + arm[0] + "\n");
      const std::string text = l77 +
                               "readback_steady_exempt_off=false\n"
                               "fence_write_deferred_off=false\n"
                               "fence_defer_gap_2000=false\n"
                               "xma_canary_fixes=false\n"
                               "timer_queue_blocking_wait=false\n"
                               "texture_placed_heaps=false\n"
                               "texture_heap_64mb=false\n"
                               "blend_minmax_fix=" +
                               arm[1] + "\n" + "short_sleep_native=" + arm[2] + "\n" +
                               "apu_priority_normal=" + arm[3] + "\n" +
                               "texture_placed_heaps_off=" + arm[4] + "\n" +
                               "timer_blocking_off=" + arm[5] + "\n" + l81_false;
      const Parsed a = Parse(text);
      Check(text.size() <= mcla::dev_overrides::kMaxBytes && a.status == FileStatus::kParsed &&
                a.ValueCount() == mcla::dev_overrides::kKeyCount && a.ignored_lines == 0 &&
                a.issue_count == 0,
            "l80 arm: every key (80), nothing ignored, fits in kMaxBytes");
      Check(a.Value(Key::kWatchCensus) == (arm[0][0] == 't') &&
                a.Value(Key::kBlendMinmaxFix) == (arm[1][0] == 't') &&
                a.Value(Key::kShortSleepNative) == (arm[2][0] == 't') &&
                a.Value(Key::kApuPriorityNormal) == (arm[3][0] == 't') &&
                a.Value(Key::kTexturePlacedHeapsOff) == (arm[4][0] == 't') &&
                a.Value(Key::kTimerBlockingOff) == (arm[5][0] == 't') &&
                a.Value(Key::kXmaCanaryFixes) == false &&
                a.Value(Key::kTimerQueueBlockingWait) == false &&
                a.Value(Key::kTexturePlacedHeaps) == false,
            "l80 arm values (l79 switches false: the l80 platform defaults apply)");
    }
    const Parsed l80_bad = Parse("short_sleep_native=native\n"
                                 "ke_delay_short_sleep_mode=true\n"
                                 "apu_priority_above_normal=true\n"
                                 "blend_minmax_fix=1\n"
                                 "timer_blocking_off=true\n"
                                 "timer_blocking_off=false\n"
                                 "apu_priority_normal=true\n");
    Check(l80_bad.ValueCount() == 2 && l80_bad.Value(Key::kTimerBlockingOff) == true &&
              l80_bad.Value(Key::kApuPriorityNormal) == true &&
              !l80_bad.Value(Key::kShortSleepNative) && !l80_bad.Value(Key::kBlendMinmaxFix) &&
              l80_bad.ignored_lines == 5 && l80_bad.issues[0].issue == LineIssue::kMalformed &&
              l80_bad.issues[1].issue == LineIssue::kUnknownName &&
              l80_bad.issues[2].issue == LineIssue::kUnknownName &&
              l80_bad.issues[3].issue == LineIssue::kMalformed &&
              l80_bad.issues[4].issue == LineIssue::kDuplicate,
          "l80 near misses: string values, the cvar name, other values and repeats are ignored");
    // l81 arms (private/l81-arms): only the keys that differ from the build
    // defaults, so the M10 Wine defaults (upload-ahead 16, disarm bridging)
    // stay with mcla_m10_defaults on the Deck: no upload-ahead, bridge, hot
    // block or other =false line; memory_watch_census=false keeps the census
    // (SDK default on in owner builds) off except in the census arm. The pc
    // arms keep the l80 pc probes (warp, album, pursuit).
    {
      const std::string base = "mcla_readback_sync_composite=true\n"
                               "mcla_native_d3d_refcount=true\n"
                               "mcla_native_xapi_interlocked=true\n"
                               "d3d12_wrm_event_wake=true\n"
                               "d3d12_replay_batched_wake=true\n"
                               "d3d12_readback_elide_unread=true\n"
                               "prewarm_seed_store=true\n"
                               "d3d12_readback_sync_targeted=true\n";
      const std::string pc = base +
                             "memory_watch_census=false\n"
                             "mcla_warp_probe=true\n"
                             "mcla_album_probe=true\n"
                             "mcla_pursuit_probe=true\n";
      const std::string arms[] = {pc,
                                  pc + "blend_minmax_fix_off=true\n",
                                  base + "memory_watch_census=false\n",
                                  base + "memory_watch_census=true\n",
                                  base + "memory_watch_census=false\n"
                                         "zero_delay_native=true\n"
                                         "guest_audio_prio_normal=true\n"};
      for (const std::string &text : arms) {
        const Parsed a = Parse(text);
        Check(text.size() < 1024 && a.status == FileStatus::kParsed && a.ignored_lines == 0 &&
                  a.issue_count == 0 && a.ValueCount() >= 9 && a.ValueCount() <= 13,
              "l81 arm parses, nothing ignored");
        Check(!a.Value(Key::kUploadAhead16) && !a.Value(Key::kUploadAhead64) &&
                  !a.Value(Key::kWatchDisarmBridge) && !a.Value(Key::kHotBlocksShadow) &&
                  !a.Value(Key::kHotBlocksOn) && !a.Value(Key::kAlbumPreviewRestore) &&
                  !a.Value(Key::kBlendMinmaxFix) && a.Value(Key::kWatchCensus).has_value(),
              "l81 arm: no line for the Wine watch defaults or the default-on fixes");
        size_t false_lines = 0;
        for (size_t i = 0; i < mcla::dev_overrides::kKeyCount; ++i) {
          false_lines += a.values[i] == false ? 1 : 0;
        }
        Check(false_lines == (a.Value(Key::kWatchCensus) == false ? 1u : 0u),
              "l81 arm: the census line is the only =false line");
      }
    }
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
  Check(mcla::dev_overrides::kMaxBytes == 4096, "4 KiB limit (l81; 2 KiB l73-l80)");
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
    Check(padded.size() == 4096, "padded size");
    const Parsed ok = Parse(padded);
    Check(ok.status == FileStatus::kParsed && ok.Value(Key::kWrmEventWake) == true,
          "exactly 4 KiB accepted");
    padded += '\n';
    const Parsed big = Parse(padded);
    Check(big.status == FileStatus::kRejected && NoValues(big), "4097 bytes rejected");
  }
  {
    // A full 75-key arm with CRLF line ends (an editor re-save) is beyond the
    // former 2 KiB and accepted since l81 (l93: 103 keys, about 2.95 KB).
    std::string full;
    for (size_t i = 0; i < mcla::dev_overrides::kKeyCount; ++i) {
      full += std::string(mcla::dev_overrides::kKeys[i].name) + "=false\r\n";
    }
    Check(full.size() < 3100, "full arm size");
    full += "# " + std::string(3200 - full.size() - 4, 'x') + "\r\n";
    const Parsed p = Parse(full);
    Check(full.size() > 2048 && full.size() <= mcla::dev_overrides::kMaxBytes &&
              p.status == FileStatus::kParsed &&
              p.ValueCount() == mcla::dev_overrides::kKeyCount && p.ignored_lines == 0,
          "full CRLF arm with a comment, between 2 and 4 KiB: accepted, every key");
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
  constexpr Parsed fence_conflict =
      Parse("fence_write_deferred=true\nfence_write_deferred_off=true\n");
  static_assert(fence_conflict.ValueCount() == 0);
  static_assert(fence_conflict.issues[0].line == 2);
  static_assert(fence_conflict.issues[0].issue == LineIssue::kConflict);
  constexpr Parsed exempt_conflict =
      Parse("readback_steady_exempt_off=true\nreadback_steady_exempt=true\n");
  static_assert(exempt_conflict.ValueCount() == 0);
  static_assert(exempt_conflict.issues[0].line == 2);
  constexpr Parsed blend_conflict = Parse("blend_minmax_fix_off=true\nblend_minmax_fix=true\n");
  static_assert(blend_conflict.ValueCount() == 0);
  static_assert(blend_conflict.issues[0].line == 2);
  static_assert(blend_conflict.issues[0].issue == LineIssue::kConflict);
  constexpr Parsed album_conflict =
      Parse("mcla_album_preview_restore=true\nalbum_preview_restore_off=true\n");
  static_assert(album_conflict.ValueCount() == 0);
  static_assert(album_conflict.issues[0].line == 2);
  static_assert(album_conflict.issues[0].issue == LineIssue::kConflict);
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
