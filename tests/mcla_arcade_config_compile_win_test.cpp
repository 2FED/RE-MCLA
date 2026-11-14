// Guest-free compile-capability matrix against actual included ValidateConfig.
// Build four separate executables with neither/FIXED60/RAW/both experiment
// macros; keep AUTOSTART/MOTION/MOTION_CONFIG/TIMER_CONFIG and INPUT_RETURN,
// SCOUT, UPDATE support common. No arguments, IO, Run, Runtime or input.
// The retained fixture defines inert controller flag stubs even where the
// production build would not expose that experiment's flag. Setting the fixed60
// stub below deliberately checks unconditional rejection, not feature presence.
#define main RetainedMotionConfigMain
#include "mcla_motion_config_win_test.cpp"
#undef main
#if !defined(MCLA_INPUT_RETURN_PROBE) || !defined(MCLA_GUEST_SOURCE_SCOUT_PROBE) || \
    !defined(MCLA_UPDATE_TIMING_PROBE)
#error Keep census/scout/update branches enabled in all four capability variants.
#endif
namespace arcade_compile_stub { unsigned calls{}; }
namespace mcla::input_return {
bool Enabled() { ++arcade_compile_stub::calls; return false; }
void Enable() { ++arcade_compile_stub::calls; throw std::runtime_error("no census enable"); }
void Install(rex::Runtime&) { ++arcade_compile_stub::calls; throw std::runtime_error("no census install"); }
void Begin(const std::string&, uint64_t) { ++arcade_compile_stub::calls; throw std::runtime_error("no census begin"); }
std::string Freeze(const std::string&) { ++arcade_compile_stub::calls; throw std::runtime_error("no census cut"); }
void Disarm() noexcept { ++arcade_compile_stub::calls; }
}
namespace {
constexpr bool kRaw =
#if defined(MCLA_RAW_CONFIG_EXPERIMENT)
    true;
#else
    false;
#endif
constexpr bool kFixed =
#if defined(MCLA_FIXED60_CONFIG_EXPERIMENT)
    true;
#else
    false;
#endif
unsigned compile_checks{}, compile_failures{};
void Check(bool value, const char* label) {
  ++compile_checks;
  if (!value) { ++compile_failures; std::cerr << "FAIL " << label << '\n'; }
}
void OptIn(bool value) {
  const auto* flag = rex::cvar::GetFlagInfo("mcla_arcade_config_probe");
  if (!flag || !flag->setter || !flag->setter(value ? "true" : "false"))
    throw std::runtime_error("new flag missing in compile variant");
}
void Configure(bool raw = false) {
  OptIn(true);
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
          "new mode is default-OFF init-only in every compile variant");
    Check(Admits(), "all defaults retain admission");
    for (bool raw : {false, true}) {
      Configure(raw);
      Check(Admits() == kRaw, raw ? "raw opt-in requires RAW compile capability" :
                                  "stock opt-in also requires RAW compile capability");
      REXCVAR_SET(mcla_fixed60_config_experiment, true);
      Check(!Admits(), "fixed60 enable always rejected by new mode");
      Configure(raw); OptIn(false);
      Check(!Admits(), "legacy Arcade still rejects configuration without opt-in");
      Configure(raw); REXCVAR_SET(mcla_arcade_navigation_probe, false);
      Check(!Admits(), "new mode cannot borrow standalone admission");
      Configure(raw); REXCVAR_SET(mcla_motion_config_probe, false);
      Check(!Admits(), "new mode needs standalone controller lifecycle");
      Configure(raw); REXCVAR_SET(mcla_arcade_sparse_capture, true);
      Check(!Admits(), "new mode cannot borrow sparse admission");
    }
    Configure(); OptIn(false);
    REXCVAR_SET(mcla_motion_config_probe, false);
    REXCVAR_SET(mcla_timer_config_probe, false);
    Check(Admits(), "legacy dense Arcade remains admitted");
    REXCVAR_SET(mcla_arcade_sparse_capture, true);
    Check(Admits(), "legacy sparse Arcade remains admitted");
    Configure(); OptIn(false); REXCVAR_SET(mcla_arcade_navigation_probe, false);
    Check(Admits(), "legacy standalone stock configuration remains admitted");
    REXCVAR_SET(mcla_raw_config_experiment, true);
    Check(Admits() == kRaw, "legacy standalone raw still requires compile support");
    Check(!arcade_compile_stub::calls && !stub::installs && !stub::begins &&
          !stub::freezes && !stub::disarms, "no controller or census lifecycle executed");
    std::cout << "ARCADE_CONFIG_COMPILE raw=" << unsigned(kRaw) << " fixed60=" << unsigned(kFixed)
              << " checks=" << compile_checks << " failures=" << compile_failures << '\n';
    std::cout.flush(); std::cerr.flush();
    if (!std::cout.good() || !std::cerr.good()) return 2;
    return compile_failures ? 1 : 0;
  } catch (const std::exception& e) {
    std::cerr << "FAIL unexpected compile-matrix exception: " << e.what() << '\n';
    std::cerr.flush();
    return std::cerr.good() ? 1 : 2;
  }
}
