// Source-only no-GPU test. Include the module once; do not also compile it as a
// separate TU in this target. Define MCLA_GUEST_SOURCE_SCOUT_PROBE=1, use matching
// private provenance SDK headers/runtime, link normal rex core/UI dependencies
// plus mcla_launcher_fs_win.cpp and bcrypt, and expose nlohmann_json headers.
// No Run/Presenter/window/GPU/process/input operations or filesystem test writes.
#include "../src/mcla_guest_source_scout_win.cpp"
#include <nlohmann/json.hpp>
#include <iostream>
#if defined(MCLA_TIMER_CONFIG_PROBE) && (!defined(MCLA_UPDATE_TIMING_PROBE) || !defined(MCLA_PHASE_CENSUS_PROBE) || !defined(MCLA_SWAP_OBSERVER_PROBE))
#error Timer-config integration test requires the production update/phase/swap compile prerequisites.
#endif

REXCVAR_DEFINE_BOOL(mcla_scout_test_peer_probe, false, "Test", "Synthetic peer");
REXCVAR_DEFINE_BOOL(mcla_autostart_probe, false, "Test", "Synthetic navigation flag");
// Exact flag names in the real cvar registry; the production validator below is
// never replaced. These declarations let the same test exercise compiled and
// noncompiled admission. Do NOT also link the observer implementations here.
REXCVAR_DEFINE_BOOL(mcla_timer_config_probe, false, "Test", "Startup inventory admission")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(mcla_update_timing_probe, false, "Test", "Update timing prerequisite")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(mcla_fixed60_config_experiment, false, "Test", "Exact fixed60 admission")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(mcla_raw_config_experiment, false, "Test", "Exact raw admission")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
#if defined(MCLA_TIMER_CONFIG_PROBE)
namespace mcla::timer_config {
bool Enabled() { return REXCVAR_GET(mcla_timer_config_probe); }
bool ExperimentEnabled() {
#if defined(MCLA_FIXED60_CONFIG_EXPERIMENT)
  return REXCVAR_GET(mcla_fixed60_config_experiment);
#else
  return false;
#endif
}
bool RawExperimentEnabled() {
#if defined(MCLA_RAW_CONFIG_EXPERIMENT)
  return REXCVAR_GET(mcla_raw_config_experiment);
#else
  return false;
#endif
}
std::string Freeze(const std::string&) { throw std::runtime_error("test must not freeze config observer"); }
void Disarm() noexcept { std::terminate(); } // No Run/cleanup path belongs in this test.
}
#endif
#if defined(MCLA_UPDATE_TIMING_PROBE)
namespace mcla::update_timing {
bool Enabled() { return REXCVAR_GET(mcla_update_timing_probe); }
void Begin(uint64_t,uint64_t,uint64_t,const std::string&) { throw std::runtime_error("test must not begin timing observer"); }
std::string Freeze(const std::string&) { throw std::runtime_error("test must not freeze timing observer"); }
void Disarm() noexcept { std::terminate(); }
}
#endif
// Scout's observer serialization declarations are nested in its phase branch.
// Timer-config variants therefore also compile PHASE+SWAP; these no-runtime
// stubs satisfy unreachable collector references, never replace its validator.
#if defined(MCLA_PHASE_CENSUS_PROBE)
REXCVAR_DEFINE_BOOL(mcla_phase_census_probe, false, "Test", "Phase compile prerequisite");
namespace mcla::phase_census {
bool Enabled() { return REXCVAR_GET(mcla_phase_census_probe); }
void Begin(uint64_t,uint64_t,uint64_t,const std::string&) { throw std::runtime_error("test must not begin phase observer"); }
std::string Finish(std::stop_token,const std::string&) { throw std::runtime_error("test must not finish phase observer"); }
void Disarm() noexcept { std::terminate(); }
}
#endif
#if defined(MCLA_SWAP_OBSERVER_PROBE)
REXCVAR_DEFINE_BOOL(mcla_swap_observer_probe, false, "Test", "Swap compile prerequisite");
namespace mcla::swap_native {
bool Enabled() { return REXCVAR_GET(mcla_swap_observer_probe); }
void Begin(uint64_t,uint64_t,const std::string&,guest_source_scout::detail::Profile) { throw std::runtime_error("test must not begin swap observer"); }
std::string Serialize(const guest_source_scout::detail::Sequence&,const std::string&,guest_source_scout::detail::Profile) { throw std::runtime_error("test must not serialize swap observer"); }
void Disarm() noexcept { std::terminate(); }
}
#endif

namespace {
namespace probe = mcla::guest_source_scout;
using JsonValue = nlohmann::json;
constexpr uint64_t readiness = 100000, frequency = 10000;
constexpr uint64_t begin = readiness + 40 * frequency;
unsigned checks = 0, failures = 0;
void Check(bool value, const char* label) {
  ++checks;
  if (!value) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
template<class F> void Throws(F&& action, const char* reason) {
  bool expected = false;
  try { action(); }
  catch (const probe::Failure& failure) { expected = std::string_view(failure.reason) == reason; }
  catch (...) {}
  Check(expected, reason);
}
template<class F> void Accepts(F&& action,const char* label) {
  bool accepted=false;
  try { action(); accepted=true; }
  catch(const std::exception& error) { std::cerr<<label<<": "<<error.what()<<'\n'; }
  catch(...) { std::cerr<<label<<": unknown exception\n"; }
  Check(accepted,label);
}

void TimerConfigAdmissionTests() {
  // This is the actual path called by OnPreSetup / autostart's delegated
  // validator, BEFORE graphics/runtime construction or the five-hook Install.
  REXCVAR_SET(mcla_source_scout_probe,true);
  REXCVAR_SET(mcla_source_scout_token,"0123456789abcdef0123456789abcdef");
  REXCVAR_SET(mcla_source_scout_profile,"short31_10hz");
  REXCVAR_SET(mcla_timer_config_probe,false);
  REXCVAR_SET(mcla_update_timing_probe,false);
#if !defined(MCLA_TIMER_OBSERVER_PROBE)
  REXCVAR_SET(mcla_fixed60_config_experiment,true);
#if defined(MCLA_FIXED60_CONFIG_EXPERIMENT)
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"FIXED60_REQUIRES_CONFIG_UPDATE_SCOUT");
  REXCVAR_SET(mcla_update_timing_probe,true);
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"FIXED60_REQUIRES_CONFIG_UPDATE_SCOUT");
  REXCVAR_SET(mcla_timer_config_probe,true);
  Accepts([]{probe::ValidateExclusiveConfig("d3d12");},"compiled fixed60 with all prerequisites admitted");
  REXCVAR_SET(mcla_source_scout_probe,false);
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"FIXED60_REQUIRES_CONFIG_UPDATE_SCOUT");
  REXCVAR_SET(mcla_source_scout_probe,true);
  REXCVAR_SET(mcla_timer_config_probe,false);
  REXCVAR_SET(mcla_update_timing_probe,false);
#else
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"FIXED60_NOT_COMPILED");
#endif
  REXCVAR_SET(mcla_fixed60_config_experiment,false);
  REXCVAR_SET(mcla_raw_config_experiment,true);
#if defined(MCLA_RAW_CONFIG_EXPERIMENT)
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"RAW_REQUIRES_CONFIG_UPDATE_SCOUT");
  REXCVAR_SET(mcla_update_timing_probe,true);
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"RAW_REQUIRES_CONFIG_UPDATE_SCOUT");
  REXCVAR_SET(mcla_timer_config_probe,true);
  Accepts([]{probe::ValidateExclusiveConfig("d3d12");},"compiled raw with all prerequisites admitted");
  REXCVAR_SET(mcla_update_timing_probe,false);
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"RAW_REQUIRES_CONFIG_UPDATE_SCOUT");
  REXCVAR_SET(mcla_update_timing_probe,true);
#if defined(MCLA_FIXED60_CONFIG_EXPERIMENT)
  REXCVAR_SET(mcla_fixed60_config_experiment,true);
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"CONFIG_EXPERIMENTS_MUTUALLY_EXCLUSIVE");
  REXCVAR_SET(mcla_source_scout_probe,false);
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"CONFIG_EXPERIMENTS_MUTUALLY_EXCLUSIVE");
  REXCVAR_SET(mcla_fixed60_config_experiment,false);
  REXCVAR_SET(mcla_source_scout_probe,true);
#endif
  REXCVAR_SET(mcla_source_scout_probe,false);
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"RAW_REQUIRES_CONFIG_UPDATE_SCOUT");
  REXCVAR_SET(mcla_source_scout_probe,true);
  REXCVAR_SET(mcla_timer_config_probe,false);
  REXCVAR_SET(mcla_update_timing_probe,false);
#else
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"RAW_NOT_COMPILED");
#endif
  REXCVAR_SET(mcla_raw_config_experiment,false);
  Accepts([]{probe::ValidateExclusiveConfig("d3d12");},"disabled startup inventory does not require update timing");
  REXCVAR_SET(mcla_timer_config_probe,true);
#if defined(MCLA_TIMER_CONFIG_PROBE)
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"TIMER_CONFIG_REQUIRES_UPDATE_TIMING");
  REXCVAR_SET(mcla_update_timing_probe,true);
  // RED before the production allowlist fix: actual validator currently throws
  // OTHER_PROBE_ENABLED_OR_INVALID despite this explicitly compiled pair.
  Accepts([]{probe::ValidateExclusiveConfig("d3d12");},"compiled startup inventory with enabled update timing admitted");
  REXCVAR_SET(mcla_scout_test_peer_probe,true);
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"OTHER_PROBE_ENABLED_OR_INVALID");
  REXCVAR_SET(mcla_scout_test_peer_probe,false);
  REXCVAR_SET(mcla_timer_config_probe,false);
  Accepts([]{probe::ValidateExclusiveConfig("d3d12");},"compiled update timing remains accepted with config disabled");
  REXCVAR_SET(mcla_update_timing_probe,false);
#else
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"OTHER_PROBE_ENABLED_OR_INVALID");
#endif
  REXCVAR_SET(mcla_timer_config_probe,false);
  REXCVAR_SET(mcla_scout_test_peer_probe,true);
  Throws([]{probe::ValidateExclusiveConfig("d3d12");},"OTHER_PROBE_ENABLED_OR_INVALID");
  REXCVAR_SET(mcla_scout_test_peer_probe,false);
  Accepts([]{probe::ValidateExclusiveConfig("d3d12");},"config admission controls restored to disabled baseline");
#endif
  REXCVAR_SET(mcla_timer_config_probe,false);
  REXCVAR_SET(mcla_update_timing_probe,false);
  REXCVAR_SET(mcla_source_scout_probe,false);
  REXCVAR_SET(mcla_source_scout_token,"");
  REXCVAR_SET(mcla_source_scout_profile,"legacy91_10hz");
}

struct Fake {
  uint64_t clock = readiness;
  size_t captures = 0, waits = 0;
  std::stop_source cancellation;
  std::function<void(Fake&, uint64_t)> waiting;
  std::function<void(Fake&, rex::ui::GuestOutputCapture&)> modify;
  rex::ui::GuestCaptureStatus outcome = rex::ui::GuestCaptureStatus::kSuccess;
  bool duplicate = false;

  probe::detail::Sequence Collect(uint64_t freq = frequency, uint64_t start = readiness,
                                 probe::detail::Profile profile = probe::detail::Profile::kLegacy91) {
    return probe::detail::Collect(cancellation.get_token(), freq, start, 2, 1, {
        [&] { return clock; },
        [&](uint64_t due, std::stop_token) {
          ++waits; clock = std::max(clock, due);
          if (waiting) waiting(*this, due);
        },
        [&](rex::ui::GuestOutputCapture& output) {
          ++captures;
          const auto before = clock;
          const auto sequence = duplicate ? 1 : captures;
          const auto publication = duplicate ? begin - 50 : before - 50;
          output.image.width = 2; output.image.height = 1; output.image.stride = 8;
          output.image.data = {uint8_t(sequence), 2, 3, 0, 4, 5, 6, 255};
          output.provenance = {7, sequence, freq, publication, publication + 10,
                              before, before + 10, before + 20, before + 30, before + 40};
          clock += 50;
          if (modify) modify(*this, output);
          return outcome;
        }}, profile);
  }
};

void SchedulerTests() {
  {
    constexpr auto limit = probe::detail::kBufferLimitBytes;
    constexpr auto headroom = probe::detail::kCaptureHeadroomBytes;
    Check(probe::detail::CanAcquireCapture(0), "empty collector reserves next result");
    Check(probe::detail::CanAcquireCapture(limit - headroom), "exact headroom boundary allowed");
    for (auto retained : {limit - headroom + 1, limit, UINT64_MAX})
      Check(!probe::detail::CanAcquireCapture(retained), "reject before allocation with insufficient headroom");
    Check(probe::detail::CanRetainCapture(limit - headroom, headroom), "exact retained budget boundary allowed");
    Check(!probe::detail::CanRetainCapture(0, headroom + 1), "oversized capacity rejects even with small image size");
    Check(!probe::detail::CanRetainCapture(limit, 0), "retention cannot bypass pre-call headroom");
    Check(!probe::detail::CanRetainCapture(UINT64_MAX, UINT64_MAX), "budget arithmetic cannot underflow");
  }
  {
    Fake fake;
    const auto result = fake.Collect();
    Check(fake.captures == 91 && fake.waits == 91, "exactly ninety-one calls/waits");
    Check(result.readiness_observed == readiness && result.begin == readiness + 40 * frequency &&
          result.slots[0].before >= readiness + 40 * frequency, "no acquisition before fixed forty-second anchor");
    for (size_t i = 0; i < 91; ++i) {
      const auto& slot = result.slots[i];
      Check(slot.due == begin + i * 1000 && slot.before == slot.due && slot.after == slot.before + 50,
            "absolute schedule and external brackets");
      Check(slot.has_capture && std::string_view(slot.status) == "COLLECTED" && !slot.reason,
            "paired slot retained");
      Check(slot.paired.provenance.acquired_sequence == i + 1 && slot.paired.image.data[0] == i + 1,
            "pixel identity comes from paired result");
    }
  }
  {
    Fake fake; fake.duplicate = true;
    fake.modify = [](Fake& self, auto& output) { output.image.data[3] = uint8_t(self.captures); };
    const auto result = fake.Collect();
    probe::detail::ValidateBufferedRgb(result);
    Check(fake.captures == 91, "duplicates are not skipped or retried");
    for (const auto& slot : result.slots)
      Check(slot.has_capture && slot.paired.provenance.acquired_sequence == 1 &&
            slot.paired.provenance.refresh_cpu_begin_qpc == begin - 50,
            "duplicate keeps original publication despite X changes");
  }
  {
    Fake fake; fake.modify = [](Fake&, auto& output) { output.image.data[0] = 1; };
    const auto result = fake.Collect();
    Check(fake.captures == 91 && result.slots[90].has_capture, "distinct sequences with identical RGB allowed");
  }
  {
    Fake fake;
    fake.waiting = [](Fake& self, uint64_t due) { self.clock = due + 1000; };
    const auto result = fake.Collect();
    Check(fake.captures == 0 && fake.waits == 91, "all expired slots cause zero calls");
    for (const auto& slot : result.slots)
      Check(std::string_view(slot.status) == "MISSED_DEADLINE" && !slot.has_capture &&
            !slot.before && !slot.after && slot.paired.image.data.empty(), "miss has no stale capture");
  }
  {
    Fake fake;
    fake.modify = [](Fake& self, auto&) { if (self.captures == 1) self.clock = begin + 2150; };
    const auto result = fake.Collect();
    Check(fake.captures == 90 && std::string_view(result.slots[1].status) == "MISSED_DEADLINE",
          "slow call expires next slot without catch-up burst");
    Check(result.slots[2].before == begin + 2150, "next still-open slot is observed honestly late");
  }
  {
    Fake fake;
    fake.waiting = [](Fake& self, uint64_t due) { self.clock = due + ((self.waits % 2) ? 990 : 0); };
    fake.modify = [](Fake& self, auto& output) {
      self.clock = output.provenance.acquire_before_qpc + 1;
      auto& p = output.provenance;
      p.acquire_after_qpc = p.copy_submit_before_qpc = p.copy_completion_observed_qpc = p.pixels_ready_qpc = p.acquire_before_qpc;
    };
    const auto result = fake.Collect();
    Check(result.slots[1].before - result.slots[0].before == 10 &&
          result.slots[2].before - result.slots[1].before == 1990,
          "slot coverage does not imply uniform acquisition cadence");
  }
  for (const auto outcome : {rex::ui::GuestCaptureStatus::kFailed, rex::ui::GuestCaptureStatus::kUnsupported}) {
    Fake fake; fake.outcome = outcome;
    const auto result = fake.Collect();
    Check(fake.captures == 1 && std::string_view(result.slots[0].status) == "FAILED", "API failure is terminal");
    Check(!result.slots[0].has_capture && result.slots[0].paired.image.data.empty() &&
          result.slots[0].paired.provenance.acquired_sequence == 0, "failure clears malformed producer output too");
    for (size_t i = 1; i < 91; ++i)
      Check(std::string_view(result.slots[i].reason) == "PREVIOUS_TERMINAL_FAILURE" &&
            !result.slots[i].before, "failure suffix never retries");
  }
  {
    Fake fake; fake.cancellation.request_stop();
    const auto result = fake.Collect();
    Check(fake.captures == 0 && fake.waits == 0, "initial stop causes no wait or acquisition");
    Check(std::string_view(result.slots[0].reason) == "STOPPED" &&
          std::string_view(result.slots[90].reason) == "PREVIOUS_STOP", "stopped suffix explicit");
  }
  {
    Fake fake;
    fake.waiting = [](Fake& self, uint64_t) {
      self.clock = readiness + 10 * frequency;
      self.cancellation.request_stop();
    };
    const auto result = fake.Collect();
    Check(fake.waits == 1 && fake.captures == 0 && result.begin == begin,
          "stop during delay performs zero captures and does not move origin");
    Check(result.slots[0].observed < result.slots[0].due &&
          std::string_view(result.slots[0].reason) == "STOPPED", "stop before schedule is an honest terminal observation");
    for (size_t i = 1; i < 91; ++i)
      Check(std::string_view(result.slots[i].reason) == "PREVIOUS_STOP" && !result.slots[i].before,
            "delay-stop suffix never waits or captures");
  }
  {
    Fake fake;
    fake.waiting = [](Fake& self, uint64_t) {
      if (self.waits == 1) self.clock = begin + 2150;
    };
    const auto result = fake.Collect();
    Check(result.begin == begin && fake.captures == 89 &&
          std::string_view(result.slots[0].status) == "MISSED_DEADLINE" &&
          std::string_view(result.slots[1].status) == "MISSED_DEADLINE" &&
          result.slots[2].before == begin + 2150, "overslept initial delay does not reanchor or catch up");
  }
  {
    Fake fake; fake.waiting = [](Fake& self, uint64_t) { if (self.waits == 4) self.cancellation.request_stop(); };
    const auto result = fake.Collect();
    Check(fake.captures == 3 && std::string_view(result.slots[3].reason) == "STOPPED",
          "stop during wait preserves earlier observations without further calls");
  }
  {
    Fake fake; fake.modify = [](Fake& self, auto&) { self.cancellation.request_stop(); };
    const auto result = fake.Collect();
    Check(fake.captures == 1 && std::string_view(result.slots[0].reason) == "STOPPED_AFTER_CAPTURE" &&
          result.slots[0].paired.image.data.empty(), "stop after API clears unaccepted image");
  }
  {
    Fake fake; fake.modify = [](Fake&, auto&) { throw std::runtime_error("synthetic"); };
    const auto result = fake.Collect();
    Check(fake.captures == 1 && std::string_view(result.slots[0].reason) == "API_FAILED" &&
          result.slots[0].paired.image.data.empty(), "throwing callback is retained terminal failure");
  }
  for (unsigned mode = 0; mode < 8; ++mode) {
    Fake fake; fake.duplicate = true;
    fake.modify = [mode](Fake& self, auto& output) {
      if (self.captures != 2) return;
      auto& p = output.provenance;
      switch (mode) {
        case 0: ++p.presenter_epoch; break;
        case 1: p.acquired_sequence = 0; break;
        case 2: ++p.qpc_frequency; break;
        case 3: ++p.refresh_cpu_begin_qpc; break;
        case 4: ++output.image.data[0]; break;
        case 5: output.image.width = 1; break;
        case 6: p.pixels_ready_qpc = self.clock + 1; break;
        case 7: output.image.data.pop_back(); break;
      }
    };
    const auto result = fake.Collect();
    if (mode == 4) {
      Check(fake.captures == 91 && result.slots[90].has_capture, "pixel mismatch never selects collection length");
      Throws([&] { probe::detail::ValidateBufferedRgb(result); }, "DUPLICATE_RGB_CHANGED");
    } else {
      Check(fake.captures == 2 && result.slots[0].has_capture && !result.slots[1].has_capture &&
            std::string_view(result.slots[1].reason) == "METADATA_INVALID", "malformed identity/provenance terminal rejection");
    }
  }
  {
    Fake fake;
    Throws([&] { fake.Collect(0); }, "INVALID_SCHEDULE");
    Throws([&] { fake.Collect(frequency, uint64_t(INT64_MAX)); }, "INVALID_SCHEDULE");
    fake.waiting = [](Fake& self, uint64_t due) { self.clock = due - 1; };
    Throws([&] { fake.Collect(); }, "WAIT_RETURNED_EARLY");
    fake.waiting = [](Fake& self, uint64_t) { self.clock = readiness - 1; };
    Throws([&] { fake.Collect(); }, "CLOCK_REGRESSION_OR_INVALID");
  }
  {
    Fake fake;
    const auto result = fake.Collect(10003);
    Check(result.begin == readiness + 40 * 10003 && result.slots[90].due == result.begin + 90 * 10003 / 10,
          "nondivisible QPF has exact integer schedule without rounding drift");
  }
}

void ShortProfileTests() {
  using Profile = probe::detail::Profile;
  constexpr auto short_profile = Profile::kShort31;
  constexpr auto bad_profile = static_cast<Profile>(2);
  Check(probe::ParseProfile("legacy91_10hz") == Profile::kLegacy91 &&
        probe::ParseProfile("short31_10hz") == short_profile, "finite exact profile names");
  for (const auto* name : {"", "short31", "short31_20hz", "SHORT31_10HZ", "short31_10hz "})
    Throws([&] { probe::ParseProfile(name); }, "INVALID_SCOUT_PROFILE");
  Fake invalid;
  Throws([&] { invalid.Collect(frequency, readiness, bad_profile); }, "INVALID_SCOUT_PROFILE");
  Check(invalid.captures == 0 && invalid.waits == 0, "invalid profile rejected before operations");
  Fake fake;
  auto sequence = fake.Collect(frequency, readiness, short_profile);
  Check(fake.captures == 31 && fake.waits == 31 && sequence.slots.size() == 91,
        "short profile retains legacy backing capacity but makes exactly31 calls");
  for (size_t i = 0; i < 31; ++i)
    Check(sequence.slots[i].due == begin + i * frequency / 10 && sequence.slots[i].has_capture,
          "short exact100ms schedule and all31 acquisitions");
  for (size_t i = 31; i < 91; ++i)
    Check(sequence.slots[i].due == 0 && !sequence.slots[i].has_capture &&
          sequence.slots[i].paired.image.data.empty(), "unused backing slots untouched");
  probe::detail::ValidateBufferedRgb(sequence, short_profile);
  Throws([&] { probe::detail::ValidateBufferedRgb(sequence, bad_profile); }, "INVALID_SCOUT_PROFILE");
  auto json = [&](const auto& value, Profile profile) {
    return probe::Json("0123456789abcdef0123456789abcdef", "SOURCE_SCOUT_CENSUS_REVIEW_PENDING",
        "FIXED_DELAY_AND_SLOTS_NO_DISPLAY_SIMULATION_OR_TIMING_CLAIM",60,0,readiness,&value,profile);
  };
  const auto parsed = JsonValue::parse(json(sequence, short_profile));
  Check(parsed["schema"] == "mcla-guest-source-scout-short31-v1" &&
        parsed["maximum_raw_capture_bytes"] == 114278400 && parsed["scout_delay_seconds"] == 40 &&
        parsed["sequence"]["scheduled_slots"] == 31 && parsed["sequence"]["schedule_hz"] == 10 &&
        parsed["sequence"]["slots"].size() == 31 &&
        parsed["sequence"]["slots"][30]["image_file"] == "slot-30.bmp", "short exact wire contract");
  Throws([&] { json(sequence, Profile::kLegacy91); }, "SCOUT_PROFILE_MISMATCH");
  Throws([&] { json(sequence, bad_profile); }, "SCOUT_PROFILE_MISMATCH");
  Fake legacy; const auto long_sequence = legacy.Collect();
  Throws([&] { json(long_sequence, short_profile); }, "SCOUT_PROFILE_MISMATCH");
  Fake fractional;
  const auto fractional_sequence = fractional.Collect(10007, readiness, short_profile);
  Check(fractional.captures == 31 && fractional_sequence.slots[30].due == readiness + 43 * 10007,
        "short schedule uses exact absolute arithmetic with non-divisible frequency");
  Fake cancelled; cancelled.cancellation.request_stop();
  const auto cancelled_sequence = cancelled.Collect(frequency, readiness, short_profile);
  Check(cancelled.captures == 0 && cancelled.waits == 0 &&
        cancelled_sequence.slots[30].due == begin + 3 * frequency,
        "short cancellation records all31 slots without capture or retry");
  Fake failed; failed.outcome = rex::ui::GuestCaptureStatus::kFailed;
  const auto failed_sequence = failed.Collect(frequency, readiness, short_profile);
  Check(failed.captures == 1 && failed_sequence.slots[30].due == begin + 3 * frequency &&
        !failed_sequence.slots[30].has_capture, "short failure preserves complete31-slot terminal suffix without retry");
  Fake missed; missed.clock = begin + 31 * frequency / 10;
  const auto missed_sequence = missed.Collect(frequency, readiness, short_profile);
  Check(missed.captures == 0 && missed.waits == 31 &&
        std::string_view(missed_sequence.slots[30].status) == "MISSED_DEADLINE", "all31 missed slots retained");
  Fake duplicate; duplicate.duplicate = true;
  duplicate.modify = [](auto& self, auto& capture) { if (self.captures == 2) capture.image.data[0] ^= 1; };
  const auto duplicate_sequence = duplicate.Collect(frequency, readiness, short_profile);
  Check(duplicate.captures == 31, "short RGB failure never shortens collection");
  Throws([&] { probe::detail::ValidateBufferedRgb(duplicate_sequence, short_profile); }, "DUPLICATE_RGB_CHANGED");
}

void SerializationAndConfigTests() {
  Check(!probe::Enabled(), "default scout flag disabled");
  probe::ValidateExclusiveConfig("vulkan");
  Check(probe::ValidToken("0123456789abcdef0123456789abcdef") &&
        !probe::ValidToken("0123456789ABCDEF0123456789ABCDEF"), "token lowercase32hex");
  REXCVAR_SET(mcla_source_scout_probe, true);
  REXCVAR_SET(mcla_source_scout_token, "0123456789abcdef0123456789abcdef");
  probe::ValidateExclusiveConfig("d3d12");
  REXCVAR_SET(mcla_autostart_probe, true);
#if defined(MCLA_AUTOSTART_PROBE)
  probe::ValidateExclusiveConfig("d3d12");
  Check(true, "compiled navigation flag accepted by scout only");
  REXCVAR_SET(mcla_scout_test_peer_probe, true);
  Throws([] { probe::ValidateExclusiveConfig("d3d12"); }, "OTHER_PROBE_ENABLED_OR_INVALID");
  REXCVAR_SET(mcla_scout_test_peer_probe, false);
#else
  Throws([] { probe::ValidateExclusiveConfig("d3d12"); }, "OTHER_PROBE_ENABLED_OR_INVALID");
#endif
  REXCVAR_SET(mcla_autostart_probe, false);
  probe::ValidateRunConfig("0123456789abcdef0123456789abcdef", probe::detail::Profile::kLegacy91);
  REXCVAR_SET(mcla_source_scout_profile, "short31_10hz");
  probe::ValidateRunConfig("0123456789abcdef0123456789abcdef", probe::detail::Profile::kShort31);
  Throws([] { probe::ValidateRunConfig("0123456789abcdef0123456789abcdef", probe::detail::Profile::kLegacy91); }, "CONFIG_CHANGED");
#if defined(MCLA_TIMER_OBSERVER_PROBE)
  Throws([] { probe::ValidateExclusiveConfig("d3d12"); }, "SHORT_PROFILE_LEGACY_TIMER_UNSUPPORTED");
#else
  probe::ValidateExclusiveConfig("d3d12");
#endif
  REXCVAR_SET(mcla_source_scout_profile, "invalid");
  Throws([] { probe::ValidateExclusiveConfig("d3d12"); }, "INVALID_SCOUT_PROFILE");
  REXCVAR_SET(mcla_source_scout_profile, "legacy91_10hz");
  Throws([] { probe::ValidateExclusiveConfig("vulkan"); }, "D3D12_REQUIRED");
  REXCVAR_SET(mcla_scout_test_peer_probe, true);
  Throws([] { probe::ValidateExclusiveConfig("d3d12"); }, "OTHER_PROBE_ENABLED_OR_INVALID");
  REXCVAR_SET(mcla_scout_test_peer_probe, false);
  REXCVAR_SET(mcla_source_scout_probe, false);
  REXCVAR_SET(mcla_source_scout_token, "");
  Fake fake;
  auto sequence = fake.Collect();
  const auto value = JsonValue::parse(probe::Json("0123456789abcdef0123456789abcdef",
      "SOURCE_SCOUT_CENSUS_REVIEW_PENDING", "FIXED_DELAY_AND_SLOTS_NO_DISPLAY_SIMULATION_OR_TIMING_CLAIM", 60, 123, readiness, &sequence));
  Check(value.at("schema") == "mcla-guest-source-scout-v1" && value.at("sequence").at("slots").size() == 91,
        "separate bounded native schema");
  Check(value.at("readiness_observed_qpc") == readiness && value.at("scout_delay_seconds") == 40 &&
        value.at("diagnostic_buffer_limit_bytes") == 536870912 && value.at("maximum_raw_capture_bytes") == 335462400,
        "fixed anchor and diagnostic-only memory declarations");
  for (const auto* key : {"native_admitted", "display_proven", "simulation_proven", "fps_proven",
                         "scene_admitted", "cycle_timing_proven", "legacy_cycle_transfer_eligible",
                         "input_injected", "desktop_capture", "source_binding_independently_reviewed"})
    Check(value.at(key) == false, "receipt does not admit capture or timing");
  for (size_t i = 0; i < 91; ++i) {
    const auto& row = value.at("sequence").at("slots").at(i);
    Check(row.at("slot") == i && row.at("due_qpc") == begin + i * 1000 &&
          row.at("capture").at("acquired_sequence") == i + 1 && row.at("reason").is_null(),
          "slot journal and paired metadata serialize independently");
    Check(row.at("image_state") == "absent" && row.at("image_bytes_written") == 0,
          "collection alone does not claim saved files");
  }
  Check(probe::ImageName(0) == "slot-00.bmp" && probe::ImageName(90) == "slot-90.bmp", "fixed unique image names");
  Throws([] { probe::ImageName(91); }, "INVALID_SLOT_INDEX");
  Fake failed; failed.outcome = rex::ui::GuestCaptureStatus::kFailed;
  const auto partial = failed.Collect();
  const auto failed_json = JsonValue::parse(probe::Json("0123456789abcdef0123456789abcdef",
      "SOURCE_SCOUT_CENSUS_INCOMPLETE_REVIEW_PENDING", "FIXED_DELAY_AND_SLOTS_NO_DISPLAY_SIMULATION_OR_TIMING_CLAIM", 60, 123, readiness, &partial));
  Check(failed_json.at("sequence").at("slots").at(0).at("capture").is_null() &&
        failed_json.at("sequence").at("slots").at(1).at("call_before_qpc").is_null(),
        "failure and unattempted suffix cannot serialize stale capture");
  const auto absent = JsonValue::parse(probe::Json("", "FAILED", "READINESS_TIMEOUT", 0, 0, 0, nullptr));
  Check(absent.at("status") == "FAILED" && absent.at("sequence").is_null(), "precollection failure explicit");

  // Deliberately over-approximate every serialized integer/string width, even
  // combinations a valid census forbids. No image-sized buffers are allocated.
  // This proves the wire-size ceiling, not that this synthetic metadata is valid.
  auto worst = fake.Collect();
  worst.frequency = worst.begin = worst.readiness_observed = UINT64_MAX;
  worst.width = worst.height = UINT32_MAX;
  for (auto& slot : worst.slots) {
    slot.due = slot.observed = slot.before = slot.after = UINT64_MAX;
    slot.status = "NOT_ATTEMPTED"; slot.reason = "PREVIOUS_TERMINAL_FAILURE";
    slot.has_capture = true; slot.image_state = "complete"; slot.image_bytes = UINT64_MAX;
    slot.paired.image.width = slot.paired.image.height = UINT32_MAX;
    slot.paired.image.stride = SIZE_MAX;
    slot.paired.provenance = {UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX,
                             UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX, UINT64_MAX};
  }
  const auto maximum_json = probe::Json("0123456789abcdef0123456789abcdef",
      "SOURCE_SCOUT_CENSUS_INCOMPLETE_REVIEW_PENDING", "FIXED_DELAY_AND_SLOTS_NO_DISPLAY_SIMULATION_OR_TIMING_CLAIM",
      UINT64_MAX, UINT64_MAX, UINT64_MAX, &worst);
  Check(maximum_json.size() > 65536 && maximum_json.size() <= 128 * 1024,
        "worst-case receipt requires larger reader and fits128KiB");
  Check(JsonValue::parse(maximum_json).at("sequence").at("slots").size() == 91,
        "worst-case serializer still emits complete parseable journal");
  Check(probe::detail::kMaximumRawCaptureBytes == 335462400 &&
        probe::detail::kMaximumRawCaptureBytes + 64ull * 1024 * 1024 <= probe::detail::kBufferLimitBytes,
        "all accepted raw images plus maximum SDK transient below512MiB diagnostic budget");
  std::cout << "worst_case_scout_receipt_bytes=" << maximum_json.size() << '\n';
}
}  // namespace

int main() {
  try { SchedulerTests(); ShortProfileTests(); SerializationAndConfigTests(); TimerConfigAdmissionTests(); }
  catch (const std::exception& error) { std::cerr << "Unexpected test exception: " << error.what() << '\n'; return 2; }
  std::cout << "source scout no-GPU checks=" << checks << " failed=" << failures << '\n';
  return failures ? 1 : 0;
}
