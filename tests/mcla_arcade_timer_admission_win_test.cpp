// Guest-free actual ValidateConfig regression; NO lifecycle/runtime/main calls.
// RED: omit MCLA_ARCADE_TIMER_PROBE, retaining the current production source.
// GREEN: define MCLA_ARCADE_TIMER_PROBE after its native implementation exists.
// The timer flag is registered HERE in BOTH variants because its production
// definition lives in the excluded observer module. This tests admission, NOT
// that module's production default/lifecycle registration (test it separately).
// Run the unchanged old 47-case Arcade configuration test separately as well.
#define main RetainedMotionConfigMain
#include "mcla_motion_config_win_test.cpp"
#undef main
#include "mcla_timer_journal.h"
#include <functional>
#include <vector>

#if !defined(MCLA_INPUT_RETURN_PROBE) || !defined(MCLA_RAW_CONFIG_EXPERIMENT) || \
    !defined(MCLA_FIXED60_CONFIG_EXPERIMENT) || !defined(MCLA_GUEST_SOURCE_SCOUT_PROBE) || \
    !defined(MCLA_UPDATE_TIMING_PROBE)
#error Compile the census/raw/fixed60/scout/update admission branches.
#endif
REXCVAR_DEFINE_BOOL(mcla_arcade_timer_probe, false, "Test",
    "Fixture registration only; actual production module is not linked")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(mcla_timer_observer_probe, false, "Test",
    "Forbidden legacy timer observer; no module linked")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace arcade_timer_admission_stub { unsigned lifecycle_calls{}; }
namespace mcla::input_return {
bool Enabled() { return REXCVAR_GET(mcla_input_return_probe); }
void Enable() { ++arcade_timer_admission_stub::lifecycle_calls; throw std::runtime_error("no census enable"); }
void Install(rex::Runtime&) { ++arcade_timer_admission_stub::lifecycle_calls; throw std::runtime_error("no census install"); }
void Begin(const std::string&, uint64_t) { ++arcade_timer_admission_stub::lifecycle_calls; throw std::runtime_error("no census begin"); }
std::string Freeze(const std::string&) { ++arcade_timer_admission_stub::lifecycle_calls; throw std::runtime_error("no census cut"); }
void Disarm() noexcept { ++arcade_timer_admission_stub::lifecycle_calls; }
}
#if defined(MCLA_ARCADE_TIMER_PROBE)
// Signatures match the new module API; no real module, observer or original is
// linked/called. The included production autostart sees its real header.
namespace mcla::arcade_timer {
bool Enabled() { return REXCVAR_GET(mcla_arcade_timer_probe); }
void Install(rex::Runtime&) { ++arcade_timer_admission_stub::lifecycle_calls; throw std::runtime_error("no timer install"); }
void Begin(uint64_t, uint64_t, uint64_t, const std::string&) {
  ++arcade_timer_admission_stub::lifecycle_calls; throw std::runtime_error("no timer begin");
}
timer_journal::Window ObservationWindow(const std::string&) {
  ++arcade_timer_admission_stub::lifecycle_calls; throw std::runtime_error("no timer window");
}
std::string Finish(std::stop_token, const std::string&) {
  ++arcade_timer_admission_stub::lifecycle_calls; throw std::runtime_error("no timer finish");
}
void Disarm() noexcept { ++arcade_timer_admission_stub::lifecycle_calls; }
void Invoke(PPCFunc*, PPCContext&, uint8_t*) {
  ++arcade_timer_admission_stub::lifecycle_calls; throw std::runtime_error("no timer invocation");
}
}
#endif
namespace {
unsigned timer_checks{}, timer_failures{};
void CheckTimer(bool value, const char* label) {
  ++timer_checks;
  if (!value) { ++timer_failures; std::cerr << "FAIL " << label << '\n'; }
}
void TimerConfig(bool raw = false) {
  REXCVAR_SET(mcla_arcade_timer_probe, true);
  REXCVAR_SET(mcla_arcade_config_probe, true);
  REXCVAR_SET(mcla_autostart_probe, true);
  REXCVAR_SET(mcla_arcade_navigation_probe, true);
  REXCVAR_SET(mcla_motion_calibration_probe, true);
  REXCVAR_SET(mcla_motion_config_probe, true);
  REXCVAR_SET(mcla_timer_config_probe, true);
  REXCVAR_SET(mcla_raw_config_experiment, raw);
  REXCVAR_SET(mcla_fixed60_config_experiment, false);
  REXCVAR_SET(mcla_input_return_probe, false);
  REXCVAR_SET(mcla_source_scout_probe, false);
  REXCVAR_SET(mcla_update_timing_probe, false);
  REXCVAR_SET(mcla_timer_observer_probe, false);
  REXCVAR_SET(mcla_motion_unknown_probe, false);
  REXCVAR_SET(mcla_motion_unknown_experiment, false);
  REXCVAR_SET(mcla_autostart_token, stub::token);
  REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
  REXCVAR_SET(mcla_autostart_start_delay_ms, 0u);
  REXCVAR_SET(mcla_arcade_sparse_capture, false);
}
}
int main(int argc, char**) {
  if (argc != 1) return 2;
  try {
    const auto* flag = rex::cvar::GetFlagInfo("mcla_arcade_timer_probe");
    CheckTimer(flag && flag->getter && flag->setter &&
        flag->type == rex::cvar::FlagType::Boolean &&
        flag->default_value == "false" && flag->getter() == "false" &&
        flag->lifecycle == rex::cvar::Lifecycle::kInitOnly,
        "fixture opt-in is default-OFF init-only (not production registration proof)");
    CheckTimer(Admits(), "all-default configuration remains admitted");
    struct Negative { const char* label; std::function<void()> mutate; };
    const std::vector<Negative> negatives{
      {"requires Arcade configuration", [] { REXCVAR_SET(mcla_arcade_config_probe, false); }},
      {"requires autostart", [] { REXCVAR_SET(mcla_autostart_probe, false); }},
      {"requires Arcade navigation", [] { REXCVAR_SET(mcla_arcade_navigation_probe, false); }},
      {"requires motion", [] { REXCVAR_SET(mcla_motion_calibration_probe, false); }},
      {"requires motion configuration", [] { REXCVAR_SET(mcla_motion_config_probe, false); }},
      {"requires timer configuration", [] { REXCVAR_SET(mcla_timer_config_probe, false); }},
      {"forbids RT96", [] { REXCVAR_SET(mcla_motion_calibration_trigger, 96u); }},
      {"forbids RT254", [] { REXCVAR_SET(mcla_motion_calibration_trigger, 254u); }},
      {"forbids sparse capture", [] { REXCVAR_SET(mcla_arcade_sparse_capture, true); }},
      {"forbids 1ms delay", [] { REXCVAR_SET(mcla_autostart_start_delay_ms, 1u); }},
      {"forbids 8000ms delay", [] { REXCVAR_SET(mcla_autostart_start_delay_ms, 8000u); }},
      {"forbids fixed60", [] { REXCVAR_SET(mcla_fixed60_config_experiment, true); }},
      {"forbids census", [] { REXCVAR_SET(mcla_input_return_probe, true); }},
      {"forbids scout", [] { REXCVAR_SET(mcla_source_scout_probe, true); }},
      {"forbids update timing", [] { REXCVAR_SET(mcla_update_timing_probe, true); }},
      {"forbids legacy timer", [] { REXCVAR_SET(mcla_timer_observer_probe, true); }},
      {"forbids unknown probe", [] { REXCVAR_SET(mcla_motion_unknown_probe, true); }},
      {"forbids unknown experiment", [] { REXCVAR_SET(mcla_motion_unknown_experiment, true); }},
      {"rejects empty token", [] { REXCVAR_SET(mcla_autostart_token, ""); }},
      {"rejects zero token", [] { REXCVAR_SET(mcla_autostart_token, "00000000000000000000000000000000"); }},
      {"rejects uppercase token", [] { REXCVAR_SET(mcla_autostart_token, "E69A68CD28E641F5A1E8E92D720F1068"); }},
      {"rejects short token", [] { REXCVAR_SET(mcla_autostart_token, "e69a68cd28e641f5a1e8e92d720f106"); }},
      {"rejects long token", [] { REXCVAR_SET(mcla_autostart_token, "e69a68cd28e641f5a1e8e92d720f10680"); }},
    };
    for (bool raw : {false, true}) {
      TimerConfig(raw);
      CheckTimer(Admits(), raw ? "explicit raw Arcade timer admitted" : "explicit stock Arcade timer admitted");
      for (const auto& negative : negatives) {
        TimerConfig(raw); negative.mutate(); CheckTimer(!Admits(), negative.label);
      }
    }
    for (bool raw : {false, true}) {
      TimerConfig(raw); REXCVAR_SET(mcla_arcade_timer_probe, false);
      CheckTimer(Admits(), raw ? "timer OFF preserves raw Arcade configuration" :
                               "timer OFF preserves stock Arcade configuration");
    }
    TimerConfig(); REXCVAR_SET(mcla_arcade_timer_probe, false);
    REXCVAR_SET(mcla_arcade_config_probe, false);
    REXCVAR_SET(mcla_motion_config_probe, false);
    REXCVAR_SET(mcla_timer_config_probe, false);
    CheckTimer(Admits(), "timer OFF preserves legacy dense RT255");
    REXCVAR_SET(mcla_arcade_sparse_capture, true);
    CheckTimer(Admits(), "timer OFF preserves legacy sparse RT255");
    REXCVAR_SET(mcla_arcade_sparse_capture, false);
    REXCVAR_SET(mcla_motion_calibration_trigger, 96u);
    CheckTimer(Admits(), "timer OFF preserves legacy dense RT96");
    TimerConfig(); REXCVAR_SET(mcla_arcade_timer_probe, false);
    REXCVAR_SET(mcla_arcade_config_probe, false);
    REXCVAR_SET(mcla_arcade_navigation_probe, false);
    CheckTimer(Admits(), "timer OFF preserves standalone stock configuration");
    REXCVAR_SET(mcla_raw_config_experiment, true);
    CheckTimer(Admits(), "timer OFF preserves standalone raw configuration");
    REXCVAR_SET(mcla_raw_config_experiment, false);
    REXCVAR_SET(mcla_fixed60_config_experiment, true);
    CheckTimer(Admits(), "timer OFF preserves standalone fixed60 configuration");
    CheckTimer(!arcade_timer_admission_stub::lifecycle_calls && !stub::installs &&
        !stub::begins && !stub::freezes && !stub::disarms,
        "admission called no timer/config/census lifecycle or original");
    std::cout << "ARCADE_TIMER_ADMISSION checks=" << timer_checks
              << " failures=" << timer_failures << '\n';
    std::cout.flush(); std::cerr.flush();
    if (!std::cout.good() || !std::cerr.good()) return 2;
    return timer_failures ? 1 : 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL unexpected admission exception: " << error.what() << '\n';
    std::cerr.flush();
    return std::cerr.good() ? 1 : 2;
  }
}
