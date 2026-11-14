// Actual navigation lifecycle with synthetic presenter/input and a stub census.
// Never launches a game or injects OS input. One mode per fresh process/root.
#define main RetainedMotionConfigMain
#include "mcla_motion_config_win_test.cpp"
#undef main
#if !defined(MCLA_INPUT_RETURN_PROBE)
#error Explicit private input census build required.
#endif
namespace census_stub {
unsigned begins{}, freezes{}, disarms{};
std::string mode, serialized;
bool enabled{};
}
namespace mcla::input_return {
void Enable() { census_stub::enabled = true; }
bool Enabled() { return census_stub::enabled; }
void Install(rex::Runtime&) { throw std::runtime_error("No actual runtime installation in synthetic test"); }
void Begin(const std::string& token, uint64_t duration_ms) {
  ++census_stub::begins;
  Need(token == stub::token && duration_ms == 30000, "exact census window/token");
  if (census_stub::mode == "begin-fail") throw std::runtime_error("CENSUS_BEGIN_TEST_FAILURE");
}
std::string Freeze(const std::string& token) {
  ++census_stub::freezes;
  Need(token == stub::token && census_stub::begins == 1, "freeze only begun census");
  if (census_stub::mode == "freeze-fail") throw std::runtime_error("CENSUS_FREEZE_TEST_FAILURE");
  if (census_stub::mode == "empty") return {};
  census_stub::serialized = "{\"synthetic_census\":true}";
  return census_stub::serialized;
}
void Disarm() noexcept { ++census_stub::disarms; }
}
int main(int argc, char** argv) {
  try {
    Need(argc == 4, "MODE FRESH_ABSOLUTE_ROOT PINNED_TITLE required");
    census_stub::mode = argv[1]; const auto& mode = census_stub::mode;
    Need(mode == "success" || mode == "disabled" || mode == "begin-fail" ||
         mode == "freeze-fail" || mode == "empty" || mode == "cancel" ||
         mode == "output-exists", "known input census integration mode");
    Registry();
    const auto* flag = rex::cvar::GetFlagInfo("mcla_input_return_probe");
    Need(flag && flag->default_value == "false" && flag->getter() == "false" &&
         flag->lifecycle == rex::cvar::Lifecycle::kInitOnly, "census defaultOFF/initonly");
    REXCVAR_SET(mcla_input_return_probe, true);
    REXCVAR_SET(mcla_motion_config_probe, false);
    Need(!Admits(), "census rejects missing standalone mode");
    REXCVAR_SET(mcla_motion_config_probe, true);
    REXCVAR_SET(mcla_fixed60_config_experiment, true);
    REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
    REXCVAR_SET(mcla_autostart_start_delay_ms, 8000u);
    Need(Admits(), "census admits exact motionconfig prerequisites");
    const bool disabled = mode == "disabled";
    REXCVAR_SET(mcla_input_return_probe, !disabled);
    const std::filesystem::path root(argv[2]);
    Need(root.is_absolute() && !std::filesystem::exists(root), "fresh absolute owned root");
    Need(std::filesystem::create_directory(root), "fresh root created");
    if (mode == "output-exists") Need(std::filesystem::create_directory(root / "input-return"), "collision fixture");
    a::Driver driver; NoCapture presenter; std::stop_source stop;
    LoadTitle(presenter, argv[3]);
    std::atomic<bool> reader_failed{false};
    unsigned edges = 0;
    std::jthread reader([&](std::stop_token done) {
      bool prior = false;
      std::optional<std::chrono::steady_clock::time_point> cancel_at;
      while (!done.stop_requested()) {
        rex::input::X_INPUT_STATE state{};
        if (driver.GetState(0, &state) != rex::X_RESULT{0} ||
            (state.gamepad.buttons != 0 && state.gamepad.buttons != mcla::autostart_input::kStartButton)) {
          reader_failed.store(true); stop.request_stop(); return;
        }
        const bool down = state.gamepad.buttons != 0;
        if (down && !prior) ++edges; prior = down;
        if (mode == "cancel" && presenter.third_capture_returned.load()) {
          const auto now = std::chrono::steady_clock::now();
          if (!cancel_at) cancel_at = now + std::chrono::seconds(1);
          if (now >= *cancel_at) { stop.request_stop(); return; }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    });
    const bool returned = a::Run(stop.get_token(), presenter, driver, root);
    reader.request_stop(); reader.join();
    const bool success = mode == "success" || disabled;
    Need(returned == success && !reader_failed.load(), "expected actual navigation result");
    Need(census_stub::disarms == unsigned(!disabled), "enabled census always disarmed exactly once");
    const bool begun = !disabled && mode != "output-exists";
    Need(census_stub::begins == unsigned(begun), "Begin occurs only after confirmed title");
    const bool pulse = mode == "success" || disabled || mode == "freeze-fail" || mode == "empty";
    Need(edges == unsigned(pulse), "at most one unchanged START pulse");
    Need(!driver.PollMotion().published_ms, "navigation test never arms RT");
    const auto nav = Json::parse(Read(root / "autostart/result.json"));
    Need(nav.at("status") == (success ? "INPUT_COMPLETE_SCENE_REVIEW_PENDING" : "FAILED"), "honest navigation receipt");
    if (success) {
      Need(nav.at("frames").size() == 6 && nav.at("delay_ms") == 8000, "unchanged full navigation schedule");
      Need(census_stub::freezes == unsigned(!disabled), "success freezes once");
    }
    if (mode == "begin-fail" || mode == "output-exists" || disabled)
      Need(census_stub::freezes == 0, "no Freeze before Begin or when disabled");
    if (mode == "freeze-fail" || mode == "empty")
      Need(census_stub::freezes == 2, "one initial freeze plus retained failure cleanup attempt");
    if (mode == "cancel") Need(census_stub::freezes == 1 && !nav.at("published_ms").is_number(), "pre-arm cancellation retains cut");
    if (disabled) Need(!std::filesystem::exists(root / "input-return"), "disabled creates no census path");
    else if (mode == "output-exists") Need(std::filesystem::is_empty(root / "input-return"), "collision untouched");
    else {
      const auto bytes = Read(root / "input-return/result.json");
      Need(bytes == ((mode == "success" || mode == "cancel") ? census_stub::serialized : ""), "raw census exact or explicit incomplete file");
    }
    std::cout << "input-return integration: " << checks << " checks PASS mode=" << mode << '\n';
    return 0;
  } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
