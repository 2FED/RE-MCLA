// Guest-free regression against actual ValidateConfig. This main never calls
// Run, creates files, constructs a runtime, or injects input. The retained
// fixture supplies controller stubs; its obsolete raw-rejection Registry is
// deliberately NOT called. Production admission is included by that fixture.
#define main RetainedMotionConfigMain
#include "mcla_motion_config_win_test.cpp"
#undef main
#if !defined(MCLA_INPUT_RETURN_PROBE) || !defined(MCLA_RAW_CONFIG_EXPERIMENT) || !defined(MCLA_FIXED60_CONFIG_EXPERIMENT)
#error Compile this test with census, raw and fixed60 support explicitly enabled.
#endif

namespace raw_motion_stub { unsigned calls{}; }
namespace mcla::input_return {
bool Enabled() { ++raw_motion_stub::calls; return false; }
void Enable() { ++raw_motion_stub::calls; throw std::runtime_error("no census lifecycle"); }
void Install(rex::Runtime&) { ++raw_motion_stub::calls; throw std::runtime_error("no runtime installation"); }
void Begin(const std::string&, uint64_t) { ++raw_motion_stub::calls; throw std::runtime_error("no census Begin"); }
std::string Freeze(const std::string&) { ++raw_motion_stub::calls; throw std::runtime_error("no census Freeze"); }
void Disarm() noexcept { ++raw_motion_stub::calls; }
}

namespace {
unsigned raw_checks{}, raw_failures{};
void CheckRaw(bool value, const char* label) {
  ++raw_checks;
  if (!value) { ++raw_failures; std::cerr << "FAIL " << label << '\n'; }
}
void RawConfig() {
  REXCVAR_SET(mcla_autostart_probe, true);
  REXCVAR_SET(mcla_motion_calibration_probe, true);
  REXCVAR_SET(mcla_motion_config_probe, true);
  REXCVAR_SET(mcla_timer_config_probe, true);
  REXCVAR_SET(mcla_fixed60_config_experiment, false);
  REXCVAR_SET(mcla_raw_config_experiment, true);
  REXCVAR_SET(mcla_input_return_probe, true);
  REXCVAR_SET(mcla_source_scout_probe, false);
  REXCVAR_SET(mcla_update_timing_probe, false);
  REXCVAR_SET(mcla_motion_unknown_probe, false);
  REXCVAR_SET(mcla_motion_unknown_experiment, false);
  REXCVAR_SET(mcla_autostart_token, stub::token);
  REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
  REXCVAR_SET(mcla_autostart_start_delay_ms, 8000u);
}
}

int main(int argc, char**) {
  if (argc != 1) return 2;
  try {
    CheckRaw(!a::MotionConfigEnabled() && Admits(), "default OFF remains admitted");
    const auto* delay = rex::cvar::GetFlagInfo("mcla_autostart_start_delay_ms");
    CheckRaw(delay && delay->getter && delay->default_value == "0" &&
             delay->getter() == "0" && delay->lifecycle == rex::cvar::Lifecycle::kInitOnly,
             "delay remains default-zero and init-only");

    RawConfig();
    CheckRaw(Admits(), "standalone raw plus census admits the same 8000-ms START delay");
    REXCVAR_SET(mcla_fixed60_config_experiment, true);
    CheckRaw(!Admits(), "raw and fixed60 are mutually exclusive");
    REXCVAR_SET(mcla_raw_config_experiment, false);
    CheckRaw(Admits(), "fixed60 with census remains admitted");
    REXCVAR_SET(mcla_input_return_probe, false);
    CheckRaw(Admits(), "legacy fixed60 delay without census remains admitted");
    REXCVAR_SET(mcla_fixed60_config_experiment, false);
    CheckRaw(!Admits(), "OFF without census rejects nonzero delay");
    REXCVAR_SET(mcla_input_return_probe, true);
    CheckRaw(Admits(), "matched OFF with census remains admitted");
    REXCVAR_SET(mcla_autostart_start_delay_ms, 0u);
    CheckRaw(Admits(), "zero-delay OFF with census remains admitted");
    REXCVAR_SET(mcla_input_return_probe, false);
    CheckRaw(Admits(), "legacy zero-delay OFF remains admitted");
    REXCVAR_SET(mcla_fixed60_config_experiment, true);
    CheckRaw(Admits(), "legacy zero-delay fixed60 remains admitted");

    RawConfig(); REXCVAR_SET(mcla_input_return_probe, false);
    CheckRaw(!Admits(), "raw cannot borrow fixed60 delayed admission without census");
    RawConfig(); REXCVAR_SET(mcla_autostart_probe, false);
    CheckRaw(!Admits(), "raw motion requires autostart");
    RawConfig(); REXCVAR_SET(mcla_motion_calibration_probe, false);
    CheckRaw(!Admits(), "raw motion requires motion calibration");
    RawConfig(); REXCVAR_SET(mcla_motion_config_probe, false);
    CheckRaw(!Admits(), "raw motion requires standalone configuration mode");
    RawConfig(); REXCVAR_SET(mcla_timer_config_probe, false);
    CheckRaw(!Admits(), "raw motion requires timer inventory");
    RawConfig(); REXCVAR_SET(mcla_source_scout_probe, true);
    CheckRaw(!Admits(), "stationary scout remains forbidden");
    RawConfig(); REXCVAR_SET(mcla_update_timing_probe, true);
    CheckRaw(!Admits(), "update timing remains forbidden");
    RawConfig(); REXCVAR_SET(mcla_motion_unknown_probe, true);
    CheckRaw(!Admits(), "unknown probe remains forbidden");
    RawConfig(); REXCVAR_SET(mcla_motion_unknown_experiment, true);
    CheckRaw(!Admits(), "unknown experiment remains forbidden");
    RawConfig(); REXCVAR_SET(mcla_autostart_token, "invalid");
    CheckRaw(!Admits(), "invalid token remains forbidden");
    for (uint32_t invalid : {1u, 7999u, 8001u, UINT32_MAX}) {
      RawConfig(); REXCVAR_SET(mcla_autostart_start_delay_ms, invalid);
      CheckRaw(!Admits(), "raw does not relax exact delay values");
    }
    RawConfig(); REXCVAR_SET(mcla_motion_calibration_trigger, 254u);
    CheckRaw(!Admits(), "raw does not relax trigger amplitudes");
    CheckRaw(!raw_motion_stub::calls && !stub::installs && !stub::begins &&
             !stub::freezes && !stub::disarms,
             "admission never invokes controller or census lifecycle");
    std::cout << "RAW_MOTION_ADMISSION checks=" << raw_checks
              << " failures=" << raw_failures << '\n';
    return raw_failures ? 1 : 0;
  } catch (const std::exception& e) {
    std::cerr << "FAIL unexpected test exception: " << e.what() << '\n';
    return 1;
  }
}
