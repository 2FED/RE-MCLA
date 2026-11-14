// Guest-free admission regression against actual ValidateConfig. No Run,
// Runtime, file IO, input driver or presenter is invoked by this main.
// The retained fixture supplies controller stubs and legacy admission checks;
// census lifecycle functions below are link stubs that must remain uncalled.
#define main RetainedMotionConfigMain
#include "mcla_motion_config_win_test.cpp"
#undef main
#if !defined(MCLA_INPUT_RETURN_PROBE)
#error This test requires the explicit input-return census build option.
#endif

namespace pair_census_stub { unsigned calls{}; }
namespace mcla::input_return {
bool Enabled() { ++pair_census_stub::calls; return false; }
void Enable() { ++pair_census_stub::calls; throw std::runtime_error("no census lifecycle in admission test"); }
void Install(rex::Runtime&) { ++pair_census_stub::calls; throw std::runtime_error("no runtime installation in admission test"); }
void Begin(const std::string&, uint64_t) { ++pair_census_stub::calls; throw std::runtime_error("no census Begin in admission test"); }
std::string Freeze(const std::string&) { ++pair_census_stub::calls; throw std::runtime_error("no census Freeze in admission test"); }
void Disarm() noexcept { ++pair_census_stub::calls; }
}

namespace {
unsigned pair_checks{}, pair_failures{};
void PairCheck(bool value, const char* label) {
  ++pair_checks;
  if (!value) { ++pair_failures; std::cerr << "FAIL " << label << '\n'; }
}
void PairConfig() {
  REXCVAR_SET(mcla_autostart_probe, true);
  REXCVAR_SET(mcla_motion_calibration_probe, true);
  REXCVAR_SET(mcla_motion_config_probe, true);
  REXCVAR_SET(mcla_timer_config_probe, true);
  REXCVAR_SET(mcla_fixed60_config_experiment, false);
  REXCVAR_SET(mcla_raw_config_experiment, false);
  REXCVAR_SET(mcla_source_scout_probe, false);
  REXCVAR_SET(mcla_update_timing_probe, false);
  REXCVAR_SET(mcla_motion_unknown_probe, false);
  REXCVAR_SET(mcla_motion_unknown_experiment, false);
  REXCVAR_SET(mcla_input_return_probe, true);
  REXCVAR_SET(mcla_autostart_token, stub::token);
  REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
  REXCVAR_SET(mcla_autostart_start_delay_ms, 8000u);
}
}

int main(int argc, char**) {
  if (argc != 1) return 2;
  try {
    // Existing zero-delay/default-OFF and fixed60 restrictions remain covered.
    // Census is still false throughout these retained checks.
    Registry();
    PairConfig();
    PairCheck(Admits(), "OFF plus census admits the same 8000-ms START delay");

    REXCVAR_SET(mcla_input_return_probe, false);
    PairCheck(!Admits(), "OFF without census still rejects nonzero delay");
    REXCVAR_SET(mcla_fixed60_config_experiment, true);
    PairCheck(Admits(), "existing fixed60 delayed path without census preserved");
    REXCVAR_SET(mcla_input_return_probe, true);
    PairCheck(Admits(), "fixed60 delayed path with census preserved");

    PairConfig(); REXCVAR_SET(mcla_autostart_probe, false);
    PairCheck(!Admits(), "census OFF control requires autostart");
    PairConfig(); REXCVAR_SET(mcla_motion_calibration_probe, false);
    PairCheck(!Admits(), "census OFF control requires motion");
    PairConfig(); REXCVAR_SET(mcla_motion_config_probe, false);
    PairCheck(!Admits(), "census OFF control requires standalone configuration mode");
    PairConfig(); REXCVAR_SET(mcla_timer_config_probe, false);
    PairCheck(!Admits(), "census OFF control requires timer inventory");
    PairConfig(); REXCVAR_SET(mcla_raw_config_experiment, true);
    PairCheck(!Admits(), "raw experiment remains forbidden");
    PairConfig(); REXCVAR_SET(mcla_source_scout_probe, true);
    PairCheck(!Admits(), "stationary scout remains forbidden");
    PairConfig(); REXCVAR_SET(mcla_update_timing_probe, true);
    PairCheck(!Admits(), "update observer remains forbidden");
    PairConfig(); REXCVAR_SET(mcla_motion_unknown_probe, true);
    PairCheck(!Admits(), "unknown probe remains forbidden");
    PairConfig(); REXCVAR_SET(mcla_motion_unknown_experiment, true);
    PairCheck(!Admits(), "unknown experiment remains forbidden");
    PairConfig(); REXCVAR_SET(mcla_autostart_token, "invalid");
    PairCheck(!Admits(), "invalid token remains forbidden");
    for (uint32_t invalid : {1u, 7999u, 8001u, UINT32_MAX}) {
      PairConfig(); REXCVAR_SET(mcla_autostart_start_delay_ms, invalid);
      PairCheck(!Admits(), "census does not relax exact delay values");
    }
    PairConfig(); REXCVAR_SET(mcla_autostart_start_delay_ms, 0u);
    PairCheck(Admits(), "zero-delay census OFF control preserved");
    REXCVAR_SET(mcla_input_return_probe, false);
    PairCheck(Admits(), "zero-delay standalone OFF path without census preserved");
    PairCheck(!pair_census_stub::calls && !stub::installs && !stub::begins &&
              !stub::freezes && !stub::disarms,
              "admission never invokes census or configuration lifecycle");
    std::cout << "INPUT_RETURN_PAIR_ADMISSION checks=" << checks + pair_checks
              << " failures=" << pair_failures << '\n';
    return pair_failures ? 1 : 0;
  } catch (const std::exception& e) {
    std::cerr << "FAIL retained admission check: " << e.what() << '\n';
    return 1;
  }
}
