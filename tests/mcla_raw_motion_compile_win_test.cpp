// Actual admission compiled four ways: neither/fixed/raw/both capabilities.
// Controller flags are deliberate stubs, including raw when unsupported, so an
// absent native raw capability cannot be bypassed by a registry entry.
#define main RetainedMotionConfigMain
#include "mcla_motion_config_win_test.cpp"
#undef main
#if !defined(MCLA_INPUT_RETURN_PROBE)
#error Census capability required for the matched delayed control.
#endif
namespace compile_stub { unsigned calls{}; }
namespace mcla::input_return {
bool Enabled() { ++compile_stub::calls; return false; }
void Enable() { ++compile_stub::calls; throw std::runtime_error("no lifecycle"); }
void Install(rex::Runtime&) { ++compile_stub::calls; throw std::runtime_error("no lifecycle"); }
void Begin(const std::string&, uint64_t) { ++compile_stub::calls; throw std::runtime_error("no lifecycle"); }
std::string Freeze(const std::string&) { ++compile_stub::calls; throw std::runtime_error("no lifecycle"); }
void Disarm() noexcept { ++compile_stub::calls; }
}
int main(int argc, char**) {
  try {
    Need(argc == 1, "no arguments or runtime needed");
    Need(Admits(), "default OFF preserved");
    REXCVAR_SET(mcla_autostart_probe, true);
    REXCVAR_SET(mcla_motion_calibration_probe, true);
    REXCVAR_SET(mcla_motion_config_probe, true);
    REXCVAR_SET(mcla_timer_config_probe, true);
    REXCVAR_SET(mcla_input_return_probe, true);
    REXCVAR_SET(mcla_autostart_token, stub::token);
    REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
    REXCVAR_SET(mcla_autostart_start_delay_ms, 8000u);
    Need(Admits(), "matched OFF available in every variant");
    REXCVAR_SET(mcla_raw_config_experiment, true);
#if defined(MCLA_RAW_CONFIG_EXPERIMENT)
    Need(Admits(), "compiled raw explicitly admitted");
#else
    Need(!Admits(), "unsupported true raw stub rejected");
#endif
    REXCVAR_SET(mcla_fixed60_config_experiment, true);
    Need(!Admits(), "raw plus fixed always rejected");
    REXCVAR_SET(mcla_raw_config_experiment, false);
    Need(Admits(), "existing fixed controller interface preserved");
    REXCVAR_SET(mcla_fixed60_config_experiment, false);
    REXCVAR_SET(mcla_raw_config_experiment, true);
    REXCVAR_SET(mcla_input_return_probe, false);
    Need(!Admits(), "raw delayed path requires census");
    REXCVAR_SET(mcla_raw_config_experiment, false);
    REXCVAR_SET(mcla_autostart_start_delay_ms, 0u);
    Need(Admits(), "legacy zero-delay OFF preserved");
    Need(!compile_stub::calls && !stub::installs && !stub::begins && !stub::freezes && !stub::disarms,
         "admission has no lifecycle effects");
    std::cout << "RAW_MOTION_COMPILE checks=" << checks << " failures=0\n";
    // Short-lived native variants have exited successfully with buffered output
    // missing. Completion is part of the test contract, not just exit status.
    std::cout.flush();
    return std::cout.good() ? 0 : 2;
  } catch (const std::exception& e) {
    std::cerr << "FAIL " << e.what() << '\n';
    std::cerr.flush();
    return 1;
  }
}
