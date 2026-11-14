// Actual RunMotionConfig/Run/RunMotion/Driver, synthetic presenter/controller.
// No game, GPU, Runtime installation, OS input, timer edits or physics proof.
// One process per MODE SCENARIO FRESH_ABSOLUTE_ROOT [PINNED_TITLE_BMP].
// MODE: off/raw/fixed60. SCENARIO: success/cancel/freeze-fail.
#define main RetainedMotionConfigMain
#include "mcla_motion_config_win_test.cpp"
#undef main
#if !defined(MCLA_INPUT_RETURN_PROBE) || !defined(MCLA_RAW_CONFIG_EXPERIMENT) || !defined(MCLA_FIXED60_CONFIG_EXPERIMENT)
#error Census plus both configuration capabilities required for this fixture.
#endif

namespace raw_pair {
unsigned enables{}, installs{}, begins{}, freezes{}, disarms{};
bool raw{}, fixed{};
std::string text;
void ModeStable() {
  Need(REXCVAR_GET(mcla_raw_config_experiment) == raw &&
       REXCVAR_GET(mcla_fixed60_config_experiment) == fixed,
       "selected controller mode remains unchanged");
}
}
namespace mcla::input_return {
bool Enabled() { return false; }
void Enable() { ++raw_pair::enables; throw std::runtime_error("no real census Enable"); }
void Install(rex::Runtime&) { ++raw_pair::installs; throw std::runtime_error("no Runtime"); }
void Begin(const std::string& token, uint64_t duration_ms) {
  ++raw_pair::begins;
  Need(token == stub::token && duration_ms == 30000, "bounded census token/window");
  raw_pair::ModeStable();
}
std::string Freeze(const std::string& token) {
  ++raw_pair::freezes;
  Need(token == stub::token && raw_pair::begins == 1, "same begun census freezes");
  raw_pair::ModeStable();
  raw_pair::text = std::string("{\"synthetic_census\":true,\"mode\":\"") +
      (raw_pair::raw ? "raw" : raw_pair::fixed ? "fixed60" : "stock") + "\"}";
  return raw_pair::text;
}
void Disarm() noexcept { ++raw_pair::disarms; }
}

int main(int argc, char** argv) {
  try {
    Need(argc == 4 || argc == 5, "MODE SCENARIO FRESH_ROOT [PINNED_TITLE] required");
    const std::string mode = argv[1], scenario = argv[2];
    Need(mode == "off" || mode == "raw" || mode == "fixed60", "explicit mode");
    Need(scenario == "success" || scenario == "cancel" || scenario == "freeze-fail", "explicit scenario");
    const bool success = scenario == "success", cancel = scenario == "cancel";
    const bool captures = scenario != "freeze-fail";
    Need((argc == 5) == captures, "title required exactly for capture scenarios");
    raw_pair::raw = mode == "raw"; raw_pair::fixed = mode == "fixed60";
    REXCVAR_SET(mcla_autostart_probe, true);
    REXCVAR_SET(mcla_motion_calibration_probe, true);
    REXCVAR_SET(mcla_motion_config_probe, true);
    REXCVAR_SET(mcla_timer_config_probe, true);
    REXCVAR_SET(mcla_fixed60_config_experiment, raw_pair::fixed);
    REXCVAR_SET(mcla_raw_config_experiment, raw_pair::raw);
    REXCVAR_SET(mcla_input_return_probe, true);
    REXCVAR_SET(mcla_autostart_token, stub::token);
    REXCVAR_SET(mcla_motion_calibration_trigger, 255u);
    REXCVAR_SET(mcla_autostart_start_delay_ms, 8000u);
    Need(Admits(), "selected mode admitted by actual ValidateConfig");
    const std::filesystem::path root(argv[3]);
    Need(root.is_absolute() && !std::filesystem::exists(root), "fresh absolute owned root");
    Need(std::filesystem::create_directory(root), "create fresh owned root");
    stub::fail_freeze = !captures;
    a::Driver driver; NoCapture presenter; std::stop_source stop;
    if (captures) LoadTitle(presenter, argv[4]); else stop.request_stop();
    std::atomic<bool> bad_input{false};
    unsigned start_edges{}, motion_edges{};
    std::jthread reader;
    if (captures) reader = std::jthread([&](std::stop_token done) {
      bool start_down{}, motion_down{};
      std::optional<std::chrono::steady_clock::time_point> cancel_due;
      while (!done.stop_requested()) {
        rex::input::X_INPUT_STATE state{};
        if (driver.GetState(0, &state) != rex::X_RESULT{0} ||
            (state.gamepad.buttons != 0 && state.gamepad.buttons != mcla::autostart_input::kStartButton) ||
            (state.gamepad.right_trigger != 0 && state.gamepad.right_trigger != 255) ||
            (state.gamepad.buttons && state.gamepad.right_trigger) || state.gamepad.left_trigger ||
            state.gamepad.thumb_lx || state.gamepad.thumb_ly || state.gamepad.thumb_rx || state.gamepad.thumb_ry ||
            (cancel && (state.gamepad.buttons || state.gamepad.right_trigger))) {
          bad_input.store(true); stop.request_stop(); return;
        }
        if (state.gamepad.buttons && !start_down) ++start_edges;
        if (state.gamepad.right_trigger && !motion_down) ++motion_edges;
        start_down = state.gamepad.buttons != 0;
        motion_down = state.gamepad.right_trigger != 0;
        if (cancel && presenter.third_capture_returned.load()) {
          const auto now = std::chrono::steady_clock::now();
          if (!cancel_due) cancel_due = now + std::chrono::seconds(1);
          if (now >= *cancel_due) { stop.request_stop(); return; }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    });
    const bool returned = a::RunMotionConfig(stop.get_token(), presenter, driver, root);
    if (reader.joinable()) { reader.request_stop(); reader.join(); }
    Need(returned == success && !bad_input.load(), "actual workflow result");
    Need(start_edges == unsigned(success) && motion_edges == unsigned(success),
         "one START then one RT pulse, none for cancellation/failure");
    Need(stub::freezes == 1 && stub::disarms == 1 && !stub::installs && !stub::begins,
         "one controller Freeze/Disarm, no Runtime installation");
    Need(raw_pair::begins == unsigned(captures) && raw_pair::freezes == unsigned(captures) &&
         raw_pair::disarms == 1 && !raw_pair::enables && !raw_pair::installs,
         "census lifecycle matches reached stage, always disarmed");
    raw_pair::ModeStable();
    rex::input::X_INPUT_STATE neutral{};
    Need(driver.GetState(0, &neutral) == rex::X_RESULT{0} && !neutral.gamepad.buttons &&
         !neutral.gamepad.right_trigger && !neutral.gamepad.left_trigger &&
         !neutral.gamepad.thumb_lx && !neutral.gamepad.thumb_ly &&
         !neutral.gamepad.thumb_rx && !neutral.gamepad.thumb_ry && !driver.ArmMotion(),
         "terminal state remains neutral and cannot rearm motion");
    const auto terminal = Json::parse(Read(root / "motion-config/result.json"));
    Need(terminal.at("schema") == "mcla-motion-config-terminal-v1" && terminal.at("token") == stub::token &&
         terminal.at("mode") == (mode == "off" ? "stock" : mode), "explicit native terminal mode/token");
    Need(terminal.at("status") == (success ? "COLLECTED_REVIEW_PENDING" : "FAILED") &&
         terminal.at("navigation_collected") == success && terminal.at("motion_collected") == success &&
         terminal.at("stop_requested") == !success && terminal.at("controller_disarmed") == true,
         "truthful terminal lifecycle");
    Need(terminal.at("injection_requested") == true && terminal.at("input_injected") == success,
         "request distinguished from actual ACK");
    for (const char* key : {"os_input", "desktop_capture", "scene_proof", "movement_proof",
                            "physical_speed_proof", "fps_proof", "configuration_accepted"})
      Need(terminal.at(key) == false, "no physical/game/OS claims");
    const auto configuration = Read(root / "motion-config/timer-config.json");
    Need(terminal.at("configuration_written") == captures && terminal.at("configuration_bytes") == configuration.size() &&
         configuration == (captures ? stub::retained : ""), "exact controller bytes and failed-cut inventory");
    Need(terminal.at("failure") == (success ? "" : cancel ? "NAVIGATION_FAILED" : "CONFIGURATION_CUT_OR_WRITE_FAILED"),
         "failure phase preserved");
    Need(Read(root / "input-return/result.json") == raw_pair::text, "exact census bytes, empty when never begun");
    const auto nav = Json::parse(Read(root / "autostart/result.json"));
    Need(nav.at("schema") == "mcla-autostart-navigation-v2" && nav.at("delay_ms") == 8000,
         "same explicit delayed navigation protocol");
    if (success) {
      const auto motion = Json::parse(Read(root / "motion-calibration/result.json"));
      const auto p = nav.at("published_ms").get<uint64_t>();
      const auto down = nav.at("down_ack_ms").get<uint64_t>();
      const auto release = nav.at("released_ms").get<uint64_t>();
      const auto neutral_ack = nav.at("neutral_ack_ms").get<uint64_t>();
      Need(nav.at("status") == "INPUT_COMPLETE_SCENE_REVIEW_PENDING" && nav.at("input_status") == 3 && nav.at("input_error") == 0 &&
           nav.at("start_due_ms").get<uint64_t>() == nav.at("title_confirmed_ms").get<uint64_t>() + 8000 &&
           nav.at("start_due_ms").get<uint64_t>() <= nav.at("arm_before_ms").get<uint64_t>() &&
           nav.at("arm_before_ms").get<uint64_t>() <= p && p <= nav.at("arm_after_ms").get<uint64_t>(),
           "same actual START delay publication brackets");
      Need(p <= down && down < p + 5000 && p + 5000 <= release && release <= neutral_ack && neutral_ack < p + 10000,
           "START hold/neutral ACK bounds unchanged");
      const auto mp = motion.at("published_ms").get<uint64_t>();
      Need(motion.at("schema") == "mcla-motion-calibration-v2" &&
           motion.at("status") == "MOTION_CALIBRATION_COLLECTED_REVIEW_PENDING" &&
           motion.at("right_trigger") == 255 && motion.at("start_neutral_acknowledged") == true &&
           motion.at("down_read_count").get<uint64_t>() > 0 &&
           motion.at("release_deadline_ms").get<uint64_t>() == mp + 3000 &&
           motion.at("neutral_ack_deadline_ms").get<uint64_t>() == mp + 8000 &&
           neutral_ack <= motion.at("frames").at(0).at("request_before_ms").get<uint64_t>(),
           "bounded RT follows actual START neutral ACK");
      Need(nav.at("frames").size() == 6 && motion.at("frames").size() == 6 && presenter.calls == 14,
           "same twelve stored images and title observations");
      Need(motion.at("capture_offsets_ms") == Json({500,1500,2500,3500,6000}), "unchanged motion schedule");
      for (size_t i = 1; i < 6; ++i)
        Need(motion.at("frames").at(i).at("due_ms").get<uint64_t>() ==
             mp + motion.at("capture_offsets_ms").at(i-1).get<uint64_t>(), "publication-anchored capture slot");
      for (const char* key : {"scene_proven", "movement_proven", "physical_speed_proven", "fps_proven"})
        Need(motion.at(key) == false, "synthetic motion does not prove physics");
      for (const char* folder : {"autostart", "motion-calibration"}) {
        unsigned images{}, files{};
        for (const auto& item : std::filesystem::directory_iterator(root / folder)) {
          Need(item.is_regular_file(), "regular owned artifact"); ++files;
          if (item.path().extension() == ".bmp") ++images;
        }
        Need(images == 6 && files == 7, "exact six images and one receipt per stage");
      }
    } else {
      Need(nav.at("status") == "FAILED" && nav.at("published_ms").is_null() &&
           nav.at("arm_before_ms").is_null() && !driver.PollMotion().published_ms &&
           !std::filesystem::exists(root / "motion-calibration"), "failure never arms START or RT");
      Need(presenter.calls == (cancel ? 3u : 0u), "cancel after title versus pre-cancelled failed cut");
    }
    std::cout << "RAW_MOTION_INTEGRATION checks=" << checks << " failures=0 mode=" << mode
              << " scenario=" << scenario << '\n';
    return 0;
  } catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
}
