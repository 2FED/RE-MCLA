// Actual ValidateConfig only, no lifecycle/game/input/capture. RED compiles the
// old autostart without MCLA_ARCADE_UPDATE_BINDING_PROBE. GREEN defines it.
// Register fixture-only flags in both variants; production module registration
// and phase dispatcher binding require independent adapter tests.
#define main RetainedMotionConfigMain
#include "mcla_motion_config_win_test.cpp"
#undef main
#include "mcla_timer_journal.h"
#include <functional>
#include <vector>

#if !defined(MCLA_ARCADE_TIMER_PROBE) || !defined(MCLA_RAW_CONFIG_EXPERIMENT) || \
    !defined(MCLA_FIXED60_CONFIG_EXPERIMENT) || !defined(MCLA_GUEST_SOURCE_SCOUT_PROBE) || \
    !defined(MCLA_UPDATE_TIMING_PROBE) || !defined(MCLA_INPUT_RETURN_PROBE)
#error Compile all legacy admission branches and the Arcade timer adapter API.
#endif
#define FIXTURE_FLAG(name) REXCVAR_DEFINE_BOOL(name, false, "Test", "Admission fixture only") \
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
FIXTURE_FLAG(mcla_arcade_timer_probe);
FIXTURE_FLAG(mcla_arcade_update_binding_probe);
FIXTURE_FLAG(mcla_phase_census_probe);
FIXTURE_FLAG(mcla_timer_observer_probe);
FIXTURE_FLAG(mcla_frame_handoff_probe);
FIXTURE_FLAG(mcla_swap_observer_probe);
#undef FIXTURE_FLAG
namespace binding_stub { unsigned calls{}; }
namespace mcla::input_return {
bool Enabled() { return REXCVAR_GET(mcla_input_return_probe); }
void Enable() { ++binding_stub::calls; throw std::runtime_error("no enable"); }
void Install(rex::Runtime&) { ++binding_stub::calls; throw std::runtime_error("no install"); }
void Begin(const std::string&, uint64_t) { ++binding_stub::calls; throw std::runtime_error("no begin"); }
std::string Freeze(const std::string&) { ++binding_stub::calls; throw std::runtime_error("no freeze"); }
void Disarm() noexcept { ++binding_stub::calls; }
}
namespace mcla::arcade_timer {
bool Enabled() { return REXCVAR_GET(mcla_arcade_timer_probe); }
#if defined(MCLA_ARCADE_UPDATE_BINDING_PROBE)
bool UpdateBindingEnabled() { return REXCVAR_GET(mcla_arcade_update_binding_probe); }
#endif
void Install(rex::Runtime&) { ++binding_stub::calls; throw std::runtime_error("no install"); }
void Begin(uint64_t,uint64_t,uint64_t,const std::string&) {
  ++binding_stub::calls; throw std::runtime_error("no begin");
}
timer_journal::Window ObservationWindow(const std::string&) {
  ++binding_stub::calls; throw std::runtime_error("no window");
}
std::string Finish(std::stop_token,const std::string&) {
  ++binding_stub::calls; throw std::runtime_error("no finish");
}
void Disarm() noexcept { ++binding_stub::calls; }
void Invoke(PPCFunc*,PPCContext&,uint8_t*) { ++binding_stub::calls; throw std::runtime_error("no invoke"); }
}
#if defined(MCLA_ARCADE_UPDATE_BINDING_PROBE)
namespace mcla::phase_census {
bool Enabled() { return REXCVAR_GET(mcla_phase_census_probe); }
void Begin(uint64_t,uint64_t,uint64_t,const std::string&) {
  ++binding_stub::calls; throw std::runtime_error("no phase begin");
}
std::string Finish(std::stop_token,const std::string&) {
  ++binding_stub::calls; throw std::runtime_error("no phase finish");
}
void Disarm() noexcept { ++binding_stub::calls; }
}
#endif
namespace {
unsigned binding_checks{}, binding_failures{};
void CheckBinding(bool ok,const char* label) {
  ++binding_checks;
  if (!ok) { ++binding_failures; std::cerr << "FAIL " << label << '\n'; }
}
void BindingConfig() {
  REXCVAR_SET(mcla_arcade_timer_probe,true);
  REXCVAR_SET(mcla_arcade_update_binding_probe,true);
  REXCVAR_SET(mcla_phase_census_probe,true);
  REXCVAR_SET(mcla_arcade_config_probe,true);
  REXCVAR_SET(mcla_autostart_probe,true);
  REXCVAR_SET(mcla_arcade_navigation_probe,true);
  REXCVAR_SET(mcla_motion_calibration_probe,true);
  REXCVAR_SET(mcla_motion_config_probe,true);
  REXCVAR_SET(mcla_timer_config_probe,true);
  REXCVAR_SET(mcla_raw_config_experiment,false);
  REXCVAR_SET(mcla_fixed60_config_experiment,false);
  REXCVAR_SET(mcla_input_return_probe,false);
  REXCVAR_SET(mcla_source_scout_probe,false);
  REXCVAR_SET(mcla_update_timing_probe,false);
  REXCVAR_SET(mcla_timer_observer_probe,false);
  REXCVAR_SET(mcla_frame_handoff_probe,false);
  REXCVAR_SET(mcla_swap_observer_probe,false);
  REXCVAR_SET(mcla_motion_unknown_probe,false);
  REXCVAR_SET(mcla_motion_unknown_experiment,false);
  REXCVAR_SET(mcla_autostart_token,stub::token);
  REXCVAR_SET(mcla_motion_calibration_trigger,255u);
  REXCVAR_SET(mcla_autostart_start_delay_ms,0u);
  REXCVAR_SET(mcla_arcade_sparse_capture,false);
}
}
int main(int argc,char**) {
  if (argc != 1) return 2;
  try {
    const auto* flag=rex::cvar::GetFlagInfo("mcla_arcade_update_binding_probe");
    CheckBinding(flag && flag->getter && flag->getter()=="false" &&
        flag->default_value=="false" && flag->lifecycle==rex::cvar::Lifecycle::kInitOnly,
        "fixture default OFF init-only, not production registration proof");
    CheckBinding(Admits(),"all defaults remain admitted");
    BindingConfig(); CheckBinding(Admits(),"explicit stock dense RT255 binding admitted");
    struct Negative { const char* label; std::function<void()> mutate; };
    const std::vector<Negative> negatives{
      {"requires timer",[]{REXCVAR_SET(mcla_arcade_timer_probe,false);}},
      {"requires phase",[]{REXCVAR_SET(mcla_phase_census_probe,false);}},
      {"requires Arcade config",[]{REXCVAR_SET(mcla_arcade_config_probe,false);}},
      {"requires autostart",[]{REXCVAR_SET(mcla_autostart_probe,false);}},
      {"requires Arcade navigation",[]{REXCVAR_SET(mcla_arcade_navigation_probe,false);}},
      {"requires motion",[]{REXCVAR_SET(mcla_motion_calibration_probe,false);}},
      {"requires motion config",[]{REXCVAR_SET(mcla_motion_config_probe,false);}},
      {"requires timer config",[]{REXCVAR_SET(mcla_timer_config_probe,false);}},
      {"stock only, raw rejected",[]{REXCVAR_SET(mcla_raw_config_experiment,true);}},
      {"fixed60 rejected",[]{REXCVAR_SET(mcla_fixed60_config_experiment,true);}},
      {"RT96 rejected",[]{REXCVAR_SET(mcla_motion_calibration_trigger,96u);}},
      {"RT254 rejected",[]{REXCVAR_SET(mcla_motion_calibration_trigger,254u);}},
      {"sparse rejected",[]{REXCVAR_SET(mcla_arcade_sparse_capture,true);}},
      {"8000ms delay rejected",[]{REXCVAR_SET(mcla_autostart_start_delay_ms,8000u);}},
      {"1ms delay rejected",[]{REXCVAR_SET(mcla_autostart_start_delay_ms,1u);}},
      {"legacy timer rejected",[]{REXCVAR_SET(mcla_timer_observer_probe,true);}},
      {"legacy update timing rejected",[]{REXCVAR_SET(mcla_update_timing_probe,true);}},
      {"scout rejected",[]{REXCVAR_SET(mcla_source_scout_probe,true);}},
      {"handoff rejected",[]{REXCVAR_SET(mcla_frame_handoff_probe,true);}},
      {"swap observer rejected",[]{REXCVAR_SET(mcla_swap_observer_probe,true);}},
      {"input census rejected",[]{REXCVAR_SET(mcla_input_return_probe,true);}},
      {"unknown probe rejected",[]{REXCVAR_SET(mcla_motion_unknown_probe,true);}},
      {"unknown experiment rejected",[]{REXCVAR_SET(mcla_motion_unknown_experiment,true);}},
      {"empty token rejected",[]{REXCVAR_SET(mcla_autostart_token,"");}},
      {"zero token rejected",[]{REXCVAR_SET(mcla_autostart_token,std::string(32,'0'));}},
      {"uppercase token rejected",[]{REXCVAR_SET(mcla_autostart_token,std::string(32,'A'));}},
      {"short token rejected",[]{REXCVAR_SET(mcla_autostart_token,std::string(31,'a'));}},
      {"long token rejected",[]{REXCVAR_SET(mcla_autostart_token,std::string(33,'a'));}},
    };
    for (const auto& n:negatives) { BindingConfig(); n.mutate(); CheckBinding(!Admits(),n.label); }
    BindingConfig(); REXCVAR_SET(mcla_arcade_update_binding_probe,false);
    CheckBinding(!Admits(),"phase not admitted by legacy Arcade timer mode");
    REXCVAR_SET(mcla_phase_census_probe,false);
    CheckBinding(Admits(),"binding OFF preserves v1 stock");
    REXCVAR_SET(mcla_raw_config_experiment,true);
    CheckBinding(Admits(),"binding OFF preserves v1 raw");
    REXCVAR_SET(mcla_phase_census_probe,true);
    CheckBinding(!Admits(),"binding OFF phase also rejected for raw");
    CheckBinding(!binding_stub::calls && !stub::installs && !stub::begins &&
        !stub::freezes && !stub::disarms,"no lifecycle or original called");
    std::cout << "ARCADE_BINDING_ADMISSION checks=" << binding_checks
              << " failures=" << binding_failures << '\n';
    std::cout.flush();std::cerr.flush();
    if (!std::cout || !std::cerr) return 2;
    return binding_failures?1:0;
  } catch (const std::exception& e) {
    std::cerr << "FAIL " << e.what() << '\n';std::cerr.flush();return std::cerr?1:2;
  }
}
