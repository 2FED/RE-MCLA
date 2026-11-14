// Guest-free admission regression against the ACTUAL included ValidateConfig.
// No Run/Runtime/presenter/input/file IO. Retained fixtures provide inert
// controller stubs; none of their runtime mains or obsolete Registry is called.
// RED only: register the future flag so failure tests the old actual allowlist,
// not a missing symbol. GREEN must omit MCLA_ARCADE_CONFIG_RED.
#define main RetainedMotionConfigMain
#include "mcla_motion_config_win_test.cpp"
#undef main
#if !defined(MCLA_INPUT_RETURN_PROBE) || !defined(MCLA_RAW_CONFIG_EXPERIMENT) || \
    !defined(MCLA_FIXED60_CONFIG_EXPERIMENT) || !defined(MCLA_GUEST_SOURCE_SCOUT_PROBE) || \
    !defined(MCLA_UPDATE_TIMING_PROBE)
#error Compile with census, raw, fixed60, scout and update branches for admission coverage.
#endif
#if defined(MCLA_ARCADE_CONFIG_RED)
REXCVAR_DEFINE_BOOL(mcla_arcade_config_probe, false, "Test",
    "RED seam only: future explicit Arcade configuration opt-in")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
#endif

namespace arcade_config_stub { unsigned calls{}; }
namespace mcla::input_return {
bool Enabled() { ++arcade_config_stub::calls; return false; }
void Enable() { ++arcade_config_stub::calls; throw std::runtime_error("no census lifecycle"); }
void Install(rex::Runtime&) { ++arcade_config_stub::calls; throw std::runtime_error("no census install"); }
void Begin(const std::string&, uint64_t) { ++arcade_config_stub::calls; throw std::runtime_error("no census begin"); }
std::string Freeze(const std::string&) { ++arcade_config_stub::calls; throw std::runtime_error("no census cut"); }
void Disarm() noexcept { ++arcade_config_stub::calls; }
}
namespace {
unsigned arcade_checks{}, arcade_failures{};
void Check(bool value, const char* label) {
  ++arcade_checks;
  if (!value) { ++arcade_failures; std::cerr << "FAIL " << label << '\n'; }
}
void SetOptIn(bool value) {
  const auto* flag = rex::cvar::GetFlagInfo("mcla_arcade_config_probe");
  if (!flag || !flag->setter || !flag->setter(value ? "true" : "false"))
    throw std::runtime_error("explicit Arcade configuration flag is unavailable");
}
void Config(bool raw = false) {
  SetOptIn(true);
  REXCVAR_SET(mcla_autostart_probe, true);
  REXCVAR_SET(mcla_arcade_navigation_probe, true);
  REXCVAR_SET(mcla_motion_calibration_probe, true);
  REXCVAR_SET(mcla_motion_config_probe, true);
  REXCVAR_SET(mcla_timer_config_probe, true);
  REXCVAR_SET(mcla_fixed60_config_experiment, false);
  REXCVAR_SET(mcla_raw_config_experiment, raw);
  REXCVAR_SET(mcla_input_return_probe, false);
  REXCVAR_SET(mcla_source_scout_probe, false);
  REXCVAR_SET(mcla_update_timing_probe, false);
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
    const auto* flag = rex::cvar::GetFlagInfo("mcla_arcade_config_probe");
    Check(flag && flag->getter && flag->setter &&
          flag->type == rex::cvar::FlagType::Boolean &&
          flag->default_value == "false" && flag->getter() == "false" &&
          flag->lifecycle == rex::cvar::Lifecycle::kInitOnly,
          "Arcade configuration is explicit default-OFF init-only boolean");
    Check(Admits(), "all defaults retain admission");
    for (bool raw : {false, true}) {
      Config(raw);
      Check(Admits(), raw ? "explicit raw Arcade configuration admitted" :
                           "explicit stock Arcade configuration admitted");
      SetOptIn(false);
      Check(!Admits(), "legacy Arcade rejects configuration without new opt-in");
      Config(raw); REXCVAR_SET(mcla_autostart_probe, false);
      Check(!Admits(), "new mode requires autostart");
      Config(raw); REXCVAR_SET(mcla_arcade_navigation_probe, false);
      Check(!Admits(), "new mode requires Arcade navigation");
      Config(raw); REXCVAR_SET(mcla_motion_calibration_probe, false);
      Check(!Admits(), "new mode requires motion collection");
      Config(raw); REXCVAR_SET(mcla_motion_config_probe, false);
      Check(!Admits(), "new mode requires configuration lifecycle");
      Config(raw); REXCVAR_SET(mcla_timer_config_probe, false);
      Check(!Admits(), "new mode requires timer configuration observer");
      Config(raw); REXCVAR_SET(mcla_motion_calibration_trigger, 96u);
      Check(!Admits(), "new mode forbids RT96");
      Config(raw); REXCVAR_SET(mcla_arcade_sparse_capture, true);
      Check(!Admits(), "new mode forbids sparse capture");
      Config(raw); REXCVAR_SET(mcla_autostart_start_delay_ms, 8000u);
      Check(!Admits(), "new mode forbids delayed START");
      Config(raw); REXCVAR_SET(mcla_input_return_probe, true);
      Check(!Admits(), "new mode forbids input-return census");
      Config(raw); REXCVAR_SET(mcla_fixed60_config_experiment, true);
      Check(!Admits(), "new mode forbids fixed60 even with compile support");
      Config(raw); REXCVAR_SET(mcla_source_scout_probe, true);
      Check(!Admits(), "new mode forbids stationary scout");
      Config(raw); REXCVAR_SET(mcla_update_timing_probe, true);
      Check(!Admits(), "new mode forbids update timing");
      Config(raw); REXCVAR_SET(mcla_motion_unknown_probe, true);
      Check(!Admits(), "new mode forbids unknown probe");
      Config(raw); REXCVAR_SET(mcla_motion_unknown_experiment, true);
      Check(!Admits(), "new mode forbids unknown experiment");
      Config(raw); REXCVAR_SET(mcla_autostart_token, "INVALID");
      Check(!Admits(), "new mode retains exact token guard");
      Config(raw); REXCVAR_SET(mcla_motion_calibration_trigger, 254u);
      Check(!Admits(), "new mode retains exact amplitude guard");
      Config(raw); REXCVAR_SET(mcla_autostart_start_delay_ms, 1u);
      Check(!Admits(), "new mode retains exact delay guard");
    }
    Config(); SetOptIn(false);
    REXCVAR_SET(mcla_motion_config_probe, false);
    REXCVAR_SET(mcla_timer_config_probe, false);
    Check(Admits(), "legacy dense RT255 Arcade remains admitted");
    REXCVAR_SET(mcla_arcade_sparse_capture, true);
    Check(Admits(), "legacy sparse RT255 Arcade remains admitted");
    REXCVAR_SET(mcla_arcade_sparse_capture, false);
    REXCVAR_SET(mcla_motion_calibration_trigger, 96u);
    Check(Admits(), "legacy dense RT96 Arcade remains admitted");
    Config(); SetOptIn(false); REXCVAR_SET(mcla_arcade_navigation_probe, false);
    Check(Admits(), "legacy standalone stock configuration remains admitted");
    REXCVAR_SET(mcla_raw_config_experiment, true);
    Check(Admits(), "legacy standalone raw configuration remains admitted");
    REXCVAR_SET(mcla_raw_config_experiment, false);
    REXCVAR_SET(mcla_fixed60_config_experiment, true);
    Check(Admits(), "legacy standalone fixed60 configuration remains admitted");
    Check(!arcade_config_stub::calls && !stub::installs && !stub::begins &&
          !stub::freezes && !stub::disarms,
          "admission invokes no controller/census lifecycle");
    std::cout << "ARCADE_CONFIG_ADMISSION checks=" << arcade_checks
              << " failures=" << arcade_failures << '\n';
    std::cout.flush();
    std::cerr.flush();
    if (!std::cout.good() || !std::cerr.good()) return 2;
    return arcade_failures ? 1 : 0;
  } catch (const std::exception& e) {
    std::cerr << "FAIL unexpected admission exception: " << e.what() << '\n';
    std::cerr.flush();
    if (!std::cerr.good()) return 2;
    return 1;
  }
}
